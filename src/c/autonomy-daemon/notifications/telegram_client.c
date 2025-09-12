#include "telegram_client.h"
#include "../shared/utils/http_client_libcurl.h"
#include "../core/types.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Telegram client now uses HTTP client library

// Initialize telegram client
int telegram_client_init(telegram_client_t* client, const telegram_config_t* config) {
    if (!client || !config) {
        return -1;
    }
    
    memset(client, 0, sizeof(telegram_client_t));
    
    // Copy configuration
    client->config = *config;
    
    // Initialize status
    client->status.enabled = config->enabled;
    safe_strncpy(client->status.chat_id, config->chat_id, sizeof(client->status.chat_id));
    client->status.total_sent = 0;
    client->status.total_failed = 0;
    client->status.last_response_code = 0;
    client->status.last_sent_time = 0;
    client->status.last_error_time = 0;
    client->status.last_error[0] = '\0';
    
    return 0;
}

// Clean up telegram client
void telegram_client_cleanup(telegram_client_t* client) {
    if (!client) return;
    
    // Clear sensitive data
    memset(client->config.token, 0, sizeof(client->config.token));
    memset(client, 0, sizeof(telegram_client_t));
}

// Get priority emoji for notification priority
const char* telegram_client_get_priority_emoji(notification_priority_t priority) {
    switch (priority) {
        case NOTIFICATION_PRIORITY_EMERGENCY:
            return "";
        case NOTIFICATION_PRIORITY_HIGH:
            return "";
        case NOTIFICATION_PRIORITY_NORMAL:
            return "";
        case NOTIFICATION_PRIORITY_LOW:
            return "";
        case NOTIFICATION_PRIORITY_LOWEST:
            return "";
        default:
            return "";
    }
}

