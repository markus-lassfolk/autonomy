#include "sms_client.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <libubus.h>
#include <libblobmsg_json.h>

// Initialize SMS client
int sms_client_init(sms_client_t* client, const sms_config_t* config) {
    if (!client || !config) {
        return -1;
    }
    
    memset(client, 0, sizeof(sms_client_t));
    
    // Copy configuration
    client->config = *config;
    
    // Initialize status
    client->status.enabled = config->enabled;
    client->status.provider = config->provider;
    strncpy(client->status.phone_number, config->phone_number, sizeof(client->status.phone_number) - 1);
    strncpy(client->status.modem_path, config->modem_path, sizeof(client->status.modem_path) - 1);
    client->status.total_sent = 0;
    client->status.total_failed = 0;
    client->status.last_sent_time = 0;
    client->status.last_error_time = 0;
    client->status.last_error[0] = '\0';
    client->status.messages_sent_this_hour = 0;
    client->status.hour_reset_time = time(NULL);
    
    // Set defaults if not configured
    if (strlen(client->config.modem_path) == 0) {
        strncpy(client->config.modem_path, "gsm.modem1", sizeof(client->config.modem_path) - 1);
    }
    
    if (client->config.max_message_length == 0) {
        client->config.max_message_length = 160; // Standard SMS length
    }
    
    if (client->config.max_messages_per_hour == 0) {
        client->config.max_messages_per_hour = 10; // Conservative default
    }
    
    return 0;
}

// Clean up SMS client
void sms_client_cleanup(sms_client_t* client) {
    if (!client) return;
    
    // Clear sensitive data
    memset(client->config.api_key, 0, sizeof(client->config.api_key));
    memset(client->config.api_secret, 0, sizeof(client->config.api_secret));
    memset(client, 0, sizeof(sms_client_t));
}

// Check rate limiting
bool sms_client_check_rate_limit(sms_client_t* client) {
    if (!client) return false;
    
    time_t now = time(NULL);
    
    // Reset hourly counter if needed
    if (now - client->status.hour_reset_time >= 3600) { // 1 hour
        client->status.messages_sent_this_hour = 0;
        client->status.hour_reset_time = now;
    }
    
    // Check hourly limit
    if (client->status.messages_sent_this_hour >= client->config.max_messages_per_hour) {
        strncpy(client->status.last_error, "Hourly SMS limit exceeded", sizeof(client->status.last_error) - 1);
        client->status.last_error_time = now;
        return false;
    }
    
    // Check cooldown period
    if (client->config.cooldown_period_seconds > 0) {
        if (now - client->status.last_sent_time < client->config.cooldown_period_seconds) {
            strncpy(client->status.last_error, "SMS cooldown period active", sizeof(client->status.last_error) - 1);
            client->status.last_error_time = now;
            return false;
        }
    }
    
    return true;
}

// Update rate limiting counters
void sms_client_update_rate_limit(sms_client_t* client) {
    if (!client) return;
    
    time_t now = time(NULL);
    client->status.messages_sent_this_hour++;
    client->status.last_sent_time = now;
}

// Format SMS message from notification event
void sms_client_format_message(sms_client_t* client, const notification_event_t* event, 
                              char* sms_text, size_t max_size) {
    if (!client || !event || !sms_text) return;
    
    char formatted_message[512];
    formatted_message[0] = '\0';
    
    // Add priority if enabled
    if (client->config.include_priority) {
        const char* priority_emoji = "";
        switch (event->priority) {
            case NOTIFICATION_PRIORITY_EMERGENCY:
                priority_emoji = "🚨";
                break;
            case NOTIFICATION_PRIORITY_HIGH:
                priority_emoji = "⚠️";
                break;
            case NOTIFICATION_PRIORITY_NORMAL:
                priority_emoji = "ℹ️";
                break;
            default:
                priority_emoji = "📝";
                break;
        }
        
        snprintf(formatted_message, sizeof(formatted_message), "%s ", priority_emoji);
    }
    
    // Add title and message
    strncat(formatted_message, event->title, sizeof(formatted_message) - strlen(formatted_message) - 1);
    strncat(formatted_message, ": ", sizeof(formatted_message) - strlen(formatted_message) - 1);
    strncat(formatted_message, event->message, sizeof(formatted_message) - strlen(formatted_message) - 1);
    
    // Add type if enabled
    if (client->config.include_type) {
        char type_info[64];
        snprintf(type_info, sizeof(type_info), " [%s]", notification_type_to_string(event->type));
        strncat(formatted_message, type_info, sizeof(formatted_message) - strlen(formatted_message) - 1);
    }
    
    // Add timestamp if enabled
    if (client->config.include_timestamp) {
        struct tm* tm_info = localtime(&event->timestamp);
        char timestamp_str[32];
        strftime(timestamp_str, sizeof(timestamp_str), " %H:%M", tm_info);
        strncat(formatted_message, timestamp_str, sizeof(formatted_message) - strlen(formatted_message) - 1);
    }
    
    // Truncate to SMS length limit
    if (strlen(formatted_message) > client->config.max_message_length) {
        formatted_message[client->config.max_message_length - 3] = '.';
        formatted_message[client->config.max_message_length - 2] = '.';
        formatted_message[client->config.max_message_length - 1] = '.';
        formatted_message[client->config.max_message_length] = '\0';
    }
    
    strncpy(sms_text, formatted_message, max_size - 1);
}

