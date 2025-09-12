#include "discord_client.h"
#include "../shared/utils/http_client_libcurl.h"
#include "../core/types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Discord client now uses HTTP client library

// Initialize discord client
int discord_client_init(discord_client_t* client, const discord_config_t* config) {
    if (!client || !config) {
        return -1;
    }
    
    memset(client, 0, sizeof(discord_client_t));
    
    // Copy configuration
    client->config = *config;
    
    // Initialize status
    client->status.enabled = config->enabled;
    safe_strncpy(client->status.webhook_url, config->webhook_url, sizeof(client->status.webhook_url));
    client->status.total_sent = 0;
    client->status.total_failed = 0;
    client->status.last_response_code = 0;
    client->status.last_sent_time = 0;
    client->status.last_error_time = 0;
    client->status.last_error[0] = '\0';
    
    return 0;
}

// Clean up discord client
void discord_client_cleanup(discord_client_t* client) {
    if (!client) return;
    
    // Clear sensitive data
    memset(client, 0, sizeof(discord_client_t));
}

// Get embed color for notification priority
int discord_client_get_embed_color(notification_priority_t priority) {
    switch (priority) {
        case NOTIFICATION_PRIORITY_EMERGENCY:
            return 0xFF0000; // Red
        case NOTIFICATION_PRIORITY_HIGH:
            return 0xFF8C00; // Orange
        case NOTIFICATION_PRIORITY_NORMAL:
            return 0x007BFF; // Blue
        case NOTIFICATION_PRIORITY_LOW:
            return 0x28A745; // Green
        case NOTIFICATION_PRIORITY_LOWEST:
            return 0x6C757D; // Gray
        default:
            return 0x007BFF; // Blue
    }
}

