#include "astro_coordinates.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Time conversion functions

// Convert Unix time to astronomical time
astro_time_t astro_time_from_unix(time_t unix_time) {
    astro_time_t astro_time;
    astro_time.unix_time = unix_time;
    astro_time.julian_date = (double)unix_time / SECONDS_PER_DAY + UNIX_EPOCH_JULIAN;
    astro_time.ut1_utc_offset = 0.0; // Simplified - real implementation would look this up
    astro_time.tai_utc_offset = 37.0; // Current TAI-UTC offset (as of 2024)
    return astro_time;
}

// Convert Julian date to astronomical time
astro_time_t astro_time_from_julian(double julian_date) {
    astro_time_t astro_time;
    astro_time.julian_date = julian_date;
    astro_time.unix_time = (time_t)((julian_date - UNIX_EPOCH_JULIAN) * SECONDS_PER_DAY);
    astro_time.ut1_utc_offset = 0.0;
    astro_time.tai_utc_offset = 37.0;
    return astro_time;
}

// Get current Julian date
double astro_julian_date_now(void) {
    time_t now = time(NULL);
    return (double)now / SECONDS_PER_DAY + UNIX_EPOCH_JULIAN;
}

// Create Earth location
earth_location_t astro_earth_location(double lat_deg, double lon_deg, double alt_m) {
    earth_location_t location;
    location.latitude_deg = lat_deg;
    location.longitude_deg = lon_deg;
    location.altitude_m = alt_m;
    return location;
}

// Convert Earth location to ITRS coordinates
vector3_t astro_earth_location_to_itrs(const earth_location_t *location, const astro_time_t *time) {
    vector3_t itrs = {0};
    
    if (!location) {
        return itrs;
    }
    
    double lat_rad = location->latitude_deg * ASTRO_DEG_TO_RAD;
    double lon_rad = location->longitude_deg * ASTRO_DEG_TO_RAD;
    double alt_m = location->altitude_m;
    
    // WGS84 ellipsoid parameters
    double a = EARTH_RADIUS_KM * 1000.0; // Semi-major axis in meters
    double f = EARTH_FLATTENING;
    double e2 = f * (2.0 - f); // First eccentricity squared
    
    // Calculate radius of curvature in the prime vertical
    double sin_lat = sin(lat_rad);
    double cos_lat = cos(lat_rad);
    double sin_lon = sin(lon_rad);
    double cos_lon = cos(lon_rad);
    
    double N = a / sqrt(1.0 - e2 * sin_lat * sin_lat);
    
    // ITRS coordinates in meters
    itrs.x = (N + alt_m) * cos_lat * cos_lon;
    itrs.y = (N + alt_m) * cos_lat * sin_lon;
    itrs.z = (N * (1.0 - e2) + alt_m) * sin_lat;
    
    // Convert to kilometers
    itrs.x /= 1000.0;
    itrs.y /= 1000.0;
    itrs.z /= 1000.0;
    
    return itrs;
}

// Calculate Greenwich Mean Sidereal Time (GMST)
double astro_greenwich_mean_sidereal_time(const astro_time_t *time) {
    if (!time) {
        return 0.0;
    }
    
    // Days since J2000.0
    double t = time->julian_date - JULIAN_EPOCH_J2000;
    
    // GMST at 0h UT1 (IAU 2000 model)
    double gmst0 = 24110.54841 + 8640184.812866 * t / JULIAN_CENTURY 
                   + 0.093104 * pow(t / JULIAN_CENTURY, 2) 
                   - 6.2e-6 * pow(t / JULIAN_CENTURY, 3);
    
    // Convert to hours and normalize
    gmst0 = fmod(gmst0 / 3600.0, 24.0);
    if (gmst0 < 0.0) gmst0 += 24.0;
    
    // Add UT1 time of day
    double fraction_of_day = fmod(time->julian_date, 1.0);
    double ut1_hours = fraction_of_day * 24.0 + time->ut1_utc_offset / 3600.0;
    
    // Earth's rotation rate (slightly faster than 1 sidereal day)
    double gmst = gmst0 + ut1_hours * 1.00273790935;
    
    // Normalize to [0, 24) hours
    gmst = fmod(gmst, 24.0);
    if (gmst < 0.0) gmst += 24.0;
    
    // Convert to degrees
    return gmst * 15.0; // 15 degrees per hour
}

