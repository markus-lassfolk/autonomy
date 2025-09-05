#ifndef ACKNOWLEDGMENT_TRACKER_H
#define ACKNOWLEDGMENT_TRACKER_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include "notification_types.h"

// Acknowledgment tracking system

// Acknowledgment structure
typedef struct {
    char notification_id[64];
    char acknowledgment_id[64];
    time_t acknowledged_time;
    char acknowledged_by[128];
    char acknowledgment_message[512];
    bool is_automatic;
} acknowledgment_t;

// Tracker configuration
typedef struct {
    bool enable_auto_acknowledgment;
    bool enable_manual_acknowledgment;
    int acknowledgment_timeout_seconds;
    int max_pending_acknowledgments;
    bool enable_escalation;
} acknowledgment_config_t;

// Function declarations
int acknowledgment_tracker_init(const acknowledgment_config_t *config);
void acknowledgment_tracker_cleanup(void);
int acknowledgment_tracker_register_notification(const char *notification_id);
int acknowledgment_tracker_acknowledge(const char *notification_id, const char *acknowledged_by, const char *message);
int acknowledgment_tracker_get_pending(char **notification_ids, int max_ids);
int acknowledgment_tracker_check_timeout(const char *notification_id, bool *is_timeout);
int acknowledgment_tracker_get_acknowledgment(const char *notification_id, acknowledgment_t *ack);

#endif // ACKNOWLEDGMENT_TRACKER_H
