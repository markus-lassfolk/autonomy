#include "overlay_management.h"
#include <time.h>
#include <libubus.h>
#include <libubox/blobmsg.h>
#include <libubox/blobmsg_json.h>
#include <json-c/json.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// UBUS policy enums
enum {
    OVERLAY_MANAGEMENT_CONFIG_ENABLED,
    OVERLAY_MANAGEMENT_CONFIG_OVERLAY_SPACE_THRESHOLD,
    OVERLAY_MANAGEMENT_CONFIG_OVERLAY_CRITICAL_THRESHOLD,
    OVERLAY_MANAGEMENT_CONFIG_CLEANUP_RETENTION_DAYS,
    OVERLAY_MANAGEMENT_CONFIG_NOTIFICATIONS_ENABLED,
    OVERLAY_MANAGEMENT_CONFIG_NOTIFY_ON_FIXES,
    OVERLAY_MANAGEMENT_CONFIG_NOTIFY_ON_CRITICAL,
    OVERLAY_MANAGEMENT_CONFIG_CHECK_INTERVAL,
    OVERLAY_MANAGEMENT_CONFIG_CLEANUP_THRESHOLD,
    OVERLAY_MANAGEMENT_CONFIG_EMERGENCY_THRESHOLD,
    OVERLAY_MANAGEMENT_CONFIG_AUTO_CLEANUP,
    OVERLAY_MANAGEMENT_CONFIG_BACKUP_BEFORE_CLEANUP,
    __OVERLAY_MANAGEMENT_CONFIG_MAX
};

// Forward declarations
static int autonomy_overlay_management_status(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req,
                                           const char *method, struct blob_attr *msg\n"\n"\n"\n"\n"\n"\n"\n");
static int autonomy_overlay_management_config(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req,
                                            const char *method, struct blob_attr *msg\n"\n"\n"\n"\n"\n"\n"\n");
int autonomy_overlay_management_set_config(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req,
                                                const char *method, struct blob_attr *msg\n"\n"\n"\n"\n"\n"\n"\n");
static int autonomy_overlay_management_set_enabled(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req,
                                                 const char *method, struct blob_attr *msg\n"\n"\n"\n"\n"\n"\n"\n");
static int autonomy_overlay_management_reset(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req,
                                           const char *method, struct blob_attr *msg\n"\n"\n"\n"\n"\n"\n"\n");
int autonomy_overlay_management_check(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req,
                                           const char *method, struct blob_attr *msg\n"\n"\n"\n"\n"\n"\n"\n");
int autonomy_overlay_management_cleanup(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req,
                                             const char *method, struct blob_attr *msg\n"\n"\n"\n"\n"\n"\n"\n");

// UBUS method definitions
static const struct ubus_method autonomy_overlay_management_methods[] = {
    UBUS_METHOD_NOARG("status", autonomy_overlay_management_status),
    UBUS_METHOD_NOARG("config", autonomy_overlay_management_config),
    UBUS_METHOD_NOARG("set_config", autonomy_overlay_management_set_config),
    UBUS_METHOD_NOARG("set_enabled", autonomy_overlay_management_set_enabled),
    UBUS_METHOD_NOARG("reset", autonomy_overlay_management_reset),
    UBUS_METHOD_NOARG("check", autonomy_overlay_management_check),
    UBUS_METHOD_NOARG("cleanup", autonomy_overlay_management_cleanup),
};

// UBUS object type
static struct ubus_object_type autonomy_overlay_management_obj_type = {
    .name = "autonomy_overlay_management",
    .methods = autonomy_overlay_management_methods,
    .n_methods = ARRAY_SIZE(autonomy_overlay_management_methods),
};

// UBUS object
static struct ubus_object autonomy_overlay_management_obj = {
    .name = "overlay_management",
    .type = &autonomy_overlay_management_obj_type,
    .methods = autonomy_overlay_management_methods,
    .n_methods = ARRAY_SIZE(autonomy_overlay_management_methods),
};

/**
 * Get overlay management status
 */
