#ifndef STARLINK_OBSTRUCTION_H
#define STARLINK_OBSTRUCTION_H

#include "types.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// Match status enumeration
typedef enum {
    MATCH_STATUS_UNKNOWN = 0,
    MATCH_STATUS_MATCHING,
    MATCH_STATUS_CONFIRMED,
    MATCH_STATUS_FAILED,
    MATCH_STATUS_TIMEOUT
} match_status_t;

// Obstruction severity enumeration
typedef enum {
    OBSTRUCTION_SEVERITY_MINOR = 0,
    OBSTRUCTION_SEVERITY_MODERATE,
    OBSTRUCTION_SEVERITY_SEVERE
} obstruction_severity_t;

// Obstruction signature data
typedef struct {
    double typical_obstruction;           // Typical obstruction percentage
    double obstruction_range_min;         // Minimum obstruction value
    double obstruction_range_max;         // Maximum obstruction value
    double typical_snr;                   // Typical SNR value
    double snr_range_min;                 // Minimum SNR value
    double snr_range_max;                 // Maximum SNR value
    double recovery_time;                 // Typical recovery time in seconds
    obstruction_severity_t severity;      // Obstruction severity
    double predictability;                // How predictable this pattern is (0-1)
    double wedge_pattern[12];             // 12-hour wedge obstruction pattern
} starlink_obstruction_signature_t;

// Environmental pattern
typedef struct {
    bool active;                          // Whether this pattern is active
    char id[64];                          // Unique pattern identifier
    char name[128];                       // Pattern name
    char description[256];                // Pattern description
    double latitude;                      // Pattern latitude
    double longitude;                     // Pattern longitude
    double accuracy;                      // GPS accuracy in meters
    double radius;                        // Pattern radius in meters
    double elevation;                     // Elevation in meters
    char environment[64];                 // Environment type
    starlink_obstruction_signature_t obstruction_data; // Obstruction characteristics
    double confidence;                    // Pattern confidence (0-1)
    int sample_count;                     // Number of samples for this pattern
    time_t first_seen;                    // First observation timestamp
    time_t last_seen;                     // Last observation timestamp
} starlink_environmental_pattern_t;

// Trend point for analysis
typedef struct {
    bool active;                          // Whether this point is active
    time_t timestamp;                     // Point timestamp
    double value;                         // Point value
    double quality;                       // Data quality score (0-1)
} trend_point_t;

// Trend point array
typedef struct {
    trend_point_t points[1440];           // Up to 24 hours of 1-minute data
    int max_points;                       // Maximum number of points
    int point_count;                      // Current number of active points
} trend_point_array_t;

// Trend analyzer configuration
typedef struct {
    int max_history_points;               // Maximum history points to store
    int min_points_for_analysis;          // Minimum points needed for analysis
    int analysis_window;                  // Analysis window in seconds
    int prediction_horizon;               // Prediction horizon in seconds
    double anomaly_threshold;             // Standard deviations for anomaly detection
    int seasonal_min_period;              // Minimum period for seasonal detection
    int seasonal_max_period;              // Maximum period for seasonal detection
    int cache_timeout;                    // Cache timeout in seconds
} starlink_trend_analyzer_config_t;

// Movement detector configuration
typedef struct {
    double min_movement_distance;         // Minimum distance to consider movement (meters)
    int movement_timeout;                 // Movement timeout in seconds
    int location_history_size;            // Number of location points to keep
    double min_accuracy_meters;           // Minimum GPS accuracy to trust (meters)
    int speed_smoothing_window;           // Number of points for speed smoothing
    double movement_speed_threshold;      // Minimum speed to consider moving (m/s)
    int stationary_time_required;         // Time required to be stationary (seconds)
    double significant_distance;          // Distance that triggers obstruction refresh (meters)
} starlink_movement_detector_config_t;

// Active match
typedef struct {
    bool active;                          // Whether this match is active
    char pattern_id[64];                  // Pattern identifier
    char pattern_name[128];               // Pattern name
    time_t start_time;                    // Match start time
    time_t last_update;                   // Last update time
    double similarity;                    // Pattern similarity score (0-1)
    double confidence;                    // Match confidence (0-1)
    match_status_t status;                // Match status
    int sample_count;                     // Number of samples in this match
} starlink_active_match_t;

// Match result
typedef struct {
    bool active;                          // Whether this result is active
    time_t timestamp;                     // Result timestamp
    char pattern_id[64];                  // Pattern identifier
    char pattern_name[128];               // Pattern name
    double similarity;                    // Pattern similarity score (0-1)
    double confidence;                    // Match confidence (0-1)
    bool success;                         // Whether match was successful
    char reason[128];                     // Success/failure reason
    int duration;                         // Match duration in seconds
    int sample_count;                     // Number of samples in this match
} starlink_match_result_t;

