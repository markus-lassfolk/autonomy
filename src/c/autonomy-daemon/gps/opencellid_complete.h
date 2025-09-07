#ifndef OPENCELLID_COMPLETE_H
#define OPENCELLID_COMPLETE_H

#include "../core/types.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// OpenCellID API configuration based on best practices
#define OPENCELLID_API_BASE_URL "https://opencellid.org"
#define OPENCELLID_API_TIMEOUT_SECONDS 30
#define OPENCELLID_MAX_API_KEY_LEN 256
#define OPENCELLID_MAX_CELLS_PER_LOOKUP 10
#define OPENCELLID_MAX_NEIGHBOR_CELLS 20
#define OPENCELLID_DEFAULT_CACHE_SIZE_MB 25
#define OPENCELLID_DEFAULT_CACHE_TTL_HOURS 24
#define OPENCELLID_NEGATIVE_CACHE_TTL_HOURS 6
#define OPENCELLID_MAX_CONTRIBUTION_BATCH_SIZE 50
#define OPENCELLID_MIN_GPS_ACCURACY_FOR_CONTRIBUTION 20.0

// Radio technology types (matching OpenCellID standards)
typedef enum {
    OPENCELLID_RADIO_UNKNOWN = 0,
    OPENCELLID_RADIO_GSM = 1,
    OPENCELLID_RADIO_UMTS = 2,
    OPENCELLID_RADIO_LTE = 3,
    OPENCELLID_RADIO_NR = 4,      // 5G New Radio
    OPENCELLID_RADIO_CDMA = 5,
    OPENCELLID_RADIO_MAX
} opencellid_radio_type_t;

// Cell tower identifier (global unique identifier)
typedef struct {
    uint16_t mcc;                           // Mobile Country Code
    uint16_t mnc;                           // Mobile Network Code
    uint32_t lac;                           // Location Area Code / TAC
    uint64_t cell_id;                       // Cell ID / eNodeB ID / gNodeB ID
    opencellid_radio_type_t radio;          // Radio technology
    uint16_t pci;                           // Physical Cell ID (LTE/NR)
    uint16_t psc;                           // Primary Scrambling Code (UMTS)
    uint32_t earfcn;                        // E-UTRA Absolute Radio Frequency Channel Number
} opencellid_cell_identifier_t;

// Cell tower location information
typedef struct {
    opencellid_cell_identifier_t cell_id;   // Cell identifier
    double latitude;                        // Latitude in decimal degrees
    double longitude;                       // Longitude in decimal degrees
    double range;                           // Accuracy radius in meters
    int samples;                            // Number of measurements used
    double confidence;                      // Confidence score (0.0-1.0)
    bool changeable;                        // Whether location can be updated
    char source[32];                        // Data source ("opencellid", "cache", "triangulation")
    time_t last_updated;                    // Last update timestamp
    time_t created_at;                      // Creation timestamp
    uint32_t access_count;                  // Number of times accessed
    time_t last_access;                     // Last access timestamp
    bool is_negative;                       // Negative cache entry (not found)
} opencellid_cell_location_t;

// Cellular metrics for signal-based weighting
typedef struct {
    int rsrp;                               // Reference Signal Received Power (dBm)
    int rsrq;                               // Reference Signal Received Quality (dB)
    int sinr;                               // Signal to Interference plus Noise Ratio (dB)
    int rssi;                               // Received Signal Strength Indicator (dBm)
    uint16_t timing_advance;                // Timing Advance value
    double timing_advance_distance;         // Calculated distance from TA (meters)
    bool timing_advance_valid;              // Whether TA is valid
    uint32_t band;                          // LTE/NR band
    uint32_t bandwidth;                     // Channel bandwidth (MHz)
} opencellid_cellular_metrics_t;

// Serving cell information
typedef struct {
    opencellid_cell_identifier_t cell_id;   // Cell identifier
    opencellid_cellular_metrics_t metrics; // Signal metrics
    bool is_registered;                     // Whether UE is registered
    char operator_name[64];                 // Network operator name
    time_t measurement_time;                // When measurement was taken
} opencellid_serving_cell_t;

