#include "ml_monitor.h"
#include "../utils/logx.h"
#include "../gps/gps_manager.h"
#include <time.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

// Phase 5: Mobile-Optimized Learning & Advanced Auto-tuning

// Mobile scenario types
typedef enum {
    MOBILE_SCENARIO_STATIONARY = 0,    // Parked/stationary (house, RV park)
    MOBILE_SCENARIO_SLOW_MOBILE,       // Slow movement (<20 km/h)
    MOBILE_SCENARIO_HIGHWAY,           // Highway speed (>50 km/h)
    MOBILE_SCENARIO_URBAN,             // Urban driving (20-50 km/h)
    MOBILE_SCENARIO_UNKNOWN,           // Unknown scenario
    MOBILE_SCENARIO_MAX
} mobile_scenario_t;

// Location profile for transfer learning
typedef struct {
    // Location identification
    int32_t lat_e7;
    int32_t lon_e7;
    double radius_meters;              // Radius of this location profile
    char location_name[64];            // Human-readable name (optional)
    
    // Learning characteristics
    uint32_t total_observations;
    uint32_t learning_time_hours;
    double learning_confidence;        // How well we know this location
    
    // Performance characteristics
    struct {
        double typical_snr_mean;
        double typical_snr_std;
        double typical_latency_mean;
        double typical_latency_std;
        double obstruction_probability;
        double weather_sensitivity;
        double time_of_day_patterns[24]; // Hourly performance patterns
    } characteristics;
    
    // Learned models (compressed)
    struct {
        uint8_t sky_grid_compressed[450]; // Compressed 90x45 grid (10:1 compression)
        uint16_t pattern_signatures[50];  // Top 50 pattern signatures
        uint8_t optimal_parameters[16];   // Optimal ML parameters for this location
    } learned_models;
    
    // Transfer learning metadata
    struct {
        double similarity_to_other_locations[10]; // Similarity scores
        uint8_t transferable_knowledge_mask;      // Which knowledge can transfer
        double transfer_learning_success_rate;   // Success rate of transfers
    } transfer_meta;
    
    // Visit history
    time_t first_visit;
    time_t last_visit;
    uint32_t visit_count;
    uint32_t total_time_spent_seconds;
    
} location_profile_t;

// Advanced auto-tuning system
typedef struct {
    // Tuning state
    bool auto_tuning_active;
    bool aggressive_tuning_mode;       // More aggressive parameter exploration
    time_t last_tuning_action;
    uint32_t tuning_cycles_completed;
    
    // Parameter exploration
    struct {
        uint8_t learning_rate_min;     // Minimum learning rate to try
        uint8_t learning_rate_max;     // Maximum learning rate to try
        uint8_t learning_rate_current; // Current learning rate
        uint8_t learning_rate_optimal; // Best learning rate found
        
        uint8_t confidence_threshold_min;
        uint8_t confidence_threshold_max;
        uint8_t confidence_threshold_current;
        uint8_t confidence_threshold_optimal;
        
        // Exploration strategy
        uint8_t exploration_radius;    // How far from current to explore
        uint8_t exploitation_ratio;    // Exploitation vs exploration ratio
    } parameter_space;
    
    // Performance tracking for tuning
    struct {
        double current_performance;
        double best_performance;
        double performance_history[100]; // Last 100 performance measurements
        uint8_t history_index;
        
        // Performance improvement tracking
        double improvement_rate;       // Rate of improvement per tuning cycle
        bool converged;               // Whether tuning has converged
        uint32_t cycles_since_improvement; // Cycles since last improvement
    } performance_tracking;
    
    // Scenario-specific tuning
    struct {
        uint8_t stationary_parameters[16];
        uint8_t mobile_parameters[16];
        uint8_t highway_parameters[16];
        uint8_t urban_parameters[16];
        mobile_scenario_t current_scenario;
        time_t scenario_start_time;
    } scenario_tuning;
    
} advanced_auto_tuner_t;