// Obstruction sample
typedef struct {
    time_t timestamp;                     // Sample timestamp
    bool currently_obstructed;            // Whether currently obstructed
    double fraction_obstructed;           // Fraction of sky obstructed (0-1)
    double time_obstructed;               // Time obstructed in seconds
    double snr;                           // Signal-to-noise ratio
    double avg_prolonged_obstruction_interval_s; // Average prolonged obstruction interval
    int valid_s;                          // Valid seconds
    int patches_valid;                    // Valid patches
    double wedge_fraction_obstructed[12]; // 12-hour wedge obstruction pattern
    double wedge_abs_fraction_obstructed[12]; // Absolute wedge obstruction pattern
} starlink_obstruction_sample_t;

// Obstruction analysis configuration
typedef struct {
    bool enabled;                         // Enable/disable obstruction analysis
    int max_patterns;                     // Maximum environmental patterns
    int min_observations_to_learn;        // Minimum observations to learn pattern
    double pattern_similarity_threshold;   // Pattern similarity threshold
    double location_radius_meters;        // Location radius for pattern matching
    int max_active_matches;               // Maximum concurrent active matches
    int match_timeout_minutes;            // Timeout for active matches
    int history_size;                     // Number of match results to keep
} starlink_obstruction_config_t;

// Obstruction analysis status
typedef struct {
    bool enabled;                         // Obstruction analysis enabled
    int pattern_count;                    // Number of active patterns
    int max_patterns;                     // Maximum patterns allowed
    int active_match_count;               // Number of active matches
    int max_active_matches;               // Maximum active matches allowed
    int total_observations;               // Total observations processed
    time_t last_analysis;                 // Last analysis timestamp
} starlink_obstruction_status_t;

// Main obstruction analysis structure
typedef struct {
    // Configuration
    bool enabled;                         // Obstruction analysis enabled
    int max_patterns;                     // Maximum patterns
    int min_observations_to_learn;        // Minimum observations to learn
    double pattern_similarity_threshold;   // Pattern similarity threshold
    double location_radius_meters;        // Location radius
    int max_active_matches;               // Maximum active matches
    int match_timeout_minutes;            // Match timeout
    int history_size;                     // History size
    
    // State
    int pattern_count;                    // Pattern count
    int active_match_count;               // Active match count
    int total_observations;               // Total observations
    time_t last_analysis;                 // Last analysis
    
    // Storage
    starlink_environmental_pattern_t patterns[100]; // Pattern storage
    starlink_active_match_t active_matches[5];      // Active matches
    starlink_match_result_t match_history[100];     // Match history
    
    // Subsystems
    starlink_trend_analyzer_config_t trend_analyzer; // Trend analyzer config
    trend_point_array_t obstruction_history;         // Obstruction history
    trend_point_array_t snr_history;                 // SNR history
    starlink_movement_detector_config_t movement_detector; // Movement detector config
    bool is_moving;                       // Current movement state
    time_t last_movement_time;            // Last movement time
} starlink_obstruction_t;

// Function prototypes

/**
 * Initialize Starlink obstruction analysis
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_obstruction_init(void);

/**
 * Record obstruction observation
 * @param sample Obstruction sample data
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_obstruction_record_observation(const starlink_obstruction_sample_t *sample);

/**
 * Get obstruction analysis status
 * @param status Status structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_obstruction_get_status(starlink_obstruction_status_t *status);

/**
 * Get environmental patterns
 * @param patterns Array to store patterns
 * @param max_patterns Maximum patterns to retrieve
 * @return Number of patterns retrieved
 */
int starlink_obstruction_get_patterns(starlink_environmental_pattern_t *patterns, int max_patterns);

/**
 * Get active matches
 * @param matches Array to store matches
 * @param max_matches Maximum matches to retrieve
 * @return Number of matches retrieved
 */
int starlink_obstruction_get_active_matches(starlink_active_match_t *matches, int max_matches);

/**
 * Get match history
 * @param results Array to store results
 * @param max_results Maximum results to retrieve
 * @return Number of results retrieved
 */
int starlink_obstruction_get_match_history(starlink_match_result_t *results, int max_results);

/**
 * Get obstruction configuration
 * @param config Configuration structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_obstruction_get_config(starlink_obstruction_config_t *config);

/**
 * Set obstruction configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_obstruction_set_config(const starlink_obstruction_config_t *config);

/**
 * Enable/disable obstruction analysis
 * @param enabled Whether to enable analysis
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_obstruction_set_enabled(bool enabled);

/**
 * Reset obstruction analysis
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_obstruction_reset(void);

/**
 * Cleanup obstruction analysis
 */
void starlink_obstruction_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // STARLINK_OBSTRUCTION_H
