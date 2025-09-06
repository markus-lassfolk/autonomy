#ifndef GPS_PERFORMANCE_H
#define GPS_PERFORMANCE_H

#include "../core/types.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// GPS source constants
#define GPS_MAX_SOURCES 32

// Performance entry structure
typedef struct {
    bool active;                        // Whether entry is active
    time_t timestamp;                   // Measurement timestamp
    int source_id;                      // GPS source ID
    double accuracy;                    // Accuracy in meters
    double response_time;               // Response time in milliseconds
    bool success;                       // Whether measurement was successful
    double reliability_score;           // Reliability score (0-1)
    double consistency_score;           // Consistency score (0-1)
    double overall_score;               // Overall performance score (0-1)
} gps_performance_entry_t;

// GPS source performance structure
typedef struct {
    int source_id;                      // GPS source ID
    int total_measurements;             // Total measurements
    int successful_measurements;        // Successful measurements
    int failed_measurements;            // Failed measurements
    double total_accuracy;              // Total accuracy
    double total_response_time;         // Total response time
    double best_accuracy;               // Best accuracy achieved
    double worst_accuracy;              // Worst accuracy achieved
    double average_accuracy;            // Average accuracy
    double average_response_time;       // Average response time
    double reliability_score;           // Reliability score (0-1)
    double consistency_score;           // Consistency score (0-1)
    double overall_score;               // Overall performance score (0-1)
    time_t last_update;                 // Last update timestamp
    int uptime;                         // Successful measurements count
    int downtime;                       // Failed measurements count
    double availability;                // Availability percentage (0-1)
} gps_source_performance_t;

// Performance configuration structure
typedef struct {
    bool enabled;                       // Enable/disable tracking
    int max_history_entries;            // Maximum history entries
    int update_interval;                // Update interval in seconds
    int window_size;                    // Performance window size
    double min_accuracy_threshold;      // Minimum accuracy threshold
    double max_accuracy_threshold;      // Maximum accuracy threshold
} gps_performance_config_t;

// Performance status structure
typedef struct {
    bool enabled;                       // Tracking enabled
    int history_entry_count;            // Current history entries
    int max_history_entries;            // Maximum history entries
    int total_measurements;             // Total measurements
    int successful_measurements;        // Successful measurements
    int failed_measurements;            // Failed measurements
    time_t last_update;                 // Last update timestamp
    double overall_success_rate;        // Overall success rate (0-1)
    double overall_average_accuracy;    // Overall average accuracy
    double overall_average_response_time; // Overall average response time
    double overall_reliability;         // Overall reliability (0-1)
    double overall_consistency;         // Overall consistency (0-1)
    double overall_score;               // Overall performance score (0-1)
} gps_performance_status_t;

// Performance system state structure
typedef struct {
    bool enabled;                       // Tracking enabled
    int max_history_entries;            // Maximum history entries
    int update_interval;                // Update interval
    int window_size;                    // Performance window size
    double min_accuracy_threshold;      // Minimum accuracy threshold
    double max_accuracy_threshold;      // Maximum accuracy threshold
    
    // State
    int history_entry_count;            // History entry count
    int total_measurements;             // Total measurements
    int successful_measurements;        // Successful measurements
    int failed_measurements;            // Failed measurements
    time_t last_update;                 // Last update
    
    // Performance history
    gps_performance_entry_t performance_history[10000]; // Performance history entries
    
    // Source performance tracking
    gps_source_performance_t source_performance[GPS_MAX_SOURCES]; // Source performance data
    
    // Overall performance metrics
    double overall_success_rate;        // Overall success rate
    double overall_average_accuracy;    // Overall average accuracy
    double overall_average_response_time; // Overall average response time
    double overall_reliability;         // Overall reliability
    double overall_consistency;         // Overall consistency
    double overall_score;               // Overall performance score
} gps_performance_t;

// Function prototypes

/**
 * Initialize GPS performance tracking
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_performance_init(void);

/**
 * Record GPS performance measurement
 * @param source_id GPS source ID
 * @param accuracy Accuracy in meters
 * @param response_time Response time in milliseconds
 * @param success Whether measurement was successful
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_performance_record_measurement(int source_id, double accuracy, double response_time, bool success);

/**
 * Get GPS performance status
 * @param status Status structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_performance_get_status(gps_performance_status_t *status);

/**
 * Get source performance data
 * @param source_id GPS source ID
 * @param source_perf Source performance structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_performance_get_source_performance(int source_id, gps_source_performance_t *source_perf);

/**
 * Get all source performance data
 * @param sources Array to store source performance data
 * @param max_sources Maximum number of sources to retrieve
 * @return Number of sources retrieved
 */
int gps_performance_get_all_sources(gps_source_performance_t *sources, int max_sources);

/**
 * Get performance history
 * @param history Array to store performance history
 * @param max_entries Maximum number of entries to retrieve
 * @param since Timestamp to get history since
 * @return Number of history entries retrieved
 */
int gps_performance_get_history(gps_performance_entry_t *history, int max_entries, time_t since);

/**
 * Get performance configuration
 * @param config Configuration structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_performance_get_config(gps_performance_config_t *config);

/**
 * Set performance configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_performance_set_config(const gps_performance_config_t *config);

/**
 * Enable/disable performance tracking
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_performance_set_enabled(bool enabled);

/**
 * Reset performance tracking
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_performance_reset(void);

/**
 * Cleanup performance tracking
 */
void gps_performance_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_PERFORMANCE_H
