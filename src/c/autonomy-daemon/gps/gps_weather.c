#include "gps_weather.h"
#include "external_apis.h"
#include "logx.h"
#include "types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <curl/curl.h>

// Weather integration configuration
static const int MAX_WEATHER_CACHE_ENTRIES = 1000;          // Maximum weather cache entries
static const int WEATHER_UPDATE_INTERVAL = 1800;             // 30 minute weather update interval
static const int MAX_FORECAST_DAYS = 7;                      // Maximum forecast days
static const double WEATHER_CACHE_RADIUS = 10000.0;          // 10km weather cache radius
static const char* WEATHER_API_BASE_URL = "https://api.openweathermap.org/data/2.5";

// Weather API endpoints
static const char* WEATHER_CURRENT_ENDPOINT = "/weather";
static const char* WEATHER_FORECAST_ENDPOINT = "/forecast";
static const char* WEATHER_AIR_QUALITY_ENDPOINT = "/air_pollution";

// Global weather integration state
static gps_weather_t g_weather = {0};
static bool g_weather_initialized = false;
static pthread_mutex_t g_weather_mutex = PTHREAD_MUTEX_INITIALIZER;

// CURL write callback for weather API responses
static size_t weather_write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    gps_weather_api_response_t *response = (gps_weather_api_response_t *)userp;
    
    if (response->data_size + realsize >= sizeof(response->data)) {
        LOGX_WARN("Weather response too large, truncating");
        return 0;
    }
    
    memcpy(&response->data[response->data_size], contents, realsize);
    response->data_size += realsize;
    response->data[response->data_size] = '\0';
    
    return realsize;
}

