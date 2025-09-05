#include "notification_manager.h"
#include "priority_queue.h"
#include "adaptive_rate_limiter.h"
#include "notification_deduplicator.h"
#include "pushover_client.h"
#include "email_client.h"
#include "slack_client.h"
#include "discord_client.h"
#include "telegram_client.h"
#include "webhook_client.h"
#include "sms_client.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

// Global notification manager instance
static notification_manager_t g_notification_manager;
static bool g_manager_initialized = false;

// Initialize notification manager
int notification_manager_init(const notification_config_t* config) {
    if (g_manager_initialized) {
        return 0; // Already initialized
    }
    
    if (!config) {
        return -1;
    }
    
    memset(&g_notification_manager, 0, sizeof(notification_manager_t));
    
    // Copy configuration
    g_notification_manager.config = *config;
    
    // Initialize priority queue
    if (priority_queue_init(&g_notification_manager.priority_queue, 1000) != 0) {
        return -1;
    }
    
    // Initialize rate limiter
    rate_limiter_config_t rate_config = {
        .initial_rate = 100,
        .min_rate = 10,
        .max_rate = 1000,
        .window_size_seconds = 3600, // 1 hour
        .emergency_rate_limit = 50,
        .high_rate_limit = 100,
        .normal_rate_limit = 200,
        .low_rate_limit = 300,
        .lowest_rate_limit = 500,
        .emergency_cooldown_seconds = 60,
        .high_cooldown_seconds = 300,
        .normal_cooldown_seconds = 600,
        .low_cooldown_seconds = 1800,
        .lowest_cooldown_seconds = 3600,
        .success_threshold = 10,
        .failure_threshold = 5,
        .adjustment_factor = 0.1,
        .min_adjustment_interval = 300
    };
    
    if (adaptive_rate_limiter_init(&g_notification_manager.rate_limiter, &rate_config) != 0) {
        priority_queue_cleanup(&g_notification_manager.priority_queue);
        return -1;
    }
    
    // Initialize deduplicator
    deduplicator_config_t dedup_config = {
        .enabled = true,
        .max_fingerprints = 1000,
        .deduplication_window_seconds = 3600, // 1 hour
        .similarity_threshold = 0.8
    };
    
    if (notification_deduplicator_init(&g_notification_manager.deduplicator, &dedup_config) != 0) {
        priority_queue_cleanup(&g_notification_manager.priority_queue);
        adaptive_rate_limiter_cleanup(&g_notification_manager.rate_limiter);
        return -1;
    }
    
    // Initialize mutex
    g_notification_manager.mutex = malloc(sizeof(pthread_mutex_t));
    if (!g_notification_manager.mutex) {
        priority_queue_cleanup(&g_notification_manager.priority_queue);
        adaptive_rate_limiter_cleanup(&g_notification_manager.rate_limiter);
        notification_deduplicator_cleanup(&g_notification_manager.deduplicator);
        return -1;
    }
    
    pthread_mutex_init(g_notification_manager.mutex, NULL);
    
    // Initialize statistics
    g_notification_manager.stats.total_notifications = 0;
    g_notification_manager.stats.sent_notifications = 0;
    g_notification_manager.stats.failed_notifications = 0;
    g_notification_manager.stats.duplicate_notifications = 0;
    g_notification_manager.stats.rate_limited_notifications = 0;
    
    // Initialize worker thread
    g_notification_manager.worker_running = false;
    g_notification_manager.worker_thread = 0;
    
    g_manager_initialized = true;
    return 0;
}

// Clean up notification manager
void notification_manager_cleanup(void) {
    if (!g_manager_initialized) return;
    
    // Stop worker thread
    notification_manager_stop_worker();
    
    // Clean up components
    if (g_notification_manager.mutex) {
        pthread_mutex_destroy(g_notification_manager.mutex);
        free(g_notification_manager.mutex);
    }
    
    priority_queue_cleanup(&g_notification_manager.priority_queue);
    adaptive_rate_limiter_cleanup(&g_notification_manager.rate_limiter);
    notification_deduplicator_cleanup(&g_notification_manager.deduplicator);
    
    g_manager_initialized = false;
}

