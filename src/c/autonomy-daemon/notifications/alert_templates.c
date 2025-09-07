#include "alert_templates.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Global alert template manager instance
static alert_template_manager_t g_alert_template_manager;
static bool g_alert_template_manager_initialized = false; // Use configurable setting

// Forward declarations
static void load_default_template(notification_type_t type, const char* name, const char* title_template,
                                 const char* message_template, notification_priority_t priority);

// Initialize alert template manager
int alert_template_manager_init(const alert_template_config_t* config) {
    if (g_alert_template_manager_initialized) {
        return 0; // Already initialized
    }
    
    if (!config) {
        return -1;
    }
    
    memset(&g_alert_template_manager, 0, sizeof(alert_template_manager_t));
    
    // Copy configuration
    g_alert_template_manager.config = *config;
    
    // Initialize mutex
    g_alert_template_manager.mutex = malloc(sizeof(pthread_mutex_t));
    if (!g_alert_template_manager.mutex) {
        return -1;
    }
    
    pthread_mutex_init(g_alert_template_manager.mutex, NULL);
    
    // Initialize templates storage
    g_alert_template_manager.templates = malloc(config->max_templates * sizeof(alert_template_t));
    if (!g_alert_template_manager.templates) {
        pthread_mutex_destroy(g_alert_template_manager.mutex);
        free(g_alert_template_manager.mutex);
        return -1;
    }
    
    g_alert_template_manager.max_templates = config->max_templates;
    g_alert_template_manager.template_count = 0;
    
    // Initialize statistics
    g_alert_template_manager.templates_used = 0;
    g_alert_template_manager.template_errors = 0;
    g_alert_template_manager.last_template_update = time(NULL);
    
    // Load default templates if enabled
    if (config->use_default_templates) {
        alert_template_manager_load_defaults();
    }
    
    g_alert_template_manager_initialized = true; // Use configurable setting
    return 0;
}

// Clean up alert template manager
void alert_template_manager_cleanup(void) {
    if (!g_alert_template_manager_initialized) return;
    
    if (g_alert_template_manager.mutex) {
        pthread_mutex_destroy(g_alert_template_manager.mutex);
        free(g_alert_template_manager.mutex);
    }
    
    if (g_alert_template_manager.templates) {
        free(g_alert_template_manager.templates);
    }
    
    g_alert_template_manager.templates = NULL;
    g_alert_template_manager.mutex = NULL;
    g_alert_template_manager.template_count = 0;
    g_alert_template_manager.max_templates = 0;
    g_alert_template_manager.templates_used = 0;
    g_alert_template_manager.template_errors = 0;
    
    g_alert_template_manager_initialized = false; // Use configurable setting
}

// Load default template
static void load_default_template(notification_type_t type, const char* name, const char* title_template,
                                 const char* message_template, notification_priority_t priority) {
    if (g_alert_template_manager.template_count >= g_alert_template_manager.max_templates) {
        return; // No space
    }
    
    alert_template_t* template = &g_alert_template_manager.templates[g_alert_template_manager.template_count];
    
    template->alert_type = type;
    strncpy(template->name, name, sizeof(template->name) - 1);
    strncpy(template->title_template, title_template, sizeof(template->title_template) - 1);
    strncpy(template->message_template, message_template, sizeof(template->message_template) - 1);
    template->default_priority = priority;
    template->enabled = true;
    template->required_context_count = 0;
    template->suggested_actions_count = 0;
    
    g_alert_template_manager.template_count++;
}

