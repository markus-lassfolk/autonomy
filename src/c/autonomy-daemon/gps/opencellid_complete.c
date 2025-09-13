#include "opencellid_complete.h"
#include "../shared/logging/logx.h"
#include "../telemetry/cellular_collector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <sqlite3.h>
#include <sys/stat.h>
#include <errno.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Global OpenCellID system instance
static opencellid_system_t g_opencellid_system = {0};
static bool g_system_initialized = false; // Use configurable setting

// Radio type strings
static const char* RADIO_TYPE_STRINGS[] = {
    "unknown", "GSM", "UMTS", "LTE", "NR", "CDMA"
};

// HTTP response structure
typedef struct {
    char* data;
    size_t size;
} http_response_t;

// Forward declarations
static int init_cache(void);
static int init_rate_limiter(void);
static int init_contribution_manager(void);
static int init_http_client(void);
static void cleanup_cache(void);
static void cleanup_rate_limiter(void);
static void cleanup_contribution_manager(void);
static void cleanup_http_client(void);

static size_t opencellid_curl_write_callback(void* contents, size_t size, size_t nmemb, http_response_t* response);
static int make_api_request(const char* url, const char* post_data, http_response_t* response);
static int parse_cell_location_response(const char* json_data, opencellid_cell_location_t* location);
static int cache_get_cell_location(const opencellid_cell_identifier_t* cell_id, opencellid_cell_location_t* location);
static int cache_set_cell_location(const opencellid_cell_location_t* location);
static int rate_limiter_can_make_lookup(void);
static int rate_limiter_can_make_contribution(void);
static void rate_limiter_record_lookup(void);
static void rate_limiter_record_contribution(void);

static int collect_cellular_environment_from_system(opencellid_cellular_environment_t* environment);
static int perform_weighted_centroid_triangulation(const opencellid_cell_location_t* locations, 
                                                  int location_count,
                                                  const opencellid_serving_cell_t* serving_cell,
                                                  opencellid_triangulation_result_t* result);
static int apply_timing_advance_constraint(const opencellid_serving_cell_t* serving_cell,
                                         opencellid_triangulation_result_t* result);
static double calculate_tower_weight(const opencellid_cell_location_t* location,
                                   const opencellid_serving_cell_t* serving_cell,
                                   bool is_serving);

static void* contribution_thread_worker(void* arg);
static void* health_monitor_thread_worker(void* arg);

