#include "gps_coordinate_utils.h"
#include "gps_location_services.h"
#include "gps_google_api.h"
#include "../shared/logging/logx.h"
#include "../utils/http_client.h"
#include "../core/types.h"
#include "../shared/utils/string_utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>
#include <json-c/json.h>

// Forward declarations
static int try_nominatim_service(double lat, double lon, gps_location_info_t *location_info);
static int try_google_service(double lat, double lon, gps_location_info_t *location_info);
static int try_here_service(double lat, double lon, gps_location_info_t *location_info);
static int try_custom_service(double lat, double lon, gps_location_info_t *location_info);
static int parse_nominatim_response(const char* json_data, gps_location_info_t *location_info);
static int parse_here_response(const char* json_data, gps_location_info_t *location_info);

// External reference to global configuration
extern autonomy_config_t g_config;

// Location services configuration
static const int MAX_LOCATION_CACHE = 1000; // Use configurable value             // Maximum cached locations
static const int LOCATION_CACHE_TTL = 86400; // Use configurable value            // 24 hour cache TTL
static const int MAX_REVERSE_GEOCODE_ATTEMPTS = 3; // Use configurable value      // Maximum reverse geocoding attempts
static const double LOCATION_CACHE_RADIUS = 100.0; // Use configurable value      // 100m cache radius
static const int MAX_PLACE_NAME_LENGTH = 256; // Use configurable value           // Maximum place name length
static const int MAX_ADDRESS_LENGTH = 512; // Use configurable value               // Maximum address length

// Location service types
static const char* LOCATION_SERVICE_NAMES[] = {
    "unknown", "nominatim", "google", "here", "custom"
};

// Global location services state
static gps_location_services_t g_location_services = {0};
static bool g_location_services_initialized = false; // Use configurable setting
static pthread_mutex_t g_location_services_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations
int find_location_in_cache(double lat, double lon);
void add_location_to_cache(const gps_location_info_t *location_info);
int find_oldest_cache_entry(void);
int perform_reverse_geocoding(double lat, double lon, gps_location_info_t *location_info);
int try_reverse_geocoding_service(gps_location_service_t service, double lat, double lon, gps_location_info_t *location_info);
void create_basic_location_info(double lat, double lon, gps_location_info_t *location_info);

// Reverse geocode GPS coordinates
int gps_location_services_reverse_geocode(double lat, double lon, gps_location_info_t *location_info) {
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
        safe_strncpy(location_info->place_name, cache_entry->place_name, sizeof(location_info->place_name));
        location_info->place_name[sizeof(location_info->place_name) - 1] = '\0';
        safe_strncpy(location_info->address, cache_entry->address, sizeof(location_info->address));
        location_info->address[sizeof(location_info->address) - 1] = '\0';
        safe_strncpy(location_info->country, cache_entry->country, sizeof(location_info->country));
        location_info->country[sizeof(location_info->country) - 1] = '\0';
        safe_strncpy(location_info->state, cache_entry->state, sizeof(location_info->state));
        location_info->state[sizeof(location_info->state) - 1] = '\0';
        safe_strncpy(location_info->city, cache_entry->city, sizeof(location_info->city));
        location_info->city[sizeof(location_info->city) - 1] = '\0';
        safe_strncpy(location_info->postal_code, cache_entry->postal_code, sizeof(location_info->postal_code));
        location_info->postal_code[sizeof(location_info->postal_code) - 1] = '\0';
        location_info->service_used = cache_entry->service_used;
        location_info->timestamp = cache_entry->timestamp;
        
        g_location_services.cache_hits++;
        
        pthread_mutex_unlock(&g_location_services_mutex);
        
        LOGX_DEBUG_MSG("Location cache hit for (%.6f, %.6f): %s", lat, lon, cache_entry->place_name);
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
int find_location_in_cache(double lat, double lon) {
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
        double distance = gps_coordinate_distance(lat, lon, cache_entry->lat, cache_entry->lon);
        if (distance <= g_location_services.cache_radius) {
            return i;
        }
    }
    
    return -1;
}

