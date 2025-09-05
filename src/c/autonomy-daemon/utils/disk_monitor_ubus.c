#include "disk_monitor.h"
#include <libubus.h>
#include <libubox/blobmsg.h>
#include <libubox/blobmsg_json.h>
#include <json-c/json.h>

// Forward declarations
static int autonomy_disk_monitor_status(struct ubus_context *ctx, struct ubus_request_data *req,
                                      const char *method, struct blob_attr *msg);
static int autonomy_disk_monitor_config(struct ubus_context *ctx, struct ubus_request_data *req,
                                       const char *method, struct blob_attr *msg);
static int autonomy_disk_monitor_set_config(struct ubus_context *ctx, struct ubus_request_data *req,
                                           const char *method, struct blob_attr *msg);
static int autonomy_disk_monitor_set_enabled(struct ubus_context *ctx, struct ubus_request_data *req,
                                            const char *method, struct blob_attr *msg);
static int autonomy_disk_monitor_reset(struct ubus_context *ctx, struct ubus_request_data *req,
                                      const char *method, struct blob_attr *msg);
static int autonomy_disk_monitor_check(struct ubus_context *ctx, struct ubus_request_data *req,
                                      const char *method, struct blob_attr *msg);
static int autonomy_disk_monitor_cleanup(struct ubus_context *ctx, struct ubus_request_data *req,
                                        const char *method, struct blob_attr *msg);

// UBUS method definitions
static const struct ubus_method_type autonomy_disk_monitor_methods[] = {
    UBUS_METHOD("status", autonomy_disk_monitor_status, 0),
    UBUS_METHOD("config", autonomy_disk_monitor_config, 0),
    UBUS_METHOD("set_config", autonomy_disk_monitor_set_config, 0),
    UBUS_METHOD("set_enabled", autonomy_disk_monitor_set_enabled, 0),
    UBUS_METHOD("reset", autonomy_disk_monitor_reset, 0),
    UBUS_METHOD("check", autonomy_disk_monitor_check, 0),
    UBUS_METHOD("cleanup", autonomy_disk_monitor_cleanup, 0),
};

// UBUS object type
static const struct ubus_object_type autonomy_disk_monitor_obj_type = {
    .name = "autonomy_disk_monitor",
    .methods = autonomy_disk_monitor_methods,
    .n_methods = ARRAY_SIZE(autonomy_disk_monitor_methods),
};

// UBUS object
static const struct ubus_object autonomy_disk_monitor_obj = {
    .name = "disk_monitor",
    .type = &autonomy_disk_monitor_obj_type,
    .methods = autonomy_disk_monitor_methods,
    .n_methods = ARRAY_SIZE(autonomy_disk_monitor_methods),
};

/**
 * Get disk monitor status
 */
