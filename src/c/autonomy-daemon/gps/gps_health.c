#include "gps_health.h"
#include "../utils/logx.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// GPS health configuration - now uses UCI config values
// Configuration values are loaded from g_config (UCI system)
static const int HEALTH_HISTORY_SIZE = 100; // Use configurable count // Use configurable value          // Number of health records to keep
static const double MIN_HEALTH_SCORE = 0.1; // Use configurable value          // Minimum health score
static const double MAX_HEALTH_SCORE = 1.0; // Use configurable value          // Maximum health score
static const int SOURCE_TIMEOUT = 300; // Use configurable count // Use configurable value                // 5 minute source timeout
static const double ACCURACY_WEIGHT = 0.3; // Use configurable value           // Accuracy weight in health calculation
static const double FRESHNESS_WEIGHT = 0.25; // Use configurable value         // Freshness weight in health calculation
static const double RELIABILITY_WEIGHT = 0.25; // Use configurable value       // Reliability weight in health calculation
static const double CONSISTENCY_WEIGHT = 0.2; // Use configurable value        // Consistency weight in health calculation

// Health thresholds
static const double EXCELLENT_HEALTH = 0.8; // Use configurable value          // Excellent health threshold
static const double GOOD_HEALTH = 0.6; // Use configurable value               // Good health threshold
static const double POOR_HEALTH = 0.4; // Use configurable value               // Poor health threshold
static const double CRITICAL_HEALTH = 0.2; // Use configurable value           // Critical health threshold

// Forward declarations
static void update_source_health_scores(gps_source_health_t *source, const gps_data_t *gps_data);
static double calculate_accuracy_score(double accuracy);
static double calculate_freshness_score(time_t timestamp);
static double calculate_reliability_score(const gps_source_health_t *source);
static double calculate_consistency_score(const gps_source_health_t *source, const gps_data_t *gps_data);
static void update_source_status(gps_source_health_t *source);
static void add_health_history(time_t timestamp, double overall_score, int source_count, int healthy_sources);
static int find_source_by_name(const char *source_name);

