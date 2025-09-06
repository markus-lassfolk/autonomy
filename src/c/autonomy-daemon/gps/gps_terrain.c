#include "gps_terrain.h"
#include "external_apis.h"
#include "../utils/logx.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

// Terrain analysis configuration
static const int MAX_TERRAIN_CACHE_ENTRIES = 2000;          // Maximum terrain cache entries
static const int TERRAIN_UPDATE_INTERVAL = 3600;             // 1 hour terrain update interval
static const double TERRAIN_CACHE_RADIUS = 5000.0;           // 5km terrain cache radius
static const int MAX_ELEVATION_POINTS = 100;                 // Maximum elevation points per analysis
static const double MIN_ELEVATION_DIFFERENCE = 1.0;          // Minimum elevation difference for analysis

// Terrain types
static const char* TERRAIN_TYPE_NAMES[] = {
    "unknown", "flat", "hilly", "mountainous", "valley", "plateau", "cliff", "canyon",
    "volcanic", "glacial", "desert", "forest", "swamp", "beach", "urban", "rural"
};

// Global terrain analysis state
static gps_terrain_t g_terrain = {0};
static bool g_terrain_initialized = false;
static pthread_mutex_t g_terrain_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations
static int perform_terrain_analysis(double lat, double lon, gps_terrain_info_t *terrain_info);
static int get_real_elevation(double lat, double lon, double* elevation);
static int get_elevation_from_google_api(double lat, double lon, double* elevation);
static int get_elevation_from_local_srtm(double lat, double lon, double* elevation);
static int get_elevation_from_open_elevation_api(double lat, double lon, double* elevation);

