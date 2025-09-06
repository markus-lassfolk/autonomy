#include "trend_analyzer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <math.h>
#include <time.h>

// Global trend analyzer instance
static trend_analyzer_t g_trend_analyzer;
static bool g_trend_analyzer_initialized = false;

// Forward declarations
void analyze_latency_trend(const telemetry_sample_t* samples, int sample_count,
                                 trend_result_t* result);
void analyze_signal_trend(const telemetry_sample_t* samples, int sample_count,
                                trend_result_t* result);
void analyze_loss_trend(const telemetry_sample_t* samples, int sample_count,
                              trend_result_t* result);
void analyze_throughput_trend(const telemetry_sample_t* samples, int sample_count,
                                    trend_result_t* result);
void calculate_overall_trend(member_trends_t* trends);
static void classify_trend(trend_result_t* result);
static double linear_regression_slope(const double* x_values, const double* y_values, int count);

// Initialize trend analyzer
int trend_analyzer_init(const trend_analyzer_config_t* config) {
    if (g_trend_analyzer_initialized) {
        return 0; // Already initialized
    }
    
    memset(&g_trend_analyzer, 0, sizeof(trend_analyzer_t));
    
    // Set default configuration if none provided
    if (config) {
        g_trend_analyzer.config = *config;
    } else {
        g_trend_analyzer.config.trend_window_seconds = 3600; // 1 hour
        g_trend_analyzer.config.min_data_points = 10;
        g_trend_analyzer.config.confidence_threshold = 0.7;
        g_trend_analyzer.config.enable_prediction = true;
        g_trend_analyzer.config.prediction_horizon_hours = 24;
    }
    
    // Initialize mutex
    g_trend_analyzer.mutex = malloc(sizeof(pthread_mutex_t));
    if (!g_trend_analyzer.mutex) {
        return -1;
    }
    
    pthread_mutex_init(g_trend_analyzer.mutex, NULL);
    
    // Initialize status
    g_trend_analyzer.last_analysis = 0;
    g_trend_analyzer.analysis_count = 0;
    g_trend_analyzer.successful_analyses = 0;
    g_trend_analyzer.member_count = 0;
    
    g_trend_analyzer_initialized = true;
    return 0;
}

// Clean up trend analyzer
void trend_analyzer_cleanup(void) {
    if (!g_trend_analyzer_initialized) return;
    
    if (g_trend_analyzer.mutex) {
        pthread_mutex_destroy(g_trend_analyzer.mutex);
        free(g_trend_analyzer.mutex);
    }
    
    g_trend_analyzer.mutex = NULL;
    g_trend_analyzer_initialized = false;
}

// Analyze trends for all members
int trend_analyzer_analyze_all(void) {
    if (!g_trend_analyzer_initialized) {
        return -1;
    }
    
    if (!telemetry_store_is_initialized()) {
        return -1;
    }
    
    pthread_mutex_lock(g_trend_analyzer.mutex);
    
    // Get all member names
    char member_names[64][128];
    int member_count = telemetry_store_get_members(member_names, 64);
    
    if (member_count > 16) {
        member_count = 16; // Limit to 16 members
    }
    
    g_trend_analyzer.member_count = member_count;
    int successful = 0;
    
    // Analyze trends for each member
    for (int i = 0; i < member_count; i++) {
        if (trend_analyzer_analyze_member(member_names[i], 
                                         &g_trend_analyzer.member_trends[i]) == 0) {
            successful++;
        }
    }
    
    // Update statistics
    g_trend_analyzer.last_analysis = time(NULL);
    g_trend_analyzer.analysis_count++;
    g_trend_analyzer.successful_analyses = successful;
    
    pthread_mutex_unlock(g_trend_analyzer.mutex);
    
    return 0;
}

