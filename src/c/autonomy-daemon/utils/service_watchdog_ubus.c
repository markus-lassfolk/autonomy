#include "service_watchdog.h"
#include "../core/types.h"
#include <time.h>
#include <libubus.h>
#include <libubox/blobmsg.h>
#include <libubox/blobmsg_json.h>
#include <json-c/json.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <fcntl.h>

// UBUS parameter enums for service watchdog
enum {
    SERVICE_WATCHDOG_CONFIG_ENABLED,
    SERVICE_WATCHDOG_CONFIG_CHECK_INTERVAL,
    SERVICE_WATCHDOG_CONFIG_AUTO_RESTART,
    SERVICE_WATCHDOG_CONFIG_MAX_RESTARTS,
    __SERVICE_WATCHDOG_CONFIG_MAX
};

// Forward declarations
static int autonomy_service_watchdog_status(struct ubus_context *ctx, struct ubus_object *obj,
                                         struct ubus_request_data *req, const char *method, struct blob_attr *msg\n"\n"\n"\n"\n"\n"\n"\n");
static int autonomy_service_watchdog_config(struct ubus_context *ctx, struct ubus_object *obj,
                                          struct ubus_request_data *req, const char *method, struct blob_attr *msg\n"\n"\n"\n"\n"\n"\n"\n");
int autonomy_service_watchdog_set_config(struct ubus_context *ctx, struct ubus_object *obj,
                                               struct ubus_request_data *req, const char *method, struct blob_attr *msg\n"\n"\n"\n"\n"\n"\n"\n");
static int autonomy_service_watchdog_set_enabled(struct ubus_context *ctx, struct ubus_object *obj,
                                               struct ubus_request_data *req, const char *method, struct blob_attr *msg\n"\n"\n"\n"\n"\n"\n"\n");
static int autonomy_service_watchdog_reset(struct ubus_context *ctx, struct ubus_object *obj,
                                         struct ubus_request_data *req, const char *method, struct blob_attr *msg\n"\n"\n"\n"\n"\n"\n"\n");
int autonomy_service_watchdog_check(struct ubus_context *ctx, struct ubus_object *obj,
                                         struct ubus_request_data *req, const char *method, struct blob_attr *msg\n"\n"\n"\n"\n"\n"\n"\n");

// UBUS method definitions
static const struct ubus_method autonomy_service_watchdog_methods[] = {
    UBUS_METHOD_NOARG("status", autonomy_service_watchdog_status),
    UBUS_METHOD_NOARG("config", autonomy_service_watchdog_config),
    UBUS_METHOD_NOARG("set_config", autonomy_service_watchdog_set_config),
    UBUS_METHOD_NOARG("set_enabled", autonomy_service_watchdog_set_enabled),
    UBUS_METHOD_NOARG("reset", autonomy_service_watchdog_reset),
    UBUS_METHOD_NOARG("check", autonomy_service_watchdog_check),
};

// UBUS object type
static struct ubus_object_type autonomy_service_watchdog_obj_type = {
    .name = "autonomy_service_watchdog",
    .methods = autonomy_service_watchdog_methods,
    .n_methods = ARRAY_SIZE(autonomy_service_watchdog_methods),
};

// UBUS object
static struct ubus_object autonomy_service_watchdog_obj = {
    .name = "service_watchdog",
    .type = &autonomy_service_watchdog_obj_type,
    .methods = autonomy_service_watchdog_methods,
    .n_methods = ARRAY_SIZE(autonomy_service_watchdog_methods),
};

/**
 * Get service watchdog status
 */
