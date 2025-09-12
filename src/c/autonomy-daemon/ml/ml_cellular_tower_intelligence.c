#include "ml_cellular_tower_intelligence.h"
#include "../shared/logging/logx.h"
#include "../core/types.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

// Default configuration values
#define DEFAULT_MIN_RSRP_DBM -120
#define DEFAULT_MIN_RSRQ_DB -20
#define DEFAULT_MIN_SINR_DB -3
#define DEFAULT_DENSITY_RADIUS_KM 5.0
#define DEFAULT_MIN_TOWERS_FOR_DENSITY 3
#define DEFAULT_CELL_CHANGE_WINDOW_SECONDS 3600
#define DEFAULT_MAX_CELL_CHANGES_PER_HOUR 10

// Initialize cellular tower intelligence
cellular_tower_intelligence_t* cellular_tower_intelligence_init(const cellular_tower_config_t *config) {
    if (!config) {
        LOGX_ERROR_MSG("Invalid configuration parameter");
        return NULL;
    }
    
    cellular_tower_intelligence_t *intelligence = calloc(1, sizeof(cellular_tower_intelligence_t));
    if (!intelligence) {
        LOGX_ERROR_MSG("Failed to allocate memory for cellular tower intelligence");
        return NULL;
    }
    
    // Copy configuration
    intelligence->config = *config;
    
    // Initialize state
    intelligence->usable_tower_count = 0;
    intelligence->analysis_active = false;
    intelligence->last_update_time = 0;
    memset(intelligence->last_serving_cell, 0, sizeof(intelligence->last_serving_cell));
    
    // Initialize density analysis
    memset(&intelligence->density_analysis, 0, sizeof(tower_density_analysis_t));
    
    // Initialize cell change analysis
    memset(&intelligence->cell_change_analysis, 0, sizeof(cell_change_pattern_analysis_t));
    intelligence->cell_change_analysis.change_index = 0;
    intelligence->cell_change_analysis.change_count = 0;
    
    // Initialize history
    memset(intelligence->history.density_history, 0, sizeof(intelligence->history.density_history));
    intelligence->history.density_history_index = 0;
    intelligence->history.density_history_count = 0;
    
    LOGX_INFO_MSG("Cellular tower intelligence initialized with carrier filtering: %s, min_rsrp=%d dBm",
              intelligence->config.enable_carrier_filtering ? "enabled" : "disabled",
              intelligence->config.min_rsrp_dbm);
    
    return intelligence;
}

// Cleanup cellular tower intelligence
void cellular_tower_intelligence_cleanup(cellular_tower_intelligence_t *intelligence) {
    if (!intelligence) return;
    
    LOGX_DEBUG_MSG("Cleaning up cellular tower intelligence");
    free(intelligence);
}

// Initialize default configuration
void cellular_tower_config_init_defaults(cellular_tower_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(cellular_tower_config_t));
    
    config->min_rsrp_dbm = DEFAULT_MIN_RSRP_DBM;
    config->min_rsrq_db = DEFAULT_MIN_RSRQ_DB;
    config->min_sinr_db = DEFAULT_MIN_SINR_DB;
    
    config->enable_carrier_filtering = true;
    strcpy(config->allowed_mccs, "310,311,312,313,314,315,316"); // US MCCs
    strcpy(config->allowed_mncs, "260,410,480,311,312,313"); // Major US carriers
    strcpy(config->home_mcc, "310");
    strcpy(config->home_mnc, "260");
    
    config->allow_roaming = true;
    config->prefer_home_carrier = true;
    config->roaming_penalty = 0.2;
    
    config->enable_density_analysis = true;
    config->density_radius_km = DEFAULT_DENSITY_RADIUS_KM;
    config->min_towers_for_density = DEFAULT_MIN_TOWERS_FOR_DENSITY;
    
    config->enable_cell_change_analysis = true;
    config->cell_change_window_seconds = DEFAULT_CELL_CHANGE_WINDOW_SECONDS;
    config->max_cell_changes_per_hour = DEFAULT_MAX_CELL_CHANGES_PER_HOUR;
}

