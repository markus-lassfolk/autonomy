#include "starlink_grpc_shared.h"
#include "../protobuf/protobuf_wire.h"
#include "../logging/logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include <arpa/inet.h>

// Global configuration
static starlink_grpc_shared_config_t g_shared_config = {0};
static bool g_initialized = false;

// gRPC framing function (from standalone client)
static int frame_grpc(const unsigned char *msg, size_t msg_len, unsigned char *out, size_t *out_len, size_t cap) {
    if (cap < msg_len + 5) return -1;
    out[0] = 0; // no compression
    out[1] = (unsigned char)((msg_len >> 24) & 0xFF);
    out[2] = (unsigned char)((msg_len >> 16) & 0xFF);
    out[3] = (unsigned char)((msg_len >> 8) & 0xFF);
    out[4] = (unsigned char)(msg_len & 0xFF);
    memcpy(out + 5, msg, msg_len);
    *out_len = msg_len + 5;
    return 0;
}

// Encode protobuf field (from standalone client)
static int encode_length_delimited_field(uint32_t field_number, const unsigned char *data, size_t data_len, 
                                        unsigned char *out, size_t *out_len, size_t cap) {
    if (!data || !out || !out_len) return -1;
    
    // Calculate total size needed
    size_t key_size = 1; // Assume single byte key
    if (field_number > 15) key_size = 2; // Two byte key
    
    size_t len_size = 1; // Assume single byte length
    if (data_len > 127) len_size = 2; // Two byte length
    if (data_len > 16383) len_size = 3; // Three byte length
    if (data_len > 2097151) len_size = 4; // Four byte length
    if (data_len > 268435455) len_size = 5; // Five byte length
    
    size_t total_size = key_size + len_size + data_len;
    if (total_size > cap) return -1;
    
    size_t pos = 0;
    
    // Encode key (field number << 3 | wire type 2)
    uint32_t key = (field_number << 3) | 2; // wire type 2 = length-delimited
    if (key < 128) {
        out[pos++] = (unsigned char)key;
    } else {
        out[pos++] = (unsigned char)((key >> 7) | 0x80);
        out[pos++] = (unsigned char)(key & 0x7F);
    }
    
    // Encode length
    if (data_len < 128) {
        out[pos++] = (unsigned char)data_len;
    } else if (data_len < 16384) {
        out[pos++] = (unsigned char)((data_len >> 7) | 0x80);
        out[pos++] = (unsigned char)(data_len & 0x7F);
    } else if (data_len < 2097152) {
        out[pos++] = (unsigned char)((data_len >> 14) | 0x80);
        out[pos++] = (unsigned char)((data_len >> 7) | 0x80);
        out[pos++] = (unsigned char)(data_len & 0x7F);
    } else if (data_len < 268435456) {
        out[pos++] = (unsigned char)((data_len >> 21) | 0x80);
        out[pos++] = (unsigned char)((data_len >> 14) | 0x80);
        out[pos++] = (unsigned char)((data_len >> 7) | 0x80);
        out[pos++] = (unsigned char)(data_len & 0x7F);
    } else {
        out[pos++] = (unsigned char)((data_len >> 28) | 0x80);
        out[pos++] = (unsigned char)((data_len >> 21) | 0x80);
        out[pos++] = (unsigned char)((data_len >> 14) | 0x80);
        out[pos++] = (unsigned char)((data_len >> 7) | 0x80);
        out[pos++] = (unsigned char)(data_len & 0x7F);
    }
    
    // Copy data
    memcpy(out + pos, data, data_len);
    pos += data_len;
    
    *out_len = pos;
    return 0;
}

