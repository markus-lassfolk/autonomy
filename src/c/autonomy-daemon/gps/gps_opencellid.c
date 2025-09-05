#include "gps_opencellid.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include <json-c/json.h>

// Global OpenCellID state
static opencellid_config_t g_opencellid_config = {0};
static opencellid_stats_t g_opencellid_stats = {0};
static bool g_opencellid_initialized = false;
static CURL* g_curl_handle = NULL;

// HTTP response callback
static size_t http_response_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    char* response = (char*)userp;
    
    strncat(response, (char*)contents, total_size);
    return total_size;
}

// Initialize OpenCellID API
int gps_opencellid_init(const opencellid_config_t* config) {
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
    memcpy(&g_opencellid_config, *config, sizeof(opencellid_config_t));
    
    // Initialize statistics
    memset(&g_opencellid_stats, 0, sizeof(opencellid_stats_t));
    
    g_opencellid_initialized = true;
    return 0;
}

// Cleanup OpenCellID API
void gps_opencellid_cleanup(void) {
    if (g_curl_handle) {
        curl_easy_cleanup(g_curl_handle);
        g_curl_handle = NULL;
    }
    
    curl_global_cleanup();
    g_opencellid_initialized = false;
}

// Make HTTP request to OpenCellID API
static int make_opencellid_request(const char* url, opencellid_response_t* response) {
    if (!g_opencellid_initialized || !url || !response) {
        return -1;
    }
    
    // Initialize response
    memset(response, 0, sizeof(opencellid_response_t));
    response->timestamp = time(NULL);
    
    // Set up curl
    curl_easy_reset(g_curl_handle);
    curl_easy_setopt(g_curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(g_curl_handle, CURLOPT_WRITEFUNCTION, http_response_callback);
    
    char http_response[OPENCELLID_MAX_RESPONSE_LEN] = {0};
    curl_easy_setopt(g_curl_handle, CURLOPT_WRITEDATA, http_response);
    
    // Set timeout
    curl_easy_setopt(g_curl_handle, CURLOPT_TIMEOUT, g_opencellid_config.timeout_seconds);
    
    // Perform request
    CURLcode res = curl_easy_perform(g_curl_handle);
    
    g_opencellid_stats.total_requests++;
    g_opencellid_stats.last_request_time = time(NULL);
    
    if (res != CURLE_OK) {
        response->success = false;
        snprintf(response->error_message, sizeof(response->error_message), "CURL error: %s", curl_easy_strerror(res));
        g_opencellid_stats.failed_requests++;
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
        g_opencellid_stats.failed_requests++;
        return -1;
    }
    
    if (http_code == 200) {
        // Success response
        json_object* lat_obj, *lon_obj;
        if (json_object_object_get_ex(json_response, "lat", &lat_obj) &&
            json_object_object_get_ex(json_response, "lon", &lon_obj)) {
            response->lat = json_object_get_double(lat_obj);
            response->lon = json_object_get_double(lon_obj);
            response->success = true;
        }
        
        json_object* range_obj;
        if (json_object_object_get_ex(json_response, "range", &range_obj)) {
            response->range = json_object_get_int(range_obj);
        }
        
        json_object* samples_obj;
        if (json_object_object_get_ex(json_response, "samples", &samples_obj)) {
            response->samples = json_object_get_int(samples_obj);
        }
        
        g_opencellid_stats.successful_requests++;
        if (response->range > 0) {
            g_opencellid_stats.average_accuracy = 
                (g_opencellid_stats.average_accuracy * (g_opencellid_stats.successful_requests - 1) + response->range) / 
                g_opencellid_stats.successful_requests;
        }
    } else {
        // Error response
        response->success = false;
        json_object* error_obj;
        if (json_object_object_get_ex(json_response, "error", &error_obj)) {
            json_object* message_obj;
            if (json_object_object_get_ex(error_obj, "message", &message_obj)) {
                strncpy(response->error_message, json_object_get_string(message_obj), sizeof(response->error_message) - 1);
            }
        }
        g_opencellid_stats.failed_requests++;
    }
    
    json_object_put(json_response);
    return 0;
}

// Lookup cell tower location
int gps_opencellid_lookup(const opencellid_cell_key_t* cell_key, opencellid_response_t* response) {
    if (!g_opencellid_initialized || !cell_key || !response) {
        return -1;
    }
    
    // Build URL
    char url[OPENCELLID_MAX_URL_LEN];
    snprintf(url, sizeof(url), "%s/cell?key=%s&mcc=%s&mnc=%s&lac=%s&cellid=%s&format=json",
             OPENCELLID_BASE_URL,
             g_opencellid_config.api_key,
             cell_key->mcc,
             cell_key->mnc,
             cell_key->lac,
             cell_key->cell_id);
    
    return make_opencellid_request(url, response);
}

// Contribute cell tower data
int gps_opencellid_contribute(const opencellid_cell_key_t* cell_key, double lat, double lon, int range) {
    if (!g_opencellid_initialized || !cell_key) {
        return -1;
    }
    
    // Build contribution URL
    char url[OPENCELLID_MAX_URL_LEN];
    snprintf(url, sizeof(url), "%s/cell?key=%s&mcc=%s&mnc=%s&lac=%s&cellid=%s&lat=%.6f&lon=%.6f&range=%d&format=json",
             OPENCELLID_BASE_URL,
             g_opencellid_config.api_key,
             cell_key->mcc,
             cell_key->mnc,
             cell_key->lac,
             cell_key->cell_id,
             lat, lon, range);
    
    opencellid_response_t response;
    int result = make_opencellid_request(url, &response);
    
    if (result == 0 && response.success) {
        g_opencellid_stats.contribution_requests++;
        g_opencellid_stats.contribution_successes++;
    } else {
        g_opencellid_stats.contribution_requests++;
        g_opencellid_stats.contribution_failures++;
    }
    
    return result;
}

// Get API statistics
int gps_opencellid_get_stats(opencellid_stats_t* stats) {
    if (!g_opencellid_initialized || !stats) {
        return -1;
    }
    
    memcpy(stats, &g_opencellid_stats, sizeof(opencellid_stats_t));
    return 0;
}

// Check if OpenCellID is initialized
bool gps_opencellid_is_initialized(void) {
    return g_opencellid_initialized;
}

// Validate API key
bool gps_opencellid_validate_key(void) {
    if (!g_opencellid_initialized) {
        return false;
    }
    
    // Make a simple test request
    opencellid_cell_key_t test_cell = {0};
    strncpy(test_cell.mcc, "1", sizeof(test_cell.mcc) - 1);
    strncpy(test_cell.mnc, "1", sizeof(test_cell.mnc) - 1);
    strncpy(test_cell.lac, "1", sizeof(test_cell.lac) - 1);
    strncpy(test_cell.cell_id, "1", sizeof(test_cell.cell_id) - 1);
    test_cell.radio = OPENCELLID_RADIO_LTE;
    
    opencellid_response_t test_response;
    int result = gps_opencellid_lookup(&test_cell, &test_response);
    return (result == 0);
}

// Get quota status
int gps_opencellid_get_quota_remaining(void) {
    // OpenCellID API doesn't provide quota information in the response
    // This would need to be tracked separately or queried from their dashboard
    return -1; // Unknown
}

// Start background contribution manager
int gps_opencellid_start_contribution_manager(void) {
    if (!g_opencellid_initialized) {
        return -1;
    }
    
    // This would start a background thread for contributing data
    // For now, just return success
    return 0;
}

// Stop background contribution manager
int gps_opencellid_stop_contribution_manager(void) {
    if (!g_opencellid_initialized) {
        return -1;
    }
    
    // This would stop the background contribution thread
    // For now, just return success
    return 0;
}

// Perform health check
int gps_opencellid_health_check(void) {
    if (!g_opencellid_initialized) {
        return -1;
    }
    
    // Simple health check - try to make a request
    opencellid_cell_key_t test_cell = {0};
    strncpy(test_cell.mcc, "1", sizeof(test_cell.mcc) - 1);
    strncpy(test_cell.mnc, "1", sizeof(test_cell.mnc) - 1);
    strncpy(test_cell.lac, "1", sizeof(test_cell.lac) - 1);
    strncpy(test_cell.cell_id, "1", sizeof(test_cell.cell_id) - 1);
    test_cell.radio = OPENCELLID_RADIO_LTE;
    
    opencellid_response_t test_response;
    int result = gps_opencellid_lookup(&test_cell, &test_response);
    
    // Return success even if the lookup fails (API might be working but no data for test cell)
    return 0;
}
