#include "decision_engine.h"
#include "../network_controller.h"
#include "../network_collector.h"
#include "../collectors/cellular_collector.h"
#include "../telemetry_comprehensive.h"
#include "../gps_comprehensive.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <math.h>

// Global decision engine instance
static decision_engine_t g_decision_engine;
static bool g_decision_engine_initialized = false;

// Forward declarations
static double calculate_connection_score(const connection_score_t* score);
static void update_decision_history(decision_result_t* decision);
static bool should_failover(const connection_score_t* scores, int score_count);
static bool can_recover(const connection_score_t* scores, int score_count);

// Initialize decision engine
int decision_engine_init(const decision_engine_config_t* config) {
    if (g_decision_engine_initialized) {
        return 0; // Already initialized
    }
    
    memset(&g_decision_engine, 0, sizeof(decision_engine_t));
    
    // Set configuration
    if (config) {
        g_decision_engine.config = *config;
    } else {
        // Default configuration
        g_decision_engine.config.enabled = true;
        g_decision_engine.config.decision_interval_seconds = 30;
        g_decision_engine.config.failover_threshold = 0.3;
        g_decision_engine.config.recovery_threshold = 0.7;
        g_decision_engine.config.cooldown_period_seconds = 300;
        g_decision_engine.config.enable_predictive_failover = true;
        
        // Default weights
        g_decision_engine.config.weights.latency_weight = 0.25;
        g_decision_engine.config.weights.loss_weight = 0.25;
        g_decision_engine.config.weights.signal_weight = 0.20;
        g_decision_engine.config.weights.throughput_weight = 0.15;
        g_decision_engine.config.weights.cost_weight = 0.10;
        g_decision_engine.config.weights.reliability_weight = 0.05;
        g_decision_engine.config.weights.historical_performance_weight = 0.05;
    }
    
    // Initialize mutex
    g_decision_engine.mutex = malloc(sizeof(pthread_mutex_t));
    if (!g_decision_engine.mutex) {
        return -1;
    }
    
    pthread_mutex_init(g_decision_engine.mutex, NULL);
    
    // Initialize decision history
    g_decision_engine.history_count = 0;
    g_decision_engine.history_index = 0;
    
    g_decision_engine_initialized = true;
    return 0;
}

// Clean up decision engine
void decision_engine_cleanup(void) {
    if (!g_decision_engine_initialized) return;
    
    if (g_decision_engine.mutex) {
        pthread_mutex_destroy(g_decision_engine.mutex);
        free(g_decision_engine.mutex);
    }
    
    g_decision_engine.mutex = NULL;
    g_decision_engine_initialized = false;
}

