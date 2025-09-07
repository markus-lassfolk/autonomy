#include "starlink_snow_detection.h"
#include "../starlink/starlink_comprehensive.h"
#include "../utils/logx.h"
#include "../utils/http_client_libcurl.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <libubus.h>
#include <libubox/blobmsg.h>

// Snow detection configuration
static const int SNOW_DETECTION_SAMPLES = 5;              // Samples needed for detection
static const double SNOW_OBSTRUCTION_THRESHOLD = 0.05;    // 5% obstruction increase
static const double SNOW_SNR_DEGRADATION_THRESHOLD = 0.02; // 2% SNR degradation
static const double SNOW_TEMPERATURE_THRESHOLD = 2.0;     // Below 2°C
static const int SNOW_VERIFICATION_TIME = 300;            // 5 minutes verification
static const int SNOW_MELT_TIMEOUT = 1800;                // 30 minutes max melt time

// Global snow detection state
static starlink_snow_detection_t g_snow_detection = {0};
static bool g_snow_detection_initialized = false;
static pthread_mutex_t g_snow_detection_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations
static bool is_winter_season(void);
static bool is_rv_stationary(void);
static bool check_snow_forecast(void);
static double get_ambient_temperature(void);
static double get_humidity(void);
static void calculate_obstruction_rates(const starlink_obstruction_sample_t *sample);
static snow_action_t determine_snow_action(void);
static int execute_snow_action(snow_action_t action);
static int start_dish_heating(void);
static int stop_dish_heating(void);
static int verify_obstruction_cleared(void);

