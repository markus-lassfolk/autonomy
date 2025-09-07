#ifndef GPS_LOCATION_SERVICES_H
#define GPS_LOCATION_SERVICES_H

#include "../core/types.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// Location service types
typedef enum {
    LOCATION_SERVICE_UNKNOWN = 0,
    LOCATION_SERVICE_NOMINATIM,
    LOCATION_SERVICE_GOOGLE,
    LOCATION_SERVICE_HERE,
    LOCATION_SERVICE_CUSTOM
} gps_location_service_t;

// GPS location information
typedef struct {
    double lat;                         // Latitude
    double lon;                         // Longitude
    char place_name[256];               // Place name
    char address[512];                  // Full address
    char country[64];                   // Country
    char state[64];                     // State/Province
    char city[64];                      // City
    char postal_code[16];               // Postal code
    gps_location_service_t service_used; // Service that provided the information
    time_t timestamp;                   // When the information was retrieved
} gps_location_info_t;

// GPS location cache entry
typedef struct {
    bool active;                        // Whether cache entry is active
    time_t timestamp;                   // Cache entry timestamp
    double lat;                         // Latitude
    double lon;                         // Longitude
    char place_name[256];               // Place name
    char address[512];                  // Full address
    char country[64];                   // Country
    char state[64];                     // State/Province
    char city[64];                      // City
    char postal_code[16];               // Postal code
    gps_location_service_t service_used; // Service that provided the information
} gps_location_cache_entry_t;

// Location services configuration
typedef struct {
    bool enabled;                       // Enable/disable location services
    int max_cache;                      // Maximum cached locations
    int cache_ttl;                      // Cache TTL in seconds
    int max_attempts;                   // Maximum reverse geocoding attempts
    double cache_radius;                // Cache radius in meters
    gps_location_service_t default_service; // Default service to use
} gps_location_services_config_t;

// Location services status
typedef struct {
    bool enabled;                       // Location services enabled
    gps_location_service_t default_service; // Default service
    int cache_hits;                     // Cache hits
    int cache_misses;                   // Cache misses
    int total_requests;                 // Total requests
    int successful_requests;            // Successful requests
    int failed_requests;                // Failed requests
    time_t last_request;                // Last request timestamp
    double cache_hit_rate;              // Cache hit rate (0.0-1.0)
    double success_rate;                // Success rate (0.0-1.0)
} gps_location_services_status_t;

// Location services system state
typedef struct {
    bool enabled;                       // Location services enabled
    int max_cache;                      // Maximum cache
    int cache_ttl;                      // Cache TTL
    int max_attempts;                   // Maximum attempts
    double cache_radius;                // Cache radius
    gps_location_service_t default_service; // Default service
    
    // Statistics
    int cache_hits;                     // Cache hits
    int cache_misses;                   // Cache misses
    int total_requests;                 // Total requests
    int successful_requests;            // Successful requests
    int failed_requests;                // Failed requests
    time_t last_request;                // Last request
    
    // Location cache
    gps_location_cache_entry_t location_cache[1000]; // Location cache
} gps_location_services_t;

// Function prototypes

/**
 * Initialize GPS location services
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_location_services_init(void);

/**
 * Reverse geocode GPS coordinates
 * @param lat Latitude
 * @param lon Longitude
 * @param location_info Location information structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_location_services_reverse_geocode(double lat, double lon, gps_location_info_t *location_info);

/**
 * Get location services status
 * @param status Status structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_location_services_get_status(gps_location_services_status_t *status);

/**
 * Get location services configuration
 * @param config Configuration structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_location_services_get_config(gps_location_services_config_t *config);

/**
 * Set location services configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_location_services_set_config(const gps_location_services_config_t *config);

/**
 * Enable/disable location services
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_location_services_set_enabled(bool enabled);

/**
 * Clear location cache
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_location_services_clear_cache(void);

/**
 * Reset location services
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_location_services_reset(void);

/**
 * Cleanup location services
 */
void gps_location_services_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_LOCATION_SERVICES_H
