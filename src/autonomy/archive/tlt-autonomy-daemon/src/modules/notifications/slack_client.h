#ifndef SLACK_CLIENT_H
#define SLACK_CLIENT_H

#include "notification_types.h"
#include <stdbool.h>
#include <time.h>

// Slack field
typedef struct {
    char title[128];
    char value[256];
    bool short_field;
} slack_field_t;

// Slack attachment
typedef struct {
    char color[16];
    char title[256];
    char text[1024];
    slack_field_t fields[16];
    int field_count;
    char footer[128];
    char footer_icon[256];
    time_t timestamp;
} slack_attachment_t;

// Slack message
typedef struct {
    char text[512];
    char username[128];
    char icon_emoji[32];
    char icon_url[256];
    char channel[64];
    slack_attachment_t attachments[4];
    int attachment_count;
} slack_message_t;

// Slack configuration
typedef struct {
    bool enabled;
    char webhook_url[512];
    char channel[64];
    char username[128];
    char icon_emoji[32];
    char icon_url[256];
    int timeout_seconds;
    int retry_attempts;
    int retry_delay_seconds;
    bool use_attachments;
    bool include_context;
} slack_config_t;

// Slack client status
typedef struct {
    bool enabled;
    char webhook_url[512];
    char channel[64];
    int total_sent;
    int total_failed;
    int last_response_code;
    time_t last_sent_time;
    time_t last_error_time;
    char last_error[256];
} slack_client_status_t;

// Slack client structure
typedef struct {
    slack_config_t config;
    slack_client_status_t status;
} slack_client_t;

// Initialize slack client
int slack_client_init(slack_client_t* client, const slack_config_t* config);

// Clean up slack client
void slack_client_cleanup(slack_client_t* client);

// Send notification via Slack
int slack_client_send(slack_client_t* client, const notification_event_t* event);

// Get slack client status
void slack_client_get_status(slack_client_t* client, slack_client_status_t* status);

// Create slack message from notification event
void slack_client_create_message(slack_client_t* client, const notification_event_t* event, 
                                slack_message_t* message);

// Get attachment color for notification priority
const char* slack_client_get_attachment_color(notification_priority_t priority);

// Get priority text for display
const char* slack_client_get_priority_text(notification_priority_t priority);

#endif // SLACK_CLIENT_H
