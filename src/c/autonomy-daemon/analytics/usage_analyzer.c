#include "usage_analyzer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>
#include <math.h>

// Global usage analyzer instance
static usage_analyzer_t g_usage_analyzer;
static bool g_usage_analyzer_initialized = false;

// Forward declarations
void analyze_data_usage(const telemetry_sample_t* samples, int sample_count,
                               data_usage_t* usage);
void analyze_bandwidth_usage(const telemetry_sample_t* samples, int sample_count,
                                   bandwidth_usage_t* usage);
void analyze_peak_usage(const telemetry_sample_t* samples, int sample_count,
                              peak_usage_t* usage);
void calculate_usage_patterns(const telemetry_sample_t* samples, int sample_count,
                                    usage_pattern_t* patterns, int max_patterns);

// Initialize usage analyzer
int usage_analyzer_init(void) {
    if (g_usage_analyzer_initialized) {
        return 0; // Already initialized
    }
    
    memset(&g_usage_analyzer, 0, sizeof(usage_analyzer_t));
    
    // Initialize mutex
    g_usage_analyzer.mutex = malloc(sizeof(pthread_mutex_t));
    if (!g_usage_analyzer.mutex) {
        return -1;
    }
    
    pthread_mutex_init(g_usage_analyzer.mutex, NULL);
    
    // Initialize status
    g_usage_analyzer.enabled = true;
    g_usage_analyzer.last_analysis = 0;
    g_usage_analyzer.analysis_count = 0;
    g_usage_analyzer.last_result = NULL;
    
    g_usage_analyzer_initialized = true;
    return 0;
}

// Clean up usage analyzer
void usage_analyzer_cleanup(void) {
    if (!g_usage_analyzer_initialized) return;
    
    if (g_usage_analyzer.mutex) {
        pthread_mutex_destroy(g_usage_analyzer.mutex);
        free(g_usage_analyzer.mutex);
    }
    
    if (g_usage_analyzer.last_result) {
        free(g_usage_analyzer.last_result);
    }
    
    g_usage_analyzer.mutex = NULL;
    g_usage_analyzer.last_result = NULL;
    g_usage_analyzer_initialized = false;
}

// Analyze usage for all members
int usage_analyzer_analyze(usage_metrics_t* result) {
    if (!g_usage_analyzer_initialized || !result) {
        return -1;
    }
    
    if (!telemetry_store_is_initialized()) {
        return -1;
    }
    
    pthread_mutex_lock(g_usage_analyzer.mutex);
    
    // Get all member names
    char member_names[64][128];
    int member_count = telemetry_store_get_members(member_names, 64);
    
    if (member_count > 16) {
        member_count = 16; // Limit to 16 members
    }
    
    result->member_count = member_count;
    result->analysis_timestamp = time(NULL);
    
    // Analyze each member
    for (int i = 0; i < member_count; i++) {
        strncpy(result->member_names[i], member_names[i], sizeof(result->member_names[i]) - 1);
        result->member_names[i][sizeof(result->member_names[i]) - 1] = '\0';
        
        // Get samples from last 24 hours
        time_t since = time(NULL) - 86400;
        telemetry_sample_t samples[1000];
        int sample_count = telemetry_store_get_samples(member_names[i], since, samples, 1000);
        
        if (sample_count > 0) {
            // Analyze data usage
            analyze_data_usage(samples, sample_count, &result->data_usage[i]);
            
            // Analyze bandwidth usage
            analyze_bandwidth_usage(samples, sample_count, &result->bandwidth_usage[i]);
            
            // Analyze peak usage
            analyze_peak_usage(samples, sample_count, &result->peak_usage[i]);
            
            // Calculate usage patterns
            calculate_usage_patterns(samples, sample_count, result->usage_patterns, 16);
        } else {
            // Set default values for no data
            memset(&result->data_usage[i], 0, sizeof(data_usage_t));
            memset(&result->bandwidth_usage[i], 0, sizeof(bandwidth_usage_t));
            memset(&result->peak_usage[i], 0, sizeof(peak_usage_t));
        }
    }
    
    // Update analyzer status
    g_usage_analyzer.last_analysis = time(NULL);
    g_usage_analyzer.analysis_count++;
    
    // Store last result
    if (g_usage_analyzer.last_result) {
        free(g_usage_analyzer.last_result);
    }
    
    g_usage_analyzer.last_result = malloc(sizeof(usage_metrics_t));
    if (g_usage_analyzer.last_result) {
        *g_usage_analyzer.last_result = *result;
    }
    
    pthread_mutex_unlock(g_usage_analyzer.mutex);
    
    return 0;
}

