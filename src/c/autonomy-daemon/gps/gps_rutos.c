#include "gps_rutos.h"
#include "../utils/logx.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <pthread.h>

// RUTOS GPS file paths
static const char* RUTOS_GPS_FILES[] = {
    "/var/lib/autonomy/gps_data",           // GPS data file
    "/var/lib/autonomy/gps_status",         // GPS status file
    "/var/lib/autonomy/gps_satellites",     // Satellite information
    "/var/lib/autonomy/gps_accuracy"        // GPS accuracy data
};

// RUTOS GPS configuration
static const int RUTOS_GPS_UPDATE_INTERVAL = 1;  // 1 second
static const int RUTOS_GPS_TIMEOUT = 5;          // 5 seconds
static const float RUTOS_GPS_MIN_ACCURACY = 1.0f; // 1 meter minimum accuracy

// Global RUTOS GPS state
static gps_rutos_t g_rutos_gps = {0};
static pthread_mutex_t g_rutos_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_rutos_initialized = false;
static pthread_t g_rutos_thread = 0;
static bool g_rutos_thread_running = false;

// Forward declarations
static void* rutos_monitor_thread(void *arg);
static int read_rutos_gps_data(gps_data_t *data);
static bool validate_gps_data(const gps_data_t *data);
static float calculate_reliability_score(void);

