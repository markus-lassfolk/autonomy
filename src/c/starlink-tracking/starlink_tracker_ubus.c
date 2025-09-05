#include "starlink_tracker.h"
#include "obstruction_analyzer.h"
#include "../shared/autonomy_types.h"
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <json-c/json.h>
#include <uci.h>

// Global tracker instance
static starlink_tracker_t *g_tracker = NULL;

// UBUS method handlers
static int starlink_tracker_ubus_status(struct ubus_context *ctx, struct ubus_object *obj,
                                       struct ubus_request_data *req, const char *method,
                                       struct blob_attr *msg);

static int starlink_tracker_ubus_predictions(struct ubus_context *ctx, struct ubus_object *obj,
                                            struct ubus_request_data *req, const char *method,
                                            struct blob_attr *msg);

static int starlink_tracker_ubus_satellites(struct ubus_context *ctx, struct ubus_object *obj,
                                           struct ubus_request_data *req, const char *method,
                                           struct blob_attr *msg);

static int starlink_tracker_ubus_start_monitoring(struct ubus_context *ctx, struct ubus_object *obj,
                                                 struct ubus_request_data *req, const char *method,
                                                 struct blob_attr *msg);

static int starlink_tracker_ubus_stop_monitoring(struct ubus_context *ctx, struct ubus_object *obj,
                                                struct ubus_request_data *req, const char *method,
                                                struct blob_attr *msg);

static int starlink_tracker_ubus_update_data(struct ubus_context *ctx, struct ubus_object *obj,
                                            struct ubus_request_data *req, const char *method,
                                            struct blob_attr *msg);

// UBUS method definitions
static const struct ubus_method starlink_tracker_methods[] = {
    UBUS_METHOD_NOARG("status", starlink_tracker_ubus_status),
    UBUS_METHOD_NOARG("predictions", starlink_tracker_ubus_predictions),
    UBUS_METHOD_NOARG("satellites", starlink_tracker_ubus_satellites),
    UBUS_METHOD_NOARG("start_monitoring", starlink_tracker_ubus_start_monitoring),
    UBUS_METHOD_NOARG("stop_monitoring", starlink_tracker_ubus_stop_monitoring),
    UBUS_METHOD_NOARG("update_data", starlink_tracker_ubus_update_data),
};

static struct ubus_object_type starlink_tracker_object_type =
    UBUS_OBJECT_TYPE("starlink_tracker", starlink_tracker_methods);

static struct ubus_object starlink_tracker_object = {
    .name = "starlink_tracker",
    .type = &starlink_tracker_object_type,
    .methods = starlink_tracker_methods,
    .n_methods = ARRAY_SIZE(starlink_tracker_methods),
};

// Initialize UBUS interface for Starlink tracker
int starlink_tracker_ubus_init(struct ubus_context *ctx, starlink_tracker_t *tracker) {
    if (!ctx || !tracker) {
        return -1;
    }
    
    g_tracker = tracker;
    
    int ret = ubus_add_object(ctx, &starlink_tracker_object);
    if (ret) {
        return -1;
    }
    
    return 0;
}

// Cleanup UBUS interface
void starlink_tracker_ubus_cleanup(struct ubus_context *ctx) {
    if (ctx) {
        ubus_remove_object(ctx, &starlink_tracker_object);
    }
    g_tracker = NULL;
}

// Status method handler
static int starlink_tracker_ubus_status(struct ubus_context *ctx, struct ubus_object *obj,
                                       struct ubus_request_data *req, const char *method,
                                       struct blob_attr *msg) {
    struct blob_buf b = {};
    
    blob_buf_init(&b, 0);
    
    if (!g_tracker) {
        blobmsg_add_string(&b, "status", "not_initialized");
        blobmsg_add_string(&b, "error", "Tracker not initialized");
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return 0;
    }
    
    // Get tracker statistics
    const tracking_stats_t *stats = starlink_tracker_get_stats(g_tracker);
    
    // Build status response
    blobmsg_add_string(&b, "status", g_tracker->monitoring_active ? "monitoring" : "idle");
    blobmsg_add_u8(&b, "initialized", g_tracker->initialized);
    blobmsg_add_u8(&b, "monitoring_active", g_tracker->monitoring_active);
    blobmsg_add_u32(&b, "visible_satellites", starlink_tracker_get_visible_satellite_count(g_tracker));
    blobmsg_add_u32(&b, "unobstructed_satellites", starlink_tracker_get_unobstructed_satellite_count(g_tracker));
    blobmsg_add_u32(&b, "total_predictions", stats->total_predictions);
    blobmsg_add_u32(&b, "correct_predictions", stats->correct_predictions);
    blobmsg_add_double(&b, "accuracy_percentage", stats->accuracy_percentage);
    blobmsg_add_u32(&b, "last_update", (uint32_t)g_tracker->last_update);
    
    // Add dish location if available
    if (g_tracker->dish_location.last_update > 0) {
        void *location_table = blobmsg_open_table(&b, "dish_location");
        blobmsg_add_double(&b, "latitude", g_tracker->dish_location.latitude);
        blobmsg_add_double(&b, "longitude", g_tracker->dish_location.longitude);
        blobmsg_add_double(&b, "altitude", g_tracker->dish_location.altitude);
        blobmsg_add_double(&b, "boresight_azimuth", g_tracker->dish_location.boresight_azimuth);
        blobmsg_add_double(&b, "boresight_elevation", g_tracker->dish_location.boresight_elevation);
        blobmsg_close_table(&b, location_table);
    }
    
    // Add obstruction map stats if available
    if (g_tracker->obstruction_map.last_update > 0) {
        obstruction_map_stats_t map_stats = obstruction_analyzer_get_map_stats(&g_tracker->obstruction_map);
        void *obstruction_table = blobmsg_open_table(&b, "obstruction_map");
        blobmsg_add_u32(&b, "total_cells", map_stats.total_cells);
        blobmsg_add_u32(&b, "obstructed_cells", map_stats.obstructed_cells);
        blobmsg_add_double(&b, "obstruction_percentage", map_stats.obstruction_percentage);
        blobmsg_add_double(&b, "average_snr", map_stats.average_snr);
        blobmsg_close_table(&b, obstruction_table);
    }
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return 0;
}