// Neighbor cell information
typedef struct {
    opencellid_cell_identifier_t cell_id;   // Cell identifier
    int rsrp;                               // RSRP measurement
    int rsrq;                               // RSRQ measurement
    uint16_t pci;                           // Physical Cell ID
    uint32_t earfcn;                        // Frequency
    char type[16];                          // "intra", "inter", "neighbor"
    time_t measurement_time;                // When measurement was taken
} opencellid_neighbor_cell_t;

// Cellular environment (serving + neighbors)
typedef struct {
    opencellid_serving_cell_t serving_cell; // Serving cell
    opencellid_neighbor_cell_t neighbors[OPENCELLID_MAX_NEIGHBOR_CELLS]; // Neighbor cells
    int neighbor_count;                     // Number of neighbors
    time_t scan_time;                       // When scan was performed
    char environment_hash[65];              // SHA256 hash of environment
    double gps_latitude;                    // GPS latitude when scanned
    double gps_longitude;                   // GPS longitude when scanned
    double gps_accuracy;                    // GPS accuracy when scanned
    bool gps_valid;                         // Whether GPS was valid
} opencellid_cellular_environment_t;

// Triangulation result
typedef struct {
    double latitude;                        // Calculated latitude
    double longitude;                       // Calculated longitude
    double accuracy;                        // Estimated accuracy (meters)
    double confidence;                      // Confidence score (0.0-1.0)
    char method[32];                        // "single_cell", "weighted_centroid", "triangulation"
    int cells_used;                         // Number of cells used
    opencellid_cell_location_t primary_cell; // Primary cell used
    opencellid_cell_location_t contributing_cells[10]; // Contributing cells
    int contributing_cell_count;            // Number of contributing cells
    time_t calculation_time;                // When calculation was performed
    double timing_advance_constraint;       // TA constraint if applied
    bool timing_advance_applied;            // Whether TA was applied
} opencellid_triangulation_result_t;

// Rate limiter configuration
typedef struct {
    int max_lookups_per_hour;               // Maximum API lookups per hour
    int max_lookups_per_day;                // Maximum API lookups per day
    int max_contributions_per_hour;         // Maximum contributions per hour
    int max_contributions_per_day;          // Maximum contributions per day
    double success_ratio_threshold;         // Minimum success ratio to maintain
    int burst_allowance;                    // Burst allowance for emergency
    int adaptive_window_minutes;            // Adaptive rate limiting window
    bool emergency_bypass_enabled;          // Allow emergency bypass
} opencellid_rate_limiter_config_t;

// Cache configuration
typedef struct {
    int max_size_mb;                        // Maximum cache size in MB
    int ttl_hours;                          // Time-to-live for positive entries
    int negative_ttl_hours;                 // Time-to-live for negative entries
    char persistence_path[256];             // Path to cache database
    int sync_interval_seconds;              // Sync to disk interval
    int max_entries_per_bucket;             // Max entries per hash bucket
    bool compression_enabled;               // Enable compression
    char eviction_policy[16];               // "lru", "lfu", "ttl"
    bool predictive_prefetch;               // Enable predictive prefetching
    double hit_ratio_threshold;             // Minimum hit ratio to maintain
} opencellid_cache_config_t;

// Contribution configuration
typedef struct {
    bool enabled;                           // Enable contributions
    int interval_minutes;                   // Contribution interval
    double min_gps_accuracy_meters;         // Minimum GPS accuracy for contribution
    double movement_threshold_meters;       // Movement threshold to trigger contribution
    double rsrp_change_threshold_db;        // RSRP change threshold
    bool timing_advance_enabled;            // Include timing advance in contributions
    double max_speed_kmh;                   // Maximum speed for valid contributions
    int batch_size;                         // Batch size for bulk contributions
    int retry_attempts;                     // Number of retry attempts
    int retry_delay_seconds;                // Delay between retries
    bool quality_filter_enabled;           // Enable quality filtering
} opencellid_contribution_config_t;

