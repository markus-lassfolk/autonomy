#include "slack_client.h"
#include "../shared/logging/logx.h"
#include "../shared/utils/http_client_libcurl.h"
#include "../core/types.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <json-c/json.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Slack client now uses HTTP client library

// Initialize slack client
int slack_client_init(slack_client_t* client, const slack_config_t* config) {
    if (!client || !config) {
        LOGX_ERROR_MSG("Slack client initialization failed: invalid parameters");
        return -1;
    }
    
    memset(client, 0, sizeof(slack_client_t));
    
    // Copy configuration
    client->config = *config;
    
    // Initialize status
    client->status.enabled = config->enabled;
    strncpy(client->status.webhook_url, config->webhook_url, sizeof(client->status.webhook_url) - 1);
    strncpy(client->status.channel, config->channel, sizeof(client->status.channel) - 1);
    client->status.total_sent = 0;
    client->status.total_failed = 0;
    client->status.last_response_code = 0;
    client->status.last_sent_time = 0;
    client->status.last_error_time = 0;
    client->status.last_error[0] = '\0';
    
    return 0;
}

// Clean up slack client
void slack_client_cleanup(slack_client_t* client) {
    if (!client) return;
    
    // Clear sensitive data
    memset(client, 0, sizeof(slack_client_t));
}

// Get attachment color for notification priority
const char* slack_client_get_attachment_color(notification_priority_t priority) {
    switch (priority) {
        case NOTIFICATION_PRIORITY_EMERGENCY:
            return "danger"; // Red
        case NOTIFICATION_PRIORITY_HIGH:
            return "warning"; // Orange
        case NOTIFICATION_PRIORITY_NORMAL:
            return "good"; // Green
        case NOTIFICATION_PRIORITY_LOW:
            return "#36a64f"; // Light green
        case NOTIFICATION_PRIORITY_LOWEST:
            return "#808080"; // Gray
        default:
            return "good"; // Green
    }
}

// Get priority text for display
const char* slack_client_get_priority_text(notification_priority_t priority) {
    switch (priority) {
        case NOTIFICATION_PRIORITY_EMERGENCY:
            return " Emergency";
        case NOTIFICATION_PRIORITY_HIGH:
            return " High";
        case NOTIFICATION_PRIORITY_NORMAL:
            return " Normal";
        case NOTIFICATION_PRIORITY_LOW:
            return " Low";
        case NOTIFICATION_PRIORITY_LOWEST:
            return " Lowest";
        default:
            return " Normal";
    }
}

