#ifndef STARLINK_TRACKER_H
#define STARLINK_TRACKER_H

#include "starlink_types.h"
#include <pthread.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>

// Forward declaration for UCI context
struct uci_context;
struct ubus_context;

// Forward declarations
typedef struct starlink_tracker starlink_tracker_t;
typedef struct space_track_connector space_track_connector_t;
typedef struct obstruction_analyzer obstruction_analyzer_t;
typedef struct prediction_engine prediction_engine_t;

// Comprehensive GPS collection functions (removed unused declarations)

// Configuration for the tracking system
typedef struct {
    char space_track_username[64];
    char space_track_password[64];
    char starlink_dish_ip[16];
    int starlink_dish_port;
    int update_interval_minutes;
    int prediction_horizon_hours;
    double min_elevation_degrees;
    double obstruction_threshold;
    bool validation_enabled;
    int cache_duration_hours;
    int rate_limit_requests_per_minute;
} starlink_tracker_config_t;

// Dish location and orientation
typedef struct dish_location {
    double latitude;
    double longitude;
    double altitude;
    double boresight_azimuth;
    double boresight_elevation;
    time_t last_update;
} dish_location_t;

// Obstruction map cell
typedef struct {
    double azimuth;
    double elevation;
    double snr_quality;
    bool is_obstructed;
} obstruction_cell_t;

// Obstruction map structure supporting both legacy grid and enhanced polar projection
typedef struct {
    // Enhanced polar projection data (123x123)
    double *snr_data;              // Raw SNR data array [123*123] = 15,129 values
    int map_diameter;              // Map diameter (123)
    int center_pixel;              // Center pixel coordinate (61)
    double max_radius_pixels;      // Maximum radius in pixels (61.5)
    double min_elevation_deg;      // Minimum elevation (25°)
    double max_elevation_deg;      // Maximum elevation (90°)

    // Legacy rectangular grid representation (azimuth/elevation grid)
    obstruction_cell_t *cells;     // Grid cells with az/el and SNR
    int num_cells;                 // Total number of cells
    int grid_width;                // Number of azimuth columns
    int grid_height;               // Number of elevation rows
    double azimuth_resolution;     // Degrees per column (360 / grid_width)
    double elevation_resolution;   // Degrees per row (90 / grid_height)

    time_t last_update;
} obstruction_map_t;

// Satellite position and visibility
typedef struct {
    char satellite_id[32];
    char norad_id[16];
    double azimuth;
    double elevation;
    double range;
    double velocity;
    bool is_visible;
    bool is_obstructed;
    double signal_quality;
    time_t timestamp;
} satellite_position_t;

// TLE (Two-Line Element) data
typedef struct {
    char satellite_name[64];
    char line1[70];
    char line2[70];
    time_t epoch;
    time_t fetched_time;
    bool is_valid;
} tle_data_t;

// Satellite constellation data
typedef struct {
    tle_data_t *satellites;
    int num_satellites;
    time_t last_update;
    bool cache_valid;
} constellation_data_t;

// Outage prediction
typedef struct {
    time_t start_time;
    time_t end_time;
    int duration_seconds;
    int risk_level; // 1=low, 2=medium, 3=high
    char description[256];
    int predicted_available_sats;
    double confidence_score;
} outage_prediction_t;

// Prediction validation data
typedef struct {
    time_t prediction_time;
    time_t actual_time;
    bool prediction_correct;
    bool actual_outage_occurred;
    int predicted_duration;
    int actual_duration;
    double accuracy_score;
} prediction_validation_t;

// Tracking statistics
typedef struct {
    int total_predictions;
    int correct_predictions;
    int false_positives;
    int missed_outages;
    double accuracy_percentage;
    time_t last_validation;
    prediction_validation_t recent_validations[100]; // Ring buffer
    int validation_index;
} tracking_stats_t;

// Logging and debugging
typedef enum {
    TRACKER_LOG_DEBUG = 0,
    TRACKER_LOG_INFO,
    TRACKER_LOG_WARN,
    TRACKER_LOG_ERROR
} tracker_log_level_t;

