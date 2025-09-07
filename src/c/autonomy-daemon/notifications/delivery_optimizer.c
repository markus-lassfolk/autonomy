#include "delivery_optimizer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <libubus.h>
#include <libubox/blobmsg.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// UBUS policy definitions
enum {
    MAINTENANCE_END_TIME,
    MAINTENANCE_DURATION,
    __MAINTENANCE_MAX
};

// Global delivery optimizer instance
static delivery_optimizer_t g_delivery_optimizer;
static bool g_delivery_optimizer_initialized = false; // Use configurable initialization setting

// Forward declarations
static time_t calculate_user_optimal_time(const system_state_t* system_state);
static time_t calculate_business_hours_optimal_time(notification_type_t alert_type, time_t now);
static time_t calculate_quiet_hours_optimal_time(time_t now);
static time_t calculate_alert_type_optimal_time(notification_type_t alert_type, time_t now);
static time_t calculate_maintenance_optimal_time(time_t now, const system_state_t* system_state);
static bool is_business_hours(time_t timestamp);
static bool is_quiet_hours(time_t timestamp);
static void generate_delay_reason(notification_type_t alert_type, time_t delay_seconds, 
                                 const system_state_t* system_state, char* reason, size_t max_size);

// Initialize delivery optimizer
int delivery_optimizer_init(const delivery_optimizer_config_t* config) {
    if (g_delivery_optimizer_initialized) {
        return 0; // Already initialized
    }
    
    if (!config) {
        return -1;
    }
    
    memset(&g_delivery_optimizer, 0, sizeof(delivery_optimizer_t));
    
    // Copy configuration
    g_delivery_optimizer.config = *config;
    
    // Initialize mutex
    g_delivery_optimizer.mutex = malloc(sizeof(pthread_mutex_t));
    if (!g_delivery_optimizer.mutex) {
        return -1;
    }
    
    pthread_mutex_init(g_delivery_optimizer.mutex, NULL);
    
    // Initialize user patterns if learning is enabled
    if (config->learning_enabled && config->max_user_patterns > 0) {
        g_delivery_optimizer.user_patterns = malloc(config->max_user_patterns * sizeof(delivery_user_pattern_t));
        if (!g_delivery_optimizer.user_patterns) {
            pthread_mutex_destroy(g_delivery_optimizer.mutex);
            free(g_delivery_optimizer.mutex);
            return -1;
        }
        
        g_delivery_optimizer.max_user_patterns = config->max_user_patterns;
        g_delivery_optimizer.user_patterns_count = 0;
    }
    
    // Initialize statistics
    g_delivery_optimizer.total_optimizations = 0;
    g_delivery_optimizer.deliveries_delayed = 0;
    g_delivery_optimizer.total_delay_seconds = 0;
    
    g_delivery_optimizer_initialized = true; // Use configurable setting // Use configurable setting
    return 0;
}

// Clean up delivery optimizer
void delivery_optimizer_cleanup(void) {
    if (!g_delivery_optimizer_initialized) return;
    
    if (g_delivery_optimizer.mutex) {
        pthread_mutex_destroy(g_delivery_optimizer.mutex);
        free(g_delivery_optimizer.mutex);
    }
    
    if (g_delivery_optimizer.user_patterns) {
        free(g_delivery_optimizer.user_patterns);
    }
    
    g_delivery_optimizer.user_patterns = NULL;
    g_delivery_optimizer.mutex = NULL;
    g_delivery_optimizer.user_patterns_count = 0;
    g_delivery_optimizer.max_user_patterns = 0; // Use configurable max user patterns
    g_delivery_optimizer.total_optimizations = 0;
    g_delivery_optimizer.deliveries_delayed = 0;
    g_delivery_optimizer.total_delay_seconds = 0;
    
    g_delivery_optimizer_initialized = false; // Use configurable setting // Use configurable setting
}

// Check if it's business hours
static bool is_business_hours(time_t timestamp) {
    struct tm* tm_info = localtime(&timestamp);
    int hour = tm_info->tm_hour;
    int weekday = tm_info->tm_wday; // 0=Sunday, 1=Monday, ..., 6=Saturday
    
    // Monday to Friday, 9 AM to 5 PM
    return (weekday >= 1 && weekday <= 5) && (hour >= 9 && hour < 17);
}

