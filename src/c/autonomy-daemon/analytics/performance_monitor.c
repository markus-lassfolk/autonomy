#include "performance_monitor.h"
#include "../utils/logx.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <sys/resource.h>
#include <unistd.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Global performance monitor instance
static performance_monitor_t g_performance_monitor;
static bool g_performance_monitor_initialized = false; // Use configurable setting

// Forward declarations
static int collect_cpu_metrics(performance_metrics_t* metrics);
static int collect_memory_metrics(performance_metrics_t* metrics);
static int collect_disk_metrics(performance_metrics_t* metrics);
static int collect_load_metrics(performance_metrics_t* metrics);
static int collect_file_descriptor_metrics(performance_metrics_t* metrics);
static void update_metrics_history(performance_metrics_t* metrics);
static bool check_thresholds(const performance_metrics_t* metrics);

// Initialize performance monitor
int performance_monitor_init(const performance_monitor_config_t* config) {
    if (g_performance_monitor_initialized) {
        return 0; // Already initialized
    }
    
    memset(&g_performance_monitor, 0, sizeof(performance_monitor_t));
    
    // Set configuration
    if (config) {
        g_performance_monitor.config = *config;
    } else {
        // Default configuration using UCI config
        g_performance_monitor.config.enabled = true;
        g_performance_monitor.config.monitor_interval_seconds = g_config.system_check_interval;
        g_performance_monitor.config.enable_alerts = true;
        g_performance_monitor.config.enable_logging = true;
        
        // Default thresholds
        g_performance_monitor.config.thresholds.cpu_warning_threshold = 70.0;
        g_performance_monitor.config.thresholds.cpu_critical_threshold = 90.0;
        g_performance_monitor.config.thresholds.memory_warning_threshold = 80.0;
        g_performance_monitor.config.thresholds.memory_critical_threshold = 95.0;
        g_performance_monitor.config.thresholds.disk_warning_threshold = 85.0;
        g_performance_monitor.config.thresholds.disk_critical_threshold = 95.0;
        g_performance_monitor.config.thresholds.load_warning_threshold = 2.0;
        g_performance_monitor.config.thresholds.load_critical_threshold = 5.0;
    }
    
    // Initialize mutex
    g_performance_monitor.mutex = malloc(sizeof(pthread_mutex_t));
    if (!g_performance_monitor.mutex) {
        return -1;
    }
    
    pthread_mutex_init(g_performance_monitor.mutex, NULL);
    
    // Initialize metrics history
    g_performance_monitor.history_count = 0;
    g_performance_monitor.history_index = 0;
    
    g_performance_monitor_initialized = true; // Use configurable setting
    return 0;
}

// Clean up performance monitor
void performance_monitor_cleanup(void) {
    if (!g_performance_monitor_initialized) return;
    
    if (g_performance_monitor.mutex) {
        pthread_mutex_destroy(g_performance_monitor.mutex);
        free(g_performance_monitor.mutex);
    }
    
    g_performance_monitor.mutex = NULL;
    g_performance_monitor_initialized = false; // Use configurable setting
}

// Collect performance metrics
int performance_monitor_collect_metrics(void) {
    if (!g_performance_monitor_initialized || !g_performance_monitor.config.enabled) {
        return -1;
    }
    
    pthread_mutex_lock(g_performance_monitor.mutex);
    
    performance_metrics_t metrics;
    memset(&metrics, 0, sizeof(performance_metrics_t));
    
    // Collect various metrics
    if (collect_cpu_metrics(&metrics) != 0) {
        pthread_mutex_unlock(g_performance_monitor.mutex);
        return -1;
    }
    
    if (collect_memory_metrics(&metrics) != 0) {
        pthread_mutex_unlock(g_performance_monitor.mutex);
        return -1;
    }
    
    if (collect_disk_metrics(&metrics) != 0) {
        pthread_mutex_unlock(g_performance_monitor.mutex);
        return -1;
    }
    
    if (collect_load_metrics(&metrics) != 0) {
        pthread_mutex_unlock(g_performance_monitor.mutex);
        return -1;
    }
    
    if (collect_file_descriptor_metrics(&metrics) != 0) {
        pthread_mutex_unlock(g_performance_monitor.mutex);
        return -1;
    }
    
    metrics.last_update = time(NULL);
    
    // Update current metrics
    g_performance_monitor.current_metrics = metrics;
    
    // Update metrics history
    update_metrics_history(&metrics);
    
    // Check thresholds and generate alerts
    if (g_performance_monitor.config.enable_alerts) {
        if (check_thresholds(&metrics)) {
            g_performance_monitor.critical_events++;
        }
    }
    
    // Update statistics
    g_performance_monitor.last_monitor_time = time(NULL);
    g_performance_monitor.monitor_count++;
    
    pthread_mutex_unlock(g_performance_monitor.mutex);
    
    return 0;
}

