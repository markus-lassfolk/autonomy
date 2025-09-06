#ifndef EXTERNAL_API_CLIENT_H
#define EXTERNAL_API_CLIENT_H

#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <math.h>
#include <sys/socket.h>

// API endpoint configuration
typedef struct {
    char base_url[256];
    char api_key[128];
    char username[128];
    char password[128];
    int timeout_seconds;
    bool use_ssl;
    char ca_cert_path[256];
} api_endpoint_config_t;

// API request
typedef struct {
    char method[16];
    char endpoint[256];
    char headers[1024];
    char body[4096];
    int timeout_seconds;
} api_request_t;

// API response
typedef struct {
    int status_code;
    char headers[2048];
    char body[8192];
    time_t timestamp;
    bool success;
} api_response_t;

// External API client structure
typedef struct {
    api_endpoint_config_t config;
    
    // Connection state
    bool connected;
    time_t last_connection;
    
    // Statistics
    int requests_sent;
    int successful_requests;
    int failed_requests;
    double average_response_time;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} external_api_client_t;

// Initialize external API client
int external_api_client_init(const api_endpoint_config_t* config);

// Clean up external API client
void external_api_client_cleanup(void);

// Send API request
int external_api_client_send_request(const api_request_t* request, api_response_t* response);

// Test API connection
int external_api_client_test_connection(void);

// Get API client status
void external_api_client_get_status(external_api_client_t* status);

// Check if API client is initialized
bool external_api_client_is_initialized(void);

// Check if API client is connected
bool external_api_client_is_connected(void);

// Get API client instance
external_api_client_t* external_api_client_get_instance(void);

#endif // EXTERNAL_API_CLIENT_H
