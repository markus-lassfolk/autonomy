#include "notification_types.h"
#include <string.h>
#include <stdio.h>

// Notification type to string conversion
const char* notification_type_to_string(notification_type_t type) {
    switch (type) {
        case NOTIFICATION_TYPE_FAILOVER: return "failover";
        case NOTIFICATION_TYPE_FAILBACK: return "failback";
        case NOTIFICATION_TYPE_MEMBER_DOWN: return "member_down";
        case NOTIFICATION_TYPE_MEMBER_UP: return "member_up";
        case NOTIFICATION_TYPE_PREDICTIVE: return "predictive";
        case NOTIFICATION_TYPE_CRITICAL_ERROR: return "critical_error";
        case NOTIFICATION_TYPE_SYSTEM_HEALTH: return "system_health";
        case NOTIFICATION_TYPE_RECOVERY: return "recovery";
        case NOTIFICATION_TYPE_STATUS_UPDATE: return "status_update";
        case NOTIFICATION_TYPE_SUMMARY: return "summary";
        case NOTIFICATION_TYPE_DATA_LIMIT_FAILOVER: return "data_limit_failover";
        case NOTIFICATION_TYPE_DATA_LIMIT_FAILBACK: return "data_limit_failback";
        case NOTIFICATION_TYPE_DATA_LIMIT_DAILY_80: return "data_limit_daily_80";
        case NOTIFICATION_TYPE_DATA_LIMIT_DAILY_100: return "data_limit_daily_100";
        case NOTIFICATION_TYPE_DATA_LIMIT_MONTHLY_80: return "data_limit_monthly_80";
        case NOTIFICATION_TYPE_DATA_LIMIT_MONTHLY_95: return "data_limit_monthly_95";
        case NOTIFICATION_TYPE_DATA_LIMIT_EXCEEDED: return "data_limit_exceeded";
        case NOTIFICATION_TYPE_DATA_LIMIT_RESET: return "data_limit_reset";
        case NOTIFICATION_TYPE_DATA_USAGE_SPIKE: return "data_usage_spike";
        case NOTIFICATION_TYPE_INFO: return "info";
        case NOTIFICATION_TYPE_WARNING: return "warning";
        case NOTIFICATION_TYPE_ERROR: return "error";
        case NOTIFICATION_TYPE_EMERGENCY: return "emergency";
        default: return "unknown";
    }
}

// String to notification type conversion
static notification_type_t string_to_notification_type(const char* str) {
    if (!str) return NOTIFICATION_TYPE_INFO;
    
    if (strcmp(str, "failover") == 0) return NOTIFICATION_TYPE_FAILOVER;
    if (strcmp(str, "failback") == 0) return NOTIFICATION_TYPE_FAILBACK;
    if (strcmp(str, "member_down") == 0) return NOTIFICATION_TYPE_MEMBER_DOWN;
    if (strcmp(str, "member_up") == 0) return NOTIFICATION_TYPE_MEMBER_UP;
    if (strcmp(str, "predictive") == 0) return NOTIFICATION_TYPE_PREDICTIVE;
    if (strcmp(str, "critical_error") == 0) return NOTIFICATION_TYPE_CRITICAL_ERROR;
    if (strcmp(str, "system_health") == 0) return NOTIFICATION_TYPE_SYSTEM_HEALTH;
    if (strcmp(str, "recovery") == 0) return NOTIFICATION_TYPE_RECOVERY;
    if (strcmp(str, "status_update") == 0) return NOTIFICATION_TYPE_STATUS_UPDATE;
    if (strcmp(str, "summary") == 0) return NOTIFICATION_TYPE_SUMMARY;
    if (strcmp(str, "data_limit_failover") == 0) return NOTIFICATION_TYPE_DATA_LIMIT_FAILOVER;
    if (strcmp(str, "data_limit_failback") == 0) return NOTIFICATION_TYPE_DATA_LIMIT_FAILBACK;
    if (strcmp(str, "data_limit_daily_80") == 0) return NOTIFICATION_TYPE_DATA_LIMIT_DAILY_80;
    if (strcmp(str, "data_limit_daily_100") == 0) return NOTIFICATION_TYPE_DATA_LIMIT_DAILY_100;
    if (strcmp(str, "data_limit_monthly_80") == 0) return NOTIFICATION_TYPE_DATA_LIMIT_MONTHLY_80;
    if (strcmp(str, "data_limit_monthly_95") == 0) return NOTIFICATION_TYPE_DATA_LIMIT_MONTHLY_95;
    if (strcmp(str, "data_limit_exceeded") == 0) return NOTIFICATION_TYPE_DATA_LIMIT_EXCEEDED;
    if (strcmp(str, "data_limit_reset") == 0) return NOTIFICATION_TYPE_DATA_LIMIT_RESET;
    if (strcmp(str, "data_usage_spike") == 0) return NOTIFICATION_TYPE_DATA_USAGE_SPIKE;
    if (strcmp(str, "info") == 0) return NOTIFICATION_TYPE_INFO;
    if (strcmp(str, "warning") == 0) return NOTIFICATION_TYPE_WARNING;
    if (strcmp(str, "error") == 0) return NOTIFICATION_TYPE_ERROR;
    if (strcmp(str, "emergency") == 0) return NOTIFICATION_TYPE_EMERGENCY;
    
    return NOTIFICATION_TYPE_INFO; // Default fallback
}