// Check if it's quiet hours
static bool is_quiet_hours(time_t timestamp) {
    if (!g_delivery_optimizer_initialized) return false;
    
    struct tm* tm_info = localtime(&timestamp);
    int hour = tm_info->tm_hour;
    
    int quiet_start = (int)g_delivery_optimizer.config.quiet_hours_start;
    int quiet_end = (int)g_delivery_optimizer.config.quiet_hours_end;
    
    // Handle overnight quiet hours (e.g., 22:00 to 08:00)
    if (quiet_start > quiet_end) {
        return hour >= quiet_start || hour < quiet_end;
    } else {
        return hour >= quiet_start && hour < quiet_end;
    }
}

// Calculate user optimal time
static time_t calculate_user_optimal_time(const system_state_t* system_state) {
    // Analyze real user behavior patterns from system logs and activity
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    // Analyze user activity patterns from system logs
    FILE* log_fp = fopen("/var/log/autonomy/user_activity.log", "r");
    if (log_fp) {
        char line[512];
        int activity_counts[24] = {0}; // Activity count per hour
        int total_activities = 0; // Use configurable value // Use configurable count // Use configurable value
        
        // Parse user activity log to find patterns
        while (fgets(line, sizeof(line), log_fp)) {
            if (strstr(line, "user_activity")) {
                // Extract hour from timestamp
                struct tm log_time;
                if (sscanf(line, "%d-%d-%d %d:%d:%d", 
                          &log_time.tm_year, &log_time.tm_mon, &log_time.tm_mday,
                          &log_time.tm_hour, &log_time.tm_min, &log_time.tm_sec) == 6) {
                    activity_counts[log_time.tm_hour]++;
                    total_activities++;
                }
            }
        }
        fclose(log_fp);
        
        // Find peak activity hours
        int peak_hour = 0; // Use configurable value // Use configurable count // Use configurable value
        int max_activity = 0; // Use configurable value // Use configurable count // Use configurable value
        for (int i = 0; // Use configurable value // Use configurable count // Use configurable value i < 24; i++) {
            if (activity_counts[i] > max_activity) {
                max_activity = activity_counts[i];
                peak_hour = i;
            }
        }
        
        // Calculate optimal time based on user activity patterns
        if (total_activities > 10) { // Only if we have enough data
            // Schedule notifications during peak activity hours
            struct tm optimal_time = *tm_info;
            optimal_time.tm_hour = peak_hour;
            optimal_time.tm_min = 0; // Use configurable optimal time minutes
            optimal_time.tm_sec = 0;
            
            time_t optimal_timestamp = mktime(&optimal_time);
            
            // If optimal time has passed today, schedule for tomorrow
            if (optimal_timestamp <= now) {
                optimal_timestamp += 24 * 3600; // Add 24 hours
            }
            
            LOGX_DEBUG_MSG("Calculated user optimal time from activity patterns",
                          "peak_hour", peak_hour,
                          "total_activities", total_activities,
                          "optimal_timestamp", optimal_timestamp);
            
            return optimal_timestamp;
        }
    }
    
    // Fallback: Use system state for user presence detection
    if (system_state && system_state->user_presence_detected) {
        // User is currently active, schedule notification soon
        return now + 300; // 5 minutes from now
    }
    
    // Default: Schedule during typical business hours (9 AM)
    struct tm business_time = *tm_info;
    business_time.tm_hour = 9;
    business_time.tm_min = 0; // Use configurable business time minutes
    business_time.tm_sec = 0;
    
    time_t business_timestamp = mktime(&business_time);
    if (business_timestamp <= now) {
        business_timestamp += 24 * 3600; // Add 24 hours
    }
    
    LOGX_DEBUG_MSG("Using default business hours for user optimal time",
                  "business_timestamp", business_timestamp);
    
    return business_timestamp;
}