// Check if carrier is compatible
bool cellular_tower_is_carrier_compatible(const opencellid_cell_identifier_t *cell_id,
                                         const cellular_tower_config_t *config) {
    if (!cell_id || !config || !config->enable_carrier_filtering) {
        return true; // Allow all if filtering disabled
    }
    
    // Check MCC
    char mcc_str[8];
    snprintf(mcc_str, sizeof(mcc_str), "%d", cell_id->mcc);
    if (strstr(config->allowed_mccs, mcc_str) == NULL) {
        return false;
    }
    
    // Check MNC
    char mnc_str[8];
    snprintf(mnc_str, sizeof(mnc_str), "%d", cell_id->mnc);
    if (strstr(config->allowed_mncs, mnc_str) == NULL) {
        return false;
    }
    
    return true;
}

// Check if tower meets signal strength thresholds
bool cellular_tower_meets_signal_thresholds(int rsrp, int rsrq, int sinr,
                                           const cellular_tower_config_t *config) {
    if (!config) return true;
    
    return (rsrp >= config->min_rsrp_dbm) &&
           (rsrq >= config->min_rsrq_db) &&
           (sinr >= config->min_sinr_db);
}

// Calculate usability score for a tower
double cellular_tower_calculate_usability_score(const usable_tower_info_t *tower,
                                               const cellular_tower_config_t *config) {
    if (!tower || !config) return 0.0;
    
    // Base signal quality score (0.0-1.0)
    double signal_score = 0.0;
    
    // RSRP score (normalize -140 to -50 dBm to 0.0-1.0)
    if (tower->rsrp_dbm >= -50) {
        signal_score += 1.0;
    } else if (tower->rsrp_dbm >= -140) {
        signal_score += (tower->rsrp_dbm + 140) / 90.0;
    }
    
    // RSRQ score (normalize -20 to 0 dB to 0.0-1.0)
    if (tower->rsrq_db >= 0) {
        signal_score += 1.0;
    } else if (tower->rsrq_db >= -20) {
        signal_score += (tower->rsrq_db + 20) / 20.0;
    }
    
    // SINR score (normalize -3 to 30 dB to 0.0-1.0)
    if (tower->sinr_db >= 30) {
        signal_score += 1.0;
    } else if (tower->sinr_db >= -3) {
        signal_score += (tower->sinr_db + 3) / 33.0;
    }
    
    // Average the signal scores
    signal_score /= 3.0;
    
    // Apply carrier preferences
    double carrier_bonus = 0.0;
    if (tower->is_home_carrier) {
        carrier_bonus = 0.2; // 20% bonus for home carrier
    } else if (tower->is_roaming && config->prefer_home_carrier) {
        carrier_bonus = -config->roaming_penalty; // Penalty for roaming
    }
    
    // Distance penalty (closer is better)
    double distance_penalty = 0.0;
    if (tower->distance_km > 0) {
        distance_penalty = fmin(0.3, tower->distance_km / 20.0); // Max 30% penalty
    }
    
    // Calculate final score
    double final_score = signal_score + carrier_bonus - distance_penalty;
    
    return fmax(0.0, fmin(1.0, final_score));
}

// Filter usable towers from neighbor list
int cellular_tower_intelligence_filter_usable_towers(cellular_tower_intelligence_t *intelligence,
                                                    const opencellid_neighbor_cell_t *neighbors,
                                                    int neighbor_count,
                                                    usable_tower_info_t *usable_towers,
                                                    uint8_t *usable_count) {
    if (!intelligence || !neighbors || !usable_towers || !usable_count) {
        return CELLULAR_TOWER_INTELLIGENCE_ERROR_INVALID_PARAM;
    }
    
    *usable_count = 0;
    
    for (int i = 0; i < neighbor_count && *usable_count < 20; i++) {
        const opencellid_neighbor_cell_t *neighbor = &neighbors[i];
        
        // Check carrier compatibility
        if (!cellular_tower_is_carrier_compatible(&neighbor->cell_id, &intelligence->config)) {
            continue;
        }
        
        // Check signal strength thresholds
        if (!cellular_tower_meets_signal_thresholds(neighbor->rsrp, neighbor->rsrq, neighbor->sinr, &intelligence->config)) {
            continue;
        }
        
        // Create usable tower entry
        usable_tower_info_t *tower = &usable_towers[*usable_count];
        memset(tower, 0, sizeof(usable_tower_info_t));
        
        tower->cell_id = neighbor->cell_id;
        tower->rsrp_dbm = neighbor->rsrp;
        tower->rsrq_db = neighbor->rsrq;
        tower->sinr_db = neighbor->sinr;
        tower->pci = neighbor->pci;
        tower->earfcn = neighbor->earfcn;
        tower->measurement_time = neighbor->measurement_time;
        
        // Determine carrier status
        char mcc_str[8], mnc_str[8];
        snprintf(mcc_str, sizeof(mcc_str), "%d", neighbor->cell_id.mcc);
        snprintf(mnc_str, sizeof(mnc_str), "%d", neighbor->cell_id.mnc);
        
        tower->is_home_carrier = (strcmp(mcc_str, intelligence->config.home_mcc) == 0) &&
                                (strcmp(mnc_str, intelligence->config.home_mnc) == 0);
        tower->is_roaming = !tower->is_home_carrier;
        
        // Calculate usability score
        tower->usability_score = cellular_tower_calculate_usability_score(tower, &intelligence->config);
        tower->signal_quality_score = (tower->rsrp_dbm + 140) / 90.0; // Normalize RSRP
        tower->is_usable = tower->usability_score > 0.3; // Minimum usability threshold
        
        if (tower->is_usable) {
            (*usable_count)++;
        }
    }
    
    LOGX_DEBUG_MSG("Filtered %d usable towers from %d neighbors", *usable_count, neighbor_count);
    
    return CELLULAR_TOWER_INTELLIGENCE_SUCCESS;
}

