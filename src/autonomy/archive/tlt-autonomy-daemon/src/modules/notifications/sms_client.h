#ifndef SMS_CLIENT_H
#define SMS_CLIENT_H

#include "notification_types.h"
#include <stdbool.h>
#include <time.h>

// SMS provider types
typedef enum {
    SMS_PROVIDER_RUTOS_UBUS = 0,
    SMS_PROVIDER_AT_COMMAND,
    SMS_PROVIDER_TWILIO,
    SMS_PROVIDER_AWS_SNS
} sms_provider_t;

// SMS configuration
typedef struct {
    bool enabled;
    sms_provider_t provider;
    char phone_number[32];
    
    // RUTOS/UBUS settings
    char modem_path[64]; // e.g., "gsm.modem1"
    
    // AT command settings
    char at_device[64];  // e.g., "/dev/ttyUSB0"
    int at_baud_rate;
    
    // External provider settings
    char api_key[256];
    char api_secret[256];
    char from_number[32];
    
    // Rate limiting
    int max_messages_per_hour;
    time_t cooldown_period_seconds;
    
    // Retry configuration
    int timeout_seconds;
    int retry_attempts;
    int retry_delay_seconds;
    
    // Message formatting
    bool include_priority;
    bool include_timestamp;
    bool include_type;
    int max_message_length;
} sms_config_t;

// SMS client status
typedef struct {
    bool enabled;
    sms_provider_t provider;
    char phone_number[32];
    char modem_path[64];
    int total_sent;
    int total_failed;
    time_t last_sent_time;
    time_t last_error_time;
    char last_error[256];
    int messages_sent_this_hour;
    time_t hour_reset_time;
} sms_client_status_t;

// SMS client structure
typedef struct {
    sms_config_t config;
    sms_client_status_t status;
} sms_client_t;

// Initialize SMS client
int sms_client_init(sms_client_t* client, const sms_config_t* config);

// Clean up SMS client
void sms_client_cleanup(sms_client_t* client);

// Send notification via SMS
int sms_client_send(sms_client_t* client, const notification_event_t* event);

// Get SMS client status
void sms_client_get_status(sms_client_t* client, sms_client_status_t* status);

// Format SMS message from notification event
void sms_client_format_message(sms_client_t* client, const notification_event_t* event, 
                              char* sms_text, size_t max_size);

// Send SMS via RUTOS UBUS
int sms_client_send_via_rutos_ubus(sms_client_t* client, const char* message);

// Send SMS via AT commands
int sms_client_send_via_at_command(sms_client_t* client, const char* message);

// Check rate limiting
bool sms_client_check_rate_limit(sms_client_t* client);

// Update rate limiting counters
void sms_client_update_rate_limit(sms_client_t* client);

#endif // SMS_CLIENT_H
