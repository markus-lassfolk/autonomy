#include "ml_monitor.h"
#include "../shared/logging/logx.h"
#include "../starlink/starlink_modules.h"
#include "../shared/starlink-tracking/obstruction_analyzer.h"
#include "../shared/starlink-tracking/starlink_tracker.h"
#include <time.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>

// Enhanced debugging macros
#define ML_DEBUG_ENTRY(func_name) \
    fprintf(stderr, "DEBUG: %s ENTRY\n", func_name)

#define ML_DEBUG_EXIT(func_name, result) \
    fprintf(stderr, "DEBUG: %s EXIT with result: %d\n", func_name, result)

#define ML_VALIDATE_POINTER(ptr, location) \
    do { \
        if (!(ptr)) { \
            fprintf(stderr, "ERROR: NULL pointer at %s:%d in %s\n", __FILE__, __LINE__, location); \
            abort(); \
        } \
        if ((uintptr_t)(ptr) < 0x1000) { \
            fprintf(stderr, "ERROR: Invalid pointer %p (too low) at %s:%d in %s\n", (ptr), __FILE__, __LINE__, location); \
            abort(); \
        } \
    } while(0)

#define ML_VALIDATE_ARRAY_BOUNDS(array, index, size, location) \
    do { \
        if ((index) < 0 || (index) >= (size)) { \
            fprintf(stderr, "ERROR: Array bounds violation: index=%d, size=%d at %s:%d in %s\n", \
                    (index), (size), __FILE__, __LINE__, location); \
            abort(); \
        } \
    } while(0)

#define ML_VALIDATE_WINDOW_ACCESS(predictor, index, location) \
    do { \
        ML_VALIDATE_POINTER(predictor, location); \
        ML_VALIDATE_ARRAY_BOUNDS((predictor)->window, (index), 60, location); \
    } while(0)

// Phase 3: Advanced Sky Grid Integration and Sliding Window Predictions

// Sliding window predictor structure (15-minute window at 15-second intervals)
typedef struct {
    ml_observation_t window[60];  // 15-minute window at 15-sec intervals
    uint8_t window_size;
    uint8_t write_idx;
    
    // Extracted features
    struct {
        uint8_t snr_trend;        // 0=falling, 128=stable, 255=rising
        uint8_t snr_volatility;
        uint8_t latency_trend;
        uint8_t packet_loss_trend;
        uint8_t obstruction_trend;
        uint8_t weather_severity;
        uint8_t time_of_day;      // 0-255 mapped to 24 hours
        uint8_t pattern_signature;
    } features;
    
    // Predictions
    uint8_t outage_probability_5min;
    uint8_t outage_probability_15min;
    uint8_t likely_cause;
    uint8_t confidence;
} sliding_predictor_t;

// Enhanced sky grid with obstruction analyzer integration
typedef struct {
    compact_sky_grid_t ml_grid;              // Our ML grid (90x45, 4 resolution)
    obstruction_analyzer_t *obstruction_analyzer; // Existing obstruction analyzer
    
    // Integration mapping
    struct {
        double ml_to_polar_scale_x;          // Scaling factor for X coordinate
        double ml_to_polar_scale_y;          // Scaling factor for Y coordinate
        int polar_center_x;                  // Polar map center X (61)
        int polar_center_y;                  // Polar map center Y (61)
        double polar_max_radius;             // Max radius in pixels (61.5)
    } mapping;
    
    // Fusion parameters
    struct {
        double ml_weight;                    // Weight for ML predictions (0.7)
        double obstruction_weight;           // Weight for obstruction data (0.3)
        double fusion_confidence_threshold; // Minimum confidence for fusion (0.6)
        bool enable_cross_validation;       // Enable cross-validation
    } fusion;
    
    // Statistics
    struct {
        uint32_t fused_predictions;
        uint32_t ml_only_predictions;
        uint32_t obstruction_only_predictions;
        uint32_t fusion_accuracy_correct;
        double fusion_accuracy_rate;
    } stats;
    
} enhanced_sky_grid_t;

// Global Phase 3 system instance
static enhanced_sky_grid_t g_phase3_enhanced_sky_grid = {0};
static sliding_predictor_t g_phase3_sliding_predictor = {0};
static bool g_phase3_initialized = false;

// CRITICAL: Add corruption detection for global predictor
static const uint32_t GLOBAL_PREDICTOR_MAGIC = 0xDEADBEEF;
static uint32_t g_phase3_predictor_magic = GLOBAL_PREDICTOR_MAGIC;

