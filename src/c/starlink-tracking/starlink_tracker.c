#include "starlink_tracker.h"
#include "space_track_connector.h"
#include "obstruction_analyzer.h"
#include "prediction_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

// Internal function prototypes
static void* starlink_tracker_update_thread(void *arg);
static void* starlink_tracker_monitoring_thread(void *arg);
static int starlink_tracker_fetch_dish_data(starlink_tracker_t *tracker);
static int starlink_tracker_update_predictions_internal(starlink_tracker_t *tracker);

// Initialize Starlink tracker
starlink_tracker_t* starlink_tracker_init(const starlink_tracker_config_t *config) {
    if (!config || !config->space_track_username[0] || !config->space_track_password[0]) {
        return NULL;
    }
    
    starlink_tracker_t *tracker = calloc(1, sizeof(starlink_tracker_t));
    if (!tracker) {
        return NULL;
    }
    
    // Copy configuration
    memcpy(&tracker->config, config, sizeof(starlink_tracker_config_t));
    
    // Initialize mutex
    if (pthread_mutex_init(&tracker->data_mutex, NULL) != 0) {
        free(tracker);
        return NULL;
    }
    
    // Initialize Space-Track connector
    space_track_config_t space_config;
    space_track_config_init_defaults(&space_config);
    strncpy(space_config.username, config->space_track_username, sizeof(space_config.username) - 1);
    space_config.username[sizeof(space_config.username) - 1] = '\0';
    strncpy(space_config.password, config->space_track_password, sizeof(space_config.password) - 1);
    space_config.password[sizeof(space_config.password) - 1] = '\0';
    space_config.rate_limit_requests_per_minute = config->rate_limit_requests_per_minute;
    space_config.cache_duration_hours = config->cache_duration_hours;
    
    tracker->space_track = space_track_connector_init(&space_config);
    if (!tracker->space_track) {
        pthread_mutex_destroy(&tracker->data_mutex);
        free(tracker);
        return NULL;
    }
    
    // Initialize obstruction analyzer
    obstruction_analysis_config_t obs_config;
    obs_config.snr_threshold = config->obstruction_threshold;
    obs_config.min_elevation = config->min_elevation_degrees;
    obs_config.max_elevation = 90.0;
    obs_config.use_adaptive_threshold = false;
    obs_config.adaptive_threshold_factor = 0.8;
    obs_config.smoothing_window_size = 3;
    
    tracker->analyzer = obstruction_analyzer_init(&obs_config);
    if (!tracker->analyzer) {
        space_track_connector_cleanup(tracker->space_track);
        pthread_mutex_destroy(&tracker->data_mutex);
        free(tracker);
        return NULL;
    }
    
    // Initialize prediction engine
    prediction_config_t pred_config;
    prediction_engine_config_init_defaults(&pred_config);
    pred_config.prediction_horizon_hours = config->prediction_horizon_hours;
    pred_config.min_elevation_degrees = config->min_elevation_degrees;
    
    tracker->engine = prediction_engine_init(&pred_config);
    if (!tracker->engine) {
        obstruction_analyzer_cleanup(tracker->analyzer);
        space_track_connector_cleanup(tracker->space_track);
        pthread_mutex_destroy(&tracker->data_mutex);
        free(tracker);
        return NULL;
    }
    
    // Set up engine dependencies
    prediction_engine_set_dish_location(tracker->engine, &tracker->dish_location);
    prediction_engine_set_obstruction_analyzer(tracker->engine, tracker->analyzer);
    
    tracker->initialized = true;
    
    return tracker;
}

