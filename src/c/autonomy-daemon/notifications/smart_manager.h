#ifndef SMART_MANAGER_H
#define SMART_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include "notification_types.h"

// Smart notification manager for intelligent delivery

// Manager configuration
typedef struct {
    bool enable_smart_routing;
    bool enable_priority_queuing;
    bool enable_adaptive_rate_limiting;
    bool enable_delivery_optimization;
    int max_concurrent_deliveries;
    int health_check_interval;
    float success_rate_threshold;
} smart_manager_config_t;

// Delivery statistics
typedef struct {
    int total_sent;
    int total_delivered;
    int total_failed;
    int total_acknowledged;
    float success_rate;
    float average_delivery_time;
    time_t last_delivery;
    int active_deliveries;
} delivery_stats_t;

// Function declarations
int smart_manager_init(const smart_manager_config_t *config);
void smart_manager_cleanup(void);
int smart_manager_send_notification(const notification_t *notification);
int smart_manager_get_stats(delivery_stats_t *stats);
int smart_manager_optimize_delivery(const char *notification_id);
int smart_manager_handle_failure(const char *notification_id, int error_code);

#endif // SMART_MANAGER_H
