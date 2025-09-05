#include "prediction_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <time.h>

// Mathematical constants
#define M_PI 3.14159265358979323846
#define DEG_TO_RAD (M_PI / 180.0)
#define RAD_TO_DEG (180.0 / M_PI)
#define JULIAN_EPOCH_OFFSET 2440587.5  // Unix epoch in Julian days

// Default prediction configuration
void prediction_engine_config_init_defaults(prediction_config_t *config) {
    if (!config) {
        return;
    }
    
    config->prediction_horizon_hours = 24;
    config->time_step_seconds = 300; // 5 minutes
    config->min_elevation_degrees = 10.0;
    config->min_satellites_for_connectivity = 1;
    config->connectivity_threshold = 0.5;
    config->use_advanced_propagation = true;
    config->consider_doppler_effects = false;
}

// Initialize prediction engine
prediction_engine_t* prediction_engine_init(const prediction_config_t *config) {
    prediction_engine_t *engine = calloc(1, sizeof(prediction_engine_t));
    if (!engine) {
        return NULL;
    }
    
    // Set configuration
    if (config) {
        memcpy(&engine->config, config, sizeof(prediction_config_t));
    } else {
        prediction_engine_config_init_defaults(&engine->config);
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&engine->prediction_mutex, NULL) != 0) {
        free(engine);
        return NULL;
    }
    
    return engine;
}

// Cleanup prediction engine
void prediction_engine_cleanup(prediction_engine_t *engine) {
    if (!engine) {
        return;
    }
    
    pthread_mutex_destroy(&engine->prediction_mutex);
    
    if (engine->satellite_elements) {
        free(engine->satellite_elements);
    }
    
    if (engine->assessments) {
        // Free satellite positions in each assessment
        for (int i = 0; i < engine->num_assessments; i++) {
            if (engine->assessments[i].satellite_positions) {
                free(engine->assessments[i].satellite_positions);
            }
        }
        free(engine->assessments);
    }
    
    free(engine);
}

// Set dish location
int prediction_engine_set_dish_location(prediction_engine_t *engine, const dish_location_t *location) {
    if (!engine || !location) {
        return PREDICTION_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&engine->prediction_mutex);
    engine->dish_location = (dish_location_t*)location; // Store reference
    pthread_mutex_unlock(&engine->prediction_mutex);
    
    return PREDICTION_SUCCESS;
}

// Set obstruction analyzer
int prediction_engine_set_obstruction_analyzer(prediction_engine_t *engine, obstruction_analyzer_t *analyzer) {
    if (!engine || !analyzer) {
        return PREDICTION_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&engine->prediction_mutex);
    engine->obstruction_analyzer = analyzer;
    pthread_mutex_unlock(&engine->prediction_mutex);
    
    return PREDICTION_SUCCESS;
}

// Load constellation data and parse TLEs
int prediction_engine_load_constellation(prediction_engine_t *engine, const constellation_data_t *constellation) {
    if (!engine || !constellation || !constellation->satellites) {
        return PREDICTION_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&engine->prediction_mutex);
    
    // Free existing satellite elements
    if (engine->satellite_elements) {
        free(engine->satellite_elements);
    }
    
    // Allocate memory for orbital elements
    engine->satellite_elements = calloc(constellation->num_satellites, sizeof(orbital_elements_t));
    if (!engine->satellite_elements) {
        pthread_mutex_unlock(&engine->prediction_mutex);
        return PREDICTION_ERROR_MEMORY_FAILED;
    }
    
    // Parse TLE data into orbital elements
    int valid_satellites = 0;
    for (int i = 0; i < constellation->num_satellites; i++) {
        if (constellation->satellites[i].is_valid) {
            int parse_result = prediction_engine_parse_tle(&constellation->satellites[i], 
                                                          &engine->satellite_elements[valid_satellites]);
            if (parse_result == PREDICTION_SUCCESS) {
                valid_satellites++;
            }
        }
    }
    
    engine->num_satellites = valid_satellites;
    
    if (engine->log_callback) {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Loaded %d valid satellites for prediction", valid_satellites);
        engine->log_callback(1, log_msg, engine->log_user_data);
    }
    
    pthread_mutex_unlock(&engine->prediction_mutex);
    
    return (valid_satellites > 0) ? PREDICTION_SUCCESS : PREDICTION_ERROR_NO_SATELLITES;
}

