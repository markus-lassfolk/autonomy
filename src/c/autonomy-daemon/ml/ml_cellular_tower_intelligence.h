#ifndef ML_CELLULAR_TOWER_INTELLIGENCE_H
#define ML_CELLULAR_TOWER_INTELLIGENCE_H

#include "ml_monitor.h"
#include "../gps/opencellid_complete.h"
#include "../network/cellular_collector.h"
#include <time.h>
#include <stdint.h>
#include <stdbool.h>

// Carrier compatibility and signal strength thresholds
typedef struct {
    // Signal strength thresholds for usable towers
    int min_rsrp_dbm;                     // Minimum RSRP for usable tower (e.g., -120 dBm)
    int min_rsrq_db;                      // Minimum RSRQ for usable tower (e.g., -20 dB)
    int min_sinr_db;                      // Minimum SINR for usable tower (e.g., -3 dB)
    
    // Carrier filtering
    bool enable_carrier_filtering;        // Enable carrier compatibility filtering
    char allowed_mccs[256];               // Comma-separated allowed MCCs (e.g., "310,311,312")
    char allowed_mncs[256];               // Comma-separated allowed MNCs (e.g., "260,410,480")
    char home_mcc[8];                     // Home country MCC
    char home_mnc[8];                     // Home carrier MNC
    
    // Roaming preferences
    bool allow_roaming;                   // Allow roaming towers
    bool prefer_home_carrier;             // Prefer home carrier towers
    double roaming_penalty;               // Penalty for roaming towers (0.0-1.0)
    
    // Tower density analysis
    bool enable_density_analysis;         // Enable tower density analysis
    double density_radius_km;             // Radius for density calculation (km)
    int min_towers_for_density;           // Minimum towers for density analysis
    
    // Cell change pattern analysis
    bool enable_cell_change_analysis;     // Enable cell change pattern analysis
    uint32_t cell_change_window_seconds;  // Window for cell change analysis
    int max_cell_changes_per_hour;        // Maximum cell changes per hour (threshold)
} cellular_tower_config_t;

// Usable tower information (filtered from all visible towers)
typedef struct {
    opencellid_cell_identifier_t cell_id; // Cell identifier
    int rsrp_dbm;                         // RSRP in dBm
    int rsrq_db;                          // RSRQ in dB
    int sinr_db;                          // SINR in dB
    uint16_t pci;                         // Physical Cell ID
    uint32_t earfcn;                      // Frequency
    
    // Usability assessment
    bool is_usable;                       // Whether tower is usable
    bool is_home_carrier;                 // Whether it's home carrier
    bool is_roaming;                      // Whether it's roaming
    double usability_score;               // 0.0-1.0 usability score
    double signal_quality_score;          // 0.0-1.0 signal quality score
    
    // Distance and location
    double distance_km;                   // Distance from current location
    double latitude;                      // Tower latitude
    double longitude;                     // Tower longitude
    double accuracy_m;                    // Location accuracy in meters
    
    time_t measurement_time;              // When measurement was taken
} usable_tower_info_t;

// Tower density analysis
typedef struct {
    int total_towers_in_radius;           // Total towers within radius
    int usable_towers_in_radius;          // Usable towers within radius
    int home_carrier_towers;              // Home carrier towers
    int roaming_towers;                   // Roaming towers
    
    double average_signal_strength;       // Average signal strength
    double best_signal_strength;          // Best signal strength
    double signal_strength_variance;      // Signal strength variance
    
    double density_score;                 // 0.0-1.0 density score
    double coverage_score;                // 0.0-1.0 coverage score
    double reliability_score;             // 0.0-1.0 reliability score
    
    time_t analysis_time;                 // When analysis was performed
} tower_density_analysis_t;

// Cell change pattern analysis
typedef struct {
    // Recent cell changes
    struct {
        char cell_id[32];                 // Cell ID
        time_t change_time;               // When change occurred
        double signal_strength;           // Signal strength at change
        bool was_forced;                  // Whether change was forced
    } recent_changes[20];                 // Last 20 cell changes
    uint8_t change_count;                 // Number of changes in buffer
    uint8_t change_index;                 // Circular buffer index
    
    // Pattern analysis
    uint32_t changes_last_hour;           // Changes in last hour
    uint32_t changes_last_day;            // Changes in last day
    double average_change_interval;       // Average time between changes
    double change_frequency_score;        // 0.0-1.0 change frequency score
    
    // Predictors
    bool high_mobility_detected;          // High mobility pattern detected
    bool poor_coverage_detected;          // Poor coverage pattern detected
    bool network_congestion_detected;     // Network congestion pattern detected
    double connectivity_risk_score;       // 0.0-1.0 connectivity risk score
    
    time_t last_analysis_time;            // Last analysis timestamp
} cell_change_pattern_analysis_t;

