#ifndef VALIDATION_MODULE_H
#define VALIDATION_MODULE_H

#include "starlink_tracker.h"
#include "prediction_engine.h"
#include <time.h>
#include <stdbool.h>

// Validation configuration
typedef struct {
    bool enabled;
    int validation_window_minutes;
    double accuracy_threshold;
    int min_samples_for_tuning;
    bool auto_tune_thresholds;
    double tuning_sensitivity;
} validation_config_t;

// Validation metrics
typedef struct {
    int true_positives;     // Correctly predicted outages
    int true_negatives;     // Correctly predicted no outages
    int false_positives;    // Predicted outages that didn't happen
    int false_negatives;    // Missed actual outages
    double precision;       // TP / (TP + FP)
    double recall;          // TP / (TP + FN)
    double f1_score;        // 2 * (precision * recall) / (precision + recall)
    double accuracy;        // (TP + TN) / (TP + TN + FP + FN)
} validation_metrics_t;

// Real-time connectivity state
typedef struct {
    time_t timestamp;
    bool is_connected;
    int signal_strength;
    double throughput_mbps;
    int ping_latency_ms;
    bool obstruction_detected;
    char status_description[128];
} connectivity_state_t;

// Validation sample
typedef struct {
    time_t prediction_time;
    time_t validation_time;
    outage_prediction_t prediction;
    connectivity_state_t actual_state;
    bool prediction_accurate;
    double accuracy_score;
    char validation_notes[256];
} validation_sample_t;

// Validation module structure
typedef struct {
    validation_config_t config;
    validation_metrics_t metrics;
    
    // Sample history
    validation_sample_t *samples;
    int num_samples;
    int max_samples;
    int sample_index; // Ring buffer index
    
    // Real-time monitoring
    connectivity_state_t current_state;
    time_t last_state_update;
    
    // Threshold tuning
    double current_obstruction_threshold;
    double current_elevation_threshold;
    bool thresholds_tuned;
    
    // Statistics
    int total_validations;
    int successful_validations;
    time_t last_validation;
    
    // Logging callback
    void (*log_callback)(int level, const char *message, void *user_data);
    void *log_user_data;
} validation_module_t;

// API Functions

// Initialization and cleanup
validation_module_t* validation_module_init(const validation_config_t *config);
void validation_module_cleanup(validation_module_t *module);

// Configuration management
int validation_module_update_config(validation_module_t *module, const validation_config_t *config);
const validation_config_t* validation_module_get_config(const validation_module_t *module);

// Real-time state monitoring
int validation_module_update_connectivity_state(validation_module_t *module);
const connectivity_state_t* validation_module_get_current_state(const validation_module_t *module);

// Prediction validation
int validation_module_validate_prediction(
    validation_module_t *module,
    const outage_prediction_t *prediction,
    const connectivity_state_t *actual_state
);

int validation_module_add_sample(
    validation_module_t *module,
    const validation_sample_t *sample
);

// Metrics calculation
validation_metrics_t validation_module_calculate_metrics(const validation_module_t *module);
double validation_module_get_current_accuracy(const validation_module_t *module);

// Threshold tuning
int validation_module_tune_thresholds(
    validation_module_t *module,
    double *obstruction_threshold,
    double *elevation_threshold
);

bool validation_module_should_tune_thresholds(const validation_module_t *module);

// Reporting
typedef struct {
    validation_metrics_t metrics;
    double current_obstruction_threshold;
    double current_elevation_threshold;
    int recent_samples;
    time_t report_time;
    char summary[512];
} validation_report_t;

validation_report_t validation_module_generate_report(const validation_module_t *module);

// Sample management
int validation_module_get_recent_samples(
    const validation_module_t *module,
    int num_samples,
    validation_sample_t **samples
);

void validation_module_clear_samples(validation_module_t *module);

// Utility functions
bool validation_module_is_outage_state(const connectivity_state_t *state);
double validation_module_calculate_accuracy_score(
    const outage_prediction_t *prediction,
    const connectivity_state_t *actual_state
);

// Configuration helpers
void validation_config_init_defaults(validation_config_t *config);

// Error codes
#define VALIDATION_SUCCESS                 0
#define VALIDATION_ERROR_INVALID_PARAM    -1
#define VALIDATION_ERROR_NOT_INITIALIZED  -2
#define VALIDATION_ERROR_MEMORY_FAILED    -3
#define VALIDATION_ERROR_NO_DATA          -4
#define VALIDATION_ERROR_INVALID_STATE    -5

#endif // VALIDATION_MODULE_H