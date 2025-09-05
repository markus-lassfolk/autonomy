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
#include <pthread.h>
#include <math.h>

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
        sscanf(uptime, "\"uptimeS\": %llu", &status->device_state.uptime_s);
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

// Thread data for parallel API collection
typedef struct {
    starlink_comprehensive_gps_t *result;
    pthread_mutex_t *mutex;
    int *success_count;
} parallel_collection_data_t;

// Thread function for collecting location data
static void* collect_location_data(void *arg) {
    parallel_collection_data_t *data = (parallel_collection_data_t*)arg;
    char response[2048];
    
    if (starlink_send_request(STARLINK_METHOD_GET_LOCATION, response, sizeof(response)) == 0) {
        // Parse location response (simplified - would need proper JSON parsing)
        pthread_mutex_lock(data->mutex);
        data->result->latitude = 0.0;  // Would parse from response
        data->result->longitude = 0.0; // Would parse from response
        data->result->altitude = 0.0;  // Would parse from response
        data->result->accuracy = 0.0;  // Would parse from response
        strncpy(data->result->gps_source, "GNC_FUSED", sizeof(data->result->gps_source) - 1);
        data->result->gps_source[sizeof(data->result->gps_source) - 1] = '\0';
        
        // Add to data sources
        if (strlen(data->result->data_sources) > 0) {
            strncat(data->result->data_sources, ",", sizeof(data->result->data_sources) - strlen(data->result->data_sources) - 1);
        }
        strncat(data->result->data_sources, "get_location", sizeof(data->result->data_sources) - strlen(data->result->data_sources) - 1);
        
        (*data->success_count)++;
        pthread_mutex_unlock(data->mutex);
    }
    
    return NULL;
}

// Thread function for collecting status data
static void* collect_status_data(void *arg) {
    parallel_collection_data_t *data = (parallel_collection_data_t*)arg;
    char response[2048];
    
    if (starlink_send_request(STARLINK_METHOD_GET_STATUS, response, sizeof(response)) == 0) {
        // Parse status response (simplified - would need proper JSON parsing)
        pthread_mutex_lock(data->mutex);
        data->result->gps_valid = true;      // Would parse from response
        data->result->gps_satellites = 8;    // Would parse from response
        data->result->no_sats_after_ttff = false; // Would parse from response
        data->result->inhibit_gps = false;   // Would parse from response
        
        // Add to data sources
        if (strlen(data->result->data_sources) > 0) {
            strncat(data->result->data_sources, ",", sizeof(data->result->data_sources) - strlen(data->result->data_sources) - 1);
        }
        strncat(data->result->data_sources, "get_status", sizeof(data->result->data_sources) - strlen(data->result->data_sources) - 1);
        
        (*data->success_count)++;
        pthread_mutex_unlock(data->mutex);
    }
    
    return NULL;
}

// Thread function for collecting diagnostics data
static void* collect_diagnostics_data(void *arg) {
    parallel_collection_data_t *data = (parallel_collection_data_t*)arg;
    char response[2048];
    
    if (starlink_send_request(STARLINK_METHOD_GET_DIAGNOSTICS, response, sizeof(response)) == 0) {
        // Parse diagnostics response (simplified - would need proper JSON parsing)
        pthread_mutex_lock(data->mutex);
        data->result->location_enabled = true;        // Would parse from response
        data->result->uncertainty_meters = 5.0;       // Would parse from response
        data->result->uncertainty_meters_valid = true; // Would parse from response
        data->result->gps_time_s = time(NULL);        // Would parse from response
        
        // Add to data sources
        if (strlen(data->result->data_sources) > 0) {
            strncat(data->result->data_sources, ",", sizeof(data->result->data_sources) - strlen(data->result->data_sources) - 1);
        }
        strncat(data->result->data_sources, "get_diagnostics", sizeof(data->result->data_sources) - strlen(data->result->data_sources) - 1);
        
        (*data->success_count)++;
        pthread_mutex_unlock(data->mutex);
    }
    
    return NULL;
}

