#include "starlink_grpc_comprehensive_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include <stdarg.h>
#include <arpa/inet.h>

// Simple logging macros for standalone use
#ifndef LOGX_INFO_MSG
#define LOGX_INFO_MSG(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#define LOGX_ERROR_MSG(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)
#define LOGX_WARN_MSG(fmt, ...) printf("[WARN] " fmt "\n", ##__VA_ARGS__)
#define LOGX_DEBUG_MSG(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#endif

// Global configuration instance
starlink_grpc_client_config_t g_starlink_grpc_config = {0};

// Initialize comprehensive gRPC client
int starlink_grpc_comprehensive_client_init(starlink_grpc_client_config_t *config) {
    fprintf(stderr, "DEBUG: starlink_grpc_comprehensive_client_init called\n");
    if (!config) {
        fprintf(stderr, "DEBUG: starlink_grpc_comprehensive_client_init failed - NULL config\n");
        return -1;
    }
    
    // Copy configuration
    fprintf(stderr, "DEBUG: starlink_grpc_comprehensive_client_init - copying config\n");
    memcpy(&g_starlink_grpc_config, config, sizeof(starlink_grpc_client_config_t));
    fprintf(stderr, "DEBUG: starlink_grpc_comprehensive_client_init - config copied\n");
    
    // Initialize curl
    fprintf(stderr, "DEBUG: starlink_grpc_comprehensive_client_init - about to initialize curl\n");
    curl_global_init(CURL_GLOBAL_DEFAULT);
    fprintf(stderr, "DEBUG: starlink_grpc_comprehensive_client_init - curl initialized\n");
    
    LOGX_INFO_MSG("Starlink gRPC comprehensive client initialized");
    fprintf(stderr, "DEBUG: starlink_grpc_comprehensive_client_init completed successfully\n");
    return 0;
}

// Parse endpoint (host:port format)
int starlink_grpc_parse_endpoint(const char *endpoint, char **host, int *port) {
    if (!endpoint || !host || !port) return -1;
    
    char *colon = strchr(endpoint, ':');
    if (!colon) {
        // No colon found, treat as host only
        *host = (char*)strdup(endpoint);
        *port = 9200; // default port
        return 0;
    }
    
    // Split at colon
    size_t host_len = colon - endpoint;
    *host = (char*)malloc(host_len + 1);
    if (!*host) return -1;
    
    strncpy(*host, endpoint, host_len);
    (*host)[host_len] = '\0';
    
    *port = atoi(colon + 1);
    if (*port <= 0 || *port > 65535) {
        free(*host);
        return -1;
    }
    
    return 0;
}

// Utility functions for flag functionality
void starlink_grpc_print_hex_data(const unsigned char *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (i % 16 == 0) printf("%04zx: ", i);
        printf("%02x ", data[i]);
        if (i % 16 == 15 || i == len - 1) {
            // Print ASCII representation
            size_t start = (i / 16) * 16;
            size_t end = i;
            printf(" |");
            for (size_t j = start; j <= end; j++) {
                printf("%c", (data[j] >= 32 && data[j] <= 126) ? data[j] : '.');
            }
            printf("|\n");
        }
    }
}

void starlink_grpc_print_timestamp(void) {
    if (g_starlink_grpc_config.timestamp_mode) {
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char buffer[64];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
        printf("[%s] ", buffer);
    }
}

void starlink_grpc_print_header(const char *status, size_t bytes) {
    if (!g_starlink_grpc_config.no_header && !g_starlink_grpc_config.silent_mode) {
        starlink_grpc_print_timestamp();
        printf("%s, bytes %zu\n", status, bytes);
    }
}

void starlink_grpc_print_debug_info(
    const char *url, 
    const char *method, 
    const unsigned char *request_data, 
    size_t request_len, 
    const unsigned char *response_data, 
    size_t response_len
) {
    if (g_starlink_grpc_config.debug_mode) {
        starlink_grpc_print_timestamp();
        printf("=== DEBUG INFO ===\n");
        printf("URL: %s\n", url);
        printf("Method: %s\n", method);
        printf("Request (%zu bytes):\n", request_len);
        if (g_starlink_grpc_config.hex_mode) {
            starlink_grpc_print_hex_data(request_data, request_len);
        } else {
            printf("%.*s\n", (int)request_len, request_data);
        }
        printf("Response (%zu bytes):\n", response_len);
        if (g_starlink_grpc_config.hex_mode) {
            starlink_grpc_print_hex_data(response_data, response_len);
        } else {
            printf("%.*s\n", (int)response_len, response_data);
        }
        printf("==================\n");
    }
}

