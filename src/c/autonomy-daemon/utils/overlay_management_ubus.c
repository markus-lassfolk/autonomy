#include "overlay_management.h"
#include <libubus.h>
#include <libubox/blobmsg.h>
#include <libubox/blobmsg_json.h>
#include <json-c/json.h>

// Forward declarations
static int autonomy_overlay_management_status(struct ubus_context *ctx, struct ubus_request_data *req,
                                           const char *method, struct blob_attr *msg);
static int autonomy_overlay_management_config(struct ubus_context *ctx, struct ubus_request_data *req,
                                            const char *method, struct blob_attr *msg);
static int autonomy_overlay_management_set_config(struct ubus_context *ctx, struct ubus_request_data *req,
                                                const char *method, struct blob_attr *msg);
static int autonomy_overlay_management_set_enabled(struct ubus_context *ctx, struct ubus_request_data *req,
                                                 const char *method, struct blob_attr *msg);
static int autonomy_overlay_management_reset(struct ubus_context *ctx, struct ubus_request_data *req,
                                           const char *method, struct blob_attr *msg);
static int autonomy_overlay_management_check(struct ubus_context *ctx, struct ubus_request_data *req,
                                           const char *method, struct blob_attr *msg);
static int autonomy_overlay_management_cleanup(struct ubus_context *ctx, struct ubus_request_data *req,
                                             const char *method, struct blob_attr *msg);

// UBUS method definitions
static const struct ubus_method_type autonomy_overlay_management_methods[] = {
    UBUS_METHOD("status", autonomy_overlay_management_status, 0),
    UBUS_METHOD("config", autonomy_overlay_management_config, 0),
    UBUS_METHOD("set_config", autonomy_overlay_management_set_config, 0),
    UBUS_METHOD("set_enabled", autonomy_overlay_management_set_enabled, 0),
    UBUS_METHOD("reset", autonomy_overlay_management_reset, 0),
    UBUS_METHOD("check", autonomy_overlay_management_check, 0),
    UBUS_METHOD("cleanup", autonomy_overlay_management_cleanup, 0),
};

// UBUS object type
static const struct ubus_object_type autonomy_overlay_management_obj_type = {
    .name = "autonomy_overlay_management",
    .methods = autonomy_overlay_management_methods,
    .n_methods = ARRAY_SIZE(autonomy_overlay_management_methods),
};

// UBUS object
static const struct ubus_object autonomy_overlay_management_obj = {
    .name = "overlay_management",
    .type = &autonomy_overlay_management_obj_type,
    .methods = autonomy_overlay_management_methods,
    .n_methods = ARRAY_SIZE(autonomy_overlay_management_methods),
};

/**
 * Get overlay management status
 */