// Cleanup Starlink tracker
void starlink_tracker_cleanup(starlink_tracker_t *tracker) {
    if (!tracker) {
        return;
    }
    
    // Stop monitoring if active
    if (tracker->monitoring_active) {
        starlink_tracker_stop_monitoring(tracker);
    }
    
    // Cleanup components
    if (tracker->engine) {
        prediction_engine_cleanup(tracker->engine);
    }
    
    if (tracker->analyzer) {
        obstruction_analyzer_cleanup(tracker->analyzer);
    }
    
    if (tracker->space_track) {
        space_track_connector_cleanup(tracker->space_track);
    }
    
    // Free allocated memory
    if (tracker->current_positions) {
        free(tracker->current_positions);
    }
    
    if (tracker->predictions) {
        free(tracker->predictions);
    }
    
    if (tracker->constellation.satellites) {
        free(tracker->constellation.satellites);
    }
    
    if (tracker->obstruction_map.cells) {
        free(tracker->obstruction_map.cells);
    }
    
    pthread_mutex_destroy(&tracker->data_mutex);
    free(tracker);
}

// Update dish location from Starlink API
int starlink_tracker_update_dish_location(starlink_tracker_t *tracker) {
    if (!tracker || !tracker->initialized) {
        return TRACKER_ERROR_NOT_INITIALIZED;
    }
    
    // This would call the existing Starlink gRPC client
    // For now, we'll simulate the call - in real implementation, 
    // this would use the existing starlink_client.c functions
    
    char grpc_command[512];
    snprintf(grpc_command, sizeof(grpc_command), 
            "grpcurl -plaintext -d '{\"getLocation\":{}}' %s:%d SpaceX.API.Device.Device/Handle",
            tracker->config.starlink_dish_ip, tracker->config.starlink_dish_port);
    
    // Execute gRPC command and parse response
    FILE *fp = popen(grpc_command, "r");
    if (!fp) {
        return TRACKER_ERROR_API_FAILURE;
    }
    
    char response[4096];
    size_t response_size = fread(response, 1, sizeof(response) - 1, fp);
    response[response_size] = '\0';
    
    int status = pclose(fp);
    if (status != 0) {
        return TRACKER_ERROR_API_FAILURE;
    }
    
    // Parse location from response
    pthread_mutex_lock(&tracker->data_mutex);
    int parse_result = obstruction_analyzer_parse_dish_response(response, NULL, &tracker->dish_location);
    pthread_mutex_unlock(&tracker->data_mutex);
    
    return (parse_result == OBSTRUCTION_SUCCESS) ? TRACKER_SUCCESS : TRACKER_ERROR_PARSE_FAILURE;
}

// Update obstruction map from Starlink API
int starlink_tracker_update_obstruction_map(starlink_tracker_t *tracker) {
    if (!tracker || !tracker->initialized) {
        return TRACKER_ERROR_NOT_INITIALIZED;
    }
    
    // Call obstruction map API
    char grpc_command[512];
    snprintf(grpc_command, sizeof(grpc_command), 
            "grpcurl -plaintext -d '{\"dishGetObstructionMap\":{}}' %s:%d SpaceX.API.Device.Device/Handle",
            tracker->config.starlink_dish_ip, tracker->config.starlink_dish_port);
    
    FILE *fp = popen(grpc_command, "r");
    if (!fp) {
        return TRACKER_ERROR_API_FAILURE;
    }
    
    char response[8192]; // Larger buffer for obstruction map
    size_t response_size = fread(response, 1, sizeof(response) - 1, fp);
    response[response_size] = '\0';
    
    int status = pclose(fp);
    if (status != 0) {
        return TRACKER_ERROR_API_FAILURE;
    }
    
    // Update obstruction map
    pthread_mutex_lock(&tracker->data_mutex);
    int update_result = obstruction_analyzer_update_map(tracker->analyzer, response);
    if (update_result == OBSTRUCTION_SUCCESS) {
        // Copy map to tracker structure
        memcpy(&tracker->obstruction_map, &tracker->analyzer->current_map, sizeof(obstruction_map_t));
    }
    pthread_mutex_unlock(&tracker->data_mutex);
    
    return (update_result == OBSTRUCTION_SUCCESS) ? TRACKER_SUCCESS : TRACKER_ERROR_PARSE_FAILURE;
}