// OpenCellID contribution data structure
typedef struct {
    opencellid_cell_identifier_t cell_id;   // Cell identifier
    double latitude;                        // GPS latitude
    double longitude;                       // GPS longitude
    double accuracy_meters;                 // GPS accuracy in meters
    int rsrp_dbm;                          // Signal strength (RSRP)
    int timing_advance;                     // Timing advance value
    opencellid_radio_type_t radio_type;     // Radio technology
    time_t timestamp;                       // Measurement timestamp
    char source[32];                        // Data source identifier
} opencellid_contribution_t;

// Main OpenCellID configuration
typedef struct {
    bool enabled;                           // Enable OpenCellID integration
    char api_key[OPENCELLID_API_KEY_LEN];   // API key
    char base_url[256];                     // Base API URL
    int timeout_seconds;                    // HTTP timeout
    int max_cells_per_lookup;               // Maximum cells per API call
    
    opencellid_rate_limiter_config_t rate_limiter; // Rate limiting config
    opencellid_cache_config_t cache;        // Cache configuration
    opencellid_contribution_config_t contribution; // Contribution config
    
    // Triangulation settings
    double timing_advance_weight;           // Weight for timing advance constraint
    double signal_strength_weight;          // Weight for signal strength
    double neighbor_cell_weight;            // Weight for neighbor cells
    int min_cells_for_triangulation;        // Minimum cells for triangulation
    double fusion_confidence_threshold;     // Minimum confidence for fusion
    
    // Health monitoring
    bool health_monitoring_enabled;         // Enable health monitoring
    int health_check_interval_minutes;      // Health check interval
    double min_success_rate;                // Minimum success rate
    int max_consecutive_failures;           // Max consecutive failures before disable
    
    // Advanced features
    bool hysteresis_enabled;                // Enable hysteresis
    int consecutive_good_threshold;         // Consecutive good measurements
    int consecutive_bad_threshold;          // Consecutive bad measurements
    bool intelligent_caching;               // Enable intelligent caching
    bool predictive_loading;                // Enable predictive loading
    bool location_clustering;               // Enable location clustering
} opencellid_config_t;

// Statistics and monitoring
typedef struct {
    // API statistics
    uint64_t total_lookups;                 // Total API lookups
    uint64_t successful_lookups;            // Successful lookups
    uint64_t failed_lookups;                // Failed lookups
    uint64_t rate_limited_lookups;          // Rate limited lookups
    uint64_t cached_lookups;                // Cache hits
    double average_response_time_ms;        // Average response time
    
    // Contribution statistics
    uint64_t total_contributions;           // Total contributions sent
    uint64_t successful_contributions;      // Successful contributions
    uint64_t failed_contributions;          // Failed contributions
    uint64_t queued_contributions;          // Queued contributions
    
    // Cache statistics
    uint64_t cache_hits;                    // Cache hits
    uint64_t cache_misses;                  // Cache misses
    uint64_t cache_entries;                 // Current cache entries
    uint64_t cache_size_bytes;              // Current cache size
    double cache_hit_ratio;                 // Cache hit ratio
    
    // Triangulation statistics
    uint64_t triangulations_performed;      // Total triangulations
    uint64_t single_cell_positions;         // Single cell positions
    uint64_t multi_cell_positions;          // Multi-cell positions
    double average_accuracy_meters;         // Average accuracy
    double average_confidence;              // Average confidence
    
    // Health statistics
    int consecutive_failures;               // Current consecutive failures
    int consecutive_successes;              // Current consecutive successes
    time_t last_success;                    // Last successful operation
    time_t last_failure;                    // Last failure
    bool healthy;                           // Current health status
    
    // Timing
    time_t stats_start_time;                // When stats collection started
    time_t last_reset;                      // Last stats reset
    time_t last_update;                     // Last stats update
} opencellid_statistics_t;

