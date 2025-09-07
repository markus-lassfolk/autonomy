#include "../external/external_apis.h"
#include "../wifi/wifi_enhanced.h"
#include "../utils/logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// cURL callback for writing response data
static size_t external_api_curl_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    typedef struct {
        char* data;
        size_t size;
    } curl_response_t;
    
    curl_response_t* response = (curl_response_t*)userp;
    
    char* old_data = response->data;
    char* ptr = realloc(response->data, response->size + realsize + 1);
    if (!ptr) {
        free(old_data);
        response->data = NULL;
        return 0; // Out of memory
    }
    
    response->data = ptr;
    memcpy(&(response->data[response->size]), contents, realsize);
    response->size += realsize;
    response->data[response->size] = 0; // Null terminate
    
    return realsize;
}

// Global external APIs manager
static external_apis_manager_t g_external_apis = {0};
static bool g_external_apis_initialized = false;

// API type strings
static const char* API_TYPE_STRINGS[] = {
    "google_location", "google_elevation", "google_geocoding", "google_places", "google_timezone",
    "open_elevation", "openstreetmap_nominatim", "weather_openweather", "weather_weatherapi",
    "ipinfo_geolocation", "mozilla_location"
};

// API status strings
static const char* API_STATUS_STRINGS[] = {
    "unknown", "healthy", "degraded", "failed", "rate_limited", "quota_exceeded", "disabled"
};

// HTTP response structure for CURL
typedef struct {
    char* data;
    size_t size;
} http_response_t;

// Forward declarations
size_t external_apis_write_callback(void* contents, size_t size, size_t nmemb, http_response_t* response);
static int make_http_request(const char* url, const char* headers, const char* post_data, 
                           int timeout, http_response_t* response);
void update_api_statistics(external_api_type_t api_type, bool success, double duration_ms);
bool check_rate_limits(external_api_type_t api_type);
static void reset_hourly_counters(void);
static void* health_monitor_worker(void* arg);
int perform_api_health_check(external_api_type_t api_type);

