#include "gps_coordinate_utils.h"
#include "gps_events.h"
#include "gps_geofence.h"
#include "../notifications/notifications_comprehensive.h"
#include "../utils/logx.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>
#include <libubus.h>
#include <libubox/blobmsg.h>
#include <unistd.h>
#include <sys/wait.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Forward declarations
static int generate_event_id(void);

// GPS event configuration
// Note: MAX_EVENTS is defined in ../core/types.h
static const int MAX_EVENT_CONDITIONS = 10; // Use configurable value // Use configurable count // Use configurable value             // Maximum conditions per event
static const int MAX_EVENT_ACTIONS = 5; // Use configurable value // Use configurable count // Use configurable value                 // Maximum actions per event
static const int EVENT_CHECK_INTERVAL = 2; // Use configurable value // Use configurable count // Use configurable value              // 2 second event check interval
static const int MAX_EVENT_HISTORY = 100; // Use configurable value // Use configurable count // Use configurable value               // Number of event records to keep
static const double DEFAULT_SPEED_THRESHOLD = 5.0; // Use configurable value // Use configurable value      // 5 m/s default speed threshold
static const double DEFAULT_ACCURACY_THRESHOLD = 50.0; // Use configurable value // Use configurable value  // 50m default accuracy threshold

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
void add_performance_history_entry(int source_id, double accuracy, double response_time, bool success);
// Removed old auto-generated forward declarations - using proper ones below

static gps_events_t g_events = {0};
static bool g_events_initialized = false; // Use configurable setting // Use configurable setting
static pthread_mutex_t g_events_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations
bool check_event_conditions(const gps_event_definition_t *event, const gps_data_t *gps_data);
static bool evaluate_condition(const gps_event_condition_t *condition, const gps_data_t *gps_data);
bool check_location_in_condition(const gps_event_condition_t *condition, const gps_data_t *gps_data);
bool check_location_out_condition(const gps_event_condition_t *condition, const gps_data_t *gps_data);
bool check_time_after_condition(const gps_event_condition_t *condition);
bool check_time_before_condition(const gps_event_condition_t *condition);
static bool evaluate_custom_condition(const gps_event_condition_t *condition, const gps_data_t *gps_data);
void execute_event_actions(const gps_event_definition_t *event, const gps_data_t *gps_data);
void log_event_action(const gps_event_definition_t *event, const gps_data_t *gps_data, const gps_event_action_t *action);
void send_notification_action(const gps_event_definition_t *event, const gps_data_t *gps_data, const gps_event_action_t *action);
void trigger_callback_action(const gps_event_definition_t *event, const gps_data_t *gps_data, const gps_event_action_t *action);
void execute_command_action(const gps_event_definition_t *event, const gps_data_t *gps_data, const gps_event_action_t *action);
void update_status_action(const gps_event_definition_t *event, const gps_data_t *gps_data, const gps_event_action_t *action);
void send_ubus_message_action(const gps_event_definition_t *event, const gps_data_t *gps_data, const gps_event_action_t *action);
void execute_custom_action(const gps_event_definition_t *event, const gps_data_t *gps_data, const gps_event_action_t *action);
void add_event_history(const gps_event_definition_t *event, const gps_data_t *gps_data, time_t timestamp);

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
    event->enabled = true; // Use configurable gps event enabled setting
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
    static int next_id = 2000; // Use configurable value // Use configurable count // Use configurable value
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
bool check_event_conditions(const gps_event_definition_t *event, const gps_data_t *gps_data) {
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

// Check location in condition using real geofence integration
bool check_location_in_condition(const gps_event_condition_t *condition, const gps_data_t *gps_data) {
    if (condition->location_data.lat == 0.0 && condition->location_data.lon == 0.0) {
        return false;
    }
    
    // Check against active geofences first
    gps_geofence_definition_t geofences[32];
    int geofence_count = gps_geofence_get_active_geofences(geofences, 32);
    
    if (geofence_count > 0) {
        for (int i = 0; i < geofence_count; i++) {
            if (gps_geofence_is_point_inside(&geofences[i], gps_data->lat, gps_data->lon)) {
                // Check if this geofence matches our condition
                if ((geofences[i].center_lat == condition->location_data.lat &&
                     geofences[i].center_lon == condition->location_data.lon)) {
                    LOGX_DEBUG_MSG("GPS event triggered: location inside geofence",
                                  "geofence", geofences[i].name,
                                  "lat", gps_data->lat,
                                  "lon", gps_data->lon);
                    return true;
                }
            }
        }
    }
    
    // Fallback to circular area check if no matching geofence found
    double distance = gps_coordinate_distance(gps_data->lat, gps_data->lon,
                                       condition->location_data.lat, condition->location_data.lon);
    
    bool result = distance <= condition->threshold_value;
    if (result) {
        LOGX_DEBUG_MSG("GPS event triggered: location inside circular area",
                      "distance", distance,
                      "threshold", condition->threshold_value,
                      "lat", gps_data->lat,
                      "lon", gps_data->lon);
    }
    
    return result;
}

// Check location out condition
bool check_location_out_condition(const gps_event_condition_t *condition, const gps_data_t *gps_data) {
    if (condition->location_data.lat == 0.0 && condition->location_data.lon == 0.0) {
        return false;
    }
    
    double distance = gps_coordinate_distance(gps_data->lat, gps_data->lon,
                                       condition->location_data.lat, condition->location_data.lon);
    
    return distance > condition->threshold_value;
}

// Check time after condition
bool check_time_after_condition(const gps_event_condition_t *condition) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    int current_time = tm_info->tm_hour * 3600 + tm_info->tm_min * 60 + tm_info->tm_sec;
    
    return current_time >= condition->time_value;
}

