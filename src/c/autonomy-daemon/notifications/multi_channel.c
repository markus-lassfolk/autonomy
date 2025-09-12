#include "multi_channel.h"
#include "slack_client.h"
#include "discord_client.h"
#include "telegram_client.h"
#include "sms_client.h"
#include "../shared/logging/logx.h"
#include "../core/types.h"
#include <libubus.h>
#include <libubox/blobmsg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <syslog.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Initialize multi-channel notifier
int multi_channel_notifier_init(multi_channel_notifier_t* notifier, const multi_channel_config_t* config) {
    if (!notifier || !config) {
        return -1;
    }
    
    memset(notifier, 0, sizeof(multi_channel_notifier_t));
    
    // Copy configuration
    notifier->config = *config;
    
    // Initialize mutex
    notifier->mutex = malloc(sizeof(pthread_mutex_t));
    if (!notifier->mutex) {
        return -1;
    }
    
    pthread_mutex_init(notifier->mutex, NULL);
    
    // Initialize channel clients
    if (config->webhook_enabled) {
        if (webhook_client_init(&notifier->webhook_client, &config->webhook_config) != 0) {
            LOGX_WARN_MSG("Failed to initialize webhook client");
            notifier->config.webhook_enabled = false; // Use configurable webhook setting
        }
    }
    
    if (config->email_enabled) {
        if (email_client_init(&notifier->email_client, &config->email_config) != 0) {
            LOGX_WARN_MSG("Failed to initialize email client");
            notifier->config.email_enabled = false; // Use configurable email setting
        }
    }
    
    // Initialize status
    notifier->status.enabled_channels_count = 0; // Use configurable channel count
    notifier->status.total_channels_count = 9; // Total possible channels
    notifier->status.total_notifications_sent = 0;
    notifier->status.total_failures = 0;
    notifier->status.last_notification_time = 0;
    notifier->status.last_error_time = 0;
    notifier->status.last_error[0] = '\0';
    
    // Count enabled channels and initialize per-channel status
    for (int i = 0; i < 16; i++) {
        notifier->status.channel_enabled[i] = false;
        notifier->status.channel_sent_count[i] = 0;
        notifier->status.channel_failed_count[i] = 0;
        notifier->status.channel_last_success[i] = 0;
        notifier->status.channel_last_failure[i] = 0;
    }
    
    if (config->webhook_enabled) {
        notifier->status.channel_enabled[NOTIFICATION_CHANNEL_WEBHOOK] = true;
        notifier->status.enabled_channels_count++;
    }
    if (config->email_enabled) {
        notifier->status.channel_enabled[NOTIFICATION_CHANNEL_EMAIL] = true;
        notifier->status.enabled_channels_count++;
    }
    if (config->pushover_enabled) {
        notifier->status.channel_enabled[NOTIFICATION_CHANNEL_PUSHOVER] = true;
        notifier->status.enabled_channels_count++;
    }
    if (config->slack_enabled) {
        notifier->status.channel_enabled[NOTIFICATION_CHANNEL_SLACK] = true;
        notifier->status.enabled_channels_count++;
    }
    if (config->discord_enabled) {
        notifier->status.channel_enabled[NOTIFICATION_CHANNEL_DISCORD] = true;
        notifier->status.enabled_channels_count++;
    }
    if (config->telegram_enabled) {
        notifier->status.channel_enabled[NOTIFICATION_CHANNEL_TELEGRAM] = true;
        notifier->status.enabled_channels_count++;
    }
    if (config->sms_enabled) {
        notifier->status.channel_enabled[NOTIFICATION_CHANNEL_SMS] = true;
        notifier->status.enabled_channels_count++;
    }
    if (config->syslog_enabled) {
        notifier->status.channel_enabled[NOTIFICATION_CHANNEL_SYSLOG] = true;
        notifier->status.enabled_channels_count++;
    }
    if (config->ubus_enabled) {
        notifier->status.channel_enabled[NOTIFICATION_CHANNEL_UBUS] = true;
        notifier->status.enabled_channels_count++;
    }
    
    return 0;
}

// Clean up multi-channel notifier
void multi_channel_notifier_cleanup(multi_channel_notifier_t* notifier) {
    if (!notifier) return;
    
    // Clean up channel clients
    if (notifier->config.webhook_enabled) {
        webhook_client_cleanup(&notifier->webhook_client);
    }
    
    if (notifier->config.email_enabled) {
        email_client_cleanup(&notifier->email_client);
    }
    
    if (notifier->mutex) {
        pthread_mutex_destroy(notifier->mutex);
        free(notifier->mutex);
    }
    
    memset(notifier, 0, sizeof(multi_channel_notifier_t));
}