// Write callback for curl
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    starlink_grpc_shared_response_t *response = (starlink_grpc_shared_response_t *)userp;
    size_t total_size = size * nmemb;
    
    if (response->response_data) {
        // Reallocate buffer
        char *new_data = realloc(response->response_data, response->response_size + total_size + 1);
        if (!new_data) {
            LOGX_ERROR_MSG("Failed to reallocate response buffer");
            return 0;
        }
        response->response_data = new_data;
    } else {
        // Allocate initial buffer
        response->response_data = malloc(total_size + 1);
        if (!response->response_data) {
            LOGX_ERROR_MSG("Failed to allocate response buffer");
            return 0;
        }
    }
    
    memcpy(response->response_data + response->response_size, contents, total_size);
    response->response_size += total_size;
    response->response_data[response->response_size] = '\0';
    
    return total_size;
}

// Initialize shared gRPC client
int starlink_grpc_shared_init(const starlink_grpc_shared_config_t *config) {
    if (!config) {
        LOGX_ERROR_MSG("starlink_grpc_shared_init: NULL config");
        return -1;
    }
    
    LOGX_DEBUG_MSG("Initializing shared Starlink gRPC client");
    
    // Copy configuration
    memcpy(&g_shared_config, config, sizeof(starlink_grpc_shared_config_t));
    
    // Initialize curl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    g_initialized = true;
    LOGX_INFO_MSG("Shared Starlink gRPC client initialized successfully");
    
    return 0;
}

// Cleanup shared gRPC client
void starlink_grpc_shared_cleanup(void) {
    if (g_initialized) {
        curl_global_cleanup();
        g_initialized = false;
        LOGX_DEBUG_MSG("Shared Starlink gRPC client cleaned up");
    }
}

// Make a gRPC call to Starlink device
int starlink_grpc_shared_call(const char *method, 
                              const unsigned char *request_data, 
                              size_t request_size,
                              starlink_grpc_shared_response_t *response) {
    if (!g_initialized) {
        LOGX_ERROR_MSG("starlink_grpc_shared_call: client not initialized");
        return -1;
    }
    
    if (!method || !response) {
        LOGX_ERROR_MSG("starlink_grpc_shared_call: NULL method or response");
        return -1;
    }
    
    LOGX_DEBUG_MSG("Making gRPC call to method: %s", method);
    
    // Initialize response
    memset(response, 0, sizeof(starlink_grpc_shared_response_t));
    response->timestamp = time(NULL);
    
    // Build URL
    char url[512];
    snprintf(url, sizeof(url), "http://%s:%d/SpaceX.API.Device.Device/Handle", 
             g_shared_config.host, g_shared_config.port);
    
    LOGX_DEBUG_MSG("gRPC request URL: %s", url);
    
    // Create gRPC frame
    unsigned char frame[1024];
    size_t frame_len = 0;
    
    if (request_data && request_size > 0) {
        if (frame_grpc(request_data, request_size, frame, &frame_len, sizeof(frame)) != 0) {
            LOGX_ERROR_MSG("Failed to frame gRPC message");
            strcpy(response->error_message, "Failed to frame gRPC message");
            return -1;
        }
    } else {
        // Empty request
        frame[0] = 0; // no compression
        frame[1] = 0; // length = 0
        frame[2] = 0;
        frame[3] = 0;
        frame[4] = 0;
        frame_len = 5;
    }
    
    // Setup curl
    CURL *curl = curl_easy_init();
    if (!curl) {
        LOGX_ERROR_MSG("Failed to initialize curl");
        strcpy(response->error_message, "Failed to initialize curl");
        return -1;
    }
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/grpc");
    headers = curl_slist_append(headers, "grpc-encoding: identity");
    headers = curl_slist_append(headers, "grpc-accept-encoding: identity");
    headers = curl_slist_append(headers, "TE: trailers");
    
    if (strlen(g_shared_config.user_agent) > 0) {
        char user_agent_header[256];
        snprintf(user_agent_header, sizeof(user_agent_header), "User-Agent: %s", g_shared_config.user_agent);
        headers = curl_slist_append(headers, user_agent_header);
    } else {
        headers = curl_slist_append(headers, "User-Agent: starlink-grpc-shared/1.0");
    }
    
    // Configure curl
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, frame);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)frame_len);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)g_shared_config.timeout);
    
    if (g_shared_config.insecure_mode) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    
    // Perform request
    LOGX_DEBUG_MSG("Performing gRPC request to %s...", url);
    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    response->http_status = (int)http_code;
    LOGX_DEBUG_MSG("gRPC request completed: result=%d, http_code=%ld", res, http_code);
    
    // Cleanup curl
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        LOGX_WARN_MSG("gRPC request failed: %s (error code %d)", curl_easy_strerror(res), res);
        
        // Handle specific curl errors gracefully
        if (res == CURLE_COULDNT_CONNECT || res == CURLE_COULDNT_RESOLVE_HOST) {
            snprintf(response->error_message, sizeof(response->error_message), 
                    "Cannot connect to Starlink device at %s:%d - device may be offline or unreachable", 
                    g_shared_config.host, g_shared_config.port);
        } else if (res == CURLE_OPERATION_TIMEDOUT) {
            snprintf(response->error_message, sizeof(response->error_message), 
                    "Starlink device timeout after %d seconds", g_shared_config.timeout);
        } else {
            snprintf(response->error_message, sizeof(response->error_message), 
                    "gRPC error: %s", curl_easy_strerror(res));
        }
        
        response->success = false;
        return -1;
    }
    
    // Handle HTTP error responses
    if (response->http_status >= 400) {
        LOGX_WARN_MSG("gRPC HTTP error: %d", response->http_status);
        
        if (response->http_status == 404) {
            snprintf(response->error_message, sizeof(response->error_message), 
                    "Starlink device not found at %s:%d - check if device is online and accessible", 
                    g_shared_config.host, g_shared_config.port);
        } else if (response->http_status == 403) {
            snprintf(response->error_message, sizeof(response->error_message), 
                    "Access denied by Starlink device - authentication may be required");
        } else if (response->http_status >= 500) {
            snprintf(response->error_message, sizeof(response->error_message), 
                    "Starlink device server error: HTTP %d", response->http_status);
        } else {
            snprintf(response->error_message, sizeof(response->error_message), 
                    "gRPC HTTP error: %d", response->http_status);
        }
        
        response->success = false;
        return -1;
    }
    
    response->success = true;
    LOGX_DEBUG_MSG("gRPC request successful, HTTP status: %d", response->http_status);
    
    return 0;
}

