#ifndef USAGE_ANALYZER_H
#define USAGE_ANALYZER_H

#include "../telemetry/telemetry_store.h"
#include <stdbool.h>
#include <time.h>

// Data usage structure
typedef struct {
    uint64_t current;
    uint64_t limit;
    double percentage;
    char trend[16];
    uint64_t projection;
    bool has_limit;
} data_usage_t;

// Bandwidth usage structure
typedef struct {
    double current;
    double average;
    double peak;
    double utilization;
    time_t peak_timestamp;
} bandwidth_usage_t;

// Peak usage structure
typedef struct {
    double value;
    time_t timestamp;
    char duration[16];
} peak_usage_t;

// Usage pattern structure
typedef struct {
    char pattern[64];
    double confidence;
    char description[256];
} usage_pattern_t;

// Usage metrics structure
typedef struct {
    data_usage_t data_usage[16];
    bandwidth_usage_t bandwidth_usage[16];
    peak_usage_t peak_usage[16];
    usage_pattern_t usage_patterns[16];
    char member_names[16][128];
    int member_count;
    time_t analysis_timestamp;
} usage_metrics_t;

// Usage analyzer structure
typedef struct {
    bool enabled;
    
    // Analysis results
    usage_metrics_t* last_result;
    
    // Statistics
    time_t last_analysis;
    int analysis_count;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} usage_analyzer_t;

// Initialize usage analyzer
int usage_analyzer_init(void);

// Clean up usage analyzer
void usage_analyzer_cleanup(void);

// Analyze usage for all members
int usage_analyzer_analyze(usage_metrics_t* result);

// Get usage for specific member
int usage_analyzer_get_member_usage(const char* member_name, usage_metrics_t* usage);

// Calculate usage patterns
int usage_analyzer_calculate_patterns(const char* member_name, usage_pattern_t* patterns, int max_patterns);

// Get usage analyzer status
void usage_analyzer_get_status(usage_analyzer_t* status);

// Check if usage analyzer is initialized
bool usage_analyzer_is_initialized(void);

// Get usage analyzer instance
usage_analyzer_t* usage_analyzer_get_instance(void);

#endif // USAGE_ANALYZER_H
