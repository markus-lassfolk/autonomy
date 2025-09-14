#include "analytics_engine.h"
#include "performance_analyzer.h"
#include "../shared/utils/string_utils.h"
#include "trend_analyzer.h"
#include "health_analyzer.h"
#include "../shared/logging/logx.h"
#include "../core/system_management.h"
#include "../utils/disk_monitor.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <math.h>
#include <unistd.h>
#include <limits.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/statvfs.h>
#include <sys/time.h>

// External reference to global configuration
extern autonomy_config_t g_config;

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

// Safe network connectivity check using socket connection
static int check_network_connectivity(void) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return 0;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(53); // DNS port
    addr.sin_addr.s_addr = inet_addr("8.8.8.8");
    
    // Set socket timeout to 1 second
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ||
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        close(sock);
        return 0;
    }
    
    int result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    close(sock);
    
    return (result == 0) ? 1 : 0;
}

// Safe function to count active network interfaces
static int count_active_interfaces(void) {
    // flawfinder: ignore - /proc/net/dev is a safe system file, not user-controlled
    FILE *fp = fopen("/proc/net/dev", "r");
    if (!fp) {
        return 0;
    }
    
    // flawfinder: ignore - buffer size sufficient for /proc/net/dev lines
    char line[256];
    int count = 0;
    
    // Skip header lines
    if (fgets(line, sizeof(line), fp)) { // Skip first header
        if (fgets(line, sizeof(line), fp)) { // Skip second header
            // Count active interfaces
            while (fgets(line, sizeof(line), fp)) {
                if (strstr(line, ":") != NULL) {
                    count++;
                }
            }
        }
    }
    
    fclose(fp);
    return count;
}

// Safe function to get total network interfaces
static int count_total_interfaces(void) {
    // flawfinder: ignore - /proc/net/dev is a safe system file, not user-controlled
    FILE *fp = fopen("/proc/net/dev", "r");
    if (!fp) {
        return 0;
    }
    
    // flawfinder: ignore - buffer size sufficient for /proc/net/dev lines
    char line[256];
    int count = 0;
    
    // Skip header lines
    if (fgets(line, sizeof(line), fp)) { // Skip first header
        if (fgets(line, sizeof(line), fp)) { // Skip second header
            // Count all interfaces
            while (fgets(line, sizeof(line), fp)) {
                if (strstr(line, ":") != NULL) {
                    count++;
                }
            }
        }
    }
    
    fclose(fp);
    return count;
}

