#include "gps_google_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include <json-c/json.h>

// Global Google API state
static google_api_config_t g_google_config = {0};
static google_api_stats_t g_google_stats = {0};
static bool g_google_initialized = false;
static CURL* g_curl_handle = NULL;

// HTTP response callback
static size_t http_response_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    char* response = (char*)userp;
    
    strncat(response, (char*)contents, total_size);
    return total_size;
}

// Initialize Google Geolocation API
int google_api_init(const google_api_config_t* config) {
    if (!config) {
        return -1;
    }
    
    // Initialize curl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    g_curl_handle = curl_easy_init();
    if (!g_curl_handle) {
        return -1;
    }
    
    // Copy configuration
    memcpy(&g_google_config, config, sizeof(google_api_config_t));
    
    // Initialize statistics
    memset(&g_google_stats, 0, sizeof(google_api_stats_t));
    
    g_google_initialized = true;
    return 0;
}

// Cleanup Google Geolocation API
void google_api_cleanup(void) {
    if (g_curl_handle) {
        curl_easy_cleanup(g_curl_handle);
        g_curl_handle = NULL;
    }
    
    curl_global_cleanup();
    g_google_initialized = false;
}

// Make HTTP request to Google API
static int make_google_request(const char* json_data, google_location_response_t* response) {
    if (!g_google_initialized || !json_data || !response) {
        return -1;
    }
    
    // Initialize response
    memset(response, 0, sizeof(google_location_response_t));
    response->timestamp = time(NULL);
    
    // Build URL
    char url[GOOGLE_API_MAX_URL_LEN];
    snprintf(url, sizeof(url), "%s?key=%s", GOOGLE_API_BASE_URL, g_google_config.api_key);
    
    // Set up curl
    curl_easy_reset(g_curl_handle);
    curl_easy_setopt(g_curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(g_curl_handle, CURLOPT_POSTFIELDS, json_data);
    curl_easy_setopt(g_curl_handle, CURLOPT_POSTFIELDSIZE, strlen(json_data));
    curl_easy_setopt(g_curl_handle, CURLOPT_WRITEFUNCTION, http_response_callback);
    
    char http_response[GOOGLE_API_MAX_RESPONSE_LEN] = {0};
    curl_easy_setopt(g_curl_handle, CURLOPT_WRITEDATA, http_response);
    
    // Set headers
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(g_curl_handle, CURLOPT_HTTPHEADER, headers);
    
    // Set timeout
    curl_easy_setopt(g_curl_handle, CURLOPT_TIMEOUT, g_google_config.timeout_seconds);
    
    // Perform request
    CURLcode res = curl_easy_perform(g_curl_handle);
    curl_slist_free_all(headers);
    
    g_google_stats.total_requests++;
    g_google_stats.last_request_time = time(NULL);
    
    if (res != CURLE_OK) {
        response->success = false;
        snprintf(response->error_message, sizeof(response->error_message), "CURL error: %s", curl_easy_strerror(res));
        g_google_stats.failed_requests++;
        return -1;
    }
    
    // Get HTTP response code
    long http_code;
    curl_easy_getinfo(g_curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
    response->error_code = (int)http_code;
    
    // Parse JSON response
    json_object* json_response = json_tokener_parse(http_response);
    if (!json_response) {
        response->success = false;
        strncpy(response->error_message, "Failed to parse JSON response", sizeof(response->error_message) - 1);
        response->error_message[sizeof(response->error_message) - 1] = '\0';
        g_google_stats.failed_requests++;
        return -1;
    }
    
    if (http_code == 200) {
        // Success response
        json_object* location_obj;
        if (json_object_object_get_ex(json_response, "location", &location_obj)) {
            json_object* lat_obj, *lng_obj;
            if (json_object_object_get_ex(location_obj, "lat", &lat_obj) &&
                json_object_object_get_ex(location_obj, "lng", &lng_obj)) {
                response->lat = json_object_get_double(lat_obj);
                response->lng = json_object_get_double(lng_obj);
                response->success = true;
            }
        }
        
        json_object* accuracy_obj;
        if (json_object_object_get_ex(json_response, "accuracy", &accuracy_obj)) {
            response->accuracy = json_object_get_double(accuracy_obj);
        }
        
        g_google_stats.successful_requests++;
        if (response->accuracy > 0) {
            g_google_stats.average_accuracy = 
                (g_google_stats.average_accuracy * (g_google_stats.successful_requests - 1) + response->accuracy) / 
                g_google_stats.successful_requests;
        }
    } else {
        // Error response
        response->success = false;
        json_object* error_obj;
        if (json_object_object_get_ex(json_response, "error", &error_obj)) {
            json_object* message_obj;
            if (json_object_object_get_ex(error_obj, "message", &message_obj)) {
                strncpy(response->error_message, json_object_get_string(message_obj), sizeof(response->error_message) - 1);
                response->error_message[sizeof(response->error_message) - 1] = '\0';
            }
        }
        g_google_stats.failed_requests++;
    }
    
    json_object_put(json_response);
    return 0;
}

// Get location using WiFi access points
int google_api_get_wifi_location(const google_wifi_ap_t* wifi_aps, int wifi_count, google_location_response_t* response) {
    if (!wifi_aps || wifi_count <= 0 || !response) {
        return -1;
    }
    
    // Build JSON request
    json_object* request = json_object_new_object();
    json_object* wifi_array = json_object_new_array();
    
    for (int i = 0; i < wifi_count && i < 32; i++) {
        json_object* wifi_obj = json_object_new_object();
        json_object_object_add(wifi_obj, "macAddress", json_object_new_string(wifi_aps[i].mac_address));
        json_object_object_add(wifi_obj, "signalStrength", json_object_new_int(wifi_aps[i].signal_strength));
        json_object_object_add(wifi_obj, "signalToNoiseRatio", json_object_new_int(wifi_aps[i].signal_to_noise_ratio));
        json_object_object_add(wifi_obj, "channel", json_object_new_int(wifi_aps[i].channel));
        json_object_array_add(wifi_array, wifi_obj);
    }
    
    json_object_object_add(request, "wifiAccessPoints", wifi_array);
    
    const char* json_string = json_object_to_json_string(request);
    int result = make_google_request(json_string, response);
    
    json_object_put(request);
    return result;
}

// Get location using cell towers
int google_api_get_cellular_location(const google_cell_tower_t* cell_towers, int cell_count, google_location_response_t* response) {
    if (!cell_towers || cell_count <= 0 || !response) {
        return -1;
    }
    
    // Build JSON request
    json_object* request = json_object_new_object();
    json_object* cell_array = json_object_new_array();
    
    for (int i = 0; i < cell_count && i < 16; i++) {
        json_object* cell_obj = json_object_new_object();
        json_object_object_add(cell_obj, "cellId", json_object_new_string(cell_towers[i].cell_id));
        json_object_object_add(cell_obj, "locationAreaCode", json_object_new_string(cell_towers[i].location_area_code));
        json_object_object_add(cell_obj, "mobileCountryCode", json_object_new_string(cell_towers[i].mobile_country_code));
        json_object_object_add(cell_obj, "mobileNetworkCode", json_object_new_string(cell_towers[i].mobile_network_code));
        json_object_object_add(cell_obj, "signalStrength", json_object_new_int(cell_towers[i].signal_strength));
        json_object_object_add(cell_obj, "age", json_object_new_int(cell_towers[i].age));
        json_object_array_add(cell_array, cell_obj);
    }
    
    json_object_object_add(request, "cellTowers", cell_array);
    
    const char* json_string = json_object_to_json_string(request);
    int result = make_google_request(json_string, response);
    
    json_object_put(request);
    return result;
}

// Get location using combined WiFi and cellular data
int google_api_get_combined_location(const google_location_request_t* request, google_location_response_t* response) {
    if (!request || !response) {
        return -1;
    }
    
    // Build JSON request
    json_object* json_request = json_object_new_object();
    
    // Add WiFi access points
    if (request->wifi_count > 0) {
        json_object* wifi_array = json_object_new_array();
        for (int i = 0; i < request->wifi_count && i < 32; i++) {
            json_object* wifi_obj = json_object_new_object();
            json_object_object_add(wifi_obj, "macAddress", json_object_new_string(request->wifi_access_points[i].mac_address));
            json_object_object_add(wifi_obj, "signalStrength", json_object_new_int(request->wifi_access_points[i].signal_strength));
            json_object_object_add(wifi_obj, "signalToNoiseRatio", json_object_new_int(request->wifi_access_points[i].signal_to_noise_ratio));
            json_object_object_add(wifi_obj, "channel", json_object_new_int(request->wifi_access_points[i].channel));
            json_object_array_add(wifi_array, wifi_obj);
        }
        json_object_object_add(json_request, "wifiAccessPoints", wifi_array);
    }
    
    // Add cell towers
    if (request->cell_count > 0) {
        json_object* cell_array = json_object_new_array();
        for (int i = 0; i < request->cell_count && i < 16; i++) {
            json_object* cell_obj = json_object_new_object();
            json_object_object_add(cell_obj, "cellId", json_object_new_string(request->cell_towers[i].cell_id));
            json_object_object_add(cell_obj, "locationAreaCode", json_object_new_string(request->cell_towers[i].location_area_code));
            json_object_object_add(cell_obj, "mobileCountryCode", json_object_new_string(request->cell_towers[i].mobile_country_code));
            json_object_object_add(cell_obj, "mobileNetworkCode", json_object_new_string(request->cell_towers[i].mobile_network_code));
            json_object_object_add(cell_obj, "signalStrength", json_object_new_int(request->cell_towers[i].signal_strength));
            json_object_array_add(cell_array, cell_obj);
        }
        json_object_object_add(json_request, "cellTowers", cell_array);
    }
    
    // Add IP consideration
    if (request->consider_ip) {
        json_object_object_add(json_request, "considerIp", json_object_new_boolean(true));
    }
    
    const char* json_string = json_object_to_json_string(json_request);
    int result = make_google_request(json_string, response);
    
    json_object_put(json_request);
    return result;
}

// Get location with IP fallback
int google_api_get_location_with_ip_fallback(const google_location_request_t* request, google_location_response_t* response) {
    if (!response) {
        return -1;
    }
    
    // Try combined location first if request provided
    if (request && (request->wifi_count > 0 || request->cell_count > 0)) {
        int result = google_api_get_combined_location(request, response);
        if (result == 0 && response->success) {
            return 0;
        }
    }
    
    // Fallback to IP-only location
    json_object* ip_request = json_object_new_object();
    json_object_object_add(ip_request, "considerIp", json_object_new_boolean(true));
    
    const char* json_string = json_object_to_json_string(ip_request);
    int result = make_google_request(json_string, response);
    
    json_object_put(ip_request);
    return result;
}

// Get API statistics
int google_api_get_stats(google_api_stats_t* stats) {
    if (!g_google_initialized || !stats) {
        return -1;
    }
    
    memcpy(stats, &g_google_stats, sizeof(google_api_stats_t));
    return 0;
}

// Check if Google API is initialized
bool google_api_is_initialized(void) {
    return g_google_initialized;
}

// Validate API key
bool google_api_validate_key(void) {
    if (!g_google_initialized) {
        return false;
    }
    
    // Make a simple test request
    google_location_response_t test_response;
    google_location_request_t test_request = {0};
    test_request.consider_ip = true;
    
    int result = google_api_get_location_with_ip_fallback(&test_request, &test_response);
    return (result == 0 && test_response.success);
}

// Check quota status
int google_api_get_quota_remaining(void) {
    // Google API doesn't provide quota information in the response
    // This would need to be tracked separately or queried from Google Cloud Console
    return -1; // Unknown
}
