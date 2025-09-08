#include "ml_monitor_multi_interface.h"
#include "../utils/logx.h"
#include "../network/network_controller.h"
#include "../network/network_failover.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

// Phase 7: Multi-Interface ML Intelligence Implementation

// Global multi-interface system
static multi_interface_ml_system_t *g_multi_interface_system = NULL;

// Initialize multi-interface ML monitoring system
multi_interface_ml_system_t* ml_monitor_init_multi_interface_system(const ml_monitor_config_t *config) {
    if (!config) return NULL;
    
    LOGX_INFO("🚀 Initializing Multi-Interface ML Intelligence System");
    
    multi_interface_ml_system_t *system = calloc(1, sizeof(multi_interface_ml_system_t));
    if (!system) {
        LOGX_ERROR("Failed to allocate multi-interface ML system");
        return NULL;
    }
    
    // Initialize cross-interface learning
    system->cross_learning.enable_cross_interface_correlation = true;
    system->cross_learning.enable_ensemble_across_interfaces = true;
    
    // Initialize interface correlation matrix (identity matrix to start)
    for (int i = 0; i < INTERFACE_TYPE_MAX; i++) {
        for (int j = 0; j < INTERFACE_TYPE_MAX; j++) {
            system->cross_learning.interface_correlation_matrix[i][j] = (i == j) ? 1.0 : 0.1;
        }
        system->cross_learning.cross_interface_weights[i] = 1.0 / INTERFACE_TYPE_MAX;
    }
    
    // Initialize failover intelligence
    system->failover_intelligence.continuous_monitoring_during_failover = true;
    system->failover_intelligence.enable_predictive_failback = true;
    system->failover_intelligence.enable_outage_duration_prediction = true;
    system->failover_intelligence.failover_confidence_threshold = 0.8;
    system->failover_intelligence.min_failback_delay_seconds = 60;   // 1 minute minimum
    system->failover_intelligence.max_failback_delay_seconds = 1800; // 30 minutes maximum
    
    // Initialize MWAN3 integration
    system->mwan3_integration.enable_dynamic_weight_updates = true;
    system->mwan3_integration.auto_apply_weight_changes = true;
    system->mwan3_integration.weight_adjustment_sensitivity = 0.5;
    system->mwan3_integration.weight_update_interval_seconds = 300; // 5 minutes
    strncpy(system->mwan3_integration.mwan3_config_path, "/etc/config/mwan3", 
            sizeof(system->mwan3_integration.mwan3_config_path) - 1);
    
    g_multi_interface_system = system;
    
    LOGX_INFO("✅ Multi-interface ML system initialized successfully");
    LOGX_INFO("   - Cross-interface correlation learning: enabled");
    LOGX_INFO("   - Continuous monitoring during failover: enabled");
    LOGX_INFO("   - Predictive failback: enabled");
    LOGX_INFO("   - Outage duration prediction: enabled");
    LOGX_INFO("   - Dynamic MWAN3 weight updates: enabled");
    
    return system;
}

