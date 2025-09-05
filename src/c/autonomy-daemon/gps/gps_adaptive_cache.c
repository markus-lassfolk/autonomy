#include "gps_adaptive_cache.h"
#include "logx.h"
#include "types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

// GPS adaptive cache configuration
static const int MAX_CACHE_ENTRIES = 2000;                 // Maximum cache entries
static const int CACHE_CLEANUP_INTERVAL = 300;             // 5 minute cleanup interval
static const double MIN_CACHE_HIT_RATIO = 0.1;             // Minimum cache hit ratio
static const int MAX_CACHE_AGE = 86400;                    // 24 hour maximum cache age
static const double CACHE_EVICTION_THRESHOLD = 0.8;        // 80% cache usage threshold

// Cache entry types
static const char* CACHE_ENTRY_TYPE_NAMES[] = {
    "unknown", "location", "route", "geofence", "obstruction", "satellite",
    "weather", "terrain", "traffic", "poi", "address", "tile"
};

// Global GPS adaptive cache state
static gps_adaptive_cache_t g_cache = {0};
static bool g_cache_initialized = false;
static pthread_mutex_t g_cache_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize GPS adaptive cache
static int gps_adaptive_cache_init(void) {
    if (g_cache_initialized) {
        LOGX_WARN("GPS adaptive cache already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_cache_mutex);
    
    // Initialize cache state
    memset(&g_cache, 0, sizeof(gps_adaptive_cache_t));
    g_cache.enabled = true;
    g_cache.max_entries = MAX_CACHE_ENTRIES;
    g_cache.cleanup_interval = CACHE_CLEANUP_INTERVAL;
    g_cache.min_hit_ratio = MIN_CACHE_HIT_RATIO;
    g_cache.max_age = MAX_CACHE_AGE;
    g_cache.eviction_threshold = CACHE_EVICTION_THRESHOLD;
    
    g_cache.entry_count = 0;
    g_cache.total_hits = 0;
    g_cache.total_misses = 0;
    g_cache.last_cleanup = 0;
    g_cache.total_cleanups = 0;
    g_cache.memory_usage = 0;
    
    // Initialize cache entries array
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
        g_cache.cache_entries[i].active = false;
        g_cache.cache_entries[i].entry_id = 0;
        g_cache.cache_entries[i].entry_type = GPS_CACHE_ENTRY_TYPE_UNKNOWN;
        g_cache.cache_entries[i].access_count = 0;
        g_cache.cache_entries[i].last_access = 0;
        g_cache.cache_entries[i].creation_time = 0;
        g_cache.cache_entries[i].size_bytes = 0;
        g_cache.cache_entries[i].priority = 0.0;
        g_cache.cache_entries[i].data = NULL;
    }
    
    g_cache_initialized = true;
    pthread_mutex_unlock(&g_cache_mutex);
    
    LOGX_INFO("GPS adaptive cache initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Generate unique cache entry ID
static int generate_cache_entry_id(void) {
    static int next_id = 5000;
    return next_id++;
}

// Add entry to cache
int gps_adaptive_cache_add_entry(gps_cache_entry_type_t entry_type, const void *data, 
                                size_t data_size, double priority) {
    if (!g_cache_initialized || !data || data_size == 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_cache_mutex);
    
    // Check if cache is full and needs cleanup
    if (g_cache.entry_count >= g_cache.max_entries) {
        perform_cache_cleanup();
    }
    
    // Find free cache slot
    int slot_index = -1;
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
        if (!g_cache.cache_entries[i].active) {
            slot_index = i;
            break;
        }
    }
    
    if (slot_index < 0) {
        pthread_mutex_unlock(&g_cache_mutex);
        LOGX_ERROR("No free slots for cache entry");
        return AUTONOMY_ERROR_NO_RESOURCES;
    }
    
    // Allocate memory for data
    void *data_copy = malloc(data_size);
    if (!data_copy) {
        pthread_mutex_unlock(&g_cache_mutex);
        LOGX_ERROR("Failed to allocate memory for cache entry");
        return AUTONOMY_ERROR_NO_MEMORY;
    }
    
    // Initialize cache entry
    gps_cache_entry_t *entry = &g_cache.cache_entries[slot_index];
    entry->active = true;
    entry->entry_id = generate_cache_entry_id();
    entry->entry_type = entry_type;
    entry->access_count = 1;
    entry->last_access = time(NULL);
    entry->creation_time = time(NULL);
    entry->size_bytes = data_size;
    entry->priority = priority;
    entry->data = data_copy;
    
    // Copy data
    memcpy(entry->data, data, data_size);
    
    g_cache.entry_count++;
    g_cache.memory_usage += data_size;
    
    pthread_mutex_unlock(&g_cache_mutex);
    
    LOGX_DEBUG("Added cache entry %d: type=%d, size=%zu, priority=%.2f", 
               entry->entry_id, entry_type, data_size, priority);
    
    return entry->entry_id;
}

// Find entry in cache
int gps_adaptive_cache_find_entry(gps_cache_entry_type_t entry_type, const void *key_data, 
                                 size_t key_size, void **data, size_t *data_size) {
    if (!g_cache_initialized || !key_data || !data || !data_size) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_cache_mutex);
    
    // Search for matching entry
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
        if (!g_cache.cache_entries[i].active || 
            g_cache.cache_entries[i].entry_type != entry_type) {
            continue;
        }
        
        gps_cache_entry_t *entry = &g_cache.cache_entries[i];
        
        // Simple key matching (in a real implementation, this would be more sophisticated)
        if (entry->size_bytes == key_size && 
            memcmp(entry->data, key_data, key_size) == 0) {
            
            // Update access statistics
            entry->access_count++;
            entry->last_access = time(NULL);
            g_cache.total_hits++;
            
            // Return data
            *data = entry->data;
            *data_size = entry->size_bytes;
            
            pthread_mutex_unlock(&g_cache_mutex);
            
            LOGX_DEBUG("Cache hit for entry %d: type=%d, access_count=%d", 
                       entry->entry_id, entry_type, entry->access_count);
            
            return AUTONOMY_SUCCESS;
        }
    }
    
    g_cache.total_misses++;
    pthread_mutex_unlock(&g_cache_mutex);
    
    LOGX_DEBUG("Cache miss for entry type=%d", entry_type);
    return AUTONOMY_ERROR_NOT_FOUND;
}

