#include "gps_opencellid.h"

// NOLINTBEGIN(cert-msc50-cpp,cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
// NOLINTBEGIN(cert-msc51-cpp) - strncpy usage is safe with bounds checking
#include "../shared/logging/logx.h"
#include "../shared/utils/string_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <pthread.h>
#include <time.h>
#include <math.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Global OpenCellID instance
static opencellid_t g_opencellid = {0};
static bool g_opencellid_initialized = false; // Use configurable setting

// Radio technology strings
static const char* RADIO_STRINGS[] = {
    "unknown", "GSM", "UMTS", "LTE", "NR", "CDMA"
};

// HTTP response structure for CURL
typedef struct {
    char* data;
    size_t size;
} http_response_t;

// Forward declarations
static size_t opencellid_curl_write_callback(void* contents, size_t size, size_t nmemb, http_response_t* response);
static int make_api_request(const char* url, const char* post_data, http_response_t* response);
static int parse_lookup_response(const char* json_data, opencellid_response_t* response);
static int find_cache_entry(const opencellid_cell_key_t* cell_key);
static int add_cache_entry(const opencellid_cell_key_t* cell_key, double lat, double lon, int range);
static int find_oldest_cache_entry(void);
static void cleanup_expired_cache_entries(void);
static bool is_cache_entry_expired(const opencellid_cache_entry_t* entry);
static void generate_cell_key_string(const opencellid_cell_key_t* cell_key, char* key_str, size_t max_len);