// Main tracker structure
typedef struct starlink_tracker {
    starlink_tracker_config_t config;
    dish_location_t dish_location;
    obstruction_map_t obstruction_map;
    constellation_data_t constellation;
    satellite_position_t *current_positions;
    int num_current_positions;
    outage_prediction_t *predictions;
    int num_predictions;
    tracking_stats_t stats;
    
    // Component instances
    space_track_connector_t *space_track;
    obstruction_analyzer_t *analyzer;
    prediction_engine_t *engine;
    
    // State
    bool initialized;
    bool monitoring_active;
    time_t last_update;
    pthread_t update_thread;
    pthread_t monitoring_thread;
    pthread_mutex_t data_mutex;
    
    // Callbacks
    void (*outage_callback)(const outage_prediction_t *prediction, void *user_data);
    void *callback_user_data;
    
    // Logging
    tracker_log_level_t log_level;
    void (*log_callback)(tracker_log_level_t level, const char *message, void *user_data);
    void *log_user_data;
    
    // Outage history
    outage_prediction_t *outage_history;
    int outage_history_count;
    int max_outage_history;
} starlink_tracker_t;

// API Functions

// Initialization and cleanup
starlink_tracker_t* starlink_tracker_init(const starlink_tracker_config_t *config);
void starlink_tracker_cleanup(starlink_tracker_t *tracker);

// UCI-specific functions for compatibility with main daemon
starlink_tracker_t* starlink_tracker_init_from_uci(struct uci_context *uci_ctx);
int starlink_tracker_ubus_init(struct ubus_context *ctx, starlink_tracker_t *tracker);
void starlink_tracker_ubus_cleanup(struct ubus_context *ctx);

// Configuration management
int starlink_tracker_update_config(starlink_tracker_t *tracker, const starlink_tracker_config_t *config);
const starlink_tracker_config_t* starlink_tracker_get_config(const starlink_tracker_t *tracker);

// Data collection
int starlink_tracker_update_dish_location(starlink_tracker_t *tracker);
int starlink_tracker_update_obstruction_map(starlink_tracker_t *tracker);
int starlink_tracker_update_constellation_data(starlink_tracker_t *tracker);

// Prediction functions
int starlink_tracker_calculate_predictions(starlink_tracker_t *tracker, int horizon_hours);
int starlink_tracker_get_predictions(const starlink_tracker_t *tracker, outage_prediction_t **predictions);
void starlink_tracker_free_predictions(outage_prediction_t *predictions, int count);

// Real-time monitoring
int starlink_tracker_start_monitoring(starlink_tracker_t *tracker);
int starlink_tracker_stop_monitoring(starlink_tracker_t *tracker);
bool starlink_tracker_is_monitoring(const starlink_tracker_t *tracker);

// Callback management
int starlink_tracker_set_outage_callback(starlink_tracker_t *tracker, 
    void (*callback)(const outage_prediction_t *prediction, void *user_data), 
    void *user_data);

// Statistics and validation
const tracking_stats_t* starlink_tracker_get_stats(const starlink_tracker_t *tracker);
int starlink_tracker_validate_prediction(starlink_tracker_t *tracker, const outage_prediction_t *prediction, bool actual_outage);

// Utility functions
int starlink_tracker_get_current_satellite_positions(const starlink_tracker_t *tracker, satellite_position_t **positions);
int starlink_tracker_get_visible_satellite_count(const starlink_tracker_t *tracker);
int starlink_tracker_get_unobstructed_satellite_count(const starlink_tracker_t *tracker);

int starlink_tracker_set_log_level(starlink_tracker_t *tracker, tracker_log_level_t level);
int starlink_tracker_set_log_callback(starlink_tracker_t *tracker, 
    void (*log_callback)(tracker_log_level_t level, const char *message, void *user_data), 
    void *user_data);

// Error codes
#define TRACKER_SUCCESS             0
#define TRACKER_ERROR_INVALID_PARAM -1
#define TRACKER_ERROR_NOT_INITIALIZED -2
#define TRACKER_ERROR_NETWORK_FAILURE -3
#define TRACKER_ERROR_API_FAILURE   -4
#define TRACKER_ERROR_PARSE_FAILURE -5
#define TRACKER_ERROR_MEMORY_FAILURE -6
#define TRACKER_ERROR_THREAD_FAILURE -7
#define TRACKER_ERROR_CONFIG_FAILURE -8

#endif // STARLINK_TRACKER_H