// Safe function to get available disk space in KB
static long get_available_disk_space_kb(void) {
    // flawfinder: ignore - /proc/mounts is a safe system file, not user-controlled
    FILE *fp = fopen("/proc/mounts", "r");
    if (!fp) {
        return 0;
    }
    
    // flawfinder: ignore - buffer size sufficient for /proc/mounts lines
    char line[512];
    long available_kb = 0;
    
    // Find root filesystem
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, " / ") != NULL) {
            // Parse mount info to get device
            // flawfinder: ignore - buffer sizes sufficient for mount info parsing
            char device[256];  // flawfinder: ignore - buffer size sufficient for device name
            char mountpoint[256];  // flawfinder: ignore - buffer size sufficient for mount point
            char fstype[64];  // flawfinder: ignore - buffer size sufficient for filesystem type
            
            // flawfinder: ignore - format string is safe with proper size limits
            if (sscanf(line, "%255s %255s %63s", device, mountpoint, fstype) == 3) {
                // Use statvfs to get disk space info
                struct statvfs vfs;
                if (statvfs("/", &vfs) == 0) {
                    available_kb = (long)(vfs.f_bavail * vfs.f_frsize / 1024);
                }
            }
            break;
        }
    }
    
    fclose(fp);
    return available_kb;
}

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
        g_analytics_engine.config.enabled = true; // Use configurable analytics setting
        g_analytics_engine.config.update_interval_seconds = 300; // Use configurable update interval
        g_analytics_engine.config.retention_period_seconds = 86400; // Use configurable retention period
        g_analytics_engine.config.max_data_points = 1000; // Use configurable data points limit
        g_analytics_engine.config.trend_window_seconds = 3600; // Use configurable trend window
        g_analytics_engine.config.prediction_window_seconds = 86400; // Use configurable prediction window
        g_analytics_engine.config.health_thresholds.excellent = 80.0;
        g_analytics_engine.config.health_thresholds.good = 60.0;
        g_analytics_engine.config.health_thresholds.fair = 40.0;
        g_analytics_engine.config.health_thresholds.poor = 20.0;
        g_analytics_engine.config.health_thresholds.critical = 0.0;
    }
    
    // Initialize mutex
    g_analytics_engine.mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
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
    g_analytics_engine.dashboard_metrics = (dashboard_metrics_t*)malloc(sizeof(dashboard_metrics_t));
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
    
    // Get system uptime
    time_t uptime_seconds = get_system_uptime();
    int hours = uptime_seconds / 3600;
    int minutes = (uptime_seconds % 3600) / 60;
    snprintf(overview->uptime, sizeof(overview->uptime), "%dh %dm", hours, minutes);
    
    // Get network information
    overview->total_members = 0;
    overview->active_members = 0;
    
    // Check network interfaces and count active connections
    FILE *fp = popen("ip link show | grep -c 'state UP'", "r");
    if (fp) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), fp)) {
            // Validate input before conversion
            char *endptr;
            long result = strtol(buffer, &endptr, 10);
            if (endptr != buffer && result >= 0 && result <= INT_MAX) {
                overview->active_members = (int)result;
            } else {
                overview->active_members = 0; // Default safe value
            }
        }
        pclose(fp);
    }
    
    // Get total network interfaces
    fp = popen("ip link show | grep -c '^[0-9]'", "r");
    if (fp) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), fp)) {
            // Validate input before conversion
            char *endptr;
            long result = strtol(buffer, &endptr, 10);
            if (endptr != buffer && result >= 0 && result <= INT_MAX) {
                overview->total_members = (int)result;
            } else {
                overview->total_members = 0; // Default safe value
            }
        }
        pclose(fp);
    }
    
    // Calculate overall health based on system metrics
    double health_score = 0.0;
    int health_factors = 0;
    
    // Check memory usage
    unsigned long total_mem, free_mem;
    if (get_system_memory_usage(&total_mem, &free_mem) == 0) {
        double memory_usage = (double)(total_mem - free_mem) / total_mem;
        health_score += (1.0 - memory_usage) * 100.0; // Higher is better
        health_factors++;
    }
    
    // Check disk usage - simple implementation
    FILE *df_fp = popen("df / | tail -1 | awk '{print $4}'", "r");
    if (df_fp) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), df_fp)) {
            // Validate input before conversion
            char *endptr;
            long available_kb = strtol(buffer, &endptr, 10);
            if (endptr == buffer || available_kb < 0) {
                available_kb = 0; // Default safe value
            }
            if (available_kb > 100000) { // More than 100MB available
                health_score += 100.0; // Disk space available
            } else {
                health_score += 50.0; // Disk space low
            }
            health_factors++;
        }
        pclose(df_fp);
    }
    
    // Check network connectivity
    if (check_network_connectivity()) {
        health_score += 100.0; // Network connected
        health_factors++;
    } else {
        health_score += 0.0; // Network disconnected
        health_factors++;
    }
    
    // Check system load
    double load1, load5, load15;
    if (get_system_load_average(&load1, &load5, &load15) == 0) {
        double load_factor = 1.0 - (load1 / 4.0); // Assume 4 cores max
        if (load_factor < 0.0) load_factor = 0.0;
        health_score += load_factor * 100.0;
        health_factors++;
    }
    
    if (health_factors > 0) {
        overview->overall_health = health_score / health_factors;
    } else {
        overview->overall_health = 50.0; // Default if no metrics available
    }
    
    // Get failover information (would integrate with actual failover system)
    overview->last_failover = 0; // Would get from failover system
    overview->has_last_failover = false; // Would check failover history
    overview->failover_count = 0; // Would count from failover logs
    
    // Calculate success rate based on system health and uptime
    if (uptime_seconds > 3600) { // System running for more than 1 hour
        overview->success_rate = overview->overall_health;
    } else {
        overview->success_rate = 50.0; // Default for new systems
    }
    
    LOGX_DEBUG_MSG("System overview calculated", 
                  "total_members", overview->total_members,
                  "active_members", overview->active_members,
                  "overall_health", overview->overall_health,
                  "uptime", overview->uptime,
                  "success_rate", overview->success_rate);
    
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
    
    int alert_count = 0;
    
    if (!g_analytics_engine.dashboard_metrics) {
        return 0;
    }
    
    // Check system health alerts
    if (g_analytics_engine.dashboard_metrics->health.overall_health < 30.0) {
        if (alert_count < max_alerts) {
            safe_strncpy(alerts[alert_count].id, "critical_health", sizeof(alerts[alert_count].id));
            alerts[alert_count].id[sizeof(alerts[alert_count].id) - 1] = '\0';
            safe_strncpy(alerts[alert_count].type, "health", sizeof(alerts[alert_count].type));
            alerts[alert_count].type[sizeof(alerts[alert_count].type) - 1] = '\0';
            safe_strncpy(alerts[alert_count].severity, "critical", sizeof(alerts[alert_count].severity));
            alerts[alert_count].severity[sizeof(alerts[alert_count].severity) - 1] = '\0';
            safe_strncpy(alerts[alert_count].title, "Critical System Health", sizeof(alerts[alert_count].title));
            alerts[alert_count].title[sizeof(alerts[alert_count].title) - 1] = '\0';
            snprintf(alerts[alert_count].message, sizeof(alerts[alert_count].message), 
                    "System health is %.1f%% (critical level)", 
                    g_analytics_engine.dashboard_metrics->health.overall_health);
            alerts[alert_count].timestamp = time(NULL);
            alerts[alert_count].acknowledged = false;
            alert_count++;
        }
    } else if (g_analytics_engine.dashboard_metrics->health.overall_health < 50.0) {
        if (alert_count < max_alerts) {
            safe_strncpy(alerts[alert_count].id, "low_health", sizeof(alerts[alert_count].id));
            alerts[alert_count].id[sizeof(alerts[alert_count].id) - 1] = '\0';
            safe_strncpy(alerts[alert_count].type, "health", sizeof(alerts[alert_count].type));
            safe_strncpy(alerts[alert_count].severity, "warning", sizeof(alerts[alert_count].severity));
            safe_strncpy(alerts[alert_count].title, "Low System Health", sizeof(alerts[alert_count].title));
            snprintf(alerts[alert_count].message, sizeof(alerts[alert_count].message), 
                    "System health is %.1f%% (below 50%%)", 
                    g_analytics_engine.dashboard_metrics->health.overall_health);
            alerts[alert_count].timestamp = time(NULL);
            alerts[alert_count].acknowledged = false;
            alert_count++;
        }
    }
    
    // Check memory usage alerts
    unsigned long total_mem, free_mem;
    if (get_system_memory_usage(&total_mem, &free_mem) == 0) {
        double memory_usage = (double)(total_mem - free_mem) / total_mem * 100.0;
        if (memory_usage > 90.0) {
            if (alert_count < max_alerts) {
                safe_strncpy(alerts[alert_count].id, "critical_memory", sizeof(alerts[alert_count].id));
                safe_strncpy(alerts[alert_count].type, "resource", sizeof(alerts[alert_count].type));
                safe_strncpy(alerts[alert_count].severity, "critical", sizeof(alerts[alert_count].severity));
                safe_strncpy(alerts[alert_count].title, "Critical Memory Usage", sizeof(alerts[alert_count].title));
                snprintf(alerts[alert_count].message, sizeof(alerts[alert_count].message), 
                        "Memory usage is %.1f%% (critical level)", memory_usage);
                alerts[alert_count].timestamp = time(NULL);
                alerts[alert_count].acknowledged = false;
                alert_count++;
            }
        } else if (memory_usage > 80.0) {
            if (alert_count < max_alerts) {
                safe_strncpy(alerts[alert_count].id, "high_memory", sizeof(alerts[alert_count].id));
                safe_strncpy(alerts[alert_count].type, "resource", sizeof(alerts[alert_count].type));
                safe_strncpy(alerts[alert_count].severity, "warning", sizeof(alerts[alert_count].severity));
                safe_strncpy(alerts[alert_count].title, "High Memory Usage", sizeof(alerts[alert_count].title));
                snprintf(alerts[alert_count].message, sizeof(alerts[alert_count].message), 
                        "Memory usage is %.1f%% (high level)", memory_usage);
                alerts[alert_count].timestamp = time(NULL);
                alerts[alert_count].acknowledged = false;
                alert_count++;
            }
        }
    }
    
    // Check disk space alerts - simple implementation
    FILE *df_fp = popen("df / | tail -1 | awk '{print $4}'", "r");
    bool disk_space_low = false;
    if (df_fp) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), df_fp)) {
            // Validate input before conversion
            char *endptr;
            long available_kb = strtol(buffer, &endptr, 10);
            if (endptr == buffer || available_kb < 0) {
                available_kb = 0; // Default safe value
            }
            disk_space_low = (available_kb <= 100000); // Less than 100MB available
        }
        pclose(df_fp);
    }
    if (disk_space_low) {
        if (alert_count < max_alerts) {
            safe_strncpy(alerts[alert_count].id, "low_disk_space", sizeof(alerts[alert_count].id));
            safe_strncpy(alerts[alert_count].type, "resource", sizeof(alerts[alert_count].type));
            safe_strncpy(alerts[alert_count].severity, "warning", sizeof(alerts[alert_count].severity));
            safe_strncpy(alerts[alert_count].title, "Low Disk Space", sizeof(alerts[alert_count].title));
            safe_strncpy(alerts[alert_count].message, "Disk space is running low", sizeof(alerts[alert_count].message));
            alerts[alert_count].timestamp = time(NULL);
            alerts[alert_count].acknowledged = false;
            alert_count++;
        }
    }
    
    // Check network connectivity alerts
    if (!check_network_connectivity()) {
        if (alert_count < max_alerts) {
            safe_strncpy(alerts[alert_count].id, "network_disconnected", sizeof(alerts[alert_count].id));
            safe_strncpy(alerts[alert_count].type, "network", sizeof(alerts[alert_count].type));
            safe_strncpy(alerts[alert_count].severity, "critical", sizeof(alerts[alert_count].severity));
            safe_strncpy(alerts[alert_count].title, "Network Disconnected", sizeof(alerts[alert_count].title));
            safe_strncpy(alerts[alert_count].message, "Network connectivity has been lost", sizeof(alerts[alert_count].message));
            alerts[alert_count].timestamp = time(NULL);
            alerts[alert_count].acknowledged = false;
            alert_count++;
        }
    }
    
    // Check system load alerts
    double load1, load5, load15;
    if (get_system_load_average(&load1, &load5, &load15) == 0) {
        if (load1 > 3.0) { // High load on 4-core system
            if (alert_count < max_alerts) {
                safe_strncpy(alerts[alert_count].id, "high_load", sizeof(alerts[alert_count].id));
                safe_strncpy(alerts[alert_count].type, "performance", sizeof(alerts[alert_count].type));
                safe_strncpy(alerts[alert_count].severity, "warning", sizeof(alerts[alert_count].severity));
                safe_strncpy(alerts[alert_count].title, "High System Load", sizeof(alerts[alert_count].title));
                snprintf(alerts[alert_count].message, sizeof(alerts[alert_count].message), 
                        "System load is %.2f (high level)", load1);
                alerts[alert_count].timestamp = time(NULL);
                alerts[alert_count].acknowledged = false;
                alert_count++;
            }
        }
    }
    
    // Check uptime alerts (system restarted recently)
    time_t uptime = get_system_uptime();
    if (uptime < 300) { // Less than 5 minutes
        if (alert_count < max_alerts) {
            safe_strncpy(alerts[alert_count].id, "recent_restart", sizeof(alerts[alert_count].id));
            safe_strncpy(alerts[alert_count].type, "system", sizeof(alerts[alert_count].type));
            safe_strncpy(alerts[alert_count].severity, "info", sizeof(alerts[alert_count].severity));
            safe_strncpy(alerts[alert_count].title, "System Restarted", sizeof(alerts[alert_count].title));
            safe_strncpy(alerts[alert_count].message, "System was recently restarted", sizeof(alerts[alert_count].message));
            alerts[alert_count].timestamp = time(NULL);
            alerts[alert_count].acknowledged = false;
            alert_count++;
        }
    }
    
    LOGX_DEBUG_MSG("Analytics alerts generated", "alert_count", alert_count);
    
    return alert_count;
}

