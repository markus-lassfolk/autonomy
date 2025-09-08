#ifndef NOTIFICATIONS_COMPREHENSIVE_H
#define NOTIFICATIONS_COMPREHENSIVE_H

#include "notification_types.h"
#include "smart_manager.h"
#include "intelligence_engine.h"
#include "acknowledgment_tracker.h"
#include "delivery_optimizer.h"

// Types are already defined in the included headers
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

// Comprehensive notification delivery status
typedef enum {
    NOTIFICATION_DELIVERY_PENDING = 0,
    NOTIFICATION_DELIVERY_SENDING,
    NOTIFICATION_DELIVERY_SENT,
    NOTIFICATION_DELIVERY_DELIVERED,
    NOTIFICATION_DELIVERY_ACKNOWLEDGED,
    NOTIFICATION_DELIVERY_FAILED,
    NOTIFICATION_DELIVERY_SUPPRESSED,
    NOTIFICATION_DELIVERY_DEDUPLICATED,
    NOTIFICATION_DELIVERY_RATE_LIMITED,
    NOTIFICATION_DELIVERY_MAX
} notification_delivery_status_t;

// Enhanced notification record with comprehensive tracking
typedef struct {
    char id[64];                           // Unique notification ID
    notification_type_t type;              // Notification type
    notification_priority_t priority;      // Priority level
    char title[256];                       // Notification title
    char message[1024];                    // Notification message
    char fingerprint[64];                  // Deduplication fingerprint
    
    // Delivery tracking
    notification_delivery_status_t status; // Current delivery status
    time_t created_at;                     // Creation timestamp
    time_t sent_at;                        // Sent timestamp
    time_t delivered_at;                   // Delivery confirmation timestamp
    time_t acknowledged_at;                // Acknowledgment timestamp
    
    // Channel delivery tracking
    bool sent_pushover;                    // Sent via Pushover
    bool sent_email;                       // Sent via email
    bool sent_sms;                         // Sent via SMS
    bool sent_webhook;                     // Sent via webhook
    bool sent_slack;                       // Sent via Slack
    bool sent_discord;                     // Sent via Discord
    bool sent_telegram;                    // Sent via Telegram
    
    // Delivery results
    bool pushover_success;                 // Pushover delivery success
    bool email_success;                    // Email delivery success
    bool sms_success;                      // SMS delivery success
    bool webhook_success;                  // Webhook delivery success
    bool slack_success;                    // Slack delivery success
    bool discord_success;                  // Discord delivery success
    bool telegram_success;                 // Telegram delivery success
    
    // Intelligence data
    bool suppressed;                       // Suppressed by rules
    bool deduplicated;                     // Deduplicated
    bool rate_limited;                     // Rate limited
    bool priority_optimized;               // Priority was optimized
    bool channels_optimized;               // Channels were optimized
    bool delivery_optimized;               // Delivery timing was optimized
    
    // Context and metadata
    char context_json[2048];               // Context data as JSON
    char source_module[64];                // Source module
    char trigger_event[128];               // Triggering event
    double processing_time_ms;             // Processing time
    int retry_count;                       // Number of retries
    
    // Acknowledgment data
    char acknowledgment_id[64];            // Acknowledgment ID (if required)
    bool acknowledgment_required;          // Whether acknowledgment is required
    time_t acknowledgment_expires_at;      // Acknowledgment expiry time
    char acknowledged_by[128];             // Who acknowledged
    
    // Quality metrics
    double delivery_confidence;            // Delivery confidence score
    double user_engagement_score;          // User engagement prediction
    double channel_effectiveness_score;    // Channel effectiveness score
} comprehensive_notification_record_t;