// Cellular tower intelligence system
typedef struct {
    cellular_tower_config_t config;
    
    // Current state
    usable_tower_info_t usable_towers[20]; // Up to 20 usable towers
    uint8_t usable_tower_count;           // Number of usable towers
    tower_density_analysis_t density_analysis;
    cell_change_pattern_analysis_t cell_change_analysis;
    
    // Historical data
    struct {
        tower_density_analysis_t density_history[24]; // Last 24 hours
        uint8_t density_history_index;    // Circular buffer index
        uint8_t density_history_count;    // Number of valid entries
    } history;
    
    // ML features
    uint8_t ml_tower_density_feature;     // 0-255 ML feature
    uint8_t ml_cell_change_feature;       // 0-255 ML feature
    uint8_t ml_coverage_quality_feature;  // 0-255 ML feature
    uint8_t ml_connectivity_risk_feature; // 0-255 ML feature
    
    // State tracking
    time_t last_update_time;              // Last update timestamp
    bool analysis_active;                 // Whether analysis is active
    char last_serving_cell[32];           // Last serving cell ID
    
    // Callbacks
    void (*tower_density_callback)(const tower_density_analysis_t *analysis, void *user_data);
    void (*cell_change_callback)(const cell_change_pattern_analysis_t *analysis, void *user_data);
    void (*connectivity_risk_callback)(double risk_score, void *user_data);
    void *callback_user_data;
} cellular_tower_intelligence_t;

// API Functions

// Initialization and cleanup
cellular_tower_intelligence_t* cellular_tower_intelligence_init(const cellular_tower_config_t *config);
void cellular_tower_intelligence_cleanup(cellular_tower_intelligence_t *intelligence);

// Configuration
void cellular_tower_config_init_defaults(cellular_tower_config_t *config);
int cellular_tower_intelligence_update_config(cellular_tower_intelligence_t *intelligence, const cellular_tower_config_t *config);

// Tower analysis and filtering
int cellular_tower_intelligence_analyze_towers(cellular_tower_intelligence_t *intelligence,
                                              const opencellid_cellular_environment_t *environment,
                                              const cellular_info_t *cellular_info);
int cellular_tower_intelligence_filter_usable_towers(cellular_tower_intelligence_t *intelligence,
                                                    const opencellid_neighbor_cell_t *neighbors,
                                                    int neighbor_count,
                                                    usable_tower_info_t *usable_towers,
                                                    uint8_t *usable_count);

// Tower density analysis
int cellular_tower_intelligence_analyze_density(cellular_tower_intelligence_t *intelligence,
                                               const usable_tower_info_t *usable_towers,
                                               uint8_t tower_count,
                                               double current_lat,
                                               double current_lon);
int cellular_tower_intelligence_get_density_analysis(const cellular_tower_intelligence_t *intelligence,
                                                    tower_density_analysis_t *analysis);

// Cell change pattern analysis
int cellular_tower_intelligence_analyze_cell_changes(cellular_tower_intelligence_t *intelligence,
                                                    const char *current_cell_id,
                                                    const cellular_info_t *cellular_info);
int cellular_tower_intelligence_get_cell_change_analysis(const cellular_tower_intelligence_t *intelligence,
                                                        cell_change_pattern_analysis_t *analysis);

// ML integration
int cellular_tower_intelligence_get_ml_features(const cellular_tower_intelligence_t *intelligence,
                                               uint8_t *tower_density_feature,
                                               uint8_t *cell_change_feature,
                                               uint8_t *coverage_quality_feature,
                                               uint8_t *connectivity_risk_feature);
int cellular_tower_intelligence_update_ml_observation(ml_observation_t *observation,
                                                     const cellular_tower_intelligence_t *intelligence);

// Utility functions
bool cellular_tower_is_carrier_compatible(const opencellid_cell_identifier_t *cell_id,
                                         const cellular_tower_config_t *config);
bool cellular_tower_meets_signal_thresholds(int rsrp, int rsrq, int sinr,
                                           const cellular_tower_config_t *config);
double cellular_tower_calculate_usability_score(const usable_tower_info_t *tower,
                                               const cellular_tower_config_t *config);
double cellular_tower_calculate_density_score(int usable_towers, int total_towers,
                                             double average_signal_strength);

// Callback management
int cellular_tower_intelligence_set_density_callback(cellular_tower_intelligence_t *intelligence,
                                                    void (*callback)(const tower_density_analysis_t *analysis, void *user_data),
                                                    void *user_data);
int cellular_tower_intelligence_set_cell_change_callback(cellular_tower_intelligence_t *intelligence,
                                                        void (*callback)(const cell_change_pattern_analysis_t *analysis, void *user_data),
                                                        void *user_data);
int cellular_tower_intelligence_set_connectivity_risk_callback(cellular_tower_intelligence_t *intelligence,
                                                              void (*callback)(double risk_score, void *user_data),
                                                              void *user_data);

// Error codes
#define CELLULAR_TOWER_INTELLIGENCE_SUCCESS                 0
#define CELLULAR_TOWER_INTELLIGENCE_ERROR_INVALID_PARAM    -1
#define CELLULAR_TOWER_INTELLIGENCE_ERROR_NOT_INITIALIZED  -2
#define CELLULAR_TOWER_INTELLIGENCE_ERROR_NO_DATA          -3
#define CELLULAR_TOWER_INTELLIGENCE_ERROR_ANALYSIS_FAILED  -4

#endif // ML_CELLULAR_TOWER_INTELLIGENCE_H
