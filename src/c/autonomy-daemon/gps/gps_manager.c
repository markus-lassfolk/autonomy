#include "gps_manager.h"
#include "gps_comprehensive.h"
#include "gps_fusion_engine.h"
#include "opencellid_complete.h"
#include "../utils/logx.h"
#include "../core/types.h"
#include "gps_rutos.h"
#include "gps_starlink.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <math.h>

// GPS manager configuration
static const int GPS_UPDATE_INTERVAL = 5;         // 5 seconds
static const int GPS_SOURCE_TIMEOUT = 60;         // 60 seconds
static const int MAX_GPS_SOURCES = 8;             // Maximum GPS sources
static const double MIN_RELIABILITY = 0.3;        // Minimum reliability threshold
static const double POSITION_THRESHOLD = 50.0;    // 50 meters position change threshold

// GPS source types
static const char* GPS_SOURCE_NAMES[] = {
    "unknown", "rutos", "starlink", "external", "simulated"
};

// Global GPS manager state
static gps_manager_t g_gps_manager = {0};
static pthread_mutex_t g_gps_manager_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_gps_manager_initialized = false;
static pthread_t g_gps_manager_thread = 0;
static bool g_gps_manager_thread_running = false;

// Initialize GPS manager system
int gps_manager_init(void) {
    if (g_gps_manager_initialized) {
        LOGX_WARN("GPS manager already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_gps_manager_mutex);
    
    // Initialize GPS manager state
    memset(&g_gps_manager, 0, sizeof(gps_manager_t));
    g_gps_manager.enabled = true;
    g_gps_manager.update_interval = GPS_UPDATE_INTERVAL;
    g_gps_manager.source_timeout = GPS_SOURCE_TIMEOUT;
    g_gps_manager.last_update = 0;
    g_gps_manager.total_updates = 0;
    g_gps_manager.source_count = 0;
    g_gps_manager.best_source = -1;
    
    // Initialize GPS sources array
    for (int i = 0; i < MAX_GPS_SOURCES; i++) {
        g_gps_manager.sources[i].enabled = false;
        g_gps_manager.sources[i].type = GPS_SOURCE_UNKNOWN;
        g_gps_manager.sources[i].last_update = 0;
        g_gps_manager.sources[i].reliability_score = 0.0;
        g_gps_manager.sources[i].data_quality = 0.0;
        memset(&g_gps_manager.sources[i].gps_data, 0, sizeof(gps_data_t));
    }
    
    // Initialize comprehensive GPS collector
    gps_comprehensive_config_t comprehensive_config = {
        .enabled = true,
        .movement_threshold_m = 50.0,
        .accuracy_threshold_m = 100.0,
        .staleness_threshold_s = 300,
        .collection_timeout_s = 30,
        .retry_attempts = 3,
        .retry_delay_s = 5,
        .enable_hybrid_prioritization = true,
        .min_acceptable_confidence = 0.3,
        .fallback_confidence_threshold = 0.7,
        .enable_data_fusion = true,
        .fusion_weight_accuracy = 0.4,
        .fusion_weight_confidence = 0.3,
        .fusion_weight_freshness = 0.3,
        .enable_movement_detection = true,
        .movement_detection_interval_s = 30.0,
        .stationary_threshold_s = 300.0,
        .movement_hysteresis_m = 10.0,
        .opencellid_enabled = true,
        .enable_health_monitoring = true,
        .health_check_interval_s = 60,
        .min_health_score = 0.5,
        .max_consecutive_failures = 5,
        .enable_location_clustering = true,
        .enable_adaptive_caching = true,
        .clustering_radius_m = 100.0,
        .max_cluster_size = 50
    };
    strcpy(comprehensive_config.source_priority, "rutos,starlink,opencellid,google");
    
    if (gps_comprehensive_init(&comprehensive_config) != AUTONOMY_SUCCESS) {
        LOGX_ERROR("Failed to initialize comprehensive GPS collector");
        pthread_mutex_unlock(&g_gps_manager_mutex);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Initialize GPS fusion engine
    gps_fusion_config_t fusion_config = {
        .enabled = true,
        .default_method = GPS_FUSION_METHOD_CONFIDENCE_WEIGHTED,
        .accuracy_weight = 0.3,
        .confidence_weight = 0.4,
        .freshness_weight = 0.2,
        .health_weight = 0.1,
        .priority_weight = 0.1,
        .min_sources_for_fusion = 2,
        .max_accuracy_difference = 1000.0,
        .max_distance_difference = 5000.0,
        .max_time_difference_s = 300.0,
        .outlier_detection_threshold = 2.0,
        .consensus_threshold = 0.8,
        .min_fusion_confidence = 0.4,
        .enable_outlier_detection = true,
        .enable_consensus_checking = true,
        .enable_temporal_smoothing = true,
        .enable_kalman_filtering = false,
        .process_noise_covariance = 0.1,
        .measurement_noise_covariance = 1.0,
        .initial_uncertainty = 100.0
    };
    
    if (gps_fusion_engine_init(&fusion_config) != AUTONOMY_SUCCESS) {
        LOGX_ERROR("Failed to initialize GPS fusion engine");
        gps_comprehensive_cleanup();
        pthread_mutex_unlock(&g_gps_manager_mutex);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Initialize unified GPS data
    g_gps_manager.unified_gps.timestamp = 0;
    g_gps_manager.unified_gps.lat = 0.0;
    g_gps_manager.unified_gps.lon = 0.0;
    g_gps_manager.unified_gps.altitude = 0.0;
    g_gps_manager.unified_gps.accuracy = 0.0;
    g_gps_manager.unified_gps.satellites = 0;
    g_gps_manager.unified_gps.fix_quality = 0;
    g_gps_manager.unified_gps.reliability_score = 0.0;
    
    g_gps_manager_initialized = true;
    pthread_mutex_unlock(&g_gps_manager_mutex);
    
    LOGX_INFO("GPS manager system initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Start GPS manager monitoring thread
int gps_manager_start_monitoring(void) {
    if (!g_gps_manager_initialized) {
        LOGX_ERROR("GPS manager not initialized");
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    if (g_gps_manager_thread_running) {
        LOGX_WARN("GPS manager monitoring already running");
        return AUTONOMY_SUCCESS;
    }
    
    // Create monitoring thread
    int ret = pthread_create(&g_gps_manager_thread, NULL, gps_manager_monitor_thread, NULL);
    if (ret != 0) {
        LOGX_ERROR("Failed to create GPS manager monitoring thread");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    g_gps_manager_thread_running = true;
    LOGX_INFO("GPS manager monitoring started");
    
    return AUTONOMY_SUCCESS;
}

// Stop GPS manager monitoring
void gps_manager_stop_monitoring(void) {
    if (!g_gps_manager_thread_running) {
        return;
    }
    
    g_gps_manager_thread_running = false;
    
    if (g_gps_manager_thread != 0) {
        pthread_join(g_gps_manager_thread, NULL);
        g_gps_manager_thread = 0;
    }
    
    LOGX_INFO("GPS manager monitoring stopped");
}

// GPS manager monitoring thread
static void* gps_manager_monitor_thread(void *arg) {
    (void)arg;
    
    LOGX_INFO("GPS manager monitoring thread started");
    
    while (g_gps_manager_thread_running) {
        // Update GPS data from all sources
        gps_manager_update_all_sources();
        
        // Calculate unified GPS position
        gps_manager_calculate_unified_position();
        
        // Sleep for update interval
        for (int i = 0; i < g_gps_manager.update_interval && g_gps_manager_thread_running; i++) {
            sleep(1);
        }
    }
    
    LOGX_INFO("GPS manager monitoring thread stopped");
    return NULL;
}

// Update GPS data from all sources
int gps_manager_update_all_sources(void) {
    if (!g_gps_manager_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_gps_manager_mutex);
    
    time_t now = time(NULL);
    
    // Check if it's time to update
    if (g_gps_manager.last_update > 0 && 
        (now - g_gps_manager.last_update) < g_gps_manager.update_interval) {
        pthread_mutex_unlock(&g_gps_manager_mutex);
        return AUTONOMY_SUCCESS;
    }
    
    LOGX_DEBUG("Updating GPS data from all sources");
    
    // Update RUTOS GPS source
    update_rutos_gps_source();
    
    // Update Starlink GPS source
    update_starlink_gps_source();
    
    // Clean up stale sources
    cleanup_stale_gps_sources(now);
    
    g_gps_manager.last_update = now;
    g_gps_manager.total_updates++;
    
    pthread_mutex_unlock(&g_gps_manager_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Update RUTOS GPS source
static void update_rutos_gps_source(void) {
    // Check if RUTOS GPS is available
    if (!gps_rutos_is_available()) {
        return;
    }
    
    // Get RUTOS GPS data
    gps_data_t rutos_data;
    if (gps_rutos_get_data(&rutos_data) == AUTONOMY_SUCCESS) {
        // Find or create RUTOS GPS source
        int source_index = find_or_create_gps_source(GPS_SOURCE_RUTOS, "rutos");
        if (source_index >= 0) {
            // Update source data
            gps_source_t *source = &g_gps_manager.sources[source_index];
            memcpy(&source->gps_data, &rutos_data, sizeof(gps_data_t));
            source->last_update = time(NULL);
            source->reliability_score = rutos_data.reliability_score;
            source->data_quality = calculate_data_quality(&rutos_data);
            
            LOGX_DEBUG("Updated RUTOS GPS source: lat=%.6f, lon=%.6f, reliability=%.2f", 
                       rutos_data.lat, rutos_data.lon, source->reliability_score);
        }
    }
}

// Update Starlink GPS source
static void update_starlink_gps_source(void) {
    // Check if Starlink GPS is available
    if (!gps_starlink_is_data_recent(300)) { // 5 minutes
        return;
    }
    
    // Get Starlink GPS data
    gps_data_t starlink_data;
    if (gps_starlink_get_data(&starlink_data) == AUTONOMY_SUCCESS) {
        // Find or create Starlink GPS source
        int source_index = find_or_create_gps_source(GPS_SOURCE_STARLINK, "starlink");
        if (source_index >= 0) {
            // Update source data
            gps_source_t *source = &g_gps_manager.sources[source_index];
            memcpy(&source->gps_data, &starlink_data, sizeof(gps_data_t));
            source->last_update = time(NULL);
            source->reliability_score = starlink_data.reliability_score;
            source->data_quality = calculate_data_quality(&starlink_data);
            
            LOGX_DEBUG("Updated Starlink GPS source: lat=%.6f, lon=%.6f, reliability=%.2f", 
                       starlink_data.lat, starlink_data.lon, source->reliability_score);
        }
    }
}

// Find or create GPS source
static int find_or_create_gps_source(gps_source_type_t type, const char *name) {
    // First, try to find existing source
    for (int i = 0; i < g_gps_manager.source_count; i++) {
        if (g_gps_manager.sources[i].type == type) {
            return i;
        }
    }
    
    // Create new source if space available
    if (g_gps_manager.source_count < MAX_GPS_SOURCES) {
        int index = g_gps_manager.source_count;
        g_gps_manager.sources[index].enabled = true;
        g_gps_manager.sources[index].type = type;
        strncpy(g_gps_manager.sources[index].name, name, sizeof(g_gps_manager.sources[index].name) - 1);
        g_gps_manager.sources[index].name[sizeof(g_gps_manager.sources[index].name) - 1] = '\0';
        g_gps_manager.sources[index].last_update = 0;
        g_gps_manager.sources[index].reliability_score = 0.0;
        g_gps_manager.sources[index].data_quality = 0.0;
        memset(&g_gps_manager.sources[index].gps_data, 0, sizeof(gps_data_t));
        
        g_gps_manager.source_count++;
        
        LOGX_DEBUG("Created new GPS source: %s (type: %s)", name, GPS_SOURCE_NAMES[type]);
        return index;
    }
    
    LOGX_WARN("Maximum GPS sources reached, cannot create new source");
    return -1;
}

// Calculate data quality score
static double calculate_data_quality(const gps_data_t *gps_data) {
    if (!gps_data) {
        return 0.0;
    }
    
    double quality = 0.0;
    
    // Accuracy-based quality
    if (gps_data->accuracy > 0) {
        if (gps_data->accuracy <= 10.0) {
            quality += 0.4;  // High accuracy
        } else if (gps_data->accuracy <= 50.0) {
            quality += 0.3;  // Medium accuracy
        } else if (gps_data->accuracy <= 100.0) {
            quality += 0.2;  // Low accuracy
        } else {
            quality += 0.1;  // Very low accuracy
        }
    }
    
    // Satellite count quality
    if (gps_data->satellites >= 8) {
        quality += 0.3;  // Excellent satellite coverage
    } else if (gps_data->satellites >= 6) {
        quality += 0.2;  // Good satellite coverage
    } else if (gps_data->satellites >= 4) {
        quality += 0.1;  // Adequate satellite coverage
    }
    
    // Fix quality
    if (gps_data->fix_quality > 0) {
        quality += 0.2;  // Good fix
    }
    
    // Data freshness quality
    time_t now = time(NULL);
    if (gps_data->timestamp > 0) {
        int age = now - gps_data->timestamp;
        if (age <= 30) {
            quality += 0.1;  // Very recent data
        } else if (age <= 60) {
            quality += 0.05;  // Recent data
        }
    }
    
    return (quality > 1.0) ? 1.0 : quality;
}

// Clean up stale GPS sources
static void cleanup_stale_gps_sources(time_t now) {
    for (int i = 0; i < g_gps_manager.source_count; i++) {
        if (g_gps_manager.sources[i].last_update > 0 &&
            (now - g_gps_manager.sources[i].last_update) > g_gps_manager.source_timeout) {
            
            LOGX_DEBUG("Removing stale GPS source: %s", g_gps_manager.sources[i].name);
            
            // Remove source by shifting remaining sources
            for (int j = i; j < g_gps_manager.source_count - 1; j++) {
                memcpy(&g_gps_manager.sources[j], &g_gps_manager.sources[j + 1], sizeof(gps_source_t));
            }
            g_gps_manager.source_count--;
            i--; // Recheck this index
        }
    }
}

// Calculate unified GPS position
int gps_manager_calculate_unified_position(void) {
    if (!g_gps_manager_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_gps_manager_mutex);
    
    if (g_gps_manager.source_count == 0) {
        pthread_mutex_unlock(&g_gps_manager_mutex);
        return AUTONOMY_ERROR_NO_DATA;
    }
    
    // Find the best GPS source
    int best_source = find_best_gps_source();
    if (best_source < 0) {
        pthread_mutex_unlock(&g_gps_manager_mutex);
        return AUTONOMY_ERROR_NO_DATA;
    }
    
    g_gps_manager.best_source = best_source;
    gps_source_t *source = &g_gps_manager.sources[best_source];
    
    // Check if position has changed significantly
    bool position_changed = check_position_change(&source->gps_data);
    
    if (position_changed) {
        // Update unified GPS data
        memcpy(&g_gps_manager.unified_gps, &source->gps_data, sizeof(gps_data_t));
        
        LOGX_DEBUG("Updated unified GPS position from %s: lat=%.6f, lon=%.6f, acc=%.1fm", 
                   source->name, source->gps_data.lat, source->gps_data.lon, source->gps_data.accuracy);
    }
    
    pthread_mutex_unlock(&g_gps_manager_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Find the best GPS source
static int find_best_gps_source(void) {
    int best_source = -1;
    double best_score = 0.0;
    
    for (int i = 0; i < g_gps_manager.source_count; i++) {
        if (!g_gps_manager.sources[i].enabled) {
            continue;
        }
        
        // Calculate combined score (reliability + quality)
        double combined_score = g_gps_manager.sources[i].reliability_score + 
                               g_gps_manager.sources[i].data_quality;
        
        if (combined_score > best_score && combined_score >= MIN_RELIABILITY) {
            best_score = combined_score;
            best_source = i;
        }
    }
    
    return best_source;
}

// Check if position has changed significantly
static bool check_position_change(const gps_data_t *new_data) {
    if (!new_data || g_gps_manager.unified_gps.timestamp == 0) {
        return true; // First data or no previous data
    }
    
    // Calculate distance between old and new positions
    double lat_diff = new_data->lat - g_gps_manager.unified_gps.lat;
    double lon_diff = new_data->lon - g_gps_manager.unified_gps.lon;
    
    // Simple distance calculation (approximate)
    double distance = sqrt(lat_diff * lat_diff + lon_diff * lon_diff) * 111000; // meters
    
    return distance > POSITION_THRESHOLD;
}

// Get unified GPS data
int gps_manager_get_unified_gps(gps_data_t *gps_data) {
    if (!g_gps_manager_initialized || !gps_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_gps_manager_mutex);
    
    memcpy(gps_data, &g_gps_manager.unified_gps, sizeof(gps_data_t));
    
    pthread_mutex_unlock(&g_gps_manager_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get GPS source information
int gps_manager_get_sources(gps_source_t *sources, int max_count, int *actual_count) {
    if (!g_gps_manager_initialized || !sources || !actual_count) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_gps_manager_mutex);
    
    *actual_count = 0;
    int count = (g_gps_manager.source_count < max_count) ? g_gps_manager.source_count : max_count;
    
    for (int i = 0; i < count; i++) {
        memcpy(&sources[i], &g_gps_manager.sources[i], sizeof(gps_source_t));
        (*actual_count)++;
    }
    
    pthread_mutex_unlock(&g_gps_manager_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get GPS manager status
int gps_manager_get_status(gps_manager_status_t *status) {
    if (!g_gps_manager_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_gps_manager_mutex);
    
    status->enabled = g_gps_manager.enabled;
    status->update_interval = g_gps_manager.update_interval;
    status->source_timeout = g_gps_manager.source_timeout;
    status->last_update = g_gps_manager.last_update;
    status->total_updates = g_gps_manager.total_updates;
    status->source_count = g_gps_manager.source_count;
    status->best_source = g_gps_manager.best_source;
    
    pthread_mutex_unlock(&g_gps_manager_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set GPS manager configuration
int gps_manager_set_config(const gps_manager_config_t *config) {
    if (!g_gps_manager_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_gps_manager_mutex);
    
    if (config->update_interval > 0) {
        g_gps_manager.update_interval = config->update_interval;
    }
    
    if (config->source_timeout > 0) {
        g_gps_manager.source_timeout = config->source_timeout;
    }
    
    g_gps_manager.enabled = config->enabled;
    
    pthread_mutex_unlock(&g_gps_manager_mutex);
    
    LOGX_INFO("GPS manager configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable GPS manager
int gps_manager_set_enabled(bool enabled) {
    if (!g_gps_manager_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_gps_manager_mutex);
    g_gps_manager.enabled = enabled;
    pthread_mutex_unlock(&g_gps_manager_mutex);
    
    LOGX_INFO("GPS manager system %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Force immediate GPS update
int gps_manager_force_update(void) {
    if (!g_gps_manager_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    LOGX_INFO("Forcing immediate GPS manager update");
    return gps_manager_update_all_sources();
}

// Cleanup GPS manager system
void gps_manager_cleanup(void) {
    if (!g_gps_manager_initialized) {
        return;
    }
    
    // Stop monitoring thread
    gps_manager_stop_monitoring();
    
    pthread_mutex_lock(&g_gps_manager_mutex);
    
    // Cleanup comprehensive GPS collector
    gps_comprehensive_cleanup();
    
    // Cleanup GPS fusion engine
    gps_fusion_engine_cleanup();
    
    g_gps_manager_initialized = false;
    pthread_mutex_unlock(&g_gps_manager_mutex);
    
    pthread_mutex_destroy(&g_gps_manager_mutex);
    
    LOGX_INFO("GPS manager system cleaned up");
}
