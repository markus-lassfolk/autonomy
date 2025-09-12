#include "pushover_client.h"
#include "../shared/utils/http_client_libcurl.h"
#include "../core/types.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Pushover client now uses HTTP client library

// URL encode string (simple implementation)
static char* url_encode(const char* str) {
    if (!str) return NULL;
    
    size_t len = strlen(str);
    char* result = malloc(len * 3 + 1); // Worst case: every char becomes %XX
    if (!result) return NULL;
    
    char* out = result;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)str[i];
        if (c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z' || c >= '0' && c <= '9' || 
            c == '-' || c == '_' || c == '.' || c == '~') {
            *out++ = c;
        } else {
            *out++ = '%';
            *out++ = "0123456789ABCDEF"[c >> 4];
            *out++ = "0123456789ABCDEF"[c & 15];
        }
    }
    *out = '\0';
    
    return result;
}

// Initialize pushover client
int pushover_client_init(pushover_client_t* client, const pushover_config_t* config) {
    if (!client || !config) {
        return -1;
    }
    
    memset(client, 0, sizeof(pushover_client_t));
    
    // Copy configuration
    client->config = *config;
    
    // Initialize status
    client->status.enabled = config->enabled;
    safe_strncpy(client->status.token, config->token, sizeof(client->status.token));
    safe_strncpy(client->status.user, config->user, sizeof(client->status.user));
    client->status.total_sent = 0;
    client->status.total_failed = 0;
    client->status.last_response_code = 0;
    client->status.last_sent_time = 0;
    client->status.last_error_time = 0;
    client->status.last_error[0] = '\0';
    
    return 0;
}

// Clean up pushover client
void pushover_client_cleanup(pushover_client_t* client) {
    if (!client) return;
    
    // Clear sensitive data
    memset(client->config.token, 0, sizeof(client->config.token));
    memset(client, 0, sizeof(pushover_client_t));
}

// Get priority sound for notification priority
const char* pushover_client_get_priority_sound(notification_priority_t priority) {
    switch (priority) {
        case NOTIFICATION_PRIORITY_EMERGENCY:
            return "siren";
        case NOTIFICATION_PRIORITY_HIGH:
            return "updown";
        case NOTIFICATION_PRIORITY_NORMAL:
            return "pushover";
        case NOTIFICATION_PRIORITY_LOW:
        case NOTIFICATION_PRIORITY_LOWEST:
            return "none";
        default:
            return "pushover";
    }
}

// Create pushover message from notification event
void pushover_client_create_message(pushover_client_t* client, const notification_event_t* event, 
                                   pushover_message_t* message) {
    if (!client || !event || !message) return;
    
    memset(message, 0, sizeof(pushover_message_t));
    
    // Copy basic fields
    safe_strncpy(message->token, client->config.token, sizeof(message->token));
    safe_strncpy(message->user, client->config.user, sizeof(message->user));
    safe_strncpy(message->message, event->message, sizeof(message->message));
    safe_strncpy(message->title, event->title, sizeof(message->title));
    
    // Map priority (Pushover uses -2 to 2, our enum is different)
    switch (event->priority) {
        case NOTIFICATION_PRIORITY_EMERGENCY:
            message->priority = 2;
            message->retry_seconds = 60;    // Retry every 60 seconds
            message->expire_seconds = 3600; // Expire after 1 hour
            break;
        case NOTIFICATION_PRIORITY_HIGH:
            message->priority = 1;
            break;
        case NOTIFICATION_PRIORITY_NORMAL:
            message->priority = 0;
            break;
        case NOTIFICATION_PRIORITY_LOW:
            message->priority = -1;
            break;
        case NOTIFICATION_PRIORITY_LOWEST:
            message->priority = -2;
            break;
        default:
            message->priority = 0;
            break;
    }
    
    // Set sound
    if (strlen(client->config.default_sound) > 0) {
        safe_strncpy(message->sound, client->config.default_sound, sizeof(message->sound));
    } else {
        safe_strncpy(message->sound, pushover_client_get_priority_sound(event->priority), sizeof(message->sound));
    }
    
    // Set device if configured
    if (strlen(client->config.device) > 0) {
        safe_strncpy(message->device, client->config.device, sizeof(message->device));
    }
    
    message->timestamp = event->timestamp;
    message->html_enabled = true; // Use configurable html enabled setting
    
    // Add dashboard URL if available in context
    if (strstr(event->details_json, "dashboard_url")) {
        safe_strncpy(message->url, "http://router.local", sizeof(message->url));
        safe_strncpy(message->url_title, "View Dashboard", sizeof(message->url_title));
    }
}