static int autonomy_overlay_management_status(struct ubus_context *ctx, struct ubus_request_data *req,
                                           const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    overlay_management_status_t status;
    int result = overlay_management_get_status(&status);
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to get overlay management status");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    // Add configuration
    blobmsg_add_u8(&bb, "enabled", status.enabled);
    blobmsg_add_u32(&bb, "check_interval", status.check_interval);
    blobmsg_add_u32(&bb, "cleanup_threshold", status.cleanup_threshold);
    blobmsg_add_u32(&bb, "emergency_threshold", status.emergency_threshold);
    blobmsg_add_u8(&bb, "auto_cleanup", status.auto_cleanup);
    blobmsg_add_u8(&bb, "backup_before_cleanup", status.backup_before_cleanup);
    
    // Add statistics
    blobmsg_add_u32(&bb, "last_check_time", status.last_check_time);
    blobmsg_add_u32(&bb, "last_cleanup_time", status.last_cleanup_time);
    blobmsg_add_u32(&bb, "total_cleanups", status.total_cleanups);
    blobmsg_add_u32(&bb, "total_emergency_cleanups", status.total_emergency_cleanups);
    blobmsg_add_u32(&bb, "total_space_freed", status.total_space_freed);
    
    // Add current usage
    blobmsg_add_u32(&bb, "current_usage_percent", status.current_usage_percent);
    blobmsg_add_u32(&bb, "current_usage_bytes", status.current_usage_bytes);
    blobmsg_add_u32(&bb, "total_space_bytes", status.total_space_bytes);
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Get overlay management configuration
 */
static int autonomy_overlay_management_config(struct ubus_context *ctx, struct ubus_request_data *req,
                                            const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    overlay_management_config_t config;
    int result = overlay_management_get_config(&config);
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to get overlay management configuration");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    blobmsg_add_u8(&bb, "enabled", config.enabled);
    blobmsg_add_u32(&bb, "check_interval", config.check_interval);
    blobmsg_add_u32(&bb, "cleanup_threshold", config.cleanup_threshold);
    blobmsg_add_u32(&bb, "emergency_threshold", config.emergency_threshold);
    blobmsg_add_u8(&bb, "auto_cleanup", config.auto_cleanup);
    blobmsg_add_u8(&bb, "backup_before_cleanup", config.backup_before_cleanup);
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Set overlay management configuration
 */
static int autonomy_overlay_management_set_config(struct ubus_context *ctx, struct ubus_request_data *req,
                                                const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    overlay_management_config_t config;
    int result = overlay_management_get_config(&config); // Get current config first
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to get current configuration");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    // Parse incoming message and update config
    struct blob_attr *tb[__OVERLAY_MANAGEMENT_CONFIG_MAX];
    enum {
        OVERLAY_MANAGEMENT_CONFIG_ENABLED = 0,
        OVERLAY_MANAGEMENT_CONFIG_CHECK_INTERVAL,
        OVERLAY_MANAGEMENT_CONFIG_CLEANUP_THRESHOLD,
        OVERLAY_MANAGEMENT_CONFIG_EMERGENCY_THRESHOLD,
        OVERLAY_MANAGEMENT_CONFIG_AUTO_CLEANUP,
        OVERLAY_MANAGEMENT_CONFIG_BACKUP_BEFORE_CLEANUP,
        __OVERLAY_MANAGEMENT_CONFIG_MAX
    };
    
    static const struct blobmsg_policy policy[__OVERLAY_MANAGEMENT_CONFIG_MAX] = {
        [OVERLAY_MANAGEMENT_CONFIG_ENABLED] = { .name = "enabled", .type = BLOBMSG_TYPE_BOOL },
        [OVERLAY_MANAGEMENT_CONFIG_CHECK_INTERVAL] = { .name = "check_interval", .type = BLOBMSG_TYPE_INT32 },
        [OVERLAY_MANAGEMENT_CONFIG_CLEANUP_THRESHOLD] = { .name = "cleanup_threshold", .type = BLOBMSG_TYPE_INT32 },
        [OVERLAY_MANAGEMENT_CONFIG_EMERGENCY_THRESHOLD] = { .name = "emergency_threshold", .type = BLOBMSG_TYPE_INT32 },
        [OVERLAY_MANAGEMENT_CONFIG_AUTO_CLEANUP] = { .name = "auto_cleanup", .type = BLOBMSG_TYPE_BOOL },
        [OVERLAY_MANAGEMENT_CONFIG_BACKUP_BEFORE_CLEANUP] = { .name = "backup_before_cleanup", .type = BLOBMSG_TYPE_BOOL },
    };
    
    blobmsg_parse(policy, __OVERLAY_MANAGEMENT_CONFIG_MAX, tb, blob_data(msg), blob_len(msg));
    
    // Update configuration with new values
    if (tb[OVERLAY_MANAGEMENT_CONFIG_ENABLED]) {
        config.enabled = blobmsg_get_bool(tb[OVERLAY_MANAGEMENT_CONFIG_ENABLED]);
    }
    
    if (tb[OVERLAY_MANAGEMENT_CONFIG_CHECK_INTERVAL]) {
        config.check_interval = blobmsg_get_u32(tb[OVERLAY_MANAGEMENT_CONFIG_CHECK_INTERVAL]);
    }
    
    if (tb[OVERLAY_MANAGEMENT_CONFIG_CLEANUP_THRESHOLD]) {
        config.cleanup_threshold = blobmsg_get_u32(tb[OVERLAY_MANAGEMENT_CONFIG_CLEANUP_THRESHOLD]);
    }
    
    if (tb[OVERLAY_MANAGEMENT_CONFIG_EMERGENCY_THRESHOLD]) {
        config.emergency_threshold = blobmsg_get_u32(tb[OVERLAY_MANAGEMENT_CONFIG_EMERGENCY_THRESHOLD]);
    }
    
    if (tb[OVERLAY_MANAGEMENT_CONFIG_AUTO_CLEANUP]) {
        config.auto_cleanup = blobmsg_get_bool(tb[OVERLAY_MANAGEMENT_CONFIG_AUTO_CLEANUP]);
    }
    
    if (tb[OVERLAY_MANAGEMENT_CONFIG_BACKUP_BEFORE_CLEANUP]) {
        config.backup_before_cleanup = blobmsg_get_bool(tb[OVERLAY_MANAGEMENT_CONFIG_BACKUP_BEFORE_CLEANUP]);
    }
    
    // Apply the new configuration
    result = overlay_management_set_config(&config);
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to set overlay management configuration");
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
 * Enable/disable overlay management
 */
static int autonomy_overlay_management_set_enabled(struct ubus_context *ctx, struct ubus_request_data *req,
                                                 const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    struct blob_attr *tb[1];
    enum {
        OVERLAY_MANAGEMENT_ENABLED = 0,
        __OVERLAY_MANAGEMENT_ENABLED_MAX
    };
    
    static const struct blobmsg_policy policy[__OVERLAY_MANAGEMENT_ENABLED_MAX] = {
        [OVERLAY_MANAGEMENT_ENABLED] = { .name = "enabled", .type = BLOBMSG_TYPE_BOOL },
    };
    
    blobmsg_parse(policy, __OVERLAY_MANAGEMENT_ENABLED_MAX, tb, blob_data(msg), blob_len(msg));
    
    if (!tb[OVERLAY_MANAGEMENT_ENABLED]) {
        blobmsg_add_string(&bb, "error", "Missing 'enabled' parameter");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    bool enabled = blobmsg_get_bool(tb[OVERLAY_MANAGEMENT_ENABLED]);
    int result = overlay_management_set_enabled(enabled);
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to set overlay management enabled state");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    blobmsg_add_string(&bb, "status", enabled ? "Overlay management enabled" : "Overlay management disabled");
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Reset overlay management statistics
 */
static int autonomy_overlay_management_reset(struct ubus_context *ctx, struct ubus_request_data *req,
                                           const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    int result = overlay_management_reset();
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to reset overlay management statistics");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    blobmsg_add_string(&bb, "status", "Overlay management statistics reset successfully");
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Manually trigger overlay management check
 */
static int autonomy_overlay_management_check(struct ubus_context *ctx, struct ubus_request_data *req,
                                           const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    int result = overlay_management_check();
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Overlay management check failed");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    blobmsg_add_string(&bb, "status", "Overlay management check completed successfully");
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Manually trigger overlay management cleanup
 */
static int autonomy_overlay_management_cleanup(struct ubus_context *ctx, struct ubus_request_data *req,
                                             const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    // Parse message to determine cleanup type
    struct blob_attr *tb[1];
    enum {
        OVERLAY_MANAGEMENT_CLEANUP_TYPE = 0,
        __OVERLAY_MANAGEMENT_CLEANUP_MAX
    };
    
    static const struct blobmsg_policy policy[__OVERLAY_MANAGEMENT_CLEANUP_MAX] = {
        [OVERLAY_MANAGEMENT_CLEANUP_TYPE] = { .name = "type", .type = BLOBMSG_TYPE_STRING },
    };
    
    blobmsg_parse(policy, __OVERLAY_MANAGEMENT_CLEANUP_MAX, tb, blob_data(msg), blob_len(msg));
    
    const char *cleanup_type = "routine";
    if (tb[OVERLAY_MANAGEMENT_CLEANUP_TYPE]) {
        cleanup_type = blobmsg_get_string(tb[OVERLAY_MANAGEMENT_CLEANUP_TYPE]);
    }
    
    int result;
    if (strcmp(cleanup_type, "emergency") == 0) {
        result = overlay_management_perform_emergency_cleanup();
    } else {
        result = overlay_management_perform_cleanup();
    }
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Overlay management cleanup failed");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    blobmsg_add_string(&bb, "status", "Overlay management cleanup completed successfully");
    blobmsg_add_string(&bb, "type", cleanup_type);
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Register overlay management UBUS object
 */
int overlay_management_ubus_register(struct ubus_context *ctx) {
    int ret = ubus_add_object(ctx, &autonomy_overlay_management_obj);
    if (ret) {
        fprintf(stderr, "Failed to add overlay management object: %s\n", ubus_strerror(ret));
        return ret;
    }
    
    fprintf(stderr, "Overlay management UBUS object registered successfully\n");
    return 0;
}

/**
 * Unregister overlay management UBUS object
 */
void overlay_management_ubus_unregister(struct ubus_context *ctx) {
    ubus_remove_object(ctx, &autonomy_overlay_management_obj);
    fprintf(stderr, "Overlay management UBUS object unregistered\n");
}
