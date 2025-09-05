#include "validation_module.h"
#include "starlink_modules.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define DEFAULT_MAX_SAMPLES 1000
#define DEFAULT_VALIDATION_WINDOW 300 // 5 minutes
#define DEFAULT_ACCURACY_THRESHOLD 0.8
#define MIN_SAMPLES_FOR_TUNING 50

// Initialize validation module
validation_module_t* validation_module_init(const validation_config_t *config) {
    validation_module_t *module = calloc(1, sizeof(validation_module_t));
    if (!module) {
        return NULL;
    }
    
    // Set configuration
    if (config) {
        memcpy(&module->config, config, sizeof(validation_config_t));
    } else {
        validation_config_init_defaults(&module->config);
    }
    
    // Allocate sample history
    module->max_samples = DEFAULT_MAX_SAMPLES;
    module->samples = calloc(module->max_samples, sizeof(validation_sample_t));
    if (!module->samples) {
        free(module);
        return NULL;
    }
    
    // Initialize thresholds
    module->current_obstruction_threshold = 0.7; // Default
    module->current_elevation_threshold = 10.0;  // Default
    module->thresholds_tuned = false;
    
    return module;
}

// Cleanup validation module
void validation_module_cleanup(validation_module_t *module) {
    if (!module) {
        return;
    }
    
    if (module->samples) {
        free(module->samples);
    }
    
    free(module);
}

// Initialize default configuration
void validation_config_init_defaults(validation_config_t *config) {
    if (!config) {
        return;
    }
    
    config->enabled = true;
    config->validation_window_minutes = 5;
    config->accuracy_threshold = DEFAULT_ACCURACY_THRESHOLD;
    config->min_samples_for_tuning = MIN_SAMPLES_FOR_TUNING;
    config->auto_tune_thresholds = true;
    config->tuning_sensitivity = 0.1;
}

// Update connectivity state from Starlink API
int validation_module_update_connectivity_state(validation_module_t *module) {
    if (!module) {
        return VALIDATION_ERROR_INVALID_PARAM;
    }
    
    // Get current Starlink status
    starlink_status_response_t status;
    int result = starlink_get_status(&status);
    
    if (result == 0) {
        module->current_state.timestamp = time(NULL);
        module->current_state.is_connected = !status.obstruction_stats.currently_obstructed;
        module->current_state.signal_strength = (int)(status.signal_quality.snr * 100);
        module->current_state.throughput_mbps = status.network_perf.downlink_throughput_bps / 1e6;
        module->current_state.ping_latency_ms = (int)status.network_perf.pop_ping_latency_ms;
        module->current_state.obstruction_detected = status.obstruction_stats.currently_obstructed;
        
        snprintf(module->current_state.status_description, sizeof(module->current_state.status_description),
                "SNR: %.2f, Latency: %dms, Obstructed: %s",
                status.signal_quality.snr,
                module->current_state.ping_latency_ms,
                module->current_state.obstruction_detected ? "Yes" : "No");
        
        module->last_state_update = time(NULL);
        return VALIDATION_SUCCESS;
    }
    
    return VALIDATION_ERROR_INVALID_STATE;
}

// Get current connectivity state
const connectivity_state_t* validation_module_get_current_state(const validation_module_t *module) {
    if (!module) {
        static connectivity_state_t empty_state = {0};
        return &empty_state;
    }
    
    return &module->current_state;
}

