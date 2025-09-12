#ifndef STARLINK_WEATHER_SNOW_MELT_CONTROL_H
#define STARLINK_WEATHER_SNOW_MELT_CONTROL_H

#include "../core/types.h"
#include "../external/external_apis.h"
#include "../utils/json_parser.h"
#include "starlink_grpc_comprehensive_client.h"
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Snow melt control modes
typedef enum {
    SNOW_MELT_OFF = 0,           // Snow melt disabled
    SNOW_MELT_AUTOMATIC,         // Automatic based on temperature
    SNOW_MELT_PREHEAT,           // Pre-heat mode for expected snow/rain
    SNOW_MELT_MANUAL             // Manual control
} snow_melt_mode_t;

// Weather conditions for snow melt decisions
typedef enum {
    WEATHER_CLEAR = 0,           // Clear weather
    WEATHER_CLOUDY,              // Cloudy
    WEATHER_RAIN,                // Rain
    WEATHER_HEAVY_RAIN,          // Heavy rain
    WEATHER_SNOW,                // Snow
    WEATHER_HEAVY_SNOW,          // Heavy snow
    WEATHER_SLEET,               // Sleet/freezing rain
    WEATHER_FOG,                 // Fog
    WEATHER_UNKNOWN              // Unknown weather
} weather_condition_t;

// Snow melt control configuration
typedef struct {
    bool enabled;                        // Enable weather-based snow melt control
    double temperature_threshold_celsius; // Temperature threshold for automatic mode (default: 5.0°C)
    int weather_check_interval_minutes;  // Weather check interval (default: 15 minutes)
    int preheat_duration_minutes;        // Pre-heat duration for expected precipitation (default: 30 minutes)
    bool use_forecast;                   // Use weather forecast for pre-heating
    int forecast_hours_ahead;            // How many hours ahead to check forecast (default: 1 hour)
    char weather_api_key[128];           // OpenWeatherMap API key
    char starlink_host[64];              // Starlink dish IP (default: 192.168.100.1)
    int starlink_port;                   // Starlink dish port (default: 9200)
    bool debug_mode;                     // Enable debug logging
} starlink_weather_snow_melt_config_t;

// Snow melt control status
typedef struct {
    bool enabled;                        // System enabled status
    snow_melt_mode_t current_mode;       // Current snow melt mode
    snow_melt_mode_t previous_mode;      // Previous snow melt mode
    time_t last_mode_change;             // Last mode change timestamp
    time_t last_weather_check;           // Last weather check timestamp
    time_t preheat_start_time;           // Pre-heat start time (if in pre-heat mode)
    int preheat_remaining_minutes;       // Remaining pre-heat time in minutes
    weather_condition_t current_weather; // Current weather condition
    weather_condition_t forecast_weather; // Forecast weather condition
    double current_temperature;          // Current temperature in Celsius
    double forecast_temperature;         // Forecast temperature in Celsius
    bool precipitation_expected;         // Whether precipitation is expected
    int precipitation_probability;       // Precipitation probability percentage
    char last_weather_description[128];  // Last weather description
    time_t last_weather_update;          // Last weather data update
} starlink_weather_snow_melt_status_t;

// Snow melt control statistics
typedef struct {
    int total_mode_changes;              // Total mode changes
    int automatic_activations;           // Automatic mode activations
    int preheat_activations;             // Pre-heat mode activations
    int manual_activations;              // Manual mode activations
    int weather_checks_performed;        // Total weather checks performed
    int successful_weather_checks;       // Successful weather checks
    int failed_weather_checks;           // Failed weather checks
    double average_weather_check_time_ms; // Average weather check time
    time_t last_automatic_activation;    // Last automatic activation
    time_t last_preheat_activation;      // Last pre-heat activation
    time_t last_manual_activation;       // Last manual activation
} starlink_weather_snow_melt_stats_t;

// Combined structure for internal use
typedef struct {
    // Configuration
    starlink_weather_snow_melt_config_t config;
    
    // Status
    starlink_weather_snow_melt_status_t status;
    
    // Statistics
    starlink_weather_snow_melt_stats_t stats;
    
    // Internal state
    bool initialized;                    // Initialization status
    time_t last_control_check;           // Last control check timestamp
    pthread_mutex_t mutex;               // Thread safety mutex
    pthread_t control_thread;            // Control thread
    bool thread_running;                 // Thread running status
    
    // Weather data cache
    external_weather_data_t current_weather_data;
    external_weather_data_t forecast_weather_data;
    time_t weather_cache_timestamp;      // Weather cache timestamp
    int weather_cache_valid_minutes;     // Weather cache validity in minutes
} starlink_weather_snow_melt_control_t;

