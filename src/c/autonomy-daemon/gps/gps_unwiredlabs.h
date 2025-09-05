#ifndef GPS_UNWIREDLABS_H
#define GPS_UNWIREDLABS_H

#include "types.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// UnwiredLabs API constants
#define UNWIREDLABS_API_BASE_URL "https://us1.unwiredlabs.com/v2/process.php"
#define UNWIREDLABS_API_MAX_TOKEN_LEN 256
#define UNWIREDLABS_API_MAX_URL_LEN 512
#define UNWIREDLABS_API_MAX_RESPONSE_LEN 8192

// UnwiredLabs cell tower information
typedef struct {
    char lac[16];                   // Location Area Code
    char cid[32];                   // Cell ID
    int mcc;                        // Mobile Country Code
    int mnc;                        // Mobile Network Code
    char radio[8];                  // Radio type (lte, gsm, umts, etc.)
    int signal;                     // Signal strength
    int psc;                        // Primary Scrambling Code (UMTS)
    int tac;                        // Tracking Area Code (LTE)
    int sid;                        // System ID (CDMA)
    int nid;                        // Network ID (CDMA)
    int bsid;                       // Base Station ID (CDMA)
} unwiredlabs_cell_t;

// UnwiredLabs WiFi access point information
typedef struct {
    char bssid[18];                 // MAC address
    int signal;                     // Signal strength in dBm
    int channel;                    // WiFi channel
} unwiredlabs_wifi_ap_t;

// UnwiredLabs request structure
typedef struct {
    char token[UNWIREDLABS_API_MAX_TOKEN_LEN];
    unwiredlabs_cell_t cells[16];   // Up to 16 cell towers
    int cell_count;
    unwiredlabs_wifi_ap_t wifi_aps[32]; // Up to 32 WiFi APs
    int wifi_count;
    bool fallbacks_enabled;         // Enable fallback methods
    bool address_lookup;            // Include address lookup
} unwiredlabs_request_t;

// UnwiredLabs response structure
typedef struct {
    bool success;                   // Whether API call was successful
    char status[32];                // Status string
    double lat;                     // Latitude
    double lon;                     // Longitude
    double accuracy;                // Accuracy in meters
    char address[256];              // Address (if requested)
    char country[64];               // Country
    char region[64];                // Region/State
    char city[64];                  // City
    char zip[16];                   // ZIP/Postal code
    char error_message[256];        // Error message if failed
    int error_code;                 // HTTP error code
    time_t timestamp;               // Response timestamp
} unwiredlabs_response_t;

// UnwiredLabs API configuration
typedef struct {
    char token[UNWIREDLABS_API_MAX_TOKEN_LEN];
    int timeout_seconds;
    int max_retries;
    bool enable_wifi;
    bool enable_cellular;
    bool enable_fallbacks;
    bool enable_address_lookup;
    double min_accuracy_threshold;  // Minimum accuracy threshold in meters
} unwiredlabs_config_t;

// UnwiredLabs API statistics
typedef struct {
    int total_requests;
    int successful_requests;
    int failed_requests;
    double average_accuracy;
    double average_response_time_ms;
    time_t last_request_time;
    int quota_remaining;
} unwiredlabs_stats_t;

/**
 * Initialize UnwiredLabs API
 * @param config API configuration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int unwiredlabs_api_init(const unwiredlabs_config_t* config);

/**
 * Cleanup UnwiredLabs API
 */
void unwiredlabs_api_cleanup(void);

/**
 * Get location using cell towers
 * @param cells Array of cell towers
 * @param cell_count Number of cell towers
 * @param response Response structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int unwiredlabs_api_get_cellular_location(const unwiredlabs_cell_t* cells, int cell_count, unwiredlabs_response_t* response);

/**
 * Get location using WiFi access points
 * @param wifi_aps Array of WiFi access points
 * @param wifi_count Number of WiFi access points
 * @param response Response structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int unwiredlabs_api_get_wifi_location(const unwiredlabs_wifi_ap_t* wifi_aps, int wifi_count, unwiredlabs_response_t* response);

/**
 * Get location using combined cell and WiFi data
 * @param request Combined request structure
 * @param response Response structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int unwiredlabs_api_get_combined_location(const unwiredlabs_request_t* request, unwiredlabs_response_t* response);

/**
 * Get API statistics
 * @param stats Statistics structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int unwiredlabs_api_get_stats(unwiredlabs_stats_t* stats);

/**
 * Check if UnwiredLabs API is initialized
 * @return true if initialized, false otherwise
 */
bool unwiredlabs_api_is_initialized(void);

/**
 * Validate API token
 * @return true if valid, false otherwise
 */
bool unwiredlabs_api_validate_token(void);

/**
 * Check quota status
 * @return remaining quota or -1 if unknown
 */
int unwiredlabs_api_get_quota_remaining(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_UNWIREDLABS_H