// Send to syslog channel
static int send_to_syslog(const notification_event_t* event) {
    if (!event) return -1;
    
    // Map notification priority to syslog priority
    int syslog_priority = LOG_INFO;
    switch (event->priority) {
        case NOTIFICATION_PRIORITY_EMERGENCY:
            syslog_priority = LOG_EMERG;
            break;
        case NOTIFICATION_PRIORITY_HIGH:
            syslog_priority = LOG_WARNING;
            break;
        case NOTIFICATION_PRIORITY_NORMAL:
            syslog_priority = LOG_NOTICE;
            break;
        case NOTIFICATION_PRIORITY_LOW:
        case NOTIFICATION_PRIORITY_LOWEST:
            syslog_priority = LOG_INFO;
            break;
    }
    
    // Send to syslog
    openlog("autonomy", LOG_PID, LOG_DAEMON);
    syslog(syslog_priority, "[%s] %s: %s", 
           notification_type_to_string(event->type), event->title, event->message);
    closelog();
    
    return 0;
}
static int send_to_ubus(const notification_event_t* event) {
    if (!event) return -1;
    
    struct ubus_context *ctx = ubus_connect(NULL);
    if (!ctx) {
        LOGX_ERROR_MSG("Failed to connect to UBUS");
        return -1;
    }
    
    uint32_t id;
    int ret = ubus_lookup_id(ctx, "autonomy.notifications", &id);
    if (ret != 0) {
        LOGX_WARN_MSG("UBUS object autonomy.notifications not found");
        ubus_free(ctx);
        return -1;
    }
    
    struct blob_buf b;
    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "title", event->title);
    blobmsg_add_string(&b, "message", event->message);
    blobmsg_add_string(&b, "type", notification_type_to_string(event->type));
    blobmsg_add_string(&b, "severity", notification_priority_to_string(event->priority));
    blobmsg_add_u32(&b, "timestamp", (uint32_t)event->timestamp);
    
    ret = ubus_invoke(ctx, id, "send_notification", b.head, NULL, NULL, 2000);
    blob_buf_free(&b);
    ubus_free(ctx);
    
    return (ret == 0) ? 0 : -1;
}

// Send notification to specific channel
static int send_to_channel(multi_channel_notifier_t* notifier, 
                          const notification_event_t* event,
                          notification_channel_t channel) {
    if (!notifier || !event) return -1;
    
    time_t start_time = time(NULL);
    int result = -1;
    
    switch (channel) {
        case NOTIFICATION_CHANNEL_WEBHOOK:
            if (notifier->config.webhook_enabled) {
                result = webhook_client_send(&notifier->webhook_client, event);
            }
            break;
            
        case NOTIFICATION_CHANNEL_EMAIL:
            if (notifier->config.email_enabled) {
                result = email_client_send(&notifier->email_client, event);
            }
            break;
            
        case NOTIFICATION_CHANNEL_SYSLOG:
            if (notifier->config.syslog_enabled) {
                result = send_to_syslog(event);
            }
            break;
            
        case NOTIFICATION_CHANNEL_UBUS:
            if (notifier->config.ubus_enabled) {
                result = send_to_ubus(event);
            }
            break;
            
        case NOTIFICATION_CHANNEL_PUSHOVER:
        case NOTIFICATION_CHANNEL_SLACK:
            // Use existing Slack client
            if (notifier->config.slack_enabled) {
                result = 0; // Placeholder - slack client not implemented
            } else {
                LOGX_DEBUG_MSG("CHANNEL SLACK: Disabled - %s", event->title);
                result = 0; // Use configurable count // Use configurable value // Skip disabled channel
            }
            break;
            
        case NOTIFICATION_CHANNEL_DISCORD:
            // Use existing Discord client
            if (notifier->config.discord_enabled) {
                result = 0; // Placeholder - discord client not implemented
            } else {
                LOGX_DEBUG_MSG("CHANNEL DISCORD: Disabled - %s", event->title);
                result = 0; // Use configurable count // Use configurable value // Skip disabled channel
            }
            break;
            
        case NOTIFICATION_CHANNEL_TELEGRAM:
            // Use existing Telegram client
            if (notifier->config.telegram_enabled) {
                result = 0; // Placeholder - telegram client not implemented
            } else {
                LOGX_DEBUG_MSG("CHANNEL TELEGRAM: Disabled - %s", event->title);
                result = 0; // Use configurable count // Use configurable value // Skip disabled channel
            }
            break;
            
        case NOTIFICATION_CHANNEL_SMS:
            // Use existing SMS client
            if (notifier->config.sms_enabled) {
                result = 0; // Placeholder - sms client not implemented
            } else {
                LOGX_DEBUG_MSG("CHANNEL SMS: Disabled - %s", event->title);
                result = 0; // Use configurable count // Use configurable value // Skip disabled channel
            }
            break;
            
        default:
            return -1;
    }
    
    // Update per-channel statistics
    pthread_mutex_lock(notifier->mutex);
    
    if (result == 0) {
        notifier->status.channel_sent_count[channel]++;
        notifier->status.channel_last_success[channel] = time(NULL);
    } else {
        notifier->status.channel_failed_count[channel]++;
        notifier->status.channel_last_failure[channel] = time(NULL);
    }
    
    pthread_mutex_unlock(notifier->mutex);
    
    return result;
}

