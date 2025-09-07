#include "notifications_comprehensive_ubus.h"
#include "notifications_comprehensive.h"
#include "../utils/logx.h"
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// UBUS parameter policies
enum {
    NOTIF_SEND_TYPE,
    NOTIF_SEND_PRIORITY,
    NOTIF_SEND_TITLE,
    NOTIF_SEND_MESSAGE,
    NOTIF_SEND_CONTEXT,
    NOTIF_SEND_SOURCE,
    __NOTIF_SEND_MAX
};

static const struct blobmsg_policy notification_send_policy[] = {
    [NOTIF_SEND_TYPE] = { .name = "type", .type = BLOBMSG_TYPE_STRING },
    [NOTIF_SEND_PRIORITY] = { .name = "priority", .type = BLOBMSG_TYPE_STRING },
    [NOTIF_SEND_TITLE] = { .name = "title", .type = BLOBMSG_TYPE_STRING },
    [NOTIF_SEND_MESSAGE] = { .name = "message", .type = BLOBMSG_TYPE_STRING },
    [NOTIF_SEND_CONTEXT] = { .name = "context", .type = BLOBMSG_TYPE_TABLE },
    [NOTIF_SEND_SOURCE] = { .name = "source_module", .type = BLOBMSG_TYPE_STRING },
};

enum {
    NOTIF_EMERGENCY_TITLE,
    NOTIF_EMERGENCY_MESSAGE,
    NOTIF_EMERGENCY_CONTEXT,
    NOTIF_EMERGENCY_SOURCE,
    __NOTIF_EMERGENCY_MAX
};

static const struct blobmsg_policy notification_emergency_policy[] = {
    [NOTIF_EMERGENCY_TITLE] = { .name = "title", .type = BLOBMSG_TYPE_STRING },
    [NOTIF_EMERGENCY_MESSAGE] = { .name = "message", .type = BLOBMSG_TYPE_STRING },
    [NOTIF_EMERGENCY_CONTEXT] = { .name = "context", .type = BLOBMSG_TYPE_TABLE },
    [NOTIF_EMERGENCY_SOURCE] = { .name = "source_module", .type = BLOBMSG_TYPE_STRING },
};

enum {
    NOTIF_STATUS_ID,
    __NOTIF_STATUS_MAX
};

static const struct blobmsg_policy notification_status_policy[] = {
    [NOTIF_STATUS_ID] = { .name = "notification_id", .type = BLOBMSG_TYPE_STRING },
};

enum {
    NOTIF_ACK_ID,
    NOTIF_ACK_BY,
    __NOTIF_ACK_MAX
};

static const struct blobmsg_policy notification_ack_policy[] = {
    [NOTIF_ACK_ID] = { .name = "notification_id", .type = BLOBMSG_TYPE_STRING },
    [NOTIF_ACK_BY] = { .name = "acknowledged_by", .type = BLOBMSG_TYPE_STRING },
};

enum {
    NOTIF_TEST_MESSAGE,
    __NOTIF_TEST_MAX
};

static const struct blobmsg_policy notification_test_policy[] = {
    [NOTIF_TEST_MESSAGE] = { .name = "test_message", .type = BLOBMSG_TYPE_STRING },
};

// Helper function to convert blob table to JSON string
static char* blob_table_to_json_string(struct blob_attr *table) {
    if (!table) return NULL;
    
    char* json_str = blobmsg_format_json(table, true);
    if (!json_str) return NULL;
    
    // Create a copy since blobmsg_format_json returns static buffer
    char* result = strdup(json_str);
    free(json_str);
    
    return result;
}

