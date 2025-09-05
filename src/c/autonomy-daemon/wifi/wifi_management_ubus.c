#include "wifi_management.h"
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <libubox/blobmsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// WiFi management UBUS methods

/**
 * Get WiFi management status
 */
int autonomy_wifi_management_status(struct ubus_context *uctx, struct ubus_object *obj,
                                   struct ubus_request_data *req, const char *method,
                                   struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    wifi_management_status_t status;
    if (wifi_management_get_status(&status) == 0) {
        blobmsg_add_string(&bb, "result", "status_retrieved");
        blobmsg_add_u8(&bb, "enabled", status.enabled);
        blobmsg_add_u32(&bb, "interfaces_count", status.interfaces_count);
        blobmsg_add_u32(&bb, "last_optimized", (uint32_t)status.last_optimized);
        blobmsg_add_u32(&bb, "optimization_count", status.optimization_count);
        blobmsg_add_u32(&bb, "successful_optimizations", status.successful_optimizations);
        blobmsg_add_u32(&bb, "failed_optimizations", status.failed_optimizations);
        blobmsg_add_u8(&bb, "scheduler_enabled", status.scheduler_enabled);
        blobmsg_add_u8(&bb, "gps_integration_enabled", status.gps_integration_enabled);
    } else {
        blobmsg_add_string(&bb, "result", "status_failed");
        blobmsg_add_string(&bb, "error", "Unable to retrieve WiFi management status");
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Get WiFi interfaces
 */
int autonomy_wifi_management_interfaces(struct ubus_context *uctx, struct ubus_object *obj,
                                       struct ubus_request_data *req, const char *method,
                                       struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    wifi_interface_t interfaces[10];
    int count = wifi_management_get_interfaces(interfaces, 10);
    
    if (count >= 0) {
        blobmsg_add_string(&bb, "result", "interfaces_retrieved");
        blobmsg_add_u32(&bb, "count", count);
        
        struct blob_attr *interfaces_array = blobmsg_open_array(&bb, "interfaces");
        for (int i = 0; i < count; i++) {
            struct blob_attr *interface = blobmsg_open_table(&bb, NULL);
            blobmsg_add_string(&bb, "name", interfaces[i].name);
            blobmsg_add_string(&bb, "band", interfaces[i].band);
            blobmsg_add_string(&bb, "frequency", interfaces[i].frequency);
            blobmsg_add_u8(&bb, "active", interfaces[i].active);
            blobmsg_close_table(&bb, interface);
        }
        blobmsg_close_array(&bb, interfaces_array);
    } else {
        blobmsg_add_string(&bb, "result", "interfaces_failed");
        blobmsg_add_string(&bb, "error", "Unable to retrieve WiFi interfaces");
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Get channel scores
 */
int autonomy_wifi_management_channel_scores(struct ubus_context *uctx, struct ubus_object *obj,
                                           struct ubus_request_data *req, const char *method,
                                           struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    wifi_channel_score_t scores[100];
    int count = wifi_management_get_channel_scores(scores, 100);
    
    if (count >= 0) {
        blobmsg_add_string(&bb, "result", "scores_retrieved");
        blobmsg_add_u32(&bb, "count", count);
        
        struct blob_attr *scores_array = blobmsg_open_array(&bb, "channel_scores");
        for (int i = 0; i < count; i++) {
            struct blob_attr *score = blobmsg_open_table(&bb, NULL);
            blobmsg_add_u32(&bb, "channel", scores[i].channel);
            blobmsg_add_u32(&bb, "score", scores[i].score);
            blobmsg_add_u32(&bb, "bss_count", scores[i].bss_count);
            blobmsg_add_u32(&bb, "noise", scores[i].noise);
            blobmsg_add_u32(&bb, "avg_rssi", scores[i].avg_rssi);
            blobmsg_close_table(&bb, score);
        }
        blobmsg_close_array(&bb, scores_array);
    } else {
        blobmsg_add_string(&bb, "result", "scores_failed");
        blobmsg_add_string(&bb, "error", "Unable to retrieve channel scores");
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Get scheduled tasks
 */
int autonomy_wifi_management_scheduled_tasks(struct ubus_context *uctx, struct ubus_object *obj,
                                            struct ubus_request_data *req, const char *method,
                                            struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    wifi_scheduled_task_t tasks[50];
    int count = wifi_management_get_scheduled_tasks(tasks, 50);
    
    if (count >= 0) {
        blobmsg_add_string(&bb, "result", "tasks_retrieved");
        blobmsg_add_u32(&bb, "count", count);
        
        struct blob_attr *tasks_array = blobmsg_open_array(&bb, "scheduled_tasks");
        for (int i = 0; i < count; i++) {
            struct blob_attr *task = blobmsg_open_table(&bb, NULL);
            blobmsg_add_u32(&bb, "type", tasks[i].type);
            blobmsg_add_u32(&bb, "scheduled_at", (uint32_t)tasks[i].scheduled_at);
            blobmsg_add_u32(&bb, "executed_at", (uint32_t)tasks[i].executed_at);
            blobmsg_add_u8(&bb, "success", tasks[i].success);
            blobmsg_add_string(&bb, "trigger", tasks[i].trigger);
            blobmsg_close_table(&bb, task);
        }
        blobmsg_close_array(&bb, tasks_array);
    } else {
        blobmsg_add_string(&bb, "result", "tasks_failed");
        blobmsg_add_string(&bb, "error", "Unable to retrieve scheduled tasks");
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Get WiFi management configuration
 */
int autonomy_wifi_management_config(struct ubus_context *uctx, struct ubus_object *obj,
                                   struct ubus_request_data *req, const char *method,
                                   struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    wifi_management_config_t config;
    if (wifi_management_get_config(&config) == 0) {
        blobmsg_add_string(&bb, "result", "config_retrieved");
        blobmsg_add_u8(&bb, "enabled", config.enabled);
        blobmsg_add_double(&bb, "movement_threshold", config.movement_threshold);
        blobmsg_add_u32(&bb, "stationary_time", config.stationary_time);
        blobmsg_add_u8(&bb, "nightly_optimization", config.nightly_optimization);
        blobmsg_add_u32(&bb, "nightly_time", config.nightly_time);
        blobmsg_add_u32(&bb, "min_improvement", config.min_improvement);
        blobmsg_add_u32(&bb, "dwell_time", config.dwell_time);
        blobmsg_add_u32(&bb, "noise_default", config.noise_default);
        blobmsg_add_u32(&bb, "vht80_threshold", config.vht80_threshold);
        blobmsg_add_u32(&bb, "vht40_threshold", config.vht40_threshold);
        blobmsg_add_u8(&bb, "use_dfs", config.use_dfs);
        blobmsg_add_u8(&bb, "dry_run", config.dry_run);
        blobmsg_add_u8(&bb, "use_enhanced_scanner", config.use_enhanced_scanner);
        blobmsg_add_u32(&bb, "strong_rssi_threshold", config.strong_rssi_threshold);
        blobmsg_add_u32(&bb, "weak_rssi_threshold", config.weak_rssi_threshold);
        blobmsg_add_u32(&bb, "utilization_weight", config.utilization_weight);
        blobmsg_add_u32(&bb, "excellent_threshold", config.excellent_threshold);
        blobmsg_add_u32(&bb, "good_threshold", config.good_threshold);
        blobmsg_add_u32(&bb, "fair_threshold", config.fair_threshold);
        blobmsg_add_u32(&bb, "poor_threshold", config.poor_threshold);
        blobmsg_add_double(&bb, "overlap_penalty_ratio", config.overlap_penalty_ratio);
    } else {
        blobmsg_add_string(&bb, "result", "config_failed");
        blobmsg_add_string(&bb, "error", "Unable to retrieve WiFi management configuration");
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Set WiFi management configuration
 */
int autonomy_wifi_management_set_config(struct ubus_context *uctx, struct ubus_object *obj,
                                       struct ubus_request_data *req, const char *method,
                                       struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    // Parse configuration from message
    wifi_management_config_t config = {0};
    struct blob_attr *tb[__BLOBMSG_TYPE_LAST];
    unsigned int rem;
    
    blobmsg_parse(blobmsg_policy_policy, 0, tb, blob_data(msg), blob_len(msg));
    
    if (tb[BLOBMSG_TYPE_BOOL]) {
        config.enabled = blobmsg_get_bool(tb[BLOBMSG_TYPE_BOOL]);
    }
    if (tb[BLOBMSG_TYPE_DOUBLE]) {
        config.movement_threshold = blobmsg_get_double(tb[BLOBMSG_TYPE_DOUBLE]);
    }
    if (tb[BLOBMSG_TYPE_INT32]) {
        config.stationary_time = blobmsg_get_u32(tb[BLOBMSG_TYPE_INT32]);
    }
    if (tb[BLOBMSG_TYPE_BOOL]) {
        config.nightly_optimization = blobmsg_get_bool(tb[BLOBMSG_TYPE_BOOL]);
    }
    if (tb[BLOBMSG_TYPE_INT32]) {
        config.nightly_time = blobmsg_get_u32(tb[BLOBMSG_TYPE_INT32]);
    }
    if (tb[BLOBMSG_TYPE_INT32]) {
        config.min_improvement = blobmsg_get_u32(tb[BLOBMSG_TYPE_INT32]);
    }
    if (tb[BLOBMSG_TYPE_INT32]) {
        config.dwell_time = blobmsg_get_u32(tb[BLOBMSG_TYPE_INT32]);
    }
    if (tb[BLOBMSG_TYPE_INT32]) {
        config.noise_default = blobmsg_get_u32(tb[BLOBMSG_TYPE_INT32]);
    }
    if (tb[BLOBMSG_TYPE_INT32]) {
        config.vht80_threshold = blobmsg_get_u32(tb[BLOBMSG_TYPE_INT32]);
    }
    if (tb[BLOBMSG_TYPE_INT32]) {
        config.vht40_threshold = blobmsg_get_u32(tb[BLOBMSG_TYPE_INT32]);
    }
    if (tb[BLOBMSG_TYPE_BOOL]) {
        config.use_dfs = blobmsg_get_bool(tb[BLOBMSG_TYPE_BOOL]);
    }
    if (tb[BLOBMSG_TYPE_BOOL]) {
        config.dry_run = blobmsg_get_bool(tb[BLOBMSG_TYPE_BOOL]);
    }
    if (tb[BLOBMSG_TYPE_BOOL]) {
        config.use_enhanced_scanner = blobmsg_get_bool(tb[BLOBMSG_TYPE_BOOL]);
    }
    if (tb[BLOBMSG_TYPE_INT32]) {
        config.strong_rssi_threshold = blobmsg_get_u32(tb[BLOBMSG_TYPE_INT32]);
    }
    if (tb[BLOBMSG_TYPE_INT32]) {
        config.weak_rssi_threshold = blobmsg_get_u32(tb[BLOBMSG_TYPE_INT32]);
    }
    if (tb[BLOBMSG_TYPE_INT32]) {
        config.utilization_weight = blobmsg_get_u32(tb[BLOBMSG_TYPE_INT32]);
    }
    if (tb[BLOBMSG_TYPE_INT32]) {
        config.excellent_threshold = blobmsg_get_u32(tb[BLOBMSG_TYPE_INT32]);
    }
    if (tb[BLOBMSG_TYPE_INT32]) {
        config.good_threshold = blobmsg_get_u32(tb[BLOBMSG_TYPE_INT32]);
    }
    if (tb[BLOBMSG_TYPE_INT32]) {
        config.fair_threshold = blobmsg_get_u32(tb[BLOBMSG_TYPE_INT32]);
    }
    if (tb[BLOBMSG_TYPE_INT32]) {
        config.poor_threshold = blobmsg_get_u32(tb[BLOBMSG_TYPE_INT32]);
    }
    if (tb[BLOBMSG_TYPE_DOUBLE]) {
        config.overlap_penalty_ratio = blobmsg_get_double(tb[BLOBMSG_TYPE_DOUBLE]);
    }
    
    int result = wifi_management_set_config(&config);
    if (result == 0) {
        blobmsg_add_string(&bb, "result", "config_updated");
        blobmsg_add_string(&bb, "message", "WiFi management configuration updated successfully");
    } else {
        blobmsg_add_string(&bb, "result", "config_update_failed");
        blobmsg_add_string(&bb, "error", "Failed to update WiFi management configuration");
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Enable/disable WiFi management
 */
int autonomy_wifi_management_set_enabled(struct ubus_context *uctx, struct ubus_object *obj,
                                        struct ubus_request_data *req, const char *method,
                                        struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    struct blob_attr *tb[__BLOBMSG_TYPE_LAST];
    unsigned int rem;
    
    blobmsg_parse(blobmsg_policy_policy, 0, tb, blob_data(msg), blob_len(msg));
    
    bool enabled = false;
    if (tb[BLOBMSG_TYPE_BOOL]) {
        enabled = blobmsg_get_bool(tb[BLOBMSG_TYPE_BOOL]);
    }
    
    int result = wifi_management_set_enabled(enabled);
    if (result == 0) {
        blobmsg_add_string(&bb, "result", "enabled_updated");
        blobmsg_add_u8(&bb, "enabled", enabled);
        blobmsg_add_string(&bb, "message", "WiFi management enabled state updated successfully");
    } else {
        blobmsg_add_string(&bb, "result", "enabled_update_failed");
        blobmsg_add_string(&bb, "error", "Failed to update WiFi management enabled state");
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Reset WiFi management
 */
int autonomy_wifi_management_reset(struct ubus_context *uctx, struct ubus_object *obj,
                                  struct ubus_request_data *req, const char *method,
                                  struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    int result = wifi_management_reset();
    if (result == 0) {
        blobmsg_add_string(&bb, "result", "reset_successful");
        blobmsg_add_string(&bb, "message", "WiFi management reset successfully");
    } else {
        blobmsg_add_string(&bb, "result", "reset_failed");
        blobmsg_add_string(&bb, "error", "Failed to reset WiFi management");
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Scan WiFi channels
 */
int autonomy_wifi_management_scan_channels(struct ubus_context *uctx, struct ubus_object *obj,
                                          struct ubus_request_data *req, const char *method,
                                          struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    struct blob_attr *tb[__BLOBMSG_TYPE_LAST];
    unsigned int rem;
    
    blobmsg_parse(blobmsg_policy_policy, 0, tb, blob_data(msg), blob_len(msg));
    
    const char *interface_name = "wlan0"; // Default interface
    if (tb[BLOBMSG_TYPE_STRING]) {
        interface_name = blobmsg_get_string(tb[BLOBMSG_TYPE_STRING]);
    }
    
    int count = wifi_management_scan_channels(interface_name);
    if (count >= 0) {
        blobmsg_add_string(&bb, "result", "scan_successful");
        blobmsg_add_string(&bb, "interface", interface_name);
        blobmsg_add_u32(&bb, "channels_scanned", count);
        blobmsg_add_string(&bb, "message", "WiFi channel scan completed successfully");
    } else {
        blobmsg_add_string(&bb, "result", "scan_failed");
        blobmsg_add_string(&bb, "interface", interface_name);
        blobmsg_add_string(&bb, "error", "Failed to scan WiFi channels");
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Optimize WiFi channels
 */
int autonomy_wifi_management_optimize_channels(struct ubus_context *uctx, struct ubus_object *obj,
                                              struct ubus_request_data *req, const char *method,
                                              struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    struct blob_attr *tb[__BLOBMSG_TYPE_LAST];
    unsigned int rem;
    
    blobmsg_parse(blobmsg_policy_policy, 0, tb, blob_data(msg), blob_len(msg));
    
    const char *interface_name = "wlan0"; // Default interface
    if (tb[BLOBMSG_TYPE_STRING]) {
        interface_name = blobmsg_get_string(tb[BLOBMSG_TYPE_STRING]);
    }
    
    int result = wifi_management_optimize_channels(interface_name);
    if (result == 0) {
        blobmsg_add_string(&bb, "result", "optimization_successful");
        blobmsg_add_string(&bb, "interface", interface_name);
        blobmsg_add_string(&bb, "message", "WiFi channel optimization completed successfully");
    } else {
        blobmsg_add_string(&bb, "result", "optimization_failed");
        blobmsg_add_string(&bb, "interface", interface_name);
        blobmsg_add_string(&bb, "error", "Failed to optimize WiFi channels");
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Update GPS location for WiFi optimization
 */
int autonomy_wifi_management_update_gps_location(struct ubus_context *uctx, struct ubus_object *obj,
                                                struct ubus_request_data *req, const char *method,
                                                struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    struct blob_attr *tb[__BLOBMSG_TYPE_LAST];
    unsigned int rem;
    
    blobmsg_parse(blobmsg_policy_policy, 0, tb, blob_data(msg), blob_len(msg));
    
    double lat = 0.0, lon = 0.0, accuracy = 0.0;
    time_t timestamp = time(NULL);
    
    if (tb[BLOBMSG_TYPE_DOUBLE]) {
        lat = blobmsg_get_double(tb[BLOBMSG_TYPE_DOUBLE]);
    }
    if (tb[BLOBMSG_TYPE_DOUBLE]) {
        lon = blobmsg_get_double(tb[BLOBMSG_TYPE_DOUBLE]);
    }
    if (tb[BLOBMSG_TYPE_DOUBLE]) {
        accuracy = blobmsg_get_double(tb[BLOBMSG_TYPE_DOUBLE]);
    }
    if (tb[BLOBMSG_TYPE_INT32]) {
        timestamp = (time_t)blobmsg_get_u32(tb[BLOBMSG_TYPE_INT32]);
    }
    
    int result = wifi_management_update_gps_location(lat, lon, accuracy, timestamp);
    if (result == 0) {
        blobmsg_add_string(&bb, "result", "gps_location_updated");
        blobmsg_add_double(&bb, "latitude", lat);
        blobmsg_add_double(&bb, "longitude", lon);
        blobmsg_add_double(&bb, "accuracy", accuracy);
        blobmsg_add_u32(&bb, "timestamp", (uint32_t)timestamp);
        blobmsg_add_string(&bb, "message", "GPS location updated for WiFi optimization");
    } else {
        blobmsg_add_string(&bb, "result", "gps_location_update_failed");
        blobmsg_add_string(&bb, "error", "Failed to update GPS location for WiFi optimization");
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}