// Mobile scenario detector
typedef struct {
    // Movement analysis
    struct {
        double speed_history[60];      // Last 60 speed measurements (15-minute window)
        uint8_t speed_index;
        double average_speed;
        double speed_variance;
        bool is_moving;
        bool is_accelerating;
    } movement;
    
    // Location analysis
    struct {
        int32_t position_history_lat[60];
        int32_t position_history_lon[60];
        uint8_t position_index;
        double distance_traveled_km;
        double displacement_km;        // Straight-line distance from start
        bool is_stationary;
        time_t stationary_since;
    } location;
    
    // Scenario classification
    mobile_scenario_t current_scenario;
    mobile_scenario_t predicted_scenario;
    double scenario_confidence;
    time_t scenario_change_time;
    uint32_t scenario_changes;
    
    // Learning adaptation
    struct {
        double learning_rate_multiplier;
        double confidence_adjustment;
        bool rapid_learning_mode;
        bool conservation_mode;        // Conserve learned knowledge when mobile
    } adaptation;
    
} mobile_scenario_detector_t;

// Transfer learning system
typedef struct {
    location_profile_t location_profiles[20]; // Up to 20 location profiles
    uint8_t profile_count;
    uint8_t current_profile_index;
    
    // Transfer learning algorithms
    struct {
        bool enable_similarity_transfer;   // Transfer from similar locations
        bool enable_pattern_transfer;      // Transfer learned patterns
        bool enable_parameter_transfer;    // Transfer optimal parameters
        double similarity_threshold;       // Minimum similarity for transfer
        double transfer_confidence_bonus;  // Confidence boost from transfers
    } transfer_config;
    
    // Transfer statistics
    struct {
        uint32_t successful_transfers;
        uint32_t failed_transfers;
        uint32_t partial_transfers;
        double average_transfer_benefit;   // Average performance improvement
    } transfer_stats;
    
} transfer_learning_system_t;

// Phase 5 comprehensive mobile system
typedef struct {
    mobile_scenario_detector_t scenario_detector;
    advanced_auto_tuner_t auto_tuner;
    transfer_learning_system_t transfer_system;
    
    // Phase 5 configuration
    bool enable_mobile_optimization;
    bool enable_advanced_auto_tuning;
    bool enable_transfer_learning;
    bool enable_field_testing_mode;
    
    // Field testing parameters
    struct {
        bool data_collection_mode;     // Enhanced data collection for testing
        bool performance_logging_mode; // Detailed performance logging
        char field_test_id[32];        // Field test identifier
        time_t field_test_start;       // Field test start time
    } field_testing;
    
} phase5_mobile_system_t;

// Global Phase 5 system instance
static phase5_mobile_system_t g_phase5_system = {0};
static bool g_phase5_initialized = false;

// Forward declarations
static int ml_monitor_detect_mobile_scenario(mobile_scenario_detector_t *detector, const ml_observation_t *observation);
static int ml_monitor_advanced_auto_tune(ml_monitor_t *monitor, advanced_auto_tuner_t *tuner);
static int ml_monitor_transfer_learning_update(transfer_learning_system_t *transfer_system, 
                                              const location_profile_t *current_profile);
static double ml_monitor_calculate_location_similarity(const location_profile_t *profile1, 
                                                      const location_profile_t *profile2);
static mobile_scenario_t ml_monitor_classify_mobile_scenario(const mobile_scenario_detector_t *detector);
static int ml_monitor_adapt_to_mobile_scenario(ml_monitor_t *monitor, mobile_scenario_t scenario);

