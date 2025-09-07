#ifndef DELIVERY_OPTIMIZER_H
#define DELIVERY_OPTIMIZER_H

#include "notification_types.h"
#include "emergency_detector.h"
#include <stdbool.h>
#include <time.h>

// Delivery plan
typedef struct {
    bool delay_delivery;
    time_t optimal_time;
    char reason[256];
    double confidence;
    time_t estimated_delay_seconds;
    time_t alternative_time;
    bool has_alternative_time;
} delivery_plan_t;

// Time window for delivery optimization
typedef struct {
    char start[8];     // "HH:MM"
    char end[8];       // "HH:MM"
    int days[8];       // Days of week (0=Sunday)
    int day_count;
    int priority;
} delivery_time_window_t;

// User behavior pattern for delivery optimization
typedef struct {
    int time_of_day;
    int day_of_week;
    time_t average_response_time_seconds;
    double activity_level;
    double confidence;
} delivery_user_pattern_t;

// Delivery optimizer configuration
typedef struct {
    bool delivery_optimization_enabled;
    bool learning_enabled;
    time_t max_delay_seconds;
    time_t quiet_hours_start; // Hours since midnight (e.g., 22 for 10 PM)
    time_t quiet_hours_end;   // Hours since midnight (e.g., 8 for 8 AM)
    bool respect_business_hours;
    bool respect_quiet_hours;
    bool respect_maintenance_windows;
    int max_user_patterns;
} delivery_optimizer_config_t;

// Delivery optimizer status
typedef struct {
    bool enabled;
    bool learning_enabled;
    int user_patterns_count;
    int max_user_patterns;
    int total_optimizations;
    int deliveries_delayed;
    time_t average_delay_seconds;
    double optimization_confidence;
} delivery_optimizer_status_t;

// Delivery optimizer structure
typedef struct {
    delivery_optimizer_config_t config;
    
    // User behavior patterns
    delivery_user_pattern_t* user_patterns;
    int max_user_patterns;
    int user_patterns_count;
    
    // Statistics
    int total_optimizations;
    int deliveries_delayed;
    time_t total_delay_seconds;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} delivery_optimizer_t;

// Initialize delivery optimizer
int delivery_optimizer_init(const delivery_optimizer_config_t* config);

// Clean up delivery optimizer
void delivery_optimizer_cleanup(void);

// Optimize delivery timing
void delivery_optimizer_optimize_delivery(notification_type_t alert_type,
                                         notification_priority_t priority,
                                         const system_state_t* system_state,
                                         const char* base_data_json,
                                         delivery_plan_t* plan);

// Update user behavior pattern
int delivery_optimizer_update_user_pattern(int time_of_day,
                                          int day_of_week,
                                          time_t response_time_seconds,
                                          double activity_level);

// Calculate optimal delivery time
time_t delivery_optimizer_calculate_optimal_time(notification_type_t alert_type,
                                                const system_state_t* system_state);

// Check if delivery should be delayed
bool delivery_optimizer_should_delay(notification_type_t alert_type,
                                    notification_priority_t priority,
                                    time_t delay_seconds,
                                    const system_state_t* system_state);

// Calculate delivery confidence
double delivery_optimizer_calculate_confidence(notification_type_t alert_type,
                                             const system_state_t* system_state);

// Get delivery optimizer status
void delivery_optimizer_get_status(delivery_optimizer_status_t* status);

// Get delivery statistics
void delivery_optimizer_get_stats(char* stats_json, size_t max_size);

// Check if delivery optimizer is initialized
bool delivery_optimizer_is_initialized(void);

// Get delivery optimizer instance
delivery_optimizer_t* delivery_optimizer_get_instance(void);

#endif // DELIVERY_OPTIMIZER_H
