#include "data_limit_notifications.h"
#include "smart_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

// Global data limit notification manager instance
static data_limit_notification_manager_t g_data_limit_manager;
static bool g_data_limit_manager_initialized = false;

// Initialize data limit notification manager
int data_limit_notification_manager_init(const data_limit_notification_config_t* config) {
    if (g_data_limit_manager_initialized) {
        return 0; // Already initialized
    }
    
    if (!config) {
        return -1;
    }
    
    memset(&g_data_limit_manager, 0, sizeof(data_limit_notification_manager_t));
    
    // Copy configuration
    g_data_limit_manager.config = *config;
    
    // Initialize mutex
    g_data_limit_manager.mutex = malloc(sizeof(pthread_mutex_t));
    if (!g_data_limit_manager.mutex) {
        return -1;
    }
    
    pthread_mutex_init(g_data_limit_manager.mutex, NULL);
    
    // Initialize daily usage trackers
    g_data_limit_manager.daily_usage_trackers = malloc(config->max_interfaces * sizeof(daily_usage_tracker_t));
    if (!g_data_limit_manager.daily_usage_trackers) {
        pthread_mutex_destroy(g_data_limit_manager.mutex);
        free(g_data_limit_manager.mutex);
        return -1;
    }
    
    g_data_limit_manager.max_interfaces = config->max_interfaces;
    g_data_limit_manager.tracked_interfaces_count = 0;
    
    // Initialize last notifications tracking
    g_data_limit_manager.last_notifications = malloc(config->max_last_notifications * sizeof(last_notification_record_t));
    if (!g_data_limit_manager.last_notifications) {
        free(g_data_limit_manager.daily_usage_trackers);
        pthread_mutex_destroy(g_data_limit_manager.mutex);
        free(g_data_limit_manager.mutex);
        return -1;
    }
    
    g_data_limit_manager.max_last_notifications = config->max_last_notifications;
    g_data_limit_manager.last_notifications_count = 0;
    
    g_data_limit_manager_initialized = true;
    return 0;
}

// Clean up data limit notification manager
void data_limit_notification_manager_cleanup(void) {
    if (!g_data_limit_manager_initialized) return;
    
    if (g_data_limit_manager.mutex) {
        pthread_mutex_destroy(g_data_limit_manager.mutex);
        free(g_data_limit_manager.mutex);
    }
    
    if (g_data_limit_manager.daily_usage_trackers) {
        free(g_data_limit_manager.daily_usage_trackers);
    }
    
    if (g_data_limit_manager.last_notifications) {
        free(g_data_limit_manager.last_notifications);
    }
    
    g_data_limit_manager.daily_usage_trackers = NULL;
    g_data_limit_manager.last_notifications = NULL;
    g_data_limit_manager.mutex = NULL;
    g_data_limit_manager.tracked_interfaces_count = 0;
    g_data_limit_manager.max_interfaces = 0; // Use configurable max interfaces
    g_data_limit_manager.last_notifications_count = 0;
    g_data_limit_manager.max_last_notifications = 0; // Use configurable max last notifications
    
    g_data_limit_manager_initialized = false;
}

