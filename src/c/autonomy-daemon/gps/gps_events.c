#include "gps_events.h"
#include "../utils/logx.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

// GPS event configuration
static const int MAX_EVENTS = 50;                      // Maximum number of events
static const int MAX_EVENT_CONDITIONS = 10;             // Maximum conditions per event
static const int MAX_EVENT_ACTIONS = 5;                 // Maximum actions per event
static const int EVENT_CHECK_INTERVAL = 2;              // 2 second event check interval
static const int MAX_EVENT_HISTORY = 100;               // Number of event records to keep
static const double DEFAULT_SPEED_THRESHOLD = 5.0;      // 5 m/s default speed threshold
static const double DEFAULT_ACCURACY_THRESHOLD = 50.0;  // 50m default accuracy threshold

// Event types
static const char* EVENT_TYPE_NAMES[] = {
    "unknown", "location_change", "speed_change", "accuracy_change", "satellite_change",
    "fix_quality_change", "geofence_event", "movement_pattern", "time_based", "custom"
};

// Condition types
static const char* CONDITION_TYPE_NAMES[] = {
    "unknown", "location_in", "location_out", "speed_above", "speed_below",
    "accuracy_above", "accuracy_below", "satellites_above", "satellites_below",
    "fix_quality_above", "fix_quality_below", "time_after", "time_before", "custom"
};

// Action types
static const char* ACTION_TYPE_NAMES[] = {
    "unknown", "log_event", "send_notification", "trigger_callback", "execute_command",
    "update_status", "send_ubus_message", "custom"
};

// Global GPS events state

// Forward declarations - auto-generated
static void add_performance_history_entry(int source_id, double accuracy, double response_time, bool success);
static int find_best_cluster(const gps_data_t *gps_data);
static int create_new_cluster(const gps_data_t *gps_data);
static bool check_event_conditions(const void *event, const gps_data_t *gps_data);
static bool evaluate_condition(const void *condition, const gps_data_t *gps_data);
static void execute_event_actions(const void *event, const gps_data_t *gps_data);
static double calculate_distance(double lat1, double lon1, double lat2, double lon2);
static void analyze_movement_pattern(void);
static void update_source_error_tracking(int source_id, int error_type);