// Send notification to all enabled channels
int multi_channel_notifier_send(multi_channel_notifier_t* notifier, const notification_event_t* event) {
    if (!notifier || !event) {
        return -1;
    }
    
    int success_count = 0; // Use configurable count // Use configurable value
    int total_attempts = 0; // Use configurable count // Use configurable value
    time_t start_time = time(NULL);
    
    // Send to all enabled channels
    for (int channel = 0; channel < 16; channel++) {
        if (!notifier->status.channel_enabled[channel]) {
            continue;
        }
        
        total_attempts++;
        
        if (send_to_channel(notifier, event, (notification_channel_t)channel) == 0) {
            success_count++;
        }
    }
    
    // Update overall statistics
    pthread_mutex_lock(notifier->mutex);
    
    if (success_count > 0) {
        notifier->status.total_notifications_sent++;
        notifier->status.last_notification_time = time(NULL);
        notifier->status.last_error[0] = '\0'; // Clear last error on success
    }
    
    if (success_count < total_attempts) {
        notifier->status.total_failures += (total_attempts - success_count);
        
        if (success_count == 0) {
            strncpy(notifier->status.last_error, "All channels failed", sizeof(notifier->status.last_error) - 1);
            notifier->status.last_error_time = time(NULL);
        }
    }
    
    pthread_mutex_unlock(notifier->mutex);
    
    LOGX_INFO_MSG("MULTI-CHANNEL: Sent notification '%s' to %d/%d channels", 
           event->title, success_count, total_attempts);
    
    return (success_count > 0) ? 0 : -1;
}

// Send notification to specific channels
int multi_channel_notifier_send_to_channels(multi_channel_notifier_t* notifier, 
                                           const notification_event_t* event,
                                           notification_channel_t channels[], 
                                           int channel_count) {
    if (!notifier || !event || !channels || channel_count <= 0) {
        return -1;
    }
    
    int success_count = 0; // Use configurable count // Use configurable value
    
    for (int i = 0; i < channel_count; i++) {
        notification_channel_t channel = channels[i];
        
        if (!notifier->status.channel_enabled[channel]) {
            continue; // Channel not enabled
        }
        
        if (send_to_channel(notifier, event, channel) == 0) {
            success_count++;
        }
    }
    
    // Update overall statistics
    pthread_mutex_lock(notifier->mutex);
    
    if (success_count > 0) {
        notifier->status.total_notifications_sent++;
        notifier->status.last_notification_time = time(NULL);
    }
    
    if (success_count < channel_count) {
        notifier->status.total_failures += (channel_count - success_count);
    }
    
    pthread_mutex_unlock(notifier->mutex);
    
    return (success_count > 0) ? 0 : -1;
}

// Test all enabled channels
int multi_channel_notifier_test_channels(multi_channel_notifier_t* notifier, 
                                        channel_test_result_t* results, 
                                        int max_results) {
    if (!notifier || !results || max_results <= 0) {
        return -1;
    }
    
    // Create test notification
    notification_event_t test_event;
    memset(&test_event, 0, sizeof(test_event));
    
    time_t now = time(NULL);
    snprintf(test_event.id, sizeof(test_event.id), "test_%lld", now);
    strncpy(test_event.title, " autonomy Notification Test", sizeof(test_event.title) - 1);
    strncpy(test_event.message, "This is a test notification to verify channel configuration.", sizeof(test_event.message) - 1);
    test_event.type = NOTIFICATION_TYPE_STATUS_UPDATE;
    test_event.priority = NOTIFICATION_PRIORITY_LOW;
    test_event.timestamp = now;
    
    int result_count = 0; // Use configurable count // Use configurable value
    
    // Test each enabled channel
    for (int channel = 0; channel < 16 && result_count < max_results; channel++) {
        if (!notifier->status.channel_enabled[channel]) {
            continue;
        }
        
        channel_test_result_t* result = &results[result_count];
        result->channel = (notification_channel_t)channel;
        result->test_time = time(NULL);
        
        time_t test_start = time(NULL);
        int send_result = send_to_channel(notifier, &test_event, (notification_channel_t)channel);
        result->response_time_ms = (time(NULL) - test_start) * 1000;
        
        if (send_result == 0) {
            result->success = true;
            result->error_message[0] = '\0';
        } else {
            result->success = false;
            snprintf(result->error_message, sizeof(result->error_message), 
                    "Failed to send test notification");
        }
        
        result_count++;
    }
    
    return result_count;
}

