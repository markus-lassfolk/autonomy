#ifndef GPS_COMPREHENSIVE_H
#define GPS_COMPREHENSIVE_H

#include "types.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// GPS source types
typedef enum {
    GPS_SOURCE_RUTOS = 0,
    GPS_SOURCE_STARLINK,
    GPS_SOURCE_OPENCELLID,
    GPS_SOURCE_GOOGLE,
    GPS_SOURCE_EXTERNAL,
    GPS_SOURCE_MAX
} gps_source_type_t;

// GPS fix types
typedef enum {
    GPS_FIX_TYPE_NONE = 0,
    GPS_FIX_TYPE_2D,
    GPS_FIX_TYPE_3D,
    GPS_FIX_TYPE_DGPS,
    GPS_FIX_TYPE_RTK_FLOAT,
    GPS_FIX_TYPE_RTK_FIXED,
    GPS_FIX_TYPE_MAX
} gps_fix_type_t;

// GPS fix quality
typedef enum {
    GPS_FIX_QUALITY_INVALID = 0,
    GPS_FIX_QUALITY_GPS = 1,
    GPS_FIX_QUALITY_DGPS = 2,
    GPS_FIX_QUALITY_PPS = 3,
    GPS_FIX_QUALITY_RTK = 4,
    GPS_FIX_QUALITY_RTK_FLOAT = 5,
    GPS_FIX_QUALITY_ESTIMATED = 6,
    GPS_FIX_QUALITY_MANUAL = 7,
    GPS_FIX_QUALITY_SIMULATED = 8,
    GPS_FIX_QUALITY_MAX
} gps_fix_quality_t;

// Standardized GPS data structure
typedef struct {
    // Basic position
    double latitude;                        // Latitude in decimal degrees
    double longitude;                       // Longitude in decimal degrees
    double altitude;                        // Altitude in meters
    double accuracy;                        // Horizontal accuracy in meters
    double vertical_accuracy;               // Vertical accuracy in meters
    
    // Quality indicators
    double confidence;                      // Confidence score (0.0-1.0)
    gps_fix_type_t fix_type;               // Fix type
    gps_fix_quality_t fix_quality;         // Fix quality
    bool valid;                            // Whether data is valid
    
    // Satellite information
    int satellites_used;                   // Number of satellites used
    int satellites_visible;                // Number of satellites visible
    double hdop;                           // Horizontal dilution of precision
    double vdop;                           // Vertical dilution of precision
    double pdop;                           // Position dilution of precision
    
    // Movement data
    double speed;                          // Speed in m/s
    double heading;                        // Heading in degrees
    double climb;                          // Climb rate in m/s
    
    // Timing
    time_t timestamp;                      // GPS timestamp
    time_t collection_time;                // When data was collected
    double collection_duration_ms;         // Collection time in milliseconds
    
    // Source information
    char source[32];                       // Source name
    gps_source_type_t source_type;         // Source type
    int source_priority;                   // Source priority (1=highest)
    bool from_cache;                       // Whether data came from cache
    
    // Quality metrics
    double age_seconds;                    // Age of GPS data
    double staleness_penalty;              // Penalty for stale data
    double accuracy_bonus;                 // Bonus for high accuracy
    double satellite_bonus;                // Bonus for satellite count
    
    // Raw data
    char raw_nmea[512];                    // Raw NMEA sentence
    char raw_json[1024];                   // Raw JSON data
} standardized_gps_data_t;

// GPS source health status
typedef struct {
    gps_source_type_t source_type;         // Source type
    char source_name[32];                  // Source name
    bool available;                        // Whether source is available
    bool healthy;                          // Whether source is healthy
    double health_score;                   // Health score (0.0-1.0)
    
    // Performance metrics
    uint64_t total_collections;            // Total collection attempts
    uint64_t successful_collections;       // Successful collections
    uint64_t failed_collections;           // Failed collections
    double success_rate;                   // Success rate (0.0-1.0)
    double average_collection_time_ms;     // Average collection time
    double average_accuracy;               // Average accuracy
    double average_confidence;             // Average confidence
    
    // Recent performance
    int consecutive_failures;              // Consecutive failures
    int consecutive_successes;             // Consecutive successes
    time_t last_success;                   // Last successful collection
    time_t last_failure;                   // Last failure
    time_t last_collection_attempt;        // Last collection attempt
    
    // Quality metrics
    double best_accuracy;                  // Best accuracy achieved
    double worst_accuracy;                 // Worst accuracy
    time_t first_seen;                     // First time source was seen
    time_t last_seen;                      // Last time source was active
} gps_source_health_t;