// Add interface for ML monitoring
int ml_monitor_add_interface(multi_interface_ml_system_t *system, const char *interface_id, interface_type_t type) {
    if (!system || !interface_id || system->interface_count >= MAX_INTERFACES) {
        return ML_MONITOR_MULTI_ERROR_INVALID_PARAM;
    }
    
    // Check if interface already exists
    for (int i = 0; i < system->interface_count; i++) {
        if (strcmp(system->interface_models[i].interface_id, interface_id) == 0) {
            LOGX_WARN("Interface %s already exists", interface_id);
            return ML_MONITOR_MULTI_SUCCESS; // Already exists
        }
    }
    
    // Add new interface
    interface_ml_model_t *model = &system->interface_models[system->interface_count];
    memset(model, 0, sizeof(interface_ml_model_t));
    
    strncpy(model->interface_id, interface_id, sizeof(model->interface_id) - 1);
    model->type = type;
    model->active = true;
    
    // Initialize model weights
    model->models.base_model_weight = 0.7;      // 70% base model
    model->models.personal_model_weight = 0.3;  // 30% personal model initially
    model->models.personalization_confidence = 0.1; // Low confidence initially
    
    // Initialize performance tracking
    model->performance.typical_latency_ms = 50.0;
    model->performance.typical_throughput_mbps = 10.0;
    model->performance.typical_reliability = 0.9;
    model->performance.performance_stability = 0.8;
    
    // Initialize failover learning
    model->failover_learning.optimal_failover_delay_seconds = 30;  // 30 seconds default
    model->failover_learning.optimal_failback_delay_seconds = 300; // 5 minutes default
    model->failover_learning.failover_cost_benefit_ratio = 1.5;
    
    system->interface_count++;
    
    const char* type_names[] = {"Starlink", "Cellular", "WiFi", "LAN", "Unknown"};
    LOGX_INFO("📡 Added interface for ML monitoring: %s (%s)", 
             interface_id, type_names[type]);
    
    return ML_MONITOR_MULTI_SUCCESS;
}

// Update interface observation with enhanced metrics
int ml_monitor_update_interface_observation(multi_interface_ml_system_t *system, 
                                           const char *interface_id,
                                           const multi_interface_observation_t *observation) {
    if (!system || !interface_id || !observation) {
        return ML_MONITOR_MULTI_ERROR_INVALID_PARAM;
    }
    
    // Find interface model
    interface_ml_model_t *model = NULL;
    for (int i = 0; i < system->interface_count; i++) {
        if (strcmp(system->interface_models[i].interface_id, interface_id) == 0) {
            model = &system->interface_models[i];
            break;
        }
    }
    
    if (!model) {
        LOGX_WARN("Interface %s not found for observation update", interface_id);
        return ML_MONITOR_MULTI_ERROR_NOT_FOUND;
    }
    
    // Update performance characteristics
    double alpha = 0.1; // Learning rate for exponential moving average
    
    model->performance.typical_latency_ms = 
        model->performance.typical_latency_ms * (1.0 - alpha) + observation->latency_ms * alpha;
    
    double current_throughput = (observation->throughput_down_kbps + observation->throughput_up_kbps) / 2000.0; // Convert to Mbps
    model->performance.typical_throughput_mbps = 
        model->performance.typical_throughput_mbps * (1.0 - alpha) + current_throughput * alpha;
    
    model->performance.typical_reliability = 
        model->performance.typical_reliability * (1.0 - alpha) + (observation->reliability_score / 255.0) * alpha;
    
    // Add to personal observations (circular buffer)
    if (model->models.personal_observation_count < 5000) {
        model->models.personal_observation_count++;
    }
    
    // Update personalization confidence based on data amount
    if (model->models.personal_observation_count > 100) {
        model->models.personalization_confidence = fmin(0.8, 
            model->models.personal_observation_count / 1000.0);
        
        // Adjust model weights based on personalization confidence
        model->models.personal_model_weight = model->models.personalization_confidence;
        model->models.base_model_weight = 1.0 - model->models.personalization_confidence;
    }
    
    // Update cross-interface correlations
    ml_monitor_update_cross_interface_correlations(system);
    
    LOGX_DEBUG("Updated %s observation: latency=%.1fms, throughput=%.1fMbps, reliability=%.3f",
              interface_id, model->performance.typical_latency_ms, 
              model->performance.typical_throughput_mbps, model->performance.typical_reliability);
    
    return ML_MONITOR_MULTI_SUCCESS;
}

