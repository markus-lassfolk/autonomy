#include "api_server.h"
#include "logx.h"
#include "types.h"
#include <microhttpd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Global HTTP daemon
static struct MHD_Daemon *g_http_daemon = NULL;
static bool g_api_server_running = false;
static int g_api_port = 8080;
static char g_api_bind_address[16] = "0.0.0.0";
static pthread_mutex_t g_api_mutex = PTHREAD_MUTEX_INITIALIZER;

// API endpoints
typedef enum {
    API_ENDPOINT_ROOT = 0,
    API_ENDPOINT_STATUS,
    API_ENDPOINT_HEALTH,
    API_ENDPOINT_CONFIG,
    API_ENDPOINT_NETWORK,
    API_ENDPOINT_GPS,
    API_ENDPOINT_STARLINK,
    API_ENDPOINT_SYSTEM,
    API_ENDPOINT_NOTIFICATIONS,
    API_ENDPOINT_COUNT
} api_endpoint_t;

// Endpoint paths
static const char* API_ENDPOINT_PATHS[] = {
    "/",
    "/api/v1/status",
    "/api/v1/health",
    "/api/v1/config",
    "/api/v1/network",
    "/api/v1/gps",
    "/api/v1/starlink",
    "/api/v1/system",
    "/api/v1/notifications"
};

// HTTP response structure
typedef struct {
    char *data;
    size_t size;
    int status_code;
    const char *content_type;
} http_response_t;

// Create HTTP response
static http_response_t* create_http_response(const char *data, int status_code, const char *content_type) {
    http_response_t *response = malloc(sizeof(http_response_t));
    if (!response) {
        return NULL;
    }
    
    response->data = data ? strdup(data) : NULL;
    response->size = data ? strlen(data) : 0;
    response->status_code = status_code;
    response->content_type = content_type ? strdup(content_type) : "application/json";
    
    return response;
}

// Free HTTP response
static void free_http_response(http_response_t *response) {
    if (response) {
        if (response->data) free(response->data);
        if (response->content_type) free(response->content_type);
        free(response);
    }
}

// Generate JSON response
static char* generate_json_response(const char *status, const char *message, const char *data) {
    char *json = malloc(1024);
    if (!json) {
        return NULL;
    }
    
    if (data) {
        snprintf(json, 1024, 
                "{\"status\":\"%s\",\"message\":\"%s\",\"data\":%s,\"timestamp\":%ld}",
                status, message, data, time(NULL));
    } else {
        snprintf(json, 1024, 
                "{\"status\":\"%s\",\"message\":\"%s\",\"timestamp\":%ld}",
                status, message, time(NULL));
    }
    
    return json;
}

// Handle root endpoint
static http_response_t* handle_root_endpoint(void) {
    char *json = generate_json_response("success", "Autonomy Daemon API v1.0", 
                                      "{\"version\":\"1.0.0\",\"endpoints\":[\"/api/v1/status\",\"/api/v1/health\",\"/api/v1/config\",\"/api/v1/network\",\"/api/v1/gps\",\"/api/v1/starlink\",\"/api/v1/system\",\"/api/v1/notifications\"]}");
    
    http_response_t *response = create_http_response(json, 200, "application/json");
    free(json);
    return response;
}

// Handle status endpoint
static http_response_t* handle_status_endpoint(void) {
    // This would normally get data from the autonomy state
    char *json = generate_json_response("success", "System status retrieved", 
                                      "{\"daemon\":\"running\",\"uptime\":3600,\"version\":\"5.4.0\"}");
    
    http_response_t *response = create_http_response(json, 200, "application/json");
    free(json);
    return response;
}

// Handle health endpoint
static http_response_t* handle_health_endpoint(void) {
    // This would normally get health data from the system
    char *json = generate_json_response("success", "Health status retrieved", 
                                      "{\"overall\":\"healthy\",\"score\":95,\"components\":{\"network\":\"healthy\",\"gps\":\"healthy\",\"starlink\":\"healthy\",\"system\":\"healthy\"}}");
    
    http_response_t *response = create_http_response(json, 200, "application/json");
    free(json);
    return response;
}