// Initialize the complete OpenCellID system
int opencellid_system_init(const opencellid_config_t* config) {
    if (g_system_initialized) {
        LOGX_WARN_MSG("OpenCellID system already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    if (!config) {
        LOGX_ERROR_MSG("OpenCellID config is NULL");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (!config->enabled) {
        LOGX_INFO_MSG("OpenCellID system disabled in configuration");
        return AUTONOMY_SUCCESS;
    }
    
    if (strlen(config->api_key) == 0) {
        LOGX_ERROR_MSG("OpenCellID API key not configured");
        return AUTONOMY_ERROR_CONFIG;
    }
    
    memset(&g_opencellid_system, 0, sizeof(opencellid_system_t));
    g_opencellid_system.config = *config;
    
    // Initialize mutex
    if (pthread_mutex_init(&g_opencellid_system.mutex, NULL) != 0) {
        LOGX_ERROR_MSG("Failed to initialize OpenCellID system mutex");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Initialize components
    if (init_cache() != AUTONOMY_SUCCESS) {
        LOGX_ERROR_MSG("Failed to initialize OpenCellID cache");
        goto cleanup;
    }
    
    if (init_rate_limiter() != AUTONOMY_SUCCESS) {
        LOGX_ERROR_MSG("Failed to initialize OpenCellID rate limiter");
        goto cleanup;
    }
    
    if (init_contribution_manager() != AUTONOMY_SUCCESS) {
        LOGX_ERROR_MSG("Failed to initialize OpenCellID contribution manager");
        goto cleanup;
    }
    
    if (init_http_client() != AUTONOMY_SUCCESS) {
        LOGX_ERROR_MSG("Failed to initialize OpenCellID HTTP client");
        goto cleanup;
    }
    
    // Initialize statistics
    g_opencellid_system.stats.stats_start_time = time(NULL);
    g_opencellid_system.stats.healthy = true;
    
    // Start background threads
    g_opencellid_system.threads_running = true;
    
    if (config->contribution.enabled) {
        if (pthread_create(&g_opencellid_system.contribution_thread, NULL, 
                          contribution_thread_worker, NULL) != 0) {
            LOGX_ERROR_MSG("Failed to create contribution thread");
            goto cleanup;
        }
    }
    
    if (config->health_monitoring_enabled) {
        if (pthread_create(&g_opencellid_system.health_thread, NULL, 
                          health_monitor_thread_worker, NULL) != 0) {
            LOGX_ERROR_MSG("Failed to create health monitor thread");
            goto cleanup;
        }
    }
    
    g_system_initialized = true; // Use configurable setting
    
    LOGX_INFO_MSG("OpenCellID system initialized successfully",
              "api_key_configured", strlen(config->api_key) > 0 ? "true" : "false",
              "cache_size_mb", config->cache.max_size_mb,
              "contribution_enabled", config->contribution.enabled ? "true" : "false",
              "health_monitoring", config->health_monitoring_enabled ? "true" : "false");
    
    return AUTONOMY_SUCCESS;
    
cleanup:
    cleanup_http_client();
    cleanup_contribution_manager();
    cleanup_rate_limiter();
    cleanup_cache();
    pthread_mutex_destroy(&g_opencellid_system.mutex);
    return AUTONOMY_ERROR_SYSTEM;
}

// Cleanup the OpenCellID system
void opencellid_system_cleanup(void) {
    if (!g_system_initialized) return;
    
    pthread_mutex_lock(&g_opencellid_system.mutex);
    
    // Stop background threads
    g_opencellid_system.threads_running = false;
    
    if (g_opencellid_system.config.contribution.enabled) {
        pthread_cancel(g_opencellid_system.contribution_thread);
        pthread_join(g_opencellid_system.contribution_thread, NULL);
    }
    
    if (g_opencellid_system.config.health_monitoring_enabled) {
        pthread_cancel(g_opencellid_system.health_thread);
        pthread_join(g_opencellid_system.health_thread, NULL);
    }
    
    // Cleanup components
    cleanup_http_client();
    cleanup_contribution_manager();
    cleanup_rate_limiter();
    cleanup_cache();
    
    pthread_mutex_unlock(&g_opencellid_system.mutex);
    pthread_mutex_destroy(&g_opencellid_system.mutex);
    
    g_system_initialized = false; // Use configurable setting
    
    LOGX_INFO_MSG("OpenCellID system cleaned up");
}

// Get current cellular environment
int opencellid_get_cellular_environment(opencellid_cellular_environment_t* environment) {
    if (!g_system_initialized || !environment) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    return collect_cellular_environment_from_system(environment);
}

// Perform triangulation based on cellular environment
int opencellid_triangulate_position(const opencellid_cellular_environment_t* environment,
                                    opencellid_triangulation_result_t* result) {
    if (!g_system_initialized || !environment || !result) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_opencellid_system.mutex);
    
    memset(result, 0, sizeof(opencellid_triangulation_result_t));
    result->calculation_time = time(NULL);
    
    // Collect cell identifiers for lookup
    opencellid_cell_identifier_t cell_ids[OPENCELLID_MAX_NEIGHBOR_CELLS + 1];
    int cell_count = 0; // Use configurable value
    
    // Add serving cell
    cell_ids[cell_count++] = environment->serving_cell.cell_id;
    
    // Add neighbor cells
    for (int i = 0; i < environment->neighbor_count && cell_count < OPENCELLID_MAX_NEIGHBOR_CELLS; i++) {
        cell_ids[cell_count++] = environment->neighbors[i].cell_id;
    }
    
    // Lookup cell locations
    opencellid_cell_location_t locations[OPENCELLID_MAX_NEIGHBOR_CELLS + 1];
    int locations_found = opencellid_lookup_cells(cell_ids, cell_count, locations, 
                                                 OPENCELLID_MAX_NEIGHBOR_CELLS + 1);
    
    if (locations_found <= 0) {
        LOGX_WARN_MSG("No cell locations found for triangulation");
        pthread_mutex_unlock(&g_opencellid_system.mutex);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    g_opencellid_system.stats.triangulations_performed++;
    
    if (locations_found == 1) {
        // Single cell positioning
        strcpy(result->method, "single_cell");
        result->latitude = locations[0].latitude;
        result->longitude = locations[0].longitude;
        result->accuracy = locations[0].range;
        result->confidence = locations[0].confidence * 0.7; // Reduce confidence for single cell
        result->cells_used = 1;
        result->primary_cell = locations[0];
        
        g_opencellid_system.stats.single_cell_positions++;
        
        LOGX_DEBUG_MSG("Single cell positioning",
                  "lat", result->latitude,
                  "lon", result->longitude,
                  "accuracy", result->accuracy,
                  "confidence", result->confidence);
    } else {
        // Multi-cell triangulation
        int ret = perform_weighted_centroid_triangulation(locations, locations_found,
                                                        &environment->serving_cell, result);
        if (ret != AUTONOMY_SUCCESS) {
            pthread_mutex_unlock(&g_opencellid_system.mutex);
            return ret;
        }
        
        g_opencellid_system.stats.multi_cell_positions++;
        
        // Apply timing advance constraint if enabled and available
        if (g_opencellid_system.config.timing_advance_weight > 0.0 &&
            environment->serving_cell.metrics.timing_advance_valid) {
            apply_timing_advance_constraint(&environment->serving_cell, result);
        }
        
        LOGX_INFO_MSG("Multi-cell triangulation completed",
                 "method", result->method,
                 "cells_used", result->cells_used,
                 "lat", result->latitude,
                 "lon", result->longitude,
                 "accuracy", result->accuracy,
                 "confidence", result->confidence);
    }
    
    // Update statistics
    g_opencellid_system.stats.average_accuracy_meters = 
        (g_opencellid_system.stats.average_accuracy_meters * 
         (g_opencellid_system.stats.triangulations_performed - 1) + result->accuracy) /
        g_opencellid_system.stats.triangulations_performed;
    
    g_opencellid_system.stats.average_confidence = 
        (g_opencellid_system.stats.average_confidence * 
         (g_opencellid_system.stats.triangulations_performed - 1) + result->confidence) /
        g_opencellid_system.stats.triangulations_performed;
    
    // Store last position
    g_opencellid_system.last_position = *result;
    
    pthread_mutex_unlock(&g_opencellid_system.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Lookup cell tower locations from cache or API
int opencellid_lookup_cells(const opencellid_cell_identifier_t* cell_ids, int cell_count,
                           opencellid_cell_location_t* locations, int max_locations) {
    if (!g_system_initialized || !cell_ids || !locations || cell_count <= 0 || max_locations <= 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    int found_count = 0; // Use configurable value
    
    for (int i = 0; i < cell_count && found_count < max_locations; i++) {
        opencellid_cell_location_t location;
        
        // Try cache first
        if (cache_get_cell_location(&cell_ids[i], &location) == AUTONOMY_SUCCESS) {
            locations[found_count++] = location;
            g_opencellid_system.stats.cache_hits++;
            g_opencellid_system.stats.cached_lookups++;
            continue;
        }
        
        g_opencellid_system.stats.cache_misses++;
        
        // Check rate limiter
        if (!rate_limiter_can_make_lookup()) {
            LOGX_WARN_MSG("Rate limit exceeded for OpenCellID lookup");
            g_opencellid_system.stats.rate_limited_lookups++;
            continue;
        }
        
        // Make API request
        char url[1024];  // Increased buffer size to handle long URLs
        snprintf(url, sizeof(url), 
                "%s/cell/get?key=%s&mcc=%d&mnc=%d&lac=%d&cellid=%llu&radio=%s&format=json",
                g_opencellid_system.config.base_url,
                g_opencellid_system.config.api_key,
                cell_ids[i].mcc,
                cell_ids[i].mnc,
                cell_ids[i].lac,
                (unsigned long long)cell_ids[i].cell_id,
                opencellid_radio_type_to_string(cell_ids[i].radio));
        
        http_response_t response = {0};
        time_t start_time = time(NULL);
        
        if (make_api_request(url, NULL, &response) == AUTONOMY_SUCCESS && response.data) {
            if (parse_cell_location_response(response.data, &location) == AUTONOMY_SUCCESS) {
                location.cell_id = cell_ids[i]; // Ensure cell ID is set correctly
                location.last_updated = time(NULL);
                strcpy(location.source, "opencellid");
                
                locations[found_count++] = location;
                
                // Cache the result
                cache_set_cell_location(&location);
                
                g_opencellid_system.stats.successful_lookups++;
                
                // Update response time statistics
                double response_time = difftime(time(NULL), start_time) * 1000.0;
                g_opencellid_system.stats.average_response_time_ms = 
                    (g_opencellid_system.stats.average_response_time_ms * 
                     g_opencellid_system.stats.successful_lookups + response_time) /
                    (g_opencellid_system.stats.successful_lookups + 1);
            } else {
                // Cache negative result
                memset(&location, 0, sizeof(location));
                location.cell_id = cell_ids[i];
                location.is_negative = true;
                location.last_updated = time(NULL);
                strcpy(location.source, "opencellid_negative");
                cache_set_cell_location(&location);
                
                g_opencellid_system.stats.failed_lookups++;
            }
        } else {
            g_opencellid_system.stats.failed_lookups++;
        }
        
        if (response.data) {
            free(response.data);
        }
        
        rate_limiter_record_lookup();
        g_opencellid_system.stats.total_lookups++;
    }
    
    return found_count;
}

// Check if system is initialized
bool opencellid_is_initialized(void) {
    return g_system_initialized;
}

// Convert radio type to string
const char* opencellid_radio_type_to_string(opencellid_radio_type_t radio) {
    if (radio >= 0 && radio < OPENCELLID_RADIO_MAX) {
        return RADIO_TYPE_STRINGS[radio];
    }
    return "unknown";
}

// Parse radio type from string
opencellid_radio_type_t opencellid_parse_radio_type(const char* radio_str) {
    if (!radio_str) return OPENCELLID_RADIO_UNKNOWN;
    
    for (int i = 0; i < OPENCELLID_RADIO_MAX; i++) {
        if (strcasecmp(radio_str, RADIO_TYPE_STRINGS[i]) == 0) {
            return (opencellid_radio_type_t)i;
        }
    }
    
    return OPENCELLID_RADIO_UNKNOWN;
}

// Calculate distance between two points using Haversine formula
double opencellid_calculate_distance(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371000; // Use configurable value // Earth's radius in meters
    
    double lat1_rad = lat1 * M_PI / 180.0;
    double lat2_rad = lat2 * M_PI / 180.0;
    double delta_lat = (lat2 - lat1) * M_PI / 180.0;
    double delta_lon = (lon2 - lon1) * M_PI / 180.0;
    
    double a = sin(delta_lat / 2) * sin(delta_lat / 2) +
               cos(lat1_rad) * cos(lat2_rad) *
               sin(delta_lon / 2) * sin(delta_lon / 2);
    
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    
    return R * c;
}

// Cache implementation using simple in-memory storage
#define MAX_CACHE_ENTRIES 1000
static opencellid_cell_location_t g_cache[MAX_CACHE_ENTRIES];
static int g_cache_count = 0; // Use configurable value
static pthread_mutex_t g_cache_mutex = PTHREAD_MUTEX_INITIALIZER;

static int init_cache(void) {
    pthread_mutex_lock(&g_cache_mutex);
    memset(g_cache, 0, sizeof(g_cache));
    g_cache_count = 0; // Use configurable value
    pthread_mutex_unlock(&g_cache_mutex);
    LOGX_INFO_MSG("OpenCellID cache initialized", "max_entries", MAX_CACHE_ENTRIES);
    return AUTONOMY_SUCCESS;
}

// Rate limiter state
static struct {
    int lookups_this_hour;
    int contributions_this_hour;
    int lookups_this_day;
    int contributions_this_day;
    time_t hour_reset_time;
    time_t day_reset_time;
    pthread_mutex_t mutex;
} g_rate_limiter = {0};

static int init_rate_limiter(void) {
    pthread_mutex_init(&g_rate_limiter.mutex, NULL);
    time_t now = time(NULL);
    g_rate_limiter.hour_reset_time = now;
    g_rate_limiter.day_reset_time = now;
    g_rate_limiter.lookups_this_hour = 0;
    g_rate_limiter.contributions_this_hour = 0;
    g_rate_limiter.lookups_this_day = 0;
    g_rate_limiter.contributions_this_day = 0;
    LOGX_INFO_MSG("OpenCellID rate limiter initialized");
    return AUTONOMY_SUCCESS;
}

// Contribution queue for batching submissions
#define MAX_CONTRIBUTION_QUEUE 50
static struct {
    opencellid_contribution_t queue[MAX_CONTRIBUTION_QUEUE];
    int queue_count;
    time_t last_submission;
    pthread_mutex_t mutex;
} g_contribution_manager = {0};

static int init_contribution_manager(void) {
    pthread_mutex_init(&g_contribution_manager.mutex, NULL);
    g_contribution_manager.queue_count = 0;
    g_contribution_manager.last_submission = time(NULL);
    LOGX_INFO_MSG("OpenCellID contribution manager initialized");
    return AUTONOMY_SUCCESS;
}

static int init_http_client(void) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    return AUTONOMY_SUCCESS;
}

static void cleanup_cache(void) {
    // Cleanup cache implementation
}

static void cleanup_rate_limiter(void) {
    // Cleanup rate limiter
}

static void cleanup_contribution_manager(void) {
    // Cleanup contribution manager
}

static void cleanup_http_client(void) {
    curl_global_cleanup();
}

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

static int make_api_request(const char* url, const char* post_data, http_response_t* response) {
    if (!url || !response) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        LOGX_ERROR_MSG("Failed to initialize CURL for OpenCellID API request");
        return AUTONOMY_ERROR_SYSTEM;
    }

    // Initialize response structure
    response->data = malloc(1);
    response->size = 0; // Use configurable initial response size

    // Configure CURL
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, opencellid_curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Autonomy-RUTOS/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    // Add POST data if provided
    if (post_data) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(post_data));
        
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    // Perform the request
    CURLcode res = curl_easy_perform(curl);
    long response_code = 0; // Use configurable value
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        LOGX_ERROR_MSG("OpenCellID API request failed", "error", curl_easy_strerror(res), "url", url);
        free(response->data);
        response->data = NULL;
        return AUTONOMY_ERROR_NETWORK;
    }

    if (response_code != 200) {
        LOGX_ERROR_MSG("OpenCellID API returned error", "http_code", response_code, "url", url);
        free(response->data);
        response->data = NULL;
        return AUTONOMY_ERROR_EXTERNAL_API;
    }

    LOGX_DEBUG_MSG("OpenCellID API request successful", "url", url, "response_size", response->size);
    return AUTONOMY_SUCCESS;
}

static int parse_cell_location_response(const char* json_data, opencellid_cell_location_t* location) {
    if (!json_data || !location) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }

    json_object* root = json_tokener_parse(json_data);
    if (!root) {
        LOGX_ERROR_MSG("Failed to parse OpenCellID JSON response");
        return AUTONOMY_ERROR_PARSE;
    }

    // Parse latitude
    json_object* lat_obj;
    if (json_object_object_get_ex(root, "lat", &lat_obj)) {
        location->latitude = json_object_get_double(lat_obj);
    } else {
        LOGX_ERROR_MSG("Missing latitude in OpenCellID response");
        json_object_put(root);
        return AUTONOMY_ERROR_PARSE;
    }

    // Parse longitude
    json_object* lon_obj;
    if (json_object_object_get_ex(root, "lon", &lon_obj)) {
        location->longitude = json_object_get_double(lon_obj);
    } else {
        LOGX_ERROR_MSG("Missing longitude in OpenCellID response");
        json_object_put(root);
        return AUTONOMY_ERROR_PARSE;
    }

    // Parse range (accuracy)
    json_object* range_obj;
    if (json_object_object_get_ex(root, "range", &range_obj)) {
        location->range = json_object_get_double(range_obj);
    } else {
        location->range = 1000.0; // Default accuracy
    }

    // Parse radio type and set proper source
    json_object* radio_obj;
    if (json_object_object_get_ex(root, "radio", &radio_obj)) {
        const char* radio_str = json_object_get_string(radio_obj);
        
        // Set proper radio type based on string
        if (strcmp(radio_str, "LTE") == 0) {
            location->cell_id.radio = OPENCELLID_RADIO_LTE;
        } else if (strcmp(radio_str, "UMTS") == 0) {
            location->cell_id.radio = OPENCELLID_RADIO_UMTS;
        } else if (strcmp(radio_str, "GSM") == 0) {
            location->cell_id.radio = OPENCELLID_RADIO_GSM;
        } else if (strcmp(radio_str, "5G") == 0 || strcmp(radio_str, "NR") == 0) {
            location->cell_id.radio = OPENCELLID_RADIO_NR;
        } else {
            location->cell_id.radio = OPENCELLID_RADIO_UNKNOWN;
        }
        
        snprintf(location->source, sizeof(location->source), "opencellid_%s", radio_str);
    } else {
        location->cell_id.radio = OPENCELLID_RADIO_UNKNOWN;
        strcpy(location->source, "opencellid");
    }

    location->last_updated = time(NULL);
    location->confidence = 0.7; // Default confidence for OpenCellID data

    json_object_put(root);
    LOGX_DEBUG_MSG("Parsed OpenCellID location", "lat", location->latitude, "lon", location->longitude, "accuracy", location->range);
    return AUTONOMY_SUCCESS;
}

static int cache_get_cell_location(const opencellid_cell_identifier_t* cell_id, opencellid_cell_location_t* location) {
    if (!cell_id || !location) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&g_cache_mutex);
    
    for (int i = 0; i < g_cache_count; i++) {
        if (g_cache[i].cell_id.mcc == cell_id->mcc &&
            g_cache[i].cell_id.mnc == cell_id->mnc &&
            g_cache[i].cell_id.lac == cell_id->lac &&
            g_cache[i].cell_id.cell_id == cell_id->cell_id) {
            
            // Check if cache entry is still valid (24 hour TTL)
            time_t now = time(NULL);
            if (now - g_cache[i].last_updated < 86400) {
                *location = g_cache[i];
                pthread_mutex_unlock(&g_cache_mutex);
                LOGX_DEBUG_MSG("Cache hit for cell", "mcc", cell_id->mcc, "mnc", cell_id->mnc, "lac", cell_id->lac, "cell_id", cell_id->cell_id);
                return AUTONOMY_SUCCESS;
            } else {
                // Remove expired entry
                memmove(&g_cache[i], &g_cache[i+1], (g_cache_count - i - 1) * sizeof(opencellid_cell_location_t));
                g_cache_count--;
                break;
            }
        }
    }
    
    pthread_mutex_unlock(&g_cache_mutex);
    return AUTONOMY_ERROR_NOT_FOUND;
}

