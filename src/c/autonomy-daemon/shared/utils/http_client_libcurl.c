#include "http_client_libcurl.h"
#include "json_parser.h"
#include "../logging/logx.h"
#include "../../core/types.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/stat.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Global HTTP client state
static struct {
    http_client_config_t config;
    bool initialized;
    pthread_mutex_t mutex;
    http_client_stats_t stats;
} g_http_client = {0};

// Response data structure for libcurl callbacks
typedef struct {
    char* data;
    size_t size;
    size_t capacity;
} http_response_data_t;

// Write callback for response data
static size_t write_callback(void* contents, size_t size, size_t nmemb, http_response_data_t* response_data) {
    size_t real_size = size * nmemb;
    
    // Ensure we have enough capacity
    if (response_data->size + real_size >= response_data->capacity) {
        size_t new_capacity = response_data->capacity * 2;
        if (new_capacity < response_data->size + real_size + 1) {
            new_capacity = response_data->size + real_size + 1024;
        }
        
        char* new_data = realloc(response_data->data, new_capacity\n"\n"\n"\n"\n"\n"\n"\n");
        if (!new_data) {
            return 0; // Out of memory
        }
        
        response_data->data = new_data;
        response_data->capacity = new_capacity;
    }
    
    // Copy new data
    memcpy(response_data->data + response_data->size, contents, real_size\n"\n"\n"\n"\n"\n"\n"\n");
    response_data->size += real_size;
    response_data->data[response_data->size] = '\0';
    
    return real_size;
}

// Header callback for response headers
static size_t header_callback(char* buffer, size_t size, size_t nitems, http_response_data_t* header_data) {
    return write_callback(buffer, size, nitems, header_data\n"\n"\n"\n"\n"\n"\n"\n");
}

