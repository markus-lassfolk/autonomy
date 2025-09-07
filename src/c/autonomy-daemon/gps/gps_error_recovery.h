#ifndef GPS_ERROR_RECOVERY_H
#define GPS_ERROR_RECOVERY_H

#include "../core/types.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

// GPS source constants
#define GPS_MAX_SOURCES 32

// GPS error types

// Note: gps_recovery_strategy_t is defined in ../core/types.h
// Note: source_status_t and related constants defined locally

// Source status for error recovery
typedef enum {
    SOURCE_STATUS_ACTIVE = 0,
    SOURCE_STATUS_DEGRADED,
    SOURCE_STATUS_FAILED,
    SOURCE_STATUS_MAINTENANCE,
    SOURCE_STATUS_MAX
} source_status_t;

// Recovery strategy constants
#define RECOVERY_STRATEGY_NONE 0
#define RECOVERY_STRATEGY_RETRY 1
#define RECOVERY_STRATEGY_FALLBACK 2
#define RECOVERY_STRATEGY_RESET 3
#define RECOVERY_STRATEGY_DEGRADE 4
#define RECOVERY_STRATEGY_SWITCH_SOURCE 5

// Error entry structure
typedef struct {
    bool active;                        // Whether entry is active
    time_t timestamp;                   // Error timestamp
    int source_id;                      // GPS source ID
    gps_error_type_t error_type;        // Error type
    int error_code;                     // Error code
    char error_message[256];            // Error message
    gps_recovery_strategy_t recovery_strategy; // Recovery strategy used
    bool recovery_successful;           // Whether recovery was successful
    int retry_count;                    // Number of retry attempts
    time_t recovery_time;               // Recovery timestamp
} gps_error_entry_t;

// Source error structure
typedef struct {
    int source_id;                      // GPS source ID
    int total_errors;                   // Total errors
    int recovered_errors;               // Recovered errors
    int unrecovered_errors;             // Unrecovered errors
    time_t last_error;                  // Last error timestamp
    double error_rate;                  // Error rate (0-1)
    double recovery_rate;               // Recovery rate (0-1)
    int current_retry_count;            // Current retry count
    time_t last_retry;                  // Last retry timestamp
    time_t backoff_until;               // Backoff until timestamp
    source_status_t status;             // Source status
} gps_source_error_local_t; // Local version with extended fields

// Note: gps_source_error_t is defined in ../core/types.h

// Error recovery configuration structure
typedef struct {
    bool enabled;                       // Enable/disable recovery
    int max_error_history;              // Maximum error history entries
    int max_retry_attempts;             // Maximum retry attempts
    int retry_delay_base;               // Base retry delay in milliseconds
    int max_retry_delay;                // Maximum retry delay in milliseconds
    int error_window_size;              // Error window size in seconds
    double error_threshold_ratio;       // Error threshold ratio
} gps_error_recovery_config_t;

// Error recovery status structure
typedef struct {
    bool enabled;                       // Recovery enabled
    int error_history_count;            // Current error history entries
    int max_error_history;              // Maximum error history entries
    int total_errors;                   // Total errors
    int recovered_errors;               // Recovered errors
    int unrecovered_errors;             // Unrecovered errors
    time_t last_recovery;               // Last recovery timestamp
    double recovery_rate;               // Recovery rate (0-1)
} gps_error_recovery_status_t;

// Error recovery system state structure
typedef struct {
    bool enabled;                       // Recovery enabled
    int max_error_history;              // Maximum error history entries
    int max_retry_attempts;             // Maximum retry attempts
    int retry_delay_base;               // Base retry delay
    int max_retry_delay;                // Maximum retry delay
    int error_window_size;              // Error window size
    double error_threshold_ratio;       // Error threshold ratio
    
    // State
    int error_history_count;            // Error history count
    int total_errors;                   // Total errors
    int recovered_errors;               // Recovered errors
    int unrecovered_errors;             // Unrecovered errors
    time_t last_recovery;               // Last recovery
    
    // Error history
    gps_error_entry_t error_history[1000]; // Error history entries
    
    // Source error tracking  
    gps_source_error_local_t source_errors[GPS_MAX_SOURCES]; // Source error data
} gps_error_recovery_t;

// Function prototypes

/**
 * Initialize GPS error recovery
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_error_recovery_init(void);

/**
 * Record GPS error
 * @param source_id GPS source ID
 * @param error_type Error type
 * @param error_code Error code
 * @param error_message Error message
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_error_recovery_record_error(int source_id, gps_error_type_t error_type, int error_code, const char *error_message);

/**
 * Get error recovery status
 * @param status Status structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_error_recovery_get_status(gps_error_recovery_status_t *status);

/**
 * Get source error information
 * @param source_id GPS source ID
 * @param source_errors Source error structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_error_recovery_get_source_errors(int source_id, gps_source_error_local_t *source_errors);

/**
 * Get all source error data
 * @param sources Array to store source error data
 * @param max_sources Maximum number of sources to retrieve
 * @return Number of sources retrieved
 */
int gps_error_recovery_get_all_sources(gps_source_error_local_t *sources, int max_sources);

/**
 * Get error history
 * @param history Array to store error history
 * @param max_entries Maximum number of entries to retrieve
 * @param since Timestamp to get history since
 * @return Number of history entries retrieved
 */
int gps_error_recovery_get_history(gps_error_entry_t *history, int max_entries, time_t since);

/**
 * Get error recovery configuration
 * @param config Configuration structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_error_recovery_get_config(gps_error_recovery_config_t *config);

/**
 * Set error recovery configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_error_recovery_set_config(const gps_error_recovery_config_t *config);

/**
 * Enable/disable error recovery
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_error_recovery_set_enabled(bool enabled);

/**
 * Force error recovery for a source
 * @param source_id GPS source ID
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_error_recovery_force_recovery(int source_id);

/**
 * Reset error recovery
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_error_recovery_reset(void);

/**
 * Cleanup error recovery
 */
void gps_error_recovery_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_ERROR_RECOVERY_H
// Note: gps_error_type_t, gps_recovery_strategy_t, and gps_source_error_t are defined in ../core/types.h
