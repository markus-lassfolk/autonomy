#include "gps_opencellid_enhanced.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <openssl/sha.h>

// Global intelligent cache instance
static intelligent_cache_t g_intelligent_cache = {0};
static bool g_cache_initialized = false;

// Initialize intelligent cache system
static int intelligent_cache_init(const intelligent_cache_config_t* config) {
    if (!config) {
        return -1;
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&g_intelligent_cache.mutex, NULL) != 0) {
        return -1;
    }
    
    // Copy configuration
    memcpy(&g_intelligent_cache.config, config, sizeof(intelligent_cache_config_t));
    
    // Initialize state
    g_intelligent_cache.last_environment = NULL;
    g_intelligent_cache.last_location_result = NULL;
    g_intelligent_cache.last_location_query = 0;
    g_intelligent_cache.debounce_timer = 0;
    
    g_cache_initialized = true;
    return 0;
}

// Cleanup intelligent cache system
static void intelligent_cache_cleanup(void) {
    if (!g_cache_initialized) {
        return;
    }
    
    pthread_mutex_lock(&g_intelligent_cache.mutex);
    
    // Free allocated memory
    if (g_intelligent_cache.last_environment) {
        free(g_intelligent_cache.last_environment);
        g_intelligent_cache.last_environment = NULL;
    }
    
    if (g_intelligent_cache.last_location_result) {
        free(g_intelligent_cache.last_location_result);
        g_intelligent_cache.last_location_result = NULL;
    }
    
    pthread_mutex_unlock(&g_intelligent_cache.mutex);
    
    pthread_mutex_destroy(&g_intelligent_cache.mutex);
    g_cache_initialized = false;
}

// Generate location hash for environment comparison
static void intelligent_cache_generate_location_hash(const cell_environment_t* env, char* hash) {
    if (!env || !hash) {
        return;
    }
    
    // Create a string representation of the environment
    char env_string[1024];
    snprintf(env_string, sizeof(env_string), 
             "%s:%d:%d:%d:%d:%s",
             env->serving_cell.cell_id,
             env->serving_cell.rsrp,
             env->serving_cell.rsrq,
             env->serving_cell.earfcn,
             env->serving_cell.pci,
             env->serving_cell.type);
    
    // Add neighbor cells
    for (int i = 0; i < env->neighbor_count && i < 16; i++) {
        char neighbor_str[128];
        snprintf(neighbor_str, sizeof(neighbor_str), 
                 ",%s:%d:%d:%d:%d:%s",
                 env->neighbor_cells[i].cell_id,
                 env->neighbor_cells[i].rsrp,
                 env->neighbor_cells[i].rsrq,
                 env->neighbor_cells[i].earfcn,
                 env->neighbor_cells[i].pci,
                 env->neighbor_cells[i].type);
        
        strncat(env_string, neighbor_str, sizeof(env_string) - strlen(env_string) - 1);
    }
    
    // Generate SHA256 hash
    unsigned char sha_hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)env_string, strlen(env_string), sha_hash);
    
    // Convert to hex string
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(&hash[i * 2], "%02x", sha_hash[i]);
    }
    hash[SHA256_DIGEST_LENGTH * 2] = '\0';
}

// Calculate tower change percentage between environments
double intelligent_cache_calculate_tower_change_percentage(const cell_environment_t* current, 
                                                          const cell_environment_t* previous) {
    if (!current || !previous) {
        return 1.0; // 100% change if either is NULL
    }
    
    int total_towers = 0;
    int changed_towers = 0;
    
    // Count serving cell
    total_towers++;
    if (strcmp(current->serving_cell.cell_id, previous->serving_cell.cell_id) != 0) {
        changed_towers++;
    }
    
    // Count neighbor cells
    int max_neighbors = (current->neighbor_count > previous->neighbor_count) ? 
                       current->neighbor_count : previous->neighbor_count;
    
    for (int i = 0; i < max_neighbors; i++) {
        total_towers++;
        
        bool current_exists = (i < current->neighbor_count);
        bool previous_exists = (i < previous->neighbor_count);
        
        if (current_exists && previous_exists) {
            if (strcmp(current->neighbor_cells[i].cell_id, previous->neighbor_cells[i].cell_id) != 0) {
                changed_towers++;
            }
        } else {
            // Tower appeared or disappeared
            changed_towers++;
        }
    }
    
    return (total_towers > 0) ? (double)changed_towers / total_towers : 0.0;
}