// Add location to cache
void add_location_to_cache(const gps_location_info_t *location_info) {
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
    safe_strncpy(cache_entry->place_name, location_info->place_name, sizeof(cache_entry->place_name));
    cache_entry->place_name[sizeof(cache_entry->place_name) - 1] = '\0';
    safe_strncpy(cache_entry->address, location_info->address, sizeof(cache_entry->address));
    cache_entry->address[sizeof(cache_entry->address) - 1] = '\0';
    safe_strncpy(cache_entry->country, location_info->country, sizeof(cache_entry->country));
    cache_entry->country[sizeof(cache_entry->country) - 1] = '\0';
    safe_strncpy(cache_entry->state, location_info->state, sizeof(cache_entry->state));
    cache_entry->state[sizeof(cache_entry->state) - 1] = '\0';
    safe_strncpy(cache_entry->city, location_info->city, sizeof(cache_entry->city));
    cache_entry->city[sizeof(cache_entry->city) - 1] = '\0';
    safe_strncpy(cache_entry->postal_code, location_info->postal_code, sizeof(cache_entry->postal_code));
    cache_entry->postal_code[sizeof(cache_entry->postal_code) - 1] = '\0';
    cache_entry->service_used = location_info->service_used;
    
    LOGX_DEBUG_MSG("Added location to cache: %s", location_info->place_name);
}

