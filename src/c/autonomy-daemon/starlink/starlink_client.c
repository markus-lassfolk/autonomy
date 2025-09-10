#include "starlink_types.h"
#include "starlink_grpc_collector.h"
#include "../core/types.h"
#include "../utils/json_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <json-c/json.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Global Starlink client configuration
static starlink_config_t g_starlink_config = {
    .host = STARLINK_DEFAULT_HOST,
    .port = STARLINK_DEFAULT_PORT,
    .timeout_seconds = STARLINK_DEFAULT_TIMEOUT,
    .grpc_first = true,
    .http_first = false,
    .predictive_enabled = true
};

// Starlink client state
static struct {
    bool initialized;
    int socket_fd;
    time_t last_connection;
    bool connection_healthy;
} g_starlink_state = {0};

// Initialize Starlink client
int starlink_client_init(const starlink_config_t *config) {
    if (!config) {
        return -1;
    }
    
    // Copy configuration
    memcpy(&g_starlink_config, config, sizeof(starlink_config_t));
    
    // Initialize state
    g_starlink_state.initialized = true;
    g_starlink_state.socket_fd = -1;
    g_starlink_state.last_connection = 0;
    g_starlink_state.connection_healthy = false;
    
    return 0;
}

// Create TCP connection to Starlink dish
int starlink_connect(void) {
    if (g_starlink_state.socket_fd >= 0) {
        close(g_starlink_state.socket_fd);
        g_starlink_state.socket_fd = -1;
    }
    
    // Create socket
    g_starlink_state.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_starlink_state.socket_fd < 0) {
        return -1;
    }
    
    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = g_starlink_config.timeout_seconds;
    timeout.tv_usec = 0;
    
    if (setsockopt(g_starlink_state.socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        close(g_starlink_state.socket_fd);
        g_starlink_state.socket_fd = -1;
        return -1;
    }
    
    if (setsockopt(g_starlink_state.socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        close(g_starlink_state.socket_fd);
        g_starlink_state.socket_fd = -1;
        return -1;
    }
    
    // Resolve hostname
    struct hostent *server = gethostbyname(g_starlink_config.host);
    if (!server) {
        close(g_starlink_state.socket_fd);
        g_starlink_state.socket_fd = -1;
        return -1;
    }
    
    // Setup server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(g_starlink_config.port);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    
    // Connect
    if (connect(g_starlink_state.socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(g_starlink_state.socket_fd);
        g_starlink_state.socket_fd = -1;
        return -1;
    }
    
    g_starlink_state.last_connection = time(NULL);
    g_starlink_state.connection_healthy = true;
    
    return 0;
}

// Disconnect from Starlink dish
void starlink_disconnect(void) {
    if (g_starlink_state.socket_fd >= 0) {
        close(g_starlink_state.socket_fd);
        g_starlink_state.socket_fd = -1;
    }
    g_starlink_state.connection_healthy = false;
}

// Send gRPC request to Starlink
int starlink_send_request(starlink_method_t method, char *response, size_t response_size) {
    if (!g_starlink_state.connection_healthy || g_starlink_state.socket_fd < 0) {
        if (starlink_connect() < 0) {
            return -1;
        }
    }
    
    // Create gRPC request using proper gRPC implementation
    char response_buffer[1024];
    memset(response_buffer, 0, sizeof(response_buffer)); // Initialize buffer
    int result = -1;
    
    switch (method) {
        case 0: // get_status
            // Temporarily disable gRPC call to test daemon stability
            result = AUTONOMY_ERROR; // Force failure to test error handling
            // result = starlink_grpc_call_get_status(response_buffer, sizeof(response_buffer));
            break;
        case 1: // get_history
            result = starlink_grpc_call_get_history(response_buffer, sizeof(response_buffer));
            break;
        case 2: // get_device_info (use get_status as fallback)
            // Temporarily disable gRPC call to test daemon stability
            result = AUTONOMY_ERROR; // Force failure to test error handling
            // result = starlink_grpc_call_get_status(response_buffer, sizeof(response_buffer));
            break;
        case 3: // get_location
            result = starlink_grpc_call_get_location(response_buffer, sizeof(response_buffer));
            break;
        case 4: // get_diagnostics
            result = starlink_grpc_call_get_diagnostics(response_buffer, sizeof(response_buffer));
            break;
        default:
            return -1;
    }
    
    if (result != AUTONOMY_SUCCESS) {
        // Clear output buffer on failure
        if (response && response_size > 0) {
            memset(response, 0, response_size);
        }
        return -1;
    }
    
    // Copy response from gRPC call to output buffer
    size_t response_len = strlen(response_buffer);
    if (response_len >= response_size) {
        response_len = response_size - 1;
    }
    memcpy(response, response_buffer, response_len);
    response[response_len] = '\0';
    
    g_starlink_state.connection_healthy = true;
    
    return response_len;
}

// Parse JSON response from Starlink using json-c
int starlink_parse_response(const char *json_response, starlink_status_response_t *status) {
    if (!json_response || !status) {
        return -1;
    }
    
    memset(status, 0, sizeof(starlink_status_response_t));
    
    json_object *root = json_tokener_parse(json_response);
    if (!root) {
        return -1;
    }
    
    // Extract device info
    json_object *device_info = json_object_object_get(root, "deviceInfo");
    if (device_info) {
        json_object_object_foreach(device_info, key, val) {
            if (strcmp(key, "id") == 0) strncpy(status->device_info.id, json_object_get_string(val), sizeof(status->device_info.id) - 1);
            if (strcmp(key, "hardwareVersion") == 0) strncpy(status->device_info.hardware_version, json_object_get_string(val), sizeof(status->device_info.hardware_version) - 1);
            if (strcmp(key, "softwareVersion") == 0) strncpy(status->device_info.software_version, json_object_get_string(val), sizeof(status->device_info.software_version) - 1);
            if (strcmp(key, "countryCode") == 0) strncpy(status->device_info.country_code, json_object_get_string(val), sizeof(status->device_info.country_code) - 1);
        }
    }
    
    // Extract device state
    json_object *device_state = json_object_object_get(root, "deviceState");
    if (device_state) {
        json_object *uptime;
        if (json_object_object_get_ex(device_state, "uptimeS", &uptime)) {
            status->device_state.uptime_s = json_object_get_uint64(uptime);
        }
    }

    // Free the document
    json_object_put(root);
    
    return 0;
}

// Get Starlink status
int starlink_get_status(starlink_status_response_t *status) {
    if (!status) {
        return -1;
    }
    
    char response[4096];
    int result = starlink_send_request(STARLINK_METHOD_GET_STATUS, response, sizeof(response));
    if (result < 0) {
        return -1;
    }
    
    return starlink_parse_response(response, status);
}

// Get Starlink device info
int starlink_get_device_info(starlink_device_info_t *device_info) {
    if (!device_info) {
        return -1;
    }
    
    char response[4096];
    int result = starlink_send_request(STARLINK_METHOD_GET_DEVICE_INFO, response, sizeof(response));
    if (result < 0) {
        return -1;
    }
    
    starlink_status_response_t status;
    if (starlink_parse_response(response, &status) == 0) {
        memcpy(device_info, &status.device_info, sizeof(starlink_device_info_t));
        return 0;
    }
    
    return -1;
}

// Get Starlink location
int starlink_get_location(starlink_lla_position_t *location) {
    if (!location) {
        return -1;
    }
    
    char response[4096];
    int result = starlink_send_request(STARLINK_METHOD_GET_LOCATION, response, sizeof(response));
    if (result < 0) {
        return -1;
    }
    
    starlink_status_response_t status;
    if (starlink_parse_response(response, &status) == 0) {
        // Extract location from status response
        location->lat = status.location.lat;
        location->lon = status.location.lon;
        location->alt = 0.0; // Altitude not always available
        return 0;
    }
    
    return -1;
}

// Check Starlink connection health
bool starlink_is_healthy(void) {
    return g_starlink_state.connection_healthy && g_starlink_state.socket_fd >= 0;
}

// Get Starlink configuration
const starlink_config_t* starlink_get_config(void) {
    return &g_starlink_config;
}

// Cleanup Starlink client
void starlink_client_cleanup(void) {
    starlink_disconnect();
    g_starlink_state.initialized = false;
}