// Check if enough time has passed since last notification
static bool should_send_notification(const char* notification_key, time_t cooldown_seconds) {
    if (!notification_key || !g_data_limit_manager_initialized) {
        return false;
    }
    
    pthread_mutex_lock(g_data_limit_manager.mutex);
    
    time_t now = time(NULL);
    bool should_send = true;
    
    // Check existing notifications
    for (int i = 0; i < g_data_limit_manager.last_notifications_count; i++) {
        last_notification_record_t* record = &g_data_limit_manager.last_notifications[i];
        if (strcmp(record->notification_key, notification_key) == 0) {
            if (now - record->last_sent_time < cooldown_seconds) {
                should_send = false;
            } else {
                record->last_sent_time = now;
            }
            pthread_mutex_unlock(g_data_limit_manager.mutex);
            return should_send;
        }
    }
    
    // Add new notification record
    if (g_data_limit_manager.last_notifications_count < g_data_limit_manager.max_last_notifications) {
        int index = g_data_limit_manager.last_notifications_count;
        strncpy(g_data_limit_manager.last_notifications[index].notification_key, 
                notification_key, sizeof(g_data_limit_manager.last_notifications[index].notification_key) - 1);
        g_data_limit_manager.last_notifications[index].last_sent_time = now;
        g_data_limit_manager.last_notifications_count++;
    } else {
        // Replace oldest record
        int oldest_index = 0;
        time_t oldest_time = g_data_limit_manager.last_notifications[0].last_sent_time;
        for (int i = 1; i < g_data_limit_manager.max_last_notifications; i++) {
            if (g_data_limit_manager.last_notifications[i].last_sent_time < oldest_time) {
                oldest_time = g_data_limit_manager.last_notifications[i].last_sent_time;
                oldest_index = i;
            }
        }
        
        strncpy(g_data_limit_manager.last_notifications[oldest_index].notification_key,
                notification_key, sizeof(g_data_limit_manager.last_notifications[oldest_index].notification_key) - 1);
        g_data_limit_manager.last_notifications[oldest_index].last_sent_time = now;
    }
    
    pthread_mutex_unlock(g_data_limit_manager.mutex);
    return true;
}

// Get or create daily usage tracker
static daily_usage_tracker_t* get_daily_usage_tracker(const char* interface_name, 
                                                     const data_limit_config_t* data_limit) {
    if (!interface_name || !data_limit || !g_data_limit_manager_initialized) {
        return NULL;
    }
    
    // Find existing tracker
    for (int i = 0; i < g_data_limit_manager.tracked_interfaces_count; i++) {
        daily_usage_tracker_t* tracker = &g_data_limit_manager.daily_usage_trackers[i];
        if (strcmp(tracker->interface_name, interface_name) == 0) {
            return tracker;
        }
    }
    
    // Create new tracker
    if (g_data_limit_manager.tracked_interfaces_count >= g_data_limit_manager.max_interfaces) {
        return NULL; // No space for more trackers
    }
    
    int index = g_data_limit_manager.tracked_interfaces_count;
    daily_usage_tracker_t* tracker = &g_data_limit_manager.daily_usage_trackers[index];
    
    strncpy(tracker->interface_name, interface_name, sizeof(tracker->interface_name) - 1);
    tracker->last_reset_date = time(NULL) - (30 * 24 * 60 * 60); // 30 days ago
    
    // Calculate daily allowance
    double remaining_mb = (double)data_limit->data_limit_mb - data_limit->current_usage_mb;
    tracker->daily_allowance_mb = remaining_mb / (double)data_limit->days_until_reset;
    if (tracker->daily_allowance_mb < 0) {
        tracker->daily_allowance_mb = 0;
    }
    
    tracker->today_usage_mb = 0;
    tracker->yesterday_usage_mb = 0;
    tracker->last_usage_check_mb = data_limit->current_usage_mb;
    tracker->last_usage_check_time = time(NULL);
    tracker->daily_warning_80_sent = false;
    tracker->daily_warning_100_sent = false;
    
    g_data_limit_manager.tracked_interfaces_count++;
    return tracker;
}

// Notify failover to limited connection
int data_limit_notification_manager_notify_failover_to_limited(const char* from_interface,
                                                             const char* to_interface,
                                                             const data_limit_config_t* data_limit) {
    if (!g_data_limit_manager_initialized || !from_interface || !to_interface || !data_limit) {
        return -1;
    }
    
    if (!should_send_notification("failover_to_limited", g_data_limit_manager.config.failover_cooldown_seconds)) {
        return 0; // Avoided spam
    }
    
    double remaining_mb = (double)data_limit->data_limit_mb - data_limit->current_usage_mb;
    double remaining_gb = remaining_mb / 1024.0;
    
    notification_event_t event;
    memset(&event, 0, sizeof(event));
    
    // Generate ID
    time_t now = time(NULL);
    snprintf(event.id, sizeof(event.id), "failover_%lld", (long long)now);
    
    // Set priority based on usage
    if (data_limit->usage_percentage > 80) {
        event.priority = NOTIFICATION_PRIORITY_HIGH;
        snprintf(event.title, sizeof(event.title), "  Failover to Data-Limited Connection");
    } else {
        event.priority = NOTIFICATION_PRIORITY_NORMAL;
        snprintf(event.title, sizeof(event.title), " Failover to Data-Limited Connection");
    }
    
    snprintf(event.message, sizeof(event.message),
             "Switched from %s to %s\n\n"
             " Data Status:\n"
             " Remaining: %.2f GB (%.1f%%)\n"
             " Used: %.2f GB of %d GB\n"
             " Resets in: %d days\n\n"
             " Monitor usage carefully!",
             from_interface, to_interface,
             remaining_gb, 100.0 - data_limit->usage_percentage,
             data_limit->current_usage_mb / 1024.0, data_limit->data_limit_mb / 1024,
             data_limit->days_until_reset);
    
    event.type = NOTIFICATION_TYPE_DATA_LIMIT;
    event.timestamp = now;
    
    // Send through smart manager if available
    if (smart_notification_manager_is_initialized()) {
        return smart_notification_manager_send(&event);
    } else {
        return smart_notification_manager_send(&event); 
    }
}

