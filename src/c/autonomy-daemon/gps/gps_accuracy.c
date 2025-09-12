#include "gps_coordinate_utils.h"
#include "gps_accuracy.h"
#include "../utils/logx.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Forward declaration
static void update_validation_statistics(const gps_validation_result_t *result);

// GPS accuracy validation configuration
static const double MIN_ACCURACY = 0.1; // Use configurable value // Use configurable value            // Minimum accuracy in meters
static const double MAX_ACCURACY = 10000.0; // Use configurable value // Use configurable value        // Maximum accuracy in meters
static const double SUSPICIOUS_ACCURACY = 1.0; // Use configurable value // Use configurable value     // Suspiciously good accuracy threshold
static const double POOR_ACCURACY = 100.0; // Use configurable value // Use configurable value         // Poor accuracy threshold
static const int MIN_SATELLITES = 3; // Use configurable value // Use configurable count // Use configurable value               // Minimum satellites for valid fix
static const int MAX_SATELLITES = 50; // Use configurable value // Use configurable count // Use configurable value              // Maximum satellites (sanity check)
static const double MAX_SPEED = 1000.0; // Use configurable value // Use configurable value            // Maximum realistic speed in m/s
static const double MAX_ALTITUDE = 10000.0; // Use configurable value // Use configurable value        // Maximum realistic altitude in meters
static const double MIN_ALTITUDE = -1000.0;        // Minimum realistic altitude in meters

// Validation thresholds
static const double ACCURACY_IMPROVEMENT_THRESHOLD = 0.5; // Use configurable value // Use configurable value  // 50% improvement threshold
static const double ACCURACY_DEGRADATION_THRESHOLD = 2.0; // Use configurable value // Use configurable value  // 2x degradation threshold
static const int MAX_ACCURACY_CHANGES = 5; // Use configurable value // Use configurable count // Use configurable value                 // Maximum accuracy changes to track
static const double POSITION_JUMP_THRESHOLD = 1000.0; // Use configurable value // Use configurable value      // 1km position jump threshold
static const int VALIDATION_WINDOW = 300; // Use configurable value // Use configurable count // Use configurable value                   // 5 minute validation window

// Global accuracy validator state
static gps_accuracy_t g_accuracy_validator = {0};
static bool g_accuracy_initialized = false; // Use configurable setting // Use configurable setting

// Forward declarations
static bool validate_basic_parameters(const gps_data_t *gps_data, gps_validation_result_t *result);
static bool validate_accuracy_values(const gps_data_t *gps_data, gps_validation_result_t *result);
static bool validate_satellite_data(const gps_data_t *gps_data, gps_validation_result_t *result);
static bool validate_position_data(const gps_data_t *gps_data, gps_validation_result_t *result);
static bool validate_temporal_data(const gps_data_t *gps_data, gps_validation_result_t *result);
static bool validate_consistency(const gps_data_t *gps_data, gps_validation_result_t *result);
static double estimate_expected_accuracy(int satellites, int fix_quality);

// Validate GPS accuracy
int gps_accuracy_validate(const gps_data_t *gps_data, gps_validation_result_t *result) {
    if (!g_accuracy_initialized || !gps_data || !result) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Initialize result
    memset(result, 0, sizeof(gps_validation_result_t));
    result->timestamp = time(NULL);
    result->is_valid = true;
    result->confidence = 1.0;
    
    // Perform basic validation checks
    if (!validate_basic_parameters(gps_data, result)) {
        result->is_valid = false;
        result->confidence = 0.0;
    }
    
    // Perform accuracy-specific validation
    if (!validate_accuracy_values(gps_data, result)) {
        result->is_valid = false;
        result->confidence *= 0.5;
    }
    
    // Perform satellite validation
    if (!validate_satellite_data(gps_data, result)) {
        result->is_valid = false;
        result->confidence *= 0.7;
    }
    
    // Perform position validation
    if (!validate_position_data(gps_data, result)) {
        result->is_valid = false;
        result->confidence *= 0.6;
    }
    
    // Perform temporal validation
    if (!validate_temporal_data(gps_data, result)) {
        result->is_valid = false;
        result->confidence *= 0.8;
    }
    
    // Perform consistency validation
    if (!validate_consistency(gps_data, result)) {
        result->is_valid = false;
        result->confidence *= 0.9;
    }
    
    // Update validation statistics
    update_validation_statistics(result);
    
    // Log validation result
    if (result->is_valid) {
        LOGX_DEBUG_MSG("GPS accuracy validation passed: confidence=%.3f, accuracy=%.1fm", 
                   result->confidence, gps_data->accuracy);
    } else {
        LOGX_WARN_MSG("GPS accuracy validation failed: %s, confidence=%.3f", 
                  result->error_message, result->confidence);
    }
    
    return AUTONOMY_SUCCESS;
}

