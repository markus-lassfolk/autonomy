#include "gps_confidence.h"
#include "logx.h"
#include "types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

// GPS confidence configuration
static const double MIN_CONFIDENCE = 0.1;         // Minimum confidence threshold
static const double MAX_CONFIDENCE = 1.0;          // Maximum confidence threshold
static const double ACCURACY_WEIGHT = 0.35;        // Accuracy weight in confidence calculation
static const double SATELLITE_WEIGHT = 0.25;       // Satellite count weight
static const double FIX_QUALITY_WEIGHT = 0.20;     // Fix quality weight
static const double FRESHNESS_WEIGHT = 0.15;       // Data freshness weight
static const double CONSISTENCY_WEIGHT = 0.05;     // Position consistency weight

// Confidence thresholds
static const double HIGH_ACCURACY_THRESHOLD = 10.0;    // 10 meters
static const double MEDIUM_ACCURACY_THRESHOLD = 50.0;  // 50 meters
static const double LOW_ACCURACY_THRESHOLD = 100.0;    // 100 meters
static const int EXCELLENT_SATELLITES = 10;            // 10+ satellites
static const int GOOD_SATELLITES = 6;                  // 6+ satellites
static const int ADEQUATE_SATELLITES = 4;              // 4+ satellites
static const int FRESH_DATA_THRESHOLD = 30;            // 30 seconds
static const int RECENT_DATA_THRESHOLD = 60;           // 60 seconds
static const int OLD_DATA_THRESHOLD = 300;             // 5 minutes

// Global confidence calculator state
static gps_confidence_t g_confidence_calc = {0};
static bool g_confidence_initialized = false;

