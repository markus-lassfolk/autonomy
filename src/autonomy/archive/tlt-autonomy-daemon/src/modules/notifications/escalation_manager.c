#include "escalation_manager.h"
#include "smart_manager.h"
#include "emergency_detector.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

// Global escalation manager instance
static escalation_manager_t g_escalation_manager;
static bool g_escalation_manager_initialized = false;

// Forward declarations
static void* escalation_loop(void* arg);
static void process_escalations(void);
static void send_escalation_notification(escalation_chain_t* chain, int level);
static void get_escalation_contacts(notification_type_t alert_type, int severity, 
                                   escalation_contact_t* contacts, int* contact_count);
static void record_escalation_completion(escalation_chain_t* chain);
static double calculate_escalation_effectiveness(escalation_chain_t* chain, time_t response_time);
static char* create_escalation_message(escalation_chain_t* chain, escalation_contact_t* contact);

// Initialize escalation manager
int escalation_manager_init(const escalation_manager_config_t* config) {
    if (g_escalation_manager_initialized) {
        return 0; // Already initialized
    }
    
    if (!config) {
        return -1;
    }
    
    memset(&g_escalation_manager, 0, sizeof(escalation_manager_t));
    
    // Copy configuration
    g_escalation_manager.config = *config;
    
    // Initialize mutex
    g_escalation_manager.mutex = malloc(sizeof(pthread_mutex_t));
    if (!g_escalation_manager.mutex) {
        return -1;
    }
    
    pthread_mutex_init(g_escalation_manager.mutex, NULL);
    
    // Initialize active escalations
    g_escalation_manager.active_escalations = malloc(config->max_active_escalations * sizeof(escalation_chain_t));
    if (!g_escalation_manager.active_escalations) {
        pthread_mutex_destroy(g_escalation_manager.mutex);
        free(g_escalation_manager.mutex);
        return -1;
    }
    
    g_escalation_manager.max_active_escalations = config->max_active_escalations;
    g_escalation_manager.active_escalations_count = 0;
    
    // Initialize escalation history
    g_escalation_manager.escalation_history = malloc(config->max_escalation_history * sizeof(escalation_record_t));
    if (!g_escalation_manager.escalation_history) {
        free(g_escalation_manager.active_escalations);
        pthread_mutex_destroy(g_escalation_manager.mutex);
        free(g_escalation_manager.mutex);
        return -1;
    }
    
    g_escalation_manager.max_escalation_history = config->max_escalation_history;
    g_escalation_manager.escalation_history_count = 0;
    
    // Start escalation monitoring thread
    g_escalation_manager.thread_running = true;
    if (pthread_create(&g_escalation_manager.escalation_thread, NULL, escalation_loop, NULL) != 0) {
        free(g_escalation_manager.escalation_history);
        free(g_escalation_manager.active_escalations);
        pthread_mutex_destroy(g_escalation_manager.mutex);
        free(g_escalation_manager.mutex);
        return -1;
    }
    
    g_escalation_manager_initialized = true;
    return 0;
}

// Clean up escalation manager
void escalation_manager_cleanup(void) {
    if (!g_escalation_manager_initialized) return;
    
    // Stop escalation thread
    g_escalation_manager.thread_running = false;
    pthread_join(g_escalation_manager.escalation_thread, NULL);
    
    if (g_escalation_manager.mutex) {
        pthread_mutex_destroy(g_escalation_manager.mutex);
        free(g_escalation_manager.mutex);
    }
    
    if (g_escalation_manager.active_escalations) {
        free(g_escalation_manager.active_escalations);
    }
    
    if (g_escalation_manager.escalation_history) {
        free(g_escalation_manager.escalation_history);
    }
    
    g_escalation_manager.active_escalations = NULL;
    g_escalation_manager.escalation_history = NULL;
    g_escalation_manager.mutex = NULL;
    g_escalation_manager.active_escalations_count = 0;
    g_escalation_manager.max_active_escalations = 0;
    g_escalation_manager.escalation_history_count = 0;
    g_escalation_manager.max_escalation_history = 0;
    
    g_escalation_manager_initialized = false;
}

