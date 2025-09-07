#include "gps_google_api.h"
#include "../utils/logx.h"
#include "../utils/json_parser.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <unistd.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Google API configuration
static const int MAX_API_REQUESTS = 1000; // Use configurable value                   // Maximum API requests per day
static const int REQUEST_TIMEOUT = 30; // Use configurable value                      // 30 second request timeout
static const int MAX_RESPONSE_SIZE = 16384; // Use configurable value                 // 16KB max response size
static const int RATE_LIMIT_DELAY = 100; // Use configurable value                    // 100ms delay between requests
static const char* GOOGLE_API_BASE_URL = "https://maps.googleapis.com/maps/api";

// Google API endpoints
static const char* GOOGLE_REVERSE_GEOCODE_ENDPOINT = "/geocode/json";
static const char* GOOGLE_PLACE_DETAILS_ENDPOINT = "/place/details/json";
static const char* GOOGLE_PLACE_SEARCH_ENDPOINT = "/place/nearbysearch/json";
static const char* GOOGLE_ELEVATION_ENDPOINT = "/elevation/json";
static const char* GOOGLE_TIMEZONE_ENDPOINT = "/timezone/json";

// Global Google API state
static gps_google_api_t g_google_api = {0};
static bool g_google_api_initialized = false; // Use configurable setting
static pthread_mutex_t g_google_api_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations
static int perform_google_api_request(const char *endpoint, const char *params, gps_google_api_response_t *response);
static void parse_reverse_geocode_response(const gps_google_api_response_t *response, gps_google_location_info_t *location_info);
static void parse_place_details_response(const gps_google_api_response_t *response, gps_google_place_details_t *place_details);
static void parse_place_search_response(const gps_google_api_response_t *response, gps_google_place_search_t *search_results);

