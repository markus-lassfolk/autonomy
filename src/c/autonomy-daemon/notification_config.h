#ifndef NOTIFICATION_CONFIG_H
#define NOTIFICATION_CONFIG_H

#include "notification_types.h"
#include "webhook_client.h"
#include "email_client.h"
#include "pushover_client.h"
#include "discord_client.h"
#include "slack_client.h"
#include <stdbool.h>
#include <time.h>

// Comprehensive notification configuration
typedef struct {
    // Global notification settings
    bool notifications_enabled;
    time_t cooldown_period_seconds;
    time_t emergency_cooldown_seconds;
    int max_notifications_hour;
    int retry_attempts;
    time_t retry_delay_seconds;
    time_t http_timeout_seconds;
    
    // Priority settings for different notification types
    notification_priority_t priority_failover;
    notification_priority_t priority_failback;
    notification_priority_t priority_member_down;
    notification_priority_t priority_member_up;
    notification_priority_t priority_predictive;
    notification_priority_t priority_critical;
    notification_priority_t priority_recovery;
    notification_priority_t priority_status_update;
    
    // Channel configurations
    bool pushover_enabled;
    pushover_config_t pushover_config;
    
    bool email_enabled;
    email_config_t email_config;
    
    bool slack_enabled;
    slack_config_t slack_config;
    
    bool discord_enabled;
    discord_config_t discord_config;
    
    bool webhook_enabled;
    webhook_config_t webhook_config;
    
    bool sms_enabled;
    char sms_provider[128];
    char sms_phone_number[32];
    
    bool syslog_enabled;
    bool ubus_enabled;
    
    // Advanced settings
    bool quiet_hours_enabled;
    char quiet_hours_start[8];
    char quiet_hours_end[8];
    bool suppress_low_priority_weekends;
    
    // Intelligence settings
    bool smart_management_enabled;
    bool contextual_alerts_enabled;
    bool emergency_detection_enabled;
    bool escalation_enabled;
    bool priority_optimization_enabled;
    bool delivery_optimization_enabled;
    bool acknowledgment_tracking_enabled;
} comprehensive_notification_config_t;

// Configuration validation result
typedef struct {
    bool is_valid;
    char error_messages[1024];
    int warning_count;
    int error_count;
} config_validation_result_t;

// Configuration manager structure
typedef struct {
    comprehensive_notification_config_t config;
    bool config_loaded;
    time_t last_loaded;
    time_t last_validated;
    config_validation_result_t last_validation;
} notification_config_manager_t;

// Initialize notification configuration manager
int notification_config_manager_init(notification_config_manager_t* config_mgr);

// Clean up notification configuration manager
void notification_config_manager_cleanup(notification_config_manager_t* config_mgr);

// Load configuration from UCI
int notification_config_manager_load_from_uci(notification_config_manager_t* config_mgr);

// Load default configuration
int notification_config_manager_load_defaults(notification_config_manager_t* config_mgr);

// Validate notification configuration
int notification_config_manager_validate(notification_config_manager_t* config_mgr,
                                        config_validation_result_t* result);

// Get configuration
const comprehensive_notification_config_t* notification_config_manager_get_config(notification_config_manager_t* config_mgr);

// Update specific channel configuration
int notification_config_manager_update_channel(notification_config_manager_t* config_mgr,
                                             notification_channel_t channel,
                                             const void* channel_config);

// Enable/disable specific channel
int notification_config_manager_set_channel_enabled(notification_config_manager_t* config_mgr,
                                                   notification_channel_t channel,
                                                   bool enabled);

// Update priority for notification type
int notification_config_manager_set_type_priority(notification_config_manager_t* config_mgr,
                                                 notification_type_t type,
                                                 notification_priority_t priority);

// Save configuration to UCI
int notification_config_manager_save_to_uci(notification_config_manager_t* config_mgr);

// Reload configuration from UCI
int notification_config_manager_reload(notification_config_manager_t* config_mgr);

// Get configuration as JSON
void notification_config_manager_get_json(notification_config_manager_t* config_mgr,
                                         char* json_output, size_t max_size);

// Import configuration from JSON
int notification_config_manager_import_json(notification_config_manager_t* config_mgr,
                                           const char* json_input);

// Check if configuration is valid
bool notification_config_manager_is_valid(notification_config_manager_t* config_mgr);

// Get last validation result
void notification_config_manager_get_validation_result(notification_config_manager_t* config_mgr,
                                                      config_validation_result_t* result);

#endif // NOTIFICATION_CONFIG_H
