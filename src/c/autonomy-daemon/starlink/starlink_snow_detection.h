#ifndef STARLINK_SNOW_DETECTION_H
#define STARLINK_SNOW_DETECTION_H

#include "../core/types.h"
#include "starlink_obstruction.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

// Snow detection configuration
#define MAX_SNOW_SAMPLES 20

// Snow action enumeration
typedef enum {
    SNOW_ACTION_NONE = 0,
    SNOW_ACTION_PREWARM,           // Pre-warm based on forecast
    SNOW_ACTION_MELT,              // Active melting
    SNOW_ACTION_VERIFY,            // Verify obstruction type
    SNOW_ACTION_CLEANUP            // Post-melt cleanup
} snow_action_t;

// Snow sample structure
typedef struct {
    time_t timestamp;              // Sample timestamp
    double fraction_obstructed;    // Obstruction percentage
    double snr;                    // Signal-to-noise ratio
    double temperature;            // Ambient temperature
    double humidity;               // Humidity level
} snow_sample_t;

// Snow detection context
typedef struct {
    bool is_stationary;                    // RV movement status
    bool is_winter_season;                 // Seasonal detection
    bool snow_forecast_active;             // Weather forecast snow
    double obstruction_increase_rate;      // Rate of obstruction increase
    double snr_degradation_rate;           // Rate of SNR degradation
    double temperature;                    // Ambient temperature
    double humidity;                       // Humidity level
    time_t last_clear_time;                // Last time dish was clear
    int consecutive_obstruction_samples;   // Consecutive obstruction detections
} snow_detection_context_t;

// Snow detection configuration
typedef struct {
    bool enabled;                          // Enable/disable snow detection
    int detection_samples;                 // Samples needed for detection
    double obstruction_threshold;          // Obstruction increase threshold
    double snr_degradation_threshold;      // SNR degradation threshold
    double temperature_threshold;          // Temperature threshold for snow
    int verification_time;                 // Verification time in seconds
    int melt_timeout;                      // Maximum melt time in seconds
    char weather_api_key[64];              // OpenWeatherMap API key
} starlink_snow_detection_config_t;

// Snow detection status
typedef struct {
    bool enabled;                          // System enabled status
    bool is_heating_active;                // Heating system status
    int consecutive_obstruction_samples;   // Current consecutive samples
    int total_detections;                  // Total snow detections
    int successful_melts;                  // Successful melt operations
    int false_positives;                   // False positive detections
    time_t last_clear_time;                // Last clear time
    time_t heating_start_time;             // Heating start time
    int heating_duration;                  // Heating duration in seconds
    snow_detection_context_t context;      // Current detection context
} starlink_snow_detection_status_t;

// Snow detection statistics
typedef struct {
    int total_detections;                  // Total detections
    int successful_melts;                  // Successful melts
    int false_positives;                   // False positives
    int prewarm_actions;                   // Pre-warming actions
    int melt_actions;                      // Melt actions
    int verify_actions;                    // Verification actions
    double average_melt_time;              // Average melt time
    double detection_accuracy;             // Detection accuracy percentage
    time_t last_detection;                 // Last detection time
    time_t last_successful_melt;           // Last successful melt time
} starlink_snow_detection_stats_t;

// Combined snow detection structure for internal use
typedef struct {
    // Configuration
    bool enabled;                          // Enable/disable snow detection
    int detection_samples;                 // Samples needed for detection
    double obstruction_threshold;          // Obstruction increase threshold
    double snr_degradation_threshold;      // SNR degradation threshold
    double temperature_threshold;          // Temperature threshold for snow
    int verification_time;                 // Verification time in seconds
    int melt_timeout;                      // Maximum melt time in seconds
    char weather_api_key[64];              // OpenWeatherMap API key
    
    // Status
    bool is_heating_active;                // Heating system status
    int consecutive_obstruction_samples;   // Current consecutive samples
    time_t last_clear_time;                // Last clear time
    time_t heating_start_time;             // Heating start time
    int heating_duration;                  // Heating duration in seconds
    snow_detection_context_t context;      // Current detection context
    
    // Statistics
    int total_detections;                  // Total detections
    int successful_melts;                  // Successful melts
    int false_positives;                   // False positives
    int prewarm_actions;                   // Pre-warming actions
    int melt_actions;                      // Melt actions
    int verify_actions;                    // Verification actions
    double average_melt_time;              // Average melt time
    double detection_accuracy;             // Detection accuracy percentage
    time_t last_detection;                 // Last detection time
    time_t last_successful_melt;           // Last successful melt time
    
    // Sample history
    snow_sample_t sample_history[MAX_SNOW_SAMPLES];
    int sample_count;                      // Number of samples collected
    
    // Additional fields for heating control
    double current_temperature;            // Current dish temperature
    time_t heating_timeout;                // Heating timeout timestamp
} starlink_snow_detection_t;

// API Functions

// Initialization and cleanup
int starlink_snow_detection_init(void);
void starlink_snow_detection_cleanup(void);

// Sample processing
int starlink_snow_detection_process_sample(const starlink_obstruction_sample_t *sample);

// Status and configuration
int starlink_snow_detection_get_status(starlink_snow_detection_status_t *status);
int starlink_snow_detection_get_config(starlink_snow_detection_config_t *config);
int starlink_snow_detection_set_config(const starlink_snow_detection_config_t *config);

// Control
int starlink_snow_detection_set_enabled(bool enabled);
int starlink_snow_detection_force_check(void);
int starlink_snow_detection_start_heating_manual(void);
int starlink_snow_detection_stop_heating_manual(void);

// Statistics
int starlink_snow_detection_get_statistics(starlink_snow_detection_stats_t *stats);
int starlink_snow_detection_reset_statistics(void);

// UCI Configuration
int starlink_snow_detection_load_uci_config(void);
int starlink_snow_detection_save_uci_config(void);

#ifdef __cplusplus
}
#endif

#endif // STARLINK_SNOW_DETECTION_H
