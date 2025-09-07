#include "obstruction_analyzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <json-c/json.h>

// Mathematical constants
#define M_PI 3.14159265358979323846
#define DEG_TO_RAD (M_PI / 180.0)
#define RAD_TO_DEG (180.0 / M_PI)

// Initialize obstruction analyzer
obstruction_analyzer_t* obstruction_analyzer_init(const obstruction_analysis_config_t *config) {
    obstruction_analyzer_t *analyzer = calloc(1, sizeof(obstruction_analyzer_t));
    if (!analyzer) {
        return NULL;
    }
    
    // Set default configuration if none provided
    if (config) {
        memcpy(&analyzer->config, config, sizeof(obstruction_analysis_config_t));
    } else {
        analyzer->config.snr_threshold = OBSTRUCTION_SNR_THRESHOLD;
        analyzer->config.min_elevation = MIN_ELEVATION_DEGREES;
        analyzer->config.max_elevation = MAX_ELEVATION_DEGREES;
        analyzer->config.use_adaptive_threshold = false;
        analyzer->config.adaptive_threshold_factor = 0.8;
        analyzer->config.smoothing_window_size = 3;
    }
    
    // Initialize obstruction map
    analyzer->current_map.cells = NULL;
    analyzer->current_map.num_cells = 0;
    analyzer->current_map.grid_width = OBSTRUCTION_GRID_WIDTH;
    analyzer->current_map.grid_height = OBSTRUCTION_GRID_HEIGHT;
    analyzer->current_map.azimuth_resolution = 360.0 / OBSTRUCTION_GRID_WIDTH;
    analyzer->current_map.elevation_resolution = 90.0 / OBSTRUCTION_GRID_HEIGHT;
    
    // Allocate smoothing buffer
    analyzer->smoothing_buffer_size = OBSTRUCTION_GRID_WIDTH * OBSTRUCTION_GRID_HEIGHT;
    analyzer->smoothing_buffer = calloc(analyzer->smoothing_buffer_size, sizeof(double));
    
    if (!analyzer->smoothing_buffer) {
        free(analyzer);
        return NULL;
    }
    
    return analyzer;
}

// Cleanup obstruction analyzer
void obstruction_analyzer_cleanup(obstruction_analyzer_t *analyzer) {
    if (!analyzer) {
        return;
    }
    
    if (analyzer->current_map.cells) {
        free(analyzer->current_map.cells);
    }
    
    if (analyzer->smoothing_buffer) {
        free(analyzer->smoothing_buffer);
    }
    
    free(analyzer);
}