// Free response data
void starlink_grpc_shared_free_response(starlink_grpc_shared_response_t *response) {
    if (response && response->response_data) {
        free(response->response_data);
        response->response_data = NULL;
        response->response_size = 0;
    }
}

// Get device info
int starlink_grpc_shared_get_device_info(starlink_grpc_shared_response_t *response) {
    LOGX_DEBUG_MSG("Getting Starlink device info");
    return starlink_grpc_shared_call("get_device_info", NULL, 0, response);
}

// Get device status
int starlink_grpc_shared_get_status(starlink_grpc_shared_response_t *response) {
    LOGX_DEBUG_MSG("Getting Starlink device status");
    return starlink_grpc_shared_call("get_status", NULL, 0, response);
}

// Get device location
int starlink_grpc_shared_get_location(starlink_grpc_shared_response_t *response) {
    LOGX_DEBUG_MSG("Getting Starlink device location");
    return starlink_grpc_shared_call("get_location", NULL, 0, response);
}

// Get device history
int starlink_grpc_shared_get_history(starlink_grpc_shared_response_t *response) {
    LOGX_DEBUG_MSG("Getting Starlink device history");
    return starlink_grpc_shared_call("get_history", NULL, 0, response);
}

// Get device context
int starlink_grpc_shared_get_context(starlink_grpc_shared_response_t *response) {
    LOGX_DEBUG_MSG("Getting Starlink device context");
    return starlink_grpc_shared_call("get_context", NULL, 0, response);
}
