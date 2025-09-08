#ifndef ML_SATELLITE_REDUNDANCY_H
#define ML_SATELLITE_REDUNDANCY_H

#include "ml_monitor.h"
#include "../starlink/starlink_types.h"
#include <time.h>
#include <stdint.h>
#include <stdbool.h>

// Satellite redundancy analysis configuration
typedef struct {
    // Thresholds for satellite count warnings
    uint8_t critical_threshold;           // Critical level (e.g., 2 satellites)
    uint8_t warning_threshold;            // Warning level (e.g., 4 satellites)
    uint8_t safe_threshold;               // Safe level (e.g., 6 satellites)
    uint8_t optimal_threshold;            // Optimal level (e.g., 8+ satellites)
    
    // Redundancy scoring parameters
    double redundancy_weight;             // Weight for redundancy in ML (0.0-1.0)
    double obstruction_penalty;           // Penalty for obstructed satellites
    double elevation_bonus;               // Bonus for high elevation satellites
    double diversity_bonus;               // Bonus for satellite diversity
    
    // Early warning settings
    bool enable_early_warning;            // Enable early warning system
    uint32_t warning_cooldown_seconds;    // Cooldown between warnings
    uint32_t prediction_horizon_seconds;  // How far ahead to predict
    
    // ML integration
    bool enable_ml_integration;           // Feed into ML algorithms
    double ml_feature_weight;             // Weight in ML feature vector
} satellite_redundancy_config_t;

// Satellite redundancy assessment
typedef struct {
    uint8_t total_visible;                // Total satellites visible
    uint8_t unobstructed_count;           // Unobstructed satellites
    uint8_t obstructed_count;             // Obstructed satellites
    uint8_t high_elevation_count;         // Satellites > 60° elevation
    uint8_t medium_elevation_count;       // Satellites 30-60° elevation
    uint8_t low_elevation_count;          // Satellites < 30° elevation
    
    // Redundancy metrics
    double redundancy_score;              // 0.0-1.0 redundancy score
    double diversity_score;               // 0.0-1.0 satellite diversity
    double elevation_score;               // 0.0-1.0 elevation distribution
    double obstruction_risk;              // 0.0-1.0 obstruction risk
    
    // Risk assessment
    uint8_t risk_level;                   // 1=low, 2=medium, 3=high, 4=critical
    bool early_warning_triggered;         // Whether early warning is active
    uint32_t time_to_critical_seconds;    // Estimated time to critical level
    
    // ML features
    uint8_t ml_redundancy_feature;        // 0-255 ML feature value
    uint8_t ml_risk_feature;              // 0-255 ML risk feature
    uint8_t ml_diversity_feature;         // 0-255 ML diversity feature
    
    time_t assessment_time;               // When assessment was made
} satellite_redundancy_assessment_t;

// Satellite redundancy predictor
typedef struct {
    satellite_redundancy_config_t config;
    satellite_redundancy_assessment_t current_assessment;
    satellite_redundancy_assessment_t last_assessment;
    
    // Historical data for prediction
    struct {
        uint8_t satellite_counts[60];     // Last 60 assessments (1 hour at 1/min)
        uint8_t obstruction_counts[60];   // Last 60 obstruction counts
        double redundancy_scores[60];     // Last 60 redundancy scores
        uint8_t write_index;              // Circular buffer index
        uint8_t count;                    // Number of valid entries
    } history;
    
    // Prediction state
    bool prediction_active;               // Whether prediction is active
    time_t last_prediction_time;          // Last prediction timestamp
    uint32_t predicted_critical_time;     // Predicted time to critical (seconds)
    double prediction_confidence;         // Prediction confidence (0.0-1.0)
    
    // Early warning state
    bool early_warning_active;            // Whether early warning is active
    time_t last_warning_time;             // Last warning timestamp
    uint32_t warning_count;               // Number of warnings issued
    
    // Callbacks
    void (*early_warning_callback)(const satellite_redundancy_assessment_t *assessment, void *user_data);
    void (*critical_threshold_callback)(const satellite_redundancy_assessment_t *assessment, void *user_data);
    void *callback_user_data;
} satellite_redundancy_predictor_t;

// API Functions

// Initialization and cleanup
satellite_redundancy_predictor_t* satellite_redundancy_init(const satellite_redundancy_config_t *config);
void satellite_redundancy_cleanup(satellite_redundancy_predictor_t *predictor);

// Configuration
void satellite_redundancy_config_init_defaults(satellite_redundancy_config_t *config);
int satellite_redundancy_update_config(satellite_redundancy_predictor_t *predictor, const satellite_redundancy_config_t *config);

// Assessment and prediction
int satellite_redundancy_assess_current(satellite_redundancy_predictor_t *predictor, 
                                       const starlink_status_response_t *starlink_data);
int satellite_redundancy_predict_future(satellite_redundancy_predictor_t *predictor, 
                                       uint32_t horizon_seconds);
int satellite_redundancy_get_assessment(const satellite_redundancy_predictor_t *predictor, 
                                       satellite_redundancy_assessment_t *assessment);

// ML integration
int satellite_redundancy_get_ml_features(const satellite_redundancy_predictor_t *predictor, 
                                        uint8_t *redundancy_feature,
                                        uint8_t *risk_feature, 
                                        uint8_t *diversity_feature);
int satellite_redundancy_update_ml_observation(ml_observation_t *observation, 
                                              const satellite_redundancy_assessment_t *assessment);

// Early warning system
bool satellite_redundancy_is_early_warning_active(const satellite_redundancy_predictor_t *predictor);
int satellite_redundancy_trigger_early_warning(satellite_redundancy_predictor_t *predictor);
int satellite_redundancy_clear_early_warning(satellite_redundancy_predictor_t *predictor);

// Callback management
int satellite_redundancy_set_early_warning_callback(satellite_redundancy_predictor_t *predictor,
                                                   void (*callback)(const satellite_redundancy_assessment_t *assessment, void *user_data),
                                                   void *user_data);
int satellite_redundancy_set_critical_callback(satellite_redundancy_predictor_t *predictor,
                                              void (*callback)(const satellite_redundancy_assessment_t *assessment, void *user_data),
                                              void *user_data);

// Utility functions
double satellite_redundancy_calculate_score(uint8_t total_visible, uint8_t unobstructed, 
                                           uint8_t high_elevation, double obstruction_ratio);
uint8_t satellite_redundancy_get_risk_level(uint8_t total_visible, uint8_t unobstructed, 
                                           double obstruction_ratio);
bool satellite_redundancy_should_trigger_warning(const satellite_redundancy_assessment_t *current,
                                                const satellite_redundancy_assessment_t *previous,
                                                const satellite_redundancy_config_t *config);

// Error codes
#define SATELLITE_REDUNDANCY_SUCCESS                 0
#define SATELLITE_REDUNDANCY_ERROR_INVALID_PARAM    -1
#define SATELLITE_REDUNDANCY_ERROR_NOT_INITIALIZED  -2
#define SATELLITE_REDUNDANCY_ERROR_NO_DATA          -3
#define SATELLITE_REDUNDANCY_ERROR_PREDICTION_FAILED -4

#endif // ML_SATELLITE_REDUNDANCY_H