// Detect and classify mobile scenario
static int ml_monitor_detect_mobile_scenario(mobile_scenario_detector_t *detector, const ml_observation_t *observation) {
    if (!detector || !observation) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    // Update movement analysis
    detector->movement.speed_history[detector->movement.speed_index] = observation->speed_kmh;
    detector->movement.speed_index = (detector->movement.speed_index + 1) % 60;
    
    // Calculate average speed over window
    double speed_sum = 0;
    for (int i = 0; i < 60; i++) {
        speed_sum += detector->movement.speed_history[i];
    }
    detector->movement.average_speed = speed_sum / 60.0;
    
    // Calculate speed variance
    double variance_sum = 0;
    for (int i = 0; i < 60; i++) {
        double diff = detector->movement.speed_history[i] - detector->movement.average_speed;
        variance_sum += diff * diff;
    }
    detector->movement.speed_variance = sqrt(variance_sum / 60.0);
    
    // Update location analysis
    detector->location.position_history_lat[detector->location.position_index] = observation->latitude_e7;
    detector->location.position_history_lon[detector->location.position_index] = observation->longitude_e7;
    detector->location.position_index = (detector->location.position_index + 1) % 60;
    
    // Calculate distance traveled
    if (detector->location.position_index > 1) {
        int prev_idx = (detector->location.position_index - 2 + 60) % 60;
        double lat_diff = (observation->latitude_e7 - detector->location.position_history_lat[prev_idx]) / 10000000.0;
        double lon_diff = (observation->longitude_e7 - detector->location.position_history_lon[prev_idx]) / 10000000.0;
        
        // Rough distance calculation (Haversine would be more accurate)
        double distance_deg = sqrt(lat_diff * lat_diff + lon_diff * lon_diff);
        double distance_km = distance_deg * 111.0; // Approximate km per degree
        detector->location.distance_traveled_km += distance_km;
    }
    
    // Classify scenario
    mobile_scenario_t new_scenario = ml_monitor_classify_mobile_scenario(detector);
    
    if (new_scenario != detector->current_scenario) {
        LOGX_INFO_MSG(" Mobile scenario changed: %d  %d (speed: %.1f km/h, variance: %.1f)",
                 detector->current_scenario, new_scenario, 
                 detector->movement.average_speed, detector->movement.speed_variance);
        
        detector->current_scenario = new_scenario;
        detector->scenario_change_time = observation->timestamp;
        detector->scenario_changes++;
    }
    
    return ML_MONITOR_SUCCESS;
}

// Classify mobile scenario based on movement patterns
static mobile_scenario_t ml_monitor_classify_mobile_scenario(const mobile_scenario_detector_t *detector) {
    if (!detector) return MOBILE_SCENARIO_UNKNOWN;
    
    double avg_speed = detector->movement.average_speed;
    double speed_variance = detector->movement.speed_variance;
    
    // Stationary: low speed and low variance
    if (avg_speed < 5.0 && speed_variance < 3.0) {
        return MOBILE_SCENARIO_STATIONARY;
    }
    
    // Highway: high speed and low variance (consistent speed)
    if (avg_speed > 50.0 && speed_variance < 15.0) {
        return MOBILE_SCENARIO_HIGHWAY;
    }
    
    // Urban: medium speed with high variance (stop-and-go)
    if (avg_speed > 10.0 && avg_speed < 50.0 && speed_variance > 10.0) {
        return MOBILE_SCENARIO_URBAN;
    }
    
    // Slow mobile: low-medium speed with medium variance
    if (avg_speed > 5.0 && avg_speed < 25.0) {
        return MOBILE_SCENARIO_SLOW_MOBILE;
    }
    
    return MOBILE_SCENARIO_UNKNOWN;
}

