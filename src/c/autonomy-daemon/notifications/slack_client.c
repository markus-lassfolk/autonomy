#include "slack_client.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <curl/curl.h>
#include <unistd.h>

// Response data structure for curl
typedef struct {
    char* data;
    size_t size;
} curl_response_t;

// Callback function for curl to write response data
static size_t curl_write_callback(void* contents, size_t size, size_t nmemb, curl_response_t* response) {
    size_t total_size = size * nmemb;
    char* new_data = realloc(response->data, response->size + total_size + 1);
    
    if (!new_data) {
        return 0; // Out of memory
    }
    
    response->data = new_data;
    memcpy(&(response->data[response->size]), contents, total_size);
    response->size += total_size;
    response->data[response->size] = '\0';
    
    return total_size;
}

// Initialize slack client
static int slack_client_init(slack_client_t* client, const slack_config_t* config) {
    if (!client || !config) {
        return -1;
    }
    
    memset(client, 0, sizeof(slack_client_t));
    
    // Copy configuration
    client->config = *config;
    
    // Initialize status
    client->status.enabled = config->enabled;
    strncpy(client->status.webhook_url, config->webhook_url, sizeof(client->status.webhook_url) - 1);
    client->status.webhook_url[sizeof(client->status.webhook_url) - 1] = '\0';
    strncpy(client->status.channel, config->channel, sizeof(client->status.channel) - 1);
    client->status.channel[sizeof(client->status.channel) - 1] = '\0';
    client->status.total_sent = 0;
    client->status.total_failed = 0;
    client->status.last_response_code = 0;
    client->status.last_sent_time = 0;
    client->status.last_error_time = 0;
    client->status.last_error[0] = '\0';
    
    return 0;
}

// Clean up slack client
static void slack_client_cleanup(slack_client_t* client) {
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
            return "🚨 Emergency";
        case NOTIFICATION_PRIORITY_HIGH:
            return "⚠️ High";
        case NOTIFICATION_PRIORITY_NORMAL:
            return "ℹ️ Normal";
        case NOTIFICATION_PRIORITY_LOW:
            return "📝 Low";
        case NOTIFICATION_PRIORITY_LOWEST:
            return "💬 Lowest";
        default:
            return "ℹ️ Normal";
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
        message->username[sizeof(message->username) - 1] = '\0';
    } else {
        strncpy(message->username, "autonomy", sizeof(message->username) - 1);
        message->username[sizeof(message->username) - 1] = '\0';
    }
    
    if (strlen(client->config.icon_emoji) > 0) {
        strncpy(message->icon_emoji, client->config.icon_emoji, sizeof(message->icon_emoji) - 1);
        message->icon_emoji[sizeof(message->icon_emoji) - 1] = '\0';
    } else if (strlen(client->config.icon_url) > 0) {
        strncpy(message->icon_url, client->config.icon_url, sizeof(message->icon_url) - 1);
        message->icon_url[sizeof(message->icon_url) - 1] = '\0';
    } else {
        strncpy(message->icon_emoji, ":satellite:", sizeof(message->icon_emoji) - 1);
        message->icon_emoji[sizeof(message->icon_emoji) - 1] = '\0';
    }
    
    // Set channel
    if (strlen(client->config.channel) > 0) {
        strncpy(message->channel, client->config.channel, sizeof(message->channel) - 1);
        message->channel[sizeof(message->channel) - 1] = '\0';
    }
    
    if (client->config.use_attachments) {
        // Create rich attachment
        slack_attachment_t* attachment = &message->attachments[0];
        message->attachment_count = 1;
        
        strncpy(attachment->color, slack_client_get_attachment_color(event->priority), sizeof(attachment->color) - 1);
        attachment->color[sizeof(attachment->color) - 1] = '\0';
        strncpy(attachment->title, event->title, sizeof(attachment->title) - 1);
        attachment->title[sizeof(attachment->title) - 1] = '\0';
        strncpy(attachment->text, event->message, sizeof(attachment->text) - 1);
        attachment->text[sizeof(attachment->text) - 1] = '\0';
        strncpy(attachment->footer, "autonomy Daemon", sizeof(attachment->footer) - 1);
        attachment->footer[sizeof(attachment->footer) - 1] = '\0';
        attachment->timestamp = event->timestamp;
        
        // Add priority field
        slack_field_t* priority_field = &attachment->fields[attachment->field_count++];
        strncpy(priority_field->title, "Priority", sizeof(priority_field->title) - 1);
        priority_field->title[sizeof(priority_field->title) - 1] = '\0';
        strncpy(priority_field->value, slack_client_get_priority_text(event->priority), sizeof(priority_field->value) - 1);
        priority_field->value[sizeof(priority_field->value) - 1] = '\0';
        priority_field->short_field = true;
        
        // Add type field
        slack_field_t* type_field = &attachment->fields[attachment->field_count++];
        strncpy(type_field->title, "Type", sizeof(type_field->title) - 1);
        type_field->title[sizeof(type_field->title) - 1] = '\0';
        strncpy(type_field->value, notification_type_to_string(event->type), sizeof(type_field->value) - 1);
        type_field->value[sizeof(type_field->value) - 1] = '\0';
        type_field->short_field = true;
        
        // Add context fields if enabled and available
        if (client->config.include_context && strlen(event->details_json) > 0) {
            // Parse simple JSON context (simplified parsing)
            if (strstr(event->details_json, "latency")) {
                slack_field_t* latency_field = &attachment->fields[attachment->field_count++];
                strncpy(latency_field->title, "Details", sizeof(latency_field->title) - 1);
                latency_field->title[sizeof(latency_field->title) - 1] = '\0';
                strncpy(latency_field->value, "See context", sizeof(latency_field->value) - 1);
                latency_field->value[sizeof(latency_field->value) - 1] = '\0';
                latency_field->short_field = true;
            }
        }
    } else {
        // Simple text message
        snprintf(message->text, sizeof(message->text),
                 "*%s*\n%s\n\n*Priority:* %s\n*Type:* %s",
                 event->title, event->message,
                 slack_client_get_priority_text(event->priority),
                 notification_type_to_string(event->type));
    }
}