// Escalation monitoring thread
static void* escalation_loop(void* arg) {
    (void)arg; // Unused parameter
    
    while (g_escalation_manager.thread_running) {
        process_escalations();
        sleep(30); // Check every 30 seconds
    }
    
    return NULL;
}

// Process active escalations
static void process_escalations(void) {
    if (!g_escalation_manager_initialized) return;
    
    pthread_mutex_lock(g_escalation_manager.mutex);
    
    time_t now = time(NULL);
    
    for (int i = 0; i < g_escalation_manager.active_escalations_count; i++) {
        escalation_chain_t* chain = &g_escalation_manager.active_escalations[i];
        
        if (chain->status != ESCALATION_STATUS_ACTIVE) {
            continue;
        }
        
        if (chain->acknowledged) {
            continue;
        }
        
        // Check if it's time for next escalation
        if (now >= chain->next_escalation && chain->current_level < chain->max_level) {
            chain->current_level++;
            
            // Calculate next escalation time (exponential backoff)
            if (chain->current_level < chain->max_level) {
                time_t next_delay = 300 * chain->current_level; // 5min, 10min, 15min, etc.
                chain->next_escalation = now + next_delay;
            }
            
            printf("ESCALATION: Escalating to level %d for chain %s\n", 
                   chain->current_level, chain->id);
            
            // Send notification for next level (unlock mutex during send)
            pthread_mutex_unlock(g_escalation_manager.mutex);
            send_escalation_notification(chain, chain->current_level);
            pthread_mutex_lock(g_escalation_manager.mutex);
        }
        
        // Check for escalation timeout
        if (chain->current_level >= chain->max_level &&
            (now - chain->start_time) > g_escalation_manager.config.escalation_cooldown_seconds) {
            
            printf("ESCALATION: Chain %s reached maximum level and timed out\n", chain->id);
            
            chain->status = ESCALATION_STATUS_COMPLETED;
            record_escalation_completion(chain);
        }
    }
    
    pthread_mutex_unlock(g_escalation_manager.mutex);
}

// Get escalation contacts based on alert type and severity
static void get_escalation_contacts(notification_type_t alert_type, int severity,
                                   escalation_contact_t* contacts, int* contact_count) {
    (void)alert_type; // May be used for customization in the future
    
    *contact_count = 0;
    
    // Level 1: On-Call Engineer
    escalation_contact_t* contact = &contacts[(*contact_count)++];
    contact->level = 1;
    strncpy(contact->name, "On-Call Engineer", sizeof(contact->name) - 1);
    contact->channels[0] = NOTIFICATION_CHANNEL_PUSHOVER;
    contact->channels[1] = NOTIFICATION_CHANNEL_SLACK;
    contact->channel_count = 2;
    contact->response_timeout_seconds = 300; // 5 minutes
    contact->contacted = false;
    contact->contacted_at = 0;
    contact->responded = false;
    contact->responded_at = 0;
    
    // Level 2: Team Lead
    contact = &contacts[(*contact_count)++];
    contact->level = 2;
    strncpy(contact->name, "Team Lead", sizeof(contact->name) - 1);
    contact->channels[0] = NOTIFICATION_CHANNEL_PUSHOVER;
    contact->channels[1] = NOTIFICATION_CHANNEL_EMAIL;
    contact->channels[2] = NOTIFICATION_CHANNEL_SLACK;
    contact->channel_count = 3;
    contact->response_timeout_seconds = 600; // 10 minutes
    contact->contacted = false;
    contact->contacted_at = 0;
    contact->responded = false;
    contact->responded_at = 0;
    
    // Level 3: Engineering Manager
    contact = &contacts[(*contact_count)++];
    contact->level = 3;
    strncpy(contact->name, "Engineering Manager", sizeof(contact->name) - 1);
    contact->channels[0] = NOTIFICATION_CHANNEL_PUSHOVER;
    contact->channels[1] = NOTIFICATION_CHANNEL_EMAIL;
    contact->channels[2] = NOTIFICATION_CHANNEL_SLACK;
    contact->channels[3] = NOTIFICATION_CHANNEL_TELEGRAM;
    contact->channel_count = 4;
    contact->response_timeout_seconds = 900; // 15 minutes
    contact->contacted = false;
    contact->contacted_at = 0;
    contact->responded = false;
    contact->responded_at = 0;
    
    // Level 4: VP Engineering (for critical emergencies)
    if (severity >= (int)EMERGENCY_LEVEL_CRITICAL) {
        contact = &contacts[(*contact_count)++];
        contact->level = 4;
        strncpy(contact->name, "VP Engineering", sizeof(contact->name) - 1);
        contact->channels[0] = NOTIFICATION_CHANNEL_PUSHOVER;
        contact->channels[1] = NOTIFICATION_CHANNEL_EMAIL;
        contact->channels[2] = NOTIFICATION_CHANNEL_TELEGRAM;
        contact->channel_count = 3;
        contact->response_timeout_seconds = 1200; // 20 minutes
        contact->contacted = false;
        contact->contacted_at = 0;
        contact->responded = false;
        contact->responded_at = 0;
    }
}

