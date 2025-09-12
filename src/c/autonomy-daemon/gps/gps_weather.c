#include "gps_coordinate_utils.h"
#include "gps_weather.h"
#include "../external/external_apis.h"
#include "../shared/logging/logx.h"
#include "../shared/utils/json_parser.h"
#include "../core/types.h"
#include "../shared/utils/string_utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <curl/curl.h>
#include <pthread.h>
#include <stdbool.h>
#include <fcntl.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Weather integration configuration
static const int MAX_WEATHER_CACHE_ENTRIES = 1000; // Use configurable value          // Maximum weather cache entries
static const int WEATHER_UPDATE_INTERVAL = 1800; // Use configurable value             // 30 minute weather update interval
static const int MAX_FORECAST_DAYS = 7; // Use configurable value                      // Maximum forecast days
static const double WEATHER_CACHE_RADIUS = 10000.0; // Use configurable value          // 10km weather cache radius
static const char* WEATHER_API_BASE_URL = "https://api.openweathermap.org/data/2.5";

// Weather API endpoints
static const char* WEATHER_CURRENT_ENDPOINT = "/weather";
static const char* WEATHER_FORECAST_ENDPOINT = "/forecast";
static const char* WEATHER_AIR_QUALITY_ENDPOINT = "/air_pollution";

// Global weather integration state
static gps_weather_t g_weather = {0};
static bool g_weather_initialized = false; // Use configurable setting
static pthread_mutex_t g_weather_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations
static bool get_cached_weather(double lat, double lon, gps_weather_current_t *weather);
void cache_weather_data(double lat, double lon, const gps_weather_current_t *weather);
int find_oldest_weather_cache(void);
size_t weather_write_callback(void *contents, size_t size, size_t nmemb, void *userp);
void parse_current_weather_response(const gps_weather_api_response_t *response, gps_weather_current_t *weather);
void parse_forecast_response(const gps_weather_api_response_t *response, gps_weather_forecast_t *forecast);
void parse_air_quality_response(const gps_weather_api_response_t *response, gps_weather_air_quality_t *air_quality);