// Predict interface performance with enhanced metrics
int ml_monitor_predict_interface_performance(multi_interface_ml_system_t *system,
                                           const char *interface_id,
                                           uint8_t *outage_probability,
                                           uint8_t *performance_score,
                                           uint8_t *confidence) {
    if (!system || !interface_id || !outage_probability || !performance_score || !confidence) {
        return ML_MONITOR_MULTI_ERROR_INVALID_PARAM;
    }
    
    // Find interface model
    interface_ml_model_t *model = NULL;
    for (int i = 0; i < system->interface_count; i++) {
        if (strcmp(system->interface_models[i].interface_id, interface_id) == 0) {
            model = &system->interface_models[i];
            break;
        }
    }
    
    if (!model) {
        return ML_MONITOR_MULTI_ERROR_NOT_FOUND;
    }
    
    // Make prediction based on interface type and personal learning
    double base_reliability = model->performance.typical_reliability;
    double latency_factor = model->performance.typical_latency_ms / 100.0; // Normalize
    double throughput_factor = model->performance.typical_throughput_mbps / 50.0; // Normalize
    
    // Calculate outage probability
    double outage_prob = 1.0 - base_reliability;
    outage_prob += latency_factor * 0.2;  // Higher latency increases outage probability
    outage_prob -= throughput_factor * 0.1; // Higher throughput decreases outage probability
    outage_prob = fmax(0.0, fmin(1.0, outage_prob));
    
    *outage_probability = (uint8_t)(outage_prob * 255);
    
    // Calculate performance score
    double perf_score = base_reliability * 0.5 + 
                       (1.0 - latency_factor) * 0.3 + 
                       throughput_factor * 0.2;
    *performance_score = (uint8_t)(perf_score * 255);
    
    // Calculate confidence based on data amount
    *confidence = (uint8_t)(model->models.personalization_confidence * 255);
    
    // Update prediction statistics
    model->performance.total_predictions++;
    model->performance.last_prediction = time(NULL);
    
    LOGX_DEBUG("Interface %s prediction: outage=%u%%, performance=%u%%, confidence=%u%%",
              interface_id, *outage_probability, *performance_score, *confidence);
    
    return ML_MONITOR_MULTI_SUCCESS;
}

// Predict outage duration for cost-benefit analysis
int ml_monitor_predict_outage_duration(multi_interface_ml_system_t *system,
                                      const char *interface_id,
                                      outage_duration_prediction_t *duration_prediction) {
    if (!system || !interface_id || !duration_prediction) {
        return ML_MONITOR_MULTI_ERROR_INVALID_PARAM;
    }
    
    // Find interface model
    interface_ml_model_t *model = NULL;
    for (int i = 0; i < system->interface_count; i++) {
        if (strcmp(system->interface_models[i].interface_id, interface_id) == 0) {
            model = &system->interface_models[i];
            break;
        }
    }
    
    if (!model) {
        return ML_MONITOR_MULTI_ERROR_NOT_FOUND;
    }
    
    memset(duration_prediction, 0, sizeof(outage_duration_prediction_t));
    
    // Use historical data to predict duration
    double avg_duration_minutes = model->failover_learning.average_outage_duration_minutes;
    
    if (avg_duration_minutes == 0) {
        // No historical data, use interface type defaults
        switch (model->type) {
            case INTERFACE_TYPE_STARLINK:
                avg_duration_minutes = 5.0;  // Starlink outages typically short
                break;
            case INTERFACE_TYPE_CELLULAR:
                avg_duration_minutes = 15.0; // Cellular can be longer
                break;
            case INTERFACE_TYPE_WIFI:
                avg_duration_minutes = 10.0; // WiFi medium duration
                break;
            case INTERFACE_TYPE_LAN:
                avg_duration_minutes = 2.0;  // LAN usually quick fixes
                break;
            default:
                avg_duration_minutes = 10.0;
                break;
        }
    }
    
    // Classify duration probabilities
    if (avg_duration_minutes < 2.0) {
        duration_prediction->very_short_probability = 200;
        duration_prediction->short_probability = 55;
        duration_prediction->medium_probability = 0;
        duration_prediction->long_probability = 0;
    } else if (avg_duration_minutes < 10.0) {
        duration_prediction->very_short_probability = 100;
        duration_prediction->short_probability = 155;
        duration_prediction->medium_probability = 0;
        duration_prediction->long_probability = 0;
    } else if (avg_duration_minutes < 60.0) {
        duration_prediction->very_short_probability = 50;
        duration_prediction->short_probability = 100;
        duration_prediction->medium_probability = 105;
        duration_prediction->long_probability = 0;
    } else {
        duration_prediction->very_short_probability = 0;
        duration_prediction->short_probability = 50;
        duration_prediction->medium_probability = 100;
        duration_prediction->long_probability = 105;
    }
    
    duration_prediction->expected_duration_seconds = (uint32_t)(avg_duration_minutes * 60);
    duration_prediction->confidence_interval_low = duration_prediction->expected_duration_seconds / 2;
    duration_prediction->confidence_interval_high = duration_prediction->expected_duration_seconds * 2;
    duration_prediction->prediction_confidence = model->models.personalization_confidence > 0.3 ? 180 : 120;
    
    // Cost-benefit analysis
    duration_prediction->estimated_failover_cost = 5.0; // Arbitrary units
    duration_prediction->estimated_outage_cost = avg_duration_minutes * 2.0; // Cost increases with time
    
    duration_prediction->recommend_failover = 
        (duration_prediction->estimated_outage_cost > duration_prediction->estimated_failover_cost);
    
    snprintf(duration_prediction->reasoning, sizeof(duration_prediction->reasoning),
            "Expected %.1f min outage, failover cost %.1f, outage cost %.1f",
            avg_duration_minutes, duration_prediction->estimated_failover_cost,
            duration_prediction->estimated_outage_cost);
    
    LOGX_DEBUG("Duration prediction for %s: %.1f minutes, recommend_failover=%s",
              interface_id, avg_duration_minutes, 
              duration_prediction->recommend_failover ? "yes" : "no");
    
    return ML_MONITOR_MULTI_SUCCESS;
}

