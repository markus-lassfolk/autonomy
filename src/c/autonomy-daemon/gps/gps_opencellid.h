#ifndef GPS_OPENCELLID_H
#define GPS_OPENCELLID_H

#include "../core/types.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <math.h>
#include <fcntl.h>

#ifdef __cplusplus
extern "C" {
#endif

// OpenCellID API constants
#define OPENCELLID_BASE_URL "https://opencellid.org/api"
#define OPENCELLID_MAX_API_KEY_LEN 256
#define OPENCELLID_MAX_URL_LEN 512
#define OPENCELLID_MAX_RESPONSE_LEN 8192

// Note: opencellid_radio_type_t and opencellid_radio_t are defined in ../core/types.h

// Cell key for unique identification
typedef struct {
    char mcc[8];                        // Mobile Country Code
    char mnc[8];                        // Mobile Network Code
    char lac[16];                       // Location Area Code
    char cell_id[32];                   // Cell ID
    opencellid_radio_t radio;           // Radio technology
} opencellid_cell_key_t;

// OpenCellID API response
typedef struct {
    bool success;                       // Whether API call was successful
    double lat;                         // Latitude
    double lon;                         // Longitude
    int mcc;                           // Mobile Country Code
    int mnc;                           // Mobile Network Code
    int lac;                           // Location Area Code
    int cell_id;                       // Cell ID
    int range;                         // Estimated range in meters
    int samples;                       // Number of samples used
    char radio[16];                    // Radio technology
    char address[256];                 // Address (if available)
    char error[256];                   // Error message (if failed)
    char error_message[256];           // Detailed error message
    int error_code;                    // Error code
    time_t timestamp;                  // Response timestamp
} opencellid_response_t;

// OpenCellID contribution data
typedef struct {
    int mcc;                           // Mobile Country Code
    int mnc;                           // Mobile Network Code
    int lac;                           // Location Area Code
    int cell_id;                       // Cell ID
    double lat;                        // Latitude
    double lon;                        // Longitude
    int signal;                        // Signal strength in dBm
    int64_t measured_at;               // Unix timestamp in milliseconds
    double rating;                     // GPS accuracy in meters
    double speed;                      // Speed in m/s
    double direction;                  // Heading in degrees
    char act[16];                      // Radio technology
    int ta;                            // Timing Advance (LTE)
    int pci;                           // Physical Cell ID (LTE/NR)
    int psc;                           // Primary Scrambling Code (UMTS)
    int tac;                           // Tracking Area Code (LTE/NR)
    int sid;                           // System ID (CDMA)
    int nid;                           // Network ID (CDMA)
    int bid;                           // Base ID (CDMA)
} opencellid_contribution_t;

// OpenCellID configuration
typedef struct {
    bool enabled;                       // Enable OpenCellID integration
    char api_key[OPENCELLID_MAX_API_KEY_LEN]; // API key
    char base_url[OPENCELLID_MAX_URL_LEN]; // Base URL
    int timeout_seconds;                // Request timeout
    bool contribute_data;               // Enable data contribution
    int max_retries;                    // Maximum retry attempts
    int rate_limit_delay_ms;            // Rate limit delay in milliseconds
    int max_cache_entries;              // Maximum cache entries
    int cache_ttl_seconds;              // Cache TTL
} opencellid_config_t;

// OpenCellID statistics
typedef struct {
    int total_requests;                 // Total API requests
    int successful_requests;            // Successful requests
    int failed_requests;                // Failed requests
    int cache_hits;                     // Cache hits
    int cache_misses;                   // Cache misses
    int contributions_sent;             // Data contributions sent
    time_t last_request;                // Last API request
    time_t last_request_time;           // Last request timestamp
    time_t last_contribution;           // Last data contribution
    double success_rate;                // Success rate (0-1)
    double average_accuracy;            // Average accuracy
    int contribution_requests;          // Contribution requests
    int contribution_successes;         // Contribution successes
    int contribution_failures;          // Contribution failures
} opencellid_stats_t;

// OpenCellID cache entry
typedef struct {
    bool active;                        // Whether entry is active
    opencellid_cell_key_t cell_key;     // Cell key
    double lat;                         // Cached latitude
    double lon;                         // Cached longitude
    int range;                          // Estimated range
    time_t timestamp;                   // Cache timestamp
    time_t ttl;                         // Time to live
} opencellid_cache_entry_t;

// OpenCellID status
typedef struct {
    bool enabled;                       // OpenCellID enabled
    bool api_key_configured;            // API key configured
    char base_url[OPENCELLID_MAX_URL_LEN]; // Base URL
    int timeout_seconds;                // Timeout
    bool contribute_data;               // Data contribution enabled
    int cache_entries;                  // Current cache entries
    int max_cache_entries;              // Maximum cache entries
    opencellid_stats_t stats;           // Statistics
} opencellid_status_t;

// OpenCellID structure
typedef struct {
    opencellid_config_t config;         // Configuration
    opencellid_stats_t stats;           // Statistics
    
    // Cache
    opencellid_cache_entry_t* cache;    // Cache entries
    int max_cache_entries;              // Maximum cache entries
    int cache_count;                    // Current cache entries
    
    // Thread safety
    pthread_mutex_t mutex;
} opencellid_t;

// Function prototypes

/**
 * Initialize OpenCellID integration
 * @param config OpenCellID configuration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_opencellid_init(const opencellid_config_t* config);

/**
 * Cleanup OpenCellID integration
 */
void gps_opencellid_cleanup(void);

/**
 * Lookup cell tower location
 * @param cell_key Cell tower key
 * @param response Response structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_opencellid_lookup(const opencellid_cell_key_t* cell_key, opencellid_response_t* response);

/**
 * Contribute cell tower data
 * @param contribution Contribution data
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_opencellid_contribute(const opencellid_contribution_t* contribution);

/**
 * Get OpenCellID status
 * @param status Status structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_opencellid_get_status(opencellid_status_t* status);

/**
 * Get OpenCellID configuration
 * @param config Configuration structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_opencellid_get_config(opencellid_config_t* config);

/**
 * Set OpenCellID configuration
 * @param config New configuration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_opencellid_set_config(const opencellid_config_t* config);

/**
 * Enable/disable OpenCellID integration
 * @param enabled Whether to enable OpenCellID
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_opencellid_set_enabled(bool enabled);

/**
 * Clear OpenCellID cache
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_opencellid_clear_cache(void);

/**
 * Get OpenCellID statistics
 * @param stats Statistics structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_opencellid_get_stats(opencellid_stats_t* stats);

/**
 * Reset OpenCellID statistics
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_opencellid_reset_stats(void);

/**
 * Check if OpenCellID is initialized
 * @return true if initialized, false otherwise
 */
bool gps_opencellid_is_initialized(void);

/**
 * Convert cell info to OpenCellID cell key
 * @param mcc Mobile Country Code
 * @param mnc Mobile Network Code
 * @param lac Location Area Code
 * @param cell_id Cell ID
 * @param radio Radio technology
 * @param cell_key Cell key structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_opencellid_create_cell_key(int mcc, int mnc, int lac, int cell_id, 
                                   opencellid_radio_t radio, opencellid_cell_key_t* cell_key);

/**
 * Convert radio technology string to enum
 * @param radio_str Radio technology string
 * @return Radio technology enum
 */
opencellid_radio_t gps_opencellid_parse_radio_type(const char* radio_str);

/**
 * Convert radio technology enum to string
 * @param radio Radio technology enum
 * @return Radio technology string
 */
const char* gps_opencellid_radio_to_string(opencellid_radio_t radio);

#ifdef __cplusplus
}
#endif

#endif // GPS_OPENCELLID_H