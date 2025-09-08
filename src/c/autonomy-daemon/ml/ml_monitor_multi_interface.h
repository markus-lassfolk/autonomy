#ifndef ML_MONITOR_MULTI_INTERFACE_H
#define ML_MONITOR_MULTI_INTERFACE_H

#include "ml_monitor.h"
#include "../network/network_controller.h"
#include "../network/network_failover.h"
#include <time.h>
#include <stdint.h>
#include <stdbool.h>

// Multi-Interface ML Monitoring System (Phase 7 Enhancement)

// Interface types for ML monitoring
typedef enum {
    INTERFACE_TYPE_STARLINK = 0,
    INTERFACE_TYPE_CELLULAR,
    INTERFACE_TYPE_WIFI,
    INTERFACE_TYPE_LAN,
    INTERFACE_TYPE_UNKNOWN,
    INTERFACE_TYPE_MAX
} interface_type_t;

// Enhanced network observation for all interface types
typedef struct __attribute__((packed)) {
    uint32_t timestamp;              // 4 bytes - Unix timestamp
    char interface_id[16];           // 16 bytes - Interface identifier
    uint8_t interface_type;          // 1 byte - Interface type
    
    // Network performance metrics (20 bytes)
    uint16_t latency_ms;             // 2 bytes - Latency
    uint16_t latency_jitter_ms;      // 2 bytes - Latency variation
    uint8_t packet_loss_pct;         // 1 byte - Packet loss percentage
    uint8_t packet_loss_burst_count; // 1 byte - Burst loss events
    uint16_t throughput_down_kbps;   // 2 bytes - Download throughput
    uint16_t throughput_up_kbps;     // 2 bytes - Upload throughput
    uint8_t connection_stability;    // 1 byte - Stability score (0-255)
    int8_t latency_trend;           // 1 byte - Trend (-128 to 127)
    int8_t throughput_trend;        // 1 byte - Throughput trend
    uint8_t performance_degradation; // 1 byte - Overall degradation score
    uint8_t quality_score;          // 1 byte - Overall quality (0-255)
    uint8_t reliability_score;      // 1 byte - Predicted reliability
    
    // Interface-specific metrics (12 bytes)
    union {
        // Starlink-specific
        struct {
            uint16_t snr_x100;       // SNR * 100
            uint8_t obstruction_pct; // Obstruction percentage
            int16_t azimuth_deg;     // Satellite azimuth
            int16_t elevation_deg;   // Satellite elevation
            uint8_t satellites_visible;
            uint8_t dish_temperature;
            uint8_t dish_status;
            uint8_t reserved[1];
        } starlink;
        
        // Cellular-specific
        struct {
            int8_t signal_strength_dbm; // Signal strength
            uint8_t signal_quality;     // Signal quality (0-255)
            uint8_t network_type;       // 2G/3G/4G/5G
            char carrier[4];            // Carrier code
            uint8_t band;               // Frequency band
            uint8_t cell_id[3];         // Cell tower ID
            uint8_t reserved[1];
        } cellular;
        
        // WiFi-specific  
        struct {
            int8_t rssi_dbm;            // WiFi signal strength
            uint8_t channel;            // WiFi channel
            uint8_t channel_utilization; // Channel busy percentage
            char ssid_hash[4];          // SSID hash for identification
            uint8_t security_type;      // Security type
            uint8_t interference_level; // Interference detection
            uint8_t ap_load;            // Access point load
            uint8_t reserved[1];
        } wifi;
        
        // LAN-specific
        struct {
            uint8_t link_speed_mbps;    // Link speed
            uint8_t duplex;             // Full/half duplex
            uint8_t cable_quality;      // Cable quality assessment
            uint8_t switch_load;        // Switch load if detectable
            uint8_t reserved[8];
        } lan;
    } interface_specific;
    
    // Location context (8 bytes)
    int32_t latitude_e7;             // Latitude * 10^7
    int32_t longitude_e7;            // Longitude * 10^7
    
    // ML predictions (8 bytes)
    uint8_t outage_probability_5min;  // 5-minute outage probability
    uint8_t outage_probability_15min; // 15-minute outage probability
    uint8_t outage_duration_class;    // Predicted duration class
    uint8_t failback_readiness;       // Ready for failback (0-255)
    uint8_t reliability_prediction;   // Predicted reliability
    uint8_t performance_prediction;   // Predicted performance
    uint8_t confidence;               // Prediction confidence
    uint8_t flags;                    // Status flags
    
} multi_interface_observation_t; // Total: ~64 bytes

