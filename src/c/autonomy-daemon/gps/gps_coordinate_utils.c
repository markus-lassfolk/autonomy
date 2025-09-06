#include "gps_coordinate_utils.h"
#include "../utils/logx.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

// Coordinate utilities configuration
static const double EARTH_RADIUS_METERS = 6371000.0;      // Earth's radius in meters
static const double EARTH_RADIUS_KM = 6371.0;             // Earth's radius in kilometers
static const double DEG_TO_RAD = M_PI / 180.0;            // Degrees to radians conversion
static const double RAD_TO_DEG = 180.0 / M_PI;            // Radians to degrees conversion
static const double MIN_LAT = -90.0;                       // Minimum latitude
static const double MAX_LAT = 90.0;                        // Maximum latitude
static const double MIN_LON = -180.0;                      // Minimum longitude
static const double MAX_LON = 180.0;                       // Maximum longitude

// Initialize GPS coordinate utilities
int gps_coordinate_utils_init(void) {
    LOGX_INFO_MSG("GPS coordinate utilities initialized");
    return AUTONOMY_SUCCESS;
}

// Calculate distance between two GPS coordinates (Haversine formula)
double gps_coordinate_distance(double lat1, double lon1, double lat2, double lon2) {
    // Convert to radians
    double lat1_rad = lat1 * DEG_TO_RAD;
    double lat2_rad = lat2 * DEG_TO_RAD;
    double delta_lat = (lat2 - lat1) * DEG_TO_RAD;
    double delta_lon = (lon2 - lon1) * DEG_TO_RAD;
    
    // Haversine formula
    double a = sin(delta_lat / 2.0) * sin(delta_lat / 2.0) +
               cos(lat1_rad) * cos(lat2_rad) *
               sin(delta_lon / 2.0) * sin(delta_lon / 2.0);
    
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    
    return EARTH_RADIUS_METERS * c;
}

// Calculate distance in kilometers
double gps_coordinate_distance_km(double lat1, double lon1, double lat2, double lon2) {
    return gps_coordinate_distance(lat1, lon1, lat2, lon2) / 1000.0;
}

// Calculate bearing between two GPS coordinates
double gps_coordinate_bearing(double lat1, double lon1, double lat2, double lon2) {
    // Convert to radians
    double lat1_rad = lat1 * DEG_TO_RAD;
    double lat2_rad = lat2 * DEG_TO_RAD;
    double delta_lon = (lon2 - lon1) * DEG_TO_RAD;
    
    // Calculate bearing
    double y = sin(delta_lon) * cos(lat2_rad);
    double x = cos(lat1_rad) * sin(lat2_rad) -
               sin(lat1_rad) * cos(lat2_rad) * cos(delta_lon);
    
    double bearing = atan2(y, x) * RAD_TO_DEG;
    
    // Normalize to 0-360 degrees
    bearing = fmod(bearing + 360.0, 360.0);
    
    return bearing;
}

// Calculate destination point given start point, bearing, and distance
int gps_coordinate_destination(double start_lat, double start_lon, double bearing, 
                              double distance_meters, double *dest_lat, double *dest_lon) {
    if (!dest_lat || !dest_lon) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Convert to radians
    double lat1_rad = start_lat * DEG_TO_RAD;
    double lon1_rad = start_lon * DEG_TO_RAD;
    double bearing_rad = bearing * DEG_TO_RAD;
    double angular_distance = distance_meters / EARTH_RADIUS_METERS;
    
    // Calculate destination coordinates
    double lat2_rad = asin(sin(lat1_rad) * cos(angular_distance) +
                           cos(lat1_rad) * sin(angular_distance) * cos(bearing_rad));
    
    double lon2_rad = lon1_rad + atan2(sin(bearing_rad) * sin(angular_distance) * cos(lat1_rad),
                                       cos(angular_distance) - sin(lat1_rad) * sin(lat2_rad));
    
    // Convert back to degrees
    *dest_lat = lat2_rad * RAD_TO_DEG;
    *dest_lon = lon2_rad * RAD_TO_DEG;
    
    // Normalize longitude to -180 to 180
    *dest_lon = fmod(*dest_lon + 540.0, 360.0) - 180.0;
    
    return AUTONOMY_SUCCESS;
}

