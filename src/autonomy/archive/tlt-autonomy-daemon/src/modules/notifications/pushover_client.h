#ifndef PUSHOVER_CLIENT_H
#define PUSHOVER_CLIENT_H

#include "notification_types.h"
#include <stdbool.h>
#include <time.h>

// Pushover configuration
typedef struct {
    bool enabled;
    char token[256];
    char user[256];
    char device[128];
    int default_priority;
    char default_sound[32];
    int timeout_seconds;
    int retry_attempts;
    int retry_delay_seconds;
} pushover_config_t;

// Pushover message
typedef struct {
    char token[256];
    char user[256];
    char message[1024];
    char title[256];
    int priority;
    char sound[32];
    char device[128];
    time_t timestamp;
    char url[512];
    char url_title[128];
    bool html_enabled;
    int retry_seconds;
    int expire_seconds;
} pushover_message_t;

// Pushover client status
typedef struct {
    bool enabled;
    char token[256];
    char user[256];
    int total_sent;
    int total_failed;
    int last_response_code;
    time_t last_sent_time;
    time_t last_error_time;
    char last_error[256];
} pushover_client_status_t;

// Pushover client structure
typedef struct {
    pushover_config_t config;
    pushover_client_status_t status;
} pushover_client_t;

// Initialize pushover client
int pushover_client_init(pushover_client_t* client, const pushover_config_t* config);

// Clean up pushover client
void pushover_client_cleanup(pushover_client_t* client);

// Send notification via Pushover
int pushover_client_send(pushover_client_t* client, const notification_event_t* event);

// Get pushover client status
void pushover_client_get_status(pushover_client_t* client, pushover_client_status_t* status);

// Create pushover message from notification event
void pushover_client_create_message(pushover_client_t* client, const notification_event_t* event, 
                                   pushover_message_t* message);

// Get priority sound for notification priority
const char* pushover_client_get_priority_sound(notification_priority_t priority);

#endif // PUSHOVER_CLIENT_H
