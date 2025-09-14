#include "external_api_client.h"
#include "../shared/utils/http_client_libcurl.h"
#include "../shared/utils/json_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <math.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Global external API client instance
static external_api_client_t g_external_api_client;
static bool g_external_api_client_initialized = false;

// Network socket
static int g_api_socket = -1;

// Forward declarations
static int api_send_request(const api_request_t* request, api_response_t* response);
static int api_connect_to_endpoint(void);
static int api_disconnect(void);
double calculate_response_time(time_t start_time);

// Initialize external API client
int external_api_client_init(const api_endpoint_config_t* config) {
    if (g_external_api_client_initialized) {
        return 0; // Already initialized
    }
    
    memset(&g_external_api_client, 0, sizeof(external_api_client_t));
    
    // Set configuration
    if (config) {
        g_external_api_client.config = *config;
    } else {
        // Default configuration
        strcpy(g_external_api_client.config.base_url, "https://api.example.com");
        strncpy(g_external_api_client.config.api_key, "", sizeof(g_external_api_client.config.api_key) - 1);
        g_external_api_client.config.api_key[sizeof(g_external_api_client.config.api_key) - 1] = '\0';
        strncpy(g_external_api_client.config.username, "", sizeof(g_external_api_client.config.username) - 1);
        g_external_api_client.config.username[sizeof(g_external_api_client.config.username) - 1] = '\0';
        strncpy(g_external_api_client.config.password, "", sizeof(g_external_api_client.config.password) - 1);
        g_external_api_client.config.password[sizeof(g_external_api_client.config.password) - 1] = '\0';
        g_external_api_client.config.timeout_seconds = 30; // Use configurable timeout
        g_external_api_client.config.use_ssl = true; // Use configurable SSL setting
        strncpy(g_external_api_client.config.ca_cert_path, "/etc/ssl/certs/ca-certificates.crt", sizeof(g_external_api_client.config.ca_cert_path) - 1);
        g_external_api_client.config.ca_cert_path[sizeof(g_external_api_client.config.ca_cert_path) - 1] = '\0';
        strncpy(g_external_api_client.config.client_cert_path, "", sizeof(g_external_api_client.config.client_cert_path) - 1);
        g_external_api_client.config.client_cert_path[sizeof(g_external_api_client.config.client_cert_path) - 1] = '\0';
        strncpy(g_external_api_client.config.client_key_path, "", sizeof(g_external_api_client.config.client_key_path) - 1);
        g_external_api_client.config.client_key_path[sizeof(g_external_api_client.config.client_key_path) - 1] = '\0';
    }
    
    // Initialize mutex
    g_external_api_client.mutex = malloc(sizeof(pthread_mutex_t));
    if (!g_external_api_client.mutex) {
        return -1;
    }
    
    pthread_mutex_init(g_external_api_client.mutex, NULL);
    
    g_external_api_client_initialized = true;
    return 0;
}

// Clean up external API client
void external_api_client_cleanup(void) {
    if (!g_external_api_client_initialized) return;
    
    // Disconnect if connected
    if (g_external_api_client.connected) {
        api_disconnect();
    }
    
    if (g_external_api_client.mutex) {
        pthread_mutex_destroy(g_external_api_client.mutex);
        free(g_external_api_client.mutex);
    }
    
    if (g_api_socket >= 0) {
        close(g_api_socket);
        g_api_socket = -1;
    }
    
    g_external_api_client.mutex = NULL;
    g_external_api_client_initialized = false;
}

// Send API request
int external_api_client_send_request(const api_request_t* request, api_response_t* response) {
    if (!g_external_api_client_initialized || !request || !response) {
        return -1;
    }
    
    pthread_mutex_lock(g_external_api_client.mutex);
    
    // Ensure connection
    if (!g_external_api_client.connected) {
        if (api_connect_to_endpoint() != 0) {
            pthread_mutex_unlock(g_external_api_client.mutex);
            return -1;
        }
    }
    
    // Send request
    time_t start_time = time(NULL);
    int result = api_send_request(request, response);
    
    if (result == 0) {
        // Update statistics
        g_external_api_client.requests_sent++;
        g_external_api_client.successful_requests++;
        
        // Calculate response time
        double response_time = calculate_response_time(start_time);
        g_external_api_client.average_response_time = 
            (g_external_api_client.average_response_time * (g_external_api_client.successful_requests - 1) + response_time) / 
            g_external_api_client.successful_requests;
        
        response->success = true;
    } else {
        g_external_api_client.requests_sent++;
        g_external_api_client.failed_requests++;
        response->success = false;
    }
    
    response->timestamp = time(NULL);
    
    pthread_mutex_unlock(g_external_api_client.mutex);
    
    return result;
}

