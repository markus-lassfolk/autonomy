#include "gps_error_recovery.h"
#include "../utils/logx.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

// Error recovery configuration
static const int MAX_ERROR_HISTORY = 1000;                   // Maximum error history entries
static const int MAX_RETRY_ATTEMPTS = 5;                     // Maximum retry attempts
static const int RETRY_DELAY_BASE = 1000;                    // Base retry delay in milliseconds
static const int MAX_RETRY_DELAY = 30000;                    // Maximum retry delay in milliseconds
static const int ERROR_WINDOW_SIZE = 3600;                   // 1 hour error window
static const double ERROR_THRESHOLD_RATIO = 0.3;             // Error threshold ratio (30%)

// Error types
static const char* ERROR_TYPE_NAMES[] = {
    "unknown", "timeout", "connection_failed", "invalid_data", "parse_error",
    "api_error", "network_error", "authentication_error", "rate_limit", "server_error"
};

// Recovery strategies
static const char* RECOVERY_STRATEGY_NAMES[] = {
    "none", "retry", "fallback", "reset", "degrade", "switch_source"
};

// Global error recovery state
static gps_error_recovery_t g_error_recovery = {0};
static bool g_error_recovery_initialized = false;
static pthread_mutex_t g_error_recovery_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize GPS error recovery
static int gps_error_recovery_init(void) {
    if (g_error_recovery_initialized) {
        LOGX_WARN("GPS error recovery already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_error_recovery_mutex);
    
    // Initialize error recovery state
    memset(&g_error_recovery, 0, sizeof(gps_error_recovery_t));
    g_error_recovery.enabled = true;
    g_error_recovery.max_error_history = MAX_ERROR_HISTORY;
    g_error_recovery.max_retry_attempts = MAX_RETRY_ATTEMPTS;
    g_error_recovery.retry_delay_base = RETRY_DELAY_BASE;
    g_error_recovery.max_retry_delay = MAX_RETRY_DELAY;
    g_error_recovery.error_window_size = ERROR_WINDOW_SIZE;
    g_error_recovery.error_threshold_ratio = ERROR_THRESHOLD_RATIO;
    
    g_error_recovery.error_history_count = 0;
    g_error_recovery.total_errors = 0;
    g_error_recovery.recovered_errors = 0;
    g_error_recovery.unrecovered_errors = 0;
    g_error_recovery.last_recovery = 0;
    
    // Initialize error history
    for (int i = 0; i < MAX_ERROR_HISTORY; i++) {
        g_error_recovery.error_history[i].active = false;
        g_error_recovery.error_history[i].timestamp = 0;
        g_error_recovery.error_history[i].source_id = 0;
        g_error_recovery.error_history[i].error_type = GPS_ERROR_TYPE_UNKNOWN;
        g_error_recovery.error_history[i].error_code = 0;
        g_error_recovery.error_history[i].error_message[0] = '\0';
        g_error_recovery.error_history[i].recovery_strategy = RECOVERY_STRATEGY_NONE;
        g_error_recovery.error_history[i].recovery_successful = false;
        g_error_recovery.error_history[i].retry_count = 0;
        g_error_recovery.error_history[i].recovery_time = 0;
    }
    
    // Initialize source error tracking
    for (int i = 0; i < GPS_MAX_SOURCES; i++) {
        g_error_recovery.source_errors[i].source_id = i;
        g_error_recovery.source_errors[i].total_errors = 0;
        g_error_recovery.source_errors[i].recovered_errors = 0;
        g_error_recovery.source_errors[i].unrecovered_errors = 0;
        g_error_recovery.source_errors[i].last_error = 0;
        g_error_recovery.source_errors[i].error_rate = 0.0;
        g_error_recovery.source_errors[i].recovery_rate = 0.0;
        g_error_recovery.source_errors[i].current_retry_count = 0;
        g_error_recovery.source_errors[i].last_retry = 0;
        g_error_recovery.source_errors[i].backoff_until = 0;
        g_error_recovery.source_errors[i].status = SOURCE_STATUS_ACTIVE;
    }
    
    g_error_recovery_initialized = true;
    pthread_mutex_unlock(&g_error_recovery_mutex);
    
    LOGX_INFO("GPS error recovery initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Record GPS error
static int gps_error_recovery_record_error(int source_id, gps_error_type_t error_type, int error_code, const char *error_message) {
    if (!g_error_recovery_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_error_recovery_mutex);
    
    g_error_recovery.total_errors++;
    
    // Add to error history
    add_error_history_entry(source_id, error_type, error_code, error_message);
    
    // Update source error tracking
    update_source_error_tracking(source_id, error_type);
    
    // Determine recovery strategy
    gps_recovery_strategy_t strategy = determine_recovery_strategy(source_id, error_type);
    
    // Attempt recovery
    bool recovery_successful = attempt_error_recovery(source_id, error_type, strategy);
    
    // Update recovery statistics
    if (recovery_successful) {
        g_error_recovery.recovered_errors++;
    } else {
        g_error_recovery.unrecovered_errors++;
    }
    
    g_error_recovery.last_recovery = time(NULL);
    
    pthread_mutex_unlock(&g_error_recovery_mutex);
    
    LOGX_WARN("GPS error recorded for source %d: %s (code: %d) - %s", 
              source_id, ERROR_TYPE_NAMES[error_type], error_code, error_message);
    
    return AUTONOMY_SUCCESS;
}

// Add error history entry
static void add_error_history_entry(int source_id, gps_error_type_t error_type, int error_code, const char *error_message) {
    // Find free history slot
    int slot_index = -1;
    for (int i = 0; i < g_error_recovery.max_error_history; i++) {
        if (!g_error_recovery.error_history[i].active) {
            slot_index = i;
            break;
        }
    }
    
    if (slot_index < 0) {
        // Remove oldest entry to make room
        slot_index = find_oldest_error_entry();
        if (slot_index >= 0) {
            g_error_recovery.error_history[slot_index].active = false;
            g_error_recovery.error_history_count--;
        }
    }
    
    if (slot_index >= 0) {
        gps_error_entry_t *entry = &g_error_recovery.error_history[slot_index];
        
        entry->active = true;
        entry->timestamp = time(NULL);
        entry->source_id = source_id;
        entry->error_type = error_type;
        entry->error_code = error_code;
        strncpy(entry->error_message, error_message ? error_message : "Unknown error", 
                sizeof(entry->error_message) - 1);
        entry->error_message[sizeof(entry->error_message) - 1] = '\0';
        entry->recovery_strategy = RECOVERY_STRATEGY_NONE;
        entry->recovery_successful = false;
        entry->retry_count = 0;
        entry->recovery_time = 0;
        
        if (slot_index >= g_error_recovery.error_history_count) {
            g_error_recovery.error_history_count = slot_index + 1;
        }
    }
}

// Find oldest error entry
static int find_oldest_error_entry(void) {
    int oldest_index = -1;
    time_t oldest_time = time(NULL);
    
    for (int i = 0; i < g_error_recovery.max_error_history; i++) {
        if (g_error_recovery.error_history[i].active && 
            g_error_recovery.error_history[i].timestamp < oldest_time) {
            oldest_time = g_error_recovery.error_history[i].timestamp;
            oldest_index = i;
        }
    }
    
    return oldest_index;
}

// Update source error tracking
static void update_source_error_tracking(int source_id, gps_error_type_t error_type) {
    if (source_id < 0 || source_id >= GPS_MAX_SOURCES) {
        return;
    }
    
    gps_source_error_t *source = &g_error_recovery.source_errors[source_id];
    
    source->total_errors++;
    source->last_error = time(NULL);
    
    // Calculate error rate over the error window
    calculate_source_error_rate(source);
    
    // Update retry count and backoff
    if (should_retry_error(source, error_type)) {
        source->current_retry_count++;
        source->last_retry = time(NULL);
        
        // Calculate exponential backoff
        int backoff_delay = calculate_backoff_delay(source->current_retry_count);
        source->backoff_until = time(NULL) + backoff_delay;
        
        LOGX_DEBUG("Source %d retry %d/%d, backoff until %ld", 
                  source_id, source->current_retry_count, g_error_recovery.max_retry_attempts, source->backoff_until);
    } else {
        // Reset retry count if error is not retryable
        source->current_retry_count = 0;
        source->backoff_until = 0;
    }
    
    // Update source status based on error rate
    update_source_status(source);
}

// Calculate source error rate
static void calculate_source_error_rate(gps_source_error_t *source) {
    time_t now = time(NULL);
    time_t window_start = now - g_error_recovery.error_window_size;
    
    int errors_in_window = 0;
    int total_measurements = 0;
    
    // Count errors in the time window
    for (int i = 0; i < g_error_recovery.error_history_count; i++) {
        if (g_error_recovery.error_history[i].active && 
            g_error_recovery.error_history[i].source_id == source->source_id &&
            g_error_recovery.error_history[i].timestamp >= window_start) {
            errors_in_window++;
        }
    }
    
    // Estimate total measurements (this would be more accurate with actual measurement data)
    total_measurements = errors_in_window + (errors_in_window * 10); // Assume 10% error rate
    
    if (total_measurements > 0) {
        source->error_rate = (double)errors_in_window / total_measurements;
    } else {
        source->error_rate = 0.0;
    }
}

// Check if error should be retried
static bool should_retry_error(const gps_source_error_t *source, gps_error_type_t error_type) {
    // Don't retry if we've exceeded max attempts
    if (source->current_retry_count >= g_error_recovery.max_retry_attempts) {
        return false;
    }
    
    // Don't retry if we're in backoff period
    if (time(NULL) < source->backoff_until) {
        return false;
    }
    
    // Don't retry certain error types
    switch (error_type) {
        case GPS_ERROR_TYPE_AUTHENTICATION_ERROR:
        case GPS_ERROR_TYPE_RATE_LIMIT:
        case GPS_ERROR_TYPE_SERVER_ERROR:
            return false;
        default:
            return true;
    }
}

// Calculate backoff delay
static int calculate_backoff_delay(int retry_count) {
    // Exponential backoff with jitter
    int base_delay = g_error_recovery.retry_delay_base * (1 << retry_count);
    
    // Add some random jitter (±20%)
    double jitter = 0.8 + ((double)(rand() % 40) / 100.0);
    int delay = (int)(base_delay * jitter);
    
    // Cap at maximum delay
    return fmin(delay, g_error_recovery.max_retry_delay);
}

// Update source status
static void update_source_status(gps_source_error_t *source) {
    if (source->error_rate > g_error_recovery.error_threshold_ratio) {
        if (source->status == SOURCE_STATUS_ACTIVE) {
            source->status = SOURCE_STATUS_DEGRADED;
            LOGX_WARN("Source %d status changed to DEGRADED (error rate: %.2f%%)", 
                     source->source_id, source->error_rate * 100.0);
        }
    } else if (source->error_rate < g_error_recovery.error_threshold_ratio * 0.5) {
        if (source->status != SOURCE_STATUS_ACTIVE) {
            source->status = SOURCE_STATUS_ACTIVE;
            LOGX_INFO("Source %d status restored to ACTIVE (error rate: %.2f%%)", 
                     source->source_id, source->error_rate * 100.0);
        }
    }
    
    // If too many consecutive errors, mark as failed
    if (source->current_retry_count >= g_error_recovery.max_retry_attempts) {
        source->status = SOURCE_STATUS_FAILED;
        LOGX_ERROR("Source %d status changed to FAILED (max retries exceeded)", source->source_id);
    }
}

// Determine recovery strategy
static gps_recovery_strategy_t determine_recovery_strategy(int source_id, gps_error_type_t error_type) {
    gps_source_error_t *source = &g_error_recovery.source_errors[source_id];
    
    // Check if source is in backoff period
    if (time(NULL) < source->backoff_until) {
        return RECOVERY_STRATEGY_NONE;
    }
    
    // Check if we should retry
    if (should_retry_error(source, error_type)) {
        return RECOVERY_STRATEGY_RETRY;
    }
    
    // Check if we should fallback to another source
    if (source->status == SOURCE_STATUS_FAILED) {
        return RECOVERY_STRATEGY_SWITCH_SOURCE;
    }
    
    // Check if we should degrade service
    if (source->status == SOURCE_STATUS_DEGRADED) {
        return RECOVERY_STRATEGY_DEGRADE;
    }
    
    // Default to no recovery
    return RECOVERY_STRATEGY_NONE;
}

// Attempt error recovery
static bool attempt_error_recovery(int source_id, gps_error_type_t error_type, gps_recovery_strategy_t strategy) {
    gps_source_error_t *source = &g_error_recovery.source_errors[source_id];
    
    switch (strategy) {
        case RECOVERY_STRATEGY_RETRY:
            return perform_retry_recovery(source, error_type);
            
        case RECOVERY_STRATEGY_FALLBACK:
            return perform_fallback_recovery(source, error_type);
            
        case RECOVERY_STRATEGY_RESET:
            return perform_reset_recovery(source, error_type);
            
        case RECOVERY_STRATEGY_DEGRADE:
            return perform_degrade_recovery(source, error_type);
            
        case RECOVERY_STRATEGY_SWITCH_SOURCE:
            return perform_switch_source_recovery(source, error_type);
            
        case RECOVERY_STRATEGY_NONE:
        default:
            return false;
    }
}

// Perform retry recovery
static bool perform_retry_recovery(gps_source_error_t *source, gps_error_type_t error_type) {
    // Simulate retry attempt
    LOGX_DEBUG("Attempting retry recovery for source %d (attempt %d/%d)", 
              source->source_id, source->current_retry_count, g_error_recovery.max_retry_attempts);
    
    // In a real implementation, this would actually retry the operation
    // For now, we'll simulate success with decreasing probability
    double success_probability = 1.0 - (source->current_retry_count * 0.2);
    bool success = ((double)(rand() % 100) / 100.0) < success_probability;
    
    if (success) {
        source->recovered_errors++;
        source->current_retry_count = 0;
        source->backoff_until = 0;
        LOGX_INFO("Retry recovery successful for source %d", source->source_id);
    } else {
        LOGX_WARN("Retry recovery failed for source %d", source->source_id);
    }
    
    return success;
}

// Perform fallback recovery
static bool perform_fallback_recovery(gps_source_error_t *source, gps_error_type_t error_type) {
    LOGX_DEBUG("Attempting fallback recovery for source %d", source->source_id);
    
    // Simulate fallback to backup source
    bool success = ((double)(rand() % 100) / 100.0) < 0.8; // 80% success rate
    
    if (success) {
        source->recovered_errors++;
        LOGX_INFO("Fallback recovery successful for source %d", source->source_id);
    } else {
        LOGX_WARN("Fallback recovery failed for source %d", source->source_id);
    }
    
    return success;
}

// Perform reset recovery
static bool perform_reset_recovery(gps_source_error_t *source, gps_error_type_t error_type) {
    LOGX_DEBUG("Attempting reset recovery for source %d", source->source_id);
    
    // Simulate source reset
    bool success = ((double)(rand() % 100) / 100.0) < 0.7; // 70% success rate
    
    if (success) {
        source->recovered_errors++;
        source->current_retry_count = 0;
        source->backoff_until = 0;
        source->status = SOURCE_STATUS_ACTIVE;
        LOGX_INFO("Reset recovery successful for source %d", source->source_id);
    } else {
        LOGX_WARN("Reset recovery failed for source %d", source->source_id);
    }
    
    return success;
}

// Perform degrade recovery
static bool perform_degrade_recovery(gps_source_error_t *source, gps_error_type_t error_type) {
    LOGX_DEBUG("Attempting degrade recovery for source %d", source->source_id);
    
    // Simulate service degradation
    bool success = ((double)(rand() % 100) / 100.0) < 0.9; // 90% success rate
    
    if (success) {
        source->recovered_errors++;
        LOGX_INFO("Degrade recovery successful for source %d", source->source_id);
    } else {
        LOGX_WARN("Degrade recovery failed for source %d", source->source_id);
    }
    
    return success;
}

// Perform switch source recovery
static bool perform_switch_source_recovery(gps_source_error_t *source, gps_error_type_t error_type) {
    LOGX_DEBUG("Attempting switch source recovery for source %d", source->source_id);
    
    // Simulate switching to another source
    bool success = ((double)(rand() % 100) / 100.0) < 0.85; // 85% success rate
    
    if (success) {
        source->recovered_errors++;
        source->status = SOURCE_STATUS_ACTIVE;
        LOGX_INFO("Switch source recovery successful for source %d", source->source_id);
    } else {
        LOGX_WARN("Switch source recovery failed for source %d", source->source_id);
    }
    
    return success;
}

// Get error recovery status
static int gps_error_recovery_get_status(gps_error_recovery_status_t *status) {
    if (!g_error_recovery_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_error_recovery_mutex);
    
    status->enabled = g_error_recovery.enabled;
    status->error_history_count = g_error_recovery.error_history_count;
    status->max_error_history = g_error_recovery.max_error_history;
    status->total_errors = g_error_recovery.total_errors;
    status->recovered_errors = g_error_recovery.recovered_errors;
    status->unrecovered_errors = g_error_recovery.unrecovered_errors;
    status->last_recovery = g_error_recovery.last_recovery;
    
    // Calculate recovery rate
    if (g_error_recovery.total_errors > 0) {
        status->recovery_rate = (double)g_error_recovery.recovered_errors / g_error_recovery.total_errors;
    } else {
        status->recovery_rate = 0.0;
    }
    
    pthread_mutex_unlock(&g_error_recovery_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get source error information
static int gps_error_recovery_get_source_errors(int source_id, gps_source_error_t *source_errors) {
    if (!g_error_recovery_initialized || !source_errors || source_id < 0 || source_id >= GPS_MAX_SOURCES) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_error_recovery_mutex);
    
    memcpy(source_errors, &g_error_recovery.source_errors[source_id], sizeof(gps_source_error_t));
    
    pthread_mutex_unlock(&g_error_recovery_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get all source error data
static int gps_error_recovery_get_all_sources(gps_source_error_t *sources, int max_sources) {
    if (!g_error_recovery_initialized || !sources || max_sources <= 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_error_recovery_mutex);
    
    int count = 0;
    for (int i = 0; i < GPS_MAX_SOURCES && count < max_sources; i++) {
        if (g_error_recovery.source_errors[i].total_errors > 0) {
            memcpy(&sources[count], &g_error_recovery.source_errors[i], sizeof(gps_source_error_t));
            count++;
        }
    }
    
    pthread_mutex_unlock(&g_error_recovery_mutex);
    
    return count;
}

// Get error history
static int gps_error_recovery_get_history(gps_error_entry_t *history, int max_entries, time_t since) {
    if (!g_error_recovery_initialized || !history || max_entries <= 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_error_recovery_mutex);
    
    int count = 0;
    for (int i = 0; i < g_error_recovery.error_history_count && count < max_entries; i++) {
        if (g_error_recovery.error_history[i].active && 
            g_error_recovery.error_history[i].timestamp >= since) {
            memcpy(&history[count], &g_error_recovery.error_history[i], sizeof(gps_error_entry_t));
            count++;
        }
    }
    
    pthread_mutex_unlock(&g_error_recovery_mutex);
    
    return count;
}

// Get error recovery configuration
static int gps_error_recovery_get_config(gps_error_recovery_config_t *config) {
    if (!g_error_recovery_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_error_recovery_mutex);
    
    config->enabled = g_error_recovery.enabled;
    config->max_error_history = g_error_recovery.max_error_history;
    config->max_retry_attempts = g_error_recovery.max_retry_attempts;
    config->retry_delay_base = g_error_recovery.retry_delay_base;
    config->max_retry_delay = g_error_recovery.max_retry_delay;
    config->error_window_size = g_error_recovery.error_window_size;
    config->error_threshold_ratio = g_error_recovery.error_threshold_ratio;
    
    pthread_mutex_unlock(&g_error_recovery_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set error recovery configuration
static int gps_error_recovery_set_config(const gps_error_recovery_config_t *config) {
    if (!g_error_recovery_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_error_recovery_mutex);
    
    g_error_recovery.enabled = config->enabled;
    g_error_recovery.max_error_history = config->max_error_history;
    g_error_recovery.max_retry_attempts = config->max_retry_attempts;
    g_error_recovery.retry_delay_base = config->retry_delay_base;
    g_error_recovery.max_retry_delay = config->max_retry_delay;
    g_error_recovery.error_window_size = config->error_window_size;
    g_error_recovery.error_threshold_ratio = config->error_threshold_ratio;
    
    pthread_mutex_unlock(&g_error_recovery_mutex);
    
    LOGX_INFO("GPS error recovery configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable error recovery
static int gps_error_recovery_set_enabled(bool enabled) {
    if (!g_error_recovery_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_error_recovery_mutex);
    g_error_recovery.enabled = enabled;
    pthread_mutex_unlock(&g_error_recovery_mutex);
    
    LOGX_INFO("GPS error recovery %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Force error recovery for a source
static int gps_error_recovery_force_recovery(int source_id) {
    if (!g_error_recovery_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    if (source_id < 0 || source_id >= GPS_MAX_SOURCES) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_error_recovery_mutex);
    
    gps_source_error_t *source = &g_error_recovery.source_errors[source_id];
    
    // Reset source error state
    source->current_retry_count = 0;
    source->backoff_until = 0;
    source->status = SOURCE_STATUS_ACTIVE;
    
    pthread_mutex_unlock(&g_error_recovery_mutex);
    
    LOGX_INFO("Forced error recovery for source %d", source_id);
    return AUTONOMY_SUCCESS;
}

// Reset error recovery
static int gps_error_recovery_reset(void) {
    if (!g_error_recovery_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_error_recovery_mutex);
    
    g_error_recovery.error_history_count = 0;
    g_error_recovery.total_errors = 0;
    g_error_recovery.recovered_errors = 0;
    g_error_recovery.unrecovered_errors = 0;
    g_error_recovery.last_recovery = 0;
    
    // Clear error history
    for (int i = 0; i < MAX_ERROR_HISTORY; i++) {
        g_error_recovery.error_history[i].active = false;
        g_error_recovery.error_history[i].timestamp = 0;
        g_error_recovery.error_history[i].source_id = 0;
        g_error_recovery.error_history[i].error_type = GPS_ERROR_TYPE_UNKNOWN;
        g_error_recovery.error_history[i].error_code = 0;
        g_error_recovery.error_history[i].error_message[0] = '\0';
        g_error_recovery.error_history[i].recovery_strategy = RECOVERY_STRATEGY_NONE;
        g_error_recovery.error_history[i].recovery_successful = false;
        g_error_recovery.error_history[i].retry_count = 0;
        g_error_recovery.error_history[i].recovery_time = 0;
    }
    
    // Reset source error tracking
    for (int i = 0; i < GPS_MAX_SOURCES; i++) {
        gps_source_error_t *source = &g_error_recovery.source_errors[i];
        source->total_errors = 0;
        source->recovered_errors = 0;
        source->unrecovered_errors = 0;
        source->last_error = 0;
        source->error_rate = 0.0;
        source->recovery_rate = 0.0;
        source->current_retry_count = 0;
        source->last_retry = 0;
        source->backoff_until = 0;
        source->status = SOURCE_STATUS_ACTIVE;
    }
    
    pthread_mutex_unlock(&g_error_recovery_mutex);
    
    LOGX_INFO("GPS error recovery reset");
    return AUTONOMY_SUCCESS;
}

// Cleanup error recovery
static void gps_error_recovery_cleanup(void) {
    if (!g_error_recovery_initialized) {
        return;
    }
    
    pthread_mutex_destroy(&g_error_recovery_mutex);
    g_error_recovery_initialized = false;
    
    LOGX_INFO("GPS error recovery cleaned up");
}