// Create slack message from notification event
void slack_client_create_message(slack_client_t* client, const notification_event_t* event, 
                                slack_message_t* message) {
    if (!client || !event || !message) return;
    
    memset(message, 0, sizeof(slack_message_t));
    
    // Set username and icon
    if (strlen(client->config.username) > 0) {
        strncpy(message->username, client->config.username, sizeof(message->username) - 1);
    } else {
        strncpy(message->username, "autonomy", sizeof(message->username) - 1);
    }
    
    if (strlen(client->config.icon_emoji) > 0) {
        strncpy(message->icon_emoji, client->config.icon_emoji, sizeof(message->icon_emoji) - 1);
    } else if (strlen(client->config.icon_url) > 0) {
        strncpy(message->icon_url, client->config.icon_url, sizeof(message->icon_url) - 1);
    } else {
        strncpy(message->icon_emoji, ":satellite:", sizeof(message->icon_emoji) - 1);
    }
    
    // Set channel
    if (strlen(client->config.channel) > 0) {
        strncpy(message->channel, client->config.channel, sizeof(message->channel) - 1);
    }
    
    if (client->config.use_attachments) {
        // Create rich attachment
        slack_attachment_t* attachment = &message->attachments[0];
        message->attachment_count = 1;
        
        strncpy(attachment->color, slack_client_get_attachment_color(event->priority), sizeof(attachment->color) - 1);
        strncpy(attachment->title, event->title, sizeof(attachment->title) - 1);
        strncpy(attachment->text, event->message, sizeof(attachment->text) - 1);
        strncpy(attachment->footer, "autonomy Daemon", sizeof(attachment->footer) - 1);
        attachment->timestamp = event->timestamp;
        
        // Add priority field
        slack_field_t* priority_field = &attachment->fields[attachment->field_count++];
        strncpy(priority_field->title, "Priority", sizeof(priority_field->title) - 1);
        strncpy(priority_field->value, slack_client_get_priority_text(event->priority), sizeof(priority_field->value) - 1);
        priority_field->short_field = true;
        
        // Add type field
        slack_field_t* type_field = &attachment->fields[attachment->field_count++];
        strncpy(type_field->title, "Type", sizeof(type_field->title) - 1);
        strncpy(type_field->value, notification_type_to_string(event->type), sizeof(type_field->value) - 1);
        type_field->short_field = true;
        
        // Add context fields if enabled and available
        if (client->config.include_context && strlen(event->details_json) > 0) {
            // Parse JSON context using json-c library
            json_object* context_obj = json_tokener_parse(event->details_json);
            if (context_obj) {
                json_object* latency_obj;
                if (json_object_object_get_ex(context_obj, "latency", &latency_obj)) {
                    slack_field_t* latency_field = &attachment->fields[attachment->field_count++];
                    strncpy(latency_field->title, "Latency", sizeof(latency_field->title) - 1);
                    snprintf(latency_field->value, sizeof(latency_field->value), "%.2f ms", 
                            json_object_get_double(latency_obj));
                    latency_field->short_field = true;
                }
                
                json_object* error_obj;
                if (json_object_object_get_ex(context_obj, "error", &error_obj)) {
                    slack_field_t* error_field = &attachment->fields[attachment->field_count++];
                    strncpy(error_field->title, "Error", sizeof(error_field->title) - 1);
                    strncpy(error_field->value, json_object_get_string(error_obj), 
                            sizeof(error_field->value) - 1);
                    error_field->short_field = true;
                }
                
                json_object_put(context_obj);
            }
        }
    } else {
        // Simple text message - truncate long strings to prevent buffer overflow
        char truncated_title[128];
        char truncated_message[256];
        strncpy(truncated_title, event->title, sizeof(truncated_title) - 1);
        truncated_title[sizeof(truncated_title) - 1] = '\0';
        strncpy(truncated_message, event->message, sizeof(truncated_message) - 1);
        truncated_message[sizeof(truncated_message) - 1] = '\0';
        
        snprintf(message->text, sizeof(message->text),
                 "*%s*\n%s\n\n*Priority:* %s\n*Type:* %s",
                 truncated_title, truncated_message,
                 slack_client_get_priority_text(event->priority),
                 notification_type_to_string(event->type));
    }
}

// Create JSON payload for Slack webhook
static char* create_slack_json(slack_message_t* message) {
    if (!message) return NULL;
    
    // Allocate buffer for JSON
    size_t buffer_size = 4096; // Use configurable value
    char* json = malloc(buffer_size);
    if (!json) return NULL;
    
    if (message->attachment_count > 0) {
        // Create JSON with attachments
        slack_attachment_t* attachment = &message->attachments[0];
        
        snprintf(json, buffer_size,
                 "{"
                 "\"username\":\"%s\","
                 "\"icon_emoji\":\"%s\","
                 "\"icon_url\":\"%s\","
                 "\"channel\":\"%s\","
                 "\"attachments\":[{"
                 "\"color\":\"%s\","
                 "\"title\":\"%s\","
                 "\"text\":\"%s\","
                 "\"footer\":\"%s\","
                 "\"ts\":%lld,"
                 "\"fields\":[",
                 message->username,
                 message->icon_emoji,
                 message->icon_url,
                 message->channel,
                 attachment->color,
                 attachment->title,
                 attachment->text,
                 attachment->footer,
                 attachment->timestamp);
        
        // Add fields
        for (int i = 0; i < attachment->field_count; i++) {
            slack_field_t* field = &attachment->fields[i];
            char field_json[512];
            snprintf(field_json, sizeof(field_json),
                     "%s{\"title\":\"%s\",\"value\":\"%s\",\"short\":%s}",
                     i > 0 ? "," : "",
                     field->title,
                     field->value,
                     field->short_field ? "true" : "false");
            strncat(json, field_json, buffer_size - strlen(json) - 1);
        }
        
        strncat(json, "]}]}", buffer_size - strlen(json) - 1);
    } else {
        // Simple text message
        snprintf(json, buffer_size,
                 "{"
                 "\"username\":\"%s\","
                 "\"icon_emoji\":\"%s\","
                 "\"icon_url\":\"%s\","
                 "\"channel\":\"%s\","
                 "\"text\":\"%s\""
                 "}",
                 message->username,
                 message->icon_emoji,
                 message->icon_url,
                 message->channel,
                 message->text);
    }
    
    return json;
}