// Update constellation data from Space-Track
int starlink_tracker_update_constellation_data(starlink_tracker_t *tracker) {
    if (!tracker || !tracker->initialized || !tracker->space_track) {
        return TRACKER_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&tracker->data_mutex);
    
    // Fetch TLE data from Space-Track
    int result = space_track_get_starlink_tles(tracker->space_track, &tracker->constellation);
    
    if (result == SPACE_TRACK_SUCCESS) {
        // Load constellation into prediction engine
        prediction_engine_load_constellation(tracker->engine, &tracker->constellation);
        tracker->last_update = time(NULL);
    }
    
    pthread_mutex_unlock(&tracker->data_mutex);
    
    return (result == SPACE_TRACK_SUCCESS) ? TRACKER_SUCCESS : TRACKER_ERROR_API_FAILURE;
}

// Calculate predictions
int starlink_tracker_calculate_predictions(starlink_tracker_t *tracker, int horizon_hours) {
    if (!tracker || !tracker->initialized) {
        return TRACKER_ERROR_NOT_INITIALIZED;
    }
    
    return starlink_tracker_update_predictions_internal(tracker);
}

// Internal prediction update
static int starlink_tracker_update_predictions_internal(starlink_tracker_t *tracker) {
    pthread_mutex_lock(&tracker->data_mutex);
    
    // Free existing predictions
    if (tracker->predictions) {
        free(tracker->predictions);
        tracker->predictions = NULL;
        tracker->num_predictions = 0;
    }
    
    // Calculate new predictions
    time_t start_time = time(NULL);
    int result = prediction_engine_calculate_predictions(
        tracker->engine,
        start_time,
        tracker->config.prediction_horizon_hours,
        &tracker->predictions,
        &tracker->num_predictions
    );
    
    if (result == PREDICTION_SUCCESS) {
        // Trigger outage callback for new predictions
        if (tracker->outage_callback && tracker->predictions) {
            for (int i = 0; i < tracker->num_predictions; i++) {
                tracker->outage_callback(&tracker->predictions[i], tracker->callback_user_data);
            }
        }
    }
    
    pthread_mutex_unlock(&tracker->data_mutex);
    
    return (result == PREDICTION_SUCCESS) ? TRACKER_SUCCESS : TRACKER_ERROR_API_FAILURE;
}

// Get current predictions
int starlink_tracker_get_predictions(const starlink_tracker_t *tracker, outage_prediction_t **predictions) {
    if (!tracker || !predictions) {
        return -1;
    }
    
    pthread_mutex_lock((pthread_mutex_t*)&tracker->data_mutex);
    
    if (!tracker->predictions || tracker->num_predictions == 0) {
        *predictions = NULL;
        pthread_mutex_unlock((pthread_mutex_t*)&tracker->data_mutex);
        return 0;
    }
    
    // Allocate memory for predictions copy
    *predictions = calloc(tracker->num_predictions, sizeof(outage_prediction_t));
    if (!*predictions) {
        pthread_mutex_unlock((pthread_mutex_t*)&tracker->data_mutex);
        return -1;
    }
    
    // Copy predictions
    memcpy(*predictions, tracker->predictions, tracker->num_predictions * sizeof(outage_prediction_t));
    int count = tracker->num_predictions;
    
    pthread_mutex_unlock((pthread_mutex_t*)&tracker->data_mutex);
    
    return count;
}

// Free predictions array
void starlink_tracker_free_predictions(outage_prediction_t *predictions, int count) {
    if (predictions) {
        free(predictions);
    }
}

// Start monitoring
int starlink_tracker_start_monitoring(starlink_tracker_t *tracker) {
    if (!tracker || !tracker->initialized) {
        return TRACKER_ERROR_NOT_INITIALIZED;
    }
    
    if (tracker->monitoring_active) {
        return TRACKER_SUCCESS; // Already monitoring
    }
    
    tracker->monitoring_active = true;
    
    // Create update thread
    if (pthread_create(&tracker->update_thread, NULL, starlink_tracker_update_thread, tracker) != 0) {
        tracker->monitoring_active = false;
        return TRACKER_ERROR_THREAD_FAILURE;
    }
    
    // Create monitoring thread
    if (pthread_create(&tracker->monitoring_thread, NULL, starlink_tracker_monitoring_thread, tracker) != 0) {
        tracker->monitoring_active = false;
        pthread_cancel(tracker->update_thread);
        pthread_join(tracker->update_thread, NULL);
        return TRACKER_ERROR_THREAD_FAILURE;
    }
    
    return TRACKER_SUCCESS;
}

