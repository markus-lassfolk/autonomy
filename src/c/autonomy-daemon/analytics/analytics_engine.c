#include "analytics_engine.h"
#include "performance_analyzer.h"
#include "trend_analyzer.h"
#include "health_analyzer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <math.h>
#include <unistd.h>

// Global analytics engine instance
static analytics_engine_t g_analytics_engine;
static bool g_analytics_engine_initialized = false;

// Forward declarations
static void* analytics_thread(void* arg);
int calculate_system_overview(system_overview_t* overview);
int calculate_performance_metrics(member_performance_metrics_t* performance);
int calculate_health_metrics(health_metrics_t* health);
static int generate_analytics_alerts(analytics_alert_t* alerts, int max_alerts);
static int generate_analytics_recommendations(analytics_recommendation_t* recommendations, 
                                             int max_recommendations);

// Initialize analytics engine
int analytics_engine_init(const analytics_config_t* config) {
    if (g_analytics_engine_initialized) {
        return 0; // Already initialized
    }
    
    memset(&g_analytics_engine, 0, sizeof(analytics_engine_t));
    
    // Set default configuration if none provided
    if (config) {
        g_analytics_engine.config = *config;
    } else {
        g_analytics_engine.config.enabled = true;
        g_analytics_engine.config.update_interval_seconds = 300; // 5 minutes
        g_analytics_engine.config.retention_period_seconds = 86400; // 24 hours
        g_analytics_engine.config.max_data_points = 1000;
        g_analytics_engine.config.trend_window_seconds = 3600; // 1 hour
        g_analytics_engine.config.prediction_window_seconds = 86400; // 24 hours
        g_analytics_engine.config.health_thresholds.excellent = 80.0;
        g_analytics_engine.config.health_thresholds.good = 60.0;
        g_analytics_engine.config.health_thresholds.fair = 40.0;
        g_analytics_engine.config.health_thresholds.poor = 20.0;
        g_analytics_engine.config.health_thresholds.critical = 0.0;
    }
    
    // Initialize mutex
    g_analytics_engine.mutex = malloc(sizeof(pthread_mutex_t));
    if (!g_analytics_engine.mutex) {
        return -1;
    }
    
    pthread_mutex_init(g_analytics_engine.mutex, NULL);
    
    // Initialize status
    g_analytics_engine.status.enabled = g_analytics_engine.config.enabled;
    g_analytics_engine.status.running = false;
    g_analytics_engine.status.last_update = 0;
    g_analytics_engine.status.update_count = 0;
    g_analytics_engine.status.error_count = 0;
    g_analytics_engine.status.start_time = 0;
    g_analytics_engine.status.update_duration_avg_ms = 0.0;
    
    // Initialize dashboard metrics
    g_analytics_engine.dashboard_metrics = malloc(sizeof(dashboard_metrics_t));
    if (!g_analytics_engine.dashboard_metrics) {
        pthread_mutex_destroy(g_analytics_engine.mutex);
        free(g_analytics_engine.mutex);
        return -1;
    }
    
    memset(g_analytics_engine.dashboard_metrics, 0, sizeof(dashboard_metrics_t));
    
    g_analytics_engine_initialized = true;
    return 0;
}

// Clean up analytics engine
void analytics_engine_cleanup(void) {
    if (!g_analytics_engine_initialized) return;
    
    // Stop engine if running
    if (g_analytics_engine.status.running) {
        analytics_engine_stop();
    }
    
    if (g_analytics_engine.mutex) {
        pthread_mutex_destroy(g_analytics_engine.mutex);
        free(g_analytics_engine.mutex);
    }
    
    if (g_analytics_engine.dashboard_metrics) {
        free(g_analytics_engine.dashboard_metrics);
    }
    
    g_analytics_engine.mutex = NULL;
    g_analytics_engine.dashboard_metrics = NULL;
    g_analytics_engine_initialized = false;
}

// Start analytics engine
int analytics_engine_start(void) {
    if (!g_analytics_engine_initialized || !g_analytics_engine.config.enabled) {
        return -1;
    }
    
    if (g_analytics_engine.status.running) {
        return 0; // Already running
    }
    
    // Initialize analytics components
    if (performance_analyzer_init() != 0) {
        return -1;
    }
    
    if (trend_analyzer_init(NULL) != 0) {
        performance_analyzer_cleanup();
        return -1;
    }
    
    health_thresholds_t health_thresholds = g_analytics_engine.config.health_thresholds;
    if (health_analyzer_init(&health_thresholds) != 0) {
        trend_analyzer_cleanup();
        performance_analyzer_cleanup();
        return -1;
    }
    
    // Initial metrics calculation
    if (analytics_engine_update_metrics() != 0) {
        health_analyzer_cleanup();
        trend_analyzer_cleanup();
        performance_analyzer_cleanup();
        return -1;
    }
    
    // Start analytics thread
    g_analytics_engine.status.running = true;
    g_analytics_engine.status.start_time = time(NULL);
    
    if (pthread_create(&g_analytics_engine.analytics_thread, NULL, 
                       analytics_thread, NULL) != 0) {
        g_analytics_engine.status.running = false;
        health_analyzer_cleanup();
        trend_analyzer_cleanup();
        performance_analyzer_cleanup();
        return -1;
    }
    
    return 0;
}