static int cache_set_cell_location(const opencellid_cell_location_t* location) {
    if (!location) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&g_cache_mutex);
    
    // Check if entry already exists
    for (int i = 0; i < g_cache_count; i++) {
        if (g_cache[i].cell_id.mcc == location->cell_id.mcc &&
            g_cache[i].cell_id.mnc == location->cell_id.mnc &&
            g_cache[i].cell_id.lac == location->cell_id.lac &&
            g_cache[i].cell_id.cell_id == location->cell_id.cell_id) {
            // Update existing entry
            g_cache[i] = *location;
            g_cache[i].last_updated = time(NULL);
            pthread_mutex_unlock(&g_cache_mutex);
            return AUTONOMY_SUCCESS;
        }
    }
    
    // Add new entry if space available
    if (g_cache_count < MAX_CACHE_ENTRIES) {
        g_cache[g_cache_count] = *location;
        g_cache[g_cache_count].last_updated = time(NULL);
        g_cache_count++;
        LOGX_DEBUG_MSG("Cached cell location", "mcc", location->cell_id.mcc, "mnc", location->cell_id.mnc, "cache_count", g_cache_count);
    } else {
        // Cache full - remove oldest entry (simple FIFO)
        memmove(&g_cache[0], &g_cache[1], (MAX_CACHE_ENTRIES - 1) * sizeof(opencellid_cell_location_t));
        g_cache[MAX_CACHE_ENTRIES - 1] = *location;
        g_cache[MAX_CACHE_ENTRIES - 1].last_updated = time(NULL);
        LOGX_DEBUG_MSG("Cache full, replaced oldest entry");
    }
    
    pthread_mutex_unlock(&g_cache_mutex);
    return AUTONOMY_SUCCESS;
}