// Notify failback from limited connection
int data_limit_notification_manager_notify_failback_from_limited(const char* from_interface,
                                                                const char* to_interface,
                                                                const data_limit_config_t* data_limit) {
    if (!g_data_limit_manager_initialized || !from_interface || !to_interface || !data_limit) {
        return -1;
    }
    
    if (!should_send_notification("failback_from_limited", g_data_limit_manager.config.failover_cooldown_seconds)) {
        return 0; // Avoided spam
    }
    
    double remaining_mb = (double)data_limit->data_limit_mb - data_limit->current_usage_mb;
    double remaining_gb = remaining_mb / 1024.0;
    
    notification_event_t event;
    memset(&event, 0, sizeof(event));
    
    // Generate ID
    time_t now = time(NULL);
    snprintf(event.id, sizeof(event.id), "failback_%lld", (long long)now);
    
    snprintf(event.title, sizeof(event.title), " Failback to Unlimited Connection");
    snprintf(event.message, sizeof(event.message),
             "Switched back from %s to %s\n\n"
             " Final Data Usage:\n"
             " Remaining: %.2f GB (%.1f%%)\n"
             " Total used: %.2f GB of %d GB\n"
             " Resets in: %d days\n\n"
             " Back to unlimited connectivity!",
             from_interface, to_interface,
             remaining_gb, 100.0 - data_limit->usage_percentage,
             data_limit->current_usage_mb / 1024.0, data_limit->data_limit_mb / 1024,
             data_limit->days_until_reset);
    
    event.type = NOTIFICATION_TYPE_DATA_LIMIT;
    event.priority = NOTIFICATION_PRIORITY_NORMAL;
    event.timestamp = now;
    
    // Send through smart manager if available
    if (smart_notification_manager_is_initialized()) {
        return smart_notification_manager_send(&event);
    } else {
        return smart_notification_manager_send(&event); 
    }
}

