#include "gps_opencellid_enhanced.h"
#include "gps_opencellid.h"
#include "gps_google_api.h"
#include "gps_unwiredlabs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

// Enhanced OpenCellID system state
static enhanced_opencellid_config_t g_enhanced_config = {0};
static enhanced_opencellid_stats_t g_enhanced_stats = {0};
static bool g_enhanced_initialized = false;
static pthread_mutex_t g_enhanced_mutex = PTHREAD_MUTEX_INITIALIZER;

// Service configurations
static google_api_config_t g_google_config = {0};
static unwiredlabs_config_t g_unwiredlabs_config = {0};
static intelligent_cache_config_t g_cache_config = {0};

// Initialize enhanced OpenCellID system
int gps_opencellid_enhanced_init(const enhanced_opencellid_config_t* config) {
    if (!config) {
        return -1;
    }
    
    pthread_mutex_lock(&g_enhanced_mutex);
    
    // Copy configuration
    memcpy(&g_enhanced_config, config, sizeof(enhanced_opencellid_config_t));
    
    // Initialize statistics
    memset(&g_enhanced_stats, 0, sizeof(enhanced_opencellid_stats_t));
    
    // Initialize intelligent cache
    g_cache_config.max_cache_age = 3600;        // 1 hour
    g_cache_config.debounce_delay = 10;         // 10 seconds
    g_cache_config.tower_change_threshold = 0.35; // 35%
    g_cache_config.top_towers_count = 5;        // Top 5 towers
    
    if (intelligent_cache_init(&g_cache_config) != 0) {
        pthread_mutex_unlock(&g_enhanced_mutex);
        return -1;
    }
    
    // Initialize Google API if configured
    if (config->google_api_enabled && strlen(config->google_api_key) > 0) {
        g_google_config.timeout_seconds = 30;
        g_google_config.max_retries = 3;
        g_google_config.enable_wifi = true;
        g_google_config.enable_cellular = true;
        g_google_config.enable_ip_fallback = true;
        g_google_config.min_accuracy_threshold = 1000.0; // 1km
        strncpy(g_google_config.api_key, config->google_api_key, sizeof(g_google_config.api_key) - 1);
        
        if (google_api_init(&g_google_config) != 0) {
            pthread_mutex_unlock(&g_enhanced_mutex);
            return -1;
        }
    }
    
    // Initialize UnwiredLabs API if configured
    if (config->unwiredlabs_enabled && strlen(config->unwiredlabs_token) > 0) {
        g_unwiredlabs_config.timeout_seconds = 30;
        g_unwiredlabs_config.max_retries = 3;
        g_unwiredlabs_config.enable_wifi = true;
        g_unwiredlabs_config.enable_cellular = true;
        g_unwiredlabs_config.enable_fallbacks = true;
        g_unwiredlabs_config.enable_address_lookup = true;
        g_unwiredlabs_config.min_accuracy_threshold = 1000.0; // 1km
        strncpy(g_unwiredlabs_config.token, config->unwiredlabs_token, sizeof(g_unwiredlabs_config.token) - 1);
        
        if (unwiredlabs_api_init(&g_unwiredlabs_config) != 0) {
            pthread_mutex_unlock(&g_enhanced_mutex);
            return -1;
        }
    }
    
    g_enhanced_initialized = true;
    pthread_mutex_unlock(&g_enhanced_mutex);
    return 0;
}

// Cleanup enhanced OpenCellID system
void gps_opencellid_enhanced_cleanup(void) {
    if (!g_enhanced_initialized) {
        return;
    }
    
    pthread_mutex_lock(&g_enhanced_mutex);
    
    // Cleanup intelligent cache
    intelligent_cache_cleanup();
    
    // Cleanup Google API
    if (google_api_is_initialized()) {
        google_api_cleanup();
    }
    
    // Cleanup UnwiredLabs API
    if (unwiredlabs_api_is_initialized()) {
        unwiredlabs_api_cleanup();
    }
    
    g_enhanced_initialized = false;
    pthread_mutex_unlock(&g_enhanced_mutex);
}

