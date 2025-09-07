#ifndef SERVICE_WATCHDOG_H
#define SERVICE_WATCHDOG_H

#include <stdbool.h>
#include <time.h>

// Maximum number of services to monitor
#define MAX_SERVICES_TO_MONITOR 20

// Service watchdog configuration
typedef struct {
    bool enabled;
    int service_timeout;           // Timeout in seconds before considering service hung
    bool auto_restart;             // Whether to automatically restart hung services
    int max_restart_attempts;      // Maximum number of restart attempts
    int restart_cooldown;          // Cooldown period between restarts in seconds
    char services_to_monitor[MAX_SERVICES_TO_MONITOR][64];  // Array of service names
    int services_count;            // Number of services in the array
} service_watchdog_config_t;

// Service watchdog statistics
typedef struct {
    time_t last_check_time;        // Last time services were checked
    int services_checked;          // Total number of services checked
    int services_restarted;        // Total number of services restarted
    int services_killed;           // Total number of services killed
    time_t last_restart_time;      // Last time a service was restarted
} service_watchdog_stats_t;

// Service watchdog status (includes config and stats)
typedef struct {
    // Configuration
    bool enabled;
    int service_timeout;
    bool auto_restart;
    int max_restart_attempts;
    int restart_cooldown;
    char services_to_monitor[MAX_SERVICES_TO_MONITOR][64];
    int services_count;
    
    // Statistics
    time_t last_check_time;
    int services_checked;
    int services_restarted;
    int services_killed;
    time_t last_restart_time;
} service_watchdog_status_t;

// Main service watchdog structure
typedef struct {
    service_watchdog_config_t config;
    service_watchdog_stats_t stats;
} service_watchdog_t;

// Function prototypes

/**
 * Initialize service watchdog
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int service_watchdog_init(void);

/**
 * Check services and restart hung ones
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int service_watchdog_check(void);

/**
 * Get service watchdog status
 * @param status Pointer to status structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int service_watchdog_get_status(service_watchdog_status_t *status);

/**
 * Get service watchdog configuration
 * @param config Pointer to config structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int service_watchdog_get_config(service_watchdog_config_t *config);

/**
 * Set service watchdog configuration
 * @param config Pointer to config structure
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int service_watchdog_set_config(const service_watchdog_config_t *config);

/**
 * Enable/disable service watchdog
 * @param enabled Whether to enable the watchdog
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int service_watchdog_set_enabled(bool enabled);

/**
 * Reset service watchdog statistics
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int service_watchdog_reset(void);

/**
 * Cleanup service watchdog resources
 */
void service_watchdog_cleanup(void);

#endif // SERVICE_WATCHDOG_H