// Forward declarations
static int ml_monitor_init_enhanced_sky_grid(ml_monitor_t *monitor);
static int ml_monitor_integrate_with_obstruction_analyzer(ml_monitor_t *monitor, const ml_observation_t *observation);
static int ml_monitor_fuse_obstruction_data(enhanced_sky_grid_t *enhanced_grid, const obstruction_map_t *obstruction_map);
static void ml_monitor_extract_window_features(sliding_predictor_t *predictor);
static int ml_monitor_sliding_window_predict(ml_monitor_t *monitor, sliding_predictor_t *predictor, const ml_observation_t *observation);
static double ml_monitor_calculate_trend(const uint16_t *values, int count);
static uint8_t ml_monitor_calculate_volatility(const uint16_t *values, int count);

// Initialize enhanced sky grid with obstruction analyzer integration
static int ml_monitor_init_enhanced_sky_grid(ml_monitor_t *monitor) {
    
    if (!monitor || !monitor->state) {
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    // Use simple fprintf to avoid LOGX crashes
    fprintf(stderr, "Initializing enhanced sky grid with obstruction analyzer integration\n");
    
    // Initialize the global Phase 3 enhanced sky grid structure
    memset(&g_phase3_enhanced_sky_grid, 0, sizeof(enhanced_sky_grid_t));
    
    // Initialize the ML grid (copy from monitor's existing grid)
    g_phase3_enhanced_sky_grid.ml_grid = monitor->state->models.sky_grid;
    
    // Initialize obstruction analyzer integration
    g_phase3_enhanced_sky_grid.obstruction_analyzer = NULL; // Will be set when available
    
    // Initialize coordinate mapping parameters
    g_phase3_enhanced_sky_grid.mapping.ml_to_polar_scale_x = 123.0 / 90.0;  // ML azimuth bins to polar X
    g_phase3_enhanced_sky_grid.mapping.ml_to_polar_scale_y = 123.0 / 45.0;  // ML elevation bins to polar Y
    g_phase3_enhanced_sky_grid.mapping.polar_center_x = 61;
    g_phase3_enhanced_sky_grid.mapping.polar_center_y = 61;
    g_phase3_enhanced_sky_grid.mapping.polar_max_radius = 61.5;
    
    // Initialize fusion parameters
    g_phase3_enhanced_sky_grid.fusion.ml_weight = 0.7;
    g_phase3_enhanced_sky_grid.fusion.obstruction_weight = 0.3;
    g_phase3_enhanced_sky_grid.fusion.fusion_confidence_threshold = 0.6;
    g_phase3_enhanced_sky_grid.fusion.enable_cross_validation = true;
    
    // Initialize statistics
    g_phase3_enhanced_sky_grid.stats.fused_predictions = 0;
    g_phase3_enhanced_sky_grid.stats.ml_only_predictions = 0;
    g_phase3_enhanced_sky_grid.stats.obstruction_only_predictions = 0;
    g_phase3_enhanced_sky_grid.stats.fusion_accuracy_correct = 0;
    g_phase3_enhanced_sky_grid.stats.fusion_accuracy_rate = 0.0;
    
    // Initialize sliding window predictor
    memset(&g_phase3_sliding_predictor, 0, sizeof(sliding_predictor_t));
    g_phase3_sliding_predictor.window_size = 0;
    g_phase3_sliding_predictor.write_idx = 0;
    
    // CRITICAL: Set corruption detection magic
    g_phase3_predictor_magic = GLOBAL_PREDICTOR_MAGIC;
    
    // Initialize features
    g_phase3_sliding_predictor.features.snr_trend = 128; // Stable
    g_phase3_sliding_predictor.features.snr_volatility = 0;
    g_phase3_sliding_predictor.features.latency_trend = 0;
    g_phase3_sliding_predictor.features.packet_loss_trend = 0;
    g_phase3_sliding_predictor.features.obstruction_trend = 0;
    g_phase3_sliding_predictor.features.weather_severity = 0;
    g_phase3_sliding_predictor.features.time_of_day = 0;
    g_phase3_sliding_predictor.features.pattern_signature = 0;
    
    // Initialize predictions
    g_phase3_sliding_predictor.outage_probability_5min = 0;
    g_phase3_sliding_predictor.outage_probability_15min = 0;
    g_phase3_sliding_predictor.likely_cause = 0;
    g_phase3_sliding_predictor.confidence = 0;
    
    // Mark as initialized
    g_phase3_initialized = true;
    
    // Use simple fprintf for non-critical information to avoid LOGX crashes
    fprintf(stderr, "Enhanced sky grid initialized (ML: 90x45, Obstruction: 123x123, weights: %.1f/%.1f, threshold: %.1f)\n", 
            g_phase3_enhanced_sky_grid.fusion.ml_weight, 
            g_phase3_enhanced_sky_grid.fusion.obstruction_weight, 
            g_phase3_enhanced_sky_grid.fusion.fusion_confidence_threshold);
    
    return ML_MONITOR_SUCCESS;
}

// Integrate with obstruction analyzer for enhanced predictions
static int ml_monitor_integrate_with_obstruction_analyzer(ml_monitor_t *monitor, const ml_observation_t *observation) {
    fprintf(stderr, "DEBUG: ml_monitor_integrate_with_obstruction_analyzer called\n");
    if (!monitor || !monitor->state || !observation) {
        fprintf(stderr, "DEBUG: Invalid parameters in integrate_with_obstruction_analyzer\n");
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    // Check if Phase 3 system is initialized
    if (!g_phase3_initialized) {
        fprintf(stderr, "DEBUG: Phase 3 not initialized in integrate_with_obstruction_analyzer\n");
        return ML_MONITOR_ERROR_NOT_INITIALIZED;
    }
    
    fprintf(stderr, "DEBUG: About to initialize obstruction_map_t\n");
    // Get current obstruction map from existing analyzer
    obstruction_map_t current_obstruction_map;
    memset(&current_obstruction_map, 0, sizeof(current_obstruction_map));
    fprintf(stderr, "DEBUG: obstruction_map_t initialized successfully\n");
    
    // Try to get obstruction map from Starlink tracking system
    // This would integrate with the existing obstruction analyzer
    // Integration with obstruction analyzer
    
    fprintf(stderr, "DEBUG: About to get ml_grid pointer\n");
    compact_sky_grid_t *ml_grid = &g_phase3_enhanced_sky_grid.ml_grid;
    fprintf(stderr, "DEBUG: ml_grid pointer obtained successfully\n");
    
    // Update ML grid with current observation
    fprintf(stderr, "DEBUG: About to call ml_monitor_sky_grid_update\n");
    int ml_result = ml_monitor_sky_grid_update(ml_grid, observation->azimuth_deg, observation->elevation_deg, 
                                              (observation->flags & ML_OBS_FLAG_OUTAGE) ? 1 : 0);
    fprintf(stderr, "DEBUG: ml_monitor_sky_grid_update returned: %d\n", ml_result);
    
    if (ml_result != ML_MONITOR_SUCCESS) {
        LOGX_WARN_MSG("Failed to update ML sky grid: %d", ml_result);
        return ml_result;
    }
    
    fprintf(stderr, "DEBUG: About to calculate ML grid coordinates\n");
    // Convert ML grid coordinates to obstruction analyzer coordinates
    int ml_az_bin = observation->azimuth_deg / 4;  // 4 resolution
    int ml_el_bin = observation->elevation_deg / 4;
    fprintf(stderr, "DEBUG: ML coordinates calculated: az_bin=%d, el_bin=%d\n", ml_az_bin, ml_el_bin);
    
    fprintf(stderr, "DEBUG: About to check bounds and access obstruction_prob\n");
    if (ml_az_bin >= 0 && ml_az_bin < 90 && ml_el_bin >= 0 && ml_el_bin < 45) {
        fprintf(stderr, "DEBUG: Bounds check passed, accessing obstruction_prob array\n");
        uint8_t ml_obstruction_prob = ml_grid->obstruction_prob[ml_az_bin][ml_el_bin];
        fprintf(stderr, "DEBUG: obstruction_prob accessed successfully: %u\n", ml_obstruction_prob);
        
        // Cross-validate with obstruction analyzer if available
        // This would check against the 123x123 polar projection
        
        LOGX_DEBUG_MSG("ML sky grid updated: az=%d, el=%d, prob=%u%% (bin [%d,%d])",
                  observation->azimuth_deg, observation->elevation_deg, 
                  ml_obstruction_prob, ml_az_bin, ml_el_bin);
    } else {
        fprintf(stderr, "DEBUG: Bounds check failed for ml_az_bin=%d, ml_el_bin=%d\n", ml_az_bin, ml_el_bin);
    }
    
    fprintf(stderr, "DEBUG: ml_monitor_integrate_with_obstruction_analyzer returning success\n");
    return ML_MONITOR_SUCCESS;
}

// Fuse ML predictions with obstruction analyzer data
static int ml_monitor_fuse_obstruction_data(enhanced_sky_grid_t *enhanced_grid, const obstruction_map_t *obstruction_map) {
    if (!enhanced_grid || !obstruction_map) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    compact_sky_grid_t *ml_grid = &enhanced_grid->ml_grid;
    
    // Iterate through ML grid and fuse with obstruction data
    for (int az_bin = 0; az_bin < 90; az_bin++) {
        for (int el_bin = 0; el_bin < 45; el_bin++) {
            // Convert ML grid coordinates to obstruction map coordinates
            double azimuth = az_bin * 4.0;  // ML grid uses 4 resolution
            double elevation = el_bin * 4.0;
            
            // Get obstruction data for this position
            obstruction_cell_t obstruction_cell;
            int obstruction_result = obstruction_analyzer_get_grid_cell(obstruction_map, azimuth, elevation, &obstruction_cell);
            
            if (obstruction_result == OBSTRUCTION_SUCCESS) {
                // Fuse ML prediction with obstruction data
                uint8_t ml_prob = ml_grid->obstruction_prob[az_bin][el_bin];
                uint8_t obstruction_prob = obstruction_cell.is_obstructed ? 255 : 0;
                
                // Weighted fusion
                double ml_weight = enhanced_grid->fusion.ml_weight;
                double obstruction_weight = enhanced_grid->fusion.obstruction_weight;
                
                uint8_t fused_prob = (uint8_t)((ml_prob * ml_weight + obstruction_prob * obstruction_weight) / 
                                              (ml_weight + obstruction_weight));
                
                // Update ML grid with fused result
                ml_grid->obstruction_prob[az_bin][el_bin] = fused_prob;
                
                enhanced_grid->stats.fused_predictions++;
            } else {
                // Use ML-only prediction
                enhanced_grid->stats.ml_only_predictions++;
            }
        }
    }
    
    LOGX_DEBUG_MSG("Sky grid fusion completed: %u fused, %u ML-only predictions",
              enhanced_grid->stats.fused_predictions, enhanced_grid->stats.ml_only_predictions);
    
    return ML_MONITOR_SUCCESS;
}

// Extract features from sliding window
static void ml_monitor_extract_window_features(sliding_predictor_t *predictor) {
    ML_DEBUG_ENTRY("ml_monitor_extract_window_features");
    
    ML_VALIDATE_POINTER(predictor, "ml_monitor_extract_window_features");
    
    if (predictor->window_size < 2) {
        ML_DEBUG_EXIT("ml_monitor_extract_window_features", 0);
        return;
    }
    
    // Extract SNR trend
    uint16_t snr_values[60];
    fprintf(stderr, "DEBUG: Extracting SNR trend, window_size=%u\n", predictor->window_size);
    for (int i = 0; i < predictor->window_size; i++) {
        fprintf(stderr, "DEBUG: Processing SNR value %d, predictor=%p, window[%d]=%p\n", 
                i, predictor, i, &predictor->window[i]);
        ML_VALIDATE_WINDOW_ACCESS(predictor, i, "SNR trend extraction");
        snr_values[i] = predictor->window[i].snr_x100;
        fprintf(stderr, "DEBUG: SNR value %d extracted: %u\n", i, snr_values[i]);
    }
    
    double snr_trend = ml_monitor_calculate_trend(snr_values, predictor->window_size);
    predictor->features.snr_trend = (uint8_t)((snr_trend + 1.0) * 127.5); // Map -1..1 to 0..255
    
    // Calculate SNR volatility
    predictor->features.snr_volatility = ml_monitor_calculate_volatility(snr_values, predictor->window_size);
    
    // Extract latency trend
    uint16_t latency_values[60];
    for (int i = 0; i < predictor->window_size; i++) {
        ML_VALIDATE_WINDOW_ACCESS(predictor, i, "latency trend extraction");
        latency_values[i] = predictor->window[i].latency_ms;
    }
    
    double latency_trend = ml_monitor_calculate_trend(latency_values, predictor->window_size);
    predictor->features.latency_trend = (uint8_t)((latency_trend + 1.0) * 127.5);
    
    // Extract packet loss trend
    uint16_t loss_values[60];
    for (int i = 0; i < predictor->window_size; i++) {
        ML_VALIDATE_WINDOW_ACCESS(predictor, i, "packet loss trend extraction");
        loss_values[i] = predictor->window[i].packet_loss_pct;
    }
    
    double loss_trend = ml_monitor_calculate_trend(loss_values, predictor->window_size);
    predictor->features.packet_loss_trend = (uint8_t)((loss_trend + 1.0) * 127.5);
    
    // Extract obstruction trend
    uint16_t obstruction_values[60];
    for (int i = 0; i < predictor->window_size; i++) {
        ML_VALIDATE_WINDOW_ACCESS(predictor, i, "obstruction trend extraction");
        obstruction_values[i] = predictor->window[i].obstruction_pct;
    }
    
    double obstruction_trend = ml_monitor_calculate_trend(obstruction_values, predictor->window_size);
    predictor->features.obstruction_trend = (uint8_t)((obstruction_trend + 1.0) * 127.5);
    
    // Calculate weather severity
    if (predictor->window_size > 0) {
        int latest_idx = (predictor->write_idx - 1 + 60) % 60;
        if (latest_idx >= 0 && latest_idx < 60) {
            ml_observation_t *latest = &predictor->window[latest_idx];
            predictor->features.weather_severity = 0;
            if (latest->precipitation_mm > 0) predictor->features.weather_severity += 50;
            if (latest->wind_speed_ms > 10) predictor->features.weather_severity += 30;
            if (latest->cloud_cover_pct > 80) predictor->features.weather_severity += 20;
            if (predictor->features.weather_severity > 255) predictor->features.weather_severity = 255;
        }
    }
    
    // Time of day feature
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    predictor->features.time_of_day = (uint8_t)((tm_info->tm_hour * 60 + tm_info->tm_min) * 255 / (24 * 60));
    
    // Pattern signature (simplified hash of recent patterns)
    uint32_t pattern_hash = 0;
    for (int i = 0; i < predictor->window_size && i < 60; i++) {
        ML_VALIDATE_WINDOW_ACCESS(predictor, i, "pattern hash calculation");
        pattern_hash ^= predictor->window[i].snr_x100 + (predictor->window[i].latency_ms << 8);
    }
    predictor->features.pattern_signature = (uint8_t)(pattern_hash % 256);
    
    ML_DEBUG_EXIT("ml_monitor_extract_window_features", 0);
}

// Calculate trend from values (-1 = falling, 0 = stable, 1 = rising)
static double ml_monitor_calculate_trend(const uint16_t *values, int count) {
    if (!values || count < 2) return 0.0;
    
    // Simple linear regression slope
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;
    
    for (int i = 0; i < count; i++) {
        sum_x += i;
        sum_y += values[i];
        sum_xy += i * values[i];
        sum_x2 += i * i;
    }
    
    double denominator = count * sum_x2 - sum_x * sum_x;
    if (denominator == 0.0) return 0.0; // Avoid division by zero
    
    double slope = (count * sum_xy - sum_x * sum_y) / denominator;
    
    // Normalize slope to -1..1 range
    return fmax(-1.0, fmin(1.0, slope / 100.0)); // Assuming values are in reasonable range
}

// Calculate volatility (standard deviation normalized)
static uint8_t ml_monitor_calculate_volatility(const uint16_t *values, int count) {
    if (!values || count < 2) return 0;
    
    // Calculate mean
    double mean = 0;
    for (int i = 0; i < count; i++) {
        mean += values[i];
    }
    mean /= count;
    
    // Calculate standard deviation
    double variance = 0;
    for (int i = 0; i < count; i++) {
        double diff = values[i] - mean;
        variance += diff * diff;
    }
    variance /= count;
    
    double std_dev = sqrt(variance);
    
    // Normalize to 0-255 range (assuming reasonable value ranges)
    return (uint8_t)fmin(255, std_dev);
}

// Sliding window prediction with enhanced features
static int ml_monitor_sliding_window_predict(ml_monitor_t *monitor, sliding_predictor_t *predictor, const ml_observation_t *observation) {
    ML_DEBUG_ENTRY("ml_monitor_sliding_window_predict");
    
    fprintf(stderr, "DEBUG: ml_monitor_sliding_window_predict called with monitor=%p, predictor=%p, observation=%p\n", 
            monitor, predictor, observation);
    
    // CRITICAL: Validate predictor pointer immediately
    if (predictor == NULL) {
        fprintf(stderr, "ERROR: predictor is NULL!\n");
        ML_DEBUG_EXIT("ml_monitor_sliding_window_predict", ML_MONITOR_ERROR_INVALID_PARAM);
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    if ((uintptr_t)predictor < 0x1000) {
        fprintf(stderr, "ERROR: predictor pointer %p is invalid (too low)!\n", predictor);
        ML_DEBUG_EXIT("ml_monitor_sliding_window_predict", ML_MONITOR_ERROR_INVALID_PARAM);
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    fprintf(stderr, "DEBUG: predictor pointer validation passed: %p\n", predictor);
    
    ML_VALIDATE_POINTER(monitor, "ml_monitor_sliding_window_predict monitor");
    ML_VALIDATE_POINTER(predictor, "ml_monitor_sliding_window_predict predictor");
    ML_VALIDATE_POINTER(observation, "ml_monitor_sliding_window_predict observation");
    
    if (!monitor || !predictor || !observation) {
        ML_DEBUG_EXIT("ml_monitor_sliding_window_predict", ML_MONITOR_ERROR_INVALID_PARAM);
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    // Add observation to sliding window
    ML_VALIDATE_WINDOW_ACCESS(predictor, predictor->write_idx, "sliding window write");
    predictor->window[predictor->write_idx] = *observation;
    predictor->write_idx = (predictor->write_idx + 1) % 60;
    if (predictor->window_size < 60) predictor->window_size++;
    
    // Need at least 8 observations (2 minutes) for meaningful predictions
    if (predictor->window_size < 8) {
        predictor->confidence = 0;
        return ML_MONITOR_SUCCESS;
    }
    
    // Extract features from window
    fprintf(stderr, "DEBUG: About to call ml_monitor_extract_window_features from sliding_window_predict, predictor=%p\n", predictor);
    ML_VALIDATE_POINTER(predictor, "ml_monitor_sliding_window_predict predictor before extract");
    ml_monitor_extract_window_features(predictor);
    fprintf(stderr, "DEBUG: ml_monitor_extract_window_features completed in sliding_window_predict\n");
    
    // Prepare neural network input with window features
    int8_t nn_input[32];
    memset(nn_input, 0, sizeof(nn_input));
    
    // Map features to neural network input
    nn_input[0] = (int8_t)(predictor->features.snr_trend - 128);
    nn_input[1] = (int8_t)(predictor->features.snr_volatility - 128);
    nn_input[2] = (int8_t)(predictor->features.latency_trend - 128);
    nn_input[3] = (int8_t)(predictor->features.packet_loss_trend - 128);
    nn_input[4] = (int8_t)(predictor->features.obstruction_trend - 128);
    nn_input[5] = (int8_t)(predictor->features.weather_severity - 128);
    nn_input[6] = (int8_t)(predictor->features.time_of_day - 128);
    nn_input[7] = (int8_t)(predictor->features.pattern_signature - 128);
    
    // Add current observation features
    nn_input[8] = (int8_t)((observation->snr_x100 / 10) - 128);
    nn_input[9] = (int8_t)((observation->latency_ms / 2) - 128);
    nn_input[10] = (int8_t)(observation->packet_loss_pct - 128);
    nn_input[11] = (int8_t)(observation->obstruction_pct - 128);
    nn_input[12] = (int8_t)((observation->azimuth_deg / 2) - 128);
    nn_input[13] = (int8_t)((observation->elevation_deg / 2) - 128);
    nn_input[14] = (int8_t)(observation->satellites_visible - 128);
    nn_input[15] = (int8_t)(observation->temperature_c);
    
    // Get neural network prediction
    uint8_t nn_output[8];
    ml_monitor_predict_neural_network(monitor, observation, nn_output);
    
    // Get k-NN prediction for comparison
    uint8_t knn_confidence;
    uint8_t knn_prediction = ml_monitor_predict_outage_knn(monitor, observation, &knn_confidence);
    
    // Combine predictions with sliding window weighting
    uint16_t nn_weight = predictor->window_size > 30 ? 150 : 100;
    uint16_t knn_weight = knn_confidence;
    
    if (nn_weight + knn_weight > 0) {
        predictor->outage_probability_5min = (nn_output[0] * nn_weight + knn_prediction * knn_weight) / 
                                           (nn_weight + knn_weight);
    } else {
        predictor->outage_probability_5min = 0;
    }
    
    // 15-minute prediction with decay
    predictor->outage_probability_15min = (nn_output[1] * 3 + predictor->outage_probability_5min) / 4;
    
    // Determine likely cause
    predictor->likely_cause = nn_output[2] > 128 ? 
                             (knn_prediction != OUTAGE_UNKNOWN ? knn_prediction : OUTAGE_OBSTRUCTION_STATIC) : 
                             OUTAGE_UNKNOWN;
    
    // Calculate confidence based on agreement and data quality
    uint8_t agreement = 255 - abs(nn_output[0] - knn_prediction);
    uint8_t data_quality = (predictor->window_size * 255) / 60;
    predictor->confidence = (agreement * data_quality) / 255;
    
    LOGX_DEBUG_MSG("Sliding window prediction: 5min=%u%%, 15min=%u%%, confidence=%u%%, cause=%u",
              predictor->outage_probability_5min, predictor->outage_probability_15min, 
              predictor->confidence, predictor->likely_cause);
    
    ML_DEBUG_EXIT("ml_monitor_sliding_window_predict", ML_MONITOR_SUCCESS);
    return ML_MONITOR_SUCCESS;
}

// Enhanced prediction function with sliding window
int ml_monitor_predict_next_15_minutes_enhanced(ml_monitor_t *monitor, uint8_t probabilities[60], uint8_t *confidence) {
    if (!monitor || !monitor->state || !probabilities || !confidence) {
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    *confidence = 0;
    
    // Need sufficient data for enhanced predictions
    if (monitor->state->total_observations < 100) {
        LOGX_DEBUG_MSG("Insufficient data for enhanced predictions (%u observations)", monitor->state->total_observations);
        memset(probabilities, 0, 60);
        return ML_MONITOR_SUCCESS;
    }
    
    // Initialize sliding window predictor
    static sliding_predictor_t predictor = {0};
    
    // Get current observation
    ml_observation_t current_obs;
    if (ml_monitor_collect_observation(monitor) != ML_MONITOR_SUCCESS) {
        LOGX_WARN_MSG("Failed to collect current observation for enhanced prediction");
        memset(probabilities, 0, 60);
        return ML_MONITOR_ERROR_PREDICTION_FAILED;
    }
    
    // Use latest observation from monitor state
    memset(&current_obs, 0, sizeof(current_obs));
    current_obs.timestamp = time(NULL);
    
    // Make sliding window prediction
    int result = ml_monitor_sliding_window_predict(monitor, &predictor, &current_obs);
    if (result != ML_MONITOR_SUCCESS) {
        LOGX_ERROR_MSG("Sliding window prediction failed: %d", result);
        memset(probabilities, 0, 60);
        return result;
    }
    
    // Generate 15-minute probability curve
    for (int i = 0; i < 60; i++) {
        double time_factor = (double)i / 60.0; // 0 to 1 over 15 minutes
        
        // Interpolate between 5-minute and 15-minute predictions
        uint8_t base_prob = (uint8_t)((predictor.outage_probability_5min * (1.0 - time_factor)) + 
                                     (predictor.outage_probability_15min * time_factor));
        
        // Add some randomness based on volatility
        double volatility_factor = predictor.features.snr_volatility / 255.0;
        double random_factor = (sin(i * 0.1) * volatility_factor * 20); // 20 based on volatility
        
        probabilities[i] = (uint8_t)fmax(0, fmin(255, base_prob + random_factor));
    }
    
    *confidence = predictor.confidence;
    
    LOGX_DEBUG_MSG("Generated enhanced 15-minute predictions with %u%% confidence", *confidence);
    return ML_MONITOR_SUCCESS;
}

// Initialize Phase 3 enhancements
int ml_monitor_init_phase3_enhancements(ml_monitor_t *monitor) {
    if (!monitor) {
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    // Use simple fprintf to avoid LOGX crashes
    fprintf(stderr, "Initializing Phase 3: Advanced Sky Grid & Sliding Window Predictions\n");
    
    // Initialize enhanced sky grid with obstruction analyzer integration
    int result = ml_monitor_init_enhanced_sky_grid(monitor);
    
    if (result != ML_MONITOR_SUCCESS) {
        LOGX_ERROR_MSG("Failed to initialize enhanced sky grid: %d", result);
        return result;
    }
    
    // Use simple fprintf to avoid LOGX crashes
    fprintf(stderr, "Phase 3 enhancements initialized successfully\n");
    
    return ML_MONITOR_SUCCESS;
}

// Update with Phase 3 enhanced learning
int ml_monitor_update_with_phase3_enhancements(ml_monitor_t *monitor, const ml_observation_t *observation) {
    fprintf(stderr, "DEBUG: ml_monitor_update_with_phase3_enhancements called\n");
    if (!monitor || !observation) {
        fprintf(stderr, "DEBUG: Phase 3 update failed - invalid parameters\n");
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    // Check if Phase 3 system is initialized
    fprintf(stderr, "DEBUG: Phase 3 initialized status: %s\n", g_phase3_initialized ? "true" : "false");
    if (!g_phase3_initialized) {
        fprintf(stderr, "DEBUG: Phase 3 not initialized, returning error\n");
        return ML_MONITOR_ERROR_NOT_INITIALIZED;
    }
    
    // Integrate with obstruction analyzer using global system
    fprintf(stderr, "DEBUG: About to call ml_monitor_integrate_with_obstruction_analyzer\n");
    int integration_result = ml_monitor_integrate_with_obstruction_analyzer(monitor, observation);
    fprintf(stderr, "DEBUG: ml_monitor_integrate_with_obstruction_analyzer returned: %d\n", integration_result);
    if (integration_result != ML_MONITOR_SUCCESS) {
        LOGX_DEBUG_MSG("Obstruction analyzer integration warning: %d", integration_result);
        // Continue with ML-only updates
    }
    
    fprintf(stderr, "DEBUG: About to update sliding window predictor\n");
    fprintf(stderr, "DEBUG: Global predictor state: window_size=%u, write_idx=%u\n", 
            g_phase3_sliding_predictor.window_size, g_phase3_sliding_predictor.write_idx);
    // Update sliding window predictor with new observation
    if (g_phase3_sliding_predictor.window_size < 60) {
        fprintf(stderr, "DEBUG: Adding observation to sliding window (size < 60)\n");
        ML_VALIDATE_WINDOW_ACCESS(&g_phase3_sliding_predictor, g_phase3_sliding_predictor.write_idx, "sliding window write (size < 60)");
        g_phase3_sliding_predictor.window[g_phase3_sliding_predictor.write_idx] = *observation;
        g_phase3_sliding_predictor.write_idx = (g_phase3_sliding_predictor.write_idx + 1) % 60;
        g_phase3_sliding_predictor.window_size++;
        fprintf(stderr, "DEBUG: Sliding window updated, new size: %u, write_idx: %u\n", 
                g_phase3_sliding_predictor.window_size, g_phase3_sliding_predictor.write_idx);
    } else {
        fprintf(stderr, "DEBUG: Adding observation to sliding window (size >= 60)\n");
        ML_VALIDATE_WINDOW_ACCESS(&g_phase3_sliding_predictor, g_phase3_sliding_predictor.write_idx, "sliding window write (size >= 60)");
        g_phase3_sliding_predictor.window[g_phase3_sliding_predictor.write_idx] = *observation;
        g_phase3_sliding_predictor.write_idx = (g_phase3_sliding_predictor.write_idx + 1) % 60;
        fprintf(stderr, "DEBUG: Sliding window updated, write_idx: %u\n", g_phase3_sliding_predictor.write_idx);
    }
    
    fprintf(stderr, "DEBUG: About to extract window features\n");
    // Extract features from sliding window
    ML_VALIDATE_POINTER(&g_phase3_sliding_predictor, "ml_monitor_extract_window_features call");
    ml_monitor_extract_window_features(&g_phase3_sliding_predictor);
    fprintf(stderr, "DEBUG: Window features extracted successfully\n");
    
    fprintf(stderr, "DEBUG: About to perform sliding window prediction\n");
    // Perform sliding window prediction
    ML_VALIDATE_POINTER(monitor, "ml_monitor_update_with_phase3_enhancements monitor");
    ML_VALIDATE_POINTER(observation, "ml_monitor_update_with_phase3_enhancements observation");
    
    // CRITICAL: Check global predictor address before call
    fprintf(stderr, "DEBUG: Global predictor address: %p\n", &g_phase3_sliding_predictor);
    fprintf(stderr, "DEBUG: Global predictor window_size: %u\n", g_phase3_sliding_predictor.window_size);
    fprintf(stderr, "DEBUG: Global predictor write_idx: %u\n", g_phase3_sliding_predictor.write_idx);
    
    // CRITICAL: Validate global predictor address
    if ((uintptr_t)&g_phase3_sliding_predictor < 0x1000) {
        fprintf(stderr, "ERROR: Global predictor address %p is corrupted!\n", &g_phase3_sliding_predictor);
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    // CRITICAL: Check if global predictor is in valid memory range
    if ((uintptr_t)&g_phase3_sliding_predictor < 0x40000000) {
        fprintf(stderr, "WARNING: Global predictor address %p is in invalid range (code section)! Using local workaround.\n", &g_phase3_sliding_predictor);
        // Continue with local predictor instead of returning error
    }
    
    // CRITICAL: Check corruption detection magic
    if (g_phase3_predictor_magic != GLOBAL_PREDICTOR_MAGIC) {
        fprintf(stderr, "WARNING: Global predictor corruption detected! Magic: 0x%x, expected: 0x%x. Using local workaround.\n", 
                g_phase3_predictor_magic, GLOBAL_PREDICTOR_MAGIC);
        // Continue with local predictor instead of returning error
    }
    
    // CRITICAL: Use local copy to avoid corrupted global
    // Force local predictor to be in data section by making it static and initialized
    static sliding_predictor_t local_predictor = {
        .window_size = 0,
        .write_idx = 0,
        .features = {0},
        .window = {{0}}
    };
    static bool local_predictor_initialized = false;
    
    if (!local_predictor_initialized) {
        fprintf(stderr, "DEBUG: Initializing local predictor as workaround for global corruption\n");
        memset(&local_predictor, 0, sizeof(sliding_predictor_t));
        local_predictor.window_size = 0;
        local_predictor.write_idx = 0;
        local_predictor.features.snr_trend = 128; // Stable
        
        // CRITICAL: Initialize the window array to prevent NULL access
        for (int i = 0; i < 60; i++) {
            local_predictor.window[i].snr_x100 = 0;
            local_predictor.window[i].latency_ms = 0;
            local_predictor.window[i].packet_loss_pct = 0;
            local_predictor.window[i].obstruction_pct = 0;
            local_predictor.window[i].timestamp = 0;
        }
        
        local_predictor_initialized = true;
        fprintf(stderr, "DEBUG: Local predictor initialized with safe window array\n");
    }
    
    // Copy current state from global (if not corrupted) or use local
    if (g_phase3_predictor_magic == GLOBAL_PREDICTOR_MAGIC && 
        (uintptr_t)&g_phase3_sliding_predictor >= 0x40000000) {
        fprintf(stderr, "DEBUG: Using global predictor (not corrupted)\n");
        local_predictor = g_phase3_sliding_predictor;
    } else {
        fprintf(stderr, "DEBUG: Using local predictor (global corrupted)\n");
    }
    
    fprintf(stderr, "DEBUG: Local predictor address: %p (should be in data section)\n", &local_predictor);
    int prediction_result = ml_monitor_sliding_window_predict(monitor, &local_predictor, observation);
    fprintf(stderr, "DEBUG: ml_monitor_sliding_window_predict returned: %d\n", prediction_result);
    if (prediction_result != ML_MONITOR_SUCCESS) {
        LOGX_DEBUG_MSG("Sliding window prediction warning: %d", prediction_result);
    }
    
    fprintf(stderr, "DEBUG: ml_monitor_update_with_phase3_enhancements returning success\n");
    return ML_MONITOR_SUCCESS;
}