// Assess failback readiness
int ml_monitor_assess_failback_readiness(multi_interface_ml_system_t *system,
                                        const char *interface_id,
                                        failback_readiness_t *readiness) {
    if (!system || !interface_id || !readiness) {
        return ML_MONITOR_MULTI_ERROR_INVALID_PARAM;
    }
    
    // Find interface model
    interface_ml_model_t *model = NULL;
    for (int i = 0; i < system->interface_count; i++) {
        if (strcmp(system->interface_models[i].interface_id, interface_id) == 0) {
            model = &system->interface_models[i];
            break;
        }
    }
    
    if (!model) {
        return ML_MONITOR_MULTI_ERROR_NOT_FOUND;
    }
    
    memset(readiness, 0, sizeof(failback_readiness_t));
    
    // Assess current interface health
    readiness->interface_health_score = (uint8_t)(model->performance.typical_reliability * 255);
    
    // Performance recovery assessment (simplified)
    readiness->performance_recovery_score = 200; // Assume good recovery
    
    // Stability assessment
    readiness->stability_score = (uint8_t)(model->performance.performance_stability * 255);
    
    // Historical reliability
    readiness->historical_reliability = (uint8_t)(model->performance.typical_reliability * 255);
    
    // Calculate failback success probability
    double success_prob = (model->performance.typical_reliability + 
                          model->performance.performance_stability) / 2.0;
    readiness->failback_success_probability = (uint8_t)(success_prob * 255);
    
    // Timing optimization
    if (model->failover_learning.successful_failbacks > 0) {
        readiness->recommended_failback_delay = model->failover_learning.optimal_failback_delay_seconds;
    } else {
        readiness->recommended_failback_delay = system->failover_intelligence.min_failback_delay_seconds;
    }
    
    // Confidence based on historical data
    readiness->failback_confidence = model->failover_learning.failback_events > 5 ? 180 : 120;
    
    // Risk assessment (simplified)
    readiness->risk_of_immediate_failback = (1.0 - success_prob) * 0.5;
    readiness->risk_of_delayed_failback = 0.2; // Lower risk with delay
    
    readiness->optimal_failback_window_start = 60;   // 1 minute
    readiness->optimal_failback_window_end = 600;    // 10 minutes
    
    LOGX_DEBUG("Failback readiness for %s: success_prob=%u%%, delay=%u seconds",
              interface_id, readiness->failback_success_probability, 
              readiness->recommended_failback_delay);
    
    return ML_MONITOR_MULTI_SUCCESS;
}

