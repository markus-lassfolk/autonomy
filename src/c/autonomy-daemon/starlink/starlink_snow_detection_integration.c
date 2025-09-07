#include "starlink_snow_detection_integration.h"
#include "starlink_snow_detection.h"
#include "starlink_snow_detection_ubus.h"
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

// Integration configuration
static const int INTEGRATION_CHECK_INTERVAL = 30;  // Check every 30 seconds
static const int INTEGRATION_SAMPLE_INTERVAL = 5;  // Sample every 5 seconds
static const int INTEGRATION_MAX_RETRIES = 3;      // Maximum retry attempts

// Global integration state
static starlink_snow_detection_integration_t g_integration = {0};
static bool g_integration_initialized = false;
static pthread_mutex_t g_integration_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t g_integration_thread;
static bool g_integration_running = false;

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
    g_integration.enabled = true;
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
    
    g_integration_initialized = true;
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
        return AUTONOMY_ERROR_THREAD_CREATION;
    }
    
    g_integration_running = true;
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
    
    g_integration_running = false;
    
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
    
    // This would integrate with the existing Starlink obstruction system
    // For now, we'll simulate getting data from the Starlink API
    
    // Get Starlink host from UCI configuration
    char starlink_host[64] = "192.168.100.1"; // Default fallback
    FILE *uci_fp = popen("uci get autonomy.starlink.host 2>/dev/null", "r");
    if (uci_fp) {
        char uci_host[64];
        if (fgets(uci_host, sizeof(uci_host), uci_fp)) {
            // Remove newline
            char *newline = strchr(uci_host, '\n');
            if (newline) *newline = '\0';
            if (strlen(uci_host) > 0) {
                strncpy(starlink_host, uci_host, sizeof(starlink_host) - 1);
                starlink_host[sizeof(starlink_host) - 1] = '\0';
            }
        }
        pclose(uci_fp);
    }
    
    // Use HTTP client instead of curl system command for security
    char starlink_url[256];
    snprintf(starlink_url, sizeof(starlink_url), "http://%s/api/v1/status", starlink_host);
    
    http_request_t* request = http_request_create(starlink_url, HTTP_METHOD_GET);
    if (!request) {
        return AUTONOMY_ERROR_OPERATION_FAILED;
    }
    
    http_response_t* response = http_request(request);
    if (!response || !response->success || !response->data) {
        if (response) http_response_free(response);
        http_request_free(request);
        return AUTONOMY_ERROR_OPERATION_FAILED;
    }
    
    // Copy response data for processing
    char response_data[4096];
    strncpy(response_data, response->data, sizeof(response_data) - 1);
    response_data[sizeof(response_data) - 1] = '\0';
    
    http_response_free(response);
    http_request_free(request);
    
    // Parse JSON response using json-c
    sample->timestamp = time(NULL);
    sample->fraction_obstructed = 0.0;
    sample->snr = 0.0;

    json_object *root = json_tokener_parse(response_data);
    if (!root) {
        return AUTONOMY_ERROR_OPERATION_FAILED;
    }

    json_object *obstruction_obj;
    if (json_object_object_get_ex(root, "fractionObstructed", &obstruction_obj)) {
        sample->fraction_obstructed = json_object_get_double(obstruction_obj);
    }

    json_object *snr_obj;
    if (json_object_object_get_ex(root, "snr", &snr_obj)) {
        sample->snr = json_object_get_double(snr_obj);
    }

    json_object_put(root);
    
    return AUTONOMY_SUCCESS;
}

// Get obstruction sample from GPS
static int get_obstruction_sample_from_gps(starlink_obstruction_sample_t *sample) {
    if (!sample) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // This would integrate with the existing GPS obstruction system
    // For now, we'll simulate getting data from GPS
    
    sample->timestamp = time(NULL);
    sample->fraction_obstructed = 0.0; // GPS doesn't directly measure obstruction
    sample->snr = 0.0; // Would need to calculate from GPS signal quality
    
    // In a real implementation, this would:
    // 1. Get GPS signal quality data
    // 2. Calculate obstruction based on signal degradation
    // 3. Estimate SNR from signal strength
    
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
    g_integration_initialized = false;
    
    LOGX_INFO_MSG("Snow detection integration cleaned up");
}