// Parse TLE data into orbital elements
int prediction_engine_parse_tle(const tle_data_t *tle, orbital_elements_t *elements) {
    if (!tle || !elements || !tle->is_valid) {
        return PREDICTION_ERROR_INVALID_PARAM;
    }
    
    // Parse TLE Line 1
    const char *line1 = tle->line1;
    if (strlen(line1) != 69 || line1[0] != '1') {
        return PREDICTION_ERROR_INVALID_TLE;
    }
    
    // Parse TLE Line 2
    const char *line2 = tle->line2;
    if (strlen(line2) != 69 || line2[0] != '2') {
        return PREDICTION_ERROR_INVALID_TLE;
    }
    
    // Parse orbital elements from TLE Line 2
    char temp_str[16];
    
    // Inclination (columns 9-16)
    strncpy(temp_str, &line2[8], 8);
    temp_str[8] = '\0';
    elements->inclination = atof(temp_str);
    
    // RAAN (columns 18-25)
    strncpy(temp_str, &line2[17], 8);
    temp_str[8] = '\0';
    elements->raan = atof(temp_str);
    
    // Eccentricity (columns 27-33, with implied decimal point)
    strncpy(temp_str, &line2[26], 7);
    temp_str[7] = '\0';
    elements->eccentricity = atof(temp_str) / 1e7;
    
    // Argument of perigee (columns 35-42)
    strncpy(temp_str, &line2[34], 8);
    temp_str[8] = '\0';
    elements->arg_perigee = atof(temp_str);
    
    // Mean anomaly (columns 44-51)
    strncpy(temp_str, &line2[43], 8);
    temp_str[8] = '\0';
    elements->mean_anomaly = atof(temp_str);
    
    // Mean motion (columns 53-63)
    strncpy(temp_str, &line2[52], 11);
    temp_str[11] = '\0';
    elements->mean_motion = atof(temp_str);
    
    // Revolution number (columns 64-68)
    strncpy(temp_str, &line2[63], 5);
    temp_str[5] = '\0';
    elements->revolution_number = atoi(temp_str);
    
    // Parse epoch and BSTAR from Line 1
    // Epoch (columns 19-32)
    strncpy(temp_str, &line1[18], 14);
    temp_str[14] = '\0';
    
    // Convert TLE epoch to Julian date
    char year_str[3];
    strncpy(year_str, temp_str, 2);
    year_str[2] = '\0';
    int year = atoi(year_str);
    year += (year < 57) ? 2000 : 1900; // Y2K windowing
    
    double day_of_year = atof(&temp_str[2]);
    elements->epoch_julian = prediction_engine_julian_date_from_year_day(year, day_of_year);
    
    // BSTAR (columns 54-61)
    strncpy(temp_str, &line1[53], 8);
    temp_str[8] = '\0';
    elements->bstar = atof(temp_str);
    
    return PREDICTION_SUCCESS;
}

// Convert year and day of year to Julian date
double prediction_engine_julian_date_from_year_day(int year, double day_of_year) {
    // Calculate Julian day number for January 1st of the year
    int a = (14 - 1) / 12;
    int y = year + 4800 - a;
    int m = 1 + 12 * a - 3;
    
    int jdn = 1 + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;
    
    // Add day of year (subtract 1 since we start from Jan 1)
    return jdn + day_of_year - 1.0;
}

// Convert Unix time to Julian date
double prediction_engine_julian_date_from_time(time_t unix_time) {
    return (double)unix_time / 86400.0 + JULIAN_EPOCH_OFFSET;
}

// Convert Julian date to Unix time
time_t prediction_engine_time_from_julian_date(double julian_date) {
    return (time_t)((julian_date - JULIAN_EPOCH_OFFSET) * 86400.0);
}