// Helper function to add channel delivery tracking to blob
static void add_delivery_tracking_to_blob(struct blob_buf *bb, const comprehensive_notification_record_t *record) {
    void *tracking_table = blobmsg_open_table(bb, "delivery_tracking");
    
    // Add each channel's delivery status
    const char* channels[] = {"pushover", "email", "sms", "webhook", "slack", "discord", "telegram"};
    bool sent[] = {record->sent_pushover, record->sent_email, record->sent_sms, 
                   record->sent_webhook, record->sent_slack, record->sent_discord, record->sent_telegram};
    bool success[] = {record->pushover_success, record->email_success, record->sms_success,
                      record->webhook_success, record->slack_success, record->discord_success, record->telegram_success};
    
    for (int i = 0; i < 7; i++) {
        void *channel_table = blobmsg_open_table(bb, channels[i]);
        blobmsg_add_u8(bb, "sent", sent[i]);
        blobmsg_add_u8(bb, "success", success[i]);
        blobmsg_close_table(bb, channel_table);
    }
    
    blobmsg_close_table(bb, tracking_table);
}

// Send comprehensive notification
int notifications_comprehensive_ubus_send(struct ubus_context *ctx, struct ubus_object *obj,
                                         struct ubus_request_data *req, const char *method,
                                         struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    if (!notifications_comprehensive_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "Comprehensive notifications not initialized");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    struct blob_attr *tb[__NOTIF_SEND_MAX];
    blobmsg_parse(notification_send_policy, __NOTIF_SEND_MAX, tb, blob_data(msg), blob_len(msg));
    
    const char* type_str = tb[NOTIF_SEND_TYPE] ? blobmsg_get_string(tb[NOTIF_SEND_TYPE]) : "info";
    const char* priority_str = tb[NOTIF_SEND_PRIORITY] ? blobmsg_get_string(tb[NOTIF_SEND_PRIORITY]) : "normal";
    const char* title = tb[NOTIF_SEND_TITLE] ? blobmsg_get_string(tb[NOTIF_SEND_TITLE]) : "Notification";
    const char* message = tb[NOTIF_SEND_MESSAGE] ? blobmsg_get_string(tb[NOTIF_SEND_MESSAGE]) : "";
    const char* source_module = tb[NOTIF_SEND_SOURCE] ? blobmsg_get_string(tb[NOTIF_SEND_SOURCE]) : "unknown";
    
    // Convert context table to JSON string
    char* context_json = NULL;
    if (tb[NOTIF_SEND_CONTEXT]) {
        context_json = blob_table_to_json_string(tb[NOTIF_SEND_CONTEXT]);
    }
    
    // Parse type and priority
    notification_type_t type = notification_parse_type(type_str);
    notification_priority_t priority = notification_parse_priority(priority_str);
    
    // Send notification
    const char* notification_id = notifications_comprehensive_send(type, priority, title, message, 
                                                                  context_json, source_module);
    
    if (notification_id) {
        blobmsg_add_u8(&bb, "success", 1);
        
        // Get notification details for response
        comprehensive_notification_record_t record;
        if (notifications_comprehensive_get_status(notification_id, &record) == AUTONOMY_SUCCESS) {
            void *notification_table = blobmsg_open_table(&bb, "notification");
            
            blobmsg_add_string(&bb, "id", record.id);
            blobmsg_add_string(&bb, "status", notification_delivery_status_to_string(record.status));
            blobmsg_add_u32(&bb, "sent_at", (uint32_t)record.sent_at);
            
            // Add channels used
            void *channels_array = blobmsg_open_array(&bb, "channels_used");
            if (record.sent_pushover) blobmsg_add_string(&bb, NULL, "pushover");
            if (record.sent_email) blobmsg_add_string(&bb, NULL, "email");
            if (record.sent_sms) blobmsg_add_string(&bb, NULL, "sms");
            if (record.sent_webhook) blobmsg_add_string(&bb, NULL, "webhook");
            if (record.sent_slack) blobmsg_add_string(&bb, NULL, "slack");
            if (record.sent_discord) blobmsg_add_string(&bb, NULL, "discord");
            if (record.sent_telegram) blobmsg_add_string(&bb, NULL, "telegram");
            blobmsg_close_array(&bb, channels_array);
            
            blobmsg_add_double(&bb, "delivery_confidence", record.delivery_confidence);
            blobmsg_add_double(&bb, "processing_time_ms", record.processing_time_ms);
            blobmsg_add_u8(&bb, "priority_optimized", record.priority_optimized);
            blobmsg_add_u8(&bb, "channels_optimized", record.channels_optimized);
            
            blobmsg_close_table(&bb, notification_table);
        }
    } else {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "Failed to send notification");
    }
    
    if (context_json) {
        free(context_json);
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return UBUS_STATUS_OK;
}