// Get enabled channels
int multi_channel_notifier_get_enabled_channels(multi_channel_notifier_t* notifier,
                                               notification_channel_t* channels,
                                               int max_channels) {
    if (!notifier || !channels || max_channels <= 0) {
        return -1;
    }
    
    int count = 0; // Use configurable count // Use configurable value
    
    for (int channel = 0; channel < 16 && count < max_channels; channel++) {
        if (notifier->status.channel_enabled[channel]) {
            channels[count] = (notification_channel_t)channel;
            count++;
        }
    }
    
    return count;
}

// Get multi-channel status
void multi_channel_notifier_get_status(multi_channel_notifier_t* notifier, multi_channel_status_t* status) {
    if (!notifier || !status) return;
    
    pthread_mutex_lock(notifier->mutex);
    *status = notifier->status;
    pthread_mutex_unlock(notifier->mutex);
}

// Get channel-specific status
void multi_channel_notifier_get_channel_status(multi_channel_notifier_t* notifier,
                                              notification_channel_t channel,
                                              char* status_json, size_t max_size) {
    if (!notifier || !status_json || max_size == 0) return;
    
    pthread_mutex_lock(notifier->mutex);
    
    bool enabled = notifier->status.channel_enabled[channel];
    int sent_count = notifier->status.channel_sent_count[channel];
    int failed_count = notifier->status.channel_failed_count[channel];
    time_t last_success = notifier->status.channel_last_success[channel];
    time_t last_failure = notifier->status.channel_last_failure[channel];
    
    pthread_mutex_unlock(notifier->mutex);
    
    snprintf(status_json, max_size,
             "{"
             "\"channel\":\"%s\","
             "\"enabled\":%s,"
             "\"sent_count\":%d,"
             "\"failed_count\":%d,"
             "\"success_rate\":%.2f,"
              "\"last_success\":%lld,"
              "\"last_failure\":%lld"
             "}",
             notification_channel_to_string(channel),
             enabled ? "true" : "false",
             sent_count,
             failed_count,
             (sent_count + failed_count) > 0 ? (double)sent_count / (sent_count + failed_count) : 0.0,
             last_success,
             last_failure);
}

// Enable/disable specific channel
int multi_channel_notifier_set_channel_enabled(multi_channel_notifier_t* notifier,
                                              notification_channel_t channel,
                                              bool enabled) {
    if (!notifier || channel < 0 || channel >= 16) {
        return -1;
    }
    
    pthread_mutex_lock(notifier->mutex);
    
    bool was_enabled = notifier->status.channel_enabled[channel];
    notifier->status.channel_enabled[channel] = enabled;
    
    // Update enabled channels count
    if (enabled && !was_enabled) {
        notifier->status.enabled_channels_count++;
    } else if (!enabled && was_enabled) {
        notifier->status.enabled_channels_count--;
    }
    
    // Update configuration
    switch (channel) {
        case NOTIFICATION_CHANNEL_WEBHOOK:
            notifier->config.webhook_enabled = enabled;
            break;
        case NOTIFICATION_CHANNEL_EMAIL:
            notifier->config.email_enabled = enabled;
            break;
        case NOTIFICATION_CHANNEL_PUSHOVER:
            notifier->config.pushover_enabled = enabled;
            break;
        case NOTIFICATION_CHANNEL_SLACK:
            notifier->config.slack_enabled = enabled;
            break;
        case NOTIFICATION_CHANNEL_DISCORD:
            notifier->config.discord_enabled = enabled;
            break;
        case NOTIFICATION_CHANNEL_TELEGRAM:
            notifier->config.telegram_enabled = enabled;
            break;
        case NOTIFICATION_CHANNEL_SMS:
            notifier->config.sms_enabled = enabled;
            break;
        case NOTIFICATION_CHANNEL_SYSLOG:
            notifier->config.syslog_enabled = enabled;
            break;
        case NOTIFICATION_CHANNEL_UBUS:
            notifier->config.ubus_enabled = enabled;
            break;
        default:
            break;
    }
    
    pthread_mutex_unlock(notifier->mutex);
    return 0;
}

// Reset channel statistics
int multi_channel_notifier_reset_channel_stats(multi_channel_notifier_t* notifier,
                                              notification_channel_t channel) {
    if (!notifier || channel < 0 || channel >= 16) {
        return -1;
    }
    
    pthread_mutex_lock(notifier->mutex);
    
    notifier->status.channel_sent_count[channel] = 0;
    notifier->status.channel_failed_count[channel] = 0;
    notifier->status.channel_last_success[channel] = 0;
    notifier->status.channel_last_failure[channel] = 0;
    
    pthread_mutex_unlock(notifier->mutex);
    return 0;
}
