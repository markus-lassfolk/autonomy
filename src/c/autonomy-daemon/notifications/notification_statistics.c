#include "notification_statistics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// Initialize notification statistics
static int notification_statistics_init(notification_statistics_t* stats) {
    if (!stats) {
        return -1;
    }
    
    memset(stats, 0, sizeof(notification_statistics_t));
    
    // Initialize mutex
    stats->mutex = malloc(sizeof(pthread_mutex_t));
    if (!stats->mutex) {
        return -1;
    }
    
    pthread_mutex_init(stats->mutex, NULL);
    
    // Initialize timestamps
    time_t now = time(NULL);
    stats->created_at = now;
    stats->last_updated = now;
    stats->time_stats.last_hour_reset = now;
    stats->time_stats.last_day_reset = now;
    stats->time_stats.last_week_reset = now;
    stats->time_stats.last_month_reset = now;
    
    // Initialize latency stats
    stats->latency_stats.min_latency_ms = LONG_MAX;
    
    return 0;
}

// Clean up notification statistics
static void notification_statistics_cleanup(notification_statistics_t* stats) {
    if (!stats) return;
    
    if (stats->mutex) {
        pthread_mutex_destroy(stats->mutex);
        free(stats->mutex);
    }
    
    memset(stats, 0, sizeof(notification_statistics_t));
}

// Increment sent counter
static void notification_statistics_increment_sent(notification_statistics_t* stats) {
    if (!stats || !stats->mutex) return;
    
    pthread_mutex_lock(stats->mutex);
    stats->total_sent++;
    stats->time_stats.last_hour++;
    stats->time_stats.last_day++;
    stats->time_stats.last_week++;
    stats->time_stats.last_month++;
    stats->last_updated = time(NULL);
    pthread_mutex_unlock(stats->mutex);
}

// Increment suppressed counter
static void notification_statistics_increment_suppressed(notification_statistics_t* stats) {
    if (!stats || !stats->mutex) return;
    
    pthread_mutex_lock(stats->mutex);
    stats->total_suppressed++;
    stats->last_updated = time(NULL);
    pthread_mutex_unlock(stats->mutex);
}

// Increment failed counter
static void notification_statistics_increment_failed(notification_statistics_t* stats) {
    if (!stats || !stats->mutex) return;
    
    pthread_mutex_lock(stats->mutex);
    stats->total_failed++;
    stats->last_updated = time(NULL);
    pthread_mutex_unlock(stats->mutex);
}

// Increment deduped counter
static void notification_statistics_increment_deduped(notification_statistics_t* stats) {
    if (!stats || !stats->mutex) return;
    
    pthread_mutex_lock(stats->mutex);
    stats->total_deduped++;
    stats->last_updated = time(NULL);
    pthread_mutex_unlock(stats->mutex);
}

// Increment rate limited counter
static void notification_statistics_increment_rate_limited(notification_statistics_t* stats) {
    if (!stats || !stats->mutex) return;
    
    pthread_mutex_lock(stats->mutex);
    stats->rate_limited++;
    stats->last_updated = time(NULL);
    pthread_mutex_unlock(stats->mutex);
}

// Increment by priority
void notification_statistics_increment_by_priority(notification_statistics_t* stats, 
                                                  notification_priority_t priority) {
    if (!stats || !stats->mutex) return;
    
    pthread_mutex_lock(stats->mutex);
    
    switch (priority) {
        case NOTIFICATION_PRIORITY_EMERGENCY:
            stats->priority_stats.emergency_count++;
            break;
        case NOTIFICATION_PRIORITY_HIGH:
            stats->priority_stats.high_count++;
            break;
        case NOTIFICATION_PRIORITY_NORMAL:
            stats->priority_stats.normal_count++;
            break;
        case NOTIFICATION_PRIORITY_LOW:
            stats->priority_stats.low_count++;
            break;
        case NOTIFICATION_PRIORITY_LOWEST:
            stats->priority_stats.lowest_count++;
            break;
    }
    
    stats->last_updated = time(NULL);
    pthread_mutex_unlock(stats->mutex);
}

// Increment by type
void notification_statistics_increment_by_type(notification_statistics_t* stats, 
                                              notification_type_t type) {
    if (!stats || !stats->mutex || type < 0 || type >= 32) return;
    
    pthread_mutex_lock(stats->mutex);
    stats->type_stats.type_counts[type]++;
    stats->last_updated = time(NULL);
    pthread_mutex_unlock(stats->mutex);
}

// Increment by channel
void notification_statistics_increment_by_channel(notification_statistics_t* stats, 
                                                 notification_channel_t channel) {
    if (!stats || !stats->mutex || channel < 0 || channel >= 16) return;
    
    pthread_mutex_lock(stats->mutex);
    stats->channel_stats.channel_counts[channel]++;
    stats->channel_stats.channel_last_success[channel] = time(NULL);
    stats->last_updated = time(NULL);
    pthread_mutex_unlock(stats->mutex);
}

