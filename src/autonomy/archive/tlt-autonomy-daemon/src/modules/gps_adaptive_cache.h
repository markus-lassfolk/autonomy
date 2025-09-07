#ifndef GPS_ADAPTIVE_CACHE_H
#define GPS_ADAPTIVE_CACHE_H

#include "types.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// GPS cache entry types
typedef enum {
    GPS_CACHE_ENTRY_TYPE_UNKNOWN = 0,
    GPS_CACHE_ENTRY_TYPE_LOCATION,
    GPS_CACHE_ENTRY_TYPE_ROUTE,
    GPS_CACHE_ENTRY_TYPE_GEOFENCE,
    GPS_CACHE_ENTRY_TYPE_OBSTRUCTION,
    GPS_CACHE_ENTRY_TYPE_SATELLITE,
    GPS_CACHE_ENTRY_TYPE_WEATHER,
    GPS_CACHE_ENTRY_TYPE_TERRAIN,
    GPS_CACHE_ENTRY_TYPE_TRAFFIC,
    GPS_CACHE_ENTRY_TYPE_POI,
    GPS_CACHE_ENTRY_TYPE_ADDRESS,
    GPS_CACHE_ENTRY_TYPE_TILE,
    GPS_CACHE_ENTRY_TYPE_MAX
} gps_cache_entry_type_t;

// GPS cache entry
typedef struct {
    bool active;                        // Whether entry is active
    int entry_id;                       // Unique entry identifier
    gps_cache_entry_type_t entry_type;  // Type of cache entry
    int access_count;                   // Number of times accessed
    time_t last_access;                 // Last access timestamp
    time_t creation_time;               // Creation timestamp
    size_t size_bytes;                  // Size of cached data
    double priority;                    // Entry priority (0-1)
    void *data;                         // Cached data
} gps_cache_entry_t;

// GPS adaptive cache configuration
typedef struct {
    bool enabled;                       // Enable/disable cache
    int max_entries;                    // Maximum cache entries
    int cleanup_interval;               // Cleanup interval in seconds
    double min_hit_ratio;               // Minimum cache hit ratio
    int max_age;                        // Maximum cache entry age
    double eviction_threshold;          // Cache eviction threshold
} gps_adaptive_cache_config_t;

// GPS adaptive cache status
typedef struct {
    bool enabled;                       // Cache enabled
    int entry_count;                    // Current entry count
    int max_entries;                    // Maximum entries
    int total_hits;                     // Total cache hits
    int total_misses;                   // Total cache misses
    time_t last_cleanup;                // Last cleanup timestamp
    int total_cleanups;                 // Total cleanups performed
    size_t memory_usage;                // Current memory usage
    double hit_ratio;                   // Cache hit ratio
    double memory_efficiency;           // Memory efficiency
} gps_adaptive_cache_status_t;

// GPS adaptive cache statistics
typedef struct {
    int total_entries;                  // Total entries
    int total_hits;                     // Total hits
    int total_misses;                   // Total misses
    int total_cleanups;                 // Total cleanups
    int total_access_count;             // Total access count
    double total_priority;              // Total priority
    size_t total_size;                  // Total size
    double average_access_count;        // Average access count
    double average_priority;            // Average priority
    double average_size;                // Average size
    int entry_counts[GPS_CACHE_ENTRY_TYPE_MAX]; // Entry counts by type
} gps_adaptive_cache_stats_t;

// GPS adaptive cache system state
typedef struct {
    bool enabled;                       // Cache enabled
    int max_entries;                    // Maximum entries
    int cleanup_interval;               // Cleanup interval
    double min_hit_ratio;               // Minimum hit ratio
    int max_age;                        // Maximum age
    double eviction_threshold;          // Eviction threshold
    
    // State
    int entry_count;                    // Entry count
    int total_hits;                     // Total hits
    int total_misses;                   // Total misses
    time_t last_cleanup;                // Last cleanup
    int total_cleanups;                 // Total cleanups
    size_t memory_usage;                // Memory usage
    
    // Cache entries
    gps_cache_entry_t cache_entries[2000]; // Cache entries
} gps_adaptive_cache_t;

// Function prototypes

/**
 * Initialize GPS adaptive cache
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_adaptive_cache_init(void);

/**
 * Add entry to cache
 * @param entry_type Type of cache entry
 * @param data Data to cache
 * @param data_size Size of data
 * @param priority Entry priority (0-1)
 * @return Entry ID on success, error code on failure
 */
int gps_adaptive_cache_add_entry(gps_cache_entry_type_t entry_type, const void *data, 
                                size_t data_size, double priority);

/**
 * Find entry in cache
 * @param entry_type Type of cache entry
 * @param key_data Key data for lookup
 * @param key_size Size of key data
 * @param data Cached data (output)
 * @param data_size Size of cached data (output)
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_adaptive_cache_find_entry(gps_cache_entry_type_t entry_type, const void *key_data, 
                                 size_t key_size, void **data, size_t *data_size);

/**
 * Update entry priority
 * @param entry_id Entry ID
 * @param new_priority New priority value
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_adaptive_cache_update_priority(int entry_id, double new_priority);

/**
 * Remove entry from cache
 * @param entry_id Entry ID to remove
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_adaptive_cache_remove_entry(int entry_id);

/**
 * Get cache status
 * @param status Status structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_adaptive_cache_get_status(gps_adaptive_cache_status_t *status);

/**
 * Get cache configuration
 * @param config Configuration structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_adaptive_cache_get_config(gps_adaptive_cache_config_t *config);

/**
 * Set cache configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_adaptive_cache_set_config(const gps_adaptive_cache_config_t *config);

/**
 * Enable/disable cache
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_adaptive_cache_set_enabled(bool enabled);

/**
 * Force cache cleanup
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_adaptive_cache_force_cleanup(void);

/**
 * Get cache statistics
 * @param stats Statistics structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_adaptive_cache_get_statistics(gps_adaptive_cache_stats_t *stats);

/**
 * Reset cache
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_adaptive_cache_reset(void);

/**
 * Cleanup adaptive cache
 */
void gps_adaptive_cache_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_ADAPTIVE_CACHE_H
