#ifndef NETWORK_DISCOVERY_H
#define NETWORK_DISCOVERY_H

#include "../core/types.h"
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Network discovery configuration
typedef struct {
    bool enabled;                    // Enable/disable discovery system
    int discovery_interval;          // Discovery interval in seconds
    int interface_timeout;           // Interface timeout in seconds
    int max_interfaces;             // Maximum interfaces to track
} network_discovery_config_t;

// Network discovery status
typedef struct {
    bool enabled;                    // Discovery system enabled
    int discovery_interval;          // Current discovery interval
    int interface_timeout;           // Current interface timeout
    int max_interfaces;             // Maximum interfaces limit
    time_t last_discovery;           // Last discovery timestamp
    int total_discoveries;           // Total discoveries performed
    int interface_count;             // Current interface count
} network_discovery_status_t;

// Network discovery system state
typedef struct {
    bool enabled;                    // Discovery system enabled
    int discovery_interval;          // Discovery interval in seconds
    int interface_timeout;           // Interface timeout in seconds
    int max_interfaces;             // Maximum interfaces to track
    time_t last_discovery;           // Last discovery timestamp
    int total_discoveries;           // Total discoveries performed
    int interface_count;             // Current interface count
    network_interface_t interfaces[32]; // Discovered interfaces array
} network_discovery_t;

// Function prototypes

/**
 * Initialize network discovery system
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int network_discovery_init(void);

/**
 * Start network discovery monitoring thread
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int network_discovery_start_monitoring(void);

/**
 * Stop network discovery monitoring
 */
void network_discovery_stop_monitoring(void);

/**
 * Scan for available network interfaces
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int network_discovery_scan_interfaces(void);

/**
 * Get discovered interfaces
 * @param interfaces Array to store interfaces
 * @param max_count Maximum interfaces to return
 * @param actual_count Actual number of interfaces returned
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int network_discovery_get_interfaces(network_interface_t *interfaces, int max_count, int *actual_count);

/**
 * Get specific interface by name
 * @param interface_name Name of interface to find
 * @param interface Interface structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int network_discovery_get_interface(const char *interface_name, network_interface_t *interface);

/**
 * Get discovery system status
 * @param status Status structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int network_discovery_get_status(network_discovery_status_t *status);

/**
 * Set discovery configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int network_discovery_set_config(const network_discovery_config_t *config);

/**
 * Enable/disable discovery system
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int network_discovery_set_enabled(bool enabled);

/**
 * Force immediate discovery scan
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int network_discovery_force_scan(void);

/**
 * Cleanup discovery system
 */
void network_discovery_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // NETWORK_DISCOVERY_H