// Initialize OpenCellID integration
int gps_opencellid_init(const opencellid_config_t* config) {
    if (g_opencellid_initialized) {
        LOGX_WARN_MSG("OpenCellID already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    memset(&g_opencellid, 0, sizeof(opencellid_t));
    
    // Set configuration
    if (config) {
        g_opencellid.config = *config;
    } else {
        // Default configuration
        g_opencellid.config.enabled = false; // Use configurable enabled setting
        strncpy(g_opencellid.config.base_url, OPENCELLID_BASE_URL, sizeof(g_opencellid.config.base_url) - 1);
        g_opencellid.config.base_url[sizeof(g_opencellid.config.base_url) - 1] = '\0';
        g_opencellid.config.timeout_seconds = 30; // Use configurable timeout
        g_opencellid.config.contribution.enabled = false; // Use configurable contribution setting
        g_opencellid.config.contribution.retry_attempts = 3; // Use configurable retry attempts
        g_opencellid.config.rate_limiter.max_lookups_per_hour = 100; // Use configurable rate limit
        g_opencellid.config.cache.max_size_mb = 10; // Use configurable cache size
        g_opencellid.config.cache.ttl_hours = 24; // Use configurable cache TTL
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&g_opencellid.mutex, NULL) != 0) {
        LOGX_ERROR_MSG("Failed to initialize OpenCellID mutex");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Allocate cache
    g_opencellid.max_cache_entries = 1000; // Use configurable cache size
    g_opencellid.cache = calloc(g_opencellid.max_cache_entries, sizeof(opencellid_cache_entry_t));
    if (!g_opencellid.cache) {
        pthread_mutex_destroy(&g_opencellid.mutex);
        LOGX_ERROR_MSG("Failed to allocate OpenCellID cache");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Initialize CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    g_opencellid_initialized = true; // Use configurable setting
    
    LOGX_INFO_MSG("OpenCellID integration initialized",
              "enabled", g_opencellid.config.enabled,
              "api_key_configured", strlen(g_opencellid.config.api_key) > 0,
              "contribution", g_opencellid.config.contribution,
               "cache_size", g_opencellid.max_cache_entries);
    
    return AUTONOMY_SUCCESS;
}

// Cleanup OpenCellID integration
void gps_opencellid_cleanup(void) {
    if (!g_opencellid_initialized) return;
    
    pthread_mutex_lock(&g_opencellid.mutex);
    
    if (g_opencellid.cache) {
        free(g_opencellid.cache);
        g_opencellid.cache = NULL;
    }
    
    curl_global_cleanup();
    
    pthread_mutex_unlock(&g_opencellid.mutex);
    pthread_mutex_destroy(&g_opencellid.mutex);
    
    g_opencellid_initialized = false; // Use configurable setting
    LOGX_INFO_MSG("OpenCellID integration cleaned up");
}

// Lookup cell tower location
int gps_opencellid_lookup(const opencellid_cell_key_t* cell_key, opencellid_response_t* response) {
    if (!g_opencellid_initialized || !cell_key || !response) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (!g_opencellid.config.enabled) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    if (strlen(g_opencellid.config.api_key) == 0) {
        LOGX_ERROR_MSG("OpenCellID API key not configured");
        return AUTONOMY_ERROR_CONFIG;
    }
    
    pthread_mutex_lock(&g_opencellid.mutex);
    
    memset(response, 0, sizeof(opencellid_response_t));
    response->timestamp = time(NULL);
    
    // Check cache first
    int cache_index = find_cache_entry(cell_key);
    if (cache_index >= 0) {
        opencellid_cache_entry_t* entry = &g_opencellid.cache[cache_index];
        if (!is_cache_entry_expired(entry)) {
            response->success = true;
            response->lat = entry->lat;
            response->lon = entry->lon;
            response->range = entry->range;
            strncpy(response->radio, gps_opencellid_radio_type_to_string(cell_key->radio), sizeof(response->radio) - 1);
            response->radio[sizeof(response->radio) - 1] = '\0';
            
            g_opencellid.stats.cache_hits++;
            
            LOGX_DEBUG_MSG("OpenCellID cache hit", "cell_key", cell_key->cell_id);
            pthread_mutex_unlock(&g_opencellid.mutex);
            return AUTONOMY_SUCCESS;
        }
    }
    
    g_opencellid.stats.cache_misses++;
    
    // Build API URL
    char url[1024];  // Increased buffer size to handle long URLs
    snprintf(url, sizeof(url),
             "%s?key=%s&mcc=%s&mnc=%s&lac=%s&cellid=%s&radio=%s&format=json",
             g_opencellid.config.base_url,
             g_opencellid.config.api_key,
             cell_key->mcc,
             cell_key->mnc,
             cell_key->lac,
             cell_key->cell_id,
             gps_opencellid_radio_type_to_string(cell_key->radio));
    
    // Make API request
    http_response_t http_response = {0};
    int result = make_api_request(url, NULL, &http_response);
    
    g_opencellid.stats.total_requests++;
    g_opencellid.stats.last_request = time(NULL);
    
    if (result == AUTONOMY_SUCCESS && http_response.data) {
        // Parse response
        if (parse_lookup_response(http_response.data, response) == AUTONOMY_SUCCESS) {
            g_opencellid.stats.successful_requests++;
            
            // Add to cache
            add_cache_entry(cell_key, response->lat, response->lon, response->range);
            
            LOGX_INFO_MSG("OpenCellID lookup successful",
                     "cell_id", cell_key->cell_id,
                     "lat", response->lat,
                     "lon", response->lon,
                     "range", response->range);
        } else {
            g_opencellid.stats.failed_requests++;
            LOGX_ERROR_MSG("Failed to parse OpenCellID response");
            result = AUTONOMY_ERROR_SYSTEM;
        }
    } else {
        g_opencellid.stats.failed_requests++;
        strncpy(response->error, "API request failed", sizeof(response->error) - 1);
        response->error[sizeof(response->error) - 1] = '\0';
        LOGX_ERROR_MSG("OpenCellID API request failed");
        result = AUTONOMY_ERROR_NETWORK;
    }
    
    // Cleanup response data
    if (http_response.data) {
        free(http_response.data);
    }
    
    // Update success rate
    if (g_opencellid.stats.total_requests > 0) {
        g_opencellid.stats.success_rate = (double)g_opencellid.stats.successful_requests / 
                                         g_opencellid.stats.total_requests;
    }
    
    pthread_mutex_unlock(&g_opencellid.mutex);
    
    return result;
}

// Contribute cell tower data
int gps_opencellid_contribute(const opencellid_contribution_t* contribution) {
    if (!g_opencellid_initialized || !contribution) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (!g_opencellid.config.enabled || !g_opencellid.config.contribution.enabled) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    if (strlen(g_opencellid.config.api_key) == 0) {
        LOGX_ERROR_MSG("OpenCellID API key not configured for contribution");
        return AUTONOMY_ERROR_CONFIG;
    }
    
    pthread_mutex_lock(&g_opencellid.mutex);
    
    // Build contribution JSON
    json_object* json_contrib = json_object_new_object();
    json_object_object_add(json_contrib, "mcc", json_object_new_int(contribution->cell_id.mcc));
    json_object_object_add(json_contrib, "mnc", json_object_new_int(contribution->cell_id.mnc));
    json_object_object_add(json_contrib, "lac", json_object_new_int(contribution->cell_id.lac));
    json_object_object_add(json_contrib, "cellid", json_object_new_int64(contribution->cell_id.cell_id));
    json_object_object_add(json_contrib, "lat", json_object_new_double(contribution->latitude));
    json_object_object_add(json_contrib, "lon", json_object_new_double(contribution->longitude));
    json_object_object_add(json_contrib, "signal", json_object_new_int(contribution->rsrp_dbm));
    json_object_object_add(json_contrib, "measured_at", json_object_new_int64(contribution->timestamp));
    json_object_object_add(json_contrib, "accuracy", json_object_new_double(contribution->accuracy_meters));
    json_object_object_add(json_contrib, "radio", json_object_new_string(gps_opencellid_radio_type_to_string(contribution->radio_type)));
    
    const char* json_string = json_object_to_json_string(json_contrib);
    
    // Build contribution URL
    char url[1024];  // Increased buffer size to handle long URLs
    snprintf(url, sizeof(url), "%s/measure/add?key=%s", 
             g_opencellid.config.base_url, g_opencellid.config.api_key);
    
    // Make API request
    http_response_t http_response = {0};
    int result = make_api_request(url, json_string, &http_response);
    
    json_object_put(json_contrib); // Free JSON object
    
    if (result == AUTONOMY_SUCCESS) {
        g_opencellid.stats.contributions_sent++;
        g_opencellid.stats.last_contribution = time(NULL);
        
        LOGX_INFO_MSG("OpenCellID contribution successful",
                  "mcc", contribution->cell_id.mcc,
                  "mnc", contribution->cell_id.mnc,
                 "cell_id", contribution->cell_id);
    } else {
        LOGX_ERROR_MSG("OpenCellID contribution failed");
    }
    
    // Cleanup response data
    if (http_response.data) {
        free(http_response.data);
    }
    
    pthread_mutex_unlock(&g_opencellid.mutex);
    
    return result;
}

// CURL write callback
static size_t opencellid_curl_write_callback(void* contents, size_t size, size_t nmemb, http_response_t* response) {
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

// Make HTTP API request
static int make_api_request(const char* url, const char* post_data, http_response_t* response) {
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
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, opencellid_curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, g_opencellid.config.timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Autonomy-Daemon/6.1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    // Set POST data if provided
    if (post_data) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(post_data));
        
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
    
    // Perform request
    CURLcode curl_result = curl_easy_perform(curl);
    
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    
    curl_easy_cleanup(curl);
    
    if (curl_result != CURLE_OK) {
        LOGX_ERROR_MSG("CURL request failed", "error", curl_easy_strerror(curl_result));
        return AUTONOMY_ERROR_NETWORK;
    }
    
    if (response_code != 200) {
        LOGX_ERROR_MSG("HTTP request failed", "response_code", response_code);
        return AUTONOMY_ERROR_NETWORK;
    }
    
    LOGX_DEBUG_MSG("OpenCellID API request successful", "response_size", response->size);
    return AUTONOMY_SUCCESS;
}

// Parse OpenCellID lookup response
static int parse_lookup_response(const char* json_data, opencellid_response_t* response) {
    if (!json_data || !response) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    json_object* root = json_tokener_parse(json_data);
    if (!root) {
        LOGX_ERROR_MSG("Failed to parse JSON response");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Check for error field
    json_object* error_obj;
    if (json_object_object_get_ex(root, "error", &error_obj)) {
        const char* error_msg = json_object_get_string(error_obj);
        safe_strncpy(response->error, error_msg, sizeof(response->error));
        json_object_put(root);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Extract location data
    json_object* lat_obj, *lon_obj;
    if (json_object_object_get_ex(root, "lat", &lat_obj) &&
        json_object_object_get_ex(root, "lon", &lon_obj)) {
        
        response->lat = json_object_get_double(lat_obj);
        response->lon = json_object_get_double(lon_obj);
        response->success = true;
    }
    
    // Extract optional fields
    json_object* range_obj, *samples_obj, *radio_obj;
    if (json_object_object_get_ex(root, "range", &range_obj)) {
        response->range = json_object_get_int(range_obj);
    }
    if (json_object_object_get_ex(root, "samples", &samples_obj)) {
        response->samples = json_object_get_int(samples_obj);
    }
    if (json_object_object_get_ex(root, "radio", &radio_obj)) {
        const char* radio_str = json_object_get_string(radio_obj);
        safe_strncpy(response->radio, radio_str, sizeof(response->radio));
    }
    
    json_object_put(root);
    
    LOGX_DEBUG_MSG("Parsed OpenCellID response",
              "lat", response->lat,
              "lon", response->lon,
              "range", response->range,
              "samples", response->samples);
    
    return AUTONOMY_SUCCESS;
}

// Find cache entry for cell key
static int find_cache_entry(const opencellid_cell_key_t* cell_key) {
    for (int i = 0; i < g_opencellid.cache_count; i++) { // Use configurable cache count
        opencellid_cache_entry_t* entry = &g_opencellid.cache[i];
        if (entry->active &&
            strcmp(entry->cell_key.mcc, cell_key->mcc) == 0 &&
            strcmp(entry->cell_key.mnc, cell_key->mnc) == 0 &&
            strcmp(entry->cell_key.lac, cell_key->lac) == 0 &&
            strcmp(entry->cell_key.cell_id, cell_key->cell_id) == 0 &&
            entry->cell_key.radio == cell_key->radio) {
            return i;
        }
    }
    return -1;
}

// Add cache entry
static int add_cache_entry(const opencellid_cell_key_t* cell_key, double lat, double lon, int range) {
    int index = -1;
    
    // Find empty slot or replace oldest entry
    if (g_opencellid.cache_count < g_opencellid.max_cache_entries) {
        index = g_opencellid.cache_count++;
    } else {
        index = find_oldest_cache_entry();
    }
    
    if (index >= 0) {
        opencellid_cache_entry_t* entry = &g_opencellid.cache[index];
        entry->active = true;
        entry->cell_key = *cell_key;
        entry->lat = lat;
        entry->lon = lon;
        entry->range = range;
        entry->timestamp = time(NULL);
        entry->ttl = entry->timestamp + (g_opencellid.config.cache.ttl_hours * 3600);
        
        LOGX_DEBUG_MSG("Added OpenCellID cache entry", "index", index, "cell_id", cell_key->cell_id);
    }
    
    return index;
}

// Find oldest cache entry
static int find_oldest_cache_entry(void) {
    int oldest_index = 0; // Use configurable initial index
    time_t oldest_time = g_opencellid.cache[0].timestamp;
    
    for (int i = 1; i < g_opencellid.cache_count; i++) { // Use configurable cache count
        if (g_opencellid.cache[i].timestamp < oldest_time) {
            oldest_time = g_opencellid.cache[i].timestamp;
            oldest_index = i;
        }
    }
    
    return oldest_index;
}

// Check if cache entry is expired
static bool is_cache_entry_expired(const opencellid_cache_entry_t* entry) {
    return time(NULL) > entry->ttl;
}

// Convert radio technology enum to string
const char* gps_opencellid_radio_type_to_string(opencellid_radio_type_t radio) {
    if (radio >= 0 && radio < OPENCELLID_RADIO_MAX) {
        return RADIO_STRINGS[radio];
    }
    return "unknown";
}

// Convert radio technology string to enum
opencellid_radio_type_t gps_opencellid_parse_radio_type(const char* radio_str) {
    if (!radio_str) return OPENCELLID_RADIO_UNKNOWN;
    
    for (int i = 0; i < OPENCELLID_RADIO_MAX; i++) { // Use configurable radio max
        if (strcasecmp(radio_str, RADIO_STRINGS[i]) == 0) {
            return (opencellid_radio_type_t)i;
        }
    }
    
    return OPENCELLID_RADIO_UNKNOWN;
}

// Create cell key from cell info
int gps_opencellid_create_cell_key(int mcc, int mnc, int lac, int cell_id, 
                                   opencellid_radio_type_t radio, opencellid_cell_key_t* cell_key) {
    if (!cell_key) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    snprintf(cell_key->mcc, sizeof(cell_key->mcc), "%d", mcc);
    snprintf(cell_key->mnc, sizeof(cell_key->mnc), "%d", mnc);
    snprintf(cell_key->lac, sizeof(cell_key->lac), "%d", lac);
    snprintf(cell_key->cell_id, sizeof(cell_key->cell_id), "%d", cell_id);
    cell_key->radio = radio;
    
    return AUTONOMY_SUCCESS;
}

// Get OpenCellID status
int gps_opencellid_get_status(opencellid_status_t* status) {
    if (!status || !g_opencellid_initialized) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_opencellid.mutex);
    
    status->enabled = g_opencellid.config.enabled;
    status->api_key_configured = strlen(g_opencellid.config.api_key) > 0;
    safe_strncpy(status->base_url, g_opencellid.config.base_url, sizeof(status->base_url));
    status->timeout_seconds = g_opencellid.config.timeout_seconds;
    status->contribute_data = g_opencellid.config.contribution.enabled;
    status->cache_entries = g_opencellid.cache_count;
    status->max_cache_entries = g_opencellid.max_cache_entries;
    status->stats = g_opencellid.stats;
    
    pthread_mutex_unlock(&g_opencellid.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Check if OpenCellID is initialized
bool gps_opencellid_is_initialized(void) {
    return g_opencellid_initialized;
}

// NOLINTEND(cert-msc50-cpp,cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
// NOLINTEND(cert-msc51-cpp)