// Initialize GPS confidence calculator
int gps_confidence_init(void) {
    if (g_confidence_initialized) {
        LOGX_WARN("GPS confidence calculator already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    // Initialize confidence calculator state
    memset(&g_confidence_calc, 0, sizeof(gps_confidence_t));
    g_confidence_calc.enabled = true;
    g_confidence_calc.min_confidence = MIN_CONFIDENCE;
    g_confidence_calc.max_confidence = MAX_CONFIDENCE;
    g_confidence_calc.accuracy_weight = ACCURACY_WEIGHT;
    g_confidence_calc.satellite_weight = SATELLITE_WEIGHT;
    g_confidence_calc.fix_quality_weight = FIX_QUALITY_WEIGHT;
    g_confidence_calc.freshness_weight = FRESHNESS_WEIGHT;
    g_confidence_calc.consistency_weight = CONSISTENCY_WEIGHT;
    
    g_confidence_initialized = true;
    
    LOGX_INFO("GPS confidence calculator initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Calculate GPS confidence score
double gps_confidence_calculate(const gps_data_t *gps_data, const gps_confidence_context_t *context) {
    if (!g_confidence_initialized || !gps_data) {
        return MIN_CONFIDENCE;
    }
    
    double confidence = 0.0;
    
    // Calculate accuracy-based confidence
    double accuracy_confidence = calculate_accuracy_confidence(gps_data->accuracy);
    confidence += accuracy_confidence * g_confidence_calc.accuracy_weight;
    
    // Calculate satellite-based confidence
    double satellite_confidence = calculate_satellite_confidence(gps_data->satellites);
    confidence += satellite_confidence * g_confidence_calc.satellite_weight;
    
    // Calculate fix quality confidence
    double fix_quality_confidence = calculate_fix_quality_confidence(gps_data->fix_quality);
    confidence += fix_quality_confidence * g_confidence_calc.fix_quality_weight;
    
    // Calculate data freshness confidence
    double freshness_confidence = calculate_freshness_confidence(gps_data->timestamp);
    confidence += freshness_confidence * g_confidence_calc.freshness_weight;
    
    // Calculate position consistency confidence (if context provided)
    if (context && context->previous_positions && context->position_count > 0) {
        double consistency_confidence = calculate_consistency_confidence(gps_data, context);
        confidence += consistency_confidence * g_confidence_calc.consistency_weight;
    }
    
    // Apply confidence bounds
    confidence = fmax(confidence, g_confidence_calc.min_confidence);
    confidence = fmin(confidence, g_confidence_calc.max_confidence);
    
    LOGX_DEBUG("GPS confidence calculated: %.3f (acc:%.3f, sat:%.3f, fix:%.3f, fresh:%.3f)", 
               confidence, accuracy_confidence, satellite_confidence, fix_quality_confidence, freshness_confidence);
    
    return confidence;
}

// Calculate accuracy-based confidence
static double calculate_accuracy_confidence(double accuracy) {
    if (accuracy <= 0) {
        return 0.0;
    }
    
    if (accuracy <= HIGH_ACCURACY_THRESHOLD) {
        return 1.0;  // Excellent accuracy
    } else if (accuracy <= MEDIUM_ACCURACY_THRESHOLD) {
        // Linear interpolation between 10m and 50m
        return 1.0 - ((accuracy - HIGH_ACCURACY_THRESHOLD) / 
                      (MEDIUM_ACCURACY_THRESHOLD - HIGH_ACCURACY_THRESHOLD)) * 0.3;
    } else if (accuracy <= LOW_ACCURACY_THRESHOLD) {
        // Linear interpolation between 50m and 100m
        return 0.7 - ((accuracy - MEDIUM_ACCURACY_THRESHOLD) / 
                      (LOW_ACCURACY_THRESHOLD - MEDIUM_ACCURACY_THRESHOLD)) * 0.4;
    } else {
        // Very low accuracy - exponential decay
        return 0.3 * exp(-(accuracy - LOW_ACCURACY_THRESHOLD) / 100.0);
    }
}

// Calculate satellite-based confidence
static double calculate_satellite_confidence(int satellite_count) {
    if (satellite_count <= 0) {
        return 0.0;
    }
    
    if (satellite_count >= EXCELLENT_SATELLITES) {
        return 1.0;  // Excellent satellite coverage
    } else if (satellite_count >= GOOD_SATELLITES) {
        // Linear interpolation between 6 and 10 satellites
        return 0.8 + ((satellite_count - GOOD_SATELLITES) / 
                      (EXCELLENT_SATELLITES - GOOD_SATELLITES)) * 0.2;
    } else if (satellite_count >= ADEQUATE_SATELLITES) {
        // Linear interpolation between 4 and 6 satellites
        return 0.5 + ((satellite_count - ADEQUATE_SATELLITES) / 
                      (GOOD_SATELLITES - ADEQUATE_SATELLITES)) * 0.3;
    } else {
        // Poor satellite coverage
        return 0.5 * (satellite_count / (double)ADEQUATE_SATELLITES);
    }
}

// Calculate fix quality confidence
static double calculate_fix_quality_confidence(int fix_quality) {
    switch (fix_quality) {
        case 0: return 0.0;   // No fix
        case 1: return 0.3;   // GPS fix
        case 2: return 0.6;   // DGPS fix
        case 4: return 0.8;   // RTK fix
        case 5: return 1.0;   // Float RTK
        case 6: return 0.9;   // Estimated (dead reckoning)
        default: return 0.5;  // Unknown fix quality
    }
}

// Calculate data freshness confidence
static double calculate_freshness_confidence(time_t timestamp) {
    if (timestamp <= 0) {
        return 0.0;
    }
    
    time_t now = time(NULL);
    int age = now - timestamp;
    
    if (age <= FRESH_DATA_THRESHOLD) {
        return 1.0;  // Very fresh data
    } else if (age <= RECENT_DATA_THRESHOLD) {
        // Linear interpolation between 30s and 60s
        return 1.0 - ((age - FRESH_DATA_THRESHOLD) / 
                      (RECENT_DATA_THRESHOLD - FRESH_DATA_THRESHOLD)) * 0.2;
    } else if (age <= OLD_DATA_THRESHOLD) {
        // Linear interpolation between 60s and 300s
        return 0.8 - ((age - RECENT_DATA_THRESHOLD) / 
                      (OLD_DATA_THRESHOLD - RECENT_DATA_THRESHOLD)) * 0.4;
    } else {
        // Very old data - exponential decay
        return 0.4 * exp(-(age - OLD_DATA_THRESHOLD) / 300.0);
    }
}

// Calculate position consistency confidence
static double calculate_consistency_confidence(const gps_data_t *current_gps, 
                                            const gps_confidence_context_t *context) {
    if (!context || !context->previous_positions || context->position_count == 0) {
        return 0.5;  // Neutral confidence if no context
    }
    
    double total_consistency = 0.0;
    int valid_positions = 0;
    
    // Calculate consistency with previous positions
    for (int i = 0; i < context->position_count && i < MAX_POSITION_HISTORY; i++) {
        const gps_data_t *prev_gps = &context->previous_positions[i];
        
        if (prev_gps->timestamp <= 0 || prev_gps->lat == 0.0 || prev_gps->lon == 0.0) {
            continue;
        }
        
        // Calculate distance between current and previous position
        double distance = calculate_distance(current_gps->lat, current_gps->lon,
                                          prev_gps->lat, prev_gps->lon);
        
        // Calculate time difference
        int time_diff = abs((int)(current_gps->timestamp - prev_gps->timestamp));
        
        // Calculate expected movement based on time difference
        double expected_movement = calculate_expected_movement(time_diff, context);
        
        // Calculate consistency score for this comparison
        double consistency = calculate_position_consistency(distance, expected_movement, 
                                                         current_gps->accuracy, prev_gps->accuracy);
        
        total_consistency += consistency;
        valid_positions++;
    }
    
    if (valid_positions == 0) {
        return 0.5;  // Neutral confidence if no valid previous positions
    }
    
    return total_consistency / valid_positions;
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

// Calculate expected movement based on time difference
static double calculate_expected_movement(int time_diff_seconds, const gps_confidence_context_t *context) {
    if (!context || context->average_speed <= 0) {
        return 0.0;  // No speed information available
    }
    
    // Convert speed from m/s to expected movement in meters
    return context->average_speed * time_diff_seconds;
}

// Calculate position consistency score
static double calculate_position_consistency(double actual_distance, double expected_movement,
                                          double current_accuracy, double previous_accuracy) {
    // Calculate combined accuracy
    double combined_accuracy = sqrt(current_accuracy * current_accuracy + 
                                   previous_accuracy * previous_accuracy);
    
    // Calculate movement difference
    double movement_diff = fabs(actual_distance - expected_movement);
    
    // Calculate consistency score
    if (movement_diff <= combined_accuracy) {
        return 1.0;  // Movement is within accuracy bounds
    } else if (movement_diff <= combined_accuracy * 2.0) {
        // Movement is within 2x accuracy bounds
        return 0.8 - (movement_diff - combined_accuracy) / combined_accuracy * 0.3;
    } else if (movement_diff <= combined_accuracy * 5.0) {
        // Movement is within 5x accuracy bounds
        return 0.5 - (movement_diff - combined_accuracy * 2.0) / (combined_accuracy * 3.0) * 0.3;
    } else {
        // Movement is significantly different from expected
        return 0.2 * exp(-(movement_diff - combined_accuracy * 5.0) / combined_accuracy);
    }
}

// Get confidence calculator configuration
int gps_confidence_get_config(gps_confidence_config_t *config) {
    if (!g_confidence_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    config->enabled = g_confidence_calc.enabled;
    config->min_confidence = g_confidence_calc.min_confidence;
    config->max_confidence = g_confidence_calc.max_confidence;
    config->accuracy_weight = g_confidence_calc.accuracy_weight;
    config->satellite_weight = g_confidence_calc.satellite_weight;
    config->fix_quality_weight = g_confidence_calc.fix_quality_weight;
    config->freshness_weight = g_confidence_calc.freshness_weight;
    config->consistency_weight = g_confidence_calc.consistency_weight;
    
    return AUTONOMY_SUCCESS;
}

// Set confidence calculator configuration
int gps_confidence_set_config(const gps_confidence_config_t *config) {
    if (!g_confidence_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Validate weights sum to approximately 1.0
    double total_weight = config->accuracy_weight + config->satellite_weight + 
                         config->fix_quality_weight + config->freshness_weight + 
                         config->consistency_weight;
    
    if (fabs(total_weight - 1.0) > 0.1) {
        LOGX_WARN("GPS confidence weights should sum to 1.0, current sum: %.3f", total_weight);
    }
    
    g_confidence_calc.enabled = config->enabled;
    g_confidence_calc.min_confidence = config->min_confidence;
    g_confidence_calc.max_confidence = config->max_confidence;
    g_confidence_calc.accuracy_weight = config->accuracy_weight;
    g_confidence_calc.satellite_weight = config->satellite_weight;
    g_confidence_calc.fix_quality_weight = config->fix_quality_weight;
    g_confidence_calc.freshness_weight = config->freshness_weight;
    g_confidence_calc.consistency_weight = config->consistency_weight;
    
    LOGX_INFO("GPS confidence calculator configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable confidence calculator
int gps_confidence_set_enabled(bool enabled) {
    if (!g_confidence_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    g_confidence_calc.enabled = enabled;
    LOGX_INFO("GPS confidence calculator %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Get confidence calculator status
int gps_confidence_get_status(gps_confidence_status_t *status) {
    if (!g_confidence_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    status->enabled = g_confidence_calc.enabled;
    status->min_confidence = g_confidence_calc.min_confidence;
    status->max_confidence = g_confidence_calc.max_confidence;
    status->accuracy_weight = g_confidence_calc.accuracy_weight;
    status->satellite_weight = g_confidence_calc.satellite_weight;
    status->fix_quality_weight = g_confidence_calc.fix_quality_weight;
    status->freshness_weight = g_confidence_calc.freshness_weight;
    status->consistency_weight = g_confidence_calc.consistency_weight;
    
    return AUTONOMY_SUCCESS;
}

// Cleanup confidence calculator
void gps_confidence_cleanup(void) {
    if (!g_confidence_initialized) {
        return;
    }
    
    g_confidence_initialized = false;
    LOGX_INFO("GPS confidence calculator cleaned up");
}