// Enhanced lookup with multiple database support
int gps_opencellid_enhanced_lookup(const opencellid_cell_key_t* cell_key, opencellid_response_t* response) {
    if (!g_enhanced_initialized || !cell_key || !response) {
        return -1;
    }
    
    pthread_mutex_lock(&g_enhanced_mutex);
    
    // Initialize response
    memset(response, 0, sizeof(opencellid_response_t));
    
    // Try services in order of preference
    int result = -1;
    
    // 1. Try Mozilla Location Service (free, no key required)
    if (g_enhanced_config.mozilla_enabled) {
        result = gps_opencellid_mozilla_lookup(cell_key, response);
        if (result == 0 && response->success) {
            g_enhanced_stats.mozilla_requests++;
            g_enhanced_stats.mozilla_successes++;
            pthread_mutex_unlock(&g_enhanced_mutex);
            return 0;
        }
        g_enhanced_stats.mozilla_requests++;
    }
    
    // 2. Try OpenCellID (free with key)
    if (g_enhanced_config.opencellid_enabled) {
        result = gps_opencellid_lookup(cell_key, response);
        if (result == 0 && response->success) {
            g_enhanced_stats.opencellid_requests++;
            g_enhanced_stats.opencellid_successes++;
            pthread_mutex_unlock(&g_enhanced_mutex);
            return 0;
        }
        g_enhanced_stats.opencellid_requests++;
    }
    
    // 3. Try UnwiredLabs (alternative service)
    if (g_enhanced_config.unwiredlabs_enabled && unwiredlabs_api_is_initialized()) {
        unwiredlabs_cell_t unwiredlabs_cell = {0};
        snprintf(unwiredlabs_cell.lac, sizeof(unwiredlabs_cell.lac), "%s", cell_key->lac);
        snprintf(unwiredlabs_cell.cid, sizeof(unwiredlabs_cell.cid), "%s", cell_key->cell_id);
        unwiredlabs_cell.mcc = atoi(cell_key->mcc);
        unwiredlabs_cell.mnc = atoi(cell_key->mnc);
        strncpy(unwiredlabs_cell.radio, "lte", sizeof(unwiredlabs_cell.radio) - 1);
        unwiredlabs_cell.signal = -100; // Default signal strength
        
        unwiredlabs_response_t unwiredlabs_response;
        result = unwiredlabs_api_get_cellular_location(&unwiredlabs_cell, 1, &unwiredlabs_response);
        if (result == 0 && unwiredlabs_response.success) {
            response->success = true;
            response->lat = unwiredlabs_response.lat;
            response->lon = unwiredlabs_response.lon;
            response->accuracy = unwiredlabs_response.accuracy;
            response->mcc = unwiredlabs_cell.mcc;
            response->mnc = unwiredlabs_cell.mnc;
            response->lac = atoi(unwiredlabs_cell.lac);
            response->cell_id = atoi(unwiredlabs_cell.cid);
            
            g_enhanced_stats.unwiredlabs_requests++;
            g_enhanced_stats.unwiredlabs_successes++;
            pthread_mutex_unlock(&g_enhanced_mutex);
            return 0;
        }
        g_enhanced_stats.unwiredlabs_requests++;
    }
    
    // 4. Try Google Geolocation API (paid, highest accuracy)
    if (g_enhanced_config.google_api_enabled && google_api_is_initialized()) {
        google_cell_tower_t google_cell = {0};
        snprintf(google_cell.cell_id, sizeof(google_cell.cell_id), "%s", cell_key->cell_id);
        snprintf(google_cell.location_area_code, sizeof(google_cell.location_area_code), "%s", cell_key->lac);
        snprintf(google_cell.mobile_country_code, sizeof(google_cell.mobile_country_code), "%s", cell_key->mcc);
        snprintf(google_cell.mobile_network_code, sizeof(google_cell.mobile_network_code), "%s", cell_key->mnc);
        google_cell.signal_strength = -100; // Default signal strength
        google_cell.age = 0;
        
        google_location_response_t google_response;
        result = google_api_get_cellular_location(&google_cell, 1, &google_response);
        if (result == 0 && google_response.success) {
            response->success = true;
            response->lat = google_response.lat;
            response->lng = google_response.lng;
            response->accuracy = google_response.accuracy;
            response->mcc = atoi(cell_key->mcc);
            response->mnc = atoi(cell_key->mnc);
            response->lac = atoi(cell_key->lac);
            response->cell_id = atoi(cell_key->cell_id);
            
            g_enhanced_stats.google_requests++;
            g_enhanced_stats.google_successes++;
            pthread_mutex_unlock(&g_enhanced_mutex);
            return 0;
        }
        g_enhanced_stats.google_requests++;
    }
    
    pthread_mutex_unlock(&g_enhanced_mutex);
    return -1;
}

