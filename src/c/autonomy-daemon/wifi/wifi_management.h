#ifndef WIFI_MANAGEMENT_H
#define WIFI_MANAGEMENT_H

#include "../core/types.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <math.h>

// WiFi management constants (defined in .c file)

#ifdef __cplusplus
extern "C" {
#endif

// Schedule type enumeration
typedef enum {
    SCHEDULE_TYPE_NIGHTLY = 0,
    SCHEDULE_TYPE_WEEKLY,
    SCHEDULE_TYPE_MANUAL
} schedule_type_t;

// WiFi interface
typedef struct {
    char name[64];                    // Interface name (e.g., "wlan0")
    char band[8];                     // Frequency band ("2.4" or "5")
    const char *frequency;            // Frequency description ("2.4GHz" or "5GHz")
    bool active;                      // Whether interface is active
} wifi_interface_t;

// WiFi channel score
typedef struct {
    int channel;                      // Channel number
    int score;                        // Interference score (lower is better)
    int bss_count;                    // Number of BSS on this channel
    int noise;                        // Noise floor in dBm
    int avg_rssi;                     // Average RSSI in dBm
    int signal;                       // Signal strength in dBm
} wifi_channel_score_t;

// WiFi scheduler configuration
typedef struct {
    bool nightly_enabled;             // Enable nightly optimization
    int nightly_time;                 // Nightly time in seconds from midnight
    int nightly_window_min;           // Nightly window in minutes
    bool weekly_enabled;              // Enable weekly optimization
    int weekly_days[7];               // Days of week (0=Sunday, 1=Monday, etc.)
    int weekly_time;                  // Weekly time in seconds from midnight
    int weekly_window_min;            // Weekly window in minutes
    int check_interval_min;           // Check interval in minutes
    bool skip_if_recent;              // Skip if optimized recently
    int recent_threshold_h;           // Recent threshold in hours
    char timezone[32];                // Timezone for scheduling
} wifi_scheduler_config_t;

// WiFi GPS integration configuration
typedef struct {
    bool enabled;                     // Enable GPS integration
    double movement_threshold;        // Movement threshold in meters
    int stationary_time;              // Stationary time in seconds
    int optimization_cooldown;        // Optimization cooldown in seconds
    double gps_accuracy_threshold;    // GPS accuracy threshold in meters
    bool location_logging;            // Enable detailed location logging
} wifi_gps_integration_config_t;

// GPS location data
typedef struct {
    double lat;                       // Latitude
    double lon;                       // Longitude
    double accuracy;                  // GPS accuracy in meters
    time_t timestamp;                 // Location timestamp
} wifi_gps_location_t;

// WiFi GPS integration state
typedef struct {
    bool enabled;                         // GPS integration enabled
    wifi_gps_integration_config_t config; // GPS integration configuration
    wifi_gps_location_t last_location;    // Last known location
    time_t last_optimized;                // Last optimization time
    time_t stationary_start;              // When stationary state started
    bool is_stationary;                   // Whether currently stationary
    double movement_threshold;            // Movement threshold in meters
    int stationary_time;                  // Stationary time in seconds
    int optimization_cooldown;            // Optimization cooldown in seconds
    double gps_accuracy_threshold;        // GPS accuracy threshold in meters
    bool location_logging;                // Enable location logging
} wifi_gps_integration_t;

// WiFi scheduled task
typedef struct {
    schedule_type_t type;             // Task type
    time_t scheduled_at;              // When task was scheduled
    time_t executed_at;               // When task was executed
    bool success;                     // Whether task succeeded
    char trigger[64];                 // What triggered the task
} wifi_scheduled_task_t;

// WiFi management configuration
typedef struct {
    bool enabled;                     // Enable WiFi management
    double movement_threshold;        // Movement threshold in meters
    int stationary_time;              // Stationary time in seconds
    bool nightly_optimization;        // Enable nightly optimization
    int nightly_time;                 // Nightly time in seconds from midnight
    int min_improvement;              // Minimum improvement required
    int dwell_time;                   // Dwell time in seconds
    int noise_default;                // Default noise floor in dBm
    int vht80_threshold;              // VHT80 threshold in dBm
    int vht40_threshold;              // VHT40 threshold in dBm
    bool use_dfs;                     // Allow DFS channels
    bool dry_run;                     // Test mode (don't apply changes)
    bool use_enhanced_scanner;        // Use enhanced scanning
    int strong_rssi_threshold;        // Strong interferer threshold
    int weak_rssi_threshold;          // Weak interferer threshold
    int utilization_weight;           // Utilization weight
    int excellent_threshold;          // Excellent score threshold
    int good_threshold;               // Good score threshold
    int fair_threshold;               // Fair score threshold
    int poor_threshold;               // Poor score threshold
    double overlap_penalty_ratio;     // Overlap penalty ratio
} wifi_management_config_t;

// WiFi management status
typedef struct {
    bool enabled;                     // WiFi management enabled
    int interfaces_count;             // Number of WiFi interfaces
    time_t last_optimized;            // Last optimization time
    int optimization_count;           // Total optimizations
    int successful_optimizations;     // Successful optimizations
    int failed_optimizations;         // Failed optimizations
    bool scheduler_enabled;           // Scheduler enabled
    bool gps_integration_enabled;     // GPS integration enabled
} wifi_management_status_t;

// Main WiFi management structure
typedef struct {
    // Configuration
    bool enabled;                     // WiFi management enabled
    wifi_management_config_t config;  // WiFi management configuration
    wifi_scheduler_config_t scheduler; // Scheduler configuration
    wifi_gps_integration_t gps_integration; // GPS integration
    
    // State
    time_t last_optimized;            // Last optimization time
    int optimization_count;            // Total optimizations
    int successful_optimizations;     // Successful optimizations
    int failed_optimizations;         // Failed optimizations
    // Additional configuration fields
    double movement_threshold;         // Movement threshold in meters
    int stationary_time;              // Stationary time in seconds
    bool nightly_optimization;        // Enable nightly optimization
    int nightly_time;                 // Nightly optimization time (seconds since midnight)
    double min_improvement;           // Minimum improvement threshold
    double optimization_cooldown_s;    // Optimization cooldown in seconds
    int dwell_time;                   // Dwell time in seconds
    int noise_default;                // Default noise floor
    int vht80_threshold;              // VHT80 threshold
    int vht40_threshold;              // VHT40 threshold
    bool use_dfs;                     // Use DFS channels
    bool dry_run;                     // Dry run mode
    bool use_enhanced_scanner;        // Use enhanced scanner
    int strong_rssi_threshold;        // Strong RSSI threshold
    int weak_rssi_threshold;          // Weak RSSI threshold
    double utilization_weight;        // Utilization weight
    int excellent_threshold;          // Excellent score threshold
    int good_threshold;               // Good score threshold
    int fair_threshold;               // Fair score threshold
    int poor_threshold;               // Poor score threshold
    double overlap_penalty_ratio;     // Overlap penalty ratio
    
    // Storage
    wifi_interface_t interfaces[10];  // WiFi interfaces
    wifi_channel_score_t channel_scores[100]; // Channel scores
    wifi_scheduled_task_t scheduled_tasks[50]; // Scheduled tasks
    
    // Counts
    int interfaces_count;             // Number of interfaces
    int max_interfaces;               // Maximum interfaces
    int channel_scores_count;         // Number of channel scores
    int max_channel_scores;           // Maximum channel scores
    int scheduled_tasks_count;        // Number of scheduled tasks
    int max_scheduled_tasks;          // Maximum scheduled tasks
} wifi_management_t;

// Function prototypes

/**
 * Initialize WiFi management
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int wifi_management_init(void);

/**
 * Discover WiFi interfaces
 * @return Number of interfaces discovered
 */
int wifi_management_discover_interfaces(void);

/**
 * Scan WiFi channels for interference
 * @param interface_name Interface to scan
 * @return Number of channels scanned
 */
int wifi_management_scan_channels(const char *interface_name);

/**
 * Optimize WiFi channels
 * @param interface_name Interface to optimize
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int wifi_management_optimize_channels(const char *interface_name);

/**
 * Check if scheduled optimization is needed
 * @return AUTONOMY_SUCCESS if optimization needed, error code otherwise
 */
int wifi_management_check_scheduled_optimization(void);

/**
 * Update GPS location for WiFi optimization
 * @param lat Latitude
 * @param lon Longitude
 * @param accuracy GPS accuracy in meters
 * @param timestamp Location timestamp
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int wifi_management_update_gps_location(double lat, double lon, double accuracy, time_t timestamp);

/**
 * Get WiFi management status
 * @param status Status structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int wifi_management_get_status(wifi_management_status_t *status);

/**
 * Get WiFi interfaces
 * @param interfaces Array to store interfaces
 * @param max_interfaces Maximum interfaces to retrieve
 * @return Number of interfaces retrieved
 */
int wifi_management_get_interfaces(wifi_interface_t *interfaces, int max_interfaces);

/**
 * Get channel scores
 * @param scores Array to store scores
 * @param max_scores Maximum scores to retrieve
 * @return Number of scores retrieved
 */
int wifi_management_get_channel_scores(wifi_channel_score_t *scores, int max_scores);

/**
 * Get scheduled tasks
 * @param tasks Array to store tasks
 * @param max_tasks Maximum tasks to retrieve
 * @return Number of tasks retrieved
 */
int wifi_management_get_scheduled_tasks(wifi_scheduled_task_t *tasks, int max_tasks);

/**
 * Get WiFi management configuration
 * @param config Configuration structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int wifi_management_get_config(wifi_management_config_t *config);

/**
 * Set WiFi management configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int wifi_management_set_config(const wifi_management_config_t *config);

/**
 * Enable/disable WiFi management
 * @param enabled Whether to enable management
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int wifi_management_set_enabled(bool enabled);

/**
 * Reset WiFi management
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int wifi_management_reset(void);

/**
 * Cleanup WiFi management
 */
void wifi_management_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGEMENT_H
