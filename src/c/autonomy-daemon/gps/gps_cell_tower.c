#include "gps_cell_tower.h"
#include "../utils/logx.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>

// Cell tower positioning configuration
static const int MAX_CELL_TOWERS = 100;                     // Maximum cell towers to track
static const int MAX_TOWER_DISTANCE = 50000;                // 50km maximum tower distance
static const double MIN_SIGNAL_STRENGTH = -120.0;            // -120 dBm minimum signal strength
static const int POSITION_UPDATE_INTERVAL = 60;              // 60 second position update interval
static const int MAX_POSITION_HISTORY = 1000;                // Maximum position history records

// Cell network types
static const char* CELL_NETWORK_TYPE_NAMES[] = {
    "unknown", "gsm", "cdma", "umts", "lte", "5g", "wifi", "bluetooth"
};

// Global cell tower positioning state
static gps_cell_tower_t g_cell_tower = {0};
static bool g_cell_tower_initialized = false;
static pthread_mutex_t g_cell_tower_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize cell tower positioning
int gps_cell_tower_init(void) {
    if (g_cell_tower_initialized) {
        LOGX_WARN_MSG("Cell tower positioning already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_cell_tower_mutex);
    
    // Initialize cell tower state
    memset(&g_cell_tower, 0, sizeof(gps_cell_tower_t));
    g_cell_tower.enabled = true;
    g_cell_tower.max_towers = MAX_CELL_TOWERS;
    g_cell_tower.max_distance = MAX_TOWER_DISTANCE;
    g_cell_tower.min_signal_strength = MIN_SIGNAL_STRENGTH;
    g_cell_tower.position_update_interval = POSITION_UPDATE_INTERVAL;
    g_cell_tower.max_position_history = MAX_POSITION_HISTORY;
    
    g_cell_tower.tower_count = 0;
    g_cell_tower.active_towers = 0;
    g_cell_tower.last_position_update = 0;
    g_cell_tower.total_position_updates = 0;
    g_cell_tower.current_lat = 0.0;
    g_cell_tower.current_lon = 0.0;
    g_cell_tower.current_accuracy = 0.0;
    
    // Initialize cell towers array
    for (int i = 0; i < MAX_CELL_TOWERS; i++) {
        g_cell_tower.cell_towers[i].active = false;
        g_cell_tower.cell_towers[i].tower_id = 0;
        g_cell_tower.cell_towers[i].network_type = CELL_NETWORK_TYPE_UNKNOWN;
        g_cell_tower.cell_towers[i].signal_strength = 0.0;
        g_cell_tower.cell_towers[i].distance = 0.0;
        g_cell_tower.cell_towers[i].last_seen = 0;
        g_cell_tower.cell_towers[i].lat = 0.0;
        g_cell_tower.cell_towers[i].lon = 0.0;
        g_cell_tower.cell_towers[i].cell_id = 0;
        g_cell_tower.cell_towers[i].lac = 0;
        g_cell_tower.cell_towers[i].mcc = 0;
        g_cell_tower.cell_towers[i].mnc = 0;
    }
    
    // Initialize position history
    for (int i = 0; i < MAX_POSITION_HISTORY; i++) {
        g_cell_tower.position_history[i].timestamp = 0;
        g_cell_tower.position_history[i].lat = 0.0;
        g_cell_tower.position_history[i].lon = 0.0;
        g_cell_tower.position_history[i].accuracy = 0.0;
        g_cell_tower.position_history[i].tower_count = 0;
        g_cell_tower.position_history[i].method = CELL_POSITION_METHOD_UNKNOWN;
    }
    
    g_cell_tower_initialized = true;
    pthread_mutex_unlock(&g_cell_tower_mutex);
    
    LOGX_INFO_MSG("Cell tower positioning initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Add cell tower information
int gps_cell_tower_add_tower(const gps_cell_tower_info_t *tower_info) {
    if (!g_cell_tower_initialized || !tower_info) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_cell_tower_mutex);
    
    // Check if tower already exists
    for (int i = 0; i < g_cell_tower.tower_count; i++) {
        if (g_cell_tower.cell_towers[i].active && 
            g_cell_tower.cell_towers[i].cell_id == tower_info->cell_id &&
            g_cell_tower.cell_towers[i].lac == tower_info->lac &&
            g_cell_tower.cell_towers[i].mcc == tower_info->mcc &&
            g_cell_tower.cell_towers[i].mnc == tower_info->mnc) {
            
            // Update existing tower
            gps_cell_tower_record_t *tower = &g_cell_tower.cell_towers[i];
            tower->signal_strength = tower_info->signal_strength;
            tower->distance = tower_info->distance;
            tower->last_seen = time(NULL);
            tower->lat = tower_info->lat;
            tower->lon = tower_info->lon;
            
            pthread_mutex_unlock(&g_cell_tower_mutex);
            
            LOGX_DEBUG_MSG("Updated existing cell tower: cell_id=%d, signal=%.1f dBm", 
                       tower_info->cell_id, tower_info->signal_strength);
            
            return AUTONOMY_SUCCESS;
        }
    }
    
    // Find free tower slot
    int tower_index = -1;
    for (int i = 0; i < MAX_CELL_TOWERS; i++) {
        if (!g_cell_tower.cell_towers[i].active) {
            tower_index = i;
            break;
        }
    }
    
    if (tower_index < 0) {
        // Remove oldest tower to make room
        tower_index = find_oldest_tower();
        if (tower_index >= 0) {
            g_cell_tower.cell_towers[tower_index].active = false;
            g_cell_tower.active_towers--;
        }
    }
    
    if (tower_index < 0) {
        pthread_mutex_unlock(&g_cell_tower_mutex);
        LOGX_ERROR_MSG("No free slots for cell tower");
        return AUTONOMY_ERROR_NO_RESOURCES;
    }
    
    // Initialize cell tower
    gps_cell_tower_record_t *tower = &g_cell_tower.cell_towers[tower_index];
    tower->active = true;
    tower->tower_id = generate_tower_id();
    tower->network_type = tower_info->network_type;
    tower->signal_strength = tower_info->signal_strength;
    tower->distance = tower_info->distance;
    tower->last_seen = time(NULL);
    tower->lat = tower_info->lat;
    tower->lon = tower_info->lon;
    tower->cell_id = tower_info->cell_id;
    tower->lac = tower_info->lac;
    tower->mcc = tower_info->mcc;
    tower->mnc = tower_info->mnc;
    
    if (tower_index >= g_cell_tower.tower_count) {
        g_cell_tower.tower_count = tower_index + 1;
    }
    g_cell_tower.active_towers++;
    
    pthread_mutex_unlock(&g_cell_tower_mutex);
    
    LOGX_INFO_MSG("Added cell tower: cell_id=%d, network=%d, signal=%.1f dBm, pos=(%.6f, %.6f)", 
               tower_info->cell_id, tower_info->network_type, tower_info->signal_strength,
               tower_info->lat, tower_info->lon);
    
    return AUTONOMY_SUCCESS;
}

// Generate unique tower ID
int generate_tower_id(void) {
    static int next_id = 6000;
    return next_id++;
}

// Find oldest tower
int find_oldest_tower(void) {
    int oldest_index = -1;
    time_t oldest_time = time(NULL);
    
    for (int i = 0; i < g_cell_tower.tower_count; i++) {
        if (g_cell_tower.cell_towers[i].active && 
            g_cell_tower.cell_towers[i].last_seen < oldest_time) {
            oldest_time = g_cell_tower.cell_towers[i].last_seen;
            oldest_index = i;
        }
    }
    
    return oldest_index;
}

// Update cell tower positioning
int gps_cell_tower_update_position(void) {
    if (!g_cell_tower_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_cell_tower_mutex);
    
    time_t now = time(NULL);
    
    // Check if enough time has passed since last position update
    if ((now - g_cell_tower.last_position_update) < g_cell_tower.position_update_interval) {
        pthread_mutex_unlock(&g_cell_tower_mutex);
        return AUTONOMY_SUCCESS;
    }
    
    g_cell_tower.last_position_update = now;
    g_cell_tower.total_position_updates++;
    
    // Calculate position using available towers
    if (g_cell_tower.active_towers >= 3) {
        calculate_triangulated_position();
    } else if (g_cell_tower.active_towers >= 1) {
        calculate_single_tower_position();
    } else {
        // No towers available
        g_cell_tower.current_lat = 0.0;
        g_cell_tower.current_lon = 0.0;
        g_cell_tower.current_accuracy = 0.0;
    }
    
    // Add to position history
    add_position_history();
    
    pthread_mutex_unlock(&g_cell_tower_mutex);
    
    LOGX_DEBUG_MSG("Cell tower position updated: (%.6f, %.6f) accuracy: %.1fm", 
               g_cell_tower.current_lat, g_cell_tower.current_lon, g_cell_tower.current_accuracy);
    
    return AUTONOMY_SUCCESS;
}

// Calculate position using triangulation
void calculate_triangulated_position(void) {
    double total_weight = 0.0;
    double weighted_lat = 0.0;
    double weighted_lon = 0.0;
    double min_accuracy = 1000.0;
    
    // Use signal strength and distance to calculate weights
    for (int i = 0; i < g_cell_tower.tower_count; i++) {
        if (!g_cell_tower.cell_towers[i].active) {
            continue;
        }
        
        gps_cell_tower_record_t *tower = &g_cell_tower.cell_towers[i];
        
        // Calculate weight based on signal strength and distance
        double signal_weight = fmax(0.0, (tower->signal_strength - g_cell_tower.min_signal_strength) / 100.0);
        double distance_weight = fmax(0.0, 1.0 - (tower->distance / g_cell_tower.max_distance));
        double weight = signal_weight * distance_weight;
        
        if (weight > 0.0) {
            weighted_lat += tower->lat * weight;
            weighted_lon += tower->lon * weight;
            total_weight += weight;
            
            // Estimate accuracy based on tower distribution
            double tower_accuracy = tower->distance * 0.1; // 10% of distance
            if (tower_accuracy < min_accuracy) {
                min_accuracy = tower_accuracy;
            }
        }
    }
    
    if (total_weight > 0.0) {
        g_cell_tower.current_lat = weighted_lat / total_weight;
        g_cell_tower.current_lon = weighted_lon / total_weight;
        g_cell_tower.current_accuracy = fmax(min_accuracy, 100.0); // Minimum 100m accuracy
    }
}

// Calculate position using single tower
void calculate_single_tower_position(void) {
    // Find the tower with the strongest signal
    int best_tower_index = -1;
    double best_signal = g_cell_tower.min_signal_strength;
    
    for (int i = 0; i < g_cell_tower.tower_count; i++) {
        if (!g_cell_tower.cell_towers[i].active) {
            continue;
        }
        
        gps_cell_tower_record_t *tower = &g_cell_tower.cell_towers[i];
        if (tower->signal_strength > best_signal) {
            best_signal = tower->signal_strength;
            best_tower_index = i;
        }
    }
    
    if (best_tower_index >= 0) {
        gps_cell_tower_record_t *tower = &g_cell_tower.cell_towers[best_tower_index];
        
        // Use tower position with estimated accuracy based on signal strength
        g_cell_tower.current_lat = tower->lat;
        g_cell_tower.current_lon = tower->lon;
        
        // Estimate accuracy based on signal strength
        double signal_quality = fmax(0.0, (tower->signal_strength - g_cell_tower.min_signal_strength) / 100.0);
        g_cell_tower.current_accuracy = 500.0 + (500.0 * (1.0 - signal_quality)); // 500m to 1000m accuracy
    }
}

// Add position to history
void add_position_history(void) {
    // Shift position history array
    for (int i = g_cell_tower.max_position_history - 1; i > 0; i--) {
        memcpy(&g_cell_tower.position_history[i], &g_cell_tower.position_history[i-1], 
               sizeof(gps_cell_position_record_t));
    }
    
    // Add new position record
    g_cell_tower.position_history[0].timestamp = time(NULL);
    g_cell_tower.position_history[0].lat = g_cell_tower.current_lat;
    g_cell_tower.position_history[0].lon = g_cell_tower.current_lon;
    g_cell_tower.position_history[0].accuracy = g_cell_tower.current_accuracy;
    g_cell_tower.position_history[0].tower_count = g_cell_tower.active_towers;
    g_cell_tower.position_history[0].method = g_cell_tower.active_towers >= 3 ? 
                                             CELL_POSITION_METHOD_TRIANGULATION : 
                                             CELL_POSITION_METHOD_SINGLE_TOWER;
}

// Get cell tower position
int gps_cell_tower_get_position(double *lat, double *lon, double *accuracy) {
    if (!g_cell_tower_initialized || !lat || !lon || !accuracy) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_cell_tower_mutex);
    
    *lat = g_cell_tower.current_lat;
    *lon = g_cell_tower.current_lon;
    *accuracy = g_cell_tower.current_accuracy;
    
    pthread_mutex_unlock(&g_cell_tower_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get cell tower status
int gps_cell_tower_get_status(gps_cell_tower_status_t *status) {
    if (!g_cell_tower_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_cell_tower_mutex);
    
    status->enabled = g_cell_tower.enabled;
    status->tower_count = g_cell_tower.tower_count;
    status->active_towers = g_cell_tower.active_towers;
    status->last_position_update = g_cell_tower.last_position_update;
    status->total_position_updates = g_cell_tower.total_position_updates;
    status->current_lat = g_cell_tower.current_lat;
    status->current_lon = g_cell_tower.current_lon;
    status->current_accuracy = g_cell_tower.current_accuracy;
    
    // Copy active tower information
    int active_towers = 0;
    for (int i = 0; i < g_cell_tower.max_towers && active_towers < 50; i++) {
        if (g_cell_tower.cell_towers[i].active) {
            memcpy(&status->active_towers_info[active_towers], &g_cell_tower.cell_towers[i], 
                   sizeof(gps_cell_tower_record_t));
            active_towers++;
        }
    }
    status->active_tower_count = active_towers;
    
    pthread_mutex_unlock(&g_cell_tower_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get cell tower configuration
int gps_cell_tower_get_config(gps_cell_tower_config_t *config) {
    if (!g_cell_tower_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_cell_tower_mutex);
    
    config->enabled = g_cell_tower.enabled;
    config->max_towers = g_cell_tower.max_towers;
    config->max_distance = g_cell_tower.max_distance;
    config->min_signal_strength = g_cell_tower.min_signal_strength;
    config->position_update_interval = g_cell_tower.position_update_interval;
    config->max_position_history = g_cell_tower.max_position_history;
    
    pthread_mutex_unlock(&g_cell_tower_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set cell tower configuration
int gps_cell_tower_set_config(const gps_cell_tower_config_t *config) {
    if (!g_cell_tower_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_cell_tower_mutex);
    
    g_cell_tower.enabled = config->enabled;
    g_cell_tower.max_towers = config->max_towers;
    g_cell_tower.max_distance = config->max_distance;
    g_cell_tower.min_signal_strength = config->min_signal_strength;
    g_cell_tower.position_update_interval = config->position_update_interval;
    g_cell_tower.max_position_history = config->max_position_history;
    
    pthread_mutex_unlock(&g_cell_tower_mutex);
    
    LOGX_INFO_MSG("Cell tower positioning configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable cell tower positioning
int gps_cell_tower_set_enabled(bool enabled) {
    if (!g_cell_tower_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_cell_tower_mutex);
    g_cell_tower.enabled = enabled;
    pthread_mutex_unlock(&g_cell_tower_mutex);
    
    LOGX_INFO_MSG("Cell tower positioning %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Force position update
int gps_cell_tower_force_position_update(void) {
    if (!g_cell_tower_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    // Reset last update time to force immediate update
    pthread_mutex_lock(&g_cell_tower_mutex);
    g_cell_tower.last_position_update = 0;
    pthread_mutex_unlock(&g_cell_tower_mutex);
    
    LOGX_INFO_MSG("Cell tower position update forced");
    return gps_cell_tower_update_position();
}

// Get cell tower statistics
int gps_cell_tower_get_statistics(gps_cell_tower_stats_t *stats) {
    if (!g_cell_tower_initialized || !stats) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_cell_tower_mutex);
    
    // Calculate statistics from cell towers
    memset(stats, 0, sizeof(gps_cell_tower_stats_t));
    
    for (int i = 0; i < g_cell_tower.tower_count; i++) {
        if (!g_cell_tower.cell_towers[i].active) {
            continue;
        }
        
        gps_cell_tower_record_t *tower = &g_cell_tower.cell_towers[i];
        
        // Count towers by network type
        if (tower->network_type < CELL_NETWORK_TYPE_MAX) {
            stats->tower_counts[tower->network_type]++;
        }
        
        // Calculate averages
        stats->total_signal_strength += tower->signal_strength;
        stats->total_distance += tower->distance;
        stats->total_towers++;
    }
    
    if (stats->total_towers > 0) {
        stats->average_signal_strength = stats->total_signal_strength / stats->total_towers;
        stats->average_distance = stats->total_distance / stats->total_towers;
    }
    
    stats->total_position_updates = g_cell_tower.total_position_updates;
    stats->current_accuracy = g_cell_tower.current_accuracy;
    
    pthread_mutex_unlock(&g_cell_tower_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Reset cell tower positioning
int gps_cell_tower_reset(void) {
    if (!g_cell_tower_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_cell_tower_mutex);
    
    g_cell_tower.tower_count = 0;
    g_cell_tower.active_towers = 0;
    g_cell_tower.last_position_update = 0;
    g_cell_tower.total_position_updates = 0;
    g_cell_tower.current_lat = 0.0;
    g_cell_tower.current_lon = 0.0;
    g_cell_tower.current_accuracy = 0.0;
    
    // Clear all cell towers
    for (int i = 0; i < MAX_CELL_TOWERS; i++) {
        g_cell_tower.cell_towers[i].active = false;
        g_cell_tower.cell_towers[i].tower_id = 0;
        g_cell_tower.cell_towers[i].network_type = CELL_NETWORK_TYPE_UNKNOWN;
        g_cell_tower.cell_towers[i].signal_strength = 0.0;
        g_cell_tower.cell_towers[i].distance = 0.0;
        g_cell_tower.cell_towers[i].last_seen = 0;
        g_cell_tower.cell_towers[i].lat = 0.0;
        g_cell_tower.cell_towers[i].lon = 0.0;
        g_cell_tower.cell_towers[i].cell_id = 0;
        g_cell_tower.cell_towers[i].lac = 0;
        g_cell_tower.cell_towers[i].mcc = 0;
        g_cell_tower.cell_towers[i].mnc = 0;
    }
    
    // Clear position history
    for (int i = 0; i < MAX_POSITION_HISTORY; i++) {
        g_cell_tower.position_history[i].timestamp = 0;
        g_cell_tower.position_history[i].lat = 0.0;
        g_cell_tower.position_history[i].lon = 0.0;
        g_cell_tower.position_history[i].accuracy = 0.0;
        g_cell_tower.position_history[i].tower_count = 0;
        g_cell_tower.position_history[i].method = CELL_POSITION_METHOD_UNKNOWN;
    }
    
    pthread_mutex_unlock(&g_cell_tower_mutex);
    
    LOGX_INFO_MSG("Cell tower positioning reset");
    return AUTONOMY_SUCCESS;
}

// Cleanup cell tower positioning
void gps_cell_tower_cleanup(void) {
    if (!g_cell_tower_initialized) {
        return;
    }
    
    pthread_mutex_destroy(&g_cell_tower_mutex);
    g_cell_tower_initialized = false;
    
    LOGX_INFO_MSG("Cell tower positioning cleaned up");
}
