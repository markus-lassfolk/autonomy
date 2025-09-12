#include "ml_monitor.h"
#include "ml_satellite_redundancy.h"
#include "ml_cellular_tower_intelligence.h"
#include "../starlink/starlink_types.h"
#include "../starlink/starlink_modules.h"
#include "../gps/opencellid_complete.h"
#include "../network/cellular_collector.h"
#include "../shared/logging/logx.h"
#include "../core/types.h"
#include <string.h>
#include <time.h>
#include <stdlib.h>

// Enhanced ML integration system
typedef struct {
    // Core ML monitor
    ml_monitor_t *ml_monitor;
    
    // Satellite redundancy analysis
    satellite_redundancy_predictor_t *satellite_redundancy;
    satellite_redundancy_config_t satellite_config;
    
    // Cellular tower intelligence
    cellular_tower_intelligence_t *cellular_intelligence;
    cellular_tower_config_t cellular_config;
    
    // Integration state
    bool enhanced_features_enabled;
    time_t last_integration_update;
    uint32_t integration_cycle_count;
    
    // Performance tracking
    struct {
        uint32_t satellite_assessments;
        uint32_t cellular_analyses;
        uint32_t ml_predictions_enhanced;
        uint32_t early_warnings_triggered;
        uint32_t carrier_filtered_towers;
    } stats;
    
} ml_enhanced_integration_t;

// Global enhanced integration instance
static ml_enhanced_integration_t *g_enhanced_integration = NULL;

// Forward declarations for callback functions
static void ml_enhanced_integration_satellite_warning_callback(const satellite_redundancy_assessment_t *assessment, void *user_data);
static void ml_enhanced_integration_cellular_risk_callback(double risk_score, void *user_data);