// Parse dish gRPC response for obstruction map and location
int obstruction_analyzer_parse_dish_response(const char *response, obstruction_map_t *map, dish_location_t *location) {
    if (!response || !map) {
        return OBSTRUCTION_ERROR_INVALID_PARAM;
    }
    
    json_object *root = json_tokener_parse(response);
    if (!root) {
        return OBSTRUCTION_ERROR_PARSE_FAILED;
    }
    
    // Parse obstruction map if present
    json_object *obstruction_response;
    if (json_object_object_get_ex(root, "dishGetObstructionMap", &obstruction_response)) {
        json_object *obstruction_map_obj;
        if (json_object_object_get_ex(obstruction_response, "obstructionMap", &obstruction_map_obj)) {
            
            // Get SNR values array
            json_object *snr_array;
            if (json_object_object_get_ex(obstruction_map_obj, "snr", &snr_array)) {
                int array_length = json_object_array_length(snr_array);
                
                // Allocate cells
                if (map->cells) {
                    free(map->cells);
                }
                map->cells = calloc(array_length, sizeof(obstruction_cell_t));
                if (!map->cells) {
                    json_object_put(root);
                    return OBSTRUCTION_ERROR_MEMORY_FAILED;
                }
                
                map->num_cells = array_length;
                
                // Parse SNR values and calculate positions
                for (int i = 0; i < array_length; i++) {
                    json_object *snr_value = json_object_array_get_idx(snr_array, i);
                    double snr = json_object_get_double(snr_value);
                    
                    // Calculate azimuth and elevation from grid index
                    int row = i / map->grid_width;
                    int col = i % map->grid_width;
                    
                    map->cells[i].azimuth = col * map->azimuth_resolution;
                    map->cells[i].elevation = (map->grid_height - 1 - row) * map->elevation_resolution;
                    map->cells[i].snr_quality = snr;
                    map->cells[i].is_obstructed = (snr < OBSTRUCTION_SNR_THRESHOLD);
                }
                
                map->last_update = time(NULL);
            }
        }
    }
    
    // Parse location data if present and location pointer provided
    if (location) {
        json_object *location_response;
        if (json_object_object_get_ex(root, "getLocation", &location_response)) {
            json_object *lat_obj, *lon_obj, *alt_obj;
            
            if (json_object_object_get_ex(location_response, "latitude", &lat_obj)) {
                location->latitude = json_object_get_double(lat_obj);
            }
            
            if (json_object_object_get_ex(location_response, "longitude", &lon_obj)) {
                location->longitude = json_object_get_double(lon_obj);
            }
            
            if (json_object_object_get_ex(location_response, "altitude", &alt_obj)) {
                location->altitude = json_object_get_double(alt_obj);
            }
            
            location->last_update = time(NULL);
        }
        
        // Parse diagnostics for boresight if present
        json_object *diagnostics_response;
        if (json_object_object_get_ex(root, "dishGetDiagnostics", &diagnostics_response)) {
            json_object *alignment_stats;
            if (json_object_object_get_ex(diagnostics_response, "alignmentStats", &alignment_stats)) {
                json_object *boresight_az, *boresight_el;
                
                if (json_object_object_get_ex(alignment_stats, "boresightAzimuthDeg", &boresight_az)) {
                    location->boresight_azimuth = json_object_get_double(boresight_az);
                }
                
                if (json_object_object_get_ex(alignment_stats, "boresightElevationDeg", &boresight_el)) {
                    location->boresight_elevation = json_object_get_double(boresight_el);
                }
            }
        }
    }
    
    json_object_put(root);
    return OBSTRUCTION_SUCCESS;
}

// Update obstruction map from gRPC response
int obstruction_analyzer_update_map(obstruction_analyzer_t *analyzer, const char *grpc_response) {
    if (!analyzer || !grpc_response) {
        return OBSTRUCTION_ERROR_INVALID_PARAM;
    }
    
    int result = obstruction_analyzer_parse_dish_response(grpc_response, &analyzer->current_map, NULL);
    
    if (result == OBSTRUCTION_SUCCESS) {
        analyzer->total_analyses++;
        analyzer->last_analysis = time(NULL);
        
        // Apply smoothing if configured
        if (analyzer->config.smoothing_window_size > 1) {
            obstruction_analyzer_apply_smoothing(analyzer, &analyzer->current_map);
        }
        
        // Update statistics
        obstruction_map_stats_t stats = obstruction_analyzer_get_map_stats(&analyzer->current_map);
        analyzer->obstructed_count = stats.obstructed_cells;
        analyzer->clear_count = stats.clear_cells;
        analyzer->average_snr = stats.average_snr;
        
        if (analyzer->log_callback) {
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg), 
                    "Updated obstruction map: %d cells, %.1f%% obstructed, avg SNR %.2f", 
                    stats.total_cells, stats.obstruction_percentage, stats.average_snr);
            analyzer->log_callback(1, log_msg, analyzer->log_user_data);
        }
    }
    
    return result;
}

