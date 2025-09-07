#include "service_watchdog.h"
#include <libubus.h>
#include <libubox/blobmsg.h>
#include <libubox/blobmsg_json.h>
#include <json-c/json.h>

// Forward declarations
static int autonomy_service_watchdog_status(struct ubus_context *ctx, struct ubus_request_data *req,
                                         const char *method, struct blob_attr *msg);
static int autonomy_service_watchdog_config(struct ubus_context *ctx, struct ubus_request_data *req,
                                          const char *method, struct blob_attr *msg);
static int autonomy_service_watchdog_set_config(struct ubus_context *ctx, struct ubus_request_data *req,
                                              const char *method, struct blob_attr *msg);
static int autonomy_service_watchdog_set_enabled(struct ubus_context *ctx, struct ubus_request_data *req,
                                               const char *method, struct blob_attr *msg);
static int autonomy_service_watchdog_reset(struct ubus_context *ctx, struct ubus_request_data *req,
                                         const char *method, struct blob_attr *msg);
static int autonomy_service_watchdog_check(struct ubus_context *ctx, struct ubus_request_data *req,
                                         const char *method, struct blob_attr *msg);

// UBUS method definitions
static const struct ubus_method_type autonomy_service_watchdog_methods[] = {
    UBUS_METHOD("status", autonomy_service_watchdog_status, 0),
    UBUS_METHOD("config", autonomy_service_watchdog_config, 0),
    UBUS_METHOD("set_config", autonomy_service_watchdog_set_config, 0),
    UBUS_METHOD("set_enabled", autonomy_service_watchdog_set_enabled, 0),
    UBUS_METHOD("reset", autonomy_service_watchdog_reset, 0),
    UBUS_METHOD("check", autonomy_service_watchdog_check, 0),
};

// UBUS object type
static const struct ubus_object_type autonomy_service_watchdog_obj_type = {
    .name = "autonomy_service_watchdog",
    .methods = autonomy_service_watchdog_methods,
    .n_methods = ARRAY_SIZE(autonomy_service_watchdog_methods),
};

// UBUS object
static const struct ubus_object autonomy_service_watchdog_obj = {
    .name = "service_watchdog",
    .type = &autonomy_service_watchdog_obj_type,
    .methods = autonomy_service_watchdog_methods,
    .n_methods = ARRAY_SIZE(autonomy_service_watchdog_methods),
};

/**
 * Get service watchdog status
 */