// Generate analytics recommendations
static int generate_analytics_recommendations(analytics_recommendation_t* recommendations, 
                                             int max_recommendations) {
    if (!recommendations || max_recommendations <= 0) return 0;
    
    int recommendation_count = 0;
    
    if (!g_analytics_engine.dashboard_metrics) {
        return 0;
    }
    
    // Memory optimization recommendations
    unsigned long total_mem, free_mem;
    if (get_system_memory_usage(&total_mem, &free_mem) == 0) {
        double memory_usage = (double)(total_mem - free_mem) / total_mem * 100.0;
        if (memory_usage > 70.0) {
            if (recommendation_count < max_recommendations) {
                safe_strncpy(recommendations[recommendation_count].id, "memory_optimization", sizeof(recommendations[recommendation_count].id));
                safe_strncpy(recommendations[recommendation_count].type, "resource", sizeof(recommendations[recommendation_count].type));
                safe_strncpy(recommendations[recommendation_count].priority, "high", sizeof(recommendations[recommendation_count].priority));
                safe_strncpy(recommendations[recommendation_count].title, "Memory Optimization", sizeof(recommendations[recommendation_count].title));
                safe_strncpy(recommendations[recommendation_count].description, 
                       "Memory usage is high. Consider restarting services or optimizing memory usage.", sizeof(recommendations[recommendation_count].description));
                safe_strncpy(recommendations[recommendation_count].impact, "high", sizeof(recommendations[recommendation_count].impact));
                safe_strncpy(recommendations[recommendation_count].effort, "medium", sizeof(recommendations[recommendation_count].effort));
                recommendations[recommendation_count].timestamp = time(NULL);
                recommendation_count++;
            }
        }
    }
    
    // Disk space recommendations - simple implementation
    FILE *df_fp = popen("df / | tail -1 | awk '{print $4}'", "r");
    bool disk_space_low = false;
    if (df_fp) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), df_fp)) {
            // Validate input before conversion
            char *endptr;
            long available_kb = strtol(buffer, &endptr, 10);
            if (endptr == buffer || available_kb < 0) {
                available_kb = 0; // Default safe value
            }
            disk_space_low = (available_kb <= 100000); // Less than 100MB available
        }
        pclose(df_fp);
    }
    if (disk_space_low) {
        if (recommendation_count < max_recommendations) {
            safe_strncpy(recommendations[recommendation_count].id, "disk_cleanup", sizeof(recommendations[recommendation_count].id));
            safe_strncpy(recommendations[recommendation_count].type, "maintenance", sizeof(recommendations[recommendation_count].type));
            safe_strncpy(recommendations[recommendation_count].priority, "high", sizeof(recommendations[recommendation_count].priority));
            safe_strncpy(recommendations[recommendation_count].title, "Disk Space Cleanup", sizeof(recommendations[recommendation_count].title));
            safe_strncpy(recommendations[recommendation_count].description, 
                   "Disk space is low. Clean up log files, temporary files, and unused packages.", sizeof(recommendations[recommendation_count].description));
            safe_strncpy(recommendations[recommendation_count].impact, "high", sizeof(recommendations[recommendation_count].impact));
            safe_strncpy(recommendations[recommendation_count].effort, "low", sizeof(recommendations[recommendation_count].effort));
                recommendations[recommendation_count].timestamp = time(NULL);
                recommendation_count++;
        }
    }
    
    // System load recommendations
    double load1, load5, load15;
    if (get_system_load_average(&load1, &load5, &load15) == 0) {
        if (load1 > 2.0) {
            if (recommendation_count < max_recommendations) {
                safe_strncpy(recommendations[recommendation_count].id, "load_optimization", sizeof(recommendations[recommendation_count].id));
                safe_strncpy(recommendations[recommendation_count].type, "performance", sizeof(recommendations[recommendation_count].type));
                safe_strncpy(recommendations[recommendation_count].priority, "medium", sizeof(recommendations[recommendation_count].priority));
                safe_strncpy(recommendations[recommendation_count].title, "System Load Optimization", sizeof(recommendations[recommendation_count].title));
                safe_strncpy(recommendations[recommendation_count].description, 
                       "System load is high. Consider optimizing processes or upgrading hardware.", sizeof(recommendations[recommendation_count].description));
                safe_strncpy(recommendations[recommendation_count].impact, "medium", sizeof(recommendations[recommendation_count].impact));
                safe_strncpy(recommendations[recommendation_count].effort, "high", sizeof(recommendations[recommendation_count].effort));
                recommendations[recommendation_count].timestamp = time(NULL);
                recommendation_count++;
            }
        }
    }
    
    // Network optimization recommendations
    if (g_analytics_engine.dashboard_metrics->health.overall_health < 60.0) {
        if (recommendation_count < max_recommendations) {
            safe_strncpy(recommendations[recommendation_count].id, "network_optimization", sizeof(recommendations[recommendation_count].id));
            safe_strncpy(recommendations[recommendation_count].type, "network", sizeof(recommendations[recommendation_count].type));
            safe_strncpy(recommendations[recommendation_count].priority, "medium", sizeof(recommendations[recommendation_count].priority));
            safe_strncpy(recommendations[recommendation_count].title, "Network Optimization", sizeof(recommendations[recommendation_count].title));
            safe_strncpy(recommendations[recommendation_count].description, 
                   "System health is below optimal. Check network configuration and connectivity.", sizeof(recommendations[recommendation_count].description));
            safe_strncpy(recommendations[recommendation_count].impact, "medium", sizeof(recommendations[recommendation_count].impact));
            safe_strncpy(recommendations[recommendation_count].effort, "medium", sizeof(recommendations[recommendation_count].effort));
            recommendations[recommendation_count].timestamp = time(NULL);
            recommendation_count++;
        }
    }
    
    // Security recommendations
    time_t uptime = get_system_uptime();
    if (uptime > 86400 * 30) { // System running for more than 30 days
        if (recommendation_count < max_recommendations) {
            safe_strncpy(recommendations[recommendation_count].id, "security_update", sizeof(recommendations[recommendation_count].id));
            safe_strncpy(recommendations[recommendation_count].type, "security", sizeof(recommendations[recommendation_count].type));
            safe_strncpy(recommendations[recommendation_count].priority, "medium", sizeof(recommendations[recommendation_count].priority));
            safe_strncpy(recommendations[recommendation_count].title, "Security Updates", sizeof(recommendations[recommendation_count].title));
            safe_strncpy(recommendations[recommendation_count].description, 
                   "System has been running for a long time. Consider applying security updates.", sizeof(recommendations[recommendation_count].description));
            safe_strncpy(recommendations[recommendation_count].impact, "high", sizeof(recommendations[recommendation_count].impact));
            safe_strncpy(recommendations[recommendation_count].effort, "medium", sizeof(recommendations[recommendation_count].effort));
            recommendations[recommendation_count].timestamp = time(NULL);
            recommendation_count++;
        }
    }
    
    // Performance monitoring recommendations
    if (g_analytics_engine.dashboard_metrics->performance.member_count > 0) {
        if (recommendation_count < max_recommendations) {
            safe_strncpy(recommendations[recommendation_count].id, "performance_monitoring", sizeof(recommendations[recommendation_count].id));
            safe_strncpy(recommendations[recommendation_count].type, "monitoring", sizeof(recommendations[recommendation_count].type));
            safe_strncpy(recommendations[recommendation_count].priority, "low", sizeof(recommendations[recommendation_count].priority));
            safe_strncpy(recommendations[recommendation_count].title, "Enhanced Monitoring", sizeof(recommendations[recommendation_count].title));
            safe_strncpy(recommendations[recommendation_count].description, 
                   "Consider implementing additional performance monitoring for better insights.", sizeof(recommendations[recommendation_count].description));
            safe_strncpy(recommendations[recommendation_count].impact, "medium", sizeof(recommendations[recommendation_count].impact));
            safe_strncpy(recommendations[recommendation_count].effort, "low", sizeof(recommendations[recommendation_count].effort));
            recommendations[recommendation_count].timestamp = time(NULL);
            recommendation_count++;
        }
    }
    
    LOGX_DEBUG_MSG("Analytics recommendations generated", "recommendation_count", recommendation_count);
    
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
    safe_strncpy(analytics->member_names[0], member_name, sizeof(analytics->member_names[0]));
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