// Generate unique notification ID
static void generate_notification_id(char* id, size_t size) {
    if (!id || size < 33) return;
    
    time_t now = time(NULL);
    unsigned int random = (unsigned int)rand();
    
    snprintf(id, size, "%08lx-%08x", (unsigned long)now, random);
}

// Check if notification should be sent based on configuration
static bool should_send_notification(notification_type_t type) {
    const notification_config_t* config = &g_notification_manager.config;
    
    switch (type) {
        case NOTIFICATION_TYPE_FAILOVER:
            return config->notify_on_failover;
        case NOTIFICATION_TYPE_FAILBACK:
            return config->notify_on_failback;
        case NOTIFICATION_TYPE_MEMBER_DOWN:
            return config->notify_on_member_down;
        case NOTIFICATION_TYPE_MEMBER_UP:
            return config->notify_on_member_up;
        case NOTIFICATION_TYPE_PREDICTIVE:
            return config->notify_on_predictive;
        case NOTIFICATION_TYPE_CRITICAL_ERROR:
            return config->notify_on_critical;
        case NOTIFICATION_TYPE_RECOVERY:
            return config->notify_on_recovery;
        case NOTIFICATION_TYPE_STATUS_UPDATE:
            return config->notify_on_status_update;
        default:
            return true; // Send other types by default
    }
}

// Get priority for notification type
static notification_priority_t get_notification_priority(notification_type_t type) {
    const notification_config_t* config = &g_notification_manager.config;
    
    switch (type) {
        case NOTIFICATION_TYPE_FAILOVER:
            return (notification_priority_t)config->priority_failover;
        case NOTIFICATION_TYPE_FAILBACK:
            return (notification_priority_t)config->priority_failback;
        case NOTIFICATION_TYPE_MEMBER_DOWN:
            return (notification_priority_t)config->priority_member_down;
        case NOTIFICATION_TYPE_MEMBER_UP:
            return (notification_priority_t)config->priority_member_up;
        case NOTIFICATION_TYPE_PREDICTIVE:
            return (notification_priority_t)config->priority_predictive;
        case NOTIFICATION_TYPE_CRITICAL_ERROR:
            return (notification_priority_t)config->priority_critical;
        case NOTIFICATION_TYPE_RECOVERY:
            return (notification_priority_t)config->priority_recovery;
        case NOTIFICATION_TYPE_STATUS_UPDATE:
            return (notification_priority_t)config->priority_status_update;
        default:
            return NOTIFICATION_PRIORITY_NORMAL;
    }
}

