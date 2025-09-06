#include "gps_geofence.h"
#include "../utils/logx.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

// Geofencing configuration
static const int MAX_GEOFENCES = 20;                  // Maximum number of geofences
static const int MAX_GEOFENCE_POINTS = 100;           // Maximum points per geofence
static const double DEFAULT_BUFFER_DISTANCE = 10.0;    // 10 meter default buffer
static const int GEOFENCE_CHECK_INTERVAL = 5;          // 5 second check interval
static const double EARTH_RADIUS = 6371000.0;         // Earth's radius in meters

// Geofence types
static const char* GEOFENCE_TYPE_NAMES[] = {
    "unknown", "circle", "polygon", "rectangle", "path"
};

// Global geofencing state
static gps_geofence_t g_geofence = {0};
static bool g_geofence_initialized = false;
static pthread_mutex_t g_geofence_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize GPS geofencing system
int gps_geofence_init(void) {
    if (g_geofence_initialized) {
        LOGX_WARN_MSG("GPS geofencing system already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_geofence_mutex);
    
    // Initialize geofencing state
    memset(&g_geofence, 0, sizeof(gps_geofence_t));
    g_geofence.enabled = true;
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
        g_geofence.geofence.geofences[i].buffer_distance = 0.0;
        g_geofence.geofences[i].enabled = false;
        g_geofence.geofences[i].last_event = 0;
        g_geofence.geofences[i].event_count = 0;
        g_geofence.geofences[i].current_status = GEOFENCE_STATUS_OUTSIDE;
    }
    
    g_geofence_initialized = true;
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
    geofence->enabled = true;
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
    geofence->enabled = true;
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
    geofence->enabled = true;
    geofence->last_event = 0;
    geofence->event_count = 0;
    geofence->current_status = GEOFENCE_STATUS_OUTSIDE;
    
    // Set geofence name
    strncpy(geofence->name, name, sizeof(geofence->name) - 1);
    geofence->name[sizeof(geofence->name) - 1] = '\0';
    
    // Copy points and calculate center
    double sum_lat = 0.0, sum_lon = 0.0;
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
static int generate_geofence_id(void) {
    static int next_id = 1000;
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
    
    pthread_mutex_unlock(&g_fusion_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Check position against specific geofence
static gps_geofence_status_t check_position_against_geofence(const gps_data_t *gps_data, 
                                                            const gps_geofence_definition_t *geofence) {
    bool inside = false;
    
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
static bool check_circle_geofence(const gps_data_t *gps_data, 
                                  const gps_geofence_definition_t *geofence) {
    double distance = calculate_distance(gps_data->lat, gps_data->lon,
                                       geofence->center_lat, geofence->center_lon);
    
    double effective_radius = geofence->radius_meters + geofence->buffer_distance;
    
    return distance <= effective_radius;
}

// Check rectangle geofence
static bool check_rectangle_geofence(const gps_data_t *gps_data, 
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
static bool check_polygon_geofence(const gps_data_t *gps_data, 
                                   const gps_geofence_definition_t *geofence) {
    int intersections = 0;
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
                inside = true;
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
        return calculate_distance(px, py, x1, y1);
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
    
    return calculate_distance(px, py, xx, yy);
}

// Handle geofence event
static void handle_geofence_event(gps_geofence_definition_t *geofence, 
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
    
    // Here you could trigger callbacks, notifications, or other actions
    // For now, we just log the event
}

// Calculate distance between two GPS coordinates (Haversine formula)
static double calculate_distance(double lat1, double lon1, double lat2, double lon2) {
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
int gps_geofence_get_status(gps_geofence_status_t *status) {
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
    int active_geofences = 0;
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
    g_geofence_initialized = false;
    
    LOGX_INFO_MSG("GPS geofencing system cleaned up");
}