// Calculate midpoint between two GPS coordinates
int gps_coordinate_midpoint(double lat1, double lon1, double lat2, double lon2, 
                           double *mid_lat, double *mid_lon) {
    if (!mid_lat || !mid_lon) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Convert to radians
    double lat1_rad = lat1 * DEG_TO_RAD;
    double lat2_rad = lat2 * DEG_TO_RAD;
    double lon1_rad = lon1 * DEG_TO_RAD;
    double lon2_rad = lon2 * DEG_TO_RAD;
    
    // Calculate midpoint
    double Bx = cos(lat2_rad) * cos(lon2_rad - lon1_rad);
    double By = cos(lat2_rad) * sin(lon2_rad - lon1_rad);
    
    double mid_lat_rad = atan2(sin(lat1_rad) + sin(lat2_rad),
                               sqrt((cos(lat1_rad) + Bx) * (cos(lat1_rad) + Bx) + By * By));
    
    double mid_lon_rad = lon1_rad + atan2(By, cos(lat1_rad) + Bx);
    
    // Convert back to degrees
    *mid_lat = mid_lat_rad * RAD_TO_DEG;
    *mid_lon = mid_lon_rad * RAD_TO_DEG;
    
    // Normalize longitude to -180 to 180
    *mid_lon = fmod(*mid_lon + 540.0, 360.0) - 180.0;
    
    return AUTONOMY_SUCCESS;
}

// Calculate area of a polygon defined by GPS coordinates
double gps_coordinate_polygon_area(const gps_coordinate_t *coordinates, int num_coordinates) {
    if (!coordinates || num_coordinates < 3) {
        return 0.0;
    }
    
    double area = 0.0;
    
    for (int i = 0; i < num_coordinates; i++) {
        int j = (i + 1) % num_coordinates;
        
        double lat1_rad = coordinates[i].lat * DEG_TO_RAD;
        double lon1_rad = coordinates[i].lon * DEG_TO_RAD;
        double lat2_rad = coordinates[j].lat * DEG_TO_RAD;
        double lon2_rad = coordinates[j].lon * DEG_TO_RAD;
        
        area += (lon2_rad - lon1_rad) * (2.0 + sin(lat1_rad) + sin(lat2_rad));
    }
    
    area = fabs(area) * EARTH_RADIUS_METERS * EARTH_RADIUS_METERS / 2.0;
    
    return area;
}

// Check if a point is inside a polygon (ray casting algorithm)
bool gps_coordinate_point_in_polygon(double lat, double lon, const gps_coordinate_t *polygon, int num_vertices) {
    if (!polygon || num_vertices < 3) {
        return false;
    }
    
    bool inside = false;
    
    for (int i = 0, j = num_vertices - 1; i < num_vertices; j = i++) {
        if (((polygon[i].lat > lat) != (polygon[j].lat > lat)) &&
            (lon < (polygon[j].lon - polygon[i].lon) * (lat - polygon[i].lat) / 
                    (polygon[j].lat - polygon[i].lat) + polygon[i].lon)) {
            inside = !inside;
        }
    }
    
    return inside;
}