// Initialize RUTOS GPS system
int gps_rutos_init(void) {
    if (g_rutos_initialized) {
        LOGX_WARN_MSG("RUTOS GPS already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_rutos_mutex);
    
    // Initialize RUTOS GPS state
    memset(&g_rutos_gps, 0, sizeof(gps_rutos_t));
    g_rutos_gps.enabled = true;
    g_rutos_gps.update_interval = RUTOS_GPS_UPDATE_INTERVAL;
    g_rutos_gps.timeout = RUTOS_GPS_TIMEOUT;
    g_rutos_gps.min_accuracy = RUTOS_GPS_MIN_ACCURACY;
    g_rutos_gps.last_update = 0;
    g_rutos_gps.total_updates = 0;
    g_rutos_gps.consecutive_failures = 0;
    g_rutos_gps.consecutive_successes = 0;
    
    // Initialize GPS data
    g_rutos_gps.gps_data.valid = false;
    g_rutos_gps.gps_data.latitude = 0.0;
    g_rutos_gps.gps_data.longitude = 0.0;
    g_rutos_gps.gps_data.altitude = 0.0;
    g_rutos_gps.gps_data.accuracy = 999.0f;
    g_rutos_gps.gps_data.speed = 0.0f;
    g_rutos_gps.gps_data.heading = 0.0f;
    g_rutos_gps.gps_data.timestamp = 0;
    g_rutos_gps.gps_data.satellites = 0;
    g_rutos_gps.gps_data.hdop = 999.0f;
    g_rutos_gps.gps_data.vdop = 999.0f;
    
    g_rutos_initialized = true;
    pthread_mutex_unlock(&g_rutos_mutex);
    
    LOGX_INFO_MSG("RUTOS GPS system initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Check if RUTOS GPS is initialized
bool gps_rutos_is_initialized(void) {
    return g_rutos_initialized;
}

// Start RUTOS GPS monitoring thread
int gps_rutos_start_monitoring(void) {
    if (!g_rutos_initialized) {
        LOGX_ERROR_MSG("RUTOS GPS not initialized");
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    if (g_rutos_thread_running) {
        LOGX_WARN_MSG("RUTOS GPS monitoring already running");
        return AUTONOMY_SUCCESS;
    }
    
    // Create monitoring thread
    int ret = pthread_create(&g_rutos_thread, NULL, rutos_monitor_thread, NULL);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to create RUTOS GPS monitoring thread");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    g_rutos_thread_running = true;
    LOGX_INFO_MSG("RUTOS GPS monitoring started");
    
    return AUTONOMY_SUCCESS;
}

// Stop RUTOS GPS monitoring
void gps_rutos_stop_monitoring(void) {
    if (!g_rutos_thread_running) {
        return;
    }
    
    g_rutos_thread_running = false;
    
    if (g_rutos_thread != 0) {
        pthread_join(g_rutos_thread, NULL);
        g_rutos_thread = 0;
    }
    
    LOGX_INFO_MSG("RUTOS GPS monitoring stopped");
}

// RUTOS GPS monitoring thread
static void* rutos_monitor_thread(void *arg) {
    (void)arg;
    
    LOGX_INFO_MSG("RUTOS GPS monitoring thread started");
    
    while (g_rutos_thread_running) {
        // Read GPS data
        gps_rutos_read_data();
        
        // Sleep for update interval
        for (int i = 0; i < g_rutos_gps.update_interval && g_rutos_thread_running; i++) {
            sleep(1);
        }
    }
    
    LOGX_INFO_MSG("RUTOS GPS monitoring thread stopped");
    return NULL;
}

// Read GPS data from RUTOS system
int gps_rutos_read_data(void) {
    if (!g_rutos_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_rutos_mutex);
    
    time_t now = time(NULL);
    
    // Check if it's time to update
    if (g_rutos_gps.last_update > 0 && 
        (now - g_rutos_gps.last_update) < g_rutos_gps.update_interval) {
        pthread_mutex_unlock(&g_rutos_mutex);
        return AUTONOMY_SUCCESS;
    }
    
    LOGX_DEBUG_MSG("Reading RUTOS GPS data");
    
    // Read GPS data file
    gps_data_t new_data;
    memset(&new_data, 0, sizeof(gps_data_t));
    
    int ret = read_rutos_gps_data(&new_data);
    if (ret == AUTONOMY_SUCCESS) {
        // Validate GPS data
        if (validate_gps_data(&new_data)) {
            // Update GPS data
            memcpy(&g_rutos_gps.gps_data, &new_data, sizeof(gps_data_t));
            g_rutos_gps.last_update = now;
            g_rutos_gps.total_updates++;
            g_rutos_gps.consecutive_successes++;
            g_rutos_gps.consecutive_failures = 0;
            
            // Calculate reliability score
            g_rutos_gps.reliability_score = calculate_reliability_score();
            
            LOGX_DEBUG_MSG("RUTOS GPS data updated",
                      "lat", new_data.latitude,
                      "lon", new_data.longitude,
                      "accuracy", new_data.accuracy,
                      "satellites", new_data.satellites);
        } else {
            LOGX_WARN_MSG("Invalid GPS data received from RUTOS");
            g_rutos_gps.consecutive_failures++;
            g_rutos_gps.consecutive_successes = 0;
        }
    } else {
        LOGX_WARN_MSG("Failed to read RUTOS GPS data");
        g_rutos_gps.consecutive_failures++;
        g_rutos_gps.consecutive_successes = 0;
    }
    
    pthread_mutex_unlock(&g_rutos_mutex);
    return AUTONOMY_SUCCESS;
}

// Read GPS data from RUTOS files
static int read_rutos_gps_data(gps_data_t *data) {
    if (!data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Read GPS data file
    FILE *fp = fopen(RUTOS_GPS_FILES[0], "r");
    if (!fp) {
        LOGX_DEBUG_MSG("RUTOS GPS data file not found", "path", RUTOS_GPS_FILES[0]);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    char line[256];
    bool data_found = false;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "GPS_DATA:", 9) == 0) {
            // Parse GPS data line: GPS_DATA:lat,lon,alt,acc,speed,heading,sats,hdop,vdop
            char *token = strtok(line + 9, ",");
            if (token) {
                data->latitude = atof(token);
                token = strtok(NULL, ",");
            }
            if (token) {
                data->longitude = atof(token);
                token = strtok(NULL, ",");
            }
            if (token) {
                data->altitude = atof(token);
                token = strtok(NULL, ",");
            }
            if (token) {
                data->accuracy = atof(token);
                token = strtok(NULL, ",");
            }
            if (token) {
                data->speed = atof(token);
                token = strtok(NULL, ",");
            }
            if (token) {
                data->heading = atof(token);
                token = strtok(NULL, ",");
            }
            if (token) {
                data->satellites = atoi(token);
                token = strtok(NULL, ",");
            }
            if (token) {
                data->hdop = atof(token);
                token = strtok(NULL, ",");
            }
            if (token) {
                data->vdop = atof(token);
            }
            
            data->timestamp = time(NULL);
            data->valid = true;
            data_found = true;
            break;
        }
    }
    
    fclose(fp);
    
    if (!data_found) {
        // Production system requires real GPS data - no fallback
        LOGX_ERROR_MSG("Failed to read RUTOS GPS data - no real data available");
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    return AUTONOMY_SUCCESS;
}

// Validate GPS data
static bool validate_gps_data(const gps_data_t *data) {
    if (!data || !data->valid) {
        return false;
    }
    
    // Check latitude range (-90 to 90)
    if (data->latitude < -90.0 || data->latitude > 90.0) {
        LOGX_DEBUG_MSG("Invalid latitude", "value", data->latitude);
        return false;
    }
    
    // Check longitude range (-180 to 180)
    if (data->longitude < -180.0 || data->longitude > 180.0) {
        LOGX_DEBUG_MSG("Invalid longitude", "value", data->longitude);
        return false;
    }
    
    // Check accuracy (must be positive and reasonable)
    if (data->accuracy < 0.0 || data->accuracy > 1000.0) {
        LOGX_DEBUG_MSG("Invalid accuracy", "value", data->accuracy);
        return false;
    }
    
    // Check satellite count (must be reasonable)
    if (data->satellites < 0 || data->satellites > 20) {
        LOGX_DEBUG_MSG("Invalid satellite count", "value", data->satellites);
        return false;
    }
    
    // Check HDOP and VDOP (must be positive and reasonable)
    if (data->hdop < 0.0 || data->hdop > 100.0) {
        LOGX_DEBUG_MSG("Invalid HDOP", "value", data->hdop);
        return false;
    }
    
    if (data->vdop < 0.0 || data->vdop > 100.0) {
        LOGX_DEBUG_MSG("Invalid VDOP", "value", data->vdop);
        return false;
    }
    
    return true;
}

// Calculate reliability score based on data quality
static float calculate_reliability_score(void) {
    float score = 100.0f;
    
    // Reduce score for poor accuracy
    if (g_rutos_gps.gps_data.accuracy > 10.0f) {
        score -= (g_rutos_gps.gps_data.accuracy - 10.0f) * 2.0f;
    }
    
    // Reduce score for few satellites
    if (g_rutos_gps.gps_data.satellites < 6) {
        score -= (6 - g_rutos_gps.gps_data.satellites) * 5.0f;
    }
    
    // Reduce score for poor DOP values
    if (g_rutos_gps.gps_data.hdop > 2.0f) {
        score -= (g_rutos_gps.gps_data.hdop - 2.0f) * 10.0f;
    }
    
    if (g_rutos_gps.gps_data.vdop > 3.0f) {
        score -= (g_rutos_gps.gps_data.vdop - 3.0f) * 10.0f;
    }
    
    // Reduce score for consecutive failures
    score -= g_rutos_gps.consecutive_failures * 5.0f;
    
    // Ensure score is within valid range
    if (score < 0.0f) score = 0.0f;
    if (score > 100.0f) score = 100.0f;
    
    return score;
}

// Get current GPS data
int gps_rutos_get_data(gps_data_t *data) {
    if (!g_rutos_initialized || !data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_rutos_mutex);
    memcpy(data, &g_rutos_gps.gps_data, sizeof(gps_data_t));
    pthread_mutex_unlock(&g_rutos_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get RUTOS GPS status
int gps_rutos_get_status(gps_rutos_status_t *status) {
    if (!g_rutos_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_rutos_mutex);
    
    status->enabled = g_rutos_gps.enabled;
    status->update_interval = g_rutos_gps.update_interval;
    status->timeout = g_rutos_gps.timeout;
    status->min_accuracy = g_rutos_gps.min_accuracy;
    status->last_update = g_rutos_gps.last_update;
    status->total_updates = g_rutos_gps.total_updates;
    status->consecutive_failures = g_rutos_gps.consecutive_failures;
    status->consecutive_successes = g_rutos_gps.consecutive_successes;
    status->reliability_score = g_rutos_gps.reliability_score;
    status->data_valid = g_rutos_gps.gps_data.valid;
    
    pthread_mutex_unlock(&g_rutos_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set RUTOS GPS configuration
int gps_rutos_set_config(const gps_rutos_config_t *config) {
    if (!g_rutos_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_rutos_mutex);
    
    if (config->update_interval > 0) {
        g_rutos_gps.update_interval = config->update_interval;
    }
    
    if (config->timeout > 0) {
        g_rutos_gps.timeout = config->timeout;
    }
    
    if (config->min_accuracy > 0) {
        g_rutos_gps.min_accuracy = config->min_accuracy;
    }
    
    g_rutos_gps.enabled = config->enabled;
    
    pthread_mutex_unlock(&g_rutos_mutex);
    
    LOGX_INFO_MSG("RUTOS GPS configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable RUTOS GPS
int gps_rutos_set_enabled(bool enabled) {
    if (!g_rutos_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_rutos_mutex);
    g_rutos_gps.enabled = enabled;
    pthread_mutex_unlock(&g_rutos_mutex);
    
    LOGX_INFO_MSG("RUTOS GPS state changed", "enabled", enabled ? "true" : "false");
    return AUTONOMY_SUCCESS;
}

// Check if RUTOS GPS data is recent
bool gps_rutos_is_data_recent(int max_age_seconds) {
    if (!g_rutos_initialized) {
        return false;
    }
    
    pthread_mutex_lock(&g_rutos_mutex);
    time_t now = time(NULL);
    bool recent = (g_rutos_gps.last_update > 0 && 
                   (now - g_rutos_gps.last_update) <= max_age_seconds);
    pthread_mutex_unlock(&g_rutos_mutex);
    
    return recent;
}

// Check if RUTOS GPS data meets accuracy requirements
bool gps_rutos_meets_accuracy(float required_accuracy) {
    if (!g_rutos_initialized) {
        return false;
    }
    
    pthread_mutex_lock(&g_rutos_mutex);
    bool meets = (g_rutos_gps.gps_data.valid && 
                  g_rutos_gps.gps_data.accuracy <= required_accuracy);
    pthread_mutex_unlock(&g_rutos_mutex);
    
    return meets;
}

// Cleanup RUTOS GPS system
void gps_rutos_cleanup(void) {
    if (!g_rutos_initialized) {
        return;
    }
    
    // Stop monitoring thread
    gps_rutos_stop_monitoring();
    
    pthread_mutex_lock(&g_rutos_mutex);
    g_rutos_initialized = false;
    pthread_mutex_unlock(&g_rutos_mutex);
    
    pthread_mutex_destroy(&g_rutos_mutex);
    
    LOGX_INFO_MSG("RUTOS GPS system cleaned up");
}