// Check if top towers have changed significantly
bool intelligent_cache_top_towers_changed(const cell_environment_t* current, 
                                         const cell_environment_t* previous) {
    if (!current || !previous) {
        return true;
    }
    
    // Sort towers by signal strength (RSRP) - higher is better
    cell_tower_info_t current_sorted[17]; // serving + up to 16 neighbors
    cell_tower_info_t previous_sorted[17];
    
    int current_count = 0;
    int previous_count = 0;
    
    // Add serving cell
    current_sorted[current_count++] = current->serving_cell;
    previous_sorted[previous_count++] = previous->serving_cell;
    
    // Add neighbor cells
    for (int i = 0; i < current->neighbor_count; i++) {
        current_sorted[current_count++] = current->neighbor_cells[i];
    }
    for (int i = 0; i < previous->neighbor_count; i++) {
        previous_sorted[previous_count++] = previous->neighbor_cells[i];
    }
    
    // Simple bubble sort by RSRP (descending)
    for (int i = 0; i < current_count - 1; i++) {
        for (int j = 0; j < current_count - i - 1; j++) {
            if (current_sorted[j].rsrp < current_sorted[j + 1].rsrp) {
                cell_tower_info_t temp = current_sorted[j];
                current_sorted[j] = current_sorted[j + 1];
                current_sorted[j + 1] = temp;
            }
        }
    }
    
    for (int i = 0; i < previous_count - 1; i++) {
        for (int j = 0; j < previous_count - i - 1; j++) {
            if (previous_sorted[j].rsrp < previous_sorted[j + 1].rsrp) {
                cell_tower_info_t temp = previous_sorted[j];
                previous_sorted[j] = previous_sorted[j + 1];
                previous_sorted[j + 1] = temp;
            }
        }
    }
    
    // Check top N towers
    int top_count = g_intelligent_cache.config.top_towers_count;
    if (top_count > current_count) top_count = current_count;
    if (top_count > previous_count) top_count = previous_count;
    
    int changed_top_towers = 0;
    for (int i = 0; i < top_count; i++) {
        if (strcmp(current_sorted[i].cell_id, previous_sorted[i].cell_id) != 0) {
            changed_top_towers++;
        }
    }
    
    // Return true if 2 or more top towers changed
    return changed_top_towers >= 2;
}

// Check if new location query should be made based on environment changes
static bool intelligent_cache_should_query_new_location(const cell_environment_t* env) {
    if (!g_cache_initialized || !env) {
        return true; // Query if not initialized or no environment
    }
    
    pthread_mutex_lock(&g_intelligent_cache.mutex);
    
    time_t now = time(NULL);
    
    // Check debounce timer
    if (now - g_intelligent_cache.debounce_timer < g_intelligent_cache.config.debounce_delay) {
        pthread_mutex_unlock(&g_intelligent_cache.mutex);
        return false; // Still in debounce period
    }
    
    // First query
    if (g_intelligent_cache.last_environment == NULL) {
        pthread_mutex_unlock(&g_intelligent_cache.mutex);
        return true;
    }
    
    // Check serving cell change
    if (strcmp(env->serving_cell.cell_id, g_intelligent_cache.last_environment->serving_cell.cell_id) != 0) {
        pthread_mutex_unlock(&g_intelligent_cache.mutex);
        return true;
    }
    
    // Check neighbor change threshold
    double change_percent = intelligent_cache_calculate_tower_change_percentage(env, g_intelligent_cache.last_environment);
    if (change_percent >= g_intelligent_cache.config.tower_change_threshold) {
        pthread_mutex_unlock(&g_intelligent_cache.mutex);
        return true;
    }
    
    // Check top tower changes
    if (intelligent_cache_top_towers_changed(env, g_intelligent_cache.last_environment)) {
        pthread_mutex_unlock(&g_intelligent_cache.mutex);
        return true;
    }
    
    // Check cache expiration
    if (now - g_intelligent_cache.last_location_query > g_intelligent_cache.config.max_cache_age) {
        pthread_mutex_unlock(&g_intelligent_cache.mutex);
        return true;
    }
    
    pthread_mutex_unlock(&g_intelligent_cache.mutex);
    return false;
}

// Update cache with new location result
static void intelligent_cache_update_location(const cell_environment_t* env, const opencellid_response_t* result) {
    if (!g_cache_initialized || !env || !result) {
        return;
    }
    
    pthread_mutex_lock(&g_intelligent_cache.mutex);
    
    // Free previous environment
    if (g_intelligent_cache.last_environment) {
        free(g_intelligent_cache.last_environment);
    }
    
    // Allocate and copy new environment
    g_intelligent_cache.last_environment = malloc(sizeof(cell_environment_t));
    if (g_intelligent_cache.last_environment) {
        memcpy(g_intelligent_cache.last_environment, env, sizeof(cell_environment_t));
        intelligent_cache_generate_location_hash(env, g_intelligent_cache.last_environment->location_hash);
    }
    
    // Free previous result
    if (g_intelligent_cache.last_location_result) {
        free(g_intelligent_cache.last_location_result);
    }
    
    // Allocate and copy new result
    g_intelligent_cache.last_location_result = malloc(sizeof(opencellid_response_t));
    if (g_intelligent_cache.last_location_result) {
        memcpy(g_intelligent_cache.last_location_result, result, sizeof(opencellid_response_t));
    }
    
    // Update timestamps
    g_intelligent_cache.last_location_query = time(NULL);
    g_intelligent_cache.debounce_timer = g_intelligent_cache.last_location_query;
    
    pthread_mutex_unlock(&g_intelligent_cache.mutex);
}

// Get cached location if available and valid
static bool intelligent_cache_get_cached_location(opencellid_response_t* result) {
    if (!g_cache_initialized || !result) {
        return false;
    }
    
    pthread_mutex_lock(&g_intelligent_cache.mutex);
    
    time_t now = time(NULL);
    
    // Check if cache is valid
    if (g_intelligent_cache.last_location_result == NULL) {
        pthread_mutex_unlock(&g_intelligent_cache.mutex);
        return false;
    }
    
    // Check cache age
    if (now - g_intelligent_cache.last_location_query > g_intelligent_cache.config.max_cache_age) {
        pthread_mutex_unlock(&g_intelligent_cache.mutex);
        return false;
    }
    
    // Copy cached result
    memcpy(result, g_intelligent_cache.last_location_result, sizeof(opencellid_response_t));
    
    pthread_mutex_unlock(&g_intelligent_cache.mutex);
    return true;
}
