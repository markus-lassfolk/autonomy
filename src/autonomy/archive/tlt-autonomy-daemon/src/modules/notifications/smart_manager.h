#ifndef SMART_MANAGER_H
#define SMART_MANAGER_H

#include "notification_types.h"
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

// Time range for suppression rules
typedef struct {
    char start[8];      // "HH:MM"
    char end[8];        // "HH:MM"
    char days[128];     // "monday,tuesday,wednesday"
    char timezone[64];
} time_range_t;

// Suppression rule
typedef struct {
    char id[64];
    char name[128];
    bool enabled;
    int priorities[8];
    int priority_count;
    notification_type_t types[8];
    int type_count;
    time_range_t time_ranges[8];
    int time_ranges_count;
    time_t created_at;
    time_t expires_at;
} suppression_rule_t;

// Smart manager statistics
typedef struct {
    uint64_t total_sent;
    uint64_t total_suppressed;
    uint64_t total_failed;
    uint64_t total_deduped;
    uint64_t rate_limited;
    uint64_t adaptive_adjustments;
    uint64_t last_hour;
    uint64_t last_day;
    time_t average_latency;
    time_t max_latency;
    time_t last_updated;
} smart_manager_stats_t;

// Smart manager status
typedef struct {
    bool enabled;
    bool quiet_hours;
    char quiet_hours_start[8];
    char quiet_hours_end[8];
    int suppression_rules_count;
    int history_count;
    int max_history_size;
    int max_suppression_rules;
} smart_manager_status_t;

// Smart manager structure
typedef struct {
    smart_manager_config_t config;
    
    // State tracking
    time_t last_notification[32];  // Indexed by notification type
    notification_record_t* notification_history;
    int max_history_size;
    int history_count;
    
    // Suppression rules
    suppression_rule_t* suppression_rules;
    int max_suppression_rules;
    int suppression_rules_count;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
    
    // Statistics
    smart_manager_stats_t stats;
} smart_notification_manager_t;

// Initialize smart notification manager
int smart_notification_manager_init(const smart_manager_config_t* config);

// Clean up smart notification manager
void smart_notification_manager_cleanup(void);

// Send notification through smart manager
int smart_notification_manager_send(const notification_event_t* event);

// Add suppression rule
int smart_notification_manager_add_suppression_rule(const suppression_rule_t* rule);

// Remove suppression rule
int smart_notification_manager_remove_suppression_rule(const char* rule_id);

// Get smart manager status
void smart_notification_manager_get_status(smart_manager_status_t* status);

// Get smart manager statistics
void smart_notification_manager_get_stats(smart_manager_stats_t* stats);

// Reset smart manager statistics
void smart_notification_manager_reset_stats(void);

// Check if smart manager is initialized
bool smart_notification_manager_is_initialized(void);

// Get smart manager instance
smart_notification_manager_t* smart_notification_manager_get_instance(void);

#endif // SMART_MANAGER_H