// Initialize GPS terrain analysis
int gps_terrain_init(void) {
    if (g_terrain_initialized) {
        LOGX_WARN("GPS terrain analysis already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_terrain_mutex);
    
    // Initialize terrain state
    memset(&g_terrain, 0, sizeof(gps_terrain_t));
    g_terrain.enabled = true;
    g_terrain.max_cache_entries = MAX_TERRAIN_CACHE_ENTRIES;
    g_terrain.update_interval = TERRAIN_UPDATE_INTERVAL;
    g_terrain.cache_radius = TERRAIN_CACHE_RADIUS;
    g_terrain.max_elevation_points = MAX_ELEVATION_POINTS;
    g_terrain.min_elevation_difference = MIN_ELEVATION_DIFFERENCE;
    
    g_terrain.cache_entry_count = 0;
    g_terrain.total_analyses = 0;
    g_terrain.successful_analyses = 0;
    g_terrain.failed_analyses = 0;
    g_terrain.last_update = 0;
    
    // Initialize terrain cache
    for (int i = 0; i < MAX_TERRAIN_CACHE_ENTRIES; i++) {
        g_terrain.terrain_cache[i].active = false;
        g_terrain.terrain_cache[i].lat = 0.0;
        g_terrain.terrain_cache[i].lon = 0.0;
        g_terrain.terrain_cache[i].timestamp = 0;
        g_terrain.terrain_cache[i].elevation = 0.0;
        g_terrain.terrain_cache[i].terrain_type = TERRAIN_TYPE_UNKNOWN;
        g_terrain.terrain_cache[i].slope = 0.0;
        g_terrain.terrain_cache[i].roughness = 0.0;
        g_terrain.terrain_cache[i].drainage = 0.0;
        g_terrain.terrain_cache[i].vegetation_density = 0.0;
        g_terrain.terrain_cache[i].soil_type = 0;
        g_terrain.terrain_cache[i].water_bodies = 0;
    }
    
    // Initialize elevation analysis
    for (int i = 0; i < MAX_ELEVATION_POINTS; i++) {
        g_terrain.elevation_analysis[i].lat = 0.0;
        g_terrain.elevation_analysis[i].lon = 0.0;
        g_terrain.elevation_analysis[i].elevation = 0.0;
        g_terrain.elevation_analysis[i].timestamp = 0;
    }
    
    g_terrain_initialized = true;
    pthread_mutex_unlock(&g_terrain_mutex);
    
    LOGX_INFO("GPS terrain analysis initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Analyze terrain for coordinates
int gps_terrain_analyze(double lat, double lon, gps_terrain_info_t *terrain_info) {
    if (!g_terrain_initialized || !terrain_info) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Check cache first
    if (get_cached_terrain(lat, lon, terrain_info)) {
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_terrain_mutex);
    
    g_terrain.total_analyses++;
    
    // Perform terrain analysis
    int result = perform_terrain_analysis(lat, lon, terrain_info);
    if (result == AUTONOMY_SUCCESS) {
        g_terrain.successful_analyses++;
        // Cache the result
        cache_terrain_data(lat, lon, terrain_info);
    } else {
        g_terrain.failed_analyses++;
    }
    
    g_terrain.last_update = time(NULL);
    
    pthread_mutex_unlock(&g_terrain_mutex);
    
    return result;
}

// Perform terrain analysis
static int perform_terrain_analysis(double lat, double lon, gps_terrain_info_t *terrain_info) {
    // Initialize terrain info
    memset(terrain_info, 0, sizeof(gps_terrain_info_t));
    terrain_info->timestamp = time(NULL);
    terrain_info->lat = lat;
    terrain_info->lon = lon;
    
    // Get real elevation data from external APIs or local data
    if (get_real_elevation(lat, lon, &terrain_info->elevation) != AUTONOMY_SUCCESS) {
        LOGX_ERROR("Failed to get real elevation data for terrain analysis");
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    // Analyze terrain characteristics
    analyze_terrain_characteristics(lat, lon, terrain_info);
    
    // Analyze slope and gradient
    analyze_slope_and_gradient(lat, lon, terrain_info);
    
    // Analyze terrain roughness
    analyze_terrain_roughness(lat, lon, terrain_info);
    
    // Analyze drainage patterns
    analyze_drainage_patterns(lat, lon, terrain_info);
    
    // Analyze vegetation and soil
    analyze_vegetation_and_soil(lat, lon, terrain_info);
    
    // Determine terrain type
    determine_terrain_type(terrain_info);
    
    // Calculate terrain difficulty
    calculate_terrain_difficulty(terrain_info);
    
    LOGX_DEBUG("Terrain analysis completed for (%.6f, %.6f)", lat, lon);
    return AUTONOMY_SUCCESS;
}

// Get real elevation data from external APIs or local elevation database
static int get_real_elevation(double lat, double lon, double* elevation) {
    if (!elevation) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Use the external APIs manager for elevation data
    if (external_apis_is_initialized()) {
        external_elevation_data_t elevation_data;
        if (external_apis_get_elevation(lat, lon, &elevation_data) == AUTONOMY_SUCCESS) {
            *elevation = elevation_data.elevation;
            LOGX_DEBUG("Elevation data obtained from external API",
                      "lat", lat, "lon", lon, "elevation", *elevation,
                      "source", elevation_data.source);
            return AUTONOMY_SUCCESS;
        }
    }
    
    // Try local SRTM data if available
    if (get_elevation_from_local_srtm(lat, lon, elevation) == AUTONOMY_SUCCESS) {
        return AUTONOMY_SUCCESS;
    }
    
    LOGX_ERROR("All elevation data sources failed for coordinates", "lat", lat, "lon", lon);
    return AUTONOMY_ERROR_NOT_FOUND;
}

// Get elevation from local SRTM data
static int get_elevation_from_local_srtm(double lat, double lon, double* elevation) {
    // Check for local SRTM data files
    // SRTM data is typically stored in .hgt files organized by 1-degree tiles
    
    // Calculate tile coordinates
    int lat_tile = (int)floor(lat);
    int lon_tile = (int)floor(lon);
    
    // Format SRTM filename (e.g., N59E018.hgt for Stockholm area)
    char filename[64];
    snprintf(filename, sizeof(filename), "/usr/share/srtm/%c%02d%c%03d.hgt",
             lat >= 0 ? 'N' : 'S', abs(lat_tile),
             lon >= 0 ? 'E' : 'W', abs(lon_tile));
    
    FILE* srtm_file = fopen(filename, "rb");
    if (!srtm_file) {
        LOGX_DEBUG("Local SRTM data file not found", "filename", filename);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    // SRTM data is 1201x1201 pixels for 1-degree tiles (3 arc-second resolution)
    int srtm_size = 1201;
    
    // Calculate pixel coordinates within the tile
    double lat_frac = lat - lat_tile;
    double lon_frac = lon - lon_tile;
    
    int pixel_lat = (int)((1.0 - lat_frac) * (srtm_size - 1)); // Flip latitude (SRTM is top-down)
    int pixel_lon = (int)(lon_frac * (srtm_size - 1));
    
    // Ensure pixels are within bounds
    if (pixel_lat < 0 || pixel_lat >= srtm_size || pixel_lon < 0 || pixel_lon >= srtm_size) {
        fclose(srtm_file);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Seek to the correct position in the file
    long offset = (long)(pixel_lat * srtm_size + pixel_lon) * 2; // 2 bytes per elevation value
    if (fseek(srtm_file, offset, SEEK_SET) != 0) {
        fclose(srtm_file);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Read elevation value (big-endian 16-bit signed integer)
    unsigned char buffer[2];
    if (fread(buffer, 1, 2, srtm_file) != 2) {
        fclose(srtm_file);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Convert big-endian to native format
    int16_t elevation_raw = (buffer[0] << 8) | buffer[1];
    
    // Check for void data (-32768)
    if (elevation_raw == -32768) {
        fclose(srtm_file);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    *elevation = (double)elevation_raw;
    
    fclose(srtm_file);
    
    LOGX_DEBUG("Local SRTM elevation data found",
              "lat", lat, "lon", lon, "elevation", *elevation,
              "filename", filename);
    
    return AUTONOMY_SUCCESS;
}

// Analyze terrain characteristics
static void analyze_terrain_characteristics(double lat, double lon, gps_terrain_info_t *terrain_info) {
    // Analyze elevation patterns in surrounding area
    double surrounding_elevations[8];
    double distances[8] = {100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0};
    
    for (int i = 0; i < 8; i++) {
        double angle = i * M_PI / 4.0; // 8 directions
        double offset_lat = lat + (distances[i] / 111000.0) * cos(angle);
        double offset_lon = lon + (distances[i] / (111000.0 * cos(lat * M_PI / 180.0))) * sin(angle);
        if (get_real_elevation(offset_lat, offset_lon, &surrounding_elevations[i]) != AUTONOMY_SUCCESS) {
            // If we can't get elevation data, skip terrain characteristics analysis
            LOGX_WARN("Failed to get elevation data for terrain characteristics");
            return;
        }
    }
    
    // Calculate elevation statistics
    double min_elevation = surrounding_elevations[0];
    double max_elevation = surrounding_elevations[0];
    double total_elevation = surrounding_elevations[0];
    
    for (int i = 1; i < 8; i++) {
        if (surrounding_elevations[i] < min_elevation) min_elevation = surrounding_elevations[i];
        if (surrounding_elevations[i] > max_elevation) max_elevation = surrounding_elevations[i];
        total_elevation += surrounding_elevations[i];
    }
    
    terrain_info->elevation_range = max_elevation - min_elevation;
    terrain_info->average_elevation = total_elevation / 8.0;
    terrain_info->elevation_variance = calculate_elevation_variance(surrounding_elevations, 8);
}

// Calculate elevation variance
static double calculate_elevation_variance(const double *elevations, int count) {
    double mean = 0.0;
    for (int i = 0; i < count; i++) {
        mean += elevations[i];
    }
    mean /= count;
    
    double variance = 0.0;
    for (int i = 0; i < count; i++) {
        double diff = elevations[i] - mean;
        variance += diff * diff;
    }
    variance /= count;
    
    return variance;
}

// Analyze slope and gradient
static void analyze_slope_and_gradient(double lat, double lon, gps_terrain_info_t *terrain_info) {
    // Calculate slope in multiple directions
    double slopes[8];
    double distances[8] = {100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0};
    
    for (int i = 0; i < 8; i++) {
        double angle = i * M_PI / 4.0;
        double offset_lat = lat + (distances[i] / 111000.0) * cos(angle);
        double offset_lon = lon + (distances[i] / (111000.0 * cos(lat * M_PI / 180.0))) * sin(angle);
        double offset_elevation;
        if (get_real_elevation(offset_lat, offset_lon, &offset_elevation) != AUTONOMY_SUCCESS) {
            continue; // Skip this point if elevation data unavailable
        }
        
        // Calculate slope (rise over run)
        double elevation_diff = offset_elevation - terrain_info->elevation;
        slopes[i] = atan2(elevation_diff, distances[i]) * 180.0 / M_PI; // Convert to degrees
    }
    
    // Find maximum slope
    terrain_info->max_slope = slopes[0];
    for (int i = 1; i < 8; i++) {
        if (fabs(slopes[i]) > fabs(terrain_info->max_slope)) {
            terrain_info->max_slope = slopes[i];
        }
    }
    
    // Calculate average slope
    double total_slope = 0.0;
    for (int i = 0; i < 8; i++) {
        total_slope += fabs(slopes[i]);
    }
    terrain_info->average_slope = total_slope / 8.0;
    
    // Determine slope direction
    terrain_info->slope_direction = calculate_slope_direction(slopes);
}

// Calculate slope direction
static double calculate_slope_direction(const double *slopes) {
    // Find the direction with the steepest slope
    int steepest_index = 0;
    double steepest_slope = fabs(slopes[0]);
    
    for (int i = 1; i < 8; i++) {
        if (fabs(slopes[i]) > steepest_slope) {
            steepest_slope = fabs(slopes[i]);
            steepest_index = i;
        }
    }
    
    return steepest_index * 45.0; // Convert to degrees (0-315)
}

// Analyze terrain roughness
static void analyze_terrain_roughness(double lat, double lon, gps_terrain_info_t *terrain_info) {
    // Sample elevation at fine intervals to calculate roughness
    double sample_elevations[25];
    int sample_index = 0;
    
    for (int y = -2; y <= 2; y++) {
        for (int x = -2; x <= 2; x++) {
            double offset_lat = lat + (y * 50.0 / 111000.0);
            double offset_lon = lon + (x * 50.0 / (111000.0 * cos(lat * M_PI / 180.0)));
            double sample_elevation;
            if (get_real_elevation(offset_lat, offset_lon, &sample_elevation) == AUTONOMY_SUCCESS) {
                sample_elevations[sample_index++] = sample_elevation;
            }
        }
    }
    
    // Calculate roughness as standard deviation of elevation differences
    double center_elevation = sample_elevations[12]; // Center point
    double total_diff_squared = 0.0;
    
    for (int i = 0; i < 25; i++) {
        if (i != 12) { // Skip center point
            double diff = sample_elevations[i] - center_elevation;
            total_diff_squared += diff * diff;
        }
    }
    
    terrain_info->roughness = sqrt(total_diff_squared / 24.0);
}

// Analyze drainage patterns
static void analyze_drainage_patterns(double lat, double lon, gps_terrain_info_t *terrain_info) {
    // Simple drainage analysis based on slope direction and elevation
    // In a real implementation, this would use hydrological models
    
    // Calculate flow accumulation based on slope
    double flow_accumulation = 0.0;
    double total_slope = 0.0;
    
    for (int i = 0; i < 8; i++) {
        double angle = i * M_PI / 4.0;
        double offset_lat = lat + (1000.0 / 111000.0) * cos(angle);
        double offset_lon = lon + (1000.0 / (111000.0 * cos(lat * M_PI / 180.0))) * sin(angle);
        double offset_elevation;
        if (get_real_elevation(offset_lat, offset_lon, &offset_elevation) != AUTONOMY_SUCCESS) {
            continue; // Skip this point if elevation data unavailable
        }
        
        if (offset_elevation < terrain_info->elevation) {
            // Downhill direction - contributes to drainage
            double slope = (terrain_info->elevation - offset_elevation) / 1000.0;
            flow_accumulation += slope;
            total_slope += slope;
        }
    }
    
    terrain_info->drainage_efficiency = flow_accumulation / 8.0;
    terrain_info->water_flow_direction = calculate_slope_direction(&total_slope);
}

// Analyze vegetation and soil
static void analyze_vegetation_and_soil(double lat, double lon, gps_terrain_info_t *terrain_info) {
    // Simulate vegetation and soil analysis based on coordinates
    // In a real implementation, this would use satellite data and soil databases
    
    // Vegetation density based on latitude and elevation
    double lat_factor = cos(lat * M_PI / 180.0); // Higher at equator
    double elevation_factor = 1.0 - (terrain_info->elevation / 5000.0); // Lower at high elevations
    
    terrain_info->vegetation_density = fmax(0.0, fmin(1.0, lat_factor * elevation_factor));
    
    // Soil type based on coordinates (simplified)
    int soil_type = ((int)(lat * 1000000) + (int)(lon * 1000000)) % 8;
    terrain_info->soil_type = soil_type;
    
    // Water bodies detection (simplified)
    double water_probability = 0.1; // Base probability
    if (terrain_info->elevation < 200.0) water_probability += 0.3; // Low elevation
    if (terrain_info->drainage_efficiency > 0.5) water_probability += 0.2; // Good drainage
    
    terrain_info->water_bodies = (water_probability > 0.3) ? 1 : 0;
}

// Determine terrain type
static void determine_terrain_type(gps_terrain_info_t *terrain_info) {
    // Classify terrain based on characteristics
    if (terrain_info->elevation_range < 50.0) {
        terrain_info->terrain_type = TERRAIN_TYPE_FLAT;
    } else if (terrain_info->elevation_range < 200.0) {
        terrain_info->terrain_type = TERRAIN_TYPE_HILLY;
    } else if (terrain_info->elevation_range < 1000.0) {
        terrain_info->terrain_type = TERRAIN_TYPE_MOUNTAINOUS;
    } else {
        terrain_info->terrain_type = TERRAIN_TYPE_MOUNTAINOUS;
    }
    
    // Refine based on slope
    if (terrain_info->max_slope > 30.0) {
        terrain_info->terrain_type = TERRAIN_TYPE_CLIFF;
    } else if (terrain_info->max_slope > 15.0 && terrain_info->elevation_range > 500.0) {
        terrain_info->terrain_type = TERRAIN_TYPE_CANYON;
    }
    
    // Refine based on vegetation
    if (terrain_info->vegetation_density > 0.7) {
        terrain_info->terrain_type = TERRAIN_TYPE_FOREST;
    } else if (terrain_info->vegetation_density < 0.1) {
        terrain_info->terrain_type = TERRAIN_TYPE_DESERT;
    }
}

// Calculate terrain difficulty
static void calculate_terrain_difficulty(gps_terrain_info_t *terrain_info) {
    // Calculate overall terrain difficulty score (0-100)
    double difficulty = 0.0;
    
    // Elevation factor (20%)
    difficulty += (terrain_info->elevation / 5000.0) * 20.0;
    
    // Slope factor (30%)
    difficulty += (fabs(terrain_info->max_slope) / 45.0) * 30.0;
    
    // Roughness factor (25%)
    difficulty += (terrain_info->roughness / 100.0) * 25.0;
    
    // Vegetation factor (15%)
    difficulty += (1.0 - terrain_info->vegetation_density) * 15.0;
    
    // Drainage factor (10%)
    difficulty += (1.0 - terrain_info->drainage_efficiency) * 10.0;
    
    terrain_info->difficulty_score = fmin(100.0, difficulty);
    
    // Classify difficulty level
    if (terrain_info->difficulty_score < 20.0) {
        terrain_info->difficulty_level = TERRAIN_DIFFICULTY_EASY;
    } else if (terrain_info->difficulty_score < 40.0) {
        terrain_info->difficulty_level = TERRAIN_DIFFICULTY_MODERATE;
    } else if (terrain_info->difficulty_score < 60.0) {
        terrain_info->difficulty_level = TERRAIN_DIFFICULTY_CHALLENGING;
    } else if (terrain_info->difficulty_score < 80.0) {
        terrain_info->difficulty_level = TERRAIN_DIFFICULTY_DIFFICULT;
    } else {
        terrain_info->difficulty_level = TERRAIN_DIFFICULTY_EXTREME;
    }
}

// Check cached terrain data
static bool get_cached_terrain(double lat, double lon, gps_terrain_info_t *terrain_info) {
    time_t now = time(NULL);
    
    for (int i = 0; i < g_terrain.cache_entry_count; i++) {
        if (!g_terrain.terrain_cache[i].active) {
            continue;
        }
        
        gps_terrain_cache_entry_t *cache = &g_terrain.terrain_cache[i];
        
        // Check if coordinates are within cache radius
        double distance = calculate_distance(lat, lon, cache->lat, cache->lon);
        if (distance <= g_terrain.cache_radius) {
            // Check if cache is still valid
            if ((now - cache->timestamp) < g_terrain.update_interval) {
                // Return cached data
                terrain_info->timestamp = cache->timestamp;
                terrain_info->elevation = cache->elevation;
                terrain_info->terrain_type = cache->terrain_type;
                terrain_info->slope = cache->slope;
                terrain_info->roughness = cache->roughness;
                terrain_info->drainage = cache->draination;
                terrain_info->vegetation_density = cache->vegetation_density;
                terrain_info->soil_type = cache->soil_type;
                terrain_info->water_bodies = cache->water_bodies;
                
                LOGX_DEBUG("Terrain data retrieved from cache for (%.6f, %.6f)", lat, lon);
                return true;
            }
        }
    }
    
    return false;
}

// Cache terrain data
static void cache_terrain_data(double lat, double lon, const gps_terrain_info_t *terrain_info) {
    // Find free cache slot
    int slot_index = -1;
    for (int i = 0; i < g_terrain.max_cache_entries; i++) {
        if (!g_terrain.terrain_cache[i].active) {
            slot_index = i;
            break;
        }
    }
    
    if (slot_index < 0) {
        // Remove oldest entry to make room
        slot_index = find_oldest_terrain_cache();
        if (slot_index >= 0) {
            g_terrain.terrain_cache[slot_index].active = false;
            g_terrain.cache_entry_count--;
        }
    }
    
    if (slot_index >= 0) {
        gps_terrain_cache_entry_t *cache = &g_terrain.terrain_cache[slot_index];
        
        cache->active = true;
        cache->lat = lat;
        cache->lon = lon;
        cache->timestamp = terrain_info->timestamp;
        cache->elevation = terrain_info->elevation;
        cache->terrain_type = terrain_info->terrain_type;
        cache->slope = terrain_info->slope;
        cache->roughness = terrain_info->roughness;
        cache->draination = terrain_info->drainage;
        cache->vegetation_density = terrain_info->vegetation_density;
        cache->soil_type = terrain_info->soil_type;
        cache->water_bodies = terrain_info->water_bodies;
        
        if (slot_index >= g_terrain.cache_entry_count) {
            g_terrain.cache_entry_count = slot_index + 1;
        }
        
        LOGX_DEBUG("Terrain data cached for (%.6f, %.6f)", lat, lon);
    }
}

// Find oldest terrain cache entry
static int find_oldest_terrain_cache(void) {
    int oldest_index = -1;
    time_t oldest_time = time(NULL);
    
    for (int i = 0; i < g_terrain.max_cache_entries; i++) {
        if (g_terrain.terrain_cache[i].active && 
            g_terrain.terrain_cache[i].timestamp < oldest_time) {
            oldest_time = g_terrain.terrain_cache[i].timestamp;
            oldest_index = i;
        }
    }
    
    return oldest_index;
}

// Calculate distance between coordinates
static double calculate_distance(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371000.0; // Earth radius in meters
    
    double lat1_rad = lat1 * M_PI / 180.0;
    double lat2_rad = lat2 * M_PI / 180.0;
    double delta_lat = (lat2 - lat1) * M_PI / 180.0;
    double delta_lon = (lon2 - lon1) * M_PI / 180.0;
    
    double a = sin(delta_lat / 2.0) * sin(delta_lat / 2.0) +
               cos(lat1_rad) * cos(lat2_rad) *
               sin(delta_lon / 2.0) * sin(delta_lon / 2.0);
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    
    return R * c;
}

// Get terrain analysis status
int gps_terrain_get_status(gps_terrain_status_t *status) {
    if (!g_terrain_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_terrain_mutex);
    
    status->enabled = g_terrain.enabled;
    status->cache_entry_count = g_terrain.cache_entry_count;
    status->max_cache_entries = g_terrain.max_cache_entries;
    status->total_analyses = g_terrain.total_analyses;
    status->successful_analyses = g_terrain.successful_analyses;
    status->failed_analyses = g_terrain.failed_analyses;
    status->last_update = g_terrain.last_update;
    
    // Calculate success rate
    if (g_terrain.total_analyses > 0) {
        status->success_rate = (double)g_terrain.successful_analyses / g_terrain.total_analyses;
    } else {
        status->success_rate = 0.0;
    }
    
    pthread_mutex_unlock(&g_terrain_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get terrain analysis configuration
int gps_terrain_get_config(gps_terrain_config_t *config) {
    if (!g_terrain_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_terrain_mutex);
    
    config->enabled = g_terrain.enabled;
    config->max_cache_entries = g_terrain.max_cache_entries;
    config->update_interval = g_terrain.update_interval;
    config->cache_radius = g_terrain.cache_radius;
    config->max_elevation_points = g_terrain.max_elevation_points;
    config->min_elevation_difference = g_terrain.min_elevation_difference;
    
    pthread_mutex_unlock(&g_terrain_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set terrain analysis configuration
int gps_terrain_set_config(const gps_terrain_config_t *config) {
    if (!g_terrain_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_terrain_mutex);
    
    g_terrain.enabled = config->enabled;
    g_terrain.max_cache_entries = config->max_cache_entries;
    g_terrain.update_interval = config->update_interval;
    g_terrain.cache_radius = config->cache_radius;
    g_terrain.max_elevation_points = config->max_elevation_points;
    g_terrain.min_elevation_difference = config->min_elevation_difference;
    
    pthread_mutex_unlock(&g_terrain_mutex);
    
    LOGX_INFO("GPS terrain analysis configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable terrain analysis
int gps_terrain_set_enabled(bool enabled) {
    if (!g_terrain_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_terrain_mutex);
    g_terrain.enabled = enabled;
    pthread_mutex_unlock(&g_terrain_mutex);
    
    LOGX_INFO("GPS terrain analysis %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Force terrain update
int gps_terrain_force_update(void) {
    if (!g_terrain_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    // Reset last update time to force immediate update
    pthread_mutex_lock(&g_terrain_mutex);
    g_terrain.last_update = 0;
    pthread_mutex_unlock(&g_terrain_mutex);
    
    LOGX_INFO("GPS terrain update forced");
    return AUTONOMY_SUCCESS;
}

// Get terrain statistics
int gps_terrain_get_statistics(gps_terrain_stats_t *stats) {
    if (!g_terrain_initialized || !stats) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_terrain_mutex);
    
    // Calculate statistics from terrain cache
    memset(stats, 0, sizeof(gps_terrain_stats_t));
    
    for (int i = 0; i < g_terrain.cache_entry_count; i++) {
        if (!g_terrain.terrain_cache[i].active) {
            continue;
        }
        
        gps_terrain_cache_entry_t *cache = &g_terrain.terrain_cache[i];
        
        // Count terrain types
        if (cache->terrain_type < TERRAIN_TYPE_MAX) {
            stats->terrain_type_counts[cache->terrain_type]++;
        }
        
        // Calculate averages
        stats->total_elevation += cache->elevation;
        stats->total_slope += fabs(cache->slope);
        stats->total_roughness += cache->roughness;
        stats->total_entries++;
    }
    
    if (stats->total_entries > 0) {
        stats->average_elevation = stats->total_elevation / stats->total_entries;
        stats->average_slope = stats->total_slope / stats->total_entries;
        stats->average_roughness = stats->total_roughness / stats->total_entries;
    }
    
    stats->total_analyses = g_terrain.total_analyses;
    stats->successful_analyses = g_terrain.successful_analyses;
    stats->failed_analyses = g_terrain.failed_analyses;
    
    pthread_mutex_unlock(&g_terrain_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Reset terrain analysis
int gps_terrain_reset(void) {
    if (!g_terrain_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_terrain_mutex);
    
    g_terrain.cache_entry_count = 0;
    g_terrain.total_analyses = 0;
    g_terrain.successful_analyses = 0;
    g_terrain.failed_analyses = 0;
    g_terrain.last_update = 0;
    
    // Clear terrain cache
    for (int i = 0; i < MAX_TERRAIN_CACHE_ENTRIES; i++) {
        g_terrain.terrain_cache[i].active = false;
        g_terrain.terrain_cache[i].lat = 0.0;
        g_terrain.terrain_cache[i].lon = 0.0;
        g_terrain.terrain_cache[i].timestamp = 0;
        g_terrain.terrain_cache[i].elevation = 0.0;
        g_terrain.terrain_cache[i].terrain_type = TERRAIN_TYPE_UNKNOWN;
        g_terrain.terrain_cache[i].slope = 0.0;
        g_terrain.terrain_cache[i].roughness = 0.0;
        g_terrain.terrain_cache[i].draination = 0.0;
        g_terrain.terrain_cache[i].vegetation_density = 0.0;
        g_terrain.terrain_cache[i].soil_type = 0;
        g_terrain.terrain_cache[i].water_bodies = 0;
    }
    
    pthread_mutex_unlock(&g_terrain_mutex);
    
    LOGX_INFO("GPS terrain analysis reset");
    return AUTONOMY_SUCCESS;
}

// Cleanup terrain analysis
void gps_terrain_cleanup(void) {
    if (!g_terrain_initialized) {
        return;
    }
    
    pthread_mutex_destroy(&g_terrain_mutex);
    g_terrain_initialized = false;
    
    LOGX_INFO("GPS terrain analysis cleaned up");
}
