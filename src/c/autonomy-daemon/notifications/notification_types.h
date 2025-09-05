#ifndef NOTIFICATION_TYPES_H
#define NOTIFICATION_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Notification types and structures

// Notification priority levels
typedef enum {
    NOTIFICATION_PRIORITY_LOW = 0,
    NOTIFICATION_PRIORITY_MEDIUM,
    NOTIFICATION_PRIORITY_HIGH,
    NOTIFICATION_PRIORITY_CRITICAL
} notification_priority_t;

// Notification delivery status
typedef enum {
    NOTIFICATION_STATUS_PENDING = 0,
    NOTIFICATION_STATUS_SENT,
    NOTIFICATION_STATUS_DELIVERED,
    NOTIFICATION_STATUS_FAILED,
    NOTIFICATION_STATUS_ACKNOWLEDGED
} notification_status_t;

// Notification delivery method
typedef enum {
    NOTIFICATION_METHOD_WEBHOOK = 0,
    NOTIFICATION_METHOD_EMAIL,
    NOTIFICATION_METHOD_PUSHOVER,
    NOTIFICATION_METHOD_DISCORD,
    NOTIFICATION_METHOD_SLACK
} notification_method_t;

// Notification structure
typedef struct {
    char id[64];
    char title[256];
    char message[1024];
    notification_priority_t priority;
    notification_method_t method;
    notification_status_t status;
    time_t created_time;
    time_t sent_time;
    time_t delivered_time;
    int retry_count;
    int max_retries;
    char recipient[256];
    char metadata[512];
} notification_t;

// Notification configuration
typedef struct {
    bool webhook_enabled;
    char webhook_url[512];
    bool email_enabled;
    char email_smtp_server[256];
    int email_smtp_port;
    char email_username[128];
    char email_password[128];
    char email_from[128];
    bool pushover_enabled;
    char pushover_token[128];
    char pushover_user[128];
    bool discord_enabled;
    char discord_webhook_url[512];
    bool slack_enabled;
    char slack_webhook_url[512];
    int rate_limit_per_minute;
    int max_retries;
} notification_config_t;

// Function declarations
int notification_init(const notification_config_t *config);
void notification_cleanup(void);
int notification_send(const notification_t *notification);
int notification_send_async(const notification_t *notification);
int notification_get_status(const char *notification_id, notification_status_t *status);
int notification_acknowledge(const char *notification_id);

#endif // NOTIFICATION_TYPES_H