// Per-interface ML model
typedef struct {
    char interface_id[32];
    interface_type_t type;
    bool active;
    
    // Interface-specific ML models
    struct {
        // Base model (inherited from interface type)
        ml_observation_t base_observations[1000];
        uint16_t base_observation_count;
        
        // Personalized model (device-specific learning)
        ml_observation_t personal_observations[5000];
        uint16_t personal_observation_count;
        
        // Model fusion weights
        double base_model_weight;      // Weight for base type model
        double personal_model_weight;  // Weight for device-specific model
        double personalization_confidence; // How much we trust device-specific data
    } models;
    
    // Performance tracking
    struct {
        uint32_t total_predictions;
        uint32_t correct_predictions;
        double accuracy;
        time_t last_prediction;
        
        // Performance characteristics
        double typical_latency_ms;
        double typical_throughput_mbps;
        double typical_reliability;
        double performance_stability;
    } performance;
    
    // Failover/failback learning
    struct {
        uint32_t failover_events;
        uint32_t successful_failovers;
        uint32_t failback_events;
        uint32_t successful_failbacks;
        double average_outage_duration_minutes;
        
        // Timing optimization
        uint32_t optimal_failover_delay_seconds;
        uint32_t optimal_failback_delay_seconds;
        double failover_cost_benefit_ratio;
    } failover_learning;
    
} interface_ml_model_t;

// Multi-interface ML monitoring system
typedef struct {
    // Interface models
    interface_ml_model_t interface_models[MAX_INTERFACES];
    uint8_t interface_count;
    
    // Base type models (shared learning)
    struct {
        ml_observation_t starlink_base_patterns[2000];
        ml_observation_t cellular_base_patterns[2000];
        ml_observation_t wifi_base_patterns[2000];
        ml_observation_t lan_base_patterns[1000];
        uint16_t pattern_counts[INTERFACE_TYPE_MAX];
    } base_models;
    
    // Cross-interface learning
    struct {
        bool enable_cross_interface_correlation;
        double interface_correlation_matrix[INTERFACE_TYPE_MAX][INTERFACE_TYPE_MAX];
        bool enable_ensemble_across_interfaces;
        double cross_interface_weights[INTERFACE_TYPE_MAX];
    } cross_learning;
    
    // Failover intelligence
    struct {
        bool continuous_monitoring_during_failover;
        bool enable_predictive_failback;
        bool enable_outage_duration_prediction;
        double failover_confidence_threshold;
        uint32_t min_failback_delay_seconds;
        uint32_t max_failback_delay_seconds;
    } failover_intelligence;
    
    // MWAN3 integration
    struct {
        bool enable_dynamic_weight_updates;
        bool auto_apply_weight_changes;
        double weight_adjustment_sensitivity;
        uint32_t weight_update_interval_seconds;
        char mwan3_config_path[256];
        
        // Current MWAN3 state
        struct {
            char interface_name[32];
            int base_weight;
            int ml_weight_adjustment;
            int current_weight;
            double ml_reliability_score;
            time_t last_weight_update;
        } mwan3_interfaces[MAX_INTERFACES];
        uint8_t mwan3_interface_count;
    } mwan3_integration;
    
} multi_interface_ml_system_t;

