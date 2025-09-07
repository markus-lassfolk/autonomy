#ifndef NOTIFICATION_TYPES_H
#define NOTIFICATION_TYPES_H

#include <time.h>
#include <stdbool.h>
#include <stdint.h>

// Notification types matching the Go implementation
typedef enum {
    // Failover and network events
    NOTIFICATION_TYPE_FAILOVER = 0,
    NOTIFICATION_TYPE_FAILBACK,
    NOTIFICATION_TYPE_MEMBER_DOWN,
    NOTIFICATION_TYPE_MEMBER_UP,
    NOTIFICATION_TYPE_PREDICTIVE,
    
    // System events
    NOTIFICATION_TYPE_CRITICAL_ERROR,
    NOTIFICATION_TYPE_SYSTEM_HEALTH,
    NOTIFICATION_TYPE_RECOVERY,
    
    // Status updates
    NOTIFICATION_TYPE_STATUS_UPDATE,
    NOTIFICATION_TYPE_SUMMARY,
    
    // Data limit notifications
    NOTIFICATION_TYPE_DATA_LIMIT_FAILOVER,
    NOTIFICATION_TYPE_DATA_LIMIT_FAILBACK,
    NOTIFICATION_TYPE_DATA_LIMIT_DAILY_80,
    NOTIFICATION_TYPE_DATA_LIMIT_DAILY_100,
    NOTIFICATION_TYPE_DATA_LIMIT_MONTHLY_80,
    NOTIFICATION_TYPE_DATA_LIMIT_MONTHLY_95,
    NOTIFICATION_TYPE_DATA_LIMIT_EXCEEDED,
    NOTIFICATION_TYPE_DATA_LIMIT_RESET,
    NOTIFICATION_TYPE_DATA_USAGE_SPIKE,
    
    // Generic types
    NOTIFICATION_TYPE_INFO,
    NOTIFICATION_TYPE_WARNING,
    NOTIFICATION_TYPE_ERROR,
    NOTIFICATION_TYPE_EMERGENCY
} notification_type_t;

// Priority levels matching Pushover API
typedef enum {
    NOTIFICATION_PRIORITY_LOWEST = -2,    // No notification/sound
    NOTIFICATION_PRIORITY_LOW = -1,       // Quiet notification
    NOTIFICATION_PRIORITY_NORMAL = 0,     // Normal notification
    NOTIFICATION_PRIORITY_HIGH = 1,       // High-priority notification
    NOTIFICATION_PRIORITY_EMERGENCY = 2   // Emergency notification with retry
} notification_priority_t;

// Notification channels
typedef enum {
    NOTIFICATION_CHANNEL_PUSHOVER = 0,
    NOTIFICATION_CHANNEL_EMAIL,
    NOTIFICATION_CHANNEL_SLACK,
    NOTIFICATION_CHANNEL_DISCORD,
    NOTIFICATION_CHANNEL_TELEGRAM,
    NOTIFICATION_CHANNEL_WEBHOOK,
    NOTIFICATION_CHANNEL_SMS,
    NOTIFICATION_CHANNEL_SYSLOG,
    NOTIFICATION_CHANNEL_UBUS
} notification_channel_t;

// Notification status
typedef enum {
    NOTIFICATION_STATUS_PENDING = 0,
    NOTIFICATION_STATUS_SENT,
    NOTIFICATION_STATUS_FAILED,
    NOTIFICATION_STATUS_ACKNOWLEDGED,
    NOTIFICATION_STATUS_SUPPRESSED,
    NOTIFICATION_STATUS_ESCALATED
} notification_status_t;

// Location data structure
typedef struct {
    double latitude;
    double longitude;
    char address[256];
    char source[64];  // "starlink", "rutos", "manual"
} notification_location_t;

// Notification event structure
typedef struct {
    char id[64];                           // Unique notification ID
    char title[128];                       // Notification title
    char message[512];                     // Notification message
    notification_type_t type;              // Notification type
    notification_priority_t priority;      // Priority level
    char sound[64];                        // Sound to play
    char url[256];                         // URL to open
    char url_title[128];                   // URL title
    time_t timestamp;                      // Creation timestamp
    
    // Enhanced context data
    char member_name[64];                  // Network member name
    char from_member[64];                  // Source member
    char to_member[64];                    // Target member
    char error_details[256];               // Error details
    char details_json[1024];               // Additional details as JSON
    
    // Rich context features
    notification_location_t *location;     // GPS coordinates
    time_t duration;                       // How long the issue lasted
    bool acknowledged;                     // Has user acknowledged this?
    char message_id[64];                  // External message ID for tracking
} notification_event_t;

// Notification configuration
typedef struct {
    // Pushover settings
    bool pushover_enabled;
    char pushover_token[128];
    char pushover_user[128];
    char pushover_device[64];
    
    // Advanced Pushover features
    char priority_threshold[32];           // "info", "warning", "critical", "emergency"
    bool acknowledgment_tracking;          // Track message acknowledgments
    bool location_enabled;                 // Include GPS coordinates
    bool rich_context_enabled;             // Include detailed metrics
    
    // Notification control
    bool notify_on_failover;
    bool notify_on_failback;
    bool notify_on_member_down;
    bool notify_on_member_up;
    bool notify_on_predictive;
    bool notify_on_critical;
    bool notify_on_recovery;
    bool notify_on_status_update;
    
    // Timing and rate limiting
    int cooldown_period_seconds;
    int max_notifications_hour;
    int emergency_cooldown_seconds;
    
    // Priority settings
    int priority_failover;
    int priority_failback;
    int priority_member_down;
    int priority_member_up;
    int priority_predictive;
    int priority_critical;
    int priority_recovery;
    int priority_status_update;
    
    // Advanced settings
    int retry_attempts;
    int retry_delay_seconds;
    int http_timeout_seconds;
    bool include_hostname;
    bool include_timestamp;
    
    // Smart rate limiting (priority-based cooldowns)
    int info_cooldown_hours;
    int warning_cooldown_hours;
    int critical_cooldown_minutes;
    int emergency_retry_interval_seconds;
} notification_config_t;