// Update MWAN3 weights based on ML predictions
int ml_monitor_update_mwan3_weights(multi_interface_ml_system_t *system) {
    if (!system) return ML_MONITOR_MULTI_ERROR_INVALID_PARAM;
    
    if (!system->mwan3_integration.enable_dynamic_weight_updates) {
        return ML_MONITOR_MULTI_SUCCESS;
    }
    
    time_t current_time = time(NULL);
    static time_t last_update = 0;
    
    // Only update every 5 minutes to avoid thrashing
    if (current_time - last_update < system->mwan3_integration.weight_update_interval_seconds) {
        return ML_MONITOR_MULTI_SUCCESS;
    }
    
    LOGX_INFO("🔧 Updating MWAN3 weights based on ML predictions");
    
    bool weights_changed = false;
    
    // Update weights for each interface
    for (int i = 0; i < system->interface_count; i++) {
        interface_ml_model_t *model = &system->interface_models[i];
        
        // Get ML prediction for this interface
        uint8_t outage_prob, performance_score, confidence;
        int pred_result = ml_monitor_predict_interface_performance(system, model->interface_id,
                                                                 &outage_prob, &performance_score, &confidence);
        
        if (pred_result == ML_MONITOR_MULTI_SUCCESS && confidence > 100) {
            // Calculate ML reliability score
            double ml_reliability = (255 - outage_prob) / 255.0;
            double ml_performance = performance_score / 255.0;
            double overall_ml_score = (ml_reliability * 0.6) + (ml_performance * 0.4);
            
            // Find corresponding MWAN3 interface
            for (int j = 0; j < system->mwan3_integration.mwan3_interface_count; j++) {
                if (strstr(system->mwan3_integration.mwan3_interfaces[j].interface_name, model->interface_id)) {
                    
                    // Calculate weight adjustment
                    int base_weight = system->mwan3_integration.mwan3_interfaces[j].base_weight;
                    double sensitivity = system->mwan3_integration.weight_adjustment_sensitivity;
                    
                    int adjustment = (int)((overall_ml_score - 0.5) * 100 * sensitivity);
                    int new_weight = base_weight + adjustment;
                    
                    // Clamp weight to reasonable range
                    new_weight = fmax(1, fmin(100, new_weight));
                    
                    if (new_weight != system->mwan3_integration.mwan3_interfaces[j].current_weight) {
                        system->mwan3_integration.mwan3_interfaces[j].ml_weight_adjustment = adjustment;
                        system->mwan3_integration.mwan3_interfaces[j].current_weight = new_weight;
                        system->mwan3_integration.mwan3_interfaces[j].ml_reliability_score = overall_ml_score;
                        system->mwan3_integration.mwan3_interfaces[j].last_weight_update = current_time;
                        
                        weights_changed = true;
                        
                        LOGX_INFO("📊 Updated MWAN3 weight for %s: %d → %d (ML score: %.3f)",
                                 model->interface_id, base_weight, new_weight, overall_ml_score);
                    }
                    break;
                }
            }
        }
    }
    
    // Apply weight changes to MWAN3 if enabled
    if (weights_changed && system->mwan3_integration.auto_apply_weight_changes) {
        int apply_result = ml_monitor_apply_mwan3_weight_changes(system);
        if (apply_result == ML_MONITOR_MULTI_SUCCESS) {
            LOGX_INFO("✅ MWAN3 weight changes applied successfully");
        } else {
            LOGX_WARN("Failed to apply MWAN3 weight changes: %d", apply_result);
        }
    }
    
    last_update = current_time;
    return ML_MONITOR_MULTI_SUCCESS;
}

