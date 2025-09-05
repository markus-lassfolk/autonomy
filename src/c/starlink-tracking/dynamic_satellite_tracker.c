#include "dynamic_satellite_tracker.h"
#include "astro_coordinates.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

// Initialize default dynamic tracking configuration
void dynamic_tracking_config_init_defaults(dynamic_tracking_config_t *config) {
    if (!config) {
        return;
    }
    
    config->tracking_interval_seconds = 15; // Starlink's scheduling window
    config->map_poll_frequency_hz = 1;      // 1 Hz as recommended by Gemini
    config->scheduling_window_seconds = 15;  // Starlink's 15-second windows
    config->trajectory_match_threshold = 2.0; // 2 degrees angular threshold
    config->enable_xor_analysis = true;      // Use XOR between maps
    config->candidate_satellite_limit = 50;  // Limit for performance
}

// Initialize dynamic satellite tracker
dynamic_satellite_tracker_t* dynamic_tracker_init(const dynamic_tracking_config_t *config) {
    dynamic_satellite_tracker_t *tracker = calloc(1, sizeof(dynamic_satellite_tracker_t));
    if (!tracker) {
        return NULL;
    }
    
    // Set configuration
    if (config) {
        memcpy(&tracker->config, config, sizeof(dynamic_tracking_config_t));
    } else {
        dynamic_tracking_config_init_defaults(&tracker->config);
    }
    
    // Allocate map storage for XOR analysis (123x123 = 15,129 doubles)
    tracker->previous_map = calloc(OBSTRUCTION_MAP_SIZE, sizeof(double));
    tracker->current_map = calloc(OBSTRUCTION_MAP_SIZE, sizeof(double));
    tracker->difference_map = calloc(OBSTRUCTION_MAP_SIZE, sizeof(double));
    
    if (!tracker->previous_map || !tracker->current_map || !tracker->difference_map) {
        dynamic_tracker_cleanup(tracker);
        return NULL;
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&tracker->data_mutex, NULL) != 0) {
        dynamic_tracker_cleanup(tracker);
        return NULL;
    }
    
    return tracker;
}

// Cleanup dynamic tracker
void dynamic_tracker_cleanup(dynamic_satellite_tracker_t *tracker) {
    if (!tracker) {
        return;
    }
    
    if (tracker->tracking_active) {
        dynamic_tracker_stop_tracking(tracker);
    }
    
    if (tracker->previous_map) free(tracker->previous_map);
    if (tracker->current_map) free(tracker->current_map);
    if (tracker->difference_map) free(tracker->difference_map);
    if (tracker->candidates) free(tracker->candidates);
    
    pthread_mutex_destroy(&tracker->data_mutex);
    free(tracker);
}

// Convert azimuth/elevation to pixel coordinates (Gemini's algorithm)
pixel_coords_t dynamic_tracker_az_el_to_pixel(double azimuth, double elevation) {
    pixel_coords_t result = {0, 0, false};
    
    const int MAP_DIAMETER = 123;
    const int CENTER_PIXEL = 61;
    const double MAX_RADIUS_PIXELS = 61.5;
    const double MIN_ELEVATION_DEG = 25.0;
    const double MAX_ELEVATION_DEG = 90.0;
    
    // Check elevation bounds
    if (elevation < MIN_ELEVATION_DEG || elevation > MAX_ELEVATION_DEG) {
        return result;
    }
    
    // Normalize elevation to radius (0 at zenith, 1 at edge)
    double radius_normalized = (MAX_ELEVATION_DEG - elevation) / (MAX_ELEVATION_DEG - MIN_ELEVATION_DEG);
    double pixel_radius = radius_normalized * MAX_RADIUS_PIXELS;
    
    // Convert polar to Cartesian (North is up, subtract 90°)
    double az_rad = (azimuth - 90.0) * ASTRO_DEG_TO_RAD;
    
    int col = (int)(CENTER_PIXEL + pixel_radius * cos(az_rad));
    int row = (int)(CENTER_PIXEL + pixel_radius * sin(az_rad));
    
    // Check bounds
    if (row >= 0 && row < MAP_DIAMETER && col >= 0 && col < MAP_DIAMETER) {
        result.row = row;
        result.col = col;
        result.valid = true;
    }
    
    return result;
}