// Stop analytics engine
int analytics_engine_stop(void) {
    if (!g_analytics_engine_initialized || !g_analytics_engine.status.running) {
        return -1;
    }
    
    g_analytics_engine.status.running = false;
    pthread_join(g_analytics_engine.analytics_thread, NULL);
    
    // Clean up components
    health_analyzer_cleanup();
    trend_analyzer_cleanup();
    performance_analyzer_cleanup();
    
    return 0;
}

// Analytics thread
static void* analytics_thread(void* arg) {
    (void)arg; // Unused parameter
    
    while (g_analytics_engine.status.running) {
        time_t start_time = time(NULL);
        
        if (analytics_engine_update_metrics() != 0) {
            g_analytics_engine.status.error_count++;
        }
        
        time_t end_time = time(NULL);
        double duration = difftime(end_time, start_time);
        
        // Update average duration
        g_analytics_engine.status.update_duration_avg_ms = 
            (g_analytics_engine.status.update_duration_avg_ms * g_analytics_engine.status.update_count + 
             duration * 1000.0) / (g_analytics_engine.status.update_count + 1);
        
        // Sleep until next update
        time_t sleep_time = g_analytics_engine.config.update_interval_seconds - (time_t)duration;
        if (sleep_time > 0) {
            sleep((unsigned int)sleep_time);
        }
    }
    
    return NULL;
}

// Update analytics metrics
int analytics_engine_update_metrics(void) {
    if (!g_analytics_engine_initialized) {
        return -1;
    }
    
    pthread_mutex_lock(g_analytics_engine.mutex);
    
    time_t now = time(NULL);
    
    // Calculate system overview
    if (calculate_system_overview(&g_analytics_engine.dashboard_metrics->overview) != 0) {
        pthread_mutex_unlock(g_analytics_engine.mutex);
        return -1;
    }
    
    // Calculate performance metrics
    if (calculate_performance_metrics(&g_analytics_engine.dashboard_metrics->performance) != 0) {
        pthread_mutex_unlock(g_analytics_engine.mutex);
        return -1;
    }
    
    // Calculate health metrics
    if (calculate_health_metrics(&g_analytics_engine.dashboard_metrics->health) != 0) {
        pthread_mutex_unlock(g_analytics_engine.mutex);
        return -1;
    }
    
    // Generate alerts
    g_analytics_engine.dashboard_metrics->alert_count = 
        generate_analytics_alerts(g_analytics_engine.dashboard_metrics->alerts, 16);
    
    // Generate recommendations
    g_analytics_engine.dashboard_metrics->recommendation_count = 
        generate_analytics_recommendations(g_analytics_engine.dashboard_metrics->recommendations, 16);
    
    // Update timestamp
    g_analytics_engine.dashboard_metrics->timestamp = now;
    g_analytics_engine.status.last_update = now;
    g_analytics_engine.status.update_count++;
    
    pthread_mutex_unlock(g_analytics_engine.mutex);
    
    return 0;
}

// Calculate system overview
int calculate_system_overview(system_overview_t* overview) {
    if (!overview) return -1;
    
    // Placeholder implementation - would integrate with actual system data
    overview->total_members = 0;
    overview->active_members = 0;
    overview->overall_health = 0.0;
    strcpy(overview->uptime, "0h 0m");
    overview->last_failover = 0;
    overview->has_last_failover = false;
    overview->failover_count = 0;
    overview->success_rate = 0.0;
    
    return 0;
}

// Calculate performance metrics
int calculate_performance_metrics(member_performance_metrics_t* performance) {
    if (!performance) return -1;
    
    if (!performance_analyzer_is_initialized()) {
        return -1;
    }
    
    // Get performance analysis
    performance_analysis_t analysis;
    if (performance_analyzer_analyze(&analysis) != 0) {
        return -1;
    }
    
    performance->member_count = analysis.member_count;
    
    for (int i = 0; i < analysis.member_count && i < 16; i++) {
        strncpy(performance->member_names[i], analysis.member_names[i], 
                sizeof(performance->member_names[i]) - 1);
        
        performance->average_latency[i] = analysis.member_performance[i].average_latency;
        performance->average_loss[i] = analysis.member_performance[i].average_loss;
        performance->average_signal[i] = analysis.member_performance[i].average_signal;
        performance->throughput[i] = analysis.member_performance[i].throughput;
        performance->response_time[i] = analysis.member_performance[i].response_time;
        performance->error_rate[i] = analysis.member_performance[i].error_rate;
        performance->availability[i] = analysis.member_performance[i].availability;
    }
    
    return 0;
}

