#ifndef HTTP_CLIENT_LIBCURL_H
#define HTTP_CLIENT_LIBCURL_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

// HTTP client using libcurl for production use
// Replaces simplified HTTP implementations

// Maximum sizes
#define HTTP_MAX_URL_LENGTH 2048
#define HTTP_MAX_HEADER_LENGTH 1024
#define HTTP_MAX_BODY_LENGTH 65536
#define HTTP_MAX_HEADERS 32
#define HTTP_MAX_USER_AGENT_LENGTH 256

// HTTP methods
typedef enum {
    HTTP_METHOD_GET,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT,
    HTTP_METHOD_DELETE,
    HTTP_METHOD_HEAD,
    HTTP_METHOD_PATCH,
    HTTP_METHOD_OPTIONS
} http_method_t;

// HTTP response
typedef struct {
    long status_code;
    char* headers;
    char* body;
    size_t body_size;
    size_t header_size;
    double total_time;
    double connect_time;
    double download_time;
    long redirect_count;
    char* redirect_url;
    char error_message[256];
} http_response_t;

// HTTP request configuration
typedef struct {
    char url[HTTP_MAX_URL_LENGTH];
    http_method_t method;
    char* body;
    size_t body_size;
    char* headers[HTTP_MAX_HEADERS];
    int header_count;
    
    // Timeouts and limits
    long connect_timeout_ms;
    long request_timeout_ms;
    long max_redirects;
    
    // Authentication
    char* username;
    char* password;
    char* bearer_token;
    
    // SSL/TLS options
    bool verify_ssl;
    char* ca_cert_path;
    char* client_cert_path;
    char* client_key_path;
    
    // Other options
    char* user_agent;
    bool follow_redirects;
    bool include_headers;
    char* proxy_url;
} http_request_t;

// HTTP client configuration
typedef struct {
    char default_user_agent[HTTP_MAX_USER_AGENT_LENGTH];
    long default_connect_timeout_ms;
    long default_request_timeout_ms;
    long default_max_redirects;
    bool default_verify_ssl;
    bool enable_cookies;
    char* cookie_jar_path;
    bool enable_compression;
    int max_concurrent_requests;
} http_client_config_t;

// Initialize HTTP client
int http_client_init(const http_client_config_t* config);
void http_client_cleanup(void);

// Request functions
http_response_t* http_request(const http_request_t* request);
http_response_t* http_get(const char* url);
http_response_t* http_post(const char* url, const char* body, const char* content_type);
http_response_t* http_put(const char* url, const char* body, const char* content_type);
http_response_t* http_delete(const char* url);

// Response management
void http_response_free(http_response_t* response);
bool http_response_is_success(const http_response_t* response);
bool http_response_is_redirect(const http_response_t* response);
bool http_response_is_error(const http_response_t* response);

// Header utilities
int http_request_add_header(http_request_t* request, const char* header);
int http_request_add_header_kv(http_request_t* request, const char* name, const char* value);
int http_request_set_auth_basic(http_request_t* request, const char* username, const char* password);
int http_request_set_auth_bearer(http_request_t* request, const char* token);
int http_request_set_content_type(http_request_t* request, const char* content_type);

// Request builders
http_request_t* http_request_create(const char* url, http_method_t method);
void http_request_free(http_request_t* request);
int http_request_set_body(http_request_t* request, const char* body, const char* content_type);
int http_request_set_json_body(http_request_t* request, const char* json);
int http_request_set_form_body(http_request_t* request, const char* form_data);

// URL utilities
char* http_url_encode(const char* input);
char* http_url_decode(const char* input);
char* http_build_query_string(const char** keys, const char** values, int count);

// Response parsing
char* http_response_get_header(const http_response_t* response, const char* header_name);
bool http_response_has_header(const http_response_t* response, const char* header_name);
char* http_response_get_body_as_string(const http_response_t* response);

// JSON response helpers
bool http_response_is_json(const http_response_t* response);
struct json_document_t* http_response_parse_json(const http_response_t* response);

// File upload/download
http_response_t* http_upload_file(const char* url, const char* file_path, const char* field_name);
int http_download_file(const char* url, const char* file_path);

// Batch requests
typedef struct {
    http_request_t** requests;
    int request_count;
    http_response_t** responses;
    bool completed;
    time_t start_time;
    time_t end_time;
} http_batch_t;

http_batch_t* http_batch_create(void);
int http_batch_add_request(http_batch_t* batch, http_request_t* request);
int http_batch_execute(http_batch_t* batch);
void http_batch_free(http_batch_t* batch);

// Statistics
typedef struct {
    unsigned long total_requests;
    unsigned long successful_requests;
    unsigned long failed_requests;
    unsigned long redirected_requests;
    double average_response_time;
    double total_bytes_sent;
    double total_bytes_received;
    time_t first_request_time;
    time_t last_request_time;
} http_client_stats_t;

void http_client_get_stats(http_client_stats_t* stats);
void http_client_reset_stats(void);

// Error handling
const char* http_get_error_string(long response_code);
bool http_is_network_error(long response_code);
bool http_is_server_error(long response_code);

// Common patterns for APIs
http_response_t* http_api_get(const char* base_url, const char* endpoint, const char* api_key);
http_response_t* http_api_post_json(const char* base_url, const char* endpoint, const char* json_body, const char* api_key);

// Specialized functions for common use cases
http_response_t* http_weather_api_request(const char* api_key, double lat, double lon);
http_response_t* http_geocoding_request(const char* api_key, const char* address);
http_response_t* http_starlink_api_request(const char* method);

#endif // HTTP_CLIENT_LIBCURL_H
