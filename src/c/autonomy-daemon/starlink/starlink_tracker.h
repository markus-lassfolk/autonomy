#ifndef STARLINK_TRACKER_H
#define STARLINK_TRACKER_H

#include "../core/types.h"
#include <stdbool.h>
#include <time.h>
#include <uci.h>

#ifdef __cplusplus
extern "C" {
#endif

// Starlink tracker configuration
typedef struct {
    bool enabled;
    int tracking_interval_seconds;
    int max_tracked_starlinks;
    bool enable_health_monitoring;
    bool enable_performance_tracking;
    bool enable_location_tracking;
    
    // Additional configuration fields
    char api_endpoint[256];
    char api_key[128];
    int update_interval_seconds;
    double dish_latitude;
    double dish_longitude;
} starlink_tracker_config_t;

// Starlink tracker status
typedef struct {
    bool initialized;
    bool ubus_enabled;
    int tracked_starlink_count;
    int active_connections;
    int total_tracking_sessions;
    time_t start_time;
    time_t last_tracking_update;
} starlink_tracker_status_t;

// Function prototypes

// Starlink tracker structure
typedef struct starlink_tracker {
    starlink_tracker_config_t config;
    starlink_tracker_status_t status;
    bool initialized;
    struct ubus_context *ubus_ctx;
} starlink_tracker_t;

// Initialize starlink tracker from UCI configuration
starlink_tracker_t* starlink_tracker_init_from_uci(struct uci_context *uci_ctx);

// Initialize starlink tracker UBUS interface
int starlink_tracker_ubus_init(struct ubus_context *ctx, starlink_tracker_t *tracker);

// Cleanup starlink tracker UBUS interface
void starlink_tracker_ubus_cleanup(struct ubus_context *ctx);

// Cleanup starlink tracker
void starlink_tracker_cleanup(starlink_tracker_t *tracker);

// Get starlink tracker status
int starlink_tracker_get_status(starlink_tracker_status_t* status);

// Get starlink tracker configuration
int starlink_tracker_get_config(starlink_tracker_config_t* config);

// Check if starlink tracker is initialized
bool starlink_tracker_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif // STARLINK_TRACKER_H