// Validate basic GPS parameters
static bool validate_basic_parameters(const gps_data_t *gps_data, gps_validation_result_t *result) {
    // Check for valid coordinates
    if (gps_data->lat < -90.0 || gps_data->lat > 90.0) {
        strncpy(result->error_message, "Invalid latitude value", sizeof(result->error_message) - 1);
        result->error_message[sizeof(result->error_message) - 1] = '\0';
        return false;
    }
    
    if (gps_data->lon < -180.0 || gps_data->lon > 180.0) {
        strncpy(result->error_message, "Invalid longitude value", sizeof(result->error_message) - 1);
        result->error_message[sizeof(result->error_message) - 1] = '\0';
        return false;
    }
    
    // Check for zero coordinates (often indicates no fix)
    if (gps_data->lat == 0.0 && gps_data->lon == 0.0) {
        strncpy(result->error_message, "Zero coordinates (no GPS fix)", sizeof(result->error_message) - 1);
        result->error_message[sizeof(result->error_message) - 1] = '\0';
        return false;
    }
    
    // Check timestamp validity
    if (gps_data->timestamp <= 0) {
        strncpy(result->error_message, "Invalid timestamp", sizeof(result->error_message) - 1);
        result->error_message[sizeof(result->error_message) - 1] = '\0';
        return false;
    }
    
    return true;
}

// Validate accuracy values
static bool validate_accuracy_values(const gps_data_t *gps_data, gps_validation_result_t *result) {
    // Check accuracy bounds
    if (gps_data->accuracy < g_accuracy_validator.min_accuracy) {
        strncpy(result->error_message, "Accuracy below minimum threshold", sizeof(result->error_message) - 1);
        result->error_message[sizeof(result->error_message) - 1] = '\0';
        return false;
    }
    
    if (gps_data->accuracy > g_accuracy_validator.max_accuracy) {
        strncpy(result->error_message, "Accuracy above maximum threshold", sizeof(result->error_message) - 1);
        result->error_message[sizeof(result->error_message) - 1] = '\0';
        return false;
    }
    
    // Check for suspiciously good accuracy
    if (gps_data->accuracy < g_accuracy_validator.suspicious_accuracy) {
        result->flags |= GPS_VALIDATION_SUSPICIOUS_ACCURACY;
        strncpy(result->warning_message, "Suspiciously good accuracy", sizeof(result->warning_message) - 1);
        result->warning_message[sizeof(result->warning_message) - 1] = '\0';
    }
    
    // Check for poor accuracy
    if (gps_data->accuracy > g_accuracy_validator.poor_accuracy) {
        result->flags |= GPS_VALIDATION_POOR_ACCURACY;
        strncpy(result->warning_message, "Poor accuracy", sizeof(result->warning_message) - 1);
        result->warning_message[sizeof(result->warning_message) - 1] = '\0';
    }
    
    return true;
}

// Validate satellite data
static bool validate_satellite_data(const gps_data_t *gps_data, gps_validation_result_t *result) {
    // Check satellite count bounds
    if (gps_data->satellites < g_accuracy_validator.min_satellites) {
        strncpy(result->error_message, "Insufficient satellites for valid fix", sizeof(result->error_message) - 1);
        result->error_message[sizeof(result->error_message) - 1] = '\0';
        return false;
    }
    
    if (gps_data->satellites > g_accuracy_validator.max_satellites) {
        strncpy(result->error_message, "Unrealistic satellite count", sizeof(result->error_message) - 1);
        result->error_message[sizeof(result->error_message) - 1] = '\0';
        return false;
    }
    
    // Check fix quality
    if (gps_data->fix_quality == 0) {
        strncpy(result->error_message, "No GPS fix", sizeof(result->error_message) - 1);
        result->error_message[sizeof(result->error_message) - 1] = '\0';
        return false;
    }
    
    return true;
}