// Get priority text for display
const char* telegram_client_get_priority_text(notification_priority_t priority) {
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

// Escape markdown special characters
void telegram_client_escape_markdown(const char* input, char* output, size_t max_size) {
    if (!input || !output || max_size == 0) return;
    
    const char* src = input;
    char* dst = output;
    size_t remaining = max_size - 1;
    
    while (*src && remaining > 1) {
        switch (*src) {
            case '*':
            case '_':
            case '`':
            case '[':
            case ']':
            case '(':
            case ')':
                if (remaining > 1) {
                    *dst++ = '\\';
                    *dst++ = *src;
                    remaining -= 2;
                }
                break;
            default:
                *dst++ = *src;
                remaining--;
                break;
        }
        src++;
    }
    *dst = '\0';
}

// Create telegram message from notification event
void telegram_client_create_message(telegram_client_t* client, const notification_event_t* event, 
                                   telegram_message_t* message) {
    if (!client || !event || !message) return;
    
    memset(message, 0, sizeof(telegram_message_t));
    
    // Set chat ID
    safe_strncpy(message->chat_id, client->config.chat_id, sizeof(message->chat_id));
    
    // Set parse mode
    if (client->config.use_markdown) {
        safe_strncpy(message->parse_mode, "Markdown", sizeof(message->parse_mode));
    }
    
    // Format message
    char escaped_title[512];
    char escaped_message[2048];
    
    if (client->config.use_markdown) {
        telegram_client_escape_markdown(event->title, escaped_title, sizeof(escaped_title));
        telegram_client_escape_markdown(event->message, escaped_message, sizeof(escaped_message));
    } else {
        safe_strncpy(escaped_title, event->title, sizeof(escaped_title));
        safe_strncpy(escaped_message, event->message, sizeof(escaped_message));
    }
    
    // Get priority emoji and text
    const char* priority_emoji = telegram_client_get_priority_emoji(event->priority);
    const char* priority_text = telegram_client_get_priority_text(event->priority);
    
    // Format timestamp
    char timestamp_str[64];
    struct tm* tm_info = gmtime(&event->timestamp);
    strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%d %H:%M:%S UTC", tm_info);
    
    if (client->config.use_markdown) {
        snprintf(message->text, sizeof(message->text),
                 "%s *%s*\n\n"
                 "%s\n\n"
                 " *Priority:* %s\n"
                 " *Type:* %s\n"
                 " *Time:* %s\n",
                 priority_emoji, escaped_title,
                 escaped_message,
                 priority_text,
                 notification_type_to_string(event->type),
                 timestamp_str);
        
        // Add context if enabled and available
        if (client->config.include_context && strlen(event->details_json) > 0) {
            strncat(message->text, "\n *Details:* See context data\n", sizeof(message->text) - strlen(message->text) - 1);
        }
        
        strncat(message->text, "\n _autonomy Daemon_", sizeof(message->text) - strlen(message->text) - 1);
    } else {
        snprintf(message->text, sizeof(message->text),
                 "%s %s\n\n"
                 "%s\n\n"
                 "Priority: %s\n"
                 "Type: %s\n"
                 "Time: %s\n\n"
                 "autonomy Daemon",
                 priority_emoji, escaped_title,
                 escaped_message,
                 priority_text,
                 notification_type_to_string(event->type),
                 timestamp_str);
    }
}

// Create JSON payload for Telegram API
static char* create_telegram_json(telegram_message_t* message) {
    if (!message) return NULL;
    
    // Allocate buffer for JSON
    size_t buffer_size = 8192; // Use configurable value
    char* json = malloc(buffer_size);
    if (!json) return NULL;
    
    // URL encode the text for JSON
    CURL* curl = curl_easy_init();
    char* encoded_text = NULL;
    if (curl) {
        encoded_text = curl_easy_escape(curl, message->text, 0);
        curl_easy_cleanup(curl);
    }
    
    snprintf(json, buffer_size,
             "{"
             "\"chat_id\":\"%s\","
             "\"text\":\"%s\"",
             message->chat_id,
             encoded_text ? encoded_text : message->text);
    
    if (strlen(message->parse_mode) > 0) {
        strncat(json, ",\"parse_mode\":\"", buffer_size - strlen(json) - 1);
        strncat(json, message->parse_mode, buffer_size - strlen(json) - 1);
        strncat(json, "\"", buffer_size - strlen(json) - 1);
    }
    
    strncat(json, "}", buffer_size - strlen(json) - 1);
    
    if (encoded_text) {
        curl_free(encoded_text);
    }
    
    return json;
}

// Send HTTP request to Telegram Bot API
static int send_telegram_request(telegram_client_t* client, telegram_message_t* message) {
    if (!client || !message) {
        return -1;
    }
    
    // Create API URL
    char api_url[512];
    snprintf(api_url, sizeof(api_url), "https://api.telegram.org/bot%s/sendMessage", client->config.token);
    
    // Create JSON payload
    char* json_payload = create_telegram_json(message);
    if (!json_payload) {
        safe_strncpy(client->status.last_error, "Failed to create JSON payload", sizeof(client->status.last_error));
        return -1;
    }
    
    // Create HTTP request
    http_request_t* request = http_request_create(api_url, HTTP_METHOD_POST);
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
    
    // Check HTTP result
    if (!http_response_is_success(response)) {
        snprintf(client->status.last_error, sizeof(client->status.last_error), 
                "HTTP error: %ld - %.100s", response->status_code, response->error_message);
        client->status.last_error_time = time(NULL);
        http_response_free(response);
        return -1;
    }
    
    // Parse response to check for Telegram API errors
    if (response->body) {
        if (strstr(response->body, "\"ok\":true")) {
            // Success
            http_response_free(response);
            return 0;
        } else if (strstr(response->body, "\"ok\":false")) {
            // Extract error description if possible
            char* desc_start = strstr(response->body, "\"description\":\"");
            if (desc_start) {
                desc_start += 15; // Skip "description":"
                char* desc_end = strchr(desc_start, '"');
                if (desc_end) {
                    size_t desc_len = desc_end - desc_start;
                    if (desc_len < sizeof(client->status.last_error) - 20) {
                        safe_strncpy(client->status.last_error, "Telegram API: ", sizeof(client->status.last_error));
                        strncat(client->status.last_error, desc_start, desc_len);
                    } else {
                        safe_strncpy(client->status.last_error, "Telegram API error", sizeof(client->status.last_error));
                    }
                } else {
                    safe_strncpy(client->status.last_error, "Telegram API error", sizeof(client->status.last_error));
                }
            } else {
                safe_strncpy(client->status.last_error, "Telegram API error", sizeof(client->status.last_error));
            }
            client->status.last_error_time = time(NULL);
            http_response_free(response);
            return -1;
        }
    }
    
    // Clean up response
    http_response_free(response);
    
    return 0;
}

// Send notification via Telegram with retry logic
int telegram_client_send(telegram_client_t* client, const notification_event_t* event) {
    if (!client || !event || !client->config.enabled) {
        return -1;
    }
    
    if (strlen(client->config.token) == 0 || strlen(client->config.chat_id) == 0) {
        safe_strncpy(client->status.last_error, "Telegram token and chat ID are required", sizeof(client->status.last_error));
        client->status.last_error_time = time(NULL);
        client->status.total_failed++;
        return -1;
    }
    
    // Create message
    telegram_message_t message;
    telegram_client_create_message(client, event, &message);
    
    // Retry logic
    int max_attempts = client->config.retry_attempts > 0 ? client->config.retry_attempts : 3;
    int retry_delay = client->config.retry_delay_seconds > 0 ? client->config.retry_delay_seconds : 5;
    
    int result = -1;
    for (int attempt = 1; attempt <= max_attempts; attempt++) {
        result = send_telegram_request(client, &message);
        
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

// Get telegram client status
void telegram_client_get_status(telegram_client_t* client, telegram_client_status_t* status) {
    if (!client || !status) return;
    
    *status = client->status;
}
