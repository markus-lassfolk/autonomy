#include "gps_manager.h"
#include "gps_opencellid_enhanced.h"
#include "../starlink/starlink_tracker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <pthread.h>

// Maximum number of location sources
#define MAX_LOCATION_SOURCES 8

// Location manager state
typedef struct {
    location_manager_config_t config;
    gps_source_t sources[MAX_LOCATION_SOURCES];
    int source_count;
    bool initialized;
    pthread_mutex_t mutex;
    time_t last_update;
} location_manager_state_t;

static location_manager_state_t g_location_manager = {0};

// Initialize location manager
int location_manager_init(const location_manager_config_t* config) {
    if (!config) {
        return -1;
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&g_location_manager.mutex, NULL) != 0) {
        return -1;
    }
    
    // Copy configuration
    memcpy(&g_location_manager.config, config, sizeof(location_manager_config_t));
    
    // Initialize sources
    memset(g_location_manager.sources, 0, sizeof(g_location_manager.sources));
    g_location_manager.source_count = 0;
    g_location_manager.last_update = 0;
    
    g_location_manager.initialized = true;
    return 0;
}

// Add location source
int location_manager_add_source(gps_source_type_t source_type, const char* name) {
    if (!g_location_manager.initialized || !name) {
        return -1;
    }
    
    pthread_mutex_lock(&g_location_manager.mutex);
    
    if (g_location_manager.source_count >= MAX_LOCATION_SOURCES) {
        pthread_mutex_unlock(&g_location_manager.mutex);
        return -1;
    }
    
    // Check if source already exists
    for (int i = 0; i < g_location_manager.source_count; i++) {
        if (g_location_manager.sources[i].type == source_type) {
            pthread_mutex_unlock(&g_location_manager.mutex);
            return 0; // Already exists
        }
    }
    
    // Add new source
    gps_source_t* source = &g_location_manager.sources[g_location_manager.source_count];
    source->enabled = true;
    source->type = source_type;
    strncpy(source->name, name, sizeof(source->name) - 1);
    source->name[sizeof(source->name) - 1] = '\0';
    source->name[sizeof(source->name) - 1] = '\0';
    source->last_update = 0;
    source->reliability_score = 0.5; // Default reliability
    source->data_quality = 0.5;      // Default quality
    memset(&source->location_data, 0, sizeof(location_data_t));
    
    g_location_manager.source_count++;
    
    pthread_mutex_unlock(&g_location_manager.mutex);
    return 0;
}

// Remove location source
int location_manager_remove_source(gps_source_type_t source_type) {
    if (!g_location_manager.initialized) {
        return -1;
    }
    
    pthread_mutex_lock(&g_location_manager.mutex);
    
    for (int i = 0; i < g_location_manager.source_count; i++) {
        if (g_location_manager.sources[i].type == source_type) {
            // Shift remaining sources
            for (int j = i; j < g_location_manager.source_count - 1; j++) {
                g_location_manager.sources[j] = g_location_manager.sources[j + 1];
            }
            g_location_manager.source_count--;
            break;
        }
    }
    
    pthread_mutex_unlock(&g_location_manager.mutex);
    return 0;
}

// Update location source data
int location_manager_update_source(gps_source_type_t source_type, const location_data_t* location) {
    if (!g_location_manager.initialized || !location) {
        return -1;
    }
    
    pthread_mutex_lock(&g_location_manager.mutex);
    
    for (int i = 0; i < g_location_manager.source_count; i++) {
        if (g_location_manager.sources[i].type == source_type) {
            gps_source_t* source = &g_location_manager.sources[i];
            memcpy(&source->location_data, location, sizeof(location_data_t));
            source->last_update = time(NULL);
            
            // Update reliability score based on data quality
            if (location->valid && location->confidence > 0.7) {
                source->reliability_score = fmin(source->reliability_score + 0.1, 1.0);
            } else if (!location->valid || location->confidence < 0.3) {
                source->reliability_score = fmax(source->reliability_score - 0.1, 0.0);
            }
            
            source->data_quality = location->confidence;
            break;
        }
    }
    
    pthread_mutex_unlock(&g_location_manager.mutex);
    return 0;
}