// Make decision
int decision_engine_make_decision(decision_result_t* result) {
    if (!g_decision_engine_initialized || !result) {
        return -1;
    }
    
    pthread_mutex_lock(g_decision_engine.mutex);
    
    // Evaluate all connections
    connection_score_t scores[16];
    int score_count = decision_engine_evaluate_connections(scores, 16);
    
    if (score_count <= 0) {
        pthread_mutex_unlock(g_decision_engine.mutex);
        return -1;
    }
    
    // Find best connection
    int best_index = 0;
    double best_score = scores[0].overall_score;
    
    for (int i = 1; i < score_count; i++) {
        if (scores[i].overall_score > best_score) {
            best_score = scores[i].overall_score;
            best_index = i;
        }
    }
    
    // Check if failover is needed
    bool needs_failover = should_failover(scores, score_count);
    
    // Populate decision result
    memset(result, 0, sizeof(decision_result_t));
    strcpy(result->selected_interface, scores[best_index].interface_name);
    result->confidence = best_score;
    result->requires_failover = needs_failover;
    result->decision_timestamp = time(NULL);
    
    // Copy scores
    memcpy(result->scores, scores, sizeof(connection_score_t) * score_count);
    result->score_count = score_count;
    
    // Generate reason
    if (needs_failover) {
        snprintf(result->reason, sizeof(result->reason),
                "Failover required due to poor performance on current interface");
    } else {
        snprintf(result->reason, sizeof(result->reason),
                "Best interface selected based on performance scoring");
    }
    
    // Update decision history
    update_decision_history(result);
    
    // Log decision to comprehensive telemetry system
    if (telemetry_comprehensive_is_initialized() && (needs_failover || result->confidence > 0.8)) {
        decision_record_t telemetry_decision = {0};
        
        // Generate decision ID
        snprintf(telemetry_decision.decision_id, sizeof(telemetry_decision.decision_id),
                "decision_%ld_%d", time(NULL), g_decision_engine.decision_count);
        
        strcpy(telemetry_decision.decision_type, needs_failover ? "failover" : "evaluation");
        strcpy(telemetry_decision.trigger, "periodic_evaluation");
        strncpy(telemetry_decision.reasoning, result->reason, sizeof(telemetry_decision.reasoning) - 1);
        telemetry_decision.confidence = result->confidence;
        
        // Interface information
        strncpy(telemetry_decision.to_interface, result->selected_interface, 
                sizeof(telemetry_decision.to_interface) - 1);
        strncpy(telemetry_decision.to_member, result->selected_interface, 
                sizeof(telemetry_decision.to_member) - 1);
        
        // Get current GPS position if available
        standardized_gps_data_t gps_data;
        if (gps_comprehensive_is_initialized() && 
            gps_comprehensive_collect_best_gps(&gps_data) == AUTONOMY_SUCCESS && gps_data.valid) {
            telemetry_decision.gps_latitude = gps_data.latitude;
            telemetry_decision.gps_longitude = gps_data.longitude;
            telemetry_decision.gps_accuracy = gps_data.accuracy;
            strncpy(telemetry_decision.gps_source, gps_data.source, sizeof(telemetry_decision.gps_source) - 1);
        }
        
        // Performance context
        if (score_count > 0) {
            telemetry_decision.to_score = scores[best_index].overall_score;
            telemetry_decision.to_latency = scores[best_index].latency_ms;
            telemetry_decision.to_loss = scores[best_index].loss_percent;
        }
        
        telemetry_decision.success = true; // Assume success for evaluation
        telemetry_decision.execution_time_ms = 0.0; // No execution for evaluation
        telemetry_decision.predictive_decision = false; // This is reactive evaluation
        
        // Create context JSON
        snprintf(telemetry_decision.context_json, sizeof(telemetry_decision.context_json),
                "{\"score_count\": %d, \"best_score\": %.2f, \"needs_failover\": %s}",
                score_count, best_score, needs_failover ? "true" : "false");
        
        // Log to telemetry system
        if (telemetry_comprehensive_log_decision(&telemetry_decision) == AUTONOMY_SUCCESS) {
            LOGX_DEBUG("Decision logged to telemetry system", "decision_id", telemetry_decision.decision_id);
        }
    }
    
    // Update statistics
    g_decision_engine.last_decision_time = time(NULL);
    g_decision_engine.decision_count++;
    
    if (needs_failover) {
        g_decision_engine.failover_count++;
    }
    
    pthread_mutex_unlock(g_decision_engine.mutex);
    
    return 0;
}

// Evaluate connection scores
int decision_engine_evaluate_connections(connection_score_t* scores, int max_scores) {
    if (!g_decision_engine_initialized || !scores || max_scores <= 0) {
        return -1;
    }
    
    // Collect real network metrics from available interfaces
    int score_count = 0;
    
    // Get available network members from network controller
    network_member_t members[16];
    int member_count = 0;
    
    if (network_controller_is_initialized() && 
        network_controller_get_members(members, 16, &member_count) == AUTONOMY_SUCCESS) {
        
        for (int i = 0; i < member_count && score_count < max_scores; i++) {
            if (!members[i].eligible) continue;
            
            // Get real metrics for this interface
            network_metrics_t metrics;
            if (network_collector_get_interface_metrics(members[i].interface, &metrics) == AUTONOMY_SUCCESS) {
                strcpy(scores[score_count].interface_name, members[i].name);
                
                // Convert metrics to scores (0.0-1.0)
                scores[score_count].latency_score = fmax(0.0, fmin(1.0, 1.0 - (metrics.ping_average_latency / 1000.0)));
                scores[score_count].loss_score = fmax(0.0, fmin(1.0, 1.0 - (metrics.ping_packet_loss / 100.0)));
                scores[score_count].signal_score = fmax(0.0, fmin(1.0, metrics.overall_health_score / 100.0));
                scores[score_count].throughput_score = 0.7; // TODO: Add throughput measurement
                
                // Class-specific scoring
                if (strcmp(members[i].class, "starlink") == 0) {
                    scores[score_count].cost_score = 0.6; // Higher cost but good performance
                    scores[score_count].reliability_score = 0.8;
                } else if (strcmp(members[i].class, "cellular") == 0) {
                    scores[score_count].cost_score = 0.4; // Expensive
                    scores[score_count].reliability_score = 0.7;
                    
                    // Use cellular collector data if available
                    cellular_info_t cellular_info;
                    if (cellular_collector_is_initialized() && 
                        cellular_collector_collect(&cellular_info) == AUTONOMY_SUCCESS) {
                        scores[score_count].signal_score = cellular_info.signal_quality / 100.0;
                        scores[score_count].reliability_score = cellular_info.stability_score / 100.0;
                    }
                } else if (strcmp(members[i].class, "wifi") == 0) {
                    scores[score_count].cost_score = 0.9; // Usually free
                    scores[score_count].reliability_score = 0.6; // Variable
                } else if (strcmp(members[i].class, "lan") == 0) {
                    scores[score_count].cost_score = 1.0; // Free
                    scores[score_count].reliability_score = 0.9; // Very reliable
                }
                
                scores[score_count].historical_score = 0.7; // TODO: Add historical tracking
                scores[score_count].last_update = time(NULL);
                scores[score_count].is_available = true;
                scores[score_count].overall_score = decision_engine_calculate_score(&scores[score_count]);
                score_count++;
            }
        }
    }
    
    // If no network controller data, fall back to basic interface enumeration
    if (score_count == 0) {
        LOGX_WARN("No network controller data available for decision engine");
        return 0; // Return 0 scores rather than simulated data
    }
    
    return score_count;
}

