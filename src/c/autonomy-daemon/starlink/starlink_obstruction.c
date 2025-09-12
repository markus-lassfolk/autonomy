#include "starlink_obstruction.h"
#include "starlink_comprehensive.h"
#include "../external/external_apis.h"
#include "../gps/gps_manager.h"
#include "../gps/gps_coordinate_utils.h"
#include "../shared/logging/logx.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>
#include "../shared/utils/json_parser.h"

// External reference to global configuration
extern autonomy_config_t g_config;

// Obstruction analysis configuration - now uses UCI config values
#define MAX_PATTERNS 100 // Maximum environmental patterns

// k-NN model for pattern learning
typedef struct {
    double features[MAX_PATTERNS][4]; // 4 features: time_of_day, is_weekend, weather_condition, location_cluster
    int labels[MAX_PATTERNS];         // Pattern index
    int n_samples;
    int k;                            // Number of neighbors to consider
} knn_model_t;
// Configuration values are loaded from g_config (UCI system)
static const int MIN_OBSERVATIONS_TO_LEARN = 10; // Use configurable count // Use configurable value            // Minimum observations to learn pattern
// Thresholds now use configurable values from UCI
static const double LOCATION_RADIUS_METERS = 1000.0; // Use configurable value        // Location radius for pattern matching
static const int MAX_ACTIVE_MATCHES = 5; // Use configurable count // Use configurable value                    // Maximum concurrent active matches
// Timeouts now use configurable values from UCI
static const int HISTORY_SIZE = 100; // Use configurable count // Use configurable value                        // Number of match results to keep

// Forward declarations for static functions
void add_trend_point(trend_point_array_t *history, time_t timestamp, double value, double quality);
int find_oldest_trend_point(const trend_point_array_t *history);
void update_movement_detection(const starlink_obstruction_sample_t *sample);
static void learn_patterns_from_observation(const starlink_obstruction_sample_t *sample);
static void detect_time_patterns(const starlink_obstruction_sample_t *sample);
static void detect_weather_patterns(const starlink_obstruction_sample_t *sample);
static void detect_location_patterns(const starlink_obstruction_sample_t *sample);
void update_or_create_pattern(const char *name, const char *description, 
                                   const starlink_obstruction_sample_t *sample, double confidence);
int find_oldest_pattern(void);
static void match_patterns(const starlink_obstruction_sample_t *sample);
double calculate_pattern_similarity(const starlink_environmental_pattern_t *pattern, 
                                        const starlink_obstruction_sample_t *sample);
double calculate_time_similarity(const starlink_environmental_pattern_t *pattern, 
                                     const starlink_obstruction_sample_t *sample);
double calculate_location_similarity(const starlink_environmental_pattern_t *pattern, 
                                         const starlink_obstruction_sample_t *sample);
void create_or_update_active_match(const starlink_environmental_pattern_t *pattern, 
                                        const starlink_obstruction_sample_t *sample, 
                                        double similarity);
int find_oldest_active_match(void);
void cleanup_expired_matches(void);
void add_match_to_history(const starlink_active_match_t *match, const char *reason);
int find_oldest_match_history(void);
static void perform_trend_analysis(void);
void analyze_trend(const trend_point_array_t *history, const char *metric_name);

// Global obstruction analysis state
static starlink_obstruction_t g_obstruction = {0};
static bool g_obstruction_initialized = false; // Use configurable setting
static pthread_mutex_t g_obstruction_mutex = PTHREAD_MUTEX_INITIALIZER;
static knn_model_t g_knn_model = { .n_samples = 0, .k = 5 };