// Convert pixel coordinates back to azimuth/elevation
int dynamic_tracker_pixel_to_az_el(int row, int col, double *azimuth, double *elevation) {
    if (!azimuth || !elevation) {
        return DYNAMIC_TRACKER_ERROR_INVALID_PARAM;
    }
    
    const int MAP_DIAMETER = 123;
    const int CENTER_PIXEL = 61;
    const double MAX_RADIUS_PIXELS = 61.5;
    const double MIN_ELEVATION_DEG = 25.0;
    const double MAX_ELEVATION_DEG = 90.0;
    
    // Check bounds
    if (row < 0 || row >= MAP_DIAMETER || col < 0 || col >= MAP_DIAMETER) {
        return DYNAMIC_TRACKER_ERROR_INVALID_PARAM;
    }
    
    // Calculate distance from center
    double dx = col - CENTER_PIXEL;
    double dy = row - CENTER_PIXEL;
    double radius = sqrt(dx * dx + dy * dy);
    
    if (radius > MAX_RADIUS_PIXELS) {
        return DYNAMIC_TRACKER_ERROR_INVALID_PARAM; // Outside valid circular area
    }
    
    // Calculate azimuth
    if (radius > 0) {
        double az_rad = atan2(dy, dx);
        *azimuth = (az_rad * ASTRO_RAD_TO_DEG) + 90.0; // Add 90° offset back
        if (*azimuth < 0) *azimuth += 360.0;
        if (*azimuth >= 360.0) *azimuth -= 360.0;
    } else {
        *azimuth = 0.0; // At center (zenith)
    }
    
    // Calculate elevation
    double radius_normalized = radius / MAX_RADIUS_PIXELS;
    *elevation = MAX_ELEVATION_DEG - (radius_normalized * (MAX_ELEVATION_DEG - MIN_ELEVATION_DEG));
    
    return DYNAMIC_TRACKER_SUCCESS;
}

// Clear obstruction map via gRPC
int dynamic_tracker_clear_obstruction_map(dynamic_satellite_tracker_t *tracker) {
    if (!tracker) {
        return DYNAMIC_TRACKER_ERROR_INVALID_PARAM;
    }
    
    // Execute gRPC command to clear obstruction map
    char grpc_command[512];
    snprintf(grpc_command, sizeof(grpc_command), 
            "grpcurl -plaintext -d '{\"dishClearObstructionMap\":{}}' 192.168.100.1:9200 SpaceX.API.Device.Device/Handle");
    
    FILE *fp = popen(grpc_command, "r");
    if (!fp) {
        return DYNAMIC_TRACKER_ERROR_GRPC_FAILED;
    }
    
    char response[1024];
    size_t bytes_read = fread(response, 1, sizeof(response) - 1, fp);
    response[bytes_read] = '\0';
    
    int status = pclose(fp);
    
    if (tracker->log_callback) {
        if (status == 0) {
            tracker->log_callback(1, "Obstruction map cleared successfully", tracker->log_user_data);
        } else {
            tracker->log_callback(3, "Failed to clear obstruction map", tracker->log_user_data);
        }
    }
    
    return (status == 0) ? DYNAMIC_TRACKER_SUCCESS : DYNAMIC_TRACKER_ERROR_GRPC_FAILED;
}

