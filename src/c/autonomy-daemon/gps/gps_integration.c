#include "gps_integration.h"
#include "../utils/logx.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

// GPS integration configuration
static const int MAX_GPS_SOURCES = 10;                  // Maximum GPS sources
static const int GPS_UPDATE_INTERVAL = 1;               // 1 second GPS update interval
static const int INTEGRATION_CHECK_INTERVAL = 5;        // 5 second integration check interval
static const double MIN_GPS_ACCURACY = 100.0;           // 100m minimum accuracy threshold
static const int MAX_GPS_HISTORY = 1000;                // Maximum GPS history records

// Global GPS integration state
static gps_integration_t g_integration = {0};
static bool g_integration_initialized = false;
static pthread_mutex_t g_integration_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize GPS integration system
static int gps_integration_init(void) {
    if (g_integration_initialized) {
        LOGX_WARN("GPS integration already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_integration_mutex);
    
    // Initialize integration state
    memset(&g_integration, 0, sizeof(gps_integration_t));
    g_integration.enabled = true;
    g_integration.max_sources = MAX_GPS_SOURCES;
    g_integration.update_interval = GPS_UPDATE_INTERVAL;
    g_integration.check_interval = INTEGRATION_CHECK_INTERVAL;
    g_integration.min_accuracy = MIN_GPS_ACCURACY;
    g_integration.history_size = MAX_GPS_HISTORY;
    
    g_integration.source_count = 0;
    g_integration.active_sources = 0;
    g_integration.total_updates = 0;
    g_integration.last_update = 0;
    g_integration.last_integration_check = 0;
    
    // Initialize GPS sources array
    for (int i = 0; i < MAX_GPS_SOURCES; i++) {
        g_integration.gps_sources[i].active = false;
        g_integration.gps_sources[i].source_id = 0;
        g_integration.gps_sources[i].source_type = GPS_SOURCE_TYPE_UNKNOWN;
        g_integration.gps_sources[i].enabled = false;
        g_integration.gps_sources[i].last_update = 0;
        g_integration.gps_sources[i].update_count = 0;
        g_integration.gps_sources[i].health_score = 0.0;
        g_integration.gps_sources[i].reliability = 0.0;
    }
    
    // Initialize GPS history
    for (int i = 0; i < MAX_GPS_HISTORY; i++) {
        g_integration.gps_history[i].timestamp = 0;
        g_integration.gps_history[i].lat = 0.0;
        g_integration.gps_history[i].lon = 0.0;
        g_integration.gps_history[i].accuracy = 0.0;
        g_integration.gps_history[i].speed = 0.0;
        g_integration.gps_history[i].source_id = 0;
        g_integration.gps_history[i].source_type = GPS_SOURCE_TYPE_UNKNOWN;
        g_integration.gps_history[i].confidence = 0.0;
        g_integration.gps_history[i].health_score = 0.0;
    }
    
    g_integration_initialized = true;
    pthread_mutex_unlock(&g_integration_mutex);
    
    LOGX_INFO("GPS integration system initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Register GPS source
static int gps_integration_register_source(const char *name, gps_source_type_t source_type) {
    if (!g_integration_initialized || !name) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_integration_mutex);
    
    // Check if source already exists
    for (int i = 0; i < g_integration.source_count; i++) {
        if (g_integration.gps_sources[i].active && 
            strcmp(g_integration.gps_sources[i].name, name) == 0) {
            pthread_mutex_unlock(&g_integration_mutex);
            LOGX_WARN("GPS source '%s' already registered", name);
            return AUTONOMY_ERROR_ALREADY_EXISTS;
        }
    }
    
    // Find free source slot
    int source_index = -1;
    for (int i = 0; i < MAX_GPS_SOURCES; i++) {
        if (!g_integration.gps_sources[i].active) {
            source_index = i;
            break;
        }
    }
    
    if (source_index < 0) {
        pthread_mutex_unlock(&g_integration_mutex);
        LOGX_ERROR("No free slots for GPS source registration");
        return AUTONOMY_ERROR_NO_RESOURCES;
    }
    
    // Initialize GPS source
    gps_integration_source_t *source = &g_integration.gps_sources[source_index];
    source->active = true;
    source->source_id = generate_source_id();
    source->source_type = source_type;
    source->enabled = true;
    source->last_update = 0;
    source->update_count = 0;
    source->health_score = 100.0;  // Start with perfect health
    source->reliability = 1.0;     // Start with perfect reliability
    
    strncpy(source->name, name, sizeof(source->name) - 1);
    source->name[sizeof(source->name) - 1] = '\0';
    
    g_integration.source_count++;
    g_integration.active_sources++;
    
    pthread_mutex_unlock(&g_integration_mutex);
    
    LOGX_INFO("Registered GPS source '%s' (type: %d) with ID %d", 
               name, source_type, source->source_id);
    
    return source->source_id;
}

// Generate unique source ID
static int generate_source_id(void) {
    static int next_id = 3000;
    return next_id++;
}

// Update GPS source data
static int gps_integration_update_source(int source_id, const gps_data_t *gps_data) {
    if (!g_integration_initialized || !gps_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_integration_mutex);
    
    // Find GPS source
    int source_index = find_source_by_id(source_id);
    if (source_index < 0) {
        pthread_mutex_unlock(&g_integration_mutex);
        LOGX_ERROR("GPS source %d not found", source_id);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    gps_integration_source_t *source = &g_integration.gps_sources[source_index];
    
    // Update source data
    source->last_update = time(NULL);
    source->update_count++;
    
    // Update GPS data
    g_integration.current_gps.lat = gps_data->lat;
    g_integration.current_gps.lon = gps_data->lon;
    g_integration.current_gps.altitude = gps_data->altitude;
    g_integration.current_gps.speed = gps_data->speed;
    g_integration.current_gps.heading = gps_data->heading;
    g_integration.current_gps.accuracy = gps_data->accuracy;
    g_integration.current_gps.satellites = gps_data->satellites;
    g_integration.current_gps.fix_quality = gps_data->fix_quality;
    g_integration.current_gps.timestamp = gps_data->timestamp;
    g_integration.current_gps.source_id = source_id;
    g_integration.current_gps.source_type = source->source_type;
    
    // Calculate confidence and health scores
    source->health_score = calculate_source_health_score(source, gps_data);
    source->reliability = calculate_source_reliability(source, gps_data);
    
    // Add to GPS history
    add_gps_history(gps_data, source_id, source->source_type, source->health_score);
    
    g_integration.total_updates++;
    g_integration.last_update = time(NULL);
    
    pthread_mutex_unlock(&g_integration_mutex);
    
    // Trigger integration checks
    perform_integration_checks();
    
    LOGX_DEBUG("Updated GPS source %d: (%.6f, %.6f) accuracy: %.1fm, health: %.1f", 
               source_id, gps_data->lat, gps_data->lon, gps_data->accuracy, source->health_score);
    
    return AUTONOMY_SUCCESS;
}

// Find source by ID
static int find_source_by_id(int source_id) {
    for (int i = 0; i < MAX_GPS_SOURCES; i++) {
        if (g_integration.gps_sources[i].active && 
            g_integration.gps_sources[i].source_id == source_id) {
            return i;
        }
    }
    return -1;
}

// Calculate source health score
static double calculate_source_health_score(const gps_integration_source_t *source, const gps_data_t *gps_data) {
    double health_score = 100.0;
    
    // Accuracy penalty
    if (gps_data->accuracy > g_integration.min_accuracy) {
        double accuracy_penalty = (gps_data->accuracy - g_integration.min_accuracy) / 10.0;
        health_score -= accuracy_penalty;
    }
    
    // Satellite count penalty
    if (gps_data->satellites < 4) {
        health_score -= (4 - gps_data->satellites) * 10.0;
    }
    
    // Fix quality penalty
    if (gps_data->fix_quality < 2) {
        health_score -= (2 - gps_data->fix_quality) * 15.0;
    }
    
    // Age penalty
    time_t now = time(NULL);
    if (source->last_update > 0) {
        double age_penalty = (now - source->last_update) / 60.0;  // 1 point per minute
        health_score -= age_penalty;
    }
    
    // Ensure health score doesn't go below 0
    if (health_score < 0.0) {
        health_score = 0.0;
    }
    
    return health_score;
}

// Calculate source reliability
static double calculate_source_reliability(const gps_integration_source_t *source, const gps_data_t *gps_data) {
    double reliability = 1.0;
    
    // Base reliability on update consistency
    if (source->update_count > 0) {
        reliability = 0.8 + (0.2 * (source->update_count / 100.0));
        if (reliability > 1.0) reliability = 1.0;
    }
    
    // Penalize for poor accuracy
    if (gps_data->accuracy > 50.0) {
        reliability *= 0.9;
    }
    
    // Penalize for low satellite count
    if (gps_data->satellites < 6) {
        reliability *= 0.8;
    }
    
    return reliability;
}

// Add GPS data to history
static void add_gps_history(const gps_data_t *gps_data, int source_id, 
                           gps_source_type_t source_type, double health_score) {
    // Shift history array
    for (int i = g_integration.history_size - 1; i > 0; i--) {
        memcpy(&g_integration.gps_history[i], &g_integration.gps_history[i-1], 
               sizeof(gps_integration_record_t));
    }
    
    // Add new record
    g_integration.gps_history[0].timestamp = gps_data->timestamp;
    g_integration.gps_history[0].lat = gps_data->lat;
    g_integration.gps_history[0].lon = gps_data->lon;
    g_integration.gps_history[0].accuracy = gps_data->accuracy;
    g_integration.gps_history[0].speed = gps_data->speed;
    g_integration.gps_history[0].source_id = source_id;
    g_integration.gps_history[0].source_type = source_type;
    g_integration.gps_history[0].confidence = calculate_gps_confidence(gps_data);
    g_integration.gps_history[0].health_score = health_score;
}

// Calculate GPS confidence
static double calculate_gps_confidence(const gps_data_t *gps_data) {
    double confidence = 1.0;
    
    // Accuracy confidence
    if (gps_data->accuracy <= 10.0) {
        confidence *= 1.0;
    } else if (gps_data->accuracy <= 25.0) {
        confidence *= 0.9;
    } else if (gps_data->accuracy <= 50.0) {
        confidence *= 0.8;
    } else {
        confidence *= 0.6;
    }
    
    // Satellite confidence
    if (gps_data->satellites >= 8) {
        confidence *= 1.0;
    } else if (gps_data->satellites >= 6) {
        confidence *= 0.95;
    } else if (gps_data->satellites >= 4) {
        confidence *= 0.9;
    } else {
        confidence *= 0.7;
    }
    
    // Fix quality confidence
    if (gps_data->fix_quality >= 2) {
        confidence *= 1.0;
    } else {
        confidence *= 0.8;
    }
    
    return confidence;
}

// Perform integration checks
static void perform_integration_checks(void) {
    time_t now = time(NULL);
    
    // Check if enough time has passed since last integration check
    if ((now - g_integration.last_integration_check) < g_integration.check_interval) {
        return;
    }
    
    g_integration.last_integration_check = now;
    
    // Check GPS source health
    check_gps_source_health();
    
    // Update best GPS source
    update_best_gps_source();
    
    // Perform GPS data fusion if multiple sources available
    if (g_integration.active_sources > 1) {
        perform_gps_data_fusion();
    }
    
    // Check for GPS events
    check_gps_events();
    
    // Update location services if coordinates changed significantly
    check_location_services_update();
    
    LOGX_DEBUG("GPS integration checks completed");
}

// Check GPS source health
static void check_gps_source_health(void) {
    for (int i = 0; i < MAX_GPS_SOURCES; i++) {
        if (!g_integration.gps_sources[i].active) {
            continue;
        }
        
        gps_integration_source_t *source = &g_integration.gps_sources[i];
        time_t now = time(NULL);
        
        // Check if source is stale
        if (source->last_update > 0 && 
            (now - source->last_update) > 300) {  // 5 minutes
            source->health_score *= 0.8;  // Reduce health score
            LOGX_WARN("GPS source '%s' is stale (last update: %ld seconds ago)", 
                      source->name, now - source->last_update);
        }
        
        // Disable source if health is too low
        if (source->health_score < 20.0 && source->enabled) {
            source->enabled = false;
            g_integration.active_sources--;
            LOGX_WARN("GPS source '%s' disabled due to poor health (score: %.1f)", 
                      source->name, source->health_score);
        }
    }
}

// Update best GPS source
static void update_best_gps_source(void) {
    int best_source_index = -1;
    double best_score = -1.0;
    
    for (int i = 0; i < MAX_GPS_SOURCES; i++) {
        if (!g_integration.gps_sources[i].active || 
            !g_integration.gps_sources[i].enabled) {
            continue;
        }
        
        gps_integration_source_t *source = &g_integration.gps_sources[i];
        double score = source->health_score * source->reliability;
        
        if (score > best_score) {
            best_score = score;
            best_source_index = i;
        }
    }
    
    if (best_source_index >= 0) {
        g_integration.best_source_id = g_integration.gps_sources[best_source_index].source_id;
        LOGX_DEBUG("Best GPS source updated: %d (score: %.1f)", 
                   g_integration.best_source_id, best_score);
    }
}

// Perform GPS data fusion
static void perform_gps_data_fusion(void) {
    // This is a placeholder for GPS data fusion
    // In a full implementation, this would call the gps_fusion module
    LOGX_DEBUG("GPS data fusion would be performed here");
}

// Check GPS events
static void check_gps_events(void) {
    // This is a placeholder for GPS event checking
    // In a full implementation, this would call the gps_events module
    LOGX_DEBUG("GPS events would be checked here");
}

// Check location services update
static void check_location_services_update(void) {
    // This is a placeholder for location services updates
    // In a full implementation, this would call the gps_location_services module
    LOGX_DEBUG("Location services would be updated here");
}

// Get GPS integration status
static int gps_integration_get_status(gps_integration_status_t *status) {
    if (!g_integration_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_integration_mutex);
    
    status->enabled = g_integration.enabled;
    status->source_count = g_integration.source_count;
    status->active_sources = g_integration.active_sources;
    status->total_updates = g_integration.total_updates;
    status->last_update = g_integration.last_update;
    status->last_integration_check = g_integration.last_integration_check;
    status->best_source_id = g_integration.best_source_id;
    
    // Copy current GPS data
    memcpy(&status->current_gps, &g_integration.current_gps, sizeof(gps_data_t));
    
    // Copy GPS sources information
    int active_sources = 0;
    for (int i = 0; i < MAX_GPS_SOURCES && active_sources < MAX_GPS_SOURCES; i++) {
        if (g_integration.gps_sources[i].active) {
            memcpy(&status->gps_sources[active_sources], &g_integration.gps_sources[i], 
                   sizeof(gps_integration_source_t));
            active_sources++;
        }
    }
    status->active_source_count = active_sources;
    
    pthread_mutex_unlock(&g_integration_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get GPS integration configuration
static int gps_integration_get_config(gps_integration_config_t *config) {
    if (!g_integration_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_integration_mutex);
    
    config->enabled = g_integration.enabled;
    config->max_sources = g_integration.max_sources;
    config->update_interval = g_integration.update_interval;
    config->check_interval = g_integration.check_interval;
    config->min_accuracy = g_integration.min_accuracy;
    config->history_size = g_integration.history_size;
    
    pthread_mutex_unlock(&g_integration_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set GPS integration configuration
static int gps_integration_set_config(const gps_integration_config_t *config) {
    if (!g_integration_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_integration_mutex);
    
    g_integration.enabled = config->enabled;
    g_integration.max_sources = config->max_sources;
    g_integration.update_interval = config->update_interval;
    g_integration.check_interval = config->check_interval;
    g_integration.min_accuracy = config->min_accuracy;
    g_integration.history_size = config->history_size;
    
    pthread_mutex_unlock(&g_integration_mutex);
    
    LOGX_INFO("GPS integration configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable GPS integration
static int gps_integration_set_enabled(bool enabled) {
    if (!g_integration_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_integration_mutex);
    g_integration.enabled = enabled;
    pthread_mutex_unlock(&g_integration_mutex);
    
    LOGX_INFO("GPS integration %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Enable/disable specific GPS source
static int gps_integration_set_source_enabled(int source_id, bool enabled) {
    if (!g_integration_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_integration_mutex);
    
    int source_index = find_source_by_id(source_id);
    if (source_index < 0) {
        pthread_mutex_unlock(&g_integration_mutex);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    gps_integration_source_t *source = &g_integration.gps_sources[source_index];
    
    if (source->enabled != enabled) {
        source->enabled = enabled;
        if (enabled) {
            g_integration.active_sources++;
        } else {
            g_integration.active_sources--;
        }
    }
    
    pthread_mutex_unlock(&g_integration_mutex);
    
    LOGX_INFO("GPS source %d %s", source_id, enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Unregister GPS source
static int gps_integration_unregister_source(int source_id) {
    if (!g_integration_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_integration_mutex);
    
    int source_index = find_source_by_id(source_id);
    if (source_index < 0) {
        pthread_mutex_unlock(&g_integration_mutex);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    gps_integration_source_t *source = &g_integration.gps_sources[source_index];
    
    if (source->enabled) {
        g_integration.active_sources--;
    }
    
    source->active = false;
    g_integration.source_count--;
    
    pthread_mutex_unlock(&g_integration_mutex);
    
    LOGX_INFO("Unregistered GPS source %d", source_id);
    return AUTONOMY_SUCCESS;
}

// Reset GPS integration
static int gps_integration_reset(void) {
    if (!g_integration_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_integration_mutex);
    
    g_integration.source_count = 0;
    g_integration.active_sources = 0;
    g_integration.total_updates = 0;
    g_integration.last_update = 0;
    g_integration.last_integration_check = 0;
    g_integration.best_source_id = 0;
    
    // Clear all GPS sources
    for (int i = 0; i < MAX_GPS_SOURCES; i++) {
        g_integration.gps_sources[i].active = false;
    }
    
    // Clear GPS history
    for (int i = 0; i < MAX_GPS_HISTORY; i++) {
        g_integration.gps_history[i].timestamp = 0;
    }
    
    pthread_mutex_unlock(&g_integration_mutex);
    
    LOGX_INFO("GPS integration system reset");
    return AUTONOMY_SUCCESS;
}

// Cleanup GPS integration
static void gps_integration_cleanup(void) {
    if (!g_integration_initialized) {
        return;
    }
    
    pthread_mutex_destroy(&g_integration_mutex);
    g_integration_initialized = false;
    
    LOGX_INFO("GPS integration system cleaned up");
}
