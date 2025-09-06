#ifndef METRICS_SERVER_H
#define METRICS_SERVER_H

#include "telemetry_store.h"
// #include <microhttpd.h> // Not available in current toolchain
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <math.h>
#include <sys/socket.h>

// Metrics server configuration
typedef struct {
    bool enabled;
    int port;
    char bind_address[64];
    int max_connections;
    time_t timeout_seconds;
    bool enable_cors;
    bool enable_compression;
    char auth_token[256];
    bool require_auth;
} metrics_server_config_t;

// Metrics server status
typedef struct {
    bool running;
    int port;
    char bind_address[64];
    int total_requests;
    int successful_requests;
    int failed_requests;
    time_t last_request_time;
    time_t start_time;
} metrics_server_status_t;

// Metrics server structure
typedef struct {
    metrics_server_config_t config;
    metrics_server_status_t status;
    
    // HTTP daemon
    struct MHD_Daemon* daemon;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} metrics_server_t;

// Request context
typedef struct {
    metrics_server_t* server;
    char* response_data;
    size_t response_size;
} request_context_t;

// Initialize metrics server
int metrics_server_init(const metrics_server_config_t* config);

// Clean up metrics server
void metrics_server_cleanup(void);

// Start metrics server
int metrics_server_start(void);

// Stop metrics server
int metrics_server_stop(void);

// Get metrics server status
void metrics_server_get_status(metrics_server_status_t* status);

// Check if metrics server is initialized
bool metrics_server_is_initialized(void);

// Check if metrics server is running
bool metrics_server_is_running(void);

// Get metrics server instance
metrics_server_t* metrics_server_get_instance(void);

#endif // METRICS_SERVER_H
