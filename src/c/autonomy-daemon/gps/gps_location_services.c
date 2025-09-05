#include "gps_location_services.h"
#include "logx.h"
#include "types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

// Location services configuration
static const int MAX_LOCATION_CACHE = 1000;             // Maximum cached locations
static const int LOCATION_CACHE_TTL = 86400;            // 24 hour cache TTL
static const int MAX_REVERSE_GEOCODE_ATTEMPTS = 3;      // Maximum reverse geocoding attempts
static const double LOCATION_CACHE_RADIUS = 100.0;      // 100m cache radius
static const int MAX_PLACE_NAME_LENGTH = 256;           // Maximum place name length
static const int MAX_ADDRESS_LENGTH = 512;               // Maximum address length

// Location service types
static const char* LOCATION_SERVICE_NAMES[] = {
    "unknown", "nominatim", "google", "here", "custom"
};

// Global location services state
static gps_location_services_t g_location_services = {0};
static bool g_location_services_initialized = false;
static pthread_mutex_t g_location_services_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize GPS location services
static int gps_location_services_init(void) {
    if (g_location_services_initialized) {
        LOGX_WARN("GPS location services already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_location_services_mutex);
    
    // Initialize location services state
    memset(&g_location_services, 0, sizeof(gps_location_services_t));
    g_location_services.enabled = true;
    g_location_services.max_cache = MAX_LOCATION_CACHE;
    g_location_services.cache_ttl = LOCATION_CACHE_TTL;
    g_location_services.max_attempts = MAX_REVERSE_GEOCODE_ATTEMPTS;
    g_location_services.cache_radius = LOCATION_CACHE_RADIUS;
    g_location_services.default_service = LOCATION_SERVICE_NOMINATIM;
    
    g_location_services.cache_hits = 0;
    g_location_services.cache_misses = 0;
    g_location_services.total_requests = 0;
    g_location_services.successful_requests = 0;
    g_location_services.failed_requests = 0;
    g_location_services.last_request = 0;
    
    // Initialize location cache
    for (int i = 0; i < MAX_LOCATION_CACHE; i++) {
        g_location_services.location_cache[i].active = false;
        g_location_services.location_cache[i].timestamp = 0;
        g_location_services.location_cache[i].lat = 0.0;
        g_location_services.location_cache[i].lon = 0.0;
        g_location_services.location_cache[i].place_name[0] = '\0';
        g_location_services.location_cache[i].address[0] = '\0';
        g_location_services.location_cache[i].country[0] = '\0';
        g_location_services.location_cache[i].state[0] = '\0';
        g_location_services.location_cache[i].city[0] = '\0';
        g_location_services.location_cache[i].postal_code[0] = '\0';
        g_location_services.location_cache[i].service_used = LOCATION_SERVICE_UNKNOWN;
    }
    
    g_location_services_initialized = true;
    pthread_mutex_unlock(&g_location_services_mutex);
    
    LOGX_INFO("GPS location services initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Reverse geocode GPS coordinates
static int gps_location_services_reverse_geocode(double lat, double lon, gps_location_info_t *location_info) {
    if (!g_location_services_initialized || !location_info) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_location_services_mutex);
    
    g_location_services.total_requests++;
    
    // Check cache first
    int cache_index = find_location_in_cache(lat, lon);
    if (cache_index >= 0) {
        // Return cached location
        gps_location_cache_entry_t *cache_entry = &g_location_services.location_cache[cache_index];
        
        location_info->lat = cache_entry->lat;
        location_info->lon = cache_entry->lon;
        strncpy(location_info->place_name, cache_entry->place_name, sizeof(location_info->place_name) - 1);
        location_info->place_name[sizeof(location_info->place_name) - 1] = '\0';
        strncpy(location_info->address, cache_entry->address, sizeof(location_info->address) - 1);
        location_info->address[sizeof(location_info->address) - 1] = '\0';
        strncpy(location_info->country, cache_entry->country, sizeof(location_info->country) - 1);
        location_info->country[sizeof(location_info->country) - 1] = '\0';
        strncpy(location_info->state, cache_entry->state, sizeof(location_info->state) - 1);
        location_info->state[sizeof(location_info->state) - 1] = '\0';
        strncpy(location_info->city, cache_entry->city, sizeof(location_info->city) - 1);
        location_info->city[sizeof(location_info->city) - 1] = '\0';
        strncpy(location_info->postal_code, cache_entry->postal_code, sizeof(location_info->postal_code) - 1);
        location_info->postal_code[sizeof(location_info->postal_code) - 1] = '\0';
        location_info->service_used = cache_entry->service_used;
        location_info->timestamp = cache_entry->timestamp;
        
        g_location_services.cache_hits++;
        
        pthread_mutex_unlock(&g_location_services_mutex);
        
        LOGX_DEBUG("Location cache hit for (%.6f, %.6f): %s", lat, lon, cache_entry->place_name);
        return AUTONOMY_SUCCESS;
    }
    
    g_location_services.cache_misses++;
    
    // Perform reverse geocoding
    int result = perform_reverse_geocoding(lat, lon, location_info);
    
    if (result == AUTONOMY_SUCCESS) {
        // Add to cache
        add_location_to_cache(location_info);
        g_location_services.successful_requests++;
    } else {
        g_location_services.failed_requests++;
    }
    
    g_location_services.last_request = time(NULL);
    
    pthread_mutex_unlock(&g_location_services_mutex);
    
    return result;
}

// Find location in cache
static int find_location_in_cache(double lat, double lon) {
    time_t now = time(NULL);
    
    for (int i = 0; i < MAX_LOCATION_CACHE; i++) {
        if (!g_location_services.location_cache[i].active) {
            continue;
        }
        
        gps_location_cache_entry_t *cache_entry = &g_location_services.location_cache[i];
        
        // Check if cache entry is still valid
        if ((now - cache_entry->timestamp) > g_location_services.cache_ttl) {
            cache_entry->active = false;
            continue;
        }
        
        // Check if coordinates are within cache radius
        double distance = calculate_distance(lat, lon, cache_entry->lat, cache_entry->lon);
        if (distance <= g_location_services.cache_radius) {
            return i;
        }
    }
    
    return -1;
}

// Add location to cache
static void add_location_to_cache(const gps_location_info_t *location_info) {
    // Find free cache slot
    int cache_index = -1;
    for (int i = 0; i < MAX_LOCATION_CACHE; i++) {
        if (!g_location_services.location_cache[i].active) {
            cache_index = i;
            break;
        }
    }
    
    if (cache_index < 0) {
        // No free slots, remove oldest entry
        cache_index = find_oldest_cache_entry();
        if (cache_index < 0) {
            return;
        }
    }
    
    // Add location to cache
    gps_location_cache_entry_t *cache_entry = &g_location_services.location_cache[cache_index];
    cache_entry->active = true;
    cache_entry->timestamp = time(NULL);
    cache_entry->lat = location_info->lat;
    cache_entry->lon = location_info->lon;
    strncpy(cache_entry->place_name, location_info->place_name, sizeof(cache_entry->place_name) - 1);
    cache_entry->place_name[sizeof(cache_entry->place_name) - 1] = '\0';
    strncpy(cache_entry->address, location_info->address, sizeof(cache_entry->address) - 1);
    cache_entry->address[sizeof(cache_entry->address) - 1] = '\0';
    strncpy(cache_entry->country, location_info->country, sizeof(cache_entry->country) - 1);
    cache_entry->country[sizeof(cache_entry->country) - 1] = '\0';
    strncpy(cache_entry->state, location_info->state, sizeof(cache_entry->state) - 1);
    cache_entry->state[sizeof(cache_entry->state) - 1] = '\0';
    strncpy(cache_entry->city, location_info->city, sizeof(cache_entry->city) - 1);
    cache_entry->city[sizeof(cache_entry->city) - 1] = '\0';
    strncpy(cache_entry->postal_code, location_info->postal_code, sizeof(cache_entry->postal_code) - 1);
    cache_entry->postal_code[sizeof(cache_entry->postal_code) - 1] = '\0';
    cache_entry->service_used = location_info->service_used;
    
    LOGX_DEBUG("Added location to cache: %s", location_info->place_name);
}

// Find oldest cache entry
static int find_oldest_cache_entry(void) {
    int oldest_index = -1;
    time_t oldest_time = time(NULL);
    
    for (int i = 0; i < MAX_LOCATION_CACHE; i++) {
        if (g_location_services.location_cache[i].active && 
            g_location_services.location_cache[i].timestamp < oldest_time) {
            oldest_time = g_location_services.location_cache[i].timestamp;
            oldest_index = i;
        }
    }
    
    return oldest_index;
}

// Perform reverse geocoding
static int perform_reverse_geocoding(double lat, double lon, gps_location_info_t *location_info) {
    // Try different services in order of preference
    gps_location_service_t services[] = {
        g_location_services.default_service,
        LOCATION_SERVICE_NOMINATIM,
        LOCATION_SERVICE_GOOGLE,
        LOCATION_SERVICE_HERE
    };
    
    for (int i = 0; i < sizeof(services) / sizeof(services[0]); i++) {
        int result = try_reverse_geocoding_service(services[i], lat, lon, location_info);
        if (result == AUTONOMY_SUCCESS) {
            return AUTONOMY_SUCCESS;
        }
    }
    
    // If all services fail, create a basic location info
    create_basic_location_info(lat, lon, location_info);
    return AUTONOMY_SUCCESS;
}

// Try reverse geocoding with specific service
static int try_reverse_geocoding_service(gps_location_service_t service, double lat, double lon, 
                                        gps_location_info_t *location_info) {
    switch (service) {
        case LOCATION_SERVICE_NOMINATIM:
            return try_nominatim_service(lat, lon, location_info);
        case LOCATION_SERVICE_GOOGLE:
            return try_google_service(lat, lon, location_info);
        case LOCATION_SERVICE_HERE:
            return try_here_service(lat, lon, location_info);
        case LOCATION_SERVICE_CUSTOM:
            return try_custom_service(lat, lon, location_info);
        default:
            return AUTONOMY_ERROR_NOT_SUPPORTED;
    }
}

// Try Nominatim service (OpenStreetMap)
static int try_nominatim_service(double lat, double lon, gps_location_info_t *location_info) {
    // For now, implement a simulated response
    // In a full implementation, this would make HTTP requests to Nominatim API
    
    location_info->lat = lat;
    location_info->lon = lon;
    location_info->service_used = LOCATION_SERVICE_NOMINATIM;
    location_info->timestamp = time(NULL);
    
    // Simulate place name based on coordinates
    snprintf(location_info->place_name, sizeof(location_info->place_name), 
             "Location at %.4f, %.4f", lat, lon);
    
    // Simulate address components
    snprintf(location_info->address, sizeof(location_info->address), 
             "Unknown Address");
    snprintf(location_info->country, sizeof(location_info->country), 
             "Unknown Country");
    snprintf(location_info->state, sizeof(location_info->state), 
             "Unknown State");
    snprintf(location_info->city, sizeof(location_info->city), 
             "Unknown City");
    snprintf(location_info->postal_code, sizeof(location_info->postal_code), 
             "");
    
    LOGX_DEBUG("Nominatim service simulated response for (%.6f, %.6f)", lat, lon);
    return AUTONOMY_SUCCESS;
}

// Try Google service
static int try_google_service(double lat, double lon, gps_location_info_t *location_info) {
    // For now, implement a simulated response
    // In a full implementation, this would make HTTP requests to Google Geocoding API
    
    location_info->lat = lat;
    location_info->lon = lon;
    location_info->service_used = LOCATION_SERVICE_GOOGLE;
    location_info->timestamp = time(NULL);
    
    // Simulate place name based on coordinates
    snprintf(location_info->place_name, sizeof(location_info->place_name), 
             "Google Location at %.4f, %.4f", lat, lon);
    
    // Simulate address components
    snprintf(location_info->address, sizeof(location_info->address), 
             "Google Address");
    snprintf(location_info->country, sizeof(location_info->country), 
             "Google Country");
    snprintf(location_info->state, sizeof(location_info->state), 
             "Google State");
    snprintf(location_info->city, sizeof(location_info->city), 
             "Google City");
    snprintf(location_info->postal_code, sizeof(location_info->postal_code), 
             "12345");
    
    LOGX_DEBUG("Google service simulated response for (%.6f, %.6f)", lat, lon);
    return AUTONOMY_SUCCESS;
}

// Try HERE service
static int try_here_service(double lat, double lon, gps_location_info_t *location_info) {
    // For now, implement a simulated response
    // In a full implementation, this would make HTTP requests to HERE Geocoding API
    
    location_info->lat = lat;
    location_info->lon = lon;
    location_info->service_used = LOCATION_SERVICE_HERE;
    location_info->timestamp = time(NULL);
    
    // Simulate place name based on coordinates
    snprintf(location_info->place_name, sizeof(location_info->place_name), 
             "HERE Location at %.4f, %.4f", lat, lon);
    
    // Simulate address components
    snprintf(location_info->address, sizeof(location_info->address), 
             "HERE Address");
    snprintf(location_info->country, sizeof(location_info->country), 
             "HERE Country");
    snprintf(location_info->state, sizeof(location_info->state), 
             "HERE State");
    snprintf(location_info->city, sizeof(location_info->city), 
             "HERE City");
    snprintf(location_info->postal_code, sizeof(location_info->postal_code), 
             "67890");
    
    LOGX_DEBUG("HERE service simulated response for (%.6f, %.6f)", lat, lon);
    return AUTONOMY_SUCCESS;
}

// Try custom service
static int try_custom_service(double lat, double lon, gps_location_info_t *location_info) {
    // For now, implement a simulated response
    // In a full implementation, this would call user-defined functions or scripts
    
    location_info->lat = lat;
    location_info->lon = lon;
    location_info->service_used = LOCATION_SERVICE_CUSTOM;
    location_info->timestamp = time(NULL);
    
    // Simulate place name based on coordinates
    snprintf(location_info->place_name, sizeof(location_info->place_name), 
             "Custom Location at %.4f, %.4f", lat, lon);
    
    // Simulate address components
    snprintf(location_info->address, sizeof(location_info->address), 
             "Custom Address");
    snprintf(location_info->country, sizeof(location_info->country), 
             "Custom Country");
    snprintf(location_info->state, sizeof(location_info->state), 
             "Custom State");
    snprintf(location_info->city, sizeof(location_info->city), 
             "Custom City");
    snprintf(location_info->postal_code, sizeof(location_info->postal_code), 
             "CUSTOM");
    
    LOGX_DEBUG("Custom service simulated response for (%.6f, %.6f)", lat, lon);
    return AUTONOMY_SUCCESS;
}

// Create basic location info when services fail
static void create_basic_location_info(double lat, double lon, gps_location_info_t *location_info) {
    location_info->lat = lat;
    location_info->lon = lon;
    location_info->service_used = LOCATION_SERVICE_UNKNOWN;
    location_info->timestamp = time(NULL);
    
    snprintf(location_info->place_name, sizeof(location_info->place_name), 
             "Unknown Location at %.4f, %.4f", lat, lon);
    snprintf(location_info->address, sizeof(location_info->address), 
             "Unknown Address");
    snprintf(location_info->country, sizeof(location_info->country), 
             "Unknown");
    snprintf(location_info->state, sizeof(location_info->state), 
             "Unknown");
    snprintf(location_info->city, sizeof(location_info->city), 
             "Unknown");
    location_info->postal_code[0] = '\0';
    
    LOGX_WARN("All reverse geocoding services failed, created basic location info");
}

// Calculate distance between two GPS coordinates (Haversine formula)
static double calculate_distance(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371000.0;  // Earth's radius in meters
    
    double lat1_rad = lat1 * M_PI / 180.0;
    double lat2_rad = lat2 * M_PI / 180.0;
    double delta_lat = (lat2 - lat1) * M_PI / 180.0;
    double delta_lon = (lon2 - lon1) * M_PI / 180.0;
    
    double a = sin(delta_lat / 2.0) * sin(delta_lat / 2.0) +
               cos(lat1_rad) * cos(lat2_rad) *
               sin(delta_lon / 2.0) * sin(delta_lon / 2.0);
    
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    
    return R * c;
}

// Get location services status
static int gps_location_services_get_status(gps_location_services_status_t *status) {
    if (!g_location_services_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_location_services_mutex);
    
    status->enabled = g_location_services.enabled;
    status->default_service = g_location_services.default_service;
    status->cache_hits = g_location_services.cache_hits;
    status->cache_misses = g_location_services.cache_misses;
    status->total_requests = g_location_services.total_requests;
    status->successful_requests = g_location_services.successful_requests;
    status->failed_requests = g_location_services.failed_requests;
    status->last_request = g_location_services.last_request;
    
    // Calculate cache hit rate
    if (status->total_requests > 0) {
        status->cache_hit_rate = (double)status->cache_hits / status->total_requests;
    } else {
        status->cache_hit_rate = 0.0;
    }
    
    // Calculate success rate
    if (status->total_requests > 0) {
        status->success_rate = (double)status->successful_requests / status->total_requests;
    } else {
        status->success_rate = 0.0;
    }
    
    pthread_mutex_unlock(&g_location_services_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get location services configuration
static int gps_location_services_get_config(gps_location_services_config_t *config) {
    if (!g_location_services_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_location_services_mutex);
    
    config->enabled = g_location_services.enabled;
    config->max_cache = g_location_services.max_cache;
    config->cache_ttl = g_location_services.cache_ttl;
    config->max_attempts = g_location_services.max_attempts;
    config->cache_radius = g_location_services.cache_radius;
    config->default_service = g_location_services.default_service;
    
    pthread_mutex_unlock(&g_location_services_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set location services configuration
static int gps_location_services_set_config(const gps_location_services_config_t *config) {
    if (!g_location_services_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_location_services_mutex);
    
    g_location_services.enabled = config->enabled;
    g_location_services.max_cache = config->max_cache;
    g_location_services.cache_ttl = config->cache_ttl;
    g_location_services.max_attempts = config->max_attempts;
    g_location_services.cache_radius = config->cache_radius;
    g_location_services.default_service = config->default_service;
    
    pthread_mutex_unlock(&g_location_services_mutex);
    
    LOGX_INFO("GPS location services configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable location services
static int gps_location_services_set_enabled(bool enabled) {
    if (!g_location_services_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_location_services_mutex);
    g_location_services.enabled = enabled;
    pthread_mutex_unlock(&g_location_services_mutex);
    
    LOGX_INFO("GPS location services %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Clear location cache
static int gps_location_services_clear_cache(void) {
    if (!g_location_services_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_location_services_mutex);
    
    for (int i = 0; i < MAX_LOCATION_CACHE; i++) {
        g_location_services.location_cache[i].active = false;
    }
    
    pthread_mutex_unlock(&g_location_services_mutex);
    
    LOGX_INFO("GPS location services cache cleared");
    return AUTONOMY_SUCCESS;
}

// Reset location services
static int gps_location_services_reset(void) {
    if (!g_location_services_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_location_services_mutex);
    
    g_location_services.cache_hits = 0;
    g_location_services.cache_misses = 0;
    g_location_services.total_requests = 0;
    g_location_services.successful_requests = 0;
    g_location_services.failed_requests = 0;
    g_location_services.last_request = 0;
    
    // Clear all cache entries
    for (int i = 0; i < MAX_LOCATION_CACHE; i++) {
        g_location_services.location_cache[i].active = false;
    }
    
    pthread_mutex_unlock(&g_location_services_mutex);
    
    LOGX_INFO("GPS location services reset");
    return AUTONOMY_SUCCESS;
}

// Cleanup location services
static void gps_location_services_cleanup(void) {
    if (!g_location_services_initialized) {
        return;
    }
    
    pthread_mutex_destroy(&g_location_services_mutex);
    g_location_services_initialized = false;
    
    LOGX_INFO("GPS location services cleaned up");
}