// API Functions

/**
 * Initialize weather-based snow melt control system
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_weather_snow_melt_control_init(void);

/**
 * Cleanup weather-based snow melt control system
 */
void starlink_weather_snow_melt_control_cleanup(void);

/**
 * Get current snow melt control status
 * @param status Pointer to status structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_weather_snow_melt_control_get_status(starlink_weather_snow_melt_status_t *status);

/**
 * Get snow melt control configuration
 * @param config Pointer to configuration structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_weather_snow_melt_control_get_config(starlink_weather_snow_melt_config_t *config);

/**
 * Set snow melt control configuration
 * @param config Pointer to configuration structure
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_weather_snow_melt_control_set_config(const starlink_weather_snow_melt_config_t *config);

/**
 * Enable or disable weather-based snow melt control
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_weather_snow_melt_control_set_enabled(bool enabled);

/**
 * Manually set snow melt mode
 * @param mode Snow melt mode to set
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_weather_snow_melt_control_set_mode(snow_melt_mode_t mode);

/**
 * Force weather check and mode update
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_weather_snow_melt_control_force_update(void);

/**
 * Get snow melt control statistics
 * @param stats Pointer to statistics structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_weather_snow_melt_control_get_statistics(starlink_weather_snow_melt_stats_t *stats);

/**
 * Reset snow melt control statistics
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_weather_snow_melt_control_reset_statistics(void);

// Internal functions

/**
 * Determine snow melt mode based on weather conditions
 * @param weather_data Current weather data
 * @param forecast_data Forecast weather data
 * @return Recommended snow melt mode
 */
snow_melt_mode_t starlink_weather_snow_melt_determine_mode(
    const external_weather_data_t *weather_data,
    const external_weather_data_t *forecast_data
);

/**
 * Convert weather description to weather condition
 * @param description Weather description string
 * @return Weather condition enum
 */
weather_condition_t starlink_weather_snow_melt_parse_weather_condition(const char *description);

/**
 * Check if precipitation is expected in the forecast
 * @param forecast_data Forecast weather data
 * @param hours_ahead Hours ahead to check
 * @return True if precipitation is expected
 */
bool starlink_weather_snow_melt_is_precipitation_expected(
    const external_weather_data_t *forecast_data,
    int hours_ahead
);

/**
 * Send snow melt command to Starlink dish via gRPC
 * @param mode Snow melt mode to set
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_weather_snow_melt_send_grpc_command(snow_melt_mode_t mode);

/**
 * Get weather data from external API
 * @param latitude Latitude coordinate
 * @param longitude Longitude coordinate
 * @param weather_data Pointer to weather data structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_weather_snow_melt_get_weather_data(
    double latitude,
    double longitude,
    external_weather_data_t *weather_data
);

/**
 * Get weather forecast from external API
 * @param latitude Latitude coordinate
 * @param longitude Longitude coordinate
 * @param hours_ahead Hours ahead for forecast
 * @param forecast_data Pointer to forecast data structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_weather_snow_melt_get_weather_forecast(
    double latitude,
    double longitude,
    int hours_ahead,
    external_weather_data_t *forecast_data
);

// UCI Configuration functions

/**
 * Load configuration from UCI
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_weather_snow_melt_control_load_uci_config(void);

/**
 * Save configuration to UCI
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_weather_snow_melt_control_save_uci_config(void);

// Utility functions

/**
 * Get snow melt mode string for logging
 * @param mode Snow melt mode
 * @return String representation of mode
 */
const char* starlink_weather_snow_melt_mode_to_string(snow_melt_mode_t mode);

/**
 * Get weather condition string for logging
 * @param condition Weather condition
 * @return String representation of condition
 */
const char* starlink_weather_snow_melt_weather_condition_to_string(weather_condition_t condition);

#ifdef __cplusplus
}
#endif

#endif // STARLINK_WEATHER_SNOW_MELT_CONTROL_H