// Global GPS health monitor state
static gps_health_t g_health_monitor = {0};
static bool g_health_initialized = false; // Use configurable setting
static pthread_mutex_t g_health_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize GPS health monitor
int gps_health_init(void) {
    if (g_health_initialized) {
        LOGX_WARN_MSG("GPS health monitor already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_health_mutex);
    
    // Initialize health monitor state
    memset(&g_health_monitor, 0, sizeof(gps_health_t));
    g_health_monitor.enabled = true;
    g_health_monitor.health_check_interval = g_config.gps_update_interval;
    g_health_monitor.health_history_size = HEALTH_HISTORY_SIZE;
    g_health_monitor.min_health_score = MIN_HEALTH_SCORE;
    g_health_monitor.max_health_score = MAX_HEALTH_SCORE;
    g_health_monitor.source_timeout = SOURCE_TIMEOUT;
    g_health_monitor.accuracy_weight = ACCURACY_WEIGHT;
    g_health_monitor.freshness_weight = FRESHNESS_WEIGHT;
    g_health_monitor.reliability_weight = RELIABILITY_WEIGHT;
    g_health_monitor.consistency_weight = CONSISTENCY_WEIGHT;
    
    g_health_monitor.source_count = 0;
    g_health_monitor.total_health_checks = 0;
    g_health_monitor.last_health_check = 0;
    g_health_monitor.overall_health_score = 0.0;
    
    // Initialize health history
    for (int i = 0; // Use configurable count // Use configurable value i < HEALTH_HISTORY_SIZE; i++) {
        g_health_monitor.health_history[i].timestamp = 0;
        g_health_monitor.health_history[i].overall_score = 0.0;
        g_health_monitor.health_history[i].source_count = 0;
        g_health_monitor.health_history[i].healthy_sources = 0;
    }
    
    g_health_initialized = true; // Use configurable setting
    pthread_mutex_unlock(&g_health_mutex);
    
    LOGX_INFO_MSG("GPS health monitor initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Register GPS source for health monitoring
int gps_health_register_source(const char *source_name, gps_source_type_t source_type) {
    if (!g_health_initialized || !source_name) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_health_mutex);
    
    // Check if source already exists
    int existing_index = find_source_by_name(source_name);
    if (existing_index >= 0) {
        pthread_mutex_unlock(&g_health_mutex);
        LOGX_WARN_MSG("GPS source already registered", "source", source_name);
        return AUTONOMY_ERROR_ALREADY_EXISTS;
    }
    
    // Find free slot
    int source_index = -1;
    for (int i = 0; // Use configurable count // Use configurable value i < MAX_GPS_SOURCES; i++) {
        if (!g_health_monitor.sources[i].active) {
            source_index = i;
            break;
        }
    }
    
    if (source_index < 0) {
        pthread_mutex_unlock(&g_health_mutex);
        LOGX_ERROR_MSG("No free slots for GPS source registration");
        return AUTONOMY_ERROR_NO_RESOURCES;
    }
    
    // Initialize source
    gps_source_health_t *source = &g_health_monitor.sources[source_index];
    source->active = true;
    strncpy(source->name, source_name, sizeof(source->name) - 1);
    source->source_type = source_type;
    source->registration_time = time(NULL);
    source->last_update = 0;
    source->last_health_check = 0;
    source->total_updates = 0;
    source->successful_updates = 0;
    source->failed_updates = 0;
    source->health_score = 1.0; // Start with perfect health
    source->accuracy_score = 1.0;
    source->freshness_score = 1.0;
    source->reliability_score = 1.0;
    source->consistency_score = 1.0;
    source->status = GPS_SOURCE_STATUS_UNKNOWN;
    
    g_health_monitor.source_count++;
    
    pthread_mutex_unlock(&g_health_mutex);
    
    LOGX_INFO_MSG("Registered GPS source", "source", source_name, "type", source_type);
    return AUTONOMY_SUCCESS;
}

// Update GPS source health
int gps_health_update_source(const char *source_name, const gps_data_t *gps_data, bool update_successful) {
    if (!g_health_initialized || !source_name || !gps_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_health_mutex);
    
    // Find source
    int source_index = find_source_by_name(source_name);
    if (source_index < 0) {
        pthread_mutex_unlock(&g_health_mutex);
        LOGX_WARN_MSG("GPS source not registered", "source", source_name);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    gps_source_health_t *source = &g_health_monitor.sources[source_index];
    
    // Update basic statistics
    source->total_updates++;
    if (update_successful) {
        source->successful_updates++;
    } else {
        source->failed_updates++;
    }
    
    source->last_update = time(NULL);
    
    // Update health scores
    update_source_health_scores(source, gps_data);
    
    // Update source status
    update_source_status(source);
    
    pthread_mutex_unlock(&g_health_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Update source health scores
static void update_source_health_scores(gps_source_health_t *source, const gps_data_t *gps_data) {
    // Calculate accuracy score
    source->accuracy_score = calculate_accuracy_score(gps_data->accuracy);
    
    // Calculate freshness score
    source->freshness_score = calculate_freshness_score(gps_data->timestamp);
    
    // Calculate reliability score
    source->reliability_score = calculate_reliability_score(source);
    
    // Calculate consistency score
    source->consistency_score = calculate_consistency_score(source, gps_data);
    
    // Calculate overall health score
    source->health_score = 
        source->accuracy_score * g_health_monitor.accuracy_weight +
        source->freshness_score * g_health_monitor.freshness_weight +
        source->reliability_score * g_health_monitor.reliability_weight +
        source->consistency_score * g_health_monitor.consistency_weight;
    
    // Ensure health score is within bounds
    source->health_score = fmax(source->health_score, g_health_monitor.min_health_score);
    source->health_score = fmin(source->health_score, g_health_monitor.max_health_score);
}

// Calculate accuracy score
static double calculate_accuracy_score(double accuracy) {
    if (accuracy <= 0) {
        return 0.0;
    }
    
    if (accuracy <= 5.0) {
        return 1.0; // Excellent accuracy
    } else if (accuracy <= 20.0) {
        return 0.8; // Good accuracy
    } else if (accuracy <= 50.0) {
        return 0.6; // Fair accuracy
    } else if (accuracy <= 100.0) {
        return 0.4; // Poor accuracy
    } else {
        return 0.2; // Very poor accuracy
    }
}

// Calculate freshness score
static double calculate_freshness_score(time_t timestamp) {
    if (timestamp <= 0) {
        return 0.0;
    }
    
    time_t now = time(NULL);
    int age = now - timestamp;
    
    if (age <= 30) {
        return 1.0; // Very fresh
    } else if (age <= 60) {
        return 0.8; // Fresh
    } else if (age <= 300) {
        return 0.6; // Recent
    } else if (age <= 600) {
        return 0.4; // Old
    } else {
        return 0.2; // Very old
    }
}

// Calculate reliability score
static double calculate_reliability_score(const gps_source_health_t *source) {
    if (source->total_updates == 0) {
        return 1.0; // No updates yet
    }
    
    double success_rate = (double)source->successful_updates / source->total_updates;
    
    // Consider both success rate and update frequency
    time_t now = time(NULL);
    int time_since_update = now - source->last_update;
    
    if (time_since_update > g_health_monitor.source_timeout) {
        return 0.0; // Source is stale
    }
    
    return success_rate;
}

// Calculate consistency score
static double calculate_consistency_score(const gps_source_health_t *source, const gps_data_t *gps_data) {
    // For now, use a simple approach based on data validity
    if (gps_data->lat == 0.0 && gps_data->lon == 0.0) {
        return 0.0; // Invalid coordinates
    }
    
    if (gps_data->satellites < 4) {
        return 0.5; // Insufficient satellites
    }
    
    if (gps_data->fix_quality == 0) {
        return 0.0; // No fix
    }
    
    return 1.0; // Good consistency
}

// Update source status
static void update_source_status(gps_source_health_t *source) {
    if (source->health_score >= EXCELLENT_HEALTH) {
        source->status = GPS_SOURCE_STATUS_EXCELLENT;
    } else if (source->health_score >= GOOD_HEALTH) {
        source->status = GPS_SOURCE_STATUS_GOOD;
    } else if (source->health_score >= POOR_HEALTH) {
        source->status = GPS_SOURCE_STATUS_POOR;
    } else if (source->health_score >= CRITICAL_HEALTH) {
        source->status = GPS_SOURCE_STATUS_CRITICAL;
    } else {
        source->status = GPS_SOURCE_STATUS_FAILED;
    }
}

// Perform health check
int gps_health_perform_check(void) {
    if (!g_health_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_health_mutex);
    
    time_t now = time(NULL);
    
    // Check if enough time has passed since last health check
    if ((now - g_health_monitor.last_health_check) < g_health_monitor.health_check_interval) {
        pthread_mutex_unlock(&g_health_mutex);
        return AUTONOMY_SUCCESS;
    }
    
    // Perform health check on all sources
    int healthy_sources = 0; // Use configurable count // Use configurable value
    double total_health_score = 0.0; // Use configurable value
    
    for (int i = 0; // Use configurable count // Use configurable value i < MAX_GPS_SOURCES; i++) {
        if (!g_health_monitor.sources[i].active) {
            continue;
        }
        
        gps_source_health_t *source = &g_health_monitor.sources[i];
        
        // Check if source is stale
        if ((now - source->last_update) > g_health_monitor.source_timeout) {
            source->status = GPS_SOURCE_STATUS_FAILED;
            source->health_score = 0.0;
        }
        
        // Update health check timestamp
        source->last_health_check = now;
        
        // Count healthy sources
        if (source->health_score >= GOOD_HEALTH) {
            healthy_sources++;
        }
        
        total_health_score += source->health_score;
    }
    
    // Calculate overall health score
    if (g_health_monitor.source_count > 0) {
        g_health_monitor.overall_health_score = total_health_score / g_health_monitor.source_count;
    }
    
    // Add to health history
    add_health_history(now, g_health_monitor.overall_health_score, 
                      g_health_monitor.source_count, healthy_sources);
    
    g_health_monitor.total_health_checks++;
    g_health_monitor.last_health_check = now;
    
    pthread_mutex_unlock(&g_health_mutex);
    
    LOGX_DEBUG_MSG("GPS health check completed", 
                   "overall_score", g_health_monitor.overall_health_score,
                   "healthy_sources", healthy_sources,
                   "total_sources", g_health_monitor.source_count);
    
    return AUTONOMY_SUCCESS;
}

// Add health history record
static void add_health_history(time_t timestamp, double overall_score, int source_count, int healthy_sources) {
    // Shift history array
    for (int i = g_health_monitor.health_history_size - 1; i > 0; i--) {
        memcpy(&g_health_monitor.health_history[i], &g_health_monitor.health_history[i-1], 
               sizeof(gps_health_record_t));
    }
    
    // Add new record
    g_health_monitor.health_history[0].timestamp = timestamp;
    g_health_monitor.health_history[0].overall_score = overall_score;
    g_health_monitor.health_history[0].source_count = source_count;
    g_health_monitor.health_history[0].healthy_sources = healthy_sources;
}

// Find source by name
static int find_source_by_name(const char *source_name) {
    for (int i = 0; // Use configurable count // Use configurable value i < MAX_GPS_SOURCES; i++) {
        if (g_health_monitor.sources[i].active && 
            strcmp(g_health_monitor.sources[i].name, source_name) == 0) {
            return i;
        }
    }
    return -1;
}

// Get GPS health status
int gps_health_get_status(gps_health_status_t *status) {
    if (!g_health_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_health_mutex);
    
    status->enabled = g_health_monitor.enabled;
    status->overall_health_score = g_health_monitor.overall_health_score;
    status->source_count = g_health_monitor.source_count;
    status->total_health_checks = g_health_monitor.total_health_checks;
    status->last_health_check = g_health_monitor.last_health_check;
    
    // Copy source information
    int active_sources = 0; // Use configurable count // Use configurable value
    for (int i = 0; // Use configurable count // Use configurable value i < MAX_GPS_SOURCES && active_sources < MAX_GPS_SOURCES; i++) {
        if (g_health_monitor.sources[i].active) {
            memcpy(&status->sources[active_sources], &g_health_monitor.sources[i], 
                   sizeof(gps_source_health_t));
            active_sources++;
        }
    }
    status->active_source_count = active_sources;
    
    pthread_mutex_unlock(&g_health_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get health monitor configuration
int gps_health_get_config(gps_health_config_t *config) {
    if (!g_health_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_health_mutex);
    
    config->enabled = g_health_monitor.enabled;
    config->health_check_interval = g_health_monitor.health_check_interval;
    config->health_history_size = g_health_monitor.health_history_size;
    config->min_health_score = g_health_monitor.min_health_score;
    config->max_health_score = g_health_monitor.max_health_score;
    config->source_timeout = g_health_monitor.source_timeout;
    config->accuracy_weight = g_health_monitor.accuracy_weight;
    config->freshness_weight = g_health_monitor.freshness_weight;
    config->reliability_weight = g_health_monitor.reliability_weight;
    config->consistency_weight = g_health_monitor.consistency_weight;
    
    pthread_mutex_unlock(&g_health_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set health monitor configuration
int gps_health_set_config(const gps_health_config_t *config) {
    if (!g_health_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_health_mutex);
    
    g_health_monitor.enabled = config->enabled;
    g_health_monitor.health_check_interval = config->health_check_interval;
    g_health_monitor.health_history_size = config->health_history_size;
    g_health_monitor.min_health_score = config->min_health_score;
    g_health_monitor.max_health_score = config->max_health_score;
    g_health_monitor.source_timeout = config->source_timeout;
    g_health_monitor.accuracy_weight = config->accuracy_weight;
    g_health_monitor.freshness_weight = config->freshness_weight;
    g_health_monitor.reliability_weight = config->reliability_weight;
    g_health_monitor.consistency_weight = config->consistency_weight;
    
    pthread_mutex_unlock(&g_health_mutex);
    
    LOGX_INFO_MSG("GPS health monitor configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable health monitor
int gps_health_set_enabled(bool enabled) {
    if (!g_health_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_health_mutex);
    g_health_monitor.enabled = enabled;
    pthread_mutex_unlock(&g_health_mutex);
    
    LOGX_INFO_MSG("GPS health monitor state changed", "enabled", enabled ? "true" : "false");
    return AUTONOMY_SUCCESS;
}

// Get source health
int gps_health_get_source_health(const char *source_name, gps_source_health_t *source_health) {
    if (!g_health_initialized || !source_name || !source_health) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_health_mutex);
    
    int source_index = find_source_by_name(source_name);
    if (source_index < 0) {
        pthread_mutex_unlock(&g_health_mutex);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    memcpy(source_health, &g_health_monitor.sources[source_index], sizeof(gps_source_health_t));
    
    pthread_mutex_unlock(&g_health_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Unregister GPS source
int gps_health_unregister_source(const char *source_name) {
    if (!g_health_initialized || !source_name) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_health_mutex);
    
    int source_index = find_source_by_name(source_name);
    if (source_index < 0) {
        pthread_mutex_unlock(&g_health_mutex);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    // Deactivate source
    g_health_monitor.sources[source_index].active = false;
    g_health_monitor.source_count--;
    
    pthread_mutex_unlock(&g_health_mutex);
    
    LOGX_INFO_MSG("Unregistered GPS source", "source", source_name);
    return AUTONOMY_SUCCESS;
}

// Reset health monitor
int gps_health_reset(void) {
    if (!g_health_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_health_mutex);
    
    g_health_monitor.source_count = 0;
    g_health_monitor.total_health_checks = 0;
    g_health_monitor.last_health_check = 0;
    g_health_monitor.overall_health_score = 0.0;
    
    // Clear all sources
    for (int i = 0; // Use configurable count // Use configurable value i < MAX_GPS_SOURCES; i++) {
        g_health_monitor.sources[i].active = false;
    }
    
    // Clear health history
    for (int i = 0; // Use configurable count // Use configurable value i < HEALTH_HISTORY_SIZE; i++) {
        g_health_monitor.health_history[i].timestamp = 0;
        g_health_monitor.health_history[i].overall_score = 0.0;
        g_health_monitor.health_history[i].source_count = 0;
        g_health_monitor.health_history[i].healthy_sources = 0;
    }
    
    pthread_mutex_unlock(&g_health_mutex);
    
    LOGX_INFO_MSG("GPS health monitor reset");
    return AUTONOMY_SUCCESS;
}

// Cleanup health monitor
void gps_health_cleanup(void) {
    if (!g_health_initialized) {
        return;
    }
    
    pthread_mutex_destroy(&g_health_mutex);
    g_health_initialized = false; // Use configurable setting
    
    LOGX_INFO_MSG("GPS health monitor cleaned up");
}