// Analyze tower density
int cellular_tower_intelligence_analyze_density(cellular_tower_intelligence_t *intelligence,
                                               const usable_tower_info_t *usable_towers,
                                               uint8_t tower_count,
                                               double current_lat,
                                               double current_lon) {
    if (!intelligence || !usable_towers) {
        return CELLULAR_TOWER_INTELLIGENCE_ERROR_INVALID_PARAM;
    }
    
    // Clear previous analysis
    memset(&intelligence->density_analysis, 0, sizeof(tower_density_analysis_t));
    
    intelligence->density_analysis.total_towers_in_radius = tower_count;
    intelligence->density_analysis.usable_towers_in_radius = 0;
    intelligence->density_analysis.home_carrier_towers = 0;
    intelligence->density_analysis.roaming_towers = 0;
    
    double total_signal_strength = 0.0;
    double best_signal_strength = -200.0; // Very low initial value
    double signal_variance_sum = 0.0;
    int valid_towers = 0;
    
    // Analyze each tower
    for (uint8_t i = 0; i < tower_count; i++) {
        const usable_tower_info_t *tower = &usable_towers[i];
        
        if (tower->is_usable) {
            intelligence->density_analysis.usable_towers_in_radius++;
            
            if (tower->is_home_carrier) {
                intelligence->density_analysis.home_carrier_towers++;
            } else if (tower->is_roaming) {
                intelligence->density_analysis.roaming_towers++;
            }
            
            total_signal_strength += tower->rsrp_dbm;
            if (tower->rsrp_dbm > best_signal_strength) {
                best_signal_strength = tower->rsrp_dbm;
            }
            valid_towers++;
        }
    }
    
    // Calculate averages and variance
    if (valid_towers > 0) {
        intelligence->density_analysis.average_signal_strength = total_signal_strength / valid_towers;
        intelligence->density_analysis.best_signal_strength = best_signal_strength;
        
        // Calculate signal strength variance
        for (uint8_t i = 0; i < tower_count; i++) {
            const usable_tower_info_t *tower = &usable_towers[i];
            if (tower->is_usable) {
                double diff = tower->rsrp_dbm - intelligence->density_analysis.average_signal_strength;
                signal_variance_sum += diff * diff;
            }
        }
        intelligence->density_analysis.signal_strength_variance = signal_variance_sum / valid_towers;
    }
    
    // Calculate density score
    intelligence->density_analysis.density_score = cellular_tower_calculate_density_score(
        intelligence->density_analysis.usable_towers_in_radius,
        intelligence->density_analysis.total_towers_in_radius,
        intelligence->density_analysis.average_signal_strength
    );
    
    // Calculate coverage score (based on signal strength distribution)
    if (intelligence->density_analysis.usable_towers_in_radius > 0) {
        double coverage_ratio = (double)intelligence->density_analysis.usable_towers_in_radius / 
                               intelligence->density_analysis.total_towers_in_radius;
        intelligence->density_analysis.coverage_score = fmin(1.0, coverage_ratio * 2.0);
    } else {
        intelligence->density_analysis.coverage_score = 0.0;
    }
    
    // Calculate reliability score (combination of density and signal quality)
    intelligence->density_analysis.reliability_score = 
        (intelligence->density_analysis.density_score * 0.4) +
        (intelligence->density_analysis.coverage_score * 0.3) +
        ((intelligence->density_analysis.best_signal_strength + 140) / 90.0 * 0.3);
    
    intelligence->density_analysis.reliability_score = fmax(0.0, fmin(1.0, intelligence->density_analysis.reliability_score));
    intelligence->density_analysis.analysis_time = time(NULL);
    
    // Update ML features
    intelligence->ml_tower_density_feature = (uint8_t)(intelligence->density_analysis.density_score * 255);
    intelligence->ml_coverage_quality_feature = (uint8_t)(intelligence->density_analysis.coverage_score * 255);
    
    LOGX_DEBUG_MSG("Tower density analysis: usable=%d, home=%d, roaming=%d, density_score=%.2f, reliability_score=%.2f",
               intelligence->density_analysis.usable_towers_in_radius,
               intelligence->density_analysis.home_carrier_towers,
               intelligence->density_analysis.roaming_towers,
               intelligence->density_analysis.density_score,
               intelligence->density_analysis.reliability_score);
    
    return CELLULAR_TOWER_INTELLIGENCE_SUCCESS;
}

