#ifndef STARLINK_SNOW_DETECTION_INTEGRATION_H
#define STARLINK_SNOW_DETECTION_INTEGRATION_H

#include "starlink_snow_detection.h"
#include "../core/types.h"
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Integration configuration
typedef struct {
    bool enabled;                          // Enable/disable integration
    int check_interval;                    // Periodic check interval in seconds
    int sample_interval;                   // Sample collection interval in seconds
    int max_retries;                       // Maximum retry attempts
} starlink_snow_detection_integration_config_t;

// Integration status
typedef struct {
    bool enabled;                          // Integration enabled status
    bool running;                          // Integration running status
    int check_interval;                    // Current check interval
    int sample_interval;                   // Current sample interval
    int max_retries;                       // Current max retries
    time_t last_sample_time;               // Last sample collection time
    time_t last_check_time;                // Last periodic check time
    int total_samples;                     // Total samples collected
    int successful_samples;                // Successful samples
    int failed_samples;                    // Failed samples
    int snow_detections;                   // Total snow detections
    int heating_activations;               // Total heating activations
    time_t last_snow_detection;            // Last snow detection time
    time_t last_heating_activation;        // Last heating activation time
    bool last_heating_status;              // Last heating status
    bool last_detection_status;            // Last detection status
} starlink_snow_detection_integration_status_t;

// Integration state (internal use)
typedef struct {
    bool enabled;                          // Enable/disable integration
    int check_interval;                    // Periodic check interval in seconds
    int sample_interval;                   // Sample collection interval in seconds
    int max_retries;                       // Maximum retry attempts
    time_t last_sample_time;               // Last sample collection time
    time_t last_check_time;                // Last periodic check time
    int total_samples;                     // Total samples collected
    int successful_samples;                // Successful samples
    int failed_samples;                    // Failed samples
    int snow_detections;                   // Total snow detections
    int heating_activations;               // Total heating activations
    time_t last_snow_detection;            // Last snow detection time
    time_t last_heating_activation;        // Last heating activation time
    bool last_heating_status;              // Last heating status
    bool last_detection_status;            // Last detection status
} starlink_snow_detection_integration_t;

// API Functions

// Initialization and cleanup
int starlink_snow_detection_integration_init(void);
void starlink_snow_detection_integration_cleanup(void);

// Control
int starlink_snow_detection_integration_start(void);
int starlink_snow_detection_integration_stop(void);
int starlink_snow_detection_integration_set_enabled(bool enabled);
int starlink_snow_detection_integration_force_check(void);

// Status and configuration
int starlink_snow_detection_integration_get_status(starlink_snow_detection_integration_status_t *status);
int starlink_snow_detection_integration_set_config(const starlink_snow_detection_integration_config_t *config);

#ifdef __cplusplus
}
#endif

#endif // STARLINK_SNOW_DETECTION_INTEGRATION_H
