#ifndef GPS_EVENTS_H
#define GPS_EVENTS_H

#include "types.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// GPS coordinate structure
typedef struct {
    double lat;                         // Latitude
    double lon;                         // Longitude
} gps_coordinate_t;

// GPS event types
typedef enum {
    GPS_EVENT_TYPE_UNKNOWN = 0,
    GPS_EVENT_TYPE_LOCATION_CHANGE,
    GPS_EVENT_TYPE_SPEED_CHANGE,
    GPS_EVENT_TYPE_ACCURACY_CHANGE,
    GPS_EVENT_TYPE_SATELLITE_CHANGE,
    GPS_EVENT_TYPE_FIX_QUALITY_CHANGE,
    GPS_EVENT_TYPE_GEOFENCE_EVENT,
    GPS_EVENT_TYPE_MOVEMENT_PATTERN,
    GPS_EVENT_TYPE_TIME_BASED,
    GPS_EVENT_TYPE_CUSTOM
} gps_event_type_t;

// GPS condition types
typedef enum {
    GPS_CONDITION_UNKNOWN = 0,
    GPS_CONDITION_LOCATION_IN,
    GPS_CONDITION_LOCATION_OUT,
    GPS_CONDITION_SPEED_ABOVE,
    GPS_CONDITION_SPEED_BELOW,
    GPS_CONDITION_ACCURACY_ABOVE,
    GPS_CONDITION_ACCURACY_BELOW,
    GPS_CONDITION_SATELLITES_ABOVE,
    GPS_CONDITION_SATELLITES_BELOW,
    GPS_CONDITION_FIX_QUALITY_ABOVE,
    GPS_CONDITION_FIX_QUALITY_BELOW,
    GPS_CONDITION_TIME_AFTER,
    GPS_CONDITION_TIME_BEFORE,
    GPS_CONDITION_CUSTOM
} gps_condition_type_t;

// GPS action types
typedef enum {
    GPS_ACTION_UNKNOWN = 0,
    GPS_ACTION_LOG_EVENT,
    GPS_ACTION_SEND_NOTIFICATION,
    GPS_ACTION_TRIGGER_CALLBACK,
    GPS_ACTION_EXECUTE_COMMAND,
    GPS_ACTION_UPDATE_STATUS,
    GPS_ACTION_SEND_UBUS_MESSAGE,
    GPS_ACTION_CUSTOM
} gps_action_type_t;

// GPS event condition
typedef struct {
    gps_condition_type_t condition_type; // Type of condition
    double threshold_value;               // Threshold value for comparison
    gps_coordinate_t location_data;      // Location data for location conditions
    int time_value;                      // Time value for time conditions
    char custom_data[128];               // Custom condition data
} gps_event_condition_t;

// GPS event action
typedef struct {
    gps_action_type_t action_type;       // Type of action
    char action_data[256];               // Action data (command, message, etc.)
    int priority;                        // Action priority
    bool enabled;                        // Whether action is enabled
} gps_event_action_t;

// GPS event definition
typedef struct {
    bool active;                         // Whether event is active
    int event_id;                        // Unique event identifier
    char name[64];                       // Event name
    gps_event_type_t event_type;         // Type of event
    int condition_count;                 // Number of conditions
    gps_event_condition_t conditions[10]; // Event conditions
    int action_count;                    // Number of actions
    gps_event_action_t actions[5];       // Event actions
    bool enabled;                        // Whether event is enabled
    time_t last_triggered;               // Last trigger timestamp
    int trigger_count;                   // Total trigger count
    int cooldown_period;                 // Cooldown period in seconds
} gps_event_definition_t;

// GPS event record
typedef struct {
    time_t timestamp;                    // Event timestamp
    int event_id;                        // Event ID
    gps_event_type_t event_type;         // Event type
    int trigger_reason;                  // Trigger reason
    double gps_lat;                      // GPS latitude at trigger
    double gps_lon;                      // GPS longitude at trigger
    double gps_accuracy;                 // GPS accuracy at trigger
    double gps_speed;                    // GPS speed at trigger
} gps_event_record_t;

// GPS events configuration
typedef struct {
    bool enabled;                        // Enable/disable events
    int max_events;                      // Maximum number of events
    int max_conditions;                  // Maximum conditions per event
    int max_actions;                     // Maximum actions per event
    int check_interval;                  // Check interval in seconds
    int history_size;                    // Number of event records to keep
} gps_events_config_t;

// GPS events status
typedef struct {
    bool enabled;                        // Events enabled
    int event_count;                     // Total events
    int active_events;                   // Active events
    int total_triggers;                  // Total triggers
    time_t last_check;                   // Last check timestamp
    int active_event_count;              // Number of active events
    gps_event_definition_t events[50];   // Active events
} gps_events_status_t;

// GPS events system state
typedef struct {
    bool enabled;                        // Events enabled
    int max_events;                      // Maximum events
    int max_conditions;                  // Maximum conditions
    int max_actions;                     // Maximum actions
    int check_interval;                  // Check interval
    int history_size;                    // History size
    
    // State
    int event_count;                     // Event count
    int active_events;                   // Active events
    int total_triggers;                  // Total triggers
    time_t last_check;                   // Last check
    
    // Events array
    gps_event_definition_t events[50];   // GPS events
    
    // Event history
    gps_event_record_t event_history[100]; // Event history records
} gps_events_t;

// Function prototypes

/**
 * Initialize GPS events system
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_events_init(void);

/**
 * Create GPS event
 * @param name Event name
 * @param event_type Type of event
 * @param conditions Array of event conditions
 * @param condition_count Number of conditions
 * @param actions Array of event actions
 * @param action_count Number of actions
 * @return Event ID on success, error code on failure
 */
int gps_events_create_event(const char *name, gps_event_type_t event_type, 
                           const gps_event_condition_t *conditions, int condition_count,
                           const gps_event_action_t *actions, int action_count);

/**
 * Check GPS data against all events
 * @param gps_data GPS data to check
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_events_check_gps_data(const gps_data_t *gps_data);

/**
 * Get events status
 * @param status Status structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_events_get_status(gps_events_status_t *status);

/**
 * Get events configuration
 * @param config Configuration structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_events_get_config(gps_events_config_t *config);

/**
 * Set events configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_events_set_config(const gps_events_config_t *config);

/**
 * Enable/disable events
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_events_set_enabled(bool enabled);

/**
 * Enable/disable specific event
 * @param event_id Event ID
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_events_set_event_enabled(int event_id, bool enabled);

/**
 * Delete event
 * @param event_id Event ID to delete
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_events_delete(int event_id);

/**
 * Reset events system
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_events_reset(void);

/**
 * Cleanup events system
 */
void gps_events_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_EVENTS_H