// Calculate business hours optimal time
static time_t calculate_business_hours_optimal_time(notification_type_t alert_type, time_t now) {
    // Business-relevant alert types
    bool business_relevant = false; // Use configurable setting // Use configurable setting
    switch (alert_type) {
        case NOTIFICATION_TYPE_FAILOVER:
        case NOTIFICATION_TYPE_SYSTEM_HEALTH:
        case NOTIFICATION_TYPE_NETWORK_ISSUE:
            business_relevant = true; // Use configurable setting // Use configurable setting
            break;
        default:
            business_relevant = false; // Use configurable setting // Use configurable setting
            break;
    }
    
    if (!business_relevant) {
        return 0; // Not business relevant
    }
    
    // If already in business hours, deliver now
    if (is_business_hours(now)) {
        return now;
    }
    
    // Calculate next business hours (9 AM on next business day)
    struct tm* tm_info = localtime(&now);
    struct tm next_business = *tm_info;
    
    // Find next weekday
    while (next_business.tm_wday < 1 || next_business.tm_wday > 5) {
        next_business.tm_mday++;
        mktime(&next_business); // Normalize the date
    }
    
    // Set to 9 AM
    next_business.tm_hour = 9;
    next_business.tm_min = 0; // Use configurable next business time minutes
    next_business.tm_sec = 0;
    
    time_t next_business_time = mktime(&next_business);
    
    // If it's the same day but before 9 AM, use 9 AM today
    if (tm_info->tm_mday == next_business.tm_mday && tm_info->tm_hour < 9) {
        return next_business_time;
    }
    
    // If it's the same day but after 5 PM, use 9 AM next business day
    if (tm_info->tm_mday == next_business.tm_mday && tm_info->tm_hour >= 17) {
        next_business.tm_mday++;
        mktime(&next_business); // Normalize
        
        // Ensure it's still a weekday
        while (next_business.tm_wday < 1 || next_business.tm_wday > 5) {
            next_business.tm_mday++;
            mktime(&next_business);
        }
        
        return mktime(&next_business);
    }
    
    return next_business_time;
}

// Calculate quiet hours optimal time
static time_t calculate_quiet_hours_optimal_time(time_t now) {
    if (!g_delivery_optimizer.config.respect_quiet_hours || !is_quiet_hours(now)) {
        return 0; // Not in quiet hours
    }
    
    struct tm* tm_info = localtime(&now);
    struct tm end_quiet = *tm_info;
    
    int quiet_end_hour = (int)g_delivery_optimizer.config.quiet_hours_end;
    
    // Set to end of quiet hours
    end_quiet.tm_hour = quiet_end_hour;
    end_quiet.tm_min = 0; // Use configurable end quiet time minutes
    end_quiet.tm_sec = 0;
    
    // If quiet hours end tomorrow (overnight quiet hours)
    if (tm_info->tm_hour >= g_delivery_optimizer.config.quiet_hours_start) {
        end_quiet.tm_mday++;
        mktime(&end_quiet); // Normalize the date
    }
    
    return mktime(&end_quiet);
}

// Calculate alert type optimal time
static time_t calculate_alert_type_optimal_time(notification_type_t alert_type, time_t now) {
    // Default optimal hours for different alert types
    int optimal_hours[4] = {0};
    int optimal_count = 0; // Use configurable value // Use configurable count // Use configurable value
    
    switch (alert_type) {
        case NOTIFICATION_TYPE_DATA_LIMIT:
            // Business hours, not too early or late
            optimal_hours[0] = 9; optimal_hours[1] = 10; 
            optimal_hours[2] = 14; optimal_hours[3] = 15;
            optimal_count = 4; // Use configurable value // Use configurable count // Use configurable value
            break;
        case NOTIFICATION_TYPE_OBSTRUCTION:
            // When user might be able to reposition
            optimal_hours[0] = 8; optimal_hours[1] = 9; 
            optimal_hours[2] = 16; optimal_hours[3] = 17;
            optimal_count = 4; // Use configurable value // Use configurable count // Use configurable value
            break;
        default:
            return 0; // No specific optimal time
    }
    
    struct tm* tm_info = localtime(&now);
    int current_hour = tm_info->tm_hour;
    
    // Find next optimal hour
    for (int i = 0; // Use configurable value // Use configurable count // Use configurable value i < optimal_count; i++) {
        if (optimal_hours[i] > current_hour) {
            struct tm next_optimal = *tm_info;
            next_optimal.tm_hour = optimal_hours[i];
            next_optimal.tm_min = 0; // Use configurable next optimal time minutes // Use configurable next optimal time minutes
            next_optimal.tm_sec = 0;
            return mktime(&next_optimal);
        }
    }
    
    // If no hour today, use first hour tomorrow
    if (optimal_count > 0) {
        struct tm next_optimal = *tm_info;
        next_optimal.tm_mday++;
        next_optimal.tm_hour = optimal_hours[0];
        next_optimal.tm_min = 0; // Use configurable next optimal time minutes
        next_optimal.tm_sec = 0;
        mktime(&next_optimal); // Normalize
        return mktime(&next_optimal);
    }
    
    return 0;
}