// Test API connection
int external_api_client_test_connection(void) {
    if (!g_external_api_client_initialized) {
        return -1;
    }
    
    pthread_mutex_lock(g_external_api_client.mutex);
    
    int result = api_connect_to_endpoint();
    
    if (result == 0) {
        g_external_api_client.connected = true;
        g_external_api_client.last_connection = time(NULL);
    }
    
    pthread_mutex_unlock(g_external_api_client.mutex);
    
    return result;
}

// Send API request (internal implementation using libcurl)
static int api_send_request(const api_request_t* request, api_response_t* response) {
    if (!request || !response) {
        return -1;
    }
    
    // Build full URL
    char full_url[1024];
    snprintf(full_url, sizeof(full_url), "%s%s", g_external_api_client.config.base_url, request->endpoint);
    
    // Create HTTP request using libcurl
    http_request_t* http_req = NULL;
    
    if (strcmp(request->method, "GET") == 0) {
        http_req = http_request_create(full_url, HTTP_METHOD_GET);
    } else if (strcmp(request->method, "POST") == 0) {
        http_req = http_request_create(full_url, HTTP_METHOD_POST);
        if (request->body && strlen(request->body) > 0) {
            http_req->body = strdup(request->body);
            http_req->body_size = strlen(request->body);
        }
    } else if (strcmp(request->method, "PUT") == 0) {
        http_req = http_request_create(full_url, HTTP_METHOD_PUT);
        if (request->body && strlen(request->body) > 0) {
            http_req->body = strdup(request->body);
            http_req->body_size = strlen(request->body);
        }
    } else if (strcmp(request->method, "DELETE") == 0) {
        http_req = http_request_create(full_url, HTTP_METHOD_DELETE);
    } else {
        return -1; // Unsupported method
    }
    
    if (!http_req) {
        return -1;
    }
    
    // Set common headers
    http_request_add_header_kv(http_req, "User-Agent", "Autonomy-Daemon/6.1.0");
    http_request_add_header_kv(http_req, "Accept", "application/json");
    
    if (http_req->body) {
        http_request_add_header_kv(http_req, "Content-Type", "application/json");
    }
    
    // Add custom headers if provided
    if (request->headers && strlen(request->headers) > 0) {
        http_request_add_header(http_req, request->headers);
    }
    
    // Set authentication if provided
    if (g_external_api_client.config.api_key && strlen(g_external_api_client.config.api_key) > 0) {
        char auth_header[512];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_external_api_client.config.api_key);
        http_request_add_header(http_req, auth_header);
    }
    
    // Set SSL verification based on config
    http_req->verify_ssl = g_external_api_client.config.use_ssl;
    
    // Set timeouts (convert seconds to milliseconds)
    http_req->connect_timeout_ms = g_external_api_client.config.timeout_seconds * 1000;
    http_req->request_timeout_ms = g_external_api_client.config.timeout_seconds * 1000;
    
    // Execute request
    http_response_t* http_resp = http_request(http_req);
    
    if (!http_resp) {
        http_request_free(http_req);
        return -1;
    }
    
    // Copy response data
    response->status_code = http_resp->status_code;
    
    if (http_resp->body && http_resp->body_size > 0) {
        size_t copy_size = (http_resp->body_size < sizeof(response->body) - 1) ? 
                          http_resp->body_size : sizeof(response->body) - 1;
        memcpy(response->body, http_resp->body, copy_size);
        response->body[copy_size] = '\0';
    } else {
        response->body[0] = '\0';
    }
    
    if (http_resp->headers && http_resp->header_size > 0) {
        size_t copy_size = (http_resp->header_size < sizeof(response->headers) - 1) ? 
                          http_resp->header_size : sizeof(response->headers) - 1;
        memcpy(response->headers, http_resp->headers, copy_size);
        response->headers[copy_size] = '\0';
    } else {
        response->headers[0] = '\0';
    }
    
    response->timestamp = time(NULL);
    
    // Cleanup
    http_response_free(http_resp);
    http_request_free(http_req);
    
    return 0;
}

