#include "starlink_api_version_monitor_ubus.h"
#include "starlink_api_version_monitor.h"
#include "starlink_comprehensive.h"
#include "../core/types.h"
#include "../shared/logging/logx.h"
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <fcntl.h>

// Helper function to add API version to blob
void add_api_version_to_blob(struct blob_buf *bb, const char *name, 
                                    const starlink_api_version_t *version) {
    void *version_table = blobmsg_open_table(bb, name\n"\n"\n"\n"\n"\n"\n"\n");
    
    blobmsg_add_string(bb, "software_version", version->software_version\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(bb, "hardware_version", version->hardware_version\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(bb, "software_part_number", version->software_part_number\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(bb, "generation_number", version->generation_number\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(bb, "boot_count", version->boot_count\n"\n"\n"\n"\n"\n"\n"\n");
    
    void *parsed_table = blobmsg_open_table(bb, "parsed_version"\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(bb, "major", version->major_version\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(bb, "minor", version->minor_version\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(bb, "patch", version->patch_version\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(bb, "build", version->build_identifier\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_close_table(bb, parsed_table\n"\n"\n"\n"\n"\n"\n"\n");
    
    blobmsg_add_u32(bb, "first_detected", (uint32_t)version->first_detected\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(bb, "last_seen", (uint32_t)version->last_seen\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(bb, "is_current", version->is_current\n"\n"\n"\n"\n"\n"\n"\n");
    
    blobmsg_close_table(bb, version_table\n"\n"\n"\n"\n"\n"\n"\n");
}

// Helper function to add version change to blob
void add_version_change_to_blob(struct blob_buf *bb, const starlink_api_version_change_t *change) {
    void *change_table = blobmsg_open_table(bb, NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    blobmsg_add_string(bb, "change_id", change->change_id\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(bb, "detected_at", (uint32_t)change->detected_at\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(bb, "endpoint", starlink_api_endpoint_to_string(change->endpoint)\n"\n"\n"\n"\n"\n"\n"\n");
    
    blobmsg_add_string(bb, "old_version", change->old_version.software_version\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(bb, "new_version", change->new_version.software_version\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(bb, "severity", starlink_api_version_change_severity_to_string(change->severity)\n"\n"\n"\n"\n"\n"\n"\n");
    
    blobmsg_add_u8(bb, "breaking_change_suspected", change->breaking_change_suspected\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(bb, "notification_sent", change->notification_sent\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(bb, "api_still_functional", change->api_still_functional\n"\n"\n"\n"\n"\n"\n"\n");
    
    blobmsg_add_string(bb, "impact_assessment", change->impact_assessment\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(bb, "recommended_actions", change->recommended_actions\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (change->validation_performed_at > 0) {
        blobmsg_add_u32(bb, "validation_performed_at", (uint32_t)change->validation_performed_at\n"\n"\n"\n"\n"\n"\n"\n");
        if (strlen(change->validation_errors) > 0) {
            blobmsg_add_string(bb, "validation_errors", change->validation_errors\n"\n"\n"\n"\n"\n"\n"\n");
        }
    }
    
    blobmsg_close_table(bb, change_table\n"\n"\n"\n"\n"\n"\n"\n");
}

// Get current Starlink API version
int starlink_api_version_ubus_get_current_version(struct ubus_context *ctx, struct ubus_object *obj,
                                                 struct ubus_request_data *req, const char *method,
                                                 struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!starlink_api_version_monitor_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Starlink API version monitor not initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }
    
    starlink_api_version_t current_version;
    if (starlink_api_version_monitor_get_current_version(&current_version) == AUTONOMY_SUCCESS) {
        blobmsg_add_u8(&bb, "success", 1\n"\n"\n"\n"\n"\n"\n"\n");
        add_api_version_to_blob(&bb, "current_version", &current_version\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "No current API version available"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return UBUS_STATUS_OK;
}

// Get API version change history
int starlink_api_version_ubus_get_change_history(struct ubus_context *ctx, struct ubus_object *obj,
                                                struct ubus_request_data *req, const char *method,
                                                struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!starlink_api_version_monitor_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Starlink API version monitor not initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }
    
    starlink_api_version_change_t changes[50];
    int change_count = starlink_api_version_monitor_get_change_history(changes, 50\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (change_count >= 0) {
        blobmsg_add_u8(&bb, "success", 1\n"\n"\n"\n"\n"\n"\n"\n");
        
        void *changes_array = blobmsg_open_array(&bb, "version_changes"\n"\n"\n"\n"\n"\n"\n"\n");
        for (int i = 0; i < change_count; i++) {
            add_version_change_to_blob(&bb, &changes[i]\n"\n"\n"\n"\n"\n"\n"\n");
        }
        blobmsg_close_array(&bb, changes_array\n"\n"\n"\n"\n"\n"\n"\n");
        
        blobmsg_add_u32(&bb, "total_changes", change_count\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Failed to get version change history"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return UBUS_STATUS_OK;
}

// Get API version monitor statistics
int starlink_api_version_ubus_get_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                            struct ubus_request_data *req, const char *method,
                                            struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!starlink_api_version_monitor_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Starlink API version monitor not initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }
    
    blobmsg_add_u8(&bb, "success", 1\n"\n"\n"\n"\n"\n"\n"\n");
    
    starlink_api_version_monitor_stats_t stats;
    if (starlink_api_version_monitor_get_statistics(&stats) == AUTONOMY_SUCCESS) {
        void *stats_table = blobmsg_open_table(&bb, "statistics"\n"\n"\n"\n"\n"\n"\n"\n");
        
        blobmsg_add_u64(&bb, "total_version_checks", stats.total_version_checks\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "version_changes_detected", stats.version_changes_detected\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "minor_changes", stats.minor_changes\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "moderate_changes", stats.moderate_changes\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "major_changes", stats.major_changes\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "unknown_changes", stats.unknown_changes\n"\n"\n"\n"\n"\n"\n"\n");
        
        blobmsg_add_u64(&bb, "notifications_sent", stats.notifications_sent\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "validation_attempts", stats.validation_attempts\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "validation_failures", stats.validation_failures\n"\n"\n"\n"\n"\n"\n"\n");
        
        blobmsg_add_double(&bb, "avg_check_time_ms", stats.average_check_time_ms\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u32(&bb, "last_version_check", (uint32_t)stats.last_version_check\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u32(&bb, "last_version_change", (uint32_t)stats.last_version_change\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Calculate monitoring duration
        double monitoring_hours = difftime(time(NULL), stats.stats_start_time) / 3600.0;
        blobmsg_add_double(&bb, "monitoring_duration_hours", monitoring_hours\n"\n"\n"\n"\n"\n"\n"\n");
        
        blobmsg_close_table(&bb, stats_table\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return UBUS_STATUS_OK;
}

// Force immediate API version check
int starlink_api_version_ubus_force_check(struct ubus_context *ctx, struct ubus_object *obj,
                                         struct ubus_request_data *req, const char *method,
                                         struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!starlink_api_version_monitor_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Starlink API version monitor not initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }
    
    time_t start_time = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Get previous version for comparison
    starlink_api_version_t previous_version;
    bool had_previous = (starlink_api_version_monitor_get_current_version(&previous_version) == AUTONOMY_SUCCESS\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Perform version check
    if (starlink_api_version_monitor_check_version() == AUTONOMY_SUCCESS) {
        blobmsg_add_u8(&bb, "success", 1\n"\n"\n"\n"\n"\n"\n"\n");
        
        void *check_table = blobmsg_open_table(&bb, "version_check"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u32(&bb, "performed_at", (uint32_t)time(NULL)\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Get current version after check
        starlink_api_version_t current_version;
        if (starlink_api_version_monitor_get_current_version(&current_version) == AUTONOMY_SUCCESS) {
            blobmsg_add_string(&bb, "version_detected", current_version.software_version\n"\n"\n"\n"\n"\n"\n"\n");
            
            bool version_changed = had_previous && 
                (strcmp(previous_version.software_version, current_version.software_version) != 0\n"\n"\n"\n"\n"\n"\n"\n");
            blobmsg_add_u8(&bb, "version_changed", version_changed\n"\n"\n"\n"\n"\n"\n"\n");
        }
        
        double check_time_ms = difftime(time(NULL), start_time) * 1000.0;
        blobmsg_add_double(&bb, "check_time_ms", check_time_ms\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Quick API functionality test
        bool api_functional = (starlink_comprehensive_is_initialized() && 
                              starlink_api_version_monitor_validate_endpoint(STARLINK_API_ENDPOINT_GET_STATUS) == AUTONOMY_SUCCESS\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u8(&bb, "api_functional", api_functional\n"\n"\n"\n"\n"\n"\n"\n");
        
        blobmsg_close_table(&bb, check_table\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Failed to perform version check"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return UBUS_STATUS_OK;
}

// Get API version monitor configuration
int starlink_api_version_ubus_get_config(struct ubus_context *ctx, struct ubus_object *obj,
                                        struct ubus_request_data *req, const char *method,
                                        struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!starlink_api_version_monitor_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Starlink API version monitor not initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }
    
    blobmsg_add_u8(&bb, "success", 1\n"\n"\n"\n"\n"\n"\n"\n");
    
    void *config_table = blobmsg_open_table(&bb, "config"\n"\n"\n"\n"\n"\n"\n"\n");
    
    blobmsg_add_u8(&bb, "enabled", g_api_version_monitor.config.enabled\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "check_interval_s", g_api_version_monitor.config.check_interval_s\n"\n"\n"\n"\n"\n"\n"\n");
    
    void *notifications_table = blobmsg_open_table(&bb, "notifications"\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(&bb, "notify_on_minor_changes", g_api_version_monitor.config.notify_on_minor_changes\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(&bb, "notify_on_moderate_changes", g_api_version_monitor.config.notify_on_moderate_changes\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(&bb, "notify_on_major_changes", g_api_version_monitor.config.notify_on_major_changes\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(&bb, "notify_on_unknown_changes", g_api_version_monitor.config.notify_on_unknown_changes\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(&bb, "send_immediate_notifications", g_api_version_monitor.config.send_immediate_notifications\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(&bb, "send_summary_notifications", g_api_version_monitor.config.send_summary_notifications\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_close_table(&bb, notifications_table\n"\n"\n"\n"\n"\n"\n"\n");
    
    void *validation_table = blobmsg_open_table(&bb, "validation"\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(&bb, "perform_validation_on_change", g_api_version_monitor.config.perform_validation_on_change\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "validation_timeout_s", g_api_version_monitor.config.validation_timeout_s\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "max_validation_retries", g_api_version_monitor.config.max_validation_retries\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_close_table(&bb, validation_table\n"\n"\n"\n"\n"\n"\n"\n");
    
    void *storage_table = blobmsg_open_table(&bb, "storage"\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "max_version_history", g_api_version_monitor.config.max_version_history\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "max_change_records", g_api_version_monitor.config.max_change_records\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(&bb, "version_storage_file", g_api_version_monitor.config.version_storage_file\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_close_table(&bb, storage_table\n"\n"\n"\n"\n"\n"\n"\n");
    
    blobmsg_close_table(&bb, config_table\n"\n"\n"\n"\n"\n"\n"\n");
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return UBUS_STATUS_OK;
}

// Validate Starlink API endpoints
int starlink_api_version_ubus_validate_endpoints(struct ubus_context *ctx, struct ubus_object *obj,
                                                struct ubus_request_data *req, const char *method,
                                                struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!starlink_api_version_monitor_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Starlink API version monitor not initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }
    
    blobmsg_add_u8(&bb, "success", 1\n"\n"\n"\n"\n"\n"\n"\n");
    
    void *validation_table = blobmsg_open_table(&bb, "validation"\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "validation_time", (uint32_t)time(NULL)\n"\n"\n"\n"\n"\n"\n"\n");
    
    bool overall_functional = true;
    
    void *endpoints_table = blobmsg_open_table(&bb, "endpoints"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Test each endpoint
    for (int i = 0; i < STARLINK_API_ENDPOINT_MAX; i++) {
        starlink_api_endpoint_t endpoint = (starlink_api_endpoint_t)i;
        const char* endpoint_name = starlink_api_endpoint_to_string(endpoint\n"\n"\n"\n"\n"\n"\n"\n");
        
        void *endpoint_table = blobmsg_open_table(&bb, endpoint_name\n"\n"\n"\n"\n"\n"\n"\n");
        
        time_t endpoint_start = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
        int validation_result = starlink_api_version_monitor_validate_endpoint(endpoint\n"\n"\n"\n"\n"\n"\n"\n");
        double response_time_ms = difftime(time(NULL), endpoint_start) * 1000.0;
        
        bool functional = (validation_result == AUTONOMY_SUCCESS\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u8(&bb, "functional", functional\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(&bb, "response_time_ms", response_time_ms\n"\n"\n"\n"\n"\n"\n"\n");
        
        if (!functional) {
            overall_functional = false;
            blobmsg_add_string(&bb, "error", "endpoint_validation_failed"\n"\n"\n"\n"\n"\n"\n"\n");
        }
        
        blobmsg_close_table(&bb, endpoint_table\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    blobmsg_close_table(&bb, endpoints_table\n"\n"\n"\n"\n"\n"\n"\n");
    
    blobmsg_add_u8(&bb, "overall_functional", overall_functional\n"\n"\n"\n"\n"\n"\n"\n");
    
    const char* recommendation = overall_functional ? 
        "All critical endpoints functional" : 
        "Some endpoints failing - investigate API changes";
    blobmsg_add_string(&bb, "recommendation", recommendation\n"\n"\n"\n"\n"\n"\n"\n");
    
    blobmsg_close_table(&bb, validation_table\n"\n"\n"\n"\n"\n"\n"\n");
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return UBUS_STATUS_OK;
}

// Perform API version monitor health check
int starlink_api_version_ubus_health_check(struct ubus_context *ctx, struct ubus_object *obj,
                                          struct ubus_request_data *req, const char *method,
                                          struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!starlink_api_version_monitor_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Starlink API version monitor not initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }
    
    blobmsg_add_u8(&bb, "success", 1\n"\n"\n"\n"\n"\n"\n"\n");
    
    void *health_table = blobmsg_open_table(&bb, "health"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Determine monitor status
    const char* monitor_status = "healthy";
    starlink_api_version_monitor_stats_t stats;
    if (starlink_api_version_monitor_get_statistics(&stats) == AUTONOMY_SUCCESS) {
        time_t now = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Check if recent checks are happening
        if (stats.last_version_check > 0) {
            double hours_since_check = difftime(now, stats.last_version_check) / 3600.0;
            if (hours_since_check > 2.0) { // More than 2 hours since last check
                monitor_status = "stale";
            }
        } else {
            monitor_status = "inactive";
        }
        
        // Check for recent validation failures
        if (stats.validation_failures > 0 && stats.validation_attempts > 0) {
            double failure_rate = (double)stats.validation_failures / stats.validation_attempts;
            if (failure_rate > 0.5) {
                monitor_status = "degraded";
            }
        }
    }
    
    blobmsg_add_string(&bb, "monitor_status", monitor_status\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Check if current version is known
    starlink_api_version_t current_version;
    bool current_version_known = (starlink_api_version_monitor_get_current_version(&current_version) == AUTONOMY_SUCCESS\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(&bb, "current_version_known", current_version_known\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (current_version_known) {
        blobmsg_add_u32(&bb, "last_check", (uint32_t)stats.last_version_check\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Determine check frequency status
    const char* check_frequency = "normal";
    if (stats.total_version_checks > 0 && stats.stats_start_time > 0) {
        double hours_running = difftime(time(NULL), stats.stats_start_time) / 3600.0;
        double expected_checks = hours_running / (g_api_version_monitor.config.check_interval_s / 3600.0\n"\n"\n"\n"\n"\n"\n"\n");
        double check_ratio = stats.total_version_checks / expected_checks;
        
        if (check_ratio < 0.8) {
            check_frequency = "low";
        } else if (check_ratio > 1.2) {
            check_frequency = "high";
        }
    }
    blobmsg_add_string(&bb, "check_frequency", check_frequency\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Count recent changes (last 24 hours)
    int recent_changes = 0;
    time_t day_ago = time(NULL) - 86400;
    if (stats.last_version_change >= day_ago) {
        recent_changes = 1; // Simplified - would need to check all changes
    }
    blobmsg_add_u32(&bb, "recent_changes", recent_changes\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Quick API functionality test
    bool api_functional = (starlink_comprehensive_is_initialized() && 
                          starlink_api_version_monitor_validate_endpoint(STARLINK_API_ENDPOINT_GET_STATUS) == AUTONOMY_SUCCESS\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(&bb, "api_functional", api_functional\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Generate recommendation
    const char* recommendation = "monitor_healthy";
    if (!api_functional) {
        recommendation = "api_issues_detected";
    } else if (recent_changes > 0) {
        recommendation = "monitor_closely";
    } else if (strcmp(monitor_status, "stale") == 0) {
        recommendation = "check_monitor_config";
    }
    blobmsg_add_string(&bb, "recommendation", recommendation\n"\n"\n"\n"\n"\n"\n"\n");
    
    blobmsg_close_table(&bb, health_table\n"\n"\n"\n"\n"\n"\n"\n");
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return UBUS_STATUS_OK;
}

// UBUS method definitions
const struct ubus_method starlink_api_version_ubus_methods[] = {
    UBUS_METHOD_NOARG("get_current_version", starlink_api_version_ubus_get_current_version),
    UBUS_METHOD_NOARG("get_change_history", starlink_api_version_ubus_get_change_history),
    UBUS_METHOD_NOARG("get_version_statistics", starlink_api_version_ubus_get_statistics),
    UBUS_METHOD_NOARG("force_version_check", starlink_api_version_ubus_force_check),
    UBUS_METHOD_NOARG("get_version_config", starlink_api_version_ubus_get_config),
    UBUS_METHOD_NOARG("validate_endpoints", starlink_api_version_ubus_validate_endpoints),
    UBUS_METHOD_NOARG("version_monitor_health_check", starlink_api_version_ubus_health_check),
};

const int starlink_api_version_ubus_methods_count = ARRAY_SIZE(starlink_api_version_ubus_methods\n"\n"\n"\n"\n"\n"\n"\n");