// Simplified SGP4 propagation (basic implementation)
int prediction_engine_propagate_satellite(
    const orbital_elements_t *elements,
    time_t target_time,
    satellite_state_t *state) {
    
    if (!elements || !state) {
        return PREDICTION_ERROR_INVALID_PARAM;
    }
    
    // Calculate time since epoch in minutes
    double target_julian = prediction_engine_julian_date_from_time(target_time);
    double minutes_since_epoch = (target_julian - elements->epoch_julian) * 1440.0; // 1440 min/day
    
    // Simplified orbital propagation (Kepler's laws)
    // This is a basic implementation - for production, use a proper SGP4 library
    
    double n = elements->mean_motion * 2.0 * M_PI / 1440.0; // Convert to rad/min
    double mean_anomaly = elements->mean_anomaly * DEG_TO_RAD + n * minutes_since_epoch;
    
    // Solve Kepler's equation (simplified)
    double eccentric_anomaly = mean_anomaly;
    for (int i = 0; i < 10; i++) { // Newton-Raphson iteration
        double f = eccentric_anomaly - elements->eccentricity * sin(eccentric_anomaly) - mean_anomaly;
        double df = 1.0 - elements->eccentricity * cos(eccentric_anomaly);
        eccentric_anomaly = eccentric_anomaly - f / df;
    }
    
    // Calculate true anomaly
    double true_anomaly = 2.0 * atan2(
        sqrt(1.0 + elements->eccentricity) * sin(eccentric_anomaly / 2.0),
        sqrt(1.0 - elements->eccentricity) * cos(eccentric_anomaly / 2.0)
    );
    
    // Calculate distance
    double semi_major_axis = pow(SGP4_GRAVITATIONAL_CONSTANT / (n * n), 1.0/3.0) / 1000.0; // Convert to km
    double distance = semi_major_axis * (1.0 - elements->eccentricity * cos(eccentric_anomaly));
    
    // Calculate position in orbital plane
    double x_orbital = distance * cos(true_anomaly);
    double y_orbital = distance * sin(true_anomaly);
    
    // Transform to ECI coordinates (simplified)
    double cos_raan = cos(elements->raan * DEG_TO_RAD);
    double sin_raan = sin(elements->raan * DEG_TO_RAD);
    double cos_inc = cos(elements->inclination * DEG_TO_RAD);
    double sin_inc = sin(elements->inclination * DEG_TO_RAD);
    double cos_arg = cos(elements->arg_perigee * DEG_TO_RAD);
    double sin_arg = sin(elements->arg_perigee * DEG_TO_RAD);
    
    // Rotation matrices
    double px = x_orbital * (cos_raan * cos_arg - sin_raan * sin_arg * cos_inc) 
              - y_orbital * (cos_raan * sin_arg + sin_raan * cos_arg * cos_inc);
    double py = x_orbital * (sin_raan * cos_arg + cos_raan * sin_arg * cos_inc) 
              - y_orbital * (sin_raan * sin_arg - cos_raan * cos_arg * cos_inc);
    double pz = x_orbital * (sin_arg * sin_inc) + y_orbital * (cos_arg * sin_inc);
    
    state->position[0] = px;
    state->position[1] = py;
    state->position[2] = pz;
    
    // Calculate velocity (simplified)
    double velocity_magnitude = sqrt(SGP4_GRAVITATIONAL_CONSTANT / (distance * 1000.0)) / 1000.0; // km/s
    state->velocity[0] = -velocity_magnitude * sin(true_anomaly + elements->arg_perigee * DEG_TO_RAD);
    state->velocity[1] = velocity_magnitude * cos(true_anomaly + elements->arg_perigee * DEG_TO_RAD);
    state->velocity[2] = 0.0; // Simplified
    
    state->timestamp = target_time;
    
    return PREDICTION_SUCCESS;
}