// Analyze trends for specific member
int trend_analyzer_analyze_member(const char* member_name, member_trends_t* trends) {
    if (!g_trend_analyzer_initialized || !member_name || !trends) {
        return -1;
    }
    
    if (!telemetry_store_is_initialized()) {
        return -1;
    }
    
    // Initialize trends structure
    memset(trends, 0, sizeof(member_trends_t));
    strncpy(trends->member_name, member_name, sizeof(trends->member_name) - 1);
    trends->member_name[sizeof(trends->member_name) - 1] = '\0';
    trends->last_analysis = time(NULL);
    
    // Get samples from trend window
    time_t since = time(NULL) - g_trend_analyzer.config.trend_window_seconds;
    telemetry_sample_t samples[1000];
    int sample_count = telemetry_store_get_samples(member_name, since, samples, 1000);
    
    if (sample_count < g_trend_analyzer.config.min_data_points) {
        return -1; // Not enough data points
    }
    
    // Analyze trends for each metric
    analyze_latency_trend(samples, sample_count, &trends->latency_trend);
    analyze_signal_trend(samples, sample_count, &trends->signal_trend);
    analyze_loss_trend(samples, sample_count, &trends->loss_trend);
    analyze_throughput_trend(samples, sample_count, &trends->throughput_trend);
    
    // Calculate overall trend
    calculate_overall_trend(trends);
    
    return 0;
}

// Analyze latency trend
void analyze_latency_trend(const telemetry_sample_t* samples, int sample_count,
                                 trend_result_t* result) {
    if (!samples || !result || sample_count <= 0) return;
    
    // Collect latency data with timestamps
    double latencies[1000];
    time_t timestamps[1000];
    int valid_count = 0;
    
    for (int i = 0; i < sample_count && i < 1000; i++) {
        if (samples[i].has_latency) {
            latencies[valid_count] = samples[i].latency_ms;
            timestamps[valid_count] = samples[i].timestamp;
            valid_count++;
        }
    }
    
    if (valid_count < g_trend_analyzer.config.min_data_points) {
        return;
    }
    
    // Calculate trend
    result->slope = trend_analyzer_calculate_slope(latencies, timestamps, valid_count);
    result->data_points_used = valid_count;
    result->analysis_timestamp = time(NULL);
    
    // Classify trend
    classify_trend(result);
    
    // Make prediction if enabled
    if (g_trend_analyzer.config.enable_prediction) {
        time_t future_time = time(NULL) + (g_trend_analyzer.config.prediction_horizon_hours * 3600);
        result->prediction = trend_analyzer_predict_value(result, future_time);
        result->has_prediction = true;
    }
}

// Analyze signal trend
void analyze_signal_trend(const telemetry_sample_t* samples, int sample_count,
                                trend_result_t* result) {
    if (!samples || !result || sample_count <= 0) return;
    
    // Collect signal data with timestamps
    double signals[1000];
    time_t timestamps[1000];
    int valid_count = 0;
    
    for (int i = 0; i < sample_count && i < 1000; i++) {
        if (samples[i].has_signal) {
            signals[valid_count] = samples[i].signal_strength;
            timestamps[valid_count] = samples[i].timestamp;
            valid_count++;
        }
    }
    
    if (valid_count < g_trend_analyzer.config.min_data_points) {
        return;
    }
    
    // Calculate trend
    result->slope = trend_analyzer_calculate_slope(signals, timestamps, valid_count);
    result->data_points_used = valid_count;
    result->analysis_timestamp = time(NULL);
    
    // Classify trend
    classify_trend(result);
    
    // Make prediction if enabled
    if (g_trend_analyzer.config.enable_prediction) {
        time_t future_time = time(NULL) + (g_trend_analyzer.config.prediction_horizon_hours * 3600);
        result->prediction = trend_analyzer_predict_value(result, future_time);
        result->has_prediction = true;
    }
}

// Analyze loss trend
void analyze_loss_trend(const telemetry_sample_t* samples, int sample_count,
                              trend_result_t* result) {
    if (!samples || !result || sample_count <= 0) return;
    
    // Collect loss data with timestamps
    double losses[1000];
    time_t timestamps[1000];
    int valid_count = 0;
    
    for (int i = 0; i < sample_count && i < 1000; i++) {
        if (samples[i].has_loss) {
            losses[valid_count] = samples[i].loss_percent;
            timestamps[valid_count] = samples[i].timestamp;
            valid_count++;
        }
    }
    
    if (valid_count < g_trend_analyzer.config.min_data_points) {
        return;
    }
    
    // Calculate trend
    result->slope = trend_analyzer_calculate_slope(losses, timestamps, valid_count);
    result->data_points_used = valid_count;
    result->analysis_timestamp = time(NULL);
    
    // Classify trend
    classify_trend(result);
    
    // Make prediction if enabled
    if (g_trend_analyzer.config.enable_prediction) {
        time_t future_time = time(NULL) + (g_trend_analyzer.config.prediction_horizon_hours * 3600);
        result->prediction = trend_analyzer_predict_value(result, future_time);
        result->has_prediction = true;
    }
}