void starlink_grpc_log_to_file(const unsigned char *data, size_t len, const char *log_file) {
    if (log_file) {
        FILE *log_fp = fopen(log_file, "a");
        if (log_fp) {
            starlink_grpc_print_timestamp();
            fprintf(log_fp, "%.*s\n", (int)len, data);
            fclose(log_fp);
        }
    }
}

void starlink_grpc_print_summary(const char *json_data) {
    if (g_starlink_grpc_config.summary_mode) {
        // Extract key metrics from JSON response
        printf("=== SUMMARY ===\n");
        
        // Look for common fields
        if (strstr(json_data, "getDeviceInfo")) {
            printf("Device Info: Available\n");
        }
        if (strstr(json_data, "dishGetStatus")) {
            printf("Dish Status: Available\n");
        }
        if (strstr(json_data, "softwareVersion")) {
            const char *start = strstr(json_data, "softwareVersion");
            if (start) {
                start = strchr(start, '"');
                if (start) {
                    start++;
                    const char *end = strchr(start, '"');
                    if (end) {
                        printf("Software Version: %.*s\n", (int)(end - start), start);
                    }
                }
            }
        }
        if (strstr(json_data, "hardwareVersion")) {
            const char *start = strstr(json_data, "hardwareVersion");
            if (start) {
                start = strchr(start, '"');
                if (start) {
                    start++;
                    const char *end = strchr(start, '"');
                    if (end) {
                        printf("Hardware Version: %.*s\n", (int)(end - start), start);
                    }
                }
            }
        }
        printf("===============\n");
    }
}

void starlink_grpc_print_compact_json(const char *json_data) {
    if (g_starlink_grpc_config.compact_mode) {
        // Remove all whitespace except inside strings
        const char *p = json_data;
        while (*p) {
            if (*p == '"') {
                putchar(*p++);
                while (*p && *p != '"') {
                    putchar(*p++);
                    if (*p == '\\' && *(p+1)) {
                        putchar(*p++);
                        putchar(*p++);
                    }
                }
                if (*p) putchar(*p++);
            } else if (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') {
                p++;
            } else {
                putchar(*p++);
            }
        }
    } else {
        printf("%s", json_data);
    }
}

void starlink_grpc_print_pretty_json(const char *json_data) {
    if (g_starlink_grpc_config.pretty_mode) {
        int indent = 0;
        const char *p = json_data;
        while (*p) {
            if (*p == '{' || *p == '[') {
                putchar(*p);
                putchar('\n');
                indent++;
                for (int i = 0; i < indent; i++) printf("  ");
                p++;
            } else if (*p == '}' || *p == ']') {
                putchar('\n');
                indent--;
                for (int i = 0; i < indent; i++) printf("  ");
                putchar(*p);
                p++;
            } else if (*p == ',') {
                putchar(*p);
                putchar('\n');
                for (int i = 0; i < indent; i++) printf("  ");
                p++;
            } else if (*p == ':') {
                putchar(*p);
                putchar(' ');
                p++;
            } else {
                putchar(*p);
                p++;
            }
        }
    } else {
        starlink_grpc_print_compact_json(json_data);
    }
}

void starlink_grpc_handle_access_denied(const char *method, const unsigned char *response) {
    if (strstr((const char*)response, "Access Denied") || 
        strstr((const char*)response, "access denied") || 
        strstr((const char*)response, "403")) {
        if (!g_starlink_grpc_config.silent_mode) {
            starlink_grpc_print_timestamp();
            printf("  Access Denied for method '%s' - This endpoint may be restricted\n", method);
        }
    }
}

void starlink_grpc_print_formatted_output(
    const char *json_data, 
    const unsigned char *raw_data, 
    size_t raw_len,
    const starlink_grpc_client_config_t *config
) {
    if (config->raw_mode) {
        // Raw mode: print actual binary data
        fwrite(raw_data, 1, raw_len, stdout);
    } else if (config->hex_mode) {
        // Hex mode: print hex representation
        starlink_grpc_print_hex_data(raw_data, raw_len);
    } else if (config->summary_mode) {
        starlink_grpc_print_summary(json_data);
    } else {
        starlink_grpc_print_pretty_json(json_data);
    }
    
    // Add newline for better formatting (except in raw mode where we want exact binary output)
    if (!config->raw_mode && !config->silent_mode) {
        printf("\n");
    }
}

// Write callback for curl
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    starlink_grpc_response_t *response = (starlink_grpc_response_t *)userp;
    
    // Reallocate buffer if needed
    if (response->response_data == NULL) {
        response->response_data = (char*)malloc(realsize + 1);
    } else {
        response->response_data = (char*)realloc(response->response_data, 
                                        response->response_size + realsize + 1);
    }
    
    if (response->response_data == NULL) {
        return 0; // Out of memory
    }
    
    // Append new data
    memcpy(&(response->response_data[response->response_size]), contents, realsize);
    response->response_size += realsize;
    response->response_data[response->response_size] = '\0';
    
    return realsize;
}