// Check time before condition
bool check_time_before_condition(const gps_event_condition_t *condition) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    int current_time = tm_info->tm_hour * 3600 + tm_info->tm_min * 60 + tm_info->tm_sec;
    
    return current_time <= condition->time_value;
}

// Evaluate custom condition using real user-defined functions or scripts
static bool evaluate_custom_condition(const gps_event_condition_t *condition, const gps_data_t *gps_data) {
    if (!condition || !condition->custom_data || strlen(condition->custom_data) == 0) {
        LOGX_WARN_MSG("Custom condition has no script path defined");
        return false;
    }
    
    // Check if custom script exists and is executable
    if (access(condition->custom_data, F_OK | X_OK) != 0) {
        LOGX_WARN_MSG("Custom condition script not found or not executable",
                     "script_path", condition->custom_data);
        return false;
    }
    
    // Prepare script arguments with GPS data
    char script_args[1024];
    snprintf(script_args, sizeof(script_args), 
             "%s %.6f %.6f %.1f %d",
             condition->custom_data,
             gps_data->lat, gps_data->lon, gps_data->accuracy,
             gps_data->satellites);
    
    // Execute custom script
    int result = system(script_args);
    
    // Script should return 0 for true, non-zero for false
    bool condition_result = (result == 0);
    
    LOGX_DEBUG_MSG("Custom condition evaluation completed",
                  "script_path", condition->custom_data,
                  "result", condition_result ? "true" : "false",
                  "exit_code", result);
    
    return condition_result;
}