// Check if a satellite at given coordinates is obstructed
obstruction_analysis_result_t obstruction_analyzer_check_satellite(
    const obstruction_analyzer_t *analyzer,
    double satellite_azimuth,
    double satellite_elevation) {
    
    obstruction_analysis_result_t result = {0};
    
    if (!analyzer) {
        result.is_obstructed = true;
        result.confidence_score = 0.0;
        strncpy(result.analysis_details, "Invalid analyzer", sizeof(result.analysis_details) - 1);
        return result;
    }
    
    // Check elevation bounds
    if (satellite_elevation < analyzer->config.min_elevation) {
        result.is_obstructed = true;
        result.confidence_score = 1.0;
        snprintf(result.analysis_details, sizeof(result.analysis_details), 
                "Below minimum elevation (%.1f° < %.1f°)", 
                satellite_elevation, analyzer->config.min_elevation);
        return result;
    }
    
    if (satellite_elevation > analyzer->config.max_elevation) {
        result.is_obstructed = false;
        result.confidence_score = 1.0;
        snprintf(result.analysis_details, sizeof(result.analysis_details), 
                "Above maximum elevation (%.1f° > %.1f°)", 
                satellite_elevation, analyzer->config.max_elevation);
        return result;
    }
    
    // Normalize azimuth to [0, 360)
    double norm_azimuth = obstruction_analyzer_normalize_azimuth(satellite_azimuth);
    
    // Get SNR quality from obstruction map
    result.snr_quality = obstruction_analyzer_interpolate_snr(&analyzer->current_map, norm_azimuth, satellite_elevation);
    
    // Determine obstruction threshold (adaptive or fixed)
    double threshold = analyzer->config.snr_threshold;
    if (analyzer->config.use_adaptive_threshold) {
        threshold = obstruction_analyzer_calculate_adaptive_threshold(analyzer, &analyzer->current_map);
    }
    
    // Determine obstruction status
    result.is_obstructed = (result.snr_quality < threshold);
    
    // Calculate confidence based on how far from threshold
    double distance_from_threshold = fabs(result.snr_quality - threshold);
    result.confidence_score = fmin(1.0, distance_from_threshold * 2.0); // Scale factor
    
    // Create analysis details
    snprintf(result.analysis_details, sizeof(result.analysis_details), 
            "Az: %.1f°, El: %.1f°, SNR: %.2f, Threshold: %.2f, %s", 
            norm_azimuth, satellite_elevation, result.snr_quality, threshold,
            result.is_obstructed ? "OBSTRUCTED" : "CLEAR");
    
    return result;
}

// Get obstruction grid cell for given coordinates
int obstruction_analyzer_get_grid_cell(
    const obstruction_map_t *map,
    double azimuth,
    double elevation,
    obstruction_cell_t *cell) {
    
    if (!map || !cell || !map->cells) {
        return OBSTRUCTION_ERROR_INVALID_PARAM;
    }
    
    // Normalize coordinates
    double norm_azimuth = obstruction_analyzer_normalize_azimuth(azimuth);
    double norm_elevation = obstruction_analyzer_clamp_elevation(elevation);
    
    // Calculate grid indices
    int col = (int)(norm_azimuth / map->azimuth_resolution);
    int row = map->grid_height - 1 - (int)(norm_elevation / map->elevation_resolution);
    
    // Clamp indices to valid range
    col = (col < 0) ? 0 : ((col >= map->grid_width) ? map->grid_width - 1 : col);
    row = (row < 0) ? 0 : ((row >= map->grid_height) ? map->grid_height - 1 : row);
    
    // Calculate linear index
    int index = row * map->grid_width + col;
    
    if (index < 0 || index >= map->num_cells) {
        return OBSTRUCTION_ERROR_INVALID_COORDS;
    }
    
    // Copy cell data
    memcpy(cell, &map->cells[index], sizeof(obstruction_cell_t));
    
    return OBSTRUCTION_SUCCESS;
}

