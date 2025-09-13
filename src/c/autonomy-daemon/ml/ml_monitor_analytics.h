#ifndef ML_MONITOR_ANALYTICS_H
#define ML_MONITOR_ANALYTICS_H

#include "ml_monitor.h"
#include "ml_monitor_multi_interface.h"
#include <time.h>
#include <stdint.h>

// ML Analytics and Visualization System

// Prediction result tracking
typedef struct {
    time_t timestamp;
    char interface_id[32];              // Bounds checked: max 31 chars + null terminator, validated in all functions
    interface_type_t interface_type;
    uint8_t predicted_outage_probability;
    uint8_t predicted_performance_score;
    bool actual_outage_occurred;
    uint8_t actual_performance_score;
    bool prediction_correct;
    uint16_t prediction_latency_ms;
    uint8_t confidence_level;
    char prediction_trigger[64];        // Bounds checked: max 63 chars + null terminator, validated in all functions
} ml_prediction_result_t;

// Interface score tracking
typedef struct {
    time_t timestamp;
    char interface_id[32];              // Bounds checked: max 31 chars + null terminator, validated in all functions
    interface_type_t interface_type;
    double overall_score;               // 0-100 overall ML reliability score
    double accuracy_score;              // Prediction accuracy (0-100)
    double stability_score;             // Connection stability (0-100)
    double performance_score;           // Performance metrics (0-100)
    double trend_score;                 // Performance trend (0-100)
    
    // Score contributors (what affected the score)
    struct {
        double latency_impact;          // How latency affected score (-50 to +50)
        double loss_impact;             // How packet loss affected score (-50 to +50)
        double signal_impact;           // How signal strength affected score (-50 to +50)
        double prediction_impact;       // How prediction accuracy affected score (-50 to +50)
        double stability_impact;        // How stability affected score (-50 to +50)
        double trend_impact;            // How trends affected score (-50 to +50)
    } score_contributors;
    
    // Raw metrics for context
    uint16_t current_latency_ms;
    uint8_t current_loss_pct;
    int16_t current_signal_dbm;
    uint8_t recent_predictions_correct;  // Out of last 10
    uint32_t consecutive_stable_minutes;
} ml_interface_score_t;

// ML system impact tracking
typedef struct {
    time_t timestamp;
    char interface_id[32];              // Bounds checked: max 31 chars + null terminator, validated in all functions
    
    // ML-driven actions and their results
    bool ml_triggered_failover;
    bool ml_prevented_unnecessary_failover;
    bool ml_optimized_weights;
    uint32_t failover_time_ms;          // How long failover took
    uint32_t connection_downtime_ms;    // How long connection was down
    
    // Comparison with non-ML behavior
    uint32_t estimated_non_ml_downtime_ms;  // Estimated downtime without ML
    int32_t ml_improvement_ms;          // Positive = ML improved things
    double stability_improvement_pct;   // How much more stable with ML
    
    // Impact metrics
    bool streaming_protected;           // Did ML protect streaming?
    uint32_t data_saved_bytes;         // Data saved by smart monitoring
    double user_experience_score;      // 0-100 user experience score
} ml_impact_event_t;

// Analytics data storage (circular buffers)
typedef struct {
    // Prediction results (last 100) - OPTIMIZED SIZE
    ml_prediction_result_t prediction_results[100];  // Reduced from 1000 to 100 for memory efficiency
    uint32_t prediction_results_count;
    uint32_t prediction_results_index;
    
    // Interface scores (last 6 hours, 1 per minute) - OPTIMIZED SIZE
    ml_interface_score_t interface_scores[MAX_INTERFACES][360];  // Reduced from 720 to 360 (6 hours)
    uint32_t interface_scores_count[MAX_INTERFACES];
    uint32_t interface_scores_index[MAX_INTERFACES];
    
    // Impact events (last 100) - OPTIMIZED SIZE
    ml_impact_event_t impact_events[100];  // Reduced from 250 to 100 for memory efficiency
    uint32_t impact_events_count;
    uint32_t impact_events_index;
    
    // Summary statistics
    struct {
        uint32_t total_predictions;
        uint32_t correct_predictions;
        double overall_accuracy_pct;
        uint32_t ml_triggered_actions;
        uint32_t successful_optimizations;
        int32_t total_improvement_ms;
        double average_user_experience;
        time_t stats_start_time;
    } summary_stats;
    
    // Per-interface summary
    struct {
        char interface_id[32];              // Bounds checked: max 31 chars + null terminator, validated in all functions
        uint32_t predictions_made;
        uint32_t predictions_correct;
        double accuracy_pct;
        double current_score;
        double best_score;
        double worst_score;
        time_t last_update;
        bool is_active;
    } interface_summary[MAX_INTERFACES];
    
} ml_analytics_data_t;

