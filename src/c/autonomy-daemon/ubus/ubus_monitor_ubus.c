#include "ubus_monitor.h"
#include <libubus.h>
#include <libubox/blobmsg.h>
#include <libubox/blobmsg_json.h>
#include <json-c/json.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <sys/socket.h>

// Forward declarations
static int autonomy_ubus_monitor_status(struct ubus_context *ctx, struct ubus_request_data *req,
                                      const char *method, struct blob_attr *msg);
static int autonomy_ubus_monitor_config(struct ubus_context *ctx, struct ubus_request_data *req,
                                       const char *method, struct blob_attr *msg);
int autonomy_ubus_monitor_set_config(struct ubus_context *ctx, struct ubus_request_data *req,
                                           const char *method, struct blob_attr *msg);
static int autonomy_ubus_monitor_set_enabled(struct ubus_context *ctx, struct ubus_request_data *req,
                                            const char *method, struct blob_attr *msg);
static int autonomy_ubus_monitor_reset(struct ubus_context *ctx, struct ubus_request_data *req,
                                      const char *method, struct blob_attr *msg);
int autonomy_ubus_monitor_check(struct ubus_context *ctx, struct ubus_request_data *req,
                                      const char *method, struct blob_attr *msg);

// UBUS method definitions
static const struct ubus_method_type autonomy_ubus_monitor_methods[] = {
    UBUS_METHOD("status", autonomy_ubus_monitor_status, 0),
    UBUS_METHOD("config", autonomy_ubus_monitor_config, 0),
    UBUS_METHOD("set_config", autonomy_ubus_monitor_set_config, 0),
    UBUS_METHOD("set_enabled", autonomy_ubus_monitor_set_enabled, 0),
    UBUS_METHOD("reset", autonomy_ubus_monitor_reset, 0),
    UBUS_METHOD("check", autonomy_ubus_monitor_check, 0),
};

// UBUS object type
static const struct ubus_object_type autonomy_ubus_monitor_obj_type = {
    .name = "autonomy_ubus_monitor",
    .methods = autonomy_ubus_monitor_methods,
    .n_methods = ARRAY_SIZE(autonomy_ubus_monitor_methods),
};

// UBUS object
static const struct ubus_object autonomy_ubus_monitor_obj = {
    .name = "ubus_monitor",
    .type = &autonomy_ubus_monitor_obj_type,
    .methods = autonomy_ubus_monitor_methods,
    .n_methods = ARRAY_SIZE(autonomy_ubus_monitor_methods),
};

/**
 * Get UBUS monitor status
 */