// Calculate connection score
double decision_engine_calculate_score(const connection_score_t* score) {
    if (!score) return 0.0;
    
    const decision_weights_t* weights = &g_decision_engine.config.weights;
    
    double total_score = 
        score->latency_score * weights->latency_weight +
        score->loss_score * weights->loss_weight +
        score->signal_score * weights->signal_weight +
        score->throughput_score * weights->throughput_weight +
        score->cost_score * weights->cost_weight +
        score->reliability_score * weights->reliability_weight +
        score->historical_score * weights->historical_performance_weight;
    
    return fmax(0.0, fmin(1.0, total_score));
}

// Check if failover is needed
bool decision_engine_needs_failover(void) {
    if (!g_decision_engine_initialized) return false;
    
    pthread_mutex_lock(g_decision_engine.mutex);
    
    connection_score_t scores[16];
    int score_count = decision_engine_evaluate_connections(scores, 16);
    bool needs_failover = should_failover(scores, score_count);
    
    pthread_mutex_unlock(g_decision_engine.mutex);
    
    return needs_failover;
}

// Check if recovery is possible
bool decision_engine_can_recover(void) {
    if (!g_decision_engine_initialized) return false;
    
    pthread_mutex_lock(g_decision_engine.mutex);
    
    connection_score_t scores[16];
    int score_count = decision_engine_evaluate_connections(scores, 16);
    bool can_recover = can_recover(scores, score_count);
    
    pthread_mutex_unlock(g_decision_engine.mutex);
    
    return can_recover;
}

// Get decision history
int decision_engine_get_history(decision_result_t* history, int max_history) {
    if (!g_decision_engine_initialized || !history || max_history <= 0) {
        return -1;
    }
    
    pthread_mutex_lock(g_decision_engine.mutex);
    
    int count = 0;
    int index = g_decision_engine.history_index;
    
    for (int i = 0; i < g_decision_engine.history_count && count < max_history; i++) {
        int history_index = (index - i + 100) % 100;
        if (g_decision_engine.decision_history[history_index].decision_timestamp > 0) {
            history[count] = g_decision_engine.decision_history[history_index];
            count++;
        }
    }
    
    pthread_mutex_unlock(g_decision_engine.mutex);
    
    return count;
}

// Update decision history
static void update_decision_history(decision_result_t* decision) {
    if (!decision) return;
    
    g_decision_engine.decision_history[g_decision_engine.history_index] = *decision;
    
    g_decision_engine.history_index = (g_decision_engine.history_index + 1) % 100;
    
    if (g_decision_engine.history_count < 100) {
        g_decision_engine.history_count++;
    }
}

// Check if failover is needed
static bool should_failover(const connection_score_t* scores, int score_count) {
    if (!scores || score_count <= 0) return false;
    
    // Check if current best score is below failover threshold
    double best_score = 0.0;
    for (int i = 0; i < score_count; i++) {
        if (scores[i].overall_score > best_score) {
            best_score = scores[i].overall_score;
        }
    }
    
    return best_score < g_decision_engine.config.failover_threshold;
}

// Check if recovery is possible
static bool can_recover(const connection_score_t* scores, int score_count) {
    if (!scores || score_count <= 0) return false;
    
    // Check if any score is above recovery threshold
    for (int i = 0; i < score_count; i++) {
        if (scores[i].overall_score > g_decision_engine.config.recovery_threshold) {
            return true;
        }
    }
    
    return false;
}

// Get decision engine status
void decision_engine_get_status(decision_engine_t* status) {
    if (!status || !g_decision_engine_initialized) return;
    
    pthread_mutex_lock(g_decision_engine.mutex);
    *status = g_decision_engine;
    pthread_mutex_unlock(g_decision_engine.mutex);
}

// Check if decision engine is initialized
bool decision_engine_is_initialized(void) {
    return g_decision_engine_initialized;
}

// Get decision engine instance
decision_engine_t* decision_engine_get_instance(void) {
    return g_decision_engine_initialized ? &g_decision_engine : NULL;
}
