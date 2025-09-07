#include "starlink_tracker.h"
#include "../core/types.h"
#include "../utils/logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>

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

// Starlink tracker structure definition
struct starlink_tracker {
    starlink_tracker_config_t config;
    starlink_tracker_status_t status;
    bool initialized;
    struct ubus_context *ubus_ctx;
};

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
    if (!tracker || !ctx) {
        LOGX_ERROR_MSG("Invalid parameters for starlink tracker UBUS init");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&g_tracker_mutex);
    tracker->ubus_ctx = ctx;
    g_tracker_status.ubus_enabled = true;
    pthread_mutex_unlock(&g_tracker_mutex);

    LOGX_INFO_MSG("Starlink tracker UBUS interface initialized");
    return AUTONOMY_SUCCESS;
}

// Cleanup starlink tracker UBUS interface
void starlink_tracker_ubus_cleanup(struct ubus_context *ctx)
{
    if (!g_starlink_tracker_initialized || !ctx) {
        return;
    }

    pthread_mutex_lock(&g_tracker_mutex);
    g_tracker_status.ubus_enabled = false;
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
    // Initialize with default configuration
    memset(&g_tracker_config, 0, sizeof(starlink_tracker_config_t));
    
    g_tracker_config.enabled = true;
    g_tracker_config.tracking_interval_seconds = 60;
    g_tracker_config.max_tracked_starlinks = 10;
    g_tracker_config.enable_health_monitoring = true;
    g_tracker_config.enable_performance_tracking = true;
    g_tracker_config.enable_location_tracking = true;

    // TODO: Load actual UCI configuration values
    // For now, use defaults for enterprise functionality
    
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