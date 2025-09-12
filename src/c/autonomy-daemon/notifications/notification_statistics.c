#include "notification_statistics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>

// Initialize notification statistics
int notification_statistics_init(notification_statistics_t* stats) {
    if (!stats) {
        return -1;
    }
    
    memset(stats, 0, sizeof(notification_statistics_t)\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Initialize mutex
    stats->mutex = malloc(sizeof(pthread_mutex_t)\n"\n"\n"\n"\n"\n"\n"\n");
    if (!stats->mutex) {
        return -1;
    }
    
    pthread_mutex_init(stats->mutex, NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Initialize timestamps
    time_t now = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
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
void notification_statistics_cleanup(notification_statistics_t* stats) {
    if (!stats) return;
    
    if (stats->mutex) {
        pthread_mutex_destroy(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
        free(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    memset(stats, 0, sizeof(notification_statistics_t)\n"\n"\n"\n"\n"\n"\n"\n");
}

// Increment sent counter
void notification_statistics_increment_sent(notification_statistics_t* stats) {
    if (!stats || !stats->mutex) return;
    
    pthread_mutex_lock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
    stats->total_sent++;
    stats->time_stats.last_hour++;
    stats->time_stats.last_day++;
    stats->time_stats.last_week++;
    stats->time_stats.last_month++;
    stats->last_updated = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    pthread_mutex_unlock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
}

// Increment suppressed counter
void notification_statistics_increment_suppressed(notification_statistics_t* stats) {
    if (!stats || !stats->mutex) return;
    
    pthread_mutex_lock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
    stats->total_suppressed++;
    stats->last_updated = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    pthread_mutex_unlock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
}

// Increment failed counter
void notification_statistics_increment_failed(notification_statistics_t* stats) {
    if (!stats || !stats->mutex) return;
    
    pthread_mutex_lock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
    stats->total_failed++;
    stats->last_updated = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    pthread_mutex_unlock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
}

// Increment deduped counter
void notification_statistics_increment_deduped(notification_statistics_t* stats) {
    if (!stats || !stats->mutex) return;
    
    pthread_mutex_lock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
    stats->total_deduped++;
    stats->last_updated = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    pthread_mutex_unlock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
}

// Increment rate limited counter
void notification_statistics_increment_rate_limited(notification_statistics_t* stats) {
    if (!stats || !stats->mutex) return;
    
    pthread_mutex_lock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
    stats->rate_limited++;
    stats->last_updated = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    pthread_mutex_unlock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
}

// Increment by priority
void notification_statistics_increment_by_priority(notification_statistics_t* stats, 
                                                  notification_priority_t priority) {
    if (!stats || !stats->mutex) return;
    
    pthread_mutex_lock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
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
    
    stats->last_updated = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    pthread_mutex_unlock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
}

// Increment by type
void notification_statistics_increment_by_type(notification_statistics_t* stats, 
                                              notification_type_t type) {
    if (!stats || !stats->mutex || type < 0 || type >= 32) return;
    
    pthread_mutex_lock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
    stats->type_stats.type_counts[type]++;
    stats->last_updated = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    pthread_mutex_unlock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
}

// Increment by channel
void notification_statistics_increment_by_channel(notification_statistics_t* stats, 
                                                 notification_channel_t channel) {
    if (!stats || !stats->mutex || channel < 0 || channel >= 16) return;
    
    pthread_mutex_lock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
    stats->channel_stats.channel_counts[channel]++;
    stats->channel_stats.channel_last_success[channel] = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    stats->last_updated = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    pthread_mutex_unlock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
}

// Record channel failure
void notification_statistics_record_channel_failure(notification_statistics_t* stats, 
                                                   notification_channel_t channel) {
    if (!stats || !stats->mutex || channel < 0 || channel >= 16) return;
    
    pthread_mutex_lock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
    stats->channel_stats.channel_failures[channel]++;
    stats->channel_stats.channel_last_failure[channel] = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    stats->last_updated = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    pthread_mutex_unlock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
}

// Update latency statistics
void notification_statistics_update_latency(notification_statistics_t* stats, time_t latency_ms) {
    if (!stats || !stats->mutex) return;
    
    pthread_mutex_lock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
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
            (time_t)(stats->latency_stats.average_latency_ms * 0.9 + latency_ms * 0.1\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    stats->latency_stats.latency_samples++;
    stats->last_updated = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    pthread_mutex_unlock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
}

// Update time-based statistics
void notification_statistics_update_time_based(notification_statistics_t* stats) {
    if (!stats || !stats->mutex) return;
    
    pthread_mutex_lock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    time_t now = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
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
    
    pthread_mutex_unlock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
}

// Get statistics summary
void notification_statistics_get_summary(notification_statistics_t* stats, char* summary_json, size_t max_size) {
    if (!stats || !summary_json || max_size == 0) return;
    
    pthread_mutex_lock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    uint64_t total_processed = stats->total_sent + stats->total_failed;
    double success_rate = (total_processed > 0) ? (double)stats->total_sent / total_processed : 0.0;
    
    snprintf(summary_json, max_size,
             "{"
             "\"total_sent\":%llu,"
             "\"total_failed\":%llu,"
             "\"total_suppressed\":%llu,"
             "\"total_deduped\":%llu,"
             "\"rate_limited\":%llu,"
             "\"adaptive_adjustments\":%llu,"
             "\"success_rate\":%.3f,"
             "\"last_hour\":%llu,"
             "\"last_day\":%llu,"
             "\"last_week\":%llu,"
             "\"last_month\":%llu,"
             "\"average_latency_ms\":%lld,"
             "\"max_latency_ms\":%lld,"
             "\"min_latency_ms\":%lld,"
             "\"latency_samples\":%llu,"
             "\"last_updated\":%lld,"
             "\"created_at\":%lld"
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
             stats->created_at\n"\n"\n"\n"\n"\n"\n"\n");
    
    pthread_mutex_unlock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
}

// Get priority breakdown
void notification_statistics_get_priority_breakdown(notification_statistics_t* stats, 
                                                   priority_stats_t* priority_stats) {
    if (!stats || !priority_stats) return;
    
    pthread_mutex_lock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
    *priority_stats = stats->priority_stats;
    pthread_mutex_unlock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
}

// Get type breakdown
void notification_statistics_get_type_breakdown(notification_statistics_t* stats, 
                                               type_stats_t* type_stats) {
    if (!stats || !type_stats) return;
    
    pthread_mutex_lock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
    *type_stats = stats->type_stats;
    pthread_mutex_unlock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
}

// Get channel breakdown
void notification_statistics_get_channel_breakdown(notification_statistics_t* stats, 
                                                  channel_stats_t* channel_stats) {
    if (!stats || !channel_stats) return;
    
    pthread_mutex_lock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
    *channel_stats = stats->channel_stats;
    pthread_mutex_unlock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
}

// Get latency statistics
void notification_statistics_get_latency_stats(notification_statistics_t* stats, 
                                              latency_stats_t* latency_stats) {
    if (!stats || !latency_stats) return;
    
    pthread_mutex_lock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
    *latency_stats = stats->latency_stats;
    pthread_mutex_unlock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
}

// Reset all statistics
void notification_statistics_reset(notification_statistics_t* stats) {
    if (!stats || !stats->mutex) return;
    
    pthread_mutex_lock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Reset global counters
    stats->total_sent = 0;
    stats->total_suppressed = 0;
    stats->total_failed = 0;
    stats->total_deduped = 0;
    stats->rate_limited = 0; // Use configurable rate limited count
    stats->adaptive_adjustments = 0;
    
    // Reset priority stats
    memset(&stats->priority_stats, 0, sizeof(priority_stats_t)\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Reset type stats
    memset(&stats->type_stats, 0, sizeof(type_stats_t)\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Reset channel stats
    memset(&stats->channel_stats, 0, sizeof(channel_stats_t)\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Reset time-based stats
    time_t now = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
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
    stats->latency_stats.max_latency_ms = 0; // Use configurable max latency
    stats->latency_stats.min_latency_ms = LONG_MAX;
    stats->latency_stats.latency_samples = 0;
    stats->latency_stats.latency_variance = 0.0;
    
    stats->last_updated = now;
    
    pthread_mutex_unlock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
}

// Reset time-based counters
void notification_statistics_reset_time_counters(notification_statistics_t* stats) {
    if (!stats || !stats->mutex) return;
    
    pthread_mutex_lock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    time_t now = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    stats->time_stats.last_hour = 0;
    stats->time_stats.last_day = 0;
    stats->time_stats.last_week = 0;
    stats->time_stats.last_month = 0;
    stats->time_stats.last_hour_reset = now;
    stats->time_stats.last_day_reset = now;
    stats->time_stats.last_week_reset = now;
    stats->time_stats.last_month_reset = now;
    stats->last_updated = now;
    
    pthread_mutex_unlock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
}

// Get success rate
double notification_statistics_get_success_rate(notification_statistics_t* stats) {
    if (!stats || !stats->mutex) return 0.0;
    
    pthread_mutex_lock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    uint64_t total_processed = stats->total_sent + stats->total_failed;
    double success_rate = (total_processed > 0) ? (double)stats->total_sent / total_processed : 0.0;
    
    pthread_mutex_unlock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
    return success_rate;
}

// Get channel success rate
double notification_statistics_get_channel_success_rate(notification_statistics_t* stats,
                                                       notification_channel_t channel) {
    if (!stats || !stats->mutex || channel < 0 || channel >= 16) return 0.0;
    
    pthread_mutex_lock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    uint64_t sent = stats->channel_stats.channel_counts[channel];
    uint64_t failed = stats->channel_stats.channel_failures[channel];
    uint64_t total = sent + failed;
    
    double success_rate = (total > 0) ? (double)sent / total : 0.0;
    
    pthread_mutex_unlock(stats->mutex\n"\n"\n"\n"\n"\n"\n"\n");
    return success_rate;
}