// Create escalation message
static char* create_escalation_message(escalation_chain_t* chain, escalation_contact_t* contact) {
    static char message[2048];
    time_t duration = time(NULL) - chain->start_time;
    
    snprintf(message, sizeof(message),
             "🚨 EMERGENCY ESCALATION - Level %d\n\n"
             "📋 Incident Details:\n"
             "• Incident ID: %s\n"
             "• Alert Type: %s\n"
             "• Duration: %ld seconds\n"
             "• Escalation Level: %d/%d\n\n"
             "👤 Escalated To: %s\n"
             "⏰ Response Required Within: %ld seconds\n\n"
             "🔍 Context:\n"
             "• Started: %s"
             "• Previous levels contacted: %d\n"
             "• Acknowledgment required to stop escalation\n\n"
             "⚡ Action Required:\n"
             "1. Acknowledge this escalation immediately\n"
             "2. Investigate the incident\n"
             "3. Take corrective action\n"
             "4. Update incident status\n\n"
             "🆘 To acknowledge: Use escalation ID %s",
             contact->level,
             chain->incident_id,
             notification_type_to_string(chain->alert_type),
             duration,
             chain->current_level,
             chain->max_level,
             contact->name,
             contact->response_timeout_seconds,
             ctime(&chain->start_time),
             contact->level - 1,
             chain->id);
    
    // Add context if available
    if (strlen(chain->context_json) > 0) {
        strncat(message, "\n\n📊 Additional Context:\n", sizeof(message) - strlen(message) - 1);
        strncat(message, chain->context_json, sizeof(message) - strlen(message) - 1);
    }
    
    return message;
}

// Send escalation notification
static void send_escalation_notification(escalation_chain_t* chain, int level) {
    if (!chain) return;
    
    // Find contact for this level
    escalation_contact_t* contact = NULL;
    for (int i = 0; i < chain->contact_count; i++) {
        if (chain->contacts[i].level == level) {
            contact = &chain->contacts[i];
            break;
        }
    }
    
    if (!contact) {
        printf("ERROR: No contact found for escalation level %d\n", level);
        return;
    }
    
    // Mark as contacted
    time_t now = time(NULL);
    contact->contacted = true;
    contact->contacted_at = now;
    
    // Create escalation notification
    notification_event_t event;
    memset(&event, 0, sizeof(event));
    
    snprintf(event.id, sizeof(event.id), "esc_%s_%d_%ld", chain->id, level, now);
    snprintf(event.title, sizeof(event.title), "🚨 ESCALATION LEVEL %d: %s", 
             level, notification_type_to_string(chain->alert_type));
    
    char* message = create_escalation_message(chain, contact);
    strncpy(event.message, message, sizeof(event.message) - 1);
    
    event.type = chain->alert_type;
    event.priority = NOTIFICATION_PRIORITY_EMERGENCY;
    event.timestamp = now;
    
    printf("ESCALATION: Sending level %d notification to %s for chain %s\n",
           level, contact->name, chain->id);
    
    // Send through smart manager if available
    if (smart_notification_manager_is_initialized()) {
        smart_notification_manager_send(&event);
    } else if (notification_manager_is_initialized()) {
        notification_manager_send(event.type, event.title, event.message, 
                                event.priority, NULL);
    }
}