// GPS collection configuration
typedef struct {
    bool enabled;                          // Enable GPS collection
    char source_priority[256];             // Source priority list
    double movement_threshold_m;           // Movement detection threshold
    double accuracy_threshold_m;           // Minimum accuracy required
    int staleness_threshold_s;             // Maximum age for GPS data
    int collection_timeout_s;              // Collection timeout
    int retry_attempts;                    // Number of retry attempts
    int retry_delay_s;                     // Delay between retries
    
    // Fusion configuration
    bool enable_hybrid_prioritization;     // Enable confidence-based fallback
    double min_acceptable_confidence;      // Minimum confidence to accept
    double fallback_confidence_threshold;  // Threshold to try next source
    bool enable_data_fusion;               // Enable multi-source fusion
    double fusion_weight_accuracy;         // Weight for accuracy in fusion
    double fusion_weight_confidence;       // Weight for confidence in fusion
    double fusion_weight_freshness;        // Weight for data freshness
    
    // Movement detection
    bool enable_movement_detection;        // Enable movement detection
    double movement_detection_interval_s;  // Movement check interval
    double stationary_threshold_s;         // Time to consider stationary
    double movement_hysteresis_m;          // Hysteresis for movement detection
    
    // Source-specific configuration
    bool google_api_enabled;               // Enable Google API
    char google_api_key[256];              // Google API key
    bool google_elevation_api_enabled;     // Enable elevation API
    bool opencellid_enabled;               // Enable OpenCellID
    char opencellid_api_key[256];          // OpenCellID API key
    bool opencellid_contribute;            // Enable OpenCellID contribution
    
    // Health monitoring
    bool enable_health_monitoring;         // Enable source health monitoring
    int health_check_interval_s;           // Health check interval
    double min_health_score;               // Minimum health score
    int max_consecutive_failures;          // Max failures before disabling source
    
    // Advanced features
    bool enable_location_clustering;       // Enable location clustering
    bool enable_adaptive_caching;          // Enable adaptive caching
    bool enable_predictive_loading;        // Enable predictive loading
    double clustering_radius_m;            // Clustering radius
    int max_cluster_size;                  // Maximum cluster size
} gps_comprehensive_config_t;

// GPS movement state
typedef struct {
    bool is_moving;                        // Currently moving
    bool was_moving;                       // Previously moving
    time_t movement_start;                 // When movement started
    time_t stationary_start;               // When became stationary
    double last_latitude;                  // Last known latitude
    double last_longitude;                 // Last known longitude
    double total_distance_m;               // Total distance traveled
    double current_speed_ms;               // Current speed in m/s
    double average_speed_ms;               // Average speed in m/s
    double max_speed_ms;                   // Maximum speed recorded
    int movement_events;                   // Number of movement events
    time_t last_movement_event;            // Last movement event
} gps_movement_state_t;

// GPS fusion result
typedef struct {
    standardized_gps_data_t fused_data;    // Fused GPS data
    standardized_gps_data_t source_data[GPS_SOURCE_MAX]; // Individual source data
    int sources_used;                      // Number of sources used
    char fusion_method[32];                // Fusion method used
    double fusion_confidence;              // Fusion confidence
    time_t fusion_time;                    // When fusion was performed
    char fusion_reasoning[256];            // Reasoning for source selection
} gps_fusion_result_t;

// Comprehensive GPS collector structure
typedef struct {
    gps_comprehensive_config_t config;     // Configuration
    gps_source_health_t source_health[GPS_SOURCE_MAX]; // Source health status
    gps_movement_state_t movement_state;   // Movement state
    standardized_gps_data_t last_known;    // Last known position
    gps_fusion_result_t last_fusion;       // Last fusion result
    
    // Statistics
    uint64_t total_collections;            // Total collections
    uint64_t successful_collections;       // Successful collections
    uint64_t failed_collections;           // Failed collections
    uint64_t fusion_operations;            // Fusion operations performed
    uint64_t movement_detections;          // Movement detections
    
    // Threading
    pthread_mutex_t mutex;                 // Main mutex
    pthread_t collection_thread;           // Collection thread
    pthread_t health_monitor_thread;       // Health monitoring thread
    bool threads_running;                  // Thread status
    
    // State
    bool initialized;                      // Initialization status
    time_t last_collection;                // Last collection time
    time_t last_health_check;              // Last health check
} gps_comprehensive_collector_t;