// Send HTTP request to Slack webhook
static int send_slack_request(slack_client_t* client, slack_message_t* message) {
    if (!client || !message) {
        return -1;
    }
    
    // Create JSON payload
    char* json_payload = create_slack_json(message);
    if (!json_payload) {
        strncpy(client->status.last_error, "Failed to create JSON payload", sizeof(client->status.last_error) - 1);
        return -1;
    }
    
    // Create HTTP request
    http_request_t* request = http_request_create(client->config.webhook_url, HTTP_METHOD_POST);
    if (!request) {
        free(json_payload);
        strncpy(client->status.last_error, "Failed to create HTTP request", sizeof(client->status.last_error) - 1);
        return -1;
    }
    
    // Set JSON body
    if (http_request_set_json_body(request, json_payload) != 0) {
        http_request_free(request);
        free(json_payload);
        strncpy(client->status.last_error, "Failed to set JSON body", sizeof(client->status.last_error) - 1);
        return -1;
    }
    
    // Set headers
    http_request_add_header(request, "User-Agent: autonomy/1.0.0");
    
    // Set timeout
    if (client->config.timeout_seconds > 0) {
        request->request_timeout_ms = client->config.timeout_seconds * 1000;
    } else {
        request->request_timeout_ms = 30000; // Default 30 seconds
    }
    
    // Perform request
    http_response_t* response = http_request(request);
    
    // Clean up request
    http_request_free(request);
    free(json_payload);
    
    if (!response) {
        strncpy(client->status.last_error, "HTTP request failed", sizeof(client->status.last_error) - 1);
        client->status.last_error_time = time(NULL);
        return -1;
    }
    
    // Store response code
    client->status.last_response_code = (int)response->status_code;
    
    // Check result
    if (!http_response_is_success(response)) {
        // Truncate error message to fit in buffer
        char truncated_error[128];
        strncpy(truncated_error, response->error_message, sizeof(truncated_error) - 1);
        truncated_error[sizeof(truncated_error) - 1] = '\0';
        snprintf(client->status.last_error, sizeof(client->status.last_error), 
                "HTTP error: %ld - %s", response->status_code, truncated_error);
        client->status.last_error_time = time(NULL);
        http_response_free(response);
        return -1;
    }
    
    // Clean up response
    http_response_free(response);
    return 0;
}

// Send notification via Slack with retry logic
int slack_client_send(slack_client_t* client, const notification_event_t* event) {
    if (!client || !event || !client->config.enabled) {
        return -1;
    }
    
    if (strlen(client->config.webhook_url) == 0) {
        strncpy(client->status.last_error, "Slack webhook URL is required", sizeof(client->status.last_error) - 1);
        client->status.last_error_time = time(NULL);
        client->status.total_failed++;
        return -1;
    }
    
    // Create message
    slack_message_t message;
    slack_client_create_message(client, event, &message);
    
    // Retry logic
    int max_attempts = client->config.retry_attempts > 0 ? client->config.retry_attempts : 3;
    int retry_delay = client->config.retry_delay_seconds > 0 ? client->config.retry_delay_seconds : 5;
    
    int result = -1;
    for (int attempt = 1; attempt <= max_attempts; attempt++) {
        result = send_slack_request(client, &message);
        
        if (result == 0) {
            // Success
            client->status.total_sent++;
            client->status.last_sent_time = time(NULL);
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

// Get slack client status
void slack_client_get_status(slack_client_t* client, slack_client_status_t* status) {
    if (!client || !status) return;
    
    *status = client->status;
}