// Analyze data usage
void analyze_data_usage(const telemetry_sample_t* samples, int sample_count,
                               data_usage_t* usage) {
    if (!samples || !usage || sample_count <= 0) return;
    
    // Calculate current usage from samples
    uint64_t total_bytes = 0;
    for (int i = 0; i < sample_count; i++) {
        if (samples[i].has_throughput) {
            // Convert Mbps to bytes (approximate)
            total_bytes += (uint64_t)(samples[i].throughput_mbps * 125000); // 1 Mbps = 125,000 bytes/s
        }
    }
    
    usage->current = total_bytes;
    usage->limit = 0; // No limit set for now
    usage->has_limit = false;
    usage->percentage = 0.0;
    
    // Calculate trend (simplified)
    if (sample_count > 10) {
        uint64_t first_half = 0, second_half = 0;
        int mid = sample_count / 2;
        
        for (int i = 0; i < mid; i++) {
            if (samples[i].has_throughput) {
                first_half += (uint64_t)(samples[i].throughput_mbps * 125000);
            }
        }
        
        for (int i = mid; i < sample_count; i++) {
            if (samples[i].has_throughput) {
                second_half += (uint64_t)(samples[i].throughput_mbps * 125000);
            }
        }
        
        if (second_half > first_half) {
            strcpy(usage->trend, "increasing");
        } else if (second_half < first_half) {
            strcpy(usage->trend, "decreasing");
        } else {
            strcpy(usage->trend, "stable");
        }
    } else {
        strcpy(usage->trend, "insufficient_data");
    }
    
    // Simple projection (24-hour trend)
    if (strcmp(usage->trend, "increasing") == 0) {
        usage->projection = (uint64_t)(usage->current * 1.2); // 20% increase
    } else if (strcmp(usage->trend, "decreasing") == 0) {
        usage->projection = (uint64_t)(usage->current * 0.8); // 20% decrease
    } else {
        usage->projection = usage->current; // Stable
    }
}

// Analyze bandwidth usage
void analyze_bandwidth_usage(const telemetry_sample_t* samples, int sample_count,
                                   bandwidth_usage_t* usage) {
    if (!samples || !usage || sample_count <= 0) return;
    
    double total_throughput = 0.0;
    double peak_throughput = 0.0;
    time_t peak_time = 0;
    
    for (int i = 0; i < sample_count; i++) {
        if (samples[i].has_throughput) {
            total_throughput += samples[i].throughput_mbps;
            
            if (samples[i].throughput_mbps > peak_throughput) {
                peak_throughput = samples[i].throughput_mbps;
                peak_time = samples[i].timestamp;
            }
        }
    }
    
    usage->current = (sample_count > 0) ? total_throughput / sample_count : 0.0;
    usage->average = usage->current;
    usage->peak = peak_throughput;
    usage->peak_timestamp = peak_time;
    usage->utilization = (peak_throughput > 0) ? (usage->current / peak_throughput) * 100.0 : 0.0;
}

