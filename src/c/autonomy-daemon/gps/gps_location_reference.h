#ifndef GPS_LOCATION_REFERENCE_H
#define GPS_LOCATION_REFERENCE_H

#include "../core/types.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <sqlite3.h>
#include <math.h>
#include <fcntl.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

// GPS location reference entry (optimized storage)
typedef struct {
    uint32_t location_id;                  // Unique location ID
    
    // Reduced precision coordinates (saves ~50% space)
    double latitude_reduced;               // Latitude with reduced precision (~10m accuracy)
    double longitude_reduced;              // Longitude with reduced precision (~10m accuracy)
    
    // Original precision for reference
    double latitude_original;              // Original full precision latitude
    double longitude_original;             // Original full precision longitude
    
    // Location metadata
    double accuracy_meters;                // GPS accuracy when first recorded
    char gps_source[16];                   // GPS source (rutos, starlink, opencellid)
    
    // Usage tracking
    time_t first_recorded;                 // When location was first recorded
    time_t last_used;                      // When location was last referenced
    uint32_t usage_count;                  // Number of times referenced
    uint32_t telemetry_samples;            // Number of telemetry samples using this location
    
    // Geographic context
    char location_name[64];                // Optional location name/description
    bool is_stationary_location;           // Whether this is a stationary location
    double avg_movement_speed_kmh;         // Average movement speed at this location
    
    // Quality metrics
    double avg_signal_quality;             // Average signal quality at this location
    double avg_latency_ms;                 // Average latency at this location
    double location_score;                 // Overall location performance score
} gps_location_reference_t;

// Location reference configuration
typedef struct {
    bool enabled;                          // Enable location referencing
    double precision_reduction_meters;     // Precision reduction (default: 10 meters)
    double movement_threshold_meters;      // Minimum movement to create new location (default: 50 meters)
    int max_locations;                     // Maximum locations to store (default: 10000)
    int cleanup_interval_hours;            // Cleanup interval (default: 24 hours)
    int min_usage_for_retention;           // Minimum usage to retain location (default: 5)
    int retention_days;                    // Location retention period (default: 30 days)
    
    // Performance optimization
    bool enable_location_clustering;       // Enable clustering nearby locations
    double clustering_radius_meters;       // Clustering radius (default: 20 meters)
    int max_cluster_size;                  // Maximum locations per cluster
    
    // Quality tracking
    bool track_location_performance;       // Track performance metrics per location
    bool enable_location_scoring;          // Enable location quality scoring
} gps_location_reference_config_t;

// Location reference statistics
typedef struct {
    uint32_t total_locations;              // Total unique locations
    uint32_t active_locations;             // Currently active locations
    uint32_t clustered_locations;          // Locations that were clustered
    uint64_t total_references;             // Total location references made
    uint64_t space_saved_bytes;            // Estimated space saved vs full coordinates
    
    double avg_precision_reduction_meters; // Average precision reduction
    double avg_usage_per_location;         // Average usage per location
    time_t oldest_location;                // Oldest location timestamp
    time_t newest_location;                // Newest location timestamp
    
    // Performance metrics
    double avg_lookup_time_ms;             // Average location lookup time
    uint64_t cache_hits;                   // Location cache hits
    uint64_t cache_misses;                 // Location cache misses
    
    time_t stats_start_time;               // When statistics started
    time_t last_cleanup;                   // Last cleanup time
} gps_location_reference_stats_t;

// Main location reference manager
typedef struct {
    gps_location_reference_config_t config; // Configuration
    gps_location_reference_stats_t stats;   // Statistics
    
    // Location cache (for performance)
    gps_location_reference_t* location_cache; // In-memory cache
    int cache_size;                        // Cache size
    int cache_count;                       // Number of cached locations
    
    // Last known location (for movement detection)
    gps_location_reference_t last_location; // Last location
    bool last_location_valid;              // Whether last location is valid
    
    // Threading
    pthread_mutex_t mutex;                 // Main mutex
    pthread_t cleanup_thread;              // Cleanup thread
    bool thread_running;                   // Thread status
    
    // Database
    sqlite3* db;                           // Database connection
    bool db_initialized;                   // Database initialization status
    
    // State
    bool initialized;                      // Initialization status
    uint32_t next_location_id;             // Next location ID
} gps_location_reference_manager_t;

// Function prototypes

/**
 * Initialize GPS location reference system
 * @param config Location reference configuration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_location_reference_init(const gps_location_reference_config_t* config);

/**
 * Cleanup GPS location reference system
 */
void gps_location_reference_cleanup(void);

/**
 * Get or create location reference for GPS coordinates
 * @param latitude Original latitude
 * @param longitude Original longitude
 * @param accuracy GPS accuracy
 * @param gps_source GPS source name
 * @param location_id Output location ID
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_location_reference_get_or_create(double latitude, double longitude, 
                                        double accuracy, const char* gps_source,
                                        uint32_t* location_id);

/**
 * Get location reference by ID
 * @param location_id Location ID
 * @param location Location reference structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_location_reference_get_by_id(uint32_t location_id, gps_location_reference_t* location);

/**
 * Update location usage statistics
 * @param location_id Location ID
 * @param signal_quality Signal quality at location
 * @param latency_ms Latency at location
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_location_reference_update_usage(uint32_t location_id, double signal_quality, double latency_ms);

/**
 * Get locations within radius
 * @param center_latitude Center latitude
 * @param center_longitude Center longitude
 * @param radius_meters Search radius in meters
 * @param locations Array to store locations
 * @param max_locations Maximum locations to return
 * @return Number of locations found, or negative error code
 */
int gps_location_reference_get_nearby(double center_latitude, double center_longitude,
                                     double radius_meters,
                                     gps_location_reference_t* locations,
                                     int max_locations);

/**
 * Get location reference statistics
 * @param stats Statistics structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_location_reference_get_statistics(gps_location_reference_stats_t* stats);

/**
 * Force cleanup of unused locations
 * @return Number of locations cleaned up, or negative error code
 */
int gps_location_reference_force_cleanup(void);

/**
 * Check if location reference system is initialized
 * @return true if initialized, false otherwise
 */
bool gps_location_reference_is_initialized(void);

// Utility functions

/**
 * Reduce GPS coordinate precision for storage optimization
 * @param coordinate Original coordinate (latitude or longitude)
 * @param precision_meters Precision in meters (e.g., 10 for ~10m precision)
 * @return Reduced precision coordinate
 */
double gps_reduce_coordinate_precision(double coordinate, double precision_meters);

/**
 * Calculate distance between two GPS coordinates
 * @param lat1 First latitude
 * @param lon1 First longitude
 * @param lat2 Second latitude
 * @param lon2 Second longitude
 * @return Distance in meters
 */
double gps_calculate_distance_meters(double lat1, double lon1, double lat2, double lon2);

/**
 * Check if movement threshold is exceeded
 * @param current_lat Current latitude
 * @param current_lon Current longitude
 * @param reference_lat Reference latitude
 * @param reference_lon Reference longitude
 * @param threshold_meters Movement threshold in meters
 * @return true if threshold exceeded, false otherwise
 */
bool gps_movement_threshold_exceeded(double current_lat, double current_lon,
                                    double reference_lat, double reference_lon,
                                    double threshold_meters);

/**
 * Estimate storage space saved by using location references
 * @param total_samples Total number of telemetry samples
 * @param unique_locations Number of unique locations
 * @return Estimated space saved in bytes
 */
uint64_t gps_location_reference_estimate_space_saved(uint64_t total_samples, uint32_t unique_locations);

#ifdef __cplusplus
}
#endif

#endif // GPS_LOCATION_REFERENCE_H