// Record channel failure
void notification_statistics_record_channel_failure(notification_statistics_t* stats, 
                                                   notification_channel_t channel) {
    if (!stats || !stats->mutex || channel < 0 || channel >= 16) return;
    
    pthread_mutex_lock(stats->mutex);
    stats->channel_stats.channel_failures[channel]++;
    stats->channel_stats.channel_last_failure[channel] = time(NULL);
    stats->last_updated = time(NULL);
    pthread_mutex_unlock(stats->mutex);
}

// Update latency statistics
static void notification_statistics_update_latency(notification_statistics_t* stats, time_t latency_ms) {
    if (!stats || !stats->mutex) return;
    
    pthread_mutex_lock(stats->mutex);
    
    // Update max latency
    if (latency_ms > stats->latency_stats.max_latency_ms) {
        stats->latency_stats.max_latency_ms = latency_ms;
    }
    
    // Update min latency
    if (latency_ms < stats->latency_stats.min_latency_ms) {
        stats->latency_stats.min_latency_ms = latency_ms;
    }
    
    // Update average latency (exponential moving average)
    if (stats->latency_stats.latency_samples == 0) {
        stats->latency_stats.average_latency_ms = latency_ms;
    } else {
        // Weighted average with more weight on recent measurements
        stats->latency_stats.average_latency_ms = 
            (time_t)(stats->latency_stats.average_latency_ms * 0.9 + latency_ms * 0.1);
    }
    
    stats->latency_stats.latency_samples++;
    stats->last_updated = time(NULL);
    
    pthread_mutex_unlock(stats->mutex);
}

// Update time-based statistics
static void notification_statistics_update_time_based(notification_statistics_t* stats) {
    if (!stats || !stats->mutex) return;
    
    pthread_mutex_lock(stats->mutex);
    
    time_t now = time(NULL);
    
    // Check if we need to reset hourly counter
    if (now - stats->time_stats.last_hour_reset >= 3600) { // 1 hour
        stats->time_stats.last_hour = 0;
        stats->time_stats.last_hour_reset = now;
    }
    
    // Check if we need to reset daily counter
    if (now - stats->time_stats.last_day_reset >= 86400) { // 24 hours
        stats->time_stats.last_day = 0;
        stats->time_stats.last_day_reset = now;
    }
    
    // Check if we need to reset weekly counter
    if (now - stats->time_stats.last_week_reset >= 604800) { // 7 days
        stats->time_stats.last_week = 0;
        stats->time_stats.last_week_reset = now;
    }
    
    // Check if we need to reset monthly counter
    if (now - stats->time_stats.last_month_reset >= 2592000) { // 30 days
        stats->time_stats.last_month = 0;
        stats->time_stats.last_month_reset = now;
    }
    
    pthread_mutex_unlock(stats->mutex);
}

// Get statistics summary
static void notification_statistics_get_summary(notification_statistics_t* stats, char* summary_json, size_t max_size) {
    if (!stats || !summary_json || max_size == 0) return;
    
    pthread_mutex_lock(stats->mutex);
    
    uint64_t total_processed = stats->total_sent + stats->total_failed;
    double success_rate = (total_processed > 0) ? (double)stats->total_sent / total_processed : 0.0;
    
    snprintf(summary_json, max_size,
             "{"
             "\"total_sent\":%lu,"
             "\"total_failed\":%lu,"
             "\"total_suppressed\":%lu,"
             "\"total_deduped\":%lu,"
             "\"rate_limited\":%lu,"
             "\"adaptive_adjustments\":%lu,"
             "\"success_rate\":%.3f,"
             "\"last_hour\":%lu,"
             "\"last_day\":%lu,"
             "\"last_week\":%lu,"
             "\"last_month\":%lu,"
             "\"average_latency_ms\":%ld,"
             "\"max_latency_ms\":%ld,"
             "\"min_latency_ms\":%ld,"
             "\"latency_samples\":%lu,"
             "\"last_updated\":%ld,"
             "\"created_at\":%ld"
             "}",
             stats->total_sent,
             stats->total_failed,
             stats->total_suppressed,
             stats->total_deduped,
             stats->rate_limited,
             stats->adaptive_adjustments,
             success_rate,
             stats->time_stats.last_hour,
             stats->time_stats.last_day,
             stats->time_stats.last_week,
             stats->time_stats.last_month,
             stats->latency_stats.average_latency_ms,
             stats->latency_stats.max_latency_ms,
             stats->latency_stats.min_latency_ms == LONG_MAX ? 0 : stats->latency_stats.min_latency_ms,
             stats->latency_stats.latency_samples,
             stats->last_updated,
             stats->created_at);
    
    pthread_mutex_unlock(stats->mutex);
}

