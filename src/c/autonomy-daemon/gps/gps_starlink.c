#include "gps_starlink.h"
#include "starlink_comprehensive.h"
#include "../utils/logx.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <json-c/json.h>

// Starlink GPS configuration
static const int GPS_UPDATE_INTERVAL = 10;        // 10 seconds
static const int GPS_TIMEOUT = 30;                // 30 seconds
static const char* DEFAULT_STARLINK_IP = "192.168.100.1";
static const int DEFAULT_STARLINK_PORT = 80;
static const char* GPS_ENDPOINT = "/api/v1/gps";

// GPS accuracy thresholds
static const double MIN_ACCURACY = 10.0;          // 10 meters
static const double MAX_ACCURACY = 100.0;         // 100 meters
static const double CONFIDENCE_THRESHOLD = 0.7;   // 70% confidence

// Global Starlink GPS state
static gps_starlink_t g_starlink_gps = {0};
static pthread_mutex_t g_starlink_gps_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_starlink_gps_initialized = false;
static pthread_t g_starlink_gps_thread = 0;
static bool g_starlink_gps_thread_running = false;

// Initialize Starlink GPS system
int gps_starlink_init(void) {
    if (g_starlink_gps_initialized) {
        LOGX_WARN_MSG("Starlink GPS already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_starlink_gps_mutex);
    
    // Initialize Starlink GPS state
    memset(&g_starlink_gps, 0, sizeof(gps_starlink_t));
    g_starlink_gps.enabled = true;
    g_starlink_gps.update_interval = GPS_UPDATE_INTERVAL;
    g_starlink_gps.timeout = GPS_TIMEOUT;
    g_starlink_gps.last_update = 0;
    g_starlink_gps.total_updates = 0;
    g_starlink_gps.successful_updates = 0;
    g_starlink_gps.failed_updates = 0;
    
    // Set default Starlink IP
    strncpy(g_starlink_gps.starlink_ip, DEFAULT_STARLINK_IP, sizeof(g_starlink_gps.starlink_ip) - 1);
    g_starlink_gps.starlink_ip[sizeof(g_starlink_gps.starlink_ip) - 1] = '\0';
    g_starlink_gps.starlink_port = DEFAULT_STARLINK_PORT;
    
    // Initialize GPS data
    g_starlink_gps.gps_data.timestamp = 0;
    g_starlink_gps.gps_data.lat = 0.0;
    g_starlink_gps.gps_data.lon = 0.0;
    g_starlink_gps.gps_data.altitude = 0.0;
    g_starlink_gps.gps_data.accuracy = 0.0;
    g_starlink_gps.gps_data.satellites = 0;
    g_starlink_gps.gps_data.fix_quality = 0;
    g_starlink_gps.gps_data.reliability_score = 0.0;
    
    g_starlink_gps_initialized = true;
    pthread_mutex_unlock(&g_starlink_gps_mutex);
    
    LOGX_INFO_MSG("Starlink GPS system initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Start Starlink GPS monitoring thread
int gps_starlink_start_monitoring(void) {
    if (!g_starlink_gps_initialized) {
        LOGX_ERROR_MSG("Starlink GPS not initialized");
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    if (g_starlink_gps_thread_running) {
        LOGX_WARN_MSG("Starlink GPS monitoring already running");
        return AUTONOMY_SUCCESS;
    }
    
    // Create monitoring thread
    int ret = pthread_create(&g_starlink_gps_thread, NULL, starlink_gps_monitor_thread, NULL);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to create Starlink GPS monitoring thread");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    g_starlink_gps_thread_running = true;
    LOGX_INFO_MSG("Starlink GPS monitoring started");
    
    return AUTONOMY_SUCCESS;
}

// Stop Starlink GPS monitoring
void gps_starlink_stop_monitoring(void) {
    if (!g_starlink_gps_thread_running) {
        return;
    }
    
    g_starlink_gps_thread_running = false;
    
    if (g_starlink_gps_thread != 0) {
        pthread_join(g_starlink_gps_thread, NULL);
        g_starlink_gps_thread = 0;
    }
    
    LOGX_INFO_MSG("Starlink GPS monitoring stopped");
}

// Starlink GPS monitoring thread
static void* starlink_gps_monitor_thread(void *arg) {
    (void)arg;
    
    LOGX_INFO_MSG("Starlink GPS monitoring thread started");
    
    while (g_starlink_gps_thread_running) {
        // Extract GPS data from Starlink
        gps_starlink_extract_data();
        
        // Sleep for update interval
        for (int i = 0; i < g_starlink_gps.update_interval && g_starlink_gps_thread_running; i++) {
            sleep(1);
        }
    }
    
    LOGX_INFO_MSG("Starlink GPS monitoring thread stopped");
    return NULL;
}

// Extract GPS data from Starlink dish
int gps_starlink_extract_data(void) {
    if (!g_starlink_gps_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_starlink_gps_mutex);
    
    time_t now = time(NULL);
    
    // Check if it's time to update
    if (g_starlink_gps.last_update > 0 && 
        (now - g_starlink_gps.last_update) < g_starlink_gps.update_interval) {
        pthread_mutex_unlock(&g_starlink_gps_mutex);
        return AUTONOMY_SUCCESS;
    }
    
    LOGX_DEBUG_MSG("Extracting GPS data from Starlink dish");
    
    // Try to get GPS data from Starlink API
    if (extract_gps_from_starlink_api()) {
        g_starlink_gps.successful_updates++;
        LOGX_DEBUG_MSG("Successfully extracted GPS data from Starlink API");
    } else {
        g_starlink_gps.failed_updates++;
        LOGX_WARN_MSG("Failed to extract GPS data from Starlink API");
        
        // No fallback - production system must use real data
        LOGX_ERROR_MSG("Starlink GPS data extraction failed - no fallback available in production mode");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    g_starlink_gps.last_update = now;
    g_starlink_gps.total_updates++;
    
    // Calculate reliability score
    calculate_gps_reliability();
    
    pthread_mutex_unlock(&g_starlink_gps_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Extract GPS data from Starlink API
static bool extract_gps_from_starlink_api(void) {
    // Create HTTP request to Starlink dish
    char request[1024];
    snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "User-Agent: Autonomy-Daemon/1.0\r\n"
        "Accept: application/json\r\n"
        "Connection: close\r\n"
        "\r\n",
        GPS_ENDPOINT, g_starlink_gps.starlink_ip, g_starlink_gps.starlink_port);
    
    // Create socket connection
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        LOGX_ERROR_MSG("Failed to create socket for Starlink API");
        return false;
    }
    
    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = g_starlink_gps.timeout;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    // Connect to Starlink dish
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(g_starlink_gps.starlink_port);
    
    if (inet_pton(AF_INET, g_starlink_gps.starlink_ip, &server_addr.sin_addr) <= 0) {
        LOGX_ERROR_MSG("Invalid Starlink IP address: %s", g_starlink_gps.starlink_ip);
        close(sock);
        return false;
    }
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        LOGX_ERROR_MSG("Failed to connect to Starlink dish at %s:%d", 
                   g_starlink_gps.starlink_ip, g_starlink_gps.starlink_port);
        close(sock);
        return false;
    }
    
    // Send HTTP request
    if (send(sock, request, strlen(request), 0) < 0) {
        LOGX_ERROR_MSG("Failed to send request to Starlink dish");
        close(sock);
        return false;
    }
    
    // Receive response
    char response[4096];
    int bytes_received = recv(sock, response, sizeof(response) - 1, 0);
    close(sock);
    
    if (bytes_received <= 0) {
        LOGX_ERROR_MSG("Failed to receive response from Starlink dish");
        return false;
    }
    
    response[bytes_received] = '\0';
    
    // Parse GPS data from response
    return parse_gps_from_response(response);
}

// Parse GPS data from Starlink API response
static bool parse_gps_from_response(const char *response) {
    if (!response) {
        return false;
    }
    
    // Find JSON data in HTTP response
    const char *json_start = strstr(response, "\r\n\r\n");
    if (!json_start) {
        json_start = strstr(response, "\n\n");
    }
    
    if (!json_start) {
        LOGX_ERROR_MSG("No JSON data found in Starlink response");
        return false;
    }
    
    json_start += 4; // Skip HTTP headers
    
    // Parse JSON response
    json_object *json = json_tokener_parse(json_start);
    if (!json) {
        LOGX_ERROR_MSG("Failed to parse JSON response from Starlink");
        return false;
    }
    
    bool success = false;
    
    // Extract GPS coordinates
    json_object *gps_obj, *lat_obj, *lon_obj, *alt_obj, *acc_obj, *sat_obj, *fix_obj;
    
    if (json_object_object_get_ex(json, "gps", &gps_obj)) {
        if (json_object_object_get_ex(gps_obj, "latitude", &lat_obj) &&
            json_object_object_get_ex(gps_obj, "longitude", &lon_obj)) {
            
            g_starlink_gps.gps_data.lat = json_object_get_double(lat_obj);
            g_starlink_gps.gps_data.lon = json_object_get_double(lon_obj);
            
            // Extract optional fields
            if (json_object_object_get_ex(gps_obj, "altitude", &alt_obj)) {
                g_starlink_gps.gps_data.altitude = json_object_get_double(alt_obj);
            }
            
            if (json_object_object_get_ex(gps_obj, "accuracy", &acc_obj)) {
                g_starlink_gps.gps_data.accuracy = json_object_get_double(acc_obj);
            }
            
            if (json_object_object_get_ex(gps_obj, "satellites", &sat_obj)) {
                g_starlink_gps.gps_data.satellites = json_object_get_int(sat_obj);
            }
            
            if (json_object_object_get_ex(gps_obj, "fix_quality", &fix_obj)) {
                g_starlink_gps.gps_data.fix_quality = json_object_get_int(fix_obj);
            }
            
            g_starlink_gps.gps_data.timestamp = time(NULL);
            success = true;
            
            LOGX_DEBUG_MSG("Extracted GPS: lat=%.6f, lon=%.6f, acc=%.1fm, sats=%d", 
                       g_starlink_gps.gps_data.lat, g_starlink_gps.gps_data.lon,
                       g_starlink_gps.gps_data.accuracy, g_starlink_gps.gps_data.satellites);
        }
    }
    
    json_object_put(json);
    
    if (!success) {
        LOGX_ERROR_MSG("Failed to extract GPS coordinates from Starlink response");
    }
    
    return success;
}

// Simulation removed - production system uses only real data

// Calculate GPS reliability score
static void calculate_gps_reliability(void) {
    double reliability = 0.0;
    
    // Base reliability on fix quality
    if (g_starlink_gps.gps_data.fix_quality > 0) {
        reliability += 0.3;
    }
    
    // Accuracy-based reliability
    if (g_starlink_gps.gps_data.accuracy > 0) {
        if (g_starlink_gps.gps_data.accuracy <= MIN_ACCURACY) {
            reliability += 0.4;  // High accuracy
        } else if (g_starlink_gps.gps_data.accuracy <= MAX_ACCURACY) {
            reliability += 0.2;  // Medium accuracy
        } else {
            reliability += 0.1;  // Low accuracy
        }
    }
    
    // Satellite count reliability
    if (g_starlink_gps.gps_data.satellites >= 6) {
        reliability += 0.2;  // Good satellite coverage
    } else if (g_starlink_gps.gps_data.satellites >= 4) {
        reliability += 0.1;  // Adequate satellite coverage
    }
    
    // Data freshness reliability
    time_t now = time(NULL);
    if (g_starlink_gps.gps_data.timestamp > 0) {
        int age = now - g_starlink_gps.gps_data.timestamp;
        if (age <= 60) {
            reliability += 0.1;  // Recent data
        } else if (age <= 300) {
            reliability += 0.05;  // Moderately recent data
        }
    }
    
    // Cap reliability at 1.0
    g_starlink_gps.gps_data.reliability_score = (reliability > 1.0) ? 1.0 : reliability;
}

// Get Starlink GPS data
int gps_starlink_get_data(gps_data_t *gps_data) {
    if (!g_starlink_gps_initialized || !gps_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_starlink_gps_mutex);
    
    // Try to use comprehensive collector if available for better data
    if (starlink_comprehensive_is_initialized()) {
        starlink_comprehensive_gps_t comprehensive_gps;
        if (starlink_comprehensive_collect_gps(&comprehensive_gps) == AUTONOMY_SUCCESS) {
            // Convert comprehensive GPS data to standard format
            gps_data->latitude = comprehensive_gps.latitude;
            gps_data->longitude = comprehensive_gps.longitude;
            gps_data->altitude = comprehensive_gps.altitude;
            gps_data->accuracy = comprehensive_gps.accuracy;
            gps_data->speed = comprehensive_gps.horizontal_speed_mps;
            gps_data->heading = 0.0; // Not available in comprehensive data
            gps_data->satellites = comprehensive_gps.gps_satellites;
            gps_data->hdop = 0.0; // Not available
            gps_data->vdop = 0.0; // Not available
            gps_data->fix_quality = comprehensive_gps.gps_valid ? 1 : 0;
            gps_data->timestamp = comprehensive_gps.collected_at;
            gps_data->valid = comprehensive_gps.valid;
            
            // Update cached data
            g_starlink_gps.gps_data = *gps_data;
            g_starlink_gps.last_update = time(NULL);
            g_starlink_gps.successful_updates++;
            
            LOGX_INFO_MSG("Starlink GPS data from comprehensive collector",
                     "sources", comprehensive_gps.data_sources,
                     "confidence", comprehensive_gps.confidence,
                     "quality", comprehensive_gps.quality_score);
            
            pthread_mutex_unlock(&g_starlink_gps_mutex);
            return AUTONOMY_SUCCESS;
        }
    }
    
    // Fallback to cached data
    memcpy(gps_data, &g_starlink_gps.gps_data, sizeof(gps_data_t));
    
    pthread_mutex_unlock(&g_starlink_gps_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get Starlink GPS status
int gps_starlink_get_status(gps_starlink_status_t *status) {
    if (!g_starlink_gps_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_starlink_gps_mutex);
    
    status->enabled = g_starlink_gps.enabled;
    status->update_interval = g_starlink_gps.update_interval;
    status->timeout = g_starlink_gps.timeout;
    status->last_update = g_starlink_gps.last_update;
    status->total_updates = g_starlink_gps.total_updates;
    status->successful_updates = g_starlink_gps.successful_updates;
    status->failed_updates = g_starlink_gps.failed_updates;
    // fallback_to_simulated removed - production uses only real data
    strncpy(status->starlink_ip, g_starlink_gps.starlink_ip, sizeof(status->starlink_ip) - 1);
    status->starlink_ip[sizeof(status->starlink_ip) - 1] = '\0';
    status->starlink_port = g_starlink_gps.starlink_port;
    
    pthread_mutex_unlock(&g_starlink_gps_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set Starlink GPS configuration
int gps_starlink_set_config(const gps_starlink_config_t *config) {
    if (!g_starlink_gps_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_starlink_gps_mutex);
    
    if (config->update_interval > 0) {
        g_starlink_gps.update_interval = config->update_interval;
    }
    
    if (config->timeout > 0) {
        g_starlink_gps.timeout = config->timeout;
    }
    
    if (config->starlink_ip[0] != '\0') {
        strncpy(g_starlink_gps.starlink_ip, config->starlink_ip, sizeof(g_starlink_gps.starlink_ip) - 1);
        g_starlink_gps.starlink_ip[sizeof(g_starlink_gps.starlink_ip) - 1] = '\0';
    }
    
    if (config->starlink_port > 0) {
        g_starlink_gps.starlink_port = config->starlink_port;
    }
    
    g_starlink_gps.enabled = config->enabled;
    // fallback_to_simulated removed - production uses only real data
    
    pthread_mutex_unlock(&g_starlink_gps_mutex);
    
    LOGX_INFO_MSG("Starlink GPS configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable Starlink GPS
int gps_starlink_set_enabled(bool enabled) {
    if (!g_starlink_gps_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_starlink_gps_mutex);
    g_starlink_gps.enabled = enabled;
    pthread_mutex_unlock(&g_starlink_gps_mutex);
    
    LOGX_INFO_MSG("Starlink GPS system %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Check if GPS data is recent
bool gps_starlink_is_data_recent(int max_age_seconds) {
    if (!g_starlink_gps_initialized) {
        return false;
    }
    
    pthread_mutex_lock(&g_starlink_gps_mutex);
    
    time_t now = time(NULL);
    bool recent = (g_starlink_gps.gps_data.timestamp > 0 && 
                   (now - g_starlink_gps.gps_data.timestamp) <= max_age_seconds);
    
    pthread_mutex_unlock(&g_starlink_gps_mutex);
    
    return recent;
}

// Check if GPS data meets accuracy requirements
bool gps_starlink_meets_accuracy(double required_accuracy) {
    if (!g_starlink_gps_initialized) {
        return false;
    }
    
    pthread_mutex_lock(&g_starlink_gps_mutex);
    
    bool meets_accuracy = (g_starlink_gps.gps_data.accuracy > 0 && 
                           g_starlink_gps.gps_data.accuracy <= required_accuracy);
    
    pthread_mutex_unlock(&g_starlink_gps_mutex);
    
    return meets_accuracy;
}

// Force immediate GPS update
int gps_starlink_force_update(void) {
    if (!g_starlink_gps_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    LOGX_INFO_MSG("Forcing immediate Starlink GPS update");
    return gps_starlink_extract_data();
}

// Cleanup Starlink GPS system
void gps_starlink_cleanup(void) {
    if (!g_starlink_gps_initialized) {
        return;
    }
    
    // Stop monitoring thread
    gps_starlink_stop_monitoring();
    
    pthread_mutex_lock(&g_starlink_gps_mutex);
    g_starlink_gps_initialized = false;
    pthread_mutex_unlock(&g_starlink_gps_mutex);
    
    pthread_mutex_destroy(&g_starlink_gps_mutex);
    
    LOGX_INFO_MSG("Starlink GPS system cleaned up");
}
