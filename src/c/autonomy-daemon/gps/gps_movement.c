#include "gps_movement.h"
#include "../utils/logx.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

// Movement detection configuration
static const int MOVEMENT_HISTORY_SIZE = 50;        // Number of positions to track
static const double STATIONARY_THRESHOLD = 5.0;     // 5 meters - stationary threshold
static const double MOVEMENT_THRESHOLD = 10.0;      // 10 meters - movement threshold
static const double SPEED_THRESHOLD = 0.5;          // 0.5 m/s - speed threshold
static const int MIN_POSITIONS_FOR_ANALYSIS = 3;    // Minimum positions needed
static const int ANALYSIS_INTERVAL = 10;             // 10 seconds between analyses
static const double MAX_REALISTIC_SPEED = 100.0;    // 100 m/s - maximum realistic speed

// Movement patterns
static const char* MOVEMENT_PATTERN_NAMES[] = {
    "unknown", "stationary", "moving", "accelerating", "decelerating", "turning", "oscillating"
};

// Global movement detector state
static gps_movement_t g_movement_detector = {0};
static bool g_movement_initialized = false;
static pthread_mutex_t g_movement_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize GPS movement detector
static int gps_movement_init(void) {
    if (g_movement_initialized) {
        LOGX_WARN("GPS movement detector already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_movement_mutex);
    
    // Initialize movement detector state
    memset(&g_movement_detector, 0, sizeof(gps_movement_t));
    g_movement_detector.enabled = true;
    g_movement_detector.stationary_threshold = STATIONARY_THRESHOLD;
    g_movement_detector.movement_threshold = MOVEMENT_THRESHOLD;
    g_movement_detector.speed_threshold = SPEED_THRESHOLD;
    g_movement_detector.max_realistic_speed = MAX_REALISTIC_SPEED;
    g_movement_detector.history_size = MOVEMENT_HISTORY_SIZE;
    g_movement_detector.min_positions = MIN_POSITIONS_FOR_ANALYSIS;
    g_movement_detector.analysis_interval = ANALYSIS_INTERVAL;
    
    g_movement_detector.position_count = 0;
    g_movement_detector.last_analysis = 0;
    g_movement_detector.total_analyses = 0;
    g_movement_detector.movement_detected = false;
    g_movement_detector.current_pattern = MOVEMENT_PATTERN_UNKNOWN;
    
    // Initialize position history
    for (int i = 0; i < MOVEMENT_HISTORY_SIZE; i++) {
        g_movement_detector.position_history[i].timestamp = 0;
        g_movement_detector.position_history[i].lat = 0.0;
        g_movement_detector.position_history[i].lon = 0.0;
        g_movement_detector.position_history[i].altitude = 0.0;
        g_movement_detector.position_history[i].accuracy = 0.0;
    }
    
    g_movement_initialized = true;
    pthread_mutex_unlock(&g_movement_mutex);
    
    LOGX_INFO("GPS movement detector initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Add GPS position for movement analysis
static int gps_movement_add_position(const gps_data_t *gps_data) {
    if (!g_movement_initialized || !gps_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_movement_mutex);
    
    // Add position to history (circular buffer)
    int index = g_movement_detector.position_count % g_movement_detector.history_size;
    
    g_movement_detector.position_history[index].timestamp = gps_data->timestamp;
    g_movement_detector.position_history[index].lat = gps_data->lat;
    g_movement_detector.position_history[index].lon = gps_data->lon;
    g_movement_detector.position_history[index].altitude = gps_data->altitude;
    g_movement_detector.position_history[index].accuracy = gps_data->accuracy;
    
    g_movement_detector.position_count++;
    
    // Perform movement analysis if enough data and time has passed
    time_t now = time(NULL);
    if (g_movement_detector.position_count >= g_movement_detector.min_positions &&
        (now - g_movement_detector.last_analysis) >= g_movement_detector.analysis_interval) {
        
        analyze_movement_pattern();
        g_movement_detector.last_analysis = now;
        g_movement_detector.total_analyses++;
    }
    
    pthread_mutex_unlock(&g_movement_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Analyze movement pattern
static void analyze_movement_pattern(void) {
    if (g_movement_detector.position_count < g_movement_detector.min_positions) {
        return;
    }
    
    // Calculate movement metrics
    movement_metrics_t metrics = calculate_movement_metrics();
    
    // Determine movement pattern
    gps_movement_pattern_t pattern = determine_movement_pattern(&metrics);
    
    // Update movement state
    g_movement_detector.current_pattern = pattern;
    g_movement_detector.movement_detected = (pattern != MOVEMENT_PATTERN_STATIONARY);
    
    // Update current metrics
    memcpy(&g_movement_detector.current_metrics, &metrics, sizeof(movement_metrics_t));
    
    LOGX_DEBUG("Movement analysis: pattern=%s, speed=%.2f m/s, distance=%.1fm", 
               MOVEMENT_PATTERN_NAMES[pattern], metrics.current_speed, metrics.total_distance);
}

// Calculate movement metrics
static movement_metrics_t calculate_movement_metrics(void) {
    movement_metrics_t metrics = {0};
    
    if (g_movement_detector.position_count < 2) {
        return metrics;
    }
    
    // Calculate total distance and speed
    double total_distance = 0.0;
    time_t total_time = 0;
    double max_speed = 0.0;
    double min_speed = 0.0;
    bool first_speed = true;
    
    // Get valid positions (non-zero coordinates)
    int valid_positions = 0;
    for (int i = 0; i < g_movement_detector.history_size && i < g_movement_detector.position_count; i++) {
        if (g_movement_detector.position_history[i].timestamp > 0 &&
            g_movement_detector.position_history[i].lat != 0.0 &&
            g_movement_detector.position_history[i].lon != 0.0) {
            valid_positions++;
        }
    }
    
    if (valid_positions < 2) {
        return metrics;
    }
    
    // Calculate distances and speeds between consecutive positions
    for (int i = 1; i < valid_positions && i < g_movement_detector.history_size; i++) {
        const position_data_t *prev = &g_movement_detector.position_history[i-1];
        const position_data_t *curr = &g_movement_detector.position_history[i];
        
        if (prev->timestamp > 0 && curr->timestamp > 0 &&
            prev->lat != 0.0 && prev->lon != 0.0 &&
            curr->lat != 0.0 && curr->lon != 0.0) {
            
            // Calculate distance
            double distance = calculate_distance(prev->lat, prev->lon, curr->lat, curr->lon);
            total_distance += distance;
            
            // Calculate time difference
            int time_diff = curr->timestamp - prev->timestamp;
            if (time_diff > 0) {
                total_time += time_diff;
                
                // Calculate speed
                double speed = distance / time_diff;
                
                // Update speed statistics
                if (first_speed) {
                    max_speed = speed;
                    min_speed = speed;
                    first_speed = false;
                } else {
                    if (speed > max_speed) max_speed = speed;
                    if (speed < min_speed) min_speed = speed;
                }
            }
        }
    }
    
    // Calculate average speed
    if (total_time > 0) {
        metrics.average_speed = total_distance / total_time;
    }
    
    // Set current speed (most recent)
    if (valid_positions >= 2) {
        const position_data_t *prev = &g_movement_detector.position_history[valid_positions-2];
        const position_data_t *curr = &g_movement_detector.position_history[valid_positions-1];
        
        if (prev->timestamp > 0 && curr->timestamp > 0) {
            double distance = calculate_distance(prev->lat, prev->lon, curr->lat, curr->lon);
            int time_diff = curr->timestamp - prev->timestamp;
            
            if (time_diff > 0) {
                metrics.current_speed = distance / time_diff;
            }
        }
    }
    
    // Set other metrics
    metrics.total_distance = total_distance;
    metrics.total_time = total_time;
    metrics.max_speed = max_speed;
    metrics.min_speed = min_speed;
    metrics.position_count = valid_positions;
    
    return metrics;
}

// Determine movement pattern based on metrics
static gps_movement_pattern_t determine_movement_pattern(const movement_metrics_t *metrics) {
    if (!metrics || metrics->position_count < 2) {
        return MOVEMENT_PATTERN_UNKNOWN;
    }
    
    // Check if stationary
    if (metrics->total_distance < g_movement_detector.stationary_threshold) {
        return MOVEMENT_PATTERN_STATIONARY;
    }
    
    // Check if moving
    if (metrics->current_speed < g_movement_detector.speed_threshold) {
        return MOVEMENT_PATTERN_STATIONARY;
    }
    
    // Check for unrealistic speeds
    if (metrics->current_speed > g_movement_detector.max_realistic_speed) {
        return MOVEMENT_PATTERN_UNKNOWN; // Likely GPS error
    }
    
    // Check for acceleration/deceleration
    if (metrics->position_count >= 3) {
        double speed_change = metrics->current_speed - metrics->average_speed;
        double change_threshold = metrics->average_speed * 0.2; // 20% change threshold
        
        if (fabs(speed_change) > change_threshold) {
            if (speed_change > 0) {
                return MOVEMENT_PATTERN_ACCELERATING;
            } else {
                return MOVEMENT_PATTERN_DECELERATING;
            }
        }
    }
    
    // Check for turning (significant direction changes)
    if (detect_turning_pattern()) {
        return MOVEMENT_PATTERN_TURNING;
    }
    
    // Check for oscillation (back and forth movement)
    if (detect_oscillation_pattern()) {
        return MOVEMENT_PATTERN_OSCILLATING;
    }
    
    // Default to moving
    return MOVEMENT_PATTERN_MOVING;
}

// Detect turning pattern
static bool detect_turning_pattern(void) {
    if (g_movement_detector.position_count < 3) {
        return false;
    }
    
    // Calculate bearing changes between consecutive positions
    double total_bearing_change = 0.0;
    int bearing_changes = 0;
    
    for (int i = 2; i < g_movement_detector.history_size && i < g_movement_detector.position_count; i++) {
        const position_data_t *prev = &g_movement_detector.position_history[i-2];
        const position_data_t *curr = &g_movement_detector.position_history[i-1];
        const position_data_t *next = &g_movement_detector.position_history[i];
        
        if (prev->timestamp > 0 && curr->timestamp > 0 && next->timestamp > 0 &&
            prev->lat != 0.0 && prev->lon != 0.0 &&
            curr->lat != 0.0 && curr->lon != 0.0 &&
            next->lat != 0.0 && next->lon != 0.0) {
            
            double bearing1 = calculate_bearing(prev->lat, prev->lon, curr->lat, curr->lon);
            double bearing2 = calculate_bearing(curr->lat, curr->lon, next->lat, next->lon);
            
            double bearing_change = fabs(bearing2 - bearing1);
            if (bearing_change > 180.0) {
                bearing_change = 360.0 - bearing_change;
            }
            
            if (bearing_change > 30.0) { // Significant turn threshold
                total_bearing_change += bearing_change;
                bearing_changes++;
            }
        }
    }
    
    // Consider it turning if average bearing change is significant
    if (bearing_changes > 0) {
        double avg_bearing_change = total_bearing_change / bearing_changes;
        return avg_bearing_change > 45.0; // 45 degree average change threshold
    }
    
    return false;
}

// Detect oscillation pattern
static bool detect_oscillation_pattern(void) {
    if (g_movement_detector.position_count < 5) {
        return false;
    }
    
    // Check for back-and-forth movement by analyzing position changes
    int direction_changes = 0;
    double prev_distance = 0.0;
    bool first_distance = true;
    
    for (int i = 1; i < g_movement_detector.history_size && i < g_movement_detector.position_count; i++) {
        const position_data_t *prev = &g_movement_detector.position_history[i-1];
        const position_data_t *curr = &g_movement_detector.position_history[i];
        
        if (prev->timestamp > 0 && curr->timestamp > 0 &&
            prev->lat != 0.0 && prev->lon != 0.0 &&
            curr->lat != 0.0 && curr->lon != 0.0) {
            
            double distance = calculate_distance(prev->lat, prev->lon, curr->lat, curr->lon);
            
            if (!first_distance) {
                // Check if distance is decreasing after increasing (or vice versa)
                if ((prev_distance > 0 && distance < prev_distance) ||
                    (prev_distance < 0 && distance > prev_distance)) {
                    direction_changes++;
                }
            }
            
            prev_distance = distance;
            first_distance = false;
        }
    }
    
    // Consider it oscillating if there are multiple direction changes
    return direction_changes >= 2;
}

// Calculate distance between two GPS coordinates (Haversine formula)
static double calculate_distance(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371000.0;  // Earth's radius in meters
    
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

// Calculate bearing between two GPS coordinates
static double calculate_bearing(double lat1, double lon1, double lat2, double lon2) {
    double lat1_rad = lat1 * M_PI / 180.0;
    double lat2_rad = lat2 * M_PI / 180.0;
    double delta_lon = (lon2 - lon1) * M_PI / 180.0;
    
    double y = sin(delta_lon) * cos(lat2_rad);
    double x = cos(lat1_rad) * sin(lat2_rad) - sin(lat1_rad) * cos(lat2_rad) * cos(delta_lon);
    
    double bearing = atan2(y, x) * 180.0 / M_PI;
    
    // Normalize to 0-360 degrees
    if (bearing < 0) {
        bearing += 360.0;
    }
    
    return bearing;
}

// Get movement detection status
static int gps_movement_get_status(gps_movement_status_t *status) {
    if (!g_movement_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_movement_mutex);
    
    status->enabled = g_movement_detector.enabled;
    status->movement_detected = g_movement_detector.movement_detected;
    status->current_pattern = g_movement_detector.current_pattern;
    status->position_count = g_movement_detector.position_count;
    status->last_analysis = g_movement_detector.last_analysis;
    status->total_analyses = g_movement_detector.total_analyses;
    
    // Copy current metrics
    memcpy(&status->current_metrics, &g_movement_detector.current_metrics, sizeof(movement_metrics_t));
    
    pthread_mutex_unlock(&g_movement_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get movement detector configuration
static int gps_movement_get_config(gps_movement_config_t *config) {
    if (!g_movement_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_movement_mutex);
    
    config->enabled = g_movement_detector.enabled;
    config->stationary_threshold = g_movement_detector.stationary_threshold;
    config->movement_threshold = g_movement_detector.movement_threshold;
    config->speed_threshold = g_movement_detector.speed_threshold;
    config->max_realistic_speed = g_movement_detector.max_realistic_speed;
    config->history_size = g_movement_detector.history_size;
    config->min_positions = g_movement_detector.min_positions;
    config->analysis_interval = g_movement_detector.analysis_interval;
    
    pthread_mutex_unlock(&g_movement_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set movement detector configuration
static int gps_movement_set_config(const gps_movement_config_t *config) {
    if (!g_movement_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_movement_mutex);
    
    g_movement_detector.enabled = config->enabled;
    g_movement_detector.stationary_threshold = config->stationary_threshold;
    g_movement_detector.movement_threshold = config->movement_threshold;
    g_movement_detector.speed_threshold = config->speed_threshold;
    g_movement_detector.max_realistic_speed = config->max_realistic_speed;
    g_movement_detector.history_size = config->history_size;
    g_movement_detector.min_positions = config->min_positions;
    g_movement_detector.analysis_interval = config->analysis_interval;
    
    pthread_mutex_unlock(&g_movement_mutex);
    
    LOGX_INFO("GPS movement detector configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable movement detector
static int gps_movement_set_enabled(bool enabled) {
    if (!g_movement_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_movement_mutex);
    g_movement_detector.enabled = enabled;
    pthread_mutex_unlock(&g_movement_mutex);
    
    LOGX_INFO("GPS movement detector %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Force movement analysis
static int gps_movement_force_analysis(void) {
    if (!g_movement_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_movement_mutex);
    
    if (g_movement_detector.position_count >= g_movement_detector.min_positions) {
        analyze_movement_pattern();
        g_movement_detector.last_analysis = time(NULL);
        g_movement_detector.total_analyses++;
        pthread_mutex_unlock(&g_movement_mutex);
        
        LOGX_INFO("Forced movement analysis completed");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_unlock(&g_movement_mutex);
    return AUTONOMY_ERROR_NO_DATA;
}

// Reset movement detector
static int gps_movement_reset(void) {
    if (!g_movement_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_movement_mutex);
    
    g_movement_detector.position_count = 0;
    g_movement_detector.last_analysis = 0;
    g_movement_detector.total_analyses = 0;
    g_movement_detector.movement_detected = false;
    g_movement_detector.current_pattern = MOVEMENT_PATTERN_UNKNOWN;
    
    // Clear position history
    for (int i = 0; i < g_movement_detector.history_size; i++) {
        g_movement_detector.position_history[i].timestamp = 0;
        g_movement_detector.position_history[i].lat = 0.0;
        g_movement_detector.position_history[i].lon = 0.0;
        g_movement_detector.position_history[i].altitude = 0.0;
        g_movement_detector.position_history[i].accuracy = 0.0;
    }
    
    pthread_mutex_unlock(&g_movement_mutex);
    
    LOGX_INFO("GPS movement detector reset");
    return AUTONOMY_SUCCESS;
}

// Cleanup movement detector
static void gps_movement_cleanup(void) {
    if (!g_movement_initialized) {
        return;
    }
    
    pthread_mutex_destroy(&g_movement_mutex);
    g_movement_initialized = false;
    
    LOGX_INFO("GPS movement detector cleaned up");
}
