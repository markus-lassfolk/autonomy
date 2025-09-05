#include "starlink_types.h"
#include "starlink_tracker.h"
#include "obstruction_analyzer.h"
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
    // Enhanced tracking support
    bool tracking_enabled;
    time_t last_obstruction_update;
    time_t last_location_update;
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
    
    // Create gRPC request (simplified - in real implementation this would be proper gRPC)
    char request[512];
    const char *method_names[] = {
        "get_status",
        "get_history", 
        "get_device_info",
        "get_location",
        "get_diagnostics"
    };
    
    // Format: {"method": "method_name"}
    snprintf(request, sizeof(request), 
             "POST / HTTP/1.1\r\n"
             "Host: %s:%d\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %zu\r\n"
             "\r\n"
             "{\"method\": \"%s\"}",
             g_starlink_config.host, g_starlink_config.port,
             strlen("{\"method\": \"method_name\"}"),
             method_names[method]);
    
    // Send request
    ssize_t sent = send(g_starlink_state.socket_fd, request, strlen(request), 0);
    if (sent < 0) {
        g_starlink_state.connection_healthy = false;
        return -1;
    }
    
    // Receive response
    ssize_t received = recv(g_starlink_state.socket_fd, response, response_size - 1, 0);
    if (received < 0) {
        g_starlink_state.connection_healthy = false;
        return -1;
    }
    
    response[received] = '\0';
    return received;
}