// Send emergency notification
int notifications_comprehensive_ubus_send_emergency(struct ubus_context *ctx, struct ubus_object *obj,
                                                   struct ubus_request_data *req, const char *method,
                                                   struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    if (!notifications_comprehensive_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "Comprehensive notifications not initialized");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    struct blob_attr *tb[__NOTIF_EMERGENCY_MAX];
    blobmsg_parse(notification_emergency_policy, __NOTIF_EMERGENCY_MAX, tb, blob_data(msg), blob_len(msg));
    
    const char* title = tb[NOTIF_EMERGENCY_TITLE] ? blobmsg_get_string(tb[NOTIF_EMERGENCY_TITLE]) : "Emergency Alert";
    const char* message = tb[NOTIF_EMERGENCY_MESSAGE] ? blobmsg_get_string(tb[NOTIF_EMERGENCY_MESSAGE]) : "";
    const char* source_module = tb[NOTIF_EMERGENCY_SOURCE] ? blobmsg_get_string(tb[NOTIF_EMERGENCY_SOURCE]) : "emergency";
    
    // Convert context table to JSON string
    char* context_json = NULL;
    if (tb[NOTIF_EMERGENCY_CONTEXT]) {
        context_json = blob_table_to_json_string(tb[NOTIF_EMERGENCY_CONTEXT]);
    }
    
    // Send emergency notification
    const char* notification_id = notifications_comprehensive_send_emergency(title, message, 
                                                                            context_json, source_module);
    
    if (notification_id) {
        blobmsg_add_u8(&bb, "success", 1);
        
        void *emergency_table = blobmsg_open_table(&bb, "emergency_notification");
        blobmsg_add_string(&bb, "id", notification_id);
        blobmsg_add_u8(&bb, "bypass_used", 1);
        
        // Get notification details
        comprehensive_notification_record_t record;
        if (notifications_comprehensive_get_status(notification_id, &record) == AUTONOMY_SUCCESS) {
            // Add channels used
            void *channels_array = blobmsg_open_array(&bb, "channels_used");
            if (record.sent_pushover) blobmsg_add_string(&bb, NULL, "pushover");
            if (record.sent_email) blobmsg_add_string(&bb, NULL, "email");
            if (record.sent_sms) blobmsg_add_string(&bb, NULL, "sms");
            if (record.sent_webhook) blobmsg_add_string(&bb, NULL, "webhook");
            blobmsg_close_array(&bb, channels_array);
            
            blobmsg_add_double(&bb, "delivery_confidence", record.delivery_confidence);
            blobmsg_add_double(&bb, "processing_time_ms", record.processing_time_ms);
        }
        
        blobmsg_close_table(&bb, emergency_table);
    } else {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "Failed to send emergency notification");
    }
    
    if (context_json) {
        free(context_json);
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return UBUS_STATUS_OK;
}