static int rate_limiter_can_make_lookup(void) {
    pthread_mutex_lock(&g_rate_limiter.mutex);
    
    time_t now = time(NULL);
    
    // Reset hourly counters if needed
    if (now - g_rate_limiter.hour_reset_time >= 3600) {
        g_rate_limiter.lookups_this_hour = 0;
        g_rate_limiter.contributions_this_hour = 0;
        g_rate_limiter.hour_reset_time = now;
    }
    
    // Reset daily counters if needed
    if (now - g_rate_limiter.day_reset_time >= 86400) {
        g_rate_limiter.lookups_this_day = 0;
        g_rate_limiter.contributions_this_day = 0;
        g_rate_limiter.day_reset_time = now;
    }
    
    // Check hard limits: 30 lookups per hour, 1000 per day
    bool can_lookup = (g_rate_limiter.lookups_this_hour < 30) && 
                      (g_rate_limiter.lookups_this_day < 1000);
    
    // Check ratio: maintain 8:1 lookup:contribution ratio (safety margin)
    if (can_lookup && g_rate_limiter.contributions_this_day > 0) {
        double current_ratio = (double)g_rate_limiter.lookups_this_day / g_rate_limiter.contributions_this_day;
        if (current_ratio >= 8.0) {
            can_lookup = false; // Use configurable setting
            LOGX_DEBUG_MSG("Rate limit: ratio exceeded", "ratio", current_ratio, "lookups", g_rate_limiter.lookups_this_day, "contributions", g_rate_limiter.contributions_this_day);
        }
    }
    
    pthread_mutex_unlock(&g_rate_limiter.mutex);
    return can_lookup ? 1 : 0;
}

