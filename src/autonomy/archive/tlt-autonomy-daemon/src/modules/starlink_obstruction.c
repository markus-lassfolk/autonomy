#include "starlink_obstruction.h"
#include "logx.h"
#include "types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

// Obstruction analysis configuration
static const int MAX_PATTERNS = 100;                        // Maximum environmental patterns
static const int MAX_HISTORY_POINTS = 1440;                 // 24 hours at 1-minute intervals
static const int MIN_OBSERVATIONS_TO_LEARN = 10;            // Minimum observations to learn pattern
static const double PATTERN_SIMILARITY_THRESHOLD = 0.7;     // Pattern similarity threshold
static const double LOCATION_RADIUS_METERS = 1000.0;        // Location radius for pattern matching
static const int MAX_ACTIVE_MATCHES = 5;                    // Maximum concurrent active matches
static const int MATCH_TIMEOUT_MINUTES = 30;                // Timeout for active matches
static const int HISTORY_SIZE = 100;                        // Number of match results to keep

// Forward declarations for static functions
static void add_trend_point(trend_point_array_t *history, time_t timestamp, double value, double quality);
static int find_oldest_trend_point(const trend_point_array_t *history);
static void update_movement_detection(const starlink_obstruction_sample_t *sample);
static void learn_patterns_from_observation(const starlink_obstruction_sample_t *sample);
static void detect_time_patterns(const starlink_obstruction_sample_t *sample);
static void detect_weather_patterns(const starlink_obstruction_sample_t *sample);
static void detect_location_patterns(const starlink_obstruction_sample_t *sample);
static void update_or_create_pattern(const char *name, const char *description, 
                                   const starlink_obstruction_sample_t *sample, double confidence);
static int find_oldest_pattern(void);
static void match_patterns(const starlink_obstruction_sample_t *sample);
static double calculate_pattern_similarity(const starlink_environmental_pattern_t *pattern, 
                                        const starlink_obstruction_sample_t *sample);
static double calculate_time_similarity(const starlink_environmental_pattern_t *pattern, 
                                     const starlink_obstruction_sample_t *sample);
static double calculate_location_similarity(const starlink_environmental_pattern_t *pattern, 
                                         const starlink_obstruction_sample_t *sample);
static void create_or_update_active_match(const starlink_environmental_pattern_t *pattern, 
                                        const starlink_obstruction_sample_t *sample, 
                                        double similarity);
static int find_oldest_active_match(void);
static void cleanup_expired_matches(void);
static void add_match_to_history(const starlink_active_match_t *match, const char *reason);
static int find_oldest_match_history(void);
static void perform_trend_analysis(void);
static void analyze_trend(const trend_point_array_t *history, const char *metric_name);

