#ifndef GPS_HEALTH_H
#define GPS_HEALTH_H

#include "types.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// GPS source status
typedef enum {
    GPS_SOURCE_STATUS_UNKNOWN = 0,
    GPS_SOURCE_STATUS_EXCELLENT,
    GPS_SOURCE_STATUS_GOOD,
    GPS_SOURCE_STATUS_POOR,
    GPS_SOURCE_STATUS_CRITICAL,
    GPS_SOURCE_STATUS_FAILED
} gps_source_status_t;

// GPS source health
typedef struct {
    bool active;                        // Whether source is active
    char name[64];                      // Source name
    gps_source_type_t source_type;      // Source type
    time_t registration_time;            // Registration timestamp
    time_t last_update;                 // Last update timestamp
    time_t last_health_check;           // Last health check timestamp
    int total_updates;                  // Total update attempts
    int successful_updates;             // Successful updates
    int failed_updates;                 // Failed updates
    double health_score;                // Overall health score (0.0-1.0)
    double accuracy_score;              // Accuracy-based score
    double freshness_score;             // Freshness-based score
    double reliability_score;           // Reliability-based score
    double consistency_score;            // Consistency-based score
    gps_source_status_t status;         // Current source status
} gps_source_health_t;

// GPS health record
typedef struct {
    time_t timestamp;                   // Health check timestamp
    double overall_score;               // Overall health score
    int source_count;                   // Number of sources
    int healthy_sources;                // Number of healthy sources
} gps_health_record_t;

// GPS health configuration
typedef struct {
    bool enabled;                       // Enable/disable health monitoring
    int health_check_interval;          // Health check interval in seconds
    int health_history_size;            // Number of health records to keep
    double min_health_score;            // Minimum health score
    double max_health_score;            // Maximum health score
    int source_timeout;                 // Source timeout in seconds
    double accuracy_weight;             // Accuracy weight in health calculation
    double freshness_weight;            // Freshness weight in health calculation
    double reliability_weight;          // Reliability weight in health calculation
    double consistency_weight;          // Consistency weight in health calculation
} gps_health_config_t;

// GPS health status
typedef struct {
    bool enabled;                       // Health monitor enabled
    double overall_health_score;        // Overall system health score
    int source_count;                   // Total registered sources
    int total_health_checks;            // Total health checks performed
    time_t last_health_check;           // Last health check timestamp
    int active_source_count;            // Number of active sources
    gps_source_health_t sources[8];     // Active source health information
} gps_health_status_t;

// GPS health monitor state
typedef struct {
    bool enabled;                       // Health monitor enabled
    int health_check_interval;          // Health check interval
    int health_history_size;            // Health history size
    double min_health_score;            // Minimum health score
    double max_health_score;            // Maximum health score
    int source_timeout;                 // Source timeout
    double accuracy_weight;             // Accuracy weight
    double freshness_weight;            // Freshness weight
    double reliability_weight;          // Reliability weight
    double consistency_weight;          // Consistency weight
    
    // State
    int source_count;                   // Registered source count
    int total_health_checks;            // Total health checks
    time_t last_health_check;           // Last health check
    double overall_health_score;        // Overall health score
    
    // Sources array
    gps_source_health_t sources[8];     // GPS sources
    
    // Health history
    gps_health_record_t health_history[100]; // Health history records
} gps_health_t;

// Function prototypes

/**
 * Initialize GPS health monitor
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_health_init(void);

/**
 * Register GPS source for health monitoring
 * @param source_name Name of the GPS source
 * @param source_type Type of GPS source
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_health_register_source(const char *source_name, gps_source_type_t source_type);

/**
 * Update GPS source health
 * @param source_name Name of the GPS source
 * @param gps_data GPS data for health calculation
 * @param update_successful Whether the update was successful
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_health_update_source(const char *source_name, const gps_data_t *gps_data, bool update_successful);

/**
 * Perform health check
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_health_perform_check(void);

/**
 * Get GPS health status
 * @param status Status structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_health_get_status(gps_health_status_t *status);

/**
 * Get health monitor configuration
 * @param config Configuration structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_health_get_config(gps_health_config_t *config);

/**
 * Set health monitor configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_health_set_config(const gps_health_config_t *config);

/**
 * Enable/disable health monitor
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_health_set_enabled(bool enabled);

/**
 * Get source health
 * @param source_name Name of the GPS source
 * @param source_health Source health structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_health_get_source_health(const char *source_name, gps_source_health_t *source_health);

/**
 * Unregister GPS source
 * @param source_name Name of the GPS source
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_health_unregister_source(const char *source_name);

/**
 * Reset health monitor
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_health_reset(void);

/**
 * Cleanup health monitor
 */
void gps_health_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_HEALTH_H