// Interpolate SNR value using bilinear interpolation
double obstruction_analyzer_interpolate_snr(
    const obstruction_map_t *map,
    double azimuth,
    double elevation) {
    
    if (!map || !map->cells) {
        return 0.0;
    }
    
    // Normalize coordinates
    double norm_azimuth = obstruction_analyzer_normalize_azimuth(azimuth);
    double norm_elevation = obstruction_analyzer_clamp_elevation(elevation);
    
    // Calculate fractional grid position
    double col_f = norm_azimuth / map->azimuth_resolution;
    double row_f = (map->grid_height - 1) - (norm_elevation / map->elevation_resolution);
    
    // Get integer indices
    int col0 = (int)floor(col_f);
    int row0 = (int)floor(row_f);
    int col1 = col0 + 1;
    int row1 = row0 + 1;
    
    // Clamp to valid range
    col0 = (col0 < 0) ? 0 : ((col0 >= map->grid_width) ? map->grid_width - 1 : col0);
    col1 = (col1 < 0) ? 0 : ((col1 >= map->grid_width) ? map->grid_width - 1 : col1);
    row0 = (row0 < 0) ? 0 : ((row0 >= map->grid_height) ? map->grid_height - 1 : row0);
    row1 = (row1 < 0) ? 0 : ((row1 >= map->grid_height) ? map->grid_height - 1 : row1);
    
    // Get the four corner values
    int idx00 = row0 * map->grid_width + col0;
    int idx01 = row0 * map->grid_width + col1;
    int idx10 = row1 * map->grid_width + col0;
    int idx11 = row1 * map->grid_width + col1;
    
    // Ensure indices are valid
    if (idx00 >= map->num_cells || idx01 >= map->num_cells || 
        idx10 >= map->num_cells || idx11 >= map->num_cells) {
        // Fallback to nearest neighbor
        int nearest_idx = ((int)round(row_f)) * map->grid_width + ((int)round(col_f));
        nearest_idx = (nearest_idx < 0) ? 0 : ((nearest_idx >= map->num_cells) ? map->num_cells - 1 : nearest_idx);
        return map->cells[nearest_idx].snr_quality;
    }
    
    double v00 = map->cells[idx00].snr_quality;
    double v01 = map->cells[idx01].snr_quality;
    double v10 = map->cells[idx10].snr_quality;
    double v11 = map->cells[idx11].snr_quality;
    
    // Calculate interpolation weights
    double w_col = col_f - col0;
    double w_row = row_f - row0;
    
    // Bilinear interpolation
    double v0 = v00 * (1.0 - w_col) + v01 * w_col;
    double v1 = v10 * (1.0 - w_col) + v11 * w_col;
    double interpolated = v0 * (1.0 - w_row) + v1 * w_row;
    
    return interpolated;
}

// Convert dish-relative coordinates to absolute coordinates
void obstruction_analyzer_dish_to_absolute_coords(
    double dish_relative_az,
    double dish_relative_el,
    double boresight_azimuth,
    double boresight_elevation,
    double *absolute_az,
    double *absolute_el) {
    
    if (!absolute_az || !absolute_el) {
        return;
    }
    
    // Implement proper spherical coordinate transformation using rotation matrices
    // Convert angles to radians
    double dish_az_rad = dish_relative_az * M_PI / 180.0;
    double dish_el_rad = dish_relative_el * M_PI / 180.0;
    double bore_az_rad = boresight_azimuth * M_PI / 180.0;
    double bore_el_rad = boresight_elevation * M_PI / 180.0;
    
    // Convert dish-relative spherical coordinates to Cartesian unit vector
    // In dish coordinate system: x=forward, y=right, z=up
    double dish_x = cos(dish_el_rad) * cos(dish_az_rad);
    double dish_y = cos(dish_el_rad) * sin(dish_az_rad);
    double dish_z = sin(dish_el_rad);
    
    // Build rotation matrix to transform from dish coordinates to absolute coordinates
    // This requires two rotations:
    // 1. Rotation around Z-axis by boresight azimuth
    // 2. Rotation around Y-axis by (90° - boresight elevation)
    
    // First rotation matrix (azimuth rotation around Z-axis)
    double cos_az = cos(bore_az_rad);
    double sin_az = sin(bore_az_rad);
    
    // Second rotation matrix (elevation rotation around Y-axis)
    // Note: We rotate by (π/2 - elevation) to align with horizon
    double pitch_angle = M_PI/2 - bore_el_rad;
    double cos_pitch = cos(pitch_angle);
    double sin_pitch = sin(pitch_angle);
    
    // Combined rotation matrix application
    // Step 1: Apply elevation rotation (around Y-axis)
    double temp_x = dish_x * cos_pitch + dish_z * sin_pitch;
    double temp_y = dish_y;
    double temp_z = -dish_x * sin_pitch + dish_z * cos_pitch;
    
    // Step 2: Apply azimuth rotation (around Z-axis)
    double absolute_x = temp_x * cos_az - temp_y * sin_az;
    double absolute_y = temp_x * sin_az + temp_y * cos_az;
    double absolute_z = temp_z;
    
    // Convert back to spherical coordinates
    double r_xy = sqrt(absolute_x * absolute_x + absolute_y * absolute_y);
    
    // Calculate absolute azimuth and elevation
    *absolute_az = atan2(absolute_y, absolute_x) * 180.0 / M_PI;
    *absolute_el = atan2(absolute_z, r_xy) * 180.0 / M_PI;
    
    // Normalize azimuth to [0, 360) range
    *absolute_az = obstruction_analyzer_normalize_azimuth(*absolute_az);
    
    // Clamp elevation to [0, 90] range
    *absolute_el = obstruction_analyzer_clamp_elevation(*absolute_el);
    
    // Validate the transformed coordinates
    if (*absolute_az < 0.0 || *absolute_az >= 360.0) {
        *absolute_az = obstruction_analyzer_normalize_azimuth(*absolute_az);
    }
    if (*absolute_el < 0.0 || *absolute_el > 90.0) {
        *absolute_el = obstruction_analyzer_clamp_elevation(*absolute_el);
    }
}