// Transform TEME to ITRS (accounts for Earth rotation)
coordinate_result_t astro_transform_teme_to_itrs(const coordinate_result_t *teme_coords) {
    coordinate_result_t itrs_result = *teme_coords;
    itrs_result.frame = COORD_FRAME_ITRS;
    
    if (!teme_coords || teme_coords->frame != COORD_FRAME_TEME) {
        itrs_result.valid = false;
        return itrs_result;
    }
    
    // Calculate Greenwich Mean Sidereal Time
    double gmst_deg = astro_greenwich_mean_sidereal_time(&teme_coords->time);
    double gmst_rad = gmst_deg * ASTRO_DEG_TO_RAD;
    
    // Rotation matrix from TEME to ITRS (rotation about Z-axis by GMST)
    double cos_gmst = cos(gmst_rad);
    double sin_gmst = sin(gmst_rad);
    
    double rotation_matrix[3][3] = {
        { cos_gmst, sin_gmst, 0.0},
        {-sin_gmst, cos_gmst, 0.0},
        {      0.0,      0.0, 1.0}
    };
    
    // Apply rotation to position
    astro_matrix_vector_multiply(rotation_matrix, &teme_coords->position, &itrs_result.position);
    
    // Apply rotation to velocity (if present)
    if (teme_coords->velocity.x != 0.0 || teme_coords->velocity.y != 0.0 || teme_coords->velocity.z != 0.0) {
        astro_matrix_vector_multiply(rotation_matrix, &teme_coords->velocity, &itrs_result.velocity);
        
        // Add Earth rotation effect to velocity
        vector3_t earth_rotation_effect = {
            -EARTH_ROTATION_RATE * itrs_result.position.y,
             EARTH_ROTATION_RATE * itrs_result.position.x,
             0.0
        };
        
        astro_vector_add(&itrs_result.velocity, &earth_rotation_effect, &itrs_result.velocity);
    }
    
    return itrs_result;
}

// Transform ITRS to topocentric (observer-centered)
coordinate_result_t astro_transform_itrs_to_topocentric(const coordinate_result_t *itrs_coords, const earth_location_t *observer) {
    coordinate_result_t topo_result = *itrs_coords;
    topo_result.frame = COORD_FRAME_TOPOCENTRIC;
    
    if (!itrs_coords || !observer || itrs_coords->frame != COORD_FRAME_ITRS) {
        topo_result.valid = false;
        return topo_result;
    }
    
    // Get observer position in ITRS
    vector3_t observer_itrs = astro_earth_location_to_itrs(observer, &itrs_coords->time);
    
    // Calculate relative position vector (satellite - observer)
    astro_vector_subtract(&itrs_coords->position, &observer_itrs, &topo_result.position);
    
    // Transform to local topocentric frame (SEZ - South, East, Zenith)
    double lat_rad = observer->latitude_deg * ASTRO_DEG_TO_RAD;
    double lon_rad = observer->longitude_deg * ASTRO_DEG_TO_RAD;
    
    double sin_lat = sin(lat_rad);
    double cos_lat = cos(lat_rad);
    double sin_lon = sin(lon_rad);
    double cos_lon = cos(lon_rad);
    
    // Transformation matrix from ITRS to local SEZ frame
    double local_matrix[3][3] = {
        { sin_lat * cos_lon,  sin_lat * sin_lon, -cos_lat},
        {         -sin_lon,           cos_lon,      0.0},
        { cos_lat * cos_lon,  cos_lat * sin_lon,  sin_lat}
    };
    
    vector3_t relative_pos = topo_result.position;
    astro_matrix_vector_multiply(local_matrix, &relative_pos, &topo_result.position);
    
    // Transform velocity if present
    if (itrs_coords->velocity.x != 0.0 || itrs_coords->velocity.y != 0.0 || itrs_coords->velocity.z != 0.0) {
        astro_matrix_vector_multiply(local_matrix, &itrs_coords->velocity, &topo_result.velocity);
    }
    
    return topo_result;
}

