#ifndef ASTRO_COORDINATES_H
#define ASTRO_COORDINATES_H

#include <time.h>
#include <math.h>
#include <stdbool.h>

// Mathematical constants
#define ASTRO_PI 3.14159265358979323846
#define ASTRO_DEG_TO_RAD (ASTRO_PI / 180.0)
#define ASTRO_RAD_TO_DEG (180.0 / ASTRO_PI)
#define ASTRO_ARCSEC_TO_RAD (ASTRO_PI / (180.0 * 3600.0))

// Earth parameters (WGS84)
#define EARTH_RADIUS_KM 6378.137
#define EARTH_FLATTENING 1.0/298.257223563
#define EARTH_ROTATION_RATE 7.2921159e-5 // rad/s
#define EARTH_GM 3.986004418e14 // m³/s²

// Time constants
#define JULIAN_EPOCH_J2000 2451545.0
#define JULIAN_CENTURY 36525.0
#define UNIX_EPOCH_JULIAN 2440587.5
#define SECONDS_PER_DAY 86400.0

// Coordinate system frames
typedef enum {
    COORD_FRAME_TEME,     // True Equator Mean Equinox (SGP4 output)
    COORD_FRAME_ITRS,     // International Terrestrial Reference System (Earth-fixed)
    COORD_FRAME_ECEF,     // Earth-Centered Earth-Fixed (alias for ITRS)
    COORD_FRAME_TOPOCENTRIC, // Observer-centered
    COORD_FRAME_ALTAZ     // Altitude-Azimuth (local horizon)
} coordinate_frame_t;

// 3D vector structure
typedef struct {
    double x;
    double y;
    double z;
} vector3_t;

// Earth location structure
typedef struct {
    double latitude_deg;   // Geodetic latitude (degrees)
    double longitude_deg;  // Longitude (degrees)
    double altitude_m;     // Height above WGS84 ellipsoid (meters)
} earth_location_t;

// Time structure with high precision
typedef struct {
    double julian_date;    // Julian date (days since J2000)
    double ut1_utc_offset; // UT1-UTC offset (seconds)
    double tai_utc_offset; // TAI-UTC offset (seconds)
    time_t unix_time;      // Unix timestamp
} astro_time_t;

// Coordinate transformation result
typedef struct {
    vector3_t position;    // Position vector
    vector3_t velocity;    // Velocity vector (if available)
    coordinate_frame_t frame;
    astro_time_t time;
    bool valid;
} coordinate_result_t;

// Topocentric coordinates (Az/El)
typedef struct {
    double azimuth_deg;    // Azimuth (degrees from North, clockwise)
    double elevation_deg;  // Elevation (degrees above horizon)
    double range_km;       // Range distance (km)
    double range_rate_km_s; // Range rate (km/s)
    astro_time_t time;
    bool valid;
} topocentric_result_t;

// Earth orientation parameters (simplified)
typedef struct {
    double x_pole_arcsec;  // Polar motion X (arcseconds)
    double y_pole_arcsec;  // Polar motion Y (arcseconds)
    double ut1_utc_sec;    // UT1-UTC difference (seconds)
    double lod_ms;         // Length of day variation (milliseconds)
    astro_time_t epoch;
} earth_orientation_t;

// API Functions

// Time functions
astro_time_t astro_time_from_unix(time_t unix_time);
astro_time_t astro_time_from_julian(double julian_date);
double astro_time_to_julian(const astro_time_t *time);
time_t astro_time_to_unix(const astro_time_t *time);

// Julian date calculations
double astro_julian_date_now(void);
double astro_julian_date_from_calendar(int year, int month, int day, int hour, int minute, double second);
void astro_calendar_from_julian(double julian_date, int *year, int *month, int *day, int *hour, int *minute, double *second);

// Earth location functions
earth_location_t astro_earth_location(double lat_deg, double lon_deg, double alt_m);
vector3_t astro_earth_location_to_itrs(const earth_location_t *location, const astro_time_t *time);