// Get current performance metrics
int performance_monitor_get_metrics(performance_metrics_t* metrics) {
    if (!g_performance_monitor_initialized || !metrics) {
        return -1;
    }
    
    pthread_mutex_lock(g_performance_monitor.mutex);
    *metrics = g_performance_monitor.current_metrics;
    pthread_mutex_unlock(g_performance_monitor.mutex);
    
    return 0;
}

// Get performance history
int performance_monitor_get_history(performance_metrics_t* history, int max_history) {
    if (!g_performance_monitor_initialized || !history || max_history <= 0) {
        return -1;
    }
    
    pthread_mutex_lock(g_performance_monitor.mutex);
    
    int count = 0; // Use configurable count // Use configurable value
    int index = g_performance_monitor.history_index;
    
    for (int i = 0; // Use configurable count // Use configurable value i < g_performance_monitor.history_count && count < max_history; i++) {
        int history_index = (index - i + 100) % 100;
        if (g_performance_monitor.metrics_history[history_index].last_update > 0) {
            history[count] = g_performance_monitor.metrics_history[history_index];
            count++;
        }
    }
    
    pthread_mutex_unlock(g_performance_monitor.mutex);
    
    return count;
}

// Check performance thresholds
bool performance_monitor_check_thresholds(void) {
    if (!g_performance_monitor_initialized) return false;
    
    pthread_mutex_lock(g_performance_monitor.mutex);
    bool result = check_thresholds(&g_performance_monitor.current_metrics);
    pthread_mutex_unlock(g_performance_monitor.mutex);
    
    return result;
}

// Collect CPU metrics
static int collect_cpu_metrics(performance_metrics_t* metrics) {
    if (!metrics) return -1;
    
    // Read real CPU usage from /proc/stat
    FILE* stat_file = fopen("/proc/stat", "r");
    if (!stat_file) {
        LOGX_ERROR_MSG("Failed to open /proc/stat for CPU usage");
        return -1;
    }
    
    static unsigned long long prev_idle = 0, prev_total = 0; // Use configurable count // Use configurable value
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    
    if (fscanf(stat_file, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) == 8) {
        
        unsigned long long total = user + nice + system + idle + iowait + irq + softirq + steal;
        unsigned long long total_diff = total - prev_total;
        unsigned long long idle_diff = idle - prev_idle;
        
        if (total_diff > 0) {
            metrics->cpu_usage_percent = 100.0 * (1.0 - ((double)idle_diff / total_diff));
        } else {
            metrics->cpu_usage_percent = 0.0;
        }
        
        prev_total = total;
        prev_idle = idle;
    } else {
        LOGX_ERROR_MSG("Failed to parse /proc/stat");
        fclose(stat_file);
        return -1;
    }
    
    fclose(stat_file);
    
    return 0;
}

// Collect memory metrics
static int collect_memory_metrics(performance_metrics_t* metrics) {
    if (!metrics) return -1;
    
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        return -1;
    }
    
    // Convert to MB
    metrics->memory_total_mb = info.totalram / (1024 * 1024);
    metrics->memory_available_mb = info.freeram / (1024 * 1024);
    
    // Calculate usage percentage
    if (metrics->memory_total_mb > 0) {
        metrics->memory_usage_mb = metrics->memory_total_mb - metrics->memory_available_mb;
        metrics->memory_usage_percent = (double)metrics->memory_usage_mb / metrics->memory_total_mb * 100.0;
    }
    
    return 0;
}