static int autonomy_service_watchdog_status(struct ubus_context *ctx, struct ubus_object *obj,
                                         struct ubus_request_data *req, const char *method, struct blob_attr *msg) {
    (void)obj; // Suppress unused parameter warning
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    service_watchdog_status_t status;
    int result = service_watchdog_get_status(&status\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to get service watchdog status"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return 0;
    }
    
    // Add configuration
    blobmsg_add_u8(&bb, "enabled", status.enabled\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "service_timeout", status.service_timeout\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(&bb, "auto_restart", status.auto_restart\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "max_restart_attempts", status.max_restart_attempts\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "restart_cooldown", status.restart_cooldown\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Add services to monitor
    struct blob_attr *services_array = blobmsg_open_array(&bb, "services_to_monitor"\n"\n"\n"\n"\n"\n"\n"\n");
    for (int i = 0; i < status.services_count; i++) {
        blobmsg_add_string(&bb, NULL, status.services_to_monitor[i]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    blobmsg_close_array(&bb, services_array\n"\n"\n"\n"\n"\n"\n"\n");
    
    blobmsg_add_u32(&bb, "services_count", status.services_count\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Add statistics
    blobmsg_add_u32(&bb, "last_check_time", status.last_check_time\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "services_checked", status.services_checked\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "services_restarted", status.services_restarted\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "services_killed", status.services_killed\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "last_restart_time", status.last_restart_time\n"\n"\n"\n"\n"\n"\n"\n");
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return 0;
}

/**
 * Get service watchdog configuration
 */
static int autonomy_service_watchdog_config(struct ubus_context *ctx, struct ubus_object *obj,
                                          struct ubus_request_data *req, const char *method, struct blob_attr *msg) {
    (void)obj; // Suppress unused parameter warning
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    service_watchdog_config_t config;
    int result = service_watchdog_get_config(&config\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to get service watchdog configuration"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return 0;
    }
    
    blobmsg_add_u8(&bb, "enabled", config.enabled\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "service_timeout", config.service_timeout\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(&bb, "auto_restart", config.auto_restart\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "max_restart_attempts", config.max_restart_attempts\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "restart_cooldown", config.restart_cooldown\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Add services to monitor
    struct blob_attr *services_array = blobmsg_open_array(&bb, "services_to_monitor"\n"\n"\n"\n"\n"\n"\n"\n");
    for (int i = 0; i < config.services_count; i++) {
        blobmsg_add_string(&bb, NULL, config.services_to_monitor[i]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    blobmsg_close_array(&bb, services_array\n"\n"\n"\n"\n"\n"\n"\n");
    
    blobmsg_add_u32(&bb, "services_count", config.services_count\n"\n"\n"\n"\n"\n"\n"\n");
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return 0;
}

/**
 * Set service watchdog configuration
 */
int autonomy_service_watchdog_set_config(struct ubus_context *ctx, struct ubus_object *obj,
                                               struct ubus_request_data *req, const char *method, struct blob_attr *msg) {
    (void)obj; // Suppress unused parameter warning
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    service_watchdog_config_t config;
    int result = service_watchdog_get_config(&config); // Get current config first
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to get current configuration"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
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
    
    blobmsg_parse(policy, __SERVICE_WATCHDOG_CONFIG_MAX, tb, blob_data(msg), blob_len(msg)\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Update configuration with new values
    if (tb[SERVICE_WATCHDOG_CONFIG_ENABLED]) {
        config.enabled = blobmsg_get_bool(tb[SERVICE_WATCHDOG_CONFIG_ENABLED]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (tb[SERVICE_WATCHDOG_CONFIG_SERVICE_TIMEOUT]) {
        config.service_timeout = blobmsg_get_u32(tb[SERVICE_WATCHDOG_CONFIG_SERVICE_TIMEOUT]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (tb[SERVICE_WATCHDOG_CONFIG_AUTO_RESTART]) {
        config.auto_restart = blobmsg_get_bool(tb[SERVICE_WATCHDOG_CONFIG_AUTO_RESTART]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (tb[SERVICE_WATCHDOG_CONFIG_MAX_RESTART_ATTEMPTS]) {
        config.max_restart_attempts = blobmsg_get_u32(tb[SERVICE_WATCHDOG_CONFIG_MAX_RESTART_ATTEMPTS]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (tb[SERVICE_WATCHDOG_CONFIG_RESTART_COOLDOWN]) {
        config.restart_cooldown = blobmsg_get_u32(tb[SERVICE_WATCHDOG_CONFIG_RESTART_COOLDOWN]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (tb[SERVICE_WATCHDOG_CONFIG_SERVICES_TO_MONITOR]) {
        struct blob_attr *services_array = tb[SERVICE_WATCHDOG_CONFIG_SERVICES_TO_MONITOR];
        size_t services_count = blobmsg_check_array(services_array, BLOBMSG_TYPE_STRING\n"\n"\n"\n"\n"\n"\n"\n");
        
        if (services_count > 0 && services_count <= MAX_SERVICES_TO_MONITOR) {
            config.services_count = services_count;
            
            struct blob_attr *attr;
            size_t rem;
            int i = 0;
            blobmsg_for_each_attr(attr, services_array, rem) {
                if (i < MAX_SERVICES_TO_MONITOR) {
                    strncpy(config.services_to_monitor[i], blobmsg_get_string(attr), 63\n"\n"\n"\n"\n"\n"\n"\n");
                    config.services_to_monitor[i][63] = '\0';
                    i++;
                }
            }
        }
    }
    
    // Apply the new configuration
    result = service_watchdog_set_config(&config\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to set service watchdog configuration"\n"\n"\n"\n"\n"\n"\n"\n");
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
 * Enable/disable service watchdog
 */
static int autonomy_service_watchdog_set_enabled(struct ubus_context *ctx, struct ubus_object *obj,
                                               struct ubus_request_data *req, const char *method, struct blob_attr *msg) {
    (void)obj; // Suppress unused parameter warning
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    struct blob_attr *tb[1];
    enum {
        SERVICE_WATCHDOG_ENABLED = 0,
        __SERVICE_WATCHDOG_ENABLED_MAX
    };
    
    static const struct blobmsg_policy policy[__SERVICE_WATCHDOG_ENABLED_MAX] = {
        [SERVICE_WATCHDOG_ENABLED] = { .name = "enabled", .type = BLOBMSG_TYPE_BOOL },
    };
    
    blobmsg_parse(policy, __SERVICE_WATCHDOG_ENABLED_MAX, tb, blob_data(msg), blob_len(msg)\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!tb[SERVICE_WATCHDOG_ENABLED]) {
        blobmsg_add_string(&bb, "error", "Missing 'enabled' parameter"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return 0;
    }
    
    bool enabled = blobmsg_get_bool(tb[SERVICE_WATCHDOG_ENABLED]\n"\n"\n"\n"\n"\n"\n"\n");
    int result = service_watchdog_set_enabled(enabled\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to set service watchdog enabled state"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return 0;
    }
    
    blobmsg_add_string(&bb, "status", enabled ? "Service watchdog enabled" : "Service watchdog disabled"\n"\n"\n"\n"\n"\n"\n"\n");
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return 0;
}

/**
 * Reset service watchdog statistics
 */
static int autonomy_service_watchdog_reset(struct ubus_context *ctx, struct ubus_object *obj,
                                         struct ubus_request_data *req, const char *method, struct blob_attr *msg) {
    (void)obj; // Suppress unused parameter warning
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    int result = service_watchdog_reset(\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to reset service watchdog statistics"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return 0;
    }
    
    blobmsg_add_string(&bb, "status", "Service watchdog statistics reset successfully"\n"\n"\n"\n"\n"\n"\n"\n");
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return 0;
}

/**
 * Manually trigger service watchdog check
 */
int autonomy_service_watchdog_check(struct ubus_context *ctx, struct ubus_object *obj,
                                         struct ubus_request_data *req, const char *method, struct blob_attr *msg) {
    (void)obj; // Suppress unused parameter warning
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    int result = service_watchdog_check(\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Service watchdog check failed"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return 0;
    }
    
    blobmsg_add_string(&bb, "status", "Service watchdog check completed successfully"\n"\n"\n"\n"\n"\n"\n"\n");
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return 0;
}

/**
 * Register service watchdog UBUS object
 */
int service_watchdog_ubus_register(struct ubus_context *ctx) {
    int ret = ubus_add_object(ctx, &autonomy_service_watchdog_obj\n"\n"\n"\n"\n"\n"\n"\n");
    if (ret) {
        fprintf(stderr, "Failed to add service watchdog object: %s\n", ubus_strerror(ret)\n"\n"\n"\n"\n"\n"\n"\n");
        return ret;
    }
    
    fprintf(stderr, "Service watchdog UBUS object registered successfully\n"\n"\n"\n"\n"\n"\n"\n"\n");
    return 0;
}

/**
 * Unregister service watchdog UBUS object
 */
void service_watchdog_ubus_unregister(struct ubus_context *ctx) {
    ubus_remove_object(ctx, &autonomy_service_watchdog_obj\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "Service watchdog UBUS object unregistered\n"\n"\n"\n"\n"\n"\n"\n"\n");
}