// Channel-specific configurations
typedef struct {
    // Pushover configuration
    struct {
        bool enabled;
        char token[128];
        char user[128];
        char device[64];
    } pushover;
    
    // Email configuration
    struct {
        bool enabled;
        char smtp_host[128];
        int smtp_port;
        char username[128];
        char password[128];
        char from[128];
        char to[512];
        bool use_tls;
        bool use_starttls;
    } email;
    
    // Slack configuration
    struct {
        bool enabled;
        char webhook_url[256];
        char channel[64];
        char username[64];
        char icon_emoji[32];
        char icon_url[256];
    } slack;
    
    // Discord configuration
    struct {
        bool enabled;
        char webhook_url[256];
        char username[64];
        char avatar_url[256];
    } discord;
    
    // Telegram configuration
    struct {
        bool enabled;
        char token[128];
        char chat_id[64];
    } telegram;
    
    // Webhook configuration
    struct {
        bool enabled;
        char url[256];
        char method[16];
        char content_type[64];
        char auth_token[128];
        int timeout_seconds;
        bool verify_ssl;
    } webhook;
    
    // SMS configuration
    struct {
        bool enabled;
        char provider[64];
        char api_key[128];
        char api_secret[128];
        char from_number[32];
        char to_numbers[512];
    } sms;
} channel_config_t;

// Smart manager configuration
typedef struct {
    // Rate limiting configuration
    int max_notifications_per_hour;
    int max_notifications_per_minute;
    int burst_limit;
    
    // Priority-based rate limiting
    int emergency_rate_limit;
    int high_rate_limit;
    int normal_rate_limit;
    int low_rate_limit;
    
    // Cooldown periods by priority
    int emergency_cooldown_seconds;
    int high_cooldown_seconds;
    int normal_cooldown_seconds;
    int low_cooldown_seconds;
    int lowest_cooldown_seconds;
    
    // Deduplication settings
    bool deduplication_enabled;
    int deduplication_window_seconds;
    double similarity_threshold;
    
    // Intelligent features
    bool adaptive_rate_limiting;
    bool priority_escalation;
    int escalation_threshold;
    int escalation_delay_seconds;
    
    // Suppression rules
    bool quiet_hours;
    char quiet_hours_start[8];     // "22:00"
    char quiet_hours_end[8];       // "08:00"
    char quiet_hours_timezone[64];
    char suppress_low_priority_days[128]; // "saturday,sunday"
    int suppress_low_priority_days_count;
    
    // Advanced settings
    int history_retention_days;
    int max_history_size;
    int max_suppression_rules;
    bool enable_statistics;
} smart_manager_config_t;

// Notification record for history
typedef struct {
    char id[64];
    notification_type_t type;
    notification_priority_t priority;
    char title[128];
    char message[512];
    time_t timestamp;
    notification_channel_t channels[8];
    int channel_count;
    bool success;
    char error_message[256];
    char context_json[1024];
    char fingerprint[64];
    bool suppressed;
    bool escalated;
} notification_record_t;

// Suppression rule
typedef struct {
    char id[64];
    char name[128];
    bool enabled;
    char conditions_json[1024];
    time_t created_at;
    time_t updated_at;
} suppression_rule_t;

// Notification statistics
typedef struct {
    uint64_t total_notifications;
    uint64_t sent_notifications;
    uint64_t failed_notifications;
    uint64_t acknowledged_notifications;
    uint64_t suppressed_notifications;
    uint64_t escalated_notifications;
    
    // Channel-specific stats
    uint64_t pushover_sent;
    uint64_t pushover_failed;
    uint64_t email_sent;
    uint64_t email_failed;
    uint64_t slack_sent;
    uint64_t slack_failed;
    uint64_t discord_sent;
    uint64_t discord_failed;
    uint64_t telegram_sent;
    uint64_t telegram_failed;
    uint64_t webhook_sent;
    uint64_t webhook_failed;
    uint64_t sms_sent;
    uint64_t sms_failed;
    
    // Priority-based stats
    uint64_t emergency_sent;
    uint64_t high_sent;
    uint64_t normal_sent;
    uint64_t low_sent;
    
    // Rate limiting stats
    uint64_t rate_limited_count;
    uint64_t deduplicated_count;
    uint64_t suppressed_count;
    
    // Timing stats
    time_t last_notification_time;
    time_t last_emergency_time;
    time_t last_critical_time;
} notification_stats_t;

// Function prototypes for notification types
const char* notification_type_to_string(notification_type_t type);
notification_type_t string_to_notification_type(const char* str);
const char* notification_priority_to_string(notification_priority_t priority);
notification_priority_t string_to_notification_priority(const char* str);
const char* notification_channel_to_string(notification_channel_t channel);
notification_channel_t string_to_notification_channel(const char* str);
const char* notification_status_to_string(notification_status_t status);

#endif // NOTIFICATION_TYPES_H
