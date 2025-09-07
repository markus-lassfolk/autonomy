#ifndef NOTIFICATION_STATISTICS_H
#define NOTIFICATION_STATISTICS_H

#include "notification_types.h"
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

// Per-priority statistics
typedef struct {
    uint64_t emergency_count;
    uint64_t high_count;
    uint64_t normal_count;
    uint64_t low_count;
    uint64_t lowest_count;
} priority_stats_t;

// Per-type statistics
typedef struct {
    uint64_t type_counts[32]; // Indexed by notification_type_t
} type_stats_t;

// Per-channel statistics
typedef struct {
    uint64_t channel_counts[16]; // Indexed by notification_channel_t
    uint64_t channel_failures[16];
    time_t channel_last_success[16];
    time_t channel_last_failure[16];
} channel_stats_t;

// Time-based statistics
typedef struct {
    uint64_t last_hour;
    uint64_t last_day;
    uint64_t last_week;
    uint64_t last_month;
    time_t last_hour_reset;
    time_t last_day_reset;
    time_t last_week_reset;
    time_t last_month_reset;
} time_based_stats_t;

// Latency statistics
typedef struct {
    time_t average_latency_ms;
    time_t max_latency_ms;
    time_t min_latency_ms;
    uint64_t latency_samples;
    double latency_variance;
} latency_stats_t;

// Comprehensive notification statistics
typedef struct {
    // Global counters
    uint64_t total_sent;
    uint64_t total_suppressed;
    uint64_t total_failed;
    uint64_t total_deduped;
    uint64_t rate_limited;
    uint64_t adaptive_adjustments;
    
    // Detailed breakdowns
    priority_stats_t priority_stats;
    type_stats_t type_stats;
    channel_stats_t channel_stats;
    time_based_stats_t time_stats;
    latency_stats_t latency_stats;
    
    // Metadata
    time_t last_updated;
    time_t created_at;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} notification_statistics_t;

// Initialize notification statistics
int notification_statistics_init(notification_statistics_t* stats);

// Clean up notification statistics
void notification_statistics_cleanup(notification_statistics_t* stats);

// Increment sent counter
void notification_statistics_increment_sent(notification_statistics_t* stats);

// Increment suppressed counter
void notification_statistics_increment_suppressed(notification_statistics_t* stats);

// Increment failed counter
void notification_statistics_increment_failed(notification_statistics_t* stats);

// Increment deduped counter
void notification_statistics_increment_deduped(notification_statistics_t* stats);

// Increment rate limited counter
void notification_statistics_increment_rate_limited(notification_statistics_t* stats);

// Increment by priority
void notification_statistics_increment_by_priority(notification_statistics_t* stats, 
                                                  notification_priority_t priority);

// Increment by type
void notification_statistics_increment_by_type(notification_statistics_t* stats, 
                                              notification_type_t type);

// Increment by channel
void notification_statistics_increment_by_channel(notification_statistics_t* stats, 
                                                 notification_channel_t channel);

// Record channel failure
void notification_statistics_record_channel_failure(notification_statistics_t* stats, 
                                                   notification_channel_t channel);

// Update latency statistics
void notification_statistics_update_latency(notification_statistics_t* stats, time_t latency_ms);

// Update time-based statistics
void notification_statistics_update_time_based(notification_statistics_t* stats);

// Get statistics summary
void notification_statistics_get_summary(notification_statistics_t* stats, char* summary_json, size_t max_size);

// Get priority breakdown
void notification_statistics_get_priority_breakdown(notification_statistics_t* stats, 
                                                   priority_stats_t* priority_stats);

// Get type breakdown
void notification_statistics_get_type_breakdown(notification_statistics_t* stats, 
                                               type_stats_t* type_stats);

// Get channel breakdown
void notification_statistics_get_channel_breakdown(notification_statistics_t* stats, 
                                                  channel_stats_t* channel_stats);

// Get latency statistics
void notification_statistics_get_latency_stats(notification_statistics_t* stats, 
                                              latency_stats_t* latency_stats);

// Reset all statistics
void notification_statistics_reset(notification_statistics_t* stats);

// Reset time-based counters
void notification_statistics_reset_time_counters(notification_statistics_t* stats);

// Get success rate
double notification_statistics_get_success_rate(notification_statistics_t* stats);

// Get channel success rate
double notification_statistics_get_channel_success_rate(notification_statistics_t* stats,
                                                       notification_channel_t channel);

#endif // NOTIFICATION_STATISTICS_H
