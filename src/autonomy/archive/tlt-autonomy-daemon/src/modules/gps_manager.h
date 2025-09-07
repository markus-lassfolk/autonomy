#ifndef GPS_MANAGER_H
#define GPS_MANAGER_H

#include "types.h"
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// GPS source types
typedef enum {
    GPS_SOURCE_UNKNOWN = 0,
    GPS_SOURCE_RUTOS,
    GPS_SOURCE_STARLINK,
    GPS_SOURCE_EXTERNAL,
    GPS_SOURCE_SIMULATED
} gps_source_type_t;

// GPS source information
typedef struct {
    bool enabled;                    // Source enabled
    gps_source_type_t type;          // Source type
    char name[32];                   // Source name
    time_t last_update;              // Last update timestamp
    double reliability_score;        // Reliability score (0.0-1.0)
    double data_quality;             // Data quality score (0.0-1.0)
    gps_data_t gps_data;             // GPS data from this source
} gps_source_t;

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
    gps_source_t sources[8];         // GPS sources array
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

#ifdef __cplusplus
}
#endif

#endif // GPS_MANAGER_H