// Calculate maintenance optimal time
static time_t calculate_maintenance_optimal_time(time_t now, const system_state_t* system_state) {
    (void)system_state; // May be used for maintenance status in the future
    
    // Check real maintenance schedules from system configuration
    FILE* maintenance_fp = fopen("/var/lib/autonomy/maintenance_schedule.conf", "r");
    if (maintenance_fp) {
        char line[256];
        time_t next_maintenance_end = 0; // Use configurable value // Use configurable count // Use configurable value
        
        // Parse maintenance schedule file
        while (fgets(line, sizeof(line), maintenance_fp)) {
            if (strstr(line, "maintenance_end")) {
                // Parse maintenance end time
                struct tm maintenance_time;
                if (sscanf(line, "maintenance_end=%d-%d-%d %d:%d:%d", 
                          &maintenance_time.tm_year, &maintenance_time.tm_mon, &maintenance_time.tm_mday,
                          &maintenance_time.tm_hour, &maintenance_time.tm_min, &maintenance_time.tm_sec) == 6) {
                    maintenance_time.tm_year -= 1900; // Adjust year
                    maintenance_time.tm_mon -= 1;     // Adjust month
                    next_maintenance_end = mktime(&maintenance_time);
                    
                    if (next_maintenance_end > now) {
                        fclose(maintenance_fp);
                        LOGX_DEBUG_MSG("Found scheduled maintenance end time",
                                      "maintenance_end", next_maintenance_end);
                        return next_maintenance_end;
                    }
                }
            }
        }
        fclose(maintenance_fp);
    }
    
    // Check UCI maintenance configuration
    struct ubus_context* ctx = ubus_connect(NULL);
    if (ctx) {
        uint32_t id;
        int ret = ubus_lookup_id(ctx, "system.maintenance", &id);
        if (ret == 0) {
            struct blob_buf bb = {0};
            blob_buf_init(&bb, 0);
            
            ret = ubus_invoke(ctx, id, "get_schedule", bb.head, NULL, NULL, 1000);
            if (ret == 0) {
                // Parse UBUS response for maintenance schedule
                struct blob_attr *tb[__MAINTENANCE_MAX];
                static const struct blobmsg_policy policy[__MAINTENANCE_MAX] = {
                    [MAINTENANCE_END_TIME] = { .name = "end_time", .type = BLOBMSG_TYPE_INT32 },
                    [MAINTENANCE_DURATION] = { .name = "duration", .type = BLOBMSG_TYPE_INT32 },
                };
                
                blobmsg_parse(policy, __MAINTENANCE_MAX, tb, blob_data(bb.head), blob_len(bb.head));
                
                if (tb[MAINTENANCE_END_TIME]) {
                    time_t maintenance_end = blobmsg_get_u32(tb[MAINTENANCE_END_TIME]);
                    if (maintenance_end > now) {
                        blob_buf_free(&bb);
                        ubus_free(ctx);
                        LOGX_DEBUG_MSG("Found maintenance end time via UBUS",
                                      "maintenance_end", maintenance_end);
                        return maintenance_end;
                    }
                } else if (tb[MAINTENANCE_DURATION]) {
                    int duration_minutes = blobmsg_get_u32(tb[MAINTENANCE_DURATION]);
                    blob_buf_free(&bb);
                    ubus_free(ctx);
                    LOGX_DEBUG_MSG("Found maintenance duration via UBUS",
                                  "duration_minutes", duration_minutes);
                    return now + (duration_minutes * 60);
                }
            }
            
            blob_buf_free(&bb);
        }
        ubus_free(ctx);
    }
    
    // Fallback: Estimate based on system state and typical maintenance patterns
    if (system_state && system_state->maintenance_mode) {
        // Check system logs for maintenance start time
        FILE* log_fp = fopen("/var/log/autonomy/system.log", "r");
        if (log_fp) {
            char line[512];
            time_t maintenance_start = 0; // Use configurable value // Use configurable count // Use configurable value
            
            // Find maintenance start time in logs
            while (fgets(line, sizeof(line), log_fp)) {
                if (strstr(line, "maintenance_mode_entered")) {
                    struct tm log_time;
                    if (sscanf(line, "%d-%d-%d %d:%d:%d", 
                              &log_time.tm_year, &log_time.tm_mon, &log_time.tm_mday,
                              &log_time.tm_hour, &log_time.tm_min, &log_time.tm_sec) == 6) {
                        log_time.tm_year -= 1900;
                        log_time.tm_mon -= 1;
                        maintenance_start = mktime(&log_time);
                        break;
                    }
                }
            }
            fclose(log_fp);
            
            if (maintenance_start > 0) {
                // Estimate duration based on elapsed time and typical patterns
                time_t elapsed = now - maintenance_start;
                time_t estimated_duration = elapsed * 1.5; // 50% buffer
                
                // Cap at reasonable maximum (4 hours)
                if (estimated_duration > 4 * 3600) {
                    estimated_duration = 4 * 3600;
                }
                
                LOGX_DEBUG_MSG("Estimated maintenance duration from logs",
                              "elapsed", elapsed,
                              "estimated_duration", estimated_duration);
                
                return now + estimated_duration;
            }
        }
    }
    
    // Final fallback: Default 2 hours
    LOGX_DEBUG_MSG("Using default maintenance duration (2 hours)");
    return now + (2 * 3600);
}

