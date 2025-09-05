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

// Enhanced coordinate conversion based on Gemini's research
typedef struct {
    int row;
    int col;
    bool valid;
} pixel_coords_t;

// Convert azimuth/elevation to pixel coordinates (Gemini's algorithm)
pixel_coords_t convert_az_el_to_pixel(double az_deg, double el_deg) {
    pixel_coords_t result = {0, 0, false};
    
    // Check elevation bounds (dish operational range is 25-90°)
    if (el_deg < MIN_ELEVATION_DEGREES || el_deg > MAX_ELEVATION_DEGREES) {
        return result; // Below horizon mask or above zenith
    }
    
    // Normalize elevation to a radius (0 at zenith, 1 at edge)
    double elevation_range = MAX_ELEVATION_DEGREES - MIN_ELEVATION_DEGREES; // 90 - 25 = 65°
    double radius_normalized = (MAX_ELEVATION_DEGREES - el_deg) / elevation_range;
    double pixel_radius = radius_normalized * OBSTRUCTION_MAX_RADIUS_PIXELS;
    
    // Convert polar (radius, azimuth) to Cartesian (x, y) coordinates
    // Subtract 90 degrees from azimuth because North (0°) is 'up' (90° in standard math)
    double az_rad = (az_deg - 90.0) * DEG_TO_RAD;
    
    int col = (int)(OBSTRUCTION_CENTER_PIXEL + pixel_radius * cos(az_rad));
    int row = (int)(OBSTRUCTION_CENTER_PIXEL + pixel_radius * sin(az_rad)); // Inverted y-axis
    
    // Ensure coordinates are within bounds
    if (row >= 0 && row < OBSTRUCTION_MAP_DIAMETER && col >= 0 && col < OBSTRUCTION_MAP_DIAMETER) {
        result.row = row;
        result.col = col;
        result.valid = true;
    }
    
    return result;
}

