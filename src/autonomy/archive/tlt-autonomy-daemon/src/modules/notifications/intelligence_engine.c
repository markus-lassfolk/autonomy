#include "intelligence_engine.h"
#include "smart_manager.h"
#include "emergency_detector.h"
#include "priority_optimizer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>

// Global intelligence engine instance
static intelligence_engine_t g_intelligence_engine;
static bool g_intelligence_engine_initialized = false;

// Forward declarations
static void* intelligence_loop(void* arg);
static void perform_intelligence_tasks(void);
static void perform_learning_tasks(void);
static void update_channel_effectiveness(void);
static void check_for_anomalies(void);
static double calculate_effectiveness_score(bool was_successful, time_t response_time);
static double calculate_average(double* values, int count);
static double calculate_confidence(double* values, int count);

// Initialize intelligence engine
int intelligence_engine_init(const intelligence_config_t* config) {
    if (g_intelligence_engine_initialized) {
        return 0; // Already initialized
    }
    
    if (!config) {
        return -1;
    }
    
    memset(&g_intelligence_engine, 0, sizeof(intelligence_engine_t));
    
    // Copy configuration
    g_intelligence_engine.config = *config;
    
    // Initialize mutex
    g_intelligence_engine.mutex = malloc(sizeof(pthread_mutex_t));
    if (!g_intelligence_engine.mutex) {
        return -1;
    }
    
    pthread_mutex_init(g_intelligence_engine.mutex, NULL);
    
    // Initialize notification patterns
    if (config->max_notification_patterns > 0) {
        g_intelligence_engine.notification_patterns = malloc(config->max_notification_patterns * sizeof(notification_pattern_t));
        if (!g_intelligence_engine.notification_patterns) {
            pthread_mutex_destroy(g_intelligence_engine.mutex);
            free(g_intelligence_engine.mutex);
            return -1;
        }
        
        g_intelligence_engine.max_notification_patterns = config->max_notification_patterns;
        g_intelligence_engine.notification_patterns_count = 0;
    }
    
    // Initialize user patterns
    if (config->max_user_patterns > 0) {
        g_intelligence_engine.user_patterns = malloc(config->max_user_patterns * sizeof(user_behavior_pattern_t));
        if (!g_intelligence_engine.user_patterns) {
            free(g_intelligence_engine.notification_patterns);
            pthread_mutex_destroy(g_intelligence_engine.mutex);
            free(g_intelligence_engine.mutex);
            return -1;
        }
        
        g_intelligence_engine.max_user_patterns = config->max_user_patterns;
        g_intelligence_engine.user_patterns_count = 0;
    }
    
    // Initialize metrics
    memset(&g_intelligence_engine.metrics, 0, sizeof(intelligence_metrics_t));
    g_intelligence_engine.metrics.last_updated = time(NULL);
    
    // Start intelligence monitoring thread
    g_intelligence_engine.thread_running = true;
    if (pthread_create(&g_intelligence_engine.intelligence_thread, NULL, intelligence_loop, NULL) != 0) {
        free(g_intelligence_engine.user_patterns);
        free(g_intelligence_engine.notification_patterns);
        pthread_mutex_destroy(g_intelligence_engine.mutex);
        free(g_intelligence_engine.mutex);
        return -1;
    }
    
    g_intelligence_engine_initialized = true;
    return 0;
}

// Clean up intelligence engine
void intelligence_engine_cleanup(void) {
    if (!g_intelligence_engine_initialized) return;
    
    // Stop intelligence thread
    g_intelligence_engine.thread_running = false;
    pthread_join(g_intelligence_engine.intelligence_thread, NULL);
    
    if (g_intelligence_engine.mutex) {
        pthread_mutex_destroy(g_intelligence_engine.mutex);
        free(g_intelligence_engine.mutex);
    }
    
    if (g_intelligence_engine.notification_patterns) {
        free(g_intelligence_engine.notification_patterns);
    }
    
    if (g_intelligence_engine.user_patterns) {
        free(g_intelligence_engine.user_patterns);
    }
    
    g_intelligence_engine.notification_patterns = NULL;
    g_intelligence_engine.user_patterns = NULL;
    g_intelligence_engine.mutex = NULL;
    g_intelligence_engine.notification_patterns_count = 0;
    g_intelligence_engine.max_notification_patterns = 0;
    g_intelligence_engine.user_patterns_count = 0;
    g_intelligence_engine.max_user_patterns = 0;
    
    g_intelligence_engine_initialized = false;
}

// Intelligence monitoring thread
static void* intelligence_loop(void* arg) {
    (void)arg; // Unused parameter
    
    while (g_intelligence_engine.thread_running) {
        perform_intelligence_tasks();
        sleep(60); // Run every minute
    }
    
    return NULL;
}