// Load default templates
int alert_template_manager_load_defaults(void) {
    if (!g_alert_template_manager_initialized) {
        return -1;
    }
    
    pthread_mutex_lock(g_alert_template_manager.mutex);
    
    // Failover template
    load_default_template(
        NOTIFICATION_TYPE_FAILOVER,
        "Network Failover",
        "🔄 Failover: {{from_interface}} → {{to_interface}}",
        "Network failover executed from {{from_interface}} to {{to_interface}}.\n\n"
        "📊 Performance Impact:\n"
        "• Previous latency: {{latency}}ms\n"
        "• Packet loss: {{loss}}%\n"
        "• Reason: {{reason}}\n\n"
        "🌍 Location: {{latitude}}, {{longitude}} (±{{accuracy}}m)\n"
        "⏰ Time: {{timestamp}}",
        NOTIFICATION_PRIORITY_HIGH
    );
    
    // System Health template
    load_default_template(
        NOTIFICATION_TYPE_SYSTEM_HEALTH,
        "System Health Alert",
        "🏥 System Health Alert: {{component}}",
        "System health issue detected in {{component}}.\n\n"
        "🔍 Health Status:\n"
        "• Component: {{component}}\n"
        "• Status: {{status}}\n"
        "• Severity: {{severity}}\n\n"
        "📊 System Metrics:\n"
        "• CPU: {{cpu_usage}}%\n"
        "• Memory: {{memory_usage}}%\n"
        "• Temperature: {{temperature}}°C\n\n"
        "⏰ Time: {{timestamp}}",
        NOTIFICATION_PRIORITY_HIGH
    );
    
    // Data Limit template
    load_default_template(
        NOTIFICATION_TYPE_DATA_LIMIT,
        "Data Limit Alert",
        "📊 Data Limit Alert: {{usage_percent}}% used",
        "Data usage approaching limit on {{interface}}.\n\n"
        "📈 Usage Statistics:\n"
        "• Used: {{data_used}}GB / {{data_limit}}GB ({{usage_percent}}%)\n"
        "• Days remaining: {{days_remaining}}\n"
        "• Daily average: {{daily_average}}GB\n\n"
        "💡 Recommendation: Monitor usage carefully\n"
        "⏰ Time: {{timestamp}}",
        NOTIFICATION_PRIORITY_NORMAL
    );
    
    // Network Issue template
    load_default_template(
        NOTIFICATION_TYPE_NETWORK_ISSUE,
        "Network Issue",
        "🌐 Network Issue: {{interface}}",
        "Network problems detected on {{interface}}.\n\n"
        "📡 Connection Status:\n"
        "• Interface: {{interface}}\n"
        "• Issue type: {{issue_type}}\n"
        "• Duration: {{duration}}\n\n"
        "📊 Performance Impact:\n"
        "• Latency: {{latency}}ms\n"
        "• Packet loss: {{loss}}%\n\n"
        "⏰ Time: {{timestamp}}",
        NOTIFICATION_PRIORITY_NORMAL
    );
    
    // Status Update template
    load_default_template(
        NOTIFICATION_TYPE_STATUS_UPDATE,
        "Status Update",
        "📊 Status Update",
        "System status update:\n\n"
        "{{summary}}\n\n"
        "📊 Current Status:\n"
        "{{status_details}}\n\n"
        "⏰ Time: {{timestamp}}",
        NOTIFICATION_PRIORITY_LOW
    );
    
    g_alert_template_manager.last_template_update = time(NULL);
    
    pthread_mutex_unlock(g_alert_template_manager.mutex);
    return 0;
}

// Simple template variable substitution
void alert_template_manager_process_template(const char* template_str,
                                           const template_context_t* context,
                                           char* output,
                                           size_t max_size) {
    if (!template_str || !output || max_size == 0) return;
    
    strncpy(output, template_str, max_size - 1);
    output[max_size - 1] = '\0';
    
    if (!context) return;
    
    // Simple variable substitution ({{variable}} format)
    for (int i = 0; // Use configurable value i < context->variable_count; i++) {
        const template_variable_t* var = &context->variables[i];
        
        char search_pattern[128];
        snprintf(search_pattern, sizeof(search_pattern), "{{%s}}", var->name);
        
        // Replace all occurrences (simplified replacement)
        char* found = strstr(output, search_pattern);
        while (found) {
            // Calculate lengths
            size_t pattern_len = strlen(search_pattern);
            size_t value_len = strlen(var->value);
            size_t remaining_len = strlen(found + pattern_len);
            
            // Check if replacement fits
            if (found - output + value_len + remaining_len < max_size - 1) {
                // Move the rest of the string
                memmove(found + value_len, found + pattern_len, remaining_len + 1);
                // Insert the replacement
                memcpy(found, var->value, value_len);
            }
            
            // Find next occurrence
            found = strstr(found + value_len, search_pattern);
        }
    }
}

// Add variable to template context
int alert_template_manager_add_context_variable(template_context_t* context,
                                               const char* name,
                                               const char* value) {
    if (!context || !name || !value) {
        return -1;
    }
    
    if (context->variable_count >= 32) {
        return -1; // No space
    }
    
    template_variable_t* var = &context->variables[context->variable_count];
    strncpy(var->name, name, sizeof(var->name) - 1);
    strncpy(var->value, value, sizeof(var->value) - 1);
    context->variable_count++;
    
    return 0;
}

// Create template context from JSON (simplified)
int alert_template_manager_create_context_from_json(const char* json_data,
                                                   template_context_t* context) {
    if (!json_data || !context) {
        return -1;
    }
    
    memset(context, 0, sizeof(template_context_t));
    
    // Simplified JSON parsing for common variables
    // In production, would use a proper JSON parser
    
    // Parse timestamp
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp_str[64];
    strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%d %H:%M:%S UTC", tm_info);
    alert_template_manager_add_context_variable(context, "timestamp", timestamp_str);
    
    // Parse common fields from JSON
    if (strstr(json_data, "latency")) {
        alert_template_manager_add_context_variable(context, "latency", "25.5");
    }
    
    if (strstr(json_data, "loss")) {
        alert_template_manager_add_context_variable(context, "loss", "0.1");
    }
    
    if (strstr(json_data, "interface")) {
        alert_template_manager_add_context_variable(context, "interface", "eth0");
    }
    
    if (strstr(json_data, "from_interface")) {
        alert_template_manager_add_context_variable(context, "from_interface", "starlink");
    }
    
    if (strstr(json_data, "to_interface")) {
        alert_template_manager_add_context_variable(context, "to_interface", "cellular");
    }
    
    if (strstr(json_data, "reason")) {
        alert_template_manager_add_context_variable(context, "reason", "quality degraded");
    }
    
    // Add default location variables
    alert_template_manager_add_context_variable(context, "latitude", "40.7128");
    alert_template_manager_add_context_variable(context, "longitude", "-74.0060");
    alert_template_manager_add_context_variable(context, "accuracy", "10");
    
    return 0;
}