static int autonomy_ubus_monitor_status(struct ubus_context *ctx, struct ubus_request_data *req,
                                      const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    ubus_monitor_status_t status;
    int result = ubus_monitor_get_status(&status);
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to get UBUS monitor status");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    // Add configuration
    blobmsg_add_u8(&bb, "enabled", status.enabled);
    blobmsg_add_u32(&bb, "check_interval", status.check_interval);
    blobmsg_add_u8(&bb, "auto_restart", status.auto_restart);
    blobmsg_add_u8(&bb, "monitor_critical_services", status.monitor_critical_services);
    
    // Add statistics
    blobmsg_add_u32(&bb, "last_check_time", status.last_check_time);
    blobmsg_add_u32(&bb, "total_checks", status.total_checks);
    blobmsg_add_u32(&bb, "total_restarts", status.total_restarts);
    blobmsg_add_u32(&bb, "total_errors", status.total_errors);
    
    // Add current health info
    blobmsg_add_u8(&bb, "ubus_healthy", status.ubus_healthy);
    blobmsg_add_u8(&bb, "rpcd_running", status.rpcd_running);
    blobmsg_add_u32(&bb, "ubus_socket_accessible", status.ubus_socket_accessible);
    blobmsg_add_u32(&bb, "available_services", status.available_services);
    
    // Add last error if any
    if (status.last_error) {
        blobmsg_add_string(&bb, "last_error", status.last_error);
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Get UBUS monitor configuration
 */
static int autonomy_ubus_monitor_config(struct ubus_context *ctx, struct ubus_request_data *req,
                                       const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    ubus_monitor_config_t config;
    int result = ubus_monitor_get_config(&config);
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to get UBUS monitor configuration");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    blobmsg_add_u8(&bb, "enabled", config.enabled);
    blobmsg_add_u32(&bb, "check_interval", config.check_interval);
    blobmsg_add_u8(&bb, "auto_restart", config.auto_restart);
    blobmsg_add_u8(&bb, "monitor_critical_services", config.monitor_critical_services);
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Set UBUS monitor configuration
 */
int autonomy_ubus_monitor_set_config(struct ubus_context *ctx, struct ubus_request_data *req,
                                           const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    ubus_monitor_config_t config;
    int result = ubus_monitor_get_config(&config); // Get current config first
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to get current configuration");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    // Parse incoming message and update config
    struct blob_attr *tb[__UBUS_MONITOR_CONFIG_MAX];
    enum {
        UBUS_MONITOR_CONFIG_ENABLED = 0,
        UBUS_MONITOR_CONFIG_CHECK_INTERVAL,
        UBUS_MONITOR_CONFIG_AUTO_RESTART,
        UBUS_MONITOR_CONFIG_MONITOR_CRITICAL_SERVICES,
        __UBUS_MONITOR_CONFIG_MAX
    };
    
    static const struct blobmsg_policy policy[__UBUS_MONITOR_CONFIG_MAX] = {
        [UBUS_MONITOR_CONFIG_ENABLED] = { .name = "enabled", .type = BLOBMSG_TYPE_BOOL },
        [UBUS_MONITOR_CONFIG_CHECK_INTERVAL] = { .name = "check_interval", .type = BLOBMSG_TYPE_INT32 },
        [UBUS_MONITOR_CONFIG_AUTO_RESTART] = { .name = "auto_restart", .type = BLOBMSG_TYPE_BOOL },
        [UBUS_MONITOR_CONFIG_MONITOR_CRITICAL_SERVICES] = { .name = "monitor_critical_services", .type = BLOBMSG_TYPE_BOOL },
    };
    
    blobmsg_parse(policy, __UBUS_MONITOR_CONFIG_MAX, tb, blob_data(msg), blob_len(msg));
    
    // Update configuration with new values
    if (tb[UBUS_MONITOR_CONFIG_ENABLED]) {
        config.enabled = blobmsg_get_bool(tb[UBUS_MONITOR_CONFIG_ENABLED]);
    }
    
    if (tb[UBUS_MONITOR_CONFIG_CHECK_INTERVAL]) {
        config.check_interval = blobmsg_get_u32(tb[UBUS_MONITOR_CONFIG_CHECK_INTERVAL]);
    }
    
    if (tb[UBUS_MONITOR_CONFIG_AUTO_RESTART]) {
        config.auto_restart = blobmsg_get_bool(tb[UBUS_MONITOR_CONFIG_AUTO_RESTART]);
    }
    
    if (tb[UBUS_MONITOR_CONFIG_MONITOR_CRITICAL_SERVICES]) {
        config.monitor_critical_services = blobmsg_get_bool(tb[UBUS_MONITOR_CONFIG_MONITOR_CRITICAL_SERVICES]);
    }
    
    // Apply the new configuration
    result = ubus_monitor_set_config(&config);
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to set UBUS monitor configuration");
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
 * Enable/disable UBUS monitor
 */
static int autonomy_ubus_monitor_set_enabled(struct ubus_context *ctx, struct ubus_request_data *req,
                                            const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    struct blob_attr *tb[1];
    enum {
        UBUS_MONITOR_ENABLED = 0,
        __UBUS_MONITOR_ENABLED_MAX
    };
    
    static const struct blobmsg_policy policy[__UBUS_MONITOR_ENABLED_MAX] = {
        [UBUS_MONITOR_ENABLED] = { .name = "enabled", .type = BLOBMSG_TYPE_BOOL },
    };
    
    blobmsg_parse(policy, __UBUS_MONITOR_ENABLED_MAX, tb, blob_data(msg), blob_len(msg));
    
    if (!tb[UBUS_MONITOR_ENABLED]) {
        blobmsg_add_string(&bb, "error", "Missing 'enabled' parameter");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    bool enabled = blobmsg_get_bool(tb[UBUS_MONITOR_ENABLED]);
    int result = ubus_monitor_set_enabled(enabled);
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to set UBUS monitor enabled state");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    blobmsg_add_string(&bb, "status", enabled ? "UBUS monitor enabled" : "UBUS monitor disabled");
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Reset UBUS monitor statistics
 */
static int autonomy_ubus_monitor_reset(struct ubus_context *ctx, struct ubus_request_data *req,
                                      const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    int result = ubus_monitor_reset();
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to reset UBUS monitor statistics");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    blobmsg_add_string(&bb, "status", "UBUS monitor statistics reset successfully");
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Manually trigger UBUS monitor check
 */
int autonomy_ubus_monitor_check(struct ubus_context *ctx, struct ubus_request_data *req,
                                      const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    int result = ubus_monitor_check_ubus_health();
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "UBUS monitor check failed");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    blobmsg_add_string(&bb, "status", "UBUS monitor check completed successfully");
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Register UBUS monitor UBUS object
 */
int ubus_monitor_ubus_register(struct ubus_context *ctx) {
    int ret = ubus_add_object(ctx, &autonomy_ubus_monitor_obj);
    if (ret) {
        fprintf(stderr, "Failed to add UBUS monitor object: %s\n", ubus_strerror(ret));
        return ret;
    }
    
    fprintf(stderr, "UBUS monitor UBUS object registered successfully\n");
    return 0;
}

/**
 * Unregister UBUS monitor UBUS object
 */
void ubus_monitor_ubus_unregister(struct ubus_context *ctx) {
    ubus_remove_object(ctx, &autonomy_ubus_monitor_obj);
    fprintf(stderr, "UBUS monitor UBUS object unregistered\n");
}