// Get notification status and delivery tracking
int notifications_comprehensive_ubus_get_status(struct ubus_context *ctx, struct ubus_object *obj,
                                               struct ubus_request_data *req, const char *method,
                                               struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    if (!notifications_comprehensive_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "Comprehensive notifications not initialized");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    struct blob_attr *tb[__NOTIF_STATUS_MAX];
    blobmsg_parse(notification_status_policy, __NOTIF_STATUS_MAX, tb, blob_data(msg), blob_len(msg));
    
    const char* notification_id = tb[NOTIF_STATUS_ID] ? blobmsg_get_string(tb[NOTIF_STATUS_ID]) : "";
    
    comprehensive_notification_record_t record;
    if (notifications_comprehensive_get_status(notification_id, &record) == AUTONOMY_SUCCESS) {
        blobmsg_add_u8(&bb, "success", 1);
        
        void *notification_table = blobmsg_open_table(&bb, "notification");
        
        blobmsg_add_string(&bb, "id", record.id);
        blobmsg_add_string(&bb, "type", notification_type_to_string(record.type));
        blobmsg_add_string(&bb, "priority", notification_priority_to_string(record.priority));
        blobmsg_add_string(&bb, "title", record.title);
        blobmsg_add_string(&bb, "status", notification_delivery_status_to_string(record.status));
        
        blobmsg_add_u32(&bb, "created_at", (uint32_t)record.created_at);
        if (record.sent_at > 0) blobmsg_add_u32(&bb, "sent_at", (uint32_t)record.sent_at);
        if (record.delivered_at > 0) blobmsg_add_u32(&bb, "delivered_at", (uint32_t)record.delivered_at);
        if (record.acknowledged_at > 0) blobmsg_add_u32(&bb, "acknowledged_at", (uint32_t)record.acknowledged_at);
        
        // Add delivery tracking
        add_delivery_tracking_to_blob(&bb, &record);
        
        // Add intelligence data
        void *intelligence_table = blobmsg_open_table(&bb, "intelligence");
        blobmsg_add_u8(&bb, "priority_optimized", record.priority_optimized);
        blobmsg_add_u8(&bb, "channels_optimized", record.channels_optimized);
        blobmsg_add_double(&bb, "delivery_confidence", record.delivery_confidence);
        blobmsg_add_double(&bb, "processing_time_ms", record.processing_time_ms);
        blobmsg_close_table(&bb, intelligence_table);
        
        // Add acknowledgment data if applicable
        if (record.acknowledgment_required) {
            void *ack_table = blobmsg_open_table(&bb, "acknowledgment");
            blobmsg_add_u8(&bb, "required", record.acknowledgment_required);
            blobmsg_add_u8(&bb, "acknowledged", record.acknowledged_at > 0);
            if (record.acknowledgment_expires_at > 0) {
                blobmsg_add_u32(&bb, "expires_at", (uint32_t)record.acknowledgment_expires_at);
            }
            if (strlen(record.acknowledged_by) > 0) {
                blobmsg_add_string(&bb, "acknowledged_by", record.acknowledged_by);
            }
            blobmsg_close_table(&bb, ack_table);
        }
        
        blobmsg_close_table(&bb, notification_table);
    } else {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "Notification not found");
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return UBUS_STATUS_OK;
}

