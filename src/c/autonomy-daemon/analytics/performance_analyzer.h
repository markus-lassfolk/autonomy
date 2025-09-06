#ifndef PERFORMANCE_ANALYZER_H
#define PERFORMANCE_ANALYZER_H

#include "../telemetry/telemetry_store.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

// Performance metrics structure
typedef struct {
    double average_latency;
    double average_loss;
    double average_signal;
    double throughput;
    double response_time;
    double error_rate;
    double availability;
    int sample_count;
    time_t last_sample_time;
    bool has_latency;
    bool has_loss;
    bool has_signal;
    bool has_throughput;
    bool has_response_time;
    bool has_error_rate;
    bool has_availability;
} member_performance_t;

// Performance analysis result
typedef struct {
    member_performance_t member_performance[16];
    char member_names[16][128];
    int member_count;
    double overall_performance_score;
    time_t analysis_timestamp;
    char summary[1024];
} performance_analysis_t;

// Performance analyzer structure
typedef struct {
    bool enabled;
    time_t last_analysis;
    int analysis_count;
    performance_analysis_t* last_result;
    pthread_mutex_t* mutex;
} performance_analyzer_t;

// Initialize performance analyzer
int performance_analyzer_init(void);

// Clean up performance analyzer
void performance_analyzer_cleanup(void);

// Analyze performance for all members
int performance_analyzer_analyze(performance_analysis_t* result);

// Get performance for specific member
int performance_analyzer_get_member_performance(const char* member_name, 
                                               member_performance_t* performance);

// Calculate performance score
double performance_analyzer_calculate_score(const member_performance_t* performance);

// Get performance analyzer status
void performance_analyzer_get_status(performance_analyzer_t* status);

// Check if performance analyzer is initialized
bool performance_analyzer_is_initialized(void);

// Get performance analyzer instance
performance_analyzer_t* performance_analyzer_get_instance(void);

#endif // PERFORMANCE_ANALYZER_H