// Record escalation completion
static void record_escalation_completion(escalation_chain_t* chain) {
    if (!chain) return;
    
    time_t end_time = time(NULL);
    time_t duration = end_time - chain->start_time;
    
    // Calculate response time
    time_t response_time = duration;
    if (chain->acknowledged && chain->acknowledged_at > 0) {
        response_time = chain->acknowledged_at - chain->start_time;
    }
    
    // Calculate effectiveness
    double effectiveness = calculate_escalation_effectiveness(chain, response_time);
    
    // Add to history
    if (g_escalation_manager.escalation_history_count < g_escalation_manager.max_escalation_history) {
        int index = g_escalation_manager.escalation_history_count;
        escalation_record_t* record = &g_escalation_manager.escalation_history[index];
        
        strncpy(record->id, chain->id, sizeof(record->id) - 1);
        strncpy(record->incident_id, chain->incident_id, sizeof(record->incident_id) - 1);
        record->alert_type = chain->alert_type;
        record->start_time = chain->start_time;
        record->end_time = end_time;
        record->duration_seconds = duration;
        record->max_level_reached = chain->current_level;
        record->total_contacts = chain->contact_count;
        record->response_time_seconds = response_time;
        record->status = chain->status;
        record->effectiveness = effectiveness;
        
        g_escalation_manager.escalation_history_count++;
    } else {
        // Shift history and add at end
        for (int i = 0; i < g_escalation_manager.max_escalation_history - 1; i++) {
            g_escalation_manager.escalation_history[i] = g_escalation_manager.escalation_history[i + 1];
        }
        
        int index = g_escalation_manager.max_escalation_history - 1;
        escalation_record_t* record = &g_escalation_manager.escalation_history[index];
        
        strncpy(record->id, chain->id, sizeof(record->id) - 1);
        strncpy(record->incident_id, chain->incident_id, sizeof(record->incident_id) - 1);
        record->alert_type = chain->alert_type;
        record->start_time = chain->start_time;
        record->end_time = end_time;
        record->duration_seconds = duration;
        record->max_level_reached = chain->current_level;
        record->total_contacts = chain->contact_count;
        record->response_time_seconds = response_time;
        record->status = chain->status;
        record->effectiveness = effectiveness;
    }
    
    // Remove from active escalations
    for (int i = 0; i < g_escalation_manager.active_escalations_count; i++) {
        if (strcmp(g_escalation_manager.active_escalations[i].id, chain->id) == 0) {
            // Shift remaining escalations
            for (int j = i; j < g_escalation_manager.active_escalations_count - 1; j++) {
                g_escalation_manager.active_escalations[j] = g_escalation_manager.active_escalations[j + 1];
            }
            g_escalation_manager.active_escalations_count--;
            break;
        }
    }
}

// Calculate escalation effectiveness
static double calculate_escalation_effectiveness(escalation_chain_t* chain, time_t response_time) {
    double effectiveness = 1.0;
    
    // Reduce effectiveness for longer response times
    if (response_time > 1800) { // 30 minutes
        effectiveness *= 0.3;
    } else if (response_time > 900) { // 15 minutes
        effectiveness *= 0.6;
    } else if (response_time > 300) { // 5 minutes
        effectiveness *= 0.8;
    }
    
    // Reduce effectiveness if escalation was cancelled
    if (chain->status == ESCALATION_STATUS_CANCELLED) {
        effectiveness *= 0.2;
    }
    
    // Reduce effectiveness for higher escalation levels reached
    if (chain->current_level > 2) {
        effectiveness *= 0.7;
    }
    
    return effectiveness;
}

