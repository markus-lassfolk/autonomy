#ifndef GPS_GEOFENCE_H
#define GPS_GEOFENCE_H

#include "../core/types.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// GPS coordinate structure
typedef struct {
    double lat;                         // Latitude
    double lon;                         // Longitude
} gps_coordinate_t;

// Geofence types
typedef enum {
    GEOFENCE_TYPE_UNKNOWN = 0,
    GEOFENCE_TYPE_CIRCLE,
    GEOFENCE_TYPE_POLYGON,
    GEOFENCE_TYPE_RECTANGLE,
    GEOFENCE_TYPE_PATH
} gps_geofence_type_t;

// Geofence status
typedef enum {
    GEOFENCE_STATUS_UNKNOWN = 0,
    GEOFENCE_STATUS_INSIDE,
    GEOFENCE_STATUS_OUTSIDE
} gps_geofence_status_t;

// Geofence definition
typedef struct {
    bool active;                        // Whether geofence is active
    int geofence_id;                    // Unique geofence identifier
    char name[64];                      // Geofence name
    gps_geofence_type_t geofence_type;  // Type of geofence
    int point_count;                    // Number of points
    gps_coordinate_t points[100];       // Geofence points
    double center_lat;                  // Center latitude
    double center_lon;                  // Center longitude
    double radius_meters;               // Radius for circular geofences
    double buffer_distance;             // Buffer distance in meters
    bool enabled;                       // Whether geofence is enabled
    time_t last_event;                  // Last event timestamp
    int event_count;                    // Total event count
    gps_geofence_status_t current_status; // Current status
} gps_geofence_definition_t;

// Geofence configuration
typedef struct {
    bool enabled;                       // Enable/disable geofencing
    int max_geofences;                  // Maximum number of geofences
    int max_points;                     // Maximum points per geofence
    double default_buffer;              // Default buffer distance
    int check_interval;                 // Check interval in seconds
} gps_geofence_config_t;

// Note: gps_geofence_status_t is defined in ../core/types.h

// Geofencing system state
typedef struct {
    bool enabled;                       // Geofencing enabled
    int max_geofences;                  // Maximum geofences
    int max_points;                     // Maximum points
    double default_buffer;              // Default buffer
    int check_interval;                 // Check interval
    
    // State
    int geofence_count;                 // Geofence count
    int active_geofences;               // Active geofences
    int total_events;                   // Total events
    time_t last_check;                  // Last check
    
    // Geofences array
    gps_geofence_definition_t geofences[20]; // Geofences
} gps_geofence_t;

// Function prototypes

/**
 * Initialize GPS geofencing system
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_geofence_init(void);

/**
 * Create circular geofence
 * @param name Geofence name
 * @param center_lat Center latitude
 * @param center_lon Center longitude
 * @param radius_meters Radius in meters
 * @param buffer_distance Buffer distance in meters
 * @return Geofence ID on success, error code on failure
 */
int gps_geofence_create_circle(const char *name, double center_lat, double center_lon, 
                               double radius_meters, double buffer_distance);

/**
 * Create rectangular geofence
 * @param name Geofence name
 * @param min_lat Minimum latitude
 * @param max_lat Maximum latitude
 * @param min_lon Minimum longitude
 * @param max_lon Maximum longitude
 * @param buffer_distance Buffer distance in meters
 * @return Geofence ID on success, error code on failure
 */
int gps_geofence_create_rectangle(const char *name, double min_lat, double max_lat, 
                                 double min_lon, double max_lon, double buffer_distance);

/**
 * Create polygon geofence
 * @param name Geofence name
 * @param points Array of coordinate points
 * @param point_count Number of points
 * @param buffer_distance Buffer distance in meters
 * @return Geofence ID on success, error code on failure
 */
int gps_geofence_create_polygon(const char *name, const gps_coordinate_t *points, 
                                int point_count, double buffer_distance);

/**
 * Check GPS position against all geofences
 * @param gps_data GPS data to check
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_geofence_check_position(const gps_data_t *gps_data);

/**
 * Get geofence status
 * @param status Status structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_geofence_get_status(gps_geofence_system_status_t *status);

/**
 * Get geofence configuration
 * @param config Configuration structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_geofence_get_config(gps_geofence_config_t *config);

/**
 * Set geofence configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_geofence_set_config(const gps_geofence_config_t *config);

/**
 * Enable/disable geofencing
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_geofence_set_enabled(bool enabled);

/**
 * Enable/disable specific geofence
 * @param geofence_id Geofence ID
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_geofence_set_geofence_enabled(int geofence_id, bool enabled);

/**
 * Delete geofence
 * @param geofence_id Geofence ID to delete
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_geofence_delete(int geofence_id);

/**
 * Reset geofencing system
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_geofence_reset(void);

/**
 * Cleanup geofencing system
 */
void gps_geofence_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_GEOFENCE_H
