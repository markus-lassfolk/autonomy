#include "webhook_client.h"
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

// Initialize webhook client
int webhook_client_init(webhook_client_t* client, const webhook_config_t* config) {
    if (!client || !config) {
        return -1;
    }
    
    memset(client, 0, sizeof(webhook_client_t));
    
    // Copy configuration
    client->config = *config;
    
    // Initialize status
    client->status.enabled = config->enabled;
    strncpy(client->status.url, config->url, sizeof(client->status.url) - 1);
    client->status.total_sent = 0;
    client->status.total_failed = 0;
    client->status.last_response_code = 0;
    client->status.last_sent_time = 0;
    client->status.last_error_time = 0;
    client->status.last_error[0] = '\0';
    
    // Initialize curl globally (should be done once per program)
    static bool curl_initialized = false;
    if (!curl_initialized) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl_initialized = true;
    }
    
    return 0;
}

// Clean up webhook client
void webhook_client_cleanup(webhook_client_t* client) {
    if (!client) return;
    
    // Clear sensitive data
    memset(&client->config, 0, sizeof(webhook_config_t));
    memset(&client->status, 0, sizeof(webhook_client_status_t));
}

// Check if notification should be sent based on filters
bool webhook_client_should_send(webhook_client_t* client, const notification_event_t* event) {
    if (!client || !event || !client->config.enabled) {
        return false;
    }
    
    // Check priority filter
    if (client->config.priority_filter_count > 0) {
        bool priority_allowed = false;
        for (int i = 0; i < client->config.priority_filter_count; i++) {
            if (client->config.priority_filter[i] == event->priority) {
                priority_allowed = true;
                break;
            }
        }
        if (!priority_allowed) {
            return false;
        }
    }
    
    // Check type filter
    if (client->config.type_filter_count > 0) {
        bool type_allowed = false;
        for (int i = 0; i < client->config.type_filter_count; i++) {
            if (client->config.type_filter[i] == event->type) {
                type_allowed = true;
                break;
            }
        }
        if (!type_allowed) {
            return false;
        }
    }
    
    return true;
}

// Create webhook payload from notification event
void webhook_client_create_payload(webhook_client_t* client, const notification_event_t* event, 
                                  webhook_payload_t* payload) {
    if (!client || !event || !payload) return;
    
    memset(payload, 0, sizeof(webhook_payload_t));
    
    // Fill payload fields
    strncpy(payload->type, notification_type_to_string(event->type), sizeof(payload->type) - 1);
    strncpy(payload->title, event->title, sizeof(payload->title) - 1);
    strncpy(payload->message, event->message, sizeof(payload->message) - 1);
    payload->priority = (int)event->priority;
    
    // Format timestamp
    struct tm* tm_info = localtime(&event->timestamp);
    strftime(payload->timestamp, sizeof(payload->timestamp), "%Y-%m-%dT%H:%M:%SZ", tm_info);
    
    // Add context JSON if available
    if (strlen(event->details_json) > 0) {
        strncpy(payload->context_json, event->details_json, sizeof(payload->context_json) - 1);
    }
    
    // Add metadata
    strncpy(payload->source, "autonomy", sizeof(payload->source) - 1);
    strncpy(payload->version, "1.0.0", sizeof(payload->version) - 1);
    
    // Add hostname (simplified - could get from system)
    strncpy(payload->hostname, "router", sizeof(payload->hostname) - 1);
}

// Create JSON payload string
static char* create_json_payload(webhook_payload_t* payload) {
    if (!payload) return NULL;
    
    // Allocate buffer for JSON (estimate size)
    size_t buffer_size = 2048;
    char* json = malloc(buffer_size);
    if (!json) return NULL;
    
    // Create JSON manually (simplified - in production would use JSON library)
    snprintf(json, buffer_size,
             "{"
             "\"type\":\"%s\","
             "\"title\":\"%s\","
             "\"message\":\"%s\","
             "\"priority\":%d,"
             "\"timestamp\":\"%s\","
             "\"source\":\"%s\","
             "\"version\":\"%s\","
             "\"hostname\":\"%s\"",
             payload->type,
             payload->title,
             payload->message,
             payload->priority,
             payload->timestamp,
             payload->source,
             payload->version,
             payload->hostname);
    
    // Add context if available
    if (strlen(payload->context_json) > 0) {
        strncat(json, ",\"context\":", buffer_size - strlen(json) - 1);
        strncat(json, payload->context_json, buffer_size - strlen(json) - 1);
    }
    
    strncat(json, "}", buffer_size - strlen(json) - 1);
    
    return json;
}