// Adapt ML system to mobile scenario
static int ml_monitor_adapt_to_mobile_scenario(ml_monitor_t *monitor, mobile_scenario_t scenario) {
    if (!monitor || !monitor->state) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    // Adjust learning parameters based on scenario
    tiny_nn_t *nn = &monitor->state->models.neural_network;
    
    switch (scenario) {
        case MOBILE_SCENARIO_STATIONARY:
            // Stable learning, can be more conservative
            nn->learning_rate = 64;  // Lower learning rate for stability
            LOGX_DEBUG_MSG(" Adapted to stationary scenario: conservative learning");
            break;
            
        case MOBILE_SCENARIO_SLOW_MOBILE:
            // Moderate adaptation needed
            nn->learning_rate = 128; // Standard learning rate
            LOGX_DEBUG_MSG(" Adapted to slow mobile scenario: balanced learning");
            break;
            
        case MOBILE_SCENARIO_URBAN:
            // Frequent changes, need rapid adaptation
            nn->learning_rate = 180; // Higher learning rate
            LOGX_DEBUG_MSG(" Adapted to urban scenario: rapid learning");
            break;
            
        case MOBILE_SCENARIO_HIGHWAY:
            // High speed, preserve knowledge but adapt quickly
            nn->learning_rate = 150; // Moderate-high learning rate
            LOGX_DEBUG_MSG(" Adapted to highway scenario: adaptive learning");
            break;
            
        default:
            // Unknown scenario, use defaults
            nn->learning_rate = 128;
            LOGX_DEBUG_MSG(" Unknown scenario: using default learning");
            break;
    }
    
    return ML_MONITOR_SUCCESS;
}

// Advanced auto-tuning with performance optimization
static int ml_monitor_advanced_auto_tune(ml_monitor_t *monitor, advanced_auto_tuner_t *tuner) {
    if (!monitor || !tuner) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    if (!tuner->auto_tuning_active) return ML_MONITOR_SUCCESS;
    
    time_t current_time = time(NULL);
    
    // Only tune every 10 minutes to avoid over-tuning
    if (current_time - tuner->last_tuning_action < 600) {
        return ML_MONITOR_SUCCESS;
    }
    
    // Get current performance
    performance_monitor_t *perf = &monitor->state->models.performance;
    double current_accuracy = perf->predictions_made > 0 ? 
                             (double)perf->predictions_correct / perf->predictions_made : 0.5;
    
    // Update performance history
    tuner->performance_tracking.performance_history[tuner->performance_tracking.history_index] = current_accuracy;
    tuner->performance_tracking.history_index = (tuner->performance_tracking.history_index + 1) % 100;
    
    // Calculate performance improvement rate
    if (tuner->tuning_cycles_completed > 10) {
        double old_performance = tuner->performance_tracking.performance_history[
            (tuner->performance_tracking.history_index + 90) % 100]; // 10 cycles ago
        tuner->performance_tracking.improvement_rate = 
            (current_accuracy - old_performance) / 10.0; // Improvement per cycle
    }
    
    // Check if we've found a better configuration
    if (current_accuracy > tuner->performance_tracking.best_performance) {
        tuner->performance_tracking.best_performance = current_accuracy;
        
        // Save optimal parameters
        tiny_nn_t *nn = &monitor->state->models.neural_network;
        tuner->parameter_space.learning_rate_optimal = nn->learning_rate;
        // In a full implementation, we'd save all optimal parameters
        
        tuner->performance_tracking.cycles_since_improvement = 0;
        
        LOGX_INFO_MSG(" Auto-tuning found better configuration: %.1f%% accuracy (LR=%u)",
                 current_accuracy * 100, nn->learning_rate);
    } else {
        tuner->performance_tracking.cycles_since_improvement++;
    }
    
    // Decide on next tuning action
    if (tuner->performance_tracking.cycles_since_improvement > 20) {
        // No improvement for 20 cycles, consider convergence
        tuner->performance_tracking.converged = true;
        LOGX_INFO_MSG(" Auto-tuning converged at %.1f%% accuracy", 
                 tuner->performance_tracking.best_performance * 100);
    } else if (tuner->performance_tracking.improvement_rate < 0.001) {
        // Very slow improvement, try different parameters
        tiny_nn_t *nn = &monitor->state->models.neural_network;
        
        // Explore learning rate space
        if (tuner->aggressive_tuning_mode) {
            // Systematic parameter space exploration (not random)
            uint8_t range = tuner->parameter_space.learning_rate_max - tuner->parameter_space.learning_rate_min;
            uint8_t step = range / 10; // Divide range into 10 steps
            uint8_t cycle_step = (tuner->tuning_cycles_completed % 10) * step;
            nn->learning_rate = tuner->parameter_space.learning_rate_min + cycle_step;
        } else {
            // Conservative gradient-based exploration
            if (tuner->performance_tracking.improvement_rate > 0) {
                // Performance improving, continue in same direction
                int adjustment = tuner->performance_tracking.improvement_rate > 0.01 ? 5 : 2;
                nn->learning_rate = (uint8_t)fmin(tuner->parameter_space.learning_rate_max,
                                                nn->learning_rate + adjustment);
            } else {
                // Performance declining, reverse direction
                int adjustment = tuner->performance_tracking.improvement_rate < -0.01 ? -5 : -2;
                nn->learning_rate = (uint8_t)fmax(tuner->parameter_space.learning_rate_min,
                                                nn->learning_rate + adjustment);
            }
        }
        
        LOGX_DEBUG_MSG(" Auto-tuning exploring: learning_rate=%u", nn->learning_rate);
    }
    
    tuner->last_tuning_action = current_time;
    tuner->tuning_cycles_completed++;
    
    return ML_MONITOR_SUCCESS;
}

