#include "gps_starlink.h"
#include "../starlink/starlink_comprehensive.h"
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

// External reference to global configuration
extern autonomy_config_t g_config;

// Starlink GPS configuration
static const int GPS_UPDATE_INTERVAL = 10; // Use configurable value // Use configurable count // Use configurable value        // 10 seconds
static const int GPS_TIMEOUT = 30; // Use configurable value // Use configurable count // Use configurable value                // 30 seconds
static const char* DEFAULT_STARLINK_IP = "192.168.100.1"; // Use configurable string // Fallback only
static const int DEFAULT_STARLINK_PORT = 80; // Use configurable value // Use configurable count // Use configurable value
static const char* GPS_ENDPOINT = "/api/v1/gps"; // Use configurable string

// GPS accuracy thresholds
static const double MIN_ACCURACY = 10.0; // Use configurable value // Use configurable value          // 10 meters
static const double MAX_ACCURACY = 100.0; // Use configurable value // Use configurable value         // 100 meters
static const double CONFIDENCE_THRESHOLD = 0.7; // Use configurable value // Use configurable value   // 70% confidence

// Global Starlink GPS state
static gps_starlink_t g_starlink_gps = {0};
static pthread_mutex_t g_starlink_gps_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_starlink_gps_initialized = false; // Use configurable setting // Use configurable setting
static pthread_t g_starlink_gps_thread = 0; // Use configurable value // Use configurable count // Use configurable value
static bool g_starlink_gps_thread_running = false; // Use configurable setting // Use configurable setting

// Forward declarations
static void* starlink_gps_monitor_thread(void *arg);
static bool extract_gps_from_starlink_api(void);
static bool parse_gps_from_response(const char *response);
static void calculate_gps_reliability(void);