// Convert ECI coordinates to topocentric (observer-relative) coordinates
int prediction_engine_eci_to_topocentric(
    const satellite_state_t *sat_state,
    const dish_location_t *observer,
    topocentric_coords_t *topo) {
    
    if (!sat_state || !observer || !topo) {
        return PREDICTION_ERROR_INVALID_PARAM;
    }
    
    // Convert observer location to ECEF
    double obs_lat_rad = observer->latitude * DEG_TO_RAD;
    double obs_lon_rad = observer->longitude * DEG_TO_RAD;
    double obs_alt_km = observer->altitude / 1000.0; // Convert to km
    
    // Earth radius at observer latitude
    double earth_radius = SGP4_EARTH_RADIUS_KM * (1.0 - SGP4_EARTH_FLATTENING * sin(obs_lat_rad) * sin(obs_lat_rad));
    
    // Observer position in ECEF
    double obs_ecef[3];
    obs_ecef[0] = (earth_radius + obs_alt_km) * cos(obs_lat_rad) * cos(obs_lon_rad);
    obs_ecef[1] = (earth_radius + obs_alt_km) * cos(obs_lat_rad) * sin(obs_lon_rad);
    obs_ecef[2] = (earth_radius * (1.0 - SGP4_EARTH_FLATTENING * SGP4_EARTH_FLATTENING) + obs_alt_km) * sin(obs_lat_rad);
    
    // Convert satellite ECI to ECEF (simplified - assumes ECI ≈ ECEF for short time periods)
    double sat_ecef[3];
    sat_ecef[0] = sat_state->position[0];
    sat_ecef[1] = sat_state->position[1];
    sat_ecef[2] = sat_state->position[2];
    
    // Calculate relative position vector
    double rel_pos[3];
    rel_pos[0] = sat_ecef[0] - obs_ecef[0];
    rel_pos[1] = sat_ecef[1] - obs_ecef[1];
    rel_pos[2] = sat_ecef[2] - obs_ecef[2];
    
    // Calculate range
    topo->range = sqrt(rel_pos[0]*rel_pos[0] + rel_pos[1]*rel_pos[1] + rel_pos[2]*rel_pos[2]);
    
    // Transform to topocentric coordinates (SEZ - South, East, Zenith)
    double sin_lat = sin(obs_lat_rad);
    double cos_lat = cos(obs_lat_rad);
    double sin_lon = sin(obs_lon_rad);
    double cos_lon = cos(obs_lon_rad);
    
    double south = sin_lat * cos_lon * rel_pos[0] + sin_lat * sin_lon * rel_pos[1] - cos_lat * rel_pos[2];
    double east = -sin_lon * rel_pos[0] + cos_lon * rel_pos[1];
    double zenith = cos_lat * cos_lon * rel_pos[0] + cos_lat * sin_lon * rel_pos[1] + sin_lat * rel_pos[2];
    
    // Calculate azimuth and elevation
    topo->azimuth = atan2(east, south) * RAD_TO_DEG;
    if (topo->azimuth < 0.0) {
        topo->azimuth += 360.0;
    }
    
    topo->elevation = atan2(zenith, sqrt(south*south + east*east)) * RAD_TO_DEG;
    
    // Calculate range rate (simplified)
    topo->range_rate = 0.0; // TODO: Implement proper range rate calculation
    
    return PREDICTION_SUCCESS;
}

// Assess connectivity at a specific time
connectivity_assessment_t prediction_engine_assess_connectivity(
    prediction_engine_t *engine,
    time_t timestamp) {
    
    connectivity_assessment_t assessment = {0};
    assessment.timestamp = timestamp;
    
    if (!engine || !engine->dish_location || !engine->obstruction_analyzer || 
        !engine->satellite_elements || engine->num_satellites == 0) {
        return assessment;
    }
    
    // Allocate memory for satellite positions
    assessment.satellite_positions = calloc(engine->num_satellites, sizeof(satellite_position_t));
    if (!assessment.satellite_positions) {
        return assessment;
    }
    
    // Propagate each satellite to the target time
    for (int i = 0; i < engine->num_satellites; i++) {
        satellite_state_t sat_state;
        int prop_result = prediction_engine_propagate_satellite(&engine->satellite_elements[i], 
                                                               timestamp, &sat_state);
        
        if (prop_result == PREDICTION_SUCCESS) {
            topocentric_coords_t topo;
            int topo_result = prediction_engine_eci_to_topocentric(&sat_state, engine->dish_location, &topo);
            
            if (topo_result == PREDICTION_SUCCESS) {
                satellite_position_t *pos = &assessment.satellite_positions[assessment.num_positions];
                
                snprintf(pos->satellite_id, sizeof(pos->satellite_id), "STARLINK-%d", i);
                pos->azimuth = topo.azimuth;
                pos->elevation = topo.elevation;
                pos->range = topo.range;
                pos->velocity = topo.range_rate;
                pos->timestamp = timestamp;
                
                // Check visibility
                pos->is_visible = (topo.elevation >= engine->config.min_elevation_degrees);
                if (pos->is_visible) {
                    assessment.visible_satellites++;
                    
                    // Check obstruction
                    obstruction_analysis_result_t obs_result = 
                        obstruction_analyzer_check_satellite(engine->obstruction_analyzer, 
                                                            topo.azimuth, topo.elevation);
                    
                    pos->is_obstructed = obs_result.is_obstructed;
                    pos->signal_quality = obs_result.snr_quality;
                    
                    if (!pos->is_obstructed) {
                        assessment.unobstructed_satellites++;
                        assessment.available_satellites++;
                    }
                }
                
                assessment.num_positions++;
            }
        }
    }
    
    // Calculate connectivity score
    if (assessment.visible_satellites > 0) {
        assessment.connectivity_score = (double)assessment.unobstructed_satellites / assessment.visible_satellites;
    } else {
        assessment.connectivity_score = 0.0;
    }
    
    // Determine prediction status
    assessment.outage_predicted = (assessment.available_satellites == 0);
    assessment.degradation_predicted = (assessment.available_satellites < engine->config.min_satellites_for_connectivity);
    
    return assessment;
}