// Main OpenCellID system structure
typedef struct {
    opencellid_config_t config;             // Configuration
    opencellid_statistics_t stats;          // Statistics
    
    // Components
    void* cache;                            // Cache implementation
    void* rate_limiter;                     // Rate limiter implementation
    void* contribution_manager;             // Contribution manager
    void* health_monitor;                   // Health monitor
    
    // State
    bool initialized;                       // Initialization status
    time_t last_environment_scan;           // Last environment scan
    opencellid_cellular_environment_t last_environment; // Last environment
    opencellid_triangulation_result_t last_position; // Last calculated position
    
    // Threading
    pthread_mutex_t mutex;                  // Main mutex
    pthread_t contribution_thread;          // Contribution thread
    pthread_t health_thread;                // Health monitoring thread
    bool threads_running;                   // Thread status
    
    // HTTP client
    void* http_client;                      // HTTP client handle
} opencellid_system_t;

// Function prototypes

/**
 * Initialize the complete OpenCellID system
 * @param config System configuration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int opencellid_system_init(const opencellid_config_t* config);

/**
 * Cleanup the OpenCellID system
 */
void opencellid_system_cleanup(void);

/**
 * Get current cellular environment (serving + neighbor cells)
 * @param environment Environment structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int opencellid_get_cellular_environment(opencellid_cellular_environment_t* environment);

/**
 * Perform triangulation based on current cellular environment
 * @param environment Cellular environment
 * @param result Triangulation result
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int opencellid_triangulate_position(const opencellid_cellular_environment_t* environment,
                                    opencellid_triangulation_result_t* result);

/**
 * Lookup cell tower locations from cache or API
 * @param cell_ids Array of cell identifiers
 * @param cell_count Number of cells
 * @param locations Array to store locations
 * @param max_locations Maximum locations to return
 * @return Number of locations found, or negative error code
 */
int opencellid_lookup_cells(const opencellid_cell_identifier_t* cell_ids, int cell_count,
                           opencellid_cell_location_t* locations, int max_locations);

/**
 * Contribute cellular measurements to OpenCellID
 * @param environment Cellular environment with GPS coordinates
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int opencellid_contribute_measurement(const opencellid_cellular_environment_t* environment);

/**
 * Get visible cell towers for map display
 * @param towers Array to store tower information
 * @param max_towers Maximum towers to return
 * @param center_lat Center latitude for search
 * @param center_lon Center longitude for search
 * @param radius_meters Search radius in meters
 * @return Number of towers found, or negative error code
 */
int opencellid_get_visible_towers(opencellid_cell_location_t* towers, int max_towers,
                                 double center_lat, double center_lon, double radius_meters);

/**
 * Get system statistics
 * @param stats Statistics structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int opencellid_get_statistics(opencellid_statistics_t* stats);

/**
 * Reset system statistics
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int opencellid_reset_statistics(void);

/**
 * Perform health check
 * @return AUTONOMY_SUCCESS if healthy, error code if unhealthy
 */
int opencellid_health_check(void);

/**
 * Check if system is initialized
 * @return true if initialized, false otherwise
 */
bool opencellid_is_initialized(void);

/**
 * Get current configuration
 * @param config Configuration structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int opencellid_get_config(opencellid_config_t* config);

/**
 * Update configuration
 * @param config New configuration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int opencellid_set_config(const opencellid_config_t* config);

// Utility functions

/**
 * Convert radio type to string
 * @param radio Radio type
 * @return Radio type string
 */
const char* opencellid_radio_type_to_string(opencellid_radio_type_t radio);

/**
 * Parse radio type from string
 * @param radio_str Radio type string
 * @return Radio type enum
 */
opencellid_radio_type_t opencellid_parse_radio_type(const char* radio_str);

/**
 * Calculate distance between two points
 * @param lat1 Latitude 1
 * @param lon1 Longitude 1
 * @param lat2 Latitude 2
 * @param lon2 Longitude 2
 * @return Distance in meters
 */
double opencellid_calculate_distance(double lat1, double lon1, double lat2, double lon2);

/**
 * Generate cell environment hash
 * @param environment Cellular environment
 * @param hash_buffer Buffer to store hash (must be at least 65 bytes)
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int opencellid_generate_environment_hash(const opencellid_cellular_environment_t* environment,
                                        char* hash_buffer);

#ifdef __cplusplus
}
#endif

#endif // OPENCELLID_COMPLETE_H