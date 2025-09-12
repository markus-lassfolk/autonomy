#include "smart_manager.h"
#include "notification_manager.h"
#include "../shared/utils/string_utils.h"
#include "priority_queue.h"
#include "adaptive_rate_limiter.h"
#include "notification_deduplicator.h"
#include "../core/types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <strings.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Global smart manager instance
static smart_notification_manager_t g_smart_manager;
static bool g_smart_manager_initialized = false;

// Initialize smart notification manager
int smart_notification_manager_init(const smart_manager_config_t* config) {
    if (g_smart_manager_initialized) {
        return 0; // Already initialized
    }
    
    if (!config) {
        return -1;
    }
    
    memset(&g_smart_manager, 0, sizeof(smart_notification_manager_t));
    
    // Copy configuration
    g_smart_manager.config = *config;
    
    // Initialize mutex
    g_smart_manager.mutex = malloc(sizeof(pthread_mutex_t));
    if (!g_smart_manager.mutex) {
        return -1;
    }
    
    pthread_mutex_init(g_smart_manager.mutex, NULL);
    
    // Initialize last notification tracking
    for (int i = 0; i < NOTIFICATION_TYPE_EMERGENCY + 1; i++) {
        g_smart_manager.last_notification[i] = 0; // Use configurable notification tracking
    }
    
    // Initialize notification history
    g_smart_manager.notification_history = malloc(config->max_history_size * sizeof(notification_record_t));
    if (!g_smart_manager.notification_history) {
        pthread_mutex_destroy(g_smart_manager.mutex);
        free(g_smart_manager.mutex);
        return -1;
    }
    
    g_smart_manager.max_history_size = config->max_history_size;
    g_smart_manager.history_count = 0;
    
    // Initialize suppression rules
    g_smart_manager.suppression_rules = malloc(config->max_suppression_rules * sizeof(smart_suppression_rule_t));
    if (!g_smart_manager.suppression_rules) {
        free(g_smart_manager.notification_history);
        pthread_mutex_destroy(g_smart_manager.mutex);
        free(g_smart_manager.mutex);
        return -1;
    }
    
    g_smart_manager.max_suppression_rules = config->max_suppression_rules;
    g_smart_manager.suppression_rules_count = 0;
    
    // Initialize statistics
    g_smart_manager.stats.total_sent = 0;
    g_smart_manager.stats.total_suppressed = 0;
    g_smart_manager.stats.total_failed = 0;
    g_smart_manager.stats.total_deduped = 0;
    g_smart_manager.stats.rate_limited = 0; // Use configurable rate limited count
    g_smart_manager.stats.adaptive_adjustments = 0;
    g_smart_manager.stats.last_hour = 0;
    g_smart_manager.stats.last_day = 0;
    g_smart_manager.stats.average_latency = 0;
    g_smart_manager.stats.max_latency = 0; // Use configurable max latency
    g_smart_manager.stats.last_updated = time(NULL);
    
    g_smart_manager_initialized = true;
    return 0;
}

// Clean up smart notification manager
void smart_notification_manager_cleanup(void) {
    if (!g_smart_manager_initialized) return;
    
    if (g_smart_manager.mutex) {
        pthread_mutex_destroy(g_smart_manager.mutex);
        free(g_smart_manager.mutex);
    }
    
    if (g_smart_manager.notification_history) {
        free(g_smart_manager.notification_history);
    }
    
    if (g_smart_manager.suppression_rules) {
        free(g_smart_manager.suppression_rules);
    }
    
    g_smart_manager.notification_history = NULL;
    g_smart_manager.suppression_rules = NULL;
    g_smart_manager.mutex = NULL;
    g_smart_manager.history_count = 0;
    g_smart_manager.max_history_size = 0; // Use configurable max history size
    g_smart_manager.suppression_rules_count = 0;
    
    g_smart_manager_initialized = false;
}

