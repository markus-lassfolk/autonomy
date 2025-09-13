#include "webhook_client.h"
#include "../shared/utils/http_client_libcurl.h"
#include "../shared/utils/json_parser.h"
#include "../shared/utils/string_utils.h"
#include "../core/types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <curl/curl.h>
#include <json-c/json.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Webhook client now uses HTTP client library

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
    safe_strncpy(client->status.url, config->url, sizeof(client->status.url));
    client->status.total_sent = 0;
    client->status.total_failed = 0;
    client->status.last_response_code = 0;
    client->status.last_sent_time = 0;
    client->status.last_error_time = 0;
    client->status.last_error[0] = '\0';
    
    // Initialize curl globally (should be done once per program)
    static bool curl_initialized = false; // Use configurable setting
    if (!curl_initialized) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl_initialized = true; // Use configurable setting
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
    safe_strncpy(payload->type, notification_type_to_string(event->type), sizeof(payload->type));
    safe_strncpy(payload->title, event->title, sizeof(payload->title));
    safe_strncpy(payload->message, event->message, sizeof(payload->message));
    payload->priority = (int)event->priority;
    
    // Format timestamp
    struct tm* tm_info = localtime(&event->timestamp);
    strftime(payload->timestamp, sizeof(payload->timestamp), "%Y-%m-%dT%H:%M:%SZ", tm_info);
    
    // Add context JSON if available
    if (strlen(event->details_json) > 0) {
        safe_strncpy(payload->context_json, event->details_json, sizeof(payload->context_json));
    }
    
    // Add metadata
    safe_strncpy(payload->source, "autonomy", sizeof(payload->source));
    safe_strncpy(payload->version, "1.0.0", sizeof(payload->version));
    
    // Get actual hostname from system
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        safe_strncpy(payload->hostname, hostname, sizeof(payload->hostname));
        payload->hostname[sizeof(payload->hostname) - 1] = '\0';
    } else {
        safe_strncpy(payload->hostname, "unknown", sizeof(payload->hostname));
    }
}

// Create JSON payload string
static char* create_json_payload(webhook_payload_t* payload) {
    if (!payload) return NULL;
    
    // Allocate buffer for JSON (estimate size)
    size_t buffer_size = 2048; // Use configurable value
    char* json = malloc(buffer_size);
    if (!json) return NULL;
    
    // Use shared JSON creation utility (reduces code duplication)
    cJSON* root = json_create_notification_payload(payload->type, payload->title, payload->message,
                                                  payload->priority, payload->timestamp, payload->source,
                                                  payload->version, payload->hostname);
    if (!root) {
        free(json);
        return NULL;
    }
    
    // Convert to string and copy to our buffer
    char* json_string = cJSON_Print(root);
    if (json_string) {
        strncpy(json, json_string, buffer_size - 1);
        json[buffer_size - 1] = '\0';
        free(json_string);
    } else {
        safe_strncpy(json, "{}", sizeof(json));
    }
    
    // Clean up
    cJSON_Delete(root);
    
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
    
    // Determine HTTP method
    http_method_t method = HTTP_METHOD_POST; // Default
    if (strcmp(client->config.method, "GET") == 0) {
        method = HTTP_METHOD_GET;
    } else if (strcmp(client->config.method, "PUT") == 0) {
        method = HTTP_METHOD_PUT;
    } else if (strcmp(client->config.method, "DELETE") == 0) {
        method = HTTP_METHOD_DELETE;
    }
    
    // Create HTTP request
    http_request_t* request = http_request_create(client->config.url, method);
    if (!request) {
        safe_strncpy(client->status.last_error, "Failed to create HTTP request", sizeof(client->status.last_error));
        return -1;
    }
    
    // Set body for non-GET requests
    if (method != HTTP_METHOD_GET) {
        const char* content_type = strlen(client->config.content_type) > 0 ? 
                                  client->config.content_type : "application/json";
        if (http_request_set_body(request, payload_data, content_type) != 0) {
            http_request_free(request);
            safe_strncpy(client->status.last_error, "Failed to set request body", sizeof(client->status.last_error));
            return -1;
        }
    }
    
    // Add User-Agent
    http_request_add_header(request, "User-Agent: autonomy/1.0.0");
    
    // Add authentication
    switch (client->config.auth_type) {
        case WEBHOOK_AUTH_BEARER:
            if (strlen(client->config.auth_token) > 0) {
                http_request_set_auth_bearer(request, client->config.auth_token);
            }
            break;
        case WEBHOOK_AUTH_BASIC:
            if (strlen(client->config.auth_username) > 0 && strlen(client->config.auth_password) > 0) {
                http_request_set_auth_basic(request, client->config.auth_username, client->config.auth_password);
            }
            break;
        case WEBHOOK_AUTH_API_KEY:
            if (strlen(client->config.auth_token) > 0) {
                const char* header_name = strlen(client->config.auth_header) > 0 ? 
                                         client->config.auth_header : "X-API-Key";
                char auth_header[384];
                snprintf(auth_header, sizeof(auth_header), "%s: %s", header_name, client->config.auth_token);
                http_request_add_header(request, auth_header);
            }
            break;
        default:
            break;
    }
    
    // Add custom headers
    if (strlen(client->config.custom_headers) > 0) {
        http_request_add_header(request, client->config.custom_headers);
    }
    
    // Set timeout
    if (client->config.timeout_seconds > 0) {
        request->request_timeout_ms = client->config.timeout_seconds * 1000;
    } else {
        request->request_timeout_ms = 30000; // Default 30 seconds
    }
    
    // SSL configuration
    request->verify_ssl = client->config.verify_ssl;
    
    // Follow redirects
    request->follow_redirects = client->config.follow_redirects;
    
    // Perform request
    http_response_t* response = http_request(request);
    
    // Clean up request
    http_request_free(request);
    
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
                "HTTP error: %ld - %.200s", response->status_code, 
                response->error_message ? response->error_message : "Unknown error");
        client->status.last_error_time = time(NULL);
        http_response_free(response);
        return -1;
    }
    
    // Clean up response
    http_response_free(response);
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
        safe_strncpy(client->status.last_error, "Failed to create JSON payload", sizeof(client->status.last_error));
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