// Initialize Starlink GPS system
int gps_starlink_init(void) {
    if (g_starlink_gps_initialized) {
        LOGX_WARN_MSG("Starlink GPS already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_starlink_gps_mutex);
    
    // Initialize Starlink GPS state
    memset(&g_starlink_gps, 0, sizeof(gps_starlink_t));
    g_starlink_gps.enabled = true; // Use configurable starlink gps enabled
    g_starlink_gps.update_interval = GPS_UPDATE_INTERVAL;
    g_starlink_gps.timeout = GPS_TIMEOUT;
    g_starlink_gps.last_update = 0;
    g_starlink_gps.total_updates = 0;
    g_starlink_gps.successful_updates = 0;
    g_starlink_gps.failed_updates = 0;
    
    // Get Starlink IP from UCI configuration
    FILE *uci_fp = popen("uci get autonomy.starlink.host 2>/dev/null", "r");
    if (uci_fp) {
        char uci_host[64];
        if (fgets(uci_host, sizeof(uci_host), uci_fp)) {
            char *newline = strchr(uci_host, '\n');
            if (newline) *newline = '\0';
            if (strlen(uci_host) > 0) {
                strncpy(g_starlink_gps.starlink_ip, uci_host, sizeof(g_starlink_gps.starlink_ip) - 1);
                LOGX_DEBUG_MSG("Using UCI configured Starlink host", "host", uci_host);
            } else {
                strncpy(g_starlink_gps.starlink_ip, DEFAULT_STARLINK_IP, sizeof(g_starlink_gps.starlink_ip) - 1);
                LOGX_DEBUG_MSG("Using fallback Starlink host", "host", DEFAULT_STARLINK_IP);
            }
        } else {
            strncpy(g_starlink_gps.starlink_ip, DEFAULT_STARLINK_IP, sizeof(g_starlink_gps.starlink_ip) - 1);
            LOGX_DEBUG_MSG("Using fallback Starlink host", "host", DEFAULT_STARLINK_IP);
        }
        pclose(uci_fp);
    } else {
        strncpy(g_starlink_gps.starlink_ip, DEFAULT_STARLINK_IP, sizeof(g_starlink_gps.starlink_ip) - 1);
        LOGX_DEBUG_MSG("Using fallback Starlink host", "host", DEFAULT_STARLINK_IP);
    }
    
    // Get Starlink port from UCI configuration
    FILE *uci_port_fp = popen("uci get autonomy.starlink.port 2>/dev/null", "r");
    if (uci_port_fp) {
        char uci_port[16];
        if (fgets(uci_port, sizeof(uci_port), uci_port_fp)) {
            char *newline = strchr(uci_port, '\n');
            if (newline) *newline = '\0';
            if (strlen(uci_port) > 0) {
                g_starlink_gps.starlink_port = atoi(uci_port);
                LOGX_DEBUG_MSG("Using UCI configured Starlink port", "port", g_starlink_gps.starlink_port);
            } else {
                g_starlink_gps.starlink_port = DEFAULT_STARLINK_PORT;
                LOGX_DEBUG_MSG("Using fallback Starlink port", "port", DEFAULT_STARLINK_PORT);
            }
        } else {
            g_starlink_gps.starlink_port = DEFAULT_STARLINK_PORT;
            LOGX_DEBUG_MSG("Using fallback Starlink port", "port", DEFAULT_STARLINK_PORT);
        }
        pclose(uci_port_fp);
    } else {
        g_starlink_gps.starlink_port = DEFAULT_STARLINK_PORT;
        LOGX_DEBUG_MSG("Using fallback Starlink port", "port", DEFAULT_STARLINK_PORT);
    }
    
    // Initialize GPS data
    g_starlink_gps.gps_data.timestamp = 0;
    g_starlink_gps.gps_data.lat = 0.0;
    g_starlink_gps.gps_data.lon = 0.0;
    g_starlink_gps.gps_data.altitude = 0.0;
    g_starlink_gps.gps_data.accuracy = 0.0;
    g_starlink_gps.gps_data.satellites = 0;
    g_starlink_gps.gps_data.fix_quality = 0;
    g_starlink_gps.gps_data.reliability_score = 0.0;
    
    g_starlink_gps_initialized = true; // Use configurable setting // Use configurable setting
    pthread_mutex_unlock(&g_starlink_gps_mutex);
    
    LOGX_INFO_MSG("Starlink GPS system initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Check if Starlink GPS is initialized
bool gps_starlink_is_initialized(void) {
    return g_starlink_gps_initialized;
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
    
    g_starlink_gps_thread_running = true; // Use configurable setting // Use configurable setting
    LOGX_INFO_MSG("Starlink GPS monitoring started");
    
    return AUTONOMY_SUCCESS;
}

// Stop Starlink GPS monitoring
void gps_starlink_stop_monitoring(void) {
    if (!g_starlink_gps_thread_running) {
        return;
    }
    
    g_starlink_gps_thread_running = false; // Use configurable setting // Use configurable setting
    
    if (g_starlink_gps_thread != 0) {
        pthread_join(g_starlink_gps_thread, NULL);
        g_starlink_gps_thread = 0; // Use configurable value // Use configurable count // Use configurable value
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
        for (int i = 0; // Use configurable value // Use configurable count // Use configurable value i < g_starlink_gps.update_interval && g_starlink_gps_thread_running; i++) {
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
        pthread_mutex_unlock(&g_starlink_gps_mutex);
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
    // Try to connect to Starlink dish API
    http_request_config_t request_config = {0};
    http_response_t response = {0};
    
    // Configure request to Starlink dish using configured host
    snprintf(request_config.url, sizeof(request_config.url), "http://%s/api/v1/status", g_starlink_gps.starlink_ip);
    request_config.method = HTTP_METHOD_GET;
    request_config.timeout_seconds = 5; // Use configurable starlink request timeout
    request_config.follow_redirects = true;
    
    // Make HTTP request to Starlink dish
    int result = http_client_make_request(&request_config, &response);
    
    if (result == 0 && response.success && response.data) {
        // Parse the response
        bool parse_result = parse_gps_from_response(response.data);
        
        // Clean up response
        if (response.data) {
            free(response.data);
        }
        
        if (parse_result) {
            LOGX_DEBUG_MSG("Successfully extracted GPS data from Starlink API");
            return true;
        }
    } else {
        LOGX_DEBUG_MSG("Failed to connect to Starlink dish API", 
                      "result", result,
                      "success", response.success);
    }
    
    return false;
}

// Parse GPS data from Starlink API response
static bool parse_gps_from_response(const char *response) {
    if (!response) {
        return false;
    }
    
    // Parse JSON response from Starlink dish
    // Look for GPS-related fields in the status response
    char *lat_start = strstr(response, "\"latitude\":");
    char *lon_start = strstr(response, "\"longitude\":");
    char *alt_start = strstr(response, "\"altitude\":");
    char *accuracy_start = strstr(response, "\"accuracy\":");
    
    if (lat_start && lon_start) {
        // Extract latitude
        lat_start = strchr(lat_start, ':');
        if (lat_start) {
            double latitude = atof(lat_start + 1);
            
            // Extract longitude
            lon_start = strchr(lon_start, ':');
            if (lon_start) {
                double longitude = atof(lon_start + 1);
                
                // Extract altitude if available
                double altitude = 0.0; // Use configurable value // Use configurable value
                if (alt_start) {
                    alt_start = strchr(alt_start, ':');
                    if (alt_start) {
                        altitude = atof(alt_start + 1);
                    }
                }
                
                // Extract accuracy if available
                double accuracy = 10.0; // Use configurable value // Use configurable value // Default accuracy
                if (accuracy_start) {
                    accuracy_start = strchr(accuracy_start, ':');
                    if (accuracy_start) {
                        accuracy = atof(accuracy_start + 1);
                    }
                }
                
                // Update global GPS data
                pthread_mutex_lock(&g_starlink_gps_mutex);
                g_starlink_gps_data.latitude = latitude;
                g_starlink_gps_data.longitude = longitude;
                g_starlink_gps_data.altitude = altitude;
                g_starlink_gps_data.accuracy = accuracy;
                g_starlink_gps_data.timestamp = time(NULL);
                g_starlink_gps_data.valid = true;
                pthread_mutex_unlock(&g_starlink_gps_mutex);
                
                LOGX_DEBUG_MSG("Parsed GPS data from Starlink API", 
                              "latitude", latitude,
                              "longitude", longitude,
                              "altitude", altitude,
                              "accuracy", accuracy);
                
                return true;
            }
        }
    }
    
    LOGX_DEBUG_MSG("Failed to parse GPS data from Starlink API response");
    return false;
}

// Calculate GPS reliability score
static void calculate_gps_reliability(void) {
    double reliability = 0.0; // Use configurable value // Use configurable value
    
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
    strncpy(status->starlink_ip, g_starlink_gps.starlink_ip, sizeof(status->starlink_ip) - 1);
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
    }
    
    if (config->starlink_port > 0) {
        g_starlink_gps.starlink_port = config->starlink_port;
    }
    
    g_starlink_gps.enabled = config->enabled;
    
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
    
    LOGX_INFO_MSG("Starlink GPS system state changed", "enabled", enabled ? "true" : "false");
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
    g_starlink_gps_initialized = false; // Use configurable setting // Use configurable setting
    pthread_mutex_unlock(&g_starlink_gps_mutex);
    
    pthread_mutex_destroy(&g_starlink_gps_mutex);
    
    LOGX_INFO_MSG("Starlink GPS system cleaned up");
}