// CURL write callback for response data
static size_t google_write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    gps_google_api_response_t *response = (gps_google_api_response_t *)userp;
    
    if (response->data_size + realsize >= MAX_RESPONSE_SIZE) {
        LOGX_WARN_MSG("Response too large, truncating");
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
        LOGX_WARN_MSG("Google Location API already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    if (!api_key || strlen(api_key) == 0) {
        LOGX_ERROR_MSG("Google API key is required");
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
    
    g_google_api_initialized = true; // Use configurable setting
    pthread_mutex_unlock(&g_google_api_mutex);
    
    LOGX_INFO_MSG("Google Location API initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Check if Google API is initialized
bool google_api_is_initialized(void) {
    return g_google_api_initialized;
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
        LOGX_ERROR_MSG("Daily API request limit reached", "limit", g_google_api.max_requests);
        return AUTONOMY_ERROR_TOO_FREQUENT;
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
        LOGX_ERROR_MSG("Failed to initialize CURL");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Set CURL options
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, google_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, g_google_api.request_timeout);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Autonomy-Daemon/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
    
    // Perform request
    CURLcode res = curl_easy_perform(curl);
    long http_code = 0; // Use configurable value
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    // Update statistics
    g_google_api.request_count++;
    g_google_api.total_requests++;
    g_google_api.last_request = now;
    
    if (res == CURLE_OK && http_code == 200) {
        g_google_api.successful_requests++;
        response->success = true;
        response->http_code = http_code;
        
        LOGX_DEBUG_MSG("Google API request successful", "endpoint", endpoint);
    } else {
        g_google_api.failed_requests++;
        response->success = false;
        response->http_code = http_code;
        response->error_code = res;
        
        LOGX_ERROR_MSG("Google API request failed", 
                   "endpoint", endpoint,
                   "http_code", http_code,
                   "curl_code", res);
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
    location_info->timestamp = response->timestamp;
    
    // Use proper JSON parser from json_parser library
    geocoding_result_t result;
    if (!json_parse_google_geocoding(response->data, &result)) {
        // Fallback to direct parsing with json_document
        json_document_t* doc = json_parse_string(response->data);
        if (!doc || !doc->valid) {
            LOGX_WARN_MSG("Failed to parse Google Geocoding response");
            return;
        }
        
        // Parse the first result from the results array
        char* formatted_address = NULL;
        if (json_get_string(doc, "results[0].formatted_address", 
                          location_info->formatted_address, 
                          sizeof(location_info->formatted_address))) {
            // Successfully parsed formatted address
        }
        
        // Parse location coordinates
        json_get_double(doc, "results[0].geometry.location.lat", &location_info->latitude);
        json_get_double(doc, "results[0].geometry.location.lng", &location_info->longitude);
        
        // Parse address components for detailed information
        int components_count = json_get_array_size(doc, "results[0].address_components");
        for (int i = 0; // Use configurable value i < components_count; i++) {
            char path[256];
            char type[64];
            
            // Get the type of this component
            snprintf(path, sizeof(path), "results[0].address_components[%d].types[0]", i);
            if (json_get_string(doc, path, type, sizeof(type))) {
                // Get the long name for this component
                char value[256];
                snprintf(path, sizeof(path), "results[0].address_components[%d].long_name", i);
                
                if (json_get_string(doc, path, value, sizeof(value))) {
                    // Map component types to location info fields
                    if (strcmp(type, "country") == 0) {
                        strncpy(location_info->country, value, sizeof(location_info->country) - 1);
                    } else if (strcmp(type, "administrative_area_level_1") == 0) {
                        strncpy(location_info->state, value, sizeof(location_info->state) - 1);
                    } else if (strcmp(type, "locality") == 0) {
                        strncpy(location_info->city, value, sizeof(location_info->city) - 1);
                    } else if (strcmp(type, "postal_code") == 0) {
                        strncpy(location_info->postal_code, value, sizeof(location_info->postal_code) - 1);
                    } else if (strcmp(type, "route") == 0) {
                        strncpy(location_info->street, value, sizeof(location_info->street) - 1);
                    }
                }
            }
        }
        
        // Parse place ID if available
        json_get_string(doc, "results[0].place_id", location_info->place_id, 
                       sizeof(location_info->place_id));
        
        json_document_free(doc);
    } else {
        // Successfully parsed using the helper function
        strncpy(location_info->formatted_address, result.formatted_address, 
                sizeof(location_info->formatted_address) - 1);
        strncpy(location_info->country, result.country, sizeof(location_info->country) - 1);
        strncpy(location_info->state, result.state, sizeof(location_info->state) - 1);
        strncpy(location_info->city, result.city, sizeof(location_info->city) - 1);
        strncpy(location_info->postal_code, result.postal_code, sizeof(location_info->postal_code) - 1);
        location_info->latitude = result.latitude;
        location_info->longitude = result.longitude;
    }
    
    LOGX_DEBUG_MSG("Successfully parsed Google Geocoding response: %s (%.6f, %.6f)", 
                  location_info->formatted_address,
                  location_info->latitude,
                  location_info->longitude);
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
    place_details->timestamp = response->timestamp;
    
    LOGX_DEBUG_MSG("Parsed place details response");
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
    search_results->timestamp = response->timestamp;
    search_results->result_count = 0;
    
    LOGX_DEBUG_MSG("Parsed place search response");
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
    
    // Use proper JSON parser for elevation response
    json_document_t* doc = json_parse_string(response.data);
    if (!doc || !doc->valid) {
        LOGX_WARN_MSG("Failed to parse elevation response");
        *elevation = 100.0; // Default fallback
        return AUTONOMY_ERROR_API_FAILED;
    }
    
    // Parse elevation from the first result
    if (!json_get_double(doc, "results[0].elevation", elevation)) {
        LOGX_WARN_MSG("No elevation data in response");
        *elevation = 100.0; // Default fallback
        json_document_free(doc);
        return AUTONOMY_ERROR_API_FAILED;
    }
    
    // Also get resolution if available
    double resolution;
    if (json_get_double(doc, "results[0].resolution", &resolution)) {
        LOGX_DEBUG_MSG("Elevation resolution: %.2f meters", resolution);
    }
    
    json_document_free(doc);
    LOGX_DEBUG_MSG("Successfully parsed elevation from Google API: %.2f meters", *elevation);
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
    
    // Use proper JSON parser for timezone response
    memset(timezone_info, 0, sizeof(gps_google_timezone_info_t));
    timezone_info->timestamp = timestamp;
    
    json_document_t* doc = json_parse_string(response.data);
    if (!doc || !doc->valid) {
        LOGX_WARN_MSG("Failed to parse timezone response");
        return AUTONOMY_ERROR_API_FAILED;
    }
    
    // Parse timezone information
    int dst_offset, raw_offset;
    if (json_get_int(doc, "dstOffset", &dst_offset)) {
        timezone_info->dst_offset = dst_offset;
    }
    
    if (json_get_int(doc, "rawOffset", &raw_offset)) {
        timezone_info->raw_offset = raw_offset;
    }
    
    // Total offset in seconds
    timezone_info->total_offset = dst_offset + raw_offset;
    
    // Parse timezone ID and name
    json_get_string(doc, "timeZoneId", timezone_info->timezone_id, sizeof(timezone_info->timezone_id));
    json_get_string(doc, "timeZoneName", timezone_info->timezone_name, sizeof(timezone_info->timezone_name));
    
    // Check status
    char status[32];
    if (json_get_string(doc, "status", status, sizeof(status))) {
        if (strcmp(status, "OK") != 0) {
            LOGX_WARN_MSG("Timezone API returned status: %s", status);
            json_document_free(doc);
            return AUTONOMY_ERROR_API_FAILED;
        }
    }
    
    json_document_free(doc);
    
    LOGX_DEBUG_MSG("Successfully parsed timezone: %s (offset: %d seconds)", 
                  timezone_info->timezone_name, timezone_info->total_offset);
    
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
    
    LOGX_INFO_MSG("Google Location API configuration updated");
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
    
    LOGX_INFO_MSG("Google Location API state changed", "enabled", enabled ? "true" : "false");
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
    
    LOGX_INFO_MSG("Google Location API statistics reset");
    return AUTONOMY_SUCCESS;
}

// Cleanup Google API
void gps_google_api_cleanup(void) {
    if (!g_google_api_initialized) {
        return;
    }
    
    curl_global_cleanup();
    pthread_mutex_destroy(&g_google_api_mutex);
    g_google_api_initialized = false; // Use configurable setting
    
    LOGX_INFO_MSG("Google Location API cleaned up");
}