#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stddef.h>
#include <stdbool.h>

// HTTP response structure
typedef struct {
    char* data;
    size_t size;
    long response_code;
    bool success;
} http_response_t;

// HTTP request configuration
typedef struct {
    const char* url;
    const char* post_data;
    const char* content_type;
    int timeout_seconds;
    const char* user_agent;
    bool follow_redirects;
    bool verify_ssl;
} http_request_config_t;

// HTTP client functions
int http_client_init(void);
void http_client_cleanup(void);
int http_client_make_request(const http_request_config_t* config, http_response_t* response);
void http_response_cleanup(http_response_t* response);

// Convenience functions
int http_client_get(const char* url, int timeout_seconds, http_response_t* response);
int http_client_post_json(const char* url, const char* json_data, int timeout_seconds, http_response_t* response);

#endif // HTTP_CLIENT_H

