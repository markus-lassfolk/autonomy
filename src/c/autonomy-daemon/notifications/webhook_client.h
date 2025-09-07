#ifndef WEBHOOK_CLIENT_H
#define WEBHOOK_CLIENT_H

#include "notification_types.h"
#include <stdbool.h>
#include <time.h>

// Webhook authentication types
typedef enum {
    WEBHOOK_AUTH_NONE = 0,
    WEBHOOK_AUTH_BEARER,
    WEBHOOK_AUTH_BASIC,
    WEBHOOK_AUTH_API_KEY,
    WEBHOOK_AUTH_CUSTOM
} webhook_auth_type_t;

// Webhook payload structure
typedef struct {
    char type[64];
    char title[256];
    char message[1024];
    int priority;
    char timestamp[32];
    char context_json[512];
    char source[32];
    char version[16];
    char hostname[128];
} webhook_payload_t;

// Webhook configuration
typedef struct {
    bool enabled;
    char name[128];
    char description[256];
    char url[512];
    char method[16];
    char content_type[64];
    
    // Authentication
    webhook_auth_type_t auth_type;
    char auth_token[256];
    char auth_username[128];
    char auth_password[128];
    char auth_header[64];
    
    // Custom headers
    char custom_headers[1024];
    
    // Retry configuration
    int retry_attempts;
    int retry_delay_seconds;
    int timeout_seconds;
    
    // SSL configuration
    bool verify_ssl;
    bool follow_redirects;
    
    // Filtering
    notification_priority_t priority_filter[8];
    int priority_filter_count;
    notification_type_t type_filter[16];
    int type_filter_count;
} webhook_config_t;

// Webhook client status
typedef struct {
    bool enabled;
    char url[512];
    int total_sent;
    int total_failed;
    int last_response_code;
    time_t last_sent_time;
    time_t last_error_time;
    char last_error[256];
} webhook_client_status_t;

// Webhook client structure
typedef struct {
    webhook_config_t config;
    webhook_client_status_t status;
} webhook_client_t;

// Initialize webhook client
int webhook_client_init(webhook_client_t* client, const webhook_config_t* config);

// Clean up webhook client
void webhook_client_cleanup(webhook_client_t* client);

// Send notification via webhook
int webhook_client_send(webhook_client_t* client, const notification_event_t* event);

// Get webhook client status
void webhook_client_get_status(webhook_client_t* client, webhook_client_status_t* status);

// Check if notification should be sent based on filters
bool webhook_client_should_send(webhook_client_t* client, const notification_event_t* event);

// Create webhook payload from notification event
void webhook_client_create_payload(webhook_client_t* client, const notification_event_t* event, 
                                  webhook_payload_t* payload);

#endif // WEBHOOK_CLIENT_H
