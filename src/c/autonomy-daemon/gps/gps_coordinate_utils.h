#ifndef GPS_COORDINATE_UTILS_H
#define GPS_COORDINATE_UTILS_H

#include "../core/autonomy_types.h"
#include <stdbool.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

// GPS coordinate structure
typedef struct {
    double lat;                         // Latitude in decimal degrees
    double lon;                         // Longitude in decimal degrees
} gps_coordinate_t;

// Function prototypes

/**
 * Initialize GPS coordinate utilities
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_coordinate_utils_init(void);

/**
 * Calculate distance between two GPS coordinates using Haversine formula
 * @param lat1 Latitude of first point
 * @param lon1 Longitude of first point
 * @param lat2 Latitude of second point
 * @param lon2 Longitude of second point
 * @return Distance in meters
 */
double gps_coordinate_distance(double lat1, double lon1, double lat2, double lon2);

/**
 * Calculate distance between two GPS coordinates in kilometers
 * @param lat1 Latitude of first point
 * @param lon1 Longitude of first point
 * @param lat2 Latitude of second point
 * @param lon2 Longitude of second point
 * @return Distance in kilometers
 */
double gps_coordinate_distance_km(double lat1, double lon1, double lat2, double lon2);

/**
 * Calculate bearing between two GPS coordinates
 * @param lat1 Latitude of first point
 * @param lon1 Longitude of first point
 * @param lat2 Latitude of second point
 * @param lon2 Longitude of second point
 * @return Bearing in degrees (0-360)
 */
double gps_coordinate_bearing(double lat1, double lon1, double lat2, double lon2);

/**
 * Calculate destination point given start point, bearing, and distance
 * @param start_lat Starting latitude
 * @param start_lon Starting longitude
 * @param bearing Bearing in degrees
 * @param distance_meters Distance in meters
 * @param dest_lat Destination latitude (output)
 * @param dest_lon Destination longitude (output)
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_coordinate_destination(double start_lat, double start_lon, double bearing, 
                              double distance_meters, double *dest_lat, double *dest_lon);

/**
 * Calculate midpoint between two GPS coordinates
 * @param lat1 Latitude of first point
 * @param lon1 Longitude of first point
 * @param lat2 Latitude of second point
 * @param lon2 Longitude of second point
 * @param mid_lat Midpoint latitude (output)
 * @param mid_lon Midpoint longitude (output)
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_coordinate_midpoint(double lat1, double lon1, double lat2, double lon2, 
                           double *mid_lat, double *mid_lon);

/**
 * Calculate area of a polygon defined by GPS coordinates
 * @param coordinates Array of polygon coordinates
 * @param num_coordinates Number of coordinates
 * @return Area in square meters
 */
double gps_coordinate_polygon_area(const gps_coordinate_t *coordinates, int num_coordinates);

/**
 * Check if a point is inside a polygon using ray casting algorithm
 * @param lat Point latitude
 * @param lon Point longitude
 * @param polygon Array of polygon coordinates
 * @param num_vertices Number of polygon vertices
 * @return true if point is inside polygon, false otherwise
 */
bool gps_coordinate_point_in_polygon(double lat, double lon, const gps_coordinate_t *polygon, int num_vertices);

/**
 * Calculate the centroid of a polygon
 * @param coordinates Array of polygon coordinates
 * @param num_coordinates Number of coordinates
 * @param centroid_lat Centroid latitude (output)
 * @param centroid_lon Centroid longitude (output)
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_coordinate_polygon_centroid(const gps_coordinate_t *coordinates, int num_coordinates, 
                                   double *centroid_lat, double *centroid_lon);

/**
 * Convert decimal degrees to degrees, minutes, seconds format
 * @param decimal_degrees Decimal degrees
 * @param degrees Degrees (output)
 * @param minutes Minutes (output)
 * @param seconds Seconds (output)
 */
void gps_coordinate_decimal_to_dms(double decimal_degrees, int *degrees, int *minutes, double *seconds);

/**
 * Convert degrees, minutes, seconds to decimal degrees
 * @param degrees Degrees
 * @param minutes Minutes
 * @param seconds Seconds
 * @return Decimal degrees
 */
double gps_coordinate_dms_to_decimal(int degrees, int minutes, double seconds);

/**
 * Validate GPS coordinates
 * @param lat Latitude
 * @param lon Longitude
 * @return true if coordinates are valid, false otherwise
 */
bool gps_coordinate_is_valid(double lat, double lon);

/**
 * Normalize longitude to -180 to 180 range
 * @param lon Longitude to normalize
 * @return Normalized longitude
 */
double gps_coordinate_normalize_lon(double lon);

/**
 * Normalize latitude to -90 to 90 range
 * @param lat Latitude to normalize
 * @return Normalized latitude
 */
double gps_coordinate_normalize_lat(double lat);

/**
 * Calculate great circle distance using spherical approximation
 * @param lat1 Latitude of first point
 * @param lon1 Longitude of first point
 * @param lat2 Latitude of second point
 * @param lon2 Longitude of second point
 * @return Distance in meters
 */
double gps_coordinate_great_circle_distance(double lat1, double lon1, double lat2, double lon2);

/**
 * Calculate rhumb line distance and bearing
 * @param lat1 Latitude of first point
 * @param lon1 Longitude of first point
 * @param lat2 Latitude of second point
 * @param lon2 Longitude of second point
 * @param distance Rhumb line distance in meters (output)
 * @param bearing Rhumb line bearing in degrees (output)
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_coordinate_rhumb_line(double lat1, double lon1, double lat2, double lon2, 
                             double *distance, double *bearing);

/**
 * Calculate intersection of two great circles
 * @param lat1 Latitude of first great circle start point
 * @param lon1 Longitude of first great circle start point
 * @param bearing1 Bearing of first great circle
 * @param lat2 Latitude of second great circle start point
 * @param lon2 Longitude of second great circle start point
 * @param bearing2 Bearing of second great circle
 * @param intersect_lat Intersection latitude (output)
 * @param intersect_lon Intersection longitude (output)
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_coordinate_great_circle_intersection(double lat1, double lon1, double bearing1,
                                            double lat2, double lon2, double bearing2,
                                            double *intersect_lat, double *intersect_lon);

/**
 * Calculate the closest point on a line segment to a given point
 * @param point_lat Point latitude
 * @param point_lon Point longitude
 * @param line_lat1 Line start latitude
 * @param line_lon1 Line start longitude
 * @param line_lat2 Line end latitude
 * @param line_lon2 Line end longitude
 * @param closest_lat Closest point latitude (output)
 * @param closest_lon Closest point longitude (output)
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_coordinate_closest_point_on_line(double point_lat, double point_lon,
                                        double line_lat1, double line_lon1,
                                        double line_lat2, double line_lon2,
                                        double *closest_lat, double *closest_lon);

/**
 * Cleanup GPS coordinate utilities
 */
void gps_coordinate_utils_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_COORDINATE_UTILS_H