// Calculate predictions for a time horizon
int prediction_engine_calculate_predictions(
    prediction_engine_t *engine,
    time_t start_time,
    int horizon_hours,
    outage_prediction_t **predictions,
    int *num_predictions) {
    
    if (!engine || !predictions || !num_predictions) {
        return PREDICTION_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&engine->prediction_mutex);
    
    // Calculate number of time steps
    int total_seconds = horizon_hours * 3600;
    int num_steps = total_seconds / engine->config.time_step_seconds;
    
    if (num_steps <= 0) {
        pthread_mutex_unlock(&engine->prediction_mutex);
        return PREDICTION_ERROR_INVALID_PARAM;
    }
    
    // Allocate assessments array
    connectivity_assessment_t *assessments = calloc(num_steps, sizeof(connectivity_assessment_t));
    if (!assessments) {
        pthread_mutex_unlock(&engine->prediction_mutex);
        return PREDICTION_ERROR_MEMORY_FAILED;
    }
    
    // Calculate connectivity for each time step
    for (int i = 0; i < num_steps; i++) {
        time_t step_time = start_time + (i * engine->config.time_step_seconds);
        assessments[i] = prediction_engine_assess_connectivity(engine, step_time);
    }
    
    // Detect outage windows
    int result = prediction_engine_detect_outage_windows(assessments, num_steps, predictions, num_predictions);
    
    // Update engine statistics
    engine->total_predictions++;
    engine->last_prediction_time = time(NULL);
    
    // Cleanup assessments
    for (int i = 0; i < num_steps; i++) {
        if (assessments[i].satellite_positions) {
            free(assessments[i].satellite_positions);
        }
    }
    free(assessments);
    
    pthread_mutex_unlock(&engine->prediction_mutex);
    
    return result;
}

// Detect outage windows from connectivity assessments
int prediction_engine_detect_outage_windows(
    const connectivity_assessment_t *assessments,
    int num_assessments,
    outage_prediction_t **outages,
    int *num_outages) {
    
    if (!assessments || !outages || !num_outages || num_assessments <= 0) {
        return PREDICTION_ERROR_INVALID_PARAM;
    }
    
    // Allocate temporary outage array (worst case: every assessment is an outage)
    outage_prediction_t *temp_outages = calloc(num_assessments, sizeof(outage_prediction_t));
    if (!temp_outages) {
        return PREDICTION_ERROR_MEMORY_FAILED;
    }
    
    int outage_count = 0;
    bool in_outage = false;
    time_t outage_start = 0;
    
    for (int i = 0; i < num_assessments; i++) {
        bool current_outage = assessments[i].outage_predicted;
        
        if (current_outage && !in_outage) {
            // Start of new outage
            in_outage = true;
            outage_start = assessments[i].timestamp;
        } else if (!current_outage && in_outage) {
            // End of outage
            in_outage = false;
            
            outage_prediction_t *outage = &temp_outages[outage_count];
            outage->start_time = outage_start;
            outage->end_time = assessments[i].timestamp;
            outage->duration_seconds = (int)(outage->end_time - outage->start_time);
            
            // Calculate risk level based on duration and satellite availability
            int min_available = INT_MAX;
            for (int j = 0; j < num_assessments; j++) {
                if (assessments[j].timestamp >= outage_start && assessments[j].timestamp <= outage->end_time) {
                    if (assessments[j].available_satellites < min_available) {
                        min_available = assessments[j].available_satellites;
                    }
                }
            }
            
            outage->risk_level = prediction_engine_calculate_risk_level(
                min_available, 0.0, outage->duration_seconds);
            outage->predicted_available_sats = min_available;
            outage->confidence_score = 0.8; // TODO: Calculate actual confidence
            
            snprintf(outage->description, sizeof(outage->description), 
                    "Predicted outage: %d satellites available, %d second duration", 
                    min_available, outage->duration_seconds);
            
            outage_count++;
        }
    }
    
    // Handle case where we end in an outage
    if (in_outage && num_assessments > 0) {
        outage_prediction_t *outage = &temp_outages[outage_count];
        outage->start_time = outage_start;
        outage->end_time = assessments[num_assessments - 1].timestamp;
        outage->duration_seconds = (int)(outage->end_time - outage->start_time);
        outage->risk_level = RISK_LEVEL_HIGH;
        outage->predicted_available_sats = 0;
        outage->confidence_score = 0.7;
        
        snprintf(outage->description, sizeof(outage->description), 
                "Ongoing outage at end of prediction window");
        
        outage_count++;
    }
    
    // Allocate final outages array
    if (outage_count > 0) {
        *outages = calloc(outage_count, sizeof(outage_prediction_t));
        if (!*outages) {
            free(temp_outages);
            return PREDICTION_ERROR_MEMORY_FAILED;
        }
        
        memcpy(*outages, temp_outages, outage_count * sizeof(outage_prediction_t));
    } else {
        *outages = NULL;
    }
    
    *num_outages = outage_count;
    free(temp_outages);
    
    return PREDICTION_SUCCESS;
}