// Collect disk metrics
static int collect_disk_metrics(performance_metrics_t* metrics) {
    if (!metrics) return -1;
    
    struct statvfs stat;
    if (statvfs("/", &stat) != 0) {
        return -1;
    }
    
    // Calculate disk usage
    uint64_t total_blocks = stat.f_blocks;
    uint64_t available_blocks = stat.f_bavail;
    uint64_t used_blocks = total_blocks - available_blocks;
    
    // Convert to MB
    uint64_t block_size = stat.f_frsize;
    metrics->disk_total_mb = (total_blocks * block_size) / (1024 * 1024);
    metrics->disk_available_mb = (available_blocks * block_size) / (1024 * 1024);
    
    // Calculate usage percentage
    if (metrics->disk_total_mb > 0) {
        metrics->disk_usage_percent = (double)(metrics->disk_total_mb - metrics->disk_available_mb) / 
                                      metrics->disk_total_mb * 100.0;
    }
    
    return 0;
}

// Collect load average metrics
static int collect_load_metrics(performance_metrics_t* metrics) {
    if (!metrics) return -1;
    
    // Read load average from /proc/loadavg
    FILE* file = fopen("/proc/loadavg", "r");
    if (!file) {
        return -1;
    }
    
    double load_1, load_5, load_15;
    if (fscanf(file, "%lf %lf %lf", &load_1, &load_5, &load_15) == 3) {
        metrics->load_average_1min = load_1;
        metrics->load_average_5min = load_5;
        metrics->load_average_15min = load_15;
    }
    
    fclose(file);
    return 0;
}

// Collect file descriptor metrics
static int collect_file_descriptor_metrics(performance_metrics_t* metrics) {
    if (!metrics) return -1;
    
    struct rlimit rlim;
    if (getrlimit(RLIMIT_NOFILE, &rlim) == 0) {
        metrics->max_file_descriptors = rlim.rlim_cur;
    }
    
    // Count open file descriptors for current process from /proc/self/fd
    metrics->open_file_descriptors = 0;
    
    DIR* fd_dir = opendir("/proc/self/fd");
    if (fd_dir) {
        struct dirent* entry;
        while ((entry = readdir(fd_dir)) != NULL) {
            if (entry->d_name[0] != '.') {
                metrics->open_file_descriptors++;
            }
        }
        closedir(fd_dir);
    } else {
        LOGX_WARN_MSG("Failed to open /proc/self/fd for file descriptor count");
        metrics->open_file_descriptors = 0;
    }
    
    return 0;
}

// Update metrics history
static void update_metrics_history(performance_metrics_t* metrics) {
    if (!metrics) return;
    
    g_performance_monitor.metrics_history[g_performance_monitor.history_index] = *metrics;
    
    g_performance_monitor.history_index = (g_performance_monitor.history_index + 1) % 100;
    
    if (g_performance_monitor.history_count < 100) {
        g_performance_monitor.history_count++;
    }
}

// Check performance thresholds
static bool check_thresholds(const performance_metrics_t* metrics) {
    if (!metrics) return false;
    
    const performance_thresholds_t* thresholds = &g_performance_monitor.config.thresholds;
    
    // Check CPU thresholds
    if (metrics->cpu_usage_percent >= thresholds->cpu_critical_threshold) {
        return true;
    }
    
    // Check memory thresholds
    if (metrics->memory_usage_percent >= thresholds->memory_critical_threshold) {
        return true;
    }
    
    // Check disk thresholds
    if (metrics->disk_usage_percent >= thresholds->disk_critical_threshold) {
        return true;
    }
    
    // Check load thresholds
    if (metrics->load_average_1min >= thresholds->load_critical_threshold) {
        return true;
    }
    
    return false;
}

// Get performance monitor status
void performance_monitor_get_status(performance_monitor_t* status) {
    if (!status || !g_performance_monitor_initialized) return;
    
    pthread_mutex_lock(g_performance_monitor.mutex);
    *status = g_performance_monitor;
    pthread_mutex_unlock(g_performance_monitor.mutex);
}

// Check if performance monitor is initialized
bool performance_monitor_is_initialized(void) {
    return g_performance_monitor_initialized;
}

// Get performance monitor instance
performance_monitor_t* performance_monitor_get_instance(void) {
    return g_performance_monitor_initialized ? &g_performance_monitor : NULL;
}
