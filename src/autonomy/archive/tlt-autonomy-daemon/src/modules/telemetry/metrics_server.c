#include "metrics_server.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Global metrics server instance
static metrics_server_t g_metrics_server;
static bool g_metrics_server_initialized = false;

// HTTP request handler
static enum MHD_Result handle_request(void* cls, struct MHD_Connection* connection,
                                     const char* url, const char* method,
                                     const char* version, const char* upload_data,
                                     size_t* upload_data_size, void** con_cls);

// Handle GET /metrics endpoint
static enum MHD_Result handle_metrics_endpoint(struct MHD_Connection* connection);

// Handle GET /health endpoint
static enum MHD_Result handle_health_endpoint(struct MHD_Connection* connection);

// Handle GET /status endpoint
static enum MHD_Result handle_status_endpoint(struct MHD_Connection* connection);

// Send JSON response
static enum MHD_Result send_json_response(struct MHD_Connection* connection, 
                                         const char* json_data, int status_code);

// Check authentication
static bool check_authentication(struct MHD_Connection* connection, const char* required_token);

// Initialize metrics server
int metrics_server_init(const metrics_server_config_t* config) {
    if (g_metrics_server_initialized) {
        return 0; // Already initialized
    }
    
    if (!config) {
        return -1;
    }
    
    memset(&g_metrics_server, 0, sizeof(metrics_server_t));
    
    // Copy configuration
    g_metrics_server.config = *config;
    
    // Initialize mutex
    g_metrics_server.mutex = malloc(sizeof(pthread_mutex_t));
    if (!g_metrics_server.mutex) {
        return -1;
    }
    
    pthread_mutex_init(g_metrics_server.mutex, NULL);
    
    // Initialize status
    g_metrics_server.status.running = false;
    g_metrics_server.status.port = config->port;
    strncpy(g_metrics_server.status.bind_address, config->bind_address, sizeof(g_metrics_server.status.bind_address) - 1);
    g_metrics_server.status.total_requests = 0;
    g_metrics_server.status.successful_requests = 0;
    g_metrics_server.status.failed_requests = 0;
    g_metrics_server.status.last_request_time = 0;
    g_metrics_server.status.start_time = 0;
    
    g_metrics_server_initialized = true;
    return 0;
}

// Clean up metrics server
void metrics_server_cleanup(void) {
    if (!g_metrics_server_initialized) return;
    
    // Stop server if running
    if (g_metrics_server.status.running) {
        metrics_server_stop();
    }
    
    if (g_metrics_server.mutex) {
        pthread_mutex_destroy(g_metrics_server.mutex);
        free(g_metrics_server.mutex);
    }
    
    g_metrics_server.daemon = NULL;
    g_metrics_server.mutex = NULL;
    
    g_metrics_server_initialized = false;
}

// Start metrics server
int metrics_server_start(void) {
    if (!g_metrics_server_initialized || !g_metrics_server.config.enabled) {
        return -1;
    }
    
    if (g_metrics_server.status.running) {
        return 0; // Already running
    }
    
    // Start HTTP daemon
    g_metrics_server.daemon = MHD_start_daemon(
        MHD_USE_INTERNAL_POLLING_THREAD,
        g_metrics_server.config.port,
        NULL, NULL, // Accept all connections
        &handle_request, &g_metrics_server,
        MHD_OPTION_CONNECTION_LIMIT, g_metrics_server.config.max_connections,
        MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int)g_metrics_server.config.timeout_seconds,
        MHD_OPTION_END
    );
    
    if (!g_metrics_server.daemon) {
        return -1;
    }
    
    pthread_mutex_lock(g_metrics_server.mutex);
    g_metrics_server.status.running = true;
    g_metrics_server.status.start_time = time(NULL);
    pthread_mutex_unlock(g_metrics_server.mutex);
    
    printf("METRICS_SERVER: Started on %s:%d\n", g_metrics_server.config.bind_address, g_metrics_server.config.port);
    return 0;
}

// Stop metrics server
int metrics_server_stop(void) {
    if (!g_metrics_server_initialized || !g_metrics_server.status.running) {
        return -1;
    }
    
    if (g_metrics_server.daemon) {
        MHD_stop_daemon(g_metrics_server.daemon);
        g_metrics_server.daemon = NULL;
    }
    
    pthread_mutex_lock(g_metrics_server.mutex);
    g_metrics_server.status.running = false;
    pthread_mutex_unlock(g_metrics_server.mutex);
    
    printf("METRICS_SERVER: Stopped\n");
    return 0;
}

// HTTP request handler
static enum MHD_Result handle_request(void* cls, struct MHD_Connection* connection,
                                     const char* url, const char* method,
                                     const char* version, const char* upload_data,
                                     size_t* upload_data_size, void** con_cls) {
    (void)version;
    (void)upload_data;
    (void)upload_data_size;
    (void)con_cls;
    
    metrics_server_t* server = (metrics_server_t*)cls;
    
    if (strcmp(method, "GET") != 0) {
        const char* response = "{\"error\":\"Method not allowed\"}";
        return send_json_response(connection, response, MHD_HTTP_METHOD_NOT_ALLOWED);
    }
    
    // Check authentication if required
    if (server->config.require_auth) {
        if (!check_authentication(connection, server->config.auth_token)) {
            const char* response = "{\"error\":\"Authentication required\"}";
            return send_json_response(connection, response, MHD_HTTP_UNAUTHORIZED);
        }
    }
    
    // Update request statistics
    pthread_mutex_lock(server->mutex);
    server->status.total_requests++;
    server->status.last_request_time = time(NULL);
    pthread_mutex_unlock(server->mutex);
    
    // Route requests
    if (strcmp(url, "/metrics") == 0) {
        return handle_metrics_endpoint(connection);
    } else if (strcmp(url, "/health") == 0) {
        return handle_health_endpoint(connection);
    } else if (strcmp(url, "/status") == 0) {
        return handle_status_endpoint(connection);
    } else {
        const char* response = "{\"error\":\"Endpoint not found\"}";
        pthread_mutex_lock(server->mutex);
        g_metrics_server.status.failed_requests++;
        pthread_mutex_unlock(server->mutex);
        return send_json_response(connection, response, MHD_HTTP_NOT_FOUND);
    }
}