// Notify daily usage threshold
int data_limit_notification_manager_notify_daily_usage_threshold(const char* interface_name,
                                                                const data_limit_config_t* data_limit,
                                                                int percentage) {
    if (!g_data_limit_manager_initialized || !interface_name || !data_limit) {
        return -1;
    }
    
    pthread_mutex_lock(g_data_limit_manager.mutex);
    daily_usage_tracker_t* tracker = get_daily_usage_tracker(interface_name, data_limit);
    if (!tracker) {
        pthread_mutex_unlock(g_data_limit_manager.mutex);
        return -1;
    }
    
    // Check if already sent
    if (percentage >= 100 && tracker->daily_warning_100_sent) {
        pthread_mutex_unlock(g_data_limit_manager.mutex);
        return 0; // Already sent
    } else if (percentage >= 80 && tracker->daily_warning_80_sent) {
        pthread_mutex_unlock(g_data_limit_manager.mutex);
        return 0; // Already sent
    }
    
    // Mark as sent
    if (percentage >= 100) {
        tracker->daily_warning_100_sent = true;
    } else {
        tracker->daily_warning_80_sent = true;
    }
    
    pthread_mutex_unlock(g_data_limit_manager.mutex);
    
    notification_event_t event;
    memset(&event, 0, sizeof(event));
    
    // Generate ID
    time_t now = time(NULL);
    snprintf(event.id, sizeof(event.id), "daily_%s_%d_%lld", interface_name, percentage, (long long)now);
    
    if (percentage >= 100) {
        snprintf(event.title, sizeof(event.title), " Daily Data Limit Exceeded!");
        event.priority = NOTIFICATION_PRIORITY_HIGH;
    } else {
        snprintf(event.title, sizeof(event.title), " Daily Data Usage: %d%%", percentage);
        event.priority = NOTIFICATION_PRIORITY_NORMAL;
    }
    
    double daily_usage_percent = (tracker->today_usage_mb / tracker->daily_allowance_mb) * 100.0;
    double remaining_daily = tracker->daily_allowance_mb - tracker->today_usage_mb;
    
    snprintf(event.message, sizeof(event.message),
             "Interface: %s\n\n"
             " Today's Usage:\n"
             " Used: %.2f MB (%.1f%%)\n"
             " Daily allowance: %.2f MB\n"
             " Remaining today: %.2f MB\n\n"
             " Monthly Status:\n"
             " Used: %.2f GB of %d GB\n"
             " Remaining: %.2f GB\n"
             " Resets in: %d days",
             interface_name,
             tracker->today_usage_mb, daily_usage_percent,
             tracker->daily_allowance_mb, remaining_daily,
             data_limit->current_usage_mb / 1024.0, data_limit->data_limit_mb / 1024,
             ((double)data_limit->data_limit_mb - data_limit->current_usage_mb) / 1024.0,
             data_limit->days_until_reset);
    
    event.type = NOTIFICATION_TYPE_DATA_LIMIT;
    event.timestamp = now;
    
    // Send through smart manager if available
    if (smart_notification_manager_is_initialized()) {
        return smart_notification_manager_send(&event);
    } else {
        return smart_notification_manager_send(&event); 
    }
}

// Notify monthly usage threshold
int data_limit_notification_manager_notify_monthly_usage_threshold(const char* interface_name,
                                                                  const data_limit_config_t* data_limit) {
    if (!g_data_limit_manager_initialized || !interface_name || !data_limit) {
        return -1;
    }
    
    char notification_key[256];
    notification_type_t notify_type;
    const char* title;
    const char* emoji;
    notification_priority_t priority = NOTIFICATION_PRIORITY_NORMAL;
    
    if (data_limit->usage_percentage >= 100.0) {
        notify_type = NOTIFICATION_TYPE_DATA_LIMIT;
        title = " Monthly Data Limit EXCEEDED!";
        emoji = "";
        priority = NOTIFICATION_PRIORITY_HIGH;
        snprintf(notification_key, sizeof(notification_key), "monthly_exceeded_%s", interface_name);
    } else if (data_limit->usage_percentage >= 95.0) {
        notify_type = NOTIFICATION_TYPE_DATA_LIMIT;
        title = " Monthly Data Limit: 95% Used";
        emoji = "";
        priority = NOTIFICATION_PRIORITY_HIGH;
        snprintf(notification_key, sizeof(notification_key), "monthly_95_%s", interface_name);
    } else if (data_limit->usage_percentage >= 80.0) {
        notify_type = NOTIFICATION_TYPE_DATA_LIMIT;
        title = " Monthly Data Limit: 80% Used";
        emoji = "";
        snprintf(notification_key, sizeof(notification_key), "monthly_80_%s", interface_name);
    } else {
        return 0; // No notification needed
    }
    
    if (!should_send_notification(notification_key, g_data_limit_manager.config.monthly_threshold_cooldown_seconds)) {
        return 0; // Avoid daily spam for same threshold
    }
    
    double remaining_mb = (double)data_limit->data_limit_mb - data_limit->current_usage_mb;
    double remaining_gb = remaining_mb / 1024.0;
    
    pthread_mutex_lock(g_data_limit_manager.mutex);
    daily_usage_tracker_t* tracker = get_daily_usage_tracker(interface_name, data_limit);
    pthread_mutex_unlock(g_data_limit_manager.mutex);
    
    notification_event_t event;
    memset(&event, 0, sizeof(event));
    
    // Generate ID
    time_t now = time(NULL);
    snprintf(event.id, sizeof(event.id), "monthly_%s_%.0f_%lld", interface_name, data_limit->usage_percentage, (long long)now);
    
    strncpy(event.title, title, sizeof(event.title) - 1);
    
    if (tracker) {
        snprintf(event.message, sizeof(event.message),
                 "Interface: %s\n\n"
                 "%s Monthly Status:\n"
                 " Used: %.2f GB of %d GB (%.1f%%)\n"
                 " Remaining: %.2f GB\n"
                 " Resets in: %d days\n\n"
                 " Daily Allowance:\n"
                 " Recommended: %.2f MB/day\n"
                 " Today used: %.2f MB\n\n"
                 " Consider switching to unlimited connection!",
                 interface_name, emoji,
                 data_limit->current_usage_mb / 1024.0, data_limit->data_limit_mb / 1024, data_limit->usage_percentage,
                 remaining_gb, data_limit->days_until_reset,
                 tracker->daily_allowance_mb, tracker->today_usage_mb);
    } else {
        snprintf(event.message, sizeof(event.message),
                 "Interface: %s\n\n"
                 "%s Monthly Status:\n"
                 " Used: %.2f GB of %d GB (%.1f%%)\n"
                 " Remaining: %.2f GB\n"
                 " Resets in: %d days\n\n"
                 " Consider switching to unlimited connection!",
                 interface_name, emoji,
                 data_limit->current_usage_mb / 1024.0, data_limit->data_limit_mb / 1024, data_limit->usage_percentage,
                 remaining_gb, data_limit->days_until_reset);
    }
    
    event.type = notify_type;
    event.priority = priority;
    event.timestamp = now;
    
    // Send through smart manager if available
    if (smart_notification_manager_is_initialized()) {
        return smart_notification_manager_send(&event);
    } else {
        return smart_notification_manager_send(&event); 
    }
}

