#ifndef ML_MONITOR_ADVANCED_NETWORKING_H
#define ML_MONITOR_ADVANCED_NETWORKING_H

#include "ml_monitor_multi_interface.h"
#include <time.h>
#include <stdint.h>
#include <stdbool.h>

// Advanced Networking Intelligence - Enhanced Phase 7

// High-frequency monitoring configuration
typedef struct {
    // Interface-specific monitoring frequencies
    struct {
        uint32_t starlink_monitor_interval_ms;   // 1000ms (1 second)
        uint32_t wifi_monitor_interval_ms;       // 1000ms (1 second)  
        uint32_t lan_monitor_interval_ms;        // 1000ms (1 second)
        uint32_t cellular_monitor_interval_ms;   // 5000ms (5 seconds - data cost)
    } monitoring_intervals;
    
    // Ping test configuration
    struct {
        bool enable_continuous_ping_tests;
        char ping_targets[8][64];               // Multiple ping targets
        uint8_t ping_target_count;
        uint32_t ping_timeout_ms;               // Ping timeout
        uint32_t ping_packet_size;              // Ping packet size
        bool enable_adaptive_ping_frequency;    // Adapt frequency based on performance
    } ping_config;
    
    // Cellular monitoring optimization
    struct {
        bool monitor_cellular_modem_metrics;    // Monitor free modem metrics
        bool reduce_cellular_ping_frequency;    // Reduce pings to save data
        uint32_t cellular_data_budget_mb_per_day; // Daily data budget for monitoring
        uint32_t cellular_data_used_today_kb;   // Data used today for monitoring
    } cellular_optimization;
    
} high_frequency_monitor_config_t;

// Predictive failover system (vs reactive)
typedef struct {
    // Predictive thresholds (much more aggressive than reactive)
    struct {
        uint32_t latency_spike_threshold_ms;    // 100ms spike = immediate concern
        uint8_t packet_loss_spike_threshold;    // 3% loss = immediate concern
        uint32_t prediction_horizon_ms;         // 3000ms (3 seconds ahead)
        double confidence_threshold_for_action; // 0.7 confidence for action
    } predictive_thresholds;
    
    // Streaming protection (your key concern)
    struct {
        bool enable_streaming_protection_mode;  // Ultra-aggressive for streaming
        uint32_t streaming_tolerance_ms;        // 2000ms max interruption
        uint32_t streaming_prediction_window_ms; // 5000ms prediction window
        double streaming_confidence_threshold;   // 0.6 confidence for streaming protection
    } streaming_protection;
    
    // Predictive action timing
    struct {
        uint32_t action_lead_time_ms;           // 2000ms lead time for action
        uint32_t min_prediction_confidence;     // Minimum confidence for prediction
        bool enable_preemptive_failover;        // Failover before outage happens
    } action_timing;
    
} predictive_failover_system_t;

// Connection stability and flapping prevention
typedef struct {
    // Stability scoring per interface
    struct {
        char interface_id[32];
        double stability_score;                 // 0.0-1.0 (1.0 = perfectly stable)
        double recent_stability;                // Short-term stability (last hour)
        double long_term_stability;             // Long-term stability (last day)
        
        // Flapping detection
        uint32_t failover_events_last_hour;
        uint32_t failover_events_last_day;
        time_t last_failover_time;
        bool currently_flapping;                // Currently experiencing flapping
        time_t flapping_start_time;
        
        // Recovery tracking
        time_t last_stable_time;                // Last time interface was stable
        uint32_t stable_duration_seconds;       // How long it's been stable
        bool in_recovery_period;                // Currently recovering from issues
        uint32_t recovery_confidence_threshold; // Confidence needed to trust again
        
    } interface_stability[MAX_INTERFACES];
    uint8_t interface_count;
    
    // Flapping prevention logic
    struct {
        bool enable_flapping_prevention;
        uint32_t flapping_threshold_events_per_hour; // 3 events = flapping
        uint32_t flapping_penalty_duration_seconds;  // 1800s (30 min) penalty
        double flapping_weight_reduction_factor;     // 0.1 = reduce weight to 10%
        uint32_t stability_required_for_recovery_seconds; // 600s (10 min) stable required
    } flapping_prevention;
    
    // Preferred failover target selection
    struct {
        char preferred_targets[MAX_INTERFACES][32]; // Ordered list of preferred targets
        uint8_t preferred_target_count;
        double target_selection_weights[MAX_INTERFACES]; // ML-driven target weights
        time_t last_target_evaluation;
    } target_selection;
    
} connection_stability_system_t;