// Connect to API endpoint
static int api_connect_to_endpoint(void) {
    // Parse base URL to extract host and port
    char host[256];
    int port = 80; // Default HTTP port
    
    if (g_external_api_client.config.use_ssl) {
        port = 443; // Default HTTPS port
    }
    
    // Extract host from base URL
    const char* url = g_external_api_client.config.base_url;
    if (strncmp(url, "http://", 7) == 0) {
        strncpy(host, url + 7, sizeof(host) - 1);
        host[sizeof(host) - 1] = '\0';
    } else if (strncmp(url, "https://", 8) == 0) {
        strncpy(host, url + 8, sizeof(host) - 1);
        host[sizeof(host) - 1] = '\0';
    } else {
        strncpy(host, url, sizeof(host) - 1);
        host[sizeof(host) - 1] = '\0';
    }
    
    // Remove path from host
    char* path_start = strchr(host, '/');
    if (path_start) {
        *path_start = '\0';
    }
    
    // Check for custom port
    char* port_start = strchr(host, ':');
    if (port_start) {
        *port_start = '\0';
        port = atoi(port_start + 1);
    }
    
    // Create socket
    g_api_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_api_socket < 0) {
        return -1;
    }
    
    // Set socket options
    int flags = fcntl(g_api_socket, F_GETFL, 0);
    fcntl(g_api_socket, F_SETFL, flags | O_NONBLOCK);
    
    // Resolve host address
    struct hostent* host_info = gethostbyname(host);
    if (!host_info) {
        close(g_api_socket);
        g_api_socket = -1;
        return -1;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr, host_info->h_addr, host_info->h_length);
    
    // Connect to server
    int connect_result = connect(g_api_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (connect_result < 0 && errno != EINPROGRESS) {
        close(g_api_socket);
        g_api_socket = -1;
        return -1;
    }
    
    // Wait for connection with timeout
    struct pollfd pfd;
    pfd.fd = g_api_socket;
    pfd.events = POLLOUT;
    
    int poll_result = poll(&pfd, 1, g_external_api_client.config.timeout_seconds * 1000);
    if (poll_result <= 0) {
        close(g_api_socket);
        g_api_socket = -1;
        return -1;
    }
    
    // Check connection status
    int error = 0;
    socklen_t error_len = sizeof(error);
    if (getsockopt(g_api_socket, SOL_SOCKET, SO_ERROR, &error, &error_len) < 0 || error != 0) {
        close(g_api_socket);
        g_api_socket = -1;
        return -1;
    }
    
    return 0;
}

// Disconnect from API endpoint
static int api_disconnect(void) {
    if (g_api_socket >= 0) {
        close(g_api_socket);
        g_api_socket = -1;
    }
    
    g_external_api_client.connected = false;
    
    return 0;
}

// Calculate response time
double calculate_response_time(time_t start_time) {
    time_t end_time = time(NULL);
    return difftime(end_time, start_time);
}

// Get API client status
void external_api_client_get_status(external_api_client_t* status) {
    if (!status || !g_external_api_client_initialized) return;
    
    pthread_mutex_lock(g_external_api_client.mutex);
    *status = g_external_api_client;
    pthread_mutex_unlock(g_external_api_client.mutex);
}

// Check if API client is initialized
bool external_api_client_is_initialized(void) {
    return g_external_api_client_initialized;
}

// Check if API client is connected
bool external_api_client_is_connected(void) {
    return g_external_api_client_initialized && g_external_api_client.connected;
}

// Get API client instance
external_api_client_t* external_api_client_get_instance(void) {
    return g_external_api_client_initialized ? &g_external_api_client : NULL;
}