// Validate a prediction against actual outcome
int validation_module_validate_prediction(
    validation_module_t *module,
    const outage_prediction_t *prediction,
    const connectivity_state_t *actual_state) {
    
    if (!module || !prediction || !actual_state) {
        return VALIDATION_ERROR_INVALID_PARAM;
    }
    
    if (!module->config.enabled) {
        return VALIDATION_SUCCESS; // Validation disabled
    }
    
    // Create validation sample
    validation_sample_t sample;
    sample.prediction_time = prediction->start_time;
    sample.validation_time = actual_state->timestamp;
    memcpy(&sample.prediction, prediction, sizeof(outage_prediction_t));
    memcpy(&sample.actual_state, actual_state, sizeof(connectivity_state_t));
    
    // Determine if prediction was accurate
    bool predicted_outage = (prediction->risk_level >= RISK_LEVEL_HIGH);
    bool actual_outage = validation_module_is_outage_state(actual_state);
    
    sample.prediction_accurate = (predicted_outage == actual_outage);
    sample.accuracy_score = validation_module_calculate_accuracy_score(prediction, actual_state);
    
    snprintf(sample.validation_notes, sizeof(sample.validation_notes),
            "Predicted: %s, Actual: %s, Score: %.2f",
            predicted_outage ? "Outage" : "Normal",
            actual_outage ? "Outage" : "Normal",
            sample.accuracy_score);
    
    // Add sample to history
    int add_result = validation_module_add_sample(module, &sample);
    
    // Update metrics
    module->metrics = validation_module_calculate_metrics(module);
    module->total_validations++;
    if (sample.prediction_accurate) {
        module->successful_validations++;
    }
    module->last_validation = time(NULL);
    
    // Check if we should tune thresholds
    if (module->config.auto_tune_thresholds && validation_module_should_tune_thresholds(module)) {
        double new_obstruction_threshold, new_elevation_threshold;
        validation_module_tune_thresholds(module, &new_obstruction_threshold, &new_elevation_threshold);
        
        if (module->log_callback) {
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg), 
                    "Auto-tuned thresholds: obstruction=%.3f, elevation=%.1f", 
                    new_obstruction_threshold, new_elevation_threshold);
            module->log_callback(1, log_msg, module->log_user_data);
        }
    }
    
    return add_result;
}

// Add validation sample to history
int validation_module_add_sample(validation_module_t *module, const validation_sample_t *sample) {
    if (!module || !sample) {
        return VALIDATION_ERROR_INVALID_PARAM;
    }
    
    // Add to ring buffer
    memcpy(&module->samples[module->sample_index], sample, sizeof(validation_sample_t));
    module->sample_index = (module->sample_index + 1) % module->max_samples;
    
    if (module->num_samples < module->max_samples) {
        module->num_samples++;
    }
    
    return VALIDATION_SUCCESS;
}

// Calculate validation metrics
validation_metrics_t validation_module_calculate_metrics(const validation_module_t *module) {
    validation_metrics_t metrics = {0};
    
    if (!module || module->num_samples == 0) {
        return metrics;
    }
    
    // Count outcomes
    for (int i = 0; i < module->num_samples; i++) {
        const validation_sample_t *sample = &module->samples[i];
        
        bool predicted_outage = (sample->prediction.risk_level >= RISK_LEVEL_HIGH);
        bool actual_outage = validation_module_is_outage_state(&sample->actual_state);
        
        if (predicted_outage && actual_outage) {
            metrics.true_positives++;
        } else if (!predicted_outage && !actual_outage) {
            metrics.true_negatives++;
        } else if (predicted_outage && !actual_outage) {
            metrics.false_positives++;
        } else if (!predicted_outage && actual_outage) {
            metrics.false_negatives++;
        }
    }
    
    // Calculate derived metrics
    int total = metrics.true_positives + metrics.true_negatives + 
                metrics.false_positives + metrics.false_negatives;
    
    if (total > 0) {
        metrics.accuracy = (double)(metrics.true_positives + metrics.true_negatives) / total;
    }
    
    if ((metrics.true_positives + metrics.false_positives) > 0) {
        metrics.precision = (double)metrics.true_positives / 
                           (metrics.true_positives + metrics.false_positives);
    }
    
    if ((metrics.true_positives + metrics.false_negatives) > 0) {
        metrics.recall = (double)metrics.true_positives / 
                        (metrics.true_positives + metrics.false_negatives);
    }
    
    if ((metrics.precision + metrics.recall) > 0) {
        metrics.f1_score = 2.0 * (metrics.precision * metrics.recall) / 
                          (metrics.precision + metrics.recall);
    }
    
    return metrics;
}