// Validate location data
bool location_manager_validate_location(const location_data_t* location) {
    if (!location) {
        return false;
    }
    
    // Check basic validity
    if (!location->valid) {
        return false;
    }
    
    // Check confidence threshold
    if (location->confidence < g_location_manager.config.min_confidence) {
        return false;
    }
    
    // Check accuracy threshold
    if (location->accuracy > g_location_manager.config.min_accuracy) {
        return false;
    }
    
    // Check coordinate validity
    if (location->latitude < -90.0 || location->latitude > 90.0) {
        return false;
    }
    
    if (location->longitude < -180.0 || location->longitude > 180.0) {
        return false;
    }
    
    // Check timestamp (not too old)
    time_t now = time(NULL);
    if (now - location->timestamp > 300) { // 5 minutes
        return false;
    }
    
    return true;
}

// Fuse multiple location sources
int location_manager_fuse_sources(const location_data_t* sources, int source_count, location_data_t* fused_location) {
    if (!sources || !fused_location || source_count <= 0) {
        return -1;
    }
    
    // Initialize fused location
    memset(fused_location, 0, sizeof(location_data_t));
    fused_location->timestamp = time(NULL);
    fused_location->valid = false;
    
    // Find valid sources
    int valid_count = 0;
    double total_weight = 0.0;
    double weighted_lat = 0.0;
    double weighted_lon = 0.0;
    double weighted_alt = 0.0;
    double min_accuracy = 999999.0;
    double max_confidence = 0.0;
    
    for (int i = 0; i < source_count; i++) {
        if (location_manager_validate_location(&sources[i])) {
            valid_count++;
            
            // Calculate weight based on confidence and accuracy
            double weight = sources[i].confidence / (sources[i].accuracy + 1.0);
            total_weight += weight;
            
            weighted_lat += sources[i].latitude * weight;
            weighted_lon += sources[i].longitude * weight;
            weighted_alt += sources[i].altitude * weight;
            
            if (sources[i].accuracy < min_accuracy) {
                min_accuracy = sources[i].accuracy;
            }
            
            if (sources[i].confidence > max_confidence) {
                max_confidence = sources[i].confidence;
            }
        }
    }
    
    if (valid_count == 0) {
        return -1;
    }
    
    // Calculate fused location
    if (total_weight > 0.0) {
        fused_location->latitude = weighted_lat / total_weight;
        fused_location->longitude = weighted_lon / total_weight;
        fused_location->altitude = weighted_alt / total_weight;
        fused_location->accuracy = min_accuracy;
        fused_location->confidence = max_confidence;
        fused_location->valid = true;
        fused_location->source_type = GPS_SOURCE_COMBINED;
        strncpy(fused_location->source_name, "fused", sizeof(fused_location->source_name) - 1);
        fused_location->source_name[sizeof(fused_location->source_name) - 1] = '\0';
        
        // Determine quality score
        if (fused_location->confidence >= 0.9) {
            strncpy(fused_location->quality_score, "excellent", sizeof(fused_location->quality_score) - 1);
            fused_location->quality_score[sizeof(fused_location->quality_score) - 1] = '\0';
        } else if (fused_location->confidence >= 0.7) {
            strncpy(fused_location->quality_score, "good", sizeof(fused_location->quality_score) - 1);
            fused_location->quality_score[sizeof(fused_location->quality_score) - 1] = '\0';
        } else if (fused_location->confidence >= 0.5) {
            strncpy(fused_location->quality_score, "fair", sizeof(fused_location->quality_score) - 1);
            fused_location->quality_score[sizeof(fused_location->quality_score) - 1] = '\0';
        } else {
            strncpy(fused_location->quality_score, "poor", sizeof(fused_location->quality_score) - 1);
            fused_location->quality_score[sizeof(fused_location->quality_score) - 1] = '\0';
        }
        fused_location->quality_score[sizeof(fused_location->quality_score) - 1] = '\0';
    }
    
    return 0;
}

