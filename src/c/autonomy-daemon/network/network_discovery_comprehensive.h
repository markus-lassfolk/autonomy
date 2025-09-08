#ifndef NETWORK_DISCOVERY_COMPREHENSIVE_H
#define NETWORK_DISCOVERY_COMPREHENSIVE_H

#include "../core/types.h"
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Comprehensive network discovery configuration
typedef struct {
    bool enabled;                    // Enable/disable comprehensive discovery
    int discovery_interval;          // Discovery interval in seconds
    int interface_timeout;           // Interface timeout in seconds
    int max_interfaces;             // Maximum interfaces to track
} network_discovery_comprehensive_config_t;

// Comprehensive network discovery status
typedef struct {
    bool enabled;                    // Comprehensive discovery system enabled
    int discovery_interval;          // Current discovery interval
    int interface_timeout;           // Current interface timeout
    int max_interfaces;             // Maximum interfaces limit
    time_t last_discovery;           // Last discovery timestamp
    int total_discoveries;           // Total discoveries performed
    int interface_count;             // Current interface count
} network_discovery_comprehensive_status_t;

// Comprehensive network discovery system state
typedef struct {
    bool enabled;                    // Comprehensive discovery system enabled
    int discovery_interval;          // Discovery interval in seconds
    int interface_timeout;           // Interface timeout in seconds
    int max_interfaces;             // Maximum interfaces to track
    time_t last_discovery;           // Last discovery timestamp
    int total_discoveries;           // Total discoveries performed
    int interface_count;             // Current interface count
    network_interface_t interfaces[MAX_INTERFACES]; // Discovered interfaces array
} network_discovery_comprehensive_t;

// Function prototypes

/**
 * Initialize comprehensive network discovery system
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int network_discovery_comprehensive_init(void);

/**
 * Get comprehensive interface information
 * @param interfaces Array to store interfaces
 * @param count Pointer to store actual count
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int get_comprehensive_interface_info(network_interface_t *interfaces, int *count);

/**
 * Get comprehensive interfaces for ubus
 * @param interfaces Array to store interfaces
 * @param max_count Maximum interfaces to return
 * @param actual_count Actual number of interfaces returned
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int network_discovery_get_comprehensive_interfaces(network_interface_t *interfaces, int max_count, int *actual_count);

/**
 * Check if interface should be included in failover
 * @param iface Interface to check
 * @return true if should be included, false otherwise
 */
bool should_include_in_failover(const network_interface_t *iface);

/**
 * Cleanup comprehensive discovery system
 */
void network_discovery_comprehensive_cleanup(void);

// Internal function prototypes (for testing/debugging)

/**
 * Parse network interfaces dump from ubus
 * @param ctx UBUS context
 * @param interfaces Array to populate
 * @param count Pointer to count
 */
void parse_network_interfaces_dump(void *ctx, network_interface_t *interfaces, int *count);

/**
 * Get MWAN3 interface information
 * @param ctx UBUS context
 * @param interfaces Array of interfaces
 * @param count Number of interfaces
 */
void get_mwan3_interface_info(void *ctx, network_interface_t *interfaces, int count);

/**
 * Get device information from network.device
 * @param ctx UBUS context
 * @param interfaces Array of interfaces
 * @param count Number of interfaces
 */
void get_device_information(void *ctx, network_interface_t *interfaces, int count);

/**
 * Get cellular information
 * @param ctx UBUS context
 * @param interfaces Array of interfaces
 * @param count Number of interfaces
 */
void get_cellular_information(void *ctx, network_interface_t *interfaces, int count);

/**
 * Get WiFi information
 * @param ctx UBUS context
 * @param interfaces Array of interfaces
 * @param count Number of interfaces
 */
void get_wifi_information(void *ctx, network_interface_t *interfaces, int count);

/**
 * Detect Starlink connections
 * @param interfaces Array of interfaces
 * @param count Number of interfaces
 */
void detect_starlink_connections(network_interface_t *interfaces, int count);

/**
 * Detect VPN connections
 * @param interfaces Array of interfaces
 * @param count Number of interfaces
 */
void detect_vpn_connections(network_interface_t *interfaces, int count);

/**
 * Get friendly names from UCI
 * @param interfaces Array of interfaces
 * @param count Number of interfaces
 */
void get_friendly_names_from_uci(network_interface_t *interfaces, int count);

#ifdef __cplusplus
}
#endif

#endif // NETWORK_DISCOVERY_COMPREHENSIVE_H
