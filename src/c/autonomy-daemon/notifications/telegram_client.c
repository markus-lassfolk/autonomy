#include "telegram_client.h"
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
    strncpy(client->status.chat_id, config->chat_id, sizeof(client->status.chat_id) - 1);
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
            return "🚨";
        case NOTIFICATION_PRIORITY_HIGH:
            return "⚠️";
        case NOTIFICATION_PRIORITY_NORMAL:
            return "ℹ️";
        case NOTIFICATION_PRIORITY_LOW:
            return "📝";
        case NOTIFICATION_PRIORITY_LOWEST:
            return "💬";
        default:
            return "ℹ️";
    }
}

// Get priority text for display
const char* telegram_client_get_priority_text(notification_priority_t priority) {
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
    strncpy(message->chat_id, client->config.chat_id, sizeof(message->chat_id) - 1);
    
    // Set parse mode
    if (client->config.use_markdown) {
        strncpy(message->parse_mode, "Markdown", sizeof(message->parse_mode) - 1);
    }
    
    // Format message
    char escaped_title[512];
    char escaped_message[2048];
    
    if (client->config.use_markdown) {
        telegram_client_escape_markdown(event->title, escaped_title, sizeof(escaped_title));
        telegram_client_escape_markdown(event->message, escaped_message, sizeof(escaped_message));
    } else {
        strncpy(escaped_title, event->title, sizeof(escaped_title) - 1);
        strncpy(escaped_message, event->message, sizeof(escaped_message) - 1);
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
                 "🏷️ *Priority:* %s\n"
                 "📋 *Type:* %s\n"
                 "⏰ *Time:* %s\n",
                 priority_emoji, escaped_title,
                 escaped_message,
                 priority_text,
                 notification_type_to_string(event->type),
                 timestamp_str);
        
        // Add context if enabled and available
        if (client->config.include_context && strlen(event->details_json) > 0) {
            strncat(message->text, "\n📊 *Details:* See context data\n", sizeof(message->text) - strlen(message->text) - 1);
        }
        
        strncat(message->text, "\n🛰️ _autonomy Daemon_", sizeof(message->text) - strlen(message->text) - 1);
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
    size_t buffer_size = 8192;
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
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        strncpy(client->status.last_error, "Failed to initialize curl", sizeof(client->status.last_error) - 1);
        return -1;
    }
    
    curl_response_t response = {0};
    CURLcode res;
    long response_code = 0;
    
    // Create API URL
    char api_url[512];
    snprintf(api_url, sizeof(api_url), "https://api.telegram.org/bot%s/sendMessage", client->config.token);
    
    // Create JSON payload
    char* json_payload = create_telegram_json(message);
    if (!json_payload) {
        curl_easy_cleanup(curl);
        strncpy(client->status.last_error, "Failed to create JSON payload", sizeof(client->status.last_error) - 1);
        return -1;
    }
    
    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, api_url);
    
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
    
    // Check result
    if (res != CURLE_OK) {
        snprintf(client->status.last_error, sizeof(client->status.last_error), 
                "curl error: %s", curl_easy_strerror(res));
        client->status.last_error_time = time(NULL);
        if (response.data) free(response.data);
        return -1;
    }
    
    if (response_code != 200) {
        snprintf(client->status.last_error, sizeof(client->status.last_error), 
                "HTTP error: %ld", response_code);
        client->status.last_error_time = time(NULL);
        if (response.data) free(response.data);
        return -1;
    }
    
    // Parse response to check for Telegram API errors
    if (response.data) {
        if (strstr(response.data, "\"ok\":true")) {
            // Success
            free(response.data);
            return 0;
        } else if (strstr(response.data, "\"ok\":false")) {
            // Extract error description if possible
            char* desc_start = strstr(response.data, "\"description\":\"");
            if (desc_start) {
                desc_start += 15; // Skip "description":"
                char* desc_end = strchr(desc_start, '"');
                if (desc_end) {
                    size_t desc_len = desc_end - desc_start;
                    if (desc_len < sizeof(client->status.last_error) - 20) {
                        strncpy(client->status.last_error, "Telegram API: ", sizeof(client->status.last_error) - 1);
                        strncat(client->status.last_error, desc_start, desc_len);
                    } else {
                        strncpy(client->status.last_error, "Telegram API error", sizeof(client->status.last_error) - 1);
                    }
                } else {
                    strncpy(client->status.last_error, "Telegram API error", sizeof(client->status.last_error) - 1);
                }
            } else {
                strncpy(client->status.last_error, "Telegram API error", sizeof(client->status.last_error) - 1);
            }
            client->status.last_error_time = time(NULL);
            free(response.data);
            return -1;
        }
        free(response.data);
    }
    
    return 0;
}

// Send notification via Telegram with retry logic
int telegram_client_send(telegram_client_t* client, const notification_event_t* event) {
    if (!client || !event || !client->config.enabled) {
        return -1;
    }
    
    if (strlen(client->config.token) == 0 || strlen(client->config.chat_id) == 0) {
        strncpy(client->status.last_error, "Telegram token and chat ID are required", sizeof(client->status.last_error) - 1);
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
