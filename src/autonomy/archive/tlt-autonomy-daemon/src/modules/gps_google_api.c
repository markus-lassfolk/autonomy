#include "gps_google_api.h"
#include "logx.h"
#include "types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <curl/curl.h>
#include <json-c/json.h>

// Google API configuration
static const int MAX_API_REQUESTS = 1000;                   // Maximum API requests per day
static const int REQUEST_TIMEOUT = 30;                      // 30 second request timeout
static const int MAX_RESPONSE_SIZE = 16384;                 // 16KB max response size
static const int RATE_LIMIT_DELAY = 100;                    // 100ms delay between requests
static const char* GOOGLE_API_BASE_URL = "https://maps.googleapis.com/maps/api";

// Google API endpoints
static const char* GOOGLE_REVERSE_GEOCODE_ENDPOINT = "/geocode/json";
static const char* GOOGLE_PLACE_DETAILS_ENDPOINT = "/place/details/json";
static const char* GOOGLE_PLACE_SEARCH_ENDPOINT = "/place/nearbysearch/json";
static const char* GOOGLE_ELEVATION_ENDPOINT = "/elevation/json";
static const char* GOOGLE_TIMEZONE_ENDPOINT = "/timezone/json";

// Global Google API state
static gps_google_api_t g_google_api = {0};
static bool g_google_api_initialized = false;
static pthread_mutex_t g_google_api_mutex = PTHREAD_MUTEX_INITIALIZER;

// CURL write callback for response data
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    gps_google_api_response_t *response = (gps_google_api_response_t *)userp;
    
    if (response->data_size + realsize >= MAX_RESPONSE_SIZE) {
        LOGX_WARN("Response too large, truncating");
        return 0;
    }
    
    memcpy(&response->data[response->data_size], contents, realsize);
    response->data_size += realsize;
    response->data[response->data_size] = '\0';
    
    return realsize;
}