// Notify unexpected usage spike
int data_limit_notification_manager_notify_unexpected_usage_spike(const char* interface_name,
                                                                 const data_limit_config_t* data_limit) {
    if (!g_data_limit_manager_initialized || !interface_name || !data_limit) {
        return -1;
    }
    
    pthread_mutex_lock(g_data_limit_manager.mutex);
    daily_usage_tracker_t* tracker = get_daily_usage_tracker(interface_name, data_limit);
    if (!tracker) {
        pthread_mutex_unlock(g_data_limit_manager.mutex);
        return -1;
    }
    
    // Calculate usage since last check
    double usage_since_last_check = data_limit->current_usage_mb - tracker->last_usage_check_mb;
    time_t now = time(NULL);
    double time_since_last_check_hours = (double)(now - tracker->last_usage_check_time) / 3600.0;
    
    if (time_since_last_check_hours < 1.0) {
        pthread_mutex_unlock(g_data_limit_manager.mutex);
        return 0; // Too soon to check
    }
    
    // Calculate hourly usage rate
    double hourly_usage_mb = usage_since_last_check / time_since_last_check_hours;
    double normal_hourly_usage = tracker->daily_allowance_mb / 24.0;
    
    // If hourly usage is more than 3x the normal daily allowance per hour, it's a spike
    bool is_spike = (hourly_usage_mb > normal_hourly_usage * 3.0) && (usage_since_last_check > 50.0);
    
    // Update tracking
    tracker->last_usage_check_mb = data_limit->current_usage_mb;
    tracker->last_usage_check_time = now;
    
    pthread_mutex_unlock(g_data_limit_manager.mutex);
    
    if (!is_spike) {
        return 0; // No spike detected
    }
    
    char notification_key[256];
    snprintf(notification_key, sizeof(notification_key), "usage_spike_%s", interface_name);
    
    if (!should_send_notification(notification_key, g_data_limit_manager.config.usage_spike_cooldown_seconds)) {
        return 0; // Avoid spam
    }
    
    notification_event_t event;
    memset(&event, 0, sizeof(event));
    
    // Generate ID
    snprintf(event.id, sizeof(event.id), "spike_%s_%lld", interface_name, (long long)now);
    
    snprintf(event.title, sizeof(event.title), " Unusual Data Usage Detected");
    snprintf(event.message, sizeof(event.message),
             "Interface: %s\n\n"
             " High usage detected:\n"
             " Last %.1f hours: %.2f MB\n"
             " Rate: %.2f MB/hour\n"
             " Normal rate: %.2f MB/hour\n\n"
             " Current Status:\n"
             " Monthly used: %.1f%%\n"
             " Remaining: %.2f GB\n"
             " Resets in: %d days\n\n"
             " Check for background downloads or streaming!",
             interface_name, time_since_last_check_hours, usage_since_last_check,
             hourly_usage_mb, normal_hourly_usage,
             data_limit->usage_percentage,
             ((double)data_limit->data_limit_mb - data_limit->current_usage_mb) / 1024.0,
             data_limit->days_until_reset);
    
    event.type = NOTIFICATION_TYPE_DATA_LIMIT;
    event.priority = NOTIFICATION_PRIORITY_HIGH;
    event.timestamp = now;
    
    // Send through smart manager if available
    if (smart_notification_manager_is_initialized()) {
        return smart_notification_manager_send(&event);
    } else {
        return smart_notification_manager_send(&event); 
    }
}