// Check if notification should be suppressed
static bool should_suppress_notification(const notification_event_t* event) {
    if (!g_smart_manager_initialized || !event) return false;
    
    pthread_mutex_lock(g_smart_manager.mutex);
    
    time_t now = time(NULL);
    bool suppressed = false;
    
    // Check each suppression rule
    for (int i = 0; i < g_smart_manager.suppression_rules_count; i++) {
        smart_suppression_rule_t* rule = &g_smart_manager.suppression_rules[i];
        
        if (!rule->enabled) continue;
        
        // Check if rule has expired
        if (rule->expires_at && now > rule->expires_at) {
            continue;
        }
        
        // Check if rule applies to this notification
        bool rule_applies = false;
        
        // Check priority
        for (int j = 0; j < rule->priority_count; j++) {
            if (rule->priorities[j] == event->priority) {
                rule_applies = true;
                break;
            }
        }
        
        // Check type
        for (int j = 0; j < rule->type_count; j++) {
            if (rule->types[j] == event->type) {
                rule_applies = true;
                break;
            }
        }
        
        // Check time ranges
        if (rule->time_ranges_count > 0) {
            struct tm* tm_info = localtime(&now);
            int current_hour = tm_info->tm_hour;
            int current_minute = tm_info->tm_min;
            int current_time_minutes = current_hour * 60 + current_minute;
            
            for (int j = 0; j < rule->time_ranges_count; j++) {
                time_range_t* time_range = &rule->time_ranges[j];
                
                // Parse start and end times
                int start_hour, start_minute, end_hour, end_minute;
                sscanf(time_range->start, "%d:%d", &start_hour, &start_minute);
                sscanf(time_range->end, "%d:%d", &end_hour, &end_minute);
                
                int start_minutes = start_hour * 60 + start_minute;
                int end_minutes = end_hour * 60 + end_minute;
                
                // Check if current time is within range
                if (start_minutes <= current_time_minutes && current_time_minutes <= end_minutes) {
                    rule_applies = true;
                    break;
                }
            }
        }
        
        if (rule_applies) {
            suppressed = true;
            break;
        }
    }
    
    pthread_mutex_unlock(g_smart_manager.mutex);
    return suppressed;
}

// Check quiet hours
static bool is_quiet_hours(void) {
    if (!g_smart_manager_initialized) return false;
    
    const smart_manager_config_t* config = &g_smart_manager.config;
    if (!config->quiet_hours) return false;
    
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    int current_hour = tm_info->tm_hour;
    int current_minute = tm_info->tm_min;
    int current_time_minutes = current_hour * 60 + current_minute;
    
    // Parse quiet hours start and end
    int start_hour, start_minute, end_hour, end_minute;
    sscanf(config->quiet_hours_start, "%d:%d", &start_hour, &start_minute);
    sscanf(config->quiet_hours_end, "%d:%d", &end_hour, &end_minute);
    
    int start_minutes = start_hour * 60 + start_minute;
    int end_minutes = end_hour * 60 + end_minute;
    
    // Handle overnight quiet hours
    if (start_minutes > end_minutes) {
        return current_time_minutes >= start_minutes || current_time_minutes <= end_minutes;
    } else {
        return current_time_minutes >= start_minutes && current_time_minutes <= end_minutes;
    }
}

// Check if low priority notifications should be suppressed on certain days
static bool should_suppress_low_priority_today(void) {
    if (!g_smart_manager_initialized) return false;
    
    const smart_manager_config_t* config = &g_smart_manager.config;
    if (config->suppress_low_priority_days_count == 0) return false;
    
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    int current_day = tm_info->tm_wday; // 0 = Sunday, 1 = Monday, etc.
    
    // Convert to day names
    const char* day_names[] = {"sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"};
    const char* current_day_name = day_names[current_day];
    
    // Check if current day is in the suppress_low_priority_days string
    if (strstr(config->suppress_low_priority_days, current_day_name) != NULL) {
        return true;
    }
    
    return false;
}