// Analyze throughput trend
void analyze_throughput_trend(const telemetry_sample_t* samples, int sample_count,
                                    trend_result_t* result) {
    if (!samples || !result || sample_count <= 0) return;
    
    // Collect throughput data with timestamps
    double throughputs[1000];
    time_t timestamps[1000];
    int valid_count = 0;
    
    for (int i = 0; i < sample_count && i < 1000; i++) {
        if (samples[i].has_throughput) {
            throughputs[valid_count] = samples[i].throughput_mbps;
            timestamps[valid_count] = samples[i].throughput_mbps;
            valid_count++;
        }
    }
    
    if (valid_count < g_trend_analyzer.config.min_data_points) {
        return;
    }
    
    // Calculate trend
    result->slope = trend_analyzer_calculate_slope(throughputs, timestamps, valid_count);
    result->data_points_used = valid_count;
    result->analysis_timestamp = time(NULL);
    
    // Classify trend
    classify_trend(result);
    
    // Make prediction if enabled
    if (g_trend_analyzer.config.enable_prediction) {
        time_t future_time = time(NULL) + (g_trend_analyzer.config.prediction_horizon_hours * 3600);
        result->prediction = trend_analyzer_predict_value(result, future_time);
        result->has_prediction = true;
    }
}

// Calculate overall trend from individual metrics
void calculate_overall_trend(member_trends_t* trends) {
    if (!trends) return;
    
    // Calculate weighted average slope
    double total_slope = 0.0;
    double total_weight = 0.0;
    
    if (trends->latency_trend.data_points_used > 0) {
        total_slope += trends->latency_trend.slope * 0.3; // 30% weight
        total_weight += 0.3;
    }
    
    if (trends->signal_trend.data_points_used > 0) {
        total_slope += trends->signal_trend.slope * 0.25; // 25% weight
        total_weight += 0.25;
    }
    
    if (trends->loss_trend.data_points_used > 0) {
        total_slope += trends->loss_trend.slope * 0.25; // 25% weight
        total_weight += 0.25;
    }
    
    if (trends->throughput_trend.data_points_used > 0) {
        total_slope += trends->throughput_trend.slope * 0.2; // 20% weight
        total_weight += 0.2;
    }
    
    if (total_weight > 0) {
        trends->overall_trend.slope = total_slope / total_weight;
        trends->overall_trend.data_points_used = 1; // Indicates valid calculation
        trends->overall_trend.analysis_timestamp = time(NULL);
        classify_trend(&trends->overall_trend);
    }
}

// Classify trend based on slope and confidence
static void classify_trend(trend_result_t* result) {
    if (!result) return;
    
    // Determine direction
    if (fabs(result->slope) < 0.01) {
        strcpy(result->direction, "stable");
    } else if (result->slope > 0) {
        strcpy(result->direction, "improving");
    } else {
        strcpy(result->direction, "degrading");
    }
    
    // Determine magnitude
    double abs_slope = fabs(result->slope);
    if (abs_slope < 0.1) {
        strcpy(result->magnitude, "small");
    } else if (abs_slope < 0.5) {
        strcpy(result->magnitude, "medium");
    } else {
        strcpy(result->magnitude, "large");
    }
    
    // Determine duration based on data points
    if (result->data_points_used < 20) {
        strcpy(result->duration, "short");
    } else if (result->data_points_used < 50) {
        strcpy(result->duration, "medium");
    } else {
        strcpy(result->duration, "long");
    }
    
    // Calculate confidence (simplified)
    result->confidence = fmin(1.0, (double)result->data_points_used / 100.0);
}

