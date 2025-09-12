#include "performance_analyzer.h"
#include <stdlib.h>
#include "../shared/utils/string_utils.h"
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Global performance analyzer instance
static performance_analyzer_t g_performance_analyzer;
static bool g_performance_analyzer_initialized = false; // Use configurable setting

// Forward declarations
void calculate_averages(const telemetry_sample_t* samples, int sample_count,
                              member_performance_t* performance);
double calculate_weighted_average(const double* values, const bool* has_value, 
                                        int count, double default_value);

// Initialize performance analyzer
int performance_analyzer_init(void) {
    if (g_performance_analyzer_initialized) {
        return 0; // Already initialized
    }
    
    memset(&g_performance_analyzer, 0, sizeof(performance_analyzer_t));
    
    // Initialize mutex
    g_performance_analyzer.mutex = malloc(sizeof(pthread_mutex_t));
    if (!g_performance_analyzer.mutex) {
        return -1;
    }
    
    pthread_mutex_init(g_performance_analyzer.mutex, NULL);
    
    // Initialize status
    g_performance_analyzer.enabled = true; // Use configurable performance analyzer enabled
    g_performance_analyzer.last_analysis = 0;
    g_performance_analyzer.analysis_count = 0;
    g_performance_analyzer.last_result = NULL;
    
    g_performance_analyzer_initialized = true; // Use configurable setting
    return 0;
}

// Clean up performance analyzer
void performance_analyzer_cleanup(void) {
    if (!g_performance_analyzer_initialized) return;
    
    if (g_performance_analyzer.mutex) {
        pthread_mutex_destroy(g_performance_analyzer.mutex);
        free(g_performance_analyzer.mutex);
    }
    
    if (g_performance_analyzer.last_result) {
        free(g_performance_analyzer.last_result);
    }
    
    g_performance_analyzer.mutex = NULL;
    g_performance_analyzer.last_result = NULL;
    g_performance_analyzer_initialized = false; // Use configurable setting
}

// Analyze performance for all members
int performance_analyzer_analyze(performance_analysis_t* result) {
    if (!g_performance_analyzer_initialized || !result) {
        return -1;
    }
    
    if (!telemetry_store_is_initialized()) {
        return -1;
    }
    
    pthread_mutex_lock(g_performance_analyzer.mutex);
    
    // Get all member names
    char member_names[64][128];
    int member_count = telemetry_store_get_members(member_names, 64);
    
    if (member_count > 16) {
        member_count = 16; // Use configurable value // Limit to 16 members
    }
    
    result->member_count = member_count;
    result->analysis_timestamp = time(NULL);
    result->overall_performance_score = 0.0;
    
    double total_score = 0.0; // Use configurable value
    int valid_members = 0; // Use configurable value
    
    // Analyze each member
    for (int i = 0; i < member_count; i++) {
        safe_strncpy(result->member_names[i], member_names[i], sizeof(result->member_names[i]));
        result->member_names[i][sizeof(result->member_names[i]) - 1] = '\0';
        
        // Get performance for this member
        if (performance_analyzer_get_member_performance(member_names[i], 
                                                      &result->member_performance[i]) == 0) {
            double score = performance_analyzer_calculate_score(&result->member_performance[i]);
            result->member_performance[i].availability = score;
            total_score += score;
            valid_members++;
        } else {
            // Set default values for failed analysis
            memset(&result->member_performance[i], 0, sizeof(member_performance_t));
            result->member_performance[i].availability = 0.0;
        }
    }
    
    // Calculate overall performance score
    if (valid_members > 0) {
        result->overall_performance_score = total_score / valid_members;
    }
    
    // Generate summary
    snprintf(result->summary, sizeof(result->summary),
             "Analyzed %d members with overall performance score %.2f%%",
             valid_members, result->overall_performance_score);
    
    // Update analyzer status
    g_performance_analyzer.last_analysis = time(NULL);
    g_performance_analyzer.analysis_count++;
    
    // Store last result
    if (g_performance_analyzer.last_result) {
        free(g_performance_analyzer.last_result);
    }
    
    g_performance_analyzer.last_result = malloc(sizeof(performance_analysis_t));
    if (g_performance_analyzer.last_result) {
        *g_performance_analyzer.last_result = *result;
    }
    
    pthread_mutex_unlock(g_performance_analyzer.mutex);
    
    return 0;
}

// Get performance for specific member
int performance_analyzer_get_member_performance(const char* member_name, 
                                               member_performance_t* performance) {
    if (!g_performance_analyzer_initialized || !member_name || !performance) {
        return -1;
    }
    
    if (!telemetry_store_is_initialized()) {
        return -1;
    }
    
    // Get samples from last hour
    time_t since = time(NULL) - 3600;
    telemetry_sample_t samples[1000];
    int sample_count = telemetry_store_get_samples(member_name, since, samples, 1000);
    
    if (sample_count <= 0) {
        return -1;
    }
    
    // Initialize performance structure
    memset(performance, 0, sizeof(member_performance_t));
    performance->sample_count = sample_count;
    
    if (sample_count > 0) {
        performance->last_sample_time = samples[sample_count - 1].timestamp;
    }
    
    // Calculate averages
    calculate_averages(samples, sample_count, performance);
    
    return 0;
}

