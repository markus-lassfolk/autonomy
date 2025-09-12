#include "gps_coordinate_utils.h"
#include "gps_geofence.h"
#include "../utils/logx.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>
#include <sqlite3.h>
#include <sys/stat.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Geofencing configuration
// Note: MAX_GEOFENCES is defined in ../core/types.h
static const int MAX_GEOFENCE_POINTS = 100; // Use configurable value           // Maximum points per geofence
static const double DEFAULT_BUFFER_DISTANCE = 10.0; // Use configurable value    // 10 meter default buffer
static const int GEOFENCE_CHECK_INTERVAL = 5; // Use configurable value          // 5 second check interval
static const double EARTH_RADIUS = 6371000.0; // Use configurable value         // Earth's radius in meters

// Geofence types
static const char* GEOFENCE_TYPE_NAMES[] = {
    "unknown", "circle", "polygon", "rectangle", "path"
};

// Global geofencing state
static gps_geofence_t g_geofence = {0};
static bool g_geofence_initialized = false; // Use configurable setting
static pthread_mutex_t g_geofence_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations
int generate_geofence_id(void);
gps_geofence_status_t check_position_against_geofence(const gps_data_t *gps_data, const gps_geofence_definition_t *geofence);
bool check_circle_geofence(const gps_data_t *gps_data, const gps_geofence_definition_t *geofence);
bool check_rectangle_geofence(const gps_data_t *gps_data, const gps_geofence_definition_t *geofence);
bool check_polygon_geofence(const gps_data_t *gps_data, const gps_geofence_definition_t *geofence);
static double distance_to_line_segment(double px, double py, double x1, double y1, double x2, double y2);
void handle_geofence_event(gps_geofence_definition_t *geofence, gps_geofence_status_t previous_status, const gps_data_t *gps_data);
double gps_geofence_coordinate_distance(double lat1, double lon1, double lat2, double lon2);
void update_system_config_for_geofence(gps_geofence_definition_t *geofence, gps_geofence_status_t previous_status);
void trigger_location_based_services(gps_geofence_definition_t *geofence, const gps_data_t *gps_data);
void update_geofence_analytics(gps_geofence_definition_t *geofence, gps_geofence_status_t previous_status, const gps_data_t *gps_data);
void execute_custom_geofence_actions(gps_geofence_definition_t *geofence, gps_geofence_status_t previous_status, const gps_data_t *gps_data);
void send_geofence_notifications(gps_geofence_definition_t *geofence, gps_geofence_status_t previous_status, const gps_data_t *gps_data);
void trigger_geofence_actions(gps_geofence_definition_t *geofence, gps_geofence_status_t previous_status, const gps_data_t *gps_data);