// Handle /metrics endpoint
static enum MHD_Result handle_metrics_endpoint(struct MHD_Connection* connection) {
    char json_response[4096];
    
    if (telemetry_store_is_initialized()) {
        telemetry_store_export_json(time(NULL) - 3600, json_response, sizeof(json_response)); // Last hour
    } else {
        snprintf(json_response, sizeof(json_response), 
                 "{\"error\":\"Telemetry store not initialized\",\"timestamp\":%ld}", time(NULL));
    }
    
    pthread_mutex_lock(g_metrics_server.mutex);
    g_metrics_server.status.successful_requests++;
    pthread_mutex_unlock(g_metrics_server.mutex);
    
    return send_json_response(connection, json_response, MHD_HTTP_OK);
}

// Handle /health endpoint
static enum MHD_Result handle_health_endpoint(struct MHD_Connection* connection) {
    char json_response[1024];
    
    bool telemetry_healthy = telemetry_store_is_initialized();
    int memory_usage = telemetry_store_get_memory_usage();
    
    snprintf(json_response, sizeof(json_response),
             "{"
             "\"status\":\"%s\","
             "\"timestamp\":%ld,"
             "\"telemetry_store\":%s,"
             "\"memory_usage_mb\":%d,"
             "\"uptime\":%ld"
             "}",
             telemetry_healthy ? "healthy" : "unhealthy",
             time(NULL),
             telemetry_healthy ? "true" : "false",
             memory_usage,
             time(NULL) - g_metrics_server.status.start_time);
    
    pthread_mutex_lock(g_metrics_server.mutex);
    g_metrics_server.status.successful_requests++;
    pthread_mutex_unlock(g_metrics_server.mutex);
    
    return send_json_response(connection, json_response, MHD_HTTP_OK);
}

// Handle /status endpoint
static enum MHD_Result handle_status_endpoint(struct MHD_Connection* connection) {
    char json_response[1024];
    
    pthread_mutex_lock(g_metrics_server.mutex);
    
    snprintf(json_response, sizeof(json_response),
             "{"
             "\"running\":%s,"
             "\"port\":%d,"
             "\"total_requests\":%d,"
             "\"successful_requests\":%d,"
             "\"failed_requests\":%d,"
             "\"last_request_time\":%ld,"
             "\"start_time\":%ld,"
             "\"uptime\":%ld"
             "}",
             g_metrics_server.status.running ? "true" : "false",
             g_metrics_server.status.port,
             g_metrics_server.status.total_requests,
             g_metrics_server.status.successful_requests,
             g_metrics_server.status.failed_requests,
             g_metrics_server.status.last_request_time,
             g_metrics_server.status.start_time,
             time(NULL) - g_metrics_server.status.start_time);
    
    g_metrics_server.status.successful_requests++;
    
    pthread_mutex_unlock(g_metrics_server.mutex);
    
    return send_json_response(connection, json_response, MHD_HTTP_OK);
}

// Send JSON response
static enum MHD_Result send_json_response(struct MHD_Connection* connection, 
                                         const char* json_data, int status_code) {
    struct MHD_Response* response = MHD_create_response_from_buffer(
        strlen(json_data), (void*)json_data, MHD_RESPMEM_MUST_COPY);
    
    if (!response) {
        return MHD_NO;
    }
    
    // Add headers
    MHD_add_response_header(response, "Content-Type", "application/json");
    
    if (g_metrics_server.config.enable_cors) {
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
        MHD_add_response_header(response, "Access-Control-Allow-Methods", "GET, OPTIONS");
        MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type, Authorization");
    }
    
    enum MHD_Result ret = MHD_queue_response(connection, status_code, response);
    MHD_destroy_response(response);
    
    return ret;
}

// Check authentication
static bool check_authentication(struct MHD_Connection* connection, const char* required_token) {
    if (!required_token || strlen(required_token) == 0) {
        return true; // No auth required
    }
    
    const char* auth_header = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Authorization");
    if (!auth_header) {
        return false;
    }
    
    // Check for Bearer token
    if (strncmp(auth_header, "Bearer ", 7) == 0) {
        const char* token = auth_header + 7;
        return strcmp(token, required_token) == 0;
    }
    
    return false;
}

// Get metrics server status
void metrics_server_get_status(metrics_server_status_t* status) {
    if (!status || !g_metrics_server_initialized) return;
    
    pthread_mutex_lock(g_metrics_server.mutex);
    *status = g_metrics_server.status;
    pthread_mutex_unlock(g_metrics_server.mutex);
}

// Check if metrics server is initialized
bool metrics_server_is_initialized(void) {
    return g_metrics_server_initialized;
}

// Check if metrics server is running
bool metrics_server_is_running(void) {
    return g_metrics_server_initialized && g_metrics_server.status.running;
}

// Get metrics server instance
metrics_server_t* metrics_server_get_instance(void) {
    return g_metrics_server_initialized ? &g_metrics_server : NULL;
}
