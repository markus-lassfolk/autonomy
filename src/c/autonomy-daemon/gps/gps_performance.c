#include "gps_performance.h"
#include "../utils/logx.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

// Performance tracking configuration
static const int MAX_PERFORMANCE_HISTORY = 10000;           // Maximum performance history entries
static const int PERFORMANCE_UPDATE_INTERVAL = 60;           // 1 minute performance update interval
static const int PERFORMANCE_WINDOW_SIZE = 3600;             // 1 hour performance window
static const double MIN_ACCURACY_THRESHOLD = 1.0;            // Minimum accuracy threshold in meters
static const double MAX_ACCURACY_THRESHOLD = 100.0;          // Maximum accuracy threshold in meters

// Performance metrics weights
static const double ACCURACY_WEIGHT = 0.35;                  // Accuracy weight in scoring
static const double RELIABILITY_WEIGHT = 0.25;               // Reliability weight in scoring
static const double SPEED_WEIGHT = 0.20;                     // Speed weight in scoring
static const double CONSISTENCY_WEIGHT = 0.20;               // Consistency weight in scoring

// Global performance tracking state
static gps_performance_t g_performance = {0};
static bool g_performance_initialized = false;
static pthread_mutex_t g_performance_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize GPS performance tracking
int gps_performance_init(void) {
    if (g_performance_initialized) {
        LOGX_WARN_MSG("GPS performance tracking already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_performance_mutex);
    
    // Initialize performance state
    memset(&g_performance, 0, sizeof(gps_performance_t));
    g_performance.enabled = true;
    g_performance.max_history_entries = MAX_PERFORMANCE_HISTORY;
    g_performance.update_interval = PERFORMANCE_UPDATE_INTERVAL;
    g_performance.window_size = PERFORMANCE_WINDOW_SIZE;
    g_performance.min_accuracy_threshold = MIN_ACCURACY_THRESHOLD;
    g_performance.max_accuracy_threshold = MAX_ACCURACY_THRESHOLD;
    
    g_performance.history_entry_count = 0;
    g_performance.total_measurements = 0;
    g_performance.successful_measurements = 0;
    g_performance.failed_measurements = 0;
    g_performance.last_update = 0;
    
    // Initialize performance history
    for (int i = 0; i < MAX_PERFORMANCE_HISTORY; i++) {
        g_performance.performance_history[i].active = false;
        g_performance.performance_history[i].timestamp = 0;
        g_performance.performance_history[i].source_id = 0;
        g_performance.performance_history[i].accuracy = 0.0;
        g_performance.performance_history[i].response_time = 0.0;
        g_performance.performance_history[i].reliability_score = 0.0;
        g_performance.performance_history[i].consistency_score = 0.0;
        g_performance.performance_history[i].overall_score = 0.0;
    }
    
    // Initialize source performance tracking
    for (int i = 0; i < GPS_MAX_SOURCES; i++) {
        g_performance.source_performance[i].source_id = i;
        g_performance.source_performance[i].total_measurements = 0;
        g_performance.source_performance[i].successful_measurements = 0;
        g_performance.source_performance[i].failed_measurements = 0;
        g_performance.source_performance[i].total_accuracy = 0.0;
        g_performance.source_performance[i].total_response_time = 0.0;
        g_performance.source_performance[i].best_accuracy = 999999.0;
        g_performance.source_performance[i].worst_accuracy = 0.0;
        g_performance.source_performance[i].average_accuracy = 0.0;
        g_performance.source_performance[i].average_response_time = 0.0;
        g_performance.source_performance[i].reliability_score = 0.0;
        g_performance.source_performance[i].consistency_score = 0.0;
        g_performance.source_performance[i].overall_score = 0.0;
        g_performance.source_performance[i].last_update = 0;
        g_performance.source_performance[i].uptime = 0;
        g_performance.source_performance[i].downtime = 0;
        g_performance.source_performance[i].availability = 0.0;
    }
    
    g_performance_initialized = true;
    pthread_mutex_unlock(&g_performance_mutex);
    
    LOGX_INFO_MSG("GPS performance tracking initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Record GPS performance measurement
int gps_performance_record_measurement(int source_id, double accuracy, double response_time, bool success) {
    if (!g_performance_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_performance_mutex);
    
    g_performance.total_measurements++;
    if (success) {
        g_performance.successful_measurements++;
    } else {
        g_performance.failed_measurements++;
    }
    
    // Add to performance history
    add_performance_history_entry(source_id, accuracy, response_time, success);
    
    // Update source performance
    update_source_performance(source_id, accuracy, response_time, success);
    
    // Calculate overall performance metrics
    calculate_overall_performance();
    
    g_performance.last_update = time(NULL);
    
    pthread_mutex_unlock(&g_performance_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Add performance history entry
static void add_performance_history_entry(int source_id, double accuracy, double response_time, bool success) {
    // Find free history slot
    int slot_index = -1;
    for (int i = 0; i < g_performance.max_history_entries; i++) {
        if (!g_performance.performance_history[i].active) {
            slot_index = i;
            break;
        }
    }
    
    if (slot_index < 0) {
        // Remove oldest entry to make room
        slot_index = find_oldest_performance_entry();
        if (slot_index >= 0) {
            g_performance.performance_history[slot_index].active = false;
            g_performance.history_entry_count--;
        }
    }
    
    if (slot_index >= 0) {
        gps_performance_entry_t *entry = &g_performance.performance_history[slot_index];
        
        entry->active = true;
        entry->timestamp = time(NULL);
        entry->source_id = source_id;
        entry->accuracy = accuracy;
        entry->response_time = response_time;
        entry->success = success;
        
        // Calculate scores
        entry->reliability_score = calculate_reliability_score(source_id);
        entry->consistency_score = calculate_consistency_score(source_id);
        entry->overall_score = calculate_overall_score(entry->reliability_score, entry->consistency_score, accuracy, response_time);
        
        if (slot_index >= g_performance.history_entry_count) {
            g_performance.history_entry_count = slot_index + 1;
        }
    }
}

// Find oldest performance entry
static int find_oldest_performance_entry(void) {
    int oldest_index = -1;
    time_t oldest_time = time(NULL);
    
    for (int i = 0; i < g_performance.max_history_entries; i++) {
        if (g_performance.performance_history[i].active && 
            g_performance.performance_history[i].timestamp < oldest_time) {
            oldest_time = g_performance.performance_history[i].timestamp;
            oldest_index = i;
        }
    }
    
    return oldest_index;
}

// Update source performance
static void update_source_performance(int source_id, double accuracy, double response_time, bool success) {
    if (source_id < 0 || source_id >= GPS_MAX_SOURCES) {
        return;
    }
    
    gps_source_performance_t *source = &g_performance.source_performance[source_id];
    
    source->total_measurements++;
    if (success) {
        source->successful_measurements++;
    } else {
        source->failed_measurements++;
    }
    
    // Update accuracy statistics
    source->total_accuracy += accuracy;
    if (accuracy < source->best_accuracy) {
        source->best_accuracy = accuracy;
    }
    if (accuracy > source->worst_accuracy) {
        source->worst_accuracy = accuracy;
    }
    source->average_accuracy = source->total_accuracy / source->total_measurements;
    
    // Update response time statistics
    source->total_response_time += response_time;
    source->average_response_time = source->total_response_time / source->total_measurements;
    
    // Update reliability score
    source->reliability_score = calculate_source_reliability(source);
    
    // Update consistency score
    source->consistency_score = calculate_source_consistency(source);
    
    // Update overall score
    source->overall_score = calculate_source_overall_score(source);
    
    // Update availability
    source->uptime = source->successful_measurements;
    source->downtime = source->failed_measurements;
    if (source->total_measurements > 0) {
        source->availability = (double)source->successful_measurements / source->total_measurements;
    }
    
    source->last_update = time(NULL);
}

// Calculate reliability score for a source
static double calculate_source_reliability(const gps_source_performance_t *source) {
    if (source->total_measurements == 0) {
        return 0.0;
    }
    
    // Base reliability on success rate
    double success_rate = (double)source->successful_measurements / source->total_measurements;
    
    // Factor in accuracy consistency
    double accuracy_consistency = 1.0;
    if (source->total_measurements > 1) {
        // Calculate coefficient of variation for accuracy
        double accuracy_std = calculate_accuracy_standard_deviation(source);
        if (source->average_accuracy > 0.0) {
            double cv = accuracy_std / source->average_accuracy;
            accuracy_consistency = fmax(0.0, 1.0 - cv);
        }
    }
    
    // Factor in response time consistency
    double response_consistency = 1.0;
    if (source->total_measurements > 1) {
        // Calculate coefficient of variation for response time
        double response_std = calculate_response_time_standard_deviation(source);
        if (source->average_response_time > 0.0) {
            double cv = response_std / source->average_response_time;
            response_consistency = fmax(0.0, 1.0 - cv);
        }
    }
    
    // Weighted reliability score
    double reliability = (success_rate * 0.6) + (accuracy_consistency * 0.25) + (response_consistency * 0.15);
    
    return fmin(1.0, fmax(0.0, reliability));
}

// Calculate consistency score for a source
static double calculate_source_consistency(const gps_source_performance_t *source) {
    if (source->total_measurements < 2) {
        return 0.0;
    }
    
    // Calculate accuracy consistency
    double accuracy_consistency = 1.0;
    double accuracy_std = calculate_accuracy_standard_deviation(source);
    if (source->average_accuracy > 0.0) {
        double cv = accuracy_std / source->average_accuracy;
        accuracy_consistency = fmax(0.0, 1.0 - cv);
    }
    
    // Calculate response time consistency
    double response_consistency = 1.0;
    double response_std = calculate_response_time_standard_deviation(source);
    if (source->average_response_time > 0.0) {
        double cv = response_std / source->average_response_time;
        response_consistency = fmax(0.0, 1.0 - cv);
    }
    
    // Weighted consistency score
    double consistency = (accuracy_consistency * 0.7) + (response_consistency * 0.3);
    
    return fmin(1.0, fmax(0.0, consistency));
}

// Calculate accuracy standard deviation
static double calculate_accuracy_standard_deviation(const gps_source_performance_t *source) {
    if (source->total_measurements < 2) {
        return 0.0;
    }
    
    // Calculate variance from recent measurements
    double total_variance = 0.0;
    int valid_measurements = 0;
    
    for (int i = 0; i < g_performance.history_entry_count; i++) {
        if (g_performance.performance_history[i].active && 
            g_performance.performance_history[i].source_id == source->source_id) {
            double diff = g_performance.performance_history[i].accuracy - source->average_accuracy;
            total_variance += diff * diff;
            valid_measurements++;
        }
    }
    
    if (valid_measurements < 2) {
        return 0.0;
    }
    
    return sqrt(total_variance / (valid_measurements - 1));
}

// Calculate response time standard deviation
static double calculate_response_time_standard_deviation(const gps_source_performance_t *source) {
    if (source->total_measurements < 2) {
        return 0.0;
    }
    
    // Calculate variance from recent measurements
    double total_variance = 0.0;
    int valid_measurements = 0;
    
    for (int i = 0; i < g_performance.history_entry_count; i++) {
        if (g_performance.performance_history[i].active && 
            g_performance.performance_history[i].source_id == source->source_id) {
            double diff = g_performance.performance_history[i].response_time - source->average_response_time;
            total_variance += diff * diff;
            valid_measurements++;
        }
    }
    
    if (valid_measurements < 2) {
        return 0.0;
    }
    
    return sqrt(total_variance / (valid_measurements - 1));
}

// Calculate source overall score
static double calculate_source_overall_score(const gps_source_performance_t *source) {
    // Weighted combination of reliability, consistency, accuracy, and response time
    double accuracy_score = 1.0 - (source->average_accuracy / g_performance.max_accuracy_threshold);
    accuracy_score = fmin(1.0, fmax(0.0, accuracy_score));
    
    double response_score = 1.0 - (source->average_response_time / 5000.0); // 5 second max
    response_score = fmin(1.0, fmax(0.0, response_score));
    
    double overall_score = (source->reliability_score * RELIABILITY_WEIGHT) +
                          (source->consistency_score * CONSISTENCY_WEIGHT) +
                          (accuracy_score * ACCURACY_WEIGHT) +
                          (response_score * SPEED_WEIGHT);
    
    return fmin(1.0, fmax(0.0, overall_score));
}

// Calculate reliability score for history entry
static double calculate_reliability_score(int source_id) {
    if (source_id < 0 || source_id >= GPS_MAX_SOURCES) {
        return 0.0;
    }
    
    return g_performance.source_performance[source_id].reliability_score;
}

// Calculate consistency score for history entry
static double calculate_consistency_score(int source_id) {
    if (source_id < 0 || source_id >= GPS_MAX_SOURCES) {
        return 0.0;
    }
    
    return g_performance.source_performance[source_id].consistency_score;
}

// Calculate overall score for history entry
static double calculate_overall_score(double reliability, double consistency, double accuracy, double response_time) {
    double accuracy_score = 1.0 - (accuracy / g_performance.max_accuracy_threshold);
    accuracy_score = fmin(1.0, fmax(0.0, accuracy_score));
    
    double response_score = 1.0 - (response_time / 5000.0); // 5 second max
    response_score = fmin(1.0, fmax(0.0, response_score));
    
    double overall_score = (reliability * RELIABILITY_WEIGHT) +
                          (consistency * CONSISTENCY_WEIGHT) +
                          (accuracy_score * ACCURACY_WEIGHT) +
                          (response_score * SPEED_WEIGHT);
    
    return fmin(1.0, fmax(0.0, overall_score));
}

// Calculate overall performance metrics
static void calculate_overall_performance(void) {
    if (g_performance.total_measurements == 0) {
        return;
    }
    
    // Calculate overall success rate
    g_performance.overall_success_rate = (double)g_performance.successful_measurements / g_performance.total_measurements;
    
    // Calculate average accuracy across all sources
    double total_accuracy = 0.0;
    double total_response_time = 0.0;
    int active_sources = 0;
    
    for (int i = 0; i < GPS_MAX_SOURCES; i++) {
        if (g_performance.source_performance[i].total_measurements > 0) {
            total_accuracy += g_performance.source_performance[i].average_accuracy;
            total_response_time += g_performance.source_performance[i].average_response_time;
            active_sources++;
        }
    }
    
    if (active_sources > 0) {
        g_performance.overall_average_accuracy = total_accuracy / active_sources;
        g_performance.overall_average_response_time = total_response_time / active_sources;
    }
    
    // Calculate overall reliability and consistency
    double total_reliability = 0.0;
    double total_consistency = 0.0;
    
    for (int i = 0; i < GPS_MAX_SOURCES; i++) {
        if (g_performance.source_performance[i].total_measurements > 0) {
            total_reliability += g_performance.source_performance[i].reliability_score;
            total_consistency += g_performance.source_performance[i].consistency_score;
        }
    }
    
    if (active_sources > 0) {
        g_performance.overall_reliability = total_reliability / active_sources;
        g_performance.overall_consistency = total_consistency / active_sources;
    }
    
    // Calculate overall score
    g_performance.overall_score = (g_performance.overall_reliability * RELIABILITY_WEIGHT) +
                                  (g_performance.overall_consistency * CONSISTENCY_WEIGHT) +
                                  ((1.0 - g_performance.overall_average_accuracy / g_performance.max_accuracy_threshold) * ACCURACY_WEIGHT) +
                                  ((1.0 - g_performance.overall_average_response_time / 5000.0) * SPEED_WEIGHT);
    
    g_performance.overall_score = fmin(1.0, fmax(0.0, g_performance.overall_score));
}

// Get GPS performance status
int gps_performance_get_status(gps_performance_status_t *status) {
    if (!g_performance_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_performance_mutex);
    
    status->enabled = g_performance.enabled;
    status->history_entry_count = g_performance.history_entry_count;
    status->max_history_entries = g_performance.max_history_entries;
    status->total_measurements = g_performance.total_measurements;
    status->successful_measurements = g_performance.successful_measurements;
    status->failed_measurements = g_performance.failed_measurements;
    status->last_update = g_performance.last_update;
    status->overall_success_rate = g_performance.overall_success_rate;
    status->overall_average_accuracy = g_performance.overall_average_accuracy;
    status->overall_average_response_time = g_performance.overall_average_response_time;
    status->overall_reliability = g_performance.overall_reliability;
    status->overall_consistency = g_performance.overall_consistency;
    status->overall_score = g_performance.overall_score;
    
    pthread_mutex_unlock(&g_performance_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get source performance
int gps_performance_get_source_performance(int source_id, gps_source_performance_t *source_perf) {
    if (!g_performance_initialized || !source_perf || source_id < 0 || source_id >= GPS_MAX_SOURCES) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_performance_mutex);
    
    memcpy(source_perf, &g_performance.source_performance[source_id], sizeof(gps_source_performance_t));
    
    pthread_mutex_unlock(&g_performance_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get all source performance data
int gps_performance_get_all_sources(gps_source_performance_t *sources, int max_sources) {
    if (!g_performance_initialized || !sources || max_sources <= 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_performance_mutex);
    
    int count = 0;
    for (int i = 0; i < GPS_MAX_SOURCES && count < max_sources; i++) {
        if (g_performance.source_performance[i].total_measurements > 0) {
            memcpy(&sources[count], &g_performance.source_performance[i], sizeof(gps_source_performance_t));
            count++;
        }
    }
    
    pthread_mutex_unlock(&g_performance_mutex);
    
    return count;
}

// Get performance history
int gps_performance_get_history(gps_performance_entry_t *history, int max_entries, time_t since) {
    if (!g_performance_initialized || !history || max_entries <= 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_performance_mutex);
    
    int count = 0;
    for (int i = 0; i < g_performance.history_entry_count && count < max_entries; i++) {
        if (g_performance.performance_history[i].active && 
            g_performance.performance_history[i].timestamp >= since) {
            memcpy(&history[count], &g_performance.performance_history[i], sizeof(gps_performance_entry_t));
            count++;
        }
    }
    
    pthread_mutex_unlock(&g_performance_mutex);
    
    return count;
}

// Get performance configuration
int gps_performance_get_config(gps_performance_config_t *config) {
    if (!g_performance_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_performance_mutex);
    
    config->enabled = g_performance.enabled;
    config->max_history_entries = g_performance.max_history_entries;
    config->update_interval = g_performance.update_interval;
    config->window_size = g_performance.window_size;
    config->min_accuracy_threshold = g_performance.min_accuracy_threshold;
    config->max_accuracy_threshold = g_performance.max_accuracy_threshold;
    
    pthread_mutex_unlock(&g_performance_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set performance configuration
int gps_performance_set_config(const gps_performance_config_t *config) {
    if (!g_performance_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_performance_mutex);
    
    g_performance.enabled = config->enabled;
    g_performance.max_history_entries = config->max_history_entries;
    g_performance.update_interval = config->update_interval;
    g_performance.window_size = config->window_size;
    g_performance.min_accuracy_threshold = config->min_accuracy_threshold;
    g_performance.max_accuracy_threshold = config->max_accuracy_threshold;
    
    pthread_mutex_unlock(&g_performance_mutex);
    
    LOGX_INFO_MSG("GPS performance tracking configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable performance tracking
int gps_performance_set_enabled(bool enabled) {
    if (!g_performance_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_performance_mutex);
    g_performance.enabled = enabled;
    pthread_mutex_unlock(&g_performance_mutex);
    
    LOGX_INFO_MSG("GPS performance tracking %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Reset performance tracking
int gps_performance_reset(void) {
    if (!g_performance_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_performance_mutex);
    
    g_performance.history_entry_count = 0;
    g_performance.total_measurements = 0;
    g_performance.successful_measurements = 0;
    g_performance.failed_measurements = 0;
    g_performance.last_update = 0;
    
    // Clear performance history
    for (int i = 0; i < MAX_PERFORMANCE_HISTORY; i++) {
        g_performance.performance_history[i].active = false;
        g_performance.performance_history[i].timestamp = 0;
        g_performance.performance_history[i].source_id = 0;
        g_performance.performance_history[i].accuracy = 0.0;
        g_performance.performance_history[i].response_time = 0.0;
        g_performance.performance_history[i].reliability_score = 0.0;
        g_performance.performance_history[i].consistency_score = 0.0;
        g_performance.performance_history[i].overall_score = 0.0;
    }
    
    // Reset source performance
    for (int i = 0; i < GPS_MAX_SOURCES; i++) {
        gps_source_performance_t *source = &g_performance.source_performance[i];
        source->total_measurements = 0;
        source->successful_measurements = 0;
        source->failed_measurements = 0;
        source->total_accuracy = 0.0;
        source->total_response_time = 0.0;
        source->best_accuracy = 999999.0;
        source->worst_accuracy = 0.0;
        source->average_accuracy = 0.0;
        source->average_response_time = 0.0;
        source->reliability_score = 0.0;
        source->consistency_score = 0.0;
        source->overall_score = 0.0;
        source->last_update = 0;
        source->uptime = 0;
        source->downtime = 0;
        source->availability = 0.0;
    }
    
    pthread_mutex_unlock(&g_performance_mutex);
    
    LOGX_INFO_MSG("GPS performance tracking reset");
    return AUTONOMY_SUCCESS;
}

// Cleanup performance tracking
void gps_performance_cleanup(void) {
    if (!g_performance_initialized) {
        return;
    }
    
    pthread_mutex_destroy(&g_performance_mutex);
    g_performance_initialized = false;
    
    LOGX_INFO_MSG("GPS performance tracking cleaned up");
}