// Perform intelligence tasks
static void perform_intelligence_tasks(void) {
    if (!g_intelligence_engine_initialized) return;
    
    // Perform learning if enabled
    if (g_intelligence_engine.config.learning_enabled) {
        perform_learning_tasks();
    }
    
    // Update channel effectiveness
    if (g_intelligence_engine.config.channel_intelligence_enabled) {
        update_channel_effectiveness();
    }
    
    // Check for pattern anomalies
    check_for_anomalies();
    
    // Update metrics timestamp
    pthread_mutex_lock(g_intelligence_engine.mutex);
    g_intelligence_engine.metrics.last_updated = time(NULL);
    pthread_mutex_unlock(g_intelligence_engine.mutex);
}

// Perform learning tasks
static void perform_learning_tasks(void) {
    pthread_mutex_lock(g_intelligence_engine.mutex);
    
    // Simple learning: analyze recent patterns
    time_t now = time(NULL);
    time_t cutoff = now - g_intelligence_engine.config.learning_window_seconds;
    
    // Count recent effective patterns
    int effective_patterns = 0;
    int total_patterns = 0;
    
    for (int i = 0; i < g_intelligence_engine.notification_patterns_count; i++) {
        notification_pattern_t* pattern = &g_intelligence_engine.notification_patterns[i];
        
        if (pattern->last_seen > cutoff) {
            total_patterns++;
            if (pattern->effectiveness_score > 0.7) {
                effective_patterns++;
            }
        }
    }
    
    // Update model accuracy
    if (total_patterns > 0) {
        g_intelligence_engine.metrics.model_accuracy = (double)effective_patterns / (double)total_patterns;
    }
    
    g_intelligence_engine.metrics.learning_iterations++;
    
    pthread_mutex_unlock(g_intelligence_engine.mutex);
}

// Update channel effectiveness
static void update_channel_effectiveness(void) {
    // This would integrate with actual delivery statistics
    // For now, simulate effectiveness based on patterns
    
    pthread_mutex_lock(g_intelligence_engine.mutex);
    
    // Simulate channel effectiveness (in production, would calculate from actual delivery data)
    // These values would be updated based on actual success/failure rates
    
    g_intelligence_engine.metrics.delivery_optimizations++;
    
    pthread_mutex_unlock(g_intelligence_engine.mutex);
}

// Check for anomalies in notification patterns
static void check_for_anomalies(void) {
    pthread_mutex_lock(g_intelligence_engine.mutex);
    
    time_t now = time(NULL);
    time_t recent_cutoff = now - 3600; // Last hour
    
    // Count recent emergency detections
    // This would integrate with actual emergency detection data
    
    // Check for unusual failure rates
    if (g_intelligence_engine.metrics.emergencies_detected > 10) {
        printf("INTELLIGENCE: Anomaly detected - high emergency rate\n");
    }
    
    pthread_mutex_unlock(g_intelligence_engine.mutex);
}

// Calculate effectiveness score
static double calculate_effectiveness_score(bool was_successful, time_t response_time) {
    double score = 1.0;
    
    // Reduce score for failures
    if (!was_successful) {
        score *= 0.1;
    }
    
    // Reduce score for slow responses
    if (response_time > 300) { // 5 minutes
        score *= 0.8;
    } else if (response_time > 60) { // 1 minute
        score *= 0.9;
    }
    
    return score;
}

// Calculate average of array
static double calculate_average(double* values, int count) {
    if (count <= 0) return 0.0;
    
    double sum = 0.0;
    for (int i = 0; i < count; i++) {
        sum += values[i];
    }
    
    return sum / (double)count;
}

// Calculate confidence based on variance
static double calculate_confidence(double* values, int count) {
    if (count < 2) return 0.5;
    
    double avg = calculate_average(values, count);
    double variance = 0.0;
    
    for (int i = 0; i < count; i++) {
        variance += pow(values[i] - avg, 2);
    }
    variance /= (double)(count - 1);
    
    // Convert variance to confidence
    double confidence = 1.0 / (1.0 + variance);
    return fmin(confidence, 1.0);
}

