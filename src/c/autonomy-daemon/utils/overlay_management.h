#ifndef OVERLAY_MANAGEMENT_H
#define OVERLAY_MANAGEMENT_H

#include "../core/types.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

// Overlay management configuration
typedef struct {
    bool enabled;                           // Enable overlay management
    int overlay_space_threshold;            // Warning threshold (percentage)
    int overlay_critical_threshold;         // Critical threshold (percentage)
    int cleanup_retention_days;             // Days to keep files before cleanup
    bool notifications_enabled;             // Enable notifications
    bool notify_on_fixes;                   // Notify on successful fixes
    bool notify_on_critical;                // Notify on critical issues
} overlay_management_config_t;

// Overlay management statistics
typedef struct {
    time_t last_check_time;                 // Last check time
    int cleanup_count;                      // Number of routine cleanups
    int emergency_cleanup_count;            // Number of emergency cleanups
    int64_t total_bytes_freed;              // Total bytes freed
    time_t last_cleanup_time;               // Last cleanup time
} overlay_management_stats_t;

// Overlay management status
typedef struct {
    bool enabled;                           // Overlay management enabled
    int overlay_space_threshold;            // Warning threshold
    int overlay_critical_threshold;         // Critical threshold
    int cleanup_retention_days;             // Cleanup retention days
    bool notifications_enabled;             // Notifications enabled
    bool notify_on_fixes;                   // Notify on fixes
    bool notify_on_critical;                // Notify on critical
    time_t last_check_time;                 // Last check time
    int cleanup_count;                      // Cleanup count
    int emergency_cleanup_count;            // Emergency cleanup count
    int64_t total_bytes_freed;              // Total bytes freed
    time_t last_cleanup_time;               // Last cleanup time
} overlay_management_status_t;

// Main overlay management structure
typedef struct {
    overlay_management_config_t config;     // Configuration
    overlay_management_stats_t stats;       // Statistics
} overlay_management_t;

// Function prototypes

/**
 * Initialize overlay management
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int overlay_management_init(void);

/**
 * Check overlay space and perform cleanup if needed
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int overlay_management_check(void);

/**
 * Get overlay management status
 * @param status Status structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int overlay_management_get_status(overlay_management_status_t *status);

/**
 * Get overlay management configuration
 * @param config Configuration structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int overlay_management_get_config(overlay_management_config_t *config);

/**
 * Set overlay management configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int overlay_management_set_config(const overlay_management_config_t *config);

/**
 * Enable/disable overlay management
 * @param enabled Whether to enable management
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int overlay_management_set_enabled(bool enabled);

/**
 * Reset overlay management
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int overlay_management_reset(void);

/**
 * Cleanup overlay management
 */
void overlay_management_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // OVERLAY_MANAGEMENT_H
