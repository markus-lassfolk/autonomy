#ifndef CONTEXTUAL_ALERTS_H
#define CONTEXTUAL_ALERTS_H

#include "notification_types.h"
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

// Alert types
typedef enum {
    ALERT_TYPE_SYSTEM_HEALTH = 0,
    ALERT_TYPE_NETWORK_ISSUE,
    ALERT_TYPE_PERFORMANCE_DEGRADATION,
    ALERT_TYPE_SECURITY_THREAT,
    ALERT_TYPE_MAINTENANCE_REQUIRED,
    ALERT_TYPE_WEATHER_ALERT,
    ALERT_TYPE_LOCATION_CHANGE,
    ALERT_TYPE_USER_ACTIVITY
} alert_type_t;

// Location context
typedef struct {
    double latitude;
    double longitude;
    double accuracy;
    char address[256];
    char timezone[64];
    char weather_condition[64];
    double temperature;
    double humidity;
    double wind_speed;
    double visibility;
    double precipitation;
    double speed;
    double direction;
    double distance_moved;
    bool is_stationary;
    time_t stationary_time;
    time_t timestamp;
} location_context_t;

// System load metrics
typedef struct {
    double cpu_usage;
    double memory_usage;
    double disk_usage;
    double temperature;
    time_t uptime;
} system_load_metrics_t;

// System info
typedef struct {
    char hostname[128];
    char model[128];
    char firmware[128];
    char serial_number[128];
    time_t uptime;
    time_t local_time;
} system_info_t;

// Use shared network interface type
#include "../shared/core/common_types.h"

// Network topology
typedef struct {
    network_interface_t interfaces[16];
    int interface_count;
    char gateway[64];
    char dns_servers[256];
} network_topology_t;

// Maintenance status
typedef struct {
    bool maintenance_mode;
    time_t last_maintenance;
    time_t next_maintenance;
    char maintenance_notes[512];
} maintenance_status_t;

// Alert template
typedef struct {
    alert_type_t alert_type;
    char name[128];
    char title[128];
    char message[512];
    notification_priority_t priority;
    bool enabled;
} alert_template_t;

// Context rule
typedef struct {
    char id[64];
    char name[128];
    bool enabled;
    alert_type_t alert_type;
    char condition[512];
    char action[512];
    int priority;
} context_rule_t;

// State key-value pair
typedef struct {
    char key[128];
    char value[512];
    time_t timestamp;
} state_key_value_t;

// Contextual alert
typedef struct {
    alert_type_t alert_type;
    time_t timestamp;
    char title[128];
    char message[512];
    location_context_t* location;
    system_load_metrics_t* system_load;
} contextual_alert_t;

// Contextual alert configuration
typedef struct {
    bool enabled;
    int max_templates;
    int max_context_rules;
    int max_state_keys;
    int max_alert_history;
    bool location_enabled;
    bool weather_enabled;
    bool system_metrics_enabled;
} contextual_alert_config_t;

// Contextual alert status
typedef struct {
    bool enabled;
    int template_count;
    int max_templates;
    int context_rules_count;
    int max_context_rules;
    int state_keys_count;
    int max_state_keys;
    int alert_history_count;
    int max_alert_history;
} contextual_alert_status_t;

// Contextual alert manager structure
typedef struct {
    contextual_alert_config_t config;
    
    // Alert templates
    alert_template_t* alert_templates;
    int max_templates;
    int template_count;
    
    // Context rules
    context_rule_t* context_rules;
    int max_context_rules;
    int context_rules_count;
    
    // State tracking
    state_key_value_t* last_known_state;
    int max_state_keys;
    int state_keys_count;
    
    // Alert history
    contextual_alert_t* alert_history;
    int max_alert_history;
    int alert_history_count;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} contextual_alert_manager_t;

// Initialize contextual alert manager
int contextual_alert_manager_init(const contextual_alert_config_t* config);

// Clean up contextual alert manager
void contextual_alert_manager_cleanup(void);

// Add alert template
int contextual_alert_manager_add_template(const alert_template_t* template);

// Add context rule
int contextual_alert_manager_add_context_rule(const context_rule_t* rule);

// Update system state
int contextual_alert_manager_update_state(const char* key, const char* value, time_t timestamp);

// Get system state value
const char* contextual_alert_manager_get_state(const char* key);

// Send contextual alert
int contextual_alert_manager_send_alert(alert_type_t alert_type, const char* title, const char* message,
                                      const location_context_t* location, const system_load_metrics_t* system_load);

// Get contextual alert manager status
void contextual_alert_manager_get_status(contextual_alert_status_t* status);

// Get alert history
int contextual_alert_manager_get_alert_history(contextual_alert_t* alerts, int max_alerts);

// Check if contextual alert manager is initialized
bool contextual_alert_manager_is_initialized(void);

// Get contextual alert manager instance
contextual_alert_manager_t* contextual_alert_manager_get_instance(void);

#endif // CONTEXTUAL_ALERTS_H
