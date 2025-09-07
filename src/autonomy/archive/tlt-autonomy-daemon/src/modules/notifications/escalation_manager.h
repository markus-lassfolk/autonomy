#ifndef ESCALATION_MANAGER_H
#define ESCALATION_MANAGER_H

#include "notification_types.h"
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

// Escalation status
typedef enum {
    ESCALATION_STATUS_ACTIVE = 0,
    ESCALATION_STATUS_PAUSED,
    ESCALATION_STATUS_COMPLETED,
    ESCALATION_STATUS_CANCELLED
} escalation_status_t;

// Escalation contact
typedef struct {
    int level;
    char name[128];
    notification_channel_t channels[8];
    int channel_count;
    time_t response_timeout_seconds;
    bool contacted;
    time_t contacted_at;
    bool responded;
    time_t responded_at;
} escalation_contact_t;

// Escalation chain
typedef struct {
    char id[64];
    char incident_id[64];
    notification_type_t alert_type;
    time_t start_time;
    int current_level;
    int max_level;
    time_t next_escalation;
    escalation_contact_t contacts[8];
    int contact_count;
    escalation_status_t status;
    bool acknowledged;
    char acknowledged_by[128];
    time_t acknowledged_at;
    char context_json[1024];
} escalation_chain_t;

// Escalation record
typedef struct {
    char id[64];
    char incident_id[64];
    notification_type_t alert_type;
    time_t start_time;
    time_t end_time;
    time_t duration_seconds;
    int max_level_reached;
    int total_contacts;
    time_t response_time_seconds;
    escalation_status_t status;
    double effectiveness;
} escalation_record_t;

// Escalation configuration
typedef struct {
    bool escalation_enabled;
    int max_escalation_level;
    time_t escalation_cooldown_seconds;
    time_t first_escalation_delay_seconds;
    int max_active_escalations;
    int max_escalation_history;
} escalation_manager_config_t;

// Escalation manager status
typedef struct {
    bool enabled;
    int active_escalations_count;
    int max_active_escalations;
    int escalation_history_count;
    int max_escalation_history;
    double average_response_time_seconds;
    double average_effectiveness;
} escalation_manager_status_t;

// Escalation manager structure
typedef struct {
    escalation_manager_config_t config;
    
    // Active escalations
    escalation_chain_t* active_escalations;
    int max_active_escalations;
    int active_escalations_count;
    
    // Escalation history
    escalation_record_t* escalation_history;
    int max_escalation_history;
    int escalation_history_count;
    
    // Thread management
    pthread_t escalation_thread;
    bool thread_running;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} escalation_manager_t;

// Initialize escalation manager
int escalation_manager_init(const escalation_manager_config_t* config);

// Clean up escalation manager
void escalation_manager_cleanup(void);

// Trigger emergency escalation
int escalation_manager_trigger_emergency_escalation(const char* incident_id,
                                                   notification_type_t alert_type,
                                                   int severity,
                                                   const char* context_json);

// Acknowledge escalation
int escalation_manager_acknowledge_escalation(const char* escalation_id, const char* acknowledged_by);

// Cancel escalation
int escalation_manager_cancel_escalation(const char* escalation_id, const char* reason);

// Get active escalations
int escalation_manager_get_active_escalations(escalation_chain_t* escalations, int max_escalations);

// Get escalation history
int escalation_manager_get_escalation_history(escalation_record_t* history, int max_history);

// Get escalation manager status
void escalation_manager_get_status(escalation_manager_status_t* status);

// Check if escalation manager is initialized
bool escalation_manager_is_initialized(void);

// Get escalation manager instance
escalation_manager_t* escalation_manager_get_instance(void);

#endif // ESCALATION_MANAGER_H