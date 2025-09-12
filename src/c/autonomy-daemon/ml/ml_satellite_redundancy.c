#include "ml_satellite_redundancy.h"
#include "../shared/logging/logx.h"
#include "../core/types.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

// Default configuration values
#define DEFAULT_CRITICAL_THRESHOLD 2
#define DEFAULT_WARNING_THRESHOLD 4
#define DEFAULT_SAFE_THRESHOLD 6
#define DEFAULT_OPTIMAL_THRESHOLD 8
#define DEFAULT_WARNING_COOLDOWN_SECONDS 300  // 5 minutes
#define DEFAULT_PREDICTION_HORIZON_SECONDS 900 // 15 minutes

// Initialize satellite redundancy predictor
satellite_redundancy_predictor_t* satellite_redundancy_init(const satellite_redundancy_config_t *config) {
    if (!config) {
        LOGX_ERROR_MSG("Invalid configuration parameter");
        return NULL;
    }
    
    satellite_redundancy_predictor_t *predictor = calloc(1, sizeof(satellite_redundancy_predictor_t));
    if (!predictor) {
        LOGX_ERROR_MSG("Failed to allocate memory for satellite redundancy predictor");
        return NULL;
    }
    
    // Copy configuration
    predictor->config = *config;
    
    // Initialize state
    predictor->prediction_active = false;
    predictor->early_warning_active = false;
    predictor->warning_count = 0;
    predictor->last_prediction_time = 0;
    predictor->last_warning_time = 0;
    
    // Initialize history buffers
    memset(predictor->history.satellite_counts, 0, sizeof(predictor->history.satellite_counts));
    memset(predictor->history.obstruction_counts, 0, sizeof(predictor->history.obstruction_counts));
    memset(predictor->history.redundancy_scores, 0, sizeof(predictor->history.redundancy_scores));
    predictor->history.write_index = 0;
    predictor->history.count = 0;
    
    // Initialize assessment
    memset(&predictor->current_assessment, 0, sizeof(satellite_redundancy_assessment_t));
    memset(&predictor->last_assessment, 0, sizeof(satellite_redundancy_assessment_t));
    
    LOGX_INFO_MSG("Satellite redundancy predictor initialized with thresholds: critical=%d, warning=%d, safe=%d, optimal=%d",
              predictor->config.critical_threshold,
              predictor->config.warning_threshold,
              predictor->config.safe_threshold,
              predictor->config.optimal_threshold);
    
    return predictor;
}

// Cleanup satellite redundancy predictor
void satellite_redundancy_cleanup(satellite_redundancy_predictor_t *predictor) {
    if (!predictor) return;
    
    LOGX_DEBUG_MSG("Cleaning up satellite redundancy predictor");
    free(predictor);
}

// Initialize default configuration
void satellite_redundancy_config_init_defaults(satellite_redundancy_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(satellite_redundancy_config_t));
    
    config->critical_threshold = DEFAULT_CRITICAL_THRESHOLD;
    config->warning_threshold = DEFAULT_WARNING_THRESHOLD;
    config->safe_threshold = DEFAULT_SAFE_THRESHOLD;
    config->optimal_threshold = DEFAULT_OPTIMAL_THRESHOLD;
    
    config->redundancy_weight = 0.3;
    config->obstruction_penalty = 0.5;
    config->elevation_bonus = 0.2;
    config->diversity_bonus = 0.1;
    
    config->enable_early_warning = true;
    config->warning_cooldown_seconds = DEFAULT_WARNING_COOLDOWN_SECONDS;
    config->prediction_horizon_seconds = DEFAULT_PREDICTION_HORIZON_SECONDS;
    
    config->enable_ml_integration = true;
    config->ml_feature_weight = 0.25;
}