// Send notification to all enabled channels
static bool send_notification_to_channels(const notification_event_t* event) {
    if (!event) return false;
    
    bool success = false;
    const channel_config_t* channels = &g_notification_manager.channels;
    
    // Send to Pushover if enabled
    if (channels->pushover.enabled) {
        pushover_client_t pushover_client;
        pushover_config_t pushover_config = {
            .enabled = true,
            .timeout_seconds = 30,
            .retry_attempts = 3,
            .retry_delay_seconds = 5
        };
        strncpy(pushover_config.token, channels->pushover.token, sizeof(pushover_config.token) - 1);
        strncpy(pushover_config.user, channels->pushover.user, sizeof(pushover_config.user) - 1);
        strncpy(pushover_config.device, channels->pushover.device, sizeof(pushover_config.device) - 1);
        
        if (pushover_client_init(&pushover_client, &pushover_config) == 0) {
            if (pushover_client_send(&pushover_client, event) == 0) {
                success = true;
            }
            pushover_client_cleanup(&pushover_client);
        }
    }
    
    // Send to email if enabled
    if (channels->email.enabled) {
        email_client_t email_client;
        email_config_t email_config = {
            .enabled = true,
            .smtp_port = channels->email.smtp_port,
            .use_tls = channels->email.use_tls,
            .use_starttls = channels->email.use_starttls,
            .timeout_seconds = 30,
            .retry_attempts = 3,
            .retry_delay_seconds = 5,
            .html_format = true,
            .include_context = true
        };
        strncpy(email_config.smtp_host, channels->email.smtp_host, sizeof(email_config.smtp_host) - 1);
        strncpy(email_config.from_address, channels->email.from, sizeof(email_config.from_address) - 1);
        strncpy(email_config.recipients, channels->email.to, sizeof(email_config.recipients) - 1);
        strncpy(email_config.username, channels->email.username, sizeof(email_config.username) - 1);
        strncpy(email_config.password, channels->email.password, sizeof(email_config.password) - 1);
        
        if (email_client_init(&email_client, &email_config) == 0) {
            if (email_client_send(&email_client, event) == 0) {
                success = true;
            }
            email_client_cleanup(&email_client);
        }
    }
    
    // Send to Slack if enabled
    if (channels->slack.enabled) {
        slack_client_t slack_client;
        slack_config_t slack_config = {
            .enabled = true,
            .timeout_seconds = 30,
            .retry_attempts = 3,
            .retry_delay_seconds = 5,
            .use_attachments = true,
            .include_context = true
        };
        strncpy(slack_config.webhook_url, channels->slack.webhook_url, sizeof(slack_config.webhook_url) - 1);
        strncpy(slack_config.channel, channels->slack.channel, sizeof(slack_config.channel) - 1);
        strncpy(slack_config.username, channels->slack.username, sizeof(slack_config.username) - 1);
        strncpy(slack_config.icon_emoji, channels->slack.icon_emoji, sizeof(slack_config.icon_emoji) - 1);
        strncpy(slack_config.icon_url, channels->slack.icon_url, sizeof(slack_config.icon_url) - 1);
        
        if (slack_client_init(&slack_client, &slack_config) == 0) {
            if (slack_client_send(&slack_client, event) == 0) {
                success = true;
            }
            slack_client_cleanup(&slack_client);
        }
    }
    
    // Send to Discord if enabled
    if (channels->discord.enabled) {
        discord_client_t discord_client;
        discord_config_t discord_config = {
            .enabled = true,
            .timeout_seconds = 30,
            .retry_attempts = 3,
            .retry_delay_seconds = 5,
            .use_embeds = true,
            .include_context = true
        };
        strncpy(discord_config.webhook_url, channels->discord.webhook_url, sizeof(discord_config.webhook_url) - 1);
        strncpy(discord_config.username, channels->discord.username, sizeof(discord_config.username) - 1);
        strncpy(discord_config.avatar_url, channels->discord.avatar_url, sizeof(discord_config.avatar_url) - 1);
        
        if (discord_client_init(&discord_client, &discord_config) == 0) {
            if (discord_client_send(&discord_client, event) == 0) {
                success = true;
            }
            discord_client_cleanup(&discord_client);
        }
    }
    
    // Send to Telegram if enabled
    if (channels->telegram.enabled) {
        telegram_client_t telegram_client;
        telegram_config_t telegram_config = {
            .enabled = true,
            .timeout_seconds = 30,
            .retry_attempts = 3,
            .retry_delay_seconds = 5,
            .use_markdown = true,
            .include_context = true
        };
        strncpy(telegram_config.token, channels->telegram.token, sizeof(telegram_config.token) - 1);
        strncpy(telegram_config.chat_id, channels->telegram.chat_id, sizeof(telegram_config.chat_id) - 1);
        
        if (telegram_client_init(&telegram_client, &telegram_config) == 0) {
            if (telegram_client_send(&telegram_client, event) == 0) {
                success = true;
            }
            telegram_client_cleanup(&telegram_client);
        }
    }
    
    // Send to webhook if enabled
    if (channels->webhook.enabled) {
        webhook_client_t webhook_client;
        webhook_config_t webhook_config = {
            .enabled = true,
            .auth_type = WEBHOOK_AUTH_NONE,
            .retry_attempts = 3,
            .retry_delay_seconds = 5,
            .timeout_seconds = 30,
            .verify_ssl = false,
            .follow_redirects = true
        };
        strncpy(webhook_config.url, channels->webhook.url, sizeof(webhook_config.url) - 1);
        strncpy(webhook_config.method, channels->webhook.method, sizeof(webhook_config.method) - 1);
        strncpy(webhook_config.content_type, channels->webhook.content_type, sizeof(webhook_config.content_type) - 1);
        
        if (webhook_client_init(&webhook_client, &webhook_config) == 0) {
            if (webhook_client_send(&webhook_client, event) == 0) {
                success = true;
            }
            webhook_client_cleanup(&webhook_client);
        }
    }
    
    // Send to SMS if enabled
    if (channels->sms.enabled) {
        sms_client_t sms_client;
        sms_config_t sms_config = {
            .enabled = true,
            .provider = SMS_PROVIDER_RUTOS_UBUS,
            .max_messages_per_hour = 10,
            .cooldown_period_seconds = 300,
            .timeout_seconds = 30,
            .retry_attempts = 3,
            .retry_delay_seconds = 5,
            .include_priority = true,
            .include_timestamp = true,
            .include_type = true,
            .max_message_length = 160
        };
        strncpy(sms_config.phone_number, channels->sms.from_number, sizeof(sms_config.phone_number) - 1);
        strncpy(sms_config.modem_path, "gsm.modem1", sizeof(sms_config.modem_path) - 1);
        
        if (sms_client_init(&sms_client, &sms_config) == 0) {
            if (sms_client_send(&sms_client, event) == 0) {
                success = true;
            }
            sms_client_cleanup(&sms_client);
        }
    }
    
    return success;
}