// Update entry priority
static int gps_adaptive_cache_update_priority(int entry_id, double new_priority) {
    if (!g_cache_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_cache_mutex);
    
    // Find cache entry
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
        if (g_cache.cache_entries[i].active && 
            g_cache.cache_entries[i].entry_id == entry_id) {
            
            g_cache.cache_entries[i].priority = new_priority;
            pthread_mutex_unlock(&g_cache_mutex);
            
            LOGX_DEBUG("Updated cache entry %d priority to %.2f", entry_id, new_priority);
            return AUTONOMY_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&g_cache_mutex);
    return AUTONOMY_ERROR_NOT_FOUND;
}

// Remove entry from cache
static int gps_adaptive_cache_remove_entry(int entry_id) {
    if (!g_cache_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_cache_mutex);
    
    // Find and remove cache entry
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
        if (g_cache.cache_entries[i].active && 
            g_cache.cache_entries[i].entry_id == entry_id) {
            
            gps_cache_entry_t *entry = &g_cache.cache_entries[i];
            
            // Free data memory
            if (entry->data) {
                g_cache.memory_usage -= entry->size_bytes;
                free(entry->data);
            }
            
            // Clear entry
            entry->active = false;
            entry->entry_id = 0;
            entry->entry_type = GPS_CACHE_ENTRY_TYPE_UNKNOWN;
            entry->access_count = 0;
            entry->last_access = 0;
            entry->creation_time = 0;
            entry->size_bytes = 0;
            entry->priority = 0.0;
            entry->data = NULL;
            
            g_cache.entry_count--;
            
            pthread_mutex_unlock(&g_cache_mutex);
            
            LOGX_DEBUG("Removed cache entry %d", entry_id);
            return AUTONOMY_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&g_cache_mutex);
    return AUTONOMY_ERROR_NOT_FOUND;
}

