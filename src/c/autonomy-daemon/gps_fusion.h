#ifndef GPS_FUSION_H
#define GPS_FUSION_H

#include "types.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// Fusion algorithms
typedef enum {
    FUSION_ALGORITHM_UNKNOWN = 0,
    FUSION_ALGORITHM_WEIGHTED_AVERAGE,
    FUSION_ALGORITHM_KALMAN_FILTER,
    FUSION_ALGORITHM_PARTICLE_FILTER,
    FUSION_ALGORITHM_LEAST_SQUARES
} gps_fusion_algorithm_t;

// GPS fusion source
typedef struct {
    bool active;                        // Whether source is active
    char name[64];                      // Source name
    gps_source_type_t source_type;      // Source type
    time_t registration_time;            // Registration timestamp
    time_t last_update;                 // Last update timestamp
    gps_data_t last_gps_data;           // Last GPS data from source
    double weight;                      // Source weight (0.0-1.0)
    double reliability;                 // Source reliability (0.0-1.0)
} gps_fusion_source_t;

// GPS fusion record
typedef struct {
    time_t timestamp;                   // Fusion timestamp
    double lat;                         // Fused latitude
    double lon;                         // Fused longitude
    double altitude;                    // Fused altitude
    double accuracy;                    // Fused accuracy
    double confidence;                  // Fusion confidence
    int source_count;                   // Number of sources used
} gps_fusion_record_t;

// GPS fusion configuration
typedef struct {
    bool enabled;                       // Enable/disable fusion
    int max_sources;                    // Maximum sources to fuse
    int min_sources;                    // Minimum sources for fusion
    double update_interval;             // Fusion update interval in seconds
    double max_source_age;              // Maximum source age in seconds
    double weight_threshold;            // Minimum weight for source inclusion
    int history_size;                   // Number of fusion records to keep
    gps_fusion_algorithm_t fusion_algorithm; // Fusion algorithm to use
} gps_fusion_config_t;

// GPS fusion status
typedef struct {
    bool enabled;                       // Fusion enabled
    gps_fusion_algorithm_t fusion_algorithm; // Current fusion algorithm
    int source_count;                   // Total registered sources
    int fusion_count;                   // Total fusions performed
    double fusion_quality;              // Current fusion quality
    time_t last_fusion;                 // Last fusion timestamp
    int active_source_count;            // Number of active sources
    gps_fusion_source_t sources[8];     // Active fusion sources
} gps_fusion_status_t;

// GPS fusion system state
typedef struct {
    bool enabled;                       // Fusion enabled
    int max_sources;                    // Maximum sources
    int min_sources;                    // Minimum sources
    double update_interval;             // Update interval
    double max_source_age;              // Maximum source age
    double weight_threshold;            // Weight threshold
    int history_size;                   // History size
    gps_fusion_algorithm_t fusion_algorithm; // Fusion algorithm
    
    // State
    int source_count;                   // Registered source count
    int fusion_count;                   // Fusion count
    time_t last_fusion;                 // Last fusion
    double fusion_quality;              // Fusion quality
    
    // Sources array
    gps_fusion_source_t sources[8];     // GPS fusion sources
    
    // Fusion history
    gps_fusion_record_t fusion_history[20]; // Fusion history records
} gps_fusion_t;

// Function prototypes

/**
 * Initialize GPS fusion system
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_fusion_init(void);

/**
 * Add GPS source for fusion
 * @param source_name Name of the GPS source
 * @param source_type Type of GPS source
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_fusion_add_source(const char *source_name, gps_source_type_t source_type);

/**
 * Update GPS source data
 * @param source_name Name of the GPS source
 * @param gps_data GPS data to update
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_fusion_update_source(const char *source_name, const gps_data_t *gps_data);

/**
 * Perform GPS data fusion
 * @param fused_data GPS data structure to populate with fused result
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_fusion_perform_fusion(gps_data_t *fused_data);

/**
 * Get fusion status
 * @param status Status structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_fusion_get_status(gps_fusion_status_t *status);

/**
 * Get fusion configuration
 * @param config Configuration structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_fusion_get_config(gps_fusion_config_t *config);

/**
 * Set fusion configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_fusion_set_config(const gps_fusion_config_t *config);

/**
 * Enable/disable fusion
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_fusion_set_enabled(bool enabled);

/**
 * Force fusion update
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_fusion_force_update(void);

/**
 * Reset fusion system
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_fusion_reset(void);

/**
 * Cleanup fusion system
 */
void gps_fusion_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_FUSION_H
