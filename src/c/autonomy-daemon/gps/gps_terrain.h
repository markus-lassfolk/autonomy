#ifndef GPS_TERRAIN_H
#define GPS_TERRAIN_H

#include "../core/types.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// Terrain types
typedef enum {
    TERRAIN_TYPE_UNKNOWN = 0,
    TERRAIN_TYPE_FLAT,
    TERRAIN_TYPE_HILLY,
    TERRAIN_TYPE_MOUNTAINOUS,
    TERRAIN_TYPE_VALLEY,
    TERRAIN_TYPE_PLATEAU,
    TERRAIN_TYPE_CLIFF,
    TERRAIN_TYPE_CANYON,
    TERRAIN_TYPE_VOLCANIC,
    TERRAIN_TYPE_GLACIAL,
    TERRAIN_TYPE_DESERT,
    TERRAIN_TYPE_FOREST,
    TERRAIN_TYPE_SWAMP,
    TERRAIN_TYPE_BEACH,
    TERRAIN_TYPE_URBAN,
    TERRAIN_TYPE_RURAL,
    TERRAIN_TYPE_MAX
} terrain_type_t;

// Terrain difficulty levels
typedef enum {
    TERRAIN_DIFFICULTY_UNKNOWN = 0,
    TERRAIN_DIFFICULTY_EASY,
    TERRAIN_DIFFICULTY_MODERATE,
    TERRAIN_DIFFICULTY_CHALLENGING,
    TERRAIN_DIFFICULTY_DIFFICULT,
    TERRAIN_DIFFICULTY_EXTREME
} terrain_difficulty_level_t;

// Terrain information
typedef struct {
    time_t timestamp;                   // Analysis timestamp
    double lat;                         // Latitude
    double lon;                         // Longitude
    double elevation;                   // Elevation in meters
    terrain_type_t terrain_type;        // Terrain type classification
    double elevation_range;             // Elevation range in surrounding area
    double average_elevation;           // Average elevation in surrounding area
    double elevation_variance;          // Elevation variance
    double max_slope;                   // Maximum slope in degrees
    double average_slope;               // Average slope in degrees
    double slope_direction;             // Direction of steepest slope
    double roughness;                   // Terrain roughness index
    double drainage_efficiency;         // Drainage efficiency (0-1)
    double water_flow_direction;        // Water flow direction in degrees
    double vegetation_density;          // Vegetation density (0-1)
    int soil_type;                      // Soil type classification
    int water_bodies;                   // Presence of water bodies
    double difficulty_score;            // Overall difficulty score (0-100)
    terrain_difficulty_level_t difficulty_level; // Difficulty level classification
} gps_terrain_info_t;

// Terrain cache entry
typedef struct {
    bool active;                        // Whether entry is active
    double lat;                         // Latitude
    double lon;                         // Longitude
    time_t timestamp;                   // Cache timestamp
    double elevation;                   // Elevation
    terrain_type_t terrain_type;        // Terrain type
    double slope;                       // Slope
    double roughness;                   // Roughness
    double draination;                  // Drainage
    double vegetation_density;          // Vegetation density
    int soil_type;                      // Soil type
    int water_bodies;                   // Water bodies
} gps_terrain_cache_entry_t;

// Elevation analysis point
typedef struct {
    double lat;                         // Latitude
    double lon;                         // Longitude
    double elevation;                   // Elevation
    time_t timestamp;                   // Analysis timestamp
} gps_elevation_point_t;

// Terrain analysis configuration
typedef struct {
    bool enabled;                       // Enable/disable analysis
    int max_cache_entries;              // Maximum cache entries
    int update_interval;                // Update interval in seconds
    double cache_radius;                // Cache radius in meters
    int max_elevation_points;           // Maximum elevation points
    double min_elevation_difference;    // Minimum elevation difference
} gps_terrain_config_t;

// Terrain analysis status
typedef struct {
    bool enabled;                       // Analysis enabled
    int cache_entry_count;              // Current cache entries
    int max_cache_entries;              // Maximum cache entries
    int total_analyses;                 // Total analyses performed
    int successful_analyses;            // Successful analyses
    int failed_analyses;                // Failed analyses
    time_t last_update;                 // Last update timestamp
    double success_rate;                // Success rate (0-1)
} gps_terrain_status_t;

// Terrain analysis statistics
typedef struct {
    int total_entries;                  // Total cache entries
    int total_analyses;                 // Total analyses
    int successful_analyses;            // Successful analyses
    int failed_analyses;                // Failed analyses
    double total_elevation;             // Total elevation
    double total_slope;                 // Total slope
    double total_roughness;             // Total roughness
    double average_elevation;           // Average elevation
    double average_slope;               // Average slope
    double average_roughness;           // Average roughness
    int terrain_type_counts[TERRAIN_TYPE_MAX]; // Terrain type counts
} gps_terrain_stats_t;

// Terrain analysis system state
typedef struct {
    bool enabled;                       // Analysis enabled
    int max_cache_entries;              // Maximum cache entries
    int update_interval;                // Update interval
    double cache_radius;                // Cache radius
    int max_elevation_points;           // Maximum elevation points
    double min_elevation_difference;    // Minimum elevation difference
    
    // State
    int cache_entry_count;              // Cache entry count
    int total_analyses;                 // Total analyses
    int successful_analyses;            // Successful analyses
    int failed_analyses;                // Failed analyses
    time_t last_update;                 // Last update
    
    // Terrain cache
    gps_terrain_cache_entry_t terrain_cache[2000]; // Terrain cache entries
    
    // Elevation analysis
    gps_elevation_point_t elevation_analysis[100]; // Elevation analysis points
} gps_terrain_t;

// Function prototypes

/**
 * Initialize GPS terrain analysis
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_terrain_init(void);

/**
 * Analyze terrain for coordinates
 * @param lat Latitude
 * @param lon Longitude
 * @param terrain_info Terrain information (output)
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_terrain_analyze(double lat, double lon, gps_terrain_info_t *terrain_info);

/**
 * Get terrain analysis status
 * @param status Status structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_terrain_get_status(gps_terrain_status_t *status);

/**
 * Get terrain analysis configuration
 * @param config Configuration structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_terrain_get_config(gps_terrain_config_t *config);

/**
 * Set terrain analysis configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_terrain_set_config(const gps_terrain_config_t *config);

/**
 * Enable/disable terrain analysis
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_terrain_set_enabled(bool enabled);

/**
 * Force terrain update
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_terrain_force_update(void);

/**
 * Get terrain statistics
 * @param stats Statistics structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_terrain_get_statistics(gps_terrain_stats_t *stats);

/**
 * Reset terrain analysis
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_terrain_reset(void);

/**
 * Cleanup terrain analysis
 */
void gps_terrain_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_TERRAIN_H