// Stop monitoring
int starlink_tracker_stop_monitoring(starlink_tracker_t *tracker) {
    if (!tracker) {
        return TRACKER_ERROR_INVALID_PARAM;
    }
    
    if (!tracker->monitoring_active) {
        return TRACKER_SUCCESS; // Not monitoring
    }
    
    tracker->monitoring_active = false;
    
    // Cancel and join threads
    pthread_cancel(tracker->update_thread);
    pthread_cancel(tracker->monitoring_thread);
    
    pthread_join(tracker->update_thread, NULL);
    pthread_join(tracker->monitoring_thread, NULL);
    
    return TRACKER_SUCCESS;
}

// Check if monitoring is active
bool starlink_tracker_is_monitoring(const starlink_tracker_t *tracker) {
    return tracker ? tracker->monitoring_active : false;
}

// Set outage callback
int starlink_tracker_set_outage_callback(starlink_tracker_t *tracker, 
    void (*callback)(const outage_prediction_t *prediction, void *user_data), 
    void *user_data) {
    
    if (!tracker) {
        return TRACKER_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&tracker->data_mutex);
    tracker->outage_callback = callback;
    tracker->callback_user_data = user_data;
    pthread_mutex_unlock(&tracker->data_mutex);
    
    return TRACKER_SUCCESS;
}

// Get current satellite positions
int starlink_tracker_get_current_satellite_positions(const starlink_tracker_t *tracker, satellite_position_t **positions) {
    if (!tracker || !positions) {
        return -1;
    }
    
    pthread_mutex_lock((pthread_mutex_t*)&tracker->data_mutex);
    
    if (!tracker->current_positions || tracker->num_current_positions == 0) {
        *positions = NULL;
        pthread_mutex_unlock((pthread_mutex_t*)&tracker->data_mutex);
        return 0;
    }
    
    // Allocate memory for positions copy
    *positions = calloc(tracker->num_current_positions, sizeof(satellite_position_t));
    if (!*positions) {
        pthread_mutex_unlock((pthread_mutex_t*)&tracker->data_mutex);
        return -1;
    }
    
    // Copy positions
    memcpy(*positions, tracker->current_positions, tracker->num_current_positions * sizeof(satellite_position_t));
    int count = tracker->num_current_positions;
    
    pthread_mutex_unlock((pthread_mutex_t*)&tracker->data_mutex);
    
    return count;
}

// Get visible satellite count
int starlink_tracker_get_visible_satellite_count(const starlink_tracker_t *tracker) {
    if (!tracker) {
        return 0;
    }
    
    pthread_mutex_lock((pthread_mutex_t*)&tracker->data_mutex);
    
    int visible_count = 0;
    for (int i = 0; i < tracker->num_current_positions; i++) {
        if (tracker->current_positions[i].is_visible) {
            visible_count++;
        }
    }
    
    pthread_mutex_unlock((pthread_mutex_t*)&tracker->data_mutex);
    
    return visible_count;
}

// Get unobstructed satellite count
int starlink_tracker_get_unobstructed_satellite_count(const starlink_tracker_t *tracker) {
    if (!tracker) {
        return 0;
    }
    
    pthread_mutex_lock((pthread_mutex_t*)&tracker->data_mutex);
    
    int unobstructed_count = 0;
    for (int i = 0; i < tracker->num_current_positions; i++) {
        if (tracker->current_positions[i].is_visible && !tracker->current_positions[i].is_obstructed) {
            unobstructed_count++;
        }
    }
    
    pthread_mutex_unlock((pthread_mutex_t*)&tracker->data_mutex);
    
    return unobstructed_count;
}