static int autonomy_disk_monitor_status(struct ubus_context *ctx, struct ubus_request_data *req,
                                      const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    disk_monitor_status_t status;
    int result = disk_monitor_get_status(&status);
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to get disk monitor status");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    // Add configuration
    blobmsg_add_u8(&bb, "enabled", status.enabled);
    blobmsg_add_u32(&bb, "check_interval", status.check_interval);
    blobmsg_add_u32(&bb, "warning_threshold", status.warning_threshold);
    blobmsg_add_u32(&bb, "critical_threshold", status.critical_threshold);
    blobmsg_add_u8(&bb, "auto_cleanup", status.auto_cleanup);
    blobmsg_add_u32(&bb, "cleanup_threshold", status.cleanup_threshold);
    
    // Add statistics
    blobmsg_add_u32(&bb, "last_check_time", status.last_check_time);
    blobmsg_add_u32(&bb, "last_cleanup_time", status.last_cleanup_time);
    blobmsg_add_u32(&bb, "total_cleanups", status.total_cleanups);
    blobmsg_add_u32(&bb, "total_emergency_cleanups", status.total_emergency_cleanups);
    blobmsg_add_u32(&bb, "total_space_freed", status.total_space_freed);
    
    // Add current disk info
    blobmsg_add_u32(&bb, "current_usage_percent", status.current_usage_percent);
    blobmsg_add_u32(&bb, "current_usage_bytes", status.current_usage_bytes);
    blobmsg_add_u32(&bb, "total_space_bytes", status.total_space_bytes);
    blobmsg_add_u32(&bb, "available_space_bytes", status.available_space_bytes);
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Get disk monitor configuration
 */
static int autonomy_disk_monitor_config(struct ubus_context *ctx, struct ubus_request_data *req,
                                       const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    disk_monitor_config_t config;
    int result = disk_monitor_get_config(&config);
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to get disk monitor configuration");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    blobmsg_add_u8(&bb, "enabled", config.enabled);
    blobmsg_add_u32(&bb, "check_interval", config.check_interval);
    blobmsg_add_u32(&bb, "warning_threshold", config.warning_threshold);
    blobmsg_add_u32(&bb, "critical_threshold", config.critical_threshold);
    blobmsg_add_u8(&bb, "auto_cleanup", config.auto_cleanup);
    blobmsg_add_u32(&bb, "cleanup_threshold", config.cleanup_threshold);
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Set disk monitor configuration
 */
static int autonomy_disk_monitor_set_config(struct ubus_context *ctx, struct ubus_request_data *req,
                                           const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    disk_monitor_config_t config;
    int result = disk_monitor_get_config(&config); // Get current config first
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to get current configuration");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    // Parse incoming message and update config
    struct blob_attr *tb[__DISK_MONITOR_CONFIG_MAX];
    enum {
        DISK_MONITOR_CONFIG_ENABLED = 0,
        DISK_MONITOR_CONFIG_CHECK_INTERVAL,
        DISK_MONITOR_CONFIG_WARNING_THRESHOLD,
        DISK_MONITOR_CONFIG_CRITICAL_THRESHOLD,
        DISK_MONITOR_CONFIG_AUTO_CLEANUP,
        DISK_MONITOR_CONFIG_CLEANUP_THRESHOLD,
        __DISK_MONITOR_CONFIG_MAX
    };
    
    static const struct blobmsg_policy policy[__DISK_MONITOR_CONFIG_MAX] = {
        [DISK_MONITOR_CONFIG_ENABLED] = { .name = "enabled", .type = BLOBMSG_TYPE_BOOL },
        [DISK_MONITOR_CONFIG_CHECK_INTERVAL] = { .name = "check_interval", .type = BLOBMSG_TYPE_INT32 },
        [DISK_MONITOR_CONFIG_WARNING_THRESHOLD] = { .name = "warning_threshold", .type = BLOBMSG_TYPE_INT32 },
        [DISK_MONITOR_CONFIG_CRITICAL_THRESHOLD] = { .name = "critical_threshold", .type = BLOBMSG_TYPE_INT32 },
        [DISK_MONITOR_CONFIG_AUTO_CLEANUP] = { .name = "auto_cleanup", .type = BLOBMSG_TYPE_BOOL },
        [DISK_MONITOR_CONFIG_CLEANUP_THRESHOLD] = { .name = "cleanup_threshold", .type = BLOBMSG_TYPE_INT32 },
    };
    
    blobmsg_parse(policy, __DISK_MONITOR_CONFIG_MAX, tb, blob_data(msg), blob_len(msg));
    
    // Update configuration with new values
    if (tb[DISK_MONITOR_CONFIG_ENABLED]) {
        config.enabled = blobmsg_get_bool(tb[DISK_MONITOR_CONFIG_ENABLED]);
    }
    
    if (tb[DISK_MONITOR_CONFIG_CHECK_INTERVAL]) {
        config.check_interval = blobmsg_get_u32(tb[DISK_MONITOR_CONFIG_CHECK_INTERVAL]);
    }
    
    if (tb[DISK_MONITOR_CONFIG_WARNING_THRESHOLD]) {
        config.warning_threshold = blobmsg_get_u32(tb[DISK_MONITOR_CONFIG_WARNING_THRESHOLD]);
    }
    
    if (tb[DISK_MONITOR_CONFIG_CRITICAL_THRESHOLD]) {
        config.critical_threshold = blobmsg_get_u32(tb[DISK_MONITOR_CONFIG_CRITICAL_THRESHOLD]);
    }
    
    if (tb[DISK_MONITOR_CONFIG_AUTO_CLEANUP]) {
        config.auto_cleanup = blobmsg_get_bool(tb[DISK_MONITOR_CONFIG_AUTO_CLEANUP]);
    }
    
    if (tb[DISK_MONITOR_CONFIG_CLEANUP_THRESHOLD]) {
        config.cleanup_threshold = blobmsg_get_u32(tb[DISK_MONITOR_CONFIG_CLEANUP_THRESHOLD]);
    }
    
    // Apply the new configuration
    result = disk_monitor_set_config(&config);
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to set disk monitor configuration");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    blobmsg_add_string(&bb, "status", "Configuration updated successfully");
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Enable/disable disk monitor
 */
static int autonomy_disk_monitor_set_enabled(struct ubus_context *ctx, struct ubus_request_data *req,
                                            const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    struct blob_attr *tb[1];
    enum {
        DISK_MONITOR_ENABLED = 0,
        __DISK_MONITOR_ENABLED_MAX
    };
    
    static const struct blobmsg_policy policy[__DISK_MONITOR_ENABLED_MAX] = {
        [DISK_MONITOR_ENABLED] = { .name = "enabled", .type = BLOBMSG_TYPE_BOOL },
    };
    
    blobmsg_parse(policy, __DISK_MONITOR_ENABLED_MAX, tb, blob_data(msg), blob_len(msg));
    
    if (!tb[DISK_MONITOR_ENABLED]) {
        blobmsg_add_string(&bb, "error", "Missing 'enabled' parameter");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    bool enabled = blobmsg_get_bool(tb[DISK_MONITOR_ENABLED]);
    int result = disk_monitor_set_enabled(enabled);
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to set disk monitor enabled state");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    blobmsg_add_string(&bb, "status", enabled ? "Disk monitor enabled" : "Disk monitor disabled");
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Reset disk monitor statistics
 */
static int autonomy_disk_monitor_reset(struct ubus_context *ctx, struct ubus_request_data *req,
                                      const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    int result = disk_monitor_reset();
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to reset disk monitor statistics");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    blobmsg_add_string(&bb, "status", "Disk monitor statistics reset successfully");
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Manually trigger disk monitor check
 */
static int autonomy_disk_monitor_check(struct ubus_context *ctx, struct ubus_request_data *req,
                                      const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    int result = disk_monitor_check_disk_space();
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Disk monitor check failed");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    blobmsg_add_string(&bb, "status", "Disk monitor check completed successfully");
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Manually trigger disk monitor cleanup
 */
static int autonomy_disk_monitor_cleanup(struct ubus_context *ctx, struct ubus_request_data *req,
                                        const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    // Parse message to determine cleanup type
    struct blob_attr *tb[1];
    enum {
        DISK_MONITOR_CLEANUP_TYPE = 0,
        __DISK_MONITOR_CLEANUP_MAX
    };
    
    static const struct blobmsg_policy policy[__DISK_MONITOR_CLEANUP_MAX] = {
        [DISK_MONITOR_CLEANUP_TYPE] = { .name = "type", .type = BLOBMSG_TYPE_STRING },
    };
    
    blobmsg_parse(policy, __DISK_MONITOR_CLEANUP_MAX, tb, blob_data(msg), blob_len(msg));
    
    const char *cleanup_type = "routine";
    if (tb[DISK_MONITOR_CLEANUP_TYPE]) {
        cleanup_type = blobmsg_get_string(tb[DISK_MONITOR_CLEANUP_TYPE]);
    }
    
    int result;
    if (strcmp(cleanup_type, "emergency") == 0) {
        result = disk_monitor_perform_emergency_cleanup();
    } else {
        result = disk_monitor_perform_cleanup();
    }
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Disk monitor cleanup failed");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    blobmsg_add_string(&bb, "status", "Disk monitor cleanup completed successfully");
    blobmsg_add_string(&bb, "type", cleanup_type);
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Register disk monitor UBUS object
 */
static int disk_monitor_ubus_register(struct ubus_context *ctx) {
    int ret = ubus_add_object(ctx, &autonomy_disk_monitor_obj);
    if (ret) {
        fprintf(stderr, "Failed to add disk monitor object: %s\n", ubus_strerror(ret));
        return ret;
    }
    
    fprintf(stderr, "Disk monitor UBUS object registered successfully\n");
    return 0;
}

/**
 * Unregister disk monitor UBUS object
 */
static void disk_monitor_ubus_unregister(struct ubus_context *ctx) {
    ubus_remove_object(ctx, &autonomy_disk_monitor_obj);
    fprintf(stderr, "Disk monitor UBUS object unregistered\n");
}