// Transform topocentric to Altitude-Azimuth
topocentric_result_t astro_transform_to_altaz(const coordinate_result_t *topocentric_coords) {
    topocentric_result_t altaz = {0};
    altaz.time = topocentric_coords->time;
    
    if (!topocentric_coords || topocentric_coords->frame != COORD_FRAME_TOPOCENTRIC) {
        altaz.valid = false;
        return altaz;
    }
    
    const vector3_t *pos = &topocentric_coords->position;
    
    // Calculate range
    altaz.range_km = astro_vector_magnitude(pos);
    
    if (altaz.range_km == 0.0) {
        altaz.valid = false;
        return altaz;
    }
    
    // In SEZ frame: S=South, E=East, Z=Zenith
    double south = pos->x;
    double east = pos->y;
    double zenith = pos->z;
    
    // Calculate azimuth (measured clockwise from North)
    // Note: South is positive X in SEZ, so azimuth = atan2(East, South) + 180°
    altaz.azimuth_deg = atan2(east, south) * ASTRO_RAD_TO_DEG;
    if (altaz.azimuth_deg < 0.0) {
        altaz.azimuth_deg += 360.0;
    }
    
    // Calculate elevation (angle above horizon)
    double horizontal_distance = sqrt(south * south + east * east);
    altaz.elevation_deg = atan2(zenith, horizontal_distance) * ASTRO_RAD_TO_DEG;
    
    // Calculate range rate if velocity is available
    if (topocentric_coords->velocity.x != 0.0 || topocentric_coords->velocity.y != 0.0 || topocentric_coords->velocity.z != 0.0) {
        // Range rate = dot product of position and velocity vectors, divided by range
        double dot_product = astro_vector_dot_product(pos, &topocentric_coords->velocity);
        altaz.range_rate_km_s = dot_product / altaz.range_km;
    }
    
    altaz.valid = true;
    return altaz;
}

// Complete transformation chain: TEME -> ITRS -> Topocentric -> AltAz
topocentric_result_t astro_transform_teme_to_altaz(
    const vector3_t *teme_position,
    const vector3_t *teme_velocity,
    const astro_time_t *time,
    const earth_location_t *observer) {
    
    topocentric_result_t result = {0};
    
    if (!teme_position || !time || !observer) {
        result.valid = false;
        return result;
    }
    
    // Step 1: Create TEME coordinate object
    coordinate_result_t teme_coords;
    teme_coords.position = *teme_position;
    if (teme_velocity) {
        teme_coords.velocity = *teme_velocity;
    } else {
        memset(&teme_coords.velocity, 0, sizeof(vector3_t));
    }
    teme_coords.frame = COORD_FRAME_TEME;
    teme_coords.time = *time;
    teme_coords.valid = true;
    
    // Step 2: TEME to ITRS
    coordinate_result_t itrs_coords = astro_transform_teme_to_itrs(&teme_coords);
    if (!itrs_coords.valid) {
        result.valid = false;
        return result;
    }
    
    // Step 3: ITRS to Topocentric
    coordinate_result_t topo_coords = astro_transform_itrs_to_topocentric(&itrs_coords, observer);
    if (!topo_coords.valid) {
        result.valid = false;
        return result;
    }
    
    // Step 4: Topocentric to AltAz
    result = astro_transform_to_altaz(&topo_coords);
    
    return result;
}

// Matrix operations

// 3x3 matrix multiplication
void astro_matrix_multiply_3x3(const double a[3][3], const double b[3][3], double result[3][3]) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            result[i][j] = 0.0;
            for (int k = 0; k < 3; k++) {
                result[i][j] += a[i][k] * b[k][j];
            }
        }
    }
}

