#ifndef ACKNOWLEDGMENT_TRACKER_H
#define ACKNOWLEDGMENT_TRACKER_H

#include "notification_types.h"
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

// Acknowledgment status
typedef enum {
    ACKNOWLEDGMENT_STATUS_PENDING = 0,
    ACKNOWLEDGMENT_STATUS_ACKNOWLEDGED,
    ACKNOWLEDGMENT_STATUS_EXPIRED,
    ACKNOWLEDGMENT_STATUS_RESOLVED
} acknowledgment_status_t;

// Acknowledgment record
typedef struct {
    char id[64];
    char notification_id[64];
    notification_type_t type;
    char message[512];
    notification_priority_t priority;
    acknowledgment_status_t status;
    time_t created_at;
    time_t acknowledged_at;
    char acknowledged_by[128];
    time_t expires_at;
    time_t resolved_at;
    char context_json[512];
    notification_channel_t channels[8];
    int channel_count;
    bool auto_resolve;
    time_t auto_resolve_time_seconds;
} acknowledgment_t;

// Acknowledgment configuration
typedef struct {
    bool enabled;
    time_t default_expiry_seconds;
    bool auto_resolve_enabled;
    time_t auto_resolve_time_seconds;
    int max_pending_per_type;
    int max_pending_per_priority;
    time_t cleanup_interval_seconds;
    notification_type_t require_acknowledgment_types[16];
    int require_acknowledgment_count;
    int max_acknowledgments;
} acknowledgment_config_t;

// Acknowledgment statistics
typedef struct {
    int total_acknowledgments;
    int pending_count;
    int acknowledged_count;
    int expired_count;
    int resolved_count;
    time_t average_response_time_seconds;
    double acknowledgment_rate;
    time_t last_cleanup;
} acknowledgment_stats_t;

// Acknowledgment tracker status
typedef struct {
    bool enabled;
    int total_acknowledgments;
    int pending_count;
    int acknowledged_count;
    int expired_count;
    int resolved_count;
    int max_acknowledgments;
    time_t last_cleanup;
    double acknowledgment_rate;
} acknowledgment_tracker_status_t;

// Acknowledgment tracker structure
typedef struct {
    acknowledgment_config_t config;
    
    // Acknowledgment storage
    acknowledgment_t* acknowledgments;
    int max_acknowledgments;
    int acknowledgment_count;
    
    // Statistics
    acknowledgment_stats_t stats;
    
    // Thread management
    pthread_t cleanup_thread;
    bool thread_running;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} acknowledgment_tracker_t;

// Initialize acknowledgment tracker
int acknowledgment_tracker_init(const acknowledgment_config_t* config);

// Clean up acknowledgment tracker
void acknowledgment_tracker_cleanup(void);

// Create acknowledgment for notification
int acknowledgment_tracker_create_acknowledgment(const notification_event_t* event,
                                                notification_channel_t channels[],
                                                int channel_count,
                                                char* acknowledgment_id,
                                                size_t max_id_size);

// Acknowledge notification
int acknowledgment_tracker_acknowledge(const char* acknowledgment_id, const char* acknowledged_by);

// Resolve acknowledgment
int acknowledgment_tracker_resolve(const char* acknowledgment_id);

// Get acknowledgment by ID
int acknowledgment_tracker_get_acknowledgment(const char* acknowledgment_id, acknowledgment_t* acknowledgment);

// Get pending acknowledgment for type
int acknowledgment_tracker_get_pending_for_type(notification_type_t type, acknowledgment_t* acknowledgment);

// Check if notification should be sent (not suppressed by pending acknowledgment)
bool acknowledgment_tracker_should_send_notification(const notification_event_t* event);

// Get acknowledgment statistics
void acknowledgment_tracker_get_stats(acknowledgment_stats_t* stats);

// Get acknowledgment tracker status
void acknowledgment_tracker_get_status(acknowledgment_tracker_status_t* status);

// List acknowledgments with filters
int acknowledgment_tracker_list_acknowledgments(acknowledgment_t* acknowledgments,
                                               int max_acknowledgments,
                                               acknowledgment_status_t status_filter,
                                               notification_type_t type_filter);

// Check if acknowledgment tracker is initialized
bool acknowledgment_tracker_is_initialized(void);

// Get acknowledgment tracker instance
acknowledgment_tracker_t* acknowledgment_tracker_get_instance(void);

#endif // ACKNOWLEDGMENT_TRACKER_H
