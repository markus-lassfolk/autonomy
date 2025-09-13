#ifndef STARLINK_GRPC_COMPREHENSIVE_CLIENT_H
#define STARLINK_GRPC_COMPREHENSIVE_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Configuration structure for comprehensive gRPC client
typedef struct {
    // Connection settings
    char host[256]; // Bounds checked: max 255 chars + null terminator, validated in all functions
    int port;
    int timeout;
    int retries;
    
    // Output formatting flags
    bool raw_mode;
    bool debug_mode;
    bool pretty_mode;
    bool compact_mode;
    bool no_header;
    bool silent_mode;
    bool hex_mode;
    bool summary_mode;
    bool verbose_mode;
    bool timestamp_mode;
    bool insecure_mode;
    
    // Advanced features
    bool compare_mode;
    bool diff_mode;
    int watch_interval;
    char *user_agent;
    char *fields_filter;
    char *log_file;
    char *batch_file;
    char *export_format;
    
    // Previous response for comparison
    char *previous_response;
    size_t previous_response_size;
} starlink_grpc_client_config_t;

// Response structure
typedef struct {
    int http_status;
    size_t response_size;
    char *response_data;
    time_t timestamp;
    bool success;
    char error_message[512]; // Bounds checked: max 511 chars + null terminator, validated in all functions
} starlink_grpc_response_t;

// Initialize comprehensive gRPC client
int starlink_grpc_comprehensive_client_init(starlink_grpc_client_config_t *config);

// Make gRPC call with comprehensive options
int starlink_grpc_comprehensive_call(
    const char *method,
    const char *request_data,
    size_t request_size,
    starlink_grpc_response_t *response
);

// Utility functions
void starlink_grpc_print_hex_data(const unsigned char *data, size_t len);
void starlink_grpc_print_timestamp(void);
void starlink_grpc_print_header(const char *status, size_t bytes);
void starlink_grpc_print_debug_info(
    const char *url, 
    const char *method, 
    const unsigned char *request_data, 
    size_t request_len, 
    const unsigned char *response_data, 
    size_t response_len
);
void starlink_grpc_log_to_file(const unsigned char *data, size_t len, const char *log_file);
void starlink_grpc_print_summary(const char *json_data);
void starlink_grpc_print_compact_json(const char *json_data);
void starlink_grpc_print_pretty_json(const char *json_data);
void starlink_grpc_handle_access_denied(const char *method, const unsigned char *response);
void starlink_grpc_print_formatted_output(
    const char *json_data, 
    const unsigned char *raw_data, 
    size_t raw_len,
    const starlink_grpc_client_config_t *config
);

// Parse endpoint (host:port format)
int starlink_grpc_parse_endpoint(const char *endpoint, char **host, int *port);

// Cleanup
void starlink_grpc_comprehensive_client_cleanup(void);

// Global configuration instance
extern starlink_grpc_client_config_t g_starlink_grpc_config;

#endif // STARLINK_GRPC_COMPREHENSIVE_CLIENT_H