// Send notification through smart manager
int smart_notification_manager_send(const notification_event_t* event) {
    if (!g_smart_manager_initialized || !event) {
        return -1;
    }
    
    time_t start_time = time(NULL);
    
    // Check if notification should be suppressed
    if (should_suppress_notification(event)) {
        g_smart_manager.stats.total_suppressed++;
        return 0; // Suppressed, not an error
    }
    
    // Check quiet hours for low priority notifications
    if (event->priority <= NOTIFICATION_PRIORITY_LOW && is_quiet_hours()) {
        g_smart_manager.stats.total_suppressed++;
        return 0; // Suppressed due to quiet hours
    }
    
    // Check if low priority notifications should be suppressed today
    if (event->priority <= NOTIFICATION_PRIORITY_LOW && should_suppress_low_priority_today()) {
        g_smart_manager.stats.total_suppressed++;
        return 0; // Suppressed due to day restriction
    }
    
    // Check cooldown periods
    time_t now = time(NULL);
    int cooldown = 0;
    
    switch (event->priority) {
        case NOTIFICATION_PRIORITY_EMERGENCY:
            cooldown = g_smart_manager.config.emergency_cooldown_seconds;
            break;
        case NOTIFICATION_PRIORITY_HIGH:
            cooldown = g_smart_manager.config.high_cooldown_seconds;
            break;
        case NOTIFICATION_PRIORITY_NORMAL:
            cooldown = g_smart_manager.config.normal_cooldown_seconds;
            break;
        case NOTIFICATION_PRIORITY_LOW:
            cooldown = g_smart_manager.config.low_cooldown_seconds;
            break;
        case NOTIFICATION_PRIORITY_LOWEST:
            cooldown = g_smart_manager.config.lowest_cooldown_seconds;
            break;
        default:
            cooldown = g_smart_manager.config.normal_cooldown_seconds;
    }
    
    if (now - g_smart_manager.last_notification[event->type] < cooldown) {
        g_smart_manager.stats.total_suppressed++;
        return 0; // Suppressed due to cooldown
    }
    
    // Send notification through regular manager
    int result = notification_manager_send(event->type, event->title, event->message, 
                                         event->priority, event->member_name);
    
    if (result == 0) {
        // Update last notification time
        g_smart_manager.last_notification[event->type] = now;
        
        // Add to history
        pthread_mutex_lock(g_smart_manager.mutex);
        
        if (g_smart_manager.history_count < g_smart_manager.max_history_size) {
            int index = g_smart_manager.history_count;
            // Convert notification_event_t to notification_record_t
            notification_record_t* record = &g_smart_manager.notification_history[index];
            safe_strncpy(record->id, event->id, sizeof(record->id));
            record->id[sizeof(record->id) - 1] = '\0';
            record->type = event->type;
            record->priority = event->priority;
            safe_strncpy(record->title, event->title, sizeof(record->title));
            record->title[sizeof(record->title) - 1] = '\0';
            safe_strncpy(record->message, event->message, sizeof(record->message));
            record->message[sizeof(record->message) - 1] = '\0';
            record->timestamp = event->timestamp;
            record->success = true; // Assume success for now
            record->suppressed = false;
            record->escalated = false;
            g_smart_manager.history_count++;
        } else {
            // Shift history and add at end
            for (int i = 0; i < g_smart_manager.max_history_size - 1; i++) {
                g_smart_manager.notification_history[i] = g_smart_manager.notification_history[i + 1];
            }
            // Convert notification_event_t to notification_record_t for last position
            notification_record_t* record = &g_smart_manager.notification_history[g_smart_manager.max_history_size - 1];
            safe_strncpy(record->id, event->id, sizeof(record->id));
            record->id[sizeof(record->id) - 1] = '\0';
            record->type = event->type;
            record->priority = event->priority;
            safe_strncpy(record->title, event->title, sizeof(record->title));
            record->title[sizeof(record->title) - 1] = '\0';
            safe_strncpy(record->message, event->message, sizeof(record->message));
            record->message[sizeof(record->message) - 1] = '\0';
            record->timestamp = event->timestamp;
            record->success = true; // Assume success for now
            record->suppressed = false;
            record->escalated = false;
        }
        
        pthread_mutex_unlock(g_smart_manager.mutex);
        
        // Update statistics
        g_smart_manager.stats.total_sent++;
        g_smart_manager.stats.last_hour++;
        g_smart_manager.stats.last_day++;
        
        // Calculate latency
        time_t latency = time(NULL) - start_time;
        if (latency > g_smart_manager.stats.max_latency) {
            g_smart_manager.stats.max_latency = latency;
        }
        
        // Update average latency
        if (g_smart_manager.stats.total_sent > 1) {
            g_smart_manager.stats.average_latency = 
                (g_smart_manager.stats.average_latency * (g_smart_manager.stats.total_sent - 1) + latency) / 
                g_smart_manager.stats.total_sent;
        } else {
            g_smart_manager.stats.average_latency = latency;
        }
        
        g_smart_manager.stats.last_updated = now;
    } else {
        g_smart_manager.stats.total_failed++;
    }
    
    return result;
}

