#ifndef ML_ENHANCED_INTEGRATION_H
#define ML_ENHANCED_INTEGRATION_H

#include "ml_monitor.h"
#include "ml_satellite_redundancy.h"
#include "ml_cellular_tower_intelligence.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Enhanced ML integration system that combines:
// - Satellite redundancy analysis with early warning
// - Cellular tower intelligence with carrier filtering
// - Enhanced ML predictions with multi-source data

// Enhanced integration statistics
typedef struct {
    uint32_t satellite_assessments;        // Number of satellite redundancy assessments
    uint32_t cellular_analyses;            // Number of cellular tower analyses
    uint32_t ml_predictions_enhanced;      // Number of enhanced ML predictions
    uint32_t early_warnings_triggered;     // Number of early warnings triggered
    uint32_t carrier_filtered_towers;      // Number of carrier-filtered towers
    uint32_t integration_cycles;           // Total integration cycles
    time_t last_update;                    // Last update timestamp
} ml_enhanced_integration_stats_t;

// API Functions

// Initialization and cleanup
int ml_enhanced_integration_init(ml_monitor_t *ml_monitor);
void ml_enhanced_integration_cleanup(void);

// Enhanced observation collection
int ml_enhanced_integration_collect_observation(ml_observation_t *observation);

// Enhanced prediction with satellite and cellular intelligence
int ml_enhanced_integration_predict_outage(uint8_t *probability, uint8_t *confidence, uint8_t *cause);

// Statistics and monitoring
int ml_enhanced_integration_get_stats(ml_enhanced_integration_stats_t *stats);
bool ml_enhanced_integration_is_enabled(void);
int ml_enhanced_integration_set_enabled(bool enabled);

// Callback functions (internal use)
void ml_enhanced_integration_satellite_warning_callback(const satellite_redundancy_assessment_t *assessment, void *user_data);
void ml_enhanced_integration_cellular_risk_callback(double risk_score, void *user_data);

#endif // ML_ENHANCED_INTEGRATION_H
