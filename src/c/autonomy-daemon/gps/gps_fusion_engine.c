#include "gps_fusion_engine.h"
#include "../utils/logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <time.h>

// Global fusion engine instance
static gps_fusion_engine_t g_fusion_engine = {0};
static bool g_fusion_initialized = false;

// Fusion method strings
static const char* FUSION_METHOD_STRINGS[] = {
    "single_source", "weighted_average", "confidence_weighted", 
    "kalman_filter", "particle_filter", "bayesian"
};

// Forward declarations
static int perform_weighted_average_fusion(const weighted_gps_data_t* weighted_data, int count,
                                         standardized_gps_data_t* result);
static int perform_confidence_weighted_fusion(const weighted_gps_data_t* weighted_data, int count,
                                             standardized_gps_data_t* result);
static int calculate_source_weights(const standardized_gps_data_t* source_data, int source_count,
                                   weighted_gps_data_t* weighted_data);
static double calculate_weight_factor(const standardized_gps_data_t* data, const char* factor_type);
static bool is_outlier(const standardized_gps_data_t* data, const standardized_gps_data_t* reference_data, 
                      int reference_count);
static double calculate_consensus_score(const standardized_gps_data_t* source_data, int source_count);
static void update_kalman_state(const standardized_gps_data_t* measurement);
static void predict_kalman_state(double dt);

