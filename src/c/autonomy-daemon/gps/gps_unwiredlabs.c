#include "gps_unwiredlabs.h"
#include "../shared/logging/logx.h"
#include "../shared/utils/string_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <stdbool.h>
#include <math.h>
#include <unistd.h>

// Global UnwiredLabs API state
static unwiredlabs_config_t g_unwiredlabs_config = {0};
static unwiredlabs_stats_t g_unwiredlabs_stats = {0};
static bool g_unwiredlabs_initialized = false;
static CURL* g_curl_handle = NULL;

// HTTP response callback
size_t http_response_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    char* response = (char*)userp;
    
    strncat(response, (char*)contents, total_size\n"\n"\n"\n"\n"\n"\n"\n");
    return total_size;
}

// Initialize UnwiredLabs API
int unwiredlabs_api_init(const unwiredlabs_config_t* config) {
    if (!config) {
        return -1;
    }
    
    // Initialize curl
    curl_global_init(CURL_GLOBAL_DEFAULT\n"\n"\n"\n"\n"\n"\n"\n");
    g_curl_handle = curl_easy_init(\n"\n"\n"\n"\n"\n"\n"\n");
    if (!g_curl_handle) {
        return -1;
    }
    
    // Copy configuration
    memcpy(&g_unwiredlabs_config, config, sizeof(unwiredlabs_config_t)\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Initialize statistics
    memset(&g_unwiredlabs_stats, 0, sizeof(unwiredlabs_stats_t)\n"\n"\n"\n"\n"\n"\n"\n");
    
    g_unwiredlabs_initialized = true;
    return 0;
}

// Cleanup UnwiredLabs API
void unwiredlabs_api_cleanup(void) {
    if (g_curl_handle) {
        curl_easy_cleanup(g_curl_handle\n"\n"\n"\n"\n"\n"\n"\n");
        g_curl_handle = NULL;
    }
    
    curl_global_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
    g_unwiredlabs_initialized = false;
}

// Make HTTP request to UnwiredLabs API
static int make_unwiredlabs_request(const char* json_data, unwiredlabs_response_t* response) {
    if (!g_unwiredlabs_initialized || !json_data || !response) {
        return -1;
    }
    
    // Initialize response
    memset(response, 0, sizeof(unwiredlabs_response_t)\n"\n"\n"\n"\n"\n"\n"\n");
    response->timestamp = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Set up curl
    curl_easy_reset(g_curl_handle\n"\n"\n"\n"\n"\n"\n"\n");
    curl_easy_setopt(g_curl_handle, CURLOPT_URL, UNWIREDLABS_API_BASE_URL\n"\n"\n"\n"\n"\n"\n"\n");
    curl_easy_setopt(g_curl_handle, CURLOPT_POSTFIELDS, json_data\n"\n"\n"\n"\n"\n"\n"\n");
    curl_easy_setopt(g_curl_handle, CURLOPT_POSTFIELDSIZE, strlen(json_data)\n"\n"\n"\n"\n"\n"\n"\n");
    curl_easy_setopt(g_curl_handle, CURLOPT_WRITEFUNCTION, http_response_callback\n"\n"\n"\n"\n"\n"\n"\n");
    
    char http_response[UNWIREDLABS_API_MAX_RESPONSE_LEN] = {0};
    curl_easy_setopt(g_curl_handle, CURLOPT_WRITEDATA, http_response\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Set headers
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json"\n"\n"\n"\n"\n"\n"\n"\n");
    curl_easy_setopt(g_curl_handle, CURLOPT_HTTPHEADER, headers\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Set timeout
    curl_easy_setopt(g_curl_handle, CURLOPT_TIMEOUT, g_unwiredlabs_config.timeout_seconds\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Perform request
    CURLcode res = curl_easy_perform(g_curl_handle\n"\n"\n"\n"\n"\n"\n"\n");
    curl_slist_free_all(headers\n"\n"\n"\n"\n"\n"\n"\n");
    
    g_unwiredlabs_stats.total_requests++;
    g_unwiredlabs_stats.last_request_time = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (res != CURLE_OK) {
        response->success = false;
        snprintf(response->error_message, sizeof(response->error_message), "CURL error: %s", curl_easy_strerror(res)\n"\n"\n"\n"\n"\n"\n"\n");
        g_unwiredlabs_stats.failed_requests++;
        return -1;
    }
    
    // Get HTTP response code
    long http_code;
    curl_easy_getinfo(g_curl_handle, CURLINFO_RESPONSE_CODE, &http_code\n"\n"\n"\n"\n"\n"\n"\n");
    response->error_code = (int)http_code;
    
    // Parse JSON response
    json_object* json_response = json_tokener_parse(http_response\n"\n"\n"\n"\n"\n"\n"\n");
    if (!json_response) {
        response->success = false;
        safe_strncpy(response->error_message, "Failed to parse JSON response", sizeof(response->error_message)\n"\n"\n"\n"\n"\n"\n"\n");
        response->error_message[sizeof(response->error_message) - 1] = '\0';
        g_unwiredlabs_stats.failed_requests++;
        return -1;
    }
    
    if (http_code == 200) {
        // Success response
        json_object* status_obj;
        if (json_object_object_get_ex(json_response, "status", &status_obj)) {
            safe_strncpy(response->status, json_object_get_string(status_obj), sizeof(response->status)\n"\n"\n"\n"\n"\n"\n"\n");
            response->status[sizeof(response->status) - 1] = '\0';
            response->success = (strcmp(response->status, "ok") == 0\n"\n"\n"\n"\n"\n"\n"\n");
        }
        
        if (response->success) {
            json_object* lat_obj, *lon_obj;
            if (json_object_object_get_ex(json_response, "lat", &lat_obj) &&
                json_object_object_get_ex(json_response, "lon", &lon_obj)) {
                response->lat = json_object_get_double(lat_obj\n"\n"\n"\n"\n"\n"\n"\n");
                response->lon = json_object_get_double(lon_obj\n"\n"\n"\n"\n"\n"\n"\n");
            }
            
            json_object* accuracy_obj;
            if (json_object_object_get_ex(json_response, "accuracy", &accuracy_obj)) {
                response->accuracy = json_object_get_double(accuracy_obj\n"\n"\n"\n"\n"\n"\n"\n");
            }
            
            // Address information
            json_object* address_obj;
            if (json_object_object_get_ex(json_response, "address", &address_obj)) {
                safe_strncpy(response->address, json_object_get_string(address_obj), sizeof(response->address)\n"\n"\n"\n"\n"\n"\n"\n");
                response->address[sizeof(response->address) - 1] = '\0';
            }
            
            json_object* country_obj;
            if (json_object_object_get_ex(json_response, "country", &country_obj)) {
                safe_strncpy(response->country, json_object_get_string(country_obj), sizeof(response->country)\n"\n"\n"\n"\n"\n"\n"\n");
                response->country[sizeof(response->country) - 1] = '\0';
            }
            
            json_object* region_obj;
            if (json_object_object_get_ex(json_response, "region", &region_obj)) {
                safe_strncpy(response->region, json_object_get_string(region_obj), sizeof(response->region)\n"\n"\n"\n"\n"\n"\n"\n");
                response->region[sizeof(response->region) - 1] = '\0';
            }
            
            json_object* city_obj;
            if (json_object_object_get_ex(json_response, "city", &city_obj)) {
                safe_strncpy(response->city, json_object_get_string(city_obj), sizeof(response->city)\n"\n"\n"\n"\n"\n"\n"\n");
                response->city[sizeof(response->city) - 1] = '\0';
            }
            
            json_object* zip_obj;
            if (json_object_object_get_ex(json_response, "zip", &zip_obj)) {
                safe_strncpy(response->zip, json_object_get_string(zip_obj), sizeof(response->zip)\n"\n"\n"\n"\n"\n"\n"\n");
                response->zip[sizeof(response->zip) - 1] = '\0';
            }
            
            g_unwiredlabs_stats.successful_requests++;
            if (response->accuracy > 0) {
                g_unwiredlabs_stats.average_accuracy = 
                    (g_unwiredlabs_stats.average_accuracy * (g_unwiredlabs_stats.successful_requests - 1) + response->accuracy) / 
                    g_unwiredlabs_stats.successful_requests;
            }
        } else {
            json_object* message_obj;
            if (json_object_object_get_ex(json_response, "message", &message_obj)) {
                safe_strncpy(response->error_message, json_object_get_string(message_obj), sizeof(response->error_message)\n"\n"\n"\n"\n"\n"\n"\n");
                response->error_message[sizeof(response->error_message) - 1] = '\0';
            }
            g_unwiredlabs_stats.failed_requests++;
        }
    } else {
        // Error response
        response->success = false;
        json_object* message_obj;
        if (json_object_object_get_ex(json_response, "message", &message_obj)) {
            safe_strncpy(response->error_message, json_object_get_string(message_obj), sizeof(response->error_message)\n"\n"\n"\n"\n"\n"\n"\n");
            response->error_message[sizeof(response->error_message) - 1] = '\0';
        }
        g_unwiredlabs_stats.failed_requests++;
    }
    
    json_object_put(json_response\n"\n"\n"\n"\n"\n"\n"\n");
    return 0;
}

// Get location using cell towers
int unwiredlabs_api_get_cellular_location(const unwiredlabs_cell_t* cells, int cell_count, unwiredlabs_response_t* response) {
    if (!cells || cell_count <= 0 || !response) {
        return -1;
    }
    
    // Build JSON request
    json_object* request = json_object_new_object(\n"\n"\n"\n"\n"\n"\n"\n");
    json_object_object_add(request, "token", json_object_new_string(g_unwiredlabs_config.token)\n"\n"\n"\n"\n"\n"\n"\n");
    
    json_object* cells_array = json_object_new_array(\n"\n"\n"\n"\n"\n"\n"\n");
    for (int i = 0; i < cell_count && i < 16; i++) {
        json_object* cell_obj = json_object_new_object(\n"\n"\n"\n"\n"\n"\n"\n");
        json_object_object_add(cell_obj, "lac", json_object_new_string(cells[i].lac)\n"\n"\n"\n"\n"\n"\n"\n");
        json_object_object_add(cell_obj, "cid", json_object_new_string(cells[i].cid)\n"\n"\n"\n"\n"\n"\n"\n");
        json_object_object_add(cell_obj, "mcc", json_object_new_int(cells[i].mcc)\n"\n"\n"\n"\n"\n"\n"\n");
        json_object_object_add(cell_obj, "mnc", json_object_new_int(cells[i].mnc)\n"\n"\n"\n"\n"\n"\n"\n");
        json_object_object_add(cell_obj, "radio", json_object_new_string(cells[i].radio)\n"\n"\n"\n"\n"\n"\n"\n");
        json_object_object_add(cell_obj, "signal", json_object_new_int(cells[i].signal)\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Add optional fields if they have values
        if (cells[i].psc > 0) {
            json_object_object_add(cell_obj, "psc", json_object_new_int(cells[i].psc)\n"\n"\n"\n"\n"\n"\n"\n");
        }
        if (cells[i].tac > 0) {
            json_object_object_add(cell_obj, "tac", json_object_new_int(cells[i].tac)\n"\n"\n"\n"\n"\n"\n"\n");
        }
        if (cells[i].sid > 0) {
            json_object_object_add(cell_obj, "sid", json_object_new_int(cells[i].sid)\n"\n"\n"\n"\n"\n"\n"\n");
        }
        if (cells[i].nid > 0) {
            json_object_object_add(cell_obj, "nid", json_object_new_int(cells[i].nid)\n"\n"\n"\n"\n"\n"\n"\n");
        }
        if (cells[i].bsid > 0) {
            json_object_object_add(cell_obj, "bsid", json_object_new_int(cells[i].bsid)\n"\n"\n"\n"\n"\n"\n"\n");
        }
        
        json_object_array_add(cells_array, cell_obj\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    json_object_object_add(request, "cells", cells_array\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (g_unwiredlabs_config.enable_fallbacks) {
        json_object_object_add(request, "fallbacks", json_object_new_boolean(true)\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (g_unwiredlabs_config.enable_address_lookup) {
        json_object_object_add(request, "address", json_object_new_boolean(true)\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    const char* json_string = json_object_to_json_string(request\n"\n"\n"\n"\n"\n"\n"\n");
    int result = make_unwiredlabs_request(json_string, response\n"\n"\n"\n"\n"\n"\n"\n");
    
    json_object_put(request\n"\n"\n"\n"\n"\n"\n"\n");
    return result;
}

// Get location using WiFi access points
int unwiredlabs_api_get_wifi_location(const unwiredlabs_wifi_ap_t* wifi_aps, int wifi_count, unwiredlabs_response_t* response) {
    if (!wifi_aps || wifi_count <= 0 || !response) {
        return -1;
    }
    
    // Build JSON request
    json_object* request = json_object_new_object(\n"\n"\n"\n"\n"\n"\n"\n");
    json_object_object_add(request, "token", json_object_new_string(g_unwiredlabs_config.token)\n"\n"\n"\n"\n"\n"\n"\n");
    
    json_object* wifi_array = json_object_new_array(\n"\n"\n"\n"\n"\n"\n"\n");
    for (int i = 0; i < wifi_count && i < 32; i++) {
        json_object* wifi_obj = json_object_new_object(\n"\n"\n"\n"\n"\n"\n"\n");
        json_object_object_add(wifi_obj, "bssid", json_object_new_string(wifi_aps[i].bssid)\n"\n"\n"\n"\n"\n"\n"\n");
        json_object_object_add(wifi_obj, "signal", json_object_new_int(wifi_aps[i].signal)\n"\n"\n"\n"\n"\n"\n"\n");
        json_object_object_add(wifi_obj, "channel", json_object_new_int(wifi_aps[i].channel)\n"\n"\n"\n"\n"\n"\n"\n");
        json_object_array_add(wifi_array, wifi_obj\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    json_object_object_add(request, "wifi", wifi_array\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (g_unwiredlabs_config.enable_fallbacks) {
        json_object_object_add(request, "fallbacks", json_object_new_boolean(true)\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (g_unwiredlabs_config.enable_address_lookup) {
        json_object_object_add(request, "address", json_object_new_boolean(true)\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    const char* json_string = json_object_to_json_string(request\n"\n"\n"\n"\n"\n"\n"\n");
    int result = make_unwiredlabs_request(json_string, response\n"\n"\n"\n"\n"\n"\n"\n");
    
    json_object_put(request\n"\n"\n"\n"\n"\n"\n"\n");
    return result;
}

// Get location using combined cell and WiFi data
int unwiredlabs_api_get_combined_location(const unwiredlabs_request_t* request, unwiredlabs_response_t* response) {
    if (!request || !response) {
        return -1;
    }
    
    // Build JSON request
    json_object* json_request = json_object_new_object(\n"\n"\n"\n"\n"\n"\n"\n");
    json_object_object_add(json_request, "token", json_object_new_string(request->token)\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Add cell towers
    if (request->cell_count > 0) {
        json_object* cells_array = json_object_new_array(\n"\n"\n"\n"\n"\n"\n"\n");
        for (int i = 0; i < request->cell_count && i < 16; i++) {
            json_object* cell_obj = json_object_new_object(\n"\n"\n"\n"\n"\n"\n"\n");
            json_object_object_add(cell_obj, "lac", json_object_new_string(request->cells[i].lac)\n"\n"\n"\n"\n"\n"\n"\n");
            json_object_object_add(cell_obj, "cid", json_object_new_string(request->cells[i].cid)\n"\n"\n"\n"\n"\n"\n"\n");
            json_object_object_add(cell_obj, "mcc", json_object_new_int(request->cells[i].mcc)\n"\n"\n"\n"\n"\n"\n"\n");
            json_object_object_add(cell_obj, "mnc", json_object_new_int(request->cells[i].mnc)\n"\n"\n"\n"\n"\n"\n"\n");
            json_object_object_add(cell_obj, "radio", json_object_new_string(request->cells[i].radio)\n"\n"\n"\n"\n"\n"\n"\n");
            json_object_object_add(cell_obj, "signal", json_object_new_int(request->cells[i].signal)\n"\n"\n"\n"\n"\n"\n"\n");
            json_object_array_add(cells_array, cell_obj\n"\n"\n"\n"\n"\n"\n"\n");
        }
        json_object_object_add(json_request, "cells", cells_array\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Add WiFi access points
    if (request->wifi_count > 0) {
        json_object* wifi_array = json_object_new_array(\n"\n"\n"\n"\n"\n"\n"\n");
        for (int i = 0; i < request->wifi_count && i < 32; i++) {
            json_object* wifi_obj = json_object_new_object(\n"\n"\n"\n"\n"\n"\n"\n");
            json_object_object_add(wifi_obj, "bssid", json_object_new_string(request->wifi_aps[i].bssid)\n"\n"\n"\n"\n"\n"\n"\n");
            json_object_object_add(wifi_obj, "signal", json_object_new_int(request->wifi_aps[i].signal)\n"\n"\n"\n"\n"\n"\n"\n");
            json_object_object_add(wifi_obj, "channel", json_object_new_int(request->wifi_aps[i].channel)\n"\n"\n"\n"\n"\n"\n"\n");
            json_object_array_add(wifi_array, wifi_obj\n"\n"\n"\n"\n"\n"\n"\n");
        }
        json_object_object_add(json_request, "wifi", wifi_array\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (request->fallbacks_enabled) {
        json_object_object_add(json_request, "fallbacks", json_object_new_boolean(true)\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (request->address_lookup) {
        json_object_object_add(json_request, "address", json_object_new_boolean(true)\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    const char* json_string = json_object_to_json_string(json_request\n"\n"\n"\n"\n"\n"\n"\n");
    int result = make_unwiredlabs_request(json_string, response\n"\n"\n"\n"\n"\n"\n"\n");
    
    json_object_put(json_request\n"\n"\n"\n"\n"\n"\n"\n");
    return result;
}

// Get API statistics
int unwiredlabs_api_get_stats(unwiredlabs_stats_t* stats) {
    if (!g_unwiredlabs_initialized || !stats) {
        return -1;
    }
    
    memcpy(stats, &g_unwiredlabs_stats, sizeof(unwiredlabs_stats_t)\n"\n"\n"\n"\n"\n"\n"\n");
    return 0;
}

// Check if UnwiredLabs API is initialized
bool unwiredlabs_api_is_initialized(void) {
    return g_unwiredlabs_initialized;
}

// Validate API token
bool unwiredlabs_api_validate_token(void) {
    if (!g_unwiredlabs_initialized) {
        return false;
    }
    
    // Validate API token with real cellular data
    unwiredlabs_response_t validation_response;
    
    // Get real cellular data from system
    unwiredlabs_cell_t real_cell = {0};
    
    // Try to get real cellular information from modem
    FILE *cell_fp = popen("mmcli -m 0 --command='AT+QCELLINFO' 2>/dev/null", "r"\n"\n"\n"\n"\n"\n"\n"\n");
    if (cell_fp) {
        char cell_info[256];
        if (fgets(cell_info, sizeof(cell_info), cell_fp)) {
            // Parse real cell information
            int mcc, mnc, lac, cid, signal;
            if (sscanf(cell_info, "+QCELLINFO: %d,%d,%d,%d,%d", &mcc, &mnc, &lac, &cid, &signal) >= 4) {
                real_cell.mcc = mcc;
                real_cell.mnc = mnc;
                snprintf(real_cell.lac, sizeof(real_cell.lac), "%d", lac\n"\n"\n"\n"\n"\n"\n"\n");
                snprintf(real_cell.cid, sizeof(real_cell.cid), "%d", cid\n"\n"\n"\n"\n"\n"\n"\n");
                real_cell.signal = signal;
                safe_strncpy(real_cell.radio, "lte", sizeof(real_cell.radio)\n"\n"\n"\n"\n"\n"\n"\n");
                real_cell.radio[sizeof(real_cell.radio) - 1] = '\0';
            }
        }
        pclose(cell_fp\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Fallback: try gsmctl for cellular data
    if (real_cell.mcc == 0) {
        FILE *gsmctl_fp = popen("gsmctl -A 'AT+QCELLINFO' 2>/dev/null", "r"\n"\n"\n"\n"\n"\n"\n"\n");
        if (gsmctl_fp) {
            char gsmctl_info[256];
            if (fgets(gsmctl_info, sizeof(gsmctl_info), gsmctl_fp)) {
                // Parse gsmctl cell information
                int mcc, mnc, lac, cid, signal;
                if (sscanf(gsmctl_info, "+QCELLINFO: %d,%d,%d,%d,%d", &mcc, &mnc, &lac, &cid, &signal) >= 4) {
                    real_cell.mcc = mcc;
                    real_cell.mnc = mnc;
                    snprintf(real_cell.lac, sizeof(real_cell.lac), "%d", lac\n"\n"\n"\n"\n"\n"\n"\n");
                    snprintf(real_cell.cid, sizeof(real_cell.cid), "%d", cid\n"\n"\n"\n"\n"\n"\n"\n");
                    real_cell.signal = signal;
                    safe_strncpy(real_cell.radio, "lte", sizeof(real_cell.radio)\n"\n"\n"\n"\n"\n"\n"\n");
                    real_cell.radio[sizeof(real_cell.radio) - 1] = '\0';
                }
            }
            pclose(gsmctl_fp\n"\n"\n"\n"\n"\n"\n"\n");
        }
    }
    
    // If we have real cellular data, use it for validation
    if (real_cell.mcc > 0) {
        int result = unwiredlabs_api_get_cellular_location(&real_cell, 1, &validation_response\n"\n"\n"\n"\n"\n"\n"\n");
        if (result == 0 && validation_response.success) {
            printf("DEBUG: "UnwiredLabs API validation successful with real cellular data"\n"\n"\n"\n"\n"\n"\n"\n");
            return true;
        }
    }
    
    // Fallback: validate API token with HTTP request
    char validation_url[512];
    snprintf(validation_url, sizeof(validation_url),
            "https://us1.unwiredlabs.com/v2/balance.php?token=%s", 
            g_unwiredlabs_config.token\n"\n"\n"\n"\n"\n"\n"\n");
    
    FILE *http_fp = popen("curl -s --connect-timeout 10 --max-time 30", "w"\n"\n"\n"\n"\n"\n"\n"\n");
    if (http_fp) {
        fprintf(http_fp, "GET %s HTTP/1.1\r\nHost: us1.unwiredlabs.com\r\n\r\n", validation_url\n"\n"\n"\n"\n"\n"\n"\n");
        pclose(http_fp\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Check response
        char http_response[256];
        FILE *http_resp = popen("curl -s --connect-timeout 10 --max-time 30", "r"\n"\n"\n"\n"\n"\n"\n"\n");
        if (http_resp && fgets(http_response, sizeof(http_response), http_resp)) {
            // Check if response contains valid balance or error message
            if (strstr(http_response, "balance") || strstr(http_response, "credits")) {
                printf("DEBUG: "UnwiredLabs API token validation successful via HTTP"\n"\n"\n"\n"\n"\n"\n"\n");
                pclose(http_resp\n"\n"\n"\n"\n"\n"\n"\n");
                return true;
            }
        }
        if (http_resp) pclose(http_resp\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    printf("WARN: "UnwiredLabs API token validation failed"\n"\n"\n"\n"\n"\n"\n"\n");
    return false;
}

// Check quota status
int unwiredlabs_api_get_quota_remaining(void) {
    // UnwiredLabs API doesn't provide quota information in the response
    // This would need to be tracked separately or queried from their dashboard
    return -1; // Unknown
}
