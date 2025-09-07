#include "contextual_alerts.h"
#include "smart_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

// Global contextual alert manager instance
static contextual_alert_manager_t g_contextual_manager;
static bool g_contextual_manager_initialized = false;

// Initialize contextual alert manager
int contextual_alert_manager_init(const contextual_alert_config_t* config) {
    if (g_contextual_manager_initialized) {
        return 0; // Already initialized
    }
    
    if (!config) {
        return -1;
    }
    
    memset(&g_contextual_manager, 0, sizeof(contextual_alert_manager_t));
    
    // Copy configuration
    g_contextual_manager.config = *config;
    
    // Initialize mutex
    g_contextual_manager.mutex = malloc(sizeof(pthread_mutex_t));
    if (!g_contextual_manager.mutex) {
        return -1;
    }
    
    pthread_mutex_init(g_contextual_manager.mutex, NULL);
    
    // Initialize alert templates
    g_contextual_manager.alert_templates = malloc(config->max_templates * sizeof(alert_template_t));
    if (!g_contextual_manager.alert_templates) {
        pthread_mutex_destroy(g_contextual_manager.mutex);
        free(g_contextual_manager.mutex);
        return -1;
    }
    
    g_contextual_manager.max_templates = config->max_templates;
    g_contextual_manager.template_count = 0;
    
    // Initialize context rules
    g_contextual_manager.context_rules = malloc(config->max_context_rules * sizeof(context_rule_t));
    if (!g_contextual_manager.context_rules) {
        free(g_contextual_manager.alert_templates);
        pthread_mutex_destroy(g_contextual_manager.mutex);
        free(g_contextual_manager.mutex);
        return -1;
    }
    
    g_contextual_manager.max_context_rules = config->max_context_rules;
    g_contextual_manager.context_rules_count = 0;
    
    // Initialize state tracking
    g_contextual_manager.last_known_state = malloc(config->max_state_keys * sizeof(state_key_value_t));
    if (!g_contextual_manager.last_known_state) {
        free(g_contextual_manager.context_rules);
        free(g_contextual_manager.alert_templates);
        pthread_mutex_destroy(g_contextual_manager.mutex);
        free(g_contextual_manager.mutex);
        return -1;
    }
    
    g_contextual_manager.max_state_keys = config->max_state_keys;
    g_contextual_manager.state_keys_count = 0;
    
    // Initialize alert history
    g_contextual_manager.alert_history = malloc(config->max_alert_history * sizeof(contextual_alert_t));
    if (!g_contextual_manager.alert_history) {
        free(g_contextual_manager.last_known_state);
        free(g_contextual_manager.context_rules);
        free(g_contextual_manager.alert_templates);
        pthread_mutex_destroy(g_contextual_manager.mutex);
        free(g_contextual_manager.mutex);
        return -1;
    }
    
    g_contextual_manager.max_alert_history = config->max_alert_history;
    g_contextual_manager.alert_history_count = 0;
    
    g_contextual_manager_initialized = true;
    return 0;
}

// Clean up contextual alert manager
void contextual_alert_manager_cleanup(void) {
    if (!g_contextual_manager_initialized) return;
    
    if (g_contextual_manager.mutex) {
        pthread_mutex_destroy(g_contextual_manager.mutex);
        free(g_contextual_manager.mutex);
    }
    
    if (g_contextual_manager.alert_templates) {
        free(g_contextual_manager.alert_templates);
    }
    
    if (g_contextual_manager.context_rules) {
        free(g_contextual_manager.context_rules);
    }
    
    if (g_contextual_manager.last_known_state) {
        free(g_contextual_manager.last_known_state);
    }
    
    if (g_contextual_manager.alert_history) {
        free(g_contextual_manager.alert_history);
    }
    
    g_contextual_manager.alert_templates = NULL;
    g_contextual_manager.context_rules = NULL;
    g_contextual_manager.last_known_state = NULL;
    g_contextual_manager.alert_history = NULL;
    g_contextual_manager.mutex = NULL;
    g_contextual_manager.template_count = 0;
    g_contextual_manager.max_templates = 0;
    g_contextual_manager.context_rules_count = 0;
    g_contextual_manager.max_context_rules = 0;
    g_contextual_manager.state_keys_count = 0;
    g_contextual_manager.max_state_keys = 0;
    g_contextual_manager.alert_history_count = 0;
    g_contextual_manager.max_alert_history = 0;
    
    g_contextual_manager_initialized = false;
}

