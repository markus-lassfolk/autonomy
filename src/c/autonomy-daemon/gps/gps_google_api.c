#include "gps_google_api.h"
#include "../shared/logging/logx.h"
#include "../shared/utils/json_parser.h"
#include "../shared/utils/http_client_libcurl.h"
#include "../shared/utils/string_utils.h"
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
static int perform_google_api_request(const char *endpoint, const char *params, gps_google_api_response_t *response\n"\n"\n"\n"\n"\n"\n"\n");
static void parse_reverse_geocode_response(const gps_google_api_response_t *response, gps_google_location_info_t *location_info\n"\n"\n"\n"\n"\n"\n"\n");
static void parse_place_details_response(const gps_google_api_response_t *response, gps_google_place_details_t *place_details\n"\n"\n"\n"\n"\n"\n"\n");
static void parse_place_search_response(const gps_google_api_response_t *response, gps_google_place_search_t *search_results\n"\n"\n"\n"\n"\n"\n"\n");

// CURL write callback for response data
static size_t google_write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    gps_google_api_response_t *response = (gps_google_api_response_t *)userp;
    
    if (response->data_size + realsize >= MAX_RESPONSE_SIZE) {
        printf("WARN: "Response too large, truncating"\n"\n"\n"\n"\n"\n"\n"\n");
        return 0;
    }
    
    memcpy(&response->data[response->data_size], contents, realsize\n"\n"\n"\n"\n"\n"\n"\n");
    response->data_size += realsize;
    response->data[response->data_size] = '\0';
    
    return realsize;
}

