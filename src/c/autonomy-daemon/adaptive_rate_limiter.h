#ifndef ADAPTIVE_RATE_LIMITER_H
#define ADAPTIVE_RATE_LIMITER_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

// Adaptive rate limiter for notification delivery

// Rate limiter configuration
typedef struct {
    int initial_rate_per_minute;
    int max_rate_per_minute;
    int min_rate_per_minute;
    float backoff_factor;
    float recovery_factor;
    int window_size_seconds;
    int max_retry_attempts;
} rate_limiter_config_t;

// Rate limiter state
typedef struct {
    int current_rate;
    int successful_requests;
    int failed_requests;
    time_t window_start;
    time_t last_request;
    int consecutive_failures;
    bool is_backed_off;
    pthread_mutex_t mutex;
} rate_limiter_t;

// Function declarations
int rate_limiter_init(rate_limiter_t *limiter, const rate_limiter_config_t *config);
void rate_limiter_cleanup(rate_limiter_t *limiter);
bool rate_limiter_allow_request(rate_limiter_t *limiter);
void rate_limiter_record_success(rate_limiter_t *limiter);
void rate_limiter_record_failure(rate_limiter_t *limiter);
int rate_limiter_get_current_rate(rate_limiter_t *limiter);
int rate_limiter_get_wait_time(rate_limiter_t *limiter);

#endif // ADAPTIVE_RATE_LIMITER_H