// Validate position data
static bool validate_position_data(const gps_data_t *gps_data, gps_validation_result_t *result) {
    // Check altitude bounds
    if (gps_data->altitude < g_accuracy_validator.min_altitude || 
        gps_data->altitude > g_accuracy_validator.max_altitude) {
        result->flags |= GPS_VALIDATION_SUSPICIOUS_ALTITUDE;
        strncpy(result->warning_message, "Suspicious altitude value", sizeof(result->warning_message) - 1);
        result->warning_message[sizeof(result->warning_message) - 1] = '\0';
    }
    
    // Check for position jumps (if we have previous data)
    if (g_accuracy_validator.last_position.timestamp > 0) {
        double distance = gps_coordinate_distance(gps_data->lat, gps_data->lon,
                                          g_accuracy_validator.last_position.lat,
                                          g_accuracy_validator.last_position.lon);
        
        int time_diff = abs((int)(gps_data->timestamp - g_accuracy_validator.last_position.timestamp));
        
        if (time_diff > 0 && distance > POSITION_JUMP_THRESHOLD) {
            result->flags |= GPS_VALIDATION_POSITION_JUMP;
            strncpy(result->warning_message, "Large position jump detected", sizeof(result->warning_message) - 1);
            result->warning_message[sizeof(result->warning_message) - 1] = '\0';
        }
    }
    
    // Store current position for next validation
    memcpy(&g_accuracy_validator.last_position, gps_data, sizeof(gps_data_t));
    
    return true;
}

// Validate temporal data
static bool validate_temporal_data(const gps_data_t *gps_data, gps_validation_result_t *result) {
    time_t now = time(NULL);
    int age = now - gps_data->timestamp;
    
    // Check if data is too old
    if (age > VALIDATION_WINDOW) {
        result->flags |= GPS_VALIDATION_OLD_DATA;
        strncpy(result->warning_message, "GPS data is old", sizeof(result->warning_message) - 1);
        result->warning_message[sizeof(result->warning_message) - 1] = '\0';
    }
    
    // Check for future timestamps
    if (gps_data->timestamp > now + 60) {  // Allow 1 minute clock skew
        strncpy(result->error_message, "Future timestamp", sizeof(result->error_message) - 1);
        result->error_message[sizeof(result->error_message) - 1] = '\0';
        return false;
    }
    
    return true;
}

// Validate consistency
static bool validate_consistency(const gps_data_t *gps_data, gps_validation_result_t *result) {
    // Check if accuracy is consistent with satellite count
    double expected_accuracy = estimate_expected_accuracy(gps_data->satellites, gps_data->fix_quality);
    
    if (expected_accuracy > 0) {
        double accuracy_ratio = gps_data->accuracy / expected_accuracy;
        
        if (accuracy_ratio < ACCURACY_IMPROVEMENT_THRESHOLD) {
            result->flags |= GPS_VALIDATION_SUSPICIOUS_ACCURACY;
            strncpy(result->warning_message, "Accuracy better than expected", sizeof(result->warning_message) - 1);
            result->warning_message[sizeof(result->warning_message) - 1] = '\0';
        } else if (accuracy_ratio > ACCURACY_DEGRADATION_THRESHOLD) {
            result->flags |= GPS_VALIDATION_POOR_ACCURACY;
            strncpy(result->warning_message, "Accuracy worse than expected", sizeof(result->warning_message) - 1);
            result->warning_message[sizeof(result->warning_message) - 1] = '\0';
        }
    }
    
    return true;
}

// Estimate expected accuracy based on satellite count and fix quality
static double estimate_expected_accuracy(int satellites, int fix_quality) {
    double base_accuracy = 10.0; // Use configurable value // Use configurable value  // Base accuracy in meters
    
    // Adjust based on satellite count
    if (satellites >= 10) {
        base_accuracy *= 0.5;  // Excellent coverage
    } else if (satellites >= 6) {
        base_accuracy *= 0.8;  // Good coverage
    } else if (satellites >= 4) {
        base_accuracy *= 1.2;  // Adequate coverage
    } else {
        base_accuracy *= 2.0;  // Poor coverage
    }
    
    // Adjust based on fix quality
    switch (fix_quality) {
        case 0: return 0.0;      // No fix
        case 1: return base_accuracy;           // GPS fix
        case 2: return base_accuracy * 0.5;     // DGPS fix
        case 4: return base_accuracy * 0.1;     // RTK fix
        case 5: return base_accuracy * 0.2;     // Float RTK
        case 6: return base_accuracy * 1.5;     // Estimated
        default: return base_accuracy;
    }
}