// Process intelligent notification
int intelligence_engine_process_notification(const notification_event_t* event,
                                            const system_state_t* system_state,
                                            notification_event_t* optimized_event) {
    if (!g_intelligence_engine_initialized || !event || !optimized_event) {
        return -1;
    }
    
    // Copy original event
    *optimized_event = *event;
    
    time_t start_time = time(NULL);
    
    // Detect emergency conditions if enabled
    if (g_intelligence_engine.config.emergency_detection_enabled && system_state) {
        emergency_level_t emergency_level = emergency_detector_detect_emergency(
            system_state, event->type, event->details_json);
        
        if (emergency_level > EMERGENCY_LEVEL_NONE) {
            // Force emergency priority
            optimized_event->priority = NOTIFICATION_PRIORITY_EMERGENCY;
            
            // Update emergency metrics
            pthread_mutex_lock(g_intelligence_engine.mutex);
            g_intelligence_engine.metrics.emergencies_detected++;
            pthread_mutex_unlock(g_intelligence_engine.mutex);
            
            printf("INTELLIGENCE: Emergency detected (level %d) for notification %s\n", 
                   emergency_level, event->id);
        }
    }
    
    // Optimize priority if enabled
    if (g_intelligence_engine.config.priority_optimization_enabled && priority_optimizer_is_initialized()) {
        notification_priority_t original_priority = optimized_event->priority;
        optimized_event->priority = priority_optimizer_optimize_priority(
            event->type, event->priority, system_state, event->details_json);
        
        if (optimized_event->priority != original_priority) {
            pthread_mutex_lock(g_intelligence_engine.mutex);
            g_intelligence_engine.metrics.priority_adjustments++;
            pthread_mutex_unlock(g_intelligence_engine.mutex);
        }
    }
    
    // Add intelligence processing time to metrics
    time_t processing_time = time(NULL) - start_time;
    if (processing_time > g_intelligence_engine.metrics.emergency_response_time_avg) {
        pthread_mutex_lock(g_intelligence_engine.mutex);
        g_intelligence_engine.metrics.emergency_response_time_avg = processing_time;
        pthread_mutex_unlock(g_intelligence_engine.mutex);
    }
    
    return 0;
}

// Learn from notification result
int intelligence_engine_learn_from_result(notification_type_t alert_type,
                                         notification_priority_t used_priority,
                                         notification_channel_t used_channels[],
                                         int used_channels_count,
                                         bool was_successful,
                                         time_t response_time) {
    if (!g_intelligence_engine_initialized || !g_intelligence_engine.config.learning_enabled) {
        return -1;
    }
    
    pthread_mutex_lock(g_intelligence_engine.mutex);
    
    // Find existing pattern or create new one
    notification_pattern_t* pattern = NULL;
    const char* alert_type_str = notification_type_to_string(alert_type);
    
    for (int i = 0; i < g_intelligence_engine.notification_patterns_count; i++) {
        if (g_intelligence_engine.notification_patterns[i].alert_type == alert_type) {
            pattern = &g_intelligence_engine.notification_patterns[i];
            break;
        }
    }
    
    if (!pattern) {
        // Create new pattern
        if (g_intelligence_engine.notification_patterns_count < g_intelligence_engine.max_notification_patterns) {
            pattern = &g_intelligence_engine.notification_patterns[g_intelligence_engine.notification_patterns_count];
            g_intelligence_engine.notification_patterns_count++;
        } else {
            // Replace oldest pattern
            int oldest_index = 0;
            time_t oldest_time = g_intelligence_engine.notification_patterns[0].last_seen;
            for (int i = 1; i < g_intelligence_engine.max_notification_patterns; i++) {
                if (g_intelligence_engine.notification_patterns[i].last_seen < oldest_time) {
                    oldest_time = g_intelligence_engine.notification_patterns[i].last_seen;
                    oldest_index = i;
                }
            }
            pattern = &g_intelligence_engine.notification_patterns[oldest_index];
        }
        
        // Initialize new pattern
        pattern->alert_type = alert_type;
        pattern->optimal_priority = (int)used_priority;
        pattern->effectiveness_score = calculate_effectiveness_score(was_successful, response_time);
        pattern->frequency = 1;
        pattern->user_response_time = response_time;
        
        // Copy channels
        pattern->optimal_channels_count = (used_channels_count < 8) ? used_channels_count : 8;
        for (int i = 0; i < pattern->optimal_channels_count; i++) {
            pattern->optimal_channels[i] = used_channels[i];
        }
    } else {
        // Update existing pattern with exponential smoothing
        double alpha = 0.3; // Learning rate
        double new_effectiveness = calculate_effectiveness_score(was_successful, response_time);
        
        pattern->effectiveness_score = (1.0 - alpha) * pattern->effectiveness_score + alpha * new_effectiveness;
        pattern->frequency++;
        
        // Update optimal priority if this was more effective
        if (new_effectiveness > pattern->effectiveness_score) {
            pattern->optimal_priority = (int)used_priority;
            
            // Update optimal channels
            pattern->optimal_channels_count = (used_channels_count < 8) ? used_channels_count : 8;
            for (int i = 0; i < pattern->optimal_channels_count; i++) {
                pattern->optimal_channels[i] = used_channels[i];
            }
        }
        
        // Update response time with exponential smoothing
        pattern->user_response_time = (time_t)((1.0 - alpha) * pattern->user_response_time + alpha * response_time);
    }
    
    pattern->last_seen = time(NULL);
    
    // Update learning metrics
    g_intelligence_engine.metrics.learning_iterations++;
    
    // Update optimization accuracy
    if (g_intelligence_engine.notification_patterns_count > 0) {
        double total_effectiveness = 0.0;
        for (int i = 0; i < g_intelligence_engine.notification_patterns_count; i++) {
            total_effectiveness += g_intelligence_engine.notification_patterns[i].effectiveness_score;
        }
        g_intelligence_engine.metrics.model_accuracy = total_effectiveness / g_intelligence_engine.notification_patterns_count;
    }
    
    pthread_mutex_unlock(g_intelligence_engine.mutex);
    return 0;
}

