#ifndef EXTERNAL_APIS_H
#define EXTERNAL_APIS_H

#include "../core/types.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

// External API types
typedef enum {
    EXTERNAL_API_GOOGLE_LOCATION = 0,
    EXTERNAL_API_GOOGLE_ELEVATION,
    EXTERNAL_API_GOOGLE_GEOCODING,
    EXTERNAL_API_GOOGLE_PLACES,
    EXTERNAL_API_GOOGLE_TIMEZONE,
    EXTERNAL_API_OPEN_ELEVATION,
    EXTERNAL_API_OPENSTREETMAP_NOMINATIM,
    EXTERNAL_API_WEATHER_OPENWEATHER,
    EXTERNAL_API_WEATHER_WEATHERAPI,
    EXTERNAL_API_IPINFO_GEOLOCATION,
    EXTERNAL_API_MOZILLA_LOCATION,
    EXTERNAL_API_MAX
} external_api_type_t;

// API status
typedef enum {
    API_STATUS_UNKNOWN = 0,
    API_STATUS_HEALTHY,
    API_STATUS_DEGRADED,
    API_STATUS_FAILED,
    API_STATUS_RATE_LIMITED,
    API_STATUS_QUOTA_EXCEEDED,
    API_STATUS_DISABLED,
    API_STATUS_MAX
} api_status_t;

// External API configuration
typedef struct {
    external_api_type_t api_type;          // API type
    bool enabled;                          // Whether API is enabled
    char name[64];                         // API name
    char base_url[256];                    // Base URL
    char api_key[256];                     // API key
    int timeout_seconds;                   // Request timeout
    int max_requests_per_hour;             // Rate limit per hour
    int max_requests_per_day;              // Rate limit per day
    int retry_attempts;                    // Number of retry attempts
    int retry_delay_seconds;               // Delay between retries
    bool use_ssl;                          // Use SSL/TLS
    char user_agent[128];                  // User agent string
    
    // Quota management
    int quota_limit_daily;                 // Daily quota limit
    int quota_limit_monthly;               // Monthly quota limit
    double cost_per_request;               // Cost per request
    
    // Health monitoring
    bool enable_health_monitoring;         // Enable health monitoring
    int health_check_interval_minutes;     // Health check interval
    double min_success_rate;               // Minimum success rate
    int max_consecutive_failures;          // Max failures before disabling
} external_api_config_t;

// API request structure
typedef struct {
    external_api_type_t api_type;          // API type
    char method[16];                       // HTTP method (GET, POST, etc.)
    char endpoint[512];                    // API endpoint
    char query_params[1024];               // Query parameters
    char headers[2048];                    // Custom headers
    char body[4096];                       // Request body
    int timeout_seconds;                   // Request timeout
    time_t request_time;                   // Request timestamp
} external_api_request_t;

// API response structure
typedef struct {
    int status_code;                       // HTTP status code
    char headers[2048];                    // Response headers
    char body[16384];                      // Response body
    size_t body_size;                      // Response body size
    time_t response_time;                  // Response timestamp
    double duration_ms;                    // Request duration
    bool success;                          // Whether request was successful
    char error_message[512];               // Error message if failed
} external_api_response_t;

// API statistics
typedef struct {
    uint64_t total_requests;               // Total requests made
    uint64_t successful_requests;          // Successful requests
    uint64_t failed_requests;              // Failed requests
    uint64_t rate_limited_requests;        // Rate limited requests
    uint64_t quota_exceeded_requests;      // Quota exceeded requests
    
    double average_response_time_ms;       // Average response time
    double success_rate;                   // Success rate (0.0-1.0)
    
    // Usage tracking
    int requests_this_hour;                // Requests in current hour
    int requests_this_day;                 // Requests in current day
    int requests_this_month;               // Requests in current month
    time_t hour_reset_time;                // When hour counter resets
    time_t day_reset_time;                 // When day counter resets
    time_t month_reset_time;               // When month counter resets
    
    // Health metrics
    int consecutive_failures;              // Current consecutive failures
    int consecutive_successes;             // Current consecutive successes
    time_t last_success;                   // Last successful request
    time_t last_failure;                   // Last failed request
    api_status_t status;                   // Current API status
    
    // Cost tracking
    double total_cost;                     // Total cost incurred
    double cost_this_month;                // Cost this month
    
    // Timing
    time_t first_request;                  // First request time
    time_t last_request;                   // Last request time
    time_t stats_reset_time;               // When stats were reset
} external_api_statistics_t;

// Location data from external APIs
typedef struct {
    double latitude;                       // Latitude
    double longitude;                      // Longitude
    double accuracy;                       // Accuracy in meters
    double confidence;                     // Confidence score (0.0-1.0)
    char source[64];                       // Data source
    char formatted_address[512];           // Formatted address
    char country[64];                      // Country
    char region[64];                       // Region/state
    char city[64];                         // City
    char postal_code[16];                  // Postal code
    time_t timestamp;                      // Data timestamp
} external_location_data_t;

