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
    
    blob_buf_init(&bb, 0);
    
    // Parse message attributes
    struct blob_attr *tb[__NOTIFICATION_ATTR_MAX];
    enum {
        NOTIFICATION_ATTR_TITLE,
        NOTIFICATION_ATTR_MESSAGE,
        NOTIFICATION_ATTR_SOURCE,
        NOTIFICATION_ATTR_TYPE,
        NOTIFICATION_ATTR_PRIORITY,
        NOTIFICATION_ATTR_CHANNEL,
        __NOTIFICATION_ATTR_MAX
    };
    
    static const struct blobmsg_policy policy[__NOTIFICATION_ATTR_MAX] = {
        [NOTIFICATION_ATTR_TITLE] = { .name = "title", .type = BLOBMSG_TYPE_STRING },
        [NOTIFICATION_ATTR_MESSAGE] = { .name = "message", .type = BLOBMSG_TYPE_STRING },
        [NOTIFICATION_ATTR_SOURCE] = { .name = "source", .type = BLOBMSG_TYPE_STRING },
        [NOTIFICATION_ATTR_TYPE] = { .name = "type", .type = BLOBMSG_TYPE_INT32 },
        [NOTIFICATION_ATTR_PRIORITY] = { .name = "priority", .type = BLOBMSG_TYPE_INT32 },
        [NOTIFICATION_ATTR_CHANNEL] = { .name = "channel", .type = BLOBMSG_TYPE_INT32 },
    };
    
    if (blobmsg_parse(policy, __NOTIFICATION_ATTR_MAX, tb, blob_data(msg), blob_len(msg)) != 0) {
        blobmsg_add_string(&bb, "status", "error");
        blobmsg_add_string(&bb, "error", "Invalid message format");
        ubus_send_reply(uctx, req, bb.head);
        blob_buf_free(&bb);
        return -1;
    }
    
    // Extract notification parameters
    const char *title = blobmsg_get_string(tb[NOTIFICATION_ATTR_TITLE]);
    const char *message = blobmsg_get_string(tb[NOTIFICATION_ATTR_MESSAGE]);
    const char *source = blobmsg_get_string(tb[NOTIFICATION_ATTR_SOURCE]);
    
    if (!title || !message || !source) {
        blobmsg_add_string(&bb, "status", "error");
        blobmsg_add_string(&bb, "error", "Missing required fields");
        ubus_send_reply(uctx, req, bb.head);
        blob_buf_free(&bb);
        return -1;
    }
    
    // Get type, priority, and channel (with defaults)
    notification_type_t type = NOTIFICATION_TYPE_INFO;
    notification_priority_t priority = NOTIFICATION_PRIORITY_NORMAL;
    notification_channel_t channel = NOTIFICATION_CHANNEL_SYSLOG;
    
    if (tb[NOTIFICATION_ATTR_TYPE]) {
        type = (notification_type_t)blobmsg_get_u32(tb[NOTIFICATION_ATTR_TYPE]);
    }
    
    if (tb[NOTIFICATION_ATTR_PRIORITY]) {
        priority = (notification_priority_t)blobmsg_get_u32(tb[NOTIFICATION_ATTR_PRIORITY]);
    }
    
    if (tb[NOTIFICATION_ATTR_CHANNEL]) {
        channel = (notification_channel_t)blobmsg_get_u32(tb[NOTIFICATION_ATTR_CHANNEL]);
    }
    
    // Add notification to manager
    int result = notification_manager_add(title, message, source, type, priority, channel);
    
    if (result == 0) {
        blobmsg_add_string(&bb, "status", "success");
        blobmsg_add_string(&bb, "message", "Notification queued successfully");
    } else {
        blobmsg_add_string(&bb, "status", "error");
        blobmsg_add_string(&bb, "error", "Failed to queue notification");
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

// Get notification status UBUS method
int autonomy_notification_status(struct ubus_context *uctx, struct ubus_object *obj,
                                struct ubus_request_data *req, const char *method,
                                struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Get notification statistics
    const notification_stats_t *stats = notification_manager_get_stats();
    
    blobmsg_add_string(&bb, "status", "operational");
    blobmsg_add_u64(&bb, "total_notifications", stats->total_notifications);
    blobmsg_add_u64(&bb, "sent_notifications", stats->sent_notifications);
    blobmsg_add_u64(&bb, "failed_notifications", stats->failed_notifications);
    blobmsg_add_u64(&bb, "acknowledged_notifications", stats->acknowledged_notifications);
    blobmsg_add_u64(&bb, "pending_notifications", stats->pending_notifications);
    blobmsg_add_u32(&bb, "last_notification_time", (uint32_t)stats->last_notification_time);
    blobmsg_add_u32(&bb, "last_success_time", (uint32_t)stats->last_success_time);
    blobmsg_add_u32(&bb, "last_failure_time", (uint32_t)stats->last_failure_time);
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

// Get notification configuration UBUS method
int autonomy_notification_config(struct ubus_context *uctx, struct ubus_object *obj,
                                struct ubus_request_data *req, const char *method,
                                struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Get notification configuration
    const notification_config_t *config = notification_manager_get_config();
    
    blobmsg_add_string(&bb, "status", "operational");
    blobmsg_add_bool(&bb, "enabled", config->enabled);
    blobmsg_add_u32(&bb, "max_retries", config->max_retries);
    blobmsg_add_u32(&bb, "retry_delay_seconds", config->retry_delay_seconds);
    blobmsg_add_bool(&bb, "rate_limiting_enabled", config->rate_limiting_enabled);
    blobmsg_add_u32(&bb, "max_per_hour", config->max_per_hour);
    blobmsg_add_u32(&bb, "max_per_day", config->max_per_day);
    blobmsg_add_u32(&bb, "min_priority", config->min_priority);
    blobmsg_add_bool(&bb, "emergency_bypass", config->emergency_bypass);
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

// Process notification queue UBUS method
int autonomy_notification_process(struct ubus_context *uctx, struct ubus_object *obj,
                                 struct ubus_request_data *req, const char *method,
                                 struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Process notification queue
    int processed = notification_manager_process_queue();
    
    blobmsg_add_string(&bb, "status", "success");
    blobmsg_add_string(&bb, "message", "Queue processing completed");
    blobmsg_add_u32(&bb, "processed_count", processed);
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

// Acknowledge notification UBUS method
int autonomy_notification_acknowledge(struct ubus_context *uctx, struct ubus_object *obj,
                                     struct ubus_request_data *req, const char *method,
                                     struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Parse message attributes
    struct blob_attr *tb[__ACK_ATTR_MAX];
    enum {
        ACK_ATTR_NOTIFICATION_ID,
        ACK_ATTR_ACKNOWLEDGED_BY,
        __ACK_ATTR_MAX
    };
    
    static const struct blobmsg_policy policy[__ACK_ATTR_MAX] = {
        [ACK_ATTR_NOTIFICATION_ID] = { .name = "notification_id", .type = BLOBMSG_TYPE_STRING },
        [ACK_ATTR_ACKNOWLEDGED_BY] = { .name = "acknowledged_by", .type = BLOBMSG_TYPE_STRING },
    };
    
    if (blobmsg_parse(policy, __ACK_ATTR_MAX, tb, blob_data(msg), blob_len(msg)) != 0) {
        blobmsg_add_string(&bb, "status", "error");
        blobmsg_add_string(&bb, "error", "Invalid message format");
        ubus_send_reply(uctx, req, bb.head);
        blob_buf_free(&bb);
        return -1;
    }
    
    // Extract parameters
    const char *notification_id = blobmsg_get_string(tb[ACK_ATTR_NOTIFICATION_ID]);
    const char *acknowledged_by = blobmsg_get_string(tb[ACK_ATTR_ACKNOWLEDGED_BY]);
    
    if (!notification_id || !acknowledged_by) {
        blobmsg_add_string(&bb, "status", "error");
        blobmsg_add_string(&bb, "error", "Missing required fields");
        ubus_send_reply(uctx, req, bb.head);
        blob_buf_free(&bb);
        return -1;
    }
    
    // Acknowledge notification
    int result = notification_manager_acknowledge(notification_id, acknowledged_by);
    
    if (result == 0) {
        blobmsg_add_string(&bb, "status", "success");
        blobmsg_add_string(&bb, "message", "Notification acknowledged successfully");
    } else {
        blobmsg_add_string(&bb, "status", "error");
        blobmsg_add_string(&bb, "error", "Failed to acknowledge notification");
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}
