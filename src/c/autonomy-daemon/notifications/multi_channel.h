#ifndef MULTI_CHANNEL_H
#define MULTI_CHANNEL_H

#include "notification_types.h"
#include "webhook_client.h"
#include "email_client.h"
#include <stdbool.h>
#include <time.h>

// Channel test result
typedef struct {
    notification_channel_t channel;
    bool success;
    char error_message[256];
    time_t test_time;
    time_t response_time_ms;
} channel_test_result_t;

// Multi-channel configuration
typedef struct {
    // Channel configurations
    webhook_config_t webhook_config;
    email_config_t email_config;
    
    // Channel enablement
    bool webhook_enabled;
    bool email_enabled;
    bool pushover_enabled;
    bool slack_enabled;
    bool discord_enabled;
    bool telegram_enabled;
    bool sms_enabled;
    bool syslog_enabled;
    bool ubus_enabled;
    
    // Global settings
    bool parallel_sending;
    int max_concurrent_sends;
    time_t channel_timeout_seconds;
    int retry_failed_channels;
} multi_channel_config_t;

// Multi-channel status
typedef struct {
    int enabled_channels_count;
    int total_channels_count;
    int total_notifications_sent;
    int total_failures;
    time_t last_notification_time;
    time_t last_error_time;
    char last_error[256];
    
    // Per-channel status
    bool channel_enabled[16];
    int channel_sent_count[16];
    int channel_failed_count[16];
    time_t channel_last_success[16];
    time_t channel_last_failure[16];
} multi_channel_status_t;

// Multi-channel notifier structure
typedef struct {
    multi_channel_config_t config;
    multi_channel_status_t status;
    
    // Channel clients
    webhook_client_t webhook_client;
    email_client_t email_client;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} multi_channel_notifier_t;

// Initialize multi-channel notifier
int multi_channel_notifier_init(multi_channel_notifier_t* notifier, const multi_channel_config_t* config);

// Clean up multi-channel notifier
void multi_channel_notifier_cleanup(multi_channel_notifier_t* notifier);

// Send notification to all enabled channels
int multi_channel_notifier_send(multi_channel_notifier_t* notifier, const notification_event_t* event);

// Send notification to specific channels
int multi_channel_notifier_send_to_channels(multi_channel_notifier_t* notifier, 
                                           const notification_event_t* event,
                                           notification_channel_t channels[], 
                                           int channel_count);

// Test all enabled channels
int multi_channel_notifier_test_channels(multi_channel_notifier_t* notifier, 
                                        channel_test_result_t* results, 
                                        int max_results);

// Get enabled channels
int multi_channel_notifier_get_enabled_channels(multi_channel_notifier_t* notifier,
                                               notification_channel_t* channels,
                                               int max_channels);

// Get multi-channel status
void multi_channel_notifier_get_status(multi_channel_notifier_t* notifier, multi_channel_status_t* status);

// Get channel-specific status
void multi_channel_notifier_get_channel_status(multi_channel_notifier_t* notifier,
                                              notification_channel_t channel,
                                              char* status_json, size_t max_size);

// Enable/disable specific channel
int multi_channel_notifier_set_channel_enabled(multi_channel_notifier_t* notifier,
                                              notification_channel_t channel,
                                              bool enabled);

// Reset channel statistics
int multi_channel_notifier_reset_channel_stats(multi_channel_notifier_t* notifier,
                                              notification_channel_t channel);

#endif // MULTI_CHANNEL_H
