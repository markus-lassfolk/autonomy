#include "discord_client.h"
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

// Initialize discord client
static int discord_client_init(discord_client_t* client, const discord_config_t* config) {
    if (!client || !config) {
        return -1;
    }
    
    memset(client, 0, sizeof(discord_client_t));
    
    // Copy configuration
    client->config = *config;
    
    // Initialize status
    client->status.enabled = config->enabled;
    strncpy(client->status.webhook_url, config->webhook_url, sizeof(client->status.webhook_url) - 1);
    client->status.webhook_url[sizeof(client->status.webhook_url) - 1] = '\0';
    client->status.total_sent = 0;
    client->status.total_failed = 0;
    client->status.last_response_code = 0;
    client->status.last_sent_time = 0;
    client->status.last_error_time = 0;
    client->status.last_error[0] = '\0';
    
    return 0;
}

// Clean up discord client
static void discord_client_cleanup(discord_client_t* client) {
    if (!client) return;
    
    // Clear sensitive data
    memset(client, 0, sizeof(discord_client_t));
}

// Get embed color for notification priority
static int discord_client_get_embed_color(notification_priority_t priority) {
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

// Create discord message from notification event
void discord_client_create_message(discord_client_t* client, const notification_event_t* event, 
                                  discord_message_t* message) {
    if (!client || !event || !message) return;
    
    memset(message, 0, sizeof(discord_message_t));
    
    // Set username and avatar
    if (strlen(client->config.username) > 0) {
        strncpy(message->username, client->config.username, sizeof(message->username) - 1);
        message->username[sizeof(message->username) - 1] = '\0';
    } else {
        strncpy(message->username, "autonomy", sizeof(message->username) - 1);
        message->username[sizeof(message->username) - 1] = '\0';
    }
    
    if (strlen(client->config.avatar_url) > 0) {
        strncpy(message->avatar_url, client->config.avatar_url, sizeof(message->avatar_url) - 1);
        message->avatar_url[sizeof(message->avatar_url) - 1] = '\0';
    }
    
    if (client->config.use_embeds) {
        // Create rich embed
        discord_embed_t* embed = &message->embeds[0];
        message->embed_count = 1;
        
        strncpy(embed->title, event->title, sizeof(embed->title) - 1);
        embed->title[sizeof(embed->title) - 1] = '\0';
        strncpy(embed->description, event->message, sizeof(embed->description) - 1);
        embed->description[sizeof(embed->description) - 1] = '\0';
        embed->color = discord_client_get_embed_color(event->priority);
        
        // Format timestamp
        struct tm* tm_info = gmtime(&event->timestamp);
        strftime(embed->timestamp, sizeof(embed->timestamp), "%Y-%m-%dT%H:%M:%SZ", tm_info);
        
        // Add footer
        strncpy(embed->footer_text, "autonomy Daemon", sizeof(embed->footer_text) - 1);
        embed->footer_text[sizeof(embed->footer_text) - 1] = '\0';
        
        // Add priority field
        discord_embed_field_t* priority_field = &embed->fields[embed->field_count++];
        strncpy(priority_field->name, "Priority", sizeof(priority_field->name) - 1);
        priority_field->name[sizeof(priority_field->name) - 1] = '\0';
        strncpy(priority_field->value, discord_client_get_priority_text(event->priority), sizeof(priority_field->value) - 1);
        priority_field->value[sizeof(priority_field->value) - 1] = '\0';
        priority_field->inline_field = true;
        
        // Add type field
        discord_embed_field_t* type_field = &embed->fields[embed->field_count++];
        strncpy(type_field->name, "Type", sizeof(type_field->name) - 1);
        type_field->name[sizeof(type_field->name) - 1] = '\0';
        strncpy(type_field->value, notification_type_to_string(event->type), sizeof(type_field->value) - 1);
        type_field->value[sizeof(type_field->value) - 1] = '\0';
        type_field->inline_field = true;
        
        // Add context fields if enabled and available
        if (client->config.include_context && strlen(event->details_json) > 0) {
            // Parse simple JSON context (simplified parsing)
            if (strstr(event->details_json, "latency")) {
                discord_embed_field_t* latency_field = &embed->fields[embed->field_count++];
                strncpy(latency_field->name, "Latency", sizeof(latency_field->name) - 1);
                latency_field->name[sizeof(latency_field->name) - 1] = '\0';
                strncpy(latency_field->value, "See details", sizeof(latency_field->value) - 1);
                latency_field->value[sizeof(latency_field->value) - 1] = '\0';
                latency_field->inline_field = true;
            }
        }
    } else {
        // Simple content message
        snprintf(message->content, sizeof(message->content),
                 "**%s**\n%s\n\n**Priority:** %s\n**Type:** %s",
                 event->title, event->message,
                 discord_client_get_priority_text(event->priority),
                 notification_type_to_string(event->type));
    }
}

// Create JSON payload for Discord webhook
static char* create_discord_json(discord_message_t* message) {
    if (!message) return NULL;
    
    // Allocate buffer for JSON
    size_t buffer_size = 4096;
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
    char* json_payload = create_discord_json(message);
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
    
    if (response_code < 200 || response_code >= 300) {
        snprintf(client->status.last_error, sizeof(client->status.last_error), 
                "HTTP error: %ld", response_code);
        client->status.last_error_time = time(NULL);
        return -1;
    }
    
    return 0;
}

// Send notification via Discord with retry logic
static int discord_client_send(discord_client_t* client, const notification_event_t* event) {
    if (!client || !event || !client->config.enabled) {
        return -1;
    }
    
    if (strlen(client->config.webhook_url) == 0) {
        strncpy(client->status.last_error, "Discord webhook URL is required", sizeof(client->status.last_error) - 1);
        client->status.last_error[sizeof(client->status.last_error) - 1] = '\0';
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
static void discord_client_get_status(discord_client_t* client, discord_client_status_t* status) {
    if (!client || !status) return;
    
    *status = client->status;
}
