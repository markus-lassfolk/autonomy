#ifndef INTELLIGENCE_ENGINE_H
#define INTELLIGENCE_ENGINE_H

#include "notification_types.h"
#include "emergency_detector.h"
#include "priority_optimizer.h"
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

// Intelligence metrics
typedef struct {
    uint64_t emergencies_detected;
    uint64_t false_positives;
    time_t emergency_response_time_avg;
    uint64_t escalations_triggered;
    double escalation_success_rate;
    uint64_t priority_adjustments;
    double optimization_accuracy;
    uint64_t delivery_optimizations;
    uint64_t learning_iterations;
    double model_accuracy;
    double prediction_confidence;
    time_t last_updated;
} intelligence_metrics_t;

// Learning pattern
typedef struct {
    notification_type_t alert_type;
    char context_json[512];
    int optimal_priority;
    notification_channel_t optimal_channels[8];
    int optimal_channels_count;
    time_t user_response_time;
    double effectiveness_score;
    int frequency;
    time_t last_seen;
} notification_pattern_t;

// User behavior pattern
typedef struct {
    int time_of_day;
    int day_of_week;
    notification_channel_t preferred_channels[8];
    int preferred_channels_count;
    time_t average_response_time;
    double activity_level;
    double confidence;
} user_behavior_pattern_t;

// Intelligence configuration
typedef struct {
    bool emergency_detection_enabled;
    bool priority_optimization_enabled;
    bool learning_enabled;
    double adaptation_rate;
    bool channel_intelligence_enabled;
    bool delivery_optimization_enabled;
    bool escalation_enabled;
    int max_escalation_level;
    time_t escalation_cooldown_seconds;
    time_t learning_window_seconds;
    int min_samples_for_learning;
    double confidence_threshold;
    int max_notification_patterns;
    int max_user_patterns;
} intelligence_config_t;

// Intelligence engine status
typedef struct {
    bool enabled;
    bool emergency_detection_enabled;
    bool priority_optimization_enabled;
    bool learning_enabled;
    bool channel_intelligence_enabled;
    bool delivery_optimization_enabled;
    int notification_patterns_count;
    int max_notification_patterns;
    int user_patterns_count;
    int max_user_patterns;
    double model_accuracy;
    time_t last_learning_update;
} intelligence_engine_status_t;

// Intelligence engine structure
typedef struct {
    intelligence_config_t config;
    
    // Learning data
    notification_pattern_t* notification_patterns;
    int max_notification_patterns;
    int notification_patterns_count;
    
    user_behavior_pattern_t* user_patterns;
    int max_user_patterns;
    int user_patterns_count;
    
    // Metrics
    intelligence_metrics_t metrics;
    
    // Thread management
    pthread_t intelligence_thread;
    bool thread_running;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} intelligence_engine_t;

// Initialize intelligence engine
int intelligence_engine_init(const intelligence_config_t* config);

// Clean up intelligence engine
void intelligence_engine_cleanup(void);

// Process intelligent notification
int intelligence_engine_process_notification(const notification_event_t* event,
                                            const system_state_t* system_state,
                                            notification_event_t* optimized_event);

// Learn from notification result
int intelligence_engine_learn_from_result(notification_type_t alert_type,
                                         notification_priority_t used_priority,
                                         notification_channel_t used_channels[],
                                         int used_channels_count,
                                         bool was_successful,
                                         time_t response_time);

// Update system state
int intelligence_engine_update_system_state(const system_state_t* system_state);

// Get intelligence metrics
void intelligence_engine_get_metrics(intelligence_metrics_t* metrics);

// Get intelligence engine status
void intelligence_engine_get_status(intelligence_engine_status_t* status);

// Get learning statistics
void intelligence_engine_get_learning_stats(char* stats_json, size_t max_size);

// Check if intelligence engine is initialized
bool intelligence_engine_is_initialized(void);

// Get intelligence engine instance
intelligence_engine_t* intelligence_engine_get_instance(void);

#endif // INTELLIGENCE_ENGINE_H