// Convert absolute coordinates to dish-relative coordinates
void obstruction_analyzer_absolute_to_dish_coords(
    double absolute_az,
    double absolute_el,
    double boresight_azimuth,
    double boresight_elevation,
    double *dish_relative_az,
    double *dish_relative_el) {
    
    if (!dish_relative_az || !dish_relative_el) {
        return;
    }
    
    // Implement inverse spherical coordinate transformation
    // Convert angles to radians
    double abs_az_rad = absolute_az * M_PI / 180.0;
    double abs_el_rad = absolute_el * M_PI / 180.0;
    double bore_az_rad = boresight_azimuth * M_PI / 180.0;
    double bore_el_rad = boresight_elevation * M_PI / 180.0;
    
    // Convert absolute spherical coordinates to Cartesian unit vector
    double absolute_x = cos(abs_el_rad) * cos(abs_az_rad);
    double absolute_y = cos(abs_el_rad) * sin(abs_az_rad);
    double absolute_z = sin(abs_el_rad);
    
    // Build inverse rotation matrix (transpose of forward rotation)
    // This requires two inverse rotations:
    // 1. Inverse rotation around Z-axis by -boresight azimuth
    // 2. Inverse rotation around Y-axis by -(90° - boresight elevation)
    
    double cos_az = cos(-bore_az_rad);
    double sin_az = sin(-bore_az_rad);
    
    double pitch_angle = -(M_PI/2 - bore_el_rad);
    double cos_pitch = cos(pitch_angle);
    double sin_pitch = sin(pitch_angle);
    
    // Apply inverse rotations in reverse order
    // Step 1: Apply inverse azimuth rotation (around Z-axis)
    double temp_x = absolute_x * cos_az - absolute_y * sin_az;
    double temp_y = absolute_x * sin_az + absolute_y * cos_az;
    double temp_z = absolute_z;
    
    // Step 2: Apply inverse elevation rotation (around Y-axis)
    double dish_x = temp_x * cos_pitch + temp_z * sin_pitch;
    double dish_y = temp_y;
    double dish_z = -temp_x * sin_pitch + temp_z * cos_pitch;
    
    // Convert back to spherical coordinates in dish reference frame
    double r_xy = sqrt(dish_x * dish_x + dish_y * dish_y);
    
    // Calculate dish-relative azimuth and elevation
    *dish_relative_az = atan2(dish_y, dish_x) * 180.0 / M_PI;
    *dish_relative_el = atan2(dish_z, r_xy) * 180.0 / M_PI;
    
    // Handle azimuth wraparound
    if (*dish_relative_az > 180.0) {
        *dish_relative_az -= 360.0;
    } else if (*dish_relative_az < -180.0) {
        *dish_relative_az += 360.0;
    }
}

// Normalize azimuth to [0, 360) range
double obstruction_analyzer_normalize_azimuth(double azimuth) {
    while (azimuth < 0.0) {
        azimuth += 360.0;
    }
    while (azimuth >= 360.0) {
        azimuth -= 360.0;
    }
    return azimuth;
}

// Clamp elevation to valid range [0, 90]
double obstruction_analyzer_clamp_elevation(double elevation) {
    if (elevation < 0.0) {
        return 0.0;
    }
    if (elevation > 90.0) {
        return 90.0;
    }
    return elevation;
}