// Make gRPC call with comprehensive options
int starlink_grpc_comprehensive_call(
    const char *method,
    const char *request_data,
    size_t request_size,
    starlink_grpc_response_t *response
) {
    if (!method || !response) {
        return -1;
    }
    
    // Initialize response
    memset(response, 0, sizeof(starlink_grpc_response_t));
    response->timestamp = time(NULL);
    
    // Build URL
    char url[512];
    snprintf(url, sizeof(url), "http://%s:%d/SpaceX.API.Device.Device/Handle", 
             g_starlink_grpc_config.host, g_starlink_grpc_config.port);
    
    // Create gRPC frame (simplified - in real implementation you'd use proper gRPC framing)
    unsigned char frame[1024];
    size_t frame_len = 0;
    
    // Add gRPC header (standard: 1 byte compression flag + 4 bytes message length)
    frame[frame_len++] = 0x00; // Compression flag (no compression)
    
    // Add message length (big-endian)
    uint32_t msg_len = htonl(request_size);
    memcpy(&frame[frame_len], &msg_len, 4);
    frame_len += 4;
    
    // Add request data
    if (request_data && request_size > 0) {
        memcpy(&frame[frame_len], request_data, request_size);
        frame_len += request_size;
    }
    
    // Debug information will be printed after response
    
    // Setup curl
    CURL *curl = curl_easy_init();
    if (!curl) {
        strcpy(response->error_message, "Failed to initialize curl");
        return -1;
    }
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/grpc");
    headers = curl_slist_append(headers, "grpc-encoding: identity");
    headers = curl_slist_append(headers, "grpc-accept-encoding: identity");
    headers = curl_slist_append(headers, "TE: trailers");
    
    if (g_starlink_grpc_config.user_agent) {
        char user_agent_header[512];
        snprintf(user_agent_header, sizeof(user_agent_header), "User-Agent: %s", 
                 g_starlink_grpc_config.user_agent);
        headers = curl_slist_append(headers, user_agent_header);
    } else {
        headers = curl_slist_append(headers, "User-Agent: starlink-comprehensive-client/1.0");
    }
    
    // Configure curl
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, frame);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)frame_len);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)g_starlink_grpc_config.timeout);
    
    if (g_starlink_grpc_config.insecure_mode) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    
    // Perform request
    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    response->http_status = (int)http_code;
    
    // Cleanup curl
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        snprintf(response->error_message, sizeof(response->error_message), 
                "curl error: %s", curl_easy_strerror(res));
        return -1;
    }
    
    // Print header with new utility function
    char status_line[64];
    snprintf(status_line, sizeof(status_line), "HTTP %d", response->http_status);
    starlink_grpc_print_header(status_line, response->response_size);
    
    // Log to file if requested
    if (g_starlink_grpc_config.log_file) {
        starlink_grpc_log_to_file((const unsigned char*)response->response_data, 
                                 response->response_size, 
                                 g_starlink_grpc_config.log_file);
    }
    
    // Handle access denied responses
    starlink_grpc_handle_access_denied(method, (const unsigned char*)response->response_data);
    
    // Add debug information for response if requested
    if (g_starlink_grpc_config.debug_mode) {
        starlink_grpc_print_debug_info(url, method, frame, frame_len, 
                                      (const unsigned char*)response->response_data, 
                                      response->response_size);
    }
    
    response->success = (http_code >= 200 && http_code < 300);
    
    return 0;
}

// Cleanup
void starlink_grpc_comprehensive_client_cleanup(void) {
    if (g_starlink_grpc_config.user_agent) {
        free(g_starlink_grpc_config.user_agent);
        g_starlink_grpc_config.user_agent = NULL;
    }
    if (g_starlink_grpc_config.fields_filter) {
        free(g_starlink_grpc_config.fields_filter);
        g_starlink_grpc_config.fields_filter = NULL;
    }
    if (g_starlink_grpc_config.log_file) {
        free(g_starlink_grpc_config.log_file);
        g_starlink_grpc_config.log_file = NULL;
    }
    if (g_starlink_grpc_config.batch_file) {
        free(g_starlink_grpc_config.batch_file);
        g_starlink_grpc_config.batch_file = NULL;
    }
    if (g_starlink_grpc_config.export_format) {
        free(g_starlink_grpc_config.export_format);
        g_starlink_grpc_config.export_format = NULL;
    }
    if (g_starlink_grpc_config.previous_response) {
        free(g_starlink_grpc_config.previous_response);
        g_starlink_grpc_config.previous_response = NULL;
    }
    
    curl_global_cleanup();
    LOGX_INFO_MSG("Starlink gRPC comprehensive client cleaned up");
}