// Calculate risk level based on various factors
risk_level_t prediction_engine_calculate_risk_level(
    int available_satellites,
    double connectivity_score,
    int duration_seconds) {
    
    // No satellites available = critical
    if (available_satellites == 0) {
        return RISK_LEVEL_CRITICAL;
    }
    
    // Very few satellites + long duration = high risk
    if (available_satellites <= 2 && duration_seconds > 300) {
        return RISK_LEVEL_HIGH;
    }
    
    // Short outages with some satellites = medium risk
    if (available_satellites <= 3 && duration_seconds > 60) {
        return RISK_LEVEL_MEDIUM;
    }
    
    // Everything else is low risk
    return RISK_LEVEL_LOW;
}

// Check if satellite is visible (above minimum elevation)
bool prediction_engine_is_satellite_visible(
    const satellite_position_t *satellite,
    double min_elevation) {
    
    if (!satellite) {
        return false;
    }
    
    return satellite->elevation >= min_elevation;
}

// Get visible satellites at a specific time
int prediction_engine_get_visible_satellites(
    prediction_engine_t *engine,
    time_t timestamp,
    satellite_position_t **visible_satellites,
    int *num_visible) {
    
    if (!engine || !visible_satellites || !num_visible) {
        return PREDICTION_ERROR_INVALID_PARAM;
    }
    
    // Get connectivity assessment
    connectivity_assessment_t assessment = prediction_engine_assess_connectivity(engine, timestamp);
    
    if (assessment.num_positions == 0) {
        *visible_satellites = NULL;
        *num_visible = 0;
        return PREDICTION_SUCCESS;
    }
    
    // Count visible satellites
    int visible_count = 0;
    for (int i = 0; i < assessment.num_positions; i++) {
        if (assessment.satellite_positions[i].is_visible) {
            visible_count++;
        }
    }
    
    if (visible_count == 0) {
        *visible_satellites = NULL;
        *num_visible = 0;
        if (assessment.satellite_positions) {
            free(assessment.satellite_positions);
        }
        return PREDICTION_SUCCESS;
    }
    
    // Allocate array for visible satellites
    *visible_satellites = calloc(visible_count, sizeof(satellite_position_t));
    if (!*visible_satellites) {
        if (assessment.satellite_positions) {
            free(assessment.satellite_positions);
        }
        return PREDICTION_ERROR_MEMORY_FAILED;
    }
    
    // Copy visible satellites
    int visible_index = 0;
    for (int i = 0; i < assessment.num_positions; i++) {
        if (assessment.satellite_positions[i].is_visible) {
            memcpy(&(*visible_satellites)[visible_index], 
                   &assessment.satellite_positions[i], 
                   sizeof(satellite_position_t));
            visible_index++;
        }
    }
    
    *num_visible = visible_count;
    
    if (assessment.satellite_positions) {
        free(assessment.satellite_positions);
    }
    
    return PREDICTION_SUCCESS;
}
