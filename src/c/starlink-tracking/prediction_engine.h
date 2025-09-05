#ifndef PREDICTION_ENGINE_H
#define PREDICTION_ENGINE_H

#include "starlink_tracker.h"
#include "obstruction_analyzer.h"
#include <math.h>
#include <pthread.h>

// SGP4 orbital propagation constants
#define SGP4_EARTH_RADIUS_KM 6378.137
#define SGP4_EARTH_FLATTENING 1.0/298.257223563
#define SGP4_GRAVITATIONAL_CONSTANT 3.986004418e14
#define SGP4_J2 1.08262998905e-3

// Prediction engine configuration
typedef struct {
    int prediction_horizon_hours;
    int time_step_seconds;
    double min_elevation_degrees;
    int min_satellites_for_connectivity;
    double connectivity_threshold;
    bool use_advanced_propagation;
    bool consider_doppler_effects;
} prediction_config_t;

// Orbital elements structure (parsed from TLE)
typedef struct {
    double inclination;         // Orbital inclination (degrees)
    double raan;               // Right Ascension of Ascending Node (degrees)
    double eccentricity;       // Orbital eccentricity
    double arg_perigee;        // Argument of perigee (degrees)
    double mean_anomaly;       // Mean anomaly (degrees)
    double mean_motion;        // Mean motion (revolutions per day)
    double epoch_julian;       // Epoch (Julian date)
    int revolution_number;     // Revolution number at epoch
    double bstar;              // BSTAR drag term
} orbital_elements_t;

// Satellite state vector
typedef struct {
    double position[3];        // Position vector (km) in ECI
    double velocity[3];        // Velocity vector (km/s) in ECI
    time_t timestamp;
} satellite_state_t;

// Topocentric coordinates
typedef struct {
    double azimuth;           // Azimuth angle (degrees)
    double elevation;         // Elevation angle (degrees)
    double range;             // Range distance (km)
    double range_rate;        // Range rate (km/s)
} topocentric_coords_t;

// Connectivity assessment for a time point
typedef struct {
    time_t timestamp;
    int visible_satellites;
    int unobstructed_satellites;
    int available_satellites;
    double connectivity_score;
    bool outage_predicted;
    bool degradation_predicted;
    satellite_position_t *satellite_positions;
    int num_positions;
} connectivity_assessment_t;

// Prediction engine structure
typedef struct prediction_engine {
    prediction_config_t config;
    dish_location_t *dish_location;
    obstruction_analyzer_t *obstruction_analyzer;
    
    // Orbital propagation state
    orbital_elements_t *satellite_elements;
    int num_satellites;
    
    // Prediction cache
    connectivity_assessment_t *assessments;
    int num_assessments;
    time_t prediction_start_time;
    time_t prediction_end_time;
    
    // Statistics
    int total_predictions;
    int outage_predictions;
    int degradation_predictions;
    time_t last_prediction_time;
    
    // Threading
    pthread_mutex_t prediction_mutex;
    
    // Logging callback
    void (*log_callback)(int level, const char *message, void *user_data);
    void *log_user_data;
} prediction_engine_t;

// API Functions

// Initialization and cleanup
prediction_engine_t* prediction_engine_init(const prediction_config_t *config);
void prediction_engine_cleanup(prediction_engine_t *analyzer);

// Configuration management
int prediction_engine_update_config(prediction_engine_t *engine, const prediction_config_t *config);
const prediction_config_t* prediction_engine_get_config(const prediction_engine_t *engine);

// Data input
int prediction_engine_set_dish_location(prediction_engine_t *engine, const dish_location_t *location);
int prediction_engine_set_obstruction_analyzer(prediction_engine_t *engine, obstruction_analyzer_t *analyzer);
int prediction_engine_load_constellation(prediction_engine_t *engine, const constellation_data_t *constellation);

// Orbital mechanics functions
int prediction_engine_parse_tle(const tle_data_t *tle, orbital_elements_t *elements);
int prediction_engine_propagate_satellite(
    const orbital_elements_t *elements,
    time_t target_time,
    satellite_state_t *state
);

// Coordinate transformations
int prediction_engine_eci_to_topocentric(
    const satellite_state_t *sat_state,
    const dish_location_t *observer,
    topocentric_coords_t *topo
);

// Prediction calculations
int prediction_engine_calculate_predictions(
    prediction_engine_t *engine,
    time_t start_time,
    int horizon_hours,
    outage_prediction_t **predictions,
    int *num_predictions
);

connectivity_assessment_t prediction_engine_assess_connectivity(
    prediction_engine_t *engine,
    time_t timestamp
);

// Satellite visibility
bool prediction_engine_is_satellite_visible(
    const satellite_position_t *satellite,
    double min_elevation
);

int prediction_engine_get_visible_satellites(
    prediction_engine_t *engine,
    time_t timestamp,
    satellite_position_t **visible_satellites,
    int *num_visible
);

// Outage detection algorithms
int prediction_engine_detect_outage_windows(
    const connectivity_assessment_t *assessments,
    int num_assessments,
    outage_prediction_t **outages,
    int *num_outages
);

// Risk assessment
typedef enum {
    RISK_LEVEL_LOW = 1,
    RISK_LEVEL_MEDIUM = 2,
    RISK_LEVEL_HIGH = 3,
    RISK_LEVEL_CRITICAL = 4
} risk_level_t;

risk_level_t prediction_engine_calculate_risk_level(
    int available_satellites,
    double connectivity_score,
    int duration_seconds
);

// Utility functions
double prediction_engine_julian_date_from_time(time_t unix_time);
time_t prediction_engine_time_from_julian_date(double julian_date);
double prediction_engine_julian_date_from_year_day(int year, double day_of_year);
double prediction_engine_sidereal_time(double julian_date, double longitude);

// SGP4 implementation helpers
int sgp4_init(const orbital_elements_t *elements);
int sgp4_propagate(const orbital_elements_t *elements, double minutes_since_epoch, satellite_state_t *state);

// Coordinate system conversions
void eci_to_ecef(const double eci[3], double julian_date, double ecef[3]);
void ecef_to_lla(const double ecef[3], double lla[3]);
void eci_to_lla(const double eci[3], double julian_date, double lla[3]);
void lla_to_topocentric(const double sat_lla[3], const double obs_lla[3], double topo[3]);

// Default configuration
void prediction_engine_config_init_defaults(prediction_config_t *config);

// Error codes
#define PREDICTION_SUCCESS                 0
#define PREDICTION_ERROR_INVALID_PARAM    -1
#define PREDICTION_ERROR_NOT_INITIALIZED  -2
#define PREDICTION_ERROR_MEMORY_FAILED    -3
#define PREDICTION_ERROR_PROPAGATION_FAILED -4
#define PREDICTION_ERROR_NO_SATELLITES    -5
#define PREDICTION_ERROR_INVALID_TLE      -6
#define PREDICTION_ERROR_THREAD_FAILED    -7

#endif // PREDICTION_ENGINE_H