// Matrix-vector multiplication
void astro_matrix_vector_multiply(const double matrix[3][3], const vector3_t *vector, vector3_t *result) {
    if (!vector || !result) {
        return;
    }
    
    double temp[3] = {vector->x, vector->y, vector->z};
    
    result->x = matrix[0][0] * temp[0] + matrix[0][1] * temp[1] + matrix[0][2] * temp[2];
    result->y = matrix[1][0] * temp[0] + matrix[1][1] * temp[1] + matrix[1][2] * temp[2];
    result->z = matrix[2][0] * temp[0] + matrix[2][1] * temp[1] + matrix[2][2] * temp[2];
}

// Matrix transpose
void astro_matrix_transpose(const double matrix[3][3], double result[3][3]) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            result[j][i] = matrix[i][j];
        }
    }
}

// Identity matrix
void astro_matrix_identity(double matrix[3][3]) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            matrix[i][j] = (i == j) ? 1.0 : 0.0;
        }
    }
}

// Vector operations

// Vector magnitude
double astro_vector_magnitude(const vector3_t *vector) {
    if (!vector) {
        return 0.0;
    }
    return sqrt(vector->x * vector->x + vector->y * vector->y + vector->z * vector->z);
}

// Normalize vector
void astro_vector_normalize(vector3_t *vector) {
    if (!vector) {
        return;
    }
    
    double magnitude = astro_vector_magnitude(vector);
    if (magnitude > 0.0) {
        vector->x /= magnitude;
        vector->y /= magnitude;
        vector->z /= magnitude;
    }
}

