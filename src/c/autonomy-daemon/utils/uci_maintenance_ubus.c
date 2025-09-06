#include "uci_maintenance.h"
#include <time.h>
#include <libubus.h>
#include <libubox/blobmsg.h>
#include <libubox/blobmsg_json.h>
#include <json-c/json.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

// Forward declarations
static int autonomy_uci_maintenance_status(struct ubus_context *ctx, struct ubus_request_data *req,
                                         const char *method, struct blob_attr *msg);
static int autonomy_uci_maintenance_reset(struct ubus_context *ctx, struct ubus_request_data *req,
                                         const char *method, struct blob_attr *msg);
static int autonomy_uci_maintenance_perform(struct ubus_context *ctx, struct ubus_request_data *req,
                                           const char *method, struct blob_attr *msg);

// UBUS method definitions
static const struct ubus_method_type autonomy_uci_maintenance_methods[] = {
    UBUS_METHOD("status", autonomy_uci_maintenance_status, 0),
    UBUS_METHOD("reset", autonomy_uci_maintenance_reset, 0),
    UBUS_METHOD("perform", autonomy_uci_maintenance_perform, 0),
};

// UBUS object type
static const struct ubus_object_type autonomy_uci_maintenance_obj_type = {
    .name = "autonomy_uci_maintenance",
    .methods = autonomy_uci_maintenance_methods,
    .n_methods = ARRAY_SIZE(autonomy_uci_maintenance_methods),
};

// UBUS object
static const struct ubus_object autonomy_uci_maintenance_obj = {
    .name = "uci_maintenance",
    .type = &autonomy_uci_maintenance_obj_type,
    .methods = autonomy_uci_maintenance_methods,
    .n_methods = ARRAY_SIZE(autonomy_uci_maintenance_methods),
};

/**
 * Get UCI maintenance status
 */
static int autonomy_uci_maintenance_status(struct ubus_context *ctx, struct ubus_request_data *req,
                                         const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    uci_maintenance_status_t status;
    int result = uci_maintenance_get_status(&status);
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to get UCI maintenance status");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    // Add statistics
    blobmsg_add_u32(&bb, "last_maintenance_time", status.last_maintenance_time);
    blobmsg_add_u32(&bb, "total_maintenance_runs", status.total_maintenance_runs);
    blobmsg_add_u32(&bb, "total_issues_fixed", status.total_issues_fixed);
    blobmsg_add_u32(&bb, "total_backups_created", status.total_backups_created);
    blobmsg_add_u32(&bb, "total_backups_restored", status.total_backups_restored);
    
    // Add last maintenance result
    if (status.last_result) {
        blobmsg_add_string(&bb, "last_result", status.last_result);
    }
    
    // Add last maintenance details
    if (status.last_details) {
        blobmsg_add_string(&bb, "last_details", status.last_details);
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Reset UCI maintenance statistics
 */
static int autonomy_uci_maintenance_reset(struct ubus_context *ctx, struct ubus_request_data *req,
                                         const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    int result = uci_maintenance_reset();
    
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to reset UCI maintenance statistics");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    blobmsg_add_string(&bb, "status", "UCI maintenance statistics reset successfully");
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Manually trigger UCI maintenance
 */
static int autonomy_uci_maintenance_perform(struct ubus_context *ctx, struct ubus_request_data *req,
                                           const char *method, struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    // Parse message to determine maintenance options
    struct blob_attr *tb[__UCI_MAINTENANCE_OPTIONS_MAX];
    enum {
        UCI_MAINTENANCE_OPTIONS_FORCE = 0,
        UCI_MAINTENANCE_OPTIONS_AUTO_FIX,
        UCI_MAINTENANCE_OPTIONS_CREATE_BACKUP,
        __UCI_MAINTENANCE_OPTIONS_MAX
    };
    
    static const struct blobmsg_policy policy[__UCI_MAINTENANCE_OPTIONS_MAX] = {
        [UCI_MAINTENANCE_OPTIONS_FORCE] = { .name = "force", .type = BLOBMSG_TYPE_BOOL },
        [UCI_MAINTENANCE_OPTIONS_AUTO_FIX] = { .name = "auto_fix", .type = BLOBMSG_TYPE_BOOL },
        [UCI_MAINTENANCE_OPTIONS_CREATE_BACKUP] = { .name = "create_backup", .type = BLOBMSG_TYPE_BOOL },
    };
    
    blobmsg_parse(policy, __UCI_MAINTENANCE_OPTIONS_MAX, tb, blob_data(msg), blob_len(msg));
    
    // Get maintenance options
    bool force = false;
    bool auto_fix = true;
    bool create_backup = true;
    
    if (tb[UCI_MAINTENANCE_OPTIONS_FORCE]) {
        force = blobmsg_get_bool(tb[UCI_MAINTENANCE_OPTIONS_FORCE]);
    }
    
    if (tb[UCI_MAINTENANCE_OPTIONS_AUTO_FIX]) {
        auto_fix = blobmsg_get_bool(tb[UCI_MAINTENANCE_OPTIONS_AUTO_FIX]);
    }
    
    if (tb[UCI_MAINTENANCE_OPTIONS_CREATE_BACKUP]) {
        create_backup = blobmsg_get_bool(tb[UCI_MAINTENANCE_OPTIONS_CREATE_BACKUP]);
    }
    
    // Perform maintenance
    uci_maintenance_result_t result;
    int ret = uci_maintenance_perform_maintenance(&result);
    
    if (ret != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "UCI maintenance failed");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    // Add result information
    blobmsg_add_string(&bb, "status", "UCI maintenance completed successfully");
    blobmsg_add_u8(&bb, "issues_found", result.issues_found > 0);
    blobmsg_add_u32(&bb, "issues_fixed", result.issues_fixed);
    blobmsg_add_u32(&bb, "backups_created", result.backups_created);
    blobmsg_add_u32(&bb, "backups_restored", result.backups_restored);
    
    if (result.details) {
        blobmsg_add_string(&bb, "details", result.details);
    }
    
    if (result.backup_path) {
        blobmsg_add_string(&bb, "backup_path", result.backup_path);
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

/**
 * Register UCI maintenance UBUS object
 */
int uci_maintenance_ubus_register(struct ubus_context *ctx) {
    int ret = ubus_add_object(ctx, &autonomy_uci_maintenance_obj);
    if (ret) {
        fprintf(stderr, "Failed to add UCI maintenance object: %s\n", ubus_strerror(ret));
        return ret;
    }
    
    fprintf(stderr, "UCI maintenance UBUS object registered successfully\n");
    return 0;
}

/**
 * Unregister UCI maintenance UBUS object
 */
void uci_maintenance_ubus_unregister(struct ubus_context *ctx) {
    ubus_remove_object(ctx, &autonomy_uci_maintenance_obj);
    fprintf(stderr, "UCI maintenance UBUS object unregistered\n");
}