// Assess current satellite redundancy
int satellite_redundancy_assess_current(satellite_redundancy_predictor_t *predictor, 
                                       const starlink_status_response_t *starlink_data) {
    if (!predictor || !starlink_data) {
        return SATELLITE_REDUNDANCY_ERROR_INVALID_PARAM;
    }
    
    // Store previous assessment
    predictor->last_assessment = predictor->current_assessment;
    
    // Clear current assessment
    memset(&predictor->current_assessment, 0, sizeof(satellite_redundancy_assessment_t));
    
    // Extract satellite data from Starlink status
    predictor->current_assessment.total_visible = starlink_data->gps_stats.gps_sats;
    predictor->current_assessment.obstruction_risk = starlink_data->obstruction_stats.fraction_obstructed;
    
    // Calculate unobstructed count (estimate based on obstruction percentage)
    double obstruction_ratio = starlink_data->obstruction_stats.fraction_obstructed;
    predictor->current_assessment.unobstructed_count = (uint8_t)(predictor->current_assessment.total_visible * (1.0 - obstruction_ratio));
    predictor->current_assessment.obstructed_count = predictor->current_assessment.total_visible - predictor->current_assessment.unobstructed_count;
    
    // Estimate elevation distribution (simplified - would need actual satellite positions)
    // For now, assume reasonable distribution
    if (predictor->current_assessment.total_visible > 0) {
        predictor->current_assessment.high_elevation_count = (uint8_t)(predictor->current_assessment.total_visible * 0.3);
        predictor->current_assessment.medium_elevation_count = (uint8_t)(predictor->current_assessment.total_visible * 0.5);
        predictor->current_assessment.low_elevation_count = predictor->current_assessment.total_visible - 
                                                           predictor->current_assessment.high_elevation_count - 
                                                           predictor->current_assessment.medium_elevation_count;
    }
    
    // Calculate redundancy score
    predictor->current_assessment.redundancy_score = satellite_redundancy_calculate_score(
        predictor->current_assessment.total_visible,
        predictor->current_assessment.unobstructed_count,
        predictor->current_assessment.high_elevation_count,
        obstruction_ratio
    );
    
    // Calculate diversity score (simplified)
    if (predictor->current_assessment.total_visible > 0) {
        double elevation_diversity = 1.0 - fabs(0.33 - (double)predictor->current_assessment.high_elevation_count / predictor->current_assessment.total_visible);
        predictor->current_assessment.diversity_score = fmax(0.0, fmin(1.0, elevation_diversity));
    } else {
        predictor->current_assessment.diversity_score = 0.0;
    }
    
    // Calculate elevation score
    if (predictor->current_assessment.total_visible > 0) {
        double high_elevation_ratio = (double)predictor->current_assessment.high_elevation_count / predictor->current_assessment.total_visible;
        predictor->current_assessment.elevation_score = fmax(0.0, fmin(1.0, high_elevation_ratio * 2.0));
    } else {
        predictor->current_assessment.elevation_score = 0.0;
    }
    
    // Calculate obstruction risk
    predictor->current_assessment.obstruction_risk = obstruction_ratio;
    
    // Determine risk level
    predictor->current_assessment.risk_level = satellite_redundancy_get_risk_level(
        predictor->current_assessment.total_visible,
        predictor->current_assessment.unobstructed_count,
        obstruction_ratio
    );
    
    // Generate ML features
    predictor->current_assessment.ml_redundancy_feature = (uint8_t)(predictor->current_assessment.redundancy_score * 255);
    predictor->current_assessment.ml_risk_feature = (uint8_t)(predictor->current_assessment.obstruction_risk * 255);
    predictor->current_assessment.ml_diversity_feature = (uint8_t)(predictor->current_assessment.diversity_score * 255);
    
    // Set assessment time
    predictor->current_assessment.assessment_time = time(NULL);
    
    // Update history
    uint8_t idx = predictor->history.write_index;
    predictor->history.satellite_counts[idx] = predictor->current_assessment.total_visible;
    predictor->history.obstruction_counts[idx] = (uint8_t)(predictor->current_assessment.obstruction_risk * 100);
    predictor->history.redundancy_scores[idx] = predictor->current_assessment.redundancy_score;
    
    predictor->history.write_index = (predictor->history.write_index + 1) % 60;
    if (predictor->history.count < 60) {
        predictor->history.count++;
    }
    
    // Check for early warning conditions
    if (predictor->config.enable_early_warning) {
        bool should_warn = satellite_redundancy_should_trigger_warning(
            &predictor->current_assessment,
            &predictor->last_assessment,
            &predictor->config
        );
        
        if (should_warn && !predictor->early_warning_active) {
            time_t now = time(NULL);
            if (now - predictor->last_warning_time >= predictor->config.warning_cooldown_seconds) {
                satellite_redundancy_trigger_early_warning(predictor);
            }
        }
    }
    
    LOGX_DEBUG_MSG("Satellite redundancy assessment: visible=%d, unobstructed=%d, redundancy=%.2f, risk_level=%d",
               predictor->current_assessment.total_visible,
               predictor->current_assessment.unobstructed_count,
               predictor->current_assessment.redundancy_score,
               predictor->current_assessment.risk_level);
    
    return SATELLITE_REDUNDANCY_SUCCESS;
}

