#ifndef GPS_ACCURACY_H
#define GPS_ACCURACY_H

#include "types.h"
#include <stdbool.h>
#include <time.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// GPS validation flags
#define GPS_VALIDATION_SUSPICIOUS_ACCURACY  0x0001
#define GPS_VALIDATION_POOR_ACCURACY        0x0002
#define GPS_VALIDATION_SUSPICIOUS_ALTITUDE  0x0004
#define GPS_VALIDATION_POSITION_JUMP        0x0008
#define GPS_VALIDATION_OLD_DATA             0x0010

// GPS validation result
typedef struct {
    time_t timestamp;                    // Validation timestamp
    bool is_valid;                       // Overall validation result
    double confidence;                   // Confidence score (0.0-1.0)
    uint32_t flags;                      // Validation flags
    char error_message[256];             // Error message if validation failed
    char warning_message[256];           // Warning message if issues detected
} gps_validation_result_t;

// GPS accuracy configuration
typedef struct {
    bool enabled;                        // Enable/disable accuracy validation
    double min_accuracy;                 // Minimum accuracy threshold in meters
    double max_accuracy;                 // Maximum accuracy threshold in meters
    double suspicious_accuracy;          // Suspiciously good accuracy threshold
    double poor_accuracy;                // Poor accuracy threshold
    int min_satellites;                  // Minimum satellites for valid fix
    int max_satellites;                  // Maximum satellites (sanity check)
    double max_speed;                    // Maximum realistic speed in m/s
    double max_altitude;                 // Maximum realistic altitude in meters
    double min_altitude;                 // Minimum realistic altitude in meters
} gps_accuracy_config_t;

// GPS accuracy statistics
typedef struct {
    int total_validations;               // Total number of validations
    int valid_count;                     // Number of valid GPS readings
    int invalid_count;                   // Number of invalid GPS readings
    int suspicious_count;                // Number of suspicious readings
    double validity_rate;                // Rate of valid readings (0.0-1.0)
    time_t last_validation;              // Timestamp of last validation
} gps_accuracy_stats_t;

// GPS accuracy validator state
typedef struct {
    bool enabled;                        // Accuracy validator enabled
    double min_accuracy;                 // Minimum accuracy threshold
    double max_accuracy;                 // Maximum accuracy threshold
    double suspicious_accuracy;          // Suspicious accuracy threshold
    double poor_accuracy;                // Poor accuracy threshold
    int min_satellites;                  // Minimum satellites
    int max_satellites;                  // Maximum satellites
    double max_speed;                    // Maximum speed
    double max_altitude;                 // Maximum altitude
    double min_altitude;                 // Minimum altitude
    
    // Statistics
    int validation_count;                // Total validations performed
    int valid_count;                     // Valid GPS readings
    int invalid_count;                   // Invalid GPS readings
    int suspicious_count;                // Suspicious readings
    time_t last_validation;              // Last validation timestamp
    
    // Previous position for jump detection
    gps_data_t last_position;            // Last validated position
} gps_accuracy_t;

// Function prototypes

/**
 * Initialize GPS accuracy validator
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_accuracy_init(void);

/**
 * Validate GPS accuracy
 * @param gps_data GPS data to validate
 * @param result Validation result structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_accuracy_validate(const gps_data_t *gps_data, gps_validation_result_t *result);

/**
 * Get accuracy validation statistics
 * @param stats Statistics structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_accuracy_get_statistics(gps_accuracy_stats_t *stats);

/**
 * Get accuracy validator configuration
 * @param config Configuration structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_accuracy_get_config(gps_accuracy_config_t *config);

/**
 * Set accuracy validator configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_accuracy_set_config(const gps_accuracy_config_t *config);

/**
 * Enable/disable accuracy validator
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_accuracy_set_enabled(bool enabled);

/**
 * Reset validation statistics
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_accuracy_reset_statistics(void);

/**
 * Cleanup accuracy validator
 */
void gps_accuracy_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_ACCURACY_H