// Trigger emergency escalation
int escalation_manager_trigger_emergency_escalation(const char* incident_id,
                                                   notification_type_t alert_type,
                                                   int severity,
                                                   const char* context_json) {
    if (!g_escalation_manager_initialized || !incident_id) {
        return -1;
    }
    
    if (!g_escalation_manager.config.escalation_enabled) {
        return 0; // Escalation disabled
    }
    
    pthread_mutex_lock(g_escalation_manager.mutex);
    
    // Check if escalation already exists for this incident
    for (int i = 0; i < g_escalation_manager.active_escalations_count; i++) {
        if (strcmp(g_escalation_manager.active_escalations[i].incident_id, incident_id) == 0) {
            pthread_mutex_unlock(g_escalation_manager.mutex);
            return 0; // Already escalating
        }
    }
    
    // Check if we have space for more escalations
    if (g_escalation_manager.active_escalations_count >= g_escalation_manager.max_active_escalations) {
        pthread_mutex_unlock(g_escalation_manager.mutex);
        return -1; // No space
    }
    
    // Create new escalation chain
    int index = g_escalation_manager.active_escalations_count;
    escalation_chain_t* chain = &g_escalation_manager.active_escalations[index];
    
    time_t now = time(NULL);
    snprintf(chain->id, sizeof(chain->id), "esc_%s_%ld", incident_id, now);
    strncpy(chain->incident_id, incident_id, sizeof(chain->incident_id) - 1);
    chain->alert_type = alert_type;
    chain->start_time = now;
    chain->current_level = 1;
    chain->max_level = g_escalation_manager.config.max_escalation_level;
    chain->next_escalation = now + g_escalation_manager.config.first_escalation_delay_seconds;
    chain->status = ESCALATION_STATUS_ACTIVE;
    chain->acknowledged = false;
    chain->acknowledged_by[0] = '\0';
    chain->acknowledged_at = 0;
    
    if (context_json) {
        strncpy(chain->context_json, context_json, sizeof(chain->context_json) - 1);
    } else {
        chain->context_json[0] = '\0';
    }
    
    // Get escalation contacts
    get_escalation_contacts(alert_type, severity, chain->contacts, &chain->contact_count);
    
    g_escalation_manager.active_escalations_count++;
    
    printf("ESCALATION: Emergency escalation triggered for incident %s (ID: %s)\n",
           incident_id, chain->id);
    
    pthread_mutex_unlock(g_escalation_manager.mutex);
    
    // Send initial emergency notification
    send_escalation_notification(chain, 1);
    
    return 0;
}

// Acknowledge escalation
int escalation_manager_acknowledge_escalation(const char* escalation_id, const char* acknowledged_by) {
    if (!g_escalation_manager_initialized || !escalation_id || !acknowledged_by) {
        return -1;
    }
    
    pthread_mutex_lock(g_escalation_manager.mutex);
    
    escalation_chain_t* chain = NULL;
    for (int i = 0; i < g_escalation_manager.active_escalations_count; i++) {
        if (strcmp(g_escalation_manager.active_escalations[i].id, escalation_id) == 0) {
            chain = &g_escalation_manager.active_escalations[i];
            break;
        }
    }
    
    if (!chain) {
        pthread_mutex_unlock(g_escalation_manager.mutex);
        return -1; // Escalation not found
    }
    
    if (chain->acknowledged) {
        pthread_mutex_unlock(g_escalation_manager.mutex);
        return -1; // Already acknowledged
    }
    
    // Mark as acknowledged
    time_t now = time(NULL);
    chain->acknowledged = true;
    strncpy(chain->acknowledged_by, acknowledged_by, sizeof(chain->acknowledged_by) - 1);
    chain->acknowledged_at = now;
    chain->status = ESCALATION_STATUS_PAUSED;
    
    printf("ESCALATION: Escalation %s acknowledged by %s\n", escalation_id, acknowledged_by);
    
    // Record completion
    record_escalation_completion(chain);
    
    pthread_mutex_unlock(g_escalation_manager.mutex);
    return 0;
}

