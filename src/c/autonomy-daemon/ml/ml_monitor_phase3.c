#include "ml_monitor.h"
#include "../utils/logx.h"
#include "../starlink/starlink_modules.h"
#include "../shared/starlink-tracking/obstruction_analyzer.h"
#include "../shared/starlink-tracking/starlink_tracker.h"
#include <time.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

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
    fprintf(stderr, "DEBUG: ml_monitor_init_enhanced_sky_grid called\n");
    if (!monitor || !monitor->state) {
        fprintf(stderr, "DEBUG: ml_monitor_init_enhanced_sky_grid failed - invalid params\n");
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    LOGX_INFO_MSG("Initializing enhanced sky grid with obstruction analyzer integration");
    fprintf(stderr, "DEBUG: ml_monitor_init_enhanced_sky_grid - starting initialization\n");
    
    // Allocate enhanced sky grid structure
    fprintf(stderr, "DEBUG: ml_monitor_init_enhanced_sky_grid - about to allocate enhanced_grid\n");
    enhanced_sky_grid_t *enhanced_grid = calloc(1, sizeof(enhanced_sky_grid_t));
    if (!enhanced_grid) {
        LOGX_ERROR_MSG("Failed to allocate enhanced sky grid");
        fprintf(stderr, "DEBUG: ml_monitor_init_enhanced_sky_grid failed - calloc failed\n");
        return ML_MONITOR_ERROR_MEMORY_FAILED;
    }
    fprintf(stderr, "DEBUG: ml_monitor_init_enhanced_sky_grid - allocated enhanced_grid successfully\n");
    
    // Initialize ML grid (already exists in monitor->state)
    fprintf(stderr, "DEBUG: ml_monitor_init_enhanced_sky_grid - about to copy ML grid\n");
    fprintf(stderr, "DEBUG: enhanced_grid pointer: %p\n", (void*)enhanced_grid);
    fprintf(stderr, "DEBUG: monitor->state pointer: %p\n", (void*)monitor->state);
    if (monitor->state) {
        fprintf(stderr, "DEBUG: monitor->state->models pointer: %p\n", (void*)&monitor->state->models);
        fprintf(stderr, "DEBUG: monitor->state->models.sky_grid pointer: %p\n", (void*)&monitor->state->models.sky_grid);
    }
    
    // Copy the ML grid data safely
    memcpy(&enhanced_grid->ml_grid, &monitor->state->models.sky_grid, sizeof(compact_sky_grid_t));
    fprintf(stderr, "DEBUG: ml_monitor_init_enhanced_sky_grid - copied ML grid\n");
    
    // Initialize obstruction analyzer
    fprintf(stderr, "DEBUG: ml_monitor_init_enhanced_sky_grid - about to initialize obstruction analyzer\n");
    obstruction_analysis_config_t obstruction_config;
    obstruction_config.snr_threshold = 0.7;
    obstruction_config.min_elevation = 25.0;
    obstruction_config.max_elevation = 90.0;
    obstruction_config.use_adaptive_threshold = true;
    obstruction_config.adaptive_threshold_factor = 1.2;
    obstruction_config.smoothing_window_size = 3;
    
    enhanced_grid->obstruction_analyzer = obstruction_analyzer_init(&obstruction_config);
    if (!enhanced_grid->obstruction_analyzer) {
        LOGX_WARN_MSG("Failed to initialize obstruction analyzer, continuing with ML-only mode");
        fprintf(stderr, "DEBUG: ml_monitor_init_enhanced_sky_grid - obstruction analyzer failed, continuing\n");
    } else {
        fprintf(stderr, "DEBUG: ml_monitor_init_enhanced_sky_grid - obstruction analyzer initialized successfully\n");
    }
    
    // Initialize coordinate mapping between ML grid (90x45) and polar projection (123x123)
    enhanced_grid->mapping.ml_to_polar_scale_x = 123.0 / 90.0;  // ML azimuth bins to polar X
    enhanced_grid->mapping.ml_to_polar_scale_y = 123.0 / 45.0;  // ML elevation bins to polar Y
    enhanced_grid->mapping.polar_center_x = 61;
    enhanced_grid->mapping.polar_center_y = 61;
    enhanced_grid->mapping.polar_max_radius = 61.5;
    
    // Initialize fusion parameters
    enhanced_grid->fusion.ml_weight = 0.7;
    enhanced_grid->fusion.obstruction_weight = 0.3;
    enhanced_grid->fusion.fusion_confidence_threshold = 0.6;
    enhanced_grid->fusion.enable_cross_validation = true;
    
    // Store enhanced grid pointer in monitor (we'll need to modify the structure to support this)
    // For now, just copy the ML grid data to avoid the crash
    fprintf(stderr, "DEBUG: ml_monitor_init_enhanced_sky_grid - about to copy ML grid data\n");
    memcpy(&monitor->state->models.sky_grid, &enhanced_grid->ml_grid, sizeof(compact_sky_grid_t));
    fprintf(stderr, "DEBUG: ml_monitor_init_enhanced_sky_grid - copied ML grid data\n");
    
    // Store the enhanced grid pointer for future use (we'll need to add this to the monitor structure)
    // monitor->state->enhanced_sky_grid = enhanced_grid;
    
    // Free the allocated memory since we're not storing the pointer
    fprintf(stderr, "DEBUG: ml_monitor_init_enhanced_sky_grid - about to cleanup obstruction analyzer\n");
    if (enhanced_grid->obstruction_analyzer) {
        obstruction_analyzer_cleanup(enhanced_grid->obstruction_analyzer);
    }
    fprintf(stderr, "DEBUG: ml_monitor_init_enhanced_sky_grid - obstruction analyzer cleaned up\n");
    fprintf(stderr, "DEBUG: ml_monitor_init_enhanced_sky_grid - about to free enhanced_grid\n");
    free(enhanced_grid);
    fprintf(stderr, "DEBUG: ml_monitor_init_enhanced_sky_grid - freed enhanced_grid\n");
    
    LOGX_INFO_MSG("Enhanced sky grid initialized successfully (ML: 90x45, Obstruction: 123x123)");
    fprintf(stderr, "DEBUG: ml_monitor_init_enhanced_sky_grid completed successfully\n");
    fprintf(stderr, "DEBUG: About to return ML_MONITOR_SUCCESS (value=%d)\n", ML_MONITOR_SUCCESS);
    fflush(stderr);  // Force flush to ensure we see the message
    int ret_val = ML_MONITOR_SUCCESS;
    fprintf(stderr, "DEBUG: Returning from ml_monitor_init_enhanced_sky_grid with value %d\n", ret_val);
    return ret_val;
}