// Get location from specific source
int location_manager_get_location_from_source(gps_source_type_t source_type, location_data_t* location) {
    if (!g_location_manager.initialized || !location) {
        return -1;
    }
    
    pthread_mutex_lock(&g_location_manager.mutex);
    
    // Find the source
    gps_source_t* source = NULL;
    for (int i = 0; i < g_location_manager.source_count; i++) {
        if (g_location_manager.sources[i].type == source_type && g_location_manager.sources[i].enabled) {
            source = &g_location_manager.sources[i];
            break;
        }
    }
    
    if (!source) {
        pthread_mutex_unlock(&g_location_manager.mutex);
        return -1;
    }
    
    // Copy location data
    memcpy(location, &source->location_data, sizeof(location_data_t));
    
    pthread_mutex_unlock(&g_location_manager.mutex);
    return 0;
}

// Get best location from all available sources
int location_manager_get_best_location(location_data_t* location) {
    if (!g_location_manager.initialized || !location) {
        return -1;
    }
    
    pthread_mutex_lock(&g_location_manager.mutex);
    
    // Collect valid locations from all sources
    location_data_t valid_sources[MAX_LOCATION_SOURCES];
    int valid_count = 0;
    
    for (int i = 0; i < g_location_manager.source_count; i++) {
        gps_source_t* source = &g_location_manager.sources[i];
        if (source->enabled && location_manager_validate_location(&source->location_data)) {
            valid_sources[valid_count++] = source->location_data;
        }
    }
    
    pthread_mutex_unlock(&g_location_manager.mutex);
    
    if (valid_count == 0) {
        return -1;
    }
    
    if (valid_count == 1) {
        // Single valid source
        memcpy(location, &valid_sources[0], sizeof(location_data_t));
        return 0;
    }
    
    // Multiple sources - use fusion if enabled
    if (g_location_manager.config.enable_fusion) {
        return location_manager_fuse_sources(valid_sources, valid_count, location);
    } else {
        // Find best single source (highest confidence * reliability)
        int best_index = 0;
        double best_score = 0.0;
        
        for (int i = 0; i < valid_count; i++) {
            // Find source reliability
            double reliability = 0.5; // Default
            for (int j = 0; j < g_location_manager.source_count; j++) {
                if (g_location_manager.sources[j].type == valid_sources[i].source_type) {
                    reliability = g_location_manager.sources[j].reliability_score;
                    break;
                }
            }
            
            double score = valid_sources[i].confidence * reliability;
            if (score > best_score) {
                best_score = score;
                best_index = i;
            }
        }
        
        memcpy(location, &valid_sources[best_index], sizeof(location_data_t));
        return 0;
    }
}

// Get location manager status
int location_manager_get_status(gps_manager_status_t* status) {
    if (!g_location_manager.initialized || !status) {
        return -1;
    }
    
    pthread_mutex_lock(&g_location_manager.mutex);
    
    status->enabled = g_location_manager.config.enabled;
    status->update_interval = g_location_manager.config.update_interval;
    status->source_timeout = g_location_manager.config.source_timeout;
    status->last_update = g_location_manager.last_update;
    status->source_count = g_location_manager.source_count;
    
    // Find best source
    status->best_source = -1;
    double best_score = 0.0;
    for (int i = 0; i < g_location_manager.source_count; i++) {
        gps_source_t* source = &g_location_manager.sources[i];
        if (source->enabled) {
            double score = source->reliability_score * source->data_quality;
            if (score > best_score) {
                best_score = score;
                status->best_source = i;
            }
        }
    }
    
    pthread_mutex_unlock(&g_location_manager.mutex);
    return 0;
}

// Cleanup location manager
void location_manager_cleanup(void) {
    if (!g_location_manager.initialized) {
        return;
    }
    
    pthread_mutex_lock(&g_location_manager.mutex);
    
    g_location_manager.source_count = 0;
    g_location_manager.initialized = false;
    
    pthread_mutex_unlock(&g_location_manager.mutex);
    
    pthread_mutex_destroy(&g_location_manager.mutex);
}