// Capture sequence of obstruction maps (Gemini's dynamic tracking method)
int dynamic_tracker_capture_map_sequence(dynamic_satellite_tracker_t *tracker, observed_trajectory_t *trajectory) {
    if (!tracker || !trajectory) {
        return DYNAMIC_TRACKER_ERROR_INVALID_PARAM;
    }
    
    // Initialize trajectory
    memset(trajectory, 0, sizeof(observed_trajectory_t));
    trajectory->start_time = time(NULL);
    
    // Clear the obstruction map to start fresh
    int clear_result = dynamic_tracker_clear_obstruction_map(tracker);
    if (clear_result != DYNAMIC_TRACKER_SUCCESS) {
        return clear_result;
    }
    
    // Wait a moment for the clear to take effect
    usleep(100000); // 100ms
    
    // Capture maps for 15 seconds at 1 Hz
    for (int second = 0; second < tracker->config.scheduling_window_seconds; second++) {
        // Get current obstruction map
        int map_result = dynamic_tracker_get_current_obstruction_map(tracker, tracker->current_map);
        if (map_result != DYNAMIC_TRACKER_SUCCESS) {
            continue; // Skip this second
        }
        
        // Perform XOR analysis if we have a previous map
        if (second > 0 && tracker->config.enable_xor_analysis) {
            dynamic_tracker_perform_map_xor(tracker->previous_map, tracker->current_map, tracker->difference_map);
            
            // Extract trajectory point from difference map
            trajectory_point_t point;
            if (dynamic_tracker_extract_trajectory_from_xor(tracker->difference_map, &point) == DYNAMIC_TRACKER_SUCCESS) {
                point.timestamp = trajectory->start_time + second;
                trajectory->points[trajectory->num_points] = point;
                trajectory->num_points++;
            }
        }
        
        // Copy current map to previous for next iteration
        memcpy(tracker->previous_map, tracker->current_map, OBSTRUCTION_MAP_SIZE * sizeof(double));
        
        // Wait for next second (accounting for processing time)
        sleep(1);
    }
    
    trajectory->end_time = time(NULL);
    trajectory->complete = (trajectory->num_points > 5); // At least 5 points for valid trajectory
    
    if (trajectory->complete) {
        // Calculate total angular distance
        trajectory->total_angular_distance = 0.0;
        for (int i = 1; i < trajectory->num_points; i++) {
            double angular_dist = astro_angular_separation(
                trajectory->points[i-1].azimuth, trajectory->points[i-1].elevation,
                trajectory->points[i].azimuth, trajectory->points[i].elevation
            );
            trajectory->total_angular_distance += angular_dist;
        }
        
        if (tracker->log_callback) {
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg), 
                    "Captured trajectory: %d points, %.2f° total distance", 
                    trajectory->num_points, trajectory->total_angular_distance);
            tracker->log_callback(1, log_msg, tracker->log_user_data);
        }
    }
    
    return DYNAMIC_TRACKER_SUCCESS;
}

// Get current obstruction map
int dynamic_tracker_get_current_obstruction_map(dynamic_satellite_tracker_t *tracker, double *map_data) {
    if (!tracker || !map_data) {
        return DYNAMIC_TRACKER_ERROR_INVALID_PARAM;
    }
    
    // Execute gRPC command to get obstruction map
    char grpc_command[512];
    snprintf(grpc_command, sizeof(grpc_command), 
            "grpcurl -plaintext -d '{\"dishGetObstructionMap\":{}}' 192.168.100.1:9200 SpaceX.API.Device.Device/Handle");
    
    FILE *fp = popen(grpc_command, "r");
    if (!fp) {
        return DYNAMIC_TRACKER_ERROR_GRPC_FAILED;
    }
    
    char response[32768]; // Large buffer for 123x123 map
    size_t bytes_read = fread(response, 1, sizeof(response) - 1, fp);
    response[bytes_read] = '\0';
    
    int status = pclose(fp);
    if (status != 0) {
        return DYNAMIC_TRACKER_ERROR_GRPC_FAILED;
    }
    
    // Parse JSON response to extract SNR data
    json_object *root = json_tokener_parse(response);
    if (!root) {
        return DYNAMIC_TRACKER_ERROR_GRPC_FAILED;
    }
    
    json_object *obstruction_response;
    if (json_object_object_get_ex(root, "dishGetObstructionMap", &obstruction_response)) {
        json_object *obstruction_map_obj;
        if (json_object_object_get_ex(obstruction_response, "obstructionMap", &obstruction_map_obj)) {
            json_object *snr_array;
            if (json_object_object_get_ex(obstruction_map_obj, "data", &snr_array)) {
                int array_length = json_object_array_length(snr_array);
                
                if (array_length == OBSTRUCTION_MAP_SIZE) {
                    // Parse SNR values into map_data
                    for (int i = 0; i < array_length; i++) {
                        json_object *snr_value = json_object_array_get_idx(snr_array, i);
                        map_data[i] = json_object_get_double(snr_value);
                    }
                    
                    json_object_put(root);
                    return DYNAMIC_TRACKER_SUCCESS;
                }
            }
        }
    }
    
    json_object_put(root);
    return DYNAMIC_TRACKER_ERROR_GRPC_FAILED;
}