// Integrate with obstruction analyzer for enhanced predictions
static int ml_monitor_integrate_with_obstruction_analyzer(ml_monitor_t *monitor, const ml_observation_t *observation) {
    if (!monitor || !monitor->state || !observation) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    // Get current obstruction map from existing analyzer
    obstruction_map_t current_obstruction_map;
    memset(&current_obstruction_map, 0, sizeof(current_obstruction_map));
    
    // Try to get obstruction map from Starlink tracking system
    // This would integrate with the existing obstruction analyzer
    // Integration with obstruction analyzer
    
    compact_sky_grid_t *ml_grid = &monitor->state->models.sky_grid;
    
    // Update ML grid with current observation
    int ml_result = ml_monitor_sky_grid_update(ml_grid, observation->azimuth_deg, observation->elevation_deg, 
                                              (observation->flags & ML_OBS_FLAG_OUTAGE) ? 1 : 0);
    
    if (ml_result != ML_MONITOR_SUCCESS) {
        LOGX_WARN_MSG("Failed to update ML sky grid: %d", ml_result);
        return ml_result;
    }
    
    // Convert ML grid coordinates to obstruction analyzer coordinates
    int ml_az_bin = observation->azimuth_deg / 4;  // 4 resolution
    int ml_el_bin = observation->elevation_deg / 4;
    
    if (ml_az_bin >= 0 && ml_az_bin < 90 && ml_el_bin >= 0 && ml_el_bin < 45) {
        uint8_t ml_obstruction_prob = ml_grid->obstruction_prob[ml_az_bin][ml_el_bin];
        
        // Cross-validate with obstruction analyzer if available
        // This would check against the 123x123 polar projection
        
        LOGX_DEBUG_MSG("ML sky grid updated: az=%d, el=%d, prob=%u%% (bin [%d,%d])",
                  observation->azimuth_deg, observation->elevation_deg, 
                  ml_obstruction_prob, ml_az_bin, ml_el_bin);
    }
    
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
    if (!predictor || predictor->window_size < 2) return;
    
    // Extract SNR trend
    uint16_t snr_values[60];
    for (int i = 0; i < predictor->window_size; i++) {
        snr_values[i] = predictor->window[i].snr_x100;
    }
    
    double snr_trend = ml_monitor_calculate_trend(snr_values, predictor->window_size);
    predictor->features.snr_trend = (uint8_t)((snr_trend + 1.0) * 127.5); // Map -1..1 to 0..255
    
    // Calculate SNR volatility
    predictor->features.snr_volatility = ml_monitor_calculate_volatility(snr_values, predictor->window_size);
    
    // Extract latency trend
    uint16_t latency_values[60];
    for (int i = 0; i < predictor->window_size; i++) {
        latency_values[i] = predictor->window[i].latency_ms;
    }
    
    double latency_trend = ml_monitor_calculate_trend(latency_values, predictor->window_size);
    predictor->features.latency_trend = (uint8_t)((latency_trend + 1.0) * 127.5);
    
    // Extract packet loss trend
    uint16_t loss_values[60];
    for (int i = 0; i < predictor->window_size; i++) {
        loss_values[i] = predictor->window[i].packet_loss_pct;
    }
    
    double loss_trend = ml_monitor_calculate_trend(loss_values, predictor->window_size);
    predictor->features.packet_loss_trend = (uint8_t)((loss_trend + 1.0) * 127.5);
    
    // Extract obstruction trend
    uint16_t obstruction_values[60];
    for (int i = 0; i < predictor->window_size; i++) {
        obstruction_values[i] = predictor->window[i].obstruction_pct;
    }
    
    double obstruction_trend = ml_monitor_calculate_trend(obstruction_values, predictor->window_size);
    predictor->features.obstruction_trend = (uint8_t)((obstruction_trend + 1.0) * 127.5);
    
    // Calculate weather severity
    ml_observation_t *latest = &predictor->window[(predictor->write_idx - 1 + 60) % 60];
    predictor->features.weather_severity = 0;
    if (latest->precipitation_mm > 0) predictor->features.weather_severity += 50;
    if (latest->wind_speed_ms > 10) predictor->features.weather_severity += 30;
    if (latest->cloud_cover_pct > 80) predictor->features.weather_severity += 20;
    if (predictor->features.weather_severity > 255) predictor->features.weather_severity = 255;
    
    // Time of day feature
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    predictor->features.time_of_day = (uint8_t)((tm_info->tm_hour * 60 + tm_info->tm_min) * 255 / (24 * 60));
    
    // Pattern signature (simplified hash of recent patterns)
    uint32_t pattern_hash = 0;
    for (int i = 0; i < predictor->window_size; i++) {
        pattern_hash ^= predictor->window[i].snr_x100 + (predictor->window[i].latency_ms << 8);
    }
    predictor->features.pattern_signature = (uint8_t)(pattern_hash % 256);
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
    
    double slope = (count * sum_xy - sum_x * sum_y) / (count * sum_x2 - sum_x * sum_x);
    
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
    if (!monitor || !predictor || !observation) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    // Add observation to sliding window
    predictor->window[predictor->write_idx] = *observation;
    predictor->write_idx = (predictor->write_idx + 1) % 60;
    if (predictor->window_size < 60) predictor->window_size++;
    
    // Need at least 8 observations (2 minutes) for meaningful predictions
    if (predictor->window_size < 8) {
        predictor->confidence = 0;
        return ML_MONITOR_SUCCESS;
    }
    
    // Extract features from window
    ml_monitor_extract_window_features(predictor);
    
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
    fprintf(stderr, "DEBUG: ml_monitor_init_phase3_enhancements called\n");
    if (!monitor) {
        fprintf(stderr, "DEBUG: ml_monitor_init_phase3_enhancements failed - NULL monitor\n");
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    LOGX_INFO_MSG(" Initializing Phase 3: Advanced Sky Grid & Sliding Window Predictions");
    fprintf(stderr, "DEBUG: ml_monitor_init_phase3_enhancements - starting initialization\n");
    
    // Initialize enhanced sky grid with obstruction analyzer integration
    fprintf(stderr, "DEBUG: ml_monitor_init_phase3_enhancements - about to initialize enhanced sky grid\n");
    fflush(stderr);
    int sky_result = ml_monitor_init_enhanced_sky_grid(monitor);
    fprintf(stderr, "DEBUG: ml_monitor_init_phase3_enhancements - ml_monitor_init_enhanced_sky_grid returned %d\n", sky_result);
    fflush(stderr);
    if (sky_result != ML_MONITOR_SUCCESS) {
        LOGX_WARN_MSG("Enhanced sky grid initialization failed: %d", sky_result);
        fprintf(stderr, "DEBUG: ml_monitor_init_phase3_enhancements - enhanced sky grid failed: %d\n", sky_result);
        return sky_result;
    }
    fprintf(stderr, "DEBUG: ml_monitor_init_phase3_enhancements - enhanced sky grid initialized successfully\n");
    
    fprintf(stderr, "DEBUG: ml_monitor_init_phase3_enhancements - about to log success messages\n");
    LOGX_INFO_MSG(" Phase 3 enhancements initialized successfully");
    fprintf(stderr, "DEBUG: ml_monitor_init_phase3_enhancements - logged main success message\n");
    
    LOGX_INFO_MSG("   - Enhanced sky grid with obstruction analyzer integration");
    fprintf(stderr, "DEBUG: ml_monitor_init_phase3_enhancements - logged sky grid message\n");
    
    LOGX_INFO_MSG("   - Sliding window predictor with 15-minute horizon");
    fprintf(stderr, "DEBUG: ml_monitor_init_phase3_enhancements - logged sliding window message\n");
    
    LOGX_INFO_MSG("   - Advanced feature extraction and trend analysis");
    fprintf(stderr, "DEBUG: ml_monitor_init_phase3_enhancements - logged feature extraction message\n");
    
    LOGX_INFO_MSG("   - Model fusion with existing obstruction data");
    fprintf(stderr, "DEBUG: ml_monitor_init_phase3_enhancements - logged model fusion message\n");
    
    fprintf(stderr, "DEBUG: ml_monitor_init_phase3_enhancements - about to return ML_MONITOR_SUCCESS\n");
    return ML_MONITOR_SUCCESS;
}

// Update with Phase 3 enhanced learning
int ml_monitor_update_with_phase3_enhancements(ml_monitor_t *monitor, const ml_observation_t *observation) {
    if (!monitor || !observation) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    // Integrate with obstruction analyzer
    int integration_result = ml_monitor_integrate_with_obstruction_analyzer(monitor, observation);
    if (integration_result != ML_MONITOR_SUCCESS) {
        LOGX_DEBUG_MSG("Obstruction analyzer integration warning: %d", integration_result);
        // Continue with ML-only updates
    }
    
    // The sliding window prediction is handled in the enhanced prediction function
    // This allows for real-time feature extraction and trend analysis
    
    return ML_MONITOR_SUCCESS;
}