// Worker thread function
static void* notification_worker_thread(void* arg) {
    (void)arg; // Unused parameter
    
    while (g_notification_manager.worker_running) {
        priority_queue_item_t item;
        
        // Try to get next notification from queue
        if (priority_queue_pop(&g_notification_manager.priority_queue, &item) == 0) {
            // Check rate limiting
            if (adaptive_rate_limiter_allow_request(&g_notification_manager.rate_limiter, item.priority)) {
                // Send notification
                bool sent = send_notification_to_channels(&item.event);
                
                if (sent) {
                    adaptive_rate_limiter_record_success(&g_notification_manager.rate_limiter);
                    g_notification_manager.stats.sent_notifications++;
                } else {
                    adaptive_rate_limiter_record_failure(&g_notification_manager.rate_limiter);
                    g_notification_manager.stats.failed_notifications++;
                }
            } else {
                g_notification_manager.stats.rate_limited_notifications++;
            }
            
            // Clean up location data
            if (item.event.location) {
                free(item.event.location);
            }
        }
        
        // Sleep for a short time
        usleep(100000); // 100ms
    }
    
    return NULL;
}

// Start notification worker thread
int notification_manager_start_worker(void) {
    if (!g_manager_initialized || g_notification_manager.worker_running) {
        return -1;
    }
    
    pthread_mutex_lock(g_notification_manager.mutex);
    
    g_notification_manager.worker_running = true;
    
    if (pthread_create(&g_notification_manager.worker_thread, NULL, 
                      notification_worker_thread, NULL) != 0) {
        g_notification_manager.worker_running = false;
        pthread_mutex_unlock(g_notification_manager.mutex);
        return -1;
    }
    
    pthread_mutex_unlock(g_notification_manager.mutex);
    return 0;
}

// Stop notification worker thread
void notification_manager_stop_worker(void) {
    if (!g_manager_initialized || !g_notification_manager.worker_running) {
        return;
    }
    
    pthread_mutex_lock(g_notification_manager.mutex);
    
    g_notification_manager.worker_running = false;
    
    pthread_mutex_unlock(g_notification_manager.mutex);
    
    // Wait for worker thread to finish
    if (g_notification_manager.worker_thread != 0) {
        pthread_join(g_notification_manager.worker_thread, NULL);
        g_notification_manager.worker_thread = 0;
    }
}