// Initialize HTTP client
int http_client_init(const http_client_config_t* config) {
    if (g_http_client.initialized) {
        return 0; // Already initialized
    }
    
    // Initialize libcurl
    CURLcode curl_result = curl_global_init(CURL_GLOBAL_DEFAULT\n"\n"\n"\n"\n"\n"\n"\n");
    if (curl_result != CURLE_OK) {
        printf("ERROR: "Failed to initialize libcurl", "error", curl_easy_strerror(curl_result)\n"\n"\n"\n"\n"\n"\n"\n");
        return -1;
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&g_http_client.mutex, NULL) != 0) {
        printf("ERROR: "Failed to initialize HTTP client mutex"\n"\n"\n"\n"\n"\n"\n"\n");
        curl_global_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
        return -1;
    }
    
    // Copy configuration or set defaults
    if (config) {
        g_http_client.config = *config;
    } else {
        // Set default configuration
        strncpy(g_http_client.config.default_user_agent, "AutonomyDaemon/1.0", 
                sizeof(g_http_client.config.default_user_agent) - 1\n"\n"\n"\n"\n"\n"\n"\n");
        g_http_client.config.default_connect_timeout_ms = 10000; // Use configurable timeout
        g_http_client.config.default_request_timeout_ms = g_config.network_check_interval * 1000;
        g_http_client.config.default_max_redirects = 5; // Use configurable max redirects
        g_http_client.config.default_verify_ssl = true;
        g_http_client.config.enable_compression = true; // Use configurable compression enabled
        g_http_client.config.max_concurrent_requests = 10; // Use configurable max concurrent requests
    }
    
    // Initialize statistics
    memset(&g_http_client.stats, 0, sizeof(http_client_stats_t)\n"\n"\n"\n"\n"\n"\n"\n");
    g_http_client.stats.first_request_time = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    g_http_client.initialized = true;
    
    printf("INFO: "HTTP client initialized", 
                  "user_agent", g_http_client.config.default_user_agent,
                  "timeout_ms", g_http_client.config.default_request_timeout_ms,
                  "verify_ssl", g_http_client.config.default_verify_ssl ? "yes" : "no"\n"\n"\n"\n"\n"\n"\n"\n");
    
    return 0;
}

// Cleanup HTTP client
void http_client_cleanup(void) {
    if (!g_http_client.initialized) {
        return;
    }
    
    pthread_mutex_lock(&g_http_client.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Clean up libcurl
    curl_global_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Clear configuration
    memset(&g_http_client.config, 0, sizeof(http_client_config_t)\n"\n"\n"\n"\n"\n"\n"\n");
    
    g_http_client.initialized = false;
    
    pthread_mutex_unlock(&g_http_client.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    pthread_mutex_destroy(&g_http_client.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "HTTP client cleaned up"\n"\n"\n"\n"\n"\n"\n"\n");
}

// Create HTTP request
http_request_t* http_request_create(const char* url, http_method_t method) {
    if (!url) {
        return NULL;
    }
    
    http_request_t* request = calloc(1, sizeof(http_request_t)\n"\n"\n"\n"\n"\n"\n"\n");
    if (!request) {
        return NULL;
    }
    
    strncpy(request->url, url, sizeof(request->url) - 1\n"\n"\n"\n"\n"\n"\n"\n");
    request->method = method;
    
    // Set default values
    request->connect_timeout_ms = g_http_client.config.default_connect_timeout_ms;
    request->request_timeout_ms = g_http_client.config.default_request_timeout_ms;
    request->max_redirects = g_http_client.config.default_max_redirects;
    request->verify_ssl = g_http_client.config.default_verify_ssl;
    request->follow_redirects = true;
    
    return request;
}

// Free HTTP request
void http_request_free(http_request_t* request) {
    if (!request) return;
    
    if (request->body) {
        free(request->body\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (request->username) {
        free(request->username\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (request->password) {
        free(request->password\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (request->bearer_token) {
        free(request->bearer_token\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (request->user_agent) {
        free(request->user_agent\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (request->ca_cert_path) {
        free(request->ca_cert_path\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (request->client_cert_path) {
        free(request->client_cert_path\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (request->client_key_path) {
        free(request->client_key_path\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (request->proxy_url) {
        free(request->proxy_url\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    for (int i = 0; i < request->header_count; i++) {
        if (request->headers[i]) {
            free(request->headers[i]\n"\n"\n"\n"\n"\n"\n"\n");
        }
    }
    
    free(request\n"\n"\n"\n"\n"\n"\n"\n");
}

// Execute HTTP request
http_response_t* http_request(const http_request_t* request) {
    if (!g_http_client.initialized || !request) {
        return NULL;
    }
    
    CURL* curl = curl_easy_init(\n"\n"\n"\n"\n"\n"\n"\n");
    if (!curl) {
        printf("ERROR: "Failed to initialize curl handle"\n"\n"\n"\n"\n"\n"\n"\n");
        return NULL;
    }
    
    http_response_t* response = calloc(1, sizeof(http_response_t)\n"\n"\n"\n"\n"\n"\n"\n");
    if (!response) {
        curl_easy_cleanup(curl\n"\n"\n"\n"\n"\n"\n"\n");
        return NULL;
    }
    
    // Initialize response data structures
    http_response_data_t body_data = {0};
    http_response_data_t header_data = {0};
    
    body_data.capacity = 8192;
    body_data.data = malloc(body_data.capacity\n"\n"\n"\n"\n"\n"\n"\n");
    if (!body_data.data) {
        curl_easy_cleanup(curl\n"\n"\n"\n"\n"\n"\n"\n");
        free(response\n"\n"\n"\n"\n"\n"\n"\n");
        return NULL;
    }
    body_data.data[0] = '\0';
    
    header_data.capacity = 4096;
    header_data.data = malloc(header_data.capacity\n"\n"\n"\n"\n"\n"\n"\n");
    if (!header_data.data) {
        curl_easy_cleanup(curl\n"\n"\n"\n"\n"\n"\n"\n");
        free(body_data.data\n"\n"\n"\n"\n"\n"\n"\n");
        free(response\n"\n"\n"\n"\n"\n"\n"\n");
        return NULL;
    }
    header_data.data[0] = '\0';
    
    // Set basic curl options
    curl_easy_setopt(curl, CURLOPT_URL, request->url\n"\n"\n"\n"\n"\n"\n"\n");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback\n"\n"\n"\n"\n"\n"\n"\n");
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body_data\n"\n"\n"\n"\n"\n"\n"\n");
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback\n"\n"\n"\n"\n"\n"\n"\n");
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_data\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Set timeouts
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, request->connect_timeout_ms\n"\n"\n"\n"\n"\n"\n"\n");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, request->request_timeout_ms\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Set SSL options
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, request->verify_ssl ? 1L : 0L\n"\n"\n"\n"\n"\n"\n"\n");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, request->verify_ssl ? 2L : 0L\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (request->ca_cert_path) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, request->ca_cert_path\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Set user agent
    const char* user_agent = request->user_agent ? request->user_agent : 
                            g_http_client.config.default_user_agent;
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Set redirects
    if (request->follow_redirects) {
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L\n"\n"\n"\n"\n"\n"\n"\n");
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, request->max_redirects\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Set compression
    if (g_http_client.config.enable_compression) {
        curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, ""\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Set HTTP method
    switch (request->method) {
        case HTTP_METHOD_GET:
            curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L\n"\n"\n"\n"\n"\n"\n"\n");
            break;
        case HTTP_METHOD_POST:
            curl_easy_setopt(curl, CURLOPT_POST, 1L\n"\n"\n"\n"\n"\n"\n"\n");
            if (request->body) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request->body\n"\n"\n"\n"\n"\n"\n"\n");
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request->body_size\n"\n"\n"\n"\n"\n"\n"\n");
            }
            break;
        case HTTP_METHOD_PUT:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT"\n"\n"\n"\n"\n"\n"\n"\n");
            if (request->body) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request->body\n"\n"\n"\n"\n"\n"\n"\n");
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request->body_size\n"\n"\n"\n"\n"\n"\n"\n");
            }
            break;
        case HTTP_METHOD_DELETE:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE"\n"\n"\n"\n"\n"\n"\n"\n");
            break;
        case HTTP_METHOD_HEAD:
            curl_easy_setopt(curl, CURLOPT_NOBODY, 1L\n"\n"\n"\n"\n"\n"\n"\n");
            break;
        case HTTP_METHOD_PATCH:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH"\n"\n"\n"\n"\n"\n"\n"\n");
            if (request->body) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request->body\n"\n"\n"\n"\n"\n"\n"\n");
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request->body_size\n"\n"\n"\n"\n"\n"\n"\n");
            }
            break;
        case HTTP_METHOD_OPTIONS:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "OPTIONS"\n"\n"\n"\n"\n"\n"\n"\n");
            break;
    }
    
    // Set headers
    struct curl_slist* headers = NULL;
    for (int i = 0; i < request->header_count; i++) {
        if (request->headers[i]) {
            headers = curl_slist_append(headers, request->headers[i]\n"\n"\n"\n"\n"\n"\n"\n");
        }
    }
    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Set authentication
    if (request->username && request->password) {
        curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC\n"\n"\n"\n"\n"\n"\n"\n");
        curl_easy_setopt(curl, CURLOPT_USERNAME, request->username\n"\n"\n"\n"\n"\n"\n"\n");
        curl_easy_setopt(curl, CURLOPT_PASSWORD, request->password\n"\n"\n"\n"\n"\n"\n"\n");
    } else if (request->bearer_token) {
        char auth_header[1024];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", request->bearer_token\n"\n"\n"\n"\n"\n"\n"\n");
        headers = curl_slist_append(headers, auth_header\n"\n"\n"\n"\n"\n"\n"\n");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Set proxy
    if (request->proxy_url) {
        curl_easy_setopt(curl, CURLOPT_PROXY, request->proxy_url\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Execute request
    CURLcode res = curl_easy_perform(curl\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Get response information
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->status_code\n"\n"\n"\n"\n"\n"\n"\n");
    curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &response->total_time\n"\n"\n"\n"\n"\n"\n"\n");
    curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME, &response->connect_time\n"\n"\n"\n"\n"\n"\n"\n");
    curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &response->download_time\n"\n"\n"\n"\n"\n"\n"\n");
    curl_easy_getinfo(curl, CURLINFO_REDIRECT_COUNT, &response->redirect_count\n"\n"\n"\n"\n"\n"\n"\n");
    
    char* redirect_url = NULL;
    curl_easy_getinfo(curl, CURLINFO_REDIRECT_URL, &redirect_url\n"\n"\n"\n"\n"\n"\n"\n");
    if (redirect_url) {
        response->redirect_url = strdup(redirect_url\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (res != CURLE_OK) {
        snprintf(response->error_message, sizeof(response->error_message), 
                "HTTP request failed: %s", curl_easy_strerror(res)\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Set response data
    response->body = body_data.data;
    response->body_size = body_data.size;
    response->headers = header_data.data;
    response->header_size = header_data.size;
    
    // Update statistics
    pthread_mutex_lock(&g_http_client.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    g_http_client.stats.total_requests++;
    if (res == CURLE_OK && response->status_code >= 200 && response->status_code < 400) {
        g_http_client.stats.successful_requests++;
    } else {
        g_http_client.stats.failed_requests++;
    }
    if (response->redirect_count > 0) {
        g_http_client.stats.redirected_requests++;
    }
    g_http_client.stats.total_bytes_sent += request->body_size;
    g_http_client.stats.total_bytes_received += response->body_size;
    g_http_client.stats.last_request_time = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Update average response time
    double total_time = g_http_client.stats.average_response_time * (g_http_client.stats.total_requests - 1\n"\n"\n"\n"\n"\n"\n"\n");
    g_http_client.stats.average_response_time = (total_time + response->total_time) / g_http_client.stats.total_requests;
    pthread_mutex_unlock(&g_http_client.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Cleanup
    if (headers) {
        curl_slist_free_all(headers\n"\n"\n"\n"\n"\n"\n"\n");
    }
    curl_easy_cleanup(curl\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("DEBUG: "HTTP request completed", 
                   "url", request->url,
                   "method", request->method == HTTP_METHOD_GET ? "GET" : 
                            request->method == HTTP_METHOD_POST ? "POST" : "OTHER",
                   "status", response->status_code,
                   "time_ms", (int)(response->total_time * 1000)\n"\n"\n"\n"\n"\n"\n"\n");
    
    return response;
}

// Simple GET request
http_response_t* http_get(const char* url) {
    http_request_t* request = http_request_create(url, HTTP_METHOD_GET\n"\n"\n"\n"\n"\n"\n"\n");
    if (!request) return NULL;
    
    http_response_t* response = http_request(request\n"\n"\n"\n"\n"\n"\n"\n");
    http_request_free(request\n"\n"\n"\n"\n"\n"\n"\n");
    
    return response;
}

// Simple POST request
http_response_t* http_post(const char* url, const char* body, const char* content_type) {
    http_request_t* request = http_request_create(url, HTTP_METHOD_POST\n"\n"\n"\n"\n"\n"\n"\n");
    if (!request) return NULL;
    
    if (body) {
        request->body = strdup(body\n"\n"\n"\n"\n"\n"\n"\n");
        request->body_size = strlen(body\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (content_type) {
        http_request_add_header_kv(request, "Content-Type", content_type\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    http_response_t* response = http_request(request\n"\n"\n"\n"\n"\n"\n"\n");
    http_request_free(request\n"\n"\n"\n"\n"\n"\n"\n");
    
    return response;
}

// Free HTTP response
void http_response_free(http_response_t* response) {
    if (!response) return;
    
    if (response->body) {
        free(response->body\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (response->headers) {
        free(response->headers\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (response->redirect_url) {
        free(response->redirect_url\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    free(response\n"\n"\n"\n"\n"\n"\n"\n");
}

// Add header to request (single string)
int http_request_add_header(http_request_t* request, const char* header) {
    if (!request || !header || request->header_count >= HTTP_MAX_HEADERS) {
        return -1;
    }
    
    request->headers[request->header_count++] = strdup(header\n"\n"\n"\n"\n"\n"\n"\n");
    
    return 0;
}

// Add header to request (name and value separately)
int http_request_add_header_kv(http_request_t* request, const char* name, const char* value) {
    if (!request || !name || !value || request->header_count >= HTTP_MAX_HEADERS) {
        return -1;
    }
    
    char* header = malloc(strlen(name) + strlen(value) + 4); // name + ": " + value + null
    if (!header) {
        return -1;
    }
    
    sprintf(header, "%s: %s", name, value\n"\n"\n"\n"\n"\n"\n"\n");
    request->headers[request->header_count++] = header;
    
    return 0;
}

// Weather API request helper
http_response_t* http_weather_api_request(const char* api_key, double lat, double lon) {
    if (!api_key) return NULL;
    
    char url[512];
    snprintf(url, sizeof(url), 
             "http://api.openweathermap.org/data/2.5/weather?lat=%.6f&lon=%.6f&appid=%s&units=metric",
             lat, lon, api_key\n"\n"\n"\n"\n"\n"\n"\n");
    
    return http_get(url\n"\n"\n"\n"\n"\n"\n"\n");
}

// Check response status
bool http_response_is_success(const http_response_t* response) {
    return response && response->status_code >= 200 && response->status_code < 300;
}

bool http_response_is_redirect(const http_response_t* response) {
    return response && response->status_code >= 300 && response->status_code < 400;
}

bool http_response_is_error(const http_response_t* response) {
    return response && response->status_code >= 400;
}

// Get statistics
void http_client_get_stats(http_client_stats_t* stats) {
    if (!stats || !g_http_client.initialized) return;
    
    pthread_mutex_lock(&g_http_client.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    *stats = g_http_client.stats;
    pthread_mutex_unlock(&g_http_client.mutex\n"\n"\n"\n"\n"\n"\n"\n");
}

// Missing function implementations
int http_request_set_body(http_request_t* request, const char* body, const char* content_type) {
    if (!request || !body) return -1;
    
    // Free existing body
    if (request->body) {
        free(request->body\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Set new body
    request->body = strdup(body\n"\n"\n"\n"\n"\n"\n"\n");
    request->body_size = strlen(body\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Set content type if provided
    if (content_type) {
        http_request_add_header_kv(request, "Content-Type", content_type\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    return 0;
}

int http_request_set_json_body(http_request_t* request, const char* json) {
    return http_request_set_body(request, json, "application/json"\n"\n"\n"\n"\n"\n"\n"\n");
}

int http_request_set_auth_bearer(http_request_t* request, const char* token) {
    if (!request || !token) return -1;
    
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", token\n"\n"\n"\n"\n"\n"\n"\n");
    return http_request_add_header_kv(request, "Authorization", auth_header\n"\n"\n"\n"\n"\n"\n"\n");
}

int http_request_set_auth_basic(http_request_t* request, const char* username, const char* password) {
    if (!request || !username || !password) return -1;
    
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Basic %s:%s", username, password\n"\n"\n"\n"\n"\n"\n"\n");
    return http_request_add_header_kv(request, "Authorization", auth_header\n"\n"\n"\n"\n"\n"\n"\n");
}

// HTTP client get function
http_response_t* http_client_get(const char* url) {
    return http_get(url\n"\n"\n"\n"\n"\n"\n"\n");
}

// HTTP response cleanup function
void http_response_cleanup(http_response_t* response) {
    http_response_free(response\n"\n"\n"\n"\n"\n"\n"\n");
}