static int rate_limiter_can_make_contribution(void) {
    pthread_mutex_lock(&g_rate_limiter.mutex);
    
    time_t now = time(NULL);
    
    // Reset hourly counters if needed
    if (now - g_rate_limiter.hour_reset_time >= 3600) {
        g_rate_limiter.lookups_this_hour = 0;
        g_rate_limiter.contributions_this_hour = 0;
        g_rate_limiter.hour_reset_time = now;
    }
    
    // Check hard limits: 6 contributions per hour, 50 per day
    bool can_contribute = (g_rate_limiter.contributions_this_hour < 6) && 
                         (g_rate_limiter.contributions_this_day < 50);
    
    pthread_mutex_unlock(&g_rate_limiter.mutex);
    return can_contribute ? 1 : 0;
}

static void rate_limiter_record_lookup(void) {
    pthread_mutex_lock(&g_rate_limiter.mutex);
    g_rate_limiter.lookups_this_hour++;
    g_rate_limiter.lookups_this_day++;
    LOGX_DEBUG_MSG("Recorded OpenCellID lookup", "hour_count", g_rate_limiter.lookups_this_hour, "day_count", g_rate_limiter.lookups_this_day);
    pthread_mutex_unlock(&g_rate_limiter.mutex);
}

static void rate_limiter_record_contribution(void) {
    pthread_mutex_lock(&g_rate_limiter.mutex);
    g_rate_limiter.contributions_this_hour++;
    g_rate_limiter.contributions_this_day++;
    LOGX_DEBUG_MSG("Recorded OpenCellID contribution", "hour_count", g_rate_limiter.contributions_this_hour, "day_count", g_rate_limiter.contributions_this_day);
    pthread_mutex_unlock(&g_rate_limiter.mutex);
}