// Coordinate transformations (Gemini's recommended chain: TEME -> ITRS -> Topocentric -> AltAz)
coordinate_result_t astro_transform_teme_to_itrs(const coordinate_result_t *teme_coords);
coordinate_result_t astro_transform_itrs_to_topocentric(const coordinate_result_t *itrs_coords, const earth_location_t *observer);
topocentric_result_t astro_transform_to_altaz(const coordinate_result_t *topocentric_coords);

// Complete transformation chain
topocentric_result_t astro_transform_teme_to_altaz(
    const vector3_t *teme_position,
    const vector3_t *teme_velocity,
    const astro_time_t *time,
    const earth_location_t *observer
);

// Earth rotation and precession
double astro_greenwich_mean_sidereal_time(const astro_time_t *time);
double astro_greenwich_apparent_sidereal_time(const astro_time_t *time);
void astro_precession_matrix(const astro_time_t *from_time, const astro_time_t *to_time, double matrix[3][3]);
void astro_nutation_matrix(const astro_time_t *time, double matrix[3][3]);

// Earth orientation parameters
earth_orientation_t astro_get_earth_orientation(const astro_time_t *time);
void astro_polar_motion_matrix(double x_arcsec, double y_arcsec, double matrix[3][3]);

// Matrix operations
void astro_matrix_multiply_3x3(const double a[3][3], const double b[3][3], double result[3][3]);
void astro_matrix_vector_multiply(const double matrix[3][3], const vector3_t *vector, vector3_t *result);
void astro_matrix_transpose(const double matrix[3][3], double result[3][3]);
void astro_matrix_identity(double matrix[3][3]);

// Vector operations
double astro_vector_magnitude(const vector3_t *vector);
void astro_vector_normalize(vector3_t *vector);
double astro_vector_dot_product(const vector3_t *a, const vector3_t *b);
void astro_vector_cross_product(const vector3_t *a, const vector3_t *b, vector3_t *result);
void astro_vector_subtract(const vector3_t *a, const vector3_t *b, vector3_t *result);
void astro_vector_add(const vector3_t *a, const vector3_t *b, vector3_t *result);

// Coordinate utilities
double astro_normalize_angle_0_360(double angle_deg);
double astro_normalize_angle_pm180(double angle_deg);
double astro_angular_separation(double az1_deg, double el1_deg, double az2_deg, double el2_deg);

// High-precision calculations for accuracy
typedef struct {
    double mean_obliquity_deg;    // Mean obliquity of the ecliptic
    double nutation_longitude_arcsec; // Nutation in longitude
    double nutation_obliquity_arcsec; // Nutation in obliquity
    double equation_of_equinoxes_sec; // Equation of the equinoxes
} nutation_data_t;

nutation_data_t astro_calculate_nutation(const astro_time_t *time);
double astro_equation_of_equinoxes(const astro_time_t *time, const nutation_data_t *nutation);

// Atmospheric refraction correction
double astro_atmospheric_refraction_correction(double elevation_deg, double temperature_c, double pressure_hpa);

// Coordinate validation
bool astro_validate_coordinates(const coordinate_result_t *coords);
bool astro_validate_topocentric(const topocentric_result_t *topo);
bool astro_validate_earth_location(const earth_location_t *location);

// Performance optimizations
typedef struct {
    // Cached calculations for repeated transformations
    double cached_gmst;
    double cached_gast;
    astro_time_t cache_time;
    bool cache_valid;
    
    // Precession/nutation matrices (expensive to calculate)
    double precession_matrix[3][3];
    double nutation_matrix[3][3];
    astro_time_t matrix_epoch;
    bool matrices_valid;
} astro_cache_t;

astro_cache_t* astro_cache_init(void);
void astro_cache_cleanup(astro_cache_t *cache);
void astro_cache_invalidate(astro_cache_t *cache);

// Error codes
#define ASTRO_SUCCESS                0
#define ASTRO_ERROR_INVALID_PARAM   -1
#define ASTRO_ERROR_INVALID_TIME    -2
#define ASTRO_ERROR_INVALID_COORDS  -3
#define ASTRO_ERROR_MATH_ERROR      -4
#define ASTRO_ERROR_OUT_OF_RANGE    -5

#endif // ASTRO_COORDINATES_H