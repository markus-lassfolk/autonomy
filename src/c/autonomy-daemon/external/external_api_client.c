#include "external_api_client.h"
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
        strcpy(g_external_api_client.config.api_key, "");
        strcpy(g_external_api_client.config.username, "");
        strcpy(g_external_api_client.config.password, "");
        g_external_api_client.config.timeout_seconds = 30;
        g_external_api_client.config.use_ssl = true;
        strcpy(g_external_api_client.config.ca_cert_path, "/etc/ssl/certs/ca-certificates.crt");
        strcpy(g_external_api_client.config.client_cert_path, "");
        strcpy(g_external_api_client.config.client_key_path, "");
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
static int external_api_client_send_request(const api_request_t* request, api_response_t* response) {
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
static int external_api_client_test_connection(void) {
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

// Send API request (internal implementation)
static int api_send_request(const api_request_t* request, api_response_t* response) {
    if (!request || !response || g_api_socket < 0) {
        return -1;
    }
    
    // This is a simplified HTTP request implementation
    // In a real system, you'd use libcurl or similar
    
    // Create HTTP request
    char http_request[8192];
    snprintf(http_request, sizeof(http_request),
             "%s %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "User-Agent: Autonomy-Daemon/6.1.0\r\n"
             "Accept: application/json\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %zu\r\n"
             "\r\n"
             "%s",
             request->method,
             request->endpoint,
             g_external_api_client.config.base_url,
             strlen(request->body),
             request->body);
    
    // Send request
    int total_sent = 0;
    int request_length = strlen(http_request);
    
    while (total_sent < request_length) {
        int sent = send(g_api_socket, http_request + total_sent, request_length - total_sent, 0);
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(1000); // Wait 1ms
                continue;
            }
            return -1;
        }
        total_sent += sent;
    }
    
    // Receive response (simplified)
    char response_buffer[8192];
    int received = recv(g_api_socket, response_buffer, sizeof(response_buffer) - 1, 0);
    
    if (received > 0) {
        response_buffer[received] = '\0';
        
        // Parse HTTP response (simplified)
        char* body_start = strstr(response_buffer, "\r\n\r\n");
        if (body_start) {
            body_start += 4; // Skip headers
            strcpy(response->body, body_start);
        }
        
        // Extract status code (simplified)
        if (strstr(response_buffer, "HTTP/1.1 200")) {
            response->status_code = 200;
        } else if (strstr(response_buffer, "HTTP/1.1 404")) {
            response->status_code = 404;
        } else {
            response->status_code = 500;
        }
        
        // Copy headers (simplified)
        strcpy(response->headers, "Content-Type: application/json");
    } else {
        return -1;
    }
    
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
        strcpy(host, url + 7);
    } else if (strncmp(url, "https://", 8) == 0) {
        strcpy(host, url + 8);
    } else {
        strcpy(host, url);
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
static bool external_api_client_is_connected(void) {
    return g_external_api_client_initialized && g_external_api_client.connected;
}

// Get API client instance
static external_api_client_t* external_api_client_get_instance(void) {
    return g_external_api_client_initialized ? &g_external_api_client : NULL;
}