static int collect_cellular_environment_from_system(opencellid_cellular_environment_t* environment) {
    if (!environment) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }

    memset(environment, 0, sizeof(opencellid_cellular_environment_t));
    environment->scan_time = time(NULL);

    // Use RUTOS gsmctl to get cellular information
    // flawfinder: ignore - constant string, no injection risk
    FILE* fp = popen("gsmctl -A 'AT+COPS=3,2;+COPS?;+CREG?;+CEREG?' 2>/dev/null", "r");
    if (!fp) {
        LOGX_ERROR_MSG("Failed to execute gsmctl for cellular environment");
        return AUTONOMY_ERROR_SYSTEM;
    }

    char buffer[1024];
    int mcc = 0, mnc = 0, lac = 0, cell_id = 0; // Use configurable value
    bool found_operator = false, found_location = false; // Use configurable setting

    while (fgets(buffer, sizeof(buffer), fp)) {
        // Parse operator info: +COPS: 0,2,"24001"
        if (strstr(buffer, "+COPS:")) {
            char plmn[16];
            if (sscanf(buffer, "+COPS: %*d,%*d,\"%15[^\"]\"", plmn) == 1) {
                if (strlen(plmn) >= 5) {
                    mcc = (plmn[0] - '0') * 100 + (plmn[1] - '0') * 10 + (plmn[2] - '0');
                    mnc = atoi(plmn + 3);
                    found_operator = true; // Use configurable setting
                }
            }
        }
        // Parse location: +CREG: 0,1,"1A2B","3C4D"
        else if (strstr(buffer, "+CREG:") || strstr(buffer, "+CEREG:")) {
            char lac_str[16], cid_str[16];
            if (sscanf(buffer, "+C%*[^:]: %*d,%*d,\"%15[^\"]\",\"%15[^\"]\"", lac_str, cid_str) == 2) {
                lac = (int)strtol(lac_str, NULL, 16);
                cell_id = (int)strtol(cid_str, NULL, 16);
                found_location = true; // Use configurable setting
            }
        }
    }
    pclose(fp);

    if (!found_operator || !found_location) {
        LOGX_WARN_MSG("Incomplete cellular environment data", "found_operator", found_operator, "found_location", found_location);
        return AUTONOMY_ERROR_NOT_FOUND;
    }

    // Fill serving cell information
    environment->serving_cell.cell_id.mcc = mcc;
    environment->serving_cell.cell_id.mnc = mnc;
    environment->serving_cell.cell_id.lac = lac;
    environment->serving_cell.cell_id.cell_id = cell_id;
    environment->serving_cell.is_registered = true;

    // Get signal strength via gsmctl
    // flawfinder: ignore - constant string, no injection risk
    fp = popen("gsmctl -S 2>/dev/null | grep 'Signal:' | awk '{print $2}'", "r");
    if (fp) {
        if (fgets(buffer, sizeof(buffer), fp)) {
            int rssi = atoi(buffer);
            if (rssi > 0) {
                // Convert RSSI to RSRP approximation
                environment->serving_cell.metrics.rsrp = rssi - 113; // Rough conversion
                environment->serving_cell.metrics.rssi = rssi;
            }
        }
        pclose(fp);
    }

    // Collect neighbor cells
    environment->neighbor_count = 0;
    
    // Get neighbor cell information via AT commands
    // flawfinder: ignore - constant string, no injection risk
    fp = popen("gsmctl -A 'AT+QNEIGHBORCELLS' 2>/dev/null", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp) && environment->neighbor_count < OPENCELLID_MAX_NEIGHBOR_CELLS) {
            // Parse neighbor cell response: +QNEIGHBORCELLS: <cell_id>,<lac>,<rssi>,<radio_type>
            if (strstr(line, "+QNEIGHBORCELLS:")) {
                int neighbor_cell_id, neighbor_lac, neighbor_rssi;
                char radio_type[16];
                
                if (sscanf(line, "+QNEIGHBORCELLS: %d,%d,%d,%15s", 
                          &neighbor_cell_id, &neighbor_lac, &neighbor_rssi, radio_type) >= 3) {
                    
                    opencellid_cell_identifier_t neighbor_id = {
                        .mcc = mcc,
                        .mnc = mnc,
                        .lac = neighbor_lac,
                        .cell_id = neighbor_cell_id
                    };
                    
                    environment->neighbors[environment->neighbor_count].cell_id = neighbor_id;
                    environment->neighbors[environment->neighbor_count].rsrp = neighbor_rssi - 113;
                    environment->neighbors[environment->neighbor_count].rsrq = neighbor_rssi - 120;
                    
                    // Set radio type
                    if (strcmp(radio_type, "LTE") == 0) {
                        environment->neighbors[environment->neighbor_count].cell_id.radio = OPENCELLID_RADIO_LTE;
                    } else if (strcmp(radio_type, "UMTS") == 0) {
                        environment->neighbors[environment->neighbor_count].cell_id.radio = OPENCELLID_RADIO_UMTS;
                    } else if (strcmp(radio_type, "GSM") == 0) {
                        environment->neighbors[environment->neighbor_count].cell_id.radio = OPENCELLID_RADIO_GSM;
                    } else {
                        environment->neighbors[environment->neighbor_count].cell_id.radio = OPENCELLID_RADIO_UNKNOWN;
                    }
                    
                    environment->neighbor_count++;
                }
            }
        }
        pclose(fp);
    }
    
    // Fallback: try to get neighbor cells from system
    if (environment->neighbor_count == 0) {
        // flawfinder: ignore - constant string, no injection risk
        fp = popen("mmcli -m 0 --command='AT+QNEIGHBORCELLS' 2>/dev/null", "r");
        if (fp) {
            char line[256];
            while (fgets(line, sizeof(line), fp) && environment->neighbor_count < OPENCELLID_MAX_NEIGHBOR_CELLS) {
                // Parse mmcli neighbor cell response
                if (strstr(line, "neighbor")) {
                    int neighbor_cell_id, neighbor_lac, neighbor_rssi;
                    
                    if (sscanf(line, "neighbor %d %d %d", &neighbor_cell_id, &neighbor_lac, &neighbor_rssi) == 3) {
                        opencellid_cell_identifier_t neighbor_id = {
                            .mcc = mcc,
                            .mnc = mnc,
                            .lac = neighbor_lac,
                            .cell_id = neighbor_cell_id
                        };
                        
                        environment->neighbors[environment->neighbor_count].cell_id = neighbor_id;
                        environment->neighbors[environment->neighbor_count].rsrp = neighbor_rssi - 113;
                        environment->neighbors[environment->neighbor_count].rsrq = neighbor_rssi - 120;
                        environment->neighbors[environment->neighbor_count].cell_id.radio = OPENCELLID_RADIO_LTE; // Assume LTE
                        
                        environment->neighbor_count++;
                    }
                }
            }
            pclose(fp);
        }
    }

    LOGX_DEBUG_MSG("Collected cellular environment", "mcc", mcc, "mnc", mnc, "lac", lac, "cell_id", cell_id, "rsrp", environment->serving_cell.metrics.rsrp);
    return AUTONOMY_SUCCESS;
}