// Send SMS via RUTOS UBUS
int sms_client_send_via_rutos_ubus(sms_client_t* client, const char* message) {
    if (!client || !message) {
        return -1;
    }
    
    struct ubus_context* ctx = ubus_connect(NULL);
    if (!ctx) {
        strncpy(client->status.last_error, "Failed to connect to UBUS", sizeof(client->status.last_error) - 1);
        client->status.last_error_time = time(NULL);
        return -1;
    }
    
    uint32_t id;
    if (ubus_lookup_id(ctx, client->config.modem_path, &id)) {
        strncpy(client->status.last_error, "Failed to find modem UBUS object", sizeof(client->status.last_error) - 1);
        client->status.last_error_time = time(NULL);
        ubus_free(ctx);
        return -1;
    }
    
    // Prepare UBUS call arguments
    struct blob_buf bb;
    blob_buf_init(&bb, 0);
    blobmsg_add_string(&bb, "number", client->config.phone_number);
    blobmsg_add_string(&bb, "message", message);
    
    // Call UBUS method to send SMS
    int result = ubus_invoke(ctx, id, "send_sms", bb.head, NULL, NULL, 5000); // 5 second timeout
    
    blob_buf_free(&bb);
    ubus_free(ctx);
    
    if (result != 0) {
        snprintf(client->status.last_error, sizeof(client->status.last_error), 
                "UBUS send_sms failed: %d", result);
        client->status.last_error_time = time(NULL);
        return -1;
    }
    
    printf("SMS: Sent via RUTOS UBUS to %s: %.50s%s\n", 
           client->config.phone_number, message, strlen(message) > 50 ? "..." : "");
    
    return 0;
}

// Send SMS via AT commands (fallback)
int sms_client_send_via_at_command(sms_client_t* client, const char* message) {
    if (!client || !message) {
        return -1;
    }
    
    if (strlen(client->config.at_device) == 0) {
        strncpy(client->status.last_error, "AT command device not configured", sizeof(client->status.last_error) - 1);
        client->status.last_error_time = time(NULL);
        return -1;
    }
    
    // This would require proper serial communication
    // For now, use a system command approach (simplified)
    
    char command[1024];
    snprintf(command, sizeof(command),
             "echo -e 'AT+CMGF=1\\r\\nAT+CMGS=\"%s\"\\r\\n%s\\x1A' > %s",
             client->config.phone_number, message, client->config.at_device);
    
    int result = system(command);
    
    if (result != 0) {
        snprintf(client->status.last_error, sizeof(client->status.last_error), 
                "AT command failed: %d", result);
        client->status.last_error_time = time(NULL);
        return -1;
    }
    
    printf("SMS: Sent via AT command to %s: %.50s%s\n", 
           client->config.phone_number, message, strlen(message) > 50 ? "..." : "");
    
    return 0;
}

// Send notification via SMS
int sms_client_send(sms_client_t* client, const notification_event_t* event) {
    if (!client || !event || !client->config.enabled) {
        return -1;
    }
    
    if (strlen(client->config.phone_number) == 0) {
        strncpy(client->status.last_error, "Phone number not configured", sizeof(client->status.last_error) - 1);
        client->status.last_error_time = time(NULL);
        client->status.total_failed++;
        return -1;
    }
    
    // Check rate limiting
    if (!sms_client_check_rate_limit(client)) {
        client->status.total_failed++;
        return -1; // Rate limited
    }
    
    // Format SMS message
    char sms_text[512];
    sms_client_format_message(client, event, sms_text, sizeof(sms_text));
    
    // Send via configured provider with retry logic
    int max_attempts = client->config.retry_attempts > 0 ? client->config.retry_attempts : 3;
    int retry_delay = client->config.retry_delay_seconds > 0 ? client->config.retry_delay_seconds : 5;
    
    int result = -1;
    for (int attempt = 1; attempt <= max_attempts; attempt++) {
        switch (client->config.provider) {
            case SMS_PROVIDER_RUTOS_UBUS:
                result = sms_client_send_via_rutos_ubus(client, sms_text);
                break;
            case SMS_PROVIDER_AT_COMMAND:
                result = sms_client_send_via_at_command(client, sms_text);
                break;
            default:
                strncpy(client->status.last_error, "Unsupported SMS provider", sizeof(client->status.last_error) - 1);
                client->status.last_error_time = time(NULL);
                result = -1;
                break;
        }
        
        if (result == 0) {
            // Success
            client->status.total_sent++;
            sms_client_update_rate_limit(client);
            client->status.last_error[0] = '\0';
            break;
        }
        
        // Failed - wait before retry (except on last attempt)
        if (attempt < max_attempts) {
            sleep(retry_delay);
        }
    }
    
    if (result != 0) {
        client->status.total_failed++;
    }
    
    return result;
}

// Get SMS client status
void sms_client_get_status(sms_client_t* client, sms_client_status_t* status) {
    if (!client || !status) return;
    
    *status = client->status;
}
