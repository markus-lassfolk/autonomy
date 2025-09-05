#ifndef GPS_OBSTRUCTION_H
#define GPS_OBSTRUCTION_H

#include "types.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// GPS obstruction types
typedef enum {
    GPS_OBSTRUCTION_TYPE_UNKNOWN = 0,
    GPS_OBSTRUCTION_TYPE_BUILDING,
    GPS_OBSTRUCTION_TYPE_TUNNEL,
    GPS_OBSTRUCTION_TYPE_MOUNTAIN,
    GPS_OBSTRUCTION_TYPE_FOREST,
    GPS_OBSTRUCTION_TYPE_URBAN_CANYON,
    GPS_OBSTRUCTION_TYPE_INDOOR,
    GPS_OBSTRUCTION_TYPE_UNDERGROUND,
    GPS_OBSTRUCTION_TYPE_VEHICLE,
    GPS_OBSTRUCTION_TYPE_WEATHER,
    GPS_OBSTRUCTION_TYPE_INTERFERENCE,
    GPS_OBSTRUCTION_TYPE_MULTIPATH,
    GPS_OBSTRUCTION_TYPE_MAX
} gps_obstruction_type_t;

// GPS obstruction record
typedef struct {
    time_t timestamp;                   // Record timestamp
    gps_obstruction_type_t obstruction_type; // Type of obstruction
    double confidence;                  // Detection confidence (0-1)
    double signal_quality;              // GPS signal quality (0-1)
    int satellite_count;                // Number of satellites
    double accuracy;                    // GPS accuracy in meters
    double gps_lat;                     // GPS latitude
    double gps_lon;                     // GPS longitude
} gps_obstruction_record_t;

// GPS satellite obstruction
typedef struct {
    int satellite_id;                   // Satellite ID
    gps_obstruction_type_t obstruction_type; // Type of obstruction
    double confidence;                  // Detection confidence (0-1)
    time_t last_detected;               // Last detection timestamp
    int detection_count;                // Number of detections
} gps_satellite_obstruction_t;

// GPS obstruction configuration
typedef struct {
    bool enabled;                       // Enable/disable obstruction analysis
    int max_records;                    // Maximum obstruction records
    int analysis_interval;              // Analysis interval in seconds
    double min_signal_quality;          // Minimum signal quality threshold
    double detection_threshold;         // Obstruction detection threshold
    int max_satellite_obstructions;     // Maximum satellite obstructions
} gps_obstruction_config_t;

// GPS obstruction status
typedef struct {
    bool enabled;                       // Obstruction analysis enabled
    bool obstruction_detected;          // Whether obstruction is currently detected
    time_t last_analysis;               // Last analysis timestamp
    int total_analyses;                 // Total analyses performed
    int obstruction_count;              // Total obstructions detected
    int recent_record_count;            // Number of recent records
    gps_obstruction_record_t recent_records[50]; // Recent obstruction records
} gps_obstruction_status_t;

// GPS obstruction statistics
typedef struct {
    int total_obstructions;             // Total obstructions detected
    int total_analyses;                 // Total analyses performed
    double total_confidence;            // Sum of all confidence values
    double total_signal_quality;        // Sum of all signal quality values
    double total_accuracy;              // Sum of all accuracy values
    int total_satellites;               // Sum of all satellite counts
    double average_confidence;          // Average confidence
    double average_signal_quality;      // Average signal quality
    double average_accuracy;            // Average accuracy
    double average_satellites;          // Average satellite count
    int obstruction_counts[GPS_OBSTRUCTION_TYPE_MAX]; // Obstruction counts by type
} gps_obstruction_stats_t;

// GPS obstruction system state
typedef struct {
    bool enabled;                       // Obstruction analysis enabled
    int max_records;                    // Maximum records
    int analysis_interval;              // Analysis interval
    double min_signal_quality;          // Minimum signal quality
    double detection_threshold;         // Detection threshold
    int max_satellite_obstructions;     // Maximum satellite obstructions
    
    // State
    int record_count;                   // Record count
    bool obstruction_detected;          // Obstruction detected
    time_t last_analysis;               // Last analysis
    int total_analyses;                 // Total analyses
    int obstruction_count;              // Obstruction count
    
    // Obstruction records
    gps_obstruction_record_t obstruction_records[1000]; // Obstruction records
    
    // Satellite obstructions
    gps_satellite_obstruction_t satellite_obstructions[50]; // Satellite obstructions
} gps_obstruction_t;

// Function prototypes

/**
 * Initialize GPS obstruction analysis
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_obstruction_init(void);

/**
 * Analyze GPS data for obstructions
 * @param gps_data GPS data to analyze
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_obstruction_analyze_gps_data(const gps_data_t *gps_data);

/**
 * Get obstruction analysis status
 * @param status Status structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_obstruction_get_status(gps_obstruction_status_t *status);

/**
 * Get obstruction analysis configuration
 * @param config Configuration structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_obstruction_get_config(gps_obstruction_config_t *config);

/**
 * Set obstruction analysis configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_obstruction_set_config(const gps_obstruction_config_t *config);

/**
 * Enable/disable obstruction analysis
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_obstruction_set_enabled(bool enabled);

/**
 * Force obstruction analysis
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_obstruction_force_analysis(void);

/**
 * Get obstruction statistics
 * @param stats Statistics structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_obstruction_get_statistics(gps_obstruction_stats_t *stats);

/**
 * Reset obstruction analysis
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_obstruction_reset(void);

/**
 * Cleanup obstruction analysis
 */
void gps_obstruction_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_OBSTRUCTION_H