// Function prototypes

/**
 * Initialize ML analytics system
 * @return ML_MONITOR_SUCCESS on success
 */
int ml_monitor_analytics_init(void);

/**
 * Cleanup ML analytics system
 */
void ml_monitor_analytics_cleanup(void);

/**
 * Record a prediction result
 * @param result Prediction result to record
 * @return ML_MONITOR_SUCCESS on success
 */
int ml_monitor_analytics_record_prediction(const ml_prediction_result_t *result);

/**
 * Update interface score
 * @param score Interface score update
 * @return ML_MONITOR_SUCCESS on success
 */
int ml_monitor_analytics_update_interface_score(const ml_interface_score_t *score);

/**
 * Record ML impact event
 * @param event Impact event to record
 * @return ML_MONITOR_SUCCESS on success
 */
int ml_monitor_analytics_record_impact_event(const ml_impact_event_t *event);

/**
 * Calculate interface score based on current metrics
 * @param interface_id Interface to calculate score for
 * @param score Output score structure
 * @return ML_MONITOR_SUCCESS on success
 */
int ml_monitor_analytics_calculate_interface_score(const char *interface_id, ml_interface_score_t *score);

/**
 * Get analytics data for visualization
 * @param data Output analytics data
 * @return ML_MONITOR_SUCCESS on success
 */
int ml_monitor_analytics_get_data(ml_analytics_data_t *data);

/**
 * Get interface score history for graphing
 * @param interface_id Interface to get history for
 * @param scores Output score array
 * @param max_scores Maximum scores to return
 * @param actual_count Actual number of scores returned
 * @return ML_MONITOR_SUCCESS on success
 */
int ml_monitor_analytics_get_interface_score_history(const char *interface_id,
                                                   ml_interface_score_t *scores,
                                                   uint32_t max_scores,
                                                   uint32_t *actual_count);

/**
 * Get prediction accuracy trends
 * @param interface_id Interface to analyze (NULL for all)
 * @param hours Number of hours to analyze
 * @param accuracy_pct Output accuracy percentage
 * @param trend_direction Output trend direction (-1=declining, 0=stable, 1=improving)
 * @return ML_MONITOR_SUCCESS on success
 */
int ml_monitor_analytics_get_accuracy_trend(const char *interface_id,
                                          uint32_t hours,
                                          double *accuracy_pct,
                                          int *trend_direction);

/**
 * Get ML impact summary
 * @param hours Number of hours to analyze
 * @param total_improvement_ms Total improvement in milliseconds
 * @param stability_improvement_pct Stability improvement percentage
 * @param actions_taken Number of ML actions taken
 * @return ML_MONITOR_SUCCESS on success
 */
int ml_monitor_analytics_get_impact_summary(uint32_t hours,
                                          int32_t *total_improvement_ms,
                                          double *stability_improvement_pct,
                                          uint32_t *actions_taken);

/**
 * Export analytics data for external analysis
 * @param format Export format ("json", "csv", "binary")
 * @param output_path Output file path
 * @return ML_MONITOR_SUCCESS on success
 */
int ml_monitor_analytics_export_data(const char *format, const char *output_path);

/**
 * Cleanup analytics system
 */
void ml_monitor_analytics_cleanup(void);

// Utility functions for score calculation
double ml_monitor_calculate_latency_score(uint16_t latency_ms);
double ml_monitor_calculate_loss_score(uint8_t loss_pct);
double ml_monitor_calculate_signal_score(int16_t signal_dbm, interface_type_t type);
double ml_monitor_calculate_prediction_score(uint32_t correct, uint32_t total);
double ml_monitor_calculate_stability_score(uint32_t stable_minutes, uint32_t total_minutes);

#endif // ML_MONITOR_ANALYTICS_H