// Handle config endpoint
static http_response_t* handle_config_endpoint(void) {
    // This would normally get configuration data
    char *json = generate_json_response("success", "Configuration retrieved", 
                                      "{\"daemon_mode\":true,\"debug_mode\":false,\"log_level\":\"info\"}");
    
    http_response_t *response = create_http_response(json, 200, "application/json");
    free(json);
    return response;
}

// Handle network endpoint
static http_response_t* handle_network_endpoint(void) {
    // This would normally get network data
    char *json = generate_json_response("success", "Network status retrieved", 
                                      "{\"interfaces\":[{\"name\":\"eth0\",\"up\":true,\"ip\":\"192.168.1.100\"},{\"name\":\"wlan0\",\"up\":false,\"ip\":\"\"}]}");
    
    http_response_t *response = create_http_response(json, 200, "application/json");
    free(json);
    return response;
}

// Handle GPS endpoint
static http_response_t* handle_gps_endpoint(void) {
    // This would normally get GPS data
    char *json = generate_json_response("success", "GPS status retrieved", 
                                      "{\"active_source\":\"rutos\",\"latitude\":37.7749,\"longitude\":-122.4194,\"accuracy\":5.2}");
    
    http_response_t *response = create_http_response(json, 200, "application/json");
    free(json);
    return response;
}

// Handle Starlink endpoint
static http_response_t* handle_starlink_endpoint(void) {
    // This would normally get Starlink data
    char *json = generate_json_response("success", "Starlink status retrieved", 
                                      "{\"connected\":true,\"dish_online\":true,\"snr\":9.5,\"throughput\":150.0}");
    
    http_response_t *response = create_http_response(json, 200, "application/json");
    free(json);
    return response;
}

// Handle system endpoint
static http_response_t* handle_system_endpoint(void) {
    // This would normally get system data
    char *json = generate_json_response("success", "System status retrieved", 
                                      "{\"memory_usage\":45.2,\"cpu_usage\":12.8,\"disk_usage\":23.1,\"uptime\":86400}");
    
    http_response_t *response = create_http_response(json, 200, "application/json");
    free(json);
    return response;
}

// Handle notifications endpoint
static http_response_t* handle_notifications_endpoint(void) {
    // This would normally get notification data
    char *json = generate_json_response("success", "Notifications status retrieved", 
                                      "{\"enabled\":true,\"total_sent\":42,\"total_failed\":1,\"queue_size\":0}");
    
    http_response_t *response = create_http_response(json, 200, "application/json");
    free(json);
    return response;
}

// Route request to appropriate handler
static http_response_t* route_request(const char *path) {
    if (!path) {
        return create_http_response("{\"error\":\"Invalid path\"}", 400, "application/json");
    }
    
    // Find matching endpoint
    for (int i = 0; i < API_ENDPOINT_COUNT; i++) {
        if (strcmp(path, API_ENDPOINT_PATHS[i]) == 0) {
            switch (i) {
                case API_ENDPOINT_ROOT:
                    return handle_root_endpoint();
                case API_ENDPOINT_STATUS:
                    return handle_status_endpoint();
                case API_ENDPOINT_HEALTH:
                    return handle_health_endpoint();
                case API_ENDPOINT_CONFIG:
                    return handle_config_endpoint();
                case API_ENDPOINT_NETWORK:
                    return handle_network_endpoint();
                case API_ENDPOINT_GPS:
                    return handle_gps_endpoint();
                case API_ENDPOINT_STARLINK:
                    return handle_starlink_endpoint();
                case API_ENDPOINT_SYSTEM:
                    return handle_system_endpoint();
                case API_ENDPOINT_NOTIFICATIONS:
                    return handle_notifications_endpoint();
                default:
                    break;
            }
        }
    }
    
    // Not found
    return create_http_response("{\"error\":\"Endpoint not found\"}", 404, "application/json");
}