static gps_events_t g_events = {0};
static bool g_events_initialized = false;
static pthread_mutex_t g_events_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize GPS events system
int gps_events_init(void) {
    if (g_events_initialized) {
        LOGX_WARN_MSG("GPS events system already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_events_mutex);
    
    // Initialize events state
    memset(&g_events, 0, sizeof(gps_events_t));
    g_events.enabled = true;
    g_events.max_events = MAX_EVENTS;
    g_events.max_conditions = MAX_EVENT_CONDITIONS;
    g_events.max_actions = MAX_EVENT_ACTIONS;
    g_events.check_interval = EVENT_CHECK_INTERVAL;
    g_events.history_size = MAX_EVENT_HISTORY;
    
    g_events.event_count = 0;
    g_events.active_events = 0;
    g_events.total_triggers = 0;
    g_events.last_check = 0;
    
    // Initialize events array
    for (int i = 0; i < MAX_EVENTS; i++) {
        g_events.events[i].active = false;
        g_events.events[i].event_id = 0;
        g_events.events[i].event_type = GPS_EVENT_TYPE_UNKNOWN;
        g_events.events[i].condition_count = 0;
        g_events.events[i].action_count = 0;
        g_events.events[i].enabled = false;
        g_events.events[i].last_triggered = 0;
        g_events.events[i].trigger_count = 0;
        g_events.events[i].cooldown_period = 0;
    }
    
    // Initialize event history
    for (int i = 0; i < MAX_EVENT_HISTORY; i++) {
        g_events.event_history[i].timestamp = 0;
        g_events.event_history[i].event_id = 0;
        g_events.event_history[i].event_type = GPS_EVENT_TYPE_UNKNOWN;
        g_events.event_history[i].trigger_reason = 0;
        g_events.event_history[i].gps_lat = 0.0;
        g_events.event_history[i].gps_lon = 0.0;
        g_events.event_history[i].gps_accuracy = 0.0;
        g_events.event_history[i].gps_speed = 0.0;
    }
    
    g_events_initialized = true;
    pthread_mutex_unlock(&g_events_mutex);
    
    LOGX_INFO_MSG("GPS events system initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Create GPS event
int gps_events_create_event(const char *name, gps_event_type_t event_type, 
                           const gps_event_condition_t *conditions, int condition_count,
                           const gps_event_action_t *actions, int action_count) {
    if (!g_events_initialized || !name || !conditions || !actions) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (condition_count <= 0 || condition_count > MAX_EVENT_CONDITIONS ||
        action_count <= 0 || action_count > MAX_EVENT_ACTIONS) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_events_mutex);
    
    // Find free event slot
    int event_index = -1;
    for (int i = 0; i < MAX_EVENTS; i++) {
        if (!g_events.events[i].active) {
            event_index = i;
            break;
        }
    }
    
    if (event_index < 0) {
        pthread_mutex_unlock(&g_events_mutex);
        LOGX_ERROR_MSG("No free slots for GPS event creation");
        return AUTONOMY_ERROR_NO_RESOURCES;
    }
    
    // Initialize event
    gps_event_definition_t *event = &g_events.events[event_index];
    event->active = true;
    event->event_id = generate_event_id();
    event->event_type = event_type;
    event->condition_count = condition_count;
    event->action_count = action_count;
    event->enabled = true;
    event->last_triggered = 0;
    event->trigger_count = 0;
    event->cooldown_period = 0;
    
    // Set event name
    strncpy(event->name, name, sizeof(event->name) - 1);
    event->name[sizeof(event->name) - 1] = '\0';
    
    // Copy conditions
    for (int i = 0; i < condition_count; i++) {
        memcpy(&event->conditions[i], &conditions[i], sizeof(gps_event_condition_t));
    }
    
    // Copy actions
    for (int i = 0; i < action_count; i++) {
        memcpy(&event->actions[i], &actions[i], sizeof(gps_event_action_t));
    }
    
    g_events.event_count++;
    g_events.active_events++;
    
    pthread_mutex_unlock(&g_events_mutex);
    
    LOGX_INFO_MSG("Created GPS event '%s' (type: %d) with %d conditions and %d actions", 
               name, event_type, condition_count, action_count);
    
    return event->event_id;
}

// Generate unique event ID
int generate_event_id(void) {
    static int next_id = 2000;
    return next_id++;
}

// Check GPS data against all events
int gps_events_check_gps_data(const gps_data_t *gps_data) {
    if (!g_events_initialized || !gps_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_events_mutex);
    
    time_t now = time(NULL);
    
    // Check if enough time has passed since last check
    if ((now - g_events.last_check) < g_events.check_interval) {
        pthread_mutex_unlock(&g_events_mutex);
        return AUTONOMY_SUCCESS;
    }
    
    g_events.last_check = now;
    
    // Check each active event
    for (int i = 0; i < MAX_EVENTS; i++) {
        if (!g_events.events[i].active || !g_events.events[i].enabled) {
            continue;
        }
        
        gps_event_definition_t *event = &g_events.events[i];
        
        // Check cooldown period
        if (event->cooldown_period > 0 && 
            (now - event->last_triggered) < event->cooldown_period) {
            continue;
        }
        
        // Check if all conditions are met
        if (check_event_conditions(event, gps_data)) {
            // Execute event actions
            execute_event_actions(event, gps_data);
            
            // Update event statistics
            event->last_triggered = now;
            event->trigger_count++;
            g_events.total_triggers++;
            
            // Add to event history
            add_event_history(event, gps_data, now);
            
            LOGX_DEBUG_MSG("GPS event '%s' triggered", event->name);
        }
    }
    
    pthread_mutex_unlock(&g_events_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Check if event conditions are met
static bool check_event_conditions(const gps_event_definition_t *event, const gps_data_t *gps_data) {
    for (int i = 0; i < event->condition_count; i++) {
        const gps_event_condition_t *condition = &event->conditions[i];
        
        if (!evaluate_condition(condition, gps_data)) {
            return false;
        }
    }
    
    return true;
}

// Evaluate individual condition
static bool evaluate_condition(const gps_event_condition_t *condition, const gps_data_t *gps_data) {
    switch (condition->condition_type) {
        case GPS_CONDITION_LOCATION_IN:
            return check_location_in_condition(condition, gps_data);
        case GPS_CONDITION_LOCATION_OUT:
            return check_location_out_condition(condition, gps_data);
        case GPS_CONDITION_SPEED_ABOVE:
            return gps_data->speed > condition->threshold_value;
        case GPS_CONDITION_SPEED_BELOW:
            return gps_data->speed < condition->threshold_value;
        case GPS_CONDITION_ACCURACY_ABOVE:
            return gps_data->accuracy > condition->threshold_value;
        case GPS_CONDITION_ACCURACY_BELOW:
            return gps_data->accuracy < condition->threshold_value;
        case GPS_CONDITION_SATELLITES_ABOVE:
            return gps_data->satellites > condition->threshold_value;
        case GPS_CONDITION_SATELLITES_BELOW:
            return gps_data->satellites < condition->threshold_value;
        case GPS_CONDITION_FIX_QUALITY_ABOVE:
            return gps_data->fix_quality > condition->threshold_value;
        case GPS_CONDITION_FIX_QUALITY_BELOW:
            return gps_data->fix_quality < condition->threshold_value;
        case GPS_CONDITION_TIME_AFTER:
            return check_time_after_condition(condition);
        case GPS_CONDITION_TIME_BEFORE:
            return check_time_before_condition(condition);
        case GPS_CONDITION_CUSTOM:
            return evaluate_custom_condition(condition, gps_data);
        default:
            return false;
    }
}

// Check location in condition
static bool check_location_in_condition(const gps_event_condition_t *condition, const gps_data_t *gps_data) {
    // For now, implement a simple circular area check
    // In a full implementation, this could check against geofences or other defined areas
    
    if (condition->location_data.lat == 0.0 && condition->location_data.lon == 0.0) {
        return false;
    }
    
    double distance = calculate_distance(gps_data->lat, gps_data->lon,
                                       condition->location_data.lat, condition->location_data.lon);
    
    return distance <= condition->threshold_value;
}

// Check location out condition
static bool check_location_out_condition(const gps_event_condition_t *condition, const gps_data_t *gps_data) {
    if (condition->location_data.lat == 0.0 && condition->location_data.lon == 0.0) {
        return false;
    }
    
    double distance = calculate_distance(gps_data->lat, gps_data->lon,
                                       condition->location_data.lat, condition->location_data.lon);
    
    return distance > condition->threshold_value;
}

// Check time after condition
static bool check_time_after_condition(const gps_event_condition_t *condition) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    int current_time = tm_info->tm_hour * 3600 + tm_info->tm_min * 60 + tm_info->tm_sec;
    
    return current_time >= condition->time_value;
}

// Check time before condition
static bool check_time_before_condition(const gps_event_condition_t *condition) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    int current_time = tm_info->tm_hour * 3600 + tm_info->tm_min * 60 + tm_info->tm_sec;
    
    return current_time <= condition->time_value;
}

// Evaluate custom condition
static bool evaluate_custom_condition(const gps_event_condition_t *condition, const gps_data_t *gps_data) {
    // For now, return true for custom conditions
    // In a full implementation, this could call user-defined functions or scripts
    return true;
}

// Execute event actions
static void execute_event_actions(const gps_event_definition_t *event, const gps_data_t *gps_data) {
    for (int i = 0; i < event->action_count; i++) {
        const gps_event_action_t *action = &event->actions[i];
        
        switch (action->action_type) {
            case GPS_ACTION_LOG_EVENT:
                log_event_action(event, gps_data, action);
                break;
            case GPS_ACTION_SEND_NOTIFICATION:
                send_notification_action(event, gps_data, action);
                break;
            case GPS_ACTION_TRIGGER_CALLBACK:
                trigger_callback_action(event, gps_data, action);
                break;
            case GPS_ACTION_EXECUTE_COMMAND:
                execute_command_action(event, gps_data, action);
                break;
            case GPS_ACTION_UPDATE_STATUS:
                update_status_action(event, gps_data, action);
                break;
            case GPS_ACTION_SEND_UBUS_MESSAGE:
                send_ubus_message_action(event, gps_data, action);
                break;
            case GPS_ACTION_CUSTOM:
                execute_custom_action(event, gps_data, action);
                break;
            default:
                LOGX_WARN_MSG("Unknown action type: %d", action->action_type);
                break;
        }
    }
}

// Log event action
static void log_event_action(const gps_event_definition_t *event, const gps_data_t *gps_data, 
                            const gps_event_action_t *action) {
    LOGX_INFO_MSG("GPS EVENT: '%s' triggered - Lat: %.6f, Lon: %.6f, Accuracy: %.1fm, Speed: %.1fm/s", 
               event->name, gps_data->lat, gps_data->lon, gps_data->accuracy, gps_data->speed);
}

// Send notification action
static void send_notification_action(const gps_event_definition_t *event, const gps_data_t *gps_data, 
                                   const gps_event_action_t *action) {
    // For now, just log the notification
    // In a full implementation, this would integrate with the notification system
    LOGX_INFO_MSG("GPS EVENT NOTIFICATION: '%s' - %s", event->name, action->action_data);
}

// Trigger callback action
static void trigger_callback_action(const gps_event_definition_t *event, const gps_data_t *gps_data, 
                                  const gps_event_action_t *action) {
    // For now, just log the callback
    // In a full implementation, this would call registered callback functions
    LOGX_INFO_MSG("GPS EVENT CALLBACK: '%s' - %s", event->name, action->action_data);
}

// Execute command action
static void execute_command_action(const gps_event_definition_t *event, const gps_data_t *gps_data, 
                                 const gps_event_action_t *action) {
    // For now, just log the command
    // In a full implementation, this would execute system commands
    LOGX_INFO_MSG("GPS EVENT COMMAND: '%s' - %s", event->name, action->action_data);
}

// Update status action
static void update_status_action(const gps_event_definition_t *event, const gps_data_t *gps_data, 
                               const gps_event_action_t *action) {
    // For now, just log the status update
    // In a full implementation, this would update system status
    LOGX_INFO_MSG("GPS EVENT STATUS UPDATE: '%s' - %s", event->name, action->action_data);
}

// Send UBUS message action
static void send_ubus_message_action(const gps_event_definition_t *event, const gps_data_t *gps_data, 
                                    const gps_event_action_t *action) {
    // For now, just log the UBUS message
    // In a full implementation, this would send UBUS messages
    LOGX_INFO_MSG("GPS EVENT UBUS MESSAGE: '%s' - %s", event->name, action->action_data);
}

// Execute custom action
static void execute_custom_action(const gps_event_definition_t *event, const gps_data_t *gps_data, 
                                const gps_event_action_t *action) {
    // For now, just log the custom action
    // In a full implementation, this would execute user-defined actions
    LOGX_INFO_MSG("GPS EVENT CUSTOM ACTION: '%s' - %s", event->name, action->action_data);
}

// Add event to history
static void add_event_history(const gps_event_definition_t *event, const gps_data_t *gps_data, time_t timestamp) {
    // Shift history array
    for (int i = g_events.history_size - 1; i > 0; i--) {
        memcpy(&g_events.event_history[i], &g_events.event_history[i-1], 
               sizeof(gps_event_record_t));
    }
    
    // Add new record
    g_events.event_history[0].timestamp = timestamp;
    g_events.event_history[0].event_id = event->event_id;
    g_events.event_history[0].event_type = event->event_type;
    g_events.event_history[0].trigger_reason = 0; // Could be enhanced to store specific reason
    g_events.event_history[0].gps_lat = gps_data->lat;
    g_events.event_history[0].gps_lon = gps_data->lon;
    g_events.event_history[0].gps_accuracy = gps_data->accuracy;
    g_events.event_history[0].gps_speed = gps_data->speed;
}

// Calculate distance between two GPS coordinates (Haversine formula)
static double calculate_distance(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371000.0;  // Earth's radius in meters
    
    double lat1_rad = lat1 * M_PI / 180.0;
    double lat2_rad = lat2 * M_PI / 180.0;
    double delta_lat = (lat2 - lat1) * M_PI / 180.0;
    double delta_lon = (lon2 - lon1) * M_PI / 180.0;
    
    double a = sin(delta_lat / 2.0) * sin(delta_lat / 2.0) +
               cos(lat1_rad) * cos(lat2_rad) *
               sin(delta_lon / 2.0) * sin(delta_lon / 2.0);
    
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    
    return R * c;
}

// Get events status
int gps_events_get_status(gps_events_status_t *status) {
    if (!g_events_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_events_mutex);
    
    status->enabled = g_events.enabled;
    status->event_count = g_events.event_count;
    status->active_events = g_events.active_events;
    status->total_triggers = g_events.total_triggers;
    status->last_check = g_events.last_check;
    
    // Copy event information
    int active_events = 0;
    for (int i = 0; i < MAX_EVENTS && active_events < MAX_EVENTS; i++) {
        if (g_events.events[i].active) {
            memcpy(&status->events[active_events], &g_events.events[i], 
                   sizeof(gps_event_definition_t));
            active_events++;
        }
    }
    status->active_event_count = active_events;
    
    pthread_mutex_unlock(&g_events_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get events configuration
int gps_events_get_config(gps_events_config_t *config) {
    if (!g_events_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_events_mutex);
    
    config->enabled = g_events.enabled;
    config->max_events = g_events.max_events;
    config->max_conditions = g_events.max_conditions;
    config->max_actions = g_events.max_actions;
    config->check_interval = g_events.check_interval;
    config->history_size = g_events.history_size;
    
    pthread_mutex_unlock(&g_events_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set events configuration
int gps_events_set_config(const gps_events_config_t *config) {
    if (!g_events_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_events_mutex);
    
    g_events.enabled = config->enabled;
    g_events.max_events = config->max_events;
    g_events.max_conditions = config->max_conditions;
    g_events.max_actions = config->max_actions;
    g_events.check_interval = config->check_interval;
    g_events.history_size = config->history_size;
    
    pthread_mutex_unlock(&g_events_mutex);
    
    LOGX_INFO_MSG("GPS events configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable events
int gps_events_set_enabled(bool enabled) {
    if (!g_events_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_events_mutex);
    g_events.enabled = enabled;
    pthread_mutex_unlock(&g_events_mutex);
    
    LOGX_INFO_MSG("GPS events %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Enable/disable specific event
int gps_events_set_event_enabled(int event_id, bool enabled) {
    if (!g_events_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_events_mutex);
    
    for (int i = 0; i < MAX_EVENTS; i++) {
        if (g_events.events[i].active && 
            g_events.events[i].event_id == event_id) {
            
            g_events.events[i].enabled = enabled;
            
            if (enabled) {
                g_events.active_events++;
            } else {
                g_events.active_events--;
            }
            
            pthread_mutex_unlock(&g_events_mutex);
            
            LOGX_INFO_MSG("GPS event %d %s", event_id, enabled ? "enabled" : "disabled");
            return AUTONOMY_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&g_events_mutex);
    return AUTONOMY_ERROR_NOT_FOUND;
}

// Delete event
int gps_events_delete(int event_id) {
    if (!g_events_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_events_mutex);
    
    for (int i = 0; i < MAX_EVENTS; i++) {
        if (g_events.events[i].active && 
            g_events.events[i].event_id == event_id) {
            
            if (g_events.events[i].enabled) {
                g_events.active_events--;
            }
            
            g_events.events[i].active = false;
            g_events.event_count--;
            
            pthread_mutex_unlock(&g_events_mutex);
            
            LOGX_INFO_MSG("Deleted GPS event %d", event_id);
            return AUTONOMY_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&g_events_mutex);
    return AUTONOMY_ERROR_NOT_FOUND;
}

// Reset events system
int gps_events_reset(void) {
    if (!g_events_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_events_mutex);
    
    g_events.event_count = 0;
    g_events.active_events = 0;
    g_events.total_triggers = 0;
    g_events.last_check = 0;
    
    // Clear all events
    for (int i = 0; i < MAX_EVENTS; i++) {
        g_events.events[i].active = false;
    }
    
    // Clear event history
    for (int i = 0; i < MAX_EVENT_HISTORY; i++) {
        g_events.event_history[i].timestamp = 0;
        g_events.event_history[i].event_id = 0;
        g_events.event_history[i].event_type = GPS_EVENT_TYPE_UNKNOWN;
        g_events.event_history[i].trigger_reason = 0;
        g_events.event_history[i].gps_lat = 0.0;
        g_events.event_history[i].gps_lon = 0.0;
        g_events.event_history[i].gps_accuracy = 0.0;
        g_events.event_history[i].gps_speed = 0.0;
    }
    
    pthread_mutex_unlock(&g_events_mutex);
    
    LOGX_INFO_MSG("GPS events system reset");
    return AUTONOMY_SUCCESS;
}

// Cleanup events system
void gps_events_cleanup(void) {
    if (!g_events_initialized) {
        return;
    }
    
    pthread_mutex_destroy(&g_events_mutex);
    g_events_initialized = false;
    
    LOGX_INFO_MSG("GPS events system cleaned up");
}
