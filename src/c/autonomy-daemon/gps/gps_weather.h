#ifndef GPS_WEATHER_H
#define GPS_WEATHER_H

#include "../core/types.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Weather conditions
typedef enum {
    WEATHER_CONDITION_UNKNOWN = 0,
    WEATHER_CONDITION_CLEAR,
    WEATHER_CONDITION_CLOUDY,
    WEATHER_CONDITION_PARTLY_CLOUDY,
    WEATHER_CONDITION_RAIN,
    WEATHER_CONDITION_SNOW,
    WEATHER_CONDITION_SLEET,
    WEATHER_CONDITION_HAIL,
    WEATHER_CONDITION_THUNDERSTORM,
    WEATHER_CONDITION_FOG,
    WEATHER_CONDITION_MIST,
    WEATHER_CONDITION_DRIZZLE,
    WEATHER_CONDITION_SHOWER,
    WEATHER_CONDITION_STORM,
    WEATHER_CONDITION_TORNADO,
    WEATHER_CONDITION_HURRICANE,
    WEATHER_CONDITION_SANDSTORM,
    WEATHER_CONDITION_DUST,
    WEATHER_CONDITION_SMOKE,
    WEATHER_CONDITION_HAZE,
    WEATHER_CONDITION_MAX
} weather_condition_t;

// Weather API response
typedef struct {
    bool success;                       // Whether request was successful
    time_t timestamp;                   // Response timestamp
    long http_code;                     // HTTP response code
    int error_code;                     // CURL error code
    char data[16384];                   // Response data
    size_t data_size;                   // Size of response data
} gps_weather_api_response_t;

// Current weather information
typedef struct {
    time_t timestamp;                   // Weather timestamp
    double temperature;                  // Temperature in Celsius
    double humidity;                     // Humidity percentage
    double pressure;                     // Atmospheric pressure in hPa
    double wind_speed;                   // Wind speed in m/s
    double wind_direction;               // Wind direction in degrees
    double visibility;                   // Visibility in meters
    double cloud_cover;                  // Cloud cover percentage
    weather_condition_t weather_condition; // Weather condition
    int air_quality_index;               // Air quality index
    char description[128];                // Weather description
    char icon[16];                        // Weather icon code
    double lat;                           // Latitude
    double lon;                           // Longitude
} gps_weather_current_t;

// Weather forecast information
typedef struct {
    time_t timestamp;                   // Forecast timestamp
    int forecast_count;                  // Number of forecast entries
    gps_weather_current_t forecasts[56]; // Forecast entries (7 days * 8 per day)
} gps_weather_forecast_t;

// Air quality information
typedef struct {
    time_t timestamp;                   // Air quality timestamp
    int air_quality_index;              // Air quality index
    double co;                          // Carbon monoxide
    double no2;                         // Nitrogen dioxide
    double o3;                          // Ozone
    double so2;                         // Sulfur dioxide
    double pm2_5;                       // PM2.5 particles
    double pm10;                        // PM10 particles
} gps_weather_air_quality_t;

// Weather cache entry
typedef struct {
    bool active;                        // Whether entry is active
    double lat;                         // Latitude
    double lon;                         // Longitude
    time_t timestamp;                   // Cache timestamp
    double temperature;                  // Temperature
    double humidity;                     // Humidity
    double pressure;                     // Pressure
    double wind_speed;                   // Wind speed
    double wind_direction;               // Wind direction
    double visibility;                   // Visibility
    double cloud_cover;                  // Cloud cover
    weather_condition_t weather_condition; // Weather condition
    int air_quality_index;               // Air quality index
} gps_weather_cache_entry_t;

// Weather integration configuration
typedef struct {
    bool enabled;                       // Enable/disable integration
    int max_cache_entries;              // Maximum cache entries
    int update_interval;                // Update interval in seconds
    int max_forecast_days;              // Maximum forecast days
    double cache_radius;                // Cache radius in meters
    char api_key[256];                  // Weather API key
} gps_weather_config_t;

// Weather integration status
typedef struct {
    bool enabled;                       // Integration enabled
    int cache_entry_count;              // Current cache entries
    int max_cache_entries;              // Maximum cache entries
    int total_requests;                 // Total API requests
    int successful_requests;            // Successful requests
    int failed_requests;                // Failed requests
    time_t last_update;                 // Last update timestamp
    double success_rate;                // Success rate (0-1)
} gps_weather_status_t;

// Weather integration statistics
typedef struct {
    int total_entries;                  // Total cache entries
    int total_requests;                 // Total requests
    int successful_requests;            // Successful requests
    int failed_requests;                // Failed requests
    double total_temperature;           // Total temperature
    double total_humidity;              // Total humidity
    double total_pressure;              // Total pressure
    double total_wind_speed;            // Total wind speed
    double average_temperature;         // Average temperature
    double average_humidity;            // Average humidity
    double average_pressure;            // Average pressure
    double average_wind_speed;          // Average wind speed
} gps_weather_stats_t;

// Weather integration system state
typedef struct {
    bool enabled;                       // Integration enabled
    int max_cache_entries;              // Maximum cache entries
    int update_interval;                // Update interval
    int max_forecast_days;              // Maximum forecast days
    double cache_radius;                // Cache radius
    char api_key[256];                  // API key
    
    // State
    int cache_entry_count;              // Cache entry count
    int total_requests;                 // Total requests
    int successful_requests;            // Successful requests
    int failed_requests;                // Failed requests
    time_t last_update;                 // Last update
    
    // Weather cache
    gps_weather_cache_entry_t weather_cache[1000]; // Weather cache entries
} gps_weather_t;

// Function prototypes

/**
 * Initialize GPS weather integration
 * @param api_key Weather API key
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_weather_init(const char *api_key);

/**
 * Get current weather for coordinates
 * @param lat Latitude
 * @param lon Longitude
 * @param weather Current weather (output)
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_weather_get_current(double lat, double lon, gps_weather_current_t *weather);

/**
 * Get weather forecast for coordinates
 * @param lat Latitude
 * @param lon Longitude
 * @param days Number of forecast days
 * @param forecast Weather forecast (output)
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_weather_get_forecast(double lat, double lon, int days, 
                            gps_weather_forecast_t *forecast);

/**
 * Get air quality for coordinates
 * @param lat Latitude
 * @param lon Longitude
 * @param air_quality Air quality (output)
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_weather_get_air_quality(double lat, double lon, gps_weather_air_quality_t *air_quality);

/**
 * Get weather integration status
 * @param status Status structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_weather_get_status(gps_weather_status_t *status);

/**
 * Get weather integration configuration
 * @param config Configuration structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_weather_get_config(gps_weather_config_t *config);

/**
 * Set weather integration configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_weather_set_config(const gps_weather_config_t *config);

/**
 * Enable/disable weather integration
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_weather_set_enabled(bool enabled);

/**
 * Force weather update
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_weather_force_update(void);

/**
 * Get weather statistics
 * @param stats Statistics structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_weather_get_statistics(gps_weather_stats_t *stats);

/**
 * Reset weather integration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_weather_reset(void);

/**
 * Cleanup weather integration
 */
void gps_weather_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_WEATHER_H