// Initialize Google Location API
int gps_google_api_init(const char *api_key) {
    if (g_google_api_initialized) {
        printf("WARN: "Google Location API already initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_SUCCESS;
    }
    
    if (!api_key || strlen(api_key) == 0) {
        printf("ERROR: "Google API key is required"\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_google_api_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Initialize Google API state
    memset(&g_google_api, 0, sizeof(gps_google_api_t)\n"\n"\n"\n"\n"\n"\n"\n");
    g_google_api.enabled = true; // Use configurable google api enabled
    g_google_api.max_requests = MAX_API_REQUESTS;
    g_google_api.request_timeout = REQUEST_TIMEOUT;
    g_google_api.rate_limit_delay = RATE_LIMIT_DELAY;
    
    safe_strncpy(g_google_api.api_key, api_key, sizeof(g_google_api.api_key)\n"\n"\n"\n"\n"\n"\n"\n");
    
    g_google_api.request_count = 0;
    g_google_api.last_request = 0;
    g_google_api.total_requests = 0;
    g_google_api.successful_requests = 0;
    g_google_api.failed_requests = 0;
    
    // Initialize CURL
    curl_global_init(CURL_GLOBAL_DEFAULT\n"\n"\n"\n"\n"\n"\n"\n");
    
    g_google_api_initialized = true; // Use configurable setting
    pthread_mutex_unlock(&g_google_api_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "Google Location API initialized successfully"\n"\n"\n"\n"\n"\n"\n"\n");
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
    time_t now = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    if (g_google_api.last_request > 0) {
        int time_since_last = (int)(now - g_google_api.last_request\n"\n"\n"\n"\n"\n"\n"\n");
        if (time_since_last < (g_google_api.rate_limit_delay / 1000)) {
            usleep((g_google_api.rate_limit_delay - (time_since_last * 1000)) * 1000\n"\n"\n"\n"\n"\n"\n"\n");
        }
    }
    
    // Check daily request limit
    if (g_google_api.request_count >= g_google_api.max_requests) {
        printf("ERROR: "Daily API request limit reached", "limit", g_google_api.max_requests\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_TOO_FREQUENT;
    }
    
    pthread_mutex_lock(&g_google_api_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Initialize response
    memset(response, 0, sizeof(gps_google_api_response_t)\n"\n"\n"\n"\n"\n"\n"\n");
    response->timestamp = now;
    
    // Build full URL
    char url[1024];
    snprintf(url, sizeof(url), "%s%s?%s&key=%s", 
             GOOGLE_API_BASE_URL, endpoint, params, g_google_api.api_key\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Use shared HTTP client (replaces 20 lines of duplicate curl code)
    http_response_t* http_resp = http_get(url\n"\n"\n"\n"\n"\n"\n"\n");
    long http_code = 0;
    bool curl_success = false;
    
    if (http_resp) {
        http_code = http_resp->status_code;
        curl_success = http_response_is_success(http_resp\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Update statistics
    g_google_api.request_count++;
    g_google_api.total_requests++;
    g_google_api.last_request = now;
    
    if (curl_success) {
        g_google_api.successful_requests++;
        response->success = true;
        response->http_code = http_code;
        
        // Copy response data
        if (http_resp->body) {
            size_t copy_size = http_resp->body_size < sizeof(response->data) - 1 ? 
                              http_resp->body_size : sizeof(response->data) - 1;
            memcpy(response->data, http_resp->body, copy_size\n"\n"\n"\n"\n"\n"\n"\n");
            response->data[copy_size] = '\0';
            response->data_size = copy_size;
        }
        
        printf("DEBUG: "Google API request successful", "endpoint", endpoint\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        g_google_api.failed_requests++;
        response->success = false;
        response->http_code = http_code;
        response->error_code = 0;
        
        printf("ERROR: "Google API request failed", "endpoint", endpoint, "http_code", http_code\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (http_resp) http_response_free(http_resp\n"\n"\n"\n"\n"\n"\n"\n");
    pthread_mutex_unlock(&g_google_api_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
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
             lat, lon\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Perform API request
    gps_google_api_response_t response;
    int result = perform_google_api_request(GOOGLE_REVERSE_GEOCODE_ENDPOINT, params, &response\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != AUTONOMY_SUCCESS) {
        return result;
    }
    
    if (!response.success) {
        return AUTONOMY_ERROR_API_FAILED;
    }
    
    // Parse response (simplified - in a real implementation, this would parse JSON)
    parse_reverse_geocode_response(&response, location_info\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

// Parse reverse geocoding response
static void parse_reverse_geocode_response(const gps_google_api_response_t *response, 
                                         gps_google_location_info_t *location_info) {
    // Initialize location info
    memset(location_info, 0, sizeof(gps_google_location_info_t)\n"\n"\n"\n"\n"\n"\n"\n");
    location_info->timestamp = response->timestamp;
    
    // Use proper JSON parser from json_parser library
    geocoding_result_t result;
    if (!json_parse_google_geocoding(response->data, &result)) {
        // Fallback to direct parsing with json_document
        json_document_t* doc = json_parse_string(response->data\n"\n"\n"\n"\n"\n"\n"\n");
        if (!doc || !doc->valid) {
            printf("WARN: "Failed to parse Google Geocoding response"\n"\n"\n"\n"\n"\n"\n"\n");
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
        json_get_double(doc, "results[0].geometry.location.lat", &location_info->latitude\n"\n"\n"\n"\n"\n"\n"\n");
        json_get_double(doc, "results[0].geometry.location.lng", &location_info->longitude\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Parse address components for detailed information
        int components_count = json_get_array_size(doc, "results[0].address_components"\n"\n"\n"\n"\n"\n"\n"\n");
        for (int i = 0; i < components_count; i++) {
            char path[256];
            char type[64];
            
            // Get the type of this component
            snprintf(path, sizeof(path), "results[0].address_components[%d].types[0]", i\n"\n"\n"\n"\n"\n"\n"\n");
            if (json_get_string(doc, path, type, sizeof(type))) {
                // Get the long name for this component
                char value[256];
                snprintf(path, sizeof(path), "results[0].address_components[%d].long_name", i\n"\n"\n"\n"\n"\n"\n"\n");
                
                if (json_get_string(doc, path, value, sizeof(value))) {
                    // Map component types to location info fields
                    if (strcmp(type, "country") == 0) {
                        safe_strncpy(location_info->country, value, sizeof(location_info->country)\n"\n"\n"\n"\n"\n"\n"\n");
                    } else if (strcmp(type, "administrative_area_level_1") == 0) {
                        safe_strncpy(location_info->state, value, sizeof(location_info->state)\n"\n"\n"\n"\n"\n"\n"\n");
                    } else if (strcmp(type, "locality") == 0) {
                        safe_strncpy(location_info->city, value, sizeof(location_info->city)\n"\n"\n"\n"\n"\n"\n"\n");
                    } else if (strcmp(type, "postal_code") == 0) {
                        safe_strncpy(location_info->postal_code, value, sizeof(location_info->postal_code)\n"\n"\n"\n"\n"\n"\n"\n");
                    } else if (strcmp(type, "route") == 0) {
                        safe_strncpy(location_info->street, value, sizeof(location_info->street)\n"\n"\n"\n"\n"\n"\n"\n");
                    }
                }
            }
        }
        
        // Parse place ID if available
        json_get_string(doc, "results[0].place_id", location_info->place_id, 
                       sizeof(location_info->place_id)\n"\n"\n"\n"\n"\n"\n"\n");
        
        json_document_free(doc\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        // Successfully parsed using the helper function
        safe_strncpy(location_info->formatted_address, result.formatted_address, 
                     sizeof(location_info->formatted_address)\n"\n"\n"\n"\n"\n"\n"\n");
        safe_strncpy(location_info->country, result.country, sizeof(location_info->country)\n"\n"\n"\n"\n"\n"\n"\n");
        safe_strncpy(location_info->state, result.state, sizeof(location_info->state)\n"\n"\n"\n"\n"\n"\n"\n");
        safe_strncpy(location_info->city, result.city, sizeof(location_info->city)\n"\n"\n"\n"\n"\n"\n"\n");
        safe_strncpy(location_info->postal_code, result.postal_code, sizeof(location_info->postal_code)\n"\n"\n"\n"\n"\n"\n"\n");
        location_info->latitude = result.latitude;
        location_info->longitude = result.longitude;
    }
    
    printf("DEBUG: "Successfully parsed Google Geocoding response: %s (%.6f, %.6f)", 
                  location_info->formatted_address,
                  location_info->latitude,
                  location_info->longitude\n"\n"\n"\n"\n"\n"\n"\n");
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
             place_id\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Perform API request
    gps_google_api_response_t response;
    int result = perform_google_api_request(GOOGLE_PLACE_DETAILS_ENDPOINT, params, &response\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != AUTONOMY_SUCCESS) {
        return result;
    }
    
    if (!response.success) {
        return AUTONOMY_ERROR_API_FAILED;
    }
    
    // Parse response
    parse_place_details_response(&response, place_details\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

// Parse place details response
static void parse_place_details_response(const gps_google_api_response_t *response, 
                                       gps_google_place_details_t *place_details) {
    // Initialize place details
    memset(place_details, 0, sizeof(gps_google_place_details_t)\n"\n"\n"\n"\n"\n"\n"\n");
    place_details->timestamp = response->timestamp;
    
    printf("DEBUG: "Parsed place details response"\n"\n"\n"\n"\n"\n"\n"\n");
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
             lat, lon, radius, type ? type : "establishment"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Perform API request
    gps_google_api_response_t response;
    int result = perform_google_api_request(GOOGLE_PLACE_SEARCH_ENDPOINT, params, &response\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != AUTONOMY_SUCCESS) {
        return result;
    }
    
    if (!response.success) {
        return AUTONOMY_ERROR_API_FAILED;
    }
    
    // Parse response
    parse_place_search_response(&response, search_results\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

// Parse place search response
static void parse_place_search_response(const gps_google_api_response_t *response, 
                                      gps_google_place_search_t *search_results) {
    // Initialize search results
    memset(search_results, 0, sizeof(gps_google_place_search_t)\n"\n"\n"\n"\n"\n"\n"\n");
    search_results->timestamp = response->timestamp;
    search_results->result_count = 0;
    
    printf("DEBUG: "Parsed place search response"\n"\n"\n"\n"\n"\n"\n"\n");
}

// Get elevation data using Google API
int gps_google_api_get_elevation(double lat, double lon, double *elevation) {
    if (!g_google_api_initialized || !elevation) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Build request parameters
    char params[512];
    snprintf(params, sizeof(params), "locations=%.6f,%.6f", lat, lon\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Perform API request
    gps_google_api_response_t response;
    int result = perform_google_api_request(GOOGLE_ELEVATION_ENDPOINT, params, &response\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != AUTONOMY_SUCCESS) {
        return result;
    }
    
    if (!response.success) {
        return AUTONOMY_ERROR_API_FAILED;
    }
    
    // Use proper JSON parser for elevation response
    json_document_t* doc = json_parse_string(response.data\n"\n"\n"\n"\n"\n"\n"\n");
    if (!doc || !doc->valid) {
        printf("WARN: "Failed to parse elevation response"\n"\n"\n"\n"\n"\n"\n"\n");
        *elevation = 100.0; // Default fallback
        return AUTONOMY_ERROR_API_FAILED;
    }
    
    // Parse elevation from the first result
    if (!json_get_double(doc, "results[0].elevation", elevation)) {
        printf("WARN: "No elevation data in response"\n"\n"\n"\n"\n"\n"\n"\n");
        *elevation = 100.0; // Default fallback
        json_document_free(doc\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_API_FAILED;
    }
    
    // Also get resolution if available
    double resolution;
    if (json_get_double(doc, "results[0].resolution", &resolution)) {
        printf("DEBUG: "Elevation resolution: %.2f meters", resolution\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    json_document_free(doc\n"\n"\n"\n"\n"\n"\n"\n");
    printf("DEBUG: "Successfully parsed elevation from Google API: %.2f meters", *elevation\n"\n"\n"\n"\n"\n"\n"\n");
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
    snprintf(params, sizeof(params), "location=%.6f,%.6f&timestamp=%lld", lat, lon, (long long)timestamp\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Perform API request
    gps_google_api_response_t response;
    int result = perform_google_api_request(GOOGLE_TIMEZONE_ENDPOINT, params, &response\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != AUTONOMY_SUCCESS) {
        return result;
    }
    
    if (!response.success) {
        return AUTONOMY_ERROR_API_FAILED;
    }
    
    // Use proper JSON parser for timezone response
    memset(timezone_info, 0, sizeof(gps_google_timezone_info_t)\n"\n"\n"\n"\n"\n"\n"\n");
    timezone_info->timestamp = timestamp;
    
    json_document_t* doc = json_parse_string(response.data\n"\n"\n"\n"\n"\n"\n"\n");
    if (!doc || !doc->valid) {
        printf("WARN: "Failed to parse timezone response"\n"\n"\n"\n"\n"\n"\n"\n");
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
    json_get_string(doc, "timeZoneId", timezone_info->timezone_id, sizeof(timezone_info->timezone_id)\n"\n"\n"\n"\n"\n"\n"\n");
    json_get_string(doc, "timeZoneName", timezone_info->timezone_name, sizeof(timezone_info->timezone_name)\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Check status
    char status[32];
    if (json_get_string(doc, "status", status, sizeof(status))) {
        if (strcmp(status, "OK") != 0) {
            printf("WARN: "Timezone API returned status: %s", status\n"\n"\n"\n"\n"\n"\n"\n");
            json_document_free(doc\n"\n"\n"\n"\n"\n"\n"\n");
            return AUTONOMY_ERROR_API_FAILED;
        }
    }
    
    json_document_free(doc\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("DEBUG: "Successfully parsed timezone: %s (offset: %d seconds)", 
                  timezone_info->timezone_name, timezone_info->total_offset\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

// Get Google API status
int gps_google_api_get_status(gps_google_api_status_t *status) {
    if (!g_google_api_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_google_api_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
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
    
    pthread_mutex_unlock(&g_google_api_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

// Get Google API configuration
int gps_google_api_get_config(gps_google_api_config_t *config) {
    if (!g_google_api_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_google_api_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    config->enabled = g_google_api.enabled;
    config->max_requests = g_google_api.max_requests;
    config->request_timeout = g_google_api.request_timeout;
    config->rate_limit_delay = g_google_api.rate_limit_delay;
    safe_strncpy(config->api_key, g_google_api.api_key, sizeof(config->api_key)\n"\n"\n"\n"\n"\n"\n"\n");
    
    pthread_mutex_unlock(&g_google_api_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

// Set Google API configuration
int gps_google_api_set_config(const gps_google_api_config_t *config) {
    if (!g_google_api_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_google_api_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    g_google_api.enabled = config->enabled;
    g_google_api.max_requests = config->max_requests;
    g_google_api.request_timeout = config->request_timeout;
    g_google_api.rate_limit_delay = config->rate_limit_delay;
    
    if (strlen(config->api_key) > 0) {
        safe_strncpy(g_google_api.api_key, config->api_key, sizeof(g_google_api.api_key)\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    pthread_mutex_unlock(&g_google_api_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "Google Location API configuration updated"\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_SUCCESS;
}

// Enable/disable Google API
int gps_google_api_set_enabled(bool enabled) {
    if (!g_google_api_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_google_api_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    g_google_api.enabled = enabled;
    pthread_mutex_unlock(&g_google_api_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "Google Location API state changed", "enabled", enabled ? "true" : "false"\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_SUCCESS;
}

// Reset Google API statistics
int gps_google_api_reset_stats(void) {
    if (!g_google_api_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_google_api_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    g_google_api.request_count = 0;
    g_google_api.total_requests = 0;
    g_google_api.successful_requests = 0;
    g_google_api.failed_requests = 0;
    
    pthread_mutex_unlock(&g_google_api_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "Google Location API statistics reset"\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_SUCCESS;
}

// Cleanup Google API
void gps_google_api_cleanup(void) {
    if (!g_google_api_initialized) {
        return;
    }
    
    curl_global_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
    pthread_mutex_destroy(&g_google_api_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    g_google_api_initialized = false; // Use configurable setting
    
    printf("INFO: "Google Location API cleaned up"\n"\n"\n"\n"\n"\n"\n"\n");
}