// Outage duration prediction
typedef struct {
    // Duration classification
    uint8_t very_short_probability;  // <2 minutes
    uint8_t short_probability;       // 2-10 minutes
    uint8_t medium_probability;      // 10-60 minutes
    uint8_t long_probability;        // >60 minutes
    
    // Expected duration
    uint32_t expected_duration_seconds;
    uint32_t confidence_interval_low;
    uint32_t confidence_interval_high;
    uint8_t prediction_confidence;
    
    // Cost-benefit analysis
    double estimated_failover_cost;
    double estimated_outage_cost;
    bool recommend_failover;
    char reasoning[128];
    
} outage_duration_prediction_t;

// Failback readiness assessment
typedef struct {
    // Readiness indicators
    uint8_t interface_health_score;     // Current interface health
    uint8_t performance_recovery_score; // How well performance has recovered
    uint8_t stability_score;            // Connection stability
    uint8_t historical_reliability;     // Historical reliability at this time/location
    
    // Timing optimization
    uint32_t recommended_failback_delay; // Seconds to wait before failback
    uint8_t failback_success_probability;
    uint8_t failback_confidence;
    
    // Risk assessment
    double risk_of_immediate_failback;
    double risk_of_delayed_failback;
    uint32_t optimal_failback_window_start;
    uint32_t optimal_failback_window_end;
    
} failback_readiness_t;

// API Functions

// Multi-interface system initialization
multi_interface_ml_system_t* ml_monitor_init_multi_interface_system(const ml_monitor_config_t *config);
void ml_monitor_cleanup_multi_interface_system(multi_interface_ml_system_t *system);

// Interface management
int ml_monitor_add_interface(multi_interface_ml_system_t *system, const char *interface_id, interface_type_t type);
int ml_monitor_remove_interface(multi_interface_ml_system_t *system, const char *interface_id);
int ml_monitor_update_interface_observation(multi_interface_ml_system_t *system, 
                                           const char *interface_id,
                                           const multi_interface_observation_t *observation);

// Continuous monitoring during failover
int ml_monitor_enable_continuous_monitoring(multi_interface_ml_system_t *system, bool enable);
int ml_monitor_validate_failover_prediction(multi_interface_ml_system_t *system,
                                           const char *interface_id,
                                           bool actual_outage_occurred,
                                           uint32_t actual_duration_seconds);

// Enhanced prediction functions
int ml_monitor_predict_interface_performance(multi_interface_ml_system_t *system,
                                           const char *interface_id,
                                           uint8_t *outage_probability,
                                           uint8_t *performance_score,
                                           uint8_t *confidence);

int ml_monitor_predict_outage_duration(multi_interface_ml_system_t *system,
                                      const char *interface_id,
                                      outage_duration_prediction_t *duration_prediction);

int ml_monitor_assess_failback_readiness(multi_interface_ml_system_t *system,
                                        const char *interface_id,
                                        failback_readiness_t *readiness);

// MWAN3 integration
int ml_monitor_update_mwan3_weights(multi_interface_ml_system_t *system);
int ml_monitor_get_mwan3_weight_recommendation(multi_interface_ml_system_t *system,
                                              const char *interface_id,
                                              int *recommended_weight,
                                              double *confidence);
int ml_monitor_apply_mwan3_weight_changes(multi_interface_ml_system_t *system);

// Cross-interface learning
int ml_monitor_update_cross_interface_correlations(multi_interface_ml_system_t *system);
int ml_monitor_transfer_learning_across_interfaces(multi_interface_ml_system_t *system,
                                                   interface_type_t source_type,
                                                   interface_type_t target_type);

// Error codes
#define ML_MONITOR_MULTI_SUCCESS              0
#define ML_MONITOR_MULTI_ERROR_INVALID_PARAM -1
#define ML_MONITOR_MULTI_ERROR_NOT_FOUND     -2
#define ML_MONITOR_MULTI_ERROR_MWAN3_FAILED  -3
#define ML_MONITOR_MULTI_ERROR_NO_DATA       -4

#endif // ML_MONITOR_MULTI_INTERFACE_H