// Elevation data from external APIs
typedef struct {
    double elevation;                      // Elevation in meters
    double resolution;                     // Data resolution
    char source[64];                       // Data source
    time_t timestamp;                      // Data timestamp
} external_elevation_data_t;

// Weather data from external APIs
typedef struct {
    double temperature_celsius;            // Temperature in Celsius
    double humidity_percent;               // Humidity percentage
    double pressure_hpa;                   // Atmospheric pressure in hPa
    double wind_speed_ms;                  // Wind speed in m/s
    double wind_direction_deg;             // Wind direction in degrees
    double visibility_km;                  // Visibility in kilometers
    char description[128];                 // Weather description
    char icon[32];                         // Weather icon code
    char source[64];                       // Data source
    time_t timestamp;                      // Data timestamp
} external_weather_data_t;

// Main external APIs manager structure
typedef struct {
    external_api_config_t configs[EXTERNAL_API_MAX]; // API configurations
    external_api_statistics_t stats[EXTERNAL_API_MAX]; // API statistics
    
    // Rate limiting
    time_t last_request_times[EXTERNAL_API_MAX]; // Last request times
    int request_counts[EXTERNAL_API_MAX];  // Request counts
    
    // Health monitoring
    bool health_monitoring_enabled;        // Health monitoring enabled
    time_t last_health_check;              // Last health check
    
    // Threading
    pthread_mutex_t mutex;                 // Main mutex
    pthread_t health_monitor_thread;       // Health monitoring thread
    bool threads_running;                  // Thread status
    
    // State
    bool initialized;                      // Initialization status
} external_apis_manager_t;

// Function prototypes

/**
 * Initialize external APIs manager
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int external_apis_init(void);

/**
 * Cleanup external APIs manager
 */
void external_apis_cleanup(void);

/**
 * Configure external API
 * @param api_type API type
 * @param config API configuration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int external_apis_configure(external_api_type_t api_type, const external_api_config_t* config);

/**
 * Make API request
 * @param request API request structure
 * @param response API response structure
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int external_apis_make_request(const external_api_request_t* request, external_api_response_t* response);

/**
 * Get location data from Google Location API
 * @param cell_towers Cellular tower data (optional)
 * @param wifi_aps WiFi access point data (optional)
 * @param location_data Location data result
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int external_apis_get_google_location(const void* cell_towers, const void* wifi_aps, 
                                     external_location_data_t* location_data);

/**
 * Get elevation data from Google or Open Elevation API
 * @param latitude Latitude
 * @param longitude Longitude
 * @param elevation_data Elevation data result
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int external_apis_get_elevation(double latitude, double longitude, external_elevation_data_t* elevation_data);

/**
 * Get reverse geocoding from Google Geocoding API
 * @param latitude Latitude
 * @param longitude Longitude
 * @param location_data Location data result
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int external_apis_get_reverse_geocoding(double latitude, double longitude, external_location_data_t* location_data);

/**
 * Get weather data from weather APIs
 * @param latitude Latitude
 * @param longitude Longitude
 * @param weather_data Weather data result
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int external_apis_get_weather(double latitude, double longitude, external_weather_data_t* weather_data);

/**
 * Get API statistics
 * @param api_type API type
 * @param stats Statistics structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int external_apis_get_statistics(external_api_type_t api_type, external_api_statistics_t* stats);

/**
 * Get all API statistics
 * @param stats_array Array to fill with statistics
 * @param max_apis Maximum APIs to return
 * @return Number of APIs returned, or negative error code
 */
int external_apis_get_all_statistics(external_api_statistics_t* stats_array, int max_apis);

/**
 * Reset API statistics
 * @param api_type API type (or EXTERNAL_API_MAX for all)
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int external_apis_reset_statistics(external_api_type_t api_type);

/**
 * Perform health check for all APIs
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int external_apis_health_check(void);

/**
 * Check if API is available and healthy
 * @param api_type API type
 * @return true if available and healthy, false otherwise
 */
bool external_apis_is_healthy(external_api_type_t api_type);

/**
 * Enable/disable API
 * @param api_type API type
 * @param enabled Whether to enable the API
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int external_apis_set_enabled(external_api_type_t api_type, bool enabled);

/**
 * Check rate limits for API
 * @param api_type API type
 * @return true if within rate limits, false if rate limited
 */
bool external_apis_check_rate_limit(external_api_type_t api_type);

/**
 * Get API status
 * @param api_type API type
 * @return API status enum
 */
api_status_t external_apis_get_status(external_api_type_t api_type);

// Utility functions

/**
 * Convert API type to string
 * @param api_type API type
 * @return API type string
 */
const char* external_api_type_to_string(external_api_type_t api_type);

/**
 * Parse API type from string
 * @param api_str API type string
 * @return API type enum
 */
external_api_type_t external_api_parse_type(const char* api_str);

/**
 * Convert API status to string
 * @param status API status
 * @return Status string
 */
const char* external_api_status_to_string(api_status_t status);

/**
 * Check if external APIs manager is initialized
 * @return true if initialized, false otherwise
 */
bool external_apis_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif // EXTERNAL_APIS_H