// Calculate trend slope using linear regression
double trend_analyzer_calculate_slope(const double* values, const time_t* timestamps, 
                                     int count) {
    if (!values || !timestamps || count < 2) return 0.0;
    
    // Convert timestamps to relative seconds for better numerical stability
    time_t base_time = timestamps[0];
    double x_values[1000];
    double y_values[1000];
    
    for (int i = 0; i < count && i < 1000; i++) {
        x_values[i] = (double)(timestamps[i] - base_time);
        y_values[i] = values[i];
    }
    
    return linear_regression_slope(x_values, y_values, count);
}

// Linear regression slope calculation
static double linear_regression_slope(const double* x_values, const double* y_values, int count) {
    if (!x_values || !y_values || count < 2) return 0.0;
    
    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0;
    
    for (int i = 0; i < count; i++) {
        sum_x += x_values[i];
        sum_y += y_values[i];
        sum_xy += x_values[i] * y_values[i];
        sum_x2 += x_values[i] * x_values[i];
    }
    
    double n = (double)count;
    double denominator = n * sum_x2 - sum_x * sum_x;
    
    if (fabs(denominator) < 1e-10) return 0.0;
    
    return (n * sum_xy - sum_x * sum_y) / denominator;
}

// Calculate trend confidence
double trend_analyzer_calculate_confidence(const double* values, const double* residuals, 
                                          int count) {
    if (!values || !residuals || count < 2) return 0.0;
    
    // Simplified confidence calculation based on data consistency
    double mean = 0.0;
    for (int i = 0; i < count; i++) {
        mean += values[i];
    }
    mean /= count;
    
    double variance = 0.0;
    for (int i = 0; i < count; i++) {
        double diff = values[i] - mean;
        variance += diff * diff;
    }
    variance /= count;
    
    // Higher variance = lower confidence
    double std_dev = sqrt(variance);
    double coefficient_of_variation = (mean != 0.0) ? std_dev / fabs(mean) : 1.0;
    
    // Convert to confidence (0-1)
    double confidence = 1.0 / (1.0 + coefficient_of_variation);
    return fmax(0.0, fmin(1.0, confidence));
}

// Predict future value
double trend_analyzer_predict_value(const trend_result_t* trend, time_t future_time) {
    if (!trend || !trend->has_prediction) return 0.0;
    
    // Simple linear prediction
    time_t time_diff = future_time - trend->analysis_timestamp;
    return trend->prediction + (trend->slope * time_diff);
}

// Get trend results for all members
int trend_analyzer_get_all_trends(member_trends_t* trends, int max_trends) {
    if (!g_trend_analyzer_initialized || !trends || max_trends <= 0) {
        return -1;
    }
    
    pthread_mutex_lock(g_trend_analyzer.mutex);
    
    int count = (max_trends < g_trend_analyzer.member_count) ? 
                max_trends : g_trend_analyzer.member_count;
    
    for (int i = 0; i < count; i++) {
        trends[i] = g_trend_analyzer.member_trends[i];
    }
    
    pthread_mutex_unlock(g_trend_analyzer.mutex);
    
    return count;
}

// Get trend results for specific member
int trend_analyzer_get_member_trends(const char* member_name, member_trends_t* trends) {
    if (!g_trend_analyzer_initialized || !member_name || !trends) {
        return -1;
    }
    
    pthread_mutex_lock(g_trend_analyzer.mutex);
    
    for (int i = 0; i < g_trend_analyzer.member_count; i++) {
        if (strcmp(g_trend_analyzer.member_trends[i].member_name, member_name) == 0) {
            *trends = g_trend_analyzer.member_trends[i];
            pthread_mutex_unlock(g_trend_analyzer.mutex);
            return 0;
        }
    }
    
    pthread_mutex_unlock(g_trend_analyzer.mutex);
    return -1; // Member not found
}

// Get trend analyzer status
void trend_analyzer_get_status(trend_analyzer_t* status) {
    if (!status || !g_trend_analyzer_initialized) return;
    
    pthread_mutex_lock(g_trend_analyzer.mutex);
    *status = g_trend_analyzer;
    pthread_mutex_unlock(g_trend_analyzer.mutex);
}

// Check if trend analyzer is initialized
bool trend_analyzer_is_initialized(void) {
    return g_trend_analyzer_initialized;
}

// Get trend analyzer instance
trend_analyzer_t* trend_analyzer_get_instance(void) {
    return g_trend_analyzer_initialized ? &g_trend_analyzer : NULL;
}