// Notify data limit reset
int data_limit_notification_manager_notify_data_limit_reset(const char* interface_name,
                                                           const data_limit_config_t* data_limit) {
    if (!g_data_limit_manager_initialized || !interface_name || !data_limit) {
        return -1;
    }
    
    notification_event_t event;
    memset(&event, 0, sizeof(event));
    
    // Generate ID
    time_t now = time(NULL);
    snprintf(event.id, sizeof(event.id), "reset_%s_%lld", interface_name, (long long)now);
    
    snprintf(event.title, sizeof(event.title), " Data Limit Reset");
    snprintf(event.message, sizeof(event.message),
             "Interface: %s\n\n"
             " Monthly data limit has reset!\n\n"
             " New Month:\n"
             " Limit: %d GB\n"
             " Used: %.2f MB\n"
             " Available: %.2f GB\n\n"
             " Daily Allowance:\n"
             " Recommended: %.2f MB/day\n\n"
             " Fresh start with full data allowance!",
             interface_name,
             data_limit->data_limit_mb / 1024,
             data_limit->current_usage_mb,
             (double)data_limit->data_limit_mb / 1024.0,
             (double)data_limit->data_limit_mb / (double)data_limit->days_until_reset);
    
    event.type = NOTIFICATION_TYPE_DATA_LIMIT;
    event.priority = NOTIFICATION_PRIORITY_NORMAL;
    event.timestamp = now;
    
    // Reset daily tracking
    pthread_mutex_lock(g_data_limit_manager.mutex);
    for (int i = 0; i < g_data_limit_manager.tracked_interfaces_count; i++) {
        daily_usage_tracker_t* tracker = &g_data_limit_manager.daily_usage_trackers[i];
        if (strcmp(tracker->interface_name, interface_name) == 0) {
            tracker->last_reset_date = now;
            tracker->daily_warning_80_sent = false;
            tracker->daily_warning_100_sent = false;
            tracker->today_usage_mb = 0;
            tracker->yesterday_usage_mb = 0;
            break;
        }
    }
    pthread_mutex_unlock(g_data_limit_manager.mutex);
    
    // Send through smart manager if available
    if (smart_notification_manager_is_initialized()) {
        return smart_notification_manager_send(&event);
    } else {
        return smart_notification_manager_send(&event); 
    }
}