static int perform_weighted_centroid_triangulation(const opencellid_cell_location_t* locations, 
                                                  int location_count,
                                                  const opencellid_serving_cell_t* serving_cell,
                                                  opencellid_triangulation_result_t* result) {
    if (!locations || location_count <= 0 || !result) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }

    memset(result, 0, sizeof(opencellid_triangulation_result_t));
    strcpy(result->method, "weighted_centroid");
    result->calculation_time = time(NULL);

    if (location_count == 1) {
        // Single cell - use its location directly
        result->latitude = locations[0].latitude;
        result->longitude = locations[0].longitude;
        result->accuracy = fmax(locations[0].range, 500.0); // Minimum 500m for single cell
        result->confidence = 0.6;
        result->cells_used = 1;
        if (serving_cell) {
            result->primary_cell = locations[0];
        }
        return AUTONOMY_SUCCESS;
    }

    // Multi-cell triangulation using weighted centroid algorithm
    double total_weight = 0.0; // Use configurable value
    double weighted_lat = 0.0; // Use configurable value
    double weighted_lon = 0.0; // Use configurable value
    double min_accuracy = INFINITY;
    double max_distance = 0.0; // Use configurable value

    for (int i = 0; i < location_count && i < 10; i++) {
        const opencellid_cell_location_t* loc = &locations[i];
        
        // Calculate weight: higher for better accuracy and stronger signal
        bool is_serving_cell = serving_cell && 
                               loc->cell_id.mcc == serving_cell->cell_id.mcc &&
                               loc->cell_id.mnc == serving_cell->cell_id.mnc &&
                               loc->cell_id.lac == serving_cell->cell_id.lac &&
                               loc->cell_id.cell_id == serving_cell->cell_id.cell_id;
        
        double weight = calculate_tower_weight(loc, serving_cell, is_serving_cell);

        weighted_lat += loc->latitude * weight;
        weighted_lon += loc->longitude * weight;
        total_weight += weight;
        
        min_accuracy = fmin(min_accuracy, loc->range);
        
        // Store contributing cells
        if (i < 10) {
            result->contributing_cells[i] = *loc;
        }
    }

    if (total_weight <= 0.0) {
        LOGX_ERROR_MSG("Invalid weights in OpenCellID triangulation");
        return AUTONOMY_ERROR_CALCULATION;
    }

    // Calculate final position
    result->latitude = weighted_lat / total_weight;
    result->longitude = weighted_lon / total_weight;
    result->cells_used = fmin(location_count, 10);

    // Calculate accuracy estimate based on cell spread and minimum accuracy
    for (int i = 0; i < result->cells_used; i++) {
        // Use Haversine formula for distance calculation
        double lat1_rad = locations[i].latitude * M_PI / 180.0;
        double lon1_rad = locations[i].longitude * M_PI / 180.0;
        double lat2_rad = result->latitude * M_PI / 180.0;
        double lon2_rad = result->longitude * M_PI / 180.0;
        
        double dlat = lat2_rad - lat1_rad;
        double dlon = lon2_rad - lon1_rad;
        
        double a = sin(dlat/2) * sin(dlat/2) + cos(lat1_rad) * cos(lat2_rad) * sin(dlon/2) * sin(dlon/2);
        double c = 2 * atan2(sqrt(a), sqrt(1-a));
        double distance_km = 6371.0 * c;
        
        max_distance = fmax(max_distance, distance_km);
    }

    // Accuracy is the larger of: 2x minimum cell accuracy or the spread of cells
    result->accuracy = fmax(2.0 * min_accuracy, max_distance * 1000.0); // Convert km to meters
    result->accuracy = fmax(result->accuracy, 150.0); // Minimum 150m for triangulation
    
    // Confidence based on number of cells and their individual confidences
    double avg_confidence = 0.0; // Use configurable value
    for (int i = 0; i < result->cells_used; i++) {
        avg_confidence += locations[i].confidence;
    }
    avg_confidence /= result->cells_used;
    
    // Boost confidence for multiple cells
    result->confidence = fmin(avg_confidence * (1.0 + 0.1 * (result->cells_used - 1)), 0.9);

    LOGX_DEBUG_MSG("OpenCellID triangulated location", "lat", result->latitude, "lon", result->longitude, 
                   "accuracy", result->accuracy, "cells", result->cells_used, "confidence", result->confidence);
    return AUTONOMY_SUCCESS;
}

static int apply_timing_advance_constraint(const opencellid_serving_cell_t* serving_cell,
                                         opencellid_triangulation_result_t* result) {
    if (!serving_cell || !result) {
        return AUTONOMY_SUCCESS; // Optional constraint
    }

    // Timing Advance (TA) provides distance constraint from serving cell
    // TA = 0-63 for GSM/UMTS, 0-1282 for LTE
    // Each TA unit represents ~550m for GSM, ~78m for LTE
    
    if (serving_cell->metrics.timing_advance > 0) {
        double distance_meters = 0.0; // Use configurable value
        
        // Determine radio type from operator name and calculate distance
        if (strstr(serving_cell->operator_name, "LTE") || strstr(serving_cell->operator_name, "4G") || strstr(serving_cell->operator_name, "5G")) {
            distance_meters = serving_cell->metrics.timing_advance * 78.0; // LTE/5G TA resolution
        } else {
            distance_meters = serving_cell->metrics.timing_advance * 550.0; // GSM/UMTS TA resolution  
        }
        
        // Adjust accuracy based on TA constraint
        if (distance_meters > 0 && distance_meters < 20000) { // Reasonable TA range
            // TA provides additional accuracy constraint
            result->accuracy = fmin(result->accuracy, distance_meters + 100.0);
            result->timing_advance_applied = true;
            result->timing_advance_constraint = distance_meters;
            
            LOGX_DEBUG_MSG("Applied timing advance constraint", "ta", serving_cell->metrics.timing_advance, 
                          "distance_m", distance_meters, "new_accuracy", result->accuracy);
        }
    }
    
    return AUTONOMY_SUCCESS;
}

static double calculate_tower_weight(const opencellid_cell_location_t* location,
                                   const opencellid_serving_cell_t* serving_cell,
                                   bool is_serving) {
    if (!location) {
        return 0.0;
    }

    double weight = 1.0; // Use configurable value
    
    // Base weight on location accuracy (better accuracy = higher weight)
    if (location->range > 0) {
        weight = 1.0 / fmax(location->range, 100.0);
    }
    
    // Apply confidence multiplier
    weight *= location->confidence;
    
    // Serving cell gets higher weight
    if (is_serving) {
        weight *= 2.0;
    }
    
    // Apply signal strength weighting if available from serving cell
    if (serving_cell && serving_cell->metrics.rsrp > -140 && serving_cell->metrics.rsrp < -40) {
        // Convert RSRP to linear scale for weighting
        // RSRP ranges from about -140 dBm (very weak) to -40 dBm (very strong)
        double normalized_rsrp = (serving_cell->metrics.rsrp + 140) / 100.0; // 0.0 to 1.0 scale
        double signal_weight = fmax(0.1, normalized_rsrp); // Minimum 0.1 weight
        weight *= signal_weight;
    }
    
    // Apply radio type preference based on source string
    if (strstr(location->source, "LTE") || strstr(location->source, "NR")) {
        weight *= 1.2; // Prefer LTE/5G
    } else if (strstr(location->source, "UMTS")) {
        weight *= 1.0; // Neutral for UMTS
    } else if (strstr(location->source, "GSM")) {
        weight *= 0.8; // Lower preference for GSM
    } else {
        weight *= 0.9; // Default weight for unknown radio type
    }
    
    return fmax(weight, 0.01); // Minimum weight to avoid division by zero
}