// Perform XOR analysis between two maps (Gemini's method)
int dynamic_tracker_perform_map_xor(const double *map1, const double *map2, double *result_map) {
    if (!map1 || !map2 || !result_map) {
        return DYNAMIC_TRACKER_ERROR_INVALID_PARAM;
    }
    
    // Calculate difference between maps
    // Since these are SNR values (not binary), we use absolute difference
    for (int i = 0; i < OBSTRUCTION_MAP_SIZE; i++) {
        result_map[i] = fabs(map2[i] - map1[i]);
    }
    
    return DYNAMIC_TRACKER_SUCCESS;
}

// Extract trajectory point from XOR difference map
int dynamic_tracker_extract_trajectory_from_xor(const double *diff_map, trajectory_point_t *point) {
    if (!diff_map || !point) {
        return DYNAMIC_TRACKER_ERROR_INVALID_PARAM;
    }
    
    // Find the pixel with the maximum difference (newly painted satellite path)
    double max_difference = 0.0;
    int max_row = -1;
    int max_col = -1;
    
    for (int row = 0; row < OBSTRUCTION_MAP_DIAMETER; row++) {
        for (int col = 0; col < OBSTRUCTION_MAP_DIAMETER; col++) {
            // Check if pixel is within valid circular area
            double dx = col - OBSTRUCTION_CENTER_PIXEL;
            double dy = row - OBSTRUCTION_CENTER_PIXEL;
            double radius = sqrt(dx * dx + dy * dy);
            
            if (radius <= OBSTRUCTION_MAX_RADIUS_PIXELS) {
                int index = row * OBSTRUCTION_MAP_DIAMETER + col;
                if (diff_map[index] > max_difference) {
                    max_difference = diff_map[index];
                    max_row = row;
                    max_col = col;
                }
            }
        }
    }
    
    // Check if we found a significant change
    if (max_difference < 0.1 || max_row == -1 || max_col == -1) {
        return DYNAMIC_TRACKER_ERROR_NO_TRAJECTORY;
    }
    
    // Convert pixel coordinates to azimuth/elevation
    point->pixel_row = max_row;
    point->pixel_col = max_col;
    point->valid = true;
    
    int coord_result = dynamic_tracker_pixel_to_az_el(max_row, max_col, &point->azimuth, &point->elevation);
    if (coord_result != DYNAMIC_TRACKER_SUCCESS) {
        point->valid = false;
        return coord_result;
    }
    
    return DYNAMIC_TRACKER_SUCCESS;
}

// Run complete satellite identification cycle (Gemini's method)
int dynamic_tracker_run_identification_cycle(dynamic_satellite_tracker_t *tracker, 
                                            const constellation_data_t *constellation,
                                            const dish_location_t *dish_location) {
    if (!tracker || !constellation || !dish_location) {
        return DYNAMIC_TRACKER_ERROR_INVALID_PARAM;
    }
    
    struct timeval cycle_start;
    gettimeofday(&cycle_start, NULL);
    tracker->cycle_start_tv = cycle_start;
    
    pthread_mutex_lock(&tracker->data_mutex);
    
    // Step 1: Capture observed trajectory using dynamic tracking
    observed_trajectory_t observed_trajectory;
    int capture_result = dynamic_tracker_capture_map_sequence(tracker, &observed_trajectory);
    
    if (capture_result != DYNAMIC_TRACKER_SUCCESS || !observed_trajectory.complete) {
        pthread_mutex_unlock(&tracker->data_mutex);
        return DYNAMIC_TRACKER_ERROR_NO_TRAJECTORY;
    }
    
    tracker->current_trajectory = observed_trajectory;
    
    // Step 2: Generate candidate satellites
    candidate_satellite_t *candidates;
    int num_candidates;
    
    int candidates_result = dynamic_tracker_generate_candidates(
        constellation, dish_location, 
        observed_trajectory.start_time, 
        tracker->config.scheduling_window_seconds,
        &candidates, &num_candidates
    );
    
    if (candidates_result != DYNAMIC_TRACKER_SUCCESS || num_candidates == 0) {
        pthread_mutex_unlock(&tracker->data_mutex);
        return DYNAMIC_TRACKER_ERROR_NO_CANDIDATES;
    }
    
    // Step 3: Identify serving satellite
    satellite_identification_t identification;
    int identification_result = dynamic_tracker_identify_serving_satellite(
        tracker, &observed_trajectory, constellation, dish_location, &identification
    );
    
    if (identification_result == DYNAMIC_TRACKER_SUCCESS) {
        tracker->last_identification = identification;
        tracker->successful_identifications++;
        
        // Trigger callback
        if (tracker->satellite_identified_callback) {
            tracker->satellite_identified_callback(&identification, tracker->callback_user_data);
        }
    }
    
    // Update statistics
    tracker->total_tracking_cycles++;
    
    struct timeval cycle_end;
    gettimeofday(&cycle_end, NULL);
    tracker->last_cycle_duration_ms = ((cycle_end.tv_sec - cycle_start.tv_sec) * 1000.0) + 
                                     ((cycle_end.tv_usec - cycle_start.tv_usec) / 1000.0);
    
    // Cleanup
    if (candidates) {
        free(candidates);
    }
    
    pthread_mutex_unlock(&tracker->data_mutex);
    
    return identification_result;
}