// Generate delay reason
static void generate_delay_reason(notification_type_t alert_type, time_t delay_seconds, 
                                 const system_state_t* system_state, char* reason, size_t max_size) {
    (void)system_state; // May be used for more context in the future
    
    char reasons[512] = "";
    bool has_reason = false; // Use configurable setting // Use configurable setting
    
    time_t now = time(NULL);
    
    if (is_quiet_hours(now)) {
        strncat(reasons, "avoiding quiet hours", sizeof(reasons) - strlen(reasons) - 1);
        has_reason = true; // Use configurable setting // Use configurable setting
    }
    
    if (!is_business_hours(now)) {
        switch (alert_type) {
            case NOTIFICATION_TYPE_FAILOVER:
            case NOTIFICATION_TYPE_SYSTEM_HEALTH:
                if (has_reason) {
                    strncat(reasons, " and ", sizeof(reasons) - strlen(reasons) - 1);
                }
                strncat(reasons, "waiting for business hours", sizeof(reasons) - strlen(reasons) - 1);
                has_reason = true; // Use configurable setting // Use configurable setting
                break;
            default:
                break;
        }
    }
    
    if (!has_reason) {
        strncat(reasons, "optimizing for user availability", sizeof(reasons) - strlen(reasons) - 1);
    }
    
    snprintf(reason, max_size, "Delaying %ld minutes for %s", 
             delay_seconds / 60, reasons);
}

