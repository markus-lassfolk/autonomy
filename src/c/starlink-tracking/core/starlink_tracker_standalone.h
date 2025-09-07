#ifndef STARLINK_TRACKER_STANDALONE_H
#define STARLINK_TRACKER_STANDALONE_H

#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

// Forward declarations for core components
struct prediction_engine_t;
struct obstruction_analyzer_t;

// Standalone configuration (no UCI dependencies)
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
    
    // Standalone-specific options
    char config_file[256];
    char log_file[256];
    char cache_directory[256];
    int http_api_port;
    bool enable_web_interface;
    int log_level; // 0=debug, 1=info, 2=warn, 3=error
} standalone_config_t;

// Simplified dish location (no complex dependencies)
typedef struct {
    double latitude;
    double longitude;
    double altitude;
    double boresight_azimuth;
    double boresight_elevation;
    time_t last_update;
} standalone_dish_location_t;

// Simplified obstruction map for standalone
typedef struct {
    double *snr_data;              // Raw SNR data [123*123]
    int map_diameter;              // 123
    int center_pixel;              // 61
    double max_radius_pixels;      // 61.5
    double min_elevation_deg;      // 25°
    double max_elevation_deg;      // 90°
    time_t last_update;
} standalone_obstruction_map_t;

// Satellite position
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
} standalone_satellite_position_t;

// Outage prediction
typedef struct {
    time_t start_time;
    time_t end_time;
    int duration_seconds;
    int risk_level; // 1=low, 2=medium, 3=high, 4=critical
    char description[256];
    int predicted_available_sats;
    double confidence_score;
} standalone_outage_prediction_t;

// TLE data
typedef struct {
    char satellite_name[64];
    char line1[70];
    char line2[70];
    time_t epoch;
    time_t fetched_time;
    bool is_valid;
} standalone_tle_data_t;

// Constellation data
typedef struct {
    standalone_tle_data_t *satellites;
    int num_satellites;
    time_t last_update;
    bool cache_valid;
} standalone_constellation_data_t;

// Main tracker structure (simplified for standalone)
typedef struct {
    standalone_config_t config;
    standalone_dish_location_t dish_location;
    standalone_obstruction_map_t obstruction_map;
    standalone_constellation_data_t constellation;
    standalone_satellite_position_t *current_positions;
    int num_current_positions;
    standalone_outage_prediction_t *predictions;
    int num_predictions;
    
    // Core components
    struct prediction_engine_t *prediction_engine;
    struct obstruction_analyzer_t *obstruction_analyzer;

    // State
    bool initialized;
    bool monitoring_active;
    time_t last_update;
    pthread_t update_thread;
    pthread_t monitoring_thread;
    pthread_mutex_t data_mutex;
    
    // Statistics
    int total_predictions;
    int correct_predictions;
    double accuracy_percentage;
    time_t last_validation;
    
    // Callbacks
    void (*outage_callback)(const standalone_outage_prediction_t *prediction, void *user_data);
    void (*log_callback)(int level, const char *message, void *user_data);
    void *callback_user_data;
} standalone_tracker_t;

// API Functions

// Initialization and cleanup
standalone_tracker_t* standalone_tracker_init(const standalone_config_t *config);
void standalone_tracker_cleanup(standalone_tracker_t *tracker);

// Configuration
int standalone_config_load_from_file(const char *filename, standalone_config_t *config);
int standalone_config_save_to_file(const char *filename, const standalone_config_t *config);
void standalone_config_init_defaults(standalone_config_t *config);

// Data updates
int standalone_tracker_update_dish_location(standalone_tracker_t *tracker);
int standalone_tracker_update_obstruction_map(standalone_tracker_t *tracker);
int standalone_tracker_update_constellation_data(standalone_tracker_t *tracker);
int standalone_tracker_calculate_predictions(standalone_tracker_t *tracker);

// Monitoring
int standalone_tracker_start_monitoring(standalone_tracker_t *tracker);
int standalone_tracker_stop_monitoring(standalone_tracker_t *tracker);
bool standalone_tracker_is_monitoring(const standalone_tracker_t *tracker);

// Data access
int standalone_tracker_get_predictions(const standalone_tracker_t *tracker, 
                                      standalone_outage_prediction_t **predictions);
int standalone_tracker_get_satellite_positions(const standalone_tracker_t *tracker, 
                                              standalone_satellite_position_t **positions);
void standalone_tracker_free_predictions(standalone_outage_prediction_t *predictions, int count);
void standalone_tracker_free_positions(standalone_satellite_position_t *positions, int count);

// Statistics
typedef struct {
    int total_predictions;
    int correct_predictions;
    int false_positives;
    int missed_outages;
    double accuracy_percentage;
    time_t last_update;
    int visible_satellites;
    int unobstructed_satellites;
    double obstruction_percentage;
} standalone_stats_t;

standalone_stats_t standalone_tracker_get_stats(const standalone_tracker_t *tracker);

// Logging
void standalone_tracker_set_log_callback(standalone_tracker_t *tracker, 
                                        void (*callback)(int level, const char *message, void *user_data),
                                        void *user_data);

// Callbacks
int standalone_tracker_set_outage_callback(standalone_tracker_t *tracker,
                                          void (*callback)(const standalone_outage_prediction_t *prediction, void *user_data),
                                          void *user_data);

// Error codes
#define STANDALONE_SUCCESS             0
#define STANDALONE_ERROR_INVALID_PARAM -1
#define STANDALONE_ERROR_NOT_INITIALIZED -2
#define STANDALONE_ERROR_CONFIG_FAILED -3
#define STANDALONE_ERROR_NETWORK_FAILED -4
#define STANDALONE_ERROR_API_FAILED    -5
#define STANDALONE_ERROR_THREAD_FAILED -6

#endif // STARLINK_TRACKER_STANDALONE_H