// Enhanced lookup with intelligent caching
int gps_opencellid_enhanced_lookup_with_cache(const opencellid_cell_key_t* cell_key, 
                                             const cell_environment_t* env, 
                                             opencellid_response_t* response) {
    if (!g_enhanced_initialized || !cell_key || !response) {
        return -1;
    }
    
    // Check if we should query new location
    if (env && !intelligent_cache_should_query_new_location(env)) {
        // Use cached location
        if (intelligent_cache_get_cached_location(response)) {
            return 0;
        }
    }
    
    // Query new location
    int result = gps_opencellid_enhanced_lookup(cell_key, response);
    
    // Update cache if successful
    if (result == 0 && env) {
        intelligent_cache_update_location(env, response);
    }
    
    return result;
}

// Get enhanced statistics
int gps_opencellid_enhanced_get_stats(enhanced_opencellid_stats_t* stats) {
    if (!g_enhanced_initialized || !stats) {
        return -1;
    }
    
    pthread_mutex_lock(&g_enhanced_mutex);
    
    // Copy base stats
    memcpy(stats, &g_enhanced_stats, sizeof(enhanced_opencellid_stats_t));
    
    // Add service-specific stats
    if (google_api_is_initialized()) {
        google_api_stats_t google_stats;
        if (google_api_get_stats(&google_stats) == 0) {
            stats->google_total_requests = google_stats.total_requests;
            stats->google_successful_requests = google_stats.successful_requests;
            stats->google_failed_requests = google_stats.failed_requests;
            stats->google_average_accuracy = google_stats.average_accuracy;
        }
    }
    
    if (unwiredlabs_api_is_initialized()) {
        unwiredlabs_stats_t unwiredlabs_stats;
        if (unwiredlabs_api_get_stats(&unwiredlabs_stats) == 0) {
            stats->unwiredlabs_total_requests = unwiredlabs_stats.total_requests;
            stats->unwiredlabs_successful_requests = unwiredlabs_stats.successful_requests;
            stats->unwiredlabs_failed_requests = unwiredlabs_stats.failed_requests;
            stats->unwiredlabs_average_accuracy = unwiredlabs_stats.average_accuracy;
        }
    }
    
    pthread_mutex_unlock(&g_enhanced_mutex);
    return 0;
}

// Perform health check
int gps_opencellid_enhanced_health_check(void) {
    if (!g_enhanced_initialized) {
        return -1;
    }
    
    pthread_mutex_lock(&g_enhanced_mutex);
    
    int health_score = 0;
    int total_services = 0;
    
    // Check OpenCellID
    if (g_enhanced_config.opencellid_enabled) {
        total_services++;
        if (gps_opencellid_is_initialized()) {
            health_score += 25;
        }
    }
    
    // Check Mozilla
    if (g_enhanced_config.mozilla_enabled) {
        total_services++;
        health_score += 25; // Mozilla is always available
    }
    
    // Check Google API
    if (g_enhanced_config.google_api_enabled) {
        total_services++;
        if (google_api_is_initialized() && google_api_validate_key()) {
            health_score += 25;
        }
    }
    
    // Check UnwiredLabs
    if (g_enhanced_config.unwiredlabs_enabled) {
        total_services++;
        if (unwiredlabs_api_is_initialized() && unwiredlabs_api_validate_token()) {
            health_score += 25;
        }
    }
    
    pthread_mutex_unlock(&g_enhanced_mutex);
    
    return (total_services > 0 && health_score > 0) ? 0 : -1;
}

// Check if enhanced OpenCellID is initialized
bool gps_opencellid_enhanced_is_initialized(void) {
    return g_enhanced_initialized;
}

// Mozilla Location Service lookup (placeholder - would need implementation)
int gps_opencellid_mozilla_lookup(const opencellid_cell_key_t* cell_key, opencellid_response_t* response) {
    // This would implement the Mozilla Location Service API
    // For now, return failure to indicate not implemented
    if (response) {
        response->success = false;
    }
    return -1;
}