// Update thread - handles periodic data updates
static void* starlink_tracker_update_thread(void *arg) {
    starlink_tracker_t *tracker = (starlink_tracker_t*)arg;
    
    while (tracker->monitoring_active) {
        // Update dish data
        starlink_tracker_fetch_dish_data(tracker);
        
        // Update constellation data (less frequently)
        time_t now = time(NULL);
        if ((now - tracker->constellation.last_update) > (tracker->config.update_interval_minutes * 60)) {
            starlink_tracker_update_constellation_data(tracker);
        }
        
        // Update predictions
        starlink_tracker_update_predictions_internal(tracker);
        
        // Sleep for update interval
        sleep(tracker->config.update_interval_minutes * 60);
    }
    
    return NULL;
}

// Monitoring thread - handles real-time monitoring and callbacks
static void* starlink_tracker_monitoring_thread(void *arg) {
    starlink_tracker_t *tracker = (starlink_tracker_t*)arg;
    
    while (tracker->monitoring_active) {
        // Get current satellite positions
        time_t now = time(NULL);
        satellite_position_t *positions;
        int num_positions = prediction_engine_get_visible_satellites(tracker->engine, now, &positions, &num_positions);
        
        if (num_positions >= 0) {
            pthread_mutex_lock(&tracker->data_mutex);
            
            // Update current positions
            if (tracker->current_positions) {
                free(tracker->current_positions);
            }
            tracker->current_positions = positions;
            tracker->num_current_positions = num_positions;
            
            pthread_mutex_unlock(&tracker->data_mutex);
        }
        
        // Check for immediate outage conditions
        int unobstructed_count = starlink_tracker_get_unobstructed_satellite_count(tracker);
        if (unobstructed_count == 0 && tracker->outage_callback) {
            // Create immediate outage prediction
            outage_prediction_t immediate_outage;
            immediate_outage.start_time = now;
            immediate_outage.end_time = now + 300; // 5 minute default
            immediate_outage.duration_seconds = 300;
            immediate_outage.risk_level = RISK_LEVEL_CRITICAL;
            immediate_outage.predicted_available_sats = 0;
            immediate_outage.confidence_score = 1.0;
            strncpy(immediate_outage.description, "Immediate outage detected - no unobstructed satellites", 
                   sizeof(immediate_outage.description) - 1);
            
            tracker->outage_callback(&immediate_outage, tracker->callback_user_data);
        }
        
        // Sleep for monitoring interval (shorter than update interval)
        sleep(30); // 30 second monitoring interval
    }
    
    return NULL;
}

// Fetch dish data (location, obstruction map, diagnostics)
static int starlink_tracker_fetch_dish_data(starlink_tracker_t *tracker) {
    if (!tracker) {
        return TRACKER_ERROR_INVALID_PARAM;
    }
    
    // Update location
    int location_result = starlink_tracker_update_dish_location(tracker);
    
    // Update obstruction map
    int obstruction_result = starlink_tracker_update_obstruction_map(tracker);
    
    // Return success if at least one update succeeded
    return (location_result == TRACKER_SUCCESS || obstruction_result == TRACKER_SUCCESS) ? 
           TRACKER_SUCCESS : TRACKER_ERROR_API_FAILURE;
}