// Function prototypes

/**
 * Initialize comprehensive GPS collector
 * @param config GPS configuration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_comprehensive_init(const gps_comprehensive_config_t* config);

/**
 * Cleanup comprehensive GPS collector
 */
void gps_comprehensive_cleanup(void);

/**
 * Collect GPS data from best available source
 * @param result GPS data result
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_comprehensive_collect_best(standardized_gps_data_t* result);

/**
 * Collect GPS data from all sources and perform fusion
 * @param result Fusion result with all source data
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_comprehensive_collect_all_and_fuse(gps_fusion_result_t* result);

/**
 * Get GPS source health status
 * @param source_type Source type
 * @param health Health status to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_comprehensive_get_source_health(gps_source_type_t source_type, gps_source_health_t* health);

/**
 * Get all GPS sources health status
 * @param health_array Array to fill with health status
 * @param max_sources Maximum sources to return
 * @return Number of sources returned, or negative error code
 */
int gps_comprehensive_get_all_source_health(gps_source_health_t* health_array, int max_sources);

/**
 * Perform movement detection
 * @param current_data Current GPS data
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_comprehensive_detect_movement(const standardized_gps_data_t* current_data);

/**
 * Get movement state
 * @param movement_state Movement state to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_comprehensive_get_movement_state(gps_movement_state_t* movement_state);

/**
 * Validate GPS data quality
 * @param gps_data GPS data to validate
 * @return AUTONOMY_SUCCESS if valid, error code if invalid
 */
int gps_comprehensive_validate_data(const standardized_gps_data_t* gps_data);

/**
 * Calculate GPS confidence score
 * @param gps_data GPS data
 * @param source_health Source health status
 * @return Confidence score (0.0-1.0)
 */
double gps_comprehensive_calculate_confidence(const standardized_gps_data_t* gps_data,
                                            const gps_source_health_t* source_health);

/**
 * Perform GPS health check for all sources
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_comprehensive_health_check(void);

/**
 * Get best available source name
 * @return Source name, or NULL if no sources available
 */
const char* gps_comprehensive_get_best_source(void);

/**
 * Check if comprehensive GPS collector is initialized
 * @return true if initialized, false otherwise
 */
bool gps_comprehensive_is_initialized(void);

/**
 * Get comprehensive GPS statistics
 * @param total_collections Total collections performed
 * @param successful_collections Successful collections
 * @param failed_collections Failed collections
 * @param fusion_operations Fusion operations performed
 * @param movement_detections Movement detections
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_comprehensive_get_statistics(uint64_t* total_collections,
                                   uint64_t* successful_collections,
                                   uint64_t* failed_collections,
                                   uint64_t* fusion_operations,
                                   uint64_t* movement_detections);

// Utility functions

/**
 * Convert GPS source type to string
 * @param source_type Source type
 * @return Source type string
 */
const char* gps_source_type_to_string(gps_source_type_t source_type);

/**
 * Parse GPS source type from string
 * @param source_str Source type string
 * @return Source type enum
 */
gps_source_type_t gps_parse_source_type(const char* source_str);

/**
 * Convert GPS fix type to string
 * @param fix_type Fix type
 * @return Fix type string
 */
const char* gps_fix_type_to_string(gps_fix_type_t fix_type);

/**
 * Convert GPS fix quality to string
 * @param fix_quality Fix quality
 * @return Fix quality string
 */
const char* gps_fix_quality_to_string(gps_fix_quality_t fix_quality);

/**
 * Calculate distance between two GPS points
 * @param lat1 Latitude 1
 * @param lon1 Longitude 1
 * @param lat2 Latitude 2
 * @param lon2 Longitude 2
 * @return Distance in meters
 */
double gps_calculate_distance(double lat1, double lon1, double lat2, double lon2);

/**
 * Calculate bearing between two GPS points
 * @param lat1 Latitude 1
 * @param lon1 Longitude 1
 * @param lat2 Latitude 2
 * @param lon2 Longitude 2
 * @return Bearing in degrees
 */
double gps_calculate_bearing(double lat1, double lon1, double lat2, double lon2);

/**
 * Calculate speed from distance and time
 * @param distance Distance in meters
 * @param time_diff Time difference in seconds
 * @return Speed in m/s
 */
double gps_calculate_speed(double distance, double time_diff);

#ifdef __cplusplus
}
#endif

#endif // GPS_COMPREHENSIVE_H