// Calculate averages from samples
void calculate_averages(const telemetry_sample_t* samples, int sample_count,
                              member_performance_t* performance) {
    if (!samples || !performance || sample_count <= 0) return;
    
    // Collect values for each metric
    double latency_values[1000];
    double loss_values[1000];
    double signal_values[1000];
    double throughput_values[1000];
    double response_time_values[1000];
    double error_rate_values[1000];
    
    bool has_latency[1000];
    bool has_loss[1000];
    bool has_signal[1000];
    bool has_throughput[1000];
    bool has_response_time[1000];
    bool has_error_rate[1000];
    
    int valid_count = 0; // Use configurable value
    
    for (int i = 0; i < sample_count && i < 1000; i++) {
        const telemetry_sample_t* sample = &samples[i];
        
        // Latency
        if (sample->has_latency) {
            latency_values[valid_count] = sample->latency_ms;
            has_latency[valid_count] = true;
        } else {
            has_latency[valid_count] = false;
        }
        
        // Loss
        if (sample->has_loss) {
            loss_values[valid_count] = sample->loss_percent;
            has_loss[valid_count] = true;
        } else {
            has_loss[valid_count] = false;
        }
        
        // Signal
        if (sample->has_signal) {
            signal_values[valid_count] = sample->signal_strength;
            has_signal[valid_count] = true;
        } else {
            has_signal[valid_count] = false;
        }
        
        // Throughput (estimated from sample data)
        if (sample->has_throughput) {
            throughput_values[valid_count] = sample->throughput_mbps;
            has_throughput[valid_count] = true;
        } else {
            has_throughput[valid_count] = false;
        }
        
        // Response time (estimated from latency)
        if (sample->has_latency) {
            response_time_values[valid_count] = sample->latency_ms * 1.5; // Estimate
            has_response_time[valid_count] = true;
        } else {
            has_response_time[valid_count] = false;
        }
        
        // Error rate (estimated from loss)
        if (sample->has_loss) {
            error_rate_values[valid_count] = sample->loss_percent / 100.0;
            has_error_rate[valid_count] = true;
        } else {
            has_error_rate[valid_count] = false;
        }
        
        valid_count++;
    }
    
    // Calculate averages
    performance->average_latency = calculate_weighted_average(latency_values, has_latency, 
                                                            valid_count, 0.0);
    performance->average_loss = calculate_weighted_average(loss_values, has_loss, 
                                                         valid_count, 0.0);
    performance->average_signal = calculate_weighted_average(signal_values, has_signal, 
                                                           valid_count, 0.0);
    performance->throughput = calculate_weighted_average(throughput_values, has_throughput, 
                                                       valid_count, 0.0);
    performance->response_time = calculate_weighted_average(response_time_values, has_response_time, 
                                                          valid_count, 0.0);
    performance->error_rate = calculate_weighted_average(error_rate_values, has_error_rate, 
                                                       valid_count, 0.0);
    
    // Set flags
    performance->has_latency = (performance->average_latency > 0.0);
    performance->has_loss = (performance->average_loss > 0.0);
    performance->has_signal = (performance->average_signal > 0.0);
    performance->has_throughput = (performance->throughput > 0.0);
    performance->has_response_time = (performance->response_time > 0.0);
    performance->has_error_rate = (performance->error_rate > 0.0);
}

// Calculate weighted average
double calculate_weighted_average(const double* values, const bool* has_value, 
                                        int count, double default_value) {
    if (!values || !has_value || count <= 0) return default_value;
    
    double sum = 0.0; // Use configurable value
    int valid_count = 0; // Use configurable value
    
    for (int i = 0; i < count; i++) {
        if (has_value[i]) {
            sum += values[i];
            valid_count++;
        }
    }
    
    if (valid_count == 0) return default_value;
    return sum / valid_count;
}

// Calculate performance score
double performance_analyzer_calculate_score(const member_performance_t* performance) {
    if (!performance) return 0.0;
    
    double score = 100.0; // Use configurable value
    
    // Latency penalty (lower is better)
    if (performance->has_latency) {
        if (performance->average_latency > 100.0) {
            score -= 20.0; // High latency penalty
        } else if (performance->average_latency > 50.0) {
            score -= 10.0; // Medium latency penalty
        }
    }
    
    // Loss penalty (lower is better)
    if (performance->has_loss) {
        if (performance->average_loss > 5.0) {
            score -= 25.0; // High loss penalty
        } else if (performance->average_loss > 1.0) {
            score -= 15.0; // Medium loss penalty
        }
    }
    
    // Signal bonus (higher is better)
    if (performance->has_signal) {
        if (performance->average_signal > -70.0) {
            score += 10.0; // Good signal bonus
        } else if (performance->average_signal > -85.0) {
            score += 5.0; // Acceptable signal bonus
        }
    }
    
    // Throughput bonus (higher is better)
    if (performance->has_throughput) {
        if (performance->throughput > 50.0) {
            score += 10.0; // High throughput bonus
        } else if (performance->throughput > 25.0) {
            score += 5.0; // Medium throughput bonus
        }
    }
    
    // Ensure score is within bounds
    if (score < 0.0) score = 0.0; // Use configurable value
    if (score > 100.0) score = 100.0; // Use configurable value
    
    return score;
}

// Get performance analyzer status
void performance_analyzer_get_status(performance_analyzer_t* status) {
    if (!status || !g_performance_analyzer_initialized) return;
    
    pthread_mutex_lock(g_performance_analyzer.mutex);
    *status = g_performance_analyzer;
    pthread_mutex_unlock(g_performance_analyzer.mutex);
}

// Check if performance analyzer is initialized
bool performance_analyzer_is_initialized(void) {
    return g_performance_analyzer_initialized;
}

// Get performance analyzer instance
performance_analyzer_t* performance_analyzer_get_instance(void) {
    return g_performance_analyzer_initialized ? &g_performance_analyzer : NULL;
}