// Initialize GPS fusion engine
int gps_fusion_engine_init(const gps_fusion_config_t* config) {
    if (g_fusion_initialized) {
        LOGX_WARN_MSG("GPS fusion engine already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    if (!config) {
        LOGX_ERROR_MSG("GPS fusion config is NULL");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    memset(&g_fusion_engine, 0, sizeof(gps_fusion_engine_t));
    g_fusion_engine.config = *config;
    
    // Initialize mutex
    if (pthread_mutex_init(&g_fusion_engine.mutex, NULL) != 0) {
        LOGX_ERROR_MSG("Failed to initialize fusion engine mutex");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Initialize Kalman filter if enabled
    if (config->enable_kalman_filtering) {
        // Initialize state vector [lat, lon, lat_velocity, lon_velocity]
        memset(g_fusion_engine.kalman_state, 0, sizeof(g_fusion_engine.kalman_state));
        
        // Initialize covariance matrix (4x4 identity * initial_uncertainty)
        memset(g_fusion_engine.kalman_covariance, 0, sizeof(g_fusion_engine.kalman_covariance));
        for (int i = 0; i < 4; i++) {
            g_fusion_engine.kalman_covariance[i * 4 + i] = config->initial_uncertainty;
        }
        
        g_fusion_engine.kalman_initialized = false; // Will be initialized on first measurement
    }
    
    // Initialize statistics
    g_fusion_engine.stats.stats_reset_time = time(NULL);
    
    g_fusion_initialized = true;
    
    LOGX_INFO_MSG("GPS fusion engine initialized",
              "method", gps_fusion_method_to_string(config->default_method),
              "outlier_detection", config->enable_outlier_detection,
              "consensus_checking", config->enable_consensus_checking,
              "kalman_filtering", config->enable_kalman_filtering,
              "temporal_smoothing", config->enable_temporal_smoothing);
    
    return AUTONOMY_SUCCESS;
}

// Cleanup GPS fusion engine
void gps_fusion_engine_cleanup(void) {
    if (!g_fusion_initialized) return;
    
    pthread_mutex_lock(&g_fusion_engine.mutex);
    
    pthread_mutex_unlock(&g_fusion_engine.mutex);
    pthread_mutex_destroy(&g_fusion_engine.mutex);
    
    g_fusion_initialized = false;
    
    LOGX_INFO_MSG("GPS fusion engine cleaned up");
}

// Fuse GPS data from multiple sources
int gps_fusion_engine_fuse(const standardized_gps_data_t* source_data, int source_count,
                          standardized_gps_data_t* result) {
    if (!g_fusion_initialized || !source_data || !result || source_count <= 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    return gps_fusion_engine_fuse_with_method(source_data, source_count, 
                                            g_fusion_engine.config.default_method, result);
}

// Fuse GPS data using specific method
int gps_fusion_engine_fuse_with_method(const standardized_gps_data_t* source_data, int source_count,
                                      gps_fusion_method_t method, standardized_gps_data_t* result) {
    if (!g_fusion_initialized || !source_data || !result || source_count <= 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_fusion_engine.mutex);
    
    time_t start_time = time(NULL);
    memset(result, 0, sizeof(standardized_gps_data_t));
    
    g_fusion_engine.stats.total_fusions++;
    
    // Filter out invalid data
    standardized_gps_data_t valid_data[GPS_SOURCE_MAX];
    int valid_count = 0;
    
    for (int i = 0; i < source_count; i++) {
        if (source_data[i].valid && 
            validate_gps_coordinates(source_data[i].latitude, source_data[i].longitude) &&
            validate_gps_accuracy(source_data[i].accuracy)) {
            valid_data[valid_count++] = source_data[i];
        }
    }
    
    if (valid_count == 0) {
        LOGX_WARN_MSG("No valid GPS data for fusion");
        g_fusion_engine.stats.failed_fusions++;
        pthread_mutex_unlock(&g_fusion_engine.mutex);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    // Outlier detection if enabled
    standardized_gps_data_t filtered_data[GPS_SOURCE_MAX];
    int filtered_count = valid_count;
    
    if (g_fusion_engine.config.enable_outlier_detection && valid_count > 2) {
        filtered_count = gps_fusion_engine_detect_outliers(valid_data, valid_count, 
                                                          filtered_data, GPS_SOURCE_MAX);
        if (filtered_count > 0) {
            memcpy(valid_data, filtered_data, filtered_count * sizeof(standardized_gps_data_t));
            valid_count = filtered_count;
            
            LOGX_DEBUG_MSG("Outlier detection completed",
                      "original_count", source_count,
                      "filtered_count", filtered_count,
                      "outliers_removed", source_count - filtered_count);
        }
    }
    
    // Consensus checking if enabled
    if (g_fusion_engine.config.enable_consensus_checking && valid_count > 1) {
        if (!gps_fusion_engine_check_consensus(valid_data, valid_count)) {
            LOGX_WARN_MSG("GPS sources lack consensus");
            g_fusion_engine.stats.consensus_failures++;
            // Continue with fusion anyway, but reduce confidence
        }
    }
    
    // Single source case
    if (valid_count == 1) {
        *result = valid_data[0];
        strcpy(result->source, "single_source");
        result->confidence *= 0.9; // Slight penalty for single source
        
        LOGX_DEBUG_MSG("Single source GPS fusion", "source", result->source);
        
        g_fusion_engine.stats.successful_fusions++;
        g_fusion_engine.stats.method_usage[GPS_FUSION_METHOD_SINGLE_SOURCE]++;
    } else {
        // Multi-source fusion
        weighted_gps_data_t weighted_data[GPS_SOURCE_MAX];
        int weight_count = calculate_source_weights(valid_data, valid_count, weighted_data);
        
        int fusion_ret = AUTONOMY_ERROR_SYSTEM;
        
        switch (method) {
            case GPS_FUSION_METHOD_WEIGHTED_AVERAGE:
                fusion_ret = perform_weighted_average_fusion(weighted_data, weight_count, result);
                break;
                
            case GPS_FUSION_METHOD_CONFIDENCE_WEIGHTED:
                fusion_ret = perform_confidence_weighted_fusion(weighted_data, weight_count, result);
                break;
                
            case GPS_FUSION_METHOD_KALMAN_FILTER:
                if (g_fusion_engine.config.enable_kalman_filtering) {
                    // For simplicity, use confidence weighted as base and apply Kalman
                    fusion_ret = perform_confidence_weighted_fusion(weighted_data, weight_count, result);
                    if (fusion_ret == AUTONOMY_SUCCESS) {
                        standardized_gps_data_t kalman_result;
                        if (gps_fusion_engine_apply_kalman_filter(result, &kalman_result) == AUTONOMY_SUCCESS) {
                            *result = kalman_result;
                        }
                    }
                } else {
                    fusion_ret = perform_confidence_weighted_fusion(weighted_data, weight_count, result);
                }
                break;
                
            default:
                fusion_ret = perform_confidence_weighted_fusion(weighted_data, weight_count, result);
                break;
        }
        
        if (fusion_ret == AUTONOMY_SUCCESS) {
            strcpy(result->source, "fused");
            result->source_type = GPS_SOURCE_MAX; // Special value for fused data
            
            g_fusion_engine.stats.successful_fusions++;
            g_fusion_engine.stats.method_usage[method]++;
        } else {
            g_fusion_engine.stats.failed_fusions++;
        }
    }
    
    // Apply temporal smoothing if enabled
    if (g_fusion_engine.config.enable_temporal_smoothing && result->valid) {
        standardized_gps_data_t smoothed_result;
        if (gps_fusion_engine_apply_smoothing(result, &smoothed_result) == AUTONOMY_SUCCESS) {
            *result = smoothed_result;
        }
    }
    
    // Update statistics
    time_t end_time = time(NULL);
    double fusion_time_ms = difftime(end_time, start_time) * 1000.0;
    
    g_fusion_engine.stats.average_fusion_time_ms = 
        (g_fusion_engine.stats.average_fusion_time_ms * (g_fusion_engine.stats.total_fusions - 1) + 
         fusion_time_ms) / g_fusion_engine.stats.total_fusions;
    
    g_fusion_engine.stats.average_sources_per_fusion = 
        (g_fusion_engine.stats.average_sources_per_fusion * (g_fusion_engine.stats.total_fusions - 1) + 
         valid_count) / g_fusion_engine.stats.total_fusions;
    
    if (result->valid) {
        g_fusion_engine.stats.average_fusion_accuracy = 
            (g_fusion_engine.stats.average_fusion_accuracy * g_fusion_engine.stats.successful_fusions + 
             result->accuracy) / (g_fusion_engine.stats.successful_fusions + 1);
        
        g_fusion_engine.stats.average_fusion_confidence = 
            (g_fusion_engine.stats.average_fusion_confidence * g_fusion_engine.stats.successful_fusions + 
             result->confidence) / (g_fusion_engine.stats.successful_fusions + 1);
    }
    
    g_fusion_engine.stats.last_fusion = time(NULL);
    g_fusion_engine.last_fusion_time = time(NULL);
    g_fusion_engine.last_result = *result;
    
    pthread_mutex_unlock(&g_fusion_engine.mutex);
    
    LOGX_INFO_MSG("GPS fusion completed",
             "method", gps_fusion_method_to_string(method),
             "sources_used", valid_count,
             "confidence", result->confidence,
             "accuracy", result->accuracy,
             "fusion_time_ms", fusion_time_ms);
    
    return AUTONOMY_SUCCESS;
}

// Perform weighted average fusion
static int perform_weighted_average_fusion(const weighted_gps_data_t* weighted_data, int count,
                                         standardized_gps_data_t* result) {
    if (count == 0) return AUTONOMY_ERROR_INVALID_PARAM;
    
    double total_weight = 0.0;
    double weighted_lat = 0.0;
    double weighted_lon = 0.0;
    double weighted_alt = 0.0;
    double weighted_accuracy = 0.0;
    double weighted_confidence = 0.0;
    
    for (int i = 0; i < count; i++) {
        double weight = weighted_data[i].weight;
        
        weighted_lat += weighted_data[i].data.latitude * weight;
        weighted_lon += weighted_data[i].data.longitude * weight;
        weighted_alt += weighted_data[i].data.altitude * weight;
        weighted_accuracy += weighted_data[i].data.accuracy * weight;
        weighted_confidence += weighted_data[i].data.confidence * weight;
        
        total_weight += weight;
    }
    
    if (total_weight == 0.0) {
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Calculate weighted averages
    result->latitude = weighted_lat / total_weight;
    result->longitude = weighted_lon / total_weight;
    result->altitude = weighted_alt / total_weight;
    result->accuracy = weighted_accuracy / total_weight;
    result->confidence = weighted_confidence / total_weight;
    result->timestamp = time(NULL);
    result->valid = true;
    
    // Use data from highest weight source for other fields
    int best_source_idx = 0;
    double best_weight = weighted_data[0].weight;
    for (int i = 1; i < count; i++) {
        if (weighted_data[i].weight > best_weight) {
            best_weight = weighted_data[i].weight;
            best_source_idx = i;
        }
    }
    
    result->satellites_used = weighted_data[best_source_idx].data.satellites_used;
    result->satellites_visible = weighted_data[best_source_idx].data.satellites_visible;
    result->hdop = weighted_data[best_source_idx].data.hdop;
    result->vdop = weighted_data[best_source_idx].data.vdop;
    result->fix_type = weighted_data[best_source_idx].data.fix_type;
    result->fix_quality = weighted_data[best_source_idx].data.fix_quality;
    
    LOGX_DEBUG_MSG("Weighted average fusion completed",
              "total_weight", total_weight,
              "sources", count,
              "lat", result->latitude,
              "lon", result->longitude,
              "accuracy", result->accuracy,
              "confidence", result->confidence);
    
    return AUTONOMY_SUCCESS;
}

// Perform confidence-weighted fusion using geodesic math
static int perform_confidence_weighted_fusion(const weighted_gps_data_t* weighted_data, int count,
                                             standardized_gps_data_t* result) {
    if (count == 0) return AUTONOMY_ERROR_INVALID_PARAM;
    
    // Use proper geodesic math for coordinate fusion (convert to Cartesian)
    double total_weight = 0.0;
    double x = 0.0, y = 0.0, z = 0.0;
    double weighted_accuracy = 0.0;
    double weighted_confidence = 0.0;
    
    for (int i = 0; i < count; i++) {
        double weight = weighted_data[i].weight * weighted_data[i].data.confidence;
        
        // Convert to Cartesian coordinates on unit sphere
        double lat_rad = weighted_data[i].data.latitude * M_PI / 180.0;
        double lon_rad = weighted_data[i].data.longitude * M_PI / 180.0;
        
        double cos_lat = cos(lat_rad);
        
        // Weighted Cartesian coordinates
        x += weight * cos_lat * cos(lon_rad);
        y += weight * cos_lat * sin(lon_rad);
        z += weight * sin(lat_rad);
        
        weighted_accuracy += weighted_data[i].data.accuracy * weight;
        weighted_confidence += weighted_data[i].data.confidence * weight;
        
        total_weight += weight;
    }
    
    if (total_weight == 0.0) {
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Normalize Cartesian coordinates
    x /= total_weight;
    y /= total_weight;
    z /= total_weight;
    
    // Convert back to latitude/longitude
    result->latitude = atan2(z, sqrt(x*x + y*y)) * 180.0 / M_PI;
    result->longitude = atan2(y, x) * 180.0 / M_PI;
    
    result->accuracy = weighted_accuracy / total_weight;
    result->confidence = weighted_confidence / total_weight;
    result->timestamp = time(NULL);
    result->valid = true;
    
    // Calculate fusion accuracy estimate based on source spread
    double max_distance = 0.0;
    for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) {
            double distance = gps_calculate_distance(
                weighted_data[i].data.latitude, weighted_data[i].data.longitude,
                weighted_data[j].data.latitude, weighted_data[j].data.longitude
            );
            if (distance > max_distance) {
                max_distance = distance;
            }
        }
    }
    
    // Adjust accuracy based on source spread
    if (max_distance > 0) {
        double spread_factor = 1.0 + (max_distance / 1000.0); // Increase uncertainty for spread
        result->accuracy *= spread_factor;
    }
    
    // Use best source data for other fields
    int best_idx = 0;
    double best_confidence = weighted_data[0].data.confidence;
    for (int i = 1; i < count; i++) {
        if (weighted_data[i].data.confidence > best_confidence) {
            best_confidence = weighted_data[i].data.confidence;
            best_idx = i;
        }
    }
    
    result->altitude = weighted_data[best_idx].data.altitude;
    result->satellites_used = weighted_data[best_idx].data.satellites_used;
    result->hdop = weighted_data[best_idx].data.hdop;
    result->fix_type = weighted_data[best_idx].data.fix_type;
    result->fix_quality = weighted_data[best_idx].data.fix_quality;
    
    LOGX_INFO_MSG("Confidence-weighted fusion completed",
             "sources", count,
             "total_weight", total_weight,
             "spread_m", max_distance,
             "lat", result->latitude,
             "lon", result->longitude,
             "accuracy", result->accuracy,
             "confidence", result->confidence);
    
    pthread_mutex_unlock(&g_fusion_engine.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Calculate weights for each GPS source
static int calculate_source_weights(const standardized_gps_data_t* source_data, int source_count,
                                   weighted_gps_data_t* weighted_data) {
    for (int i = 0; i < source_count; i++) {
        weighted_data[i].data = source_data[i];
        
        // Calculate individual weight factors
        weighted_data[i].accuracy_factor = calculate_weight_factor(&source_data[i], "accuracy");
        weighted_data[i].confidence_factor = calculate_weight_factor(&source_data[i], "confidence");
        weighted_data[i].freshness_factor = calculate_weight_factor(&source_data[i], "freshness");
        weighted_data[i].health_factor = 1.0; // Would use source health if available
        weighted_data[i].priority_factor = 1.0 / source_data[i].source_priority;
        
        // Combine factors into final weight
        weighted_data[i].weight = 
            weighted_data[i].accuracy_factor * g_fusion_engine.config.accuracy_weight +
            weighted_data[i].confidence_factor * g_fusion_engine.config.confidence_weight +
            weighted_data[i].freshness_factor * g_fusion_engine.config.freshness_weight +
            weighted_data[i].health_factor * g_fusion_engine.config.health_weight +
            weighted_data[i].priority_factor * g_fusion_engine.config.priority_weight;
        
        // Ensure minimum weight
        if (weighted_data[i].weight < 0.01) {
            weighted_data[i].weight = 0.01;
        }
        
        snprintf(weighted_data[i].weight_reasoning, sizeof(weighted_data[i].weight_reasoning),
                "acc:%.2f conf:%.2f fresh:%.2f prio:%.2f -> %.2f",
                weighted_data[i].accuracy_factor, weighted_data[i].confidence_factor,
                weighted_data[i].freshness_factor, weighted_data[i].priority_factor,
                weighted_data[i].weight);
        
        LOGX_DEBUG_MSG("GPS source weight calculated",
                  "source", source_data[i].source,
                  "weight", weighted_data[i].weight,
                  "reasoning", weighted_data[i].weight_reasoning);
    }
    
    return source_count;
}

// Calculate weight factor for specific aspect
static double calculate_weight_factor(const standardized_gps_data_t* data, const char* factor_type) {
    if (!data || !factor_type) return 0.0;
    
    if (strcmp(factor_type, "accuracy") == 0) {
        if (data->accuracy <= 0) return 0.5;
        // Inverse relationship: better accuracy = higher weight
        return 1.0 / (1.0 + data->accuracy / 50.0); // 50m reference
    } else if (strcmp(factor_type, "confidence") == 0) {
        return data->confidence;
    } else if (strcmp(factor_type, "freshness") == 0) {
        double age_seconds = difftime(time(NULL), data->timestamp);
        return exp(-age_seconds / 300.0); // Exponential decay over 5 minutes
    }
    
    return 1.0;
}

// Validation functions
static bool validate_gps_coordinates(double latitude, double longitude) {
    return (latitude >= -90.0 && latitude <= 90.0 && 
            longitude >= -180.0 && longitude <= 180.0 &&
            latitude != 0.0 && longitude != 0.0);
}

static bool validate_gps_accuracy(double accuracy) {
    return (accuracy > 0.0 && accuracy <= 50000.0); // Up to 50km accuracy
}

// Utility function to convert fusion method to string
const char* gps_fusion_method_to_string(gps_fusion_method_t method) {
    if (method >= 0 && method < GPS_FUSION_METHOD_MAX) {
        return FUSION_METHOD_STRINGS[method];
    }
    return "unknown";
}

// Check if fusion engine is initialized
bool gps_fusion_engine_is_initialized(void) {
    return g_fusion_initialized;
}

// Additional functions would be implemented here...
// (outlier detection, consensus checking, Kalman filtering, etc.)