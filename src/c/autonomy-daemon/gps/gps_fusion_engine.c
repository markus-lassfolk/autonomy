#include "gps_fusion_engine.h"
#include "../shared/logging/logx.h"
#include "../shared/utils/string_utils.h"
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
static bool validate_gps_coordinates(double latitude, double longitude);
static bool validate_gps_accuracy(double accuracy);

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
              "outlier_detection", config->enable_outlier_detection ? "true" : "false",
              "consensus_checking", config->enable_consensus_checking ? "true" : "false",
              "kalman_filtering", config->enable_kalman_filtering ? "true" : "false",
              "temporal_smoothing", config->enable_temporal_smoothing ? "true" : "false");
    
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
        safe_strncpy(result->source, "single_source", sizeof(result->source));
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
            safe_strncpy(result->source, "fused", sizeof(result->source));
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
    double max_distance = 0.0; // Use configurable initial max distance
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

// Get fusion engine statistics
int gps_fusion_engine_get_statistics(gps_fusion_statistics_t* stats) {
    if (!g_fusion_initialized || !stats) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_fusion_engine.mutex);
    *stats = g_fusion_engine.stats;
    pthread_mutex_unlock(&g_fusion_engine.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Reset fusion engine statistics
int gps_fusion_engine_reset_statistics(void) {
    if (!g_fusion_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_fusion_engine.mutex);
    
    memset(&g_fusion_engine.stats, 0, sizeof(gps_fusion_statistics_t));
    g_fusion_engine.stats.stats_reset_time = time(NULL);
    
    pthread_mutex_unlock(&g_fusion_engine.mutex);
    
    LOGX_INFO_MSG("GPS fusion engine statistics reset");
    return AUTONOMY_SUCCESS;
}

// Outlier detection and filtering functions
int gps_fusion_engine_detect_outliers(const standardized_gps_data_t* source_data, int source_count,
                                     standardized_gps_data_t* filtered_data, int max_filtered) {
    if (!source_data || !filtered_data || source_count <= 0 || max_filtered <= 0) {
        return 0;
    }
    
    if (!g_fusion_initialized || !g_fusion_engine.config.enable_outlier_detection) {
        // If outlier detection is disabled, copy all data
        int copy_count = (source_count < max_filtered) ? source_count : max_filtered;
        for (int i = 0; i < copy_count; i++) {
            filtered_data[i] = source_data[i];
        }
        return copy_count;
    }
    
    int filtered_count = 0;
    double threshold = g_fusion_engine.config.outlier_detection_threshold;
    
    // Calculate centroid of all points
    double centroid_lat = 0.0, centroid_lon = 0.0;
    for (int i = 0; i < source_count; i++) {
        centroid_lat += source_data[i].latitude;
        centroid_lon += source_data[i].longitude;
    }
    centroid_lat /= source_count;
    centroid_lon /= source_count;
    
    // Calculate distances from centroid and identify outliers
    double* distances = malloc(source_count * sizeof(double));
    if (!distances) {
        return 0;
    }
    
    for (int i = 0; i < source_count; i++) {
        distances[i] = gps_calculate_distance(
            source_data[i].latitude, source_data[i].longitude,
            centroid_lat, centroid_lon
        );
    }
    
    // Calculate median distance for outlier detection
    double* sorted_distances = malloc(source_count * sizeof(double));
    if (!sorted_distances) {
        free(distances);
        return 0;
    }
    
    memcpy(sorted_distances, distances, source_count * sizeof(double));
    
    // Simple bubble sort for small arrays
    for (int i = 0; i < source_count - 1; i++) {
        for (int j = 0; j < source_count - i - 1; j++) {
            if (sorted_distances[j] > sorted_distances[j + 1]) {
                double temp = sorted_distances[j];
                sorted_distances[j] = sorted_distances[j + 1];
                sorted_distances[j + 1] = temp;
            }
        }
    }
    
    double median_distance = sorted_distances[source_count / 2];
    double outlier_threshold = median_distance * threshold;
    
    // Filter out outliers
    for (int i = 0; i < source_count && filtered_count < max_filtered; i++) {
        if (distances[i] <= outlier_threshold) {
            filtered_data[filtered_count] = source_data[i];
            filtered_count++;
        } else {
            g_fusion_engine.stats.outliers_detected++;
        }
    }
    
    free(distances);
    free(sorted_distances);
    
    LOGX_DEBUG_MSG("Outlier detection completed", 
                  "original_count", source_count,
                  "filtered_count", filtered_count,
                  "outliers_removed", source_count - filtered_count);
    
    return filtered_count;
}

bool gps_fusion_engine_check_consensus(const standardized_gps_data_t* source_data, int source_count) {
    if (!source_data || source_count <= 0) {
        return false;
    }
    
    if (!g_fusion_initialized || !g_fusion_engine.config.enable_consensus_checking) {
        return true; // If consensus checking is disabled, assume consensus
    }
    
    if (source_count < 2) {
        return true; // Single source always has consensus
    }
    
    double consensus_threshold = g_fusion_engine.config.consensus_threshold;
    double max_distance = g_fusion_engine.config.max_distance_difference;
    
    // Calculate pairwise distances between all sources
    int consensus_count = 0;
    int total_pairs = 0;
    
    for (int i = 0; i < source_count; i++) {
        for (int j = i + 1; j < source_count; j++) {
            double distance = gps_calculate_distance(
                source_data[i].latitude, source_data[i].longitude,
                source_data[j].latitude, source_data[j].longitude
            );
            
            total_pairs++;
            
            if (distance <= max_distance) {
                consensus_count++;
            }
        }
    }
    
    if (total_pairs == 0) {
        return true;
    }
    
    double consensus_ratio = (double)consensus_count / total_pairs;
    bool has_consensus = consensus_ratio >= consensus_threshold;
    
    if (!has_consensus) {
        g_fusion_engine.stats.consensus_failures++;
    }
    
    LOGX_DEBUG_MSG("Consensus check completed", 
                  "source_count", source_count,
                  "consensus_ratio", consensus_ratio,
                  "has_consensus", has_consensus);
    
    return has_consensus;
}

int gps_fusion_engine_apply_smoothing(const standardized_gps_data_t* current_data,
                                     standardized_gps_data_t* smoothed_result) {
    if (!current_data || !smoothed_result) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (!g_fusion_initialized || !g_fusion_engine.config.enable_temporal_smoothing) {
        // If smoothing is disabled, copy data as-is
        *smoothed_result = *current_data;
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_fusion_engine.mutex);
    
    // Add current data to history
    if (g_fusion_engine.smoothing_history_count < 10) {
        g_fusion_engine.smoothing_history[g_fusion_engine.smoothing_history_count] = *current_data;
        g_fusion_engine.smoothing_history_count++;
    } else {
        // Shift history and add new data
        for (int i = 0; i < 9; i++) {
            g_fusion_engine.smoothing_history[i] = g_fusion_engine.smoothing_history[i + 1];
        }
        g_fusion_engine.smoothing_history[9] = *current_data;
    }
    
    // Apply exponential moving average smoothing
    double alpha = 0.3; // Smoothing factor (0.0 = no smoothing, 1.0 = no history)
    
    if (g_fusion_engine.smoothing_history_count == 1) {
        *smoothed_result = *current_data;
    } else {
        // Initialize with first data point
        *smoothed_result = g_fusion_engine.smoothing_history[0];
        
        // Apply exponential moving average
        for (int i = 1; i < g_fusion_engine.smoothing_history_count; i++) {
            smoothed_result->latitude = alpha * g_fusion_engine.smoothing_history[i].latitude + 
                                      (1.0 - alpha) * smoothed_result->latitude;
            smoothed_result->longitude = alpha * g_fusion_engine.smoothing_history[i].longitude + 
                                       (1.0 - alpha) * smoothed_result->longitude;
            smoothed_result->accuracy = alpha * g_fusion_engine.smoothing_history[i].accuracy + 
                                      (1.0 - alpha) * smoothed_result->accuracy;
            smoothed_result->confidence = alpha * g_fusion_engine.smoothing_history[i].confidence + 
                                        (1.0 - alpha) * smoothed_result->confidence;
        }
        
        // Copy other fields from current data
        smoothed_result->timestamp = current_data->timestamp;
        smoothed_result->source_type = current_data->source_type;
        smoothed_result->speed = current_data->speed;
        smoothed_result->heading = current_data->heading;
        smoothed_result->altitude = current_data->altitude;
    }
    
    pthread_mutex_unlock(&g_fusion_engine.mutex);
    
    LOGX_DEBUG_MSG("GPS smoothing applied", 
                  "history_count", g_fusion_engine.smoothing_history_count,
                  "original_lat", current_data->latitude,
                  "smoothed_lat", smoothed_result->latitude,
                  "original_lon", current_data->longitude,
                  "smoothed_lon", smoothed_result->longitude);
    
    return AUTONOMY_SUCCESS;
}

int gps_fusion_engine_apply_kalman_filter(const standardized_gps_data_t* measurement,
                                         standardized_gps_data_t* filtered_result) {
    if (!measurement || !filtered_result) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (!g_fusion_initialized || !g_fusion_engine.config.enable_kalman_filtering) {
        // If Kalman filtering is disabled, copy data as-is
        *filtered_result = *measurement;
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_fusion_engine.mutex);
    
    // Initialize Kalman filter if not already done
    if (!g_fusion_engine.kalman_initialized) {
        // Initialize state vector [lat, lon, lat_velocity, lon_velocity]
        g_fusion_engine.kalman_state[0] = measurement->latitude;
        g_fusion_engine.kalman_state[1] = measurement->longitude;
        g_fusion_engine.kalman_state[2] = 0.0; // lat_velocity
        g_fusion_engine.kalman_state[3] = 0.0; // lon_velocity
        
        // Initialize covariance matrix (4x4 identity matrix scaled by initial uncertainty)
        double initial_uncertainty = g_fusion_engine.config.initial_uncertainty;
        for (int i = 0; i < 16; i++) {
            g_fusion_engine.kalman_covariance[i] = 0.0;
        }
        g_fusion_engine.kalman_covariance[0] = initial_uncertainty;  // lat variance
        g_fusion_engine.kalman_covariance[5] = initial_uncertainty;  // lon variance
        g_fusion_engine.kalman_covariance[10] = initial_uncertainty; // lat_vel variance
        g_fusion_engine.kalman_covariance[15] = initial_uncertainty; // lon_vel variance
        
        g_fusion_engine.kalman_initialized = true;
    }
    
    // Kalman filter prediction step
    double dt = 1.0; // Assume 1 second time step
    double process_noise = g_fusion_engine.config.process_noise_covariance;
    double measurement_noise = g_fusion_engine.config.measurement_noise_covariance;
    
    // State transition matrix F (constant velocity model)
    double F[16] = {
        1.0, 0.0, dt,  0.0,
        0.0, 1.0, 0.0, dt,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    };
    
    // Process noise matrix Q
    double Q[16] = {
        process_noise * dt * dt * dt * dt / 4, 0.0, process_noise * dt * dt * dt / 2, 0.0,
        0.0, process_noise * dt * dt * dt * dt / 4, 0.0, process_noise * dt * dt * dt / 2,
        process_noise * dt * dt * dt / 2, 0.0, process_noise * dt * dt, 0.0,
        0.0, process_noise * dt * dt * dt / 2, 0.0, process_noise * dt * dt
    };
    
    // Predict state: x_pred = F * x
    double state_pred[4];
    for (int i = 0; i < 4; i++) {
        state_pred[i] = 0.0;
        for (int j = 0; j < 4; j++) {
            state_pred[i] += F[i * 4 + j] * g_fusion_engine.kalman_state[j];
        }
    }
    
    // Predict covariance: P_pred = F * P * F^T + Q
    double cov_pred[16];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            cov_pred[i * 4 + j] = Q[i * 4 + j];
            for (int k = 0; k < 4; k++) {
                for (int l = 0; l < 4; l++) {
                    cov_pred[i * 4 + j] += F[i * 4 + k] * g_fusion_engine.kalman_covariance[k * 4 + l] * F[j * 4 + l];
                }
            }
        }
    }
    
    // Measurement matrix H (we only observe position, not velocity)
    double H[8] = {
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0
    };
    
    // Measurement noise matrix R
    double R[4] = {
        measurement_noise, 0.0,
        0.0, measurement_noise
    };
    
    // Innovation: y = z - H * x_pred
    double innovation[2];
    innovation[0] = measurement->latitude - state_pred[0];
    innovation[1] = measurement->longitude - state_pred[1];
    
    // Innovation covariance: S = H * P_pred * H^T + R
    double S[4];
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            S[i * 2 + j] = R[i * 2 + j];
            for (int k = 0; k < 4; k++) {
                for (int l = 0; l < 4; l++) {
                    S[i * 2 + j] += H[i * 4 + k] * cov_pred[k * 4 + l] * H[j * 4 + l];
                }
            }
        }
    }
    
    // Kalman gain: K = P_pred * H^T * S^(-1)
    double K[8];
    double S_det = S[0] * S[3] - S[1] * S[2];
    if (fabs(S_det) < 1e-10) {
        // Singular matrix, use identity
        S_det = 1.0;
        S[0] = S[3] = 1.0;
        S[1] = S[2] = 0.0;
    }
    
    double S_inv[4] = {
        S[3] / S_det, -S[1] / S_det,
        -S[2] / S_det, S[0] / S_det
    };
    
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 2; j++) {
            K[i * 2 + j] = 0.0;
            for (int k = 0; k < 4; k++) {
                K[i * 2 + j] += cov_pred[i * 4 + k] * H[j * 4 + k];
            }
            double temp = 0.0;
            for (int l = 0; l < 2; l++) {
                temp += K[i * 2 + l] * S_inv[l * 2 + j];
            }
            K[i * 2 + j] = temp;
        }
    }
    
    // Update state: x = x_pred + K * y
    for (int i = 0; i < 4; i++) {
        g_fusion_engine.kalman_state[i] = state_pred[i];
        for (int j = 0; j < 2; j++) {
            g_fusion_engine.kalman_state[i] += K[i * 2 + j] * innovation[j];
        }
    }
    
    // Update covariance: P = (I - K * H) * P_pred
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            double I_KH = (i == j) ? 1.0 : 0.0;
            for (int k = 0; k < 2; k++) {
                I_KH -= K[i * 2 + k] * H[k * 4 + j];
            }
            g_fusion_engine.kalman_covariance[i * 4 + j] = 0.0;
            for (int l = 0; l < 4; l++) {
                g_fusion_engine.kalman_covariance[i * 4 + j] += I_KH * cov_pred[l * 4 + j];
            }
        }
    }
    
    // Set filtered result
    filtered_result->latitude = g_fusion_engine.kalman_state[0];
    filtered_result->longitude = g_fusion_engine.kalman_state[1];
    filtered_result->accuracy = measurement->accuracy; // Keep original accuracy
    filtered_result->confidence = measurement->confidence; // Keep original confidence
    filtered_result->timestamp = measurement->timestamp;
    filtered_result->source_type = measurement->source_type;
    filtered_result->speed = measurement->speed;
    filtered_result->heading = measurement->heading;
    filtered_result->altitude = measurement->altitude;
    
    pthread_mutex_unlock(&g_fusion_engine.mutex);
    
    LOGX_DEBUG_MSG("Kalman filter applied", 
                  "original_lat", measurement->latitude,
                  "filtered_lat", filtered_result->latitude,
                  "original_lon", measurement->longitude,
                  "filtered_lon", filtered_result->longitude);
    
    return AUTONOMY_SUCCESS;
}

int gps_fusion_engine_get_config(gps_fusion_config_t* config) {
    if (!g_fusion_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_fusion_engine.mutex);
    *config = g_fusion_engine.config;
    pthread_mutex_unlock(&g_fusion_engine.mutex);
    
    return AUTONOMY_SUCCESS;
}

int gps_fusion_engine_set_config(const gps_fusion_config_t* config) {
    if (!g_fusion_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_fusion_engine.mutex);
    g_fusion_engine.config = *config;
    pthread_mutex_unlock(&g_fusion_engine.mutex);
    
    LOGX_INFO_MSG("GPS fusion engine configuration updated");
    return AUTONOMY_SUCCESS;
}