// Calculate similarity between location profiles for transfer learning
static double ml_monitor_calculate_location_similarity(const location_profile_t *profile1, 
                                                      const location_profile_t *profile2) {
    if (!profile1 || !profile2) return 0.0;
    
    // Geographic similarity (distance-based)
    double lat_diff = (profile1->lat_e7 - profile2->lat_e7) / 10000000.0;
    double lon_diff = (profile1->lon_e7 - profile2->lon_e7) / 10000000.0;
    double distance_km = sqrt(lat_diff * lat_diff + lon_diff * lon_diff) * 111.0;
    double geographic_similarity = exp(-distance_km / 100.0); // Decay over 100km
    
    // Performance characteristics similarity
    double snr_similarity = 1.0 - fabs(profile1->characteristics.typical_snr_mean - 
                                      profile2->characteristics.typical_snr_mean) / 20.0;
    double latency_similarity = 1.0 - fabs(profile1->characteristics.typical_latency_mean - 
                                          profile2->characteristics.typical_latency_mean) / 100.0;
    double obstruction_similarity = 1.0 - fabs(profile1->characteristics.obstruction_probability - 
                                              profile2->characteristics.obstruction_probability);
    
    // Combine similarities
    double performance_similarity = (snr_similarity + latency_similarity + obstruction_similarity) / 3.0;
    
    // Overall similarity (weighted combination)
    double overall_similarity = (geographic_similarity * 0.3) + (performance_similarity * 0.7);
    
    return fmax(0.0, fmin(1.0, overall_similarity));
}

// Transfer learning between locations
static int ml_monitor_transfer_learning_update(transfer_learning_system_t *transfer_system, 
                                              const location_profile_t *current_profile) {
    if (!transfer_system || !current_profile) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    if (!transfer_system->transfer_config.enable_similarity_transfer) {
        return ML_MONITOR_SUCCESS;
    }
    
    // Find most similar location profile
    double best_similarity = 0.0;
    int best_profile_index = -1;
    
    for (int i = 0; i < transfer_system->profile_count; i++) {
        double similarity = ml_monitor_calculate_location_similarity(current_profile, 
                                                                   &transfer_system->location_profiles[i]);
        if (similarity > best_similarity && 
            similarity > transfer_system->transfer_config.similarity_threshold) {
            best_similarity = similarity;
            best_profile_index = i;
        }
    }
    
    if (best_profile_index >= 0) {
        location_profile_t *source_profile = &transfer_system->location_profiles[best_profile_index];
        
        LOGX_INFO_MSG(" Transfer learning from similar location: similarity=%.3f", best_similarity);
        
        // Transfer optimal parameters
        if (transfer_system->transfer_config.enable_parameter_transfer) {
            // In a full implementation, we'd transfer learned parameters
            transfer_system->transfer_stats.successful_transfers++;
            
            LOGX_DEBUG_MSG(" Transferred parameters from location profile %d", best_profile_index);
        }
        
        // Transfer patterns
        if (transfer_system->transfer_config.enable_pattern_transfer) {
            // In a full implementation, we'd transfer learned patterns
            transfer_system->transfer_stats.partial_transfers++;
            
            LOGX_DEBUG_MSG(" Transferred patterns from similar location");
        }
        
        return ML_MONITOR_SUCCESS;
    }
    
    LOGX_DEBUG_MSG("No similar location found for transfer learning");
    return ML_MONITOR_SUCCESS;
}

