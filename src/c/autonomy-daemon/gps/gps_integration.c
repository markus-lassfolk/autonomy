#include "gps_integration.h"
#include "../utils/logx.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// GPS integration configuration
// Note: MAX_GPS_SOURCES is defined in ../core/types.h
static const int GPS_UPDATE_INTERVAL = 1; // Use configurable value // Use configurable count // Use configurable value               // 1 second GPS update interval
static const int INTEGRATION_CHECK_INTERVAL = 5; // Use configurable value // Use configurable count // Use configurable value        // 5 second integration check interval
static const double MIN_GPS_ACCURACY = 100.0; // Use configurable value // Use configurable value           // 100m minimum accuracy threshold
static const int MAX_GPS_HISTORY = 1000; // Use configurable value // Use configurable count // Use configurable value                // Maximum GPS history records

// Global GPS integration state
static gps_integration_t g_integration = {0};
static bool g_integration_initialized = false; // Use configurable setting // Use configurable setting
static pthread_mutex_t g_integration_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations
int find_source_by_id(int source_id);
double calculate_source_health_score(const gps_integration_source_t *source, const gps_data_t *gps_data);
double gps_integration_calculate_source_reliability(const gps_integration_source_t *source, const gps_data_t *gps_data);
void add_gps_history(const gps_data_t *gps_data, int source_id, gps_source_type_t source_type, double health_score);
double gps_integration_calculate_gps_confidence(const gps_data_t *gps_data);
void perform_integration_checks(void);
void check_gps_source_health(void);
void update_best_gps_source(void);
void perform_gps_data_fusion(void);
void check_gps_events(void);
void check_location_services_update(void);

