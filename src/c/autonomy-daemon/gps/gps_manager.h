#ifndef GPS_MANAGER_H
#define GPS_MANAGER_H

#include "../core/types.h"
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Note: gps_source_type_t is defined in ../core/types.h

// Location data structure
typedef struct {
    double latitude;
    double longitude;
    double altitude;
    double accuracy;                 // Accuracy in meters
    double confidence;               // Confidence score (0.0-1.0)
    gps_source_type_t source_type;   // Source that provided this location
    char source_name[32];            // Human-readable source name
    time_t timestamp;                // When this location was obtained
    bool valid;                      // Whether this location is valid
    char quality_score[16];          // excellent, good, fair, poor
} location_data_t;

// GPS source information
typedef struct {
    bool enabled;                    // Source enabled
    gps_source_type_t type;          // Source type
    char name[32];                   // Source name
    time_t last_update;              // Last update timestamp
    double reliability_score;        // Reliability score (0.0-1.0)
    double data_quality;             // Data quality score (0.0-1.0)
    gps_data_t gps_data;             // GPS data from this source
    location_data_t location_data;   // Location data from this source
} gps_source_t;

// Location manager configuration
typedef struct {
    bool enabled;                    // Enable/disable location manager
    int update_interval;             // Update interval in seconds
    int source_timeout;              // Source timeout in seconds
    double min_confidence;           // Minimum confidence threshold
    double min_accuracy;             // Minimum accuracy threshold (meters)
    bool enable_fusion;              // Enable multi-source fusion
    bool enable_validation;          // Enable location validation
    int max_sources;                 // Maximum number of sources
} location_manager_config_t;

// GPS manager configuration
typedef struct {
    bool enabled;                    // Enable/disable GPS manager
    int update_interval;             // Update interval in seconds
    int source_timeout;              // Source timeout in seconds
} gps_manager_config_t;

// GPS manager status
typedef struct {
    bool enabled;                    // GPS manager enabled
    int update_interval;             // Current update interval
    int source_timeout;              // Current source timeout
    time_t last_update;              // Last update timestamp
    int total_updates;               // Total updates performed
    int source_count;                // Number of active sources
    int best_source;                 // Index of best source
} gps_manager_status_t;

// GPS manager system state
typedef struct {
    bool enabled;                    // GPS manager enabled
    int update_interval;             // Update interval in seconds
    int source_timeout;              // Source timeout in seconds
    time_t last_update;              // Last update timestamp
    int total_updates;               // Total updates performed
    int source_count;                // Number of active sources
    int best_source;                 // Index of best source
    gps_source_t sources[MAX_GPS_SOURCES];         // GPS sources array
    gps_data_t unified_gps;          // Unified GPS position
} gps_manager_t;

// Function prototypes

/**
 * Initialize GPS manager system
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_manager_init(void);

/**
 * Start GPS manager monitoring thread
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_manager_start_monitoring(void);

/**
 * Stop GPS manager monitoring
 */
void gps_manager_stop_monitoring(void);

/**
 * Update GPS data from all sources
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_manager_update_all_sources(void);

/**
 * Calculate unified GPS position
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_manager_calculate_unified_position(void);

/**
 * Get unified GPS data
 * @param gps_data GPS data structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_manager_get_unified_gps(gps_data_t *gps_data);

/**
 * Get GPS source information
 * @param sources Array to store sources
 * @param max_count Maximum sources to return
 * @param actual_count Actual number of sources returned
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_manager_get_sources(gps_source_t *sources, int max_count, int *actual_count);

/**
 * Get GPS manager status
 * @param status Status structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_manager_get_status(gps_manager_status_t *status);

/**
 * Set GPS manager configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_manager_set_config(const gps_manager_config_t *config);

/**
 * Update GPS manager configuration from global UCI config
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_manager_update_from_uci_config(void);

/**
 * Enable/disable GPS manager
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_manager_set_enabled(bool enabled);

/**
 * Force immediate GPS update
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_manager_force_update(void);

/**
 * Cleanup GPS manager system
 */
void gps_manager_cleanup(void);

// Location Manager Functions
/**
 * Initialize location manager
 * @param config Location manager configuration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int location_manager_init(const location_manager_config_t* config);

/**
 * Get best location from all available sources
 * @param location Location data structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int location_manager_get_best_location(location_data_t* location);

/**
 * Get location from specific source
 * @param source_type Source type to query
 * @param location Location data structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int location_manager_get_location_from_source(gps_source_type_t source_type, location_data_t* location);

/**
 * Validate location data
 * @param location Location to validate
 * @return true if valid, false otherwise
 */
bool location_manager_validate_location(const location_data_t* location);

/**
 * Fuse multiple location sources
 * @param sources Array of location sources
 * @param source_count Number of sources
 * @param fused_location Fused location result
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int location_manager_fuse_sources(const location_data_t* sources, int source_count, location_data_t* fused_location);

/**
 * Add location source
 * @param source_type Source type
 * @param name Source name
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int location_manager_add_source(gps_source_type_t source_type, const char* name);

/**
 * Remove location source
 * @param source_type Source type to remove
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int location_manager_remove_source(gps_source_type_t source_type);

/**
 * Update location source data
 * @param source_type Source type
 * @param location New location data
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int location_manager_update_source(gps_source_type_t source_type, const location_data_t* location);

/**
 * Get location manager status
 * @param status Status structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int location_manager_get_status(gps_manager_status_t* status);

/**
 * Cleanup location manager
 */
void location_manager_cleanup(void);

/**
 * Get current location from GPS manager
 * @param location Pointer to gps_data_t structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_manager_get_current_location(gps_data_t *location);

#ifdef __cplusplus
}
#endif

#endif // GPS_MANAGER_H