// Check if connectivity state represents an outage
bool validation_module_is_outage_state(const connectivity_state_t *state) {
    if (!state) {
        return true; // Assume outage if no state
    }
    
    // Consider it an outage if:
    // 1. Not connected, OR
    // 2. Very high latency (>2000ms), OR
    // 3. Very low throughput (<1 Mbps) AND obstruction detected
    return (!state->is_connected ||
            state->ping_latency_ms > 2000 ||
            (state->throughput_mbps < 1.0 && state->obstruction_detected));
}

// Calculate accuracy score for a prediction
double validation_module_calculate_accuracy_score(
    const outage_prediction_t *prediction,
    const connectivity_state_t *actual_state) {
    
    if (!prediction || !actual_state) {
        return 0.0;
    }
    
    bool predicted_outage = (prediction->risk_level >= RISK_LEVEL_HIGH);
    bool actual_outage = validation_module_is_outage_state(actual_state);
    
    // Base score for correct/incorrect prediction
    double base_score = (predicted_outage == actual_outage) ? 1.0 : 0.0;
    
    // Adjust score based on confidence and risk level accuracy
    if (base_score > 0.0) {
        // Bonus for high confidence when correct
        base_score *= prediction->confidence_score;
        
        // Additional bonus for appropriate risk level
        if (actual_outage) {
            // Higher risk levels should get bonus for actual outages
            base_score *= (1.0 + (prediction->risk_level - 1) * 0.1);
        }
    } else {
        // Penalty is reduced if confidence was low
        base_score = -(1.0 - prediction->confidence_score) * 0.5;
    }
    
    // Clamp to [0, 1] range
    return fmax(0.0, fmin(1.0, base_score));
}

// Tune thresholds based on validation history
int validation_module_tune_thresholds(
    validation_module_t *module,
    double *obstruction_threshold,
    double *elevation_threshold) {
    
    if (!module || !obstruction_threshold || !elevation_threshold) {
        return VALIDATION_ERROR_INVALID_PARAM;
    }
    
    if (module->num_samples < module->config.min_samples_for_tuning) {
        return VALIDATION_ERROR_NO_DATA;
    }
    
    // Analyze false positives and false negatives to adjust thresholds
    int false_positives = 0;
    int false_negatives = 0;
    
    for (int i = 0; i < module->num_samples; i++) {
        const validation_sample_t *sample = &module->samples[i];
        
        bool predicted_outage = (sample->prediction.risk_level >= RISK_LEVEL_HIGH);
        bool actual_outage = validation_module_is_outage_state(&sample->actual_state);
        
        if (predicted_outage && !actual_outage) {
            false_positives++;
            // For false positives, we might want to increase obstruction threshold
        } else if (!predicted_outage && actual_outage) {
            false_negatives++;
            // For false negatives, we might want to decrease obstruction threshold
        }
    }
    
    // Calculate threshold adjustments
    double current_accuracy = validation_module_get_current_accuracy(module);
    
    if (current_accuracy < module->config.accuracy_threshold) {
        // Adjust thresholds to improve accuracy
        if (false_positives > false_negatives) {
            // Too many false positives - increase obstruction threshold (be more conservative)
            *obstruction_threshold = module->current_obstruction_threshold + module->config.tuning_sensitivity;
        } else if (false_negatives > false_positives) {
            // Too many false negatives - decrease obstruction threshold (be more sensitive)
            *obstruction_threshold = module->current_obstruction_threshold - module->config.tuning_sensitivity;
        } else {
            *obstruction_threshold = module->current_obstruction_threshold;
        }
        
        // Clamp obstruction threshold to reasonable range
        *obstruction_threshold = fmax(0.1, fmin(1.0, *obstruction_threshold));
        
        // For now, keep elevation threshold constant
        *elevation_threshold = module->current_elevation_threshold;
        
        // Update stored thresholds
        module->current_obstruction_threshold = *obstruction_threshold;
        module->current_elevation_threshold = *elevation_threshold;
        module->thresholds_tuned = true;
        
        return VALIDATION_SUCCESS;
    }
    
    // No tuning needed - return current thresholds
    *obstruction_threshold = module->current_obstruction_threshold;
    *elevation_threshold = module->current_elevation_threshold;
    
    return VALIDATION_SUCCESS;
}