// Add alert template
int contextual_alert_manager_add_template(const alert_template_t* template) {
    if (!g_contextual_manager_initialized || !template) {
        return -1;
    }
    
    pthread_mutex_lock(g_contextual_manager.mutex);
    
    if (g_contextual_manager.template_count >= g_contextual_manager.max_templates) {
        pthread_mutex_unlock(g_contextual_manager.mutex);
        return -1; // No space for more templates
    }
    
    int index = g_contextual_manager.template_count;
    g_contextual_manager.alert_templates[index] = *template;
    g_contextual_manager.template_count++;
    
    pthread_mutex_unlock(g_contextual_manager.mutex);
    return 0;
}

// Add context rule
int contextual_alert_manager_add_context_rule(const context_rule_t* rule) {
    if (!g_contextual_manager_initialized || !rule) {
        return -1;
    }
    
    pthread_mutex_lock(g_contextual_manager.mutex);
    
    if (g_contextual_manager.context_rules_count >= g_contextual_manager.max_context_rules) {
        pthread_mutex_unlock(g_contextual_manager.mutex);
        return -1; // No space for more rules
    }
    
    int index = g_contextual_manager.context_rules_count;
    g_contextual_manager.context_rules[index] = *rule;
    g_contextual_manager.context_rules_count++;
    
    pthread_mutex_unlock(g_contextual_manager.mutex);
    return 0;
}

// Update system state
int contextual_alert_manager_update_state(const char* key, const char* value, time_t timestamp) {
    if (!g_contextual_manager_initialized || !key || !value) {
        return -1;
    }
    
    pthread_mutex_lock(g_contextual_manager.mutex);
    
    // Find existing key or add new one
    int key_index = -1;
    for (int i = 0; i < g_contextual_manager.state_keys_count; i++) {
        if (strcmp(g_contextual_manager.last_known_state[i].key, key) == 0) {
            key_index = i;
            break;
        }
    }
    
    if (key_index == -1) {
        // Add new key
        if (g_contextual_manager.state_keys_count >= g_contextual_manager.max_state_keys) {
            pthread_mutex_unlock(g_contextual_manager.mutex);
            return -1; // No space for more state keys
        }
        
        key_index = g_contextual_manager.state_keys_count;
        strncpy(g_contextual_manager.last_known_state[key_index].key, key, sizeof(g_contextual_manager.last_known_state[key_index].key) - 1);
        g_contextual_manager.state_keys_count++;
    }
    
    // Update value and timestamp
    strncpy(g_contextual_manager.last_known_state[key_index].value, value, sizeof(g_contextual_manager.last_known_state[key_index].value) - 1);
    g_contextual_manager.last_known_state[key_index].timestamp = timestamp;
    
    pthread_mutex_unlock(g_contextual_manager.mutex);
    return 0;
}

// Get system state value
const char* contextual_alert_manager_get_state(const char* key) {
    if (!g_contextual_manager_initialized || !key) {
        return NULL;
    }
    
    pthread_mutex_lock(g_contextual_manager.mutex);
    
    for (int i = 0; i < g_contextual_manager.state_keys_count; i++) {
        if (strcmp(g_contextual_manager.last_known_state[i].key, key) == 0) {
            const char* value = g_contextual_manager.last_known_state[i].value;
            pthread_mutex_unlock(g_contextual_manager.mutex);
            return value;
        }
    }
    
    pthread_mutex_unlock(g_contextual_manager.mutex);
    return NULL;
}

