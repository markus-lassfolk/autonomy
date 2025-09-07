#include "pushover_client.h"
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

// URL encode string
static char* url_encode(const char* str) {
    if (!str) return NULL;
    
    CURL* curl = curl_easy_init();
    if (!curl) return NULL;
    
    char* encoded = curl_easy_escape(curl, str, 0);
    if (!encoded) {
        curl_easy_cleanup(curl);
        return NULL;
    }
    
    // Copy to our own buffer
    size_t len = strlen(encoded);
    char* result = malloc(len + 1);
    if (result) {
        strcpy(result, encoded);
    }
    
    curl_free(encoded);
    curl_easy_cleanup(curl);
    
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
    strncpy(client->status.token, config->token, sizeof(client->status.token) - 1);
    strncpy(client->status.user, config->user, sizeof(client->status.user) - 1);
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
    strncpy(message->token, client->config.token, sizeof(message->token) - 1);
    strncpy(message->user, client->config.user, sizeof(message->user) - 1);
    strncpy(message->message, event->message, sizeof(message->message) - 1);
    strncpy(message->title, event->title, sizeof(message->title) - 1);
    
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
        strncpy(message->sound, client->config.default_sound, sizeof(message->sound) - 1);
    } else {
        strncpy(message->sound, pushover_client_get_priority_sound(event->priority), sizeof(message->sound) - 1);
    }
    
    // Set device if configured
    if (strlen(client->config.device) > 0) {
        strncpy(message->device, client->config.device, sizeof(message->device) - 1);
    }
    
    message->timestamp = event->timestamp;
    message->html_enabled = true;
    
    // Add dashboard URL if available in context
    if (strstr(event->details_json, "dashboard_url")) {
        strncpy(message->url, "http://router.local", sizeof(message->url) - 1);
        strncpy(message->url_title, "View Dashboard", sizeof(message->url_title) - 1);
    }
}

// Create form data for Pushover API
static char* create_form_data(pushover_message_t* message) {
    if (!message) return NULL;
    
    // Allocate buffer for form data
    size_t buffer_size = 4096;
    char* form_data = malloc(buffer_size);
    if (!form_data) return NULL;
    
    // URL encode fields
    char* encoded_message = url_encode(message->message);
    char* encoded_title = url_encode(message->title);
    
    // Create form data
    snprintf(form_data, buffer_size,
             "token=%s&user=%s&message=%s&title=%s&priority=%d&sound=%s&html=1&timestamp=%ld",
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
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        strncpy(client->status.last_error, "Failed to initialize curl", sizeof(client->status.last_error) - 1);
        return -1;
    }
    
    curl_response_t response = {0};
    CURLcode res;
    long response_code = 0;
    
    // Create form data
    char* form_data = create_form_data(message);
    if (!form_data) {
        curl_easy_cleanup(curl);
        strncpy(client->status.last_error, "Failed to create form data", sizeof(client->status.last_error) - 1);
        return -1;
    }
    
    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.pushover.net/1/messages.json");
    
    // Set POST data
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, form_data);
    
    // Set content type
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, 
                     curl_slist_append(NULL, "Content-Type: application/x-www-form-urlencoded"));
    
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
    curl_easy_cleanup(curl);
    free(form_data);
    
    // Check result
    if (res != CURLE_OK) {
        snprintf(client->status.last_error, sizeof(client->status.last_error), 
                "curl error: %s", curl_easy_strerror(res));
        client->status.last_error_time = time(NULL);
        if (response.data) free(response.data);
        return -1;
    }
    
    // Check response code
    if (response_code != 200) {
        snprintf(client->status.last_error, sizeof(client->status.last_error), 
                "HTTP error: %ld", response_code);
        client->status.last_error_time = time(NULL);
        if (response.data) free(response.data);
        return -1;
    }
    
    // Parse response (simplified - in production would parse JSON)
    if (response.data) {
        if (strstr(response.data, "\"status\":1")) {
            // Success
            if (response.data) free(response.data);
            return 0;
        } else {
            // Extract error message if possible
            char* error_start = strstr(response.data, "\"errors\":");
            if (error_start) {
                strncpy(client->status.last_error, "Pushover API error", sizeof(client->status.last_error) - 1);
            } else {
                strncpy(client->status.last_error, "Pushover API returned error", sizeof(client->status.last_error) - 1);
            }
            client->status.last_error_time = time(NULL);
            free(response.data);
            return -1;
        }
    }
    
    return 0;
}

// Send notification via Pushover with retry logic
int pushover_client_send(pushover_client_t* client, const notification_event_t* event) {
    if (!client || !event || !client->config.enabled) {
        return -1;
    }
    
    if (strlen(client->config.token) == 0 || strlen(client->config.user) == 0) {
        strncpy(client->status.last_error, "Pushover token and user are required", sizeof(client->status.last_error) - 1);
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