// Check if elevation is in valid range
bool obstruction_analyzer_is_elevation_valid(double elevation) {
    return (elevation >= 0.0 && elevation <= 90.0);
}

// Check if azimuth is in valid range
bool obstruction_analyzer_is_azimuth_valid(double azimuth) {
    // Azimuth can be any value, we'll normalize it
    return !isnan(azimuth) && !isinf(azimuth);
}

// Calculate adaptive threshold based on map statistics
double obstruction_analyzer_calculate_adaptive_threshold(
    const obstruction_analyzer_t *analyzer,
    const obstruction_map_t *map) {
    
    if (!analyzer || !map || !map->cells) {
        return analyzer ? analyzer->config.snr_threshold : OBSTRUCTION_SNR_THRESHOLD;
    }
    
    // Calculate statistics
    double sum = 0.0;
    int valid_count = 0;
    
    for (int i = 0; i < map->num_cells; i++) {
        if (!isnan(map->cells[i].snr_quality) && !isinf(map->cells[i].snr_quality)) {
            sum += map->cells[i].snr_quality;
            valid_count++;
        }
    }
    
    if (valid_count == 0) {
        return analyzer->config.snr_threshold;
    }
    
    double mean_snr = sum / valid_count;
    
    // Calculate standard deviation
    double variance_sum = 0.0;
    for (int i = 0; i < map->num_cells; i++) {
        if (!isnan(map->cells[i].snr_quality) && !isinf(map->cells[i].snr_quality)) {
            double diff = map->cells[i].snr_quality - mean_snr;
            variance_sum += diff * diff;
        }
    }
    
    double std_dev = sqrt(variance_sum / valid_count);
    
    // Adaptive threshold = mean - (factor * std_dev)
    double adaptive_threshold = mean_snr - (analyzer->config.adaptive_threshold_factor * std_dev);
    
    // Ensure threshold is reasonable
    if (adaptive_threshold < 0.1) {
        adaptive_threshold = 0.1;
    }
    if (adaptive_threshold > 1.0) {
        adaptive_threshold = 1.0;
    }
    
    return adaptive_threshold;
}

// Get statistics for obstruction map
obstruction_map_stats_t obstruction_analyzer_get_map_stats(const obstruction_map_t *map) {
    obstruction_map_stats_t stats = {0};
    
    if (!map || !map->cells) {
        return stats;
    }
    
    stats.total_cells = map->num_cells;
    stats.analysis_time = map->last_update;
    
    double sum = 0.0;
    double min_snr = 1.0;
    double max_snr = 0.0;
    int valid_count = 0;
    
    for (int i = 0; i < map->num_cells; i++) {
        if (!isnan(map->cells[i].snr_quality) && !isinf(map->cells[i].snr_quality)) {
            double snr = map->cells[i].snr_quality;
            sum += snr;
            valid_count++;
            
            if (snr < min_snr) min_snr = snr;
            if (snr > max_snr) max_snr = snr;
            
            if (map->cells[i].is_obstructed) {
                stats.obstructed_cells++;
            } else {
                stats.clear_cells++;
            }
        }
    }
    
    if (valid_count > 0) {
        stats.average_snr = sum / valid_count;
        stats.min_snr = min_snr;
        stats.max_snr = max_snr;
        stats.obstruction_percentage = (double)stats.obstructed_cells / valid_count * 100.0;
    }
    
    return stats;
}

