#ifndef OBSTRUCTION_ANALYZER_H
#define OBSTRUCTION_ANALYZER_H

#include "starlink_tracker.h"
#include <math.h>
#include <stdbool.h>

// Constants for obstruction analysis (corrected based on Gemini's research)
#define OBSTRUCTION_MAP_DIAMETER 123    // Starlink uses 123x123 polar projection
#define OBSTRUCTION_MAP_SIZE (OBSTRUCTION_MAP_DIAMETER * OBSTRUCTION_MAP_DIAMETER)
#define OBSTRUCTION_CENTER_PIXEL 61     // Center pixel (123/2 - 1)
#define OBSTRUCTION_MAX_RADIUS_PIXELS 61.5 // Maximum radius in pixels
// Legacy grid defaults for rectangular representation
#define OBSTRUCTION_GRID_WIDTH 360
#define OBSTRUCTION_GRID_HEIGHT 90
#define OBSTRUCTION_SNR_THRESHOLD 0.7   // Default SNR threshold for obstruction
#define MIN_ELEVATION_DEGREES 25.0      // Dish's operational minimum (per Gemini)
#define MAX_ELEVATION_DEGREES 90.0      // Zenith

// Obstruction analysis configuration
typedef struct {
    double snr_threshold;
    double min_elevation;
    double max_elevation;
    bool use_adaptive_threshold;
    double adaptive_threshold_factor;
    int smoothing_window_size;
} obstruction_analysis_config_t;

// Obstruction analyzer structure
typedef struct obstruction_analyzer {
    obstruction_analysis_config_t config;
    obstruction_map_t current_map;
    
    // Interpolation and smoothing
    double *smoothing_buffer;
    int smoothing_buffer_size;
    
    // Statistics
    int total_analyses;
    int obstructed_count;
    int clear_count;
    double average_snr;
    time_t last_analysis;
    
    // Logging callback
    void (*log_callback)(int level, const char *message, void *user_data);
    void *log_user_data;
} obstruction_analyzer_t;

// Analysis result structure
typedef struct {
    bool is_obstructed;
    double snr_quality;
    double confidence_score;
    char analysis_details[256];
} obstruction_analysis_result_t;

// API Functions

// Initialization and cleanup
obstruction_analyzer_t* obstruction_analyzer_init(const obstruction_analysis_config_t *config);
void obstruction_analyzer_cleanup(obstruction_analyzer_t *analyzer);

// Configuration management
int obstruction_analyzer_update_config(obstruction_analyzer_t *analyzer, const obstruction_analysis_config_t *config);
const obstruction_analysis_config_t* obstruction_analyzer_get_config(const obstruction_analyzer_t *analyzer);

// Obstruction map processing
int obstruction_analyzer_update_map(obstruction_analyzer_t *analyzer, const char *grpc_response);
int obstruction_analyzer_parse_dish_response(const char *response, obstruction_map_t *map, dish_location_t *location);

// Satellite obstruction analysis
obstruction_analysis_result_t obstruction_analyzer_check_satellite(
    const obstruction_analyzer_t *analyzer,
    double satellite_azimuth,
    double satellite_elevation
);

int obstruction_analyzer_check_multiple_satellites(
    const obstruction_analyzer_t *analyzer,
    const satellite_position_t *satellites,
    int num_satellites,
    obstruction_analysis_result_t *results
);

// Grid operations
int obstruction_analyzer_get_grid_cell(
    const obstruction_map_t *map,
    double azimuth,
    double elevation,
    obstruction_cell_t *cell
);

double obstruction_analyzer_interpolate_snr(
    const obstruction_map_t *map,
    double azimuth,
    double elevation
);

// Coordinate transformations
void obstruction_analyzer_dish_to_absolute_coords(
    double dish_relative_az,
    double dish_relative_el,
    double boresight_azimuth,
    double boresight_elevation,
    double *absolute_az,
    double *absolute_el
);

void obstruction_analyzer_absolute_to_dish_coords(
    double absolute_az,
    double absolute_el,
    double boresight_azimuth,
    double boresight_elevation,
    double *dish_relative_az,
    double *dish_relative_el
);

// Utility functions
bool obstruction_analyzer_is_elevation_valid(double elevation);
bool obstruction_analyzer_is_azimuth_valid(double azimuth);
double obstruction_analyzer_normalize_azimuth(double azimuth);
double obstruction_analyzer_clamp_elevation(double elevation);

// Adaptive threshold calculation
double obstruction_analyzer_calculate_adaptive_threshold(
    const obstruction_analyzer_t *analyzer,
    const obstruction_map_t *map
);

// Smoothing and filtering
int obstruction_analyzer_apply_smoothing(
    obstruction_analyzer_t *analyzer,
    obstruction_map_t *map
);

// Statistics and reporting
typedef struct {
    int total_cells;
    int obstructed_cells;
    int clear_cells;
    double average_snr;
    double min_snr;
    double max_snr;
    double obstruction_percentage;
    time_t analysis_time;
} obstruction_map_stats_t;

obstruction_map_stats_t obstruction_analyzer_get_map_stats(const obstruction_map_t *map);

// Visualization helpers (for debugging)
int obstruction_analyzer_export_map_csv(const obstruction_map_t *map, const char *filename);
int obstruction_analyzer_print_map_summary(const obstruction_map_t *map);

// Error codes specific to obstruction analysis
#define OBSTRUCTION_SUCCESS                 0
#define OBSTRUCTION_ERROR_INVALID_PARAM    -1
#define OBSTRUCTION_ERROR_NOT_INITIALIZED  -2
#define OBSTRUCTION_ERROR_PARSE_FAILED     -3
#define OBSTRUCTION_ERROR_MEMORY_FAILED    -4
#define OBSTRUCTION_ERROR_INVALID_COORDS   -5
#define OBSTRUCTION_ERROR_NO_DATA          -6

#endif // OBSTRUCTION_ANALYZER_H