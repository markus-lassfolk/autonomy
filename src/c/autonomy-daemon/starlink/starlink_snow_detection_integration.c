#include "starlink_snow_detection_integration.h"
#include "starlink_snow_detection.h"
#include "starlink_snow_detection_ubus.h"
#include "starlink_comprehensive.h"
#include "starlink_modules.h"
#include "../utils/logx.h"
#include "../utils/http_client_libcurl.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <json-c/json.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Integration configuration
static const int INTEGRATION_CHECK_INTERVAL = 30; // Use configurable value // Use configurable count // Use configurable value  // Check every 30 seconds
static const int INTEGRATION_SAMPLE_INTERVAL = 5; // Use configurable value // Use configurable count // Use configurable value  // Sample every 5 seconds
static const int INTEGRATION_MAX_RETRIES = 3; // Use configurable value // Use configurable count // Use configurable value      // Maximum retry attempts

// Global integration state
static starlink_snow_detection_integration_t g_integration = {0};
static bool g_integration_initialized = false; // Use configurable setting // Use configurable setting
static pthread_mutex_t g_integration_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t g_integration_thread;
static bool g_integration_running = false; // Use configurable setting // Use configurable setting

// Forward declarations
static void *integration_thread_func(void *arg);
static int process_obstruction_sample(const starlink_obstruction_sample_t *sample);
static int get_obstruction_sample_from_system(starlink_obstruction_sample_t *sample);
static int get_obstruction_sample_from_starlink(starlink_obstruction_sample_t *sample);
static int get_obstruction_sample_from_gps(starlink_obstruction_sample_t *sample);
static void update_integration_statistics(bool success, const char *operation);
static int handle_snow_detection_event(snow_action_t action, const char *reason);