// Initialize Google Location API
int gps_google_api_init(const char *api_key) {
    if (g_google_api_initialized) {
        LOGX_WARN("Google Location API already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    if (!api_key || strlen(api_key) == 0) {
        LOGX_ERROR("Google API key is required");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_google_api_mutex);
    
    // Initialize Google API state
    memset(&g_google_api, 0, sizeof(gps_google_api_t));
    g_google_api.enabled = true;
    g_google_api.max_requests = MAX_API_REQUESTS;
    g_google_api.request_timeout = REQUEST_TIMEOUT;
    g_google_api.rate_limit_delay = RATE_LIMIT_DELAY;
    
    strncpy(g_google_api.api_key, api_key, sizeof(g_google_api.api_key) - 1);
    
    g_google_api.request_count = 0;
    g_google_api.last_request = 0;
    g_google_api.total_requests = 0;
    g_google_api.successful_requests = 0;
    g_google_api.failed_requests = 0;
    
    // Initialize CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    pthread_mutex_unlock(&g_google_api_mutex);
    
    LOGX_INFO("Google Location API initialized successfully with key: %s...", 
               g_google_api.api_key[0] ? "***" : "none");
    return AUTONOMY_SUCCESS;
}

// Perform HTTP request to Google API
static int perform_google_api_request(const char *endpoint, const char *params, 
                                    gps_google_api_response_t *response) {
    if (!g_google_api.enabled) {
        return AUTONOMY_ERROR_NOT_ENABLED;
    }
    
    // Check rate limiting
    time_t now = time(NULL);
    if (g_google_api.last_request > 0) {
        int time_since_last = (int)(now - g_google_api.last_request);
        if (time_since_last < (g_google_api.rate_limit_delay / 1000)) {
            usleep((g_google_api.rate_limit_delay - (time_since_last * 1000)) * 1000);
        }
    }
    
    // Check daily request limit
    if (g_google_api.request_count >= g_google_api.max_requests) {
        LOGX_ERROR("Daily API request limit reached (%d)", g_google_api.max_requests);
        return AUTONOMY_ERROR_RATE_LIMITED;
    }
    
    pthread_mutex_lock(&g_google_api_mutex);
    
    // Initialize response
    memset(response, 0, sizeof(gps_google_api_response_t));
    response->timestamp = now;
    
    // Build full URL
    char url[1024];
    snprintf(url, sizeof(url), "%s%s?%s&key=%s", 
             GOOGLE_API_BASE_URL, endpoint, params, g_google_api.api_key);
    
    // Initialize CURL
    CURL *curl = curl_easy_init();
    if (!curl) {
        pthread_mutex_unlock(&g_google_api_mutex);
        LOGX_ERROR("Failed to initialize CURL");
        return AUTONOMY_ERROR_INTERNAL;
    }
    
    // Set CURL options
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, g_google_api.request_timeout);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Autonomy-Daemon/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
    
    // Perform request
    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    // Update statistics
    g_google_api.request_count++;
    g_google_api.total_requests++;
    g_google_api.last_request = now;
    
    if (res == CURLE_OK && http_code == 200) {
        g_google_api.successful_requests++;
        response->success = true;
        response->http_code = http_code;
        
        LOGX_DEBUG("Google API request successful: %s", endpoint);
    } else {
        g_google_api.failed_requests++;
        response->success = false;
        response->http_code = http_code;
        response->error_code = res;
        
        LOGX_ERROR("Google API request failed: %s, HTTP: %ld, CURL: %d", 
                   endpoint, http_code, res);
    }
    
    curl_easy_cleanup(curl);
    pthread_mutex_unlock(&g_google_api_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Reverse geocoding using Google API
int gps_google_api_reverse_geocode(double lat, double lon, 
                                  gps_google_location_info_t *location_info) {
    if (!g_google_api_initialized || !location_info) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Build request parameters
    char params[512];
    snprintf(params, sizeof(params), "latlng=%.6f,%.6f&result_type=street_address|route|premise|subpremise|neighborhood|sublocality|locality|administrative_area_level_1|country", 
             lat, lon);
    
    // Perform API request
    gps_google_api_response_t response;
    int result = perform_google_api_request(GOOGLE_REVERSE_GEOCODE_ENDPOINT, params, &response);
    if (result != AUTONOMY_SUCCESS) {
        return result;
    }
    
    if (!response.success) {
        return AUTONOMY_ERROR_API_FAILED;
    }
    
    // Parse response (simplified - in a real implementation, this would parse JSON)
    parse_reverse_geocode_response(&response, location_info);
    
    return AUTONOMY_SUCCESS;
}

// Parse reverse geocoding response
static void parse_reverse_geocode_response(const gps_google_api_response_t *response, 
                                         gps_google_location_info_t *location_info) {
    // Initialize location info
    memset(location_info, 0, sizeof(gps_google_location_info_t));
    
    // Parse JSON response using json-c library
    json_object *root = json_tokener_parse(response->data);
    if (!root) {
        LOGX_ERROR("Failed to parse Google Geocoding API JSON response");
        return;
    }
    
    location_info->timestamp = response->timestamp;
    
    // Check status
    json_object *status_obj;
    if (json_object_object_get_ex(root, "status", &status_obj)) {
        const char *status = json_object_get_string(status_obj);
        if (strcmp(status, "OK") != 0) {
            LOGX_ERROR("Google Geocoding API error", "status", status);
            json_object_put(root);
            return;
        }
    }
    
    // Get results array
    json_object *results_obj;
    if (json_object_object_get_ex(root, "results", &results_obj)) {
        int results_len = json_object_array_length(results_obj);
        if (results_len > 0) {
            json_object *first_result = json_object_array_get_idx(results_obj, 0);
            if (first_result) {
                // Get formatted address
                json_object *formatted_addr_obj;
                if (json_object_object_get_ex(first_result, "formatted_address", &formatted_addr_obj)) {
                    const char *formatted_addr = json_object_get_string(formatted_addr_obj);
                    strncpy(location_info->formatted_address, formatted_addr, 
                           sizeof(location_info->formatted_address) - 1);
                }
                
                // Get place ID
                json_object *place_id_obj;
                if (json_object_object_get_ex(first_result, "place_id", &place_id_obj)) {
                    const char *place_id = json_object_get_string(place_id_obj);
                    strncpy(location_info->place_id, place_id, sizeof(location_info->place_id) - 1);
                }
                
                // Parse address components
                json_object *components_obj;
                if (json_object_object_get_ex(first_result, "address_components", &components_obj)) {
                    int components_len = json_object_array_length(components_obj);
                    
                    for (int i = 0; i < components_len; i++) {
                        json_object *component = json_object_array_get_idx(components_obj, i);
                        if (!component) continue;
                        
                        json_object *types_obj, *long_name_obj, *short_name_obj;
                        if (json_object_object_get_ex(component, "types", &types_obj) &&
                            json_object_object_get_ex(component, "long_name", &long_name_obj)) {
                            
                            const char *long_name = json_object_get_string(long_name_obj);
                            int types_len = json_object_array_length(types_obj);
                            
                            for (int j = 0; j < types_len; j++) {
                                json_object *type_obj = json_object_array_get_idx(types_obj, j);
                                if (!type_obj) continue;
                                
                                const char *type = json_object_get_string(type_obj);
                                
                                if (strcmp(type, "street_number") == 0) {
                                    strncpy(location_info->street_number, long_name, sizeof(location_info->street_number) - 1);
                                } else if (strcmp(type, "route") == 0) {
                                    strncpy(location_info->route, long_name, sizeof(location_info->route) - 1);
                                } else if (strcmp(type, "locality") == 0) {
                                    strncpy(location_info->locality, long_name, sizeof(location_info->locality) - 1);
                                } else if (strcmp(type, "administrative_area_level_1") == 0) {
                                    strncpy(location_info->administrative_area, long_name, sizeof(location_info->administrative_area) - 1);
                                } else if (strcmp(type, "country") == 0) {
                                    strncpy(location_info->country, long_name, sizeof(location_info->country) - 1);
                                } else if (strcmp(type, "postal_code") == 0) {
                                    strncpy(location_info->postal_code, long_name, sizeof(location_info->postal_code) - 1);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    json_object_put(root);
    LOGX_DEBUG("Parsed Google Geocoding response",
              "country", location_info->country,
              "city", location_info->locality,
              "address", location_info->formatted_address);
    
    LOGX_DEBUG("Parsed reverse geocoding response");
}

// Get place details using Google API
int gps_google_api_get_place_details(const char *place_id, 
                                    gps_google_place_details_t *place_details) {
    if (!g_google_api_initialized || !place_id || !place_details) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Build request parameters
    char params[512];
    snprintf(params, sizeof(params), "place_id=%s&fields=name,formatted_address,geometry,types,place_id,photos,formatted_phone_number,website,rating,opening_hours,price_level", 
             place_id);
    
    // Perform API request
    gps_google_api_response_t response;
    int result = perform_google_api_request(GOOGLE_PLACE_DETAILS_ENDPOINT, params, &response);
    if (result != AUTONOMY_SUCCESS) {
        return result;
    }
    
    if (!response.success) {
        return AUTONOMY_ERROR_API_FAILED;
    }
    
    // Parse response
    parse_place_details_response(&response, place_details);
    
    return AUTONOMY_SUCCESS;
}

// Parse place details response
static void parse_place_details_response(const gps_google_api_response_t *response, 
                                       gps_google_place_details_t *place_details) {
    // Initialize place details
    memset(place_details, 0, sizeof(gps_google_place_details_t));
    
    // This is a simplified parser - in a real implementation, you would use a JSON library
    place_details->timestamp = response->timestamp;
    
    LOGX_DEBUG("Parsed place details response");
}

// Search for places near a location
int gps_google_api_place_search(double lat, double lon, double radius, 
                               const char *type, gps_google_place_search_t *search_results) {
    if (!g_google_api_initialized || !search_results) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Build request parameters
    char params[512];
    snprintf(params, sizeof(params), "location=%.6f,%.6f&radius=%.0f&type=%s", 
             lat, lon, radius, type ? type : "establishment");
    
    // Perform API request
    gps_google_api_response_t response;
    int result = perform_google_api_request(GOOGLE_PLACE_SEARCH_ENDPOINT, params, &response);
    if (result != AUTONOMY_SUCCESS) {
        return result;
    }
    
    if (!response.success) {
        return AUTONOMY_ERROR_API_FAILED;
    }
    
    // Parse response
    parse_place_search_response(&response, search_results);
    
    return AUTONOMY_SUCCESS;
}

// Parse place search response
static void parse_place_search_response(const gps_google_api_response_t *response, 
                                      gps_google_place_search_t *search_results) {
    // Initialize search results
    memset(search_results, 0, sizeof(gps_google_place_search_t));
    
    // This is a simplified parser - in a real implementation, you would use a JSON library
    search_results->timestamp = response->timestamp;
    search_results->result_count = 0;
    
    LOGX_DEBUG("Parsed place search response");
}

// Get elevation data using Google API
int gps_google_api_get_elevation(double lat, double lon, double *elevation) {
    if (!g_google_api_initialized || !elevation) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Build request parameters
    char params[512];
    snprintf(params, sizeof(params), "locations=%.6f,%.6f", lat, lon);
    
    // Perform API request
    gps_google_api_response_t response;
    int result = perform_google_api_request(GOOGLE_ELEVATION_ENDPOINT, params, &response);
    if (result != AUTONOMY_SUCCESS) {
        return result;
    }
    
    if (!response.success) {
        return AUTONOMY_ERROR_API_FAILED;
    }
    
    // Parse JSON response using json-c
    json_object *root = json_tokener_parse(response->data);
    if (!root) {
        LOGX_ERROR("Failed to parse Google Elevation API JSON response");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Check status
    json_object *status_obj;
    if (json_object_object_get_ex(root, "status", &status_obj)) {
        const char *status = json_object_get_string(status_obj);
        if (strcmp(status, "OK") != 0) {
            LOGX_ERROR("Google Elevation API error", "status", status);
            json_object_put(root);
            return AUTONOMY_ERROR_SYSTEM;
        }
    }
    
    // Get results array
    json_object *results_obj;
    if (json_object_object_get_ex(root, "results", &results_obj)) {
        int results_len = json_object_array_length(results_obj);
        if (results_len > 0) {
            json_object *first_result = json_object_array_get_idx(results_obj, 0);
            if (first_result) {
                json_object *elevation_obj;
                if (json_object_object_get_ex(first_result, "elevation", &elevation_obj)) {
                    *elevation = json_object_get_double(elevation_obj);
                    LOGX_DEBUG("Google Elevation API success", "elevation", *elevation);
                    json_object_put(root);
                    return AUTONOMY_SUCCESS;
                }
            }
        }
    }
    
    json_object_put(root);
    LOGX_ERROR("No elevation data in Google API response");
    
    return AUTONOMY_SUCCESS;
}

// Get timezone information using Google API
int gps_google_api_get_timezone(double lat, double lon, time_t timestamp, 
                                gps_google_timezone_info_t *timezone_info) {
    if (!g_google_api_initialized || !timezone_info) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Build request parameters
    char params[512];
    snprintf(params, sizeof(params), "location=%.6f,%.6f&timestamp=%ld", lat, lon, timestamp);
    
    // Perform API request
    gps_google_api_response_t response;
    int result = perform_google_api_request(GOOGLE_TIMEZONE_ENDPOINT, params, &response);
    if (result != AUTONOMY_SUCCESS) {
        return result;
    }
    
    if (!response.success) {
        return AUTONOMY_ERROR_API_FAILED;
    }
    
    // Parse response (simplified)
    memset(timezone_info, 0, sizeof(gps_google_timezone_info_t));
    timezone_info->timestamp = timestamp;
    
    return AUTONOMY_SUCCESS;
}

// Get Google API status
int gps_google_api_get_status(gps_google_api_status_t *status) {
    if (!g_google_api_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_google_api_mutex);
    
    status->enabled = g_google_api.enabled;
    status->request_count = g_google_api.request_count;
    status->max_requests = g_google_api.max_requests;
    status->last_request = g_google_api.last_request;
    status->total_requests = g_google_api.total_requests;
    status->successful_requests = g_google_api.successful_requests;
    status->failed_requests = g_google_api.failed_requests;
    
    // Calculate success rate
    if (g_google_api.total_requests > 0) {
        status->success_rate = (double)g_google_api.successful_requests / g_google_api.total_requests;
    } else {
        status->success_rate = 0.0;
    }
    
    pthread_mutex_unlock(&g_google_api_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get Google API configuration
int gps_google_api_get_config(gps_google_api_config_t *config) {
    if (!g_google_api_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_google_api_mutex);
    
    config->enabled = g_google_api.enabled;
    config->max_requests = g_google_api.max_requests;
    config->request_timeout = g_google_api.request_timeout;
    config->rate_limit_delay = g_google_api.rate_limit_delay;
    strncpy(config->api_key, g_google_api.api_key, sizeof(config->api_key) - 1);
    
    pthread_mutex_unlock(&g_google_api_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set Google API configuration
int gps_google_api_set_config(const gps_google_api_config_t *config) {
    if (!g_google_api_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_google_api_mutex);
    
    g_google_api.enabled = config->enabled;
    g_google_api.max_requests = config->max_requests;
    g_google_api.request_timeout = config->request_timeout;
    g_google_api.rate_limit_delay = config->rate_limit_delay;
    
    if (strlen(config->api_key) > 0) {
        strncpy(g_google_api.api_key, config->api_key, sizeof(g_google_api.api_key) - 1);
    }
    
    pthread_mutex_unlock(&g_google_api_mutex);
    
    LOGX_INFO("Google Location API configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable Google API
int gps_google_api_set_enabled(bool enabled) {
    if (!g_google_api_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_google_api_mutex);
    g_google_api.enabled = enabled;
    pthread_mutex_unlock(&g_google_api_mutex);
    
    LOGX_INFO("Google Location API %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Reset Google API statistics
int gps_google_api_reset_stats(void) {
    if (!g_google_api_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_google_api_mutex);
    
    g_google_api.request_count = 0;
    g_google_api.total_requests = 0;
    g_google_api.successful_requests = 0;
    g_google_api.failed_requests = 0;
    
    pthread_mutex_unlock(&g_google_api_mutex);
    
    LOGX_INFO("Google Location API statistics reset");
    return AUTONOMY_SUCCESS;
}

// Cleanup Google API
void gps_google_api_cleanup(void) {
    if (!g_google_api_initialized) {
        return;
    }
    
    curl_global_cleanup();
    pthread_mutex_destroy(&g_google_api_mutex);
    g_google_api_initialized = false;
    
    LOGX_INFO("Google Location API cleaned up");
}
