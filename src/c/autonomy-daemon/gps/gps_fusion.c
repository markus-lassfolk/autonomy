#include "gps_fusion.h"
#include "../utils/logx.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

// GPS fusion configuration
// Note: MAX_FUSION_SOURCES is defined in ../core/types.h
static const int MIN_FUSION_SOURCES = 2;               // Minimum sources for fusion
static const double FUSION_UPDATE_INTERVAL = 5.0;      // 5 second fusion update interval
static const double MAX_SOURCE_AGE = 60.0;             // 60 second maximum source age
static const double FUSION_WEIGHT_THRESHOLD = 0.3;     // Minimum weight for source inclusion
static const int FUSION_HISTORY_SIZE = 20;             // Number of fused positions to track

// Forward declarations
static void update_source_metrics(gps_fusion_source_t *source, const gps_data_t *gps_data);
static void update_source_reliability(gps_fusion_source_t *source);
static int perform_weighted_average_fusion(gps_data_t *fused_data);
static int perform_kalman_filter_fusion(gps_data_t *fused_data);
static int perform_least_squares_fusion(gps_data_t *fused_data);
static double calculate_fusion_quality(void);
static void add_fusion_history(const gps_data_t *fused_data);
static int find_fusion_source_by_name(const char *source_name);

// Fusion algorithms
static const char* FUSION_ALGORITHM_NAMES[] = {
    "unknown", "weighted_average", "kalman_filter", "particle_filter", "least_squares"
};