// Perform cache cleanup
static void perform_cache_cleanup(void) {
    time_t now = time(NULL);
    
    // Check if enough time has passed since last cleanup
    if ((now - g_cache.last_cleanup) < g_cache.cleanup_interval) {
        return;
    }
    
    g_cache.last_cleanup = now;
    g_cache.total_cleanups++;
    
    LOGX_DEBUG("Starting cache cleanup - entries: %d, memory: %zu bytes", 
               g_cache.entry_count, g_cache.memory_usage);
    
    // Calculate cache hit ratio
    double hit_ratio = 0.0;
    if (g_cache.total_hits + g_cache.total_misses > 0) {
        hit_ratio = (double)g_cache.total_hits / (g_cache.total_hits + g_cache.total_misses);
    }
    
    // Determine cleanup strategy based on hit ratio and memory usage
    if (hit_ratio < g_cache.min_hit_ratio || 
        g_cache.entry_count > g_cache.max_entries * g_cache.eviction_threshold) {
        
        // Aggressive cleanup - remove low-priority and old entries
        perform_aggressive_cleanup();
    } else {
        // Gentle cleanup - remove only expired entries
        perform_gentle_cleanup();
    }
    
    LOGX_DEBUG("Cache cleanup completed - entries: %d, memory: %zu bytes", 
               g_cache.entry_count, g_cache.memory_usage);
}

// Perform aggressive cache cleanup
static void perform_aggressive_cleanup(void) {
    // Sort entries by priority and age for eviction
    int eviction_candidates[MAX_CACHE_ENTRIES];
    int candidate_count = 0;
    
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
        if (g_cache.cache_entries[i].active) {
            eviction_candidates[candidate_count++] = i;
        }
    }
    
    // Sort by eviction score (lower priority + older age = higher eviction score)
    for (int i = 0; i < candidate_count - 1; i++) {
        for (int j = i + 1; j < candidate_count; j++) {
            int idx1 = eviction_candidates[i];
            int idx2 = eviction_candidates[j];
            
            double score1 = calculate_eviction_score(&g_cache.cache_entries[idx1]);
            double score2 = calculate_eviction_score(&g_cache.cache_entries[idx2]);
            
            if (score1 < score2) {
                int temp = eviction_candidates[i];
                eviction_candidates[i] = eviction_candidates[j];
                eviction_candidates[j] = temp;
            }
        }
    }
    
    // Evict entries until we're below threshold
    int target_entries = g_cache.max_entries * g_cache.eviction_threshold;
    int entries_to_evict = g_cache.entry_count - target_entries;
    
    for (int i = 0; i < entries_to_evict && i < candidate_count; i++) {
        int entry_index = eviction_candidates[i];
        gps_cache_entry_t *entry = &g_cache.cache_entries[entry_index];
        
        // Free data memory
        if (entry->data) {
            g_cache.memory_usage -= entry->size_bytes;
            free(entry->data);
        }
        
        // Clear entry
        entry->active = false;
        entry->entry_id = 0;
        entry->entry_type = GPS_CACHE_ENTRY_TYPE_UNKNOWN;
        entry->access_count = 0;
        entry->last_access = 0;
        entry->creation_time = 0;
        entry->size_bytes = 0;
        entry->priority = 0.0;
        entry->data = NULL;
        
        g_cache.entry_count--;
    }
    
    LOGX_INFO("Aggressive cache cleanup: evicted %d entries", entries_to_evict);
}