// Initialize enhanced ML integration
int ml_enhanced_integration_init(ml_monitor_t *ml_monitor) {
    if (!ml_monitor) {
        LOGX_ERROR_MSG("Invalid ML monitor parameter");
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    if (g_enhanced_integration) {
        LOGX_WARN_MSG("Enhanced ML integration already initialized");
        return ML_MONITOR_SUCCESS;
    }
    
    g_enhanced_integration = calloc(1, sizeof(ml_enhanced_integration_t));
    if (!g_enhanced_integration) {
        LOGX_ERROR_MSG("Failed to allocate memory for enhanced ML integration");
        return ML_MONITOR_ERROR_MEMORY_FAILED;
    }
    
    g_enhanced_integration->ml_monitor = ml_monitor;
    g_enhanced_integration->enhanced_features_enabled = true;
    g_enhanced_integration->last_integration_update = time(NULL);
    g_enhanced_integration->integration_cycle_count = 0;
    
    // Initialize satellite redundancy analysis
    satellite_redundancy_config_init_defaults(&g_enhanced_integration->satellite_config);
    g_enhanced_integration->satellite_redundancy = satellite_redundancy_init(&g_enhanced_integration->satellite_config);
    if (!g_enhanced_integration->satellite_redundancy) {
        LOGX_ERROR_MSG("Failed to initialize satellite redundancy analysis");
        free(g_enhanced_integration);
        g_enhanced_integration = NULL;
        return ML_MONITOR_ERROR_MEMORY_FAILED;
    }
    
    // Initialize cellular tower intelligence
    cellular_tower_config_init_defaults(&g_enhanced_integration->cellular_config);
    g_enhanced_integration->cellular_intelligence = cellular_tower_intelligence_init(&g_enhanced_integration->cellular_config);
    if (!g_enhanced_integration->cellular_intelligence) {
        LOGX_ERROR_MSG("Failed to initialize cellular tower intelligence");
        satellite_redundancy_cleanup(g_enhanced_integration->satellite_redundancy);
        free(g_enhanced_integration);
        g_enhanced_integration = NULL;
        return ML_MONITOR_ERROR_MEMORY_FAILED;
    }
    
    // Set up callbacks
    satellite_redundancy_set_early_warning_callback(g_enhanced_integration->satellite_redundancy,
                                                   ml_enhanced_integration_satellite_warning_callback,
                                                   g_enhanced_integration);
    
    cellular_tower_intelligence_set_connectivity_risk_callback(g_enhanced_integration->cellular_intelligence,
                                                              ml_enhanced_integration_cellular_risk_callback,
                                                              g_enhanced_integration);
    
    LOGX_INFO_MSG("Enhanced ML integration initialized with satellite redundancy and cellular tower intelligence");
    
    return ML_MONITOR_SUCCESS;
}

// Cleanup enhanced ML integration
void ml_enhanced_integration_cleanup(void) {
    if (!g_enhanced_integration) return;
    
    LOGX_DEBUG_MSG("Cleaning up enhanced ML integration");
    
    if (g_enhanced_integration->satellite_redundancy) {
        satellite_redundancy_cleanup(g_enhanced_integration->satellite_redundancy);
    }
    
    if (g_enhanced_integration->cellular_intelligence) {
        cellular_tower_intelligence_cleanup(g_enhanced_integration->cellular_intelligence);
    }
    
    free(g_enhanced_integration);
    g_enhanced_integration = NULL;
}

// Enhanced observation collection with satellite redundancy and cellular intelligence
int ml_enhanced_integration_collect_observation(ml_observation_t *observation) {
    if (!g_enhanced_integration || !observation) {
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    // Collect base observation from ML monitor
    int result = ml_monitor_collect_observation(g_enhanced_integration->ml_monitor);
    if (result != ML_MONITOR_SUCCESS) {
        LOGX_WARN_MSG("Failed to collect base ML observation");
        return result;
    }
    
    // Get the latest observation
    ml_observation_t *latest_obs = &g_enhanced_integration->ml_monitor->state->recent.observations[
        g_enhanced_integration->ml_monitor->state->recent.write_index];
    
    // Copy base observation
    *observation = *latest_obs;
    
    // Enhance with satellite redundancy analysis
    if (g_enhanced_integration->satellite_redundancy) {
        // Get Starlink data for satellite analysis
        starlink_collection_result_t starlink_data;
        if (starlink_collect_data(&starlink_data) == AUTONOMY_SUCCESS) {
        // Assess satellite redundancy
        satellite_redundancy_assess_current(g_enhanced_integration->satellite_redundancy, &starlink_data.status);
            
            // Get satellite redundancy assessment
            satellite_redundancy_assessment_t assessment;
            if (satellite_redundancy_get_assessment(g_enhanced_integration->satellite_redundancy, &assessment) == SATELLITE_REDUNDANCY_SUCCESS) {
                // Update observation with satellite redundancy data
                satellite_redundancy_update_ml_observation(observation, &assessment);
                
                g_enhanced_integration->stats.satellite_assessments++;
                
                LOGX_DEBUG_MSG("Enhanced observation with satellite redundancy: visible=%d, redundancy=%.2f, risk_level=%d",
                           assessment.total_visible, assessment.redundancy_score, assessment.risk_level);
            }
        }
    }
    
    // Enhance with cellular tower intelligence
    if (g_enhanced_integration->cellular_intelligence) {
        // Get cellular environment data
        opencellid_cellular_environment_t environment;
        if (opencellid_get_cellular_environment(&environment) == AUTONOMY_SUCCESS) {
            // Get cellular info
            cellular_info_t cellular_info;
            if (cellular_collector_collect(&cellular_info) == AUTONOMY_SUCCESS) {
                // Analyze cellular towers
                cellular_tower_intelligence_analyze_towers(g_enhanced_integration->cellular_intelligence,
                                                          &environment, &cellular_info);
                
                // Filter usable towers
                usable_tower_info_t usable_towers[20];
                uint8_t usable_count;
                cellular_tower_intelligence_filter_usable_towers(g_enhanced_integration->cellular_intelligence,
                                                               environment.neighbors,
                                                               environment.neighbor_count,
                                                               usable_towers,
                                                               &usable_count);
                
                // Analyze tower density
                if (environment.gps_valid) {
                    cellular_tower_intelligence_analyze_density(g_enhanced_integration->cellular_intelligence,
                                                              usable_towers, usable_count,
                                                              environment.gps_latitude,
                                                              environment.gps_longitude);
                }
                
                // Analyze cell change patterns
                cellular_tower_intelligence_analyze_cell_changes(g_enhanced_integration->cellular_intelligence,
                                                               cellular_info.cell_id, &cellular_info);
                
                // Update observation with cellular intelligence data
                cellular_tower_intelligence_update_ml_observation(observation, g_enhanced_integration->cellular_intelligence);
                
                g_enhanced_integration->stats.cellular_analyses++;
                g_enhanced_integration->stats.carrier_filtered_towers += usable_count;
                
                LOGX_DEBUG_MSG("Enhanced observation with cellular intelligence: usable_towers=%d, density_score=%.2f, risk_score=%.2f",
                           usable_count,
                           g_enhanced_integration->cellular_intelligence->density_analysis.density_score,
                           g_enhanced_integration->cellular_intelligence->cell_change_analysis.connectivity_risk_score);
            }
        }
    }
    
    // Update integration statistics
    g_enhanced_integration->integration_cycle_count++;
    g_enhanced_integration->last_integration_update = time(NULL);
    
    return ML_MONITOR_SUCCESS;
}

// Enhanced prediction with satellite redundancy and cellular intelligence
int ml_enhanced_integration_predict_outage(uint8_t *probability, uint8_t *confidence, uint8_t *cause) {
    if (!g_enhanced_integration || !probability || !confidence || !cause) {
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    // Get base prediction from ML monitor
    ml_observation_t observation;
    if (ml_enhanced_integration_collect_observation(&observation) != ML_MONITOR_SUCCESS) {
        return ML_MONITOR_ERROR_PREDICTION_FAILED;
    }
    
    // Get base ML prediction
    uint8_t base_probability, base_confidence, base_cause;
    if (ml_monitor_predict_ensemble(g_enhanced_integration->ml_monitor, &observation,
                                   &base_probability, &base_confidence, &base_cause) != ML_MONITOR_SUCCESS) {
        return ML_MONITOR_ERROR_PREDICTION_FAILED;
    }
    
    // Enhance prediction with satellite redundancy analysis
    if (g_enhanced_integration->satellite_redundancy) {
        satellite_redundancy_assessment_t assessment;
        if (satellite_redundancy_get_assessment(g_enhanced_integration->satellite_redundancy, &assessment) == SATELLITE_REDUNDANCY_SUCCESS) {
            // Adjust probability based on satellite redundancy
            if (assessment.risk_level >= 3) { // High or critical risk
                base_probability = (uint8_t)fmin(255, base_probability * 1.3); // 30% increase
                base_confidence = (uint8_t)fmin(255, base_confidence * 1.2); // 20% increase
            } else if (assessment.risk_level == 1) { // Low risk
                base_probability = (uint8_t)fmax(0, base_probability * 0.8); // 20% decrease
            }
            
            // Set cause based on satellite risk
            if (assessment.risk_level >= 3) {
                *cause = OUTAGE_OBSTRUCTION_DYNAMIC; // Satellite-related outage
            }
        }
    }
    
    // Enhance prediction with cellular intelligence
    if (g_enhanced_integration->cellular_intelligence) {
        cell_change_pattern_analysis_t cell_analysis;
        if (cellular_tower_intelligence_get_cell_change_analysis(g_enhanced_integration->cellular_intelligence, &cell_analysis) == CELLULAR_TOWER_INTELLIGENCE_SUCCESS) {
            // Adjust probability based on cell change patterns
            if (cell_analysis.connectivity_risk_score > 0.7) { // High connectivity risk
                base_probability = (uint8_t)fmin(255, base_probability * 1.2); // 20% increase
            }
            
            // Set cause based on cellular patterns
            if (cell_analysis.poor_coverage_detected) {
                *cause = OUTAGE_COVERAGE_GAP;
            } else if (cell_analysis.network_congestion_detected) {
                *cause = OUTAGE_NETWORK_CONGESTION;
            }
        }
    }
    
    *probability = base_probability;
    *confidence = base_confidence;
    
    g_enhanced_integration->stats.ml_predictions_enhanced++;
    
    LOGX_DEBUG_MSG("Enhanced outage prediction: probability=%d, confidence=%d, cause=%d",
               *probability, *confidence, *cause);
    
    return ML_MONITOR_SUCCESS;
}

// Satellite early warning callback
static void ml_enhanced_integration_satellite_warning_callback(const satellite_redundancy_assessment_t *assessment, void *user_data) {
    if (!assessment || !user_data) return;
    
    ml_enhanced_integration_t *integration = (ml_enhanced_integration_t*)user_data;
    integration->stats.early_warnings_triggered++;
    
    LOGX_WARN_MSG("Satellite redundancy early warning: visible=%d, unobstructed=%d, risk_level=%d, redundancy=%.2f",
              assessment->total_visible, assessment->unobstructed_count, 
              assessment->risk_level, assessment->redundancy_score);
    
    // Could trigger additional actions here, such as:
    // - Pre-emptively switching to cellular
    // - Adjusting MWAN3 weights
    // - Sending notifications
}

// Cellular connectivity risk callback
static void ml_enhanced_integration_cellular_risk_callback(double risk_score, void *user_data) {
    if (!user_data) return;
    
    ml_enhanced_integration_t *integration = (ml_enhanced_integration_t*)user_data;
    
    if (risk_score > 0.8) { // High risk threshold
        LOGX_WARN_MSG("High cellular connectivity risk detected: %.2f", risk_score);
        
        // Could trigger additional actions here, such as:
        // - Adjusting cellular interface weights
        // - Preparing for failover
        // - Sending alerts
    }
}

// Enhanced integration statistics structure
typedef struct {
    uint32_t satellite_assessments;
    uint32_t cellular_analyses;
    uint32_t ml_predictions_enhanced;
    uint32_t early_warnings_triggered;
    uint32_t carrier_filtered_towers;
    uint32_t integration_cycles;
    time_t last_update;
} ml_enhanced_integration_stats_t;

// Get enhanced integration statistics
int ml_enhanced_integration_get_stats(ml_enhanced_integration_stats_t *stats) {
    if (!g_enhanced_integration || !stats) {
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    stats->satellite_assessments = g_enhanced_integration->stats.satellite_assessments;
    stats->cellular_analyses = g_enhanced_integration->stats.cellular_analyses;
    stats->ml_predictions_enhanced = g_enhanced_integration->stats.ml_predictions_enhanced;
    stats->early_warnings_triggered = g_enhanced_integration->stats.early_warnings_triggered;
    stats->carrier_filtered_towers = g_enhanced_integration->stats.carrier_filtered_towers;
    stats->integration_cycles = g_enhanced_integration->integration_cycle_count;
    stats->last_update = g_enhanced_integration->last_integration_update;
    
    return ML_MONITOR_SUCCESS;
}

// Check if enhanced features are enabled
bool ml_enhanced_integration_is_enabled(void) {
    return g_enhanced_integration != NULL && g_enhanced_integration->enhanced_features_enabled;
}

// Enable/disable enhanced features
int ml_enhanced_integration_set_enabled(bool enabled) {
    if (!g_enhanced_integration) {
        return ML_MONITOR_ERROR_NOT_INITIALIZED;
    }
    
    g_enhanced_integration->enhanced_features_enabled = enabled;
    
    LOGX_INFO_MSG("Enhanced ML integration features %s", enabled ? "enabled" : "disabled");
    
    return ML_MONITOR_SUCCESS;
}
