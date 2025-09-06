#ifndef DISK_MONITOR_H
#define DISK_MONITOR_H

#include "../core/types.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

// Disk space information
typedef struct {
    const char *path;                       // Path being monitored
    double total_gb;                        // Total disk space in GB
    double used_gb;                         // Used disk space in GB
    double available_gb;                    // Available disk space in GB
    double usage_percent;                   // Usage percentage
    int64_t inodes_total;                   // Total inodes
    int64_t inodes_used;                    // Used inodes
    int64_t inodes_free;                    // Free inodes
} disk_space_info_t;

// Disk monitor configuration
typedef struct {
    bool enabled;                           // Enable disk monitoring
    double critical_threshold_gb;           // Critical disk space threshold
    double warning_threshold_gb;            // Warning disk space threshold
    double cleanup_threshold_gb;            // Threshold to trigger cleanup
    int max_log_size_mb;                   // Maximum log file size in MB
    int max_temp_age_hours;                // Maximum age for temp files in hours
    const char *monitor_paths[10];         // Paths to monitor
    int monitor_paths_count;                // Number of monitor paths
} disk_monitor_config_t;

// Disk monitor statistics
typedef struct {
    time_t last_check_time;                 // Last check time
    int cleanup_count;                      // Number of cleanups performed
    int64_t total_bytes_freed;              // Total bytes freed
    time_t last_cleanup_time;               // Last cleanup time
} disk_monitor_stats_t;

// Disk monitor status
typedef struct {
    bool enabled;                           // Disk monitor enabled
    double critical_threshold_gb;           // Critical threshold
    double warning_threshold_gb;            // Warning threshold
    double cleanup_threshold_gb;            // Cleanup threshold
    int max_log_size_mb;                   // Max log size
    int max_temp_age_hours;                // Max temp age
    time_t last_check_time;                 // Last check time
    int cleanup_count;                      // Cleanup count
    int64_t total_bytes_freed;              // Total bytes freed
    time_t last_cleanup_time;               // Last cleanup time
} disk_monitor_status_t;

// Main disk monitor structure
typedef struct {
    disk_monitor_config_t config;           // Configuration
    disk_monitor_stats_t stats;             // Statistics
} disk_monitor_t;

// Function prototypes

/**
 * Initialize disk monitor
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int disk_monitor_init(void);

/**
 * Check disk space for monitored paths
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int disk_monitor_check_disk_space(void);

/**
 * Perform routine cleanup
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int disk_monitor_perform_cleanup(void);

/**
 * Perform emergency cleanup
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int disk_monitor_perform_emergency_cleanup(void);

/**
 * Get disk monitor status
 * @param status Status structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int disk_monitor_get_status(disk_monitor_status_t *status);

/**
 * Get disk monitor configuration
 * @param config Configuration structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int disk_monitor_get_config(disk_monitor_config_t *config);

/**
 * Set disk monitor configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int disk_monitor_set_config(const disk_monitor_config_t *config);

/**
 * Enable/disable disk monitor
 * @param enabled Whether to enable monitoring
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int disk_monitor_set_enabled(bool enabled);

/**
 * Reset disk monitor
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int disk_monitor_reset(void);

/**
 * Cleanup disk monitor
 */
void disk_monitor_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // DISK_MONITOR_H