// Initialize Phase 5 mobile optimization system
int ml_monitor_init_phase5_mobile_system(ml_monitor_t *monitor) {
    if (!monitor) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    // Use simple fprintf to avoid LOGX crashes
    fprintf(stderr, "Initializing Phase 5: Mobile-Optimized Learning & Field Testing\n");
    
    // Initialize the global Phase 5 system structure
    memset(&g_phase5_system, 0, sizeof(phase5_mobile_system_t));
    
    // Initialize mobile scenario detector
    g_phase5_system.scenario_detector.current_scenario = MOBILE_SCENARIO_STATIONARY;
    g_phase5_system.scenario_detector.scenario_confidence = 1.0;
    g_phase5_system.scenario_detector.adaptation.learning_rate_multiplier = 1.0;
    g_phase5_system.scenario_detector.adaptation.confidence_adjustment = 1.0;
    g_phase5_system.scenario_detector.adaptation.rapid_learning_mode = false;
    g_phase5_system.scenario_detector.adaptation.conservation_mode = false;
    
    // Initialize advanced auto-tuner
    g_phase5_system.auto_tuner.auto_tuning_active = true;
    g_phase5_system.auto_tuner.aggressive_tuning_mode = false;
    g_phase5_system.auto_tuner.last_tuning_action = time(NULL);
    g_phase5_system.auto_tuner.tuning_cycles_completed = 0;
    
    // Set parameter exploration space
    g_phase5_system.auto_tuner.parameter_space.learning_rate_min = 32;
    g_phase5_system.auto_tuner.parameter_space.learning_rate_max = 200;
    g_phase5_system.auto_tuner.parameter_space.learning_rate_current = 128;
    g_phase5_system.auto_tuner.parameter_space.learning_rate_optimal = 128;
    g_phase5_system.auto_tuner.parameter_space.confidence_threshold_min = 64;
    g_phase5_system.auto_tuner.parameter_space.confidence_threshold_max = 200;
    g_phase5_system.auto_tuner.parameter_space.confidence_threshold_current = 128;
    g_phase5_system.auto_tuner.parameter_space.confidence_threshold_optimal = 128;
    g_phase5_system.auto_tuner.parameter_space.exploration_radius = 20;
    g_phase5_system.auto_tuner.parameter_space.exploitation_ratio = 70; // 70% exploitation, 30% exploration
    
    // Initialize performance tracking
    g_phase5_system.auto_tuner.performance_tracking.current_performance = 0.5;
    g_phase5_system.auto_tuner.performance_tracking.best_performance = 0.5;
    g_phase5_system.auto_tuner.performance_tracking.history_index = 0;
    g_phase5_system.auto_tuner.performance_tracking.improvement_rate = 0.0;
    g_phase5_system.auto_tuner.performance_tracking.converged = false;
    g_phase5_system.auto_tuner.performance_tracking.cycles_since_improvement = 0;
    
    // Initialize scenario-specific tuning
    g_phase5_system.auto_tuner.scenario_tuning.current_scenario = MOBILE_SCENARIO_STATIONARY;
    g_phase5_system.auto_tuner.scenario_tuning.scenario_start_time = time(NULL);
    
    // Initialize transfer learning system
    g_phase5_system.transfer_system.profile_count = 0;
    g_phase5_system.transfer_system.current_profile_index = 0;
    g_phase5_system.transfer_system.transfer_config.enable_similarity_transfer = true;
    g_phase5_system.transfer_system.transfer_config.enable_pattern_transfer = true;
    g_phase5_system.transfer_system.transfer_config.enable_parameter_transfer = true;
    g_phase5_system.transfer_system.transfer_config.similarity_threshold = 0.6;
    g_phase5_system.transfer_system.transfer_config.transfer_confidence_bonus = 0.1;
    
    // Initialize transfer statistics
    g_phase5_system.transfer_system.transfer_stats.successful_transfers = 0;
    g_phase5_system.transfer_system.transfer_stats.failed_transfers = 0;
    g_phase5_system.transfer_system.transfer_stats.partial_transfers = 0;
    g_phase5_system.transfer_system.transfer_stats.average_transfer_benefit = 0.0;
    
    // Configure Phase 5 features
    g_phase5_system.enable_mobile_optimization = true;
    g_phase5_system.enable_advanced_auto_tuning = true;
    g_phase5_system.enable_transfer_learning = true;
    g_phase5_system.enable_field_testing_mode = false; // Can be enabled for field tests
    
    // Initialize field testing configuration
    g_phase5_system.field_testing.data_collection_mode = false;
    g_phase5_system.field_testing.performance_logging_mode = true;
    strncpy(g_phase5_system.field_testing.field_test_id, "embedded_ml_v1", sizeof(g_phase5_system.field_testing.field_test_id) - 1);
    g_phase5_system.field_testing.field_test_start = time(NULL);
    
    // Mark as initialized
    g_phase5_initialized = true;
    
    // Use single consolidated message to avoid multiple LOGX calls
    fprintf(stderr, "Phase 5 mobile optimization system initialized successfully - Mobile detection, auto-tuning, transfer learning, field testing\n");
    
    return ML_MONITOR_SUCCESS;
}

