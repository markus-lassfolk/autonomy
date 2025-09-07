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
#include <json-c/json.h>

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
    
    // Create gRPC request using grpcurl
    char request[512];
    const char *method_names[] = {
        "get_status",
        "get_history", 
        "get_device_info",
        "get_location",
        "get_diagnostics"
    };
    
    snprintf(request, sizeof(request),
             "grpcurl -plaintext -d '{\\"%s\\":{}}' %s:%d SpaceX.API.Device.Device/Handle",
             method_names[method], g_starlink_config.host, g_starlink_config.port);
    
    FILE *fp = popen(request, "r");
    if (!fp) {
        return -1;
    }
    
    size_t bytes_read = fread(response, 1, response_size - 1, fp);
    response[bytes_read] = '\0';
    
    int status = pclose(fp);
    if (status != 0) {
        g_starlink_state.connection_healthy = false;
        return -1;
    }
    
    return bytes_read;
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
    
    json_object *result_obj;
    if (json_object_object_get_ex(root, "result", &result_obj)) {
        // Handle nested result object for some responses
        root = result_obj;
    }

    // Extract device info
    json_object_object_foreach(root, key, val) {
        if (strcmp(key, "id") == 0) {
            strncpy(status->device_info.id, json_object_get_string(val), sizeof(status->device_info.id) - 1);
        } else if (strcmp(key, "hardwareVersion") == 0) {
            strncpy(status->device_info.hardware_version, json_object_get_string(val), sizeof(status->device_info.hardware_version) - 1);
        } else if (strcmp(key, "softwareVersion") == 0) {
            strncpy(status->device_info.software_version, json_object_get_string(val), sizeof(status->device_info.software_version) - 1);
        } else if (strcmp(key, "countryCode") == 0) {
            strncpy(status->device_info.country_code, json_object_get_string(val), sizeof(status->device_info.country_code) - 1);
        } else if (strcmp(key, "uptimeS") == 0) {
            status->device_state.uptime_s = json_object_get_uint64(val);
        } else if (strcmp(key, "gpsValid") == 0) {
            status->gps_stats.gps_valid = json_object_get_boolean(val);
        } else if (strcmp(key, "gpsSats") == 0) {
            status->gps_stats.gps_sats = json_object_get_int(val);
        } else if (strcmp(key, "latitude") == 0) {
            status->location.lat = json_object_get_double(val);
        } else if (strcmp(key, "longitude") == 0) {
            status->location.lon = json_object_get_double(val);
        }
    }
    
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
    
    if (starlink_send_request(STARLINK_METHOD_GET_LOCATION, response, sizeof(response)) >= 0) {
        json_object *root = json_tokener_parse(response);
        if (root) {
            pthread_mutex_lock(data->mutex);
            json_object_object_foreach(root, key, val) {
                if (strcmp(key, "latitude") == 0) data->result->latitude = json_object_get_double(val);
                if (strcmp(key, "longitude") == 0) data->result->longitude = json_object_get_double(val);
                if (strcmp(key, "altitude") == 0) data->result->altitude = json_object_get_double(val);
            }
            strncpy(data->result->gps_source, "GNC_FUSED", sizeof(data->result->gps_source) - 1);
            
            if (strlen(data->result->data_sources) > 0) {
                strncat(data->result->data_sources, ",", sizeof(data->result->data_sources) - strlen(data->result->data_sources) - 1);
            }
            strncat(data->result->data_sources, "get_location", sizeof(data->result->data_sources) - strlen(data->result->data_sources) - 1);
            
            (*data->success_count)++;
            pthread_mutex_unlock(data->mutex);
            json_object_put(root);
        }
    }
    
    return NULL;
}

// Thread function for collecting status data
static void* collect_status_data(void *arg) {
    parallel_collection_data_t *data = (parallel_collection_data_t*)arg;
    char response[2048];
    
    if (starlink_send_request(STARLINK_METHOD_GET_STATUS, response, sizeof(response)) >= 0) {
        json_object *root = json_tokener_parse(response);
        if (root) {
            pthread_mutex_lock(data->mutex);
            json_object_object_foreach(root, key, val) {
                if (strcmp(key, "gpsValid") == 0) data->result->gps_valid = json_object_get_boolean(val);
                if (strcmp(key, "gpsSats") == 0) data->result->gps_satellites = json_object_get_int(val);
            }
            
            if (strlen(data->result->data_sources) > 0) {
                strncat(data->result->data_sources, ",", sizeof(data->result->data_sources) - strlen(data->result->data_sources) - 1);
            }
            strncat(data->result->data_sources, "get_status", sizeof(data->result->data_sources) - strlen(data->result->data_sources) - 1);
            
            (*data->success_count)++;
            pthread_mutex_unlock(data->mutex);
            json_object_put(root);
        }
    }
    
    return NULL;
}

// Thread function for collecting diagnostics data
static void* collect_diagnostics_data(void *arg) {
    parallel_collection_data_t *data = (parallel_collection_data_t*)arg;
    char response[2048];
    
    if (starlink_send_request(STARLINK_METHOD_GET_DIAGNOSTICS, response, sizeof(response)) >= 0) {
        json_object *root = json_tokener_parse(response);
        if (root) {
            pthread_mutex_lock(data->mutex);
            json_object_object_foreach(root, key, val) {
                 if (strcmp(key, "locationEnabled") == 0) data->result->location_enabled = json_object_get_boolean(val);
                 if (strcmp(key, "uncertaintyMeters") == 0) data->result->uncertainty_meters = json_object_get_double(val);
            }

            if (strlen(data->result->data_sources) > 0) {
                strncat(data->result->data_sources, ",", sizeof(data->result->data_sources) - strlen(data->result->data_sources) - 1);
            }
            strncat(data->result->data_sources, "get_diagnostics", sizeof(data->result->data_sources) - strlen(data->result->data_sources) - 1);
            
            (*data->success_count)++;
            pthread_mutex_unlock(data->mutex);
            json_object_put(root);
        }
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
    if (starlink_send_request(STARLINK_METHOD_GET_DIAGNOSTICS, response, sizeof(response)) >= 0) {
        memset(gps, 0, sizeof(starlink_comprehensive_gps_t));
        
        json_object *root = json_tokener_parse(response);
        if(root) {
            json_object_object_foreach(root, key, val) {
                if (strcmp(key, "latitude") == 0) gps->latitude = json_object_get_double(val);
                if (strcmp(key, "longitude") == 0) gps->longitude = json_object_get_double(val);
                if (strcmp(key, "altitude") == 0) gps->altitude = json_object_get_double(val);
            }
            json_object_put(root);
        }

        gps->collected_at = time(NULL);
        gps->valid = true;
        gps->confidence = 0.5; // Lower confidence for fallback
        strncpy(gps->quality_score, "fair", sizeof(gps->quality_score) - 1);
        strncpy(gps->data_sources, "get_diagnostics", sizeof(gps->data_sources) - 1);
        return 0;
    }
    
    return -1;
}