// Generate candidate satellites for identification
int dynamic_tracker_generate_candidates(
    const constellation_data_t *constellation,
    const dish_location_t *dish_location,
    time_t start_time,
    int duration_seconds,
    candidate_satellite_t **candidates,
    int *num_candidates) {
    
    if (!constellation || !dish_location || !candidates || !num_candidates) {
        return DYNAMIC_TRACKER_ERROR_INVALID_PARAM;
    }
    
    // Allocate candidates array
    *candidates = calloc(constellation->num_satellites, sizeof(candidate_satellite_t));
    if (!*candidates) {
        return DYNAMIC_TRACKER_ERROR_INVALID_PARAM;
    }
    
    *num_candidates = 0;
    
    // Convert dish location to Earth location for astro calculations
    earth_location_t observer = astro_earth_location(
        dish_location->latitude, 
        dish_location->longitude, 
        dish_location->altitude
    );
    
    // Check each satellite for visibility during the tracking window
    for (int i = 0; i < constellation->num_satellites; i++) {
        if (!constellation->satellites[i].is_valid) {
            continue;
        }
        
        // Parse TLE to orbital elements
        orbital_elements_t elements;
        if (prediction_engine_parse_tle(&constellation->satellites[i], &elements) != PREDICTION_SUCCESS) {
            continue;
        }
        
        // Check if satellite is potentially visible during window
        bool is_visible = false;
        astro_time_t check_time = astro_time_from_unix(start_time);
        
        // Quick visibility check at start, middle, and end of window
        for (int check_offset = 0; check_offset <= duration_seconds; check_offset += duration_seconds/2) {
            astro_time_t test_time = astro_time_from_unix(start_time + check_offset);
            
            // Simplified propagation for visibility check
            satellite_state_t sat_state;
            if (prediction_engine_propagate_satellite(&elements, start_time + check_offset, &sat_state) == PREDICTION_SUCCESS) {
                topocentric_result_t topo = astro_transform_teme_to_altaz(
                    &sat_state.position, &sat_state.velocity, &test_time, &observer
                );
                
                if (topo.valid && topo.elevation_deg >= MIN_ELEVATION_DEGREES) {
                    is_visible = true;
                    break;
                }
            }
        }
        
        if (is_visible) {
            candidate_satellite_t *candidate = &(*candidates)[*num_candidates];
            
            strncpy(candidate->satellite_id, constellation->satellites[i].satellite_name, 
                   sizeof(candidate->satellite_id) - 1);
            snprintf(candidate->norad_id, sizeof(candidate->norad_id), "%d", i); // Simplified
            candidate->elements = elements;
            candidate->match_score = INFINITY; // Will be calculated later
            candidate->is_best_match = false;
            
            // Calculate predicted trajectory for this candidate
            int trajectory_result = dynamic_tracker_calculate_predicted_trajectory(
                &elements, dish_location, start_time, duration_seconds,
                candidate->predicted_points, 16
            );
            
            if (trajectory_result == DYNAMIC_TRACKER_SUCCESS) {
                (*num_candidates)++;
                
                // Stop if we hit the candidate limit for performance
                if (*num_candidates >= 50) { // Reasonable limit
                    break;
                }
            }
        }
    }
    
    return DYNAMIC_TRACKER_SUCCESS;
}

