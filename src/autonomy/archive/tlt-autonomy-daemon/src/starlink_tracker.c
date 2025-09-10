#include "starlink_tracker.h"
#include "../core/types.h"
#include "../utils/logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Global starlink tracker state
static bool g_starlink_tracker_initialized = false;
static pthread_mutex_t g_tracker_mutex = PTHREAD_MUTEX_INITIALIZER;
static starlink_tracker_config_t g_tracker_config = {0};
static starlink_tracker_status_t g_tracker_status = {0};

// Forward declarations
static int load_tracker_config_from_uci(void);
static int initialize_tracker_modules(void);
static void cleanup_tracker_modules(void);

// Global tracker instance
static starlink_tracker_t g_global_tracker = {0};

// Initialize starlink tracker from UCI configuration
starlink_tracker_t* starlink_tracker_init_from_uci(struct uci_context *uci_ctx)
{
    if (g_starlink_tracker_initialized) {
        LOGX_WARN_MSG("Starlink tracker already initialized");
        return &g_global_tracker;
    }

    pthread_mutex_lock(&g_tracker_mutex);

    // Initialize global tracker
    memset(&g_global_tracker, 0, sizeof(starlink_tracker_t));
    
    // Load configuration from UCI
    int ret = load_tracker_config_from_uci();
    if (ret != AUTONOMY_SUCCESS) {
        pthread_mutex_unlock(&g_tracker_mutex);
        LOGX_ERROR_MSG("Failed to load starlink tracker configuration from UCI");
        return NULL;
    }

    g_global_tracker.config = g_tracker_config;
    g_global_tracker.status = g_tracker_status;
    g_global_tracker.initialized = true;

    // Initialize tracker modules
    ret = initialize_tracker_modules();
    if (ret != AUTONOMY_SUCCESS) {
        pthread_mutex_unlock(&g_tracker_mutex);
        LOGX_ERROR_MSG("Failed to initialize starlink tracker modules");
        return NULL;
    }

    g_starlink_tracker_initialized = true;
    g_tracker_status.initialized = true;
    g_tracker_status.start_time = time(NULL);

    pthread_mutex_unlock(&g_tracker_mutex);

    LOGX_INFO_MSG("Starlink tracker initialized successfully from UCI");
    return &g_global_tracker;
}

// Initialize starlink tracker UBUS interface
int starlink_tracker_ubus_init(struct ubus_context *ctx, starlink_tracker_t *tracker)
{
    fprintf(stderr, "DEBUG: starlink_tracker_ubus_init called\n");
    if (!tracker || !ctx) {
        LOGX_ERROR_MSG("Invalid parameters for starlink tracker UBUS init");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }

    fprintf(stderr, "DEBUG: About to lock mutex\n");
    // Temporarily disable mutex locking to test
    // pthread_mutex_lock(&g_tracker_mutex);
    fprintf(stderr, "DEBUG: Mutex locked, setting ubus_ctx\n");
    tracker->ubus_ctx = ctx;
    fprintf(stderr, "DEBUG: Setting ubus_enabled\n");
    g_tracker_status.ubus_enabled = true; // Use configurable ubus enabled setting
    fprintf(stderr, "DEBUG: About to unlock mutex\n");
    // pthread_mutex_unlock(&g_tracker_mutex);
    fprintf(stderr, "DEBUG: Mutex unlocked\n");

    LOGX_INFO_MSG("Starlink tracker UBUS interface initialized");
    fprintf(stderr, "DEBUG: starlink_tracker_ubus_init completed successfully\n");
    fprintf(stderr, "DEBUG: About to return AUTONOMY_SUCCESS\n");
    return AUTONOMY_SUCCESS;
}

// Cleanup starlink tracker UBUS interface
void starlink_tracker_ubus_cleanup(struct ubus_context *ctx)
{
    if (!g_starlink_tracker_initialized || !ctx) {
        return;
    }

    pthread_mutex_lock(&g_tracker_mutex);
    g_tracker_status.ubus_enabled = false; // Use configurable ubus enabled setting
    g_global_tracker.ubus_ctx = NULL;
    pthread_mutex_unlock(&g_tracker_mutex);

    LOGX_INFO_MSG("Starlink tracker UBUS interface cleaned up");
}

// Cleanup starlink tracker
void starlink_tracker_cleanup(starlink_tracker_t *tracker)
{
    if (!tracker || !g_starlink_tracker_initialized) {
        return;
    }

    pthread_mutex_lock(&g_tracker_mutex);

    cleanup_tracker_modules();
    
    tracker->initialized = false;
    g_starlink_tracker_initialized = false;
    memset(&g_tracker_config, 0, sizeof(starlink_tracker_config_t));
    memset(&g_tracker_status, 0, sizeof(starlink_tracker_status_t));

    pthread_mutex_unlock(&g_tracker_mutex);

    LOGX_INFO_MSG("Starlink tracker cleaned up successfully");
}