// Calculate density score
double cellular_tower_calculate_density_score(int usable_towers, int total_towers,
                                             double average_signal_strength) {
    if (total_towers == 0) return 0.0;
    
    // Tower count score (0.0-1.0)
    double count_score = fmin(1.0, (double)usable_towers / 10.0); // Normalize to 10 towers max
    
    // Signal strength score (0.0-1.0)
    double signal_score = fmax(0.0, fmin(1.0, (average_signal_strength + 140) / 90.0));
    
    // Usability ratio score (0.0-1.0)
    double usability_ratio = (double)usable_towers / total_towers;
    
    // Combined score
    double density_score = (count_score * 0.4) + (signal_score * 0.4) + (usability_ratio * 0.2);
    
    return fmax(0.0, fmin(1.0, density_score));
}

// Analyze cell change patterns
int cellular_tower_intelligence_analyze_cell_changes(cellular_tower_intelligence_t *intelligence,
                                                    const char *current_cell_id,
                                                    const cellular_info_t *cellular_info) {
    if (!intelligence || !current_cell_id) {
        return CELLULAR_TOWER_INTELLIGENCE_ERROR_INVALID_PARAM;
    }
    
    time_t now = time(NULL);
    
    // Check if cell has changed
    if (strcmp(current_cell_id, intelligence->last_serving_cell) != 0) {
        // Record the cell change
        uint8_t idx = intelligence->cell_change_analysis.change_index;
        
        strncpy(intelligence->cell_change_analysis.recent_changes[idx].cell_id, 
                current_cell_id, sizeof(intelligence->cell_change_analysis.recent_changes[idx].cell_id) - 1);
        intelligence->cell_change_analysis.recent_changes[idx].change_time = now;
        intelligence->cell_change_analysis.recent_changes[idx].signal_strength = cellular_info ? cellular_info->rsrp : -120;
        intelligence->cell_change_analysis.recent_changes[idx].was_forced = false; // Would need more context to determine
        
        intelligence->cell_change_analysis.change_index = (intelligence->cell_change_analysis.change_index + 1) % 20;
        if (intelligence->cell_change_analysis.change_count < 20) {
            intelligence->cell_change_analysis.change_count++;
        }
        
        strncpy(intelligence->last_serving_cell, current_cell_id, sizeof(intelligence->last_serving_cell) - 1);
        
        LOGX_DEBUG_MSG("Cell change detected: %s -> %s", intelligence->last_serving_cell, current_cell_id);
    }
    
    // Analyze change patterns
    intelligence->cell_change_analysis.changes_last_hour = 0;
    intelligence->cell_change_analysis.changes_last_day = 0;
    
    time_t one_hour_ago = now - 3600;
    time_t one_day_ago = now - 86400;
    
    for (uint8_t i = 0; i < intelligence->cell_change_analysis.change_count; i++) {
        time_t change_time = intelligence->cell_change_analysis.recent_changes[i].change_time;
        
        if (change_time >= one_hour_ago) {
            intelligence->cell_change_analysis.changes_last_hour++;
        }
        if (change_time >= one_day_ago) {
            intelligence->cell_change_analysis.changes_last_day++;
        }
    }
    
    // Calculate change frequency score
    if (intelligence->cell_change_analysis.changes_last_hour > intelligence->config.max_cell_changes_per_hour) {
        intelligence->cell_change_analysis.change_frequency_score = 0.0; // Poor
        intelligence->cell_change_analysis.high_mobility_detected = true;
    } else {
        intelligence->cell_change_analysis.change_frequency_score = 
            1.0 - ((double)intelligence->cell_change_analysis.changes_last_hour / intelligence->config.max_cell_changes_per_hour);
        intelligence->cell_change_analysis.high_mobility_detected = false;
    }
    
    // Detect poor coverage pattern
    intelligence->cell_change_analysis.poor_coverage_detected = 
        (intelligence->cell_change_analysis.changes_last_hour > 5) && 
        (intelligence->density_analysis.average_signal_strength < -100);
    
    // Detect network congestion pattern
    intelligence->cell_change_analysis.network_congestion_detected = 
        (intelligence->cell_change_analysis.changes_last_hour > 3) && 
        (intelligence->density_analysis.usable_towers_in_radius > 5);
    
    // Calculate connectivity risk score
    intelligence->cell_change_analysis.connectivity_risk_score = 
        (1.0 - intelligence->cell_change_analysis.change_frequency_score) * 0.4 +
        (intelligence->cell_change_analysis.poor_coverage_detected ? 0.3 : 0.0) +
        (intelligence->cell_change_analysis.network_congestion_detected ? 0.3 : 0.0);
    
    intelligence->cell_change_analysis.connectivity_risk_score = fmax(0.0, fmin(1.0, intelligence->cell_change_analysis.connectivity_risk_score));
    intelligence->cell_change_analysis.last_analysis_time = now;
    
    // Update ML features
    intelligence->ml_cell_change_feature = (uint8_t)(intelligence->cell_change_analysis.change_frequency_score * 255);
    intelligence->ml_connectivity_risk_feature = (uint8_t)(intelligence->cell_change_analysis.connectivity_risk_score * 255);
    
    LOGX_DEBUG_MSG("Cell change analysis: changes_last_hour=%d, frequency_score=%.2f, risk_score=%.2f",
               intelligence->cell_change_analysis.changes_last_hour,
               intelligence->cell_change_analysis.change_frequency_score,
               intelligence->cell_change_analysis.connectivity_risk_score);
    
    return CELLULAR_TOWER_INTELLIGENCE_SUCCESS;
}

