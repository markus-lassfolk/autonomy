#ifndef UBUS_MONITOR_H
#define UBUS_MONITOR_H

#include "../core/types.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

// UBUS health information
typedef struct {
    bool ubus_responding;                   // Whether UBUS is responding
    bool rpcd_running;                      // Whether rpcd service is running
    bool ubus_socket_exists;                // Whether UBUS socket exists
    int services_count;                     // Number of available UBUS services
    char last_error[256];                   // Last error message
    char last_check_time[64];               // Last check time (RFC3339 format)
    int fix_attempts;                       // Number of fix attempts
    char last_fix_time[64];                 // Last fix time (RFC3339 format)
} ubus_health_info_t;

// UBUS monitor configuration
typedef struct {
    bool enabled;                           // Enable UBUS monitoring
    int check_interval;                     // Check interval in seconds
    int max_fix_attempts;                   // Maximum fix attempts per hour
    bool auto_fix;                          // Automatically fix UBUS issues
    int restart_timeout;                    // Timeout for service restarts in seconds
    int min_services_expected;              // Minimum expected UBUS services
    char critical_services[10][32];         // Critical services that must be available
    int critical_services_count;            // Number of critical services
} ubus_monitor_config_t;

// UBUS monitor status
typedef struct {
    bool enabled;                           // UBUS monitor enabled
    int check_interval;                     // Check interval
    int max_fix_attempts;                   // Max fix attempts
    bool auto_fix;                          // Auto fix enabled
    int restart_timeout;                    // Restart timeout
    int min_services_expected;              // Min services expected
    int critical_services_count;            // Critical services count
    char critical_services[10][32];         // Critical services
    int fix_attempts;                       // Fix attempts
    time_t last_fix_time;                   // Last fix time
} ubus_monitor_status_t;

// Main UBUS monitor structure
typedef struct {
    ubus_monitor_config_t config;           // Configuration
    int fix_attempts;                       // Current fix attempts
    time_t last_fix_time;                   // Last fix time
} ubus_monitor_t;

// Function prototypes

/**
 * Initialize UBUS monitor
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int ubus_monitor_init(void);

/**
 * Check UBUS health
 * @param info Health information structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int ubus_monitor_check_ubus_health(ubus_health_info_t *info);

/**
 * Get UBUS monitor status
 * @param status Status structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int ubus_monitor_get_status(ubus_monitor_status_t *status);

/**
 * Get UBUS monitor configuration
 * @param config Configuration structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int ubus_monitor_get_config(ubus_monitor_config_t *config);

/**
 * Set UBUS monitor configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int ubus_monitor_set_config(const ubus_monitor_config_t *config);

/**
 * Enable/disable UBUS monitor
 * @param enabled Whether to enable monitoring
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int ubus_monitor_set_enabled(bool enabled);

/**
 * Reset UBUS monitor
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int ubus_monitor_reset(void);

/**
 * Cleanup UBUS monitor
 */
void ubus_monitor_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // UBUS_MONITOR_H
