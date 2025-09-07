#ifndef STARLINK_TRACKER_H
#define STARLINK_TRACKER_H

#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include "starlink_types.h"

// Forward declarations for UCI and UBUS types
struct uci_context;
struct ubus_context;

// Starlink tracker configuration
typedef struct {
    char space_track_username[64];
    char space_track_password[64];
    char tle_file_path[256];
    int update_interval_minutes;
    bool predictive_enabled;
    double prediction_horizon_hours;
} starlink_tracker_config_t;

// Starlink tracker state
typedef struct {
    bool initialized;
    time_t last_tle_update;
    time_t last_prediction_update;
    int satellite_count;
    bool tracking_active;
    char current_location[64];
    double current_lat;
    double current_lon;
    double current_alt;
} starlink_tracker_state_t;

// Main starlink tracker structure
typedef struct starlink_tracker {
    starlink_tracker_config_t config;
    starlink_tracker_state_t state;
    void *satellite_data;  // Opaque pointer to satellite tracking data
    void *prediction_engine;  // Opaque pointer to prediction engine
} starlink_tracker_t;

// Function declarations
int starlink_tracker_init(starlink_tracker_t *tracker, const starlink_tracker_config_t *config);
void starlink_tracker_cleanup(starlink_tracker_t *tracker);
int starlink_tracker_update_tle(starlink_tracker_t *tracker);
int starlink_tracker_update_predictions(starlink_tracker_t *tracker);
int starlink_tracker_get_visibility(starlink_tracker_t *tracker, double lat, double lon, double alt, time_t timestamp);
starlink_tracker_t* starlink_tracker_init_from_uci(struct uci_context *uci_ctx);
int starlink_tracker_ubus_init(struct ubus_context *ctx, starlink_tracker_t *tracker);
void starlink_tracker_ubus_cleanup(void *ctx);

#endif // STARLINK_TRACKER_H