// Parse enhanced obstruction map (15,129 SNR values in 123x123 grid)
int obstruction_analyzer_parse_enhanced_dish_response(const char *response, obstruction_map_t *map, dish_location_t *location) {
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
            
            // Get SNR data array (should be 15,129 values for 123x123 grid)
            json_object *snr_array;
            if (json_object_object_get_ex(obstruction_map_obj, "data", &snr_array)) {
                int array_length = json_object_array_length(snr_array);
                
                // Verify this is the expected 123x123 grid
                if (array_length != OBSTRUCTION_MAP_SIZE) {
                    json_object_put(root);
                    return OBSTRUCTION_ERROR_PARSE_FAILED;
                }
                
                // Allocate SNR data array
                if (map->snr_data) {
                    free(map->snr_data);
                }
                map->snr_data = calloc(OBSTRUCTION_MAP_SIZE, sizeof(double));
                if (!map->snr_data) {
                    json_object_put(root);
                    return OBSTRUCTION_ERROR_MEMORY_FAILED;
                }
                
                // Parse SNR values
                for (int i = 0; i < array_length; i++) {
                    json_object *snr_value = json_object_array_get_idx(snr_array, i);
                    map->snr_data[i] = json_object_get_double(snr_value);
                }
                
                // Set map parameters
                map->map_diameter = OBSTRUCTION_MAP_DIAMETER;
                map->center_pixel = OBSTRUCTION_CENTER_PIXEL;
                map->max_radius_pixels = OBSTRUCTION_MAX_RADIUS_PIXELS;
                map->min_elevation_deg = MIN_ELEVATION_DEGREES;
                map->max_elevation_deg = MAX_ELEVATION_DEGREES;
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

// Enhanced SNR lookup using proper polar projection (Gemini's method)
double obstruction_analyzer_get_snr_at_position(const obstruction_map_t *map, double azimuth, double elevation) {
    if (!map || !map->snr_data) {
        return 0.0;
    }
    
    // Convert to pixel coordinates using Gemini's algorithm
    pixel_coords_t pixel = convert_az_el_to_pixel(azimuth, elevation);
    
    if (!pixel.valid) {
        return 0.0; // Outside valid range
    }
    
    // Get SNR value from 2D array
    int index = pixel.row * OBSTRUCTION_MAP_DIAMETER + pixel.col;
    
    if (index < 0 || index >= OBSTRUCTION_MAP_SIZE) {
        return 0.0;
    }
    
    return map->snr_data[index];
}

// Enhanced satellite obstruction check with detailed SNR analysis
obstruction_analysis_result_t obstruction_analyzer_check_satellite_enhanced(
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
    
    // Check elevation bounds (dish operational range)
    if (satellite_elevation < MIN_ELEVATION_DEGREES) {
        result.is_obstructed = true;
        result.confidence_score = 1.0;
        snprintf(result.analysis_details, sizeof(result.analysis_details), 
                "Below dish minimum elevation (%.1f° < %.1f°)", 
                satellite_elevation, MIN_ELEVATION_DEGREES);
        return result;
    }
    
    if (satellite_elevation > MAX_ELEVATION_DEGREES) {
        result.is_obstructed = false;
        result.confidence_score = 1.0;
        snprintf(result.analysis_details, sizeof(result.analysis_details), 
                "At zenith (%.1f°)", satellite_elevation);
        return result;
    }
    
    // Get SNR quality from enhanced polar projection map
    result.snr_quality = obstruction_analyzer_get_snr_at_position(&analyzer->current_map, 
                                                                  satellite_azimuth, 
                                                                  satellite_elevation);
    
    // Determine obstruction threshold (adaptive or fixed)
    double threshold = analyzer->config.snr_threshold;
    if (analyzer->config.use_adaptive_threshold) {
        threshold = obstruction_analyzer_calculate_adaptive_threshold_enhanced(analyzer, &analyzer->current_map);
    }
    
    // Enhanced obstruction classification based on SNR levels
    if (result.snr_quality < threshold * 0.5) {
        result.is_obstructed = true;
        result.confidence_score = 1.0;
        snprintf(result.analysis_details, sizeof(result.analysis_details), 
                "CRITICAL obstruction: SNR %.3f << threshold %.3f", 
                result.snr_quality, threshold);
    } else if (result.snr_quality < threshold) {
        result.is_obstructed = true;
        result.confidence_score = 0.7;
        snprintf(result.analysis_details, sizeof(result.analysis_details), 
                "MARGINAL obstruction: SNR %.3f < threshold %.3f", 
                result.snr_quality, threshold);
    } else {
        result.is_obstructed = false;
        result.confidence_score = fmin(1.0, result.snr_quality / threshold);
        snprintf(result.analysis_details, sizeof(result.analysis_details), 
                "CLEAR: SNR %.3f > threshold %.3f", 
                result.snr_quality, threshold);
    }
    
    return result;
}

// Enhanced adaptive threshold calculation for 123x123 map
double obstruction_analyzer_calculate_adaptive_threshold_enhanced(
    const obstruction_analyzer_t *analyzer,
    const obstruction_map_t *map) {
    
    if (!analyzer || !map || !map->snr_data) {
        return analyzer ? analyzer->config.snr_threshold : OBSTRUCTION_SNR_THRESHOLD;
    }
    
    // Calculate statistics over valid sky area only (inside the circle)
    double sum = 0.0;
    int valid_count = 0;
    
    for (int row = 0; row < OBSTRUCTION_MAP_DIAMETER; row++) {
        for (int col = 0; col < OBSTRUCTION_MAP_DIAMETER; col++) {
            // Check if pixel is within the circular sky projection
            double dx = col - OBSTRUCTION_CENTER_PIXEL;
            double dy = row - OBSTRUCTION_CENTER_PIXEL;
            double radius = sqrt(dx * dx + dy * dy);
            
            if (radius <= OBSTRUCTION_MAX_RADIUS_PIXELS) {
                int index = row * OBSTRUCTION_MAP_DIAMETER + col;
                double snr = map->snr_data[index];
                
                if (!isnan(snr) && !isinf(snr) && snr >= 0.0) {
                    sum += snr;
                    valid_count++;
                }
            }
        }
    }
    
    if (valid_count == 0) {
        return analyzer->config.snr_threshold;
    }
    
    double mean_snr = sum / valid_count;
    
    // Calculate standard deviation over valid pixels only
    double variance_sum = 0.0;
    for (int row = 0; row < OBSTRUCTION_MAP_DIAMETER; row++) {
        for (int col = 0; col < OBSTRUCTION_MAP_DIAMETER; col++) {
            double dx = col - OBSTRUCTION_CENTER_PIXEL;
            double dy = row - OBSTRUCTION_CENTER_PIXEL;
            double radius = sqrt(dx * dx + dy * dy);
            
            if (radius <= OBSTRUCTION_MAX_RADIUS_PIXELS) {
                int index = row * OBSTRUCTION_MAP_DIAMETER + col;
                double snr = map->snr_data[index];
                
                if (!isnan(snr) && !isinf(snr) && snr >= 0.0) {
                    double diff = snr - mean_snr;
                    variance_sum += diff * diff;
                }
            }
        }
    }
    
    double std_dev = sqrt(variance_sum / valid_count);
    
    // Adaptive threshold = mean - (factor * std_dev)
    double adaptive_threshold = mean_snr - (analyzer->config.adaptive_threshold_factor * std_dev);
    
    // Ensure threshold is reasonable
    adaptive_threshold = fmax(0.1, fmin(1.0, adaptive_threshold));
    
    return adaptive_threshold;
}

// Enhanced obstruction map statistics for 123x123 polar projection
obstruction_map_stats_t obstruction_analyzer_get_enhanced_map_stats(const obstruction_map_t *map) {
    obstruction_map_stats_t stats = {0};
    
    if (!map || !map->snr_data) {
        return stats;
    }
    
    stats.analysis_time = map->last_update;
    
    double sum = 0.0;
    double min_snr = 1.0;
    double max_snr = 0.0;
    int valid_count = 0;
    int obstructed_count = 0;
    
    // Only analyze pixels within the circular sky projection
    for (int row = 0; row < OBSTRUCTION_MAP_DIAMETER; row++) {
        for (int col = 0; col < OBSTRUCTION_MAP_DIAMETER; col++) {
            // Check if pixel is within the circular projection
            double dx = col - OBSTRUCTION_CENTER_PIXEL;
            double dy = row - OBSTRUCTION_CENTER_PIXEL;
            double radius = sqrt(dx * dx + dy * dy);
            
            if (radius <= OBSTRUCTION_MAX_RADIUS_PIXELS) {
                int index = row * OBSTRUCTION_MAP_DIAMETER + col;
                double snr = map->snr_data[index];
                
                if (!isnan(snr) && !isinf(snr) && snr >= 0.0) {
                    sum += snr;
                    valid_count++;
                    
                    if (snr < min_snr) min_snr = snr;
                    if (snr > max_snr) max_snr = snr;
                    
                    if (snr < OBSTRUCTION_SNR_THRESHOLD) {
                        obstructed_count++;
                    }
                }
            }
        }
    }
    
    stats.total_cells = valid_count;
    stats.obstructed_cells = obstructed_count;
    stats.clear_cells = valid_count - obstructed_count;
    
    if (valid_count > 0) {
        stats.average_snr = sum / valid_count;
        stats.min_snr = min_snr;
        stats.max_snr = max_snr;
        stats.obstruction_percentage = (double)obstructed_count / valid_count * 100.0;
    }
    
    return stats;
}

// Export enhanced obstruction map to CSV with proper coordinates
int obstruction_analyzer_export_enhanced_map_csv(const obstruction_map_t *map, const char *filename) {
    if (!map || !filename || !map->snr_data) {
        return OBSTRUCTION_ERROR_INVALID_PARAM;
    }
    
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        return OBSTRUCTION_ERROR_INVALID_PARAM;
    }
    
    // Write CSV header
    fprintf(fp, "Row,Col,Azimuth,Elevation,SNR,IsObstructed,InValidArea\n");
    
    // Write data for each pixel
    for (int row = 0; row < OBSTRUCTION_MAP_DIAMETER; row++) {
        for (int col = 0; col < OBSTRUCTION_MAP_DIAMETER; col++) {
            // Check if pixel is within the circular projection
            double dx = col - OBSTRUCTION_CENTER_PIXEL;
            double dy = row - OBSTRUCTION_CENTER_PIXEL;
            double radius = sqrt(dx * dx + dy * dy);
            bool in_valid_area = (radius <= OBSTRUCTION_MAX_RADIUS_PIXELS);
            
            int index = row * OBSTRUCTION_MAP_DIAMETER + col;
            double snr = map->snr_data[index];
            
            // Calculate azimuth and elevation for this pixel (reverse conversion)
            double azimuth = 0.0;
            double elevation = 0.0;
            
            if (in_valid_area && radius > 0) {
                // Convert back to az/el coordinates
                double az_rad = atan2(dy, dx);
                azimuth = (az_rad * RAD_TO_DEG) + 90.0; // Add 90° offset back
                if (azimuth < 0) azimuth += 360.0;
                if (azimuth >= 360.0) azimuth -= 360.0;
                
                double radius_normalized = radius / OBSTRUCTION_MAX_RADIUS_PIXELS;
                elevation = MAX_ELEVATION_DEGREES - (radius_normalized * (MAX_ELEVATION_DEGREES - MIN_ELEVATION_DEGREES));
            }
            
            fprintf(fp, "%d,%d,%.2f,%.2f,%.6f,%d,%d\n",
                   row, col, azimuth, elevation, snr,
                   (snr < OBSTRUCTION_SNR_THRESHOLD && in_valid_area) ? 1 : 0,
                   in_valid_area ? 1 : 0);
        }
    }
    
    fclose(fp);
    return OBSTRUCTION_SUCCESS;
}

// Enhanced interpolation for 123x123 polar map
double obstruction_analyzer_interpolate_snr_enhanced(const obstruction_map_t *map, double azimuth, double elevation) {
    if (!map || !map->snr_data) {
        return 0.0;
    }
    
    // Get pixel coordinates
    pixel_coords_t pixel = convert_az_el_to_pixel(azimuth, elevation);
    
    if (!pixel.valid) {
        return 0.0;
    }
    
    // For now, use nearest neighbor (could enhance with bilinear interpolation)
    int index = pixel.row * OBSTRUCTION_MAP_DIAMETER + pixel.col;
    
    if (index < 0 || index >= OBSTRUCTION_MAP_SIZE) {
        return 0.0;
    }
    
    return map->snr_data[index];
}

// Active satellite detection (based on Gemini's suggestion)
typedef struct {
    char satellite_id[32];
    bool is_active;
    double signal_strength;
    time_t last_seen;
} active_satellite_info_t;

int obstruction_analyzer_detect_active_satellite(
    const char *starlink_status_response,
    active_satellite_info_t *active_sat) {
    
    if (!starlink_status_response || !active_sat) {
        return OBSTRUCTION_ERROR_INVALID_PARAM;
    }
    
    json_object *root = json_tokener_parse(starlink_status_response);
    if (!root) {
        return OBSTRUCTION_ERROR_PARSE_FAILED;
    }
    
    // Look for currently_obstructed and seconds_to_first_non_obstructed_satellite
    json_object *obstruction_stats;
    if (json_object_object_get_ex(root, "obstructionStats", &obstruction_stats)) {
        json_object *currently_obstructed, *seconds_to_clear;
        
        bool is_obstructed = false;
        if (json_object_object_get_ex(obstruction_stats, "currentlyObstructed", &currently_obstructed)) {
            is_obstructed = json_object_get_boolean(currently_obstructed);
        }
        
        int seconds_to_clear_sat = 0;
        if (json_object_object_get_ex(obstruction_stats, "secondsToFirstNonObstructedSatellite", &seconds_to_clear)) {
            seconds_to_clear_sat = json_object_get_int(seconds_to_clear);
        }
        
        // If not currently obstructed, there's likely an active satellite
        active_sat->is_active = !is_obstructed;
        active_sat->last_seen = time(NULL);
        
        if (is_obstructed) {
            snprintf(active_sat->satellite_id, sizeof(active_sat->satellite_id), 
                    "UNKNOWN_OBSTRUCTED_%ds", seconds_to_clear_sat);
            active_sat->signal_strength = 0.0;
        } else {
            strncpy(active_sat->satellite_id, "ACTIVE_SATELLITE", sizeof(active_sat->satellite_id) - 1);
            // Try to get signal strength from status
            json_object *signal_quality;
            if (json_object_object_get_ex(root, "signalQuality", &signal_quality)) {
                json_object *snr;
                if (json_object_object_get_ex(signal_quality, "snr", &snr)) {
                    active_sat->signal_strength = json_object_get_double(snr);
                }
            }
        }
    }
    
    json_object_put(root);
    return OBSTRUCTION_SUCCESS;
}