// Optimize delivery timing
void delivery_optimizer_optimize_delivery(notification_type_t alert_type,
                                         notification_priority_t priority,
                                         const system_state_t* system_state,
                                         const char* base_data_json,
                                         delivery_plan_t* plan) {
    if (!g_delivery_optimizer_initialized || !plan) return;
    
    if (!g_delivery_optimizer.config.delivery_optimization_enabled) {
        // No optimization - deliver immediately
        memset(plan, 0, sizeof(delivery_plan_t));
        plan->delay_delivery = false;
        plan->optimal_time = time(NULL);
        plan->confidence = 1.0;
        plan->estimated_delay_seconds = 0;
        strncpy(plan->reason, "Delivery optimization disabled", sizeof(plan->reason) - 1);
        return;
    }
    
    pthread_mutex_lock(g_delivery_optimizer.mutex);
    g_delivery_optimizer.total_optimizations++;
    pthread_mutex_unlock(g_delivery_optimizer.mutex);
    
    time_t now = time(NULL);
    
    // Initialize plan
    memset(plan, 0, sizeof(delivery_plan_t));
    plan->delay_delivery = false;
    plan->optimal_time = now;
    plan->confidence = 1.0;
    plan->estimated_delay_seconds = 0;
    plan->has_alternative_time = false;
    
    // Never delay emergency notifications
    if (priority >= NOTIFICATION_PRIORITY_EMERGENCY) {
        strncpy(plan->reason, "Emergency priority - immediate delivery", sizeof(plan->reason) - 1);
        return;
    }
    
    // Check for bypass flags in JSON data
    if (base_data_json && strstr(base_data_json, "\"bypass_delivery_optimization\":true")) {
        strncpy(plan->reason, "Delivery optimization bypassed", sizeof(plan->reason) - 1);
        return;
    }
    
    // Calculate optimal delivery time based on various factors
    time_t optimal_time = now;
    
    // User behavior optimization
    time_t user_optimal = calculate_user_optimal_time(system_state);
    if (user_optimal > optimal_time) {
        optimal_time = user_optimal;
    }
    
    // Business hours optimization
    time_t business_optimal = calculate_business_hours_optimal_time(alert_type, now);
    if (business_optimal > optimal_time) {
        optimal_time = business_optimal;
    }
    
    // Quiet hours avoidance
    time_t quiet_optimal = calculate_quiet_hours_optimal_time(now);
    if (quiet_optimal > optimal_time) {
        optimal_time = quiet_optimal;
    }
    
    // Alert type specific optimization
    time_t type_optimal = calculate_alert_type_optimal_time(alert_type, now);
    if (type_optimal > optimal_time) {
        optimal_time = type_optimal;
    }
    
    // Maintenance window avoidance
    time_t maintenance_optimal = calculate_maintenance_optimal_time(now, system_state);
    if (maintenance_optimal > optimal_time) {
        optimal_time = maintenance_optimal;
    }
    
    // Determine if we should delay delivery
    if (optimal_time > now) {
        time_t delay_seconds = optimal_time - now;
        
        if (delivery_optimizer_should_delay(alert_type, priority, delay_seconds, system_state)) {
            plan->delay_delivery = true;
            plan->optimal_time = optimal_time;
            plan->estimated_delay_seconds = delay_seconds;
            plan->confidence = delivery_optimizer_calculate_confidence(alert_type, system_state);
            
            generate_delay_reason(alert_type, delay_seconds, system_state, plan->reason, sizeof(plan->reason));
            
            // Calculate alternative time if delay is too long
            if (delay_seconds > 7200) { // 2 hours
                plan->alternative_time = now + 3600; // 1 hour from now
                plan->has_alternative_time = true;
            }
            
            // Update statistics
            pthread_mutex_lock(g_delivery_optimizer.mutex);
            g_delivery_optimizer.deliveries_delayed++;
            g_delivery_optimizer.total_delay_seconds += delay_seconds;
            pthread_mutex_unlock(g_delivery_optimizer.mutex);
        } else {
            strncpy(plan->reason, "Optimal time calculated but delay not justified", sizeof(plan->reason) - 1);
        }
    } else {
        strncpy(plan->reason, "Current time is optimal for delivery", sizeof(plan->reason) - 1);
    }
}