// Calculate the centroid of a polygon
int gps_coordinate_polygon_centroid(const gps_coordinate_t *coordinates, int num_coordinates, 
                                   double *centroid_lat, double *centroid_lon) {
    if (!coordinates || num_coordinates < 3 || !centroid_lat || !centroid_lon) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    double area = 0.0;
    double centroid_x = 0.0;
    double centroid_y = 0.0;
    
    for (int i = 0; i < num_coordinates; i++) {
        int j = (i + 1) % num_coordinates;
        
        double cross = coordinates[i].lon * coordinates[j].lat - coordinates[j].lon * coordinates[i].lat;
        area += cross;
        
        centroid_x += (coordinates[i].lon + coordinates[j].lon) * cross;
        centroid_y += (coordinates[i].lat + coordinates[j].lat) * cross;
    }
    
    area *= 0.5;
    
    if (fabs(area) < 1e-10) {
        // Degenerate polygon
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    centroid_x /= (6.0 * area);
    centroid_y /= (6.0 * area);
    
    *centroid_lon = centroid_x;
    *centroid_lat = centroid_y;
    
    return AUTONOMY_SUCCESS;
}

// Convert decimal degrees to degrees, minutes, seconds format
void gps_coordinate_decimal_to_dms(double decimal_degrees, int *degrees, int *minutes, double *seconds) {
    if (!degrees || !minutes || !seconds) {
        return;
    }
    
    *degrees = (int)decimal_degrees;
    double minutes_decimal = fabs(decimal_degrees - *degrees) * 60.0;
    *minutes = (int)minutes_decimal;
    *seconds = (minutes_decimal - *minutes) * 60.0;
}

// Convert degrees, minutes, seconds to decimal degrees
double gps_coordinate_dms_to_decimal(int degrees, int minutes, double seconds) {
    double decimal = (double)degrees;
    
    if (degrees >= 0) {
        decimal += (double)minutes / 60.0 + seconds / 3600.0;
    } else {
        decimal -= (double)minutes / 60.0 + seconds / 3600.0;
    }
    
    return decimal;
}

// Validate GPS coordinates
bool gps_coordinate_is_valid(double lat, double lon) {
    return (lat >= MIN_LAT && lat <= MAX_LAT && 
            lon >= MIN_LON && lon <= MAX_LON);
}

// Normalize longitude to -180 to 180 range
double gps_coordinate_normalize_lon(double lon) {
    while (lon > 180.0) lon -= 360.0;
    while (lon < -180.0) lon += 360.0;
    return lon;
}

// Normalize latitude to -90 to 90 range
double gps_coordinate_normalize_lat(double lat) {
    if (lat > 90.0) lat = 90.0;
    if (lat < -90.0) lat = -90.0;
    return lat;
}

// Calculate great circle distance (spherical approximation)
double gps_coordinate_great_circle_distance(double lat1, double lon1, double lat2, double lon2) {
    double lat1_rad = lat1 * DEG_TO_RAD;
    double lat2_rad = lat2 * DEG_TO_RAD;
    double delta_lon = (lon2 - lon1) * DEG_TO_RAD;
    
    double cos_delta_lon = cos(delta_lon);
    double cos_lat1 = cos(lat1_rad);
    double cos_lat2 = cos(lat2_rad);
    double sin_lat1 = sin(lat1_rad);
    double sin_lat2 = sin(lat2_rad);
    
    double cos_angle = sin_lat1 * sin_lat2 + cos_lat1 * cos_lat2 * cos_delta_lon;
    
    // Handle floating point precision issues
    if (cos_angle > 1.0) cos_angle = 1.0;
    if (cos_angle < -1.0) cos_angle = -1.0;
    
    double angle = acos(cos_angle);
    
    return EARTH_RADIUS_METERS * angle;
}

// Calculate rhumb line distance and bearing
int gps_coordinate_rhumb_line(double lat1, double lon1, double lat2, double lon2, 
                             double *distance, double *bearing) {
    if (!distance || !bearing) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    double lat1_rad = lat1 * DEG_TO_RAD;
    double lat2_rad = lat2 * DEG_TO_RAD;
    double delta_lat = lat2_rad - lat1_rad;
    double delta_lon = (lon2 - lon1) * DEG_TO_RAD;
    
    // Calculate rhumb line bearing
    double lat_avg = (lat1_rad + lat2_rad) / 2.0;
    double q = cos(lat_avg);
    
    if (fabs(q) < 1e-10) {
        // At poles, rhumb line becomes a meridian
        *bearing = (delta_lat > 0) ? 0.0 : 180.0;
        *distance = EARTH_RADIUS_METERS * fabs(delta_lat);
    } else {
        *bearing = atan2(delta_lon, delta_lat) * RAD_TO_DEG;
        if (*bearing < 0) *bearing += 360.0;
        
        // Calculate rhumb line distance
        double lat_diff = log(tan(lat2_rad / 2.0 + M_PI / 4.0) / 
                              tan(lat1_rad / 2.0 + M_PI / 4.0));
        double q_squared = q * q;
        double distance_lat = EARTH_RADIUS_METERS * delta_lat;
        double distance_lon = EARTH_RADIUS_METERS * delta_lon * q;
        
        *distance = sqrt(distance_lat * distance_lat + distance_lon * distance_lon);
    }
    
    return AUTONOMY_SUCCESS;
}

// Calculate intersection of two great circles
int gps_coordinate_great_circle_intersection(double lat1, double lon1, double bearing1,
                                            double lat2, double lon2, double bearing2,
                                            double *intersect_lat, double *intersect_lon) {
    if (!intersect_lat || !intersect_lon) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    double lat1_rad = lat1 * DEG_TO_RAD;
    double lon1_rad = lon1 * DEG_TO_RAD;
    double lat2_rad = lat2 * DEG_TO_RAD;
    double lon2_rad = lon2 * DEG_TO_RAD;
    double bearing1_rad = bearing1 * DEG_TO_RAD;
    double bearing2_rad = bearing2 * DEG_TO_RAD;
    
    // Calculate great circle intersection
    double cos_lat1 = cos(lat1_rad);
    double sin_lat1 = sin(lat1_rad);
    double cos_bearing1 = cos(bearing1_rad);
    double sin_bearing1 = sin(bearing1_rad);
    
    double cos_lat2 = cos(lat2_rad);
    double sin_lat2 = sin(lat2_rad);
    double cos_bearing2 = cos(bearing2_rad);
    double sin_bearing2 = sin(bearing2_rad);
    
    // Calculate intersection point
    double x = cos_lat1 * cos_bearing1 * sin_lat2 - sin_lat1 * cos_lat2 * cos_bearing2;
    double y = cos_lat1 * sin_bearing1 * sin_lat2 - sin_lat1 * cos_lat2 * sin_bearing2;
    double z = cos_lat1 * cos_lat2 * sin(bearing1_rad - bearing2_rad);
    
    double lat_intersect_rad = atan2(z, sqrt(x * x + y * y));
    double lon_intersect_rad = atan2(y, x);
    
    *intersect_lat = lat_intersect_rad * RAD_TO_DEG;
    *intersect_lon = lon_intersect_rad * RAD_TO_DEG;
    
    // Normalize longitude
    *intersect_lon = gps_coordinate_normalize_lon(*intersect_lon);
    
    return AUTONOMY_SUCCESS;
}

// Calculate the closest point on a line segment to a given point
int gps_coordinate_closest_point_on_line(double point_lat, double point_lon,
                                        double line_lat1, double line_lon1,
                                        double line_lat2, double line_lon2,
                                        double *closest_lat, double *closest_lon) {
    if (!closest_lat || !closest_lon) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Convert to radians
    double p_lat_rad = point_lat * DEG_TO_RAD;
    double p_lon_rad = point_lon * DEG_TO_RAD;
    double l1_lat_rad = line_lat1 * DEG_TO_RAD;
    double l1_lon_rad = line_lon1 * DEG_TO_RAD;
    double l2_lat_rad = line_lat2 * DEG_TO_RAD;
    double l2_lon_rad = line_lon2 * DEG_TO_RAD;
    
    // Calculate line segment vector
    double dlat = l2_lat_rad - l1_lat_rad;
    double dlon = l2_lon_rad - l1_lon_rad;
    
    // Calculate parameter t for closest point
    double t = ((p_lat_rad - l1_lat_rad) * dlat + (p_lon_rad - l1_lon_rad) * dlon) /
               (dlat * dlat + dlon * dlon);
    
    // Clamp t to [0, 1] to ensure point is on line segment
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    
    // Calculate closest point
    *closest_lat = (l1_lat_rad + t * dlat) * RAD_TO_DEG;
    *closest_lon = (l1_lon_rad + t * dlon) * RAD_TO_DEG;
    
    return AUTONOMY_SUCCESS;
}

// Cleanup GPS coordinate utilities
void gps_coordinate_utils_cleanup(void) {
    LOGX_INFO_MSG("GPS coordinate utilities cleaned up");
}
