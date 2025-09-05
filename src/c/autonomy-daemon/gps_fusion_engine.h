#ifndef GPS_FUSION_ENGINE_H
#define GPS_FUSION_ENGINE_H

#include "gps_comprehensive.h"
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Fusion methods
typedef enum {
    GPS_FUSION_METHOD_SINGLE_SOURCE = 0,
    GPS_FUSION_METHOD_WEIGHTED_AVERAGE,
    GPS_FUSION_METHOD_CONFIDENCE_WEIGHTED,
    GPS_FUSION_METHOD_KALMAN_FILTER,
    GPS_FUSION_METHOD_PARTICLE_FILTER,
    GPS_FUSION_METHOD_BAYESIAN,
    GPS_FUSION_METHOD_MAX
} gps_fusion_method_t;

// Weighted GPS source data
typedef struct {
    standardized_gps_data_t data;          // GPS data
    double weight;                         // Calculated weight
    double accuracy_factor;                // Accuracy-based factor
    double confidence_factor;              // Confidence-based factor
    double freshness_factor;               // Freshness-based factor
    double health_factor;                  // Source health factor
    double priority_factor;                // Source priority factor
    char weight_reasoning[256];            // Reasoning for weight calculation
} weighted_gps_data_t;

// Fusion engine configuration
typedef struct {
    bool enabled;                          // Enable fusion engine
    gps_fusion_method_t default_method;    // Default fusion method
    
    // Weighting factors
    double accuracy_weight;                // Weight for accuracy (0.0-1.0)
    double confidence_weight;              // Weight for confidence (0.0-1.0)
    double freshness_weight;               // Weight for data freshness (0.0-1.0)
    double health_weight;                  // Weight for source health (0.0-1.0)
    double priority_weight;                // Weight for source priority (0.0-1.0)
    
    // Fusion thresholds
    double min_sources_for_fusion;         // Minimum sources for fusion
    double max_accuracy_difference;        // Max accuracy difference for fusion
    double max_distance_difference;        // Max distance difference for fusion
    double max_time_difference_s;          // Max time difference for fusion
    
    // Quality control
    double outlier_detection_threshold;    // Threshold for outlier detection
    double consensus_threshold;            // Threshold for consensus
    double min_fusion_confidence;          // Minimum confidence for fusion result
    
    // Advanced features
    bool enable_outlier_detection;         // Enable outlier detection
    bool enable_consensus_checking;        // Enable consensus checking
    bool enable_temporal_smoothing;        // Enable temporal smoothing
    bool enable_kalman_filtering;          // Enable Kalman filtering
    
    // Kalman filter parameters
    double process_noise_covariance;       // Process noise covariance
    double measurement_noise_covariance;   // Measurement noise covariance
    double initial_uncertainty;            // Initial uncertainty
} gps_fusion_config_t;

// Fusion statistics
typedef struct {
    uint64_t total_fusions;                // Total fusion operations
    uint64_t successful_fusions;           // Successful fusions
    uint64_t failed_fusions;               // Failed fusions
    uint64_t outliers_detected;            // Outliers detected and removed
    uint64_t consensus_failures;           // Consensus check failures
    
    double average_fusion_time_ms;         // Average fusion time
    double average_sources_per_fusion;     // Average sources per fusion
    double average_fusion_accuracy;        // Average fusion accuracy
    double average_fusion_confidence;      // Average fusion confidence
    
    uint64_t method_usage[GPS_FUSION_METHOD_MAX]; // Usage count per method
    
    time_t last_fusion;                    // Last fusion time
    time_t stats_reset_time;               // When stats were reset
} gps_fusion_statistics_t;

// Fusion engine structure
typedef struct {
    gps_fusion_config_t config;            // Configuration
    gps_fusion_statistics_t stats;         // Statistics
    
    // Kalman filter state (if enabled)
    double kalman_state[4];                // [lat, lon, lat_velocity, lon_velocity]
    double kalman_covariance[16];          // 4x4 covariance matrix
    bool kalman_initialized;               // Kalman filter initialization status
    
    // Temporal smoothing state
    standardized_gps_data_t smoothing_history[10]; // History for smoothing
    int smoothing_history_count;           // Number of entries in history
    
    // Threading
    pthread_mutex_t mutex;                 // Fusion engine mutex
    
    // State
    bool initialized;                      // Initialization status
    time_t last_fusion_time;               // Last fusion operation
    standardized_gps_data_t last_result;   // Last fusion result
} gps_fusion_engine_t;

// Function prototypes

/**
 * Initialize GPS fusion engine
 * @param config Fusion engine configuration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_fusion_engine_init(const gps_fusion_config_t* config);

/**
 * Cleanup GPS fusion engine
 */
void gps_fusion_engine_cleanup(void);

/**
 * Fuse GPS data from multiple sources
 * @param source_data Array of GPS data from different sources
 * @param source_count Number of sources
 * @param result Fused GPS data result
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_fusion_engine_fuse(const standardized_gps_data_t* source_data, int source_count,
                          standardized_gps_data_t* result);

/**
 * Fuse GPS data using specific method
 * @param source_data Array of GPS data from different sources
 * @param source_count Number of sources
 * @param method Fusion method to use
 * @param result Fused GPS data result
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_fusion_engine_fuse_with_method(const standardized_gps_data_t* source_data, int source_count,
                                      gps_fusion_method_t method, standardized_gps_data_t* result);

/**
 * Detect and remove outliers from GPS data
 * @param source_data Array of GPS data
 * @param source_count Number of sources
 * @param filtered_data Array for filtered data
 * @param max_filtered Maximum filtered entries
 * @return Number of sources after outlier removal
 */
int gps_fusion_engine_detect_outliers(const standardized_gps_data_t* source_data, int source_count,
                                     standardized_gps_data_t* filtered_data, int max_filtered);

/**
 * Check consensus among GPS sources
 * @param source_data Array of GPS data
 * @param source_count Number of sources
 * @return true if consensus exists, false otherwise
 */
bool gps_fusion_engine_check_consensus(const standardized_gps_data_t* source_data, int source_count);

/**
 * Apply temporal smoothing to GPS data
 * @param current_data Current GPS data
 * @param smoothed_result Smoothed GPS data result
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_fusion_engine_apply_smoothing(const standardized_gps_data_t* current_data,
                                     standardized_gps_data_t* smoothed_result);

/**
 * Apply Kalman filtering to GPS data
 * @param measurement GPS measurement data
 * @param filtered_result Kalman filtered result
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_fusion_engine_apply_kalman_filter(const standardized_gps_data_t* measurement,
                                         standardized_gps_data_t* filtered_result);

/**
 * Get fusion engine statistics
 * @param stats Statistics structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_fusion_engine_get_statistics(gps_fusion_statistics_t* stats);

/**
 * Reset fusion engine statistics
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_fusion_engine_reset_statistics(void);

/**
 * Check if fusion engine is initialized
 * @return true if initialized, false otherwise
 */
bool gps_fusion_engine_is_initialized(void);

/**
 * Get fusion engine configuration
 * @param config Configuration structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_fusion_engine_get_config(gps_fusion_config_t* config);

/**
 * Set fusion engine configuration
 * @param config New configuration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_fusion_engine_set_config(const gps_fusion_config_t* config);

#ifdef __cplusplus
}
#endif

#endif // GPS_FUSION_ENGINE_H