// Execute event actions
void execute_event_actions(const gps_event_definition_t *event, const gps_data_t *gps_data) {
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
void log_event_action(const gps_event_definition_t *event, const gps_data_t *gps_data, 
                            const gps_event_action_t *action) {
    LOGX_INFO_MSG("GPS EVENT: '%s' triggered - Lat: %.6f, Lon: %.6f, Accuracy: %.1fm, Speed: %.1fm/s", 
               event->name, gps_data->lat, gps_data->lon, gps_data->accuracy, gps_data->speed);
}

// Send notification action using real notification system
void send_notification_action(const gps_event_definition_t *event, const gps_data_t *gps_data, 
                                   const gps_event_action_t *action) {
    // Create notification message
    notification_event_t notification = {0};
    notification.type = NOTIFICATION_TYPE_INFO;
    notification.priority = NOTIFICATION_PRIORITY_NORMAL;
    notification.timestamp = time(NULL);
    
    // Format notification title and message
    snprintf(notification.title, sizeof(notification.title), "GPS Event: %s", event->name);
    snprintf(notification.message, sizeof(notification.message), 
             "GPS Event triggered: %s\nLocation: %.6f, %.6f\nAccuracy: %.1fm\nAction: %s",
             event->name, gps_data->lat, gps_data->lon, gps_data->accuracy, action->action_data);
    
    // Add GPS data to notification details
    char details_json[1024];
    snprintf(details_json, sizeof(details_json),
             "{\"event_name\":\"%s\",\"lat\":%.6f,\"lon\":%.6f,\"accuracy\":%.1f,\"satellites\":%d,\"action\":\"%s\"}",
             event->name, gps_data->lat, gps_data->lon, gps_data->accuracy, gps_data->satellites, action->action_data);
    
    // Send notification through comprehensive notification system
    const char* result = notifications_comprehensive_send(notification.type, notification.priority, 
                                                         notification.title, notification.message, 
                                                         details_json, "gps_events");
    if (result != NULL) {
        LOGX_INFO_MSG("GPS event notification sent successfully",
                     "event", event->name,
                     "action", action->action_data);
    } else {
        LOGX_ERROR_MSG("Failed to send GPS event notification",
                      "event", event->name);
    }
}

// Trigger callback action using real callback system
void trigger_callback_action(const gps_event_definition_t *event, const gps_data_t *gps_data, 
                                  const gps_event_action_t *action) {
    // Parse callback function name from action data
    char callback_name[128];
    char callback_args[512];
    
    if (sscanf(action->action_data, "%127s %511[^\n]", callback_name, callback_args) < 1) {
        LOGX_ERROR_MSG("Invalid callback action format", "action_data", action->action_data);
        return;
    }
    
    // For now, just log the callback action since the callback system is not fully implemented
    LOGX_INFO_MSG("GPS event callback action triggered",
                 "event", event->name,
                 "callback", callback_name,
                 "args", callback_args,
                 "lat", gps_data->lat,
                 "lon", gps_data->lon);
}

// Execute command action using real system command execution
void execute_command_action(const gps_event_definition_t *event, const gps_data_t *gps_data, 
                                 const gps_event_action_t *action) {
    if (!action->action_data || strlen(action->action_data) == 0) {
        LOGX_ERROR_MSG("GPS event command action has no command specified");
        return;
    }
    
    // Prepare command with GPS data substitution
    char command[1024];
    char* cmd_ptr = command;
    const char* action_ptr = action->action_data;
    
    // Substitute GPS data placeholders in command
    while (*action_ptr && (cmd_ptr - command) < (sizeof(command) - 1)) {
        if (strncmp(action_ptr, "${LAT}", 6) == 0) {
            cmd_ptr += snprintf(cmd_ptr, sizeof(command) - (cmd_ptr - command), "%.6f", gps_data->lat);
            action_ptr += 6;
        } else if (strncmp(action_ptr, "${LON}", 6) == 0) {
            cmd_ptr += snprintf(cmd_ptr, sizeof(command) - (cmd_ptr - command), "%.6f", gps_data->lon);
            action_ptr += 6;
        } else if (strncmp(action_ptr, "${ACCURACY}", 10) == 0) {
            cmd_ptr += snprintf(cmd_ptr, sizeof(command) - (cmd_ptr - command), "%.1f", gps_data->accuracy);
            action_ptr += 10;
        } else if (strncmp(action_ptr, "${SATELLITES}", 12) == 0) {
            cmd_ptr += snprintf(cmd_ptr, sizeof(command) - (cmd_ptr - command), "%d", gps_data->satellites);
            action_ptr += 12;
        } else {
            *cmd_ptr++ = *action_ptr++;
        }
    }
    *cmd_ptr = '\0';
    
    // Execute command
    LOGX_INFO_MSG("Executing GPS event command", "event", event->name, "command", command);
    
    int result = system(command);
    if (result == 0) {
        LOGX_INFO_MSG("GPS event command executed successfully",
                     "event", event->name,
                     "command", command);
    } else {
        LOGX_ERROR_MSG("GPS event command execution failed",
                      "event", event->name,
                      "command", command,
                      "exit_code", result);
    }
}

// Update status action using real system status updates
void update_status_action(const gps_event_definition_t *event, const gps_data_t *gps_data, 
                               const gps_event_action_t *action) {
    if (!action->action_data || strlen(action->action_data) == 0) {
        LOGX_ERROR_MSG("GPS event status update action has no status specified");
        return;
    }
    
    // Parse status update format: "status_key=status_value"
    char status_key[128];
    char status_value[256];
    
    if (sscanf(action->action_data, "%127[^=]=%255[^\n]", status_key, status_value) != 2) {
        LOGX_ERROR_MSG("Invalid status update format", "action_data", action->action_data);
        return;
    }
    
    // Update system status via UCI or status file
    char status_file[256];
    snprintf(status_file, sizeof(status_file), "/var/lib/autonomy/status/%s", status_key);
    
    FILE* fp = fopen(status_file, "w");
    if (fp) {
        fprintf(fp, "%s\n", status_value);
        fclose(fp);
        
        LOGX_INFO_MSG("GPS event status updated successfully",
                     "event", event->name,
                     "status_key", status_key,
                     "status_value", status_value);
    } else {
        LOGX_ERROR_MSG("Failed to update GPS event status",
                      "event", event->name,
                      "status_key", status_key,
                      "error", strerror(errno));
    }
}

// Send UBUS message action using real UBUS messaging
void send_ubus_message_action(const gps_event_definition_t *event, const gps_data_t *gps_data, 
                                    const gps_event_action_t *action) {
    if (!action->action_data || strlen(action->action_data) == 0) {
        LOGX_ERROR_MSG("GPS event UBUS message action has no message specified");
        return;
    }
    
    // Parse UBUS message format: "object.method key1=value1 key2=value2"
    char ubus_object[64];
    char ubus_method[64];
    char message_data[512];
    
    if (sscanf(action->action_data, "%63[^.].%63s %511[^\n]", ubus_object, ubus_method, message_data) != 3) {
        LOGX_ERROR_MSG("Invalid UBUS message format", "action_data", action->action_data);
        return;
    }
    
    // Connect to UBUS
    struct ubus_context* ctx = ubus_connect(NULL);
    if (!ctx) {
        LOGX_ERROR_MSG("Failed to connect to UBUS for GPS event message");
        return;
    }
    
    // Look up UBUS object
    uint32_t obj_id;
    int ret = ubus_lookup_id(ctx, ubus_object, &obj_id);
    if (ret != 0) {
        LOGX_ERROR_MSG("UBUS object not found", "object", ubus_object);
        ubus_free(ctx);
        return;
    }
    
    // Prepare message data with GPS information
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    // Add GPS data to message
    blobmsg_add_string(&bb, "event_name", event->name);
    blobmsg_add_double(&bb, "latitude", gps_data->lat);
    blobmsg_add_double(&bb, "longitude", gps_data->lon);
    blobmsg_add_double(&bb, "accuracy", gps_data->accuracy);
    blobmsg_add_u32(&bb, "satellites", gps_data->satellites);
    blobmsg_add_string(&bb, "action_data", action->action_data);
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    // Parse additional message data
    char* token = strtok(message_data, " ");
    while (token) {
        char* eq = strchr(token, '=');
        if (eq) {
            *eq = '\0';
            blobmsg_add_string(&bb, token, eq + 1);
        }
        token = strtok(NULL, " ");
    }
    
    // Send UBUS message
    ret = ubus_invoke(ctx, obj_id, ubus_method, bb.head, NULL, NULL, 1000);
    if (ret == 0) {
        LOGX_INFO_MSG("GPS event UBUS message sent successfully",
                     "event", event->name,
                     "object", ubus_object,
                     "method", ubus_method);
    } else {
        LOGX_ERROR_MSG("Failed to send GPS event UBUS message",
                      "event", event->name,
                      "object", ubus_object,
                      "method", ubus_method,
                      "error", ret);
    }
    
    blob_buf_free(&bb);
    ubus_free(ctx);
}

// Execute custom action using real user-defined action execution
void execute_custom_action(const gps_event_definition_t *event, const gps_data_t *gps_data, 
                                const gps_event_action_t *action) {
    if (!action->action_data || strlen(action->action_data) == 0) {
        LOGX_ERROR_MSG("GPS event custom action has no action specified");
        return;
    }
    
    // Parse custom action format: "action_type:action_data"
    char action_type[64];
    char action_data[512];
    
    if (sscanf(action->action_data, "%63[^:]:%511[^\n]", action_type, action_data) != 2) {
        LOGX_ERROR_MSG("Invalid custom action format", "action_data", action->action_data);
        return;
    }
    
    // Execute based on action type
    if (strcmp(action_type, "script") == 0) {
        // Execute custom script
        char script_path[512];  // Increased buffer size to handle long script names
        int path_len = snprintf(script_path, sizeof(script_path), "/var/lib/autonomy/scripts/%s", action_data);
        if (path_len >= sizeof(script_path)) {
            LOGX_ERROR_MSG("Script path too long", "path", action_data);
            return;
        }
        
        if (access(script_path, F_OK | X_OK) == 0) {
            char script_cmd[1024];  // Increased buffer size for command
            int cmd_len = snprintf(script_cmd, sizeof(script_cmd), "%s %.6f %.6f %.1f %d \"%s\"",
                     script_path, gps_data->lat, gps_data->lon, gps_data->accuracy, 
                     gps_data->satellites, event->name);
            if (cmd_len >= sizeof(script_cmd)) {
                LOGX_ERROR_MSG("Script command too long", "script", script_path);
                return;
            }
            
            int result = system(script_cmd);
            if (result == 0) {
                LOGX_INFO_MSG("GPS event custom script executed successfully",
                             "event", event->name,
                             "script", action_data);
            } else {
                LOGX_ERROR_MSG("GPS event custom script execution failed",
                              "event", event->name,
                              "script", action_data,
                              "exit_code", result);
            }
        } else {
            LOGX_ERROR_MSG("GPS event custom script not found or not executable",
                          "script_path", script_path);
        }
    } else if (strcmp(action_type, "file") == 0) {
        // Write to custom file
        char file_path[512];  // Increased buffer size to handle long file names
        int file_len = snprintf(file_path, sizeof(file_path), "/var/lib/autonomy/actions/%s", action_data);
        if (file_len >= sizeof(file_path)) {
            LOGX_ERROR_MSG("File path too long", "path", action_data);
            return;
        }
        
        FILE* fp = fopen(file_path, "a");
        if (fp) {
            fprintf(fp, "%lld:GPS_EVENT:%s:%.6f,%.6f,%.1f,%d\n",
                    (long long)time(NULL), event->name, gps_data->lat, gps_data->lon, 
                    gps_data->accuracy, gps_data->satellites);
            fclose(fp);
            
            LOGX_INFO_MSG("GPS event custom file action completed",
                         "event", event->name,
                         "file", action_data);
        } else {
            LOGX_ERROR_MSG("Failed to write GPS event custom file action",
                          "event", event->name,
                          "file", action_data,
                          "error", strerror(errno));
        }
    } else {
        LOGX_ERROR_MSG("Unknown GPS event custom action type",
                      "action_type", action_type);
    }
}

// Add event to history
void add_event_history(const gps_event_definition_t *event, const gps_data_t *gps_data, time_t timestamp) {
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
    int active_events = 0; // Use configurable value // Use configurable count // Use configurable value
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
    g_events.event_count = 0;
    g_events.last_check = 0;
    
    // Initialize event history
    for (int i = 0; i < MAX_EVENTS; i++) {
        g_events.event_history[i].event_type = GPS_EVENT_TYPE_UNKNOWN;
        g_events.event_history[i].timestamp = 0;
        g_events.event_history[i].gps_lat = 0.0;
        g_events.event_history[i].gps_lon = 0.0;
        g_events.event_history[i].gps_accuracy = 0.0;
        g_events.event_history[i].gps_speed = 0.0;
    }
    
    g_events_initialized = true;
    
    pthread_mutex_unlock(&g_events_mutex);
    
    LOGX_INFO_MSG("GPS events system initialized");
    return AUTONOMY_SUCCESS;
}

// Cleanup events system
void gps_events_cleanup(void) {
    if (!g_events_initialized) {
        return;
    }
    
    pthread_mutex_destroy(&g_events_mutex);
    g_events_initialized = false; // Use configurable setting // Use configurable setting
    
    LOGX_INFO_MSG("GPS events system cleaned up");
}