// Find oldest cache entry
int find_oldest_cache_entry(void) {
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
int perform_reverse_geocoding(double lat, double lon, gps_location_info_t *location_info) {
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
int try_reverse_geocoding_service(gps_location_service_t service, double lat, double lon, 
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
int try_nominatim_service(double lat, double lon, gps_location_info_t *location_info) {
    http_response_t response;
    char url[1024];
    int result;
    
    // Build Nominatim reverse geocoding URL
    snprintf(url, sizeof(url), 
             "https://nominatim.openstreetmap.org/reverse?format=json&lat=%.6f&lon=%.6f&zoom=18&addressdetails=1",
             lat, lon);
    
    // Make HTTP request using shared HTTP client
    result = http_client_get(url, 30, &response);
    if (result != 0 || !response.success) {
        LOGX_ERROR_MSG("Nominatim API request failed for (%.6f, %.6f)", lat, lon);
        http_response_cleanup(&response);
        return AUTONOMY_ERROR_NETWORK;
    }
    
    // Initialize location info
    location_info->lat = lat;
    location_info->lon = lon;
    location_info->service_used = LOCATION_SERVICE_NOMINATIM;
    location_info->timestamp = time(NULL);
    
    // Clear all fields first
    memset(location_info->place_name, 0, sizeof(location_info->place_name));
    memset(location_info->address, 0, sizeof(location_info->address));
    memset(location_info->country, 0, sizeof(location_info->country));
    memset(location_info->state, 0, sizeof(location_info->state));
    memset(location_info->city, 0, sizeof(location_info->city));
    memset(location_info->postal_code, 0, sizeof(location_info->postal_code));
    
    // Parse JSON response
    result = parse_nominatim_response(response.data, location_info);
    if (result != AUTONOMY_SUCCESS) {
        LOGX_ERROR_MSG("Failed to parse Nominatim response for (%.6f, %.6f)", lat, lon);
        http_response_cleanup(&response);
        return result;
    }
    
    // Fallback to basic info if parsing failed to populate fields
    if (strlen(location_info->place_name) == 0) {
        snprintf(location_info->place_name, sizeof(location_info->place_name), 
                 "Location at %.4f, %.4f", lat, lon);
    }
    if (strlen(location_info->address) == 0) {
        snprintf(location_info->address, sizeof(location_info->address), 
                 "Unknown Address");
    }
    if (strlen(location_info->country) == 0) {
        snprintf(location_info->country, sizeof(location_info->country), 
                 "Unknown Country");
    }
    if (strlen(location_info->state) == 0) {
        snprintf(location_info->state, sizeof(location_info->state), 
                 "Unknown State");
    }
    if (strlen(location_info->city) == 0) {
        snprintf(location_info->city, sizeof(location_info->city), 
                 "Unknown City");
    }
    
    http_response_cleanup(&response);
    
    LOGX_DEBUG_MSG("Nominatim service response for (%.6f, %.6f): %s", lat, lon, location_info->place_name);
    return AUTONOMY_SUCCESS;
}

// Try Google service
int try_google_service(double lat, double lon, gps_location_info_t *location_info) {
    gps_google_location_info_t google_location;
    int result;
    
    // Use the existing Google API implementation
    result = gps_google_api_reverse_geocode(lat, lon, &google_location);
    if (result != AUTONOMY_SUCCESS) {
        LOGX_ERROR_MSG("Google API reverse geocoding failed for (%.6f, %.6f)", lat, lon);
        return result;
    }
    
    // Convert Google API response to our location info format
    location_info->lat = lat;
    location_info->lon = lon;
    location_info->service_used = LOCATION_SERVICE_GOOGLE;
    location_info->timestamp = time(NULL);
    
    // Copy Google API response data
    safe_strncpy(location_info->place_name, google_location.formatted_address, sizeof(location_info->place_name));
    safe_strncpy(location_info->address, google_location.formatted_address, sizeof(location_info->address));
    safe_strncpy(location_info->country, google_location.country, sizeof(location_info->country));
    safe_strncpy(location_info->state, google_location.administrative_area, sizeof(location_info->state));
    safe_strncpy(location_info->city, google_location.locality, sizeof(location_info->city));
    safe_strncpy(location_info->postal_code, google_location.postal_code, sizeof(location_info->postal_code));
    
    // Ensure null termination
    location_info->place_name[sizeof(location_info->place_name) - 1] = '\0';
    location_info->address[sizeof(location_info->address) - 1] = '\0';
    location_info->country[sizeof(location_info->country) - 1] = '\0';
    location_info->state[sizeof(location_info->state) - 1] = '\0';
    location_info->city[sizeof(location_info->city) - 1] = '\0';
    location_info->postal_code[sizeof(location_info->postal_code) - 1] = '\0';
    
    LOGX_DEBUG_MSG("Google service response for (%.6f, %.6f): %s", lat, lon, location_info->place_name);
    return AUTONOMY_SUCCESS;
}

// Try HERE service
int try_here_service(double lat, double lon, gps_location_info_t *location_info) {
    http_response_t response;
    char url[1024];
    int result;
    
    // Get HERE API key from configuration or environment
    const char* here_api_key = NULL;
    
    // First try to get from environment variable - CRITICAL FIX: Validate environment variable
    here_api_key = getenv("HERE_API_KEY"); // flawfinder: ignore
    if (here_api_key && strlen(here_api_key) > 256) {
        here_api_key = NULL; // Reject overly long environment variables
    }
    
    // If not found in environment, try to get from UCI configuration
    if (!here_api_key) {
        FILE *uci_fp = popen("uci get autonomy.gps.here_api_key 2>/dev/null", "r");
        if (uci_fp) {
            char key_buffer[256];
            if (fgets(key_buffer, sizeof(key_buffer), uci_fp)) {
                // Remove newline
                char *newline = strchr(key_buffer, '\n');
                if (newline) *newline = '\0';
                
                // Remove quotes if present
                if (key_buffer[0] == '\'' && key_buffer[strlen(key_buffer)-1] == '\'') {
                    key_buffer[strlen(key_buffer)-1] = '\0';
                    here_api_key = key_buffer + 1;
                } else {
                    here_api_key = key_buffer;
                }
            }
            pclose(uci_fp);
        }
    }
    
    // If still no API key, use a default or return error
    if (!here_api_key || strlen(here_api_key) == 0) {
        LOGX_WARN_MSG("HERE API key not configured, skipping reverse geocoding");
        return AUTONOMY_ERROR_NOT_CONFIGURED;
    }
    
    // Build HERE reverse geocoding URL
    snprintf(url, sizeof(url), 
             "https://revgeocode.search.hereapi.com/v1/revgeocode?at=%.6f,%.6f&apikey=%s",
             lat, lon, here_api_key);
    
    // Make HTTP request using shared HTTP client
    result = http_client_get(url, 30, &response);
    if (result != 0 || !response.success) {
        LOGX_ERROR_MSG("HERE API request failed for (%.6f, %.6f)", lat, lon);
        http_response_cleanup(&response);
        return AUTONOMY_ERROR_NETWORK;
    }
    
    // Initialize location info
    location_info->lat = lat;
    location_info->lon = lon;
    location_info->service_used = LOCATION_SERVICE_HERE;
    location_info->timestamp = time(NULL);
    
    // Clear all fields first
    memset(location_info->place_name, 0, sizeof(location_info->place_name));
    memset(location_info->address, 0, sizeof(location_info->address));
    memset(location_info->country, 0, sizeof(location_info->country));
    memset(location_info->state, 0, sizeof(location_info->state));
    memset(location_info->city, 0, sizeof(location_info->city));
    memset(location_info->postal_code, 0, sizeof(location_info->postal_code));
    
    // Parse JSON response
    result = parse_here_response(response.data, location_info);
    if (result != AUTONOMY_SUCCESS) {
        LOGX_ERROR_MSG("Failed to parse HERE response for (%.6f, %.6f)", lat, lon);
        http_response_cleanup(&response);
        return result;
    }
    
    // Fallback to basic info if parsing failed to populate fields
    if (strlen(location_info->place_name) == 0) {
        snprintf(location_info->place_name, sizeof(location_info->place_name), 
                 "Location at %.4f, %.4f", lat, lon);
    }
    if (strlen(location_info->address) == 0) {
        snprintf(location_info->address, sizeof(location_info->address), 
                 "Unknown Address");
    }
    if (strlen(location_info->country) == 0) {
        snprintf(location_info->country, sizeof(location_info->country), 
                 "Unknown Country");
    }
    if (strlen(location_info->state) == 0) {
        snprintf(location_info->state, sizeof(location_info->state), 
                 "Unknown State");
    }
    if (strlen(location_info->city) == 0) {
        snprintf(location_info->city, sizeof(location_info->city), 
                 "Unknown City");
    }
    
    http_response_cleanup(&response);
    
    LOGX_DEBUG_MSG("HERE service response for (%.6f, %.6f): %s", lat, lon, location_info->place_name);
    return AUTONOMY_SUCCESS;
}

// Try custom service
int try_custom_service(double lat, double lon, gps_location_info_t *location_info) {
    // Check if custom service script is configured
    char custom_script[256];
    FILE *uci_fp = popen("uci get autonomy.gps.custom_location_script 2>/dev/null", "r");
    if (uci_fp && fgets(custom_script, sizeof(custom_script), uci_fp)) {
        pclose(uci_fp);
        
        // Remove newline
        char *newline = strchr(custom_script, '\n');
        if (newline) *newline = '\0';
        
        // Execute custom script with coordinates
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "%s %.6f %.6f 2>/dev/null", custom_script, lat, lon);
        
        FILE *script_fp = popen(cmd, "r");
        if (script_fp) {
            char response[1024];
            if (fgets(response, sizeof(response), script_fp)) {
                // Parse script response (expected format: "place_name|address|city|state|country")
                char *tokens[5];
                int token_count = 0; // Use configurable value
                char *token = strtok(response, "|");
                while (token && token_count < 5) {
                    tokens[token_count++] = token;
                    token = strtok(NULL, "|");
                }
                pclose(script_fp);
                
                if (token_count >= 1) {
                    safe_strncpy(location_info->place_name, tokens[0], sizeof(location_info->place_name));
                    location_info->place_name[sizeof(location_info->place_name) - 1] = '\0';
                    
                    if (token_count >= 2) {
                        safe_strncpy(location_info->address, tokens[1], sizeof(location_info->address));
                        location_info->address[sizeof(location_info->address) - 1] = '\0';
                    }
                    if (token_count >= 3) {
                        safe_strncpy(location_info->city, tokens[2], sizeof(location_info->city));
                        location_info->city[sizeof(location_info->city) - 1] = '\0';
                    }
                    if (token_count >= 4) {
                        safe_strncpy(location_info->state, tokens[3], sizeof(location_info->state));
                        location_info->state[sizeof(location_info->state) - 1] = '\0';
                    }
                    if (token_count >= 5) {
                        safe_strncpy(location_info->country, tokens[4], sizeof(location_info->country));
                        location_info->country[sizeof(location_info->country) - 1] = '\0';
                    }
                    
                    location_info->lat = lat;
                    location_info->lon = lon;
                    location_info->service_used = LOCATION_SERVICE_CUSTOM;
                    location_info->timestamp = time(NULL);
                    
                    return AUTONOMY_SUCCESS;
                }
            }
            pclose(script_fp);
        }
    } else {
        if (uci_fp) pclose(uci_fp);
    }
    
    // Fallback: create basic location info
    location_info->lat = lat;
    location_info->lon = lon;
    location_info->service_used = LOCATION_SERVICE_CUSTOM;
    location_info->timestamp = time(NULL);
    
    snprintf(location_info->place_name, sizeof(location_info->place_name), 
             "Location at %.4f, %.4f", lat, lon);
    
    // Set default address components
    safe_strncpy(location_info->address, "Unknown Address", sizeof(location_info->address));
    location_info->address[sizeof(location_info->address) - 1] = '\0';
    safe_strncpy(location_info->country, "Unknown Country", sizeof(location_info->country));
    location_info->country[sizeof(location_info->country) - 1] = '\0';
    safe_strncpy(location_info->state, "Unknown State", sizeof(location_info->state));
    location_info->state[sizeof(location_info->state) - 1] = '\0';
    safe_strncpy(location_info->city, "Unknown City", sizeof(location_info->city));
    location_info->city[sizeof(location_info->city) - 1] = '\0';
    safe_strncpy(location_info->postal_code, "00000", sizeof(location_info->postal_code));
    location_info->postal_code[sizeof(location_info->postal_code) - 1] = '\0';
    
    LOGX_DEBUG_MSG("Custom service fallback response for (%.6f, %.6f)", lat, lon);
    return AUTONOMY_SUCCESS;
}

// Create enhanced location info when services fail
void create_basic_location_info(double lat, double lon, gps_location_info_t *location_info) {
    location_info->lat = lat;
    location_info->lon = lon;
    location_info->service_used = LOCATION_SERVICE_UNKNOWN;
    location_info->timestamp = time(NULL);

    // Provide more meaningful information based on coordinates
    char hemisphere_lat = (lat >= 0) ? 'N' : 'S';
    char hemisphere_lon = (lon >= 0) ? 'E' : 'W';
    double abs_lat = fabs(lat);
    double abs_lon = fabs(lon);

    // Create descriptive location name
    snprintf(location_info->place_name, sizeof(location_info->place_name),
             "%.2f%c, %.2f%c", abs_lat, hemisphere_lat, abs_lon, hemisphere_lon);

    // Create more detailed address information
    char continent[32] = "Unknown Continent";
    char region[32] = "Unknown Region";

    // Determine continent based on latitude/longitude ranges
    if (lat >= -60 && lat <= 80) {
        if (lon >= -20 && lon <= 60) { // Europe/Africa
            if (lat >= 35) {
                safe_strncpy(continent, "Europe", sizeof(continent));
                safe_strncpy(region, "Northern Europe", sizeof(region));
            } else if (lat >= 0) {
                safe_strncpy(continent, "Europe", sizeof(continent));
                safe_strncpy(region, "Southern Europe", sizeof(region));
            } else {
                safe_strncpy(continent, "Africa", sizeof(continent));
                safe_strncpy(region, lat >= -20 ? "Northern Africa" : "Southern Africa", sizeof(region));
            }
        } else if (lon >= -130 && lon <= -60) { // Americas
            safe_strncpy(continent, "North America", sizeof(continent));
            safe_strncpy(region, lat >= 30 ? "Northern US/Canada" : "Central/South America", sizeof(region));
        } else if (lon >= 100 && lon <= 180) { // Asia/Pacific
            safe_strncpy(continent, "Asia", sizeof(continent));
            safe_strncpy(region, lat >= 20 ? "East Asia" : "Southeast Asia", sizeof(region));
        } else if (lon >= 60 && lon <= 100) { // Middle East/Asia
            safe_strncpy(continent, "Asia", sizeof(continent));
            safe_strncpy(region, "Central Asia", sizeof(region));
        }
    }

    // Create enhanced address
    snprintf(location_info->address, sizeof(location_info->address),
             "Approximate Location: %s, %s (%.4f, %.4f)", region, continent, lat, lon);

    // Set basic country/state info
    snprintf(location_info->country, sizeof(location_info->country), "%s", continent);
    snprintf(location_info->state, sizeof(location_info->state), "%s", region);
    snprintf(location_info->city, sizeof(location_info->city), "Unknown City");

    // Generate a pseudo postal code based on coordinates (for caching purposes)
    int postal_lat = (int)((abs_lat + 90) * 100); // 0-18000
    int postal_lon = (int)((abs_lon + 180) * 100); // 0-36000
    snprintf(location_info->postal_code, sizeof(location_info->postal_code),
             "%04d-%04d", postal_lat, postal_lon);

    LOGX_WARN_MSG("All reverse geocoding services failed, created enhanced location info with geographic context");
}

// Calculate distance between two GPS coordinates (Haversine formula)

// Get location services status
int gps_location_services_get_status(gps_location_services_status_t *status) {
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
int gps_location_services_get_config(gps_location_services_config_t *config) {
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
int gps_location_services_set_config(const gps_location_services_config_t *config) {
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
    
    LOGX_INFO_MSG("GPS location services configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable location services
int gps_location_services_set_enabled(bool enabled) {
    if (!g_location_services_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_location_services_mutex);
    g_location_services.enabled = enabled;
    pthread_mutex_unlock(&g_location_services_mutex);
    
    LOGX_INFO_MSG("GPS location services %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Clear location cache
int gps_location_services_clear_cache(void) {
    if (!g_location_services_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_location_services_mutex);
    
    for (int i = 0; i < MAX_LOCATION_CACHE; i++) {
        g_location_services.location_cache[i].active = false;
    }
    
    pthread_mutex_unlock(&g_location_services_mutex);
    
    LOGX_INFO_MSG("GPS location services cache cleared");
    return AUTONOMY_SUCCESS;
}

// Reset location services
int gps_location_services_reset(void) {
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
    
    LOGX_INFO_MSG("GPS location services reset");
    return AUTONOMY_SUCCESS;
}

// Cleanup location services
void gps_location_services_cleanup(void) {
    if (!g_location_services_initialized) {
        return;
    }
    
    pthread_mutex_destroy(&g_location_services_mutex);
    g_location_services_initialized = false; // Use configurable setting
    
    // Cleanup HTTP client
    http_client_cleanup();
    
    LOGX_INFO_MSG("GPS location services cleaned up");
}

// JSON parsing functions (HTTP requests now handled by shared http_client)

// Parse Nominatim API response
static int parse_nominatim_response(const char* json_data, gps_location_info_t *location_info) {
    json_object* root = json_tokener_parse(json_data);
    if (!root) {
        LOGX_ERROR_MSG("Failed to parse Nominatim JSON response");
        return AUTONOMY_ERROR_INVALID_DATA;
    }
    
    // Nominatim returns an array, get the first result
    json_object* results = NULL;
    if (json_object_object_get_ex(root, "results", &results) && json_object_is_type(results, json_type_array)) {
        json_object* first_result = json_object_array_get_idx(results, 0);
        if (first_result) {
            // Parse address components
            json_object* address = NULL;
            if (json_object_object_get_ex(first_result, "address", &address)) {
                json_object* country = NULL;
                json_object* state = NULL;
                json_object* city = NULL;
                json_object* postcode = NULL;
                
                if (json_object_object_get_ex(address, "country", &country)) {
                    snprintf(location_info->country, sizeof(location_info->country), "%s", json_object_get_string(country));
                }
                if (json_object_object_get_ex(address, "state", &state)) {
                    snprintf(location_info->state, sizeof(location_info->state), "%s", json_object_get_string(state));
                }
                if (json_object_object_get_ex(address, "city", &city)) {
                    snprintf(location_info->city, sizeof(location_info->city), "%s", json_object_get_string(city));
                }
                if (json_object_object_get_ex(address, "postcode", &postcode)) {
                    snprintf(location_info->postal_code, sizeof(location_info->postal_code), "%s", json_object_get_string(postcode));
                }
            }
            
            // Get display name (place name)
            json_object* display_name = NULL;
            if (json_object_object_get_ex(first_result, "display_name", &display_name)) {
                snprintf(location_info->place_name, sizeof(location_info->place_name), "%s", json_object_get_string(display_name));
            }
            
            // Get formatted address
            json_object* formatted = NULL;
            if (json_object_object_get_ex(first_result, "formatted", &formatted)) {
                snprintf(location_info->address, sizeof(location_info->address), "%s", json_object_get_string(formatted));
            }
        }
    }
    
    json_object_put(root);
    return AUTONOMY_SUCCESS;
}

// Parse Google Geocoding API response
static int parse_google_response(const char* json_data, gps_location_info_t *location_info) {
    json_object* root = json_tokener_parse(json_data);
    if (!root) {
        LOGX_ERROR_MSG("Failed to parse Google JSON response");
        return AUTONOMY_ERROR_INVALID_DATA;
    }
    
    // Check for errors
    json_object* error = NULL;
    if (json_object_object_get_ex(root, "error", &error)) {
        LOGX_ERROR_MSG("Google API returned error");
        json_object_put(root);
        return AUTONOMY_ERROR_INVALID_DATA;
    }
    
    // Get results array
    json_object* results = NULL;
    if (json_object_object_get_ex(root, "results", &results) && json_object_is_type(results, json_type_array)) {
        json_object* first_result = json_object_array_get_idx(results, 0);
        if (first_result) {
            // Parse address components
            json_object* address_components = NULL;
            if (json_object_object_get_ex(first_result, "address_components", &address_components) && 
                json_object_is_type(address_components, json_type_array)) {
                
                int num_components = json_object_array_length(address_components);
                for (int i = 0; i < num_components; i++) {
                    json_object* component = json_object_array_get_idx(address_components, i);
                    json_object* types = NULL;
                    json_object* long_name = NULL;
                    
                    if (json_object_object_get_ex(component, "types", &types) &&
                        json_object_object_get_ex(component, "long_name", &long_name)) {
                        
                        // Check component types
                        int num_types = json_object_array_length(types);
                        for (int j = 0; j < num_types; j++) {
                            json_object* type = json_object_array_get_idx(types, j);
                            const char* type_str = json_object_get_string(type);
                            
                            if (strcmp(type_str, "country") == 0) {
                                snprintf(location_info->country, sizeof(location_info->country), "%s", json_object_get_string(long_name));
                            } else if (strcmp(type_str, "administrative_area_level_1") == 0) {
                                snprintf(location_info->state, sizeof(location_info->state), "%s", json_object_get_string(long_name));
                            } else if (strcmp(type_str, "locality") == 0 || strcmp(type_str, "administrative_area_level_2") == 0) {
                                snprintf(location_info->city, sizeof(location_info->city), "%s", json_object_get_string(long_name));
                            } else if (strcmp(type_str, "postal_code") == 0) {
                                snprintf(location_info->postal_code, sizeof(location_info->postal_code), "%s", json_object_get_string(long_name));
                            }
                        }
                    }
                }
            }
            
            // Get formatted address
            json_object* formatted_address = NULL;
            if (json_object_object_get_ex(first_result, "formatted_address", &formatted_address)) {
                snprintf(location_info->address, sizeof(location_info->address), "%s", json_object_get_string(formatted_address));
            }
            
            // Use formatted address as place name if no specific place name
            if (strlen(location_info->place_name) == 0) {
                snprintf(location_info->place_name, sizeof(location_info->place_name), "%s", location_info->address);
            }
        }
    }
    
    json_object_put(root);
    return AUTONOMY_SUCCESS;
}

// Parse HERE Geocoding API response
static int parse_here_response(const char* json_data, gps_location_info_t *location_info) {
    json_object* root = json_tokener_parse(json_data);
    if (!root) {
        LOGX_ERROR_MSG("Failed to parse HERE JSON response");
        return AUTONOMY_ERROR_INVALID_DATA;
    }
    
    // Get results array
    json_object* results = NULL;
    if (json_object_object_get_ex(root, "results", &results) && json_object_is_type(results, json_type_array)) {
        json_object* first_result = json_object_array_get_idx(results, 0);
        if (first_result) {
            // Parse address components
            json_object* address = NULL;
            if (json_object_object_get_ex(first_result, "address", &address)) {
                json_object* country = NULL;
                json_object* state = NULL;
                json_object* city = NULL;
                json_object* postal_code = NULL;
                
                if (json_object_object_get_ex(address, "country", &country)) {
                    snprintf(location_info->country, sizeof(location_info->country), "%s", json_object_get_string(country));
                }
                if (json_object_object_get_ex(address, "state", &state)) {
                    snprintf(location_info->state, sizeof(location_info->state), "%s", json_object_get_string(state));
                }
                if (json_object_object_get_ex(address, "city", &city)) {
                    snprintf(location_info->city, sizeof(location_info->city), "%s", json_object_get_string(city));
                }
                if (json_object_object_get_ex(address, "postalCode", &postal_code)) {
                    snprintf(location_info->postal_code, sizeof(location_info->postal_code), "%s", json_object_get_string(postal_code));
                }
            }
            
            // Get label (formatted address)
            json_object* label = NULL;
            if (json_object_object_get_ex(first_result, "label", &label)) {
                snprintf(location_info->address, sizeof(location_info->address), "%s", json_object_get_string(label));
                snprintf(location_info->place_name, sizeof(location_info->place_name), "%s", json_object_get_string(label));
            }
        }
    }
    
    json_object_put(root);
    return AUTONOMY_SUCCESS;
}

// Initialize GPS location services
int gps_location_services_init(void) {
    if (g_location_services_initialized) {
        LOGX_WARN_MSG("GPS location services already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_location_services_mutex);
    
    // Initialize location services state
    memset(&g_location_services, 0, sizeof(gps_location_services_t));
    g_location_services.enabled = true;
    g_location_services.max_cache = 1000;
    g_location_services.cache_ttl = 3600; // 1 hour
    g_location_services.max_attempts = 3;
    g_location_services.cache_radius = 100.0; // 100 meters
    g_location_services.default_service = LOCATION_SERVICE_NOMINATIM;
    
    g_location_services_initialized = true;
    
    pthread_mutex_unlock(&g_location_services_mutex);
    
    LOGX_INFO_MSG("GPS location services initialized");
    return AUTONOMY_SUCCESS;
}