// Get priority breakdown
void notification_statistics_get_priority_breakdown(notification_statistics_t* stats, 
                                                   priority_stats_t* priority_stats) {
    if (!stats || !priority_stats) return;
    
    pthread_mutex_lock(stats->mutex);
    *priority_stats = stats->priority_stats;
    pthread_mutex_unlock(stats->mutex);
}

// Get type breakdown
void notification_statistics_get_type_breakdown(notification_statistics_t* stats, 
                                               type_stats_t* type_stats) {
    if (!stats || !type_stats) return;
    
    pthread_mutex_lock(stats->mutex);
    *type_stats = stats->type_stats;
    pthread_mutex_unlock(stats->mutex);
}

// Get channel breakdown
void notification_statistics_get_channel_breakdown(notification_statistics_t* stats, 
                                                  channel_stats_t* channel_stats) {
    if (!stats || !channel_stats) return;
    
    pthread_mutex_lock(stats->mutex);
    *channel_stats = stats->channel_stats;
    pthread_mutex_unlock(stats->mutex);
}

// Get latency statistics
void notification_statistics_get_latency_stats(notification_statistics_t* stats, 
                                              latency_stats_t* latency_stats) {
    if (!stats || !latency_stats) return;
    
    pthread_mutex_lock(stats->mutex);
    *latency_stats = stats->latency_stats;
    pthread_mutex_unlock(stats->mutex);
}

// Reset all statistics
static void notification_statistics_reset(notification_statistics_t* stats) {
    if (!stats || !stats->mutex) return;
    
    pthread_mutex_lock(stats->mutex);
    
    // Reset global counters
    stats->total_sent = 0;
    stats->total_suppressed = 0;
    stats->total_failed = 0;
    stats->total_deduped = 0;
    stats->rate_limited = 0;
    stats->adaptive_adjustments = 0;
    
    // Reset priority stats
    memset(&stats->priority_stats, 0, sizeof(priority_stats_t));
    
    // Reset type stats
    memset(&stats->type_stats, 0, sizeof(type_stats_t));
    
    // Reset channel stats
    memset(&stats->channel_stats, 0, sizeof(channel_stats_t));
    
    // Reset time-based stats
    time_t now = time(NULL);
    stats->time_stats.last_hour = 0;
    stats->time_stats.last_day = 0;
    stats->time_stats.last_week = 0;
    stats->time_stats.last_month = 0;
    stats->time_stats.last_hour_reset = now;
    stats->time_stats.last_day_reset = now;
    stats->time_stats.last_week_reset = now;
    stats->time_stats.last_month_reset = now;
    
    // Reset latency stats
    stats->latency_stats.average_latency_ms = 0;
    stats->latency_stats.max_latency_ms = 0;
    stats->latency_stats.min_latency_ms = LONG_MAX;
    stats->latency_stats.latency_samples = 0;
    stats->latency_stats.latency_variance = 0.0;
    
    stats->last_updated = now;
    
    pthread_mutex_unlock(stats->mutex);
}

// Reset time-based counters
static void notification_statistics_reset_time_counters(notification_statistics_t* stats) {
    if (!stats || !stats->mutex) return;
    
    pthread_mutex_lock(stats->mutex);
    
    time_t now = time(NULL);
    stats->time_stats.last_hour = 0;
    stats->time_stats.last_day = 0;
    stats->time_stats.last_week = 0;
    stats->time_stats.last_month = 0;
    stats->time_stats.last_hour_reset = now;
    stats->time_stats.last_day_reset = now;
    stats->time_stats.last_week_reset = now;
    stats->time_stats.last_month_reset = now;
    stats->last_updated = now;
    
    pthread_mutex_unlock(stats->mutex);
}

// Get success rate
static double notification_statistics_get_success_rate(notification_statistics_t* stats) {
    if (!stats || !stats->mutex) return 0.0;
    
    pthread_mutex_lock(stats->mutex);
    
    uint64_t total_processed = stats->total_sent + stats->total_failed;
    double success_rate = (total_processed > 0) ? (double)stats->total_sent / total_processed : 0.0;
    
    pthread_mutex_unlock(stats->mutex);
    return success_rate;
}

// Get channel success rate
double notification_statistics_get_channel_success_rate(notification_statistics_t* stats,
                                                       notification_channel_t channel) {
    if (!stats || !stats->mutex || channel < 0 || channel >= 16) return 0.0;
    
    pthread_mutex_lock(stats->mutex);
    
    uint64_t sent = stats->channel_stats.channel_counts[channel];
    uint64_t failed = stats->channel_stats.channel_failures[channel];
    uint64_t total = sent + failed;
    
    double success_rate = (total > 0) ? (double)sent / total : 0.0;
    
    pthread_mutex_unlock(stats->mutex);
    return success_rate;
}