// Notification priority to string conversion
const char* notification_priority_to_string(notification_priority_t priority) {
    switch (priority) {
        case NOTIFICATION_PRIORITY_LOWEST: return "lowest";
        case NOTIFICATION_PRIORITY_LOW: return "low";
        case NOTIFICATION_PRIORITY_NORMAL: return "normal";
        case NOTIFICATION_PRIORITY_HIGH: return "high";
        case NOTIFICATION_PRIORITY_EMERGENCY: return "emergency";
        default: return "normal";
    }
}

// String to notification priority conversion
notification_priority_t string_to_notification_priority(const char* str) {
    if (!str) return NOTIFICATION_PRIORITY_NORMAL;
    
    if (strcmp(str, "lowest") == 0) return NOTIFICATION_PRIORITY_LOWEST;
    if (strcmp(str, "low") == 0) return NOTIFICATION_PRIORITY_LOW;
    if (strcmp(str, "normal") == 0) return NOTIFICATION_PRIORITY_NORMAL;
    if (strcmp(str, "high") == 0) return NOTIFICATION_PRIORITY_HIGH;
    if (strcmp(str, "emergency") == 0) return NOTIFICATION_PRIORITY_EMERGENCY;
    
    return NOTIFICATION_PRIORITY_NORMAL; // Default fallback
}

// Notification channel to string conversion
const char* notification_channel_to_string(notification_channel_t channel) {
    switch (channel) {
        case NOTIFICATION_CHANNEL_PUSHOVER: return "pushover";
        case NOTIFICATION_CHANNEL_EMAIL: return "email";
        case NOTIFICATION_CHANNEL_SLACK: return "slack";
        case NOTIFICATION_CHANNEL_DISCORD: return "discord";
        case NOTIFICATION_CHANNEL_TELEGRAM: return "telegram";
        case NOTIFICATION_CHANNEL_WEBHOOK: return "webhook";
        case NOTIFICATION_CHANNEL_SMS: return "sms";
        case NOTIFICATION_CHANNEL_SYSLOG: return "syslog";
        case NOTIFICATION_CHANNEL_UBUS: return "ubus";
        default: return "unknown";
    }
}

// String to notification channel conversion
notification_channel_t string_to_notification_channel(const char* str) {
    if (!str) return NOTIFICATION_CHANNEL_WEBHOOK;
    
    if (strcmp(str, "pushover") == 0) return NOTIFICATION_CHANNEL_PUSHOVER;
    if (strcmp(str, "email") == 0) return NOTIFICATION_CHANNEL_EMAIL;
    if (strcmp(str, "slack") == 0) return NOTIFICATION_CHANNEL_SLACK;
    if (strcmp(str, "discord") == 0) return NOTIFICATION_CHANNEL_DISCORD;
    if (strcmp(str, "telegram") == 0) return NOTIFICATION_CHANNEL_TELEGRAM;
    if (strcmp(str, "webhook") == 0) return NOTIFICATION_CHANNEL_WEBHOOK;
    if (strcmp(str, "sms") == 0) return NOTIFICATION_CHANNEL_SMS;
    if (strcmp(str, "syslog") == 0) return NOTIFICATION_CHANNEL_SYSLOG;
    if (strcmp(str, "ubus") == 0) return NOTIFICATION_CHANNEL_UBUS;
    
    return NOTIFICATION_CHANNEL_WEBHOOK; // Default fallback
}

// Notification status to string conversion
const char* notification_status_to_string(notification_status_t status) {
    switch (status) {
        case NOTIFICATION_STATUS_PENDING: return "pending";
        case NOTIFICATION_STATUS_SENT: return "sent";
        case NOTIFICATION_STATUS_FAILED: return "failed";
        case NOTIFICATION_STATUS_ACKNOWLEDGED: return "acknowledged";
        case NOTIFICATION_STATUS_SUPPRESSED: return "suppressed";
        case NOTIFICATION_STATUS_ESCALATED: return "escalated";
        default: return "unknown";
    }
}
