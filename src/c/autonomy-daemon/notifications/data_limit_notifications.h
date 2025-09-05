#ifndef DATA_LIMIT_NOTIFICATIONS_H
#define DATA_LIMIT_NOTIFICATIONS_H

#include "notification_types.h"
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

// Daily usage tracker
typedef struct {
    char interface_name[64];
    time_t last_reset_date;
    double daily_allowance_mb;
    double today_usage_mb;
    double yesterday_usage_mb;
    double last_usage_check_mb;
    time_t last_usage_check_time;
    bool daily_warning_80_sent;
    bool daily_warning_100_sent;
} daily_usage_tracker_t;

// Data limit configuration
typedef struct {
    char interface_name[64];
    bool enabled;
    int data_limit_mb;
    double current_usage_mb;
    double usage_percentage;
    int days_until_reset;
} data_limit_config_t;

// Data limit notification configuration
typedef struct {
    bool enabled;
    int max_interfaces;
    int max_last_notifications;
    time_t failover_cooldown_seconds;
    time_t daily_threshold_cooldown_seconds;
    time_t monthly_threshold_cooldown_seconds;
    time_t usage_spike_cooldown_seconds;
} data_limit_notification_config_t;

// Last notification record
typedef struct {
    char notification_key[128];
    time_t last_sent_time;
} last_notification_record_t;

// Data limit notification status
typedef struct {
    bool enabled;
    int tracked_interfaces_count;
    int max_interfaces;
    int last_notifications_count;
    int max_last_notifications;
} data_limit_notification_status_t;

// Data limit notification manager structure
typedef struct {
    data_limit_notification_config_t config;
    
    // Usage tracking
    daily_usage_tracker_t* daily_usage_trackers;
    int max_interfaces;
    int tracked_interfaces_count;
    
    // Last notification tracking
    last_notification_record_t* last_notifications;
    int max_last_notifications;
    int last_notifications_count;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} data_limit_notification_manager_t;

// Initialize data limit notification manager
int data_limit_notification_manager_init(const data_limit_notification_config_t* config);

// Clean up data limit notification manager
void data_limit_notification_manager_cleanup(void);

// Notify failover to limited connection
int data_limit_notification_manager_notify_failover_to_limited(const char* from_interface, 
                                                             const char* to_interface,
                                                             const data_limit_config_t* data_limit);

// Notify failback from limited connection
int data_limit_notification_manager_notify_failback_from_limited(const char* from_interface,
                                                                const char* to_interface,
                                                                const data_limit_config_t* data_limit);

// Notify daily usage threshold
int data_limit_notification_manager_notify_daily_usage_threshold(const char* interface_name,
                                                                const data_limit_config_t* data_limit,
                                                                int percentage);

// Notify monthly usage threshold
int data_limit_notification_manager_notify_monthly_usage_threshold(const char* interface_name,
                                                                  const data_limit_config_t* data_limit);

// Notify unexpected usage spike
int data_limit_notification_manager_notify_unexpected_usage_spike(const char* interface_name,
                                                                 const data_limit_config_t* data_limit);

// Notify data limit reset
int data_limit_notification_manager_notify_data_limit_reset(const char* interface_name,
                                                           const data_limit_config_t* data_limit);

// Update usage tracking
int data_limit_notification_manager_update_usage_tracking(const char* interface_name,
                                                         const data_limit_config_t* data_limit);

// Check all data limit notifications
int data_limit_notification_manager_check_all_notifications(data_limit_config_t* data_limits,
                                                           int data_limits_count);

// Get data limit notification manager status
void data_limit_notification_manager_get_status(data_limit_notification_status_t* status);

// Check if data limit notification manager is initialized
bool data_limit_notification_manager_is_initialized(void);

// Get data limit notification manager instance
data_limit_notification_manager_t* data_limit_notification_manager_get_instance(void);

#endif // DATA_LIMIT_NOTIFICATIONS_H