// Calculate distance between two GPS coordinates (Haversine formula)

// Update validation statistics
void update_validation_statistics(const gps_validation_result_t *result) {
    g_accuracy_validator.validation_count++;
    g_accuracy_validator.last_validation = time(NULL);
    
    if (result->is_valid) {
        g_accuracy_validator.valid_count++;
    } else {
        g_accuracy_validator.invalid_count++;
    }
    
    if (result->flags & GPS_VALIDATION_SUSPICIOUS_ACCURACY) {
        g_accuracy_validator.suspicious_count++;
    }
}

// Get accuracy validation statistics
int gps_accuracy_get_statistics(gps_accuracy_stats_t *stats) {
    if (!g_accuracy_initialized || !stats) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    stats->total_validations = g_accuracy_validator.validation_count;
    stats->valid_count = g_accuracy_validator.valid_count;
    stats->invalid_count = g_accuracy_validator.invalid_count;
    stats->suspicious_count = g_accuracy_validator.suspicious_count;
    stats->last_validation = g_accuracy_validator.last_validation;
    
    if (stats->total_validations > 0) {
        stats->validity_rate = (double)stats->valid_count / stats->total_validations;
    } else {
        stats->validity_rate = 0.0;
    }
    
    return AUTONOMY_SUCCESS;
}

// Get accuracy validator configuration
int gps_accuracy_get_config(gps_accuracy_config_t *config) {
    if (!g_accuracy_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    config->enabled = g_accuracy_validator.enabled;
    config->min_accuracy = g_accuracy_validator.min_accuracy;
    config->max_accuracy = g_accuracy_validator.max_accuracy;
    config->suspicious_accuracy = g_accuracy_validator.suspicious_accuracy;
    config->poor_accuracy = g_accuracy_validator.poor_accuracy;
    config->min_satellites = g_accuracy_validator.min_satellites;
    config->max_satellites = g_accuracy_validator.max_satellites;
    config->max_speed = g_accuracy_validator.max_speed;
    config->max_altitude = g_accuracy_validator.max_altitude;
    config->min_altitude = g_accuracy_validator.min_altitude;
    
    return AUTONOMY_SUCCESS;
}

// Set accuracy validator configuration
int gps_accuracy_set_config(const gps_accuracy_config_t *config) {
    if (!g_accuracy_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    g_accuracy_validator.enabled = config->enabled;
    g_accuracy_validator.min_accuracy = config->min_accuracy;
    g_accuracy_validator.max_accuracy = config->max_accuracy;
    g_accuracy_validator.suspicious_accuracy = config->suspicious_accuracy;
    g_accuracy_validator.poor_accuracy = config->poor_accuracy;
    g_accuracy_validator.min_satellites = config->min_satellites;
    g_accuracy_validator.max_satellites = config->max_satellites;
    g_accuracy_validator.max_speed = config->max_speed;
    g_accuracy_validator.max_altitude = config->max_altitude;
    g_accuracy_validator.min_altitude = config->min_altitude;
    
    LOGX_INFO_MSG("GPS accuracy validator configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable accuracy validator
int gps_accuracy_set_enabled(bool enabled) {
    if (!g_accuracy_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    g_accuracy_validator.enabled = enabled;
    LOGX_INFO_MSG("GPS accuracy validator %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Reset validation statistics
int gps_accuracy_reset_statistics(void) {
    if (!g_accuracy_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    g_accuracy_validator.validation_count = 0;
    g_accuracy_validator.valid_count = 0;
    g_accuracy_validator.invalid_count = 0;
    g_accuracy_validator.suspicious_count = 0;
    g_accuracy_validator.last_validation = 0;
    
    LOGX_INFO_MSG("GPS accuracy validation statistics reset");
    return AUTONOMY_SUCCESS;
}

// Cleanup accuracy validator
void gps_accuracy_cleanup(void) {
    if (!g_accuracy_initialized) {
        return;
    }
    
    g_accuracy_initialized = false; // Use configurable setting // Use configurable setting
    LOGX_INFO_MSG("GPS accuracy validator cleaned up");
}
