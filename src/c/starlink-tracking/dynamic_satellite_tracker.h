#ifndef DYNAMIC_SATELLITE_TRACKER_H
#define DYNAMIC_SATELLITE_TRACKER_H

#include "starlink_tracker.h"
#include "obstruction_analyzer.h"
#include <pthread.h>

// Dynamic tracking configuration based on Gemini's research
typedef struct {
    int tracking_interval_seconds;      // How often to run 15-second tracking cycles
    int map_poll_frequency_hz;          // Polls per second during tracking (1 Hz recommended)
    int scheduling_window_seconds;      // Starlink's 15-second scheduling window
    double trajectory_match_threshold;  // Angular distance threshold for trajectory matching
    bool enable_xor_analysis;           // Use XOR between consecutive maps
    int candidate_satellite_limit;      // Max candidates to consider for performance
} dynamic_tracking_config_t;

// Observed trajectory point
typedef struct {
    time_t timestamp;
    int pixel_row;
    int pixel_col;
    double azimuth;
    double elevation;
    bool valid;
} trajectory_point_t;

// Observed satellite trajectory (15-second window)
typedef struct {
    trajectory_point_t points[16];      // Up to 16 points (15 seconds + start)
    int num_points;
    time_t start_time;
    time_t end_time;
    double total_angular_distance;
    bool complete;
} observed_trajectory_t;

// Candidate satellite for matching
typedef struct {
    char satellite_id[32];
    char norad_id[16];
    orbital_elements_t elements;
    trajectory_point_t predicted_points[16];
    int num_predicted_points;
    double match_score;                 // Angular distance from observed trajectory
    bool is_best_match;
} candidate_satellite_t;

// Satellite identification result
typedef struct {
    char identified_satellite_id[32];
    char norad_id[16];
    double confidence_score;            // 0.0 to 1.0
    double angular_error_degrees;       // Average angular error
    int num_trajectory_points_matched;
    time_t identification_time;
    observed_trajectory_t observed_path;
    candidate_satellite_t best_match;
    char identification_details[256];
} satellite_identification_t;

// Dynamic tracker structure
typedef struct {
    dynamic_tracking_config_t config;
    
    // Current tracking cycle
    bool tracking_active;
    time_t cycle_start_time;
    observed_trajectory_t current_trajectory;
    
    // Map storage for XOR analysis
    double *previous_map;               // Previous 123x123 map
    double *current_map;                // Current 123x123 map
    double *difference_map;             // XOR difference map
    
    // Satellite identification
    satellite_identification_t last_identification;
    candidate_satellite_t *candidates;
    int num_candidates;
    
    // Performance tracking
    struct timeval cycle_start_tv;
    double last_cycle_duration_ms;
    int total_tracking_cycles;
    int successful_identifications;
    
    // Threading
    pthread_t tracking_thread;
    pthread_mutex_t data_mutex;
    bool should_stop;
    
    // Callbacks
    void (*satellite_identified_callback)(const satellite_identification_t *id, void *user_data);
    void (*tracking_cycle_callback)(const observed_trajectory_t *trajectory, void *user_data);
    void *callback_user_data;
    
    // Logging
    void (*log_callback)(int level, const char *message, void *user_data);
    void *log_user_data;
} dynamic_satellite_tracker_t;

// API Functions

// Initialization and cleanup
dynamic_satellite_tracker_t* dynamic_tracker_init(const dynamic_tracking_config_t *config);
void dynamic_tracker_cleanup(dynamic_satellite_tracker_t *tracker);

// Configuration
void dynamic_tracking_config_init_defaults(dynamic_tracking_config_t *config);
int dynamic_tracker_update_config(dynamic_satellite_tracker_t *tracker, const dynamic_tracking_config_t *config);

// Dynamic tracking control
int dynamic_tracker_start_tracking(dynamic_satellite_tracker_t *tracker);
int dynamic_tracker_stop_tracking(dynamic_satellite_tracker_t *tracker);
bool dynamic_tracker_is_tracking(const dynamic_satellite_tracker_t *tracker);

// Single tracking cycle (15 seconds)
int dynamic_tracker_run_identification_cycle(dynamic_satellite_tracker_t *tracker, 
                                            const constellation_data_t *constellation,
                                            const dish_location_t *dish_location);

// Trajectory analysis
int dynamic_tracker_clear_obstruction_map(dynamic_satellite_tracker_t *tracker);
int dynamic_tracker_capture_map_sequence(dynamic_satellite_tracker_t *tracker, observed_trajectory_t *trajectory);
int dynamic_tracker_extract_trajectory_from_xor(const double *diff_map, trajectory_point_t *point);

// Satellite identification
int dynamic_tracker_identify_serving_satellite(
    dynamic_satellite_tracker_t *tracker,
    const observed_trajectory_t *observed,
    const constellation_data_t *constellation,
    const dish_location_t *dish_location,
    satellite_identification_t *result
);

// Candidate satellite processing
int dynamic_tracker_generate_candidates(
    const constellation_data_t *constellation,
    const dish_location_t *dish_location,
    time_t start_time,
    int duration_seconds,
    candidate_satellite_t **candidates,
    int *num_candidates
);

int dynamic_tracker_calculate_predicted_trajectory(
    const orbital_elements_t *elements,
    const dish_location_t *observer,
    time_t start_time,
    int duration_seconds,
    trajectory_point_t *points,
    int max_points
);

// Trajectory matching algorithms
double dynamic_tracker_calculate_trajectory_match_score(
    const observed_trajectory_t *observed,
    const candidate_satellite_t *candidate
);

double dynamic_tracker_angular_separation(double az1, double el1, double az2, double el2);

// Map operations based on Gemini's 123x123 format
int dynamic_tracker_get_current_obstruction_map(dynamic_satellite_tracker_t *tracker, double *map_data);
int dynamic_tracker_perform_map_xor(const double *map1, const double *map2, double *result_map);

// Coordinate conversion for 123x123 polar projection (Gemini's algorithm)
typedef struct {
    int row;
    int col;
    bool valid;
} pixel_coords_t;

pixel_coords_t dynamic_tracker_az_el_to_pixel(double azimuth, double elevation);
int dynamic_tracker_pixel_to_az_el(int row, int col, double *azimuth, double *elevation);

// Performance monitoring
typedef struct {
    int total_cycles;
    int successful_identifications;
    double identification_rate_percent;
    double average_cycle_duration_ms;
    double average_match_confidence;
    int active_candidates_per_cycle;
    time_t last_identification;
} dynamic_tracking_stats_t;

dynamic_tracking_stats_t dynamic_tracker_get_stats(const dynamic_satellite_tracker_t *tracker);

// Callback management
int dynamic_tracker_set_identification_callback(
    dynamic_satellite_tracker_t *tracker,
    void (*callback)(const satellite_identification_t *id, void *user_data),
    void *user_data
);

// Error codes
#define DYNAMIC_TRACKER_SUCCESS                 0
#define DYNAMIC_TRACKER_ERROR_INVALID_PARAM    -1
#define DYNAMIC_TRACKER_ERROR_NOT_INITIALIZED  -2
#define DYNAMIC_TRACKER_ERROR_GRPC_FAILED      -3
#define DYNAMIC_TRACKER_ERROR_NO_TRAJECTORY    -4
#define DYNAMIC_TRACKER_ERROR_NO_CANDIDATES    -5
#define DYNAMIC_TRACKER_ERROR_THREAD_FAILED    -6
#define DYNAMIC_TRACKER_ERROR_TIMEOUT          -7

#endif // DYNAMIC_SATELLITE_TRACKER_H