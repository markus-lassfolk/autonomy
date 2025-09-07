#include "http_client.h"
#include "logx.h"
#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

// Global state
static bool g_http_client_initialized = false;
static pthread_mutex_t g_http_client_mutex = PTHREAD_MUTEX_INITIALIZER;

// CURL write callback function
static size_t http_write_callback(void* contents, size_t size, size_t nmemb, http_response_t* response) {
    size_t total_size = size * nmemb;
    
    char* new_data = realloc(response->data, response->size + total_size + 1);
    if (!new_data) {
        LOGX_ERROR_MSG("Failed to allocate memory for HTTP response");
        return 0;
    }
    
    response->data = new_data;
    memcpy(&(response->data[response->size]), contents, total_size);
    response->size += total_size;
    response->data[response->size] = '\0';
    
    return total_size;
}

// Initialize HTTP client
int http_client_init(void) {
    pthread_mutex_lock(&g_http_client_mutex);
    
    if (g_http_client_initialized) {
        pthread_mutex_unlock(&g_http_client_mutex);
        return 0; // Already initialized
    }
    
    CURLcode curl_result = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (curl_result != CURLE_OK) {
        LOGX_ERROR_MSG("Failed to initialize CURL", "error", curl_easy_strerror(curl_result));
        pthread_mutex_unlock(&g_http_client_mutex);
        return -1;
    }
    
    g_http_client_initialized = true;
    pthread_mutex_unlock(&g_http_client_mutex);
    
    LOGX_INFO_MSG("HTTP client initialized successfully");
    return 0;
}

// Cleanup HTTP client
void http_client_cleanup(void) {
    pthread_mutex_lock(&g_http_client_mutex);
    
    if (g_http_client_initialized) {
        curl_global_cleanup();
        g_http_client_initialized = false;
        LOGX_INFO_MSG("HTTP client cleaned up");
    }
    
    pthread_mutex_unlock(&g_http_client_mutex);
}

// Make HTTP request
int http_client_make_request(const http_request_config_t* config, http_response_t* response) {
    if (!config || !response) {
        return -1;
    }
    
    if (!g_http_client_initialized) {
        LOGX_ERROR_MSG("HTTP client not initialized");
        return -1;
    }
    
    // Initialize response
    memset(response, 0, sizeof(http_response_t));
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        LOGX_ERROR_MSG("Failed to initialize CURL");
        return -1;
    }
    
    // Set CURL options
    curl_easy_setopt(curl, CURLOPT_URL, config->url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, config->timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, config->user_agent ? config->user_agent : "Autonomy-Daemon/6.1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, config->follow_redirects ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, config->verify_ssl ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, config->verify_ssl ? 2L : 0L);
    
    // Set POST data if provided
    if (config->post_data) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, config->post_data);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(config->post_data));
        
        struct curl_slist* headers = NULL;
        if (config->content_type) {
            headers = curl_slist_append(headers, config->content_type);
        } else {
            headers = curl_slist_append(headers, "Content-Type: application/json");
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
    
    // Perform request
    CURLcode curl_result = curl_easy_perform(curl);
    
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->response_code);
    
    curl_easy_cleanup(curl);
    
    if (curl_result != CURLE_OK) {
        LOGX_ERROR_MSG("CURL request failed", "error", curl_easy_strerror(curl_result));
        response->success = false;
        return -1;
    }
    
    if (response->response_code < 200 || response->response_code >= 300) {
        LOGX_ERROR_MSG("HTTP request failed", "response_code", response->response_code);
        response->success = false;
        return -1;
    }
    
    response->success = true;
    LOGX_DEBUG_MSG("HTTP request successful", "response_size", response->size, "response_code", response->response_code);
    return 0;
}

// Cleanup HTTP response
void http_response_cleanup(http_response_t* response) {
    if (response && response->data) {
        free(response->data);
        response->data = NULL;
        response->size = 0;
    }
}

// Convenience function for GET requests
int http_client_get(const char* url, int timeout_seconds, http_response_t* response) {
    http_request_config_t config = {
        .url = url,
        .post_data = NULL,
        .content_type = NULL,
        .timeout_seconds = timeout_seconds,
        .user_agent = "Autonomy-Daemon/6.1.0",
        .follow_redirects = true,
        .verify_ssl = true
    };
    
    return http_client_make_request(&config, response);
}

// Convenience function for POST JSON requests
int http_client_post_json(const char* url, const char* json_data, int timeout_seconds, http_response_t* response) {
    http_request_config_t config = {
        .url = url,
        .post_data = json_data,
        .content_type = "Content-Type: application/json",
        .timeout_seconds = timeout_seconds,
        .user_agent = "Autonomy-Daemon/6.1.0",
        .follow_redirects = true,
        .verify_ssl = true
    };
    
    return http_client_make_request(&config, response);
}

