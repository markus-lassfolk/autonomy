#ifndef ADAPTIVE_RATE_LIMITER_H
#define ADAPTIVE_RATE_LIMITER_H

#include "notification_types.h"
#include <stdbool.h>
#include <time.h>

// Rate limiter configuration
typedef struct {
    int initial_rate;                    // Initial requests per window
    int min_rate;                        // Minimum rate limit
    int max_rate;                        // Maximum rate limit
    int window_size_seconds;             // Time window size
    
    // Priority-based rate limits
    int emergency_rate_limit;            // Emergency priority rate limit
    int high_rate_limit;                 // High priority rate limit
    int normal_rate_limit;               // Normal priority rate limit
    int low_rate_limit;                  // Low priority rate limit
    int lowest_rate_limit;               // Lowest priority rate limit
    
    // Priority-based cooldowns
    int emergency_cooldown_seconds;      // Emergency priority cooldown
    int high_cooldown_seconds;           // High priority cooldown
    int normal_cooldown_seconds;         // Normal priority cooldown
    int low_cooldown_seconds;            // Low priority cooldown
    int lowest_cooldown_seconds;         // Lowest priority cooldown
    
    // Adaptive parameters
    int success_threshold;               // Successes before rate increase
    int failure_threshold;               // Failures before rate decrease
    double adjustment_factor;             // Rate adjustment factor
    int min_adjustment_interval;         // Minimum time between adjustments
} rate_limiter_config_t;

// Rate limiter structure
typedef struct {
    rate_limiter_config_t config;
    
    // Current state
    int current_rate;
    time_t last_adjustment;
    
    // Success/failure tracking
    int success_count;
    int failure_count;
    int total_requests;
    
    // Sliding window
    int window_size;
    time_t window_start;
    int request_count;
    
    // Priority-based tracking
    time_t priority_last_request[5];     // Indexed by priority
    int priority_request_count[5];       // Indexed by priority
    
    // Adaptive parameters
    int success_threshold;
    int failure_threshold;
    double adjustment_factor;
    int min_rate;
    int max_rate;
} adaptive_rate_limiter_t;

// Rate limiter statistics
typedef struct {
    int current_rate;
    int min_rate;
    int max_rate;
    int success_count;
    int failure_count;
    int total_requests;
    time_t window_start;
    int request_count;
    int window_size;
    time_t last_adjustment;
    time_t priority_last_request[5];
    int priority_request_count[5];
} rate_limiter_stats_t;

// Initialize rate limiter
int adaptive_rate_limiter_init(adaptive_rate_limiter_t* limiter, const rate_limiter_config_t* config);

// Check if request is allowed
bool adaptive_rate_limiter_allow_request(adaptive_rate_limiter_t* limiter, 
                                       notification_priority_t priority);

// Record successful request
void adaptive_rate_limiter_record_success(adaptive_rate_limiter_t* limiter);

// Record failed request
void adaptive_rate_limiter_record_failure(adaptive_rate_limiter_t* limiter);

// Adjust rate based on success/failure
void adaptive_rate_limiter_adjust_rate(adaptive_rate_limiter_t* limiter, bool success);

// Get current rate limit
int adaptive_rate_limiter_get_current_rate(const adaptive_rate_limiter_t* limiter);

// Set rate limit manually
void adaptive_rate_limiter_set_rate(adaptive_rate_limiter_t* limiter, int new_rate);

// Get rate limiter statistics
void adaptive_rate_limiter_get_stats(const adaptive_rate_limiter_t* limiter, 
                                   rate_limiter_stats_t* stats);

// Reset rate limiter
void adaptive_rate_limiter_reset(adaptive_rate_limiter_t* limiter);

// Clean up rate limiter
void adaptive_rate_limiter_cleanup(adaptive_rate_limiter_t* limiter);

// Check if rate limiter is in emergency mode
bool adaptive_rate_limiter_is_emergency_mode(const adaptive_rate_limiter_t* limiter);

// Get time until next allowed request for priority
int adaptive_rate_limiter_get_wait_time(const adaptive_rate_limiter_t* limiter, 
                                      notification_priority_t priority);

#endif // ADAPTIVE_RATE_LIMITER_H
