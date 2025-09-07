#ifndef EMAIL_CLIENT_H
#define EMAIL_CLIENT_H

#include "notification_types.h"
#include <stdbool.h>
#include <time.h>

// Email configuration
typedef struct {
    bool enabled;
    char smtp_host[256];
    int smtp_port;
    char from_address[256];
    char from_name[128];
    char recipients[1024]; // Comma-separated list
    
    // Authentication
    char username[256];
    char password[256];
    
    // TLS configuration
    bool use_tls;
    bool use_starttls;
    bool verify_ssl;
    
    // Timeout configuration
    int timeout_seconds;
    
    // Retry configuration
    int retry_attempts;
    int retry_delay_seconds;
    
    // Formatting options
    bool html_format;
    bool include_context;
    char custom_subject_prefix[64];
} email_config_t;

// Email client status
typedef struct {
    bool enabled;
    char smtp_host[256];
    int smtp_port;
    int total_sent;
    int total_failed;
    time_t last_sent_time;
    time_t last_error_time;
    char last_error[256];
    int recipient_count;
} email_client_status_t;

// Email client structure
typedef struct {
    email_config_t config;
    email_client_status_t status;
    char recipients_array[16][256]; // Parsed recipients
    int recipient_count;
} email_client_t;

// Initialize email client
int email_client_init(email_client_t* client, const email_config_t* config);

// Clean up email client
void email_client_cleanup(email_client_t* client);

// Send notification via email
int email_client_send(email_client_t* client, const notification_event_t* event);

// Get email client status
void email_client_get_status(email_client_t* client, email_client_status_t* status);

// Format email subject
void email_client_format_subject(email_client_t* client, const notification_event_t* event, 
                                char* subject, size_t max_size);

// Format email body
void email_client_format_body(email_client_t* client, const notification_event_t* event, 
                             char* body, size_t max_size);

// Get priority color for HTML formatting
const char* email_client_get_priority_color(notification_priority_t priority);

// Get priority text for display
const char* email_client_get_priority_text(notification_priority_t priority);

#endif // EMAIL_CLIENT_H