// Comprehensive notification statistics
typedef struct {
    // Overall statistics
    uint64_t total_notifications;          // Total notifications processed
    uint64_t successful_notifications;     // Successfully delivered
    uint64_t failed_notifications;         // Failed to deliver
    uint64_t suppressed_notifications;     // Suppressed by rules
    uint64_t deduplicated_notifications;   // Deduplicated
    uint64_t rate_limited_notifications;   // Rate limited
    
    // Channel statistics
    uint64_t pushover_sent;                // Pushover notifications sent
    uint64_t email_sent;                   // Email notifications sent
    uint64_t sms_sent;                     // SMS notifications sent
    uint64_t webhook_sent;                 // Webhook notifications sent
    uint64_t slack_sent;                   // Slack notifications sent
    uint64_t discord_sent;                 // Discord notifications sent
    uint64_t telegram_sent;                // Telegram notifications sent
    
    // Channel success rates
    double pushover_success_rate;          // Pushover success rate
    double email_success_rate;             // Email success rate
    double sms_success_rate;               // SMS success rate
    double webhook_success_rate;           // Webhook success rate
    double slack_success_rate;             // Slack success rate
    double discord_success_rate;           // Discord success rate
    double telegram_success_rate;          // Telegram success rate
    
    // Priority statistics
    uint64_t emergency_notifications;      // Emergency priority
    uint64_t high_notifications;           // High priority
    uint64_t normal_notifications;         // Normal priority
    uint64_t low_notifications;            // Low priority
    
    // Intelligence statistics
    uint64_t priority_optimizations;       // Priority optimizations performed
    uint64_t channel_optimizations;        // Channel optimizations performed
    uint64_t delivery_optimizations;       // Delivery optimizations performed
    uint64_t emergency_detections;         // Emergency conditions detected
    
    // Performance metrics
    double average_processing_time_ms;     // Average processing time
    double average_delivery_time_ms;       // Average delivery time
    double average_acknowledgment_time_ms; // Average acknowledgment time
    double overall_effectiveness_score;    // Overall effectiveness
    
    // Time tracking
    time_t stats_start_time;               // When statistics started
    time_t last_notification;              // Last notification time
    time_t last_reset;                     // Last statistics reset
} comprehensive_notification_statistics_t;

// Comprehensive notification configuration
typedef struct {
    // Basic configuration
    bool enabled;                          // Enable comprehensive notifications
    bool intelligence_enabled;             // Enable notification intelligence
    bool acknowledgment_tracking_enabled;  // Enable acknowledgment tracking
    bool delivery_optimization_enabled;    // Enable delivery optimization
    
    // Rate limiting configuration
    int max_notifications_per_hour;        // Max notifications per hour
    int max_notifications_per_minute;      // Max notifications per minute
    int burst_limit;                       // Burst limit
    
    // Priority rate limits
    int emergency_rate_limit;              // Emergency notifications per hour
    int high_rate_limit;                   // High priority per hour
    int normal_rate_limit;                 // Normal priority per hour
    int low_rate_limit;                    // Low priority per hour
    
    // Cooldown periods (seconds)
    int emergency_cooldown_s;              // Emergency cooldown
    int high_cooldown_s;                   // High priority cooldown
    int normal_cooldown_s;                 // Normal priority cooldown
    int low_cooldown_s;                    // Low priority cooldown
    
    // Deduplication configuration
    bool deduplication_enabled;            // Enable deduplication
    int deduplication_window_s;            // Deduplication window in seconds
    double similarity_threshold;           // Similarity threshold (0.0-1.0)
    
    // Intelligence configuration
    bool priority_optimization_enabled;    // Enable priority optimization
    bool channel_intelligence_enabled;     // Enable channel intelligence
    bool emergency_detection_enabled;      // Enable emergency detection
    bool learning_enabled;                 // Enable learning from delivery results
    
    // Acknowledgment configuration
    bool acknowledgment_required_critical; // Require ack for critical notifications
    bool acknowledgment_required_emergency; // Require ack for emergency notifications
    int acknowledgment_timeout_s;          // Acknowledgment timeout
    bool auto_resolve_enabled;             // Auto-resolve acknowledgments
    int auto_resolve_time_s;               // Auto-resolve time
    
    // Channel configuration
    bool pushover_enabled;                 // Enable Pushover
    bool email_enabled;                    // Enable email
    bool sms_enabled;                      // Enable SMS
    bool webhook_enabled;                  // Enable webhook
    bool slack_enabled;                    // Enable Slack
    bool discord_enabled;                  // Enable Discord
    bool telegram_enabled;                 // Enable Telegram
    
    // Email configuration
    char email_smtp_server[256];           // SMTP server
    int email_smtp_port;                   // SMTP port
    char email_username[128];              // Email username
    char email_password[128];              // Email password
    char email_recipients[512];            // Email recipients
    
    // Telegram configuration
    char telegram_bot_token[128];          // Telegram bot token
    char telegram_chat_id[64];             // Telegram chat ID
    
    // Webhook configuration
    char webhook_url[512];                 // Webhook URL
    int webhook_timeout;                   // Webhook timeout
    
    // Rate limiting
    bool rate_limit_enabled;               // Enable rate limiting
    int rate_limit_max_per_hour;           // Max per hour
    int rate_limit_max_per_day;            // Max per day
    
    // Quality thresholds
    double min_delivery_confidence;        // Minimum delivery confidence
    double min_channel_effectiveness;      // Minimum channel effectiveness
    int max_retry_attempts;                // Maximum retry attempts
    int retry_backoff_s;                   // Retry backoff time
} comprehensive_notification_config_t;