// Calculate predicted trajectory for a candidate satellite
int dynamic_tracker_calculate_predicted_trajectory(
    const orbital_elements_t *elements,
    const dish_location_t *observer,
    time_t start_time,
    int duration_seconds,
    trajectory_point_t *points,
    int max_points) {
    
    if (!elements || !observer || !points || max_points == 0) {
        return DYNAMIC_TRACKER_ERROR_INVALID_PARAM;
    }
    
    earth_location_t earth_obs = astro_earth_location(
        observer->latitude, observer->longitude, observer->altitude
    );
    
    int point_count = 0;
    
    // Calculate positions at 1-second intervals
    for (int second = 0; second < duration_seconds && point_count < max_points; second++) {
        time_t point_time = start_time + second;
        astro_time_t astro_time = astro_time_from_unix(point_time);
        
        // Propagate satellite
        satellite_state_t sat_state;
        if (prediction_engine_propagate_satellite(elements, point_time, &sat_state) == PREDICTION_SUCCESS) {
            
            // Transform to topocentric coordinates
            topocentric_result_t topo = astro_transform_teme_to_altaz(
                &sat_state.position, &sat_state.velocity, &astro_time, &earth_obs
            );
            
            if (topo.valid && topo.elevation_deg >= MIN_ELEVATION_DEGREES) {
                trajectory_point_t *point = &points[point_count];
                point->timestamp = point_time;
                point->azimuth = topo.azimuth_deg;
                point->elevation = topo.elevation_deg;
                point->valid = true;
                
                // Convert to pixel coordinates
                pixel_coords_t pixel = dynamic_tracker_az_el_to_pixel(point->azimuth, point->elevation);
                point->pixel_row = pixel.row;
                point->pixel_col = pixel.col;
                
                point_count++;
            }
        }
    }
    
    return (point_count > 0) ? DYNAMIC_TRACKER_SUCCESS : DYNAMIC_TRACKER_ERROR_NO_TRAJECTORY;
}

// Calculate trajectory match score (Gemini's correlation algorithm)
double dynamic_tracker_calculate_trajectory_match_score(
    const observed_trajectory_t *observed,
    const candidate_satellite_t *candidate) {
    
    if (!observed || !candidate || observed->num_points == 0 || candidate->num_predicted_points == 0) {
        return INFINITY; // Worst possible score
    }
    
    double total_error = 0.0;
    int matched_points = 0;
    
    // Compare trajectories point by point
    int min_points = (observed->num_points < candidate->num_predicted_points) ? 
                     observed->num_points : candidate->num_predicted_points;
    
    for (int i = 0; i < min_points; i++) {
        if (observed->points[i].valid && candidate->predicted_points[i].valid) {
            // Calculate angular separation between observed and predicted points
            double angular_error = astro_angular_separation(
                observed->points[i].azimuth, observed->points[i].elevation,
                candidate->predicted_points[i].azimuth, candidate->predicted_points[i].elevation
            );
            
            total_error += angular_error;
            matched_points++;
        }
    }
    
    if (matched_points == 0) {
        return INFINITY;
    }
    
    // Return average angular error
    return total_error / matched_points;
}

