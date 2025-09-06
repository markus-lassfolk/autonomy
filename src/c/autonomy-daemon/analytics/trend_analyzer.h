#ifndef TREND_ANALYZER_H
#define TREND_ANALYZER_H

#include "../telemetry/telemetry_store.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

// Trend analysis configuration
typedef struct {
    time_t trend_window_seconds;
    int min_data_points;
    double confidence_threshold;
    bool enable_prediction;
    int prediction_horizon_hours;
} trend_analyzer_config_t;

// Trend analysis result
typedef struct {
    char direction[16]; // "improving", "stable", "degrading"
    double slope;
    double confidence;
    char magnitude[16]; // "small", "medium", "large"
    char duration[16];  // "short", "medium", "long"
    double prediction;
    bool has_prediction;
    int data_points_used;
    time_t analysis_timestamp;
} trend_result_t;

// Trend analysis for multiple metrics
typedef struct {
    trend_result_t latency_trend;
    trend_result_t signal_trend;
    trend_result_t loss_trend;
    trend_result_t throughput_trend;
    trend_result_t overall_trend;
    char member_name[128];
    time_t last_analysis;
} member_trends_t;

// Trend analyzer structure
typedef struct {
    trend_analyzer_config_t config;
    
    // Analysis results
    member_trends_t member_trends[16];
    int member_count;
    
    // Statistics
    time_t last_analysis;
    int analysis_count;
    int successful_analyses;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} trend_analyzer_t;

// Initialize trend analyzer
int trend_analyzer_init(const trend_analyzer_config_t* config);

// Clean up trend analyzer
void trend_analyzer_cleanup(void);

// Analyze trends for all members
int trend_analyzer_analyze_all(void);

// Analyze trends for specific member
int trend_analyzer_analyze_member(const char* member_name, member_trends_t* trends);

// Get trend analysis for specific metric
int trend_analyzer_analyze_metric(const char* member_name, const char* metric_name,
                                  trend_result_t* result);

// Get trend results for all members
int trend_analyzer_get_all_trends(member_trends_t* trends, int max_trends);

// Get trend results for specific member
int trend_analyzer_get_member_trends(const char* member_name, member_trends_t* trends);

// Calculate trend slope using linear regression
double trend_analyzer_calculate_slope(const double* values, const time_t* timestamps, 
                                     int count);

// Calculate trend confidence
double trend_analyzer_calculate_confidence(const double* values, const double* residuals, 
                                          int count);

// Predict future value
double trend_analyzer_predict_value(const trend_result_t* trend, time_t future_time);

// Get trend analyzer status
void trend_analyzer_get_status(trend_analyzer_t* status);

// Check if trend analyzer is initialized
bool trend_analyzer_is_initialized(void);

// Get trend analyzer instance
trend_analyzer_t* trend_analyzer_get_instance(void);

#endif // TREND_ANALYZER_H