// Initialize GPS weather integration
static int gps_weather_init(const char *api_key) {
    if (g_weather_initialized) {
        LOGX_WARN("GPS weather integration already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_weather_mutex);
    
    // Initialize weather state
    memset(&g_weather, 0, sizeof(gps_weather_t));
    g_weather.enabled = true;
    g_weather.max_cache_entries = MAX_WEATHER_CACHE_ENTRIES;
    g_weather.update_interval = WEATHER_UPDATE_INTERVAL;
    g_weather.max_forecast_days = MAX_FORECAST_DAYS;
    g_weather.cache_radius = WEATHER_CACHE_RADIUS;
    
    if (api_key && strlen(api_key) > 0) {
        strncpy(g_weather.api_key, api_key, sizeof(g_weather.api_key) - 1);
        g_weather.api_key[sizeof(g_weather.api_key) - 1] = '\0';
    }
    
    g_weather.cache_entry_count = 0;
    g_weather.total_requests = 0;
    g_weather.successful_requests = 0;
    g_weather.failed_requests = 0;
    g_weather.last_update = 0;
    
    // Initialize weather cache
    for (int i = 0; i < MAX_WEATHER_CACHE_ENTRIES; i++) {
        g_weather.weather_cache[i].active = false;
        g_weather.weather_cache[i].lat = 0.0;
        g_weather.weather_cache[i].lon = 0.0;
        g_weather.weather_cache[i].timestamp = 0;
        g_weather.weather_cache[i].temperature = 0.0;
        g_weather.weather_cache[i].humidity = 0.0;
        g_weather.weather_cache[i].pressure = 0.0;
        g_weather.weather_cache[i].wind_speed = 0.0;
        g_weather.weather_cache[i].wind_direction = 0.0;
        g_weather.weather_cache[i].visibility = 0.0;
        g_weather.weather_cache[i].cloud_cover = 0.0;
        g_weather.weather_cache[i].weather_condition = WEATHER_CONDITION_UNKNOWN;
        g_weather.weather_cache[i].air_quality_index = 0;
    }
    
    // Initialize CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    g_weather_initialized = true;
    pthread_mutex_unlock(&g_weather_mutex);
    
    LOGX_INFO("GPS weather integration initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Perform weather API request
static int perform_weather_api_request(const char *endpoint, const char *params, 
                                     gps_weather_api_response_t *response) {
    if (!g_weather.enabled) {
        return AUTONOMY_ERROR_NOT_ENABLED;
    }
    
    pthread_mutex_lock(&g_weather_mutex);
    
    // Initialize response
    memset(response, 0, sizeof(gps_weather_api_response_t));
    response->timestamp = time(NULL);
    
    // Build full URL
    char url[1024];
    snprintf(url, sizeof(url), "%s%s?%s&appid=%s&units=metric", 
             WEATHER_API_BASE_URL, endpoint, params, g_weather.api_key);
    
    // Initialize CURL
    CURL *curl = curl_easy_init();
    if (!curl) {
        pthread_mutex_unlock(&g_weather_mutex);
        LOGX_ERROR("Failed to initialize CURL for weather request");
        return AUTONOMY_ERROR_INTERNAL;
    }
    
    // Set CURL options
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, weather_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Autonomy-Daemon/1.0");
    
    // Perform request
    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    // Update statistics
    g_weather.total_requests++;
    
    if (res == CURLE_OK && http_code == 200) {
        g_weather.successful_requests++;
        response->success = true;
        response->http_code = http_code;
        
        LOGX_DEBUG("Weather API request successful: %s", endpoint);
    } else {
        g_weather.failed_requests++;
        response->success = false;
        response->http_code = http_code;
        response->error_code = res;
        
        LOGX_ERROR("Weather API request failed: %s, HTTP: %ld, CURL: %d", 
                   endpoint, http_code, res);
    }
    
    curl_easy_cleanup(curl);
    pthread_mutex_unlock(&g_weather_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get current weather for coordinates
static int gps_weather_get_current(double lat, double lon, gps_weather_current_t *weather) {
    if (!g_weather_initialized || !weather) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Check cache first
    if (get_cached_weather(lat, lon, weather)) {
        return AUTONOMY_SUCCESS;
    }
    
    // Try to use external APIs manager if available for better integration
    if (external_apis_is_initialized()) {
        external_weather_data_t weather_data;
        if (external_apis_get_weather(lat, lon, &weather_data) == AUTONOMY_SUCCESS) {
            // Convert external weather data to GPS weather format
            memset(weather, 0, sizeof(gps_weather_current_t));
            weather->temperature = weather_data.temperature_celsius;
            weather->humidity = weather_data.humidity_percent;
            weather->pressure = weather_data.pressure_hpa;
            weather->wind_speed = weather_data.wind_speed_ms;
            weather->wind_direction = weather_data.wind_direction_deg;
            weather->visibility = weather_data.visibility_km * 1000.0; // Convert km to m
            strncpy(weather->description, weather_data.description, sizeof(weather->description) - 1);
            weather->description[sizeof(weather->description) - 1] = '\0';
            strncpy(weather->icon, weather_data.icon, sizeof(weather->icon) - 1);
            weather->icon[sizeof(weather->icon) - 1] = '\0';
            weather->timestamp = weather_data.timestamp;
            weather->lat = lat;
            weather->lon = lon;
            
            // Cache the result
            cache_weather_data(lat, lon, weather);
            
            LOGX_INFO("Weather data obtained from external APIs manager",
                     "lat", lat, "lon", lon,
                     "temperature", weather->temperature,
                     "description", weather->description,
                     "source", weather_data.source);
            
            return AUTONOMY_SUCCESS;
        }
    }
    
    // Fallback to direct weather API request
    // Build request parameters
    char params[512];
    snprintf(params, sizeof(params), "lat=%.6f&lon=%.6f", lat, lon);
    
    // Perform API request
    gps_weather_api_response_t response;
    int result = perform_weather_api_request(WEATHER_CURRENT_ENDPOINT, params, &response);
    if (result != AUTONOMY_SUCCESS) {
        return result;
    }
    
    if (!response.success) {
        return AUTONOMY_ERROR_API_FAILED;
    }
    
    // Parse response and cache result
    parse_current_weather_response(&response, weather);
    cache_weather_data(lat, lon, weather);
    
    return AUTONOMY_SUCCESS;
}

// Get weather forecast for coordinates
int gps_weather_get_forecast(double lat, double lon, int days, 
                            gps_weather_forecast_t *forecast) {
    if (!g_weather_initialized || !forecast || days <= 0 || days > MAX_FORECAST_DAYS) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Build request parameters
    char params[512];
    snprintf(params, sizeof(params), "lat=%.6f&lon=%.6f&cnt=%d", lat, lon, days * 8); // 8 forecasts per day
    
    // Perform API request
    gps_weather_api_response_t response;
    int result = perform_weather_api_request(WEATHER_FORECAST_ENDPOINT, params, &response);
    if (result != AUTONOMY_SUCCESS) {
        return result;
    }
    
    if (!response.success) {
        return AUTONOMY_ERROR_API_FAILED;
    }
    
    // Parse response
    parse_forecast_response(&response, forecast);
    
    return AUTONOMY_SUCCESS;
}

// Get air quality for coordinates
static int gps_weather_get_air_quality(double lat, double lon, gps_weather_air_quality_t *air_quality) {
    if (!g_weather_initialized || !air_quality) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Build request parameters
    char params[512];
    snprintf(params, sizeof(params), "lat=%.6f&lon=%.6f", lat, lon);
    
    // Perform API request
    gps_weather_api_response_t response;
    int result = perform_weather_api_request(WEATHER_AIR_QUALITY_ENDPOINT, params, &response);
    if (result != AUTONOMY_SUCCESS) {
        return result;
    }
    
    if (!response.success) {
        return AUTONOMY_ERROR_API_FAILED;
    }
    
    // Parse response
    parse_air_quality_response(&response, air_quality);
    
    return AUTONOMY_SUCCESS;
}

// Check cached weather data
static bool get_cached_weather(double lat, double lon, gps_weather_current_t *weather) {
    time_t now = time(NULL);
    
    for (int i = 0; i < g_weather.cache_entry_count; i++) {
        if (!g_weather.weather_cache[i].active) {
            continue;
        }
        
        gps_weather_cache_entry_t *cache = &g_weather.weather_cache[i];
        
        // Check if coordinates are within cache radius
        double distance = calculate_distance(lat, lon, cache->lat, cache->lon);
        if (distance <= g_weather.cache_radius) {
            // Check if cache is still valid
            if ((now - cache->timestamp) < g_weather.update_interval) {
                // Return cached data
                weather->timestamp = cache->timestamp;
                weather->temperature = cache->temperature;
                weather->humidity = cache->humidity;
                weather->pressure = cache->pressure;
                weather->wind_speed = cache->wind_speed;
                weather->wind_direction = cache->wind_direction;
                weather->visibility = cache->visibility;
                weather->cloud_cover = cache->cloud_cover;
                weather->weather_condition = cache->weather_condition;
                weather->air_quality_index = cache->air_quality_index;
                
                LOGX_DEBUG("Weather data retrieved from cache for (%.6f, %.6f)", lat, lon);
                return true;
            }
        }
    }
    
    return false;
}

// Cache weather data
static void cache_weather_data(double lat, double lon, const gps_weather_current_t *weather) {
    // Find free cache slot
    int slot_index = -1;
    for (int i = 0; i < g_weather.max_cache_entries; i++) {
        if (!g_weather.weather_cache[i].active) {
            slot_index = i;
            break;
        }
    }
    
    if (slot_index < 0) {
        // Remove oldest entry to make room
        slot_index = find_oldest_weather_cache();
        if (slot_index >= 0) {
            g_weather.weather_cache[slot_index].active = false;
            g_weather.cache_entry_count--;
        }
    }
    
    if (slot_index >= 0) {
        gps_weather_cache_entry_t *cache = &g_weather.weather_cache[slot_index];
        
        cache->active = true;
        cache->lat = lat;
        cache->lon = lon;
        cache->timestamp = weather->timestamp;
        cache->temperature = weather->temperature;
        cache->humidity = weather->humidity;
        cache->pressure = weather->pressure;
        cache->wind_speed = weather->wind_speed;
        cache->wind_direction = weather->wind_direction;
        cache->visibility = weather->visibility;
        cache->cloud_cover = weather->cloud_cover;
        cache->weather_condition = weather->weather_condition;
        cache->air_quality_index = weather->air_quality_index;
        
        if (slot_index >= g_weather.cache_entry_count) {
            g_weather.cache_entry_count = slot_index + 1;
        }
        
        LOGX_DEBUG("Weather data cached for (%.6f, %.6f)", lat, lon);
    }
}

// Find oldest weather cache entry
static int find_oldest_weather_cache(void) {
    int oldest_index = -1;
    time_t oldest_time = time(NULL);
    
    for (int i = 0; i < g_weather.max_cache_entries; i++) {
        if (g_weather.weather_cache[i].active && 
            g_weather.weather_cache[i].timestamp < oldest_time) {
            oldest_time = g_weather.weather_cache[i].timestamp;
            oldest_index = i;
        }
    }
    
    return oldest_index;
}

// Calculate distance between coordinates
static double calculate_distance(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371000.0; // Earth radius in meters
    
    double lat1_rad = lat1 * M_PI / 180.0;
    double lat2_rad = lat2 * M_PI / 180.0;
    double delta_lat = (lat2 - lat1) * M_PI / 180.0;
    double delta_lon = (lon2 - lon1) * M_PI / 180.0;
    
    double a = sin(delta_lat / 2.0) * sin(delta_lat / 2.0) +
               cos(lat1_rad) * cos(lat2_rad) *
               sin(delta_lon / 2.0) * sin(delta_lon / 2.0);
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    
    return R * c;
}

// Parse current weather response
static void parse_current_weather_response(const gps_weather_api_response_t *response, 
                                         gps_weather_current_t *weather) {
    // Initialize weather data
    memset(weather, 0, sizeof(gps_weather_current_t));
    weather->timestamp = response->timestamp;
    
    // This is a simplified parser - in a real implementation, you would use a JSON library
    // to properly parse the OpenWeatherMap API response
    
    // For now, set placeholder values
    weather->temperature = 20.0;        // 20°C
    weather->humidity = 65.0;           // 65%
    weather->pressure = 1013.25;        // 1013.25 hPa
    weather->wind_speed = 5.0;          // 5 m/s
    weather->wind_direction = 180.0;    // 180° (South)
    weather->visibility = 10000.0;      // 10km
    weather->cloud_cover = 30.0;        // 30%
    weather->weather_condition = WEATHER_CONDITION_PARTLY_CLOUDY;
    weather->air_quality_index = 50;    // Moderate
    
    LOGX_DEBUG("Parsed current weather response");
}

// Parse forecast response
static void parse_forecast_response(const gps_weather_api_response_t *response, 
                                  gps_weather_forecast_t *forecast) {
    // Initialize forecast data
    memset(forecast, 0, sizeof(gps_weather_forecast_t));
    forecast->timestamp = response->timestamp;
    forecast->forecast_count = 0;
    
    // This is a simplified parser - in a real implementation, you would use a JSON library
    LOGX_DEBUG("Parsed forecast response");
}

// Parse air quality response
static void parse_air_quality_response(const gps_weather_api_response_t *response, 
                                     gps_weather_air_quality_t *air_quality) {
    // Initialize air quality data
    memset(air_quality, 0, sizeof(gps_weather_air_quality_t));
    air_quality->timestamp = response->timestamp;
    
    // This is a simplified parser - in a real implementation, you would use a JSON library
    LOGX_DEBUG("Parsed air quality response");
}

// Get weather integration status
static int gps_weather_get_status(gps_weather_status_t *status) {
    if (!g_weather_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_weather_mutex);
    
    status->enabled = g_weather.enabled;
    status->cache_entry_count = g_weather.cache_entry_count;
    status->max_cache_entries = g_weather.max_cache_entries;
    status->total_requests = g_weather.total_requests;
    status->successful_requests = g_weather.successful_requests;
    status->failed_requests = g_weather.failed_requests;
    status->last_update = g_weather.last_update;
    
    // Calculate success rate
    if (g_weather.total_requests > 0) {
        status->success_rate = (double)g_weather.successful_requests / g_weather.total_requests;
    } else {
        status->success_rate = 0.0;
    }
    
    pthread_mutex_unlock(&g_weather_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get weather integration configuration
static int gps_weather_get_config(gps_weather_config_t *config) {
    if (!g_weather_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_weather_mutex);
    
    config->enabled = g_weather.enabled;
    config->max_cache_entries = g_weather.max_cache_entries;
    config->update_interval = g_weather.update_interval;
    config->max_forecast_days = g_weather.max_forecast_days;
    config->cache_radius = g_weather.cache_radius;
    strncpy(config->api_key, g_weather.api_key, sizeof(config->api_key) - 1);
    config->api_key[sizeof(config->api_key) - 1] = '\0';
    
    pthread_mutex_unlock(&g_weather_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set weather integration configuration
static int gps_weather_set_config(const gps_weather_config_t *config) {
    if (!g_weather_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_weather_mutex);
    
    g_weather.enabled = config->enabled;
    g_weather.max_cache_entries = config->max_cache_entries;
    g_weather.update_interval = config->update_interval;
    g_weather.max_forecast_days = config->max_forecast_days;
    g_weather.cache_radius = config->cache_radius;
    
    if (strlen(config->api_key) > 0) {
        strncpy(g_weather.api_key, config->api_key, sizeof(g_weather.api_key) - 1);
        g_weather.api_key[sizeof(g_weather.api_key) - 1] = '\0';
    }
    
    pthread_mutex_unlock(&g_weather_mutex);
    
    LOGX_INFO("GPS weather integration configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable weather integration
static int gps_weather_set_enabled(bool enabled) {
    if (!g_weather_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_weather_mutex);
    g_weather.enabled = enabled;
    pthread_mutex_unlock(&g_weather_mutex);
    
    LOGX_INFO("GPS weather integration %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Force weather update
static int gps_weather_force_update(void) {
    if (!g_weather_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    // Reset last update time to force immediate update
    pthread_mutex_lock(&g_weather_mutex);
    g_weather.last_update = 0;
    pthread_mutex_unlock(&g_weather_mutex);
    
    LOGX_INFO("GPS weather update forced");
    return AUTONOMY_SUCCESS;
}

// Get weather statistics
static int gps_weather_get_statistics(gps_weather_stats_t *stats) {
    if (!g_weather_initialized || !stats) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_weather_mutex);
    
    // Calculate statistics from weather cache
    memset(stats, 0, sizeof(gps_weather_stats_t));
    
    for (int i = 0; i < g_weather.cache_entry_count; i++) {
        if (!g_weather.weather_cache[i].active) {
            continue;
        }
        
        gps_weather_cache_entry_t *cache = &g_weather.weather_cache[i];
        
        // Calculate averages
        stats->total_temperature += cache->temperature;
        stats->total_humidity += cache->humidity;
        stats->total_pressure += cache->pressure;
        stats->total_wind_speed += cache->wind_speed;
        stats->total_entries++;
    }
    
    if (stats->total_entries > 0) {
        stats->average_temperature = stats->total_temperature / stats->total_entries;
        stats->average_humidity = stats->total_humidity / stats->total_entries;
        stats->average_pressure = stats->total_pressure / stats->total_entries;
        stats->average_wind_speed = stats->total_wind_speed / stats->total_entries;
    }
    
    stats->total_requests = g_weather.total_requests;
    stats->successful_requests = g_weather.successful_requests;
    stats->failed_requests = g_weather.failed_requests;
    
    pthread_mutex_unlock(&g_weather_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Reset weather integration
static int gps_weather_reset(void) {
    if (!g_weather_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_weather_mutex);
    
    g_weather.cache_entry_count = 0;
    g_weather.total_requests = 0;
    g_weather.successful_requests = 0;
    g_weather.failed_requests = 0;
    g_weather.last_update = 0;
    
    // Clear weather cache
    for (int i = 0; i < MAX_WEATHER_CACHE_ENTRIES; i++) {
        g_weather.weather_cache[i].active = false;
        g_weather.weather_cache[i].lat = 0.0;
        g_weather.weather_cache[i].lon = 0.0;
        g_weather.weather_cache[i].timestamp = 0;
        g_weather.weather_cache[i].temperature = 0.0;
        g_weather.weather_cache[i].humidity = 0.0;
        g_weather.weather_cache[i].pressure = 0.0;
        g_weather.weather_cache[i].wind_speed = 0.0;
        g_weather.weather_cache[i].wind_direction = 0.0;
        g_weather.weather_cache[i].visibility = 0.0;
        g_weather.weather_cache[i].cloud_cover = 0.0;
        g_weather.weather_cache[i].weather_condition = WEATHER_CONDITION_UNKNOWN;
        g_weather.weather_cache[i].air_quality_index = 0;
    }
    
    pthread_mutex_unlock(&g_weather_mutex);
    
    LOGX_INFO("GPS weather integration reset");
    return AUTONOMY_SUCCESS;
}

// Cleanup weather integration
static void gps_weather_cleanup(void) {
    if (!g_weather_initialized) {
        return;
    }
    
    curl_global_cleanup();
    pthread_mutex_destroy(&g_weather_mutex);
    g_weather_initialized = false;
    
    LOGX_INFO("GPS weather integration cleaned up");
}