// Global obstruction analysis state
static starlink_obstruction_t g_obstruction = {0};
static bool g_obstruction_initialized = false;
static pthread_mutex_t g_obstruction_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize Starlink obstruction analysis
int starlink_obstruction_init(void) {
    if (g_obstruction_initialized) {
        LOGX_WARN("Starlink obstruction analysis already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_obstruction_mutex);
    
    // Initialize obstruction state
    memset(&g_obstruction, 0, sizeof(starlink_obstruction_t));
    g_obstruction.enabled = true;
    g_obstruction.max_patterns = MAX_PATTERNS;
    g_obstruction.min_observations_to_learn = MIN_OBSERVATIONS_TO_LEARN;
    g_obstruction.pattern_similarity_threshold = PATTERN_SIMILARITY_THRESHOLD;
    g_obstruction.location_radius_meters = LOCATION_RADIUS_METERS;
    g_obstruction.max_active_matches = MAX_ACTIVE_MATCHES;
    g_obstruction.match_timeout_minutes = MATCH_TIMEOUT_MINUTES;
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
    g_obstruction.trend_analyzer.max_history_points = MAX_HISTORY_POINTS;
    g_obstruction.trend_analyzer.min_points_for_analysis = 10;
    g_obstruction.trend_analyzer.analysis_window = 3600; // 1 hour
    g_obstruction.trend_analyzer.prediction_horizon = 300; // 5 minutes
    g_obstruction.trend_analyzer.anomaly_threshold = 2.0; // 2 standard deviations
    g_obstruction.trend_analyzer.seasonal_min_period = 300; // 5 minutes
    g_obstruction.trend_analyzer.seasonal_max_period = 86400; // 24 hours
    g_obstruction.trend_analyzer.cache_timeout = 300; // 5 minutes
    
    // Initialize movement detector
    g_obstruction.movement_detector.min_movement_distance = 10.0; // 10 meters
    g_obstruction.movement_detector.movement_timeout = 300; // 5 minutes
    g_obstruction.movement_detector.location_history_size = 100;
    g_obstruction.movement_detector.min_accuracy_meters = 20.0; // 20 meters
    g_obstruction.movement_detector.speed_smoothing_window = 5;
    g_obstruction.movement_detector.movement_speed_threshold = 1.0; // 1 m/s
    g_obstruction.movement_detector.stationary_time_required = 120; // 2 minutes
    g_obstruction.movement_detector.significant_distance = 50.0; // 50 meters
    
    g_obstruction_initialized = true;
    pthread_mutex_unlock(&g_obstruction_mutex);
    
    LOGX_INFO("Starlink obstruction analysis initialized successfully");
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
    add_trend_point(&g_obstruction.trend_analyzer.obstruction_history, 
                   sample->timestamp, sample->fraction_obstructed, 1.0);
    add_trend_point(&g_obstruction.trend_analyzer.snr_history, 
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
static void add_trend_point(trend_point_array_t *history, time_t timestamp, double value, double quality) {
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
static int find_oldest_trend_point(const trend_point_array_t *history) {
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

// Update movement detection
static void update_movement_detection(const starlink_obstruction_sample_t *sample) {
    // This would integrate with GPS location data
    // For now, we'll simulate movement detection based on obstruction changes
    
    static double last_obstruction = 0.0;
    static time_t last_movement_check = 0;
    
    time_t now = time(NULL);
    
    // Check for significant obstruction changes that might indicate movement
    if (fabs(sample->fraction_obstructed - last_obstruction) > 0.05) { // 5% change
        if (now - last_movement_check > 60) { // Check every minute
            // Simulate movement detection
            g_obstruction.movement_detector.is_moving = true;
            g_obstruction.movement_detector.last_movement_time = now;
            
            LOGX_DEBUG("Movement detected based on obstruction change: %.2f%% -> %.2f%%", 
                      last_obstruction * 100, sample->fraction_obstructed * 100);
        }
        last_movement_check = now;
    }
    
    last_obstruction = sample->fraction_obstructed;
}

// Learn patterns from observation
static void learn_patterns_from_observation(const starlink_obstruction_sample_t *sample) {
    // Check if we have enough observations to learn patterns
    if (g_obstruction.total_observations < g_obstruction.min_observations_to_learn) {
        return;
    }
    
    // Look for patterns in obstruction data
    // This is a simplified pattern learning algorithm
    // In a real implementation, this would use more sophisticated ML techniques
    
    // Check for time-based patterns (daily, weekly, seasonal)
    detect_time_patterns(sample);
    
    // Check for weather-related patterns
    detect_weather_patterns(sample);
    
    // Check for location-based patterns
    detect_location_patterns(sample);
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

// Detect weather-related patterns
static void detect_weather_patterns(const starlink_obstruction_sample_t *sample) {
    // This would integrate with weather data
    // For now, we'll simulate weather pattern detection
    
    // Simulate weather conditions based on time of day and season
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

// Detect location-based patterns
static void detect_location_patterns(const starlink_obstruction_sample_t *sample) {
    // This would integrate with GPS location data
    // For now, we'll simulate location pattern detection
    
    // Simulate urban vs rural patterns based on obstruction characteristics
    if (sample->fraction_obstructed > 0.1) { // High obstruction
        update_or_create_pattern("urban", "Urban environment obstruction pattern", 
                               sample, 0.7);
    } else if (sample->fraction_obstructed < 0.05) { // Low obstruction
        update_or_create_pattern("rural", "Rural environment obstruction pattern", 
                               sample, 0.7);
    }
}

// Update or create pattern
static void update_or_create_pattern(const char *name, const char *description, 
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
        strncpy(pattern->description, description, sizeof(pattern->description) - 1);
        pattern->description[sizeof(pattern->description) - 1] = '\0';
        
        // Update pattern data
        pattern->obstruction_data.typical_obstruction = sample->fraction_obstructed;
        pattern->obstruction_data.typical_snr = sample->snr;
        pattern->obstruction_data.severity = (sample->fraction_obstructed > 0.15) ? "severe" : 
                                           (sample->fraction_obstructed > 0.08) ? "moderate" : "minor";
        
        pattern->confidence = confidence;
        pattern->sample_count++;
        pattern->last_seen = sample->timestamp;
        
        if (pattern->first_seen == 0) {
            pattern->first_seen = sample->timestamp;
        }
        
        LOGX_DEBUG("Pattern updated/created: %s (confidence: %.2f, samples: %d)", 
                  name, confidence, pattern->sample_count);
    }
}

// Find oldest pattern
static int find_oldest_pattern(void) {
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

// Match patterns against current observation
static void match_patterns(const starlink_obstruction_sample_t *sample) {
    // Find best matching patterns
    for (int i = 0; i < g_obstruction.pattern_count; i++) {
        if (!g_obstruction.patterns[i].active) {
            continue;
        }
        
        starlink_environmental_pattern_t *pattern = &g_obstruction.patterns[i];
        
        // Calculate similarity score
        double similarity = calculate_pattern_similarity(pattern, sample);
        
        if (similarity >= g_obstruction.pattern_similarity_threshold) {
            // Pattern matched - create or update active match
            create_or_update_active_match(pattern, sample, similarity);
        }
    }
    
    // Clean up expired matches
    cleanup_expired_matches();
}

// Calculate pattern similarity
static double calculate_pattern_similarity(const starlink_environmental_pattern_t *pattern, 
                                        const starlink_obstruction_sample_t *sample) {
    double similarity = 0.0;
    
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
static double calculate_time_similarity(const starlink_environmental_pattern_t *pattern, 
                                     const starlink_obstruction_sample_t *sample) {
    // This would implement sophisticated time pattern matching
    // For now, return a base similarity
    return 0.8;
}

// Calculate location similarity
static double calculate_location_similarity(const starlink_environmental_pattern_t *pattern, 
                                         const starlink_obstruction_sample_t *sample) {
    // This would implement location-based similarity
    // For now, return a base similarity
    return 0.8;
}

// Create or update active match
static void create_or_update_active_match(const starlink_environmental_pattern_t *pattern, 
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
        strncpy(match->pattern_name, pattern->name, sizeof(match->pattern_name) - 1);
        match->pattern_name[sizeof(match->pattern_name) - 1] = '\0';
        
        if (match->start_time == 0) {
            match->start_time = sample->timestamp;
        }
        
        match->last_update = sample->timestamp;
        match->similarity = similarity;
        match->confidence = pattern->confidence;
        match->status = MATCH_STATUS_MATCHING;
        match->sample_count++;
        
        LOGX_DEBUG("Active match updated: %s (similarity: %.2f, confidence: %.2f)", 
                  pattern->name, similarity, pattern->confidence);
    }
}

// Find oldest active match
static int find_oldest_active_match(void) {
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
static void cleanup_expired_matches(void) {
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
            
            LOGX_DEBUG("Active match expired: %s", g_obstruction.active_matches[i].pattern_name);
        }
    }
}

// Add match to history
static void add_match_to_history(const starlink_active_match_t *match, const char *reason) {
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
        strncpy(result->pattern_name, match->pattern_name, sizeof(result->pattern_name) - 1);
        result->pattern_name[sizeof(result->pattern_name) - 1] = '\0';
        result->similarity = match->similarity;
        result->confidence = match->confidence;
        result->success = (match->status == MATCH_STATUS_CONFIRMED);
        strncpy(result->reason, reason, sizeof(result->reason) - 1);
        result->reason[sizeof(result->reason) - 1] = '\0';
        result->duration = match->last_update - match->start_time;
        result->sample_count = match->sample_count;
    }
}

// Find oldest match history entry
static int find_oldest_match_history(void) {
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
    analyze_trend(&g_obstruction.trend_analyzer.obstruction_history, "obstruction");
    
    // Analyze SNR trends
    analyze_trend(&g_obstruction.trend_analyzer.snr_history, "snr");
}

// Analyze trend for a specific metric
static void analyze_trend(const trend_point_array_t *history, const char *metric_name) {
    if (history->point_count < g_obstruction.trend_analyzer.min_points_for_analysis) {
        return;
    }
    
    // Calculate basic statistics
    double sum = 0.0;
    double sum_squared = 0.0;
    int valid_points = 0;
    
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
                LOGX_DEBUG("Anomaly detected in %s: value=%.2f, mean=%.2f, deviation=%.2f", 
                          metric_name, history->points[i].value, mean, deviation);
            }
        }
    }
    
    LOGX_DEBUG("Trend analysis for %s: mean=%.2f, std_dev=%.2f, points=%d", 
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
    
    int count = 0;
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
    
    int count = 0;
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
    
    int count = 0;
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
    
    LOGX_INFO("Starlink obstruction analysis configuration updated");
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
    
    LOGX_INFO("Starlink obstruction analysis %s", enabled ? "enabled" : "disabled");
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
    
    LOGX_INFO("Starlink obstruction analysis reset");
    return AUTONOMY_SUCCESS;
}

// Cleanup obstruction analysis
void starlink_obstruction_cleanup(void) {
    if (!g_obstruction_initialized) {
        return;
    }
    
    pthread_mutex_destroy(&g_obstruction_mutex);
    g_obstruction_initialized = false;
    
    LOGX_INFO("Starlink obstruction analysis cleaned up");
}
