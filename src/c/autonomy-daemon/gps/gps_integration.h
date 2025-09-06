#ifndef GPS_INTEGRATION_H
#define GPS_INTEGRATION_H

#include "../core/types.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// GPS source types
    GPS_SOURCE_TYPE_RUTOS,
    GPS_SOURCE_TYPE_STARLINK,
    GPS_SOURCE_TYPE_EXTERNAL,
    GPS_SOURCE_TYPE_SIMULATED,
    GPS_SOURCE_TYPE_CUSTOM
} gps_source_type_t;

// GPS integration source
typedef struct {
    bool active;                        // Whether source is active
    int source_id;                      // Unique source identifier
    char name[64];                      // Source name
    gps_source_type_t source_type;      // Type of GPS source
    bool enabled;                        // Whether source is enabled
    time_t last_update;                 // Last update timestamp
    int update_count;                   // Total update count
    double health_score;                // Source health score (0-100)
    double reliability;                 // Source reliability (0-1)
} gps_integration_source_t;

// GPS integration record
typedef struct {
    time_t timestamp;                   // Record timestamp
    double lat;                         // Latitude
    double lon;                         // Longitude
    double accuracy;                    // Accuracy in meters
    double speed;                       // Speed in m/s
    int source_id;                      // Source ID
    gps_source_type_t source_type;      // Source type
    double confidence;                  // GPS confidence (0-1)
    double health_score;                // Source health score
} gps_integration_record_t;

// GPS integration configuration
typedef struct {
    bool enabled;                       // Enable/disable integration
    int max_sources;                    // Maximum GPS sources
    int update_interval;                // Update interval in seconds
    int check_interval;                 // Integration check interval
    double min_accuracy;                // Minimum accuracy threshold
    int history_size;                   // GPS history size
} gps_integration_config_t;

// GPS integration status
typedef struct {
    bool enabled;                       // Integration enabled
    int source_count;                   // Total sources
    int active_sources;                 // Active sources
    int total_updates;                  // Total updates
    time_t last_update;                 // Last update timestamp
    time_t last_integration_check;      // Last integration check
    int best_source_id;                 // Best GPS source ID
    gps_data_t current_gps;             // Current GPS data
    int active_source_count;            // Number of active sources
    gps_integration_source_t gps_sources[10]; // GPS sources
} gps_integration_status_t;

// GPS integration system state
typedef struct {
    bool enabled;                       // Integration enabled
    int max_sources;                    // Maximum sources
    int update_interval;                // Update interval
    int check_interval;                 // Check interval
    double min_accuracy;                // Minimum accuracy
    int history_size;                   // History size
    
    // State
    int source_count;                   // Source count
    int active_sources;                 // Active sources
    int total_updates;                  // Total updates
    time_t last_update;                 // Last update
    time_t last_integration_check;      // Last integration check
    int best_source_id;                 // Best source ID
    
    // Current GPS data
    gps_data_t current_gps;             // Current GPS data
    
    // GPS sources
    gps_integration_source_t gps_sources[10]; // GPS sources
    
    // GPS history
    gps_integration_record_t gps_history[1000]; // GPS history
} gps_integration_t;

// Function prototypes

/**
 * Initialize GPS integration system
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_integration_init(void);

/**
 * Register GPS source
 * @param name Source name
 * @param source_type Type of GPS source
 * @return Source ID on success, error code on failure
 */
int gps_integration_register_source(const char *name, gps_source_type_t source_type);

/**
 * Update GPS source data
 * @param source_id Source ID
 * @param gps_data GPS data to update
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_integration_update_source(int source_id, const gps_data_t *gps_data);

/**
 * Get GPS integration status
 * @param status Status structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_integration_get_status(gps_integration_status_t *status);

/**
 * Get GPS integration configuration
 * @param config Configuration structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_integration_get_config(gps_integration_config_t *config);

/**
 * Set GPS integration configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_integration_set_config(const gps_integration_config_t *config);

/**
 * Enable/disable GPS integration
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_integration_set_enabled(bool enabled);

/**
 * Enable/disable specific GPS source
 * @param source_id Source ID
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_integration_set_source_enabled(int source_id, bool enabled);

/**
 * Unregister GPS source
 * @param source_id Source ID to unregister
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_integration_unregister_source(int source_id);

/**
 * Reset GPS integration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_integration_reset(void);

/**
 * Cleanup GPS integration
 */
void gps_integration_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_INTEGRATION_H