// Parse JSON response from Starlink (simplified parser)
int starlink_parse_response(const char *json_response, starlink_status_response_t *status) {
    if (!json_response || !status) {
        return -1;
    }
    
    // Initialize status structure
    memset(status, 0, sizeof(starlink_status_response_t));
    
    // Simple JSON parsing (in production, use a proper JSON library)
    // This is a simplified version - real implementation would use cJSON or similar
    
    // Extract basic fields using string search
    const char *device_id = strstr(json_response, "\"id\":");
    if (device_id) {
        sscanf(device_id, "\"id\": \"%63[^\"]\"", status->device_info.id);
    }
    
    const char *hardware_ver = strstr(json_response, "\"hardwareVersion\":");
    if (hardware_ver) {
        sscanf(hardware_ver, "\"hardwareVersion\": \"%31[^\"]\"", status->device_info.hardware_version);
    }
    
    const char *software_ver = strstr(json_response, "\"softwareVersion\":");
    if (software_ver) {
        sscanf(software_ver, "\"softwareVersion\": \"%31[^\"]\"", status->device_info.software_version);
    }
    
    const char *country = strstr(json_response, "\"countryCode\":");
    if (country) {
        sscanf(country, "\"countryCode\": \"%7[^\"]\"", status->device_info.country_code);
    }
    
    // Extract numeric fields
    const char *uptime = strstr(json_response, "\"uptimeS\":");
    if (uptime) {
        sscanf(uptime, "\"uptimeS\": %lu", &status->device_state.uptime_s);
    }
    
    const char *gps_valid = strstr(json_response, "\"gpsValid\":");
    if (gps_valid) {
        status->gps_stats.gps_valid = strstr(gps_valid, "true") != NULL;
    }
    
    const char *gps_sats = strstr(json_response, "\"gpsSats\":");
    if (gps_sats) {
        sscanf(gps_sats, "\"gpsSats\": %d", &status->gps_stats.gps_sats);
    }
    
    const char *lat = strstr(json_response, "\"lat\":");
    if (lat) {
        sscanf(lat, "\"lat\": %lf", &status->location.lat);
    }
    
    const char *lon = strstr(json_response, "\"lon\":");
    if (lon) {
        sscanf(lon, "\"lon\": %lf", &status->location.lon);
    }
    
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

// Enhanced tracking functions

// Get obstruction map via gRPC
int starlink_get_obstruction_map(char *response, size_t response_size) {
    if (!response || response_size == 0) {
        return -1;
    }
    
    char grpc_command[512];
    snprintf(grpc_command, sizeof(grpc_command), 
            "grpcurl -plaintext -d '{\"dishGetObstructionMap\":{}}' %s:%d SpaceX.API.Device.Device/Handle",
            g_starlink_config.host, g_starlink_config.port);
    
    FILE *fp = popen(grpc_command, "r");
    if (!fp) {
        return -1;
    }
    
    size_t bytes_read = fread(response, 1, response_size - 1, fp);
    response[bytes_read] = '\0';
    
    int status = pclose(fp);
    if (status == 0) {
        g_starlink_state.last_obstruction_update = time(NULL);
        return 0;
    }
    
    return -1;
}

// Get diagnostics via gRPC (includes boresight)
int starlink_get_diagnostics(char *response, size_t response_size) {
    if (!response || response_size == 0) {
        return -1;
    }
    
    char grpc_command[512];
    snprintf(grpc_command, sizeof(grpc_command), 
            "grpcurl -plaintext -d '{\"dishGetDiagnostics\":{}}' %s:%d SpaceX.API.Device.Device/Handle",
            g_starlink_config.host, g_starlink_config.port);
    
    FILE *fp = popen(grpc_command, "r");
    if (!fp) {
        return -1;
    }
    
    size_t bytes_read = fread(response, 1, response_size - 1, fp);
    response[bytes_read] = '\0';
    
    int status = pclose(fp);
    return (status == 0) ? 0 : -1;
}

// Enhanced location function with altitude support
int starlink_get_enhanced_location(dish_location_t *location) {
    if (!location) {
        return -1;
    }
    
    char response[4096];
    char grpc_command[512];
    snprintf(grpc_command, sizeof(grpc_command), 
            "grpcurl -plaintext -d '{\"getLocation\":{}}' %s:%d SpaceX.API.Device.Device/Handle",
            g_starlink_config.host, g_starlink_config.port);
    
    FILE *fp = popen(grpc_command, "r");
    if (!fp) {
        return -1;
    }
    
    size_t bytes_read = fread(response, 1, sizeof(response) - 1, fp);
    response[bytes_read] = '\0';
    
    int status = pclose(fp);
    if (status != 0) {
        return -1;
    }
    
    // Parse location and get diagnostics for boresight
    int location_result = obstruction_analyzer_parse_dish_response(response, NULL, location);
    
    if (location_result == OBSTRUCTION_SUCCESS) {
        g_starlink_state.last_location_update = time(NULL);
        return 0;
    }
    
    return -1;
}

// Get combined tracking data (location + obstruction map + diagnostics)
int starlink_get_tracking_data(dish_location_t *location, char *obstruction_response, size_t obstruction_size) {
    if (!location || !obstruction_response || obstruction_size == 0) {
        return -1;
    }
    
    // Get location and diagnostics
    int location_result = starlink_get_enhanced_location(location);
    
    // Get obstruction map
    int obstruction_result = starlink_get_obstruction_map(obstruction_response, obstruction_size);
    
    // Return success if at least one succeeded
    return (location_result == 0 || obstruction_result == 0) ? 0 : -1;
}

// Enable/disable tracking mode
int starlink_set_tracking_enabled(bool enabled) {
    g_starlink_state.tracking_enabled = enabled;
    return 0;
}

// Check if tracking is enabled
bool starlink_is_tracking_enabled(void) {
    return g_starlink_state.tracking_enabled;
}

// Get last update times
time_t starlink_get_last_obstruction_update(void) {
    return g_starlink_state.last_obstruction_update;
}

time_t starlink_get_last_location_update(void) {
    return g_starlink_state.last_location_update;
}

// Test connectivity to dish for tracking
int starlink_test_tracking_connectivity(void) {
    char test_response[1024];
    
    // Try to get status as a connectivity test
    int result = starlink_send_request(STARLINK_METHOD_GET_STATUS, test_response, sizeof(test_response));
    
    if (result == 0) {
        g_starlink_state.connection_healthy = true;
        g_starlink_state.last_connection = time(NULL);
        return 0;
    } else {
        g_starlink_state.connection_healthy = false;
        return -1;
    }
}

// Cleanup Starlink client
void starlink_client_cleanup(void) {
    starlink_disconnect();
    g_starlink_state.initialized = false;
    g_starlink_state.tracking_enabled = false;
}
