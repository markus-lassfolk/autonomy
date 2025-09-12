#ifndef ANALYTICS_ENGINE_H
#define ANALYTICS_ENGINE_H

#include "../telemetry/telemetry_store.h"
#include "health_analyzer.h"
#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>

// Note: health_thresholds_t is defined in health_analyzer.h

// Analytics configuration
typedef struct {
    bool enabled;
    time_t update_interval_seconds;
    time_t retention_period_seconds;
    int max_data_points;
    time_t trend_window_seconds;
    time_t prediction_window_seconds;
    health_thresholds_t health_thresholds;
} analytics_config_t;

// Trend analysis
typedef struct {
    char direction[16]; // "improving", "stable", "degrading"
    double slope;
    double confidence;
    char magnitude[16]; // "small", "medium", "large"
    char duration[16];  // "short", "medium", "long"
    double prediction;
    bool has_prediction;
} trend_t;

// System overview metrics
typedef struct {
    int total_members;
    int active_members;
    double overall_health;
    char uptime[64];
    time_t last_failover;
    int failover_count;
    double success_rate;
    bool has_last_failover;
} system_overview_t;

// Member performance metrics (different from system performance_metrics_t)
typedef struct {
    double average_latency[16];
    double average_loss[16];
    double average_signal[16];
    double throughput[16];
    double response_time[16];
    double error_rate[16];
    double availability[16];
    char member_names[16][128];
    int member_count;
} member_performance_metrics_t;

// Member health
typedef struct {
    double score;
    char status[32];
    char issues[512];
    time_t last_check;
    trend_t trend;
} member_health_t; // Note: member_health_t defined in health_analyzer.h

// Health metrics
typedef struct {
    member_health_t member_health[16];
    char member_names[16][128];
    int member_count;
    double overall_health;
    trend_t health_trend;
    char recommendations[1024];
} health_metrics_t;

// Analytics alert
typedef struct {
    char id[64];
    char type[64];
    char severity[32];
    char title[256];
    char message[512];
    time_t timestamp;
    bool acknowledged;
} analytics_alert_t;

// Analytics recommendation
typedef struct {
    char id[64];
    char type[64];
    char priority[32];
    char title[256];
    char description[512];
    char impact[128];
    char effort[128];
    time_t timestamp;
} analytics_recommendation_t;

// Dashboard metrics
typedef struct {
    time_t timestamp;
    system_overview_t overview;
    member_performance_metrics_t performance;
    health_metrics_t health;
    analytics_alert_t alerts[16];
    int alert_count;
    analytics_recommendation_t recommendations[16];
    int recommendation_count;
} dashboard_metrics_t;

// Analytics engine status
typedef struct {
    bool enabled;
    bool running;
    time_t last_update;
    int update_count;
    int error_count;
    time_t start_time;
    double update_duration_avg_ms;
} analytics_engine_status_t;

// Analytics engine structure
typedef struct {
    analytics_config_t config;
    
    // Current metrics
    dashboard_metrics_t* dashboard_metrics;
    
    // Statistics
    analytics_engine_status_t status;
    
    // Thread management
    pthread_t analytics_thread;
    bool thread_running;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} analytics_engine_t;

// Initialize analytics engine
int analytics_engine_init(const analytics_config_t* config);

// Clean up analytics engine
void analytics_engine_cleanup(void);

// Start analytics engine
int analytics_engine_start(void);

// Stop analytics engine
int analytics_engine_stop(void);

// Get dashboard metrics
void analytics_engine_get_dashboard_metrics(dashboard_metrics_t* metrics);

// Get member analytics
int analytics_engine_get_member_analytics(const char* member_name, int hours,
                                         member_performance_metrics_t* analytics);

// Update analytics metrics
int analytics_engine_update_metrics(void);

// Generate analytics alerts
int analytics_engine_generate_alerts(analytics_alert_t* alerts, int max_alerts);

// Generate analytics recommendations
int analytics_engine_generate_recommendations(analytics_recommendation_t* recommendations, 
                                             int max_recommendations);

// Get analytics engine status
void analytics_engine_get_status(analytics_engine_status_t* status);

// Check if analytics engine is initialized
bool analytics_engine_is_initialized(void);

// Check if analytics engine is running
bool analytics_engine_is_running(void);

// Get analytics engine instance
analytics_engine_t* analytics_engine_get_instance(void);

#endif // ANALYTICS_ENGINE_H