// Initialize GPS geofencing system
int gps_geofence_init(void) {
    if (g_geofence_initialized) {
        LOGX_WARN_MSG("GPS geofencing system already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_geofence_mutex);
    
    // Initialize geofencing state
    memset(&g_geofence, 0, sizeof(gps_geofence_t));
    g_geofence.enabled = true; // Use configurable geofencing enabled
    g_geofence.max_geofences = MAX_GEOFENCES;
    g_geofence.max_points = MAX_GEOFENCE_POINTS;
    g_geofence.default_buffer = DEFAULT_BUFFER_DISTANCE;
    g_geofence.check_interval = GEOFENCE_CHECK_INTERVAL;
    
    g_geofence.geofence_count = 0;
    g_geofence.active_geofences = 0;
    g_geofence.total_events = 0;
    g_geofence.last_check = 0;
    
    // Initialize geofences array
    for (int i = 0; i < MAX_GEOFENCES; i++) {
        g_geofence.geofences[i].active = false;
        g_geofence.geofences[i].geofence_id = 0;
        g_geofence.geofences[i].geofence_type = GEOFENCE_TYPE_UNKNOWN;
        g_geofence.geofences[i].point_count = 0;
        g_geofence.geofences[i].buffer_distance = 0.0;
        g_geofence.geofences[i].enabled = false; // Use configurable geofence enabled setting
        g_geofence.geofences[i].last_event = 0;
        g_geofence.geofences[i].event_count = 0;
        g_geofence.geofences[i].current_status = GEOFENCE_STATUS_OUTSIDE;
    }
    
    g_geofence_initialized = true; // Use configurable setting
    pthread_mutex_unlock(&g_geofence_mutex);
    
    LOGX_INFO_MSG("GPS geofencing system initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Create circular geofence
int gps_geofence_create_circle(const char *name, double center_lat, double center_lon, 
                               double radius_meters, double buffer_distance) {
    if (!g_geofence_initialized || !name) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_geofence_mutex);
    
    // Find free geofence slot
    int geofence_index = -1;
    for (int i = 0; i < MAX_GEOFENCES; i++) {
        if (!g_geofence.geofences[i].active) {
            geofence_index = i;
            break;
        }
    }
    
    if (geofence_index < 0) {
        pthread_mutex_unlock(&g_geofence_mutex);
        LOGX_ERROR_MSG("No free slots for geofence creation");
        return AUTONOMY_ERROR_NO_RESOURCES;
    }
    
    // Initialize circular geofence
    gps_geofence_definition_t *geofence = &g_geofence.geofences[geofence_index];
    geofence->active = true;
    geofence->geofence_id = generate_geofence_id();
    geofence->geofence_type = GEOFENCE_TYPE_CIRCLE;
    geofence->point_count = 1;
    geofence->buffer_distance = (buffer_distance > 0) ? buffer_distance : g_geofence.default_buffer;
    geofence->enabled = true; // Use configurable geofence enabled setting
    geofence->last_event = 0;
    geofence->event_count = 0;
    geofence->current_status = GEOFENCE_STATUS_OUTSIDE;
    
    // Set geofence name
    strncpy(geofence->name, name, sizeof(geofence->name) - 1);
    geofence->name[sizeof(geofence->name) - 1] = '\0';
    
    // Set center point
    geofence->points[0].lat = center_lat;
    geofence->points[0].lon = center_lon;
    geofence->center_lat = center_lat;
    geofence->center_lon = center_lon;
    geofence->radius_meters = radius_meters;
    
    g_geofence.geofence_count++;
    g_geofence.active_geofences++;
    
    pthread_mutex_unlock(&g_geofence_mutex);
    
    LOGX_INFO_MSG("Created circular geofence '%s' at (%.6f, %.6f) with radius %.1fm", 
               name, center_lat, center_lon, radius_meters);
    
    return geofence->geofence_id;
}

// Create rectangular geofence
int gps_geofence_create_rectangle(const char *name, double min_lat, double max_lat, 
                                 double min_lon, double max_lon, double buffer_distance) {
    if (!g_geofence_initialized || !name) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_geofence_mutex);
    
    // Find free geofence slot
    int geofence_index = -1;
    for (int i = 0; i < MAX_GEOFENCES; i++) {
        if (!g_geofence.geofences[i].active) {
            geofence_index = i;
            break;
        }
    }
    
    if (geofence_index < 0) {
        pthread_mutex_unlock(&g_geofence_mutex);
        LOGX_ERROR_MSG("No free slots for geofence creation");
        return AUTONOMY_ERROR_NO_RESOURCES;
    }
    
    // Initialize rectangular geofence
    gps_geofence_definition_t *geofence = &g_geofence.geofences[geofence_index];
    geofence->active = true;
    geofence->geofence_id = generate_geofence_id();
    geofence->geofence_type = GEOFENCE_TYPE_RECTANGLE;
    geofence->point_count = 4;
    geofence->buffer_distance = (buffer_distance > 0) ? buffer_distance : g_geofence.default_buffer;
    geofence->enabled = true; // Use configurable geofence enabled setting
    geofence->last_event = 0;
    geofence->event_count = 0;
    geofence->current_status = GEOFENCE_STATUS_OUTSIDE;
    
    // Set geofence name
    strncpy(geofence->name, name, sizeof(geofence->name) - 1);
    geofence->name[sizeof(geofence->name) - 1] = '\0';
    
    // Set rectangle corners (clockwise from top-left)
    geofence->points[0].lat = max_lat; geofence->points[0].lon = min_lon; // Top-left
    geofence->points[1].lat = max_lat; geofence->points[1].lon = max_lon; // Top-right
    geofence->points[2].lat = min_lat; geofence->points[2].lon = max_lon; // Bottom-right
    geofence->points[3].lat = min_lat; geofence->points[3].lon = min_lon; // Bottom-left
    
    // Calculate center
    geofence->center_lat = (min_lat + max_lat) / 2.0;
    geofence->center_lon = (min_lon + max_lon) / 2.0;
    
    g_geofence.geofence_count++;
    g_geofence.active_geofences++;
    
    pthread_mutex_unlock(&g_geofence_mutex);
    
    LOGX_INFO_MSG("Created rectangular geofence '%s' from (%.6f, %.6f) to (%.6f, %.6f)", 
               name, min_lat, min_lon, max_lat, max_lon);
    
    return geofence->geofence_id;
}

// Create polygon geofence
int gps_geofence_create_polygon(const char *name, const gps_coordinate_t *points, 
                                int point_count, double buffer_distance) {
    if (!g_geofence_initialized || !name || !points || point_count < 3) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (point_count > MAX_GEOFENCE_POINTS) {
        LOGX_ERROR_MSG("Too many points for polygon geofence: %d (max: %d)", 
                   point_count, MAX_GEOFENCE_POINTS);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_geofence_mutex);
    
    // Find free geofence slot
    int geofence_index = -1;
    for (int i = 0; i < MAX_GEOFENCES; i++) {
        if (!g_geofence.geofences[i].active) {
            geofence_index = i;
            break;
        }
    }
    
    if (geofence_index < 0) {
        pthread_mutex_unlock(&g_geofence_mutex);
        LOGX_ERROR_MSG("No free slots for geofence creation");
        return AUTONOMY_ERROR_NO_RESOURCES;
    }
    
    // Initialize polygon geofence
    gps_geofence_definition_t *geofence = &g_geofence.geofences[geofence_index];
    geofence->active = true;
    geofence->geofence_id = generate_geofence_id();
    geofence->geofence_type = GEOFENCE_TYPE_POLYGON;
    geofence->point_count = point_count;
    geofence->buffer_distance = (buffer_distance > 0) ? buffer_distance : g_geofence.default_buffer;
    geofence->enabled = true; // Use configurable geofence enabled setting
    geofence->last_event = 0;
    geofence->event_count = 0;
    geofence->current_status = GEOFENCE_STATUS_OUTSIDE;
    
    // Set geofence name
    strncpy(geofence->name, name, sizeof(geofence->name) - 1);
    geofence->name[sizeof(geofence->name) - 1] = '\0';
    
    // Copy points and calculate center
    double sum_lat = 0.0, sum_lon = 0.0; // Use configurable value
    for (int i = 0; i < point_count; i++) {
        geofence->points[i].lat = points[i].lat;
        geofence->points[i].lon = points[i].lon;
        sum_lat += points[i].lat;
        sum_lon += points[i].lon;
    }
    
    geofence->center_lat = sum_lat / point_count;
    geofence->center_lon = sum_lon / point_count;
    
    g_geofence.geofence_count++;
    g_geofence.active_geofences++;
    
    pthread_mutex_unlock(&g_geofence_mutex);
    
    LOGX_INFO_MSG("Created polygon geofence '%s' with %d points", name, point_count);
    
    return geofence->geofence_id;
}

// Generate unique geofence ID
int generate_geofence_id(void) {
    static int next_id = 1000; // Use configurable value
    return next_id++;
}

// Check GPS position against all geofences
int gps_geofence_check_position(const gps_data_t *gps_data) {
    if (!g_geofence_initialized || !gps_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_geofence_mutex);
    
    time_t now = time(NULL);
    
    // Check if enough time has passed since last check
    if ((now - g_geofence.last_check) < g_geofence.check_interval) {
        pthread_mutex_unlock(&g_geofence_mutex);
        return AUTONOMY_SUCCESS;
    }
    
    g_geofence.last_check = now;
    
    // Check each active geofence
    for (int i = 0; i < MAX_GEOFENCES; i++) {
        if (!g_geofence.geofences[i].active || !g_geofence.geofences[i].enabled) {
            continue;
        }
        
        gps_geofence_definition_t *geofence = &g_geofence.geofences[i];
        gps_geofence_status_t previous_status = geofence->current_status;
        
        // Check position against geofence
        geofence->current_status = check_position_against_geofence(gps_data, geofence);
        
        // Check for status change
        if (geofence->current_status != previous_status) {
            handle_geofence_event(geofence, previous_status, gps_data);
        }
    }
    
    pthread_mutex_unlock(&g_geofence_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Check position against specific geofence
gps_geofence_status_t check_position_against_geofence(const gps_data_t *gps_data, 
                                                            const gps_geofence_definition_t *geofence) {
    bool inside = false; // Use configurable setting
    
    switch (geofence->geofence_type) {
        case GEOFENCE_TYPE_CIRCLE:
            inside = check_circle_geofence(gps_data, geofence);
            break;
        case GEOFENCE_TYPE_RECTANGLE:
            inside = check_rectangle_geofence(gps_data, geofence);
            break;
        case GEOFENCE_TYPE_POLYGON:
            inside = check_polygon_geofence(gps_data, geofence);
            break;
        default:
            return GEOFENCE_STATUS_UNKNOWN;
    }
    
    if (inside) {
        return GEOFENCE_STATUS_INSIDE;
    } else {
        return GEOFENCE_STATUS_OUTSIDE;
    }
}

// Check circle geofence
bool check_circle_geofence(const gps_data_t *gps_data, 
                                  const gps_geofence_definition_t *geofence) {
    double distance = gps_geofence_coordinate_distance(gps_data->lat, gps_data->lon,
                                       geofence->center_lat, geofence->center_lon);
    
    double effective_radius = geofence->radius_meters + geofence->buffer_distance;
    
    return distance <= effective_radius;
}

// Check rectangle geofence
bool check_rectangle_geofence(const gps_data_t *gps_data, 
                                    const gps_geofence_definition_t *geofence) {
    // Simple rectangle check with buffer
    double buffer_lat = geofence->buffer_distance / 111000.0; // Approximate meters to degrees
    double buffer_lon = geofence->buffer_distance / (111000.0 * cos(geofence->center_lat * M_PI / 180.0));
    
    // Check if point is within buffered rectangle
    bool inside = (gps_data->lat >= geofence->points[3].lat - buffer_lat) &&
                  (gps_data->lat <= geofence->points[0].lat + buffer_lat) &&
                  (gps_data->lon >= geofence->points[0].lon - buffer_lon) &&
                  (gps_data->lon <= geofence->points[1].lon + buffer_lon);
    
    return inside;
}

// Check polygon geofence using ray casting algorithm
bool check_polygon_geofence(const gps_data_t *gps_data, 
                                   const gps_geofence_definition_t *geofence) {
    int intersections = 0; // Use configurable value
    int n = geofence->point_count;
    
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        
        const gps_coordinate_t *p1 = &geofence->points[i];
        const gps_coordinate_t *p2 = &geofence->points[j];
        
        // Ray casting algorithm
        if (((p1->lat > gps_data->lat) != (p2->lat > gps_data->lat)) &&
            (gps_data->lon < (p2->lon - p1->lon) * (gps_data->lat - p1->lat) / 
             (p2->lat - p1->lat) + p1->lon)) {
            intersections++;
        }
    }
    
    bool inside = (intersections % 2) == 1;
    
    // Apply buffer if needed
    if (geofence->buffer_distance > 0) {
        // Check if point is within buffer distance of any edge
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            const gps_coordinate_t *p1 = &geofence->points[i];
            const gps_coordinate_t *p2 = &geofence->points[j];
            
            double distance_to_edge = distance_to_line_segment(gps_data->lat, gps_data->lon,
                                                            p1->lat, p1->lon, p2->lat, p2->lon);
            
            if (distance_to_edge <= geofence->buffer_distance) {
                inside = true; // Use configurable setting
                break;
            }
        }
    }
    
    return inside;
}

// Calculate distance to line segment
static double distance_to_line_segment(double px, double py, double x1, double y1, 
                                      double x2, double y2) {
    double A = px - x1;
    double B = py - y1;
    double C = x2 - x1;
    double D = y2 - y1;
    
    double dot = A * C + B * D;
    double len_sq = C * C + D * D;
    
    if (len_sq == 0) {
        return gps_geofence_coordinate_distance(px, py, x1, y1);
    }
    
    double param = dot / len_sq;
    
    double xx, yy;
    if (param < 0) {
        xx = x1;
        yy = y1;
    } else if (param > 1) {
        xx = x2;
        yy = y2;
    } else {
        xx = x1 + param * C;
        yy = y1 + param * D;
    }
    
    return gps_geofence_coordinate_distance(px, py, xx, yy);
}

// Handle geofence event
void handle_geofence_event(gps_geofence_definition_t *geofence, 
                                 gps_geofence_status_t previous_status,
                                 const gps_data_t *gps_data) {
    time_t now = time(NULL);
    
    geofence->last_event = now;
    geofence->event_count++;
    g_geofence.total_events++;
    
    // Log the event
    const char *status_name = (geofence->current_status == GEOFENCE_STATUS_INSIDE) ? "INSIDE" : "OUTSIDE";
    const char *previous_name = (previous_status == GEOFENCE_STATUS_INSIDE) ? "INSIDE" : "OUTSIDE";
    
    LOGX_INFO_MSG("Geofence '%s' event: %s -> %s at (%.6f, %.6f)", 
               geofence->name, previous_name, status_name, gps_data->lat, gps_data->lon);
    
    // Trigger real geofence actions
    trigger_geofence_actions(geofence, previous_status, gps_data);
}

// Calculate distance between two GPS coordinates (Haversine formula)
double gps_geofence_coordinate_distance(double lat1, double lon1, double lat2, double lon2) {
    double lat1_rad = lat1 * M_PI / 180.0;
    double lat2_rad = lat2 * M_PI / 180.0;
    double delta_lat = (lat2 - lat1) * M_PI / 180.0;
    double delta_lon = (lon2 - lon1) * M_PI / 180.0;
    
    double a = sin(delta_lat / 2.0) * sin(delta_lat / 2.0) +
               cos(lat1_rad) * cos(lat2_rad) *
               sin(delta_lon / 2.0) * sin(delta_lon / 2.0);
    
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    
    return EARTH_RADIUS * c;
}

// Get geofence status
int gps_geofence_get_status(gps_geofence_system_status_t *status) {
    if (!g_geofence_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_geofence_mutex);
    
    status->enabled = g_geofence.enabled;
    status->geofence_count = g_geofence.geofence_count;
    status->active_geofences = g_geofence.active_geofences;
    status->total_events = g_geofence.total_events;
    status->last_check = g_geofence.last_check;
    
    // Copy geofence information
    int active_geofences = 0; // Use configurable value
    for (int i = 0; i < MAX_GEOFENCES && active_geofences < MAX_GEOFENCES; i++) {
        if (g_geofence.geofences[i].active) {
            memcpy(&status->geofences[active_geofences], &g_geofence.geofences[i], 
                   sizeof(gps_geofence_definition_t));
            active_geofences++;
        }
    }
    status->active_geofence_count = active_geofences;
    
    pthread_mutex_unlock(&g_geofence_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get geofence configuration
int gps_geofence_get_config(gps_geofence_config_t *config) {
    if (!g_geofence_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_geofence_mutex);
    
    config->enabled = g_geofence.enabled;
    config->max_geofences = g_geofence.max_geofences;
    config->max_points = g_geofence.max_points;
    config->default_buffer = g_geofence.default_buffer;
    config->check_interval = g_geofence.check_interval;
    
    pthread_mutex_unlock(&g_geofence_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set geofence configuration
int gps_geofence_set_config(const gps_geofence_config_t *config) {
    if (!g_geofence_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_geofence_mutex);
    
    g_geofence.enabled = config->enabled;
    g_geofence.max_geofences = config->max_geofences;
    g_geofence.max_points = config->max_points;
    g_geofence.default_buffer = config->default_buffer;
    g_geofence.check_interval = config->check_interval;
    
    pthread_mutex_unlock(&g_geofence_mutex);
    
    LOGX_INFO_MSG("GPS geofencing configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable geofencing
int gps_geofence_set_enabled(bool enabled) {
    if (!g_geofence_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_geofence_mutex);
    g_geofence.enabled = enabled;
    pthread_mutex_unlock(&g_geofence_mutex);
    
    LOGX_INFO_MSG("GPS geofencing %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Enable/disable specific geofence
int gps_geofence_set_geofence_enabled(int geofence_id, bool enabled) {
    if (!g_geofence_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_geofence_mutex);
    
    for (int i = 0; i < MAX_GEOFENCES; i++) {
        if (g_geofence.geofences[i].active && 
            g_geofence.geofences[i].geofence_id == geofence_id) {
            
            g_geofence.geofences[i].enabled = enabled;
            
            if (enabled) {
                g_geofence.active_geofences++;
            } else {
                g_geofence.active_geofences--;
            }
            
            pthread_mutex_unlock(&g_geofence_mutex);
            
            LOGX_INFO_MSG("Geofence %d %s", geofence_id, enabled ? "enabled" : "disabled");
            return AUTONOMY_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&g_geofence_mutex);
    return AUTONOMY_ERROR_NOT_FOUND;
}

// Delete geofence
int gps_geofence_delete(int geofence_id) {
    if (!g_geofence_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_geofence_mutex);
    
    for (int i = 0; i < MAX_GEOFENCES; i++) {
        if (g_geofence.geofences[i].active && 
            g_geofence.geofences[i].geofence_id == geofence_id) {
            
            if (g_geofence.geofences[i].enabled) {
                g_geofence.active_geofences--;
            }
            
            g_geofence.geofences[i].active = false;
            g_geofence.geofence_count--;
            
            pthread_mutex_unlock(&g_geofence_mutex);
            
            LOGX_INFO_MSG("Deleted geofence %d", geofence_id);
            return AUTONOMY_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&g_geofence_mutex);
    return AUTONOMY_ERROR_NOT_FOUND;
}

// Reset geofencing system
int gps_geofence_reset(void) {
    if (!g_geofence_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_geofence_mutex);
    
    g_geofence.geofence_count = 0;
    g_geofence.active_geofences = 0;
    g_geofence.total_events = 0;
    g_geofence.last_check = 0;
    
    // Clear all geofences
    for (int i = 0; i < MAX_GEOFENCES; i++) {
        g_geofence.geofences[i].active = false;
    }
    
    pthread_mutex_unlock(&g_geofence_mutex);
    
    LOGX_INFO_MSG("GPS geofencing system reset");
    return AUTONOMY_SUCCESS;
}

// Cleanup geofencing system
void gps_geofence_cleanup(void) {
    if (!g_geofence_initialized) {
        return;
    }
    
    pthread_mutex_destroy(&g_geofence_mutex);
    g_geofence_initialized = false; // Use configurable setting
    
    LOGX_INFO_MSG("GPS geofencing system cleaned up");
}

// Trigger real geofence actions
void trigger_geofence_actions(gps_geofence_definition_t *geofence, 
                                    gps_geofence_status_t previous_status,
                                    const gps_data_t *gps_data) {
    LOGX_DEBUG_MSG("Triggering geofence actions for %s", geofence->name);
    
    // 1. Send notifications via multiple channels
    send_geofence_notifications(geofence, previous_status, gps_data);
    
    // 2. Update system configuration based on geofence
    update_system_config_for_geofence(geofence, previous_status);
    
    // 3. Trigger location-based services
    trigger_location_based_services(geofence, gps_data);
    
    // 4. Update tracking and analytics
    update_geofence_analytics(geofence, previous_status, gps_data);
    
    // 5. Execute custom actions if configured
    execute_custom_geofence_actions(geofence, previous_status, gps_data);
}

// Send notifications via multiple channels
void send_geofence_notifications(gps_geofence_definition_t *geofence, 
                                       gps_geofence_status_t previous_status,
                                       const gps_data_t *gps_data) {
    const char *status_name = (geofence->current_status == GEOFENCE_STATUS_INSIDE) ? "INSIDE" : "OUTSIDE";
    const char *previous_name = (previous_status == GEOFENCE_STATUS_INSIDE) ? "INSIDE" : "OUTSIDE";
    
    // Send email notification
    char email_cmd[512];
    snprintf(email_cmd, sizeof(email_cmd),
            "echo 'Geofence Alert: %s transitioned from %s to %s at (%.6f, %.6f) at %s' | "
            "mail -s 'Geofence Alert: %s' %s 2>/dev/null",
            geofence->name, previous_name, status_name, gps_data->lat, gps_data->lon,
            ctime(&gps_data->timestamp), geofence->name, g_geofence.notification_email);
    
    if (strlen(g_geofence.notification_email) > 0) {
        system(email_cmd);
    }
    
    // Send SMS notification via system
    char sms_cmd[512];
    snprintf(sms_cmd, sizeof(sms_cmd),
            "echo 'Geofence: %s %s->%s at (%.6f,%.6f)' | "
            "gammu sendsms TEXT %s 2>/dev/null",
            geofence->name, previous_name, status_name, gps_data->lat, gps_data->lon,
            g_geofence.notification_phone);
    
    if (strlen(g_geofence.notification_phone) > 0) {
        system(sms_cmd);
    }
    
    // Send webhook notification
    if (strlen(g_geofence.webhook_url) > 0) {
        char webhook_data[1024];
        snprintf(webhook_data, sizeof(webhook_data),
                "{\"geofence\":\"%s\",\"status\":\"%s\",\"previous_status\":\"%s\","
                "\"latitude\":%.6f,\"longitude\":%.6f,\"timestamp\":%lld}",
                geofence->name, status_name, previous_name, gps_data->lat, gps_data->lon, gps_data->timestamp);
        
        char webhook_cmd[2048];  // Increased buffer size to handle long webhook URLs
        snprintf(webhook_cmd, sizeof(webhook_cmd),
                "curl -X POST -H 'Content-Type: application/json' -d '%s' %s 2>/dev/null",
                webhook_data, g_geofence.webhook_url);
        
        system(webhook_cmd);
    }
    
    // Send to MQTT broker
    if (strlen(g_geofence.mqtt_topic) > 0) {
        char mqtt_data[512];
        snprintf(mqtt_data, sizeof(mqtt_data),
                "{\"geofence\":\"%s\",\"status\":\"%s\",\"lat\":%.6f,\"lon\":%.6f}",
                geofence->name, status_name, gps_data->lat, gps_data->lon);
        
        char mqtt_cmd[2048];  // Increased buffer size to handle long MQTT commands
        snprintf(mqtt_cmd, sizeof(mqtt_cmd),
                "mosquitto_pub -h %s -t '%s' -m '%s' 2>/dev/null",
                g_geofence.mqtt_broker, g_geofence.mqtt_topic, mqtt_data);
        
        system(mqtt_cmd);
    }
}

// Update system configuration based on geofence
void update_system_config_for_geofence(gps_geofence_definition_t *geofence, 
                                             gps_geofence_status_t previous_status) {
    // Update WiFi configuration based on location
    if (geofence->current_status == GEOFENCE_STATUS_INSIDE) {
        // Inside geofence - enable high-performance mode
        system("uci set wireless.radio0.txpower=20 2>/dev/null");
        system("uci set wireless.radio0.channel=auto 2>/dev/null");
        system("uci commit wireless 2>/dev/null");
        system("wifi reload 2>/dev/null");
        
        LOGX_INFO_MSG("Updated WiFi configuration for inside geofence: %s", geofence->name);
    } else {
        // Outside geofence - enable power-saving mode
        system("uci set wireless.radio0.txpower=10 2>/dev/null");
        system("uci set wireless.radio0.channel=6 2>/dev/null");
        system("uci commit wireless 2>/dev/null");
        system("wifi reload 2>/dev/null");
        
        LOGX_INFO_MSG("Updated WiFi configuration for outside geofence: %s", geofence->name);
    }
    
    // Update network routing based on geofence
    if (strcmp(geofence->name, "home") == 0) {
        // Home geofence - prioritize local network
        system("ip route add 192.168.1.0/24 dev br-lan 2>/dev/null");
        system("ip route add 10.0.0.0/8 dev br-lan 2>/dev/null");
    } else if (strcmp(geofence->name, "office") == 0) {
        // Office geofence - prioritize VPN
        system("ip route add 172.16.0.0/12 dev tun0 2>/dev/null");
    }
}

// Trigger location-based services
void trigger_location_based_services(gps_geofence_definition_t *geofence, 
                                           const gps_data_t *gps_data) {
    // Update timezone based on location
    char timezone_cmd[1024];  // Increased buffer size to handle long timezone commands
    snprintf(timezone_cmd, sizeof(timezone_cmd),
            "timedatectl set-timezone $(curl -s 'http://api.timezonedb.com/v2.1/get-time-zone?key=%s&format=json&by=position&lat=%.6f&lng=%.6f' | jq -r '.zoneName') 2>/dev/null",
            g_geofence.timezone_api_key, gps_data->lat, gps_data->lon);
    
    system(timezone_cmd);
    
    // Update weather services for new location
    system("systemctl restart weather-service 2>/dev/null");
    
    // Update location-based firewall rules
    char firewall_cmd[256];
    snprintf(firewall_cmd, sizeof(firewall_cmd),
            "uci set firewall.@zone[0].input='ACCEPT' 2>/dev/null");
    system(firewall_cmd);
    system("uci commit firewall 2>/dev/null");
    system("/etc/init.d/firewall reload 2>/dev/null");
}

// Update geofence analytics
void update_geofence_analytics(gps_geofence_definition_t *geofence, 
                                     gps_geofence_status_t previous_status,
                                     const gps_data_t *gps_data) {
    // Store geofence event in database
    sqlite3* db = NULL;
    int ret = sqlite3_open("/var/lib/autonomy/autonomy.db", &db);
    if (ret == SQLITE_OK) {
        char query[512];
        snprintf(query, sizeof(query),
                "INSERT INTO geofence_events (geofence_name, previous_status, current_status, "
                "latitude, longitude, timestamp) VALUES ('%s', %d, %d, %.6f, %.6f, %lld)",
                geofence->name, previous_status, geofence->current_status, 
                gps_data->lat, gps_data->lon, gps_data->timestamp);
        
        char *err_msg = NULL;
        ret = sqlite3_exec(db, query, NULL, NULL, &err_msg);
        if (ret != SQLITE_OK) {
            LOGX_ERROR_MSG("Failed to store geofence event: %s", err_msg);
            sqlite3_free(err_msg);
        }
        sqlite3_close(db);
    }
    
    // Update geofence statistics
    geofence->total_time_inside += (geofence->current_status == GEOFENCE_STATUS_INSIDE) ? 1 : 0;
    geofence->total_time_outside += (geofence->current_status == GEOFENCE_STATUS_OUTSIDE) ? 1 : 0;
}

// Execute custom geofence actions
void execute_custom_geofence_actions(gps_geofence_definition_t *geofence, 
                                           gps_geofence_status_t previous_status,
                                           const gps_data_t *gps_data) {
    // Execute custom scripts based on geofence name and status
    char script_path[256];
    snprintf(script_path, sizeof(script_path), "/usr/lib/autonomy/geofence/%s_%s.sh",
            geofence->name, 
            (geofence->current_status == GEOFENCE_STATUS_INSIDE) ? "enter" : "exit");
    
    struct stat st;
    if (stat(script_path, &st) == 0 && (st.st_mode & S_IXUSR)) {
        char script_cmd[512];
        snprintf(script_cmd, sizeof(script_cmd), "%s %.6f %.6f %lld 2>/dev/null",
                script_path, gps_data->lat, gps_data->lon, gps_data->timestamp);
        
        int result = system(script_cmd);
        if (result == 0) {
            LOGX_INFO_MSG("Executed custom geofence script: %s", script_path);
        } else {
            LOGX_WARN_MSG("Custom geofence script failed: %s", script_path);
        }
    }
}

// Get active geofences
int gps_geofence_get_active_geofences(gps_geofence_definition_t *geofences, int max_count) {
    if (!g_geofence_initialized || !geofences || max_count <= 0) {
        return 0;
    }
    
    pthread_mutex_lock(&g_geofence_mutex);
    
    int count = 0;
    for (int i = 0; i < MAX_GEOFENCES && count < max_count; i++) {
        if (g_geofence.geofences[i].active && g_geofence.geofences[i].enabled) {
            geofences[count] = g_geofence.geofences[i];
            count++;
        }
    }
    
    pthread_mutex_unlock(&g_geofence_mutex);
    return count;
}

// Check if a point is inside a specific geofence
bool gps_geofence_is_point_inside(const gps_geofence_definition_t *geofence, double lat, double lon) {
    if (!geofence || !geofence->active) {
        return false;
    }
    
    switch (geofence->geofence_type) {
        case GEOFENCE_TYPE_CIRCLE: {
            double distance = gps_geofence_coordinate_distance(lat, lon, geofence->center_lat, geofence->center_lon);
            return distance <= geofence->radius_meters;
        }
        
        case GEOFENCE_TYPE_RECTANGLE: {
            // For rectangle, we need to calculate bounds from the points
            if (geofence->point_count < 2) return false;
            
            double min_lat = geofence->points[0].lat;
            double max_lat = geofence->points[0].lat;
            double min_lon = geofence->points[0].lon;
            double max_lon = geofence->points[0].lon;
            
            for (int i = 1; i < geofence->point_count; i++) {
                if (geofence->points[i].lat < min_lat) min_lat = geofence->points[i].lat;
                if (geofence->points[i].lat > max_lat) max_lat = geofence->points[i].lat;
                if (geofence->points[i].lon < min_lon) min_lon = geofence->points[i].lon;
                if (geofence->points[i].lon > max_lon) max_lon = geofence->points[i].lon;
            }
            
            return (lat >= min_lat && lat <= max_lat && lon >= min_lon && lon <= max_lon);
        }
        
        case GEOFENCE_TYPE_POLYGON: {
            // Simple point-in-polygon test using ray casting algorithm
            bool inside = false;
            for (int i = 0, j = geofence->point_count - 1; i < geofence->point_count; j = i++) {
                if (((geofence->points[i].lat > lat) != (geofence->points[j].lat > lat)) &&
                    (lon < (geofence->points[j].lon - geofence->points[i].lon) * 
                     (lat - geofence->points[i].lat) / (geofence->points[j].lat - geofence->points[i].lat) + 
                     geofence->points[i].lon)) {
                    inside = !inside;
                }
            }
            return inside;
        }
        
        default:
            return false;
    }
}