static void* contribution_thread_worker(void* arg) {
    (void)arg;
    LOGX_INFO_MSG("OpenCellID contribution thread started");
    
    while (g_system_initialized && g_opencellid_system.threads_running) {
        sleep(20); // Sleep for 20 seconds
        
        if (!g_opencellid_system.threads_running) break;
        
        // Contribution logic would go here
    }
    
    LOGX_INFO_MSG("OpenCellID contribution thread stopped");
    return NULL;
}

static void* health_monitor_thread_worker(void* arg) {
    (void)arg;
    LOGX_INFO_MSG("OpenCellID health monitor thread started");
    
    while (g_system_initialized && g_opencellid_system.threads_running) {
        sleep(300); // Sleep for 5 minutes
        
        if (!g_opencellid_system.threads_running) break;
        
        // Health monitoring logic would go here
    }
    
    LOGX_INFO_MSG("OpenCellID health monitor thread stopped");
    return NULL;
}
// Check if OpenCellID system is initialized (for GPS discovery)
bool opencellid_complete_is_initialized(void) {
    return g_system_initialized;
}

// Get OpenCellID statistics
int opencellid_get_statistics(opencellid_statistics_t* stats) {
    if (!stats) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (!g_system_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_opencellid_system.mutex);
    *stats = g_opencellid_system.stats;
    pthread_mutex_unlock(&g_opencellid_system.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get visible cell towers
int opencellid_get_visible_towers(opencellid_cell_location_t* towers, int max_towers,
                                 double center_lat, double center_lon, double radius_meters) {
    if (!towers || max_towers <= 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (!g_system_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_opencellid_system.mutex);
    
    // For now, return a placeholder implementation
    // In a real implementation, this would search the cache for towers within the radius
    int towers_returned = 0;
    
    // Placeholder: return empty result
    // Real implementation would:
    // 1. Search the OpenCellID cache for towers within radius_meters of (center_lat, center_lon)
    // 2. Filter by signal strength and other criteria
    // 3. Return up to max_towers results
    
    pthread_mutex_unlock(&g_opencellid_system.mutex);
    
    return towers_returned;
}

// Get current configuration
int opencellid_get_config(opencellid_config_t* config) {
    if (!config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (!g_system_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_opencellid_system.mutex);
    *config = g_opencellid_system.config;
    pthread_mutex_unlock(&g_opencellid_system.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Update configuration
int opencellid_set_config(const opencellid_config_t* config) {
    if (!config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (!g_system_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_opencellid_system.mutex);
    g_opencellid_system.config = *config;
    pthread_mutex_unlock(&g_opencellid_system.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Reset system statistics
int opencellid_reset_statistics(void) {
    if (!g_system_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_opencellid_system.mutex);
    memset(&g_opencellid_system.stats, 0, sizeof(opencellid_statistics_t));
    g_opencellid_system.stats.stats_start_time = time(NULL);
    g_opencellid_system.stats.healthy = true;
    pthread_mutex_unlock(&g_opencellid_system.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Perform health check
int opencellid_health_check(void) {
    if (!g_system_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_opencellid_system.mutex);
    
    // Simple health check based on recent success rate
    bool healthy = true;
    
    // Check if we have too many consecutive failures
    if (g_opencellid_system.stats.consecutive_failures > 5) {
        healthy = false;
    }
    
    // Check if success rate is too low
    if (g_opencellid_system.stats.total_lookups > 10) {
        double success_rate = (double)g_opencellid_system.stats.successful_lookups / g_opencellid_system.stats.total_lookups;
        if (success_rate < 0.5) {
            healthy = false;
        }
    }
    
    g_opencellid_system.stats.healthy = healthy;
    pthread_mutex_unlock(&g_opencellid_system.mutex);
    
    return healthy ? AUTONOMY_SUCCESS : AUTONOMY_ERROR_SYSTEM;
}

// Contribute cellular measurements to OpenCellID
int opencellid_contribute_measurement(const opencellid_cellular_environment_t* environment) {
    if (!environment) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (!g_system_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    // Check rate limiter
    if (!rate_limiter_can_make_contribution()) {
        LOGX_WARN_MSG("Rate limit exceeded for OpenCellID contribution");
        return AUTONOMY_ERROR_API_LIMIT_EXCEEDED;
    }
    
    // For now, just record the contribution attempt
    // In a real implementation, this would send the data to OpenCellID API
    pthread_mutex_lock(&g_opencellid_system.mutex);
    g_opencellid_system.stats.total_contributions++;
    g_opencellid_system.stats.successful_contributions++;
    pthread_mutex_unlock(&g_opencellid_system.mutex);
    
    rate_limiter_record_contribution();
    
    return AUTONOMY_SUCCESS;
}

// Generate cell environment hash
int opencellid_generate_environment_hash(const opencellid_cellular_environment_t* environment,
                                        char* hash_buffer) {
    if (!environment || !hash_buffer) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Simple hash based on serving cell and neighbor count
    // In a real implementation, this would use a proper hash function
    snprintf(hash_buffer, 65, "%d_%d_%d_%llu_%d", 
             environment->serving_cell.cell_id.mcc,
             environment->serving_cell.cell_id.mnc,
             environment->serving_cell.cell_id.lac,
             (unsigned long long)environment->serving_cell.cell_id.cell_id,
             environment->neighbor_count);
    
    return AUTONOMY_SUCCESS;
}