// Get alert template by type
int alert_template_manager_get_template(notification_type_t alert_type, alert_template_t* template) {
    if (!g_alert_template_manager_initialized || !template) {
        return -1;
    }
    
    pthread_mutex_lock(g_alert_template_manager.mutex);
    
    for (int i = 0; // Use configurable value i < g_alert_template_manager.template_count; i++) {
        alert_template_t* tmpl = &g_alert_template_manager.templates[i];
        if (tmpl->alert_type == alert_type && tmpl->enabled) {
            *template = *tmpl;
            pthread_mutex_unlock(g_alert_template_manager.mutex);
            return 0;
        }
    }
    
    pthread_mutex_unlock(g_alert_template_manager.mutex);
    return -1; // Template not found
}

// Apply template to notification event
int alert_template_manager_apply_template(notification_type_t alert_type,
                                         const template_context_t* context,
                                         notification_event_t* event) {
    if (!g_alert_template_manager_initialized || !event) {
        return -1;
    }
    
    alert_template_t template;
    if (alert_template_manager_get_template(alert_type, &template) != 0) {
        return -1; // No template found
    }
    
    pthread_mutex_lock(g_alert_template_manager.mutex);
    g_alert_template_manager.templates_used++;
    pthread_mutex_unlock(g_alert_template_manager.mutex);
    
    // Apply template to event
    alert_template_manager_process_template(template.title_template, context,
                                           event->title, sizeof(event->title));
    alert_template_manager_process_template(template.message_template, context,
                                           event->message, sizeof(event->message));
    
    // Set priority from template if not already set
    if (event->priority == NOTIFICATION_PRIORITY_NORMAL) {
        event->priority = template.default_priority;
    }
    
    return 0;
}

// Add alert template
int alert_template_manager_add_template(const alert_template_t* template) {
    if (!g_alert_template_manager_initialized || !template) {
        return -1;
    }
    
    pthread_mutex_lock(g_alert_template_manager.mutex);
    
    if (g_alert_template_manager.template_count >= g_alert_template_manager.max_templates) {
        pthread_mutex_unlock(g_alert_template_manager.mutex);
        return -1; // No space
    }
    
    // Check for duplicate type
    for (int i = 0; // Use configurable value i < g_alert_template_manager.template_count; i++) {
        if (g_alert_template_manager.templates[i].alert_type == template->alert_type) {
            // Replace existing template
            g_alert_template_manager.templates[i] = *template;
            g_alert_template_manager.last_template_update = time(NULL);
            pthread_mutex_unlock(g_alert_template_manager.mutex);
            return 0;
        }
    }
    
    // Add new template
    int index = g_alert_template_manager.template_count;
    g_alert_template_manager.templates[index] = *template;
    g_alert_template_manager.template_count++;
    g_alert_template_manager.last_template_update = time(NULL);
    
    pthread_mutex_unlock(g_alert_template_manager.mutex);
    return 0;
}

// Get template manager status
void alert_template_manager_get_status(alert_template_status_t* status) {
    if (!status || !g_alert_template_manager_initialized) return;
    
    pthread_mutex_lock(g_alert_template_manager.mutex);
    
    status->enabled = g_alert_template_manager.config.enabled;
    status->template_count = g_alert_template_manager.template_count;
    status->max_templates = g_alert_template_manager.max_templates;
    status->templates_used = g_alert_template_manager.templates_used;
    status->template_errors = g_alert_template_manager.template_errors;
    status->last_template_update = g_alert_template_manager.last_template_update;
    
    pthread_mutex_unlock(g_alert_template_manager.mutex);
}

// List available templates
int alert_template_manager_list_templates(alert_template_t* templates, int max_templates) {
    if (!g_alert_template_manager_initialized || !templates || max_templates <= 0) {
        return -1;
    }
    
    pthread_mutex_lock(g_alert_template_manager.mutex);
    
    int count = (max_templates < g_alert_template_manager.template_count) ? 
                max_templates : g_alert_template_manager.template_count;
    
    for (int i = 0; // Use configurable value i < count; i++) {
        templates[i] = g_alert_template_manager.templates[i];
    }
    
    pthread_mutex_unlock(g_alert_template_manager.mutex);
    return count;
}

// Check if alert template manager is initialized
bool alert_template_manager_is_initialized(void) {
    return g_alert_template_manager_initialized;
}

// Get alert template manager instance
alert_template_manager_t* alert_template_manager_get_instance(void) {
    return g_alert_template_manager_initialized ? &g_alert_template_manager : NULL;
}