// Send notification
int notification_manager_send(notification_type_t type, const char* title, const char* message,
                            notification_priority_t priority, const char* member_name) {
    if (!g_manager_initialized || !title || !message) {
        return -1;
    }
    
    // Check if we should send this type of notification
    if (!should_send_notification(type)) {
        return 0; // Not an error, just not configured to send
    }
    
    // Check rate limiting
    if (!adaptive_rate_limiter_allow_request(&g_notification_manager.rate_limiter, priority)) {
        g_notification_manager.stats.rate_limited_notifications++;
        return -1; // Rate limited
    }
    
    // Create notification event
    notification_event_t event;
    memset(&event, 0, sizeof(event));
    
    generate_notification_id(event.id, sizeof(event.id));
    strncpy(event.title, title, sizeof(event.title) - 1);
    strncpy(event.message, message, sizeof(event.message) - 1);
    event.type = type;
    event.priority = priority;
    event.timestamp = time(NULL);
    
    if (member_name) {
        strncpy(event.member_name, member_name, sizeof(event.member_name) - 1);
    }
    
    // Check for duplicates
    if (notification_deduplicator_is_duplicate(&g_notification_manager.deduplicator, &event)) {
        g_notification_manager.stats.duplicate_notifications++;
        return 0; // Duplicate, not an error
    }
    
    // Add to priority queue
    if (priority_queue_push(&g_notification_manager.priority_queue, &event, priority, event.timestamp) != 0) {
        return -1; // Queue full
    }
    
    g_notification_manager.stats.total_notifications++;
    
    return 0;
}

// Send notification with default priority
int notification_manager_send_default(notification_type_t type, const char* title, const char* message) {
    notification_priority_t priority = get_notification_priority(type);
    return notification_manager_send(type, title, message, priority, NULL);
}

// Get notification manager status
void notification_manager_get_status(notification_status_t* status) {
    if (!status || !g_manager_initialized) return;
    
    pthread_mutex_lock(g_notification_manager.mutex);
    
    status->enabled = g_notification_manager.config.pushover_enabled ||
                     g_notification_manager.config.notify_on_failover ||
                     g_notification_manager.config.notify_on_failback ||
                     g_notification_manager.config.notify_on_member_down ||
                     g_notification_manager.config.notify_on_member_up ||
                     g_notification_manager.config.notify_on_predictive ||
                     g_notification_manager.config.notify_on_critical ||
                     g_notification_manager.config.notify_on_recovery ||
                     g_notification_manager.config.notify_on_status_update;
    
    status->worker_running = g_notification_manager.worker_running;
    status->queue_size = priority_queue_size(&g_notification_manager.priority_queue);
    status->queue_empty = priority_queue_is_empty(&g_notification_manager.priority_queue);
    status->queue_full = priority_queue_is_full(&g_notification_manager.priority_queue);
    
    // Get rate limiter status
    rate_limiter_stats_t rate_stats;
    adaptive_rate_limiter_get_stats(&g_notification_manager.rate_limiter, &rate_stats);
    status->current_rate_limit = rate_stats.current_rate;
    status->rate_limiter_emergency_mode = adaptive_rate_limiter_is_emergency_mode(&g_notification_manager.rate_limiter);
    
    // Get deduplicator status
    deduplicator_stats_t dedup_stats;
    notification_deduplicator_get_stats(&g_notification_manager.deduplicator, &dedup_stats);
    status->deduplication_enabled = dedup_stats.deduplication_window > 0;
    status->duplicate_rate = dedup_stats.duplicate_rate;
    
    pthread_mutex_unlock(g_notification_manager.mutex);
}

// Get notification manager statistics
void notification_manager_get_stats(notification_stats_t* stats) {
    if (!stats || !g_manager_initialized) return;
    
    pthread_mutex_lock(g_notification_manager.mutex);
    
    *stats = g_notification_manager.stats;
    
    pthread_mutex_unlock(g_notification_manager.mutex);
}

// Reset notification manager statistics
void notification_manager_reset_stats(void) {
    if (!g_manager_initialized) return;
    
    pthread_mutex_lock(g_notification_manager.mutex);
    
    memset(&g_notification_manager.stats, 0, sizeof(notification_stats_t));
    
    pthread_mutex_unlock(g_notification_manager.mutex);
}

// Check if notification manager is initialized
bool notification_manager_is_initialized(void) {
    return g_manager_initialized;
}

// Get notification manager instance
notification_manager_t* notification_manager_get_instance(void) {
    return g_manager_initialized ? &g_notification_manager : NULL;
}