// Vector dot product
double astro_vector_dot_product(const vector3_t *a, const vector3_t *b) {
    if (!a || !b) {
        return 0.0;
    }
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

// Vector cross product
void astro_vector_cross_product(const vector3_t *a, const vector3_t *b, vector3_t *result) {
    if (!a || !b || !result) {
        return;
    }
    
    result->x = a->y * b->z - a->z * b->y;
    result->y = a->z * b->x - a->x * b->z;
    result->z = a->x * b->y - a->y * b->x;
}

// Vector subtraction
void astro_vector_subtract(const vector3_t *a, const vector3_t *b, vector3_t *result) {
    if (!a || !b || !result) {
        return;
    }
    
    result->x = a->x - b->x;
    result->y = a->y - b->y;
    result->z = a->z - b->z;
}

// Vector addition
void astro_vector_add(const vector3_t *a, const vector3_t *b, vector3_t *result) {
    if (!a || !b || !result) {
        return;
    }
    
    result->x = a->x + b->x;
    result->y = a->y + b->y;
    result->z = a->z + b->z;
}

// Coordinate utilities

// Normalize angle to [0, 360) degrees
double astro_normalize_angle_0_360(double angle_deg) {
    while (angle_deg < 0.0) {
        angle_deg += 360.0;
    }
    while (angle_deg >= 360.0) {
        angle_deg -= 360.0;
    }
    return angle_deg;
}

// Normalize angle to [-180, 180) degrees
double astro_normalize_angle_pm180(double angle_deg) {
    while (angle_deg < -180.0) {
        angle_deg += 360.0;
    }
    while (angle_deg >= 180.0) {
        angle_deg -= 360.0;
    }
    return angle_deg;
}

// Calculate angular separation between two points on the celestial sphere
double astro_angular_separation(double az1_deg, double el1_deg, double az2_deg, double el2_deg) {
    // Convert to radians
    double az1 = az1_deg * ASTRO_DEG_TO_RAD;
    double el1 = el1_deg * ASTRO_DEG_TO_RAD;
    double az2 = az2_deg * ASTRO_DEG_TO_RAD;
    double el2 = el2_deg * ASTRO_DEG_TO_RAD;
    
    // Convert to Cartesian coordinates on unit sphere
    double x1 = cos(el1) * cos(az1);
    double y1 = cos(el1) * sin(az1);
    double z1 = sin(el1);
    
    double x2 = cos(el2) * cos(az2);
    double y2 = cos(el2) * sin(az2);
    double z2 = sin(el2);
    
    // Calculate dot product
    double dot_product = x1 * x2 + y1 * y2 + z1 * z2;
    
    // Clamp to valid range for acos
    if (dot_product > 1.0) dot_product = 1.0;
    if (dot_product < -1.0) dot_product = -1.0;
    
    // Calculate angular separation
    double separation_rad = acos(dot_product);
    
    return separation_rad * ASTRO_RAD_TO_DEG;
}

// Calculate atmospheric refraction correction
double astro_atmospheric_refraction_correction(double elevation_deg, double temperature_c, double pressure_hpa) {
    if (elevation_deg <= 0.0) {
        return 0.0; // No correction below horizon
    }
    
    if (elevation_deg >= 90.0) {
        return 0.0; // No correction at zenith
    }
    
    // Standard atmospheric model
    double pressure_correction = pressure_hpa / 1013.25;
    double temperature_correction = 283.0 / (273.0 + temperature_c);
    
    // Approximate refraction formula (good for elevations > 15°)
    double elevation_rad = elevation_deg * ASTRO_DEG_TO_RAD;
    double tan_elevation = tan(elevation_rad);
    
    // Refraction in arcseconds
    double refraction_arcsec = 58.1 / tan_elevation - 0.07 / (tan_elevation * tan_elevation * tan_elevation) + 0.000086 / (tan_elevation * tan_elevation * tan_elevation * tan_elevation * tan_elevation);
    
    // Apply atmospheric corrections
    refraction_arcsec *= pressure_correction * temperature_correction;
    
    // Convert to degrees
    return refraction_arcsec / 3600.0;
}

// Validation functions

// Validate coordinate result
bool astro_validate_coordinates(const coordinate_result_t *coords) {
    if (!coords || !coords->valid) {
        return false;
    }
    
    // Check for NaN or infinite values
    if (isnan(coords->position.x) || isnan(coords->position.y) || isnan(coords->position.z) ||
        isinf(coords->position.x) || isinf(coords->position.y) || isinf(coords->position.z)) {
        return false;
    }
    
    // Check if position magnitude is reasonable (for Earth satellites)
    double magnitude = astro_vector_magnitude(&coords->position);
    if (magnitude < 6000.0 || magnitude > 50000.0) { // 6,000 to 50,000 km from Earth center
        return false;
    }
    
    return true;
}

// Validate topocentric result
bool astro_validate_topocentric(const topocentric_result_t *topo) {
    if (!topo || !topo->valid) {
        return false;
    }
    
    // Check for NaN or infinite values
    if (isnan(topo->azimuth_deg) || isnan(topo->elevation_deg) || isnan(topo->range_km) ||
        isinf(topo->azimuth_deg) || isinf(topo->elevation_deg) || isinf(topo->range_km)) {
        return false;
    }
    
    // Check ranges
    if (topo->azimuth_deg < 0.0 || topo->azimuth_deg >= 360.0) {
        return false;
    }
    
    if (topo->elevation_deg < -90.0 || topo->elevation_deg > 90.0) {
        return false;
    }
    
    if (topo->range_km <= 0.0) {
        return false;
    }
    
    return true;
}

// Validate Earth location
bool astro_validate_earth_location(const earth_location_t *location) {
    if (!location) {
        return false;
    }
    
    // Check latitude bounds
    if (location->latitude_deg < -90.0 || location->latitude_deg > 90.0) {
        return false;
    }
    
    // Check longitude bounds
    if (location->longitude_deg < -180.0 || location->longitude_deg > 180.0) {
        return false;
    }
    
    // Check altitude bounds (reasonable range for ground-based observers)
    if (location->altitude_m < -500.0 || location->altitude_m > 10000.0) {
        return false;
    }
    
    return true;
}