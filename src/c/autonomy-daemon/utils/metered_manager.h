#ifndef METERED_MANAGER_H
#define METERED_MANAGER_H

#include <stdbool.h>
#include <time.h>
#include <stdint.h>

// Data usage thresholds
typedef struct {
    uint64_t warning_threshold_bytes;
    uint64_t critical_threshold_bytes;
    uint64_t hard_limit_bytes;
    double warning_percentage;
    double critical_percentage;
} data_thresholds_t;

// Data usage statistics
typedef struct {
    uint64_t current_usage_bytes;
    uint64_t daily_usage_bytes;
    uint64_t monthly_usage_bytes;
    uint64_t total_usage_bytes;
    double current_percentage;
    double daily_percentage;
    double monthly_percentage;
    time_t last_reset;
    time_t billing_cycle_start;
} data_usage_stats_t;

// Metered connection status
typedef struct {
    bool is_metered;
    bool is_roaming;
    char connection_type[32];
    char carrier[64];
    char plan_name[128];
    uint64_t plan_limit_bytes;
    uint64_t remaining_bytes;
    double remaining_percentage;
    time_t last_check;
} metered_connection_status_t;

// Metered manager configuration
typedef struct {
    bool enabled;
    int check_interval_seconds;
    bool auto_throttle;
    bool send_notifications;
    data_thresholds_t thresholds;
    char interfaces[16][32];
    int interface_count;
} metered_manager_config_t;

// Metered manager structure
typedef struct {
    metered_manager_config_t config;
    
    // Current status
    metered_connection_status_t connection_status;
    data_usage_stats_t usage_stats;
    
    // Interface monitoring
    char monitored_interfaces[16][32];
    int monitored_interface_count;
    
    // Statistics
    time_t last_check;
    int check_count;
    int throttling_events;
    int notification_count;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} metered_manager_t;

// Initialize metered manager
int metered_manager_init(const metered_manager_config_t* config);

// Clean up metered manager
void metered_manager_cleanup(void);

// Check metered connection status
int metered_manager_check_status(void);

// Get data usage statistics
int metered_manager_get_usage_stats(data_usage_stats_t* stats);

// Get metered connection status
int metered_manager_get_connection_status(metered_connection_status_t* status);

// Check if connection is metered
bool metered_manager_is_metered(void);

// Check if roaming
bool metered_manager_is_roaming(void);

// Get remaining data
uint64_t metered_manager_get_remaining_data(void);

// Reset usage counters
int metered_manager_reset_usage(void);

// Set data thresholds
int metered_manager_set_thresholds(const data_thresholds_t* thresholds);

// Get metered manager status
void metered_manager_get_status(metered_manager_t* status);

// Check if metered manager is initialized
bool metered_manager_is_initialized(void);

// Get metered manager instance
metered_manager_t* metered_manager_get_instance(void);

#endif // METERED_MANAGER_H
