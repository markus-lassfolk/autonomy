#ifndef GPS_MOVEMENT_H
#define GPS_MOVEMENT_H

#include "../core/types.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

// Movement patterns
typedef enum {
    MOVEMENT_PATTERN_UNKNOWN = 0,
    MOVEMENT_PATTERN_STATIONARY,
    MOVEMENT_PATTERN_MOVING,
    MOVEMENT_PATTERN_ACCELERATING,
    MOVEMENT_PATTERN_DECELERATING,
    MOVEMENT_PATTERN_TURNING,
    MOVEMENT_PATTERN_OSCILLATING
} gps_movement_pattern_t;

// Position data for movement analysis
typedef struct {
    time_t timestamp;                   // Position timestamp
    double lat;                         // Latitude
    double lon;                         // Longitude
    double altitude;                    // Altitude
    double accuracy;                    // Position accuracy
} position_data_t;

// Movement metrics
typedef struct {
    double current_speed;               // Current speed in m/s
    double average_speed;               // Average speed in m/s
    double max_speed;                   // Maximum speed in m/s
    double min_speed;                   // Minimum speed in m/s
    double total_distance;              // Total distance traveled in meters
    time_t total_time;                  // Total time in seconds
    int position_count;                 // Number of positions analyzed
} movement_metrics_t;

// Movement detector configuration
typedef struct {
    bool enabled;                       // Enable/disable movement detection
    double stationary_threshold;         // Stationary threshold in meters
    double movement_threshold;          // Movement threshold in meters
    double speed_threshold;             // Speed threshold in m/s
    double max_realistic_speed;         // Maximum realistic speed in m/s
    int history_size;                   // Position history size
    int min_positions;                  // Minimum positions for analysis
    int analysis_interval;              // Analysis interval in seconds
} gps_movement_config_t;

// Movement detector status
typedef struct {
    bool enabled;                       // Movement detector enabled
    bool movement_detected;             // Movement currently detected
    gps_movement_pattern_t current_pattern; // Current movement pattern
    int position_count;                 // Number of positions tracked
    time_t last_analysis;               // Last analysis timestamp
    int total_analyses;                 // Total analyses performed
    movement_metrics_t current_metrics; // Current movement metrics
} gps_movement_status_t;

// Movement detector state
typedef struct {
    bool enabled;                       // Movement detector enabled
    double stationary_threshold;         // Stationary threshold
    double movement_threshold;          // Movement threshold
    double speed_threshold;             // Speed threshold
    double max_realistic_speed;         // Maximum realistic speed
    int history_size;                   // Position history size
    int min_positions;                  // Minimum positions needed
    int analysis_interval;              // Analysis interval
    
    // State
    int position_count;                 // Number of positions tracked
    time_t last_analysis;               // Last analysis timestamp
    int total_analyses;                 // Total analyses performed
    bool movement_detected;             // Movement currently detected
    gps_movement_pattern_t current_pattern; // Current movement pattern
    
    // Position history (circular buffer)
    position_data_t position_history[50]; // Position history array
    
    // Current metrics
    movement_metrics_t current_metrics; // Current movement metrics
} gps_movement_t;

// Function prototypes

/**
 * Initialize GPS movement detector
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_movement_init(void);

/**
 * Add GPS position for movement analysis
 * @param gps_data GPS data to analyze
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_movement_add_position(const gps_data_t *gps_data);

/**
 * Get movement detection status
 * @param status Status structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_movement_get_status(gps_movement_status_t *status);

/**
 * Get movement detector configuration
 * @param config Configuration structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_movement_get_config(gps_movement_config_t *config);

/**
 * Set movement detector configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_movement_set_config(const gps_movement_config_t *config);

/**
 * Enable/disable movement detector
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_movement_set_enabled(bool enabled);

/**
 * Force movement analysis
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_movement_force_analysis(void);

/**
 * Reset movement detector
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_movement_reset(void);

/**
 * Cleanup movement detector
 */
void gps_movement_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_MOVEMENT_H
