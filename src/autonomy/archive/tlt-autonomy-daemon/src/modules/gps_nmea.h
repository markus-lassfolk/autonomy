#ifndef GPS_NMEA_H
#define GPS_NMEA_H

#include "types.h"
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// NMEA satellite information
typedef struct {
    int id;                             // Satellite ID
    int elevation;                      // Elevation angle in degrees
    int azimuth;                        // Azimuth angle in degrees
    int snr;                           // Signal-to-noise ratio
    bool used;                          // Whether satellite is used in fix
} nmea_satellite_t;

// NMEA parser configuration
typedef struct {
    bool enabled;                       // Enable/disable NMEA parser
    int max_sentence_length;            // Maximum sentence length
    int min_sentence_length;            // Minimum sentence length
    int max_satellites;                 // Maximum satellites to track
} gps_nmea_config_t;

// NMEA parser statistics
typedef struct {
    int parse_count;                    // Total sentences parsed
    int successful_parses;              // Successfully parsed sentences
    int failed_parses;                  // Failed parse attempts
    double success_rate;                // Success rate (0.0-1.0)
    time_t last_parse;                  // Last parse timestamp
} gps_nmea_stats_t;

// NMEA parser state
typedef struct {
    bool enabled;                       // NMEA parser enabled
    int max_sentence_length;            // Maximum sentence length
    int min_sentence_length;            // Minimum sentence length
    int max_satellites;                 // Maximum satellites to track
    
    // Statistics
    int parse_count;                    // Total sentences parsed
    int successful_parses;              // Successfully parsed sentences
    int failed_parses;                  // Failed parse attempts
    time_t last_parse;                  // Last parse timestamp
    
    // Satellite tracking
    nmea_satellite_t satellites[20];    // Satellite information array
} gps_nmea_t;

// Function prototypes

/**
 * Initialize NMEA parser
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_nmea_init(void);

/**
 * Parse NMEA sentence
 * @param sentence NMEA sentence string to parse
 * @param gps_data GPS data structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_nmea_parse_sentence(const char *sentence, gps_data_t *gps_data);

/**
 * Get NMEA parser statistics
 * @param stats Statistics structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_nmea_get_statistics(gps_nmea_stats_t *stats);

/**
 * Get NMEA parser configuration
 * @param config Configuration structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_nmea_get_config(gps_nmea_config_t *config);

/**
 * Set NMEA parser configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_nmea_set_config(const gps_nmea_config_t *config);

/**
 * Enable/disable NMEA parser
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_nmea_set_enabled(bool enabled);

/**
 * Reset NMEA parser statistics
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_nmea_reset_statistics(void);

/**
 * Cleanup NMEA parser
 */
void gps_nmea_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_NMEA_H