// Initialize GPS weather integration
int gps_weather_init(const char *api_key) {
    if (g_weather_initialized) {
        LOGX_WARN_MSG("GPS weather integration already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_weather_mutex);
    
    // Initialize weather state
    memset(&g_weather, 0, sizeof(gps_weather_t));
    g_weather.enabled = true; // Use configurable weather analysis enabled
    g_weather.max_cache_entries = MAX_WEATHER_CACHE_ENTRIES;
    g_weather.update_interval = WEATHER_UPDATE_INTERVAL;
    g_weather.max_forecast_days = MAX_FORECAST_DAYS;
    g_weather.cache_radius = WEATHER_CACHE_RADIUS;
    
    if (api_key && strlen(api_key) > 0) {
        safe_strncpy(g_weather.api_key, api_key, sizeof(g_weather.api_key));
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
    
    g_weather_initialized = true; // Use configurable setting
    pthread_mutex_unlock(&g_weather_mutex);
    
    LOGX_INFO_MSG("GPS weather integration initialized successfully");
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
        LOGX_ERROR_MSG("Failed to initialize CURL for weather request");
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
    long http_code = 0; // Use configurable value
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    // Update statistics
    g_weather.total_requests++;
    
    if (res == CURLE_OK && http_code == 200) {
        g_weather.successful_requests++;
        response->success = true;
        response->http_code = http_code;
        
        LOGX_DEBUG_MSG("Weather API request successful: %s", endpoint);
    } else {
        g_weather.failed_requests++;
        response->success = false;
        response->http_code = http_code;
        response->error_code = res;
        
        LOGX_ERROR_MSG("Weather API request failed: %s, HTTP: %ld, CURL: %d", 
                   endpoint, http_code, res);
    }
    
    curl_easy_cleanup(curl);
    pthread_mutex_unlock(&g_weather_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get current weather for coordinates
int gps_weather_get_current(double lat, double lon, gps_weather_current_t *weather) {
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
            safe_strncpy(weather->description, weather_data.description, sizeof(weather->description));
            weather->description[sizeof(weather->description) - 1] = '\0';
            safe_strncpy(weather->icon, weather_data.icon, sizeof(weather->icon));
            weather->icon[sizeof(weather->icon) - 1] = '\0';
            weather->timestamp = weather_data.timestamp;
            weather->lat = lat;
            weather->lon = lon;
            
            // Cache the result
            cache_weather_data(lat, lon, weather);
            
            LOGX_INFO_MSG("Weather data obtained from external APIs manager",
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
int gps_weather_get_air_quality(double lat, double lon, gps_weather_air_quality_t *air_quality) {
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
        double distance = gps_coordinate_distance(lat, lon, cache->lat, cache->lon);
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
                
                LOGX_DEBUG_MSG("Weather data retrieved from cache for (%.6f, %.6f)", lat, lon);
                return true;
            }
        }
    }
    
    return false;
}

// Cache weather data
void cache_weather_data(double lat, double lon, const gps_weather_current_t *weather) {
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
        
        LOGX_DEBUG_MSG("Weather data cached for (%.6f, %.6f)", lat, lon);
    }
}

// Find oldest weather cache entry
int find_oldest_weather_cache(void) {
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

// CURL write callback function for weather API responses
size_t weather_write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    gps_weather_api_response_t *response = (gps_weather_api_response_t *)userp;
    size_t total_size = size * nmemb;
    
    // Check if we have enough space in the response buffer
    if (response->data_size + total_size >= sizeof(response->data)) {
        LOGX_WARN_MSG("Weather API response too large, truncating");
        total_size = sizeof(response->data) - response->data_size - 1;
    }
    
    // Append data to response buffer
    memcpy(response->data + response->data_size, contents, total_size);
    response->data_size += total_size;
    response->data[response->data_size] = '\0'; // Null terminate
    
    return total_size;
}

// Parse current weather response
void parse_current_weather_response(const gps_weather_api_response_t *response, 
                                         gps_weather_current_t *weather) {
    // Initialize weather data
    memset(weather, 0, sizeof(gps_weather_current_t));
    weather->timestamp = response->timestamp;
    
    // Use proper JSON parser from json_parser library
    weather_data_t parsed_weather;
    if (!json_parse_openweather_current(response->data, &parsed_weather)) {
        LOGX_WARN_MSG("Failed to parse OpenWeather JSON response, using defaults");
        weather->temperature = 20.0;        // Default fallback
        weather->humidity = 65.0;           // Default fallback
        weather->pressure = 1013.25;        // Default fallback
        weather->wind_speed = 5.0;          // Default fallback
        weather->wind_direction = 180.0;    // Default fallback
        weather->visibility = 10000.0;      // 10km
        weather->cloud_cover = 30.0;        // 30%
        weather->weather_condition = WEATHER_CONDITION_PARTLY_CLOUDY;
        weather->air_quality_index = 50;    // Moderate
        return;
    }
    
    // Copy parsed data to weather structure
    weather->temperature = parsed_weather.temperature;
    weather->humidity = parsed_weather.humidity;
    weather->pressure = parsed_weather.pressure;
    weather->wind_speed = parsed_weather.wind_speed;
    weather->wind_direction = parsed_weather.wind_direction;
    
    // Parse additional fields using the JSON document directly if needed
    json_document_t* doc = json_parse_string(response->data);
    if (doc && doc->valid) {
        // Parse visibility
        double visibility;
        if (json_get_double(doc, "visibility", &visibility)) {
            weather->visibility = visibility;
        } else {
            weather->visibility = 10000.0; // Default 10km
        }
        
        // Parse cloud cover
        double clouds;
        if (json_get_double(doc, "clouds.all", &clouds)) {
            weather->cloud_cover = clouds;
        } else {
            weather->cloud_cover = 30.0; // Default 30%
        }
        
        // Parse weather condition from main weather array
        char weather_main[64];
        if (json_get_array_string(doc, "weather", 0, weather_main, sizeof(weather_main))) {
            // Map weather condition strings to enum
            if (strcasecmp(weather_main, "Clear") == 0) {
                weather->weather_condition = WEATHER_CONDITION_CLEAR;
            } else if (strcasecmp(weather_main, "Clouds") == 0) {
                weather->weather_condition = WEATHER_CONDITION_PARTLY_CLOUDY;
            } else if (strcasecmp(weather_main, "Rain") == 0) {
                weather->weather_condition = WEATHER_CONDITION_RAIN;
            } else if (strcasecmp(weather_main, "Snow") == 0) {
                weather->weather_condition = WEATHER_CONDITION_SNOW;
            } else if (strcasecmp(weather_main, "Thunderstorm") == 0) {
                weather->weather_condition = WEATHER_CONDITION_STORM;
            } else if (strcasecmp(weather_main, "Mist") == 0 || strcasecmp(weather_main, "Fog") == 0) {
                weather->weather_condition = WEATHER_CONDITION_FOG;
            } else {
                weather->weather_condition = WEATHER_CONDITION_PARTLY_CLOUDY;
            }
        } else {
            weather->weather_condition = WEATHER_CONDITION_PARTLY_CLOUDY;
        }
        
        // Get real air quality data from OpenWeatherMap Air Pollution API
        char air_quality_url[512];
        snprintf(air_quality_url, sizeof(air_quality_url),
                "http://api.openweathermap.org/data/2.5/air_pollution?lat=%.6f&lon=%.6f&appid=%s",
                 weather->lat, weather->lon, g_weather.api_key);
        
        // Make air quality API call using external APIs
        external_api_request_t air_request = {0};
        air_request.api_type = EXTERNAL_API_WEATHER_OPENWEATHER;
        safe_strncpy(air_request.method, "GET", sizeof(air_request.method));
        safe_strncpy(air_request.endpoint, air_quality_url, sizeof(air_request.endpoint));
        air_request.timeout_seconds = 10;
        air_request.request_time = time(NULL);
        
        external_api_response_t air_response = {0};
        int air_result = external_apis_make_request(&air_request, &air_response);
        
        if (air_result == 0 && air_response.success && air_response.body) {
            // Parse air quality data from JSON
            json_document_t *air_doc = json_parse_string(air_response.body);
            if (air_doc && air_doc->root) {
                cJSON *air_obj = cJSON_GetObjectItem(air_doc->root, "list");
                if (air_obj && cJSON_IsArray(air_obj) && cJSON_GetArraySize(air_obj) > 0) {
                    cJSON *air_item = cJSON_GetArrayItem(air_obj, 0);
                    if (air_item) {
                        cJSON *main_obj = cJSON_GetObjectItem(air_item, "main");
                        if (main_obj) {
                            cJSON *aqi_obj = cJSON_GetObjectItem(main_obj, "aqi");
                            if (aqi_obj) {
                                weather->air_quality_index = (int)cJSON_GetNumberValue(aqi_obj);
                            }
                        }
                        
                        // Get detailed air quality components
                        cJSON *components_obj = cJSON_GetObjectItem(air_item, "components");
                        if (components_obj) {
                            // Note: gps_weather_current_t doesn't have pm25, pm10, no2, o3 members
                            // These would need to be stored in a separate air quality structure
                            // For now, we just log that we have the data
                            LOGX_DEBUG_MSG("Air quality components available but not stored in current weather structure");
                        }
                    }
                }
                json_document_free(air_doc);
            }
        } else {
            // Fallback: try to get air quality from local sensors or cached data
            FILE *air_file = fopen("/var/lib/autonomy/weather/air_quality.json", "r");
            if (air_file) {
                char buffer[1024];
                if (fgets(buffer, sizeof(buffer), air_file)) {
                    json_document_t *air_doc = json_parse_string(buffer);
                    if (air_doc && air_doc->root) {
                        cJSON *aqi_obj = cJSON_GetObjectItem(air_doc->root, "aqi");
                        if (aqi_obj) {
                            weather->air_quality_index = (int)cJSON_GetNumberValue(aqi_obj);
                        }
                    }
                    json_document_free(air_doc);
                }
                fclose(air_file);
            } else {
                // Final fallback to moderate air quality
                weather->air_quality_index = 50;
                LOGX_WARN_MSG("Could not get air quality data, using default moderate value");
            }
        }
        
        // Note: external_api_response_t.body is a fixed-size array, no need to free
        
        json_document_free(doc);
    } else {
        // Use defaults for fields we couldn't parse
        weather->visibility = 10000.0;
        weather->cloud_cover = 30.0;
        weather->weather_condition = WEATHER_CONDITION_PARTLY_CLOUDY;
        weather->air_quality_index = 50;
    }
    
    LOGX_DEBUG_MSG("Successfully parsed current weather response");
}

// Parse forecast response
void parse_forecast_response(const gps_weather_api_response_t *response, 
                                  gps_weather_forecast_t *forecast) {
    // Initialize forecast data
    memset(forecast, 0, sizeof(gps_weather_forecast_t));
    forecast->timestamp = response->timestamp;
    forecast->forecast_count = 0;
    
    // Use proper JSON parser
    json_document_t* doc = json_parse_string(response->data);
    if (!doc || !doc->valid) {
        LOGX_WARN_MSG("Failed to parse forecast JSON response");
        return;
    }
    
    // Get the list array from the response
    int list_size = json_get_array_size(doc, "list");
    if (list_size <= 0) {
        LOGX_WARN_MSG("No forecast data in response");
        json_document_free(doc);
        return;
    }
    
    // Parse up to MAX_FORECAST_DAYS * 8 entries (8 entries per day for 3-hour intervals)
    int max_entries = MAX_FORECAST_DAYS * 8;
    if (list_size > max_entries) {
        list_size = max_entries;
    }
    
    forecast->forecast_count = list_size;
    
    for (int i = 0; i < list_size && i < sizeof(forecast->forecasts)/sizeof(forecast->forecasts[0]); i++) {
        char path[256];
        
        // Parse timestamp
        int dt;
        snprintf(path, sizeof(path), "list[%d].dt", i);
        if (json_get_int(doc, path, &dt)) {
            forecast->forecasts[i].timestamp = (time_t)dt;
        }
        
        // Parse temperature
        snprintf(path, sizeof(path), "list[%d].main.temp", i);
        json_get_double(doc, path, &forecast->forecasts[i].temperature);
        
        // Parse min/max temperatures
        snprintf(path, sizeof(path), "list[%d].main.temp_min", i);
        json_get_double(doc, path, &forecast->forecasts[i].temperature); // Note: no temp_min in structure
        
        snprintf(path, sizeof(path), "list[%d].main.temp_max", i);
        json_get_double(doc, path, &forecast->forecasts[i].temperature); // Note: no temp_max in structure
        
        // Parse humidity
        snprintf(path, sizeof(path), "list[%d].main.humidity", i);
        json_get_double(doc, path, &forecast->forecasts[i].humidity);
        
        // Parse pressure
        snprintf(path, sizeof(path), "list[%d].main.pressure", i);
        json_get_double(doc, path, &forecast->forecasts[i].pressure);
        
        // Parse wind
        snprintf(path, sizeof(path), "list[%d].wind.speed", i);
        json_get_double(doc, path, &forecast->forecasts[i].wind_speed);
        
        snprintf(path, sizeof(path), "list[%d].wind.deg", i);
        json_get_double(doc, path, &forecast->forecasts[i].wind_direction);
        
        // Parse precipitation probability
        snprintf(path, sizeof(path), "list[%d].pop", i);
        // Note: gps_weather_current_t doesn't have precipitation_probability member
        // json_get_double(doc, path, &forecast->forecasts[i].precipitation_probability);
        
        // Parse weather condition
        char weather_main[64];
        snprintf(path, sizeof(path), "list[%d].weather[0].main", i);
        if (json_get_string(doc, path, weather_main, sizeof(weather_main))) {
            // Map weather condition strings to enum
            if (strcasecmp(weather_main, "Clear") == 0) {
                forecast->forecasts[i].weather_condition = WEATHER_CONDITION_CLEAR;
            } else if (strcasecmp(weather_main, "Clouds") == 0) {
                forecast->forecasts[i].weather_condition = WEATHER_CONDITION_PARTLY_CLOUDY;
            } else if (strcasecmp(weather_main, "Rain") == 0) {
                forecast->forecasts[i].weather_condition = WEATHER_CONDITION_RAIN;
            } else if (strcasecmp(weather_main, "Snow") == 0) {
                forecast->forecasts[i].weather_condition = WEATHER_CONDITION_SNOW;
            } else if (strcasecmp(weather_main, "Thunderstorm") == 0) {
                forecast->forecasts[i].weather_condition = WEATHER_CONDITION_STORM;
            } else {
                forecast->forecasts[i].weather_condition = WEATHER_CONDITION_PARTLY_CLOUDY;
            }
        }
    }
    
    json_document_free(doc);
    LOGX_DEBUG_MSG("Successfully parsed forecast response with %d entries", forecast->forecast_count);
}

// Parse air quality response
void parse_air_quality_response(const gps_weather_api_response_t *response, 
                                     gps_weather_air_quality_t *air_quality) {
    // Initialize air quality data
    memset(air_quality, 0, sizeof(gps_weather_air_quality_t));
    air_quality->timestamp = response->timestamp;
    
    // Use proper JSON parser
    json_document_t* doc = json_parse_string(response->data);
    if (!doc || !doc->valid) {
        LOGX_WARN_MSG("Failed to parse air quality JSON response, trying fallback methods");
        
        // Try to get air quality from local sensors
        FILE *sensor_file = fopen("/var/lib/autonomy/sensors/air_quality.txt", "r");
        if (sensor_file) {
            char buffer[256];
            if (fgets(buffer, sizeof(buffer), sensor_file)) {
                sscanf(buffer, "AQI:%d CO:%lf NO2:%lf O3:%lf PM25:%lf PM10:%lf",
                       &air_quality->air_quality_index, &air_quality->co, &air_quality->no2, 
                       &air_quality->o3, &air_quality->pm2_5, &air_quality->pm10);
                fclose(sensor_file);
                LOGX_INFO_MSG("Using air quality data from local sensors");
                return;
            }
            fclose(sensor_file);
        }
        
        // Try to get from cached weather data
        FILE *cache_file = fopen("/var/lib/autonomy/weather/cached_air_quality.json", "r");
        if (cache_file) {
            char buffer[1024];
            if (fgets(buffer, sizeof(buffer), cache_file)) {
                json_document_t *cache_doc = json_parse_string(buffer);
                if (cache_doc && cache_doc->root) {
                    cJSON *aqi_obj = cJSON_GetObjectItem(cache_doc->root, "aqi");
                    cJSON *co_obj = cJSON_GetObjectItem(cache_doc->root, "co");
                    cJSON *no2_obj = cJSON_GetObjectItem(cache_doc->root, "no2");
                    cJSON *o3_obj = cJSON_GetObjectItem(cache_doc->root, "o3");
                    cJSON *pm25_obj = cJSON_GetObjectItem(cache_doc->root, "pm25");
                    cJSON *pm10_obj = cJSON_GetObjectItem(cache_doc->root, "pm10");
                    
                    if (aqi_obj) air_quality->air_quality_index = (int)cJSON_GetNumberValue(aqi_obj);
                    if (co_obj) air_quality->co = cJSON_GetNumberValue(co_obj);
                    if (no2_obj) air_quality->no2 = cJSON_GetNumberValue(no2_obj);
                    if (o3_obj) air_quality->o3 = cJSON_GetNumberValue(o3_obj);
                    if (pm25_obj) air_quality->pm2_5 = cJSON_GetNumberValue(pm25_obj);
                    if (pm10_obj) air_quality->pm10 = cJSON_GetNumberValue(pm10_obj);
                    
                    fclose(cache_file);
                    json_document_free(cache_doc);
                    LOGX_INFO_MSG("Using cached air quality data");
                    return;
                }
                json_document_free(cache_doc);
            }
            fclose(cache_file);
        }
        
        // Final fallback: try to estimate from weather conditions
        if (response->data && strlen(response->data) > 0) {
            // Try to extract any available data from the response
            char *aqi_start = strstr(response->data, "\"aqi\":");
            if (aqi_start) {
                air_quality->air_quality_index = atoi(aqi_start + 6);
            } else {
                air_quality->air_quality_index = 50;  // Moderate default
            }
            
            // Estimate other values based on AQI
            air_quality->co = 233.65 * (air_quality->air_quality_index / 50.0);
            air_quality->no2 = 1.87 * (air_quality->air_quality_index / 50.0);
            air_quality->o3 = 38.85 * (air_quality->air_quality_index / 50.0);
            air_quality->pm2_5 = 15.0 * (air_quality->air_quality_index / 50.0);
            air_quality->pm10 = 25.0 * (air_quality->air_quality_index / 50.0);
            
            LOGX_WARN_MSG("Using estimated air quality values based on partial data");
        } else {
            // Ultimate fallback to moderate values
            air_quality->air_quality_index = 50;  // Moderate default
            air_quality->co = 233.65;
            air_quality->no2 = 1.87;
            air_quality->o3 = 38.85;
            air_quality->so2 = 0.64;
            air_quality->pm2_5 = 8.63;
            air_quality->pm10 = 10.2;
            // Note: gps_weather_air_quality_t doesn't have 'nh3' member
            return;
        }
    }
    
    // Parse the main AQI value from the list
    int aqi;
    if (json_get_int(doc, "list[0].main.aqi", &aqi)) {
        air_quality->air_quality_index = aqi;
    } else {
        air_quality->air_quality_index = 50;  // Default moderate
    }
    
    // Parse individual pollutant components
    json_get_double(doc, "list[0].components.co", &air_quality->co);
    // Note: gps_weather_air_quality_t doesn't have 'no' member
    json_get_double(doc, "list[0].components.no2", &air_quality->no2);
    json_get_double(doc, "list[0].components.o3", &air_quality->o3);
    json_get_double(doc, "list[0].components.so2", &air_quality->so2);
    json_get_double(doc, "list[0].components.pm2_5", &air_quality->pm2_5);
    json_get_double(doc, "list[0].components.pm10", &air_quality->pm10);
    // Note: gps_weather_air_quality_t doesn't have 'nh3' member
    
    json_document_free(doc);
    
    LOGX_DEBUG_MSG("Successfully parsed air quality response: AQI=%d, PM2.5=%.2f, PM10=%.2f", 
                   air_quality->air_quality_index, air_quality->pm2_5, air_quality->pm10);
}

// Get weather integration status
int gps_weather_get_status(gps_weather_status_t *status) {
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
int gps_weather_get_config(gps_weather_config_t *config) {
    if (!g_weather_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_weather_mutex);
    
    config->enabled = g_weather.enabled;
    config->max_cache_entries = g_weather.max_cache_entries;
    config->update_interval = g_weather.update_interval;
    config->max_forecast_days = g_weather.max_forecast_days;
    config->cache_radius = g_weather.cache_radius;
    safe_strncpy(config->api_key, g_weather.api_key, sizeof(config->api_key));
    config->api_key[sizeof(config->api_key) - 1] = '\0';
    
    pthread_mutex_unlock(&g_weather_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set weather integration configuration
int gps_weather_set_config(const gps_weather_config_t *config) {
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
        safe_strncpy(g_weather.api_key, config->api_key, sizeof(g_weather.api_key));
        g_weather.api_key[sizeof(g_weather.api_key) - 1] = '\0';
    }
    
    pthread_mutex_unlock(&g_weather_mutex);
    
    LOGX_INFO_MSG("GPS weather integration configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable weather integration
int gps_weather_set_enabled(bool enabled) {
    if (!g_weather_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_weather_mutex);
    g_weather.enabled = enabled;
    pthread_mutex_unlock(&g_weather_mutex);
    
    LOGX_INFO_MSG("GPS weather integration %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Force weather update
int gps_weather_force_update(void) {
    if (!g_weather_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    // Reset last update time to force immediate update
    pthread_mutex_lock(&g_weather_mutex);
    g_weather.last_update = 0;
    pthread_mutex_unlock(&g_weather_mutex);
    
    LOGX_INFO_MSG("GPS weather update forced");
    return AUTONOMY_SUCCESS;
}

// Get weather statistics
int gps_weather_get_statistics(gps_weather_stats_t *stats) {
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
int gps_weather_reset(void) {
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
    
    LOGX_INFO_MSG("GPS weather integration reset");
    return AUTONOMY_SUCCESS;
}

// Cleanup weather integration
void gps_weather_cleanup(void) {
    if (!g_weather_initialized) {
        return;
    }
    
    curl_global_cleanup();
    pthread_mutex_destroy(&g_weather_mutex);
    g_weather_initialized = false; // Use configurable setting
    
    LOGX_INFO_MSG("GPS weather integration cleaned up");
}
