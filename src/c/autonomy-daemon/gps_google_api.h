#ifndef GPS_GOOGLE_API_H
#define GPS_GOOGLE_API_H

#include "types.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// Google API response
typedef struct {
    bool success;                       // Whether request was successful
    time_t timestamp;                   // Response timestamp
    long http_code;                     // HTTP response code
    int error_code;                     // CURL error code
    char data[16384];                   // Response data
    size_t data_size;                   // Size of response data
} gps_google_api_response_t;

// Google location information
typedef struct {
    time_t timestamp;                   // Information timestamp
    char formatted_address[512];        // Formatted address
    char street_number[64];             // Street number
    char route[128];                    // Street name/route
    char locality[128];                 // City/locality
    char administrative_area[128];      // State/province
    char country[64];                   // Country
    char postal_code[32];               // Postal code
    char place_id[128];                 // Google Place ID
} gps_google_location_info_t;

// Google place details
typedef struct {
    time_t timestamp;                   // Details timestamp
    char name[256];                     // Place name
    char formatted_address[512];        // Formatted address
    char place_id[128];                 // Google Place ID
    char types[512];                    // Place types
    char phone_number[64];              // Phone number
    char website[256];                  // Website URL
    double rating;                      // Place rating
    int price_level;                    // Price level
    bool open_now;                      // Whether place is open now
} gps_google_place_details_t;

// Google place search results
typedef struct {
    time_t timestamp;                   // Search timestamp
    int result_count;                   // Number of results
    char next_page_token[256];          // Next page token
    char results[20][256];              // Place IDs (up to 20)
} gps_google_place_search_t;

// Google timezone information
typedef struct {
    time_t timestamp;                   // Timezone timestamp
    char timezone_id[128];              // Timezone ID
    char timezone_name[128];            // Timezone name
    int raw_offset;                     // Raw offset in seconds
    int dst_offset;                     // DST offset in seconds
} gps_google_timezone_info_t;

// Google API configuration
typedef struct {
    bool enabled;                       // Enable/disable API
    int max_requests;                   // Maximum daily requests
    int request_timeout;                // Request timeout in seconds
    int rate_limit_delay;               // Rate limit delay in milliseconds
    char api_key[256];                  // Google API key
} gps_google_api_config_t;

// Google API status
typedef struct {
    bool enabled;                       // API enabled
    int request_count;                  // Current daily request count
    int max_requests;                   // Maximum daily requests
    time_t last_request;                // Last request timestamp
    int total_requests;                 // Total requests made
    int successful_requests;            // Successful requests
    int failed_requests;                // Failed requests
    double success_rate;                // Success rate (0-1)
} gps_google_api_status_t;

// Google API system state
typedef struct {
    bool enabled;                       // API enabled
    int max_requests;                   // Maximum requests
    int request_timeout;                // Request timeout
    int rate_limit_delay;               // Rate limit delay
    char api_key[256];                  // API key
    
    // State
    int request_count;                  // Request count
    time_t last_request;                // Last request
    int total_requests;                 // Total requests
    int successful_requests;            // Successful requests
    int failed_requests;                // Failed requests
} gps_google_api_t;

// Function prototypes

/**
 * Initialize Google Location API
 * @param api_key Google API key
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_google_api_init(const char *api_key);

/**
 * Reverse geocoding using Google API
 * @param lat Latitude
 * @param lon Longitude
 * @param location_info Location information (output)
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_google_api_reverse_geocode(double lat, double lon, 
                                  gps_google_location_info_t *location_info);

/**
 * Get place details using Google API
 * @param place_id Google Place ID
 * @param place_details Place details (output)
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_google_api_get_place_details(const char *place_id, 
                                    gps_google_place_details_t *place_details);

/**
 * Search for places near a location
 * @param lat Latitude
 * @param lon Longitude
 * @param radius Search radius in meters
 * @param type Place type (optional)
 * @param search_results Search results (output)
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_google_api_place_search(double lat, double lon, double radius, 
                               const char *type, gps_google_place_search_t *search_results);

/**
 * Get elevation data using Google API
 * @param lat Latitude
 * @param lon Longitude
 * @param elevation Elevation in meters (output)
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_google_api_get_elevation(double lat, double lon, double *elevation);

/**
 * Get timezone information using Google API
 * @param lat Latitude
 * @param lon Longitude
 * @param timestamp Timestamp for timezone calculation
 * @param timezone_info Timezone information (output)
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_google_api_get_timezone(double lat, double lon, time_t timestamp, 
                                gps_google_timezone_info_t *timezone_info);

/**
 * Get Google API status
 * @param status Status structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_google_api_get_status(gps_google_api_status_t *status);

/**
 * Get Google API configuration
 * @param config Configuration structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_google_api_get_config(gps_google_api_config_t *config);

/**
 * Set Google API configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_google_api_set_config(const gps_google_api_config_t *config);

/**
 * Enable/disable Google API
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_google_api_set_enabled(bool enabled);

/**
 * Reset Google API statistics
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_google_api_reset_stats(void);

/**
 * Cleanup Google API
 */
void gps_google_api_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_GOOGLE_API_H