// Main comprehensive notifications manager
typedef struct {
    comprehensive_notification_config_t config; // Configuration
    comprehensive_notification_statistics_t stats; // Statistics
    
    // Core components (existing)
    smart_notification_manager_t* smart_manager;     // Smart manager
    intelligence_engine_t* intelligence_engine;     // Intelligence engine
    acknowledgment_tracker_t* acknowledgment_tracker; // Acknowledgment tracker
    delivery_optimizer_t* delivery_optimizer;       // Delivery optimizer
    
    // Enhanced tracking
    comprehensive_notification_record_t* records; // Notification records
    int max_records;                       // Maximum records to keep
    int record_count;                      // Current record count
    int record_index;                      // Current record index (circular buffer)
    
    // Performance tracking
    double* processing_times;              // Processing time samples
    double* delivery_times;                // Delivery time samples
    int max_samples;                       // Maximum samples to keep
    int sample_count;                      // Current sample count
    int sample_index;                      // Current sample index
    
    // Threading
    pthread_mutex_t mutex;                 // Main mutex
    pthread_t processing_thread;           // Processing thread
    pthread_t analytics_thread;            // Analytics thread
    bool threads_running;                  // Thread status
    
    // State
    bool initialized;                      // Initialization status
    time_t last_analytics_update;          // Last analytics update
    time_t last_cleanup;                   // Last cleanup time
} comprehensive_notifications_manager_t;

// Function prototypes

/**
 * Initialize comprehensive notifications system
 * @param config Comprehensive notification configuration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int notifications_comprehensive_init(const comprehensive_notification_config_t* config);

/**
 * Cleanup comprehensive notifications system
 */
void notifications_comprehensive_cleanup(void);

/**
 * Send comprehensive notification with full intelligence and tracking
 * @param type Notification type
 * @param priority Priority level
 * @param title Notification title
 * @param message Notification message
 * @param context_json Context data as JSON string
 * @param source_module Source module name
 * @return Notification record ID on success, NULL on failure
 */
const char* notifications_comprehensive_send(notification_type_t type,
                                            notification_priority_t priority,
                                            const char* title,
                                            const char* message,
                                            const char* context_json,
                                            const char* source_module);

/**
 * Send emergency notification with bypass capabilities
 * @param title Emergency title
 * @param message Emergency message
 * @param context_json Context data
 * @param source_module Source module
 * @return Notification record ID on success, NULL on failure
 */
const char* notifications_comprehensive_send_emergency(const char* title,
                                                      const char* message,
                                                      const char* context_json,
                                                      const char* source_module);

/**
 * Get notification delivery status
 * @param notification_id Notification ID
 * @param record Comprehensive record structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int notifications_comprehensive_get_status(const char* notification_id,
                                          comprehensive_notification_record_t* record);

/**
 * Get comprehensive notification statistics
 * @param stats Statistics structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int notifications_comprehensive_get_statistics(comprehensive_notification_statistics_t* stats);

/**
 * Get notification history
 * @param records Array to store notification records
 * @param max_records Maximum records to return
 * @param start_time Start time filter (0 for all)
 * @param end_time End time filter (0 for all)
 * @return Number of records returned, or negative error code
 */
int notifications_comprehensive_get_history(comprehensive_notification_record_t* records,
                                           int max_records,
                                           time_t start_time,
                                           time_t end_time);