// Send HTTP request with curl
static int send_webhook_request(webhook_client_t* client, const char* payload_data) {
    if (!client || !payload_data) {
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
    
    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, client->config.url);
    
    // Set HTTP method
    if (strcmp(client->config.method, "GET") == 0) {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    } else if (strcmp(client->config.method, "PUT") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_data);
    } else {
        // Default to POST
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_data);
    }
    
    // Set content type
    struct curl_slist* headers = NULL;
    char content_type_header[128];
    snprintf(content_type_header, sizeof(content_type_header), "Content-Type: %s", 
             strlen(client->config.content_type) > 0 ? client->config.content_type : "application/json");
    headers = curl_slist_append(headers, content_type_header);
    
    // Add User-Agent
    headers = curl_slist_append(headers, "User-Agent: autonomy/1.0.0");
    
    // Add authentication
    switch (client->config.auth_type) {
        case WEBHOOK_AUTH_BEARER:
            if (strlen(client->config.auth_token) > 0) {
                char auth_header[384];
                snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", client->config.auth_token);
                headers = curl_slist_append(headers, auth_header);
            }
            break;
        case WEBHOOK_AUTH_BASIC:
            if (strlen(client->config.auth_username) > 0 && strlen(client->config.auth_password) > 0) {
                curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
                curl_easy_setopt(curl, CURLOPT_USERNAME, client->config.auth_username);
                curl_easy_setopt(curl, CURLOPT_PASSWORD, client->config.auth_password);
            }
            break;
        case WEBHOOK_AUTH_API_KEY:
            if (strlen(client->config.auth_token) > 0) {
                char auth_header[384];
                const char* header_name = strlen(client->config.auth_header) > 0 ? 
                                         client->config.auth_header : "X-API-Key";
                snprintf(auth_header, sizeof(auth_header), "%s: %s", header_name, client->config.auth_token);
                headers = curl_slist_append(headers, auth_header);
            }
            break;
        default:
            break;
    }
    
    // Add custom headers (simplified parsing)
    if (strlen(client->config.custom_headers) > 0) {
        // In a real implementation, would properly parse custom headers
        headers = curl_slist_append(headers, client->config.custom_headers);
    }
    
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Set timeout
    if (client->config.timeout_seconds > 0) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, client->config.timeout_seconds);
    } else {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // Default 30 seconds
    }
    
    // SSL configuration
    if (!client->config.verify_ssl) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    
    // Follow redirects
    if (client->config.follow_redirects) {
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
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

// Send notification via webhook with retry logic
int webhook_client_send(webhook_client_t* client, const notification_event_t* event) {
    if (!client || !event) {
        return -1;
    }
    
    if (!webhook_client_should_send(client, event)) {
        return 0; // Filtered out
    }
    
    // Create payload
    webhook_payload_t payload;
    webhook_client_create_payload(client, event, &payload);
    
    // Create JSON payload
    char* json_payload = create_json_payload(&payload);
    if (!json_payload) {
        strncpy(client->status.last_error, "Failed to create JSON payload", sizeof(client->status.last_error) - 1);
        client->status.last_error_time = time(NULL);
        client->status.total_failed++;
        return -1;
    }
    
    // Retry logic
    int max_attempts = client->config.retry_attempts > 0 ? client->config.retry_attempts : 3;
    int retry_delay = client->config.retry_delay_seconds > 0 ? client->config.retry_delay_seconds : 5;
    
    int result = -1;
    for (int attempt = 1; attempt <= max_attempts; attempt++) {
        result = send_webhook_request(client, json_payload);
        
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
    
    free(json_payload);
    
    if (result != 0) {
        client->status.total_failed++;
    }
    
    return result;
}

// Get webhook client status
void webhook_client_get_status(webhook_client_t* client, webhook_client_status_t* status) {
    if (!client || !status) return;
    
    *status = client->status;
}