// Calculate redundancy score
double satellite_redundancy_calculate_score(uint8_t total_visible, uint8_t unobstructed, 
                                           uint8_t high_elevation, double obstruction_ratio) {
    if (total_visible == 0) return 0.0;
    
    // Base score from satellite count (normalized to 0-1)
    double count_score = fmin(1.0, (double)total_visible / 12.0);
    
    // Unobstructed ratio bonus
    double unobstructed_ratio = (double)unobstructed / total_visible;
    double unobstructed_bonus = unobstructed_ratio * 0.3;
    
    // High elevation bonus
    double high_elevation_ratio = (double)high_elevation / total_visible;
    double elevation_bonus = high_elevation_ratio * 0.2;
    
    // Obstruction penalty
    double obstruction_penalty = obstruction_ratio * 0.4;
    
    // Calculate final score
    double score = count_score + unobstructed_bonus + elevation_bonus - obstruction_penalty;
    
    return fmax(0.0, fmin(1.0, score));
}

// Get risk level based on satellite count and obstruction
uint8_t satellite_redundancy_get_risk_level(uint8_t total_visible, uint8_t unobstructed, 
                                           double obstruction_ratio) {
    if (total_visible <= 2 || unobstructed <= 1) {
        return 4; // Critical
    } else if (total_visible <= 4 || unobstructed <= 2 || obstruction_ratio > 0.3) {
        return 3; // High
    } else if (total_visible <= 6 || unobstructed <= 3 || obstruction_ratio > 0.15) {
        return 2; // Medium
    } else {
        return 1; // Low
    }
}

// Check if early warning should be triggered
bool satellite_redundancy_should_trigger_warning(const satellite_redundancy_assessment_t *current,
                                                const satellite_redundancy_assessment_t *previous,
                                                const satellite_redundancy_config_t *config) {
    if (!current || !config) return false;
    
    // Trigger warning if approaching critical threshold
    if (current->total_visible <= config->warning_threshold) {
        return true;
    }
    
    // Trigger warning if redundancy score is declining rapidly
    if (previous && previous->redundancy_score > 0) {
        double score_decline = previous->redundancy_score - current->redundancy_score;
        if (score_decline > 0.2) { // 20% decline
            return true;
        }
    }
    
    // Trigger warning if obstruction is increasing rapidly
    if (previous && previous->obstruction_risk < current->obstruction_risk) {
        double obstruction_increase = current->obstruction_risk - previous->obstruction_risk;
        if (obstruction_increase > 0.15) { // 15% increase
            return true;
        }
    }
    
    return false;
}

// Trigger early warning
int satellite_redundancy_trigger_early_warning(satellite_redundancy_predictor_t *predictor) {
    if (!predictor) {
        return SATELLITE_REDUNDANCY_ERROR_INVALID_PARAM;
    }
    
    predictor->early_warning_active = true;
    predictor->last_warning_time = time(NULL);
    predictor->warning_count++;
    
    LOGX_WARN_MSG("Satellite redundancy early warning triggered: visible=%d, unobstructed=%d, risk_level=%d",
              predictor->current_assessment.total_visible,
              predictor->current_assessment.unobstructed_count,
              predictor->current_assessment.risk_level);
    
    // Call early warning callback
    if (predictor->early_warning_callback) {
        predictor->early_warning_callback(&predictor->current_assessment, predictor->callback_user_data);
    }
    
    return SATELLITE_REDUNDANCY_SUCCESS;
}