/**
 * Acknowledge notification
 * @param notification_id Notification ID
 * @param acknowledged_by Who acknowledged
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int notifications_comprehensive_acknowledge(const char* notification_id,
                                           const char* acknowledged_by);

/**
 * Test notification delivery to all channels
 * @param test_message Test message
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int notifications_comprehensive_test_all_channels(const char* test_message);

/**
 * Get channel effectiveness scores
 * @param pushover_score Pushover effectiveness score
 * @param email_score Email effectiveness score
 * @param sms_score SMS effectiveness score
 * @param webhook_score Webhook effectiveness score
 * @param slack_score Slack effectiveness score
 * @param discord_score Discord effectiveness score
 * @param telegram_score Telegram effectiveness score
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int notifications_comprehensive_get_channel_effectiveness(double* pushover_score,
                                                         double* email_score,
                                                         double* sms_score,
                                                         double* webhook_score,
                                                         double* slack_score,
                                                         double* discord_score,
                                                         double* telegram_score);

/**
 * Reset comprehensive notification statistics
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int notifications_comprehensive_reset_statistics(void);

/**
 * Get comprehensive notification configuration
 * @param config Configuration structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int notifications_comprehensive_get_config(comprehensive_notification_config_t* config);

/**
 * Set comprehensive notification configuration
 * @param config New configuration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int notifications_comprehensive_set_config(const comprehensive_notification_config_t* config);

/**
 * Check if comprehensive notifications system is initialized
 * @return true if initialized, false otherwise
 */
bool notifications_comprehensive_is_initialized(void);

/**
 * Perform comprehensive notifications health check
 * @return AUTONOMY_SUCCESS if healthy, error code if issues detected
 */
int notifications_comprehensive_health_check(void);

// Utility functions for reusable functionality

/**
 * Generate notification fingerprint for deduplication
 * @param type Notification type
 * @param title Notification title
 * @param message Notification message
 * @param fingerprint Buffer to store fingerprint (min 64 bytes)
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int notifications_generate_fingerprint(notification_type_t type,
                                       const char* title,
                                       const char* message,
                                       char* fingerprint);

/**
 * Calculate notification similarity
 * @param record1 First notification record
 * @param record2 Second notification record
 * @return Similarity score (0.0-1.0)
 */
double notifications_calculate_similarity(const comprehensive_notification_record_t* record1,
                                         const comprehensive_notification_record_t* record2);

/**
 * Check if notification should be suppressed
 * @param type Notification type
 * @param priority Priority level
 * @param context_json Context data
 * @return true if should be suppressed, false otherwise
 */
bool notifications_should_suppress(notification_type_t type,
                                   notification_priority_t priority,
                                   const char* context_json);

/**
 * Optimize notification priority based on context
 * @param type Notification type
 * @param base_priority Base priority
 * @param context_json Context data
 * @return Optimized priority
 */
notification_priority_t notifications_optimize_priority(notification_type_t type,
                                                       notification_priority_t base_priority,
                                                       const char* context_json);

/**
 * Select optimal delivery channels for notification
 * @param type Notification type
 * @param priority Priority level
 * @param context_json Context data
 * @param channels Array to store selected channels (min 7 elements)
 * @return Number of channels selected
 */
int notifications_select_optimal_channels(notification_type_t type,
                                         notification_priority_t priority,
                                         const char* context_json,
                                         bool* channels);

/**
 * Calculate delivery confidence score
 * @param type Notification type
 * @param priority Priority level
 * @param selected_channels Selected delivery channels
 * @return Confidence score (0.0-1.0)
 */
double notifications_calculate_delivery_confidence(notification_type_t type,
                                                  notification_priority_t priority,
                                                  const bool* selected_channels);

/**
 * Update channel effectiveness based on delivery results
 * @param channel_index Channel index (0=pushover, 1=email, etc.)
 * @param success Whether delivery was successful
 * @param delivery_time_ms Delivery time in milliseconds
 * @param acknowledged Whether notification was acknowledged
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int notifications_update_channel_effectiveness(int channel_index,
                                              bool success,
                                              double delivery_time_ms,
                                              bool acknowledged);

// Utility string conversion functions

/**
 * Convert delivery status to string
 * @param status Delivery status
 * @return Status string
 */
const char* notification_delivery_status_to_string(notification_delivery_status_t status);

/**
 * Convert notification type to string
 * @param type Notification type
 * @return Type string
 */
const char* notification_type_to_string(notification_type_t type);

/**
 * Convert priority to string
 * @param priority Priority level
 * @return Priority string
 */
const char* notification_priority_to_string(notification_priority_t priority);

/**
 * Parse notification type from string
 * @param type_str Type string
 * @return Notification type
 */
notification_type_t notification_parse_type(const char* type_str);

/**
 * Parse priority from string
 * @param priority_str Priority string
 * @return Priority level
 */
notification_priority_t notification_parse_priority(const char* priority_str);

#ifdef __cplusplus
}
#endif

#endif // NOTIFICATIONS_COMPREHENSIVE_H