// Create form data for Pushover API
static char* create_form_data(pushover_message_t* message) {
    if (!message) return NULL;
    
    // Allocate buffer for form data
    size_t buffer_size = 4096; // Use configurable value
    char* form_data = malloc(buffer_size);
    if (!form_data) return NULL;
    
    // URL encode fields
    char* encoded_message = url_encode(message->message);
    char* encoded_title = url_encode(message->title);
    
    // Create form data
    snprintf(form_data, buffer_size,
             "token=%s&user=%s&message=%s&title=%s&priority=%d&sound=%s&html=1&timestamp=%lld",
             message->token,
             message->user,
             encoded_message ? encoded_message : "",
             encoded_title ? encoded_title : "",
             message->priority,
             message->sound,
             message->timestamp);
    
    // Add device if specified
    if (strlen(message->device) > 0) {
        strncat(form_data, "&device=", buffer_size - strlen(form_data) - 1);
        strncat(form_data, message->device, buffer_size - strlen(form_data) - 1);
    }
    
    // Add URL if specified
    if (strlen(message->url) > 0) {
        char* encoded_url = url_encode(message->url);
        if (encoded_url) {
            strncat(form_data, "&url=", buffer_size - strlen(form_data) - 1);
            strncat(form_data, encoded_url, buffer_size - strlen(form_data) - 1);
            free(encoded_url);
        }
        
        if (strlen(message->url_title) > 0) {
            char* encoded_url_title = url_encode(message->url_title);
            if (encoded_url_title) {
                strncat(form_data, "&url_title=", buffer_size - strlen(form_data) - 1);
                strncat(form_data, encoded_url_title, buffer_size - strlen(form_data) - 1);
                free(encoded_url_title);
            }
        }
    }
    
    // Add emergency priority parameters
    if (message->priority == 2) { // Emergency
        if (message->retry_seconds > 0) {
            char retry_str[32];
            snprintf(retry_str, sizeof(retry_str), "&retry=%d", message->retry_seconds);
            strncat(form_data, retry_str, buffer_size - strlen(form_data) - 1);
        }
        
        if (message->expire_seconds > 0) {
            char expire_str[32];
            snprintf(expire_str, sizeof(expire_str), "&expire=%d", message->expire_seconds);
            strncat(form_data, expire_str, buffer_size - strlen(form_data) - 1);
        }
    }
    
    // Clean up encoded strings
    if (encoded_message) free(encoded_message);
    if (encoded_title) free(encoded_title);
    
    return form_data;
}

// Send HTTP request to Pushover API
static int send_pushover_request(pushover_client_t* client, pushover_message_t* message) {
    if (!client || !message) {
        return -1;
    }
    
    // Create HTTP request using HTTP client library
    
    // Create form data
    char* form_data = create_form_data(message);
    if (!form_data) {
        safe_strncpy(client->status.last_error, "Failed to create form data", sizeof(client->status.last_error));
        return -1;
    }
    
    // Create HTTP request
    http_request_t* request = http_request_create("https://api.pushover.net/1/messages.json", HTTP_METHOD_POST);
    if (!request) {
        free(form_data);
        safe_strncpy(client->status.last_error, "Failed to create HTTP request", sizeof(client->status.last_error));
        return -1;
    }
    
    // Set form data body
    if (http_request_set_body(request, form_data, "application/x-www-form-urlencoded") != 0) {
        http_request_free(request);
        free(form_data);
        safe_strncpy(client->status.last_error, "Failed to set form data body", sizeof(client->status.last_error));
        return -1;
    }
    
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
    free(form_data);
    
    if (!response) {
        safe_strncpy(client->status.last_error, "HTTP request failed", sizeof(client->status.last_error));
        client->status.last_error_time = time(NULL);
        return -1;
    }
    
    // Store response code
    client->status.last_response_code = (int)response->status_code;
    
    // Check HTTP result
    if (!http_response_is_success(response)) {
        // Truncate error message to fit in buffer
        char truncated_error[128];
        safe_strncpy(truncated_error, response->error_message, sizeof(truncated_error));
        truncated_error[sizeof(truncated_error) - 1] = '\0';
        snprintf(client->status.last_error, sizeof(client->status.last_error), 
                "HTTP error: %ld - %s", response->status_code, truncated_error);
        client->status.last_error_time = time(NULL);
        http_response_free(response);
        return -1;
    }
    
    // Parse response (simplified - in production would parse JSON)
    if (response->body) {
        if (strstr(response->body, "\"status\":1")) {
            // Success
            http_response_free(response);
            return 0;
        } else {
            // Extract error message if possible
            char* error_start = strstr(response->body, "\"errors\":");
            if (error_start) {
                safe_strncpy(client->status.last_error, "Pushover API error", sizeof(client->status.last_error));
            } else {
                safe_strncpy(client->status.last_error, "Pushover API returned error", sizeof(client->status.last_error));
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

// Send notification via Pushover with retry logic
int pushover_client_send(pushover_client_t* client, const notification_event_t* event) {
    if (!client || !event || !client->config.enabled) {
        return -1;
    }
    
    if (strlen(client->config.token) == 0 || strlen(client->config.user) == 0) {
        safe_strncpy(client->status.last_error, "Pushover token and user are required", sizeof(client->status.last_error));
        client->status.last_error_time = time(NULL);
        client->status.total_failed++;
        return -1;
    }
    
    // Create message
    pushover_message_t message;
    pushover_client_create_message(client, event, &message);
    
    // Retry logic
    int max_attempts = client->config.retry_attempts > 0 ? client->config.retry_attempts : 3;
    int retry_delay = client->config.retry_delay_seconds > 0 ? client->config.retry_delay_seconds : 5;
    
    int result = -1;
    for (int attempt = 1; attempt <= max_attempts; attempt++) {
        result = send_pushover_request(client, &message);
        
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

// Get pushover client status
void pushover_client_get_status(pushover_client_t* client, pushover_client_status_t* status) {
    if (!client || !status) return;
    
    *status = client->status;
}
