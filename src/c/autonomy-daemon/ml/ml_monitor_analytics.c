#include "ml_monitor_analytics.h"
#include "ml_monitor_network_discovery_integration.h"
#include "../shared/utils/string_utils.h"
#include "../shared/logging/logx.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <stddef.h>  // For offsetof

// Global analytics data - dynamically allocated due to large size (2.55 MB)
static ml_analytics_data_t *g_analytics_data = NULL;
static pthread_mutex_t g_analytics_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_analytics_initialized = false;

// Initialize ML analytics system
int ml_monitor_analytics_init(void) {
    fprintf(stderr, "DEBUG: ml_monitor_analytics_init called\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "DEBUG: ml_monitor_analytics_init - checking if already initialized\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "DEBUG: g_analytics_initialized = %d\n", g_analytics_initialized\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (g_analytics_initialized && g_analytics_data) {
        fprintf(stderr, "DEBUG: ml_monitor_analytics_init - already initialized\n"\n"\n"\n"\n"\n"\n"\n"\n");
        return ML_MONITOR_SUCCESS;
    }
    
    fprintf(stderr, "DEBUG: ml_monitor_analytics_init - about to lock mutex\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "DEBUG: g_analytics_mutex address: %p\n", (void*)&g_analytics_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    pthread_mutex_lock(&g_analytics_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "DEBUG: ml_monitor_analytics_init - mutex locked\n"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Allocate analytics data dynamically - optimized for memory efficiency
    fprintf(stderr, "DEBUG: ml_monitor_analytics_init - about to allocate analytics data\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "DEBUG: sizeof(ml_analytics_data_t): %zu bytes (%.2f MB)\n", 
            sizeof(ml_analytics_data_t), sizeof(ml_analytics_data_t) / (1024.0 * 1024.0)\n"\n"\n"\n"\n"\n"\n"\n");
    fflush(stderr\n"\n"\n"\n"\n"\n"\n"\n");
    
    g_analytics_data = calloc(1, sizeof(ml_analytics_data_t)\n"\n"\n"\n"\n"\n"\n"\n");
    if (!g_analytics_data) {
        fprintf(stderr, "ERROR: Failed to allocate %zu bytes for analytics data\n", sizeof(ml_analytics_data_t)\n"\n"\n"\n"\n"\n"\n"\n");
        pthread_mutex_unlock(&g_analytics_mutex\n"\n"\n"\n"\n"\n"\n"\n");
        return ML_MONITOR_ERROR_MEMORY_FAILED;
    }
    fprintf(stderr, "DEBUG: ml_monitor_analytics_init - allocated analytics data at %p\n", (void*)g_analytics_data\n"\n"\n"\n"\n"\n"\n"\n");
    fflush(stderr\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Initialize fields individually
    fprintf(stderr, "DEBUG: ml_monitor_analytics_init - initializing prediction results (max 100)\n"\n"\n"\n"\n"\n"\n"\n"\n");
    g_analytics_data->prediction_results_count = 0;
    g_analytics_data->prediction_results_index = 0;
    fprintf(stderr, "DEBUG: ml_monitor_analytics_init - prediction results initialized\n"\n"\n"\n"\n"\n"\n"\n"\n");
    
    fprintf(stderr, "DEBUG: ml_monitor_analytics_init - initializing interface scores (max 360 per interface)\n"\n"\n"\n"\n"\n"\n"\n"\n");
    for (int i = 0; i < MAX_INTERFACES; i++) {
        g_analytics_data->interface_scores_count[i] = 0;
        g_analytics_data->interface_scores_index[i] = 0;
    }
    fprintf(stderr, "DEBUG: ml_monitor_analytics_init - interface scores initialized\n"\n"\n"\n"\n"\n"\n"\n"\n");
    
    fprintf(stderr, "DEBUG: ml_monitor_analytics_init - initializing impact events (max 100)\n"\n"\n"\n"\n"\n"\n"\n"\n");
    g_analytics_data->impact_events_count = 0;
    g_analytics_data->impact_events_index = 0;
    fprintf(stderr, "DEBUG: ml_monitor_analytics_init - impact events initialized\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "DEBUG: ml_monitor_analytics_init - about to set start time\n"\n"\n"\n"\n"\n"\n"\n"\n");
    g_analytics_data->summary_stats.stats_start_time = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "DEBUG: ml_monitor_analytics_init - start time set\n"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Initialize interface summaries
    fprintf(stderr, "DEBUG: ml_monitor_analytics_init - about to initialize interface summaries\n"\n"\n"\n"\n"\n"\n"\n"\n");
    for (int i = 0; i < MAX_INTERFACES; i++) {
        g_analytics_data->interface_summary[i].is_active = false;
        g_analytics_data->interface_summary[i].current_score = 50.0; // Start neutral
        g_analytics_data->interface_summary[i].best_score = 0.0;
        g_analytics_data->interface_summary[i].worst_score = 100.0;
    }
    fprintf(stderr, "DEBUG: ml_monitor_analytics_init - interface summaries initialized\n"\n"\n"\n"\n"\n"\n"\n"\n");
    
    g_analytics_initialized = true;
    fprintf(stderr, "DEBUG: ml_monitor_analytics_init - about to unlock mutex\n"\n"\n"\n"\n"\n"\n"\n"\n");
    pthread_mutex_unlock(&g_analytics_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "DEBUG: ml_monitor_analytics_init - mutex unlocked\n"\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: " ML Analytics system initialized"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "DEBUG: ml_monitor_analytics_init completed successfully\n"\n"\n"\n"\n"\n"\n"\n"\n");
    return ML_MONITOR_SUCCESS;
}

// Cleanup ML analytics system
void ml_monitor_analytics_cleanup(void) {
    if (g_analytics_data) {
        free(g_analytics_data\n"\n"\n"\n"\n"\n"\n"\n");
        g_analytics_data = NULL;
    }
    g_analytics_initialized = false;
}

// Record a prediction result
int ml_monitor_analytics_record_prediction(const ml_prediction_result_t *result) {
    if (!g_analytics_initialized || !g_analytics_data || !result) {
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_analytics_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Add to circular buffer
    uint32_t idx = g_analytics_data->prediction_results_index;
    g_analytics_data->prediction_results[idx] = *result;
    
    g_analytics_data->prediction_results_index = (idx + 1) % 100;  // Updated from 1000 to 100 for memory efficiency
    if (g_analytics_data->prediction_results_count < 100) {  // Updated from 1000 to 100 for memory efficiency
        g_analytics_data->prediction_results_count++;
    }
    
    // Update summary statistics
    g_analytics_data->summary_stats.total_predictions++;
    if (result->prediction_correct) {
        g_analytics_data->summary_stats.correct_predictions++;
    }
    
    if (g_analytics_data->summary_stats.total_predictions > 0) {
        g_analytics_data->summary_stats.overall_accuracy_pct = 
            (double)g_analytics_data->summary_stats.correct_predictions * 100.0 / 
            g_analytics_data->summary_stats.total_predictions;
    }
    
    // Update interface summary
    for (int i = 0; i < MAX_INTERFACES; i++) {
        if (strcmp(g_analytics_data->interface_summary[i].interface_id, result->interface_id) == 0 ||
            !g_analytics_data->interface_summary[i].is_active) {
            
            if (!g_analytics_data->interface_summary[i].is_active) {
                strncpy(g_analytics_data->interface_summary[i].interface_id, result->interface_id, 31\n"\n"\n"\n"\n"\n"\n"\n");
                g_analytics_data->interface_summary[i].is_active = true;
                g_analytics_data->interface_summary[i].predictions_made = 0;
                g_analytics_data->interface_summary[i].predictions_correct = 0;
            }
            
            g_analytics_data->interface_summary[i].predictions_made++;
            if (result->prediction_correct) {
                g_analytics_data->interface_summary[i].predictions_correct++;
            }
            
            if (g_analytics_data->interface_summary[i].predictions_made > 0) {
                g_analytics_data->interface_summary[i].accuracy_pct = 
                    (double)g_analytics_data->interface_summary[i].predictions_correct * 100.0 / 
                    g_analytics_data->interface_summary[i].predictions_made;
            }
            
            g_analytics_data->interface_summary[i].last_update = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
            break;
        }
    }
    
    pthread_mutex_unlock(&g_analytics_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("DEBUG: " Recorded prediction result for %s: %s (confidence: %u%%)",
              result->interface_id, 
              result->prediction_correct ? "CORRECT" : "INCORRECT",
              result->confidence_level\n"\n"\n"\n"\n"\n"\n"\n");
    
    return ML_MONITOR_SUCCESS;
}

// Update interface score
int ml_monitor_analytics_update_interface_score(const ml_interface_score_t *score) {
    if (!g_analytics_initialized || !score) {
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_analytics_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Find interface slot
    int interface_slot = -1;
    for (int i = 0; i < MAX_INTERFACES; i++) {
        if (strcmp(g_analytics_data->interface_summary[i].interface_id, score->interface_id) == 0) {
            interface_slot = i;
            break;
        } else if (!g_analytics_data->interface_summary[i].is_active) {
            interface_slot = i;
            strncpy(g_analytics_data->interface_summary[i].interface_id, score->interface_id, 31\n"\n"\n"\n"\n"\n"\n"\n");
            g_analytics_data->interface_summary[i].is_active = true;
            break;
        }
    }
    
    if (interface_slot == -1) {
        pthread_mutex_unlock(&g_analytics_mutex\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_NO_RESOURCES;
    }
    
    // Add to circular buffer for this interface
    uint32_t idx = g_analytics_data->interface_scores_index[interface_slot];
    g_analytics_data->interface_scores[interface_slot][idx] = *score;
    
    g_analytics_data->interface_scores_index[interface_slot] = (idx + 1) % 360;  // Updated from 720 to 360 for memory efficiency
    if (g_analytics_data->interface_scores_count[interface_slot] < 360) {  // Updated from 720 to 360 for memory efficiency
        g_analytics_data->interface_scores_count[interface_slot]++;
    }
    
    // Update interface summary
    g_analytics_data->interface_summary[interface_slot].current_score = score->overall_score;
    g_analytics_data->interface_summary[interface_slot].last_update = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (score->overall_score > g_analytics_data->interface_summary[interface_slot].best_score) {
        g_analytics_data->interface_summary[interface_slot].best_score = score->overall_score;
    }
    if (score->overall_score < g_analytics_data->interface_summary[interface_slot].worst_score) {
        g_analytics_data->interface_summary[interface_slot].worst_score = score->overall_score;
    }
    
    pthread_mutex_unlock(&g_analytics_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("DEBUG: " Updated interface score for %s: %.1f (latency impact: %+.1f, loss impact: %+.1f)",
              score->interface_id, score->overall_score,
              score->score_contributors.latency_impact,
              score->score_contributors.loss_impact\n"\n"\n"\n"\n"\n"\n"\n");
    
    return ML_MONITOR_SUCCESS;
}

// Record ML impact event
int ml_monitor_analytics_record_impact_event(const ml_impact_event_t *event) {
    if (!g_analytics_initialized || !event) {
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_analytics_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Add to circular buffer
    uint32_t idx = g_analytics_data->impact_events_index;
    g_analytics_data->impact_events[idx] = *event;
    
    g_analytics_data->impact_events_index = (idx + 1) % 100;  // Updated from 250 to 100 for memory efficiency
    if (g_analytics_data->impact_events_count < 100) {  // Updated from 250 to 100 for memory efficiency
        g_analytics_data->impact_events_count++;
    }
    
    // Update summary statistics
    if (event->ml_triggered_failover || event->ml_prevented_unnecessary_failover || event->ml_optimized_weights) {
        g_analytics_data->summary_stats.ml_triggered_actions++;
    }
    
    if (event->ml_improvement_ms > 0) {
        g_analytics_data->summary_stats.successful_optimizations++;
        g_analytics_data->summary_stats.total_improvement_ms += event->ml_improvement_ms;
    }
    
    // Update average user experience
    if (g_analytics_data->impact_events_count > 0) {
        double total_ux = 0;
        uint32_t count = g_analytics_data->impact_events_count;
        for (uint32_t i = 0; i < count; i++) {
            total_ux += g_analytics_data->impact_events[i].user_experience_score;
        }
        g_analytics_data->summary_stats.average_user_experience = total_ux / count;
    }
    
    pthread_mutex_unlock(&g_analytics_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: " Recorded ML impact event for %s: improvement %+dms, UX score %.1f",
             event->interface_id, event->ml_improvement_ms, event->user_experience_score\n"\n"\n"\n"\n"\n"\n"\n");
    
    return ML_MONITOR_SUCCESS;
}

// Calculate interface score based on current metrics
int ml_monitor_analytics_calculate_interface_score(const char *interface_id, ml_interface_score_t *score) {
    if (!interface_id || !score) {
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    // Get current interface information
    network_interface_t interfaces[MAX_INTERFACES];
    int interface_count = 0;
    
    extern int get_enhanced_comprehensive_interface_info(network_interface_t *interfaces, int *count\n"\n"\n"\n"\n"\n"\n"\n");
    int result = get_enhanced_comprehensive_interface_info(interfaces, &interface_count\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != AUTONOMY_SUCCESS) {
        return result;
    }
    
    // Find the interface
    network_interface_t *interface = NULL;
    for (int i = 0; i < interface_count; i++) {
        if (strcmp(interfaces[i].name, interface_id) == 0) {
            interface = &interfaces[i];
            break;
        }
    }
    
    if (!interface) {
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    // Initialize score structure
    memset(score, 0, sizeof(ml_interface_score_t)\n"\n"\n"\n"\n"\n"\n"\n");
    score->timestamp = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    safe_strncpy(score->interface_id, interface_id, sizeof(score->interface_id)\n"\n"\n"\n"\n"\n"\n"\n");
    score->interface_type = ml_monitor_map_interface_type(interface\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Get raw metrics
    score->current_latency_ms = interface->real_time_metrics.ping_latency_ms;
    score->current_loss_pct = 100 - interface->real_time_metrics.ping_success_rate;
    
    if (strcmp(interface->type, "cellular") == 0) {
        score->current_signal_dbm = interface->enhanced_cellular_info.signal_strength_dbm;
    } else {
        score->current_signal_dbm = interface->signal_strength;
    }
    
    // Calculate component scores
    double latency_score = ml_monitor_calculate_latency_score(score->current_latency_ms\n"\n"\n"\n"\n"\n"\n"\n");
    double loss_score = ml_monitor_calculate_loss_score(score->current_loss_pct\n"\n"\n"\n"\n"\n"\n"\n");
    double signal_score = ml_monitor_calculate_signal_score(score->current_signal_dbm, score->interface_type\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Get prediction accuracy
    pthread_mutex_lock(&g_analytics_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    for (int i = 0; i < MAX_INTERFACES; i++) {
        if (strcmp(g_analytics_data->interface_summary[i].interface_id, interface_id) == 0) {
            score->recent_predictions_correct = 
                g_analytics_data->interface_summary[i].predictions_correct;
            break;
        }
    }
    pthread_mutex_unlock(&g_analytics_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    double prediction_score = ml_monitor_calculate_prediction_score(
        score->recent_predictions_correct, 10); // Last 10 predictions
    
    // Calculate stability (consecutive stable minutes)
    if (interface->performance_history.history_count > 0) {
        score->consecutive_stable_minutes = 0;
        for (int i = interface->performance_history.history_count - 1; i >= 0; i--) {
            int idx = (interface->performance_history.history_index - 1 - i + 60) % 60;
            if (interface->performance_history.health_history[idx] > 128) { // > 50% health
                score->consecutive_stable_minutes++;
            } else {
                break;
            }
        }
    }
    
    double stability_score = ml_monitor_calculate_stability_score(
        score->consecutive_stable_minutes, interface->performance_history.history_count\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Calculate component scores and impacts
    score->accuracy_score = prediction_score;
    score->stability_score = stability_score;
    score->performance_score = (latency_score + loss_score + signal_score) / 3.0;
    
    // Calculate trend score from performance history
    if (interface->performance_history.history_count >= 10) {
        double trend_factor = 1.0 + interface->performance_history.health_trend * 0.5;
        score->trend_score = fmax(0.0, fmin(100.0, 50.0 * trend_factor)\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        score->trend_score = 50.0; // Neutral when no trend data
    }
    
    // Calculate overall score (weighted average)
    score->overall_score = (
        score->performance_score * 0.4 +   // 40% performance
        score->accuracy_score * 0.25 +     // 25% prediction accuracy
        score->stability_score * 0.25 +    // 25% stability
        score->trend_score * 0.1            // 10% trend
    \n"\n"\n"\n"\n"\n"\n"\n");
    
    // Calculate score contributors (impact of each factor)
    double baseline = 50.0;
    score->score_contributors.latency_impact = (latency_score - baseline) * 0.4;
    score->score_contributors.loss_impact = (loss_score - baseline) * 0.4;
    score->score_contributors.signal_impact = (signal_score - baseline) * 0.4;
    score->score_contributors.prediction_impact = (prediction_score - baseline) * 0.25;
    score->score_contributors.stability_impact = (stability_score - baseline) * 0.25;
    score->score_contributors.trend_impact = (score->trend_score - baseline) * 0.1;
    
    return ML_MONITOR_SUCCESS;
}

// Utility functions for score calculation
double ml_monitor_calculate_latency_score(uint16_t latency_ms) {
    if (latency_ms == 0) return 50.0; // No data
    
    // Score based on latency ranges
    if (latency_ms <= 20) return 100.0;       // Excellent
    if (latency_ms <= 50) return 90.0;        // Very good
    if (latency_ms <= 100) return 80.0;       // Good
    if (latency_ms <= 200) return 60.0;       // Fair
    if (latency_ms <= 500) return 40.0;       // Poor
    if (latency_ms <= 1000) return 20.0;      // Very poor
    return 10.0;                              // Terrible
}

double ml_monitor_calculate_loss_score(uint8_t loss_pct) {
    if (loss_pct == 0) return 100.0;          // Perfect
    if (loss_pct <= 1) return 95.0;           // Excellent
    if (loss_pct <= 2) return 85.0;           // Very good
    if (loss_pct <= 5) return 70.0;           // Good
    if (loss_pct <= 10) return 50.0;          // Fair
    if (loss_pct <= 20) return 30.0;          // Poor
    if (loss_pct <= 50) return 15.0;          // Very poor
    return 5.0;                               // Terrible
}

double ml_monitor_calculate_signal_score(int16_t signal_dbm, interface_type_t type) {
    if (signal_dbm == 0) return 50.0; // No data
    
    switch (type) {
        case INTERFACE_TYPE_CELLULAR:
            // Cellular signal strength scoring
            if (signal_dbm >= -70) return 100.0;      // Excellent
            if (signal_dbm >= -85) return 85.0;       // Very good
            if (signal_dbm >= -100) return 70.0;      // Good
            if (signal_dbm >= -110) return 50.0;      // Fair
            if (signal_dbm >= -120) return 30.0;      // Poor
            return 10.0;                              // Very poor
            
        case INTERFACE_TYPE_WIFI:
            // WiFi signal strength scoring
            if (signal_dbm >= -30) return 100.0;      // Excellent
            if (signal_dbm >= -50) return 90.0;       // Very good
            if (signal_dbm >= -60) return 80.0;       // Good
            if (signal_dbm >= -70) return 60.0;       // Fair
            if (signal_dbm >= -80) return 40.0;       // Poor
            return 20.0;                              // Very poor
            
        case INTERFACE_TYPE_STARLINK:
        case INTERFACE_TYPE_LAN:
        default:
            // For wired connections, signal strength is not applicable
            return 100.0; // Always good for wired
    }
}

double ml_monitor_calculate_prediction_score(uint32_t correct, uint32_t total) {
    if (total == 0) return 50.0; // No predictions yet
    
    double accuracy = (double)correct / total;
    return accuracy * 100.0;
}

double ml_monitor_calculate_stability_score(uint32_t stable_minutes, uint32_t total_minutes) {
    if (total_minutes == 0) return 50.0; // No data
    
    double stability_ratio = (double)stable_minutes / total_minutes;
    return stability_ratio * 100.0;
}

// Get analytics data for visualization
int ml_monitor_analytics_get_data(ml_analytics_data_t *data) {
    if (!g_analytics_initialized || !data) {
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_analytics_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    *data = *g_analytics_data;
    pthread_mutex_unlock(&g_analytics_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    return ML_MONITOR_SUCCESS;
}

// Get interface score history for graphing
int ml_monitor_analytics_get_interface_score_history(const char *interface_id,
                                                   ml_interface_score_t *scores,
                                                   uint32_t max_scores,
                                                   uint32_t *actual_count) {
    if (!interface_id || !scores || !actual_count) {
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_analytics_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Find interface slot
    int interface_slot = -1;
    for (int i = 0; i < MAX_INTERFACES; i++) {
        if (strcmp(g_analytics_data->interface_summary[i].interface_id, interface_id) == 0) {
            interface_slot = i;
            break;
        }
    }
    
    if (interface_slot == -1) {
        *actual_count = 0;
        pthread_mutex_unlock(&g_analytics_mutex\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    // Copy score history
    uint32_t available_scores = g_analytics_data->interface_scores_count[interface_slot];
    uint32_t scores_to_copy = fmin(available_scores, max_scores\n"\n"\n"\n"\n"\n"\n"\n");
    
    for (uint32_t i = 0; i < scores_to_copy; i++) {
        uint32_t src_idx = (g_analytics_data->interface_scores_index[interface_slot] - scores_to_copy + i + 360) % 360;  // Updated from 720 to 360 for memory efficiency
        scores[i] = g_analytics_data->interface_scores[interface_slot][src_idx];
    }
    
    *actual_count = scores_to_copy;
    pthread_mutex_unlock(&g_analytics_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    return ML_MONITOR_SUCCESS;
}

// Get prediction accuracy trends
int ml_monitor_analytics_get_accuracy_trend(const char *interface_id,
                                          uint32_t hours,
                                          double *accuracy_pct,
                                          int *trend_direction) {
    if (!accuracy_pct || !trend_direction) {
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_analytics_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    time_t cutoff_time = time(NULL) - (hours * 3600\n"\n"\n"\n"\n"\n"\n"\n");
    uint32_t total_predictions = 0;
    uint32_t correct_predictions = 0;
    
    // Analyze predictions within time window
    uint32_t count = g_analytics_data->prediction_results_count;
    for (uint32_t i = 0; i < count; i++) {
        ml_prediction_result_t *result = &g_analytics_data->prediction_results[i];
        
        if (result->timestamp < cutoff_time) continue;
        
        if (interface_id && strcmp(result->interface_id, interface_id) != 0) continue;
        
        total_predictions++;
        if (result->prediction_correct) {
            correct_predictions++;
        }
    }
    
    if (total_predictions > 0) {
        *accuracy_pct = (double)correct_predictions * 100.0 / total_predictions;
    } else {
        *accuracy_pct = 0.0;
    }
    
    // Calculate trend direction (compare first half vs second half)
    if (total_predictions >= 10) {
        uint32_t half_point = total_predictions / 2;
        uint32_t first_half_correct = 0, second_half_correct = 0;
        uint32_t first_half_total = 0, second_half_total = 0;
        
        uint32_t processed = 0;
        for (uint32_t i = 0; i < count && processed < total_predictions; i++) {
            ml_prediction_result_t *result = &g_analytics_data->prediction_results[i];
            
            if (result->timestamp < cutoff_time) continue;
            if (interface_id && strcmp(result->interface_id, interface_id) != 0) continue;
            
            if (processed < half_point) {
                first_half_total++;
                if (result->prediction_correct) first_half_correct++;
            } else {
                second_half_total++;
                if (result->prediction_correct) second_half_correct++;
            }
            processed++;
        }
        
        if (first_half_total > 0 && second_half_total > 0) {
            double first_half_accuracy = (double)first_half_correct / first_half_total;
            double second_half_accuracy = (double)second_half_correct / second_half_total;
            
            if (second_half_accuracy > first_half_accuracy + 0.05) {
                *trend_direction = 1; // Improving
            } else if (second_half_accuracy < first_half_accuracy - 0.05) {
                *trend_direction = -1; // Declining
            } else {
                *trend_direction = 0; // Stable
            }
        } else {
            *trend_direction = 0;
        }
    } else {
        *trend_direction = 0;
    }
    
    pthread_mutex_unlock(&g_analytics_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    return ML_MONITOR_SUCCESS;
}

// Get ML impact summary
int ml_monitor_analytics_get_impact_summary(uint32_t hours,
                                          int32_t *total_improvement_ms,
                                          double *stability_improvement_pct,
                                          uint32_t *actions_taken) {
    if (!total_improvement_ms || !stability_improvement_pct || !actions_taken) {
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_analytics_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    time_t cutoff_time = time(NULL) - (hours * 3600\n"\n"\n"\n"\n"\n"\n"\n");
    *total_improvement_ms = 0;
    *actions_taken = 0;
    double total_stability_improvement = 0.0;
    uint32_t stability_events = 0;
    
    uint32_t count = g_analytics_data->impact_events_count;
    for (uint32_t i = 0; i < count; i++) {
        ml_impact_event_t *event = &g_analytics_data->impact_events[i];
        
        if (event->timestamp < cutoff_time) continue;
        
        *total_improvement_ms += event->ml_improvement_ms;
        
        if (event->ml_triggered_failover || event->ml_prevented_unnecessary_failover || 
            event->ml_optimized_weights) {
            (*actions_taken)++;
        }
        
        if (event->stability_improvement_pct > 0) {
            total_stability_improvement += event->stability_improvement_pct;
            stability_events++;
        }
    }
    
    if (stability_events > 0) {
        *stability_improvement_pct = total_stability_improvement / stability_events;
    } else {
        *stability_improvement_pct = 0.0;
    }
    
    pthread_mutex_unlock(&g_analytics_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    return ML_MONITOR_SUCCESS;
}