// HTTP request handler
static int http_request_handler(void *cls, struct MHD_Connection *connection,
                               const char *url, const char *method,
                               const char *version, const char *upload_data,
                               size_t *upload_data_size, void **con_cls) {
    (void)cls;
    (void)version;
    (void)upload_data;
    (void)upload_data_size;
    (void)con_cls;
    
    // Only handle GET requests for now
    if (strcmp(method, "GET") != 0) {
        const char *error_msg = "{\"error\":\"Method not allowed\"}";
        struct MHD_Response *response = MHD_create_response_from_buffer(
            strlen(error_msg), (void*)error_msg, MHD_RESPMEM_PERSISTENT);
        
        MHD_add_response_header(response, "Content-Type", "application/json");
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
        
        int ret = MHD_queue_response(connection, 405, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    // Route the request
    http_response_t *http_response = route_request(url);
    if (!http_response) {
        const char *error_msg = "{\"error\":\"Internal server error\"}";
        struct MHD_Response *response = MHD_create_response_from_buffer(
            strlen(error_msg), (void*)error_msg, MHD_RESPMEM_PERSISTENT);
        
        MHD_add_response_header(response, "Content-Type", "application/json");
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
        
        int ret = MHD_queue_response(connection, 500, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    // Create MHD response
    struct MHD_Response *response = MHD_create_response_from_buffer(
        http_response->size, (void*)http_response->data, MHD_RESPMEM_MUST_FREE);
    
    MHD_add_response_header(response, "Content-Type", http_response->content_type);
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    
    int ret = MHD_queue_response(connection, http_response->status_code, response);
    MHD_destroy_response(response);
    
    // Clean up
    free_http_response(http_response);
    
    return ret;
}

// Initialize API server
static int api_server_init(int port, const char *bind_address) {
    if (g_api_server_running) {
        LOGX_WARN("API server already running");
        return AUTONOMY_SUCCESS;
    }
    
    if (port > 0) {
        g_api_port = port;
    }
    
    if (bind_address) {
        strncpy(g_api_bind_address, bind_address, sizeof(g_api_bind_address) - 1);
        g_api_bind_address[sizeof(g_api_bind_address) - 1] = '\0';
    }
    
    // Create HTTP daemon
    g_http_daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, g_api_port, NULL, NULL,
                                     &http_request_handler, NULL,
                                     MHD_OPTION_END);
    
    if (!g_http_daemon) {
        LOGX_ERROR("Failed to start HTTP daemon on port %d", g_api_port);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    g_api_server_running = true;
    LOGX_INFO("API server started on %s:%d", g_api_bind_address, g_api_port);
    
    return AUTONOMY_SUCCESS;
}

// Start API server with default settings
static int api_server_start(void) {
    return api_server_init(8080, "0.0.0.0");
}

// Stop API server
static void api_server_stop(void) {
    if (g_http_daemon) {
        MHD_stop_daemon(g_http_daemon);
        g_http_daemon = NULL;
    }
    
    g_api_server_running = false;
    LOGX_INFO("API server stopped");
}

// Check if API server is running
static bool api_server_is_running(void) {
    return g_api_server_running;
}

// Get API server port
static int api_server_get_port(void) {
    return g_api_port;
}

// Get API server bind address
const char* api_server_get_bind_address(void) {
    return g_api_bind_address;
}

// Set API server configuration
static int api_server_set_config(int port, const char *bind_address) {
    if (g_api_server_running) {
        LOGX_WARN("Cannot change configuration while server is running");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (port > 0) {
        g_api_port = port;
    }
    
    if (bind_address) {
        strncpy(g_api_bind_address, bind_address, sizeof(g_api_bind_address) - 1);
        g_api_bind_address[sizeof(g_api_bind_address) - 1] = '\0';
    }
    
    return AUTONOMY_SUCCESS;
}

// Cleanup API server
static void api_server_cleanup(void) {
    api_server_stop();
    pthread_mutex_destroy(&g_api_mutex);
    LOGX_INFO("API server cleaned up");
}