// Get priority text for display
const char* discord_client_get_priority_text(notification_priority_t priority) {
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

// Create discord message from notification event
void discord_client_create_message(discord_client_t* client, const notification_event_t* event, 
                                  discord_message_t* message) {
    if (!client || !event || !message) return;
    
    memset(message, 0, sizeof(discord_message_t));
    
    // Set username and avatar
    if (strlen(client->config.username) > 0) {
        safe_strncpy(message->username, client->config.username, sizeof(message->username));
    } else {
        safe_strncpy(message->username, "autonomy", sizeof(message->username));
    }
    
    if (strlen(client->config.avatar_url) > 0) {
        safe_strncpy(message->avatar_url, client->config.avatar_url, sizeof(message->avatar_url));
    }
    
    if (client->config.use_embeds) {
        // Create rich embed
        discord_embed_t* embed = &message->embeds[0];
        message->embed_count = 1;
        
        safe_strncpy(embed->title, event->title, sizeof(embed->title));
        safe_strncpy(embed->description, event->message, sizeof(embed->description));
        embed->color = discord_client_get_embed_color(event->priority);
        
        // Format timestamp
        struct tm* tm_info = gmtime(&event->timestamp);
        strftime(embed->timestamp, sizeof(embed->timestamp), "%Y-%m-%dT%H:%M:%SZ", tm_info);
        
        // Add footer
        safe_strncpy(embed->footer_text, "autonomy Daemon", sizeof(embed->footer_text));
        
        // Add priority field
        discord_embed_field_t* priority_field = &embed->fields[embed->field_count++];
        safe_strncpy(priority_field->name, "Priority", sizeof(priority_field->name));
        safe_strncpy(priority_field->value, discord_client_get_priority_text(event->priority), sizeof(priority_field->value));
        priority_field->inline_field = true;
        
        // Add type field
        discord_embed_field_t* type_field = &embed->fields[embed->field_count++];
        safe_strncpy(type_field->name, "Type", sizeof(type_field->name));
        safe_strncpy(type_field->value, notification_type_to_string(event->type), sizeof(type_field->value));
        type_field->inline_field = true;
        
        // Add context fields if enabled and available
        if (client->config.include_context && strlen(event->details_json) > 0) {
            // Parse simple JSON context (simplified parsing)
            if (strstr(event->details_json, "latency")) {
                discord_embed_field_t* latency_field = &embed->fields[embed->field_count++];
                safe_strncpy(latency_field->name, "Latency", sizeof(latency_field->name));
                safe_strncpy(latency_field->value, "See details", sizeof(latency_field->value));
                latency_field->inline_field = true;
            }
        }
    } else {
        // Simple content message
        snprintf(message->content, sizeof(message->content),
                 "**%.100s**\n%.200s\n\n**Priority:** %s\n**Type:** %s",
                 event->title, event->message,
                 discord_client_get_priority_text(event->priority),
                 notification_type_to_string(event->type));
    }
}

// Create JSON payload for Discord webhook
static char* create_discord_json(discord_message_t* message) {
    if (!message) return NULL;
    
    // Allocate buffer for JSON
    size_t buffer_size = 4096; // Use configurable value
    char* json = malloc(buffer_size);
    if (!json) return NULL;
    
    if (message->embed_count > 0) {
        // Create JSON with embeds
        discord_embed_t* embed = &message->embeds[0];
        
        snprintf(json, buffer_size,
                 "{"
                 "\"username\":\"%s\","
                 "\"avatar_url\":\"%s\","
                 "\"embeds\":[{"
                 "\"title\":\"%s\","
                 "\"description\":\"%s\","
                 "\"color\":%d,"
                 "\"timestamp\":\"%s\","
                 "\"footer\":{\"text\":\"%s\"},"
                 "\"fields\":[",
                 message->username,
                 message->avatar_url,
                 embed->title,
                 embed->description,
                 embed->color,
                 embed->timestamp,
                 embed->footer_text);
        
        // Add fields
        for (int i = 0; i < embed->field_count; i++) {
            discord_embed_field_t* field = &embed->fields[i];
            char field_json[512];
            snprintf(field_json, sizeof(field_json),
                     "%s{\"name\":\"%s\",\"value\":\"%s\",\"inline\":%s}",
                     i > 0 ? "," : "",
                     field->name,
                     field->value,
                     field->inline_field ? "true" : "false");
            strncat(json, field_json, buffer_size - strlen(json) - 1);
        }
        
        strncat(json, "]}]}", buffer_size - strlen(json) - 1);
    } else {
        // Simple content message
        snprintf(json, buffer_size,
                 "{"
                 "\"username\":\"%s\","
                 "\"avatar_url\":\"%s\","
                 "\"content\":\"%s\""
                 "}",
                 message->username,
                 message->avatar_url,
                 message->content);
    }
    
    return json;
}

// Send HTTP request to Discord webhook
static int send_discord_request(discord_client_t* client, discord_message_t* message) {
    if (!client || !message) {
        return -1;
    }
    
    // Create JSON payload
    char* json_payload = create_discord_json(message);
    if (!json_payload) {
        safe_strncpy(client->status.last_error, "Failed to create JSON payload", sizeof(client->status.last_error));
        return -1;
    }
    
    // Create HTTP request
    http_request_t* request = http_request_create(client->config.webhook_url, HTTP_METHOD_POST);
    if (!request) {
        free(json_payload);
        safe_strncpy(client->status.last_error, "Failed to create HTTP request", sizeof(client->status.last_error));
        return -1;
    }
    
    // Set JSON body
    if (http_request_set_json_body(request, json_payload) != 0) {
        http_request_free(request);
        free(json_payload);
        safe_strncpy(client->status.last_error, "Failed to set JSON body", sizeof(client->status.last_error));
        return -1;
    }
    
    // Add headers
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
        safe_strncpy(client->status.last_error, "HTTP request failed", sizeof(client->status.last_error));
        client->status.last_error_time = time(NULL);
        return -1;
    }
    
    // Store response code
    client->status.last_response_code = (int)response->status_code;
    
    // Check result
    if (!http_response_is_success(response)) {
        snprintf(client->status.last_error, sizeof(client->status.last_error), 
                "HTTP error: %ld - %.100s", response->status_code, response->error_message);
        client->status.last_error_time = time(NULL);
        http_response_free(response);
        return -1;
    }
    
    // Clean up response
    http_response_free(response);
    
    return 0;
}

// Send notification via Discord with retry logic
int discord_client_send(discord_client_t* client, const notification_event_t* event) {
    if (!client || !event || !client->config.enabled) {
        return -1;
    }
    
    if (strlen(client->config.webhook_url) == 0) {
        safe_strncpy(client->status.last_error, "Discord webhook URL is required", sizeof(client->status.last_error));
        client->status.last_error_time = time(NULL);
        client->status.total_failed++;
        return -1;
    }
    
    // Create message
    discord_message_t message;
    discord_client_create_message(client, event, &message);
    
    // Retry logic
    int max_attempts = client->config.retry_attempts > 0 ? client->config.retry_attempts : 3;
    int retry_delay = client->config.retry_delay_seconds > 0 ? client->config.retry_delay_seconds : 5;
    
    int result = -1;
    for (int attempt = 1; attempt <= max_attempts; attempt++) {
        result = send_discord_request(client, &message);
        
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

// Get discord client status
void discord_client_get_status(discord_client_t* client, discord_client_status_t* status) {
    if (!client || !status) return;
    
    *status = client->status;
}