// Apply smoothing to obstruction map
int obstruction_analyzer_apply_smoothing(obstruction_analyzer_t *analyzer, obstruction_map_t *map) {
    if (!analyzer || !map || !map->cells || analyzer->config.smoothing_window_size <= 1) {
        return OBSTRUCTION_SUCCESS; // No smoothing needed
    }
    
    int window_size = analyzer->config.smoothing_window_size;
    int half_window = window_size / 2;
    
    // Use pre-allocated smoothing buffer
    if (analyzer->smoothing_buffer_size < map->num_cells) {
        // This case should ideally not happen if grid size is constant
        // Handle error or reallocate if necessary
        return OBSTRUCTION_ERROR_MEMORY_FAILED;
    }
    double *smoothed = analyzer->smoothing_buffer;
    
    // Apply 2D smoothing filter
    for (int row = 0; row < map->grid_height; row++) {
        for (int col = 0; col < map->grid_width; col++) {
            int center_idx = row * map->grid_width + col;
            double sum = 0.0;
            int count = 0;
            
            // Average over window
            for (int dr = -half_window; dr <= half_window; dr++) {
                for (int dc = -half_window; dc <= half_window; dc++) {
                    int r = row + dr;
                    int c = col + dc;
                    
                    // Check bounds
                    if (r >= 0 && r < map->grid_height && c >= 0 && c < map->grid_width) {
                        int idx = r * map->grid_width + c;
                        if (idx < map->num_cells) {
                            sum += map->cells[idx].snr_quality;
                            count++;
                        }
                    }
                }
            }
            
            if (count > 0) {
                smoothed[center_idx] = sum / count;
            } else {
                smoothed[center_idx] = map->cells[center_idx].snr_quality;
            }
        }
    }
    
    // Update map with smoothed values
    for (int i = 0; i < map->num_cells; i++) {
        map->cells[i].snr_quality = smoothed[i];
        map->cells[i].is_obstructed = (smoothed[i] < analyzer->config.snr_threshold);
    }
    
    return OBSTRUCTION_SUCCESS;
}

// Check multiple satellites for obstruction
int obstruction_analyzer_check_multiple_satellites(
    const obstruction_analyzer_t *analyzer,
    const satellite_position_t *satellites,
    int num_satellites,
    obstruction_analysis_result_t *results) {
    
    if (!analyzer || !satellites || !results || num_satellites <= 0) {
        return OBSTRUCTION_ERROR_INVALID_PARAM;
    }
    
    for (int i = 0; i < num_satellites; i++) {
        results[i] = obstruction_analyzer_check_satellite(analyzer, 
                                                          satellites[i].azimuth, 
                                                          satellites[i].elevation);
    }
    
    return OBSTRUCTION_SUCCESS;
}

// Export obstruction map to CSV for debugging
int obstruction_analyzer_export_map_csv(const obstruction_map_t *map, const char *filename) {
    if (!map || !filename || !map->cells) {
        return OBSTRUCTION_ERROR_INVALID_PARAM;
    }
    
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        return OBSTRUCTION_ERROR_INVALID_PARAM;
    }
    
    // Write CSV header
    fprintf(fp, "Row,Col,Azimuth,Elevation,SNR,IsObstructed\n");
    
    // Write data
    for (int row = 0; row < map->grid_height; row++) {
        for (int col = 0; col < map->grid_width; col++) {
            int idx = row * map->grid_width + col;
            if (idx < map->num_cells) {
                fprintf(fp, "%d,%d,%.2f,%.2f,%.3f,%d\n",
                       row, col,
                       map->cells[idx].azimuth,
                       map->cells[idx].elevation,
                       map->cells[idx].snr_quality,
                       map->cells[idx].is_obstructed ? 1 : 0);
            }
        }
    }
    
    fclose(fp);
    return OBSTRUCTION_SUCCESS;
}

// Print obstruction map summary
int obstruction_analyzer_print_map_summary(const obstruction_map_t *map) {
    if (!map) {
        return OBSTRUCTION_ERROR_INVALID_PARAM;
    }
    
    obstruction_map_stats_t stats = obstruction_analyzer_get_map_stats(map);
    
    printf("Obstruction Map Summary:\n");
    printf("  Total cells: %d\n", stats.total_cells);
    printf("  Obstructed cells: %d\n", stats.obstructed_cells);
    printf("  Clear cells: %d\n", stats.clear_cells);
    printf("  Obstruction percentage: %.1f%%\n", stats.obstruction_percentage);
    printf("  Average SNR: %.3f\n", stats.average_snr);
    printf("  Min SNR: %.3f\n", stats.min_snr);
    printf("  Max SNR: %.3f\n", stats.max_snr);
    printf("  Last update: %s", ctime(&stats.analysis_time));
    
    return OBSTRUCTION_SUCCESS;
}