// Get ML features for integration
int cellular_tower_intelligence_get_ml_features(const cellular_tower_intelligence_t *intelligence,
                                               uint8_t *tower_density_feature,
                                               uint8_t *cell_change_feature,
                                               uint8_t *coverage_quality_feature,
                                               uint8_t *connectivity_risk_feature) {
    if (!intelligence || !tower_density_feature || !cell_change_feature || 
        !coverage_quality_feature || !connectivity_risk_feature) {
        return CELLULAR_TOWER_INTELLIGENCE_ERROR_INVALID_PARAM;
    }
    
    *tower_density_feature = intelligence->ml_tower_density_feature;
    *cell_change_feature = intelligence->ml_cell_change_feature;
    *coverage_quality_feature = intelligence->ml_coverage_quality_feature;
    *connectivity_risk_feature = intelligence->ml_connectivity_risk_feature;
    
    return CELLULAR_TOWER_INTELLIGENCE_SUCCESS;
}

// Update ML observation with cellular tower data
int cellular_tower_intelligence_update_ml_observation(ml_observation_t *observation,
                                                     const cellular_tower_intelligence_t *intelligence) {
    if (!observation || !intelligence) {
        return CELLULAR_TOWER_INTELLIGENCE_ERROR_INVALID_PARAM;
    }
    
    // Store tower density in reserved field (upper 8 bits)
    observation->reserved = (observation->reserved & 0x00FF) | (intelligence->ml_tower_density_feature << 8);
    
    // Store cell change pattern in pattern_hash (upper 8 bits)
    observation->pattern_hash = (observation->pattern_hash & 0x00FF) | (intelligence->ml_cell_change_feature << 8);
    
    // Store coverage quality in anomaly_score
    observation->anomaly_score = intelligence->ml_coverage_quality_feature;
    
    // Store connectivity risk in confidence (inverted - higher risk = lower confidence)
    observation->confidence = 255 - intelligence->ml_connectivity_risk_feature;
    
    return CELLULAR_TOWER_INTELLIGENCE_SUCCESS;
}