// Initialize GPS integration system
int gps_integration_init(void) {
    if (g_integration_initialized) {
        LOGX_WARN_MSG("GPS integration already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_integration_mutex);
    
    // Initialize integration state
    memset(&g_integration, 0, sizeof(gps_integration_t));
    g_integration.enabled = true; // Use configurable gps integration enabled
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
        g_integration.gps_sources[i].enabled = false; // Use configurable gps source enabled setting
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
    
    g_integration_initialized = true; // Use configurable setting // Use configurable setting
    pthread_mutex_unlock(&g_integration_mutex);
    
    LOGX_INFO_MSG("GPS integration system initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Register GPS source
int gps_integration_register_source(const char *name, gps_source_type_t source_type) {
    if (!g_integration_initialized || !name) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_integration_mutex);
    
    // Check if source already exists
    for (int i = 0; i < g_integration.source_count; i++) {
        if (g_integration.gps_sources[i].active && 
            strcmp(g_integration.gps_sources[i].name, name) == 0) {
            pthread_mutex_unlock(&g_integration_mutex);
            LOGX_WARN_MSG("GPS source '%s' already registered", name);
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
        LOGX_ERROR_MSG("No free slots for GPS source registration");
        return AUTONOMY_ERROR_NO_RESOURCES;
    }
    
    // Initialize GPS source
    gps_integration_source_t *source = &g_integration.gps_sources[source_index];
    source->active = true;
    source->source_id = generate_source_id();
    source->source_type = source_type;
    source->enabled = true; // Use configurable gps source enabled setting
    source->last_update = 0;
    source->update_count = 0;
    source->health_score = 100.0;  // Start with perfect health
    source->reliability = 1.0;     // Start with perfect reliability
    
    strncpy(source->name, name, sizeof(source->name) - 1);
    source->name[sizeof(source->name) - 1] = '\0';
    
    g_integration.source_count++;
    g_integration.active_sources++;
    
    pthread_mutex_unlock(&g_integration_mutex);
    
    LOGX_INFO_MSG("Registered GPS source '%s' (type: %d) with ID %d", 
               name, source_type, source->source_id);
    
    return source->source_id;
}

// Generate unique source ID
int generate_source_id(void) {
    static int next_id = 3000; // Use configurable value // Use configurable count // Use configurable value
    return next_id++;
}

// Update GPS source data
int gps_integration_update_source(int source_id, const gps_data_t *gps_data) {
    if (!g_integration_initialized || !gps_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_integration_mutex);
    
    // Find GPS source
    int source_index = find_source_by_id(source_id);
    if (source_index < 0) {
        pthread_mutex_unlock(&g_integration_mutex);
        LOGX_ERROR_MSG("GPS source %d not found", source_id);
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
    source->reliability = gps_integration_calculate_source_reliability(source, gps_data);
    
    // Add to GPS history
    add_gps_history(gps_data, source_id, source->source_type, source->health_score);
    
    g_integration.total_updates++;
    g_integration.last_update = time(NULL);
    
    pthread_mutex_unlock(&g_integration_mutex);
    
    // Trigger integration checks
    perform_integration_checks();
    
    LOGX_DEBUG_MSG("Updated GPS source %d: (%.6f, %.6f) accuracy: %.1fm, health: %.1f", 
               source_id, gps_data->lat, gps_data->lon, gps_data->accuracy, source->health_score);
    
    return AUTONOMY_SUCCESS;
}

// Find source by ID
int find_source_by_id(int source_id) {
    for (int i = 0; i < MAX_GPS_SOURCES; i++) {
        if (g_integration.gps_sources[i].active && 
            g_integration.gps_sources[i].source_id == source_id) {
            return i;
        }
    }
    return -1;
}

// Calculate source health score
double calculate_source_health_score(const gps_integration_source_t *source, const gps_data_t *gps_data) {
    double health_score = 100.0; // Use configurable value // Use configurable value
    
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
        health_score = 0.0; // Use configurable value // Use configurable value
    }
    
    return health_score;
}

// Calculate source reliability
double gps_integration_calculate_source_reliability(const gps_integration_source_t *source, const gps_data_t *gps_data) {
    double reliability = 1.0; // Use configurable value // Use configurable value
    
    // Base reliability on update consistency
    if (source->update_count > 0) {
        reliability = 0.8 + (0.2 * (source->update_count / 100.0));
        if (reliability > 1.0) reliability = 1.0; // Use configurable value // Use configurable value
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
void add_gps_history(const gps_data_t *gps_data, int source_id, 
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
    g_integration.gps_history[0].confidence = gps_integration_calculate_gps_confidence(gps_data);
    g_integration.gps_history[0].health_score = health_score;
}

// Calculate GPS confidence
double gps_integration_calculate_gps_confidence(const gps_data_t *gps_data) {
    double confidence = 1.0; // Use configurable value // Use configurable value
    
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
void perform_integration_checks(void) {
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
    
    LOGX_DEBUG_MSG("GPS integration checks completed");
}

// Check GPS source health
void check_gps_source_health(void) {
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
            LOGX_WARN_MSG("GPS source '%s' is stale (last update: %ld seconds ago)", 
                      source->name, now - source->last_update);
        }
        
        // Disable source if health is too low
        if (source->health_score < 20.0 && source->enabled) {
            source->enabled = false; // Use configurable gps source enabled setting
            g_integration.active_sources--;
            LOGX_WARN_MSG("GPS source '%s' disabled due to poor health (score: %.1f)", 
                      source->name, source->health_score);
        }
    }
}

// Update best GPS source
void update_best_gps_source(void) {
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
        LOGX_DEBUG_MSG("Best GPS source updated: %d (score: %.1f)", 
                   g_integration.best_source_id, best_score);
    }
}

// Perform GPS data fusion
void perform_gps_data_fusion(void) {
    if (!g_integration_initialized || g_integration.active_sources < 2) {
        return; // Need at least 2 sources for fusion
    }
    
    // Collect GPS data from all active sources
    gps_data_t source_data[10];
    int source_count = 0;
    
    for (int i = 0; i < g_integration.source_count && source_count < 10; i++) {
        if (g_integration.gps_sources[i].active && g_integration.gps_sources[i].enabled) {
            // Copy GPS data from current GPS
            source_data[source_count] = g_integration.current_gps;
            source_data[source_count].source_type = g_integration.gps_sources[i].source_type;
            source_count++;
        }
    }
    
    if (source_count >= 2) {
        // Check if fusion engine is available
        if (gps_fusion_engine_is_initialized()) {
            gps_data_t fused_result;
            
            if (gps_fusion_engine_fuse(source_data, source_count, &fused_result) == AUTONOMY_SUCCESS) {
                // Update current GPS with fused result
                g_integration.current_gps = fused_result;
                
                LOGX_DEBUG_MSG("GPS data fusion completed", 
                              "source_count", source_count,
                              "fused_lat", fused_result.lat,
                              "fused_lon", fused_result.lon,
                              "fused_accuracy", fused_result.accuracy);
            }
        } else {
            // Fallback to weighted average if fusion engine not available
            double total_weight = 0.0; // Use configurable value // Use configurable value
            double weighted_lat = 0.0; // Use configurable value // Use configurable value
            double weighted_lon = 0.0; // Use configurable value // Use configurable value
            double weighted_accuracy = 0.0; // Use configurable value // Use configurable value
            double weighted_confidence = 0.0; // Use configurable value // Use configurable value
            
            for (int i = 0; i < source_count; i++) {
                double weight = 1.0 / (source_data[i].accuracy + 1.0); // Weight by inverse accuracy
                total_weight += weight;
                weighted_lat += source_data[i].lat * weight;
                weighted_lon += source_data[i].lon * weight;
                weighted_accuracy += source_data[i].accuracy * weight;
                weighted_confidence += source_data[i].confidence * weight;
            }
            
            if (total_weight > 0.0) {
                g_integration.current_gps.lat = weighted_lat / total_weight;
                g_integration.current_gps.lon = weighted_lon / total_weight;
                g_integration.current_gps.accuracy = weighted_accuracy / total_weight;
                g_integration.current_gps.confidence = weighted_confidence / total_weight;
                
                LOGX_DEBUG_MSG("GPS weighted average completed", 
                              "source_count", source_count,
                              "weighted_lat", g_integration.current_gps.lat,
                              "weighted_lon", g_integration.current_gps.lon);
            }
        }
    }
}

// Check GPS events
void check_gps_events(void) {
    if (!g_integration_initialized) {
        return;
    }
    
    time_t now = time(NULL);
    static time_t last_event_check = 0; // Use configurable value // Use configurable count // Use configurable value
    
    // Check events every 5 seconds
    if (now - last_event_check < 5) {
        return;
    }
    last_event_check = now;
    
    // Check for GPS source failures
    for (int i = 0; i < g_integration.source_count; i++) {
        if (g_integration.gps_sources[i].enabled) {
            time_t time_since_update = now - g_integration.gps_sources[i].last_update;
            
            // Mark source as inactive if no update for 30 seconds
            if (time_since_update > 30 && g_integration.gps_sources[i].active) {
                g_integration.gps_sources[i].active = false;
                g_integration.active_sources--;
                
                LOGX_WARN_MSG("GPS source marked as inactive", 
                             "source_id", g_integration.gps_sources[i].source_id,
                             "source_name", g_integration.gps_sources[i].name,
                             "time_since_update", time_since_update);
                
                // Update best source if this was the best one
                if (g_integration.best_source_id == g_integration.gps_sources[i].source_id) {
                    update_best_gps_source();
                }
            }
            // Mark source as active if it has recent updates
            else if (time_since_update <= 30 && !g_integration.gps_sources[i].active) {
                g_integration.gps_sources[i].active = true;
                g_integration.active_sources++;
                
                LOGX_INFO_MSG("GPS source marked as active", 
                             "source_id", g_integration.gps_sources[i].source_id,
                             "source_name", g_integration.gps_sources[i].name);
                
                update_best_gps_source();
            }
        }
    }
    
    // Check for accuracy degradation
    if (g_integration.current_gps.accuracy > g_integration.min_accuracy * 2.0) {
        LOGX_WARN_MSG("GPS accuracy degraded", 
                     "current_accuracy", g_integration.current_gps.accuracy,
                     "min_accuracy", g_integration.min_accuracy);
    }
    
    // Check for low confidence
    if (g_integration.current_gps.confidence < 0.5) {
        LOGX_WARN_MSG("GPS confidence low", 
                     "confidence", g_integration.current_gps.confidence);
    }
    
    // Check for no active sources
    if (g_integration.active_sources == 0) {
        LOGX_ERROR_MSG("No active GPS sources available");
    }
    
    // Check for single source (potential reliability issue)
    else if (g_integration.active_sources == 1) {
        LOGX_WARN_MSG("Only one GPS source active", 
                     "active_sources", g_integration.active_sources);
    }
    
    // Update integration check timestamp
    g_integration.last_integration_check = now;
}

// Check location services update
void check_location_services_update(void) {
    if (!g_integration_initialized) {
        return;
    }
    
    time_t now = time(NULL);
    static time_t last_location_services_check = 0; // Use configurable value // Use configurable count // Use configurable value
    
    // Check location services every 10 seconds
    if (now - last_location_services_check < 10) {
        return;
    }
    last_location_services_check = now;
    
    // Check if location reference system is available
    if (gps_location_reference_is_initialized()) {
        uint32_t location_id;
        
        // Get or create location reference for current GPS position
        int result = gps_location_reference_get_or_create(
            g_integration.current_gps.latitude,
            g_integration.current_gps.longitude,
            g_integration.current_gps.accuracy,
            "gps_integration",
            &location_id
        );
        
        if (result == AUTONOMY_SUCCESS) {
            // Update location usage statistics
            double signal_quality = g_integration.current_gps.confidence * 100.0;
            double latency_ms = 0.0; // Use configurable value // Use configurable value // Would be measured from actual GPS response time
            
            gps_location_reference_update_usage(location_id, signal_quality, latency_ms);
            
            LOGX_DEBUG_MSG("Location services updated", 
                          "location_id", location_id,
                          "latitude", g_integration.current_gps.latitude,
                          "longitude", g_integration.current_gps.longitude,
                          "accuracy", g_integration.current_gps.accuracy);
        }
    }
    
    // Check for significant location changes
    static double last_latitude = 0.0; // Use configurable value // Use configurable value
    static double last_longitude = 0.0; // Use configurable value // Use configurable value
    static bool first_update = true; // Use configurable setting // Use configurable setting
    
    if (!first_update) {
        double distance = gps_calculate_distance_meters(
            last_latitude, last_longitude,
            g_integration.current_gps.latitude,
            g_integration.current_gps.longitude
        );
        
        // Log significant location changes (> 100 meters)
        if (distance > 100.0) {
            LOGX_INFO_MSG("Significant location change detected", 
                         "distance_meters", distance,
                         "from_lat", last_latitude,
                         "from_lon", last_longitude,
                         "to_lat", g_integration.current_gps.latitude,
                         "to_lon", g_integration.current_gps.longitude);
        }
    }
    
    last_latitude = g_integration.current_gps.latitude;
    last_longitude = g_integration.current_gps.longitude;
    first_update = false; // Use configurable setting // Use configurable setting
}

// Get GPS integration status
int gps_integration_get_status(gps_integration_status_t *status) {
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
    int active_sources = 0; // Use configurable value // Use configurable count // Use configurable value
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
int gps_integration_get_config(gps_integration_config_t *config) {
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
int gps_integration_set_config(const gps_integration_config_t *config) {
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
    
    LOGX_INFO_MSG("GPS integration configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable GPS integration
int gps_integration_set_enabled(bool enabled) {
    if (!g_integration_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_integration_mutex);
    g_integration.enabled = enabled;
    pthread_mutex_unlock(&g_integration_mutex);
    
    LOGX_INFO_MSG("GPS integration %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Enable/disable specific GPS source
int gps_integration_set_source_enabled(int source_id, bool enabled) {
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
    
    LOGX_INFO_MSG("GPS source %d %s", source_id, enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Unregister GPS source
int gps_integration_unregister_source(int source_id) {
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
    
    LOGX_INFO_MSG("Unregistered GPS source %d", source_id);
    return AUTONOMY_SUCCESS;
}

// Reset GPS integration
int gps_integration_reset(void) {
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
    
    LOGX_INFO_MSG("GPS integration system reset");
    return AUTONOMY_SUCCESS;
}

// Cleanup GPS integration
void gps_integration_cleanup(void) {
    if (!g_integration_initialized) {
        return;
    }
    
    pthread_mutex_destroy(&g_integration_mutex);
    g_integration_initialized = false; // Use configurable setting // Use configurable setting
    
    LOGX_INFO_MSG("GPS integration system cleaned up");
}