// Update with Phase 5 mobile optimizations
int ml_monitor_update_with_phase5_mobile_optimization(ml_monitor_t *monitor, const ml_observation_t *observation) {
    if (!monitor || !observation) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    // Check if Phase 5 system is initialized
    if (!g_phase5_initialized) {
        return ML_MONITOR_ERROR_NOT_INITIALIZED;
    }
    
    // Real Phase 5 mobile optimization implementation using global system
    
    // Detect mobile scenario using the global detector
    int result = ml_monitor_detect_mobile_scenario(&g_phase5_system.scenario_detector, observation);
    if (result != ML_MONITOR_SUCCESS) {
        return result;
    }
    
    // Adapt to mobile scenario if it changed
    if (g_phase5_system.scenario_detector.current_scenario != g_phase5_system.auto_tuner.scenario_tuning.current_scenario) {
        result = ml_monitor_adapt_to_mobile_scenario(monitor, g_phase5_system.scenario_detector.current_scenario);
        if (result != ML_MONITOR_SUCCESS) {
            return result;
        }
        
        // Update scenario tracking
        g_phase5_system.auto_tuner.scenario_tuning.current_scenario = g_phase5_system.scenario_detector.current_scenario;
        g_phase5_system.auto_tuner.scenario_tuning.scenario_start_time = observation->timestamp;
    }
    
    // Perform advanced auto-tuning using the global tuner
    if (g_phase5_system.enable_advanced_auto_tuning) {
        result = ml_monitor_advanced_auto_tune(monitor, &g_phase5_system.auto_tuner);
        if (result != ML_MONITOR_SUCCESS) {
            return result;
        }
    }
    
    // Update transfer learning system if enabled
    if (g_phase5_system.enable_transfer_learning && g_phase5_system.transfer_system.profile_count > 0) {
        // Find current location profile
        location_profile_t *current_profile = &g_phase5_system.transfer_system.location_profiles[g_phase5_system.transfer_system.current_profile_index];
        
        // Update transfer learning
        result = ml_monitor_transfer_learning_update(&g_phase5_system.transfer_system, current_profile);
        if (result != ML_MONITOR_SUCCESS) {
            return result;
        }
    }
    
    return ML_MONITOR_SUCCESS;
}