// Global GPS fusion state
static gps_fusion_t g_fusion = {0};
static bool g_fusion_initialized = false;
static pthread_mutex_t g_fusion_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize GPS fusion system
int gps_fusion_init(void) {
    if (g_fusion_initialized) {
        LOGX_WARN_MSG("GPS fusion system already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_fusion_mutex);
    
    // Initialize fusion state
    memset(&g_fusion, 0, sizeof(gps_fusion_t));
    g_fusion.enabled = true;
    g_fusion.max_sources = MAX_FUSION_SOURCES;
    g_fusion.min_sources = MIN_FUSION_SOURCES;
    g_fusion.update_interval = FUSION_UPDATE_INTERVAL;
    g_fusion.max_source_age = MAX_SOURCE_AGE;
    g_fusion.weight_threshold = FUSION_WEIGHT_THRESHOLD;
    g_fusion.history_size = FUSION_HISTORY_SIZE;
    g_fusion.fusion_algorithm = FUSION_ALGORITHM_WEIGHTED_AVERAGE;
    
    g_fusion.source_count = 0;
    g_fusion.fusion_count = 0;
    g_fusion.last_fusion = 0;
    g_fusion.fusion_quality = 0.0;
    
    // Initialize fusion history
    for (int i = 0; i < FUSION_HISTORY_SIZE; i++) {
        g_fusion.fusion_history[i].timestamp = 0;
        g_fusion.fusion_history[i].lat = 0.0;
        g_fusion.fusion_history[i].lon = 0.0;
        g_fusion.fusion_history[i].altitude = 0.0;
        g_fusion.fusion_history[i].accuracy = 0.0;
        g_fusion.fusion_history[i].confidence = 0.0;
        g_fusion.fusion_history[i].source_count = 0;
    }
    
    g_fusion_initialized = true;
    pthread_mutex_unlock(&g_fusion_mutex);
    
    LOGX_INFO_MSG("GPS fusion system initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Add GPS source for fusion
int gps_fusion_add_source(const char *source_name, gps_source_type_t source_type) {
    if (!g_fusion_initialized || !source_name) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_fusion_mutex);
    
    // Check if source already exists
    int existing_index = find_fusion_source_by_name(source_name);
    if (existing_index >= 0) {
        pthread_mutex_unlock(&g_fusion_mutex);
        LOGX_WARN_MSG("GPS fusion source '%s' already registered", source_name);
        return AUTONOMY_ERROR_ALREADY_EXISTS;
    }
    
    // Find free slot
    int source_index = -1;
    for (int i = 0; i < MAX_FUSION_SOURCES; i++) {
        if (!g_fusion.sources[i].active) {
            source_index = i;
            break;
        }
    }
    
    if (source_index < 0) {
        pthread_mutex_unlock(&g_fusion_mutex);
        LOGX_ERROR_MSG("No free slots for GPS fusion source");
        return AUTONOMY_ERROR_NO_RESOURCES;
    }
    
    // Initialize fusion source
    gps_fusion_source_t *source = &g_fusion.sources[source_index];
    source->active = true;
    strncpy(source->name, source_name, sizeof(source->name) - 1);
    source->name[sizeof(source->name) - 1] = '\0';
    source->source_type = source_type;
    source->registration_time = time(NULL);
    source->last_update = 0;
    source->last_gps_data.timestamp = 0;
    source->last_gps_data.lat = 0.0;
    source->last_gps_data.lon = 0.0;
    source->last_gps_data.altitude = 0.0;
    source->last_gps_data.accuracy = 0.0;
    source->last_gps_data.satellites = 0;
    source->last_gps_data.fix_quality = 0;
    source->weight = 1.0;
    source->reliability = 1.0;
    
    g_fusion.source_count++;
    
    pthread_mutex_unlock(&g_fusion_mutex);
    
    LOGX_INFO_MSG("Added GPS fusion source '%s' (type: %d)", source_name, source_type);
    return AUTONOMY_SUCCESS;
}

// Update GPS source data
int gps_fusion_update_source(const char *source_name, const gps_data_t *gps_data) {
    if (!g_fusion_initialized || !source_name || !gps_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_fusion_mutex);
    
    // Find source
    int source_index = find_fusion_source_by_name(source_name);
    if (source_index < 0) {
        pthread_mutex_unlock(&g_fusion_mutex);
        LOGX_WARN_MSG("GPS fusion source '%s' not found", source_name);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    gps_fusion_source_t *source = &g_fusion.sources[source_index];
    
    // Update source data
    memcpy(&source->last_gps_data, gps_data, sizeof(gps_data_t));
    source->last_update = time(NULL);
    
    // Update source weight and reliability
    update_source_metrics(source, gps_data);
    
    pthread_mutex_unlock(&g_fusion_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Update source metrics
static void update_source_metrics(gps_fusion_source_t *source, const gps_data_t *gps_data) {
    // Calculate accuracy-based weight
    double accuracy_weight = 1.0;
    if (gps_data->accuracy > 0) {
        accuracy_weight = 1.0 / (1.0 + gps_data->accuracy / 10.0);
    }
    
    // Calculate satellite-based weight
    double satellite_weight = 1.0;
    if (gps_data->satellites > 0) {
        satellite_weight = fmin(gps_data->satellites / 10.0, 1.0);
    }
    
    // Calculate fix quality weight
    double fix_quality_weight = 1.0;
    switch (gps_data->fix_quality) {
        case 0: fix_quality_weight = 0.0; break;   // No fix
        case 1: fix_quality_weight = 0.7; break;   // GPS fix
        case 2: fix_quality_weight = 0.9; break;   // DGPS fix
        case 4: fix_quality_weight = 1.0; break;   // RTK fix
        case 5: fix_quality_weight = 0.95; break;  // Float RTK
        case 6: fix_quality_weight = 0.8; break;   // Estimated
        default: fix_quality_weight = 0.5; break;
    }
    
    // Calculate age-based weight
    time_t now = time(NULL);
    double age_weight = 1.0;
    if (gps_data->timestamp > 0) {
        int age = now - gps_data->timestamp;
        if (age > 0) {
            age_weight = exp(-age / 30.0); // Exponential decay
        }
    }
    
    // Combine weights
    source->weight = accuracy_weight * 0.4 + satellite_weight * 0.2 + 
                     fix_quality_weight * 0.3 + age_weight * 0.1;
    
    // Update reliability based on consistency
    update_source_reliability(source);
}

// Update source reliability
static void update_source_reliability(gps_fusion_source_t *source) {
    // Simple reliability calculation based on data quality
    double reliability = 1.0;
    
    // Reduce reliability for invalid coordinates
    if (source->last_gps_data.lat == 0.0 && source->last_gps_data.lon == 0.0) {
        reliability *= 0.1;
    }
    
    // Reduce reliability for poor fix quality
    if (source->last_gps_data.fix_quality == 0) {
        reliability *= 0.2;
    }
    
    // Reduce reliability for insufficient satellites
    if (source->last_gps_data.satellites < 4) {
        reliability *= 0.5;
    }
    
    // Reduce reliability for old data
    time_t now = time(NULL);
    if (source->last_gps_data.timestamp > 0) {
        int age = now - source->last_gps_data.timestamp;
        if (age > 60) {
            reliability *= 0.3;
        }
    }
    
    source->reliability = reliability;
}

// Perform GPS data fusion
int gps_fusion_perform_fusion(gps_data_t *fused_data) {
    if (!g_fusion_initialized || !fused_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_fusion_mutex);
    
    // Check if we have enough sources
    if (g_fusion.source_count < g_fusion.min_sources) {
        pthread_mutex_unlock(&g_fusion_mutex);
        return AUTONOMY_ERROR_NO_DATA;
    }
    
    // Check if enough time has passed since last fusion
    time_t now = time(NULL);
    if ((now - g_fusion.last_fusion) < g_fusion.update_interval) {
        pthread_mutex_unlock(&g_fusion_mutex);
        return AUTONOMY_ERROR_NO_DATA;
    }
    
    // Perform fusion based on selected algorithm
    int fusion_result = AUTONOMY_ERROR_NOT_SUPPORTED;
    
    switch (g_fusion.fusion_algorithm) {
        case FUSION_ALGORITHM_WEIGHTED_AVERAGE:
            fusion_result = perform_weighted_average_fusion(fused_data);
            break;
        case FUSION_ALGORITHM_KALMAN_FILTER:
            fusion_result = perform_kalman_filter_fusion(fused_data);
            break;
        case FUSION_ALGORITHM_LEAST_SQUARES:
            fusion_result = perform_least_squares_fusion(fused_data);
            break;
        default:
            fusion_result = perform_weighted_average_fusion(fused_data);
            break;
    }
    
    if (fusion_result == AUTONOMY_SUCCESS) {
        // Add to fusion history
        add_fusion_history(fused_data);
        
        g_fusion.fusion_count++;
        g_fusion.last_fusion = now;
        
        // Calculate fusion quality
        g_fusion.fusion_quality = calculate_fusion_quality();
        
        LOGX_DEBUG_MSG("GPS fusion completed: lat=%.6f, lon=%.6f, accuracy=%.1fm, quality=%.3f", 
                   fused_data->lat, fused_data->lon, fused_data->accuracy, g_fusion.fusion_quality);
    }
    
    pthread_mutex_unlock(&g_fusion_mutex);
    
    return fusion_result;
}

// Perform weighted average fusion
static int perform_weighted_average_fusion(gps_data_t *fused_data) {
    time_t now = time(NULL);
    double total_weight = 0.0;
    double weighted_lat = 0.0;
    double weighted_lon = 0.0;
    double weighted_alt = 0.0;
    double weighted_acc = 0.0;
    int valid_sources = 0;
    
    // Calculate weighted averages
    for (int i = 0; i < MAX_FUSION_SOURCES; i++) {
        if (!g_fusion.sources[i].active) {
            continue;
        }
        
        const gps_fusion_source_t *source = &g_fusion.sources[i];
        const gps_data_t *gps_data = &source->last_gps_data;
        
        // Check if source data is valid and recent
        if (gps_data->timestamp <= 0 || 
            (now - gps_data->timestamp) > g_fusion.max_source_age ||
            source->weight < g_fusion.weight_threshold) {
            continue;
        }
        
        // Check if coordinates are valid
        if (gps_data->lat == 0.0 && gps_data->lon == 0.0) {
            continue;
        }
        
        // Calculate effective weight (source weight * reliability)
        double effective_weight = source->weight * source->reliability;
        
        weighted_lat += gps_data->lat * effective_weight;
        weighted_lon += gps_data->lon * effective_weight;
        weighted_alt += gps_data->altitude * effective_weight;
        weighted_acc += gps_data->accuracy * effective_weight;
        total_weight += effective_weight;
        valid_sources++;
    }
    
    if (valid_sources < g_fusion.min_sources || total_weight <= 0) {
        return AUTONOMY_ERROR_NO_DATA;
    }
    
    // Calculate fused position
    fused_data->lat = weighted_lat / total_weight;
    fused_data->lon = weighted_lon / total_weight;
    fused_data->altitude = weighted_alt / total_weight;
    fused_data->accuracy = weighted_acc / total_weight;
    fused_data->timestamp = now;
    fused_data->satellites = valid_sources;
    fused_data->fix_quality = 1; // Assume good fix for fused data
    
    return AUTONOMY_SUCCESS;
}

// Perform Kalman filter fusion (simplified)
static int perform_kalman_filter_fusion(gps_data_t *fused_data) {
    // For now, fall back to weighted average
    // A full Kalman filter implementation would require significant additional complexity
    return perform_weighted_average_fusion(fused_data);
}

// Perform least squares fusion (simplified)
static int perform_least_squares_fusion(gps_data_t *fused_data) {
    // For now, fall back to weighted average
    // A full least squares implementation would require significant additional complexity
    return perform_weighted_average_fusion(fused_data);
}

// Calculate fusion quality
static double calculate_fusion_quality(void) {
    if (g_fusion.source_count < g_fusion.min_sources) {
        return 0.0;
    }
    
    double total_quality = 0.0;
    int valid_sources = 0;
    
    for (int i = 0; i < MAX_FUSION_SOURCES; i++) {
        if (!g_fusion.sources[i].active) {
            continue;
        }
        
        const gps_fusion_source_t *source = &g_fusion.sources[i];
        
        if (source->last_gps_data.timestamp > 0) {
            total_quality += source->weight * source->reliability;
            valid_sources++;
        }
    }
    
    if (valid_sources == 0) {
        return 0.0;
    }
    
    return total_quality / valid_sources;
}

// Add fusion history
static void add_fusion_history(const gps_data_t *fused_data) {
    // Shift history array
    for (int i = g_fusion.history_size - 1; i > 0; i--) {
        memcpy(&g_fusion.fusion_history[i], &g_fusion.fusion_history[i-1], 
               sizeof(gps_fusion_record_t));
    }
    
    // Add new record
    g_fusion.fusion_history[0].timestamp = fused_data->timestamp;
    g_fusion.fusion_history[0].lat = fused_data->lat;
    g_fusion.fusion_history[0].lon = fused_data->lon;
    g_fusion.fusion_history[0].altitude = fused_data->altitude;
    g_fusion.fusion_history[0].accuracy = fused_data->accuracy;
    g_fusion.fusion_history[0].confidence = g_fusion.fusion_quality;
    g_fusion.fusion_history[0].source_count = fused_data->satellites;
}

// Find fusion source by name
static int find_fusion_source_by_name(const char *source_name) {
    for (int i = 0; i < MAX_FUSION_SOURCES; i++) {
        if (g_fusion.sources[i].active && 
            strcmp(g_fusion.sources[i].name, source_name) == 0) {
            return i;
        }
    }
    return -1;
}

// Get fusion status
int gps_fusion_get_status(gps_fusion_status_t *status) {
    if (!g_fusion_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_fusion_mutex);
    
    status->enabled = g_fusion.enabled;
    status->fusion_algorithm = g_fusion.fusion_algorithm;
    status->source_count = g_fusion.source_count;
    status->fusion_count = g_fusion.fusion_count;
    status->fusion_quality = g_fusion.fusion_quality;
    status->last_fusion = g_fusion.last_fusion;
    
    // Copy source information
    int active_sources = 0;
    for (int i = 0; i < MAX_FUSION_SOURCES && active_sources < MAX_FUSION_SOURCES; i++) {
        if (g_fusion.sources[i].active) {
            memcpy(&status->sources[active_sources], &g_fusion.sources[i], 
                   sizeof(gps_fusion_source_t));
            active_sources++;
        }
    }
    status->active_source_count = active_sources;
    
    pthread_mutex_unlock(&g_fusion_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get fusion configuration
int gps_fusion_get_config(gps_fusion_config_t *config) {
    if (!g_fusion_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_fusion_mutex);
    
    config->enabled = g_fusion.enabled;
    config->max_sources = g_fusion.max_sources;
    config->min_sources = g_fusion.min_sources;
    config->update_interval = g_fusion.update_interval;
    config->max_source_age = g_fusion.max_source_age;
    config->weight_threshold = g_fusion.weight_threshold;
    config->history_size = g_fusion.history_size;
    config->fusion_algorithm = g_fusion.fusion_algorithm;
    
    pthread_mutex_unlock(&g_fusion_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set fusion configuration
int gps_fusion_set_config(const gps_fusion_config_t *config) {
    if (!g_fusion_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_fusion_mutex);
    
    g_fusion.enabled = config->enabled;
    g_fusion.max_sources = config->max_sources;
    g_fusion.min_sources = config->min_sources;
    g_fusion.update_interval = config->update_interval;
    g_fusion.max_source_age = config->max_source_age;
    g_fusion.weight_threshold = config->weight_threshold;
    g_fusion.history_size = config->history_size;
    g_fusion.fusion_algorithm = config->fusion_algorithm;
    
    pthread_mutex_unlock(&g_fusion_mutex);
    
    LOGX_INFO_MSG("GPS fusion configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable fusion
int gps_fusion_set_enabled(bool enabled) {
    if (!g_fusion_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_fusion_mutex);
    g_fusion.enabled = enabled;
    pthread_mutex_unlock(&g_fusion_mutex);
    
    LOGX_INFO_MSG("GPS fusion %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Force fusion update
int gps_fusion_force_update(void) {
    if (!g_fusion_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    gps_data_t fused_data;
    int result = gps_fusion_perform_fusion(&fused_data);
    
    if (result == AUTONOMY_SUCCESS) {
        LOGX_INFO_MSG("Forced GPS fusion update completed");
    }
    
    return result;
}

// Reset fusion system
int gps_fusion_reset(void) {
    if (!g_fusion_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_fusion_mutex);
    
    g_fusion.source_count = 0;
    g_fusion.fusion_count = 0;
    g_fusion.last_fusion = 0;
    g_fusion.fusion_quality = 0.0;
    
    // Clear all sources
    for (int i = 0; i < MAX_FUSION_SOURCES; i++) {
        g_fusion.sources[i].active = false;
    }
    
    // Clear fusion history
    for (int i = 0; i < FUSION_HISTORY_SIZE; i++) {
        g_fusion.fusion_history[i].timestamp = 0;
        g_fusion.fusion_history[i].lat = 0.0;
        g_fusion.fusion_history[i].lon = 0.0;
        g_fusion.fusion_history[i].altitude = 0.0;
        g_fusion.fusion_history[i].accuracy = 0.0;
        g_fusion.fusion_history[i].confidence = 0.0;
        g_fusion.fusion_history[i].source_count = 0;
    }
    
    pthread_mutex_unlock(&g_fusion_mutex);
    
    LOGX_INFO_MSG("GPS fusion system reset");
    return AUTONOMY_SUCCESS;
}

// Cleanup fusion system
void gps_fusion_cleanup(void) {
    if (!g_fusion_initialized) {
        return;
    }
    
    pthread_mutex_destroy(&g_fusion_mutex);
    g_fusion_initialized = false;
    
    LOGX_INFO_MSG("GPS fusion system cleaned up");
}