// Analyze peak usage
void analyze_peak_usage(const telemetry_sample_t* samples, int sample_count,
                              peak_usage_t* usage) {
    if (!samples || !usage || sample_count <= 0) return;
    
    double peak_value = 0.0;
    time_t peak_time = 0;
    
    for (int i = 0; i < sample_count; i++) {
        if (samples[i].has_throughput && samples[i].throughput_mbps > peak_value) {
            peak_value = samples[i].throughput_mbps;
            peak_time = samples[i].timestamp;
        }
    }
    
    usage->value = peak_value;
    usage->timestamp = peak_time;
    
    // Determine duration based on sample count
    if (sample_count < 60) {
        strcpy(usage->duration, "short");
    } else if (sample_count < 1440) {
        strcpy(usage->duration, "medium");
    } else {
        strcpy(usage->duration, "long");
    }
}

// Calculate usage patterns
void calculate_usage_patterns(const telemetry_sample_t* samples, int sample_count,
                                    usage_pattern_t* patterns, int max_patterns) {
    if (!samples || !patterns || sample_count <= 0 || max_patterns <= 0) return;
    
    int pattern_count = 0;
    
    // Pattern 1: High usage during business hours
    if (sample_count > 100) {
        int business_hour_samples = 0;
        int total_business_hours = 0;
        
        for (int i = 0; i < sample_count; i++) {
            struct tm* tm_info = localtime(&samples[i].timestamp);
            if (tm_info && tm_info->tm_hour >= 9 && tm_info->tm_hour <= 17) {
                total_business_hours++;
                if (samples[i].has_throughput && samples[i].throughput_mbps > 50.0) {
                    business_hour_samples++;
                }
            }
        }
        
        if (total_business_hours > 0 && pattern_count < max_patterns) {
            double confidence = (double)business_hour_samples / total_business_hours;
            if (confidence > 0.6) {
                strcpy(patterns[pattern_count].pattern, "business_hours_peak");
                patterns[pattern_count].confidence = confidence;
                strcpy(patterns[pattern_count].description, 
                       "High bandwidth usage during business hours (9 AM - 5 PM)");
                pattern_count++;
            }
        }
    }
    
    // Pattern 2: Consistent low usage
    if (sample_count > 50 && pattern_count < max_patterns) {
        int low_usage_samples = 0;
        
        for (int i = 0; i < sample_count; i++) {
            if (samples[i].has_throughput && samples[i].throughput_mbps < 10.0) {
                low_usage_samples++;
            }
        }
        
        double confidence = (double)low_usage_samples / sample_count;
        if (confidence > 0.8) {
            strcpy(patterns[pattern_count].pattern, "consistent_low_usage");
            patterns[pattern_count].confidence = confidence;
            strcpy(patterns[pattern_count].description, 
                   "Consistently low bandwidth usage throughout the day");
            pattern_count++;
        }
    }
    
    // Pattern 3: Evening surge
    if (sample_count > 100 && pattern_count < max_patterns) {
        int evening_samples = 0;
        int total_evening = 0;
        
        for (int i = 0; i < sample_count; i++) {
            struct tm* tm_info = localtime(&samples[i].timestamp);
            if (tm_info && tm_info->tm_hour >= 18 && tm_info->tm_hour <= 22) {
                total_evening++;
                if (samples[i].has_throughput && samples[i].throughput_mbps > 40.0) {
                    evening_samples++;
                }
            }
        }
        
        if (total_evening > 0) {
            double confidence = (double)evening_samples / total_evening;
            if (confidence > 0.7) {
                strcpy(patterns[pattern_count].pattern, "evening_surge");
                patterns[pattern_count].confidence = confidence;
                strcpy(patterns[pattern_count].description, 
                       "Bandwidth usage surge during evening hours (6 PM - 10 PM)");
                pattern_count++;
            }
        }
    }
    
    // Pattern 4: Weekend vs weekday difference
    if (sample_count > 200 && pattern_count < max_patterns) {
        int weekday_samples = 0, weekend_samples = 0;
        double weekday_avg = 0.0, weekend_avg = 0.0;
        
        for (int i = 0; i < sample_count; i++) {
            struct tm* tm_info = localtime(&samples[i].timestamp);
            if (tm_info && samples[i].has_throughput) {
                if (tm_info->tm_wday >= 1 && tm_info->tm_wday <= 5) { // Monday to Friday
                    weekday_avg += samples[i].throughput_mbps;
                    weekday_samples++;
                } else { // Saturday and Sunday
                    weekend_avg += samples[i].throughput_mbps;
                    weekend_samples++;
                }
            }
        }
        
        if (weekday_samples > 0 && weekend_samples > 0) {
            weekday_avg /= weekday_samples;
            weekend_avg /= weekend_samples;
            
            double difference = fabs(weekday_avg - weekend_avg);
            if (difference > 20.0) { // Significant difference
                strcpy(patterns[pattern_count].pattern, "weekday_weekend_diff");
                patterns[pattern_count].confidence = fmin(1.0, difference / 100.0);
                strcpy(patterns[pattern_count].description, 
                       "Significant difference between weekday and weekend usage patterns");
                pattern_count++;
            }
        }
    }
}