// Perform gentle cache cleanup
static void perform_gentle_cleanup(void) {
    time_t now = time(NULL);
    int expired_count = 0;
    
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
        if (!g_cache.cache_entries[i].active) {
            continue;
        }
        
        gps_cache_entry_t *entry = &g_cache.cache_entries[i];
        
        // Check if entry has expired
        if ((now - entry->creation_time) > g_cache.max_age) {
            // Free data memory
            if (entry->data) {
                g_cache.memory_usage -= entry->size_bytes;
                free(entry->data);
            }
            
            // Clear entry
            entry->active = false;
            entry->entry_id = 0;
            entry->entry_type = GPS_CACHE_ENTRY_TYPE_UNKNOWN;
            entry->access_count = 0;
            entry->last_access = 0;
            entry->creation_time = 0;
            entry->size_bytes = 0;
            entry->priority = 0.0;
            entry->data = NULL;
            
            g_cache.entry_count--;
            expired_count++;
        }
    }
    
    if (expired_count > 0) {
        LOGX_INFO("Gentle cache cleanup: removed %d expired entries", expired_count);
    }
}

// Calculate eviction score for an entry
static double calculate_eviction_score(const gps_cache_entry_t *entry) {
    time_t now = time(NULL);
    
    // Priority factor (higher priority = lower eviction score)
    double priority_factor = 1.0 - entry->priority;
    
    // Age factor (older = higher eviction score)
    double age_factor = (double)(now - entry->creation_time) / g_cache.max_age;
    
    // Access factor (more access = lower eviction score)
    double access_factor = 1.0 - fmin(entry->access_count / 100.0, 1.0);
    
    // Weighted combination
    double eviction_score = (priority_factor * 0.4) + (age_factor * 0.4) + (access_factor * 0.2);
    
    return eviction_score;
}

