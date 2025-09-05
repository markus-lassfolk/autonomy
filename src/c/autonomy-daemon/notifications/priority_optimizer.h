#ifndef PRIORITY_OPTIMIZER_H
#define PRIORITY_OPTIMIZER_H

#include "notification_types.h"
#include "emergency_detector.h"
#include <stdbool.h>
#include <time.h>

// Learning data for priority optimization
typedef struct {
    char alert_type[64];
    int optimal_priority;
    double effectiveness_score;
    double confidence_score;
    time_t last_updated;
} priority_learning_data_t;

// Priority optimizer configuration
typedef struct {
    bool priority_optimization_enabled;
    bool learning_enabled;
    double adaptation_rate;
    double confidence_threshold;
    int max_learning_entries;
} priority_optimizer_config_t;

// Priority scores structure
typedef struct {
    double context_score;
    double learning_score;
    double urgency_score;
    double business_score;
    double total_adjustment;
} priority_scores_t;

// Priority optimizer status
typedef struct {
    bool enabled;
    bool learning_enabled;
    int learning_entries_count;
    int max_learning_entries;
    double adaptation_rate;
    double confidence_threshold;
    int total_optimizations;
    int priority_adjustments_made;
} priority_optimizer_status_t;

// Priority optimizer structure
typedef struct {
    priority_optimizer_config_t config;
    
    // Learning data
    priority_learning_data_t* learning_data;
    int max_learning_entries;
    int learning_entries_count;
    
    // Statistics
    int total_optimizations;
    int priority_adjustments_made;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} priority_optimizer_t;

// Initialize priority optimizer
int priority_optimizer_init(const priority_optimizer_config_t* config);

// Clean up priority optimizer
void priority_optimizer_cleanup(void);

// Optimize notification priority
notification_priority_t priority_optimizer_optimize_priority(notification_type_t alert_type,
                                                           notification_priority_t base_priority,
                                                           const system_state_t* system_state,
                                                           const char* base_data_json);

// Update learning data
int priority_optimizer_update_learning(notification_type_t alert_type,
                                      notification_priority_t used_priority,
                                      double effectiveness_score);

// Get priority scores breakdown
void priority_optimizer_get_priority_scores(notification_type_t alert_type,
                                           notification_priority_t base_priority,
                                           const system_state_t* system_state,
                                           const char* base_data_json,
                                           priority_scores_t* scores);

// Get priority optimizer status
void priority_optimizer_get_status(priority_optimizer_status_t* status);

// Get optimization statistics
void priority_optimizer_get_stats(char* stats_json, size_t max_size);

// Check if priority optimizer is initialized
bool priority_optimizer_is_initialized(void);

// Get priority optimizer instance
priority_optimizer_t* priority_optimizer_get_instance(void);

#endif // PRIORITY_OPTIMIZER_H