// Calculate health metrics
int calculate_health_metrics(health_metrics_t* health) {
    if (!health) return -1;
    
    if (!health_analyzer_is_initialized()) {
        return -1;
    }
    
    // Get health analysis
    health_analysis_t analysis;
    if (health_analyzer_analyze(&analysis) != 0) {
        return -1;
    }
    
    health->member_count = analysis.member_count;
    health->overall_health = analysis.overall_health;
    
    for (int i = 0; i < analysis.member_count && i < 16; i++) {
        strncpy(health->member_names[i], analysis.member_names[i], 
                sizeof(health->member_names[i]) - 1);
        
        health->member_health[i] = analysis.member_health[i];
    }
    
    return 0;
}

// Generate analytics alerts
static int generate_analytics_alerts(analytics_alert_t* alerts, int max_alerts) {
    if (!alerts || max_alerts <= 0) return 0;
    
    // Placeholder implementation - would generate alerts based on analysis
    int alert_count = 0;
    
    // Example alert for low overall health
    if (g_analytics_engine.dashboard_metrics && 
        g_analytics_engine.dashboard_metrics->health.overall_health < 50.0) {
        
        if (alert_count < max_alerts) {
            strcpy(alerts[alert_count].id, "low_health");
            strcpy(alerts[alert_count].type, "health");
            strcpy(alerts[alert_count].severity, "warning");
            strcpy(alerts[alert_count].title, "Low Overall Health");
            strcpy(alerts[alert_count].message, "System overall health is below 50%");
            alerts[alert_count].timestamp = time(NULL);
            alerts[alert_count].acknowledged = false;
            alert_count++;
        }
    }
    
    return alert_count;
}

// Generate analytics recommendations
static int generate_analytics_recommendations(analytics_recommendation_t* recommendations, 
                                             int max_recommendations) {
    if (!recommendations || max_recommendations <= 0) return 0;
    
    // Placeholder implementation - would generate recommendations based on analysis
    int recommendation_count = 0;
    
    // Example recommendation for performance improvement
    if (g_analytics_engine.dashboard_metrics && 
        g_analytics_engine.dashboard_metrics->performance.member_count > 0) {
        
        if (recommendation_count < max_recommendations) {
            strcpy(recommendations[recommendation_count].id, "perf_improve");
            strcpy(recommendations[recommendation_count].type, "performance");
            strcpy(recommendations[recommendation_count].priority, "medium");
            strcpy(recommendations[recommendation_count].title, "Performance Optimization");
            strcpy(recommendations[recommendation_count].description, 
                   "Consider optimizing network configuration for better performance");
            strcpy(recommendations[recommendation_count].impact, "medium");
            strcpy(recommendations[recommendation_count].effort, "low");
            recommendations[recommendation_count].timestamp = time(NULL);
            recommendation_count++;
        }
    }
    
    return recommendation_count;
}

// Get dashboard metrics
void analytics_engine_get_dashboard_metrics(dashboard_metrics_t* metrics) {
    if (!metrics || !g_analytics_engine_initialized) return;
    
    pthread_mutex_lock(g_analytics_engine.mutex);
    
    if (g_analytics_engine.dashboard_metrics) {
        *metrics = *g_analytics_engine.dashboard_metrics;
    } else {
        memset(metrics, 0, sizeof(dashboard_metrics_t));
        metrics->timestamp = time(NULL);
    }
    
    pthread_mutex_unlock(g_analytics_engine.mutex);
}

// Get member analytics
int analytics_engine_get_member_analytics(const char* member_name, int hours,
                                         member_performance_metrics_t* analytics) {
    if (!g_analytics_engine_initialized || !member_name || !analytics) {
        return -1;
    }
    
    if (!performance_analyzer_is_initialized()) {
        return -1;
    }
    
    // Get performance for specific member
    member_performance_t performance;
    if (performance_analyzer_get_member_performance(member_name, &performance) != 0) {
        return -1;
    }
    
    // Convert to performance metrics format
    analytics->member_count = 1;
    strncpy(analytics->member_names[0], member_name, sizeof(analytics->member_names[0]) - 1);
    analytics->member_names[0][sizeof(analytics->member_names[0]) - 1] = '\0';
    analytics->average_latency[0] = performance.average_latency;
    analytics->average_loss[0] = performance.average_loss;
    analytics->average_signal[0] = performance.average_signal;
    analytics->throughput[0] = performance.throughput;
    analytics->response_time[0] = performance.response_time;
    analytics->error_rate[0] = performance.error_rate;
    analytics->availability[0] = performance.availability;
    
    return 0;
}

// Get analytics engine status
void analytics_engine_get_status(analytics_engine_status_t* status) {
    if (!status || !g_analytics_engine_initialized) return;
    
    pthread_mutex_lock(g_analytics_engine.mutex);
    *status = g_analytics_engine.status;
    pthread_mutex_unlock(g_analytics_engine.mutex);
}

// Check if analytics engine is initialized
bool analytics_engine_is_initialized(void) {
    return g_analytics_engine_initialized;
}

// Check if analytics engine is running
bool analytics_engine_is_running(void) {
    return g_analytics_engine_initialized && g_analytics_engine.status.running;
}

// Get analytics engine instance
analytics_engine_t* analytics_engine_get_instance(void) {
    return g_analytics_engine_initialized ? &g_analytics_engine : NULL;
}