// Predictions method handler
static int starlink_tracker_ubus_predictions(struct ubus_context *ctx, struct ubus_object *obj,
                                            struct ubus_request_data *req, const char *method,
                                            struct blob_attr *msg) {
    struct blob_buf b = {};
    
    blob_buf_init(&b, 0);
    
    if (!g_tracker) {
        blobmsg_add_string(&b, "error", "Tracker not initialized");
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return 0;
    }
    
    // Get current predictions
    outage_prediction_t *predictions;
    int num_predictions = starlink_tracker_get_predictions(g_tracker, &predictions);
    
    if (num_predictions < 0) {
        blobmsg_add_string(&b, "error", "Failed to get predictions");
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return 0;
    }
    
    blobmsg_add_u32(&b, "count", num_predictions);
    
    if (num_predictions > 0) {
        void *predictions_array = blobmsg_open_array(&b, "predictions");
        
        for (int i = 0; i < num_predictions; i++) {
            void *prediction_table = blobmsg_open_table(&b, NULL);
            blobmsg_add_u32(&b, "start_time", (uint32_t)predictions[i].start_time);
            blobmsg_add_u32(&b, "end_time", (uint32_t)predictions[i].end_time);
            blobmsg_add_u32(&b, "duration_seconds", predictions[i].duration_seconds);
            blobmsg_add_u32(&b, "risk_level", predictions[i].risk_level);
            blobmsg_add_string(&b, "description", predictions[i].description);
            blobmsg_add_u32(&b, "predicted_available_sats", predictions[i].predicted_available_sats);
            blobmsg_add_double(&b, "confidence_score", predictions[i].confidence_score);
            blobmsg_close_table(&b, prediction_table);
        }
        
        blobmsg_close_array(&b, predictions_array);
        
        starlink_tracker_free_predictions(predictions, num_predictions);
    }
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return 0;
}

// Satellites method handler
static int starlink_tracker_ubus_satellites(struct ubus_context *ctx, struct ubus_object *obj,
                                           struct ubus_request_data *req, const char *method,
                                           struct blob_attr *msg) {
    struct blob_buf b = {};
    
    blob_buf_init(&b, 0);
    
    if (!g_tracker) {
        blobmsg_add_string(&b, "error", "Tracker not initialized");
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return 0;
    }
    
    // Get current satellite positions
    satellite_position_t *positions;
    int num_positions = starlink_tracker_get_current_satellite_positions(g_tracker, &positions);
    
    if (num_positions < 0) {
        blobmsg_add_string(&b, "error", "Failed to get satellite positions");
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return 0;
    }
    
    blobmsg_add_u32(&b, "count", num_positions);
    blobmsg_add_u32(&b, "visible_count", starlink_tracker_get_visible_satellite_count(g_tracker));
    blobmsg_add_u32(&b, "unobstructed_count", starlink_tracker_get_unobstructed_satellite_count(g_tracker));
    
    if (num_positions > 0) {
        void *satellites_array = blobmsg_open_array(&b, "satellites");
        
        for (int i = 0; i < num_positions; i++) {
            void *satellite_table = blobmsg_open_table(&b, NULL);
            blobmsg_add_string(&b, "satellite_id", positions[i].satellite_id);
            blobmsg_add_double(&b, "azimuth", positions[i].azimuth);
            blobmsg_add_double(&b, "elevation", positions[i].elevation);
            blobmsg_add_double(&b, "range", positions[i].range);
            blobmsg_add_u8(&b, "is_visible", positions[i].is_visible);
            blobmsg_add_u8(&b, "is_obstructed", positions[i].is_obstructed);
            blobmsg_add_double(&b, "signal_quality", positions[i].signal_quality);
            blobmsg_close_table(&b, satellite_table);
        }
        
        blobmsg_close_array(&b, satellites_array);
        
        free(positions);
    }
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return 0;
}