// Get usage for specific member
int usage_analyzer_get_member_usage(const char* member_name, usage_metrics_t* usage) {
    if (!g_usage_analyzer_initialized || !member_name || !usage) {
        return -1;
    }
    
    if (!telemetry_store_is_initialized()) {
        return -1;
    }
    
    // Get samples from last 24 hours
    time_t since = time(NULL) - 86400;
    telemetry_sample_t samples[1000];
    int sample_count = telemetry_store_get_samples(member_name, since, samples, 1000);
    
    if (sample_count <= 0) {
        return -1;
    }
    
    // Initialize usage structure
    memset(usage, 0, sizeof(usage_metrics_t));
    usage->member_count = 1;
    strncpy(usage->member_names[0], member_name, sizeof(usage->member_names[0]) - 1);
    usage->member_names[0][sizeof(usage->member_names[0]) - 1] = '\0';
    usage->analysis_timestamp = time(NULL);
    
    // Analyze usage
    analyze_data_usage(samples, sample_count, &usage->data_usage[0]);
    analyze_bandwidth_usage(samples, sample_count, &usage->bandwidth_usage[0]);
    analyze_peak_usage(samples, sample_count, &usage->peak_usage[0]);
    calculate_usage_patterns(samples, sample_count, usage->usage_patterns, 16);
    
    return 0;
}

// Calculate usage patterns
int usage_analyzer_calculate_patterns(const char* member_name, usage_pattern_t* patterns, int max_patterns) {
    if (!g_usage_analyzer_initialized || !member_name || !patterns || max_patterns <= 0) {
        return -1;
    }
    
    if (!telemetry_store_is_initialized()) {
        return -1;
    }
    
    // Get samples from last week
    time_t since = time(NULL) - 604800; // 7 days
    telemetry_sample_t samples[1000];
    int sample_count = telemetry_store_get_samples(member_name, since, samples, 1000);
    
    if (sample_count <= 0) {
        return 0;
    }
    
    calculate_usage_patterns(samples, sample_count, patterns, max_patterns);
    
    // Count valid patterns
    int pattern_count = 0;
    for (int i = 0; i < max_patterns; i++) {
        if (patterns[i].pattern[0] != '\0') {
            pattern_count++;
        }
    }
    
    return pattern_count;
}

// Get usage analyzer status
void usage_analyzer_get_status(usage_analyzer_t* status) {
    if (!status || !g_usage_analyzer_initialized) return;
    
    pthread_mutex_lock(g_usage_analyzer.mutex);
    *status = g_usage_analyzer;
    pthread_mutex_unlock(g_usage_analyzer.mutex);
}

// Check if usage analyzer is initialized
bool usage_analyzer_is_initialized(void) {
    return g_usage_analyzer_initialized;
}

// Get usage analyzer instance
usage_analyzer_t* usage_analyzer_get_instance(void) {
    return g_usage_analyzer_initialized ? &g_usage_analyzer : NULL;
}