// Send contextual alert
int contextual_alert_manager_send_alert(alert_type_t alert_type, const char* title, const char* message,
                                      const location_context_t* location, const system_load_metrics_t* system_load) {
    if (!g_contextual_manager_initialized || !title || !message) {
        return -1;
    }
    
    // Find appropriate template
    alert_template_t* template = NULL;
    pthread_mutex_lock(g_contextual_manager.mutex);
    
    for (int i = 0; i < g_contextual_manager.template_count; i++) {
        if (g_contextual_manager.alert_templates[i].alert_type == alert_type) {
            template = &g_contextual_manager.alert_templates[i];
            break;
        }
    }
    
    pthread_mutex_unlock(g_contextual_manager.mutex);
    
    // Create enhanced notification event
    notification_event_t event;
    memset(&event, 0, sizeof(event));
    
    // Generate ID
    time_t now = time(NULL);
    unsigned int random = (unsigned int)rand();
    snprintf(event.id, sizeof(event.id), "%08lx-%08x", (unsigned long)now, random);
    
    // Use template if available, otherwise use provided values
    if (template) {
        strncpy(event.title, template->title, sizeof(event.title) - 1);
        strncpy(event.message, template->message, sizeof(event.message) - 1);
        event.priority = template->priority;
    } else {
        strncpy(event.title, title, sizeof(event.title) - 1);
        strncpy(event.message, message, sizeof(event.message) - 1);
        event.priority = NOTIFICATION_PRIORITY_NORMAL;
    }
    
    event.type = NOTIFICATION_TYPE_SYSTEM_HEALTH;
    event.timestamp = now;
    
    // Add location context if available
    if (location) {
        event.location = malloc(sizeof(notification_location_t));
        if (event.location) {
            event.location->latitude = location->latitude;
            event.location->longitude = location->longitude;
            strncpy(event.location->address, location->address, sizeof(event.location->address) - 1);
            strncpy(event.location->source, "contextual", sizeof(event.location->source) - 1);
        }
    }
    
    // Add system context to details
    if (system_load) {
        snprintf(event.details_json, sizeof(event.details_json),
                "{\"cpu_usage\":%.2f,\"memory_usage\":%.2f,\"disk_usage\":%.2f,\"temperature\":%.2f}",
                system_load->cpu_usage, system_load->memory_usage, 
                system_load->disk_usage, system_load->temperature);
    }
    
    // Send through smart manager if available
    if (smart_notification_manager_is_initialized()) {
        int result = smart_notification_manager_send(&event);
        
        // Add to alert history
        if (result == 0) {
            pthread_mutex_lock(g_contextual_manager.mutex);
            
            if (g_contextual_manager.alert_history_count < g_contextual_manager.max_alert_history) {
                int index = g_contextual_manager.alert_history_count;
                g_contextual_manager.alert_history[index].alert_type = alert_type;
                g_contextual_manager.alert_history[index].timestamp = now;
                strncpy(g_contextual_manager.alert_history[index].title, event.title, sizeof(g_contextual_manager.alert_history[index].title) - 1);
                strncpy(g_contextual_manager.alert_history[index].message, event.message, sizeof(g_contextual_manager.alert_history[index].message) - 1);
                g_contextual_manager.alert_history_count++;
            } else {
                // Shift history and add at end
                for (int i = 0; i < g_contextual_manager.max_alert_history - 1; i++) {
                    g_contextual_manager.alert_history[i] = g_contextual_manager.alert_history[i + 1];
                }
                int index = g_contextual_manager.max_alert_history - 1;
                g_contextual_manager.alert_history[index].alert_type = alert_type;
                g_contextual_manager.alert_history[index].timestamp = now;
                strncpy(g_contextual_manager.alert_history[index].title, event.title, sizeof(g_contextual_manager.alert_history[index].title) - 1);
                strncpy(g_contextual_manager.alert_history[index].message, event.message, sizeof(g_contextual_manager.alert_history[index].message) - 1);
            }
            
            pthread_mutex_unlock(g_contextual_manager.mutex);
        }
        
        // Clean up location data
        if (event.location) {
            free(event.location);
        }
        
        return result;
    } else {
        // Fall back to regular notification manager
        int result = notification_manager_send(event.type, event.title, event.message, 
                                            event.priority, NULL);
        
        // Clean up location data
        if (event.location) {
            free(event.location);
        }
        
        return result;
    }
}

// Get contextual alert manager status
void contextual_alert_manager_get_status(contextual_alert_status_t* status) {
    if (!status || !g_contextual_manager_initialized) return;
    
    pthread_mutex_lock(g_contextual_manager.mutex);
    
    status->enabled = true;
    status->template_count = g_contextual_manager.template_count;
    status->max_templates = g_contextual_manager.max_templates;
    status->context_rules_count = g_contextual_manager.context_rules_count;
    status->max_context_rules = g_contextual_manager.max_context_rules;
    status->state_keys_count = g_contextual_manager.state_keys_count;
    status->max_state_keys = g_contextual_manager.max_state_keys;
    status->alert_history_count = g_contextual_manager.alert_history_count;
    status->max_alert_history = g_contextual_manager.max_alert_history;
    
    pthread_mutex_unlock(g_contextual_manager.mutex);
}

// Get alert history
int contextual_alert_manager_get_alert_history(contextual_alert_t* alerts, int max_alerts) {
    if (!alerts || max_alerts <= 0 || !g_contextual_manager_initialized) {
        return -1;
    }
    
    pthread_mutex_lock(g_contextual_manager.mutex);
    
    int count = (max_alerts < g_contextual_manager.alert_history_count) ? 
                max_alerts : g_contextual_manager.alert_history_count;
    
    for (int i = 0; i < count; i++) {
        int source_index = g_contextual_manager.alert_history_count - count + i;
        alerts[i] = g_contextual_manager.alert_history[source_index];
    }
    
    pthread_mutex_unlock(g_contextual_manager.mutex);
    return count;
}

// Check if contextual alert manager is initialized
bool contextual_alert_manager_is_initialized(void) {
    return g_contextual_manager_initialized;
}

// Get contextual alert manager instance
contextual_alert_manager_t* contextual_alert_manager_get_instance(void) {
    return g_contextual_manager_initialized ? &g_contextual_manager : NULL;
}
