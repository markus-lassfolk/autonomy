#ifndef NOTIFICATION_EVENTS_H
#define NOTIFICATION_EVENTS_H

#include "notification_types.h"
#include <stdbool.h>
#include <time.h>

// Network member information for events
typedef struct {
    char name[128];
    char interface[64];
    char class[32];
    char ip_address[64];
    bool is_up;
} network_member_t;

// Metrics information for events
typedef struct {
    double latency_ms;
    double loss_percent;
    double jitter_ms;
    double obstruction_pct;
    int rsrp;
    int rsrq;
    double throughput_mbps;
    bool has_latency;
    bool has_loss;
    bool has_jitter;
    bool has_obstruction;
    bool has_rsrp;
    bool has_rsrq;
    bool has_throughput;
} event_metrics_t;

// Event builder structure
typedef struct {
    bool initialized;
} event_builder_t;

// Initialize event builder
int event_builder_init(event_builder_t* builder);

// Clean up event builder
void event_builder_cleanup(event_builder_t* builder);

// Create failover notification event
int event_builder_create_failover_event(event_builder_t* builder,
                                       const network_member_t* from_member,
                                       const network_member_t* to_member,
                                       const char* reason,
                                       const event_metrics_t* metrics,
                                       notification_event_t* event);

// Create failback notification event
int event_builder_create_failback_event(event_builder_t* builder,
                                       const network_member_t* from_member,
                                       const network_member_t* to_member,
                                       const event_metrics_t* metrics,
                                       notification_event_t* event);

// Create member down notification event
int event_builder_create_member_down_event(event_builder_t* builder,
                                          const network_member_t* member,
                                          const char* reason,
                                          const event_metrics_t* metrics,
                                          notification_event_t* event);

// Create member up notification event
int event_builder_create_member_up_event(event_builder_t* builder,
                                        const network_member_t* member,
                                        const event_metrics_t* metrics,
                                        notification_event_t* event);

// Create predictive warning notification event
int event_builder_create_predictive_event(event_builder_t* builder,
                                         const network_member_t* member,
                                         const char* prediction,
                                         double confidence,
                                         const event_metrics_t* metrics,
                                         notification_event_t* event);

// Create critical error notification event
int event_builder_create_critical_error_event(event_builder_t* builder,
                                             const char* component,
                                             const char* error_message,
                                             const char* details_json,
                                             notification_event_t* event);

// Create recovery notification event
int event_builder_create_recovery_event(event_builder_t* builder,
                                       const char* component,
                                       const char* details_json,
                                       notification_event_t* event);

// Create status update notification event
int event_builder_create_status_update_event(event_builder_t* builder,
                                            const char* summary,
                                            const char* stats_json,
                                            notification_event_t* event);

// Create summary notification event
int event_builder_create_summary_event(event_builder_t* builder,
                                      const char* period,
                                      const char* stats_json,
                                      notification_event_t* event);

// Create test notification event
int event_builder_create_test_event(event_builder_t* builder,
                                   notification_event_t* event);

// Create custom notification event
int event_builder_create_custom_event(event_builder_t* builder,
                                     notification_type_t type,
                                     const char* title,
                                     const char* message,
                                     notification_priority_t priority,
                                     notification_event_t* event);

// Helper functions for formatting

// Get emoji for member class
const char* event_builder_get_member_emoji(const char* member_class);

// Format metrics as string
void event_builder_format_metrics(const event_metrics_t* metrics, const char* member_class,
                                 char* formatted, size_t max_size);

// Format failover reason with emoji
void event_builder_format_failover_reason(const char* reason, char* formatted_reason,
                                         char* reason_emoji, size_t max_size);

#endif // NOTIFICATION_EVENTS_H