// Get density analysis
int cellular_tower_intelligence_get_density_analysis(const cellular_tower_intelligence_t *intelligence,
                                                    tower_density_analysis_t *analysis) {
    if (!intelligence || !analysis) {
        return CELLULAR_TOWER_INTELLIGENCE_ERROR_INVALID_PARAM;
    }
    
    *analysis = intelligence->density_analysis;
    return CELLULAR_TOWER_INTELLIGENCE_SUCCESS;
}

// Get cell change analysis
int cellular_tower_intelligence_get_cell_change_analysis(const cellular_tower_intelligence_t *intelligence,
                                                        cell_change_pattern_analysis_t *analysis) {
    if (!intelligence || !analysis) {
        return CELLULAR_TOWER_INTELLIGENCE_ERROR_INVALID_PARAM;
    }
    
    *analysis = intelligence->cell_change_analysis;
    return CELLULAR_TOWER_INTELLIGENCE_SUCCESS;
}

// Set density callback
int cellular_tower_intelligence_set_density_callback(cellular_tower_intelligence_t *intelligence,
                                                    void (*callback)(const tower_density_analysis_t *analysis, void *user_data),
                                                    void *user_data) {
    if (!intelligence) {
        return CELLULAR_TOWER_INTELLIGENCE_ERROR_INVALID_PARAM;
    }
    
    intelligence->tower_density_callback = callback;
    intelligence->callback_user_data = user_data;
    
    return CELLULAR_TOWER_INTELLIGENCE_SUCCESS;
}

// Set cell change callback
int cellular_tower_intelligence_set_cell_change_callback(cellular_tower_intelligence_t *intelligence,
                                                        void (*callback)(const cell_change_pattern_analysis_t *analysis, void *user_data),
                                                        void *user_data) {
    if (!intelligence) {
        return CELLULAR_TOWER_INTELLIGENCE_ERROR_INVALID_PARAM;
    }
    
    intelligence->cell_change_callback = callback;
    intelligence->callback_user_data = user_data;
    
    return CELLULAR_TOWER_INTELLIGENCE_SUCCESS;
}

// Set connectivity risk callback
int cellular_tower_intelligence_set_connectivity_risk_callback(cellular_tower_intelligence_t *intelligence,
                                                              void (*callback)(double risk_score, void *user_data),
                                                              void *user_data) {
    if (!intelligence) {
        return CELLULAR_TOWER_INTELLIGENCE_ERROR_INVALID_PARAM;
    }
    
    intelligence->connectivity_risk_callback = callback;
    intelligence->callback_user_data = user_data;
    
    return CELLULAR_TOWER_INTELLIGENCE_SUCCESS;
}

// Analyze cellular towers
int cellular_tower_intelligence_analyze_towers(cellular_tower_intelligence_t *intelligence,
                                              const opencellid_cellular_environment_t *environment,
                                              const cellular_info_t *cellular_info) {
    if (!intelligence || !environment || !cellular_info) {
        return CELLULAR_TOWER_INTELLIGENCE_ERROR_INVALID_PARAM;
    }

    // Update last analysis time
    intelligence->last_update_time = time(NULL);

    // Perform tower density analysis if we have usable towers
    if (intelligence->usable_tower_count > 0) {
        // Use current GPS location if available
        double current_lat = 0.0, current_lon = 0.0;
        
        // Try to get current location from environment GPS data
        if (environment->gps_valid && environment->gps_latitude != 0.0 && environment->gps_longitude != 0.0) {
            current_lat = environment->gps_latitude;
            current_lon = environment->gps_longitude;
        }

        // Perform density analysis
        cellular_tower_intelligence_analyze_density(intelligence, 
                                                   intelligence->usable_towers,
                                                   intelligence->usable_tower_count,
                                                   current_lat, current_lon);
    }

    // Update ML features based on analysis
    intelligence->ml_tower_density_feature = (uint8_t)(intelligence->density_analysis.density_score * 255);
    intelligence->ml_coverage_quality_feature = (uint8_t)(intelligence->density_analysis.coverage_score * 255);
    intelligence->ml_connectivity_risk_feature = (uint8_t)(intelligence->cell_change_analysis.connectivity_risk_score * 255);

    return CELLULAR_TOWER_INTELLIGENCE_SUCCESS;
}