// Clear early warning
int satellite_redundancy_clear_early_warning(satellite_redundancy_predictor_t *predictor) {
    if (!predictor) {
        return SATELLITE_REDUNDANCY_ERROR_INVALID_PARAM;
    }
    
    if (predictor->early_warning_active) {
        predictor->early_warning_active = false;
        
        LOGX_INFO_MSG("Satellite redundancy early warning cleared");
    }
    
    return SATELLITE_REDUNDANCY_SUCCESS;
}

// Get ML features for integration
int satellite_redundancy_get_ml_features(const satellite_redundancy_predictor_t *predictor, 
                                        uint8_t *redundancy_feature,
                                        uint8_t *risk_feature, 
                                        uint8_t *diversity_feature) {
    if (!predictor || !redundancy_feature || !risk_feature || !diversity_feature) {
        return SATELLITE_REDUNDANCY_ERROR_INVALID_PARAM;
    }
    
    *redundancy_feature = predictor->current_assessment.ml_redundancy_feature;
    *risk_feature = predictor->current_assessment.ml_risk_feature;
    *diversity_feature = predictor->current_assessment.ml_diversity_feature;
    
    return SATELLITE_REDUNDANCY_SUCCESS;
}

// Update ML observation with satellite redundancy data
int satellite_redundancy_update_ml_observation(ml_observation_t *observation, 
                                              const satellite_redundancy_assessment_t *assessment) {
    if (!observation || !assessment) {
        return SATELLITE_REDUNDANCY_ERROR_INVALID_PARAM;
    }
    
    // Update satellite count (already in observation)
    observation->satellites_visible = assessment->total_visible;
    
    // Use reserved fields for additional redundancy data
    // Store redundancy score in reserved[0] (0-255)
    observation->reserved = (uint16_t)(assessment->redundancy_score * 255);
    
    // Store risk level in flags (bits 4-7)
    observation->flags = (observation->flags & 0x0F) | ((assessment->risk_level & 0x0F) << 4);
    
    // Store diversity score in pattern_hash (lower 8 bits)
    observation->pattern_hash = (observation->pattern_hash & 0xFF00) | (uint8_t)(assessment->diversity_score * 255);
    
    return SATELLITE_REDUNDANCY_SUCCESS;
}

// Get current assessment
int satellite_redundancy_get_assessment(const satellite_redundancy_predictor_t *predictor, 
                                       satellite_redundancy_assessment_t *assessment) {
    if (!predictor || !assessment) {
        return SATELLITE_REDUNDANCY_ERROR_INVALID_PARAM;
    }
    
    *assessment = predictor->current_assessment;
    return SATELLITE_REDUNDANCY_SUCCESS;
}

// Set early warning callback
int satellite_redundancy_set_early_warning_callback(satellite_redundancy_predictor_t *predictor,
                                                   void (*callback)(const satellite_redundancy_assessment_t *assessment, void *user_data),
                                                   void *user_data) {
    if (!predictor) {
        return SATELLITE_REDUNDANCY_ERROR_INVALID_PARAM;
    }
    
    predictor->early_warning_callback = callback;
    predictor->callback_user_data = user_data;
    
    return SATELLITE_REDUNDANCY_SUCCESS;
}

// Set critical threshold callback
int satellite_redundancy_set_critical_callback(satellite_redundancy_predictor_t *predictor,
                                              void (*callback)(const satellite_redundancy_assessment_t *assessment, void *user_data),
                                              void *user_data) {
    if (!predictor) {
        return SATELLITE_REDUNDANCY_ERROR_INVALID_PARAM;
    }
    
    predictor->critical_threshold_callback = callback;
    predictor->callback_user_data = user_data;
    
    return SATELLITE_REDUNDANCY_SUCCESS;
}

// Check if early warning is active
bool satellite_redundancy_is_early_warning_active(const satellite_redundancy_predictor_t *predictor) {
    return predictor ? predictor->early_warning_active : false;
}
