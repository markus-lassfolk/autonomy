#ifndef PERFORMANCE_MONITOR_H
#define PERFORMANCE_MONITOR_H

#include <stdbool.h>
#include <time.h>
#include <sys/resource.h>

// Performance metrics
typedef struct {
    double cpu_usage_percent;
    double memory_usage_mb;
    double memory_usage_percent;
    uint64_t memory_available_mb;
    uint64_t memory_total_mb;
    double disk_usage_percent;
    uint64_t disk_available_mb;
    uint64_t disk_total_mb;
    int open_file_descriptors;
    int max_file_descriptors;
    double load_average_1min;
    double load_average_5min;
    double load_average_15min;
    time_t last_update;
} performance_metrics_t;

// Performance thresholds
typedef struct {
    double cpu_warning_threshold;
    double cpu_critical_threshold;
    double memory_warning_threshold;
    double memory_critical_threshold;
    double disk_warning_threshold;
    double disk_critical_threshold;
    double load_warning_threshold;
    double load_critical_threshold;
} performance_thresholds_t;

// Performance monitor configuration
typedef struct {
    bool enabled;
    int monitor_interval_seconds;
    bool enable_alerts;
    bool enable_logging;
    performance_thresholds_t thresholds;
} performance_monitor_config_t;

// Performance monitor structure
typedef struct {
    performance_monitor_config_t config;
    
    // Current metrics
    performance_metrics_t current_metrics;
    
    // Historical metrics
    performance_metrics_t metrics_history[100];
    int history_count;
    int history_index;
    
    // Statistics
    time_t last_monitor_time;
    int monitor_count;
    int alert_count;
    int critical_events;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} performance_monitor_t;

// Initialize performance monitor
int performance_monitor_init(const performance_monitor_config_t* config);

// Clean up performance monitor
void performance_monitor_cleanup(void);

// Collect performance metrics
int performance_monitor_collect_metrics(void);

// Get current performance metrics
int performance_monitor_get_metrics(performance_metrics_t* metrics);

// Get performance history
int performance_monitor_get_history(performance_metrics_t* history, int max_history);

// Check performance thresholds
bool performance_monitor_check_thresholds(void);

// Get performance monitor status
void performance_monitor_get_status(performance_monitor_t* status);

// Check if performance monitor is initialized
bool performance_monitor_is_initialized(void);

// Get performance monitor instance
performance_monitor_t* performance_monitor_get_instance(void);

#endif // PERFORMANCE_MONITOR_H