// Create JSON payload for Slack webhook
static char* create_slack_json(slack_message_t* message) {
    if (!message) return NULL;
    
    // Allocate buffer for JSON
    size_t buffer_size = 4096;
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
                 "\"ts\":%ld,"
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
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        strncpy(client->status.last_error, "Failed to initialize curl", sizeof(client->status.last_error) - 1);
        client->status.last_error[sizeof(client->status.last_error) - 1] = '\0';
        return -1;
    }
    
    curl_response_t response = {0};
    CURLcode res;
    long response_code = 0;
    
    // Create JSON payload
    char* json_payload = create_slack_json(message);
    if (!json_payload) {
        curl_easy_cleanup(curl);
        strncpy(client->status.last_error, "Failed to create JSON payload", sizeof(client->status.last_error) - 1);
        client->status.last_error[sizeof(client->status.last_error) - 1] = '\0';
        return -1;
    }
    
    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, client->config.webhook_url);
    
    // Set POST data
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
    
    // Set headers
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "User-Agent: autonomy/1.0.0");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Set timeout
    if (client->config.timeout_seconds > 0) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, client->config.timeout_seconds);
    } else {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // Default 30 seconds
    }
    
    // Set response callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    // Perform request
    res = curl_easy_perform(curl);
    
    // Get response code
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    client->status.last_response_code = (int)response_code;
    
    // Clean up
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(json_payload);
    
    if (response.data) {
        free(response.data);
    }
    
    // Check result
    if (res != CURLE_OK) {
        snprintf(client->status.last_error, sizeof(client->status.last_error), 
                "curl error: %s", curl_easy_strerror(res));
        client->status.last_error_time = time(NULL);
        return -1;
    }
    
    if (response_code != 200) {
        snprintf(client->status.last_error, sizeof(client->status.last_error), 
                "HTTP error: %ld", response_code);
        client->status.last_error_time = time(NULL);
        return -1;
    }
    
    return 0;
}

// Send notification via Slack with retry logic
static int slack_client_send(slack_client_t* client, const notification_event_t* event) {
    if (!client || !event || !client->config.enabled) {
        return -1;
    }
    
    if (strlen(client->config.webhook_url) == 0) {
        strncpy(client->status.last_error, "Slack webhook URL is required", sizeof(client->status.last_error) - 1);
        client->status.last_error[sizeof(client->status.last_error) - 1] = '\0';
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
static void slack_client_get_status(slack_client_t* client, slack_client_status_t* status) {
    if (!client || !status) return;
    
    *status = client->status;
}