// Calculate confidence score based on collected data
static void calculate_confidence_score(starlink_comprehensive_gps_t *gps) {
    double confidence = 0.0;
    
    // Base confidence from data sources
    int source_count = 0;
    char *sources = gps->data_sources;
    while (*sources) {
        if (*sources == ',') source_count++;
        sources++;
    }
    source_count++; // Count the last source
    
    if (source_count >= 2) {
        confidence += 0.3; // Multiple sources
    }
    
    // GPS validity
    if (gps->gps_valid) {
        confidence += 0.4;
    }
    
    // Satellite count
    if (gps->gps_satellites >= 4) {
        confidence += 0.2;
    }
    
    // Accuracy assessment
    if (gps->accuracy > 0 && gps->accuracy < 10) {
        confidence += 0.1;
    }
    
    gps->confidence = fmin(confidence, 1.0);
    
    // Determine quality score
    if (gps->confidence >= 0.9) {
        strncpy(gps->quality_score, "excellent", sizeof(gps->quality_score) - 1);
    } else if (gps->confidence >= 0.7) {
        strncpy(gps->quality_score, "good", sizeof(gps->quality_score) - 1);
    } else if (gps->confidence >= 0.5) {
        strncpy(gps->quality_score, "fair", sizeof(gps->quality_score) - 1);
    } else {
        strncpy(gps->quality_score, "poor", sizeof(gps->quality_score) - 1);
    }
    gps->quality_score[sizeof(gps->quality_score) - 1] = '\0';
}

// Comprehensive Starlink GPS collection with parallel API calls
int starlink_collect_comprehensive_gps(starlink_comprehensive_gps_t *gps) {
    if (!gps || !g_starlink_state.initialized) {
        return -1;
    }
    
    // Initialize result structure
    memset(gps, 0, sizeof(starlink_comprehensive_gps_t));
    gps->collected_at = time(NULL);
    
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);
    
    // Create threads for parallel collection
    pthread_t threads[3];
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    int success_count = 0;
    
    parallel_collection_data_t thread_data = {
        .result = gps,
        .mutex = &mutex,
        .success_count = &success_count
    };
    
    // Start parallel collection threads
    pthread_create(&threads[0], NULL, collect_location_data, &thread_data);
    pthread_create(&threads[1], NULL, collect_status_data, &thread_data);
    pthread_create(&threads[2], NULL, collect_diagnostics_data, &thread_data);
    
    // Wait for all threads to complete
    for (int i = 0; i < 3; i++) {
        pthread_join(threads[i], NULL);
    }
    
    pthread_mutex_destroy(&mutex);
    
    // Calculate collection time
    gettimeofday(&end_time, NULL);
    gps->collection_ms = (end_time.tv_sec - start_time.tv_sec) * 1000 + 
                        (end_time.tv_usec - start_time.tv_usec) / 1000;
    
    // Calculate confidence and quality
    calculate_confidence_score(gps);
    
    // Set validity based on success
    gps->valid = (success_count > 0);
    
    return (success_count > 0) ? 0 : -1;
}

// Get location with fallback logic
int starlink_get_location_with_fallback(starlink_comprehensive_gps_t *gps) {
    if (!gps) {
        return -1;
    }
    
    // Try comprehensive collection first
    if (starlink_collect_comprehensive_gps(gps) == 0) {
        return 0;
    }
    
    // Fallback to diagnostics coordinates only
    char response[2048];
    if (starlink_send_request(STARLINK_METHOD_GET_DIAGNOSTICS, response, sizeof(response)) == 0) {
        // Parse diagnostics response for coordinates (simplified)
        memset(gps, 0, sizeof(starlink_comprehensive_gps_t));
        gps->latitude = 0.0;  // Would parse from response
        gps->longitude = 0.0; // Would parse from response
        gps->altitude = 0.0;  // Would parse from response
        gps->accuracy = 0.0;  // Would parse from response
        gps->collected_at = time(NULL);
        gps->valid = true;
        gps->confidence = 0.5; // Lower confidence for fallback
        strncpy(gps->quality_score, "fair", sizeof(gps->quality_score) - 1);
        strncpy(gps->data_sources, "get_diagnostics", sizeof(gps->data_sources) - 1);
        return 0;
    }
    
    return -1;
}
