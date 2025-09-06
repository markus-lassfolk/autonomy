#ifndef PREDICTIVE_ANALYZER_H
#define PREDICTIVE_ANALYZER_H

#include "../telemetry/telemetry_store.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <math.h>

// Prediction configuration
typedef struct {
    time_t prediction_window_seconds;
    int min_data_points;
    double confidence_threshold;
    bool enable_machine_learning;
    int prediction_horizon_hours;
} predictive_analyzer_config_t;

// Maintenance window prediction
typedef struct {
    time_t start_time;
    time_t end_time;
    char type[64];
    double probability;
    char description[256];
    bool is_scheduled;
} maintenance_window_t;

// Capacity forecast
typedef struct {
    double current_usage;
    double predicted_usage;
    char time_to_limit[64];
    char recommendation[256];
    double confidence;
} capacity_forecast_t;

// Risk assessment
typedef struct {
    char risk_level[32]; // "low", "medium", "high", "critical"
    double risk_score;
    char risk_factors[512];
    char mitigation[256];
    time_t assessment_timestamp;
} risk_assessment_t;

// Prediction metrics
typedef struct {
    maintenance_window_t maintenance_windows[8];
    int maintenance_window_count;
    capacity_forecast_t capacity_forecasts[16];
    int capacity_forecast_count;
    risk_assessment_t risk_assessments[16];
    int risk_assessment_count;
    time_t prediction_timestamp;
} prediction_metrics_t;

// Predictive analyzer structure
typedef struct {
    predictive_analyzer_config_t config;
    
    // Prediction results
    prediction_metrics_t* last_result;
    
    // Statistics
    time_t last_analysis;
    int analysis_count;
    int successful_predictions;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} predictive_analyzer_t;

// Initialize predictive analyzer
int predictive_analyzer_init(const predictive_analyzer_config_t* config);

// Clean up predictive analyzer
void predictive_analyzer_cleanup(void);

// Analyze predictions for all members
int predictive_analyzer_analyze(prediction_metrics_t* result);

// Get predictions for specific member
int predictive_analyzer_get_member_predictions(const char* member_name, prediction_metrics_t* predictions);

// Predict failover probability
double predictive_analyzer_predict_failover_probability(const char* member_name);

// Predict maintenance windows
int predictive_analyzer_predict_maintenance_windows(const char* member_name, 
                                                   maintenance_window_t* windows, int max_windows);

// Predict capacity requirements
int predictive_analyzer_predict_capacity(const char* member_name, capacity_forecast_t* forecast);

// Assess risk levels
int predictive_analyzer_assess_risk(const char* member_name, risk_assessment_t* assessment);

// Get predictive analyzer status
void predictive_analyzer_get_status(predictive_analyzer_t* status);

// Check if predictive analyzer is initialized
bool predictive_analyzer_is_initialized(void);

// Get predictive analyzer instance
predictive_analyzer_t* predictive_analyzer_get_instance(void);

#endif // PREDICTIVE_ANALYZER_H
