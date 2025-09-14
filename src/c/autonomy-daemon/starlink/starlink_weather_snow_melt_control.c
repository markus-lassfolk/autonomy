#include "starlink_weather_snow_melt_control.h"
#include "../shared/logging/logx.h"
#include "../shared/utils/string_utils.h"
#include "../shared/utils/uci_manager.h"
#include "../gps/gps_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <uci.h>

// External UCI functions and context
extern struct uci_context *uci_ctx;
extern const char* ucix_get_option(struct uci_context *ctx, const char *package, const char *section, const char *option);
extern int ucix_add_option(struct uci_context *ctx, const char *package, const char *section, const char *option, const char *value);
extern int ucix_add_option_int(struct uci_context *ctx, const char *package, const char *section, const char *option, int value);
extern int ucix_logged_commit(struct uci_context *ctx, const char *package);

// Wrapper function to match the signature used in this file
static int ucix_get_option_wrapper(const char *package, const char *section, const char *option, char *value, size_t size) {
    if (!uci_ctx || !package || !section || !option || !value || size == 0) {
        return -1;
    }
    
    const char *result = ucix_get_option(uci_ctx, package, section, option);
    if (result) {
        strncpy(value, result, size - 1);
        value[size - 1] = '\0';
        return 0;  // Success
    }
    
    return -1;  // Not found or error
}

// Global instance
static starlink_weather_snow_melt_control_t g_snow_melt_control = {0};

// Default configuration
static const starlink_weather_snow_melt_config_t DEFAULT_CONFIG = {
    .enabled = true,
    .temperature_threshold_celsius = 5.0,
    .weather_check_interval_minutes = 15,
    .preheat_duration_minutes = 30,
    .use_forecast = true,
    .forecast_hours_ahead = 1,
    .weather_api_key = "",
    .starlink_host = "192.168.100.1",
    .starlink_port = 9200,
    .debug_mode = false
};