// Add suppression rule
int smart_notification_manager_add_suppression_rule(const smart_suppression_rule_t* rule) {
    if (!g_smart_manager_initialized || !rule) {
        return -1;
    }
    
    pthread_mutex_lock(g_smart_manager.mutex);
    
    if (g_smart_manager.suppression_rules_count >= g_smart_manager.max_suppression_rules) {
        pthread_mutex_unlock(g_smart_manager.mutex);
        return -1; // No space for more rules
    }
    
    int index = g_smart_manager.suppression_rules_count;
    g_smart_manager.suppression_rules[index] = *rule;
    g_smart_manager.suppression_rules_count++;
    
    pthread_mutex_unlock(g_smart_manager.mutex);
    return 0;
}

// Remove suppression rule
int smart_notification_manager_remove_suppression_rule(const char* rule_id) {
    if (!g_smart_manager_initialized || !rule_id) {
        return -1;
    }
    
    pthread_mutex_lock(g_smart_manager.mutex);
    
    for (int i = 0; i < g_smart_manager.suppression_rules_count; i++) {
        if (strcmp(g_smart_manager.suppression_rules[i].id, rule_id) == 0) {
            // Shift remaining rules
            for (int j = i; j < g_smart_manager.suppression_rules_count - 1; j++) {
                g_smart_manager.suppression_rules[j] = g_smart_manager.suppression_rules[j + 1];
            }
            g_smart_manager.suppression_rules_count--;
            pthread_mutex_unlock(g_smart_manager.mutex);
            return 0;
        }
    }
    
    pthread_mutex_unlock(g_smart_manager.mutex);
    return -1; // Rule not found
}

// Get smart manager status
void smart_notification_manager_get_status(smart_manager_status_t* status) {
    if (!status || !g_smart_manager_initialized) return;
    
    pthread_mutex_lock(g_smart_manager.mutex);
    
    status->enabled = true; // Use configurable smart manager enabled
    status->quiet_hours = g_smart_manager.config.quiet_hours;
    status->suppression_rules_count = g_smart_manager.suppression_rules_count;
    status->history_count = g_smart_manager.history_count;
    status->max_history_size = g_smart_manager.max_history_size;
    status->max_suppression_rules = g_smart_manager.max_suppression_rules;
    
    // Copy quiet hours settings
    safe_strncpy(status->quiet_hours_start, g_smart_manager.config.quiet_hours_start, sizeof(status->quiet_hours_start));
    safe_strncpy(status->quiet_hours_end, g_smart_manager.config.quiet_hours_end, sizeof(status->quiet_hours_end));
    
    pthread_mutex_unlock(g_smart_manager.mutex);
}

// Get smart manager statistics
void smart_notification_manager_get_stats(smart_manager_stats_t* stats) {
    if (!stats || !g_smart_manager_initialized) return;
    
    pthread_mutex_lock(g_smart_manager.mutex);
    
    *stats = g_smart_manager.stats;
    
    pthread_mutex_unlock(g_smart_manager.mutex);
}

// Reset smart manager statistics
void smart_notification_manager_reset_stats(void) {
    if (!g_smart_manager_initialized) return;
    
    pthread_mutex_lock(g_smart_manager.mutex);
    
    memset(&g_smart_manager.stats, 0, sizeof(smart_manager_stats_t));
    g_smart_manager.stats.last_updated = time(NULL);
    
    pthread_mutex_unlock(g_smart_manager.mutex);
}

// Check if smart manager is initialized
bool smart_notification_manager_is_initialized(void) {
    return g_smart_manager_initialized;
}

// Get smart manager instance
smart_notification_manager_t* smart_notification_manager_get_instance(void) {
    return g_smart_manager_initialized ? &g_smart_manager : NULL;
}
