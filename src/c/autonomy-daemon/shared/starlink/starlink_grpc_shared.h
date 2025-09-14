#ifndef STARLINK_GRPC_SHARED_H
#define STARLINK_GRPC_SHARED_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Shared gRPC client configuration
typedef struct {
    char host[256];                 // Starlink device host
    int port;                       // Starlink device port (default: 9200)
    int timeout;                    // Request timeout in seconds
    int max_retries;                // Maximum retry attempts
    int retry_delay_ms;             // Delay between retries in milliseconds
    bool debug_mode;                // Enable debug logging
    bool insecure_mode;             // Allow insecure connections
    char user_agent[128];           // User agent string
} starlink_grpc_shared_config_t;

// gRPC response structure
typedef struct {
    bool success;                   // Whether the request was successful
    int http_status;                // HTTP status code
    char *response_data;            // Response data (allocated, must be freed)
    size_t response_size;           // Size of response data
    char error_message[512];        // Error message if failed
    time_t timestamp;               // Request timestamp
} starlink_grpc_shared_response_t;

// Initialize shared gRPC client
int starlink_grpc_shared_init(const starlink_grpc_shared_config_t *config);

// Cleanup shared gRPC client
void starlink_grpc_shared_cleanup(void);

// Make a gRPC call to Starlink device
int starlink_grpc_shared_call(const char *method, 
                              const unsigned char *request_data, 
                              size_t request_size,
                              starlink_grpc_shared_response_t *response);

// Free response data
void starlink_grpc_shared_free_response(starlink_grpc_shared_response_t *response);

// Get device info
int starlink_grpc_shared_get_device_info(starlink_grpc_shared_response_t *response);

// Get device status
int starlink_grpc_shared_get_status(starlink_grpc_shared_response_t *response);

// Get device location
int starlink_grpc_shared_get_location(starlink_grpc_shared_response_t *response);

// Get device history
int starlink_grpc_shared_get_history(starlink_grpc_shared_response_t *response);

// Get device context
int starlink_grpc_shared_get_context(starlink_grpc_shared_response_t *response);

#endif // STARLINK_GRPC_SHARED_H