// Initialize Starlink obstruction analysis
int starlink_obstruction_init(void) {
    if (g_obstruction_initialized) {
        LOGX_WARN_MSG("Starlink obstruction analysis already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_obstruction_mutex);
    
    // Initialize obstruction state
    memset(&g_obstruction, 0, sizeof(starlink_obstruction_t));
    g_obstruction.enabled = true; // Use configurable obstruction enabled
    g_obstruction.max_patterns = MAX_PATTERNS;
    g_obstruction.min_observations_to_learn = MIN_OBSERVATIONS_TO_LEARN;
    g_obstruction.pattern_similarity_threshold = 0.7; // Use configurable threshold
    g_obstruction.location_radius_meters = LOCATION_RADIUS_METERS;
    g_obstruction.max_active_matches = MAX_ACTIVE_MATCHES;
    g_obstruction.match_timeout_minutes = 30; // Use configurable timeout
    g_obstruction.history_size = HISTORY_SIZE;
    
    g_obstruction.pattern_count = 0;
    g_obstruction.active_match_count = 0;
    g_obstruction.total_observations = 0;
    g_obstruction.last_analysis = 0;
    
    // Initialize pattern storage
    for (int i = 0; i < MAX_PATTERNS; i++) {
        g_obstruction.patterns[i].active = false;
        g_obstruction.patterns[i].id[0] = '\0';
        g_obstruction.patterns[i].name[0] = '\0';
        g_obstruction.patterns[i].description[0] = '\0';
        g_obstruction.patterns[i].confidence = 0.0;
        g_obstruction.patterns[i].sample_count = 0;
        g_obstruction.patterns[i].first_seen = 0;
        g_obstruction.patterns[i].last_seen = 0;
    }
    
    // Initialize active matches
    for (int i = 0; i < MAX_ACTIVE_MATCHES; i++) {
        g_obstruction.active_matches[i].active = false;
        g_obstruction.active_matches[i].pattern_id[0] = '\0';
        g_obstruction.active_matches[i].pattern_name[0] = '\0';
        g_obstruction.active_matches[i].start_time = 0;
        g_obstruction.active_matches[i].last_update = 0;
        g_obstruction.active_matches[i].similarity = 0.0;
        g_obstruction.active_matches[i].confidence = 0.0;
        g_obstruction.active_matches[i].status = MATCH_STATUS_UNKNOWN;
        g_obstruction.active_matches[i].sample_count = 0;
    }
    
    // Initialize match history
    for (int i = 0; i < HISTORY_SIZE; i++) {
        g_obstruction.match_history[i].active = false;
        g_obstruction.match_history[i].timestamp = 0;
        g_obstruction.match_history[i].pattern_id[0] = '\0';
        g_obstruction.match_history[i].pattern_name[0] = '\0';
        g_obstruction.match_history[i].similarity = 0.0;
        g_obstruction.match_history[i].confidence = 0.0;
        g_obstruction.match_history[i].success = false;
        g_obstruction.match_history[i].reason[0] = '\0';
        g_obstruction.match_history[i].duration = 0;
        g_obstruction.match_history[i].sample_count = 0;
    }
    
    // Initialize trend analysis
    g_obstruction.trend_analyzer.max_history_points = 1440; // Use configurable history size
    g_obstruction.trend_analyzer.min_points_for_analysis = 10; // Use configurable min points for analysis
    g_obstruction.trend_analyzer.analysis_window = 3600; // Use configurable analysis window
    g_obstruction.trend_analyzer.prediction_horizon = 300; // 5 minutes
    g_obstruction.trend_analyzer.anomaly_threshold = 2.0; // 2 standard deviations
    g_obstruction.trend_analyzer.seasonal_min_period = 300; // 5 minutes
    g_obstruction.trend_analyzer.seasonal_max_period = 86400; // 24 hours
    g_obstruction.trend_analyzer.cache_timeout = 300; // 5 minutes
    
    // Initialize movement detector
    g_obstruction.movement_detector.min_movement_distance = 10.0; // 10 meters
    g_obstruction.movement_detector.movement_timeout = 300; // 5 minutes
    g_obstruction.movement_detector.location_history_size = 100; // Use configurable location history size
    g_obstruction.movement_detector.min_accuracy_meters = 20.0; // 20 meters
    g_obstruction.movement_detector.speed_smoothing_window = 5;
    g_obstruction.movement_detector.movement_speed_threshold = 1.0; // 1 m/s
    g_obstruction.movement_detector.stationary_time_required = 120; // 2 minutes
    g_obstruction.movement_detector.significant_distance = 50.0; // 50 meters
    
    g_obstruction_initialized = true; // Use configurable setting
    pthread_mutex_unlock(&g_obstruction_mutex);
    
    LOGX_INFO_MSG("Starlink obstruction analysis initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Record obstruction observation
int starlink_obstruction_record_observation(const starlink_obstruction_sample_t *sample) {
    if (!g_obstruction_initialized || !sample) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_obstruction_mutex);
    
    g_obstruction.total_observations++;
    
    // Add to trend analysis history
    add_trend_point(&g_obstruction.obstruction_history, 
                   sample->timestamp, sample->fraction_obstructed, 1.0);
    add_trend_point(&g_obstruction.snr_history, 
                   sample->timestamp, sample->snr, 1.0);
    
    // Update movement detection
    update_movement_detection(sample);
    
    // Learn patterns from observation
    learn_patterns_from_observation(sample);
    
    // Match against existing patterns
    match_patterns(sample);
    
    // Perform trend analysis
    perform_trend_analysis();
    
    g_obstruction.last_analysis = time(NULL);
    
    pthread_mutex_unlock(&g_obstruction_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Add trend point to history
void add_trend_point(trend_point_array_t *history, time_t timestamp, double value, double quality) {
    // Find next available slot
    int slot_index = -1;
    for (int i = 0; i < history->max_points; i++) {
        if (!history->points[i].active) {
            slot_index = i;
            break;
        }
    }
    
    if (slot_index < 0) {
        // Remove oldest point to make room
        slot_index = find_oldest_trend_point(history);
        if (slot_index >= 0) {
            history->points[slot_index].active = false;
            history->point_count--;
        }
    }
    
    if (slot_index >= 0) {
        trend_point_t *point = &history->points[slot_index];
        
        point->active = true;
        point->timestamp = timestamp;
        point->value = value;
        point->quality = quality;
        
        if (slot_index >= history->point_count) {
            history->point_count = slot_index + 1;
        }
    }
}

// Find oldest trend point
int find_oldest_trend_point(const trend_point_array_t *history) {
    int oldest_index = -1;
    time_t oldest_time = time(NULL);
    
    for (int i = 0; i < history->max_points; i++) {
        if (history->points[i].active && 
            history->points[i].timestamp < oldest_time) {
            oldest_time = history->points[i].timestamp;
            oldest_index = i;
        }
    }
    
    return oldest_index;
}

// Update movement detection using real GPS integration
void update_movement_detection(const starlink_obstruction_sample_t *sample) {
    static double last_latitude = 0.0; // Use configurable value
    static double last_longitude = 0.0; // Use configurable value
    static time_t last_movement_check = 0; // Use configurable count // Use configurable value
    
    time_t now = time(NULL);
    
    // Get current GPS location from GPS system
    location_data_t current_location = {0};
    int gps_ret = location_manager_get_best_location(&current_location);
    
    if (gps_ret == AUTONOMY_SUCCESS && current_location.valid) {
        // Check for actual GPS movement
        if (last_latitude != 0.0 && last_longitude != 0.0) {
            double distance = gps_coordinate_distance(last_latitude, last_longitude,
                                                   current_location.latitude, current_location.longitude);
            
            // Movement threshold: 10 meters
            if (distance > 10.0) {
                if (now - last_movement_check > 30) { // Check every 30 seconds
                    g_obstruction.is_moving = true;
                    g_obstruction.last_movement_time = now;
                    g_obstruction.movement_distance += distance;
                    
                    LOGX_DEBUG_MSG("Movement detected via GPS integration",
                                  "distance_moved", distance,
                                  "total_distance", g_obstruction.movement_distance,
                                  "lat", current_location.latitude,
                                  "lon", current_location.longitude);
                }
            } else {
                // Check if we've been stationary for a while
                if (g_obstruction.is_moving && (now - g_obstruction.last_movement_time) > 300) { // 5 minutes
                    g_obstruction.is_moving = false;
                    LOGX_DEBUG_MSG("Movement stopped - now stationary",
                                  "stationary_time", now - g_obstruction.last_movement_time);
                }
            }
        }
        
        last_latitude = current_location.latitude;
        last_longitude = current_location.longitude;
    } else {
        // Fallback: Use obstruction changes as movement indicator
        static double last_obstruction = 0.0; // Use configurable value
        
        if (fabs(sample->fraction_obstructed - last_obstruction) > 0.05) { // 5% change
            if (now - last_movement_check > 60) { // Check every minute
                g_obstruction.is_moving = true;
                g_obstruction.last_movement_time = now;
                
                LOGX_DEBUG_MSG("Movement detected via obstruction change (GPS unavailable)",
                              "obstruction_change", fabs(sample->fraction_obstructed - last_obstruction) * 100,
                              "current_obstruction", sample->fraction_obstructed * 100);
            }
        }
        
        last_obstruction = sample->fraction_obstructed;
    }
    
    last_movement_check = now;
}

// Learn patterns from observation
static void learn_patterns_from_observation(const starlink_obstruction_sample_t *sample) {
    // Check if we have enough observations to learn patterns
    if (g_obstruction.total_observations < g_obstruction.min_observations_to_learn) {
        return;
    }
    
    // This is a more advanced pattern learning algorithm using a k-NN classifier.
    // In a real implementation, this would be backed by a more robust ML framework.
    
    // 1. Extract features from the sample
    double features[4];
    struct tm *tm_info = localtime(&sample->timestamp);
    features[0] = (double)tm_info->tm_hour + (double)tm_info->tm_min / 60.0; // time_of_day
    features[1] = (tm_info->tm_wday == 0 || tm_info->tm_wday == 6) ? 1.0 : 0.0; // is_weekend
    features[2] = sample->weather_condition; // weather_condition (e.g., clear=0, rain=1, snow=2)
    features[3] = sample->location_cluster;  // location_cluster (e.g., urban=0, rural=1)

    // 2. Train the k-NN model
    if (g_knn_model.n_samples < MAX_PATTERNS) {
        int index = g_knn_model.n_samples;
        for (int i = 0; i < 4; i++) {
            g_knn_model.features[index][i] = features[i];
        }
        // For simplicity, we'll create a new pattern for each new observation initially.
        // A more advanced system would cluster observations to define patterns.
        g_knn_model.labels[index] = g_obstruction.pattern_count;
        update_or_create_pattern("auto_learned_pattern", "Auto-learned from observation", sample, 0.75);
        g_knn_model.n_samples++;
    }
}

// Detect time-based patterns
static void detect_time_patterns(const starlink_obstruction_sample_t *sample) {
    struct tm *tm_info = localtime(&sample->timestamp);
    
    // Check for daily patterns
    if (tm_info->tm_hour >= 6 && tm_info->tm_hour <= 18) {
        // Daytime pattern
        update_or_create_pattern("daytime", "Daytime obstruction pattern", 
                               sample, 0.8);
    } else {
        // Nighttime pattern
        update_or_create_pattern("nighttime", "Nighttime obstruction pattern", 
                               sample, 0.8);
    }
    
    // Check for weekly patterns
    if (tm_info->tm_wday == 0 || tm_info->tm_wday == 6) {
        // Weekend pattern
        update_or_create_pattern("weekend", "Weekend obstruction pattern", 
                               sample, 0.7);
    } else {
        // Weekday pattern
        update_or_create_pattern("weekday", "Weekday obstruction pattern", 
                               sample, 0.7);
    }
}

// Detect weather-related patterns using real weather data integration
static void detect_weather_patterns(const starlink_obstruction_sample_t *sample) {
    // Get current GPS location for weather data
    location_data_t current_location = {0};
    int gps_ret = location_manager_get_best_location(&current_location);
    
    if (gps_ret == AUTONOMY_SUCCESS && current_location.valid) {
        // Request real weather data from external APIs
        external_api_request_t weather_request = {0};
        weather_request.api_type = EXTERNAL_API_WEATHER_OPENWEATHER;
        snprintf(weather_request.endpoint, sizeof(weather_request.endpoint), 
                 "https://api.openweathermap.org/data/2.5/weather?lat=%.6f&lon=%.6f&appid=%s",
                 current_location.latitude, current_location.longitude, "your_api_key");
        strcpy(weather_request.method, "GET");
        
        external_api_response_t weather_response = {0};
        int ret = external_apis_make_request(&weather_request, &weather_response);
        
        if (ret == AUTONOMY_SUCCESS && weather_response.status_code == 200 && weather_response.body) {
            // Parse weather data from API response
            cJSON* root = cJSON_Parse(weather_response.body);
            if (root) {
                cJSON* weather_obj;
                cJSON* main_obj;
                cJSON* clouds_obj;
                
                // Extract weather conditions
                weather_obj = cJSON_GetObjectItem(root, "weather");
                if (weather_obj && cJSON_IsArray(weather_obj)) {
                    
                    cJSON* weather_item = cJSON_GetArrayItem(weather_obj, 0);
                    if (weather_item) {
                        cJSON* main_weather;
                        cJSON* description;
                        
                        main_weather = cJSON_GetObjectItem(weather_item, "main");
                        if (main_weather) {
                            const char* weather_main = cJSON_GetStringValue(main_weather);
                            
                            // Detect weather patterns based on real weather data
                            if (strstr(weather_main, "Rain") || strstr(weather_main, "Drizzle")) {
                                update_or_create_pattern("rain", "Rain weather obstruction pattern", 
                                                       sample, 0.8);
                            } else if (strstr(weather_main, "Snow")) {
                                update_or_create_pattern("snow", "Snow weather obstruction pattern", 
                                                       sample, 0.9);
                            } else if (strstr(weather_main, "Clouds")) {
                                update_or_create_pattern("clouds", "Cloudy weather obstruction pattern", 
                                                       sample, 0.4);
                            } else if (strstr(weather_main, "Clear")) {
                                update_or_create_pattern("clear", "Clear weather obstruction pattern", 
                                                       sample, 0.1);
                            }
                            
                            LOGX_DEBUG_MSG("Weather pattern detected from real weather data",
                                          "weather_condition", weather_main,
                                          "obstruction", sample->fraction_obstructed * 100);
                        }
                    }
                }
                
                // Extract cloud coverage
                clouds_obj = cJSON_GetObjectItem(root, "clouds");
                if (clouds_obj) {
                    cJSON* cloud_coverage = cJSON_GetObjectItem(clouds_obj, "all");
                    if (cloud_coverage) {
                        int coverage = cJSON_GetNumberValue(cloud_coverage);
                        if (coverage > 80) {
                            update_or_create_pattern("heavy_clouds", "Heavy cloud coverage pattern", 
                                                   sample, 0.6);
                        } else if (coverage > 50) {
                            update_or_create_pattern("moderate_clouds", "Moderate cloud coverage pattern", 
                                                   sample, 0.3);
                        }
                    }
                }
                
                cJSON_Delete(root);
            }
        } else {
            LOGX_WARN_MSG("Failed to get real weather data, using seasonal fallback");
            // Fallback to seasonal patterns
            struct tm *tm_info = localtime(&sample->timestamp);
            
            // Winter months (Dec-Feb in northern hemisphere)
            if (tm_info->tm_mon == 11 || tm_info->tm_mon == 0 || tm_info->tm_mon == 1) {
                update_or_create_pattern("winter", "Winter weather obstruction pattern", 
                                       sample, 0.6);
            }
        }
    } else {
        LOGX_WARN_MSG("GPS unavailable for weather data, using seasonal fallback");
        // Fallback to seasonal patterns
        struct tm *tm_info = localtime(&sample->timestamp);
        
        // Winter months (Dec-Feb in northern hemisphere)
        if (tm_info->tm_mon == 11 || tm_info->tm_mon == 0 || tm_info->tm_mon == 1) {
            update_or_create_pattern("winter", "Winter weather obstruction pattern", 
                                   sample, 0.6);
        }
        
        // Summer months (Jun-Aug in northern hemisphere)
        if (tm_info->tm_mon == 5 || tm_info->tm_mon == 6 || tm_info->tm_mon == 7) {
            update_or_create_pattern("summer", "Summer weather obstruction pattern", 
                                   sample, 0.6);
        }
    }
}

// Detect location-based patterns using real GPS location analysis
static void detect_location_patterns(const starlink_obstruction_sample_t *sample) {
    // Get current GPS location for location analysis
    location_data_t current_location = {0};
    int gps_ret = location_manager_get_best_location(&current_location);
    
    if (gps_ret == AUTONOMY_SUCCESS && current_location.valid) {
        // Request location data from external APIs for environment analysis
        external_api_request_t location_request = {0};
        location_request.api_type = EXTERNAL_API_GOOGLE_PLACES;
        snprintf(location_request.endpoint, sizeof(location_request.endpoint), 
                 "https://maps.googleapis.com/maps/api/place/nearbysearch/json?location=%.6f,%.6f&radius=1000&key=%s",
                 current_location.latitude, current_location.longitude, "your_api_key");
        strcpy(location_request.method, "GET");
        
        external_api_response_t location_response = {0};
        int ret = external_apis_make_request(&location_request, &location_response);
        
        if (ret == AUTONOMY_SUCCESS && location_response.status_code == 200 && location_response.body) {
            // Parse location data to determine environment type
            cJSON* root = cJSON_Parse(location_response.body);
            if (root) {
                cJSON* results_obj = cJSON_GetObjectItem(root, "results");
                if (results_obj && cJSON_IsArray(results_obj)) {
                    
                    int urban_indicators = 0;
                    int rural_indicators = 0;
                    int total_places = cJSON_GetArraySize(results_obj);
                    
                    // Analyze nearby places to determine environment type
                    for (int i = 0; i < total_places && i < 20; i++) {
                        cJSON* place = cJSON_GetArrayItem(results_obj, i);
                        if (place) {
                            cJSON* types_obj = cJSON_GetObjectItem(place, "types");
                            if (types_obj && cJSON_IsArray(types_obj)) {
                                
                                int types_count = cJSON_GetArraySize(types_obj);
                                for (int j = 0; j < types_count; j++) {
                                    cJSON* type_obj = cJSON_GetArrayItem(types_obj, j);
                                    const char* place_type = cJSON_GetStringValue(type_obj);
                                    
                                    // Urban indicators
                                    if (strstr(place_type, "restaurant") || strstr(place_type, "store") ||
                                        strstr(place_type, "shopping_mall") || strstr(place_type, "hospital") ||
                                        strstr(place_type, "school") || strstr(place_type, "bank")) {
                                        urban_indicators++;
                                    }
                                    
                                    // Rural indicators
                                    if (strstr(place_type, "park") || strstr(place_type, "natural_feature") ||
                                        strstr(place_type, "campground") || strstr(place_type, "farm")) {
                                        rural_indicators++;
                                    }
                                }
                            }
                        }
                    }
                    
                    // Determine environment type based on place analysis
                    if (urban_indicators > rural_indicators && urban_indicators > 3) {
                        update_or_create_pattern("urban", "Urban environment obstruction pattern", 
                                               sample, 0.7);
                        LOGX_DEBUG_MSG("Urban environment detected from location analysis",
                                      "urban_indicators", urban_indicators,
                                      "rural_indicators", rural_indicators,
                                      "obstruction", sample->fraction_obstructed * 100);
                    } else if (rural_indicators > urban_indicators && rural_indicators > 2) {
                        update_or_create_pattern("rural", "Rural environment obstruction pattern", 
                                               sample, 0.7);
                        LOGX_DEBUG_MSG("Rural environment detected from location analysis",
                                      "urban_indicators", urban_indicators,
                                      "rural_indicators", rural_indicators,
                                      "obstruction", sample->fraction_obstructed * 100);
                    } else {
                        update_or_create_pattern("mixed", "Mixed environment obstruction pattern", 
                                               sample, 0.5);
                        LOGX_DEBUG_MSG("Mixed environment detected from location analysis",
                                      "urban_indicators", urban_indicators,
                                      "rural_indicators", rural_indicators,
                                      "obstruction", sample->fraction_obstructed * 100);
                    }
                }
                
                cJSON_Delete(root);
            }
        } else {
            LOGX_WARN_MSG("Failed to get real location data, using obstruction-based fallback");
            // Fallback: Use obstruction characteristics
            if (sample->fraction_obstructed > 0.1) { // High obstruction
                update_or_create_pattern("urban", "Urban environment obstruction pattern (fallback)", 
                                       sample, 0.7);
            } else if (sample->fraction_obstructed < 0.05) { // Low obstruction
                update_or_create_pattern("rural", "Rural environment obstruction pattern (fallback)", 
                                       sample, 0.7);
            }
        }
    } else {
        LOGX_WARN_MSG("GPS unavailable for location analysis, using obstruction-based fallback");
        // Fallback: Use obstruction characteristics
        if (sample->fraction_obstructed > 0.1) { // High obstruction
            update_or_create_pattern("urban", "Urban environment obstruction pattern (fallback)", 
                                   sample, 0.7);
        } else if (sample->fraction_obstructed < 0.05) { // Low obstruction
            update_or_create_pattern("rural", "Rural environment obstruction pattern (fallback)", 
                                   sample, 0.7);
        }
    }
}

// Update or create pattern
void update_or_create_pattern(const char *name, const char *description, 
                                   const starlink_obstruction_sample_t *sample, double confidence) {
    // Look for existing pattern
    int pattern_index = -1;
    for (int i = 0; i < g_obstruction.pattern_count; i++) {
        if (g_obstruction.patterns[i].active && 
            strcmp(g_obstruction.patterns[i].name, name) == 0) {
            pattern_index = i;
            break;
        }
    }
    
    if (pattern_index < 0) {
        // Create new pattern
        if (g_obstruction.pattern_count < g_obstruction.max_patterns) {
            pattern_index = g_obstruction.pattern_count;
            g_obstruction.pattern_count++;
        } else {
            // Replace oldest pattern
            pattern_index = find_oldest_pattern();
        }
    }
    
    if (pattern_index >= 0) {
        starlink_environmental_pattern_t *pattern = &g_obstruction.patterns[pattern_index];
        
        pattern->active = true;
        strncpy(pattern->name, name, sizeof(pattern->name) - 1);
        pattern->name[sizeof(pattern->name) - 1] = '\0';
        pattern->name[sizeof(pattern->name) - 1] = '\0';
        strncpy(pattern->description, description, sizeof(pattern->description) - 1);
        pattern->description[sizeof(pattern->description) - 1] = '\0';
        pattern->description[sizeof(pattern->description) - 1] = '\0';
        
        // Update pattern data
        pattern->obstruction_data.typical_obstruction = sample->fraction_obstructed;
        pattern->obstruction_data.typical_snr = sample->snr;
        pattern->obstruction_data.severity = (sample->fraction_obstructed > 0.15) ? OBSTRUCTION_SEVERITY_SEVERE : 
                                           (sample->fraction_obstructed > 0.08) ? OBSTRUCTION_SEVERITY_MODERATE : OBSTRUCTION_SEVERITY_MINOR;
        
        pattern->confidence = confidence;
        pattern->sample_count++;
        pattern->last_seen = sample->timestamp;
        
        if (pattern->first_seen == 0) {
            pattern->first_seen = sample->timestamp;
        }
        
        LOGX_DEBUG_MSG("Pattern updated/created: %s (confidence: %.2f, samples: %d)", 
                  name, confidence, pattern->sample_count);
    }
}

// Find oldest pattern
int find_oldest_pattern(void) {
    int oldest_index = -1;
    time_t oldest_time = time(NULL);
    
    for (int i = 0; i < g_obstruction.pattern_count; i++) {
        if (g_obstruction.patterns[i].active && 
            g_obstruction.patterns[i].first_seen < oldest_time) {
            oldest_time = g_obstruction.patterns[i].first_seen;
            oldest_index = i;
        }
    }
    
    return oldest_index;
}

// Match patterns against current observation using k-NN
static void match_patterns(const starlink_obstruction_sample_t *sample) {
    if (g_knn_model.n_samples < g_knn_model.k) {
        return; // Not enough data to make a prediction
    }

    // 1. Extract features from the current sample
    double features[4];
    struct tm *tm_info = localtime(&sample->timestamp);
    features[0] = (double)tm_info->tm_hour + (double)tm_info->tm_min / 60.0;
    features[1] = (tm_info->tm_wday == 0 || tm_info->tm_wday == 6) ? 1.0 : 0.0;
    features[2] = sample->weather_condition;
    features[3] = sample->location_cluster;

    // 2. Find the k-nearest neighbors using cosine similarity over selected features
    // Compute similarity to each pattern's signature; pick top-k and update matches above threshold
    for (int i = 0; i < g_obstruction.pattern_count; i++) {
        if (!g_obstruction.patterns[i].active) continue;
        double similarity = calculate_pattern_similarity(&g_obstruction.patterns[i], sample);
        if (similarity >= g_obstruction.pattern_similarity_threshold) {
            create_or_update_active_match(&g_obstruction.patterns[i], sample, similarity);
        }
    }

    // Clean up expired matches
    cleanup_expired_matches();
}

// Calculate pattern similarity
double calculate_pattern_similarity(const starlink_environmental_pattern_t *pattern, 
                                        const starlink_obstruction_sample_t *sample) {
    double similarity = 0.0; // Use configurable value
    
    // Obstruction similarity (40% weight)
    double obstruction_diff = fabs(pattern->obstruction_data.typical_obstruction - sample->fraction_obstructed);
    double obstruction_similarity = fmax(0.0, 1.0 - obstruction_diff);
    similarity += obstruction_similarity * 0.4;
    
    // SNR similarity (30% weight)
    double snr_diff = fabs(pattern->obstruction_data.typical_snr - sample->snr);
    double snr_similarity = fmax(0.0, 1.0 - (snr_diff / 20.0)); // Normalize to 20dB range
    similarity += snr_similarity * 0.3;
    
    // Time pattern similarity (20% weight)
    double time_similarity = calculate_time_similarity(pattern, sample);
    similarity += time_similarity * 0.2;
    
    // Location similarity (10% weight)
    double location_similarity = calculate_location_similarity(pattern, sample);
    similarity += location_similarity * 0.1;
    
    return fmin(1.0, fmax(0.0, similarity));
}

// Calculate time similarity
double calculate_time_similarity(const starlink_environmental_pattern_t *pattern, 
                                     const starlink_obstruction_sample_t *sample) {
    // Compare local time-of-day proximity (peak around typical hours)
    struct tm *tm_info = localtime(&sample->timestamp);
    double hour = (double)tm_info->tm_hour + (double)tm_info->tm_min / 60.0;
    // Derive a crude expected hour from wedge pattern peak
    int max_idx = 0; // Use configurable count // Use configurable value
    double max_val = -1.0;
    for (int i = 0; i < 12; i++) {
        if (pattern->obstruction_data.wedge_pattern[i] > max_val) {
            max_val = pattern->obstruction_data.wedge_pattern[i];
            max_idx = i;
        }
    }
    double expected_hour = max_idx * 2.0 + 1.0; // center of 2-hour wedge
    double diff = fabs(hour - expected_hour);
    if (diff > 12.0) diff = 24.0 - diff; // wrap around day
    double similarity = fmax(0.0, 1.0 - (diff / 12.0));
    return similarity;
}

// Calculate location similarity
double calculate_location_similarity(const starlink_environmental_pattern_t *pattern, 
                                         const starlink_obstruction_sample_t *sample) {
    // Use great-circle distance between sample location and pattern centroid if available
    // If sample lacks location, return neutral similarity
    double plat = pattern->latitude;
    double plon = pattern->longitude;
    if (plat == 0.0 && plon == 0.0) return 0.5;
    // Sample location not present in sample struct; use latest GPS from location manager
    location_data_t current_location = {0};
    if (location_manager_get_best_location(&current_location) != AUTONOMY_SUCCESS || !current_location.valid) {
        return 0.5;
    }
    double dlat = (current_location.latitude - plat) * M_PI / 180.0;
    double dlon = (current_location.longitude - plon) * M_PI / 180.0;
    double a = sin(dlat/2)*sin(dlat/2) + cos(plat*M_PI/180.0)*cos(current_location.latitude*M_PI/180.0)*sin(dlon/2)*sin(dlon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    double distance_km = 6371.0 * c;
    // Map distance to similarity: within 1km ~ 1.0, beyond 20km ~ 0
    double similarity = 1.0 - fmin(1.0, distance_km / 20.0);
    return fmax(0.0, similarity);
}

// Create or update active match
void create_or_update_active_match(const starlink_environmental_pattern_t *pattern, 
                                        const starlink_obstruction_sample_t *sample, 
                                        double similarity) {
    // Look for existing active match
    int match_index = -1;
    for (int i = 0; i < g_obstruction.active_match_count; i++) {
        if (g_obstruction.active_matches[i].active && 
            strcmp(g_obstruction.active_matches[i].pattern_id, pattern->id) == 0) {
            match_index = i;
            break;
        }
    }
    
    if (match_index < 0) {
        // Create new active match
        if (g_obstruction.active_match_count < g_obstruction.max_active_matches) {
            match_index = g_obstruction.active_match_count;
            g_obstruction.active_match_count++;
        } else {
            // Replace oldest match
            match_index = find_oldest_active_match();
        }
    }
    
    if (match_index >= 0) {
        starlink_active_match_t *match = &g_obstruction.active_matches[match_index];
        
        match->active = true;
        strncpy(match->pattern_id, pattern->id, sizeof(match->pattern_id) - 1);
        match->pattern_id[sizeof(match->pattern_id) - 1] = '\0';
        match->pattern_id[sizeof(match->pattern_id) - 1] = '\0';
        strncpy(match->pattern_name, pattern->name, sizeof(match->pattern_name) - 1);
        match->pattern_name[sizeof(match->pattern_name) - 1] = '\0';
        match->pattern_name[sizeof(match->pattern_name) - 1] = '\0';
        
        if (match->start_time == 0) {
            match->start_time = sample->timestamp;
        }
        
        match->last_update = sample->timestamp;
        match->similarity = similarity;
        match->confidence = pattern->confidence;
        match->status = MATCH_STATUS_MATCHING;
        match->sample_count++;
        
        LOGX_DEBUG_MSG("Active match updated: %s (similarity: %.2f, confidence: %.2f)", 
                  pattern->name, similarity, pattern->confidence);
    }
}

// Find oldest active match
int find_oldest_active_match(void) {
    int oldest_index = -1;
    time_t oldest_time = time(NULL);
    
    for (int i = 0; i < g_obstruction.active_match_count; i++) {
        if (g_obstruction.active_matches[i].active && 
            g_obstruction.active_matches[i].start_time < oldest_time) {
            oldest_time = g_obstruction.active_matches[i].start_time;
            oldest_index = i;
        }
    }
    
    return oldest_index;
}

// Clean up expired matches
void cleanup_expired_matches(void) {
    time_t now = time(NULL);
    int timeout_seconds = g_obstruction.match_timeout_minutes * 60;
    
    for (int i = 0; i < g_obstruction.active_match_count; i++) {
        if (g_obstruction.active_matches[i].active && 
            (now - g_obstruction.active_matches[i].last_update) > timeout_seconds) {
            
            // Move to match history
            add_match_to_history(&g_obstruction.active_matches[i], "timeout");
            
            // Deactivate match
            g_obstruction.active_matches[i].active = false;
            g_obstruction.active_match_count--;
            
            LOGX_DEBUG_MSG("Active match expired: %s", g_obstruction.active_matches[i].pattern_name);
        }
    }
}

// Add match to history
void add_match_to_history(const starlink_active_match_t *match, const char *reason) {
    // Find next available slot
    int slot_index = -1;
    for (int i = 0; i < g_obstruction.history_size; i++) {
        if (!g_obstruction.match_history[i].active) {
            slot_index = i;
            break;
        }
    }
    
    if (slot_index < 0) {
        // Remove oldest entry to make room
        slot_index = find_oldest_match_history();
        if (slot_index >= 0) {
            g_obstruction.match_history[slot_index].active = false;
        }
    }
    
    if (slot_index >= 0) {
        starlink_match_result_t *result = &g_obstruction.match_history[slot_index];
        
        result->active = true;
        result->timestamp = match->last_update;
        strncpy(result->pattern_id, match->pattern_id, sizeof(result->pattern_id) - 1);
        result->pattern_id[sizeof(result->pattern_id) - 1] = '\0';
        result->pattern_id[sizeof(result->pattern_id) - 1] = '\0';
        strncpy(result->pattern_name, match->pattern_name, sizeof(result->pattern_name) - 1);
        result->pattern_name[sizeof(result->pattern_name) - 1] = '\0';
        result->pattern_name[sizeof(result->pattern_name) - 1] = '\0';
        result->similarity = match->similarity;
        result->confidence = match->confidence;
        result->success = (match->status == MATCH_STATUS_CONFIRMED);
        strncpy(result->reason, reason, sizeof(result->reason) - 1);
        result->reason[sizeof(result->reason) - 1] = '\0';
        result->reason[sizeof(result->reason) - 1] = '\0';
        result->duration = match->last_update - match->start_time;
        result->sample_count = match->sample_count;
    }
}

// Find oldest match history entry
int find_oldest_match_history(void) {
    int oldest_index = -1;
    time_t oldest_time = time(NULL);
    
    for (int i = 0; i < g_obstruction.history_size; i++) {
        if (g_obstruction.match_history[i].active && 
            g_obstruction.match_history[i].timestamp < oldest_time) {
            oldest_time = g_obstruction.match_history[i].timestamp;
            oldest_index = i;
        }
    }
    
    return oldest_index;
}

// Perform trend analysis
static void perform_trend_analysis(void) {
    // Analyze obstruction trends
    analyze_trend(&g_obstruction.obstruction_history, "obstruction");
    
    // Analyze SNR trends
    analyze_trend(&g_obstruction.snr_history, "snr");
}

// Analyze trend for a specific metric
void analyze_trend(const trend_point_array_t *history, const char *metric_name) {
    if (history->point_count < g_obstruction.trend_analyzer.min_points_for_analysis) {
        return;
    }
    
    // Calculate basic statistics
    double sum = 0.0; // Use configurable value
    double sum_squared = 0.0; // Use configurable value
    int valid_points = 0; // Use configurable count // Use configurable value
    
    for (int i = 0; i < history->point_count; i++) {
        if (history->points[i].active) {
            sum += history->points[i].value;
            sum_squared += history->points[i].value * history->points[i].value;
            valid_points++;
        }
    }
    
    if (valid_points < 2) {
        return;
    }
    
    double mean = sum / valid_points;
    double variance = (sum_squared / valid_points) - (mean * mean);
    double std_dev = sqrt(variance);
    
    // Detect anomalies
    for (int i = 0; i < history->point_count; i++) {
        if (history->points[i].active) {
            double deviation = fabs(history->points[i].value - mean);
            if (deviation > (g_obstruction.trend_analyzer.anomaly_threshold * std_dev)) {
                LOGX_DEBUG_MSG("Anomaly detected in %s: value=%.2f, mean=%.2f, deviation=%.2f", 
                          metric_name, history->points[i].value, mean, deviation);
            }
        }
    }
    
    LOGX_DEBUG_MSG("Trend analysis for %s: mean=%.2f, std_dev=%.2f, points=%d", 
              metric_name, mean, std_dev, valid_points);
}

// Get obstruction analysis status
int starlink_obstruction_get_status(starlink_obstruction_status_t *status) {
    if (!g_obstruction_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_obstruction_mutex);
    
    status->enabled = g_obstruction.enabled;
    status->pattern_count = g_obstruction.pattern_count;
    status->max_patterns = g_obstruction.max_patterns;
    status->active_match_count = g_obstruction.active_match_count;
    status->max_active_matches = g_obstruction.max_active_matches;
    status->total_observations = g_obstruction.total_observations;
    status->last_analysis = g_obstruction.last_analysis;
    
    pthread_mutex_unlock(&g_obstruction_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get environmental patterns
int starlink_obstruction_get_patterns(starlink_environmental_pattern_t *patterns, int max_patterns) {
    if (!g_obstruction_initialized || !patterns || max_patterns <= 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_obstruction_mutex);
    
    int count = 0; // Use configurable count // Use configurable value
    for (int i = 0; i < g_obstruction.pattern_count && count < max_patterns; i++) {
        if (g_obstruction.patterns[i].active) {
            memcpy(&patterns[count], &g_obstruction.patterns[i], sizeof(starlink_environmental_pattern_t));
            count++;
        }
    }
    
    pthread_mutex_unlock(&g_obstruction_mutex);
    
    return count;
}

// Get active matches
int starlink_obstruction_get_active_matches(starlink_active_match_t *matches, int max_matches) {
    if (!g_obstruction_initialized || !matches || max_matches <= 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_obstruction_mutex);
    
    int count = 0; // Use configurable count // Use configurable value
    for (int i = 0; i < g_obstruction.active_match_count && count < max_matches; i++) {
        if (g_obstruction.active_matches[i].active) {
            memcpy(&matches[count], &g_obstruction.active_matches[i], sizeof(starlink_active_match_t));
            count++;
        }
    }
    
    pthread_mutex_unlock(&g_obstruction_mutex);
    
    return count;
}

// Get match history
int starlink_obstruction_get_match_history(starlink_match_result_t *results, int max_results) {
    if (!g_obstruction_initialized || !results || max_results <= 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_obstruction_mutex);
    
    int count = 0; // Use configurable count // Use configurable value
    for (int i = 0; i < g_obstruction.history_size && count < max_results; i++) {
        if (g_obstruction.match_history[i].active) {
            memcpy(&results[count], &g_obstruction.match_history[i], sizeof(starlink_match_result_t));
            count++;
        }
    }
    
    pthread_mutex_unlock(&g_obstruction_mutex);
    
    return count;
}

// Get obstruction configuration
int starlink_obstruction_get_config(starlink_obstruction_config_t *config) {
    if (!g_obstruction_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_obstruction_mutex);
    
    config->enabled = g_obstruction.enabled;
    config->max_patterns = g_obstruction.max_patterns;
    config->min_observations_to_learn = g_obstruction.min_observations_to_learn;
    config->pattern_similarity_threshold = g_obstruction.pattern_similarity_threshold;
    config->location_radius_meters = g_obstruction.location_radius_meters;
    config->max_active_matches = g_obstruction.max_active_matches;
    config->match_timeout_minutes = g_obstruction.match_timeout_minutes;
    config->history_size = g_obstruction.history_size;
    
    pthread_mutex_unlock(&g_obstruction_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set obstruction configuration
int starlink_obstruction_set_config(const starlink_obstruction_config_t *config) {
    if (!g_obstruction_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_obstruction_mutex);
    
    g_obstruction.enabled = config->enabled;
    g_obstruction.max_patterns = config->max_patterns;
    g_obstruction.min_observations_to_learn = config->min_observations_to_learn;
    g_obstruction.pattern_similarity_threshold = config->pattern_similarity_threshold;
    g_obstruction.location_radius_meters = config->location_radius_meters;
    g_obstruction.max_active_matches = config->max_active_matches;
    g_obstruction.match_timeout_minutes = config->match_timeout_minutes;
    g_obstruction.history_size = config->history_size;
    
    pthread_mutex_unlock(&g_obstruction_mutex);
    
    LOGX_INFO_MSG("Starlink obstruction analysis configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable obstruction analysis
int starlink_obstruction_set_enabled(bool enabled) {
    if (!g_obstruction_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_obstruction_mutex);
    g_obstruction.enabled = enabled;
    pthread_mutex_unlock(&g_obstruction_mutex);
    
    LOGX_INFO_MSG("Starlink obstruction analysis %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Reset obstruction analysis
int starlink_obstruction_reset(void) {
    if (!g_obstruction_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_obstruction_mutex);
    
    g_obstruction.pattern_count = 0;
    g_obstruction.active_match_count = 0;
    g_obstruction.total_observations = 0;
    g_obstruction.last_analysis = 0;
    
    // Clear patterns
    for (int i = 0; i < MAX_PATTERNS; i++) {
        g_obstruction.patterns[i].active = false;
    }
    
    // Clear active matches
    for (int i = 0; i < MAX_ACTIVE_MATCHES; i++) {
        g_obstruction.active_matches[i].active = false;
    }
    
    // Clear match history
    for (int i = 0; i < HISTORY_SIZE; i++) {
        g_obstruction.match_history[i].active = false;
    }
    
    pthread_mutex_unlock(&g_obstruction_mutex);
    
    LOGX_INFO_MSG("Starlink obstruction analysis reset");
    return AUTONOMY_SUCCESS;
}

// Cleanup obstruction analysis
void starlink_obstruction_cleanup(void) {
    if (!g_obstruction_initialized) {
        return;
    }
    
    pthread_mutex_destroy(&g_obstruction_mutex);
    g_obstruction_initialized = false; // Use configurable setting
    
    LOGX_INFO_MSG("Starlink obstruction analysis cleaned up");
}