// Get cache status
static int gps_adaptive_cache_get_status(gps_adaptive_cache_status_t *status) {
    if (!g_cache_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_cache_mutex);
    
    status->enabled = g_cache.enabled;
    status->entry_count = g_cache.entry_count;
    status->max_entries = g_cache.max_entries;
    status->total_hits = g_cache.total_hits;
    status->total_misses = g_cache.total_misses;
    status->last_cleanup = g_cache.last_cleanup;
    status->total_cleanups = g_cache.total_cleanups;
    status->memory_usage = g_cache.memory_usage;
    
    // Calculate hit ratio
    if (g_cache.total_hits + g_cache.total_misses > 0) {
        status->hit_ratio = (double)g_cache.total_hits / (g_cache.total_hits + g_cache.total_misses);
    } else {
        status->hit_ratio = 0.0;
    }
    
    // Calculate memory efficiency
    status->memory_efficiency = (double)g_cache.entry_count / g_cache.max_entries;
    
    pthread_mutex_unlock(&g_cache_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get cache configuration
static int gps_adaptive_cache_get_config(gps_adaptive_cache_config_t *config) {
    if (!g_cache_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_cache_mutex);
    
    config->enabled = g_cache.enabled;
    config->max_entries = g_cache.max_entries;
    config->cleanup_interval = g_cache.cleanup_interval;
    config->min_hit_ratio = g_cache.min_hit_ratio;
    config->max_age = g_cache.max_age;
    config->eviction_threshold = g_cache.eviction_threshold;
    
    pthread_mutex_unlock(&g_cache_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set cache configuration
static int gps_adaptive_cache_set_config(const gps_adaptive_cache_config_t *config) {
    if (!g_cache_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_cache_mutex);
    
    g_cache.enabled = config->enabled;
    g_cache.max_entries = config->max_entries;
    g_cache.cleanup_interval = config->cleanup_interval;
    g_cache.min_hit_ratio = config->min_hit_ratio;
    g_cache.max_age = config->max_age;
    g_cache.eviction_threshold = config->eviction_threshold;
    
    pthread_mutex_unlock(&g_cache_mutex);
    
    LOGX_INFO("GPS adaptive cache configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable cache
static int gps_adaptive_cache_set_enabled(bool enabled) {
    if (!g_cache_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_cache_mutex);
    g_cache.enabled = enabled;
    pthread_mutex_unlock(&g_cache_mutex);
    
    LOGX_INFO("GPS adaptive cache %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Force cache cleanup
static int gps_adaptive_cache_force_cleanup(void) {
    if (!g_cache_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_cache_mutex);
    
    // Reset last cleanup time to force immediate cleanup
    g_cache.last_cleanup = 0;
    
    // Perform cleanup
    perform_cache_cleanup();
    
    pthread_mutex_unlock(&g_cache_mutex);
    
    LOGX_INFO("GPS adaptive cache cleanup forced");
    return AUTONOMY_SUCCESS;
}

// Get cache statistics
static int gps_adaptive_cache_get_statistics(gps_adaptive_cache_stats_t *stats) {
    if (!g_cache_initialized || !stats) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_cache_mutex);
    
    // Calculate statistics from cache entries
    memset(stats, 0, sizeof(gps_adaptive_cache_stats_t));
    
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
        if (!g_cache.cache_entries[i].active) {
            continue;
        }
        
        gps_cache_entry_t *entry = &g_cache.cache_entries[i];
        
        // Count entries by type
        if (entry->entry_type < GPS_CACHE_ENTRY_TYPE_MAX) {
            stats->entry_counts[entry->entry_type]++;
        }
        
        // Calculate averages
        stats->total_access_count += entry->access_count;
        stats->total_priority += entry->priority;
        stats->total_size += entry->size_bytes;
    }
    
    if (g_cache.entry_count > 0) {
        stats->average_access_count = (double)stats->total_access_count / g_cache.entry_count;
        stats->average_priority = stats->total_priority / g_cache.entry_count;
        stats->average_size = (double)stats->total_size / g_cache.entry_count;
    }
    
    stats->total_entries = g_cache.entry_count;
    stats->total_hits = g_cache.total_hits;
    stats->total_misses = g_cache.total_misses;
    stats->total_cleanups = g_cache.total_cleanups;
    
    pthread_mutex_unlock(&g_cache_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Reset cache
static int gps_adaptive_cache_reset(void) {
    if (!g_cache_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_cache_mutex);
    
    // Clear all cache entries
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
        if (g_cache.cache_entries[i].active && g_cache.cache_entries[i].data) {
            free(g_cache.cache_entries[i].data);
        }
        
        g_cache.cache_entries[i].active = false;
        g_cache.cache_entries[i].entry_id = 0;
        g_cache.cache_entries[i].entry_type = GPS_CACHE_ENTRY_TYPE_UNKNOWN;
        g_cache.cache_entries[i].access_count = 0;
        g_cache.cache_entries[i].last_access = 0;
        g_cache.cache_entries[i].creation_time = 0;
        g_cache.cache_entries[i].size_bytes = 0;
        g_cache.cache_entries[i].priority = 0.0;
        g_cache.cache_entries[i].data = NULL;
    }
    
    g_cache.entry_count = 0;
    g_cache.total_hits = 0;
    g_cache.total_misses = 0;
    g_cache.last_cleanup = 0;
    g_cache.total_cleanups = 0;
    g_cache.memory_usage = 0;
    
    pthread_mutex_unlock(&g_cache_mutex);
    
    LOGX_INFO("GPS adaptive cache reset");
    return AUTONOMY_SUCCESS;
}

// Cleanup adaptive cache
static void gps_adaptive_cache_cleanup(void) {
    if (!g_cache_initialized) {
        return;
    }
    
    pthread_mutex_lock(&g_cache_mutex);
    
    // Free all cached data
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
        if (g_cache.cache_entries[i].active && g_cache.cache_entries[i].data) {
            free(g_cache.cache_entries[i].data);
        }
    }
    
    pthread_mutex_unlock(&g_cache_mutex);
    
    pthread_mutex_destroy(&g_cache_mutex);
    g_cache_initialized = false;
    
    LOGX_INFO("GPS adaptive cache cleaned up");
}