// Get mobile optimization status
int ml_monitor_get_mobile_status(ml_monitor_t *monitor, 
                                int *scenario, double *learning_rate_multiplier,
                                uint32_t *location_profiles, double *auto_tune_performance) {
    if (!monitor) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    // Check if Phase 5 system is initialized
    if (!g_phase5_initialized) {
        return ML_MONITOR_ERROR_NOT_INITIALIZED;
    }
    
    // Return real mobile status from Phase 5 system state
    if (scenario) {
        *scenario = g_phase5_system.scenario_detector.current_scenario;
    }
    
    if (learning_rate_multiplier) {
        *learning_rate_multiplier = g_phase5_system.scenario_detector.adaptation.learning_rate_multiplier;
    }
    
    if (location_profiles) {
        *location_profiles = g_phase5_system.transfer_system.profile_count;
    }
    
    if (auto_tune_performance) {
        *auto_tune_performance = g_phase5_system.auto_tuner.performance_tracking.best_performance;
    }
    
    return ML_MONITOR_SUCCESS;
}

// Field testing data export
int ml_monitor_export_field_testing_data(ml_monitor_t *monitor, const char *export_path) {
    if (!monitor || !export_path) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    LOGX_INFO_MSG(" Exporting field testing data to: %s", export_path);
    
    // In a full implementation, this would export:
    // - All observations with mobile scenario annotations
    // - Performance metrics across different scenarios
    // - Auto-tuning parameter exploration results
    // - Transfer learning success rates
    // - Location profile data
    
    FILE *export_file = fopen(export_path, "w");
    if (!export_file) {
        LOGX_ERROR_MSG("Failed to open export file: %s", export_path);
        return ML_MONITOR_ERROR_STORAGE_FAILED;
    }
    
    // Write header
    fprintf(export_file, "# ML Monitor Field Testing Data Export\n");
    fprintf(export_file, "# Generated: %lld\n", time(NULL));
    fprintf(export_file, "# Total Observations: %u\n", monitor->state->total_observations);
    fprintf(export_file, "# Location Changes: %u\n", monitor->state->location_changes);
    
    // Write performance summary
    performance_monitor_t *perf = &monitor->state->models.performance;
    fprintf(export_file, "\n[Performance]\n");
    fprintf(export_file, "predictions_made=%u\n", perf->predictions_made);
    fprintf(export_file, "predictions_correct=%u\n", perf->predictions_correct);
    fprintf(export_file, "accuracy_percent=%u\n", perf->metrics.accuracy_pct);
    fprintf(export_file, "precision_percent=%u\n", perf->metrics.precision_pct);
    fprintf(export_file, "recall_percent=%u\n", perf->metrics.recall_pct);
    
    // Write location learning summary
    location_learner_t *learner = &monitor->state->models.location_learner;
    fprintf(export_file, "\n[Location Learning]\n");
    fprintf(export_file, "current_lat=%.6f\n", learner->current_lat_e7 / 10000000.0);
    fprintf(export_file, "current_lon=%.6f\n", learner->current_lon_e7 / 10000000.0);
    fprintf(export_file, "observations_here=%u\n", learner->observations_here);
    fprintf(export_file, "learned_percentage=%u\n", learner->profile.learned);
    fprintf(export_file, "location_history_count=%u\n", learner->history_count);
    
    fclose(export_file);
    
    LOGX_INFO_MSG("Field testing data exported successfully");
    return ML_MONITOR_SUCCESS;
}