// Start monitoring method handler
static int starlink_tracker_ubus_start_monitoring(struct ubus_context *ctx, struct ubus_object *obj,
                                                 struct ubus_request_data *req, const char *method,
                                                 struct blob_attr *msg) {
    struct blob_buf b = {};
    
    blob_buf_init(&b, 0);
    
    if (!g_tracker) {
        blobmsg_add_string(&b, "status", "error");
        blobmsg_add_string(&b, "message", "Tracker not initialized");
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return 0;
    }
    
    int result = starlink_tracker_start_monitoring(g_tracker);
    
    if (result == TRACKER_SUCCESS) {
        blobmsg_add_string(&b, "status", "success");
        blobmsg_add_string(&b, "message", "Monitoring started");
    } else {
        blobmsg_add_string(&b, "status", "error");
        blobmsg_add_string(&b, "message", "Failed to start monitoring");
        blobmsg_add_u32(&b, "error_code", result);
    }
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return 0;
}

// Stop monitoring method handler
static int starlink_tracker_ubus_stop_monitoring(struct ubus_context *ctx, struct ubus_object *obj,
                                                struct ubus_request_data *req, const char *method,
                                                struct blob_attr *msg) {
    struct blob_buf b = {};
    
    blob_buf_init(&b, 0);
    
    if (!g_tracker) {
        blobmsg_add_string(&b, "status", "error");
        blobmsg_add_string(&b, "message", "Tracker not initialized");
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return 0;
    }
    
    int result = starlink_tracker_stop_monitoring(g_tracker);
    
    if (result == TRACKER_SUCCESS) {
        blobmsg_add_string(&b, "status", "success");
        blobmsg_add_string(&b, "message", "Monitoring stopped");
    } else {
        blobmsg_add_string(&b, "status", "error");
        blobmsg_add_string(&b, "message", "Failed to stop monitoring");
        blobmsg_add_u32(&b, "error_code", result);
    }
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return 0;
}

// Update data method handler
static int starlink_tracker_ubus_update_data(struct ubus_context *ctx, struct ubus_object *obj,
                                            struct ubus_request_data *req, const char *method,
                                            struct blob_attr *msg) {
    struct blob_buf b = {};
    
    blob_buf_init(&b, 0);
    
    if (!g_tracker) {
        blobmsg_add_string(&b, "status", "error");
        blobmsg_add_string(&b, "message", "Tracker not initialized");
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return 0;
    }
    
    // Update dish location
    int location_result = starlink_tracker_update_dish_location(g_tracker);
    
    // Update obstruction map
    int obstruction_result = starlink_tracker_update_obstruction_map(g_tracker);
    
    // Update constellation data
    int constellation_result = starlink_tracker_update_constellation_data(g_tracker);
    
    // Calculate new predictions
    int prediction_result = starlink_tracker_calculate_predictions(g_tracker, g_tracker->config.prediction_horizon_hours);
    
    blobmsg_add_string(&b, "status", "success");
    blobmsg_add_string(&b, "message", "Data update completed");
    blobmsg_add_u8(&b, "location_updated", location_result == TRACKER_SUCCESS);
    blobmsg_add_u8(&b, "obstruction_updated", obstruction_result == TRACKER_SUCCESS);
    blobmsg_add_u8(&b, "constellation_updated", constellation_result == TRACKER_SUCCESS);
    blobmsg_add_u8(&b, "predictions_updated", prediction_result == TRACKER_SUCCESS);
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return 0;
}

// Initialize Starlink tracker with configuration from UCI
starlink_tracker_t* starlink_tracker_init_from_uci(struct uci_context *uci_ctx) {
    if (!uci_ctx) {
        return NULL;
    }
    
    // Load configuration from UCI
    starlink_tracker_config_t config = {0};
    
    // Set defaults
    strncpy(config.starlink_dish_ip, "192.168.100.1", sizeof(config.starlink_dish_ip) - 1);
    config.starlink_dish_port = 9200;
    config.update_interval_minutes = 60;
    config.prediction_horizon_hours = 24;
    config.min_elevation_degrees = 10.0;
    config.obstruction_threshold = 0.7;
    config.validation_enabled = true;
    config.cache_duration_hours = 24;
    config.rate_limit_requests_per_minute = 15;
    
    // TODO: Load actual configuration from UCI
    // This would read from /etc/config/autonomy or similar
    
    // Try to get credentials from environment
    const char *username = getenv("SPACE_TRACK_USERNAME");
    const char *password = getenv("SPACE_TRACK_PASSWORD");
    
    if (username) {
        strncpy(config.space_track_username, username, sizeof(config.space_track_username) - 1);
    }
    
    if (password) {
        strncpy(config.space_track_password, password, sizeof(config.space_track_password) - 1);
    }
    
    // Check if we have required credentials
    if (!config.space_track_username[0] || !config.space_track_password[0]) {
        return NULL; // Cannot initialize without credentials
    }
    
    return starlink_tracker_init(&config);
}