// Cancel escalation
int escalation_manager_cancel_escalation(const char* escalation_id, const char* reason) {
    if (!g_escalation_manager_initialized || !escalation_id) {
        return -1;
    }
    
    pthread_mutex_lock(g_escalation_manager.mutex);
    
    escalation_chain_t* chain = NULL;
    for (int i = 0; i < g_escalation_manager.active_escalations_count; i++) {
        if (strcmp(g_escalation_manager.active_escalations[i].id, escalation_id) == 0) {
            chain = &g_escalation_manager.active_escalations[i];
            break;
        }
    }
    
    if (!chain) {
        pthread_mutex_unlock(g_escalation_manager.mutex);
        return -1; // Escalation not found
    }
    
    chain->status = ESCALATION_STATUS_CANCELLED;
    
    printf("ESCALATION: Escalation %s cancelled. Reason: %s\n", 
           escalation_id, reason ? reason : "No reason provided");
    
    // Record completion
    record_escalation_completion(chain);
    
    pthread_mutex_unlock(g_escalation_manager.mutex);
    return 0;
}

// Get active escalations
int escalation_manager_get_active_escalations(escalation_chain_t* escalations, int max_escalations) {
    if (!g_escalation_manager_initialized || !escalations || max_escalations <= 0) {
        return -1;
    }
    
    pthread_mutex_lock(g_escalation_manager.mutex);
    
    int count = (max_escalations < g_escalation_manager.active_escalations_count) ? 
                max_escalations : g_escalation_manager.active_escalations_count;
    
    for (int i = 0; i < count; i++) {
        escalations[i] = g_escalation_manager.active_escalations[i];
    }
    
    pthread_mutex_unlock(g_escalation_manager.mutex);
    return count;
}

// Get escalation history
int escalation_manager_get_escalation_history(escalation_record_t* history, int max_history) {
    if (!g_escalation_manager_initialized || !history || max_history <= 0) {
        return -1;
    }
    
    pthread_mutex_lock(g_escalation_manager.mutex);
    
    int count = (max_history < g_escalation_manager.escalation_history_count) ? 
                max_history : g_escalation_manager.escalation_history_count;
    
    // Return most recent records
    int start_index = g_escalation_manager.escalation_history_count - count;
    if (start_index < 0) start_index = 0;
    
    for (int i = 0; i < count; i++) {
        history[i] = g_escalation_manager.escalation_history[start_index + i];
    }
    
    pthread_mutex_unlock(g_escalation_manager.mutex);
    return count;
}

// Get escalation manager status
void escalation_manager_get_status(escalation_manager_status_t* status) {
    if (!status || !g_escalation_manager_initialized) return;
    
    pthread_mutex_lock(g_escalation_manager.mutex);
    
    status->enabled = g_escalation_manager.config.escalation_enabled;
    status->active_escalations_count = g_escalation_manager.active_escalations_count;
    status->max_active_escalations = g_escalation_manager.max_active_escalations;
    status->escalation_history_count = g_escalation_manager.escalation_history_count;
    status->max_escalation_history = g_escalation_manager.max_escalation_history;
    
    // Calculate averages
    if (g_escalation_manager.escalation_history_count > 0) {
        double total_response_time = 0.0;
        double total_effectiveness = 0.0;
        
        for (int i = 0; i < g_escalation_manager.escalation_history_count; i++) {
            total_response_time += (double)g_escalation_manager.escalation_history[i].response_time_seconds;
            total_effectiveness += g_escalation_manager.escalation_history[i].effectiveness;
        }
        
        status->average_response_time_seconds = total_response_time / (double)g_escalation_manager.escalation_history_count;
        status->average_effectiveness = total_effectiveness / (double)g_escalation_manager.escalation_history_count;
    } else {
        status->average_response_time_seconds = 0.0;
        status->average_effectiveness = 0.0;
    }
    
    pthread_mutex_unlock(g_escalation_manager.mutex);
}

// Check if escalation manager is initialized
bool escalation_manager_is_initialized(void) {
    return g_escalation_manager_initialized;
}

// Get escalation manager instance
escalation_manager_t* escalation_manager_get_instance(void) {
    return g_escalation_manager_initialized ? &g_escalation_manager : NULL;
}