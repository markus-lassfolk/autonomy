#ifndef ALERT_TEMPLATES_H
#define ALERT_TEMPLATES_H

#include "notification_types.h"
#include <stdbool.h>
#include <time.h>

// Template variable
typedef struct {
    char name[64];
    char value[256];
} template_variable_t;

// Alert template
typedef struct {
    notification_type_t alert_type;
    char name[128];
    char title_template[256];
    char message_template[2048];
    notification_priority_t default_priority;
    bool enabled;
    
    // Required context variables
    char required_context[16][64];
    int required_context_count;
    
    // Suggested actions
    char suggested_actions[8][256];
    int suggested_actions_count;
} alert_template_t;

// Template context
typedef struct {
    template_variable_t variables[32];
    int variable_count;
} template_context_t;

// Alert template manager configuration
typedef struct {
    bool enabled;
    int max_templates;
    bool use_default_templates;
    char template_directory[256];
} alert_template_config_t;

// Alert template manager status
typedef struct {
    bool enabled;
    int template_count;
    int max_templates;
    int templates_used;
    int template_errors;
    time_t last_template_update;
} alert_template_status_t;

// Alert template manager structure
typedef struct {
    alert_template_config_t config;
    
    // Templates storage
    alert_template_t* templates;
    int max_templates;
    int template_count;
    
    // Statistics
    int templates_used;
    int template_errors;
    time_t last_template_update;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} alert_template_manager_t;

// Initialize alert template manager
int alert_template_manager_init(const alert_template_config_t* config);

// Clean up alert template manager
void alert_template_manager_cleanup(void);

// Add alert template
int alert_template_manager_add_template(const alert_template_t* template);

// Get alert template by type
int alert_template_manager_get_template(notification_type_t alert_type, alert_template_t* template);

// Apply template to notification event
int alert_template_manager_apply_template(notification_type_t alert_type,
                                         const template_context_t* context,
                                         notification_event_t* event);

// Create template context from JSON
int alert_template_manager_create_context_from_json(const char* json_data,
                                                   template_context_t* context);

// Add variable to template context
int alert_template_manager_add_context_variable(template_context_t* context,
                                               const char* name,
                                               const char* value);

// Process template string with context variables
void alert_template_manager_process_template(const char* template_str,
                                           const template_context_t* context,
                                           char* output,
                                           size_t max_size);

// Load default templates
int alert_template_manager_load_defaults(void);

// Get template manager status
void alert_template_manager_get_status(alert_template_status_t* status);

// List available templates
int alert_template_manager_list_templates(alert_template_t* templates, int max_templates);

// Check if alert template manager is initialized
bool alert_template_manager_is_initialized(void);

// Get alert template manager instance
alert_template_manager_t* alert_template_manager_get_instance(void);

#endif // ALERT_TEMPLATES_H