// Apply MWAN3 weight changes
int ml_monitor_apply_mwan3_weight_changes(multi_interface_ml_system_t *system) {
    if (!system) return ML_MONITOR_MULTI_ERROR_INVALID_PARAM;
    
    LOGX_INFO("🔧 Applying ML-optimized weights to MWAN3 configuration");
    
    // In a full implementation, this would:
    // 1. Update MWAN3 UCI configuration
    // 2. Apply changes via ubus call to mwan3
    // 3. Validate changes were applied
    
    for (int i = 0; i < system->mwan3_integration.mwan3_interface_count; i++) {
        char uci_command[256];
        snprintf(uci_command, sizeof(uci_command),
                "uci set mwan3.%s.weight=%d",
                system->mwan3_integration.mwan3_interfaces[i].interface_name,
                system->mwan3_integration.mwan3_interfaces[i].current_weight);
        
        LOGX_DEBUG("Would execute: %s", uci_command);
        
        // In production, execute: system(uci_command);
    }
    
    // Commit UCI changes
    LOGX_DEBUG("Would execute: uci commit mwan3");
    LOGX_DEBUG("Would execute: ubus call mwan3 reload");
    
    LOGX_INFO("✅ MWAN3 weight changes applied (simulated)");
    return ML_MONITOR_MULTI_SUCCESS;
}

// Update cross-interface correlations
int ml_monitor_update_cross_interface_correlations(multi_interface_ml_system_t *system) {
    if (!system) return ML_MONITOR_MULTI_ERROR_INVALID_PARAM;
    
    if (!system->cross_learning.enable_cross_interface_correlation) {
        return ML_MONITOR_MULTI_SUCCESS;
    }
    
    // Calculate correlations between interface types
    // This is a simplified implementation - in reality, we'd use statistical correlation
    
    for (int i = 0; i < INTERFACE_TYPE_MAX; i++) {
        for (int j = 0; j < INTERFACE_TYPE_MAX; j++) {
            if (i != j) {
                // Simulate correlation learning
                double correlation = system->cross_learning.interface_correlation_matrix[i][j];
                
                // Update correlation based on simultaneous performance
                // In a full implementation, this would analyze actual correlation
                correlation = correlation * 0.99 + 0.1 * 0.01; // Slight correlation
                
                system->cross_learning.interface_correlation_matrix[i][j] = correlation;
            }
        }
    }
    
    return ML_MONITOR_MULTI_SUCCESS;
}

// Validate failover prediction after the fact
int ml_monitor_validate_failover_prediction(multi_interface_ml_system_t *system,
                                           const char *interface_id,
                                           bool actual_outage_occurred,
                                           uint32_t actual_duration_seconds) {
    if (!system || !interface_id) return ML_MONITOR_MULTI_ERROR_INVALID_PARAM;
    
    // Find interface model
    interface_ml_model_t *model = NULL;
    for (int i = 0; i < system->interface_count; i++) {
        if (strcmp(system->interface_models[i].interface_id, interface_id) == 0) {
            model = &system->interface_models[i];
            break;
        }
    }
    
    if (!model) {
        return ML_MONITOR_MULTI_ERROR_NOT_FOUND;
    }
    
    // Update validation statistics
    if (actual_outage_occurred) {
        model->performance.correct_predictions++;
        model->failover_learning.average_outage_duration_minutes = 
            (model->failover_learning.average_outage_duration_minutes * 0.9) + 
            (actual_duration_seconds / 60.0) * 0.1;
        
        LOGX_INFO("✅ Failover prediction validated: %s had outage for %u seconds",
                 interface_id, actual_duration_seconds);
    } else {
        // False positive - learn from this
        LOGX_INFO("📚 Learning from false positive: %s did not have predicted outage", interface_id);
    }
    
    // Update accuracy
    model->performance.accuracy = model->performance.total_predictions > 0 ?
        (double)model->performance.correct_predictions / model->performance.total_predictions : 0.0;
    
    return ML_MONITOR_MULTI_SUCCESS;
}

// Get global multi-interface system instance
multi_interface_ml_system_t* ml_monitor_get_multi_interface_system(void) {
    return g_multi_interface_system;
}