// Validate prediction against actual outcome
int starlink_tracker_validate_prediction(starlink_tracker_t *tracker, const outage_prediction_t *prediction, bool actual_outage) {
    if (!tracker || !prediction) {
        return TRACKER_ERROR_INVALID_PARAM;
    }
    
    if (!tracker->config.validation_enabled) {
        return TRACKER_SUCCESS; // Validation disabled
    }
    
    pthread_mutex_lock(&tracker->data_mutex);
    
    // Create validation record
    prediction_validation_t validation;
    validation.prediction_time = prediction->start_time;
    validation.actual_time = time(NULL);
    validation.prediction_correct = (actual_outage == (prediction->risk_level >= RISK_LEVEL_HIGH));
    validation.actual_outage_occurred = actual_outage;
    validation.predicted_duration = prediction->duration_seconds;
    // Track actual duration based on real outage data
    if (actual_outage) {
        // If we have historical data, calculate actual duration
        if (tracker->outage_history && tracker->outage_history_count > 0) {
            // Find the most recent outage that matches this prediction
            for (int i = tracker->outage_history_count - 1; i >= 0; i--) {
                if (tracker->outage_history[i].start_time >= prediction->start_time &&
                    tracker->outage_history[i].start_time <= prediction->start_time + prediction->duration_seconds) {
                    validation.actual_duration = tracker->outage_history[i].duration_seconds;
                    break;
                }
            }
            // If no matching outage found, use prediction duration as fallback
            if (validation.actual_duration == 0) {
                validation.actual_duration = prediction->duration_seconds;
            }
        } else {
            // No historical data available, use prediction duration
            validation.actual_duration = prediction->duration_seconds;
        }
    } else {
        validation.actual_duration = 0; // No outage occurred
    }
    
    // Calculate accuracy score
    if (validation.prediction_correct) {
        validation.accuracy_score = 1.0;
        tracker->stats.correct_predictions++;
    } else {
        validation.accuracy_score = 0.0;
        if (actual_outage) {
            tracker->stats.missed_outages++;
        } else {
            tracker->stats.false_positives++;
        }
    }
    
    // Add to validation history (ring buffer)
    tracker->stats.recent_validations[tracker->stats.validation_index] = validation;
    tracker->stats.validation_index = (tracker->stats.validation_index + 1) % 100;
    
    // Update overall statistics
    tracker->stats.total_predictions++;
    tracker->stats.accuracy_percentage = 
        (double)tracker->stats.correct_predictions / tracker->stats.total_predictions * 100.0;
    tracker->stats.last_validation = time(NULL);
    
    pthread_mutex_unlock(&tracker->data_mutex);
    
    return TRACKER_SUCCESS;
}

// Get tracking statistics
const tracking_stats_t* starlink_tracker_get_stats(const starlink_tracker_t *tracker) {
    if (!tracker) {
        static tracking_stats_t empty_stats = {0};
        return &empty_stats;
    }
    
    return &tracker->stats;
}

// Set log level
int starlink_tracker_set_log_level(starlink_tracker_t *tracker, tracker_log_level_t level) {
    if (!tracker) {
        return TRACKER_ERROR_INVALID_PARAM;
    }
    
    // Map tracker log levels to LOGX levels
    switch (level) {
        case TRACKER_LOG_LEVEL_DEBUG:
            // LOGX_DEBUG is already the default for debug messages
            break;
        case TRACKER_LOG_LEVEL_INFO:
            // LOGX_INFO is already the default for info messages
            break;
        case TRACKER_LOG_LEVEL_WARNING:
            // LOGX_WARN is already the default for warning messages
            break;
        case TRACKER_LOG_LEVEL_ERROR:
            // LOGX_ERROR is already the default for error messages
            break;
        default:
            return TRACKER_ERROR_INVALID_PARAM;
    }
    
    tracker->log_level = level;
    
    LOGX_INFO_MSG("Starlink tracker log level set", "level", level);
    
    return TRACKER_SUCCESS;
}

// Set log callback
int starlink_tracker_set_log_callback(starlink_tracker_t *tracker, 
    void (*log_callback)(tracker_log_level_t level, const char *message, void *user_data), 
    void *user_data) {
    
    if (!tracker) {
        return TRACKER_ERROR_INVALID_PARAM;
    }
    
    // Store the callback and user data
    tracker->log_callback = log_callback;
    tracker->log_user_data = user_data;
    
    // If a callback is provided, we can use it for custom logging
    // Otherwise, we'll use the standard LOGX system
    if (log_callback) {
        LOGX_INFO_MSG("Starlink tracker log callback set");
    } else {
        LOGX_INFO_MSG("Starlink tracker log callback cleared, using standard logging");
    }
    
    return TRACKER_SUCCESS;
}