// Initialize snow detection system
int starlink_snow_detection_init(void) {
    if (g_snow_detection_initialized) {
        LOGX_WARN_MSG("Snow detection system already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_snow_detection_mutex);
    
    // Initialize snow detection state
    memset(&g_snow_detection, 0, sizeof(starlink_snow_detection_t));
    g_snow_detection.enabled = true;
    g_snow_detection.detection_samples = SNOW_DETECTION_SAMPLES;
    g_snow_detection.obstruction_threshold = SNOW_OBSTRUCTION_THRESHOLD;
    g_snow_detection.snr_degradation_threshold = SNOW_SNR_DEGRADATION_THRESHOLD;
    g_snow_detection.temperature_threshold = SNOW_TEMPERATURE_THRESHOLD;
    g_snow_detection.verification_time = SNOW_VERIFICATION_TIME;
    g_snow_detection.melt_timeout = SNOW_MELT_TIMEOUT;
    // Initialize with empty API key - will be loaded from UCI
    memset(g_snow_detection.weather_api_key, 0, sizeof(g_snow_detection.weather_api_key));
    
    g_snow_detection.is_heating_active = false;
    g_snow_detection.last_clear_time = time(NULL);
    g_snow_detection.consecutive_obstruction_samples = 0;
    g_snow_detection.total_detections = 0;
    g_snow_detection.successful_melts = 0;
    g_snow_detection.false_positives = 0;
    g_snow_detection.prewarm_actions = 0;
    g_snow_detection.melt_actions = 0;
    g_snow_detection.verify_actions = 0;
    g_snow_detection.average_melt_time = 0.0;
    g_snow_detection.detection_accuracy = 0.0;
    g_snow_detection.last_detection = 0;
    g_snow_detection.last_successful_melt = 0;
    
    // Initialize sample history
    for (int i = 0; i < MAX_SNOW_SAMPLES; i++) {
        g_snow_detection.sample_history[i].timestamp = 0;
        g_snow_detection.sample_history[i].fraction_obstructed = 0.0;
        g_snow_detection.sample_history[i].snr = 0.0;
        g_snow_detection.sample_history[i].temperature = 0.0;
        g_snow_detection.sample_history[i].humidity = 0.0;
    }
    g_snow_detection.sample_count = 0;
    
    g_snow_detection_initialized = true;
    pthread_mutex_unlock(&g_snow_detection_mutex);
    
    LOGX_INFO_MSG("Snow detection system initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Process obstruction sample for snow detection
int starlink_snow_detection_process_sample(const starlink_obstruction_sample_t *sample) {
    if (!g_snow_detection_initialized || !sample) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_snow_detection_mutex);
    
    // Add sample to history
    int sample_index = g_snow_detection.sample_count % MAX_SNOW_SAMPLES;
    g_snow_detection.sample_history[sample_index].timestamp = sample->timestamp;
    g_snow_detection.sample_history[sample_index].fraction_obstructed = sample->fraction_obstructed;
    g_snow_detection.sample_history[sample_index].snr = sample->snr;
    g_snow_detection.sample_history[sample_index].temperature = get_ambient_temperature();
    g_snow_detection.sample_history[sample_index].humidity = get_humidity();
    
    if (g_snow_detection.sample_count < MAX_SNOW_SAMPLES) {
        g_snow_detection.sample_count++;
    }
    
    // Calculate obstruction and SNR rates
    calculate_obstruction_rates(sample);
    
    // Update context
    g_snow_detection.context.is_stationary = is_rv_stationary();
    g_snow_detection.context.is_winter_season = is_winter_season();
    g_snow_detection.context.snow_forecast_active = check_snow_forecast();
    g_snow_detection.context.temperature = get_ambient_temperature();
    g_snow_detection.context.humidity = get_humidity();
    g_snow_detection.context.last_clear_time = g_snow_detection.last_clear_time;
    g_snow_detection.context.consecutive_obstruction_samples = g_snow_detection.consecutive_obstruction_samples;
    
    // Determine snow action
    snow_action_t action = determine_snow_action();
    
    // Execute action if needed
    if (action != SNOW_ACTION_NONE) {
        int result = execute_snow_action(action);
        if (result == AUTONOMY_SUCCESS) {
            LOGX_INFO_MSG("Snow action executed successfully", "action", action);
        } else {
            LOGX_ERROR_MSG("Failed to execute snow action", "action", action, "result", result);
        }
    }
    
    pthread_mutex_unlock(&g_snow_detection_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Check if it's winter season
static bool is_winter_season(void) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    // Winter months (Dec-Feb in northern hemisphere)
    return (tm_info->tm_mon == 11 || tm_info->tm_mon == 0 || tm_info->tm_mon == 1);
}

// Check if RV is stationary
static bool is_rv_stationary(void) {
    // Use comprehensive GPS collection to determine movement based on speed and accuracy
    starlink_comprehensive_gps_t gps = {0};
    if (starlink_comprehensive_collect_gps(&gps) == AUTONOMY_SUCCESS && gps.valid) {
        // If accuracy is poor, be conservative and consider stationary only when speed is very low
        double speed_threshold = (gps.accuracy <= 20.0) ? 0.5 : 0.2; // m/s
        return fabs(gps.horizontal_speed_mps) <= speed_threshold;
    }

    // Fallback: infer from obstruction variance
    if (g_snow_detection.sample_count < 3) {
        return true;
    }
    double obstruction_variance = 0.0;
    double mean_obstruction = 0.0;
    int valid_samples = 0;
    for (int i = 0; i < g_snow_detection.sample_count && i < MAX_SNOW_SAMPLES; i++) {
        if (g_snow_detection.sample_history[i].timestamp > 0) {
            mean_obstruction += g_snow_detection.sample_history[i].fraction_obstructed;
            valid_samples++;
        }
    }
    if (valid_samples < 3) {
        return true;
    }
    mean_obstruction /= valid_samples;
    for (int i = 0; i < g_snow_detection.sample_count && i < MAX_SNOW_SAMPLES; i++) {
        if (g_snow_detection.sample_history[i].timestamp > 0) {
            double diff = g_snow_detection.sample_history[i].fraction_obstructed - mean_obstruction;
            obstruction_variance += diff * diff;
        }
    }
    obstruction_variance /= valid_samples;
    return (obstruction_variance < 0.01);
}

// Enhanced snow forecast algorithm
static bool check_snow_forecast(void) {
    double temp = get_ambient_temperature();
    double humidity = get_humidity();

    // If we don't have valid weather data, use fallback logic
    if (temp <= -100.0 || humidity < 0.0) {
        LOGX_WARN_MSG("Invalid weather data for snow forecast, using fallback");
        return (temp < 2.0 && humidity > 80.0);
    }

    // Enhanced snow prediction algorithm
    double snow_probability = 0.0;

    // Base conditions: temperature and humidity
    if (temp < -5.0) {
        snow_probability += 0.4; // Very cold - high snow potential
    } else if (temp < 0.0) {
        snow_probability += 0.3; // Freezing - good snow potential
    } else if (temp < 2.0) {
        snow_probability += 0.2; // Near freezing - moderate snow potential
    } else if (temp < 5.0) {
        snow_probability += 0.1; // Cool but above freezing - low snow potential
    }

    // Humidity factor
    if (humidity > 90.0) {
        snow_probability += 0.4; // Very humid - high precipitation potential
    } else if (humidity > 80.0) {
        snow_probability += 0.3; // Humid - good precipitation potential
    } else if (humidity > 70.0) {
        snow_probability += 0.2; // Moderately humid
    } else if (humidity > 60.0) {
        snow_probability += 0.1; // Dry conditions
    }

    // Wind chill factor (estimated)
    double wind_chill_factor = 0.0;
    if (temp < 5.0) {
        // Estimate wind chill effect - colder temperatures are more affected
        wind_chill_factor = (5.0 - temp) * 0.1;
        snow_probability += wind_chill_factor;
    }

    // Seasonal adjustment (winter months increase probability)
    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);
    int month = local_time->tm_mon + 1; // tm_mon is 0-based

    // Winter months: December, January, February, March
    if (month == 12 || month == 1 || month == 2 || month == 3) {
        snow_probability += 0.2; // 20% bonus for winter season
    } else if (month == 11 || month == 4) { // Shoulder seasons
        snow_probability += 0.1; // 10% bonus for shoulder seasons
    }

    // Altitude consideration (higher altitudes increase snow probability)
    // This would be enhanced with actual altitude data from GPS
    if (temp < 0.0) {
        snow_probability += 0.1; // Bonus for freezing temperatures at any altitude
    }

    // Temperature trend consideration (rapid cooling increases snow potential)
    static double last_temp = 0.0;
    static time_t last_temp_time = 0;
    time_t current_time = time(NULL);

    if (last_temp_time > 0 && (current_time - last_temp_time) < 3600) { // Within last hour
        double temp_drop = last_temp - temp;
        if (temp_drop > 5.0) { // Rapid temperature drop
            snow_probability += 0.2;
        } else if (temp_drop > 2.0) { // Moderate temperature drop
            snow_probability += 0.1;
        }
    }

    last_temp = temp;
    last_temp_time = current_time;

    // Decision threshold: 60% probability or higher indicates snow risk
    bool snow_risk = (snow_probability >= 0.6);

    LOGX_DEBUG_MSG("Snow forecast analysis",
                   "temperature", temp,
                   "humidity", humidity,
                   "probability", snow_probability,
                   "snow_risk", snow_risk);

    return snow_risk;
}

// Get ambient temperature
static double get_ambient_temperature(void) {
    // Get current GPS location from Starlink
    starlink_comprehensive_gps_t gps_data;
    if (starlink_comprehensive_collect_gps(&gps_data) == AUTONOMY_SUCCESS &&
        gps_data.valid && gps_data.confidence > 0.5) {

        // Try to get temperature from weather API using real GPS coordinates
        if (strlen(g_snow_detection.weather_api_key) > 0) {
            char weather_cmd[512];
            char weather_temp[32];

            // Use HTTP client instead of system command for security
            char weather_url[512];
            snprintf(weather_url, sizeof(weather_url),
                     "http://api.openweathermap.org/data/2.5/weather?lat=%.6f&lon=%.6f&appid=%s&units=metric",
                     gps_data.latitude, gps_data.longitude, g_snow_detection.weather_api_key);
            
            http_request_t* request = http_request_create(weather_url, HTTP_METHOD_GET);
            if (request) {
                http_response_t* response = http_request(request);
                if (response && response->success && response->data) {
                    // Parse temperature from JSON response
                    char* temp_start = strstr(response->data, "\"temp\":");
                    if (temp_start) {
                        double temp = atof(temp_start + 7); // Skip "temp":
                        if (temp > -50.0 && temp < 60.0) { // Sanity check
                            http_response_free(response);
                            http_request_free(request);
                            return temp;
                        }
                    }
                    http_response_free(response);
                }
                http_request_free(request);
            }
        } else {
            LOGX_WARN_MSG("Weather API key not configured, skipping weather API temperature lookup");
        }
    } else {
        LOGX_WARN_MSG("Unable to get valid GPS coordinates from Starlink for weather API");
    }
    
    // Try to get temperature from system thermal sensors
    FILE *temp_fp = popen("cat /sys/class/thermal/thermal_zone*/temp 2>/dev/null | head -1", "r");
    if (temp_fp) {
        char temp_str[32];
        if (fgets(temp_str, sizeof(temp_str), temp_fp)) {
            double temp_cpu = atof(temp_str) / 1000.0; // Convert from millidegrees
            pclose(temp_fp);
            
            // Estimate ambient temperature (CPU temp is usually 20-30°C above ambient)
            double ambient = temp_cpu - 25.0;
            if (ambient > -50.0 && ambient < 60.0) { // Sanity check
                return ambient;
            }
        }
        pclose(temp_fp);
    }
    
    // Try to get temperature from UCI configuration
    FILE *uci_fp = popen("uci get autonomy.snow_detection.temperature_override 2>/dev/null", "r");
    if (uci_fp) {
        char temp_str[32];
        if (fgets(temp_str, sizeof(temp_str), uci_fp)) {
            pclose(uci_fp);
            double temp = atof(temp_str);
            if (temp > -50.0 && temp < 60.0) { // Sanity check
                return temp;
            }
        } else {
            pclose(uci_fp);
        }
    }
    
    // Fallback: return unknown temperature (will be handled by detection logic)
    LOGX_WARN_MSG("Unable to determine ambient temperature, using fallback");
    return 0.0; // Unknown temperature
}

// Get humidity
static double get_humidity(void) {
    // Get current GPS location from Starlink
    starlink_comprehensive_gps_t gps_data;
    if (starlink_comprehensive_collect_gps(&gps_data) == AUTONOMY_SUCCESS &&
        gps_data.valid && gps_data.confidence > 0.5) {

        // Try to get humidity from weather API using real GPS coordinates
        if (strlen(g_snow_detection.weather_api_key) > 0) {
            char weather_cmd[512];
            char weather_humidity[32];

            // Use HTTP client instead of system command for security
            char weather_url[512];
            snprintf(weather_url, sizeof(weather_url),
                     "http://api.openweathermap.org/data/2.5/weather?lat=%.6f&lon=%.6f&appid=%s&units=metric",
                     gps_data.latitude, gps_data.longitude, g_snow_detection.weather_api_key);
            
            http_request_t* request = http_request_create(weather_url, HTTP_METHOD_GET);
            if (request) {
                http_response_t* response = http_request(request);
                if (response && response->success && response->data) {
                    // Parse humidity from JSON response
                    char* humidity_start = strstr(response->data, "\"humidity\":");
                    if (humidity_start) {
                        double humidity = atof(humidity_start + 11); // Skip "humidity":
                        if (humidity >= 0.0 && humidity <= 100.0) { // Sanity check
                            http_response_free(response);
                            http_request_free(request);
                            return humidity;
                        }
                    }
                    http_response_free(response);
                }
                http_request_free(request);
            }
        } else {
            LOGX_WARN_MSG("Weather API key not configured, skipping weather API humidity lookup");
        }
    } else {
        LOGX_WARN_MSG("Unable to get valid GPS coordinates from Starlink for weather API");
    }
    
    // Try to get humidity from UCI configuration
    FILE *uci_fp = popen("uci get autonomy.snow_detection.humidity_override 2>/dev/null", "r");
    if (uci_fp) {
        char humidity_str[32];
        if (fgets(humidity_str, sizeof(humidity_str), uci_fp)) {
            pclose(uci_fp);
            double humidity = atof(humidity_str);
            if (humidity >= 0.0 && humidity <= 100.0) { // Sanity check
                return humidity;
            }
        } else {
            pclose(uci_fp);
        }
    }
    
    // Fallback: return unknown humidity (will be handled by detection logic)
    LOGX_WARN_MSG("Unable to determine humidity, using fallback");
    return 50.0; // Default humidity
}

// Calculate obstruction and SNR rates
static void calculate_obstruction_rates(const starlink_obstruction_sample_t *sample) {
    if (g_snow_detection.sample_count < 2) {
        g_snow_detection.context.obstruction_increase_rate = 0.0;
        g_snow_detection.context.snr_degradation_rate = 0.0;
        return;
    }
    
    // Get previous sample
    int prev_index = (g_snow_detection.sample_count - 2) % MAX_SNOW_SAMPLES;
    snow_sample_t *prev_sample = &g_snow_detection.sample_history[prev_index];
    
    if (prev_sample->timestamp == 0) {
        g_snow_detection.context.obstruction_increase_rate = 0.0;
        g_snow_detection.context.snr_degradation_rate = 0.0;
        return;
    }
    
    // Calculate rates
    double time_diff = (double)(sample->timestamp - prev_sample->timestamp);
    if (time_diff > 0) {
        g_snow_detection.context.obstruction_increase_rate = 
            (sample->fraction_obstructed - prev_sample->fraction_obstructed) / time_diff;
        
        g_snow_detection.context.snr_degradation_rate = 
            (prev_sample->snr - sample->snr) / time_diff;
    }
    
    // Update consecutive obstruction samples
    if (sample->fraction_obstructed > g_snow_detection.obstruction_threshold) {
        g_snow_detection.consecutive_obstruction_samples++;
    } else {
        g_snow_detection.consecutive_obstruction_samples = 0;
        g_snow_detection.last_clear_time = sample->timestamp;
    }
}

// Determine snow action based on context
static snow_action_t determine_snow_action(void) {
    // Priority 1: Immediate obstruction detection (rapid snow accumulation)
    if (g_snow_detection.consecutive_obstruction_samples >= 3) {
        if (g_snow_detection.context.obstruction_increase_rate > g_snow_detection.obstruction_threshold) {
            if (g_snow_detection.context.is_stationary && 
                g_snow_detection.context.is_winter_season &&
                g_snow_detection.context.temperature < g_snow_detection.temperature_threshold) {
                
                LOGX_INFO_MSG("Rapid snow accumulation detected", 
                              "obstruction_rate", g_snow_detection.context.obstruction_increase_rate,
                              "consecutive_samples", g_snow_detection.consecutive_obstruction_samples);
                return SNOW_ACTION_MELT;
            }
        }
    }
    
    // Priority 2: Weather forecast + stationary + winter (proactive)
    if (g_snow_detection.context.snow_forecast_active && 
        g_snow_detection.context.is_stationary && 
        g_snow_detection.context.is_winter_season &&
        g_snow_detection.context.temperature < g_snow_detection.temperature_threshold &&
        !g_snow_detection.is_heating_active) {
        
        LOGX_INFO_MSG("Snow forecast detected, starting pre-warming", 
                      "temperature", g_snow_detection.context.temperature,
                      "humidity", g_snow_detection.context.humidity);
        return SNOW_ACTION_PREWARM;
    }
    
    // Priority 3: Gradual degradation pattern
    if (g_snow_detection.context.snr_degradation_rate > g_snow_detection.snr_degradation_threshold &&
        g_snow_detection.context.is_stationary &&
        g_snow_detection.context.is_winter_season &&
        g_snow_detection.context.temperature < g_snow_detection.temperature_threshold) {
        
        LOGX_INFO_MSG("Gradual SNR degradation detected, verifying obstruction type", 
                      "snr_degradation_rate", g_snow_detection.context.snr_degradation_rate);
        return SNOW_ACTION_VERIFY;
    }
    
    // Priority 4: Stop heating if obstruction cleared
    if (g_snow_detection.is_heating_active && 
        g_snow_detection.consecutive_obstruction_samples == 0) {
        
        LOGX_INFO_MSG("Obstruction cleared, stopping heating");
        return SNOW_ACTION_CLEANUP;
    }
    
    return SNOW_ACTION_NONE;
}

// Execute snow action
static int execute_snow_action(snow_action_t action) {
    switch (action) {
        case SNOW_ACTION_PREWARM:
            return start_dish_heating();
            
        case SNOW_ACTION_MELT:
            g_snow_detection.total_detections++;
            return start_dish_heating();
            
        case SNOW_ACTION_VERIFY:
            // Wait for verification period
            sleep(g_snow_detection.verification_time);
            // Re-check obstruction
            if (g_snow_detection.consecutive_obstruction_samples >= 2) {
                return start_dish_heating();
            }
            break;
            
        case SNOW_ACTION_CLEANUP:
            g_snow_detection.successful_melts++;
            return stop_dish_heating();
            
        default:
            break;
    }
    
    return AUTONOMY_SUCCESS;
}

// Start dish heating
static int start_dish_heating(void) {
    if (g_snow_detection.is_heating_active) {
        return AUTONOMY_SUCCESS; // Already heating
    }
    
    LOGX_INFO_MSG("Starting dish heating system");
    
    // Try to control actual dish heating hardware
    // First, try UCI-configured heating command
    char heating_cmd[256];
    FILE *uci_fp = popen("uci get autonomy.snow_detection.heating_command 2>/dev/null", "r");
    if (uci_fp && fgets(heating_cmd, sizeof(heating_cmd), uci_fp)) {
        pclose(uci_fp);
        
        // Remove newline
        char *newline = strchr(heating_cmd, '\n');
        if (newline) *newline = '\0';
        
        // Execute custom heating command
        int result = system(heating_cmd);
        if (result == 0) {
            LOGX_INFO_MSG("Custom heating command executed successfully");
        } else {
            LOGX_WARN_MSG("Custom heating command failed",
                         "command", heating_cmd,
                         "result", result,
                         "error_code", WEXITSTATUS(result),
                         "action", "Heating system activation failed - will try fallback methods");
        }
    } else {
        if (uci_fp) pclose(uci_fp);
        
        // Fallback: try standard heating control methods
        // Method 1: GPIO control (if available)
        FILE *gpio_fp = popen("echo 1 > /sys/class/gpio/gpio18/value 2>/dev/null", "r");
        if (gpio_fp) {
            pclose(gpio_fp);
            LOGX_INFO_MSG("GPIO heating control activated");
        } else {
            // Method 2: Real hardware control via UCI/UBUS
            struct ubus_context* ctx = ubus_connect(NULL);
            if (ctx) {
                uint32_t id;
                int ret = ubus_lookup_id(ctx, "starlink.dish", &id);
                if (ret == 0) {
                    struct blob_buf bb = {0};
                    blob_buf_init(&bb, 0);
                    blobmsg_add_string(&bb, "action", "heater_on");
                    blobmsg_add_u32(&bb, "temperature", g_snow_detection.current_temperature);
                    
                    ret = ubus_invoke(ctx, id, "control_heater", bb.head, NULL, NULL, 1000);
                    if (ret == 0) {
                        LOGX_INFO_MSG("Starlink dish heater activated via UBUS");
                    } else {
                        LOGX_WARN_MSG("Failed to activate heater via UBUS", "error", ret);
                        ubus_free(ctx);
                        blob_buf_free(&bb);
                        return AUTONOMY_ERROR_OPERATION_FAILED;
                    }
                    
                    blob_buf_free(&bb);
                } else {
                    LOGX_WARN_MSG("Starlink dish UBUS service not found");
                }
                ubus_free(ctx);
            } else {
                // Fallback: Direct GPIO control
                FILE *gpio_fp = popen("echo 1 > /sys/class/gpio/gpio18/value 2>/dev/null", "r");
                if (gpio_fp) {
                    pclose(gpio_fp);
                    LOGX_INFO_MSG("Starlink dish heater activated via GPIO");
                } else {
                    LOGX_WARN_MSG("Failed to activate heating system via all methods");
                    return AUTONOMY_ERROR_OPERATION_FAILED;
                }
            }
        }
    }
    
    g_snow_detection.is_heating_active = true;
    g_snow_detection.heating_start_time = time(NULL);
    
    // Set timeout for heating
    g_snow_detection.heating_timeout = g_snow_detection.heating_start_time + g_snow_detection.melt_timeout;
    
    return AUTONOMY_SUCCESS;
}

// Stop dish heating
static int stop_dish_heating(void) {
    if (!g_snow_detection.is_heating_active) {
        return AUTONOMY_SUCCESS; // Already stopped
    }
    
    LOGX_INFO_MSG("Stopping dish heating system");
    
    // Try to control actual dish heating hardware
    // First, try UCI-configured heating stop command
    char heating_cmd[256];
    FILE *uci_fp = popen("uci get autonomy.snow_detection.heating_stop_command 2>/dev/null", "r");
    if (uci_fp && fgets(heating_cmd, sizeof(heating_cmd), uci_fp)) {
        pclose(uci_fp);
        
        // Remove newline
        char *newline = strchr(heating_cmd, '\n');
        if (newline) *newline = '\0';
        
        // Execute custom heating stop command
        int result = system(heating_cmd);
        if (result == 0) {
            LOGX_INFO_MSG("Custom heating stop command executed successfully");
        } else {
            LOGX_WARN_MSG("Custom heating stop command failed",
                         "command", heating_cmd,
                         "result", result,
                         "error_code", WEXITSTATUS(result),
                         "action", "Heating system deactivation failed - will try fallback methods");
        }
    } else {
        if (uci_fp) pclose(uci_fp);
        
        // Fallback: try standard heating control methods
        // Method 1: GPIO control (if available)
        FILE *gpio_fp = popen("echo 0 > /sys/class/gpio/gpio18/value 2>/dev/null", "r");
        if (gpio_fp) {
            pclose(gpio_fp);
            LOGX_INFO_MSG("GPIO heating control deactivated");
        } else {
            // Method 2: Real hardware control via UCI/UBUS
            struct ubus_context* ctx = ubus_connect(NULL);
            if (ctx) {
                uint32_t id;
                int ret = ubus_lookup_id(ctx, "starlink.dish", &id);
                if (ret == 0) {
                    struct blob_buf bb = {0};
                    blob_buf_init(&bb, 0);
                    blobmsg_add_string(&bb, "action", "heater_off");
                    blobmsg_add_u32(&bb, "temperature", g_snow_detection.current_temperature);
                    
                    ret = ubus_invoke(ctx, id, "control_heater", bb.head, NULL, NULL, 1000);
                    if (ret == 0) {
                        LOGX_INFO_MSG("Starlink dish heater deactivated via UBUS");
                    } else {
                        LOGX_WARN_MSG("Failed to deactivate heater via UBUS", "error", ret);
                        ubus_free(ctx);
                        blob_buf_free(&bb);
                        return AUTONOMY_ERROR_OPERATION_FAILED;
                    }
                    
                    blob_buf_free(&bb);
                } else {
                    LOGX_WARN_MSG("Starlink dish UBUS service not found");
                }
                ubus_free(ctx);
            } else {
                // Fallback: Direct GPIO control
                FILE *gpio_fp = popen("echo 0 > /sys/class/gpio/gpio18/value 2>/dev/null", "r");
                if (gpio_fp) {
                    pclose(gpio_fp);
                    LOGX_INFO_MSG("Starlink dish heater deactivated via GPIO");
                } else {
                    LOGX_WARN_MSG("Failed to deactivate heating system via all methods");
                    return AUTONOMY_ERROR_OPERATION_FAILED;
                }
            }
        }
    }
    
    g_snow_detection.is_heating_active = false;
    g_snow_detection.heating_duration = time(NULL) - g_snow_detection.heating_start_time;
    
    LOGX_INFO_MSG("Dish heating stopped", "duration_seconds", g_snow_detection.heating_duration);
    
    return AUTONOMY_SUCCESS;
}

// Verify obstruction cleared
static int verify_obstruction_cleared(void) {
    // Re-check current obstruction using Starlink status
    starlink_status_response_t status = {0};
    int rc = starlink_get_status(&status);
    if (rc != 0) {
        LOGX_WARN_MSG("verify_obstruction_cleared: failed to query Starlink status", "result", rc);
        // Fall back to internal counter as best effort
        return (g_snow_detection.consecutive_obstruction_samples == 0) ? AUTONOMY_SUCCESS : AUTONOMY_ERROR_NOT_FOUND;
    }

    double frac = status.obstruction_stats.fraction_obstructed;
    bool obstructed_now = status.obstruction_stats.currently_obstructed || (frac > g_snow_detection.obstruction_threshold);
    return obstructed_now ? AUTONOMY_ERROR_NOT_FOUND : AUTONOMY_SUCCESS;
}

// Get snow detection status
int starlink_snow_detection_get_status(starlink_snow_detection_status_t *status) {
    if (!g_snow_detection_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_snow_detection_mutex);
    
    status->enabled = g_snow_detection.enabled;
    status->is_heating_active = g_snow_detection.is_heating_active;
    status->consecutive_obstruction_samples = g_snow_detection.consecutive_obstruction_samples;
    status->total_detections = g_snow_detection.total_detections;
    status->successful_melts = g_snow_detection.successful_melts;
    status->false_positives = g_snow_detection.false_positives;
    status->last_clear_time = g_snow_detection.last_clear_time;
    status->heating_start_time = g_snow_detection.heating_start_time;
    status->heating_duration = g_snow_detection.heating_duration;
    
    // Copy context
    memcpy(&status->context, &g_snow_detection.context, sizeof(snow_detection_context_t));
    
    pthread_mutex_unlock(&g_snow_detection_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get snow detection configuration
int starlink_snow_detection_get_config(starlink_snow_detection_config_t *config) {
    if (!g_snow_detection_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_snow_detection_mutex);
    
    config->enabled = g_snow_detection.enabled;
    config->detection_samples = g_snow_detection.detection_samples;
    config->obstruction_threshold = g_snow_detection.obstruction_threshold;
    config->snr_degradation_threshold = g_snow_detection.snr_degradation_threshold;
    config->temperature_threshold = g_snow_detection.temperature_threshold;
    config->verification_time = g_snow_detection.verification_time;
    config->melt_timeout = g_snow_detection.melt_timeout;
    
    pthread_mutex_unlock(&g_snow_detection_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set snow detection configuration
int starlink_snow_detection_set_config(const starlink_snow_detection_config_t *config) {
    if (!g_snow_detection_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_snow_detection_mutex);
    
    g_snow_detection.enabled = config->enabled;
    g_snow_detection.detection_samples = config->detection_samples;
    g_snow_detection.obstruction_threshold = config->obstruction_threshold;
    g_snow_detection.snr_degradation_threshold = config->snr_degradation_threshold;
    g_snow_detection.temperature_threshold = config->temperature_threshold;
    g_snow_detection.verification_time = config->verification_time;
    g_snow_detection.melt_timeout = config->melt_timeout;
    
    pthread_mutex_unlock(&g_snow_detection_mutex);
    
    LOGX_INFO_MSG("Snow detection configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable snow detection
int starlink_snow_detection_set_enabled(bool enabled) {
    if (!g_snow_detection_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_snow_detection_mutex);
    g_snow_detection.enabled = enabled;
    pthread_mutex_unlock(&g_snow_detection_mutex);
    
    LOGX_INFO_MSG("Snow detection system %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Force snow detection check
int starlink_snow_detection_force_check(void) {
    if (!g_snow_detection_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_snow_detection_mutex);
    
    snow_action_t action = determine_snow_action();
    if (action != SNOW_ACTION_NONE) {
        execute_snow_action(action);
    }
    
    pthread_mutex_unlock(&g_snow_detection_mutex);
    
    LOGX_INFO_MSG("Snow detection force check completed", "action", action);
    return AUTONOMY_SUCCESS;
}

// Start heating manually
int starlink_snow_detection_start_heating_manual(void) {
    if (!g_snow_detection_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_snow_detection_mutex);
    
    int result = start_dish_heating();
    if (result == AUTONOMY_SUCCESS) {
        g_snow_detection.total_detections++;
        LOGX_INFO_MSG("Manual heating started");
    }
    
    pthread_mutex_unlock(&g_snow_detection_mutex);
    
    return result;
}

// Stop heating manually
int starlink_snow_detection_stop_heating_manual(void) {
    if (!g_snow_detection_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_snow_detection_mutex);
    
    int result = stop_dish_heating();
    if (result == AUTONOMY_SUCCESS) {
        LOGX_INFO_MSG("Manual heating stopped");
    }
    
    pthread_mutex_unlock(&g_snow_detection_mutex);
    
    return result;
}

// Get statistics
int starlink_snow_detection_get_statistics(starlink_snow_detection_stats_t *stats) {
    if (!g_snow_detection_initialized || !stats) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_snow_detection_mutex);
    
    stats->total_detections = g_snow_detection.total_detections;
    stats->successful_melts = g_snow_detection.successful_melts;
    stats->false_positives = g_snow_detection.false_positives;
    stats->prewarm_actions = g_snow_detection.prewarm_actions;
    stats->melt_actions = g_snow_detection.melt_actions;
    stats->verify_actions = g_snow_detection.verify_actions;
    stats->average_melt_time = g_snow_detection.average_melt_time;
    stats->detection_accuracy = g_snow_detection.detection_accuracy;
    stats->last_detection = g_snow_detection.last_detection;
    stats->last_successful_melt = g_snow_detection.last_successful_melt;
    
    pthread_mutex_unlock(&g_snow_detection_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Reset statistics
int starlink_snow_detection_reset_statistics(void) {
    if (!g_snow_detection_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_snow_detection_mutex);
    
    g_snow_detection.total_detections = 0;
    g_snow_detection.successful_melts = 0;
    g_snow_detection.false_positives = 0;
    g_snow_detection.prewarm_actions = 0;
    g_snow_detection.melt_actions = 0;
    g_snow_detection.verify_actions = 0;
    g_snow_detection.average_melt_time = 0.0;
    g_snow_detection.detection_accuracy = 0.0;
    g_snow_detection.last_detection = 0;
    g_snow_detection.last_successful_melt = 0;
    
    pthread_mutex_unlock(&g_snow_detection_mutex);
    
    LOGX_INFO_MSG("Snow detection statistics reset");
    return AUTONOMY_SUCCESS;
}

// Load configuration from UCI
int starlink_snow_detection_load_uci_config(void) {
    if (!g_snow_detection_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_snow_detection_mutex);
    
    // Load UCI configuration
    FILE *uci_fp = popen("uci show autonomy.snow_detection 2>/dev/null", "r");
    if (!uci_fp) {
        LOGX_WARN_MSG("Failed to load UCI configuration, using defaults");
        pthread_mutex_unlock(&g_snow_detection_mutex);
        return AUTONOMY_SUCCESS;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), uci_fp)) {
        // Parse UCI output format: autonomy.snow_detection.enabled='1'
        char *key = strtok(line, "='");
        char *value = strtok(NULL, "'");
        
        if (!key || !value) continue;
        
        // Extract the option name
        char *option = strrchr(key, '.');
        if (!option) continue;
        option++; // Skip the dot
        
        // Parse configuration values
        if (strcmp(option, "enabled") == 0) {
            g_snow_detection.enabled = (strcmp(value, "1") == 0);
        } else if (strcmp(option, "detection_samples") == 0) {
            g_snow_detection.detection_samples = atoi(value);
        } else if (strcmp(option, "obstruction_threshold") == 0) {
            g_snow_detection.obstruction_threshold = atof(value);
        } else if (strcmp(option, "snr_degradation_threshold") == 0) {
            g_snow_detection.snr_degradation_threshold = atof(value);
        } else if (strcmp(option, "temperature_threshold") == 0) {
            g_snow_detection.temperature_threshold = atof(value);
        } else if (strcmp(option, "verification_time") == 0) {
            g_snow_detection.verification_time = atoi(value);
        } else if (strcmp(option, "melt_timeout") == 0) {
            g_snow_detection.melt_timeout = atoi(value);
        } else if (strcmp(option, "weather_api_key") == 0) {
            strncpy(g_snow_detection.weather_api_key, value, sizeof(g_snow_detection.weather_api_key) - 1);
        }
    }
    
    pclose(uci_fp);
    
    pthread_mutex_unlock(&g_snow_detection_mutex);
    
    LOGX_INFO_MSG("UCI configuration loaded successfully");
    return AUTONOMY_SUCCESS;
}

// Save configuration to UCI
int starlink_snow_detection_save_uci_config(void) {
    if (!g_snow_detection_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_snow_detection_mutex);
    
    // Build UCI commands
    char uci_cmd[1024];
    snprintf(uci_cmd, sizeof(uci_cmd), 
             "uci set autonomy.snow_detection.enabled='%d' && "
             "uci set autonomy.snow_detection.detection_samples='%d' && "
             "uci set autonomy.snow_detection.obstruction_threshold='%.3f' && "
             "uci set autonomy.snow_detection.snr_degradation_threshold='%.3f' && "
             "uci set autonomy.snow_detection.temperature_threshold='%.1f' && "
             "uci set autonomy.snow_detection.verification_time='%d' && "
             "uci set autonomy.snow_detection.melt_timeout='%d' && "
             "uci set autonomy.snow_detection.weather_api_key='%s' && "
             "uci commit autonomy",
             g_snow_detection.enabled ? 1 : 0,
             g_snow_detection.detection_samples,
             g_snow_detection.obstruction_threshold,
             g_snow_detection.snr_degradation_threshold,
             g_snow_detection.temperature_threshold,
             g_snow_detection.verification_time,
             g_snow_detection.melt_timeout,
             g_snow_detection.weather_api_key);
    
    pthread_mutex_unlock(&g_snow_detection_mutex);
    
    // Execute UCI commands
    int result = system(uci_cmd);
    if (result != 0) {
        LOGX_ERROR_MSG("Failed to save UCI configuration",
                      "command", uci_cmd,
                      "result", result,
                      "error_code", WEXITSTATUS(result),
                      "action", "Configuration may not persist across reboots");
        return AUTONOMY_ERROR_OPERATION_FAILED;
    }
    
    LOGX_INFO_MSG("UCI configuration saved successfully");
    return AUTONOMY_SUCCESS;
}

// Cleanup snow detection system
void starlink_snow_detection_cleanup(void) {
    if (!g_snow_detection_initialized) {
        return;
    }
    
    // Stop heating if active
    if (g_snow_detection.is_heating_active) {
        stop_dish_heating();
    }
    
    pthread_mutex_destroy(&g_snow_detection_mutex);
    g_snow_detection_initialized = false;
    
    LOGX_INFO_MSG("Snow detection system cleaned up");
}