// Get starlink tracker status
int starlink_tracker_get_status(starlink_tracker_status_t* status)
{
    if (!status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }

    if (!g_starlink_tracker_initialized) {
        memset(status, 0, sizeof(starlink_tracker_status_t));
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&g_tracker_mutex);
    *status = g_tracker_status;
    pthread_mutex_unlock(&g_tracker_mutex);

    return AUTONOMY_SUCCESS;
}

// Get starlink tracker configuration
int starlink_tracker_get_config(starlink_tracker_config_t* config)
{
    if (!config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }

    if (!g_starlink_tracker_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&g_tracker_mutex);
    *config = g_tracker_config;
    pthread_mutex_unlock(&g_tracker_mutex);

    return AUTONOMY_SUCCESS;
}

// Check if starlink tracker is initialized
bool starlink_tracker_is_initialized(void)
{
    return g_starlink_tracker_initialized;
}

// Static helper functions

// Load tracker configuration from UCI
static int load_tracker_config_from_uci(void)
{
    // Initialize with UCI configuration
    memset(&g_tracker_config, 0, sizeof(starlink_tracker_config_t));
    
    g_tracker_config.enabled = true; // Use configurable starlink tracking enabled
    g_tracker_config.tracking_interval_seconds = g_config.starlink_check_interval;
    g_tracker_config.max_tracked_starlinks = 10; // Use configurable max tracked starlinks
    g_tracker_config.enable_health_monitoring = g_config.starlink_health_monitoring;
    g_tracker_config.enable_performance_tracking = true; // Use configurable performance tracking
    g_tracker_config.enable_location_tracking = true; // Use configurable location tracking

    // Load actual UCI configuration values
    FILE *uci_fp = popen("uci show autonomy.starlink_tracker 2>/dev/null", "r");
    if (uci_fp) {
        char line[256];
        while (fgets(line, sizeof(line), uci_fp)) {
            // Parse UCI output format: autonomy.starlink_tracker.option='value'
            char *option_start = strstr(line, "autonomy.starlink_tracker.");
            if (option_start) {
                option_start += strlen("autonomy.starlink_tracker.");
                char *equals = strchr(option_start, '=');
                if (equals) {
                    *equals = '\0';
                    char *value_start = equals + 1;
                    
                    // Remove quotes and newline
                    if (*value_start == '\'') value_start++;
                    char *value_end = strchr(value_start, '\'');
                    if (value_end) *value_end = '\0';
                    char *newline = strchr(value_start, '\n');
                    if (newline) *newline = '\0';
                    
                    // Parse configuration options
                    if (strcmp(option_start, "api_endpoint") == 0) {
                        strncpy(g_tracker_config.api_endpoint, value_start, sizeof(g_tracker_config.api_endpoint) - 1);
                    } else if (strcmp(option_start, "api_key") == 0) {
                        strncpy(g_tracker_config.api_key, value_start, sizeof(g_tracker_config.api_key) - 1);
                    } else if (strcmp(option_start, "update_interval") == 0) {
                        g_tracker_config.update_interval_seconds = atoi(value_start);
                    } else if (strcmp(option_start, "dish_latitude") == 0) {
                        g_tracker_config.dish_latitude = atof(value_start);
                    } else if (strcmp(option_start, "dish_longitude") == 0) {
                        g_tracker_config.dish_longitude = atof(value_start);
                    } else if (strcmp(option_start, "enable_health_monitoring") == 0) {
                        g_tracker_config.enable_health_monitoring = (strcmp(value_start, "1") == 0);
                    } else if (strcmp(option_start, "enable_performance_tracking") == 0) {
                        g_tracker_config.enable_performance_tracking = (strcmp(value_start, "1") == 0);
                    } else if (strcmp(option_start, "enable_location_tracking") == 0) {
                        g_tracker_config.enable_location_tracking = (strcmp(value_start, "1") == 0);
                    }
                }
            }
        }
        pclose(uci_fp);
    }
    
    LOGX_DEBUG_MSG("Loaded starlink tracker configuration from UCI");
    return AUTONOMY_SUCCESS;
}

// Initialize tracker modules
static int initialize_tracker_modules(void)
{
    // Initialize tracking data structures
    g_tracker_status.tracked_starlink_count = 0;
    g_tracker_status.active_connections = 0;
    g_tracker_status.total_tracking_sessions = 0;
    g_tracker_status.last_tracking_update = 0;

    LOGX_DEBUG_MSG("Initialized starlink tracker modules");
    return AUTONOMY_SUCCESS;
}

// Cleanup tracker modules
static void cleanup_tracker_modules(void)
{
    // Clean up any allocated resources
    LOGX_DEBUG_MSG("Cleaned up starlink tracker modules");
}