#ifndef GPS_CONFIDENCE_H
#define GPS_CONFIDENCE_H

#include "types.h"
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Maximum position history for consistency calculations
#define MAX_POSITION_HISTORY 10

// GPS confidence context for advanced calculations
typedef struct {
    gps_data_t *previous_positions;    // Array of previous GPS positions
    int position_count;                 // Number of previous positions
    double average_speed;               // Average speed in m/s
    time_t last_update;                 // Last context update
} gps_confidence_context_t;

// GPS confidence configuration
typedef struct {
    bool enabled;                       // Enable/disable confidence calculator
    double min_confidence;              // Minimum confidence threshold
    double max_confidence;              // Maximum confidence threshold
    double accuracy_weight;             // Weight for accuracy-based confidence
    double satellite_weight;            // Weight for satellite-based confidence
    double fix_quality_weight;          // Weight for fix quality confidence
    double freshness_weight;            // Weight for data freshness confidence
    double consistency_weight;          // Weight for position consistency confidence
} gps_confidence_config_t;

// GPS confidence status
typedef struct {
    bool enabled;                       // Confidence calculator enabled
    double min_confidence;              // Current minimum confidence
    double max_confidence;              // Current maximum confidence
    double accuracy_weight;             // Current accuracy weight
    double satellite_weight;            // Current satellite weight
    double fix_quality_weight;          // Current fix quality weight
    double freshness_weight;            // Current freshness weight
    double consistency_weight;          // Current consistency weight
} gps_confidence_status_t;

// GPS confidence calculator state
typedef struct {
    bool enabled;                       // Confidence calculator enabled
    double min_confidence;              // Minimum confidence threshold
    double max_confidence;              // Maximum confidence threshold
    double accuracy_weight;             // Accuracy weight in calculation
    double satellite_weight;            // Satellite count weight
    double fix_quality_weight;          // Fix quality weight
    double freshness_weight;            // Data freshness weight
    double consistency_weight;          // Position consistency weight
} gps_confidence_t;

// Function prototypes

/**
 * Initialize GPS confidence calculator
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_confidence_init(void);

/**
 * Calculate GPS confidence score
 * @param gps_data GPS data to evaluate
 * @param context Optional context for advanced calculations
 * @return Confidence score between 0.0 and 1.0
 */
double gps_confidence_calculate(const gps_data_t *gps_data, const gps_confidence_context_t *context);

/**
 * Get confidence calculator configuration
 * @param config Configuration structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_confidence_get_config(gps_confidence_config_t *config);

/**
 * Set confidence calculator configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_confidence_set_config(const gps_confidence_config_t *config);

/**
 * Enable/disable confidence calculator
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_confidence_set_enabled(bool enabled);

/**
 * Get confidence calculator status
 * @param status Status structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_confidence_get_status(gps_confidence_status_t *status);

/**
 * Cleanup confidence calculator
 */
void gps_confidence_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_CONFIDENCE_H