// Identify serving satellite (Gemini's correlation method)
int dynamic_tracker_identify_serving_satellite(
    dynamic_satellite_tracker_t *tracker,
    const observed_trajectory_t *observed,
    const constellation_data_t *constellation,
    const dish_location_t *dish_location,
    satellite_identification_t *result) {
    
    if (!tracker || !observed || !constellation || !result) {
        return DYNAMIC_TRACKER_ERROR_INVALID_PARAM;
    }
    
    // Generate candidates
    candidate_satellite_t *candidates;
    int num_candidates;
    
    int candidates_result = dynamic_tracker_generate_candidates(
        constellation, dish_location, 
        observed->start_time, tracker->config.scheduling_window_seconds,
        &candidates, &num_candidates
    );
    
    if (candidates_result != DYNAMIC_TRACKER_SUCCESS || num_candidates == 0) {
        return DYNAMIC_TRACKER_ERROR_NO_CANDIDATES;
    }
    
    // Calculate match scores for all candidates
    double best_score = INFINITY;
    int best_candidate_index = -1;
    
    for (int i = 0; i < num_candidates; i++) {
        double score = dynamic_tracker_calculate_trajectory_match_score(observed, &candidates[i]);
        candidates[i].match_score = score;
        
        if (score < best_score) {
            best_score = score;
            best_candidate_index = i;
        }
    }
    
    // Check if we found a reasonable match
    if (best_candidate_index == -1 || best_score > tracker->config.trajectory_match_threshold) {
        free(candidates);
        return DYNAMIC_TRACKER_ERROR_NO_CANDIDATES;
    }
    
    // Fill result structure
    candidate_satellite_t *best_match = &candidates[best_candidate_index];
    best_match->is_best_match = true;
    
    strncpy(result->identified_satellite_id, best_match->satellite_id, sizeof(result->identified_satellite_id) - 1);
    strncpy(result->norad_id, best_match->norad_id, sizeof(result->norad_id) - 1);
    result->angular_error_degrees = best_score;
    result->confidence_score = fmax(0.0, 1.0 - (best_score / tracker->config.trajectory_match_threshold));
    result->num_trajectory_points_matched = observed->num_points;
    result->identification_time = time(NULL);
    result->observed_path = *observed;
    result->best_match = *best_match;
    
    snprintf(result->identification_details, sizeof(result->identification_details),
            "Matched %s with %.2f° average error from %d trajectory points",
            result->identified_satellite_id, result->angular_error_degrees, 
            result->num_trajectory_points_matched);
    
    free(candidates);
    
    return DYNAMIC_TRACKER_SUCCESS;
}

// Get tracking statistics
dynamic_tracking_stats_t dynamic_tracker_get_stats(const dynamic_satellite_tracker_t *tracker) {
    dynamic_tracking_stats_t stats = {0};
    
    if (!tracker) {
        return stats;
    }
    
    stats.total_cycles = tracker->total_tracking_cycles;
    stats.successful_identifications = tracker->successful_identifications;
    
    if (stats.total_cycles > 0) {
        stats.identification_rate_percent = 
            (double)stats.successful_identifications / stats.total_cycles * 100.0;
    }
    
    stats.average_cycle_duration_ms = tracker->last_cycle_duration_ms;
    stats.average_match_confidence = tracker->last_identification.confidence_score;
    stats.last_identification = tracker->last_identification.identification_time;
    
    return stats;
}

// Start tracking thread
int dynamic_tracker_start_tracking(dynamic_satellite_tracker_t *tracker) {
    if (!tracker || tracker->tracking_active) {
        return DYNAMIC_TRACKER_ERROR_INVALID_PARAM;
    }
    
    tracker->tracking_active = true;
    tracker->should_stop = false;
    
    // Create tracking thread
    if (pthread_create(&tracker->tracking_thread, NULL, dynamic_tracker_thread_main, tracker) != 0) {
        tracker->tracking_active = false;
        return DYNAMIC_TRACKER_ERROR_THREAD_FAILED;
    }
    
    return DYNAMIC_TRACKER_SUCCESS;
}

// Stop tracking
int dynamic_tracker_stop_tracking(dynamic_satellite_tracker_t *tracker) {
    if (!tracker || !tracker->tracking_active) {
        return DYNAMIC_TRACKER_SUCCESS;
    }
    
    tracker->should_stop = true;
    
    // Wait for thread to complete
    pthread_join(tracker->tracking_thread, NULL);
    
    tracker->tracking_active = false;
    
    return DYNAMIC_TRACKER_SUCCESS;
}

// Main tracking thread (runs identification cycles)
static void* dynamic_tracker_thread_main(void *arg) {
    dynamic_satellite_tracker_t *tracker = (dynamic_satellite_tracker_t*)arg;
    
    while (!tracker->should_stop) {
        // TODO: Get constellation and dish location from main tracker
        // For now, this is a placeholder that would integrate with the main system
        
        // Run identification cycle
        // int result = dynamic_tracker_run_identification_cycle(tracker, constellation, dish_location);
        
        // Sleep until next cycle
        sleep(tracker->config.tracking_interval_seconds);
    }
    
    return NULL;
}