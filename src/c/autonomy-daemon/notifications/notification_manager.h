#ifndef NOTIFICATION_MANAGER_H
#define NOTIFICATION_MANAGER_H

#include "notification_types.h"
#include "priority_queue.h"
#include "adaptive_rate_limiter.h"
#include "notification_deduplicator.h"
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

// Notification manager structure
typedef struct {
    notification_config_t config;
    channel_config_t channels;
    
    // Core components
    priority_queue_t priority_queue;
    adaptive_rate_limiter_t rate_limiter;
    notification_deduplicator_t deduplicator;
    
    // Threading
    pthread_mutex_t* mutex;
    bool worker_running;
    pthread_t worker_thread;
    
    // Statistics
    notification_stats_t stats;
} notification_manager_t;

// Notification status structure
typedef struct {
    bool enabled;
    bool worker_running;
    int queue_size;
    bool queue_empty;
    bool queue_full;
    int current_rate_limit;
    bool rate_limiter_emergency_mode;
    bool deduplication_enabled;
    double duplicate_rate;
} notification_manager_status_t;

// Initialize notification manager
int notification_manager_init(const notification_config_t* config);

// Clean up notification manager
void notification_manager_cleanup(void);

// Start notification worker thread
int notification_manager_start_worker(void);

// Stop notification worker thread
void notification_manager_stop_worker(void);

// Send notification with specified priority
int notification_manager_send(notification_type_t type, const char* title, const char* message,
                            notification_priority_t priority, const char* member_name);

// Send notification with default priority for type
int notification_manager_send_default(notification_type_t type, const char* title, const char* message);

// Get notification manager status
void notification_manager_get_status(notification_manager_status_t* status);

// Get notification manager statistics
void notification_manager_get_stats(notification_stats_t* stats);

// Reset notification manager statistics
void notification_manager_reset_stats(void);

// Check if notification manager is initialized
bool notification_manager_is_initialized(void);

// Get notification manager instance
notification_manager_t* notification_manager_get_instance(void);

// Convenience functions for common notification types
int notification_manager_send_failover(const char* title, const char* message, const char* member_name);
int notification_manager_send_failback(const char* title, const char* message, const char* member_name);
int notification_manager_send_member_down(const char* title, const char* message, const char* member_name);
int notification_manager_send_member_up(const char* title, const char* message, const char* member_name);
int notification_manager_send_predictive(const char* title, const char* message, const char* member_name);
int notification_manager_send_critical_error(const char* title, const char* message);
int notification_manager_send_recovery(const char* title, const char* message);
int notification_manager_send_status_update(const char* title, const char* message);

// Data limit notification functions
int notification_manager_send_data_limit_failover(const char* title, const char* message, const char* member_name);
int notification_manager_send_data_limit_failback(const char* title, const char* message, const char* member_name);
int notification_manager_send_data_limit_daily_80(const char* title, const char* message);
int notification_manager_send_data_limit_daily_100(const char* title, const char* message);
int notification_manager_send_data_limit_monthly_80(const char* title, const char* message);
int notification_manager_send_data_limit_monthly_95(const char* title, const char* message);
int notification_manager_send_data_limit_exceeded(const char* title, const char* message);
int notification_manager_send_data_limit_reset(const char* title, const char* message);
int notification_manager_send_data_usage_spike(const char* title, const char* message);

#endif // NOTIFICATION_MANAGER_H
