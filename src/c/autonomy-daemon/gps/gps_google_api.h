#ifndef GPS_GOOGLE_API_H
#define GPS_GOOGLE_API_H

#include "../core/types.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Google Geolocation API constants
#define GOOGLE_API_BASE_URL "https://www.googleapis.com/geolocation/v1/geolocate"
#define GOOGLE_API_MAX_KEY_LEN 256
#define GOOGLE_API_MAX_URL_LEN 512
#define GOOGLE_API_MAX_RESPONSE_LEN 8192

// WiFi access point information
typedef struct {
    char mac_address[18];          // MAC address (e.g., "00:11:22:33:44:55")
    int signal_strength;           // Signal strength in dBm
    int signal_to_noise_ratio;     // Signal to noise ratio
    int channel;                   // WiFi channel
    int frequency;                 // Frequency in MHz
} google_wifi_ap_t;

// Cell tower information for Google API
typedef struct {
    char cell_id[32];              // Cell ID
    char location_area_code[16];   // Location Area Code
    char mobile_country_code[8];   // Mobile Country Code
    char mobile_network_code[8];   // Mobile Network Code
    int signal_strength;           // Signal strength in dBm
    int age;                       // Age of the reading in seconds
    int timing_advance;            // Timing advance
} google_cell_tower_t;

// Google API request structure
typedef struct {
    google_wifi_ap_t wifi_access_points[32];  // Up to 32 WiFi APs
    int wifi_count;
    google_cell_tower_t cell_towers[16];      // Up to 16 cell towers
    int cell_count;
    bool consider_ip;                          // Consider IP address
} google_location_request_t;

// Google API response structure
typedef struct {
    bool success;                   // Whether API call was successful
    double lat;                     // Latitude
    double lng;                     // Longitude
    double accuracy;                // Accuracy in meters
    char error_message[256];        // Error message if failed
    int error_code;                 // HTTP error code
    time_t timestamp;               // Response timestamp
} google_location_response_t;

// Google API configuration
typedef struct {
    char api_key[GOOGLE_API_MAX_KEY_LEN];
    int timeout_seconds;
    int max_retries;
    bool enable_wifi;
    bool enable_cellular;
    bool enable_ip_fallback;
    double min_accuracy_threshold;  // Minimum accuracy threshold in meters
} google_api_config_t;

// Google API statistics
typedef struct {
    int total_requests;
    int successful_requests;
    int failed_requests;
    double average_accuracy;
    double average_response_time_ms;
    time_t last_request_time;
    int quota_remaining;
} google_api_stats_t;

/**
 * Initialize Google Geolocation API
 * @param config API configuration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int google_api_init(const google_api_config_t* config);

/**
 * Cleanup Google Geolocation API
 */
void google_api_cleanup(void);

/**
 * Get location using WiFi access points
 * @param wifi_aps Array of WiFi access points
 * @param wifi_count Number of WiFi access points
 * @param response Response structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int google_api_get_wifi_location(const google_wifi_ap_t* wifi_aps, int wifi_count, google_location_response_t* response);

/**
 * Get location using cell towers
 * @param cell_towers Array of cell towers
 * @param cell_count Number of cell towers
 * @param response Response structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int google_api_get_cellular_location(const google_cell_tower_t* cell_towers, int cell_count, google_location_response_t* response);

/**
 * Get location using combined WiFi and cellular data
 * @param request Combined request structure
 * @param response Response structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int google_api_get_combined_location(const google_location_request_t* request, google_location_response_t* response);

/**
 * Get location with IP fallback
 * @param request Request structure (can be NULL for IP-only)
 * @param response Response structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int google_api_get_location_with_ip_fallback(const google_location_request_t* request, google_location_response_t* response);

/**
 * Get API statistics
 * @param stats Statistics structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int google_api_get_stats(google_api_stats_t* stats);

/**
 * Check if Google API is initialized
 * @return true if initialized, false otherwise
 */
bool google_api_is_initialized(void);

/**
 * Validate API key
 * @return true if valid, false otherwise
 */
bool google_api_validate_key(void);

/**
 * Check quota status
 * @return remaining quota or -1 if unknown
 */
int google_api_get_quota_remaining(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_GOOGLE_API_H