// Update usage tracking
int data_limit_notification_manager_update_usage_tracking(const char* interface_name,
                                                         const data_limit_config_t* data_limit) {
    if (!g_data_limit_manager_initialized || !interface_name || !data_limit) {
        return -1;
    }
    
    pthread_mutex_lock(g_data_limit_manager.mutex);
    
    daily_usage_tracker_t* tracker = get_daily_usage_tracker(interface_name, data_limit);
    if (!tracker) {
        pthread_mutex_unlock(g_data_limit_manager.mutex);
        return -1;
    }
    
    time_t now = time(NULL);
    struct tm* now_tm = localtime(&now);
    struct tm* last_check_tm = localtime(&tracker->last_usage_check_time);
    
    // Check if it's a new day
    bool is_new_day = (now_tm->tm_year != last_check_tm->tm_year ||
                       now_tm->tm_mon != last_check_tm->tm_mon ||
                       now_tm->tm_mday != last_check_tm->tm_mday);
    
    if (is_new_day) {
        tracker->yesterday_usage_mb = tracker->today_usage_mb;
        tracker->today_usage_mb = 0;
        tracker->daily_warning_80_sent = false;
        tracker->daily_warning_100_sent = false;
    }
    
    // Calculate today's usage
    if (tracker->last_usage_check_mb > 0) {
        double usage_increase = data_limit->current_usage_mb - tracker->last_usage_check_mb;
        if (usage_increase > 0) {
            tracker->today_usage_mb += usage_increase;
        }
    }
    
    tracker->last_usage_check_mb = data_limit->current_usage_mb;
    tracker->last_usage_check_time = now;
    
    pthread_mutex_unlock(g_data_limit_manager.mutex);
    return 0;
}

// Check all data limit notifications
int data_limit_notification_manager_check_all_notifications(data_limit_config_t* data_limits,
                                                           int data_limits_count) {
    if (!g_data_limit_manager_initialized || !data_limits || data_limits_count <= 0) {
        return -1;
    }
    
    for (int i = 0; i < data_limits_count; i++) {
        data_limit_config_t* data_limit = &data_limits[i];
        
        if (!data_limit->enabled) {
            continue;
        }
        
        // Update usage tracking
        data_limit_notification_manager_update_usage_tracking(data_limit->interface_name, data_limit);
        
        // Check monthly thresholds
        data_limit_notification_manager_notify_monthly_usage_threshold(data_limit->interface_name, data_limit);
        
        // Check for usage spikes
        data_limit_notification_manager_notify_unexpected_usage_spike(data_limit->interface_name, data_limit);
        
        // Check daily thresholds
        pthread_mutex_lock(g_data_limit_manager.mutex);
        daily_usage_tracker_t* tracker = get_daily_usage_tracker(data_limit->interface_name, data_limit);
        if (tracker) {
            double daily_usage_percent = (tracker->today_usage_mb / tracker->daily_allowance_mb) * 100.0;
            
            if (daily_usage_percent >= 100.0 && !tracker->daily_warning_100_sent) {
                pthread_mutex_unlock(g_data_limit_manager.mutex);
                data_limit_notification_manager_notify_daily_usage_threshold(data_limit->interface_name, data_limit, 100);
                pthread_mutex_lock(g_data_limit_manager.mutex);
            } else if (daily_usage_percent >= 80.0 && !tracker->daily_warning_80_sent) {
                pthread_mutex_unlock(g_data_limit_manager.mutex);
                data_limit_notification_manager_notify_daily_usage_threshold(data_limit->interface_name, data_limit, 80);
                pthread_mutex_lock(g_data_limit_manager.mutex);
            }
        }
        pthread_mutex_unlock(g_data_limit_manager.mutex);
    }
    
    return 0;
}

// Get data limit notification manager status
void data_limit_notification_manager_get_status(data_limit_notification_status_t* status) {
    if (!status || !g_data_limit_manager_initialized) return;
    
    pthread_mutex_lock(g_data_limit_manager.mutex);
    
    status->enabled = g_data_limit_manager.config.enabled;
    status->tracked_interfaces_count = g_data_limit_manager.tracked_interfaces_count;
    status->max_interfaces = g_data_limit_manager.max_interfaces;
    status->last_notifications_count = g_data_limit_manager.last_notifications_count;
    status->max_last_notifications = g_data_limit_manager.max_last_notifications;
    
    pthread_mutex_unlock(g_data_limit_manager.mutex);
}

// Check if data limit notification manager is initialized
bool data_limit_notification_manager_is_initialized(void) {
    return g_data_limit_manager_initialized;
}

// Get data limit notification manager instance
data_limit_notification_manager_t* data_limit_notification_manager_get_instance(void) {
    return g_data_limit_manager_initialized ? &g_data_limit_manager : NULL;
}