static int autonomy_service_watchdog_status(struct ubus_context *ctx, struct ubus_request_data *req,
                                         const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    service_watchdog_status_t status;
    int result = service_watchdog_get_status(&status);
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to get service watchdog status");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    // Add configuration
    blobmsg_add_u8(&bb, "enabled", status.enabled);
    blobmsg_add_u32(&bb, "service_timeout", status.service_timeout);
    blobmsg_add_u8(&bb, "auto_restart", status.auto_restart);
    blobmsg_add_u32(&bb, "max_restart_attempts", status.max_restart_attempts);
    blobmsg_add_u32(&bb, "restart_cooldown", status.restart_cooldown);
    
    // Add services to monitor
    struct blob_attr *services_array = blobmsg_open_array(&bb, "services_to_monitor");
    for (int i = 0; i < status.services_count; i++) {
        blobmsg_add_string(&bb, NULL, status.services_to_monitor[i]);
    }
    blobmsg_close_array(&bb, services_array);
    
    blobmsg_add_u32(&bb, "services_count", status.services_count);
    
    // Add statistics
    blobmsg_add_u32(&bb, "last_check_time", status.last_check_time);
    blobmsg_add_u32(&bb, "services_checked", status.services_checked);
    blobmsg_add_u32(&bb, "services_restarted", status.services_restarted);
    blobmsg_add_u32(&bb, "services_killed", status.services_killed);
    blobmsg_add_u32(&bb, "last_restart_time", status.last_restart_time);
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Get service watchdog configuration
 */
static int autonomy_service_watchdog_config(struct ubus_context *ctx, struct ubus_request_data *req,
                                          const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    service_watchdog_config_t config;
    int result = service_watchdog_get_config(&config);
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to get service watchdog configuration");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    blobmsg_add_u8(&bb, "enabled", config.enabled);
    blobmsg_add_u32(&bb, "service_timeout", config.service_timeout);
    blobmsg_add_u8(&bb, "auto_restart", config.auto_restart);
    blobmsg_add_u32(&bb, "max_restart_attempts", config.max_restart_attempts);
    blobmsg_add_u32(&bb, "restart_cooldown", config.restart_cooldown);
    
    // Add services to monitor
    struct blob_attr *services_array = blobmsg_open_array(&bb, "services_to_monitor");
    for (int i = 0; i < config.services_count; i++) {
        blobmsg_add_string(&bb, NULL, config.services_to_monitor[i]);
    }
    blobmsg_close_array(&bb, services_array);
    
    blobmsg_add_u32(&bb, "services_count", config.services_count);
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Set service watchdog configuration
 */
static int autonomy_service_watchdog_set_config(struct ubus_context *ctx, struct ubus_request_data *req,
                                              const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    service_watchdog_config_t config;
    int result = service_watchdog_get_config(&config); // Get current config first
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to get current configuration");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    // Parse incoming message and update config
    struct blob_attr *tb[__SERVICE_WATCHDOG_CONFIG_MAX];
    enum {
        SERVICE_WATCHDOG_CONFIG_ENABLED = 0,
        SERVICE_WATCHDOG_CONFIG_SERVICE_TIMEOUT,
        SERVICE_WATCHDOG_CONFIG_AUTO_RESTART,
        SERVICE_WATCHDOG_CONFIG_MAX_RESTART_ATTEMPTS,
        SERVICE_WATCHDOG_CONFIG_RESTART_COOLDOWN,
        SERVICE_WATCHDOG_CONFIG_SERVICES_TO_MONITOR,
        SERVICE_WATCHDOG_CONFIG_SERVICES_COUNT,
        __SERVICE_WATCHDOG_CONFIG_MAX
    };
    
    static const struct blobmsg_policy policy[__SERVICE_WATCHDOG_CONFIG_MAX] = {
        [SERVICE_WATCHDOG_CONFIG_ENABLED] = { .name = "enabled", .type = BLOBMSG_TYPE_BOOL },
        [SERVICE_WATCHDOG_CONFIG_SERVICE_TIMEOUT] = { .name = "service_timeout", .type = BLOBMSG_TYPE_INT32 },
        [SERVICE_WATCHDOG_CONFIG_AUTO_RESTART] = { .name = "auto_restart", .type = BLOBMSG_TYPE_BOOL },
        [SERVICE_WATCHDOG_CONFIG_MAX_RESTART_ATTEMPTS] = { .name = "max_restart_attempts", .type = BLOBMSG_TYPE_INT32 },
        [SERVICE_WATCHDOG_CONFIG_RESTART_COOLDOWN] = { .name = "restart_cooldown", .type = BLOBMSG_TYPE_INT32 },
        [SERVICE_WATCHDOG_CONFIG_SERVICES_TO_MONITOR] = { .name = "services_to_monitor", .type = BLOBMSG_TYPE_ARRAY },
        [SERVICE_WATCHDOG_CONFIG_SERVICES_COUNT] = { .name = "services_count", .type = BLOBMSG_TYPE_INT32 },
    };
    
    blobmsg_parse(policy, __SERVICE_WATCHDOG_CONFIG_MAX, tb, blob_data(msg), blob_len(msg));
    
    // Update configuration with new values
    if (tb[SERVICE_WATCHDOG_CONFIG_ENABLED]) {
        config.enabled = blobmsg_get_bool(tb[SERVICE_WATCHDOG_CONFIG_ENABLED]);
    }
    
    if (tb[SERVICE_WATCHDOG_CONFIG_SERVICE_TIMEOUT]) {
        config.service_timeout = blobmsg_get_u32(tb[SERVICE_WATCHDOG_CONFIG_SERVICE_TIMEOUT]);
    }
    
    if (tb[SERVICE_WATCHDOG_CONFIG_AUTO_RESTART]) {
        config.auto_restart = blobmsg_get_bool(tb[SERVICE_WATCHDOG_CONFIG_AUTO_RESTART]);
    }
    
    if (tb[SERVICE_WATCHDOG_CONFIG_MAX_RESTART_ATTEMPTS]) {
        config.max_restart_attempts = blobmsg_get_u32(tb[SERVICE_WATCHDOG_CONFIG_MAX_RESTART_ATTEMPTS]);
    }
    
    if (tb[SERVICE_WATCHDOG_CONFIG_RESTART_COOLDOWN]) {
        config.restart_cooldown = blobmsg_get_u32(tb[SERVICE_WATCHDOG_CONFIG_RESTART_COOLDOWN]);
    }
    
    if (tb[SERVICE_WATCHDOG_CONFIG_SERVICES_TO_MONITOR]) {
        struct blob_attr *services_array = tb[SERVICE_WATCHDOG_CONFIG_SERVICES_TO_MONITOR];
        size_t services_count = blobmsg_check_array(services_array, BLOBMSG_TYPE_STRING);
        
        if (services_count > 0 && services_count <= MAX_SERVICES_TO_MONITOR) {
            config.services_count = services_count;
            
            struct blob_attr *attr;
            size_t rem;
            int i = 0;
            blobmsg_for_each_attr(attr, services_array, rem) {
                if (i < MAX_SERVICES_TO_MONITOR) {
                    strncpy(config.services_to_monitor[i], blobmsg_get_string(attr), 63);
                    config.services_to_monitor[i][63] = '\0';
                    i++;
                }
            }
        }
    }
    
    // Apply the new configuration
    result = service_watchdog_set_config(&config);
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to set service watchdog configuration");
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
 * Enable/disable service watchdog
 */
static int autonomy_service_watchdog_set_enabled(struct ubus_context *ctx, struct ubus_request_data *req,
                                               const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    struct blob_attr *tb[1];
    enum {
        SERVICE_WATCHDOG_ENABLED = 0,
        __SERVICE_WATCHDOG_ENABLED_MAX
    };
    
    static const struct blobmsg_policy policy[__SERVICE_WATCHDOG_ENABLED_MAX] = {
        [SERVICE_WATCHDOG_ENABLED] = { .name = "enabled", .type = BLOBMSG_TYPE_BOOL },
    };
    
    blobmsg_parse(policy, __SERVICE_WATCHDOG_ENABLED_MAX, tb, blob_data(msg), blob_len(msg));
    
    if (!tb[SERVICE_WATCHDOG_ENABLED]) {
        blobmsg_add_string(&bb, "error", "Missing 'enabled' parameter");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    bool enabled = blobmsg_get_bool(tb[SERVICE_WATCHDOG_ENABLED]);
    int result = service_watchdog_set_enabled(enabled);
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to set service watchdog enabled state");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    blobmsg_add_string(&bb, "status", enabled ? "Service watchdog enabled" : "Service watchdog disabled");
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Reset service watchdog statistics
 */
static int autonomy_service_watchdog_reset(struct ubus_context *ctx, struct ubus_request_data *req,
                                         const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    int result = service_watchdog_reset();
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to reset service watchdog statistics");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    blobmsg_add_string(&bb, "status", "Service watchdog statistics reset successfully");
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Manually trigger service watchdog check
 */
static int autonomy_service_watchdog_check(struct ubus_context *ctx, struct ubus_request_data *req,
                                         const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    int result = service_watchdog_check();
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Service watchdog check failed");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    blobmsg_add_string(&bb, "status", "Service watchdog check completed successfully");
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Register service watchdog UBUS object
 */
int service_watchdog_ubus_register(struct ubus_context *ctx) {
    int ret = ubus_add_object(ctx, &autonomy_service_watchdog_obj);
    if (ret) {
        fprintf(stderr, "Failed to add service watchdog object: %s\n", ubus_strerror(ret));
        return ret;
    }
    
    fprintf(stderr, "Service watchdog UBUS object registered successfully\n");
    return 0;
}

/**
 * Unregister service watchdog UBUS object
 */
void service_watchdog_ubus_unregister(struct ubus_context *ctx) {
    ubus_remove_object(ctx, &autonomy_service_watchdog_obj);
    fprintf(stderr, "Service watchdog UBUS object unregistered\n");
}