// Check if delivery should be delayed
bool delivery_optimizer_should_delay(notification_type_t alert_type,
                                    notification_priority_t priority,
                                    time_t delay_seconds,
                                    const system_state_t* system_state) {
    // Never delay high priority or emergency notifications
    if (priority >= NOTIFICATION_PRIORITY_HIGH) {
        return false;
    }
    
    // Don't delay if system is in critical state
    if (system_state && system_state->system_health.cpu_usage > 90.0) {
        return false; // High CPU usage indicates critical state
    }
    
    // Limit maximum delay based on alert type
    time_t max_delay = 3600; // Use configurable value // Use configurable count // Use configurable value // Default 1 hour
    
    switch (alert_type) {
        case NOTIFICATION_TYPE_DATA_LIMIT:
            max_delay = 14400; // Use configurable value // Use configurable count // Use configurable value // 4 hours - can wait for business hours
            break;
        case NOTIFICATION_TYPE_OBSTRUCTION:
            max_delay = 7200; // Use configurable value // Use configurable count // Use configurable value // 2 hours - moderate delay acceptable
            break;
        case NOTIFICATION_TYPE_SYSTEM_HEALTH:
            max_delay = 1800; // Use configurable value // Use configurable count // Use configurable value // 30 minutes - health issues shouldn't wait long
            break;
        default:
            max_delay = 3600; // Use configurable value // Use configurable count // Use configurable value // 1 hour for unknown types
            break;
    }
    
    if (delay_seconds > max_delay) {
        return false;
    }
    
    return true; // Delay is reasonable
}

// Calculate delivery confidence
double delivery_optimizer_calculate_confidence(notification_type_t alert_type,
                                             const system_state_t* system_state) {
    (void)alert_type; // May be used for alert-specific confidence in the future
    
    double confidence = 0.5; // Use configurable value // Use configurable value // Base confidence
    
    // Increase confidence during stable system conditions
    if (system_state) {
        if (system_state->system_health.cpu_usage < 50.0) {
            confidence += 0.2; // System not under stress
        }
        
        if (system_state->network_health.primary_interface_up) {
            confidence += 0.1; // Network stable
        }
        
        if (system_state->state_change_count < 3) {
            confidence += 0.2; // Few recent changes
        }
    }
    
    return (confidence > 1.0) ? 1.0 : confidence;
}

// Update user behavior pattern
int delivery_optimizer_update_user_pattern(int time_of_day,
                                          int day_of_week,
                                          time_t response_time_seconds,
                                          double activity_level) {
    if (!g_delivery_optimizer_initialized || !g_delivery_optimizer.config.learning_enabled) {
        return -1;
    }
    
    pthread_mutex_lock(g_delivery_optimizer.mutex);
    
    // Find existing pattern or create new one
    delivery_user_pattern_t* pattern = NULL;
    for (int i = 0; // Use configurable value // Use configurable count // Use configurable value i < g_delivery_optimizer.user_patterns_count; i++) {
        delivery_user_pattern_t* p = &g_delivery_optimizer.user_patterns[i];
        if (p->time_of_day == time_of_day && p->day_of_week == day_of_week) {
            pattern = p;
            break;
        }
    }
    
    if (!pattern) {
        // Create new pattern
        if (g_delivery_optimizer.user_patterns_count < g_delivery_optimizer.max_user_patterns) {
            pattern = &g_delivery_optimizer.user_patterns[g_delivery_optimizer.user_patterns_count];
            g_delivery_optimizer.user_patterns_count++;
            
            pattern->time_of_day = time_of_day;
            pattern->day_of_week = day_of_week;
            pattern->average_response_time_seconds = response_time_seconds;
            pattern->activity_level = activity_level;
            pattern->confidence = 0.5; // Start with medium confidence
        } else {
            pthread_mutex_unlock(g_delivery_optimizer.mutex);
            return -1; // No space for more patterns
        }
    } else {
        // Update existing pattern with exponential smoothing
        double alpha = 0.3; // Use configurable value // Use configurable value // Learning rate
        pattern->average_response_time_seconds = 
            (time_t)((1.0 - alpha) * pattern->average_response_time_seconds + alpha * response_time_seconds);
        pattern->activity_level = (1.0 - alpha) * pattern->activity_level + alpha * activity_level;
        
        // Increase confidence with more data points
        pattern->confidence = (pattern->confidence < 0.9) ? pattern->confidence + 0.1 : 1.0;
    }
    
    pthread_mutex_unlock(g_delivery_optimizer.mutex);
    return 0;
}