// Get comprehensive notification statistics
int notifications_comprehensive_ubus_get_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                                   struct ubus_request_data *req, const char *method,
                                                   struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    if (!notifications_comprehensive_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "Comprehensive notifications not initialized");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    blobmsg_add_u8(&bb, "success", 1);
    
    comprehensive_notification_statistics_t stats;
    if (notifications_comprehensive_get_statistics(&stats) == AUTONOMY_SUCCESS) {
        void *statistics_table = blobmsg_open_table(&bb, "statistics");
        
        // Overall statistics
        void *overall_table = blobmsg_open_table(&bb, "overall");
        blobmsg_add_u64(&bb, "total_notifications", stats.total_notifications);
        blobmsg_add_u64(&bb, "successful_notifications", stats.successful_notifications);
        blobmsg_add_u64(&bb, "failed_notifications", stats.failed_notifications);
        
        double success_rate = stats.total_notifications > 0 ? 
            (double)stats.successful_notifications / stats.total_notifications : 0.0;
        blobmsg_add_double(&bb, "success_rate", success_rate);
        
        blobmsg_add_u64(&bb, "suppressed_notifications", stats.suppressed_notifications);
        blobmsg_add_u64(&bb, "deduplicated_notifications", stats.deduplicated_notifications);
        blobmsg_add_u64(&bb, "rate_limited_notifications", stats.rate_limited_notifications);
        blobmsg_close_table(&bb, overall_table);
        
        // Channel statistics
        void *channels_table = blobmsg_open_table(&bb, "channels");
        const char* channel_names[] = {"pushover", "email", "sms", "webhook", "slack", "discord", "telegram"};
        uint64_t channel_sent[] = {stats.pushover_sent, stats.email_sent, stats.sms_sent, 
                                  stats.webhook_sent, stats.slack_sent, stats.discord_sent, stats.telegram_sent};
        double channel_rates[] = {stats.pushover_success_rate, stats.email_success_rate, stats.sms_success_rate,
                                 stats.webhook_success_rate, stats.slack_success_rate, stats.discord_success_rate, 
                                 stats.telegram_success_rate};
        
        for (int i = 0; i < 7; i++) {
            if (channel_sent[i] > 0) {
                void *channel_table = blobmsg_open_table(&bb, channel_names[i]);
                blobmsg_add_u64(&bb, "sent", channel_sent[i]);
                blobmsg_add_double(&bb, "success_rate", channel_rates[i]);
                blobmsg_close_table(&bb, channel_table);
            }
        }
        blobmsg_close_table(&bb, channels_table);
        
        // Priority statistics
        void *priorities_table = blobmsg_open_table(&bb, "priorities");
        blobmsg_add_u64(&bb, "emergency", stats.emergency_notifications);
        blobmsg_add_u64(&bb, "high", stats.high_notifications);
        blobmsg_add_u64(&bb, "normal", stats.normal_notifications);
        blobmsg_add_u64(&bb, "low", stats.low_notifications);
        blobmsg_close_table(&bb, priorities_table);
        
        // Intelligence statistics
        void *intelligence_table = blobmsg_open_table(&bb, "intelligence");
        blobmsg_add_u64(&bb, "priority_optimizations", stats.priority_optimizations);
        blobmsg_add_u64(&bb, "channel_optimizations", stats.channel_optimizations);
        blobmsg_add_u64(&bb, "delivery_optimizations", stats.delivery_optimizations);
        blobmsg_add_u64(&bb, "emergency_detections", stats.emergency_detections);
        blobmsg_close_table(&bb, intelligence_table);
        
        // Performance metrics
        void *performance_table = blobmsg_open_table(&bb, "performance");
        blobmsg_add_double(&bb, "avg_processing_time_ms", stats.average_processing_time_ms);
        blobmsg_add_double(&bb, "avg_delivery_time_ms", stats.average_delivery_time_ms);
        blobmsg_add_double(&bb, "avg_acknowledgment_time_ms", stats.average_acknowledgment_time_ms);
        blobmsg_add_double(&bb, "overall_effectiveness", stats.overall_effectiveness_score);
        blobmsg_close_table(&bb, performance_table);
        
        blobmsg_close_table(&bb, statistics_table);
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return UBUS_STATUS_OK;
}

// Additional UBUS method implementations would continue here...

// UBUS method definitions
const struct ubus_method notifications_comprehensive_ubus_methods[] = {
    UBUS_METHOD("send", notifications_comprehensive_ubus_send, notification_send_policy),
    UBUS_METHOD("send_emergency", notifications_comprehensive_ubus_send_emergency, notification_emergency_policy),
    UBUS_METHOD("get_status", notifications_comprehensive_ubus_get_status, notification_status_policy),
    UBUS_METHOD_NOARG("get_statistics", notifications_comprehensive_ubus_get_statistics),
    UBUS_METHOD("get_history", notifications_comprehensive_ubus_get_history, NULL),
    UBUS_METHOD("acknowledge", notifications_comprehensive_ubus_acknowledge, notification_ack_policy),
    UBUS_METHOD("test_channels", notifications_comprehensive_ubus_test_channels, notification_test_policy),
    UBUS_METHOD_NOARG("get_channel_effectiveness", notifications_comprehensive_ubus_get_channel_effectiveness),
    UBUS_METHOD_NOARG("get_config", notifications_comprehensive_ubus_get_config),
    UBUS_METHOD("set_config", notifications_comprehensive_ubus_set_config, NULL),
    UBUS_METHOD_NOARG("reset_statistics", notifications_comprehensive_ubus_reset_statistics),
    UBUS_METHOD_NOARG("health_check", notifications_comprehensive_ubus_health_check),
};

const int notifications_comprehensive_ubus_methods_count = ARRAY_SIZE(notifications_comprehensive_ubus_methods);