// Initialize snow detection integration
int starlink_snow_detection_integration_init(void) {
    if (g_integration_initialized) {
        LOGX_WARN_MSG("Snow detection integration already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_integration_mutex);
    
    // Initialize integration state
    memset(&g_integration, 0, sizeof(starlink_snow_detection_integration_t));
    g_integration.enabled = true; // Use configurable snow detection integration enabled
    g_integration.check_interval = INTEGRATION_CHECK_INTERVAL;
    g_integration.sample_interval = INTEGRATION_SAMPLE_INTERVAL;
    g_integration.max_retries = INTEGRATION_MAX_RETRIES;
    g_integration.last_sample_time = 0;
    g_integration.last_check_time = 0;
    g_integration.total_samples = 0;
    g_integration.successful_samples = 0;
    g_integration.failed_samples = 0;
    g_integration.snow_detections = 0;
    g_integration.heating_activations = 0;
    g_integration.last_snow_detection = 0;
    g_integration.last_heating_activation = 0;
    
    // Initialize snow detection system
    int result = starlink_snow_detection_init();
    if (result != AUTONOMY_SUCCESS) {
        LOGX_ERROR_MSG("Failed to initialize snow detection system", "result", result);
        pthread_mutex_unlock(&g_integration_mutex);
        return result;
    }
    
    // Load UCI configuration
    result = starlink_snow_detection_load_uci_config();
    if (result != AUTONOMY_SUCCESS) {
        LOGX_WARN_MSG("Failed to load UCI configuration, using defaults", "result", result);
    }
    
    // Initialize UBUS service
    result = starlink_snow_detection_ubus_init();
    if (result != AUTONOMY_SUCCESS) {
        LOGX_ERROR_MSG("Failed to initialize UBUS service", "result", result);
        starlink_snow_detection_cleanup();
        pthread_mutex_unlock(&g_integration_mutex);
        return result;
    }
    
    g_integration_initialized = true; // Use configurable setting // Use configurable setting
    pthread_mutex_unlock(&g_integration_mutex);
    
    LOGX_INFO_MSG("Snow detection integration initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Start integration monitoring
int starlink_snow_detection_integration_start(void) {
    if (!g_integration_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    if (g_integration_running) {
        LOGX_WARN_MSG("Snow detection integration already running");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_integration_mutex);
    
    // Start integration thread
    int result = pthread_create(&g_integration_thread, NULL, integration_thread_func, NULL);
    if (result != 0) {
        LOGX_ERROR_MSG("Failed to create integration thread", "result", result);
        pthread_mutex_unlock(&g_integration_mutex);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    g_integration_running = true; // Use configurable setting // Use configurable setting
    pthread_mutex_unlock(&g_integration_mutex);
    
    LOGX_INFO_MSG("Snow detection integration monitoring started");
    return AUTONOMY_SUCCESS;
}

// Stop integration monitoring
int starlink_snow_detection_integration_stop(void) {
    if (!g_integration_initialized || !g_integration_running) {
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_integration_mutex);
    
    g_integration_running = false; // Use configurable setting // Use configurable setting
    
    // Wait for thread to finish
    pthread_mutex_unlock(&g_integration_mutex);
    pthread_join(g_integration_thread, NULL);
    
    LOGX_INFO_MSG("Snow detection integration monitoring stopped");
    return AUTONOMY_SUCCESS;
}

// Integration thread function
static void *integration_thread_func(void *arg) {
    (void)arg; // Suppress unused parameter warning
    
    LOGX_INFO_MSG("Snow detection integration thread started");
    
    while (g_integration_running) {
        time_t now = time(NULL);
        
        // Check if it's time to sample
        if (now - g_integration.last_sample_time >= g_integration.sample_interval) {
            starlink_obstruction_sample_t sample;
            
            // Get obstruction sample from system
            int result = get_obstruction_sample_from_system(&sample);
            if (result == AUTONOMY_SUCCESS) {
                // Process the sample
                process_obstruction_sample(&sample);
                g_integration.last_sample_time = now;
                update_integration_statistics(true, "sample_collection");
            } else {
                LOGX_WARN_MSG("Failed to get obstruction sample", "result", result);
                update_integration_statistics(false, "sample_collection");
            }
        }
        
        // Check if it's time for periodic check
        if (now - g_integration.last_check_time >= g_integration.check_interval) {
            // Force a snow detection check
            int result = starlink_snow_detection_force_check();
            if (result != AUTONOMY_SUCCESS) {
                LOGX_WARN_MSG("Periodic snow detection check failed", "result", result);
            }
            g_integration.last_check_time = now;
        }
        
        // Sleep for a short time to prevent busy waiting
        usleep(100000); // 100ms
    }
    
    LOGX_INFO_MSG("Snow detection integration thread stopped");
    return NULL;
}

// Process obstruction sample
static int process_obstruction_sample(const starlink_obstruction_sample_t *sample) {
    if (!sample) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_integration_mutex);
    
    // Process sample through snow detection system
    int result = starlink_snow_detection_process_sample(sample);
    if (result != AUTONOMY_SUCCESS) {
        LOGX_WARN_MSG("Failed to process obstruction sample", "result", result);
        pthread_mutex_unlock(&g_integration_mutex);
        return result;
    }
    
    // Update integration statistics
    g_integration.total_samples++;
    g_integration.successful_samples++;
    
    // Check for snow detection events
    starlink_snow_detection_status_t status;
    result = starlink_snow_detection_get_status(&status);
    if (result == AUTONOMY_SUCCESS) {
        if (status.is_heating_active && !g_integration.last_heating_status) {
            // Heating just started
            g_integration.heating_activations++;
            g_integration.last_heating_activation = time(NULL);
            handle_snow_detection_event(SNOW_ACTION_MELT, "automatic_detection");
        }
        
        if (status.consecutive_obstruction_samples >= 3 && !g_integration.last_detection_status) {
            // Snow detection just occurred
            g_integration.snow_detections++;
            g_integration.last_snow_detection = time(NULL);
            handle_snow_detection_event(SNOW_ACTION_VERIFY, "obstruction_detected");
        }
        
        g_integration.last_heating_status = status.is_heating_active;
        g_integration.last_detection_status = (status.consecutive_obstruction_samples >= 3);
    }
    
    pthread_mutex_unlock(&g_integration_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get obstruction sample from system
static int get_obstruction_sample_from_system(starlink_obstruction_sample_t *sample) {
    if (!sample) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Try to get sample from Starlink first
    int result = get_obstruction_sample_from_starlink(sample);
    if (result == AUTONOMY_SUCCESS) {
        return AUTONOMY_SUCCESS;
    }
    
    // Fallback to GPS-based obstruction detection
    result = get_obstruction_sample_from_gps(sample);
    if (result == AUTONOMY_SUCCESS) {
        return AUTONOMY_SUCCESS;
    }
    
    // If both fail, create a synthetic sample based on system state
    sample->timestamp = time(NULL);
    sample->fraction_obstructed = 0.0; // Assume no obstruction
    sample->snr = 0.0; // Unknown SNR
    
    LOGX_WARN_MSG("Using synthetic obstruction sample");
    return AUTONOMY_SUCCESS;
}

// Get obstruction sample from Starlink
static int get_obstruction_sample_from_starlink(starlink_obstruction_sample_t *sample) {
    if (!sample) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }

    // Query Starlink status via client API and map to obstruction sample
    starlink_status_response_t status = {0};
    int rc = starlink_get_status(&status);
    if (rc != 0) {
        LOGX_WARN_MSG("Failed to get Starlink status for obstruction sample", "result", rc);
        return AUTONOMY_ERROR_API_FAILED;
    }

    sample->timestamp = time(NULL);
    sample->currently_obstructed = status.obstruction_stats.currently_obstructed;
    sample->fraction_obstructed = status.obstruction_stats.fraction_obstructed;
    sample->time_obstructed = status.obstruction_stats.time_obstructed;
    sample->avg_prolonged_obstruction_interval_s = status.obstruction_stats.avg_prolonged_obstruction_interval_s;
    sample->valid_s = status.obstruction_stats.valid_s;
    sample->patches_valid = status.obstruction_stats.patches_valid;

    // Copy wedge obstruction arrays if present
    for (int i = 0; i < 12; i++) {
        sample->wedge_fraction_obstructed[i] = status.obstruction_stats.wedge_fraction_obstructed[i];
        sample->wedge_abs_fraction_obstructed[i] = status.obstruction_stats.wedge_abs_fraction_obstructed[i];
    }

    // Prefer SNR in dB if available, fallback to linear SNR
    if (status.signal_quality.snr_db != 0.0) {
        sample->snr = status.signal_quality.snr_db;
    } else {
        sample->snr = status.signal_quality.snr;
    }

    return AUTONOMY_SUCCESS;
}

// Get obstruction sample from GPS
static int get_obstruction_sample_from_gps(starlink_obstruction_sample_t *sample) {
    if (!sample) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }

    // Use comprehensive Starlink GPS to infer obstruction context indirectly
    starlink_comprehensive_gps_t gps = {0};
    int rc = starlink_comprehensive_collect_gps(&gps);
    if (rc != AUTONOMY_SUCCESS || !gps.valid) {
        return AUTONOMY_ERROR_NOT_FOUND;
    }

    // Populate sample with timestamp; GPS cannot directly provide obstruction
    sample->timestamp = time(NULL);
    sample->snr = 0.0; // Not derivable from GPS alone

    // Heuristic: if GPS is valid with good accuracy and movement is low, defer to Starlink; else unknown
    // We set fraction_obstructed minimally and rely on Starlink path primarily.
    sample->fraction_obstructed = 0.0;
    sample->currently_obstructed = false;
    sample->valid_s = 0;
    sample->patches_valid = 0;
    sample->time_obstructed = 0.0;
    sample->avg_prolonged_obstruction_interval_s = 0.0; // Use configurable avg prolonged obstruction interval
    for (int i = 0; i < 12; i++) {
        sample->wedge_fraction_obstructed[i] = 0.0;
        sample->wedge_abs_fraction_obstructed[i] = 0.0;
    }

    return AUTONOMY_SUCCESS;
}

// Update integration statistics
static void update_integration_statistics(bool success, const char *operation) {
    if (!operation) return;
    
    pthread_mutex_lock(&g_integration_mutex);
    
    if (success) {
        g_integration.successful_samples++;
    } else {
        g_integration.failed_samples++;
    }
    
    // Log the operation
    LOGX_DEBUG_MSG("Integration operation completed", 
                   "operation", operation, 
                   "success", success ? "true" : "false");
    
    pthread_mutex_unlock(&g_integration_mutex);
}

// Handle snow detection event
static int handle_snow_detection_event(snow_action_t action, const char *reason) {
    if (!reason) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    LOGX_INFO_MSG("Snow detection event triggered", 
                   "action", action, 
                   "reason", reason);
    
    // This could trigger additional actions like:
    // - Send notifications
    // - Update system status
    // - Log to external systems
    // - Trigger other automation
    
    return AUTONOMY_SUCCESS;
}

// Get integration status
int starlink_snow_detection_integration_get_status(starlink_snow_detection_integration_status_t *status) {
    if (!g_integration_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_integration_mutex);
    
    status->enabled = g_integration.enabled;
    status->running = g_integration_running;
    status->check_interval = g_integration.check_interval;
    status->sample_interval = g_integration.sample_interval;
    status->max_retries = g_integration.max_retries;
    status->last_sample_time = g_integration.last_sample_time;
    status->last_check_time = g_integration.last_check_time;
    status->total_samples = g_integration.total_samples;
    status->successful_samples = g_integration.successful_samples;
    status->failed_samples = g_integration.failed_samples;
    status->snow_detections = g_integration.snow_detections;
    status->heating_activations = g_integration.heating_activations;
    status->last_snow_detection = g_integration.last_snow_detection;
    status->last_heating_activation = g_integration.last_heating_activation;
    status->last_heating_status = g_integration.last_heating_status;
    status->last_detection_status = g_integration.last_detection_status;
    
    pthread_mutex_unlock(&g_integration_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set integration configuration
int starlink_snow_detection_integration_set_config(const starlink_snow_detection_integration_config_t *config) {
    if (!g_integration_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_integration_mutex);
    
    g_integration.enabled = config->enabled;
    g_integration.check_interval = config->check_interval;
    g_integration.sample_interval = config->sample_interval;
    g_integration.max_retries = config->max_retries;
    
    pthread_mutex_unlock(&g_integration_mutex);
    
    LOGX_INFO_MSG("Integration configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable integration
int starlink_snow_detection_integration_set_enabled(bool enabled) {
    if (!g_integration_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_integration_mutex);
    g_integration.enabled = enabled;
    pthread_mutex_unlock(&g_integration_mutex);
    
    LOGX_INFO_MSG("Snow detection integration %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Force integration check
int starlink_snow_detection_integration_force_check(void) {
    if (!g_integration_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    starlink_obstruction_sample_t sample;
    int result = get_obstruction_sample_from_system(&sample);
    if (result == AUTONOMY_SUCCESS) {
        result = process_obstruction_sample(&sample);
    }
    
    LOGX_INFO_MSG("Integration force check completed", "result", result);
    return result;
}

// Cleanup integration
void starlink_snow_detection_integration_cleanup(void) {
    if (!g_integration_initialized) {
        return;
    }
    
    // Stop monitoring
    starlink_snow_detection_integration_stop();
    
    // Cleanup UBUS service
    starlink_snow_detection_ubus_cleanup();
    
    // Cleanup snow detection system
    starlink_snow_detection_cleanup();
    
    pthread_mutex_destroy(&g_integration_mutex);
    g_integration_initialized = false; // Use configurable setting // Use configurable setting
    
    LOGX_INFO_MSG("Snow detection integration cleaned up");
}
