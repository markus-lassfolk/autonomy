#ifndef DECISION_ENGINE_H
#define DECISION_ENGINE_H

#include <stdbool.h>
#include <time.h>

// Decision factors
typedef struct {
    double latency_weight;
    double loss_weight;
    double signal_weight;
    double throughput_weight;
    double cost_weight;
    double reliability_weight;
    double historical_performance_weight;
} decision_weights_t;

// Connection score
typedef struct {
    char interface_name[32];
    double overall_score;
    double latency_score;
    double loss_score;
    double signal_score;
    double throughput_score;
    double cost_score;
    double reliability_score;
    double historical_score;
    time_t last_update;
    bool is_available;
} connection_score_t;

// Decision result
typedef struct {
    char selected_interface[32];
    char reason[256];
    double confidence;
    bool requires_failover;
    time_t decision_timestamp;
    connection_score_t scores[16];
    int score_count;
} decision_result_t;

// Decision engine configuration
typedef struct {
    bool enabled;
    int decision_interval_seconds;
    double failover_threshold;
    double recovery_threshold;
    int cooldown_period_seconds;
    bool enable_predictive_failover;
    decision_weights_t weights;
} decision_engine_config_t;

// Decision engine structure
typedef struct {
    decision_engine_config_t config;
    
    // Current decision
    decision_result_t last_decision;
    
    // Historical decisions
    decision_result_t decision_history[100];
    int history_count;
    int history_index;
    
    // Statistics
    time_t last_decision_time;
    int decision_count;
    int failover_count;
    int recovery_count;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} decision_engine_t;

// Initialize decision engine
int decision_engine_init(const decision_engine_config_t* config);

// Clean up decision engine
void decision_engine_cleanup(void);

// Make decision
int decision_engine_make_decision(decision_result_t* result);

// Evaluate connection scores
int decision_engine_evaluate_connections(connection_score_t* scores, int max_scores);

// Calculate connection score
double decision_engine_calculate_score(const connection_score_t* score);

// Check if failover is needed
bool decision_engine_needs_failover(void);

// Check if recovery is possible
bool decision_engine_can_recover(void);

// Get decision history
int decision_engine_get_history(decision_result_t* history, int max_history);

// Get decision engine status
void decision_engine_get_status(decision_engine_t* status);

// Check if decision engine is initialized
bool decision_engine_is_initialized(void);

// Get decision engine instance
decision_engine_t* decision_engine_get_instance(void);

#endif // DECISION_ENGINE_H