static int autonomy_overlay_management_status(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req,
                                           const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    overlay_management_status_t status;
    int result = overlay_management_get_status(&status\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to get overlay management status"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return 0;
    }
    
    // Add configuration
    blobmsg_add_u8(&bb, "enabled", status.enabled\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "check_interval", status.check_interval\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "cleanup_threshold", status.cleanup_threshold\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "emergency_threshold", status.emergency_threshold\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(&bb, "auto_cleanup", status.auto_cleanup\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(&bb, "backup_before_cleanup", status.backup_before_cleanup\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Add statistics
    blobmsg_add_u32(&bb, "last_check_time", status.last_check_time\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "last_cleanup_time", status.last_cleanup_time\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "total_cleanups", status.total_cleanups\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "total_emergency_cleanups", status.total_emergency_cleanups\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "total_space_freed", status.total_space_freed\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Add current usage
    blobmsg_add_u32(&bb, "current_usage_percent", status.current_usage_percent\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "current_usage_bytes", status.current_usage_bytes\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "total_space_bytes", status.total_space_bytes\n"\n"\n"\n"\n"\n"\n"\n");
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return 0;
}

/**
 * Get overlay management configuration
 */
static int autonomy_overlay_management_config(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req,
                                            const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    overlay_management_config_t config;
    int result = overlay_management_get_config(&config\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to get overlay management configuration"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return 0;
    }
    
    blobmsg_add_u8(&bb, "enabled", config.enabled\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "check_interval", config.check_interval\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "cleanup_threshold", config.cleanup_threshold\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "emergency_threshold", config.emergency_threshold\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(&bb, "auto_cleanup", config.auto_cleanup\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(&bb, "backup_before_cleanup", config.backup_before_cleanup\n"\n"\n"\n"\n"\n"\n"\n");
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return 0;
}

/**
 * Set overlay management configuration
 */
int autonomy_overlay_management_set_config(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req,
                                                const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    overlay_management_config_t config;
    int result = overlay_management_get_config(&config); // Get current config first
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to get current configuration"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
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
    
    blobmsg_parse(policy, __OVERLAY_MANAGEMENT_CONFIG_MAX, tb, blob_data(msg), blob_len(msg)\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Update configuration with new values
    if (tb[OVERLAY_MANAGEMENT_CONFIG_ENABLED]) {
        config.enabled = blobmsg_get_bool(tb[OVERLAY_MANAGEMENT_CONFIG_ENABLED]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (tb[OVERLAY_MANAGEMENT_CONFIG_CHECK_INTERVAL]) {
        config.check_interval = blobmsg_get_u32(tb[OVERLAY_MANAGEMENT_CONFIG_CHECK_INTERVAL]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (tb[OVERLAY_MANAGEMENT_CONFIG_CLEANUP_THRESHOLD]) {
        config.cleanup_threshold = blobmsg_get_u32(tb[OVERLAY_MANAGEMENT_CONFIG_CLEANUP_THRESHOLD]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (tb[OVERLAY_MANAGEMENT_CONFIG_EMERGENCY_THRESHOLD]) {
        config.emergency_threshold = blobmsg_get_u32(tb[OVERLAY_MANAGEMENT_CONFIG_EMERGENCY_THRESHOLD]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (tb[OVERLAY_MANAGEMENT_CONFIG_AUTO_CLEANUP]) {
        config.auto_cleanup = blobmsg_get_bool(tb[OVERLAY_MANAGEMENT_CONFIG_AUTO_CLEANUP]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (tb[OVERLAY_MANAGEMENT_CONFIG_BACKUP_BEFORE_CLEANUP]) {
        config.backup_before_cleanup = blobmsg_get_bool(tb[OVERLAY_MANAGEMENT_CONFIG_BACKUP_BEFORE_CLEANUP]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Apply the new configuration
    result = overlay_management_set_config(&config\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to set overlay management configuration"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return 0;
    }
    
    blobmsg_add_string(&bb, "status", "Configuration updated successfully"\n"\n"\n"\n"\n"\n"\n"\n");
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return 0;
}

/**
 * Enable/disable overlay management
 */
static int autonomy_overlay_management_set_enabled(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req,
                                                 const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    struct blob_attr *tb[1];
    enum {
        OVERLAY_MANAGEMENT_ENABLED = 0,
        __OVERLAY_MANAGEMENT_ENABLED_MAX
    };
    
    static const struct blobmsg_policy policy[__OVERLAY_MANAGEMENT_ENABLED_MAX] = {
        [OVERLAY_MANAGEMENT_ENABLED] = { .name = "enabled", .type = BLOBMSG_TYPE_BOOL },
    };
    
    blobmsg_parse(policy, __OVERLAY_MANAGEMENT_ENABLED_MAX, tb, blob_data(msg), blob_len(msg)\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!tb[OVERLAY_MANAGEMENT_ENABLED]) {
        blobmsg_add_string(&bb, "error", "Missing 'enabled' parameter"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return 0;
    }
    
    bool enabled = blobmsg_get_bool(tb[OVERLAY_MANAGEMENT_ENABLED]\n"\n"\n"\n"\n"\n"\n"\n");
    int result = overlay_management_set_enabled(enabled\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to set overlay management enabled state"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return 0;
    }
    
    blobmsg_add_string(&bb, "status", enabled ? "Overlay management enabled" : "Overlay management disabled"\n"\n"\n"\n"\n"\n"\n"\n");
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return 0;
}

/**
 * Reset overlay management statistics
 */
static int autonomy_overlay_management_reset(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req,
                                           const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    int result = overlay_management_reset(\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to reset overlay management statistics"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return 0;
    }
    
    blobmsg_add_string(&bb, "status", "Overlay management statistics reset successfully"\n"\n"\n"\n"\n"\n"\n"\n");
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return 0;
}

/**
 * Manually trigger overlay management check
 */
int autonomy_overlay_management_check(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req,
                                           const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    int result = overlay_management_check(\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Overlay management check failed"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return 0;
    }
    
    blobmsg_add_string(&bb, "status", "Overlay management check completed successfully"\n"\n"\n"\n"\n"\n"\n"\n");
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return 0;
}

/**
 * Manually trigger overlay management cleanup
 */
int autonomy_overlay_management_cleanup(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req,
                                             const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Parse message to determine cleanup type
    struct blob_attr *tb[1];
    enum {
        OVERLAY_MANAGEMENT_CLEANUP_TYPE = 0,
        __OVERLAY_MANAGEMENT_CLEANUP_MAX
    };
    
    static const struct blobmsg_policy policy[__OVERLAY_MANAGEMENT_CLEANUP_MAX] = {
        [OVERLAY_MANAGEMENT_CLEANUP_TYPE] = { .name = "type", .type = BLOBMSG_TYPE_STRING },
    };
    
    blobmsg_parse(policy, __OVERLAY_MANAGEMENT_CLEANUP_MAX, tb, blob_data(msg), blob_len(msg)\n"\n"\n"\n"\n"\n"\n"\n");
    
    const char *cleanup_type = "routine";
    if (tb[OVERLAY_MANAGEMENT_CLEANUP_TYPE]) {
        cleanup_type = blobmsg_get_string(tb[OVERLAY_MANAGEMENT_CLEANUP_TYPE]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    int result;
    if (strcmp(cleanup_type, "emergency") == 0) {
        result = overlay_management_perform_emergency_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        result = overlay_management_perform_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Overlay management cleanup failed"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return 0;
    }
    
    blobmsg_add_string(&bb, "status", "Overlay management cleanup completed successfully"\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(&bb, "type", cleanup_type\n"\n"\n"\n"\n"\n"\n"\n");
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return 0;
}

/**
 * Register overlay management UBUS object
 */
int overlay_management_ubus_register(struct ubus_context *ctx) {
    int ret = ubus_add_object(ctx, &autonomy_overlay_management_obj\n"\n"\n"\n"\n"\n"\n"\n");
    if (ret) {
        fprintf(stderr, "Failed to add overlay management object: %s\n", ubus_strerror(ret)\n"\n"\n"\n"\n"\n"\n"\n");
        return ret;
    }
    
    fprintf(stderr, "Overlay management UBUS object registered successfully\n"\n"\n"\n"\n"\n"\n"\n"\n");
    return 0;
}

/**
 * Unregister overlay management UBUS object
 */
void overlay_management_ubus_unregister(struct ubus_context *ctx) {
    ubus_remove_object(ctx, &autonomy_overlay_management_obj\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "Overlay management UBUS object unregistered\n"\n"\n"\n"\n"\n"\n"\n"\n");
}