// Background validation intelligence
typedef struct {
    // Background monitoring of all interfaces
    struct {
        char interface_id[32];
        bool currently_primary;                 // Is this the active interface?
        bool background_monitoring_active;      // Monitoring in background
        
        // "What if" analysis
        struct {
            uint32_t background_observations;
            uint32_t predicted_outages_if_primary;
            uint32_t actual_outages_detected;
            double accuracy_if_this_was_primary;   // Accuracy if this was primary
            double performance_if_this_was_primary; // Performance if this was primary
        } what_if_analysis;
        
        // Model validation
        struct {
            uint32_t model_predictions_made;
            uint32_t model_predictions_correct;
            double model_accuracy;
            time_t last_model_update;
            bool model_needs_retraining;
        } model_validation;
        
    } background_interfaces[MAX_INTERFACES];
    uint8_t background_interface_count;
    
    // Cross-validation intelligence
    struct {
        bool enable_cross_validation;
        double cross_validation_accuracy;       // How well we predict backup performance
        uint32_t cross_validation_samples;
        time_t last_cross_validation;
    } cross_validation;
    
    // Ensemble validation across interfaces
    struct {
        bool enable_ensemble_validation;
        double ensemble_accuracy_across_interfaces;
        uint32_t ensemble_predictions_made;
        uint32_t ensemble_predictions_correct;
    } ensemble_validation;
    
} background_validation_intelligence_t;

// Advanced networking intelligence system
typedef struct {
    high_frequency_monitor_config_t high_freq_config;
    predictive_failover_system_t predictive_system;
    connection_stability_system_t stability_system;
    background_validation_intelligence_t background_intelligence;
    
    // Advanced features
    bool enable_streaming_protection;
    bool enable_flapping_prevention;
    bool enable_background_validation;
    bool enable_predictive_failover;
    
    // Performance tracking
    struct {
        uint32_t outages_prevented;             // Outages prevented by prediction
        uint32_t false_alarms;                  // False positive failovers
        uint32_t flapping_events_prevented;     // Flapping prevented
        double average_failover_accuracy;       // Accuracy of failover decisions
        time_t last_performance_update;
    } advanced_performance;
    
} advanced_networking_intelligence_t;

// API Functions

// Advanced networking intelligence initialization
advanced_networking_intelligence_t* ml_monitor_init_advanced_networking(const ml_monitor_config_t *config);
void ml_monitor_cleanup_advanced_networking(advanced_networking_intelligence_t *system);

// High-frequency monitoring
int ml_monitor_start_high_frequency_monitoring(advanced_networking_intelligence_t *system);
int ml_monitor_stop_high_frequency_monitoring(advanced_networking_intelligence_t *system);
int ml_monitor_update_monitoring_frequency(advanced_networking_intelligence_t *system, 
                                          const char *interface_id, uint32_t new_interval_ms);

// Predictive failover (vs reactive)
int ml_monitor_evaluate_predictive_failover(advanced_networking_intelligence_t *system,
                                           const char *interface_id,
                                           bool *should_failover_now,
                                           uint32_t *predicted_outage_in_ms,
                                           double *confidence);
int ml_monitor_trigger_predictive_failover(advanced_networking_intelligence_t *system,
                                          const char *from_interface,
                                          const char *to_interface,
                                          const char *reason);

// Connection stability and flapping prevention
int ml_monitor_update_connection_stability(advanced_networking_intelligence_t *system,
                                          const char *interface_id,
                                          const multi_interface_observation_t *observation);
int ml_monitor_get_preferred_failover_target(advanced_networking_intelligence_t *system,
                                            const char *current_interface,
                                            char *preferred_target,
                                            double *target_confidence);
int ml_monitor_detect_connection_flapping(advanced_networking_intelligence_t *system,
                                         const char *interface_id,
                                         bool *is_flapping,
                                         uint32_t *penalty_time_remaining);

// Background validation intelligence
int ml_monitor_start_background_validation(advanced_networking_intelligence_t *system);
int ml_monitor_update_background_validation(advanced_networking_intelligence_t *system,
                                           const char *interface_id,
                                           const multi_interface_observation_t *observation);
int ml_monitor_get_background_validation_results(advanced_networking_intelligence_t *system,
                                                const char *interface_id,
                                                double *accuracy_if_primary,
                                                double *performance_if_primary,
                                                uint32_t *predictions_validated);

// Streaming protection
int ml_monitor_enable_streaming_protection(advanced_networking_intelligence_t *system, bool enable);
int ml_monitor_evaluate_streaming_impact(advanced_networking_intelligence_t *system,
                                        const char *interface_id,
                                        uint32_t predicted_outage_duration_ms,
                                        bool *protect_streaming,
                                        const char **protection_reason);

// Advanced performance metrics
int ml_monitor_get_advanced_performance_metrics(advanced_networking_intelligence_t *system,
                                               uint32_t *outages_prevented,
                                               uint32_t *false_alarms,
                                               uint32_t *flapping_prevented,
                                               double *failover_accuracy);

#endif // ML_MONITOR_ADVANCED_NETWORKING_H