// Check if thresholds should be tuned
bool validation_module_should_tune_thresholds(const validation_module_t *module) {
    if (!module || !module->config.auto_tune_thresholds) {
        return false;
    }
    
    if (module->num_samples < module->config.min_samples_for_tuning) {
        return false;
    }
    
    double current_accuracy = validation_module_get_current_accuracy(module);
    return (current_accuracy < module->config.accuracy_threshold);
}

// Get current accuracy
double validation_module_get_current_accuracy(const validation_module_t *module) {
    if (!module || module->num_samples == 0) {
        return 0.0;
    }
    
    validation_metrics_t metrics = validation_module_calculate_metrics(module);
    return metrics.accuracy;
}

// Generate validation report
validation_report_t validation_module_generate_report(const validation_module_t *module) {
    validation_report_t report = {0};
    
    if (!module) {
        return report;
    }
    
    report.metrics = validation_module_calculate_metrics(module);
    report.current_obstruction_threshold = module->current_obstruction_threshold;
    report.current_elevation_threshold = module->current_elevation_threshold;
    report.recent_samples = module->num_samples;
    report.report_time = time(NULL);
    
    // Generate summary
    snprintf(report.summary, sizeof(report.summary),
            "Validation Report: %d samples, %.1f%% accuracy, %.1f%% precision, %.1f%% recall, "
            "Thresholds: obs=%.3f, el=%.1f°",
            module->num_samples,
            report.metrics.accuracy * 100.0,
            report.metrics.precision * 100.0,
            report.metrics.recall * 100.0,
            report.current_obstruction_threshold,
            report.current_elevation_threshold);
    
    return report;
}

// Get recent validation samples
int validation_module_get_recent_samples(
    const validation_module_t *module,
    int num_samples,
    validation_sample_t **samples) {
    
    if (!module || !samples || num_samples <= 0) {
        return VALIDATION_ERROR_INVALID_PARAM;
    }
    
    int available_samples = fmin(num_samples, module->num_samples);
    if (available_samples == 0) {
        *samples = NULL;
        return 0;
    }
    
    *samples = calloc(available_samples, sizeof(validation_sample_t));
    if (!*samples) {
        return VALIDATION_ERROR_MEMORY_FAILED;
    }
    
    // Copy most recent samples
    int start_index = (module->sample_index - available_samples + module->max_samples) % module->max_samples;
    
    for (int i = 0; i < available_samples; i++) {
        int sample_index = (start_index + i) % module->max_samples;
        memcpy(&(*samples)[i], &module->samples[sample_index], sizeof(validation_sample_t));
    }
    
    return available_samples;
}

// Clear validation samples
void validation_module_clear_samples(validation_module_t *module) {
    if (!module) {
        return;
    }
    
    module->num_samples = 0;
    module->sample_index = 0;
    memset(&module->metrics, 0, sizeof(validation_metrics_t));
}

// Update connectivity state from current Starlink status
int validation_module_update_from_starlink(validation_module_t *module) {
    if (!module) {
        return VALIDATION_ERROR_INVALID_PARAM;
    }
    
    // This function would integrate with the existing Starlink client
    // to get real-time connectivity status
    return validation_module_update_connectivity_state(module);
}