// Update system state
int intelligence_engine_update_system_state(const system_state_t* system_state) {
    if (!g_intelligence_engine_initialized || !system_state) {
        return -1;
    }
    
    // This would store the system state for use in intelligent decisions
    // For now, just update metrics timestamp
    pthread_mutex_lock(g_intelligence_engine.mutex);
    g_intelligence_engine.metrics.last_updated = time(NULL);
    pthread_mutex_unlock(g_intelligence_engine.mutex);
    
    return 0;
}

// Get intelligence metrics
void intelligence_engine_get_metrics(intelligence_metrics_t* metrics) {
    if (!metrics || !g_intelligence_engine_initialized) return;
    
    pthread_mutex_lock(g_intelligence_engine.mutex);
    *metrics = g_intelligence_engine.metrics;
    pthread_mutex_unlock(g_intelligence_engine.mutex);
}

// Get intelligence engine status
void intelligence_engine_get_status(intelligence_engine_status_t* status) {
    if (!status || !g_intelligence_engine_initialized) return;
    
    pthread_mutex_lock(g_intelligence_engine.mutex);
    
    status->enabled = true;
    status->emergency_detection_enabled = g_intelligence_engine.config.emergency_detection_enabled;
    status->priority_optimization_enabled = g_intelligence_engine.config.priority_optimization_enabled;
    status->learning_enabled = g_intelligence_engine.config.learning_enabled;
    status->channel_intelligence_enabled = g_intelligence_engine.config.channel_intelligence_enabled;
    status->delivery_optimization_enabled = g_intelligence_engine.config.delivery_optimization_enabled;
    status->notification_patterns_count = g_intelligence_engine.notification_patterns_count;
    status->max_notification_patterns = g_intelligence_engine.max_notification_patterns;
    status->user_patterns_count = g_intelligence_engine.user_patterns_count;
    status->max_user_patterns = g_intelligence_engine.max_user_patterns;
    status->model_accuracy = g_intelligence_engine.metrics.model_accuracy;
    status->last_learning_update = g_intelligence_engine.metrics.last_updated;
    
    pthread_mutex_unlock(g_intelligence_engine.mutex);
}

// Get learning statistics
void intelligence_engine_get_learning_stats(char* stats_json, size_t max_size) {
    if (!stats_json || max_size == 0 || !g_intelligence_engine_initialized) return;
    
    pthread_mutex_lock(g_intelligence_engine.mutex);
    
    snprintf(stats_json, max_size,
             "{"
             "\"learning_enabled\":%s,"
             "\"notification_patterns\":%d,"
             "\"user_patterns\":%d,"
             "\"model_accuracy\":%.3f,"
             "\"learning_iterations\":%lu,"
             "\"emergencies_detected\":%lu,"
             "\"priority_adjustments\":%lu,"
             "\"delivery_optimizations\":%lu,"
             "\"prediction_confidence\":%.3f,"
             "\"last_updated\":%ld"
             "}",
             g_intelligence_engine.config.learning_enabled ? "true" : "false",
             g_intelligence_engine.notification_patterns_count,
             g_intelligence_engine.user_patterns_count,
             g_intelligence_engine.metrics.model_accuracy,
             g_intelligence_engine.metrics.learning_iterations,
             g_intelligence_engine.metrics.emergencies_detected,
             g_intelligence_engine.metrics.priority_adjustments,
             g_intelligence_engine.metrics.delivery_optimizations,
             g_intelligence_engine.metrics.prediction_confidence,
             g_intelligence_engine.metrics.last_updated);
    
    pthread_mutex_unlock(g_intelligence_engine.mutex);
}

// Check if intelligence engine is initialized
bool intelligence_engine_is_initialized(void) {
    return g_intelligence_engine_initialized;
}

// Get intelligence engine instance
intelligence_engine_t* intelligence_engine_get_instance(void) {
    return g_intelligence_engine_initialized ? &g_intelligence_engine : NULL;
}