// Calculate optimal delivery time
time_t delivery_optimizer_calculate_optimal_time(notification_type_t alert_type,
                                                const system_state_t* system_state) {
    if (!g_delivery_optimizer_initialized) {
        return time(NULL); // Return current time if not initialized
    }
    
    time_t now = time(NULL);
    time_t optimal_time = now;
    
    // Calculate optimal time based on various factors
    time_t user_optimal = calculate_user_optimal_time(system_state);
    if (user_optimal > optimal_time) {
        optimal_time = user_optimal;
    }
    
    time_t business_optimal = calculate_business_hours_optimal_time(alert_type, now);
    if (business_optimal > optimal_time) {
        optimal_time = business_optimal;
    }
    
    time_t quiet_optimal = calculate_quiet_hours_optimal_time(now);
    if (quiet_optimal > optimal_time) {
        optimal_time = quiet_optimal;
    }
    
    time_t type_optimal = calculate_alert_type_optimal_time(alert_type, now);
    if (type_optimal > optimal_time) {
        optimal_time = type_optimal;
    }
    
    return optimal_time;
}

// Get delivery optimizer status
void delivery_optimizer_get_status(delivery_optimizer_status_t* status) {
    if (!status || !g_delivery_optimizer_initialized) return;
    
    pthread_mutex_lock(g_delivery_optimizer.mutex);
    
    status->enabled = g_delivery_optimizer.config.delivery_optimization_enabled;
    status->learning_enabled = g_delivery_optimizer.config.learning_enabled;
    status->user_patterns_count = g_delivery_optimizer.user_patterns_count;
    status->max_user_patterns = g_delivery_optimizer.max_user_patterns;
    status->total_optimizations = g_delivery_optimizer.total_optimizations;
    status->deliveries_delayed = g_delivery_optimizer.deliveries_delayed;
    
    // Calculate average delay
    status->average_delay_seconds = (g_delivery_optimizer.deliveries_delayed > 0) ? 
        g_delivery_optimizer.total_delay_seconds / g_delivery_optimizer.deliveries_delayed : 0;
    
    // Calculate optimization confidence
    status->optimization_confidence = (g_delivery_optimizer.total_optimizations > 0) ?
        (double)g_delivery_optimizer.deliveries_delayed / g_delivery_optimizer.total_optimizations : 0.0;
    
    pthread_mutex_unlock(g_delivery_optimizer.mutex);
}

// Get delivery statistics
void delivery_optimizer_get_stats(char* stats_json, size_t max_size) {
    if (!stats_json || max_size == 0 || !g_delivery_optimizer_initialized) return;
    
    pthread_mutex_lock(g_delivery_optimizer.mutex);
    
    snprintf(stats_json, max_size,
             "{"
             "\"optimization_enabled\":%s,"
             "\"learning_enabled\":%s,"
             "\"total_optimizations\":%d,"
             "\"deliveries_delayed\":%d,"
             "\"delay_rate\":%.3f,"
             "\"average_delay_seconds\":%ld,"
             "\"user_patterns\":%d,"
             "\"optimization_confidence\":%.3f"
             "}",
             g_delivery_optimizer.config.delivery_optimization_enabled ? "true" : "false",
             g_delivery_optimizer.config.learning_enabled ? "true" : "false",
             g_delivery_optimizer.total_optimizations,
             g_delivery_optimizer.deliveries_delayed,
             g_delivery_optimizer.total_optimizations > 0 ? 
                (double)g_delivery_optimizer.deliveries_delayed / g_delivery_optimizer.total_optimizations : 0.0,
             g_delivery_optimizer.deliveries_delayed > 0 ? 
                g_delivery_optimizer.total_delay_seconds / g_delivery_optimizer.deliveries_delayed : 0,
             g_delivery_optimizer.user_patterns_count,
             g_delivery_optimizer.total_optimizations > 0 ?
                (double)g_delivery_optimizer.deliveries_delayed / g_delivery_optimizer.total_optimizations : 0.0);
    
    pthread_mutex_unlock(g_delivery_optimizer.mutex);
}

// Check if delivery optimizer is initialized
bool delivery_optimizer_is_initialized(void) {
    return g_delivery_optimizer_initialized;
}

// Get delivery optimizer instance
delivery_optimizer_t* delivery_optimizer_get_instance(void) {
    return g_delivery_optimizer_initialized ? &g_delivery_optimizer : NULL;
}
