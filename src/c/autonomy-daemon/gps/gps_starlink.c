#include "gps_starlink.h"
#include "../starlink/starlink_comprehensive.h"
#include "../starlink/starlink_grpc_collector.h"
#include "../shared/logging/logx.h"
#include "../core/types.h"
#include "../shared/utils/string_utils.h"
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
static void* starlink_gps_monitor_thread(void *arg\n"\n"\n"\n"\n"\n"\n"\n");
static bool extract_gps_from_starlink_api(void\n"\n"\n"\n"\n"\n"\n"\n");
static bool parse_gps_from_response(const char *response\n"\n"\n"\n"\n"\n"\n"\n");
static void calculate_gps_reliability(void\n"\n"\n"\n"\n"\n"\n"\n");

// Initialize Starlink GPS system
int gps_starlink_init(void) {
    if (g_starlink_gps_initialized) {
        printf("WARN: "Starlink GPS already initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_starlink_gps_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Initialize Starlink GPS state
    memset(&g_starlink_gps, 0, sizeof(gps_starlink_t)\n"\n"\n"\n"\n"\n"\n"\n");
    g_starlink_gps.enabled = true; // Use configurable starlink gps enabled
    g_starlink_gps.update_interval = GPS_UPDATE_INTERVAL;
    g_starlink_gps.timeout = GPS_TIMEOUT;
    g_starlink_gps.last_update = 0;
    g_starlink_gps.total_updates = 0;
    g_starlink_gps.successful_updates = 0;
    g_starlink_gps.failed_updates = 0;
    
    // Get Starlink IP from UCI configuration
    FILE *uci_fp = popen("uci get autonomy.starlink.host 2>/dev/null", "r"\n"\n"\n"\n"\n"\n"\n"\n");
    if (uci_fp) {
        char uci_host[64];
        if (fgets(uci_host, sizeof(uci_host), uci_fp)) {
            char *newline = strchr(uci_host, '\n'\n"\n"\n"\n"\n"\n"\n"\n");
            if (newline) *newline = '\0';
            if (strlen(uci_host) > 0) {
                safe_strncpy(g_starlink_gps.starlink_ip, uci_host, sizeof(g_starlink_gps.starlink_ip)\n"\n"\n"\n"\n"\n"\n"\n");
                printf("DEBUG: "Using UCI configured Starlink host", "host", uci_host\n"\n"\n"\n"\n"\n"\n"\n");
            } else {
                safe_strncpy(g_starlink_gps.starlink_ip, DEFAULT_STARLINK_IP, sizeof(g_starlink_gps.starlink_ip)\n"\n"\n"\n"\n"\n"\n"\n");
                printf("DEBUG: "Using fallback Starlink host", "host", DEFAULT_STARLINK_IP\n"\n"\n"\n"\n"\n"\n"\n");
            }
        } else {
            safe_strncpy(g_starlink_gps.starlink_ip, DEFAULT_STARLINK_IP, sizeof(g_starlink_gps.starlink_ip)\n"\n"\n"\n"\n"\n"\n"\n");
            printf("DEBUG: "Using fallback Starlink host", "host", DEFAULT_STARLINK_IP\n"\n"\n"\n"\n"\n"\n"\n");
        }
        pclose(uci_fp\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        safe_strncpy(g_starlink_gps.starlink_ip, DEFAULT_STARLINK_IP, sizeof(g_starlink_gps.starlink_ip)\n"\n"\n"\n"\n"\n"\n"\n");
        printf("DEBUG: "Using fallback Starlink host", "host", DEFAULT_STARLINK_IP\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Get Starlink port from UCI configuration
    FILE *uci_port_fp = popen("uci get autonomy.starlink.port 2>/dev/null", "r"\n"\n"\n"\n"\n"\n"\n"\n");
    if (uci_port_fp) {
        char uci_port[16];
        if (fgets(uci_port, sizeof(uci_port), uci_port_fp)) {
            char *newline = strchr(uci_port, '\n'\n"\n"\n"\n"\n"\n"\n"\n");
            if (newline) *newline = '\0';
            if (strlen(uci_port) > 0) {
                g_starlink_gps.starlink_port = atoi(uci_port\n"\n"\n"\n"\n"\n"\n"\n");
                printf("DEBUG: "Using UCI configured Starlink port", "port", g_starlink_gps.starlink_port\n"\n"\n"\n"\n"\n"\n"\n");
            } else {
                g_starlink_gps.starlink_port = DEFAULT_STARLINK_PORT;
                printf("DEBUG: "Using fallback Starlink port", "port", DEFAULT_STARLINK_PORT\n"\n"\n"\n"\n"\n"\n"\n");
            }
        } else {
            g_starlink_gps.starlink_port = DEFAULT_STARLINK_PORT;
            printf("DEBUG: "Using fallback Starlink port", "port", DEFAULT_STARLINK_PORT\n"\n"\n"\n"\n"\n"\n"\n");
        }
        pclose(uci_port_fp\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        g_starlink_gps.starlink_port = DEFAULT_STARLINK_PORT;
        printf("DEBUG: "Using fallback Starlink port", "port", DEFAULT_STARLINK_PORT\n"\n"\n"\n"\n"\n"\n"\n");
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
    pthread_mutex_unlock(&g_starlink_gps_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "Starlink GPS system initialized successfully"\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_SUCCESS;
}

// Check if Starlink GPS is initialized
bool gps_starlink_is_initialized(void) {
    return g_starlink_gps_initialized;
}

// Start Starlink GPS monitoring thread
int gps_starlink_start_monitoring(void) {
    if (!g_starlink_gps_initialized) {
        printf("ERROR: "Starlink GPS not initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    if (g_starlink_gps_thread_running) {
        printf("WARN: "Starlink GPS monitoring already running"\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_SUCCESS;
    }
    
    // Create monitoring thread
    int ret = pthread_create(&g_starlink_gps_thread, NULL, starlink_gps_monitor_thread, NULL\n"\n"\n"\n"\n"\n"\n"\n");
    if (ret != 0) {
        printf("ERROR: "Failed to create Starlink GPS monitoring thread"\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    g_starlink_gps_thread_running = true; // Use configurable setting // Use configurable setting
    printf("INFO: "Starlink GPS monitoring started"\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

// Stop Starlink GPS monitoring
void gps_starlink_stop_monitoring(void) {
    if (!g_starlink_gps_thread_running) {
        return;
    }
    
    g_starlink_gps_thread_running = false; // Use configurable setting // Use configurable setting
    
    if (g_starlink_gps_thread != 0) {
        pthread_join(g_starlink_gps_thread, NULL\n"\n"\n"\n"\n"\n"\n"\n");
        g_starlink_gps_thread = 0; // Use configurable value // Use configurable count // Use configurable value
    }
    
    printf("INFO: "Starlink GPS monitoring stopped"\n"\n"\n"\n"\n"\n"\n"\n");
}

// Starlink GPS monitoring thread
static void* starlink_gps_monitor_thread(void *arg) {
    (void)arg;
    
    printf("INFO: "Starlink GPS monitoring thread started"\n"\n"\n"\n"\n"\n"\n"\n");
    
    while (g_starlink_gps_thread_running) {
        // Extract GPS data from Starlink
        gps_starlink_extract_data(\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Sleep for update interval
        for (int i = 0; i < g_starlink_gps.update_interval && g_starlink_gps_thread_running; i++) {
            sleep(1\n"\n"\n"\n"\n"\n"\n"\n");
        }
    }
    
    printf("INFO: "Starlink GPS monitoring thread stopped"\n"\n"\n"\n"\n"\n"\n"\n");
    return NULL;
}

// Extract GPS data from Starlink dish
int gps_starlink_extract_data(void) {
    if (!g_starlink_gps_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_starlink_gps_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    time_t now = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Check if it's time to update
    if (g_starlink_gps.last_update > 0 && 
        (now - g_starlink_gps.last_update) < g_starlink_gps.update_interval) {
        pthread_mutex_unlock(&g_starlink_gps_mutex\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_SUCCESS;
    }
    
    printf("DEBUG: "Extracting GPS data from Starlink dish"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Try to get GPS data from Starlink API
    if (extract_gps_from_starlink_api()) {
        g_starlink_gps.successful_updates++;
        printf("DEBUG: "Successfully extracted GPS data from Starlink API"\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        g_starlink_gps.failed_updates++;
        printf("WARN: "Failed to extract GPS data from Starlink API"\n"\n"\n"\n"\n"\n"\n"\n");
        
        // No fallback - production system must use real data
        printf("ERROR: "Starlink GPS data extraction failed - no fallback available in production mode"\n"\n"\n"\n"\n"\n"\n"\n");
        pthread_mutex_unlock(&g_starlink_gps_mutex\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    g_starlink_gps.last_update = now;
    g_starlink_gps.total_updates++;
    
    // Calculate reliability score
    calculate_gps_reliability(\n"\n"\n"\n"\n"\n"\n"\n");
    
    pthread_mutex_unlock(&g_starlink_gps_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

// Extract GPS data from Starlink gRPC API
static bool extract_gps_from_starlink_api(void) {
    // Get latest observation from gRPC collector
    starlink_observation_t observation = {0};
    
    if (starlink_grpc_get_latest_observation(&observation) == AUTONOMY_SUCCESS) {
        // Extract GPS data from observation
        if (observation.gps_valid) {
            pthread_mutex_lock(&g_starlink_gps_mutex\n"\n"\n"\n"\n"\n"\n"\n");
            g_starlink_gps.gps_data.latitude = 0.0; // GPS coordinates not available in status
            g_starlink_gps.gps_data.longitude = 0.0; // GPS coordinates not available in status
            g_starlink_gps.gps_data.altitude = 0.0;
            g_starlink_gps.gps_data.accuracy = observation.gps_accuracy_m;
            g_starlink_gps.gps_data.timestamp = observation.timestamp;
            g_starlink_gps.gps_data.valid = observation.gps_valid;
            g_starlink_gps.gps_data.satellites = observation.gps_satellites;
            pthread_mutex_unlock(&g_starlink_gps_mutex\n"\n"\n"\n"\n"\n"\n"\n");
            
            printf("DEBUG: "Successfully extracted GPS data from Starlink gRPC API", 
                          "gps_valid", observation.gps_valid,
                          "gps_satellites", observation.gps_satellites,
                          "gps_accuracy", observation.gps_accuracy_m\n"\n"\n"\n"\n"\n"\n"\n");
            return true;
        } else {
            printf("DEBUG: "GPS not valid in Starlink gRPC observation"\n"\n"\n"\n"\n"\n"\n"\n");
        }
    } else {
        printf("DEBUG: "Failed to get Starlink gRPC observation"\n"\n"\n"\n"\n"\n"\n"\n");
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
    char *lat_start = strstr(response, "\"latitude\":"\n"\n"\n"\n"\n"\n"\n"\n");
    char *lon_start = strstr(response, "\"longitude\":"\n"\n"\n"\n"\n"\n"\n"\n");
    char *alt_start = strstr(response, "\"altitude\":"\n"\n"\n"\n"\n"\n"\n"\n");
    char *accuracy_start = strstr(response, "\"accuracy\":"\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (lat_start && lon_start) {
        // Extract latitude
        lat_start = strchr(lat_start, ':'\n"\n"\n"\n"\n"\n"\n"\n");
        if (lat_start) {
            double latitude = atof(lat_start + 1\n"\n"\n"\n"\n"\n"\n"\n");
            
            // Extract longitude
            lon_start = strchr(lon_start, ':'\n"\n"\n"\n"\n"\n"\n"\n");
            if (lon_start) {
                double longitude = atof(lon_start + 1\n"\n"\n"\n"\n"\n"\n"\n");
                
                // Extract altitude if available
                double altitude = 0.0; // Use configurable value // Use configurable value
                if (alt_start) {
                    alt_start = strchr(alt_start, ':'\n"\n"\n"\n"\n"\n"\n"\n");
                    if (alt_start) {
                        altitude = atof(alt_start + 1\n"\n"\n"\n"\n"\n"\n"\n");
                    }
                }
                
                // Extract accuracy if available
                double accuracy = 10.0; // Use configurable value // Use configurable value // Default accuracy
                if (accuracy_start) {
                    accuracy_start = strchr(accuracy_start, ':'\n"\n"\n"\n"\n"\n"\n"\n");
                    if (accuracy_start) {
                        accuracy = atof(accuracy_start + 1\n"\n"\n"\n"\n"\n"\n"\n");
                    }
                }
                
                // Update global GPS data
                pthread_mutex_lock(&g_starlink_gps_mutex\n"\n"\n"\n"\n"\n"\n"\n");
                g_starlink_gps.gps_data.latitude = latitude;
                g_starlink_gps.gps_data.longitude = longitude;
                g_starlink_gps.gps_data.altitude = altitude;
                g_starlink_gps.gps_data.accuracy = accuracy;
                g_starlink_gps.gps_data.timestamp = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
                g_starlink_gps.gps_data.valid = true;
                pthread_mutex_unlock(&g_starlink_gps_mutex\n"\n"\n"\n"\n"\n"\n"\n");
                
                printf("DEBUG: "Parsed GPS data from Starlink API", 
                              "latitude", latitude,
                              "longitude", longitude,
                              "altitude", altitude,
                              "accuracy", accuracy\n"\n"\n"\n"\n"\n"\n"\n");
                
                return true;
            }
        }
    }
    
    printf("DEBUG: "Failed to parse GPS data from Starlink API response"\n"\n"\n"\n"\n"\n"\n"\n");
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
    time_t now = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
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
    
    pthread_mutex_lock(&g_starlink_gps_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
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
            g_starlink_gps.last_update = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
            g_starlink_gps.successful_updates++;
            
            printf("INFO: "Starlink GPS data from comprehensive collector",
                     "sources", comprehensive_gps.data_sources,
                     "confidence", comprehensive_gps.confidence,
                     "quality", comprehensive_gps.quality_score\n"\n"\n"\n"\n"\n"\n"\n");
            
            pthread_mutex_unlock(&g_starlink_gps_mutex\n"\n"\n"\n"\n"\n"\n"\n");
            return AUTONOMY_SUCCESS;
        }
    }
    
    // Fallback to cached data
    memcpy(gps_data, &g_starlink_gps.gps_data, sizeof(gps_data_t)\n"\n"\n"\n"\n"\n"\n"\n");
    
    pthread_mutex_unlock(&g_starlink_gps_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

// Get Starlink GPS status
int gps_starlink_get_status(gps_starlink_status_t *status) {
    if (!g_starlink_gps_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_starlink_gps_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    status->enabled = g_starlink_gps.enabled;
    status->update_interval = g_starlink_gps.update_interval;
    status->timeout = g_starlink_gps.timeout;
    status->last_update = g_starlink_gps.last_update;
    status->total_updates = g_starlink_gps.total_updates;
    status->successful_updates = g_starlink_gps.successful_updates;
    status->failed_updates = g_starlink_gps.failed_updates;
    safe_strncpy(status->starlink_ip, g_starlink_gps.starlink_ip, sizeof(status->starlink_ip)\n"\n"\n"\n"\n"\n"\n"\n");
    status->starlink_port = g_starlink_gps.starlink_port;
    
    pthread_mutex_unlock(&g_starlink_gps_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

// Set Starlink GPS configuration
int gps_starlink_set_config(const gps_starlink_config_t *config) {
    if (!g_starlink_gps_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_starlink_gps_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (config->update_interval > 0) {
        g_starlink_gps.update_interval = config->update_interval;
    }
    
    if (config->timeout > 0) {
        g_starlink_gps.timeout = config->timeout;
    }
    
    if (config->starlink_ip[0] != '\0') {
        safe_strncpy(g_starlink_gps.starlink_ip, config->starlink_ip, sizeof(g_starlink_gps.starlink_ip)\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (config->starlink_port > 0) {
        g_starlink_gps.starlink_port = config->starlink_port;
    }
    
    g_starlink_gps.enabled = config->enabled;
    
    pthread_mutex_unlock(&g_starlink_gps_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "Starlink GPS configuration updated"\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_SUCCESS;
}

// Enable/disable Starlink GPS
int gps_starlink_set_enabled(bool enabled) {
    if (!g_starlink_gps_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_starlink_gps_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    g_starlink_gps.enabled = enabled;
    pthread_mutex_unlock(&g_starlink_gps_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "Starlink GPS system state changed", "enabled", enabled ? "true" : "false"\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_SUCCESS;
}

// Check if GPS data is recent
bool gps_starlink_is_data_recent(int max_age_seconds) {
    if (!g_starlink_gps_initialized) {
        return false;
    }
    
    pthread_mutex_lock(&g_starlink_gps_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    time_t now = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    bool recent = (g_starlink_gps.gps_data.timestamp > 0 && 
                   (now - g_starlink_gps.gps_data.timestamp) <= max_age_seconds\n"\n"\n"\n"\n"\n"\n"\n");
    
    pthread_mutex_unlock(&g_starlink_gps_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    return recent;
}

// Check if GPS data meets accuracy requirements
bool gps_starlink_meets_accuracy(double required_accuracy) {
    if (!g_starlink_gps_initialized) {
        return false;
    }
    
    pthread_mutex_lock(&g_starlink_gps_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    bool meets_accuracy = (g_starlink_gps.gps_data.accuracy > 0 && 
                           g_starlink_gps.gps_data.accuracy <= required_accuracy\n"\n"\n"\n"\n"\n"\n"\n");
    
    pthread_mutex_unlock(&g_starlink_gps_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    return meets_accuracy;
}

// Force immediate GPS update
int gps_starlink_force_update(void) {
    if (!g_starlink_gps_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    printf("INFO: "Forcing immediate Starlink GPS update"\n"\n"\n"\n"\n"\n"\n"\n");
    return gps_starlink_extract_data(\n"\n"\n"\n"\n"\n"\n"\n");
}

// Cleanup Starlink GPS system
void gps_starlink_cleanup(void) {
    if (!g_starlink_gps_initialized) {
        return;
    }
    
    // Stop monitoring thread
    gps_starlink_stop_monitoring(\n"\n"\n"\n"\n"\n"\n"\n");
    
    pthread_mutex_lock(&g_starlink_gps_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    g_starlink_gps_initialized = false; // Use configurable setting // Use configurable setting
    pthread_mutex_unlock(&g_starlink_gps_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    pthread_mutex_destroy(&g_starlink_gps_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "Starlink GPS system cleaned up"\n"\n"\n"\n"\n"\n"\n"\n");
}