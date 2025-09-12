#include "notification_ubus.h"
#include "notification_manager.h"
#include "notification_types.h"
#include <libubus.h>
#include <libubox/blobmsg.h>
#include <libubox/blobmsg_json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Send notification UBUS method
int autonomy_notification_send(struct ubus_context *uctx, struct ubus_object *obj,
                              struct ubus_request_data *req, const char *method,
                              struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Parse message attributes
    enum {
        NOTIFICATION_ATTR_TITLE,
        NOTIFICATION_ATTR_MESSAGE,
        NOTIFICATION_ATTR_SOURCE,
        NOTIFICATION_ATTR_TYPE,
        NOTIFICATION_ATTR_PRIORITY,
        NOTIFICATION_ATTR_CHANNEL,
        __NOTIFICATION_ATTR_MAX
    };
    struct blob_attr *tb[__NOTIFICATION_ATTR_MAX];
    
    static const struct blobmsg_policy policy[__NOTIFICATION_ATTR_MAX] = {
        [NOTIFICATION_ATTR_TITLE] = { .name = "title", .type = BLOBMSG_TYPE_STRING },
        [NOTIFICATION_ATTR_MESSAGE] = { .name = "message", .type = BLOBMSG_TYPE_STRING },
        [NOTIFICATION_ATTR_SOURCE] = { .name = "source", .type = BLOBMSG_TYPE_STRING },
        [NOTIFICATION_ATTR_TYPE] = { .name = "type", .type = BLOBMSG_TYPE_INT32 },
        [NOTIFICATION_ATTR_PRIORITY] = { .name = "priority", .type = BLOBMSG_TYPE_INT32 },
        [NOTIFICATION_ATTR_CHANNEL] = { .name = "channel", .type = BLOBMSG_TYPE_INT32 },
    };
    
    if (blobmsg_parse(policy, __NOTIFICATION_ATTR_MAX, tb, blob_data(msg), blob_len(msg)) != 0) {
        blobmsg_add_string(&bb, "status", "error"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Invalid message format"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(uctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return -1;
    }
    
    // Extract notification parameters
    const char *title = blobmsg_get_string(tb[NOTIFICATION_ATTR_TITLE]\n"\n"\n"\n"\n"\n"\n"\n");
    const char *message = blobmsg_get_string(tb[NOTIFICATION_ATTR_MESSAGE]\n"\n"\n"\n"\n"\n"\n"\n");
    const char *source = blobmsg_get_string(tb[NOTIFICATION_ATTR_SOURCE]\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!title || !message || !source) {
        blobmsg_add_string(&bb, "status", "error"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Missing required fields"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(uctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return -1;
    }
    
    // Get type, priority, and channel (with defaults)
    notification_type_t type = NOTIFICATION_TYPE_INFO;
    notification_priority_t priority = NOTIFICATION_PRIORITY_NORMAL;
    notification_channel_t channel = NOTIFICATION_CHANNEL_SYSLOG;
    
    if (tb[NOTIFICATION_ATTR_TYPE]) {
        type = (notification_type_t)blobmsg_get_u32(tb[NOTIFICATION_ATTR_TYPE]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (tb[NOTIFICATION_ATTR_PRIORITY]) {
        priority = (notification_priority_t)blobmsg_get_u32(tb[NOTIFICATION_ATTR_PRIORITY]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (tb[NOTIFICATION_ATTR_CHANNEL]) {
        channel = (notification_channel_t)blobmsg_get_u32(tb[NOTIFICATION_ATTR_CHANNEL]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Send notification via manager
    int result = notification_manager_send(type, title, message, priority, NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (result == 0) {
        blobmsg_add_string(&bb, "status", "success"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "message", "Notification queued successfully"\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        blobmsg_add_string(&bb, "status", "error"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Failed to queue notification"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL)\n"\n"\n"\n"\n"\n"\n"\n");
    
    ubus_send_reply(uctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return 0;
}

// Get notification status UBUS method
int autonomy_notification_status(struct ubus_context *uctx, struct ubus_object *obj,
                                struct ubus_request_data *req, const char *method,
                                struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Get notification statistics
    notification_stats_t stats;
    notification_manager_get_stats(&stats\n"\n"\n"\n"\n"\n"\n"\n");
    
    blobmsg_add_string(&bb, "status", "operational"\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u64(&bb, "total_notifications", stats.total_notifications\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u64(&bb, "sent_notifications", stats.sent_notifications\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u64(&bb, "failed_notifications", stats.failed_notifications\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u64(&bb, "acknowledged_notifications", stats.acknowledged_notifications\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u64(&bb, "suppressed_notifications", stats.suppressed_notifications\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "last_notification_time", (uint32_t)stats.last_notification_time\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "last_emergency_time", (uint32_t)stats.last_emergency_time\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "last_critical_time", (uint32_t)stats.last_critical_time\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL)\n"\n"\n"\n"\n"\n"\n"\n");
    
    ubus_send_reply(uctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return 0;
}

// Get notification configuration UBUS method
int autonomy_notification_config(struct ubus_context *uctx, struct ubus_object *obj,
                                struct ubus_request_data *req, const char *method,
                                struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Return basic configuration status
    blobmsg_add_string(&bb, "status", "operational"\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "enabled", 1\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "max_retries", 3\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "retry_delay_seconds", 5\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "rate_limiting_enabled", 1\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "max_per_hour", 100\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "max_per_day", 1000\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "min_priority", 0\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "emergency_bypass", 1\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL)\n"\n"\n"\n"\n"\n"\n"\n");
    
    ubus_send_reply(uctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return 0;
}

// Process notification queue UBUS method
int autonomy_notification_process(struct ubus_context *uctx, struct ubus_object *obj,
                                 struct ubus_request_data *req, const char *method,
                                 struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Process notification queue (placeholder)
    int processed = 0;
    
    blobmsg_add_string(&bb, "status", "success"\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(&bb, "message", "Queue processing completed"\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "processed_count", processed\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL)\n"\n"\n"\n"\n"\n"\n"\n");
    
    ubus_send_reply(uctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return 0;
}

// Acknowledge notification UBUS method
int autonomy_notification_acknowledge(struct ubus_context *uctx, struct ubus_object *obj,
                                     struct ubus_request_data *req, const char *method,
                                     struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Parse message attributes
    enum {
        ACK_ATTR_NOTIFICATION_ID,
        ACK_ATTR_ACKNOWLEDGED_BY,
        __ACK_ATTR_MAX
    };
    struct blob_attr *tb[__ACK_ATTR_MAX];
    
    static const struct blobmsg_policy policy[__ACK_ATTR_MAX] = {
        [ACK_ATTR_NOTIFICATION_ID] = { .name = "notification_id", .type = BLOBMSG_TYPE_STRING },
        [ACK_ATTR_ACKNOWLEDGED_BY] = { .name = "acknowledged_by", .type = BLOBMSG_TYPE_STRING },
    };
    
    if (blobmsg_parse(policy, __ACK_ATTR_MAX, tb, blob_data(msg), blob_len(msg)) != 0) {
        blobmsg_add_string(&bb, "status", "error"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Invalid message format"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(uctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return -1;
    }
    
    // Extract parameters
    const char *notification_id = blobmsg_get_string(tb[ACK_ATTR_NOTIFICATION_ID]\n"\n"\n"\n"\n"\n"\n"\n");
    const char *acknowledged_by = blobmsg_get_string(tb[ACK_ATTR_ACKNOWLEDGED_BY]\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!notification_id || !acknowledged_by) {
        blobmsg_add_string(&bb, "status", "error"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Missing required fields"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(uctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return -1;
    }
    
    // Acknowledge notification (placeholder)
    int result = 0; // Success for now
    
    if (result == 0) {
        blobmsg_add_string(&bb, "status", "success"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "message", "Notification acknowledged successfully"\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        blobmsg_add_string(&bb, "status", "error"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Failed to acknowledge notification"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL)\n"\n"\n"\n"\n"\n"\n"\n");
    
    ubus_send_reply(uctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return 0;
}