// Initialize weather-based snow melt control system
int starlink_weather_snow_melt_control_init(void) {
    if (g_snow_melt_control.initialized) {
        return AUTONOMY_SUCCESS;
    }
    
    LOGX_INFO_MSG("Initializing weather-based snow melt control system");
    
    // Initialize mutex
    if (pthread_mutex_init(&g_snow_melt_control.mutex, NULL) != 0) {
        LOGX_ERROR_MSG("Failed to initialize snow melt control mutex");
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    // Set default configuration
    memcpy(&g_snow_melt_control.config, &DEFAULT_CONFIG, sizeof(starlink_weather_snow_melt_config_t));
    
    // Initialize status
    memset(&g_snow_melt_control.status, 0, sizeof(starlink_weather_snow_melt_status_t));
    g_snow_melt_control.status.current_mode = SNOW_MELT_OFF;
    g_snow_melt_control.status.previous_mode = SNOW_MELT_OFF;
    g_snow_melt_control.status.current_weather = WEATHER_UNKNOWN;
    g_snow_melt_control.status.forecast_weather = WEATHER_UNKNOWN;
    
    // Initialize statistics
    memset(&g_snow_melt_control.stats, 0, sizeof(starlink_weather_snow_melt_stats_t));
    
    // Load configuration from UCI
    int config_result = starlink_weather_snow_melt_control_load_uci_config();
    if (config_result != AUTONOMY_SUCCESS) {
        LOGX_WARN_MSG("Failed to load UCI configuration, using defaults");
    }
    
    // Initialize external APIs if not already done
    extern int external_apis_init(void);
    int api_result = external_apis_init();
    if (api_result != AUTONOMY_SUCCESS) {
        printf("WARN: Failed to initialize external APIs: %d\n", api_result);
    }
    
    // Initialize gRPC client
    extern int starlink_grpc_comprehensive_client_init(starlink_grpc_client_config_t *config);
    starlink_grpc_client_config_t grpc_config = {0};
    safe_strncpy(grpc_config.host, g_snow_melt_control.config.starlink_host, sizeof(grpc_config.host));
    grpc_config.host[sizeof(grpc_config.host) - 1] = '\0';
    grpc_config.port = g_snow_melt_control.config.starlink_port;
    grpc_config.timeout = 10;
    grpc_config.retries = 3;
    grpc_config.debug_mode = g_snow_melt_control.config.debug_mode;
    
    int grpc_result = starlink_grpc_comprehensive_client_init(&grpc_config);
    if (grpc_result != AUTONOMY_SUCCESS) {
        printf("WARN: Failed to initialize gRPC client: %d\n", grpc_result);
    }
    
    g_snow_melt_control.initialized = true;
    g_snow_melt_control.weather_cache_valid_minutes = 10; // Cache weather data for 10 minutes
    
    LOGX_INFO_MSG("Weather-based snow melt control system initialized successfully");
    printf("INFO: Temperature threshold: %.1fC, Check interval: %d minutes\n",
           g_snow_melt_control.config.temperature_threshold_celsius,
           g_snow_melt_control.config.weather_check_interval_minutes);
    
    return AUTONOMY_SUCCESS;
}

// Cleanup weather-based snow melt control system
void starlink_weather_snow_melt_control_cleanup(void) {
    if (!g_snow_melt_control.initialized) {
        return;
    }
    
    LOGX_INFO_MSG("Cleaning up weather-based snow melt control system");
    
    // Stop control thread if running
    if (g_snow_melt_control.thread_running) {
        g_snow_melt_control.thread_running = false;
        if (g_snow_melt_control.control_thread) {
            pthread_join(g_snow_melt_control.control_thread, NULL);
        }
    }
    
    // Cleanup mutex
    pthread_mutex_destroy(&g_snow_melt_control.mutex);
    
    g_snow_melt_control.initialized = false;
    
    LOGX_INFO_MSG("Weather-based snow melt control system cleaned up");
}

// Get current snow melt control status
int starlink_weather_snow_melt_control_get_status(starlink_weather_snow_melt_status_t *status) {
    if (!g_snow_melt_control.initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_snow_melt_control.mutex);
    memcpy(status, &g_snow_melt_control.status, sizeof(starlink_weather_snow_melt_status_t));
    pthread_mutex_unlock(&g_snow_melt_control.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get snow melt control configuration
int starlink_weather_snow_melt_control_get_config(starlink_weather_snow_melt_config_t *config) {
    if (!g_snow_melt_control.initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_snow_melt_control.mutex);
    memcpy(config, &g_snow_melt_control.config, sizeof(starlink_weather_snow_melt_config_t));
    pthread_mutex_unlock(&g_snow_melt_control.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set snow melt control configuration
int starlink_weather_snow_melt_control_set_config(const starlink_weather_snow_melt_config_t *config) {
    if (!g_snow_melt_control.initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_snow_melt_control.mutex);
    memcpy(&g_snow_melt_control.config, config, sizeof(starlink_weather_snow_melt_config_t));
    pthread_mutex_unlock(&g_snow_melt_control.mutex);
    
    // Save to UCI
    starlink_weather_snow_melt_control_save_uci_config();
    
    LOGX_INFO_MSG("Snow melt control configuration updated");
    
    return AUTONOMY_SUCCESS;
}

// Enable or disable weather-based snow melt control
int starlink_weather_snow_melt_control_set_enabled(bool enabled) {
    if (!g_snow_melt_control.initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_snow_melt_control.mutex);
    g_snow_melt_control.config.enabled = enabled;
    pthread_mutex_unlock(&g_snow_melt_control.mutex);
    
    // Save to UCI
    starlink_weather_snow_melt_control_save_uci_config();
    
    printf("INFO: Snow melt control %s\n", enabled ? "enabled" : "disabled");
    
    return AUTONOMY_SUCCESS;
}

// Manually set snow melt mode
int starlink_weather_snow_melt_control_set_mode(snow_melt_mode_t mode) {
    if (!g_snow_melt_control.initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_snow_melt_control.mutex);
    
    if (g_snow_melt_control.status.current_mode != mode) {
        g_snow_melt_control.status.previous_mode = g_snow_melt_control.status.current_mode;
        g_snow_melt_control.status.current_mode = mode;
        g_snow_melt_control.status.last_mode_change = time(NULL);
        g_snow_melt_control.stats.total_mode_changes++;
        
        // Update statistics based on mode
        switch (mode) {
            case SNOW_MELT_AUTOMATIC:
                g_snow_melt_control.stats.automatic_activations++;
                g_snow_melt_control.stats.last_automatic_activation = time(NULL);
                break;
            case SNOW_MELT_PREHEAT:
                g_snow_melt_control.stats.preheat_activations++;
                g_snow_melt_control.stats.last_preheat_activation = time(NULL);
                g_snow_melt_control.status.preheat_start_time = time(NULL);
                g_snow_melt_control.status.preheat_remaining_minutes = g_snow_melt_control.config.preheat_duration_minutes;
                break;
            case SNOW_MELT_MANUAL:
                g_snow_melt_control.stats.manual_activations++;
                g_snow_melt_control.stats.last_manual_activation = time(NULL);
                break;
            default:
                break;
        }
        
        pthread_mutex_unlock(&g_snow_melt_control.mutex);
        
        // Send command to Starlink dish
        int result = starlink_weather_snow_melt_send_grpc_command(mode);
        if (result == AUTONOMY_SUCCESS) {
            printf("INFO: Snow melt mode changed to %s\n", starlink_weather_snow_melt_mode_to_string(mode));
        } else {
            printf("ERROR: Failed to send snow melt command to Starlink dish: %d\n", result);
        }
        
        return result;
    }
    
    pthread_mutex_unlock(&g_snow_melt_control.mutex);
    return AUTONOMY_SUCCESS;
}

// Force weather check and mode update
int starlink_weather_snow_melt_control_force_update(void) {
    if (!g_snow_melt_control.initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    if (!g_snow_melt_control.config.enabled) {
        return AUTONOMY_SUCCESS; // System disabled, nothing to do
    }
    
    LOGX_INFO_MSG("Forcing weather check and snow melt mode update");
    
    // Get current GPS location
    gps_data_t gps_data;
    extern int gps_manager_get_current_location(gps_data_t *gps_data);
    int gps_result = gps_manager_get_current_location(&gps_data);
    if (gps_result != AUTONOMY_SUCCESS) {
        printf("WARN: Failed to get GPS location for weather check: %d\n", gps_result);
        return gps_result;
    }
    
    // Get current weather data
    external_weather_data_t current_weather;
    int weather_result = starlink_weather_snow_melt_get_weather_data(
        gps_data.latitude, gps_data.longitude, &current_weather);
    if (weather_result != AUTONOMY_SUCCESS) {
        printf("WARN: Failed to get current weather data: %d\n", weather_result);
        return weather_result;
    }
    
    // Get forecast weather data if enabled
    external_weather_data_t forecast_weather = {0};
    if (g_snow_melt_control.config.use_forecast) {
        int forecast_result = starlink_weather_snow_melt_get_weather_forecast(
            gps_data.latitude, gps_data.longitude, 
            g_snow_melt_control.config.forecast_hours_ahead, &forecast_weather);
        if (forecast_result != AUTONOMY_SUCCESS) {
            printf("WARN: Failed to get weather forecast: %d\n", forecast_result);
        }
    }
    
    // Determine recommended mode
    snow_melt_mode_t recommended_mode = starlink_weather_snow_melt_determine_mode(
        &current_weather, &forecast_weather);
    
    // Update status
    pthread_mutex_lock(&g_snow_melt_control.mutex);
    g_snow_melt_control.status.last_weather_check = time(NULL);
    g_snow_melt_control.status.current_temperature = current_weather.temperature_celsius;
    g_snow_melt_control.status.current_weather = starlink_weather_snow_melt_parse_weather_condition(
        current_weather.description);
    g_snow_melt_control.status.forecast_temperature = forecast_weather.temperature_celsius;
    g_snow_melt_control.status.forecast_weather = starlink_weather_snow_melt_parse_weather_condition(
        forecast_weather.description);
    g_snow_melt_control.status.precipitation_expected = starlink_weather_snow_melt_is_precipitation_expected(
        &forecast_weather, g_snow_melt_control.config.forecast_hours_ahead);
    strncpy(g_snow_melt_control.status.last_weather_description, current_weather.description,
            sizeof(g_snow_melt_control.status.last_weather_description) - 1);
    g_snow_melt_control.status.last_weather_description[sizeof(g_snow_melt_control.status.last_weather_description) - 1] = '\0';
    g_snow_melt_control.status.last_weather_update = time(NULL);
    pthread_mutex_unlock(&g_snow_melt_control.mutex);
    
    // Update statistics
    g_snow_melt_control.stats.weather_checks_performed++;
    g_snow_melt_control.stats.successful_weather_checks++;
    
    // Set mode if it changed
    if (recommended_mode != g_snow_melt_control.status.current_mode) {
        printf("INFO: Weather conditions changed, updating snow melt mode from %s to %s\n",
               starlink_weather_snow_melt_mode_to_string(g_snow_melt_control.status.current_mode),
               starlink_weather_snow_melt_mode_to_string(recommended_mode));
        
        return starlink_weather_snow_melt_control_set_mode(recommended_mode);
    }
    
    printf("INFO: Weather check completed, no mode change needed (current: %s)\n",
           starlink_weather_snow_melt_mode_to_string(g_snow_melt_control.status.current_mode));
    
    return AUTONOMY_SUCCESS;
}

// Get snow melt control statistics
int starlink_weather_snow_melt_control_get_statistics(starlink_weather_snow_melt_stats_t *stats) {
    if (!g_snow_melt_control.initialized || !stats) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_snow_melt_control.mutex);
    memcpy(stats, &g_snow_melt_control.stats, sizeof(starlink_weather_snow_melt_stats_t));
    pthread_mutex_unlock(&g_snow_melt_control.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Reset snow melt control statistics
int starlink_weather_snow_melt_control_reset_statistics(void) {
    if (!g_snow_melt_control.initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_snow_melt_control.mutex);
    memset(&g_snow_melt_control.stats, 0, sizeof(starlink_weather_snow_melt_stats_t));
    pthread_mutex_unlock(&g_snow_melt_control.mutex);
    
    LOGX_INFO_MSG("Snow melt control statistics reset");
    
    return AUTONOMY_SUCCESS;
}

// Determine snow melt mode based on weather conditions
snow_melt_mode_t starlink_weather_snow_melt_determine_mode(
    const external_weather_data_t *weather_data,
    const external_weather_data_t *forecast_data) {
    
    if (!weather_data) {
        return SNOW_MELT_OFF;
    }
    
    double current_temp = weather_data->temperature_celsius;
    double forecast_temp = forecast_data ? forecast_data->temperature_celsius : current_temp;
    
    // Rule 1: Snow melt is OFF when temperature is above +5C
    if (current_temp > g_snow_melt_control.config.temperature_threshold_celsius) {
        return SNOW_MELT_OFF;
    }
    
    // Rule 2: Check for expected precipitation (snow or heavy rain) within forecast period
    if (forecast_data && g_snow_melt_control.config.use_forecast) {
        weather_condition_t forecast_condition = starlink_weather_snow_melt_parse_weather_condition(
            forecast_data->description);
        
        // Check if precipitation is expected
        if (forecast_condition == WEATHER_SNOW || 
            forecast_condition == WEATHER_HEAVY_SNOW ||
            forecast_condition == WEATHER_HEAVY_RAIN ||
            forecast_condition == WEATHER_SLEET) {
            
            printf("INFO: Precipitation expected in forecast: %s, setting PREHEAT mode\n",
                   forecast_data->description);
            return SNOW_MELT_PREHEAT;
        }
    }
    
    // Rule 3: Check current weather for snow/rain
    weather_condition_t current_condition = starlink_weather_snow_melt_parse_weather_condition(
        weather_data->description);
    
    if (current_condition == WEATHER_SNOW || 
        current_condition == WEATHER_HEAVY_SNOW ||
        current_condition == WEATHER_HEAVY_RAIN ||
        current_condition == WEATHER_SLEET) {
        
        printf("INFO: Current precipitation detected: %s, setting PREHEAT mode\n",
               weather_data->description);
        return SNOW_MELT_PREHEAT;
    }
    
    // Rule 4: Automatic mode when below +5C but no precipitation
    if (current_temp <= g_snow_melt_control.config.temperature_threshold_celsius) {
        return SNOW_MELT_AUTOMATIC;
    }
    
    // Default: OFF
    return SNOW_MELT_OFF;
}

// Convert weather description to weather condition
weather_condition_t starlink_weather_snow_melt_parse_weather_condition(const char *description) {
    if (!description) {
        return WEATHER_UNKNOWN;
    }
    
    // Convert to lowercase for comparison
    char desc_lower[128];
    safe_strncpy(desc_lower, description, sizeof(desc_lower));
    desc_lower[sizeof(desc_lower) - 1] = '\0';
    
    for (int i = 0; desc_lower[i]; i++) {
        desc_lower[i] = tolower(desc_lower[i]);
    }
    
    // Check for snow conditions
    if (strstr(desc_lower, "snow") || strstr(desc_lower, "blizzard")) {
        if (strstr(desc_lower, "heavy") || strstr(desc_lower, "intense")) {
            return WEATHER_HEAVY_SNOW;
        }
        return WEATHER_SNOW;
    }
    
    // Check for rain conditions
    if (strstr(desc_lower, "rain") || strstr(desc_lower, "shower")) {
        if (strstr(desc_lower, "heavy") || strstr(desc_lower, "intense") || 
            strstr(desc_lower, "torrential")) {
            return WEATHER_HEAVY_RAIN;
        }
        return WEATHER_RAIN;
    }
    
    // Check for sleet/freezing rain
    if (strstr(desc_lower, "sleet") || strstr(desc_lower, "freezing rain") ||
        strstr(desc_lower, "ice")) {
        return WEATHER_SLEET;
    }
    
    // Check for fog
    if (strstr(desc_lower, "fog") || strstr(desc_lower, "mist")) {
        return WEATHER_FOG;
    }
    
    // Check for cloudy conditions
    if (strstr(desc_lower, "cloud") || strstr(desc_lower, "overcast")) {
        return WEATHER_CLOUDY;
    }
    
    // Check for clear conditions
    if (strstr(desc_lower, "clear") || strstr(desc_lower, "sunny") || 
        strstr(desc_lower, "fair")) {
        return WEATHER_CLEAR;
    }
    
    return WEATHER_UNKNOWN;
}

// Check if precipitation is expected in the forecast
bool starlink_weather_snow_melt_is_precipitation_expected(
    const external_weather_data_t *forecast_data,
    int hours_ahead) {
    
    if (!forecast_data) {
        return false;
    }
    
    weather_condition_t condition = starlink_weather_snow_melt_parse_weather_condition(
        forecast_data->description);
    
    return (condition == WEATHER_SNOW || 
            condition == WEATHER_HEAVY_SNOW ||
            condition == WEATHER_HEAVY_RAIN ||
            condition == WEATHER_SLEET);
}

// Send snow melt command to Starlink dish via gRPC
int starlink_weather_snow_melt_send_grpc_command(snow_melt_mode_t mode) {
    if (!g_snow_melt_control.initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    // Prepare gRPC request based on mode
    char request_json[512];
    const char *method = "SpaceX.API.Device.Device/Handle";
    
    switch (mode) {
        case SNOW_MELT_OFF:
            snprintf(request_json, sizeof(request_json),
                "{\"set_thermal_state\":{\"target_thermal_state\":{\"heater\":false}}}");
            break;
            
        case SNOW_MELT_AUTOMATIC:
            snprintf(request_json, sizeof(request_json),
                "{\"set_thermal_state\":{\"target_thermal_state\":{\"heater\":true,\"heater_mode\":\"AUTO\"}}}");
            break;
            
        case SNOW_MELT_PREHEAT:
            snprintf(request_json, sizeof(request_json),
                "{\"set_thermal_state\":{\"target_thermal_state\":{\"heater\":true,\"heater_mode\":\"PREHEAT\"}}}");
            break;
            
        case SNOW_MELT_MANUAL:
            snprintf(request_json, sizeof(request_json),
                "{\"set_thermal_state\":{\"target_thermal_state\":{\"heater\":true,\"heater_mode\":\"MANUAL\"}}}");
            break;
            
        default:
            printf("ERROR: Unknown snow melt mode: %d\n", mode);
            return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Send gRPC request
    extern int starlink_grpc_comprehensive_call(
        const char *method,
        const char *request_data,
        size_t request_size,
        starlink_grpc_response_t *response);
    
    starlink_grpc_response_t response = {0};
    int result = starlink_grpc_comprehensive_call(
        method, request_json, strlen(request_json), &response);
    
    if (result == AUTONOMY_SUCCESS && response.success) {
        printf("INFO: Successfully sent snow melt command: %s\n", 
               starlink_weather_snow_melt_mode_to_string(mode));
        
        if (g_snow_melt_control.config.debug_mode && response.response_data) {
            printf("DEBUG: Starlink response: %s\n", response.response_data);
        }
    } else {
        printf("ERROR: Failed to send snow melt command: %d\n", result);
        if (response.error_message[0]) {
            printf("ERROR: gRPC error: %s\n", response.error_message);
        }
    }
    
    // Cleanup response
    if (response.response_data) {
        free(response.response_data);
    }
    
    return result;
}

// Get weather data from external API
int starlink_weather_snow_melt_get_weather_data(
    double latitude,
    double longitude,
    external_weather_data_t *weather_data) {
    
    if (!weather_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Check cache first
    time_t now = time(NULL);
    if (g_snow_melt_control.weather_cache_timestamp > 0 &&
        (now - g_snow_melt_control.weather_cache_timestamp) < 
        (g_snow_melt_control.weather_cache_valid_minutes * 60)) {
        
        memcpy(weather_data, &g_snow_melt_control.current_weather_data, 
               sizeof(external_weather_data_t));
        
        if (g_snow_melt_control.config.debug_mode) {
            printf("DEBUG: Using cached weather data (age: %lld seconds)\n",
                    (long long)(now - g_snow_melt_control.weather_cache_timestamp));
        }
        
        return AUTONOMY_SUCCESS;
    }
    
    // Get fresh weather data
    extern int external_apis_get_weather(double latitude, double longitude, external_weather_data_t* weather_data);
    int result = external_apis_get_weather(latitude, longitude, weather_data);
    
    if (result == AUTONOMY_SUCCESS) {
        // Update cache
        memcpy(&g_snow_melt_control.current_weather_data, weather_data, 
               sizeof(external_weather_data_t));
        g_snow_melt_control.weather_cache_timestamp = now;
        
        if (g_snow_melt_control.config.debug_mode) {
            printf("DEBUG: Fresh weather data: %.1fC, %s\n",
                   weather_data->temperature_celsius, weather_data->description);
        }
    }
    
    return result;
}

// Get weather forecast from external API
int starlink_weather_snow_melt_get_weather_forecast(
    double latitude,
    double longitude,
    int hours_ahead,
    external_weather_data_t *forecast_data) {
    
    if (!forecast_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // For now, we'll use the same API as current weather
    // In a full implementation, this would call a forecast API
    int result = starlink_weather_snow_melt_get_weather_data(latitude, longitude, forecast_data);
    
    if (result == AUTONOMY_SUCCESS) {
        // Adjust timestamp to reflect forecast time
        forecast_data->timestamp += (hours_ahead * 3600);
        
        if (g_snow_melt_control.config.debug_mode) {
            printf("DEBUG: Weather forecast for +%d hours: %.1fC, %s\n",
                   hours_ahead, forecast_data->temperature_celsius, forecast_data->description);
        }
    }
    
    return result;
}

// Load configuration from UCI
int starlink_weather_snow_melt_control_load_uci_config(void) {
    if (!g_snow_melt_control.initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    LOGX_INFO_MSG("Loading snow melt control configuration from UCI");
    
    // Load configuration values
    char value[256];
    
    // Enabled
    if (ucix_get_option_wrapper("autonomy", "snow_melt_control", "enabled", value, sizeof(value)) == 0) {
        g_snow_melt_control.config.enabled = (strcmp(value, "1") == 0 || strcmp(value, "true") == 0);
    }
    
    // Temperature threshold
    if (ucix_get_option_wrapper("autonomy", "snow_melt_control", "temperature_threshold", value, sizeof(value)) == 0) {
        g_snow_melt_control.config.temperature_threshold_celsius = atof(value);
    }
    
    // Weather check interval
    if (ucix_get_option_wrapper("autonomy", "snow_melt_control", "weather_check_interval", value, sizeof(value)) == 0) {
        g_snow_melt_control.config.weather_check_interval_minutes = atoi(value);
    }
    
    // Preheat duration
    if (ucix_get_option_wrapper("autonomy", "snow_melt_control", "preheat_duration", value, sizeof(value)) == 0) {
        g_snow_melt_control.config.preheat_duration_minutes = atoi(value);
    }
    
    // Use forecast
    if (ucix_get_option_wrapper("autonomy", "snow_melt_control", "use_forecast", value, sizeof(value)) == 0) {
        g_snow_melt_control.config.use_forecast = (strcmp(value, "1") == 0 || strcmp(value, "true") == 0);
    }
    
    // Forecast hours ahead
    if (ucix_get_option_wrapper("autonomy", "snow_melt_control", "forecast_hours_ahead", value, sizeof(value)) == 0) {
        g_snow_melt_control.config.forecast_hours_ahead = atoi(value);
    }
    
    // Weather API key
    if (ucix_get_option_wrapper("autonomy", "snow_melt_control", "weather_api_key", value, sizeof(value)) == 0) {
        strncpy(g_snow_melt_control.config.weather_api_key, value, 
                sizeof(g_snow_melt_control.config.weather_api_key) - 1);
        g_snow_melt_control.config.weather_api_key[sizeof(g_snow_melt_control.config.weather_api_key) - 1] = '\0';
    }
    
    // Starlink host
    if (ucix_get_option_wrapper("autonomy", "snow_melt_control", "starlink_host", value, sizeof(value)) == 0) {
        strncpy(g_snow_melt_control.config.starlink_host, value, 
                sizeof(g_snow_melt_control.config.starlink_host) - 1);
        g_snow_melt_control.config.starlink_host[sizeof(g_snow_melt_control.config.starlink_host) - 1] = '\0';
    }
    
    // Starlink port
    if (ucix_get_option_wrapper("autonomy", "snow_melt_control", "starlink_port", value, sizeof(value)) == 0) {
        g_snow_melt_control.config.starlink_port = atoi(value);
    }
    
    // Debug mode
    if (ucix_get_option_wrapper("autonomy", "snow_melt_control", "debug_mode", value, sizeof(value)) == 0) {
        g_snow_melt_control.config.debug_mode = (strcmp(value, "1") == 0 || strcmp(value, "true") == 0);
    }
    
    LOGX_INFO_MSG("Snow melt control configuration loaded from UCI");
    
    return AUTONOMY_SUCCESS;
}

// Save configuration to UCI
int starlink_weather_snow_melt_control_save_uci_config(void) {
    if (!g_snow_melt_control.initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    if (!uci_ctx) {
        LOGX_ERROR_MSG("UCI context not available");
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    LOGX_INFO_MSG("Saving snow melt control configuration to UCI");
    
    int ret = AUTONOMY_SUCCESS;
    char value_buf[64];
    
    // Save all configuration values using UCI API
    if (ucix_add_option_int(uci_ctx, "autonomy", "snow_melt_control", "enabled", 
                           g_snow_melt_control.config.enabled ? 1 : 0) != 0) {
        ret = AUTONOMY_ERROR_SYSTEM;
    }
    
    snprintf(value_buf, sizeof(value_buf), "%.1f", g_snow_melt_control.config.temperature_threshold_celsius);
    if (ucix_add_option(uci_ctx, "autonomy", "snow_melt_control", "temperature_threshold", value_buf) != 0) {
        ret = AUTONOMY_ERROR_SYSTEM;
    }
    
    if (ucix_add_option_int(uci_ctx, "autonomy", "snow_melt_control", "weather_check_interval", 
                           g_snow_melt_control.config.weather_check_interval_minutes) != 0) {
        ret = AUTONOMY_ERROR_SYSTEM;
    }
    
    if (ucix_add_option_int(uci_ctx, "autonomy", "snow_melt_control", "preheat_duration", 
                           g_snow_melt_control.config.preheat_duration_minutes) != 0) {
        ret = AUTONOMY_ERROR_SYSTEM;
    }
    
    if (ucix_add_option_int(uci_ctx, "autonomy", "snow_melt_control", "use_forecast", 
                           g_snow_melt_control.config.use_forecast ? 1 : 0) != 0) {
        ret = AUTONOMY_ERROR_SYSTEM;
    }
    
    if (ucix_add_option_int(uci_ctx, "autonomy", "snow_melt_control", "forecast_hours_ahead", 
                           g_snow_melt_control.config.forecast_hours_ahead) != 0) {
        ret = AUTONOMY_ERROR_SYSTEM;
    }
    
    if (ucix_add_option(uci_ctx, "autonomy", "snow_melt_control", "weather_api_key", 
                       g_snow_melt_control.config.weather_api_key) != 0) {
        ret = AUTONOMY_ERROR_SYSTEM;
    }
    
    if (ucix_add_option(uci_ctx, "autonomy", "snow_melt_control", "starlink_host", 
                       g_snow_melt_control.config.starlink_host) != 0) {
        ret = AUTONOMY_ERROR_SYSTEM;
    }
    
    if (ucix_add_option_int(uci_ctx, "autonomy", "snow_melt_control", "starlink_port", 
                           g_snow_melt_control.config.starlink_port) != 0) {
        ret = AUTONOMY_ERROR_SYSTEM;
    }
    
    if (ucix_add_option_int(uci_ctx, "autonomy", "snow_melt_control", "debug_mode", 
                           g_snow_melt_control.config.debug_mode ? 1 : 0) != 0) {
        ret = AUTONOMY_ERROR_SYSTEM;
    }
    
    // Commit changes if all sets succeeded
    if (ret == AUTONOMY_SUCCESS) {
        if (ucix_logged_commit(uci_ctx, "autonomy") == 0) {
            LOGX_INFO_MSG("Snow melt control configuration saved to UCI");
            return AUTONOMY_SUCCESS;
        } else {
            LOGX_ERROR_MSG("Failed to commit snow melt control configuration to UCI");
            return AUTONOMY_ERROR_SYSTEM;
        }
    } else {
        LOGX_ERROR_MSG("Failed to set one or more snow melt control configuration values in UCI");
        return ret;
    }
}

// Get snow melt mode string for logging
const char* starlink_weather_snow_melt_mode_to_string(snow_melt_mode_t mode) {
    switch (mode) {
        case SNOW_MELT_OFF: return "OFF";
        case SNOW_MELT_AUTOMATIC: return "AUTOMATIC";
        case SNOW_MELT_PREHEAT: return "PREHEAT";
        case SNOW_MELT_MANUAL: return "MANUAL";
        default: return "UNKNOWN";
    }
}

// Get weather condition string for logging
const char* starlink_weather_snow_melt_weather_condition_to_string(weather_condition_t condition) {
    switch (condition) {
        case WEATHER_CLEAR: return "CLEAR";
        case WEATHER_CLOUDY: return "CLOUDY";
        case WEATHER_RAIN: return "RAIN";
        case WEATHER_HEAVY_RAIN: return "HEAVY_RAIN";
        case WEATHER_SNOW: return "SNOW";
        case WEATHER_HEAVY_SNOW: return "HEAVY_SNOW";
        case WEATHER_SLEET: return "SLEET";
        case WEATHER_FOG: return "FOG";
        case WEATHER_UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}
