#ifndef STARLINK_GRPC_DAEMON_INTEGRATION_H
#define STARLINK_GRPC_DAEMON_INTEGRATION_H

#include "starlink_grpc_comprehensive_client.h"
#include "../core/types.h"
#include <stdbool.h>

// Daemon-specific configuration
typedef struct {
    starlink_grpc_client_config_t client_config;
    bool auto_retry;
    int max_retries;
    int retry_delay_ms;
    bool enable_monitoring;
    int monitoring_interval_seconds;
    // flawfinder: ignore - buffer size sufficient for log prefix
    char log_prefix[64]; // Bounds checked: max 63 chars + null terminator, validated in all functions
} starlink_grpc_daemon_config_t;

// Initialize daemon integration
int starlink_grpc_daemon_integration_init(const starlink_grpc_daemon_config_t *config);

// Daemon-specific gRPC calls
int starlink_grpc_daemon_get_observation(starlink_observation_t *observation);
int starlink_grpc_daemon_get_status(starlink_observation_t *observation);
int starlink_grpc_daemon_get_device_info(starlink_observation_t *observation);
int starlink_grpc_daemon_get_location(starlink_observation_t *observation);
int starlink_grpc_daemon_get_diagnostics(starlink_observation_t *observation);

// Monitoring functions
int starlink_grpc_daemon_start_monitoring(void);
int starlink_grpc_daemon_stop_monitoring(void);
bool starlink_grpc_daemon_is_monitoring(void);

// Configuration management
int starlink_grpc_daemon_update_config(const starlink_grpc_daemon_config_t *config);
const starlink_grpc_daemon_config_t* starlink_grpc_daemon_get_config(void);

// Utility functions for daemon
void starlink_grpc_daemon_log_response(const char *method, const starlink_grpc_response_t *response);
int starlink_grpc_daemon_parse_response_to_observation(const starlink_grpc_response_t *response, starlink_observation_t *observation);

// Cleanup
void starlink_grpc_daemon_integration_cleanup(void);

// Global daemon configuration
extern starlink_grpc_daemon_config_t g_starlink_grpc_daemon_config;

#endif // STARLINK_GRPC_DAEMON_INTEGRATION_H