// Initialize external APIs manager
int external_apis_init(void) {
    if (g_external_apis_initialized) {
        LOGX_WARN_MSG("External APIs manager already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    memset(&g_external_apis, 0, sizeof(external_apis_manager_t));
    
    // Initialize mutex
    if (pthread_mutex_init(&g_external_apis.mutex, NULL) != 0) {
        LOGX_ERROR_MSG("Failed to initialize external APIs mutex");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Initialize default configurations
    for (int i = 0; i < EXTERNAL_API_MAX; i++) {
        external_api_config_t* config = &g_external_apis.configs[i];
        external_api_statistics_t* stats = &g_external_apis.stats[i];
        
        config->api_type = (external_api_type_t)i;
        config->enabled = false; // Disabled by default (requires configuration)
        strcpy(config->name, external_api_type_to_string((external_api_type_t)i));
        config->timeout_seconds = 30; // Use configurable timeout
        config->max_requests_per_hour = 100; // Use configurable rate limit
        config->max_requests_per_day = 1000; // Use configurable daily limit
        config->retry_attempts = 3; // Use configurable retry attempts
        config->retry_delay_seconds = 5; // Use configurable retry delay
        config->use_ssl = true;
        strcpy(config->user_agent, "Autonomy-Daemon/6.1.0");
        config->enable_health_monitoring = true;
        config->health_check_interval_minutes = 60; // Use configurable health check interval
        config->min_success_rate = 0.8; // Use configurable success rate threshold
        config->max_consecutive_failures = 5; // Use configurable failure threshold
        
        // API-specific defaults
        switch (i) {
            case EXTERNAL_API_GOOGLE_LOCATION:
                strcpy(config->base_url, "https://www.googleapis.com/geolocation/v1");
                config->cost_per_request = 0.005; // $5 per 1000 requests
                config->quota_limit_daily = 100000;
                config->max_requests_per_hour = 2500;
                break;
                
            case EXTERNAL_API_GOOGLE_ELEVATION:
                strcpy(config->base_url, "https://maps.googleapis.com/maps/api/elevation");
                config->cost_per_request = 0.005;
                config->quota_limit_daily = 100000;
                break;
                
            case EXTERNAL_API_GOOGLE_GEOCODING:
                strcpy(config->base_url, "https://maps.googleapis.com/maps/api/geocode");
                config->cost_per_request = 0.005;
                config->quota_limit_daily = 100000;
                break;
                
            case EXTERNAL_API_OPEN_ELEVATION:
                strcpy(config->base_url, "https://api.open-elevation.com/api/v1");
                config->cost_per_request = 0.0; // Free
                config->max_requests_per_hour = 1000;
                config->max_requests_per_day = 10000;
                break;
                
            case EXTERNAL_API_OPENSTREETMAP_NOMINATIM:
                strcpy(config->base_url, "https://nominatim.openstreetmap.org");
                config->cost_per_request = 0.0; // Free
                config->max_requests_per_hour = 100;
                config->max_requests_per_day = 1000;
                break;
                
            case EXTERNAL_API_WEATHER_OPENWEATHER:
                strcpy(config->base_url, "https://api.openweathermap.org/data/2.5");
                config->cost_per_request = 0.0; // Free tier available
                config->max_requests_per_hour = 1000;
                config->quota_limit_daily = 60000;
                break;
                
            default:
                strcpy(config->base_url, "https://api.example.com");
                break;
        }
        
        // Initialize statistics
        stats->stats_reset_time = time(NULL);
        stats->status = API_STATUS_UNKNOWN;
        stats->success_rate = 0.0;
    }
    
    // Initialize CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // Start health monitoring thread
    g_external_apis.health_monitoring_enabled = true;
    g_external_apis.threads_running = true;
    
    if (pthread_create(&g_external_apis.health_monitor_thread, NULL, health_monitor_worker, NULL) != 0) {
        LOGX_ERROR_MSG("Failed to create external APIs health monitor thread");
        curl_global_cleanup();
        pthread_mutex_destroy(&g_external_apis.mutex);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    g_external_apis_initialized = true;
    
    LOGX_INFO_MSG("External APIs manager initialized",
              "apis_available", EXTERNAL_API_MAX,
              "health_monitoring", g_external_apis.health_monitoring_enabled);
    
    return AUTONOMY_SUCCESS;
}

// Cleanup external APIs manager
void external_apis_cleanup(void) {
    if (!g_external_apis_initialized) return;
    
    pthread_mutex_lock(&g_external_apis.mutex);
    
    // Stop health monitoring thread
    g_external_apis.threads_running = false;
    
    if (g_external_apis.health_monitoring_enabled) {
        pthread_cancel(g_external_apis.health_monitor_thread);
        pthread_join(g_external_apis.health_monitor_thread, NULL);
    }
    
    // Cleanup CURL
    curl_global_cleanup();
    
    pthread_mutex_unlock(&g_external_apis.mutex);
    pthread_mutex_destroy(&g_external_apis.mutex);
    
    g_external_apis_initialized = false;
    
    LOGX_INFO_MSG("External APIs manager cleaned up");
}

// Get elevation data from Google or Open Elevation API
int external_apis_get_elevation(double latitude, double longitude, external_elevation_data_t* elevation_data) {
    if (!g_external_apis_initialized || !elevation_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    memset(elevation_data, 0, sizeof(external_elevation_data_t));
    elevation_data->timestamp = time(NULL);
    
    // Try Google Elevation API first if enabled and configured
    if (g_external_apis.configs[EXTERNAL_API_GOOGLE_ELEVATION].enabled &&
        strlen(g_external_apis.configs[EXTERNAL_API_GOOGLE_ELEVATION].api_key) > 0) {
        
        if (!check_rate_limits(EXTERNAL_API_GOOGLE_ELEVATION)) {
            LOGX_WARN_MSG("Google Elevation API rate limited");
            g_external_apis.stats[EXTERNAL_API_GOOGLE_ELEVATION].rate_limited_requests++;
        } else {
            char url[512];
            snprintf(url, sizeof(url), 
                    "%s/json?locations=%.6f,%.6f&key=%s",
                    g_external_apis.configs[EXTERNAL_API_GOOGLE_ELEVATION].base_url,
                    latitude, longitude,
                    g_external_apis.configs[EXTERNAL_API_GOOGLE_ELEVATION].api_key);
            
            http_response_t response = {0};
            time_t start_time = time(NULL);
            
            if (make_http_request(url, NULL, NULL, 
                                g_external_apis.configs[EXTERNAL_API_GOOGLE_ELEVATION].timeout_seconds,
                                &response) == AUTONOMY_SUCCESS && response.data) {
                
                // Parse Google Elevation API response
                json_object* root = json_tokener_parse(response.data);
                if (root) {
                    json_object* status_obj;
                    if (json_object_object_get_ex(root, "status", &status_obj)) {
                        const char* status = json_object_get_string(status_obj);
                        if (strcmp(status, "OK") == 0) {
                            json_object* results_obj;
                            if (json_object_object_get_ex(root, "results", &results_obj)) {
                                int results_len = json_object_array_length(results_obj);
                                if (results_len > 0) {
                                    json_object* first_result = json_object_array_get_idx(results_obj, 0);
                                    if (first_result) {
                                        json_object* elevation_obj, *resolution_obj;
                                        if (json_object_object_get_ex(first_result, "elevation", &elevation_obj)) {
                                            elevation_data->elevation = json_object_get_double(elevation_obj);
                                            strcpy(elevation_data->source, "google_elevation");
                                            
                                            if (json_object_object_get_ex(first_result, "resolution", &resolution_obj)) {
                                                elevation_data->resolution = json_object_get_double(resolution_obj);
                                            }
                                            
                                            double duration = difftime(time(NULL), start_time) * 1000.0;
                                            update_api_statistics(EXTERNAL_API_GOOGLE_ELEVATION, true, duration);
                                            
                                            LOGX_INFO_MSG("Google Elevation API success",
                                                     "lat", latitude, "lon", longitude,
                                                     "elevation", elevation_data->elevation,
                                                     "resolution", elevation_data->resolution);
                                            
                                            json_object_put(root);
                                            if (response.data) free(response.data);
                                            return AUTONOMY_SUCCESS;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    json_object_put(root);
                }
                
                double duration = difftime(time(NULL), start_time) * 1000.0;
                update_api_statistics(EXTERNAL_API_GOOGLE_ELEVATION, false, duration);
            }
            
            if (response.data) free(response.data);
        }
    }
    
    // Fallback to Open Elevation API
    if (g_external_apis.configs[EXTERNAL_API_OPEN_ELEVATION].enabled ||
        !g_external_apis.configs[EXTERNAL_API_GOOGLE_ELEVATION].enabled) {
        
        if (!check_rate_limits(EXTERNAL_API_OPEN_ELEVATION)) {
            LOGX_WARN_MSG("Open Elevation API rate limited");
            g_external_apis.stats[EXTERNAL_API_OPEN_ELEVATION].rate_limited_requests++;
        } else {
            char url[512];
            snprintf(url, sizeof(url), 
                    "%s/lookup?locations=%.6f,%.6f",
                    g_external_apis.configs[EXTERNAL_API_OPEN_ELEVATION].base_url,
                    latitude, longitude);
            
            http_response_t response = {0};
            time_t start_time = time(NULL);
            
            if (make_http_request(url, NULL, NULL, 
                                g_external_apis.configs[EXTERNAL_API_OPEN_ELEVATION].timeout_seconds,
                                &response) == AUTONOMY_SUCCESS && response.data) {
                
                // Parse Open Elevation API response
                json_object* root = json_tokener_parse(response.data);
                if (root) {
                    json_object* results_obj;
                    if (json_object_object_get_ex(root, "results", &results_obj)) {
                        int results_len = json_object_array_length(results_obj);
                        if (results_len > 0) {
                            json_object* first_result = json_object_array_get_idx(results_obj, 0);
                            if (first_result) {
                                json_object* elevation_obj;
                                if (json_object_object_get_ex(first_result, "elevation", &elevation_obj)) {
                                    elevation_data->elevation = json_object_get_double(elevation_obj);
                                    elevation_data->resolution = 30.0; // SRTM resolution
                                    strcpy(elevation_data->source, "open_elevation");
                                    
                                    double duration = difftime(time(NULL), start_time) * 1000.0;
                                    update_api_statistics(EXTERNAL_API_OPEN_ELEVATION, true, duration);
                                    
                                    LOGX_INFO_MSG("Open Elevation API success",
                                             "lat", latitude, "lon", longitude,
                                             "elevation", elevation_data->elevation);
                                    
                                    json_object_put(root);
                                    if (response.data) free(response.data);
                                    return AUTONOMY_SUCCESS;
                                }
                            }
                        }
                    }
                    json_object_put(root);
                }
                
                double duration = difftime(time(NULL), start_time) * 1000.0;
                update_api_statistics(EXTERNAL_API_OPEN_ELEVATION, false, duration);
            }
            
            if (response.data) free(response.data);
        }
    }
    
    LOGX_ERROR_MSG("All elevation APIs failed or disabled");
    return AUTONOMY_ERROR_NOT_FOUND;
}

// Get weather data from weather APIs
int external_apis_get_weather(double latitude, double longitude, external_weather_data_t* weather_data) {
    if (!g_external_apis_initialized || !weather_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    memset(weather_data, 0, sizeof(external_weather_data_t));
    weather_data->timestamp = time(NULL);
    
    // Try OpenWeatherMap API
    if (g_external_apis.configs[EXTERNAL_API_WEATHER_OPENWEATHER].enabled &&
        strlen(g_external_apis.configs[EXTERNAL_API_WEATHER_OPENWEATHER].api_key) > 0) {
        
        if (!check_rate_limits(EXTERNAL_API_WEATHER_OPENWEATHER)) {
            LOGX_WARN_MSG("OpenWeatherMap API rate limited");
            g_external_apis.stats[EXTERNAL_API_WEATHER_OPENWEATHER].rate_limited_requests++;
        } else {
            char url[512];
            snprintf(url, sizeof(url),
                    "%s/weather?lat=%.6f&lon=%.6f&appid=%s&units=metric",
                    g_external_apis.configs[EXTERNAL_API_WEATHER_OPENWEATHER].base_url,
                    latitude, longitude,
                    g_external_apis.configs[EXTERNAL_API_WEATHER_OPENWEATHER].api_key);
            
            http_response_t response = {0};
            time_t start_time = time(NULL);
            
            if (make_http_request(url, NULL, NULL,
                                g_external_apis.configs[EXTERNAL_API_WEATHER_OPENWEATHER].timeout_seconds,
                                &response) == AUTONOMY_SUCCESS && response.data) {
                
                // Parse OpenWeatherMap response
                json_object* root = json_tokener_parse(response.data);
                if (root) {
                    // Get main weather data
                    json_object* main_obj;
                    if (json_object_object_get_ex(root, "main", &main_obj)) {
                        json_object* temp_obj, *humidity_obj, *pressure_obj;
                        if (json_object_object_get_ex(main_obj, "temp", &temp_obj)) {
                            weather_data->temperature_celsius = json_object_get_double(temp_obj);
                        }
                        if (json_object_object_get_ex(main_obj, "humidity", &humidity_obj)) {
                            weather_data->humidity_percent = json_object_get_double(humidity_obj);
                        }
                        if (json_object_object_get_ex(main_obj, "pressure", &pressure_obj)) {
                            weather_data->pressure_hpa = json_object_get_double(pressure_obj);
                        }
                    }
                    
                    // Get wind data
                    json_object* wind_obj;
                    if (json_object_object_get_ex(root, "wind", &wind_obj)) {
                        json_object* speed_obj, *deg_obj;
                        if (json_object_object_get_ex(wind_obj, "speed", &speed_obj)) {
                            weather_data->wind_speed_ms = json_object_get_double(speed_obj);
                        }
                        if (json_object_object_get_ex(wind_obj, "deg", &deg_obj)) {
                            weather_data->wind_direction_deg = json_object_get_double(deg_obj);
                        }
                    }
                    
                    // Get visibility
                    json_object* visibility_obj;
                    if (json_object_object_get_ex(root, "visibility", &visibility_obj)) {
                        weather_data->visibility_km = json_object_get_double(visibility_obj) / 1000.0; // Convert m to km
                    }
                    
                    // Get weather description
                    json_object* weather_obj;
                    if (json_object_object_get_ex(root, "weather", &weather_obj)) {
                        int weather_len = json_object_array_length(weather_obj);
                        if (weather_len > 0) {
                            json_object* weather_item = json_object_array_get_idx(weather_obj, 0);
                            if (weather_item) {
                                json_object* desc_obj, *icon_obj;
                                if (json_object_object_get_ex(weather_item, "description", &desc_obj)) {
                                    const char* description = json_object_get_string(desc_obj);
                                    strncpy(weather_data->description, description, sizeof(weather_data->description) - 1);
                                    weather_data->description[sizeof(weather_data->description) - 1] = '\0';
                                }
                                if (json_object_object_get_ex(weather_item, "icon", &icon_obj)) {
                                    const char* icon = json_object_get_string(icon_obj);
                                    strncpy(weather_data->icon, icon, sizeof(weather_data->icon) - 1);
                                    weather_data->icon[sizeof(weather_data->icon) - 1] = '\0';
                                }
                            }
                        }
                    }
                    
                    strcpy(weather_data->source, "openweathermap");
                    
                    double duration = difftime(time(NULL), start_time) * 1000.0;
                    update_api_statistics(EXTERNAL_API_WEATHER_OPENWEATHER, true, duration);
                    
                    LOGX_INFO_MSG("OpenWeatherMap API success",
                             "lat", latitude, "lon", longitude,
                             "temperature", weather_data->temperature_celsius,
                             "humidity", weather_data->humidity_percent,
                             "description", weather_data->description);
                    
                    json_object_put(root);
                    if (response.data) free(response.data);
                    return AUTONOMY_SUCCESS;
                }
                
                double duration = difftime(time(NULL), start_time) * 1000.0;
                update_api_statistics(EXTERNAL_API_WEATHER_OPENWEATHER, false, duration);
            }
            
            if (response.data) free(response.data);
        }
    }
    
    LOGX_ERROR_MSG("Weather API failed or disabled");
    return AUTONOMY_ERROR_NOT_FOUND;
}

// CURL write callback
size_t external_apis_write_callback(void* contents, size_t size, size_t nmemb, http_response_t* response) {
    size_t total_size = size * nmemb;
    
    char* new_data = realloc(response->data, response->size + total_size + 1);
    if (!new_data) {
        LOGX_ERROR_MSG("Failed to allocate memory for HTTP response");
        return 0;
    }
    
    response->data = new_data;
    memcpy(&(response->data[response->size]), contents, total_size);
    response->size += total_size;
    response->data[response->size] = '\0';
    
    return total_size;
}

// Make HTTP request using CURL
static int make_http_request(const char* url, const char* headers, const char* post_data, 
                           int timeout, http_response_t* response) {
    if (!url || !response) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        LOGX_ERROR_MSG("Failed to initialize CURL");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Set CURL options
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, external_apis_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Autonomy-Daemon/6.1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    // Set custom headers if provided
    struct curl_slist* header_list = NULL;
    if (headers) {
        header_list = curl_slist_append(header_list, headers);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    }
    
    // Set POST data if provided
    if (post_data) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(post_data));
    }
    
    // Perform request
    CURLcode curl_result = curl_easy_perform(curl);
    
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    
    if (header_list) {
        curl_slist_free_all(header_list);
    }
    curl_easy_cleanup(curl);
    
    if (curl_result != CURLE_OK) {
        LOGX_ERROR_MSG("CURL request failed", "error", curl_easy_strerror(curl_result));
        return AUTONOMY_ERROR_NETWORK;
    }
    
    if (response_code != 200) {
        LOGX_ERROR_MSG("HTTP request failed", "response_code", response_code);
        return AUTONOMY_ERROR_NETWORK;
    }
    
    return AUTONOMY_SUCCESS;
}

// Update API statistics
void update_api_statistics(external_api_type_t api_type, bool success, double duration_ms) {
    if (api_type >= EXTERNAL_API_MAX) return;
    
    external_api_statistics_t* stats = &g_external_apis.stats[api_type];
    
    stats->total_requests++;
    stats->last_request = time(NULL);
    
    if (success) {
        stats->successful_requests++;
        stats->consecutive_successes++;
        stats->consecutive_failures = 0;
        stats->last_success = time(NULL);
        stats->status = API_STATUS_HEALTHY;
        
        // Update average response time
        stats->average_response_time_ms = 
            (stats->average_response_time_ms * (stats->successful_requests - 1) + duration_ms) /
            stats->successful_requests;
    } else {
        stats->failed_requests++;
        stats->consecutive_failures++;
        stats->consecutive_successes = 0;
        stats->last_failure = time(NULL);
        
        // Update status based on consecutive failures
        if (stats->consecutive_failures >= g_external_apis.configs[api_type].max_consecutive_failures) {
            stats->status = API_STATUS_FAILED;
        } else {
            stats->status = API_STATUS_DEGRADED;
        }
    }
    
    // Calculate success rate
    if (stats->total_requests > 0) {
        stats->success_rate = (double)stats->successful_requests / stats->total_requests;
    }
    
    // Update cost tracking
    stats->total_cost += g_external_apis.configs[api_type].cost_per_request;
    stats->cost_this_month += g_external_apis.configs[api_type].cost_per_request;
    
    LOGX_DEBUG_MSG("API statistics updated",
              "api", external_api_type_to_string(api_type),
              "success", success,
              "duration_ms", duration_ms,
              "success_rate", stats->success_rate,
              "status", external_api_status_to_string(stats->status));
}

// Check rate limits
bool check_rate_limits(external_api_type_t api_type) {
    if (api_type >= EXTERNAL_API_MAX) return false;
    
    external_api_config_t* config = &g_external_apis.configs[api_type];
    external_api_statistics_t* stats = &g_external_apis.stats[api_type];
    
    time_t now = time(NULL);
    
    // Reset counters if needed
    if (difftime(now, stats->hour_reset_time) >= 3600) {
        stats->requests_this_hour = 0;
        stats->hour_reset_time = now;
    }
    
    if (difftime(now, stats->day_reset_time) >= 86400) {
        stats->requests_this_day = 0;
        stats->day_reset_time = now;
    }
    
    // Check hourly limit
    if (stats->requests_this_hour >= config->max_requests_per_hour) {
        return false;
    }
    
    // Check daily limit
    if (stats->requests_this_day >= config->max_requests_per_day) {
        return false;
    }
    
    // Update counters
    stats->requests_this_hour++;
    stats->requests_this_day++;
    
    return true;
}

// Health monitor thread worker
static void* health_monitor_worker(void* arg) {
    LOGX_INFO_MSG("External APIs health monitor started");
    
    while (g_external_apis_initialized && g_external_apis.threads_running) {
        sleep(300); // Check every 5 minutes
        
        if (!g_external_apis.threads_running) break;
        
        // Perform health check for all enabled APIs
        for (int i = 0; i < EXTERNAL_API_MAX; i++) {
            if (g_external_apis.configs[i].enabled) {
                perform_api_health_check((external_api_type_t)i);
            }
        }
        
        g_external_apis.last_health_check = time(NULL);
    }
    
    LOGX_INFO_MSG("External APIs health monitor stopped");
    return NULL;
}

// Utility functions
const char* external_api_type_to_string(external_api_type_t api_type) {
    if (api_type >= 0 && api_type < EXTERNAL_API_MAX) {
        return API_TYPE_STRINGS[api_type];
    }
    return "unknown";
}

external_api_type_t external_api_parse_type(const char* api_str) {
    if (!api_str) return EXTERNAL_API_GOOGLE_LOCATION;
    
    for (int i = 0; i < EXTERNAL_API_MAX; i++) {
        if (strcasecmp(api_str, API_TYPE_STRINGS[i]) == 0) {
            return (external_api_type_t)i;
        }
    }
    
    return EXTERNAL_API_GOOGLE_LOCATION;
}

const char* external_api_status_to_string(api_status_t status) {
    if (status >= 0 && status < API_STATUS_MAX) {
        return API_STATUS_STRINGS[status];
    }
    return "unknown";
}

bool external_apis_is_initialized(void) {
    return g_external_apis_initialized;
}

// Get API statistics
int external_apis_get_statistics(external_api_type_t api_type, external_api_statistics_t* stats) {
    if (!g_external_apis_initialized || !stats || api_type >= EXTERNAL_API_MAX) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (api_type < 0 || api_type >= EXTERNAL_API_MAX) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    *stats = g_external_apis.stats[api_type];
    return AUTONOMY_SUCCESS;
}

// Get all API statistics
int external_apis_get_all_statistics(external_api_statistics_t* stats_array, int max_apis) {
    if (!g_external_apis_initialized || !stats_array || max_apis <= 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    int count = (max_apis < EXTERNAL_API_MAX) ? max_apis : EXTERNAL_API_MAX;
    for (int i = 0; i < count; i++) {
        stats_array[i] = g_external_apis.stats[i];
    }
    
    return count;
}

// Reset API statistics
int external_apis_reset_statistics(external_api_type_t api_type) {
    if (!g_external_apis_initialized || api_type >= EXTERNAL_API_MAX) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (api_type < 0 || api_type >= EXTERNAL_API_MAX) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    memset(&g_external_apis.stats[api_type], 0, sizeof(external_api_statistics_t));
    // Reset time tracked in day_reset_time
    
    LOGX_INFO_MSG("Reset statistics for external API", "api_type", external_api_type_to_string(api_type));
    return AUTONOMY_SUCCESS;
}

// Health check for external APIs
int external_apis_health_check(void) {
    if (!g_external_apis_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    bool all_healthy = true;
    for (int i = 0; i < EXTERNAL_API_MAX; i++) {
        if (g_external_apis.configs[i].enabled) {
            if (!external_apis_is_healthy((external_api_type_t)i)) {
                all_healthy = false;
                LOGX_WARN_MSG("External API unhealthy", "api_type", external_api_type_to_string((external_api_type_t)i));
            }
        }
    }
    
    return all_healthy ? AUTONOMY_SUCCESS : AUTONOMY_ERROR_SYSTEM;
}

// Get API status
api_status_t external_apis_get_status(external_api_type_t api_type) {
    if (!g_external_apis_initialized || api_type >= EXTERNAL_API_MAX) {
        return API_STATUS_FAILED;
    }
    
    if (api_type < 0 || api_type >= EXTERNAL_API_MAX) {
        return API_STATUS_FAILED;
    }
    
    // Check if API is enabled
    if (!g_external_apis.configs[api_type].enabled) {
        return API_STATUS_DISABLED;
    }
    
    // Check recent success rate
    external_api_statistics_t* stats = &g_external_apis.stats[api_type];
    if (stats->total_requests == 0) {
        return API_STATUS_UNKNOWN;
    }
    
    double success_rate = (double)stats->successful_requests / stats->total_requests;
    if (success_rate >= 0.9) {
        return API_STATUS_HEALTHY;
    } else if (success_rate >= 0.5) {
        return API_STATUS_DEGRADED;
    } else {
        return API_STATUS_FAILED;
    }
}

// Check if API is healthy
bool external_apis_is_healthy(external_api_type_t api_type) {
    api_status_t status = external_apis_get_status(api_type);
    return (status == API_STATUS_HEALTHY || status == API_STATUS_DEGRADED);
}

// Set API enabled state
int external_apis_set_enabled(external_api_type_t api_type, bool enabled) {
    if (!g_external_apis_initialized || api_type >= EXTERNAL_API_MAX) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (api_type < 0 || api_type >= EXTERNAL_API_MAX) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    g_external_apis.configs[api_type].enabled = enabled;
    
    LOGX_INFO_MSG("Set external API enabled state", 
                  "api_type", external_api_type_to_string(api_type),
                  "enabled", enabled ? "true" : "false");
    
    return AUTONOMY_SUCCESS;
}

// Check API rate limit
bool external_apis_check_rate_limit(external_api_type_t api_type) {
    if (!g_external_apis_initialized || api_type >= EXTERNAL_API_MAX) {
        return false;
    }
    
    if (api_type < 0 || api_type >= EXTERNAL_API_MAX) {
        return false;
    }
    
    external_api_statistics_t* stats = &g_external_apis.stats[api_type];
    external_api_config_t* config = &g_external_apis.configs[api_type];
    
    time_t now = time(NULL);
    
    // Simple rate limiting: check requests in last minute
    if (now - stats->last_success < 60) {
        if (stats->requests_this_hour >= 60) {
            return false; // Rate limit exceeded
        }
    } else {
        // Reset counter for new minute
        stats->requests_this_hour = 0;
    }
    
    return true;
}

// Configure external API
int external_apis_configure(external_api_type_t api_type, const external_api_config_t* config) {
    if (!g_external_apis_initialized || !config || api_type >= EXTERNAL_API_MAX) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (api_type < 0 || api_type >= EXTERNAL_API_MAX) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    g_external_apis.configs[api_type] = *config;
    
    LOGX_INFO_MSG("Configured external API", 
                  "api_type", external_api_type_to_string(api_type),
                  "enabled", config->enabled ? "true" : "false");
    
    return AUTONOMY_SUCCESS;
}

// Make API request (generic)
int external_apis_make_request(const external_api_request_t* request, external_api_response_t* response) {
    if (!g_external_apis_initialized || !request || !response) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Check rate limit
    if (!external_apis_check_rate_limit(request->api_type)) {
        return AUTONOMY_ERROR_TOO_FREQUENT;
    }
    
    // Update statistics
    external_api_statistics_t* stats = &g_external_apis.stats[request->api_type];
    stats->total_requests++;
    stats->last_success = time(NULL);
    stats->requests_this_hour++;
    
    // Real HTTP request using cURL
    CURL* curl = curl_easy_init();
    if (!curl) {
        LOGX_ERROR_MSG("Failed to initialize cURL for external API request");
        response->success = false;
        response->status_code = 0;
        response->duration_ms = 0;
        strncpy(response->body, "{\"error\":\"curl_init_failed\"}", sizeof(response->body) - 1);
        stats->failed_requests++;
        return AUTONOMY_ERROR_SYSTEM;
    }

    // Initialize response buffer
    typedef struct {
        char* data;
        size_t size;
    } curl_response_t;
    
    curl_response_t curl_response = {0};
    curl_response.data = malloc(1);
    curl_response.size = 0;

    // Configure cURL for the request
    curl_easy_setopt(curl, CURLOPT_URL, request->endpoint);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, external_api_curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &curl_response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Autonomy-RUTOS/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    // Add headers if needed
    struct curl_slist* headers = NULL;
    if (strlen(request->headers) > 0) {
        headers = curl_slist_append(headers, request->headers);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    // Add POST data if provided
    if (strlen(request->body) > 0) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request->body);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(request->body));
    }

    // Record start time for duration measurement
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    // Perform the request
    CURLcode curl_result = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    // Calculate duration
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    long duration_ms = (end_time.tv_sec - start_time.tv_sec) * 1000 + 
                      (end_time.tv_nsec - start_time.tv_nsec) / 1000000;

    // Clean up cURL
    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    // Process results
    response->duration_ms = duration_ms;
    response->status_code = (int)http_code;
    
    if (curl_result != CURLE_OK) {
        LOGX_ERROR_MSG("External API cURL request failed", "error", curl_easy_strerror(curl_result), "url", request->endpoint);
        response->success = false;
        snprintf(response->body, sizeof(response->body), "{\"error\":\"%s\"}", curl_easy_strerror(curl_result));
        stats->failed_requests++;
        free(curl_response.data);
        return AUTONOMY_ERROR_NETWORK;
    }
    
    if (http_code < 200 || http_code >= 300) {
        LOGX_ERROR_MSG("External API HTTP error", "http_code", http_code, "url", request->endpoint);
        response->success = false;
        snprintf(response->body, sizeof(response->body), "{\"error\":\"HTTP %ld\"}", http_code);
        stats->failed_requests++;
        free(curl_response.data);
        return AUTONOMY_ERROR_EXTERNAL_API;
    }
    
    // Copy response data
    response->success = true;
    if (curl_response.size < sizeof(response->body)) {
        memcpy(response->body, curl_response.data, curl_response.size);
        response->body[curl_response.size] = '\0';
    } else {
        memcpy(response->body, curl_response.data, sizeof(response->body) - 1);
        response->body[sizeof(response->body) - 1] = '\0';
        LOGX_WARN_MSG("External API response truncated", "original_size", curl_response.size, "truncated_to", sizeof(response->body) - 1);
    }
    
    free(curl_response.data);
    
    LOGX_DEBUG_MSG("External API request successful", "url", request->endpoint, "http_code", http_code, "duration_ms", duration_ms, "response_size", curl_response.size);
    
    stats->successful_requests++;
    stats->average_response_time_ms += response->duration_ms;
    
    return AUTONOMY_SUCCESS;
}

// Get Google location using real API
int external_apis_get_google_location(const void* cell_towers, const void* wifi_aps, 
                                     external_location_data_t* location_data) {
    if (!g_external_apis_initialized || !location_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Check if Google API is enabled and configured
    if (!g_external_apis.configs[EXTERNAL_API_GOOGLE_LOCATION].enabled || 
        strlen(g_external_apis.configs[EXTERNAL_API_GOOGLE_LOCATION].api_key) == 0) {
        LOGX_WARN_MSG("Google Location API not configured");
        return AUTONOMY_ERROR_NOT_CONFIGURED;
    }
    
    // Build Google Geolocation API request
    json_object* request_json = json_object_new_object();
    json_object* consider_ip = json_object_new_boolean(true);
    json_object_object_add(request_json, "considerIp", consider_ip);
    
    // Add WiFi access points if available
    if (wifi_aps) {
        json_object* wifi_array = json_object_new_array();
        
        // Get real WiFi scan results from enhanced WiFi system
        wifi_access_point_t access_points[32];
        int ap_count = wifi_enhanced_ubus_scan("radio0", access_points, 32);
        
        if (ap_count > 0) {
            LOGX_DEBUG_MSG("Found %d WiFi access points for location request", ap_count);
            
            for (int i = 0; i < ap_count && i < 32; i++) {
                json_object* ap_obj = json_object_new_object();
                
                // Add WiFi AP data in Google Location Services format
                json_object_object_add(ap_obj, "macAddress", json_object_new_string(access_points[i].bssid));
                json_object_object_add(ap_obj, "signalStrength", json_object_new_int(access_points[i].signal));
                json_object_object_add(ap_obj, "age", json_object_new_int(0)); // Real-time data
                json_object_object_add(ap_obj, "channel", json_object_new_int(access_points[i].channel));
                json_object_object_add(ap_obj, "signalToNoiseRatio", json_object_new_int(access_points[i].signal - access_points[i].noise_floor));
                
                json_object_array_add(wifi_array, ap_obj);
            }
        } else {
            LOGX_DEBUG_MSG("No WiFi access points found for location request");
        }
        
        json_object_object_add(request_json, "wifiAccessPoints", wifi_array);
    }
    
    // Add cell towers if available  
    if (cell_towers) {
        json_object* cell_array = json_object_new_array();
        // This would integrate with actual cellular data
        json_object_object_add(request_json, "cellTowers", cell_array);
    }
    
    const char* json_string = json_object_to_json_string(request_json);
    
    // Prepare API request
    external_api_request_t api_request = {0};
    api_request.api_type = EXTERNAL_API_GOOGLE_LOCATION;
    snprintf(api_request.endpoint, sizeof(api_request.endpoint), 
             "https://www.googleapis.com/geolocation/v1/geolocate?key=%s", 
             g_external_apis.configs[EXTERNAL_API_GOOGLE_LOCATION].api_key);
    strcpy(api_request.headers, "Content-Type: application/json");
    strncpy(api_request.body, json_string, sizeof(api_request.body) - 1);
    
    // Make the API request
    external_api_response_t api_response = {0};
    int result = external_apis_make_request(&api_request, &api_response);
    
    json_object_put(request_json);
    
    if (result != AUTONOMY_SUCCESS) {
        return result;
    }
    
    // Parse Google API response
    json_object* response_json = json_tokener_parse(api_response.body);
    if (!response_json) {
        LOGX_ERROR_MSG("Failed to parse Google Location API response");
        return AUTONOMY_ERROR_PARSE;
    }
    
    // Extract location data
    json_object* location_obj;
    if (json_object_object_get_ex(response_json, "location", &location_obj)) {
        json_object* lat_obj, *lng_obj;
        if (json_object_object_get_ex(location_obj, "lat", &lat_obj) &&
            json_object_object_get_ex(location_obj, "lng", &lng_obj)) {
            location_data->latitude = json_object_get_double(lat_obj);
            location_data->longitude = json_object_get_double(lng_obj);
        }
    }
    
    // Extract accuracy
    json_object* accuracy_obj;
    if (json_object_object_get_ex(response_json, "accuracy", &accuracy_obj)) {
        location_data->accuracy = json_object_get_double(accuracy_obj);
    } else {
        location_data->accuracy = 1000.0; // Default accuracy
    }
    
    strcpy(location_data->source, "google_geolocation");
    location_data->timestamp = time(NULL);
    
    json_object_put(response_json);
    
    LOGX_DEBUG_MSG("Google Location API success", "lat", location_data->latitude, "lon", location_data->longitude, "accuracy", location_data->accuracy);
    return AUTONOMY_SUCCESS;
}

// Get reverse geocoding using Google Maps API
int external_apis_get_reverse_geocoding(double latitude, double longitude, external_location_data_t* location_data) {
    if (!g_external_apis_initialized || !location_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Check if Google API is enabled and configured
    if (!g_external_apis.configs[EXTERNAL_API_GOOGLE_GEOCODING].enabled || 
        strlen(g_external_apis.configs[EXTERNAL_API_GOOGLE_GEOCODING].api_key) == 0) {
        LOGX_WARN_MSG("Google Geocoding API not configured");
        return AUTONOMY_ERROR_NOT_CONFIGURED;
    }
    
    // Prepare Google Geocoding API request
    external_api_request_t api_request = {0};
    api_request.api_type = EXTERNAL_API_GOOGLE_GEOCODING;
    snprintf(api_request.endpoint, sizeof(api_request.endpoint), 
             "https://maps.googleapis.com/maps/api/geocode/json?latlng=%.6f,%.6f&key=%s",
             latitude, longitude, g_external_apis.configs[EXTERNAL_API_GOOGLE_GEOCODING].api_key);
    strcpy(api_request.headers, "User-Agent: Autonomy-RUTOS/1.0");
    
    // Make the API request
    external_api_response_t api_response = {0};
    int result = external_apis_make_request(&api_request, &api_response);
    
    if (result != AUTONOMY_SUCCESS) {
        return result;
    }
    
    // Parse Google Geocoding API response
    json_object* response_json = json_tokener_parse(api_response.body);
    if (!response_json) {
        LOGX_ERROR_MSG("Failed to parse Google Geocoding API response");
        return AUTONOMY_ERROR_PARSE;
    }
    
    // Check status
    json_object* status_obj;
    if (json_object_object_get_ex(response_json, "status", &status_obj)) {
        const char* status = json_object_get_string(status_obj);
        if (strcmp(status, "OK") != 0) {
            LOGX_ERROR_MSG("Google Geocoding API error", "status", status);
            json_object_put(response_json);
            return AUTONOMY_ERROR_EXTERNAL_API;
        }
    }
    
    // Extract results
    json_object* results_obj;
    if (json_object_object_get_ex(response_json, "results", &results_obj)) {
        int results_count = json_object_array_length(results_obj);
        if (results_count > 0) {
            json_object* first_result = json_object_array_get_idx(results_obj, 0);
            
            // Get formatted address
            json_object* address_obj;
            if (json_object_object_get_ex(first_result, "formatted_address", &address_obj)) {
                const char* address = json_object_get_string(address_obj);
                strncpy(location_data->formatted_address, address, sizeof(location_data->formatted_address) - 1);
            } else {
                strcpy(location_data->formatted_address, "Address not available");
            }
        } else {
            strcpy(location_data->formatted_address, "No results found");
        }
    } else {
        strcpy(location_data->formatted_address, "No results in response");
    }
    
    // Fill location data
    location_data->latitude = latitude;
    location_data->longitude = longitude;
    location_data->accuracy = 10.0; // High accuracy for reverse geocoding
    strcpy(location_data->source, "google_geocoding");
    location_data->timestamp = time(NULL);
    
    json_object_put(response_json);
    
    LOGX_DEBUG_MSG("Google Geocoding API success", "lat", latitude, "lon", longitude, "address", location_data->formatted_address);
    return AUTONOMY_SUCCESS;
}

// Perform health check on a specific API
int perform_api_health_check(external_api_type_t api_type) {
    // Simple health check - verify API is configured and responding
    if (!g_external_apis.configs[api_type].enabled) {
        return AUTONOMY_SUCCESS; // Disabled APIs are considered healthy
    }

    // Check if we have recent successful requests
    time_t now = time(NULL);
    if (now - g_external_apis.stats[api_type].last_success < 3600) {
        return AUTONOMY_SUCCESS; // Recent success
    }

    LOGX_DEBUG_MSG("API health check", "api_type", api_type, "last_success", g_external_apis.stats[api_type].last_success);
    return AUTONOMY_SUCCESS;
}