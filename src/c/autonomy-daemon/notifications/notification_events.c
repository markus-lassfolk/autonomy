#include "notification_events.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <strings.h>

// Initialize event builder
int event_builder_init(event_builder_t* builder) {
    if (!builder) {
        return -1;
    }
    
    builder->initialized = true;
    return 0;
}

// Clean up event builder
void event_builder_cleanup(event_builder_t* builder) {
    if (!builder) return;
    
    builder->initialized = false;
}

// Get emoji for member class
const char* event_builder_get_member_emoji(const char* member_class) {
    if (!member_class) return "⚠️";
    
    if (strcasecmp(member_class, "starlink") == 0) {
        return "🛰️";
    } else if (strcasecmp(member_class, "cellular") == 0) {
        return "📱";
    } else if (strcasecmp(member_class, "wifi") == 0) {
        return "📶";
    } else if (strcasecmp(member_class, "lan") == 0) {
        return "🌐";
    } else {
        return "⚠️";
    }
}

// Format failover reason with emoji
void event_builder_format_failover_reason(const char* reason, char* formatted_reason,
                                         char* reason_emoji, size_t max_size) {
    if (!reason || !formatted_reason || !reason_emoji) return;
    
    if (strcasecmp(reason, "predictive") == 0) {
        strncpy(reason_emoji, "🔮", max_size - 1);
        strncpy(formatted_reason, "Predictive failover triggered", max_size - 1);
    } else if (strcasecmp(reason, "quality") == 0) {
        strncpy(reason_emoji, "📶", max_size - 1);
        strncpy(formatted_reason, "Signal quality degraded", max_size - 1);
    } else if (strcasecmp(reason, "latency") == 0) {
        strncpy(reason_emoji, "🐌", max_size - 1);
        strncpy(formatted_reason, "High latency detected", max_size - 1);
    } else if (strcasecmp(reason, "loss") == 0) {
        strncpy(reason_emoji, "📉", max_size - 1);
        strncpy(formatted_reason, "Packet loss detected", max_size - 1);
    } else if (strcasecmp(reason, "manual") == 0) {
        strncpy(reason_emoji, "👤", max_size - 1);
        strncpy(formatted_reason, "Manual failover requested", max_size - 1);
    } else {
        strncpy(reason_emoji, "🔄", max_size - 1);
        strncpy(formatted_reason, "Failover triggered", max_size - 1);
    }
}

// Format metrics as string
void event_builder_format_metrics(const event_metrics_t* metrics, const char* member_class,
                                 char* formatted, size_t max_size) {
    if (!metrics || !formatted) return;
    
    formatted[0] = '\0';
    
    if (metrics->has_latency) {
        char latency_icon[8];
        if (metrics->latency_ms > 500) {
            strncpy(latency_icon, "🔴", sizeof(latency_icon) - 1);
        } else if (metrics->latency_ms > 200) {
            strncpy(latency_icon, "🟡", sizeof(latency_icon) - 1);
        } else {
            strncpy(latency_icon, "🟢", sizeof(latency_icon) - 1);
        }
        
        char temp[128];
        snprintf(temp, sizeof(temp), "%s Latency: %.1f ms\n", latency_icon, metrics->latency_ms);
        strncat(formatted, temp, max_size - strlen(formatted) - 1);
    }
    
    if (metrics->has_loss) {
        char loss_icon[8];
        if (metrics->loss_percent > 5) {
            strncpy(loss_icon, "🔴", sizeof(loss_icon) - 1);
        } else if (metrics->loss_percent > 1) {
            strncpy(loss_icon, "🟡", sizeof(loss_icon) - 1);
        } else {
            strncpy(loss_icon, "🟢", sizeof(loss_icon) - 1);
        }
        
        char temp[128];
        snprintf(temp, sizeof(temp), "%s Loss: %.1f%%\n", loss_icon, metrics->loss_percent);
        strncat(formatted, temp, max_size - strlen(formatted) - 1);
    }
    
    if (metrics->has_jitter) {
        char temp[128];
        snprintf(temp, sizeof(temp), "📈 Jitter: %.1f ms\n", metrics->jitter_ms);
        strncat(formatted, temp, max_size - strlen(formatted) - 1);
    }
    
    if (metrics->has_obstruction && member_class && strcasecmp(member_class, "starlink") == 0) {
        char obstruction_icon[8];
        if (metrics->obstruction_pct > 15) {
            strncpy(obstruction_icon, "🔴", sizeof(obstruction_icon) - 1);
        } else if (metrics->obstruction_pct > 5) {
            strncpy(obstruction_icon, "🟡", sizeof(obstruction_icon) - 1);
        } else {
            strncpy(obstruction_icon, "🟢", sizeof(obstruction_icon) - 1);
        }
        
        char temp[128];
        snprintf(temp, sizeof(temp), "%s Obstruction: %.1f%%\n", obstruction_icon, metrics->obstruction_pct);
        strncat(formatted, temp, max_size - strlen(formatted) - 1);
    }
    
    if (metrics->has_rsrp && member_class && strcasecmp(member_class, "cellular") == 0) {
        char temp[128];
        snprintf(temp, sizeof(temp), "• RSRP: %d dBm\n", metrics->rsrp);
        strncat(formatted, temp, max_size - strlen(formatted) - 1);
    }
    
    if (metrics->has_rsrq && member_class && strcasecmp(member_class, "cellular") == 0) {
        char temp[128];
        snprintf(temp, sizeof(temp), "• RSRQ: %d dB\n", metrics->rsrq);
        strncat(formatted, temp, max_size - strlen(formatted) - 1);
    }
}

// Create failover notification event
int event_builder_create_failover_event(event_builder_t* builder,
                                       const network_member_t* from_member,
                                       const network_member_t* to_member,
                                       const char* reason,
                                       const event_metrics_t* metrics,
                                       notification_event_t* event) {
    if (!builder || !builder->initialized || !event) {
        return -1;
    }
    
    memset(event, 0, sizeof(notification_event_t));
    
    // Generate ID
    time_t now = time(NULL);
    snprintf(event->id, sizeof(event->id), "failover_%ld", now);
    
    // Format reason
    char reason_emoji[16];
    char formatted_reason[256];
    event_builder_format_failover_reason(reason ? reason : "unknown", formatted_reason, reason_emoji, sizeof(formatted_reason));
    
    // Create title
    snprintf(event->title, sizeof(event->title), "%s Network Failover", reason_emoji);
    
    // Create message
    char message_buffer[2048];
    snprintf(message_buffer, sizeof(message_buffer), "%s\n\n", formatted_reason);
    
    if (from_member) {
        char temp[256];
        snprintf(temp, sizeof(temp), "From: %s (%s)\n", from_member->name, from_member->class);
        strncat(message_buffer, temp, sizeof(message_buffer) - strlen(message_buffer) - 1);
    }
    
    if (to_member) {
        char temp[256];
        snprintf(temp, sizeof(temp), "To: %s (%s)\n", to_member->name, to_member->class);
        strncat(message_buffer, temp, sizeof(message_buffer) - strlen(message_buffer) - 1);
    }
    
    if (metrics) {
        strncat(message_buffer, "\n📊 Current Metrics:\n", sizeof(message_buffer) - strlen(message_buffer) - 1);
        char metrics_str[512];
        event_builder_format_metrics(metrics, to_member ? to_member->class : NULL, metrics_str, sizeof(metrics_str));
        strncat(message_buffer, metrics_str, sizeof(message_buffer) - strlen(message_buffer) - 1);
    }
    
    strncpy(event->message, message_buffer, sizeof(event->message) - 1);
    
    // Set other fields
    event->type = NOTIFICATION_TYPE_FAILOVER;
    event->priority = NOTIFICATION_PRIORITY_HIGH;
    event->timestamp = now;
    
    // Add details JSON
    snprintf(event->details_json, sizeof(event->details_json),
             "{\"reason\":\"%s\",\"from_member\":\"%s\",\"to_member\":\"%s\"}",
             reason ? reason : "unknown",
             from_member ? from_member->name : "",
             to_member ? to_member->name : "");
    
    return 0;
}

// Create failback notification event
int event_builder_create_failback_event(event_builder_t* builder,
                                       const network_member_t* from_member,
                                       const network_member_t* to_member,
                                       const event_metrics_t* metrics,
                                       notification_event_t* event) {
    if (!builder || !builder->initialized || !event) {
        return -1;
    }
    
    memset(event, 0, sizeof(notification_event_t));
    
    // Generate ID
    time_t now = time(NULL);
    snprintf(event->id, sizeof(event->id), "failback_%ld", now);
    
    // Create title
    strncpy(event->title, "✅ Network Restored", sizeof(event->title) - 1);
    
    // Create message
    char message_buffer[2048];
    strncpy(message_buffer, "Primary connection restored\n\n", sizeof(message_buffer) - 1);
    
    if (from_member) {
        char temp[256];
        snprintf(temp, sizeof(temp), "From: %s (%s)\n", from_member->name, from_member->class);
        strncat(message_buffer, temp, sizeof(message_buffer) - strlen(message_buffer) - 1);
    }
    
    if (to_member) {
        char temp[256];
        snprintf(temp, sizeof(temp), "To: %s (%s)\n", to_member->name, to_member->class);
        strncat(message_buffer, temp, sizeof(message_buffer) - strlen(message_buffer) - 1);
    }
    
    if (metrics) {
        strncat(message_buffer, "\nRestored Connection Quality:\n", sizeof(message_buffer) - strlen(message_buffer) - 1);
        char metrics_str[512];
        event_builder_format_metrics(metrics, to_member ? to_member->class : NULL, metrics_str, sizeof(metrics_str));
        strncat(message_buffer, metrics_str, sizeof(message_buffer) - strlen(message_buffer) - 1);
    }
    
    strncpy(event->message, message_buffer, sizeof(event->message) - 1);
    
    // Set other fields
    event->type = NOTIFICATION_TYPE_CRITICAL_ERROR;
    event->priority = NOTIFICATION_PRIORITY_NORMAL;
    event->timestamp = now;
    
    // Add details JSON
    snprintf(event->details_json, sizeof(event->details_json),
             "{\"event_type\":\"failback\",\"from_member\":\"%s\",\"to_member\":\"%s\"}",
             from_member ? from_member->name : "",
             to_member ? to_member->name : "");
    
    return 0;
}

// Create member down notification event
int event_builder_create_member_down_event(event_builder_t* builder,
                                          const network_member_t* member,
                                          const char* reason,
                                          const event_metrics_t* metrics,
                                          notification_event_t* event) {
    if (!builder || !builder->initialized || !member || !event) {
        return -1;
    }
    
    memset(event, 0, sizeof(notification_event_t));
    
    // Generate ID
    time_t now = time(NULL);
    snprintf(event->id, sizeof(event->id), "member_down_%s_%ld", member->name, now);
    
    // Get emoji for member class
    const char* emoji = event_builder_get_member_emoji(member->class);
    
    // Create title
    char class_title[64];
    strncpy(class_title, member->class, sizeof(class_title) - 1);
    class_title[0] = toupper(class_title[0]); // Capitalize first letter
    
    snprintf(event->title, sizeof(event->title), "%s %s Connection Down", emoji, class_title);
    
    // Create message
    char message_buffer[2048];
    snprintf(message_buffer, sizeof(message_buffer), "%s connection has failed\n\n", class_title);
    
    char temp[256];
    snprintf(temp, sizeof(temp), "Member: %s\n", member->name);
    strncat(message_buffer, temp, sizeof(message_buffer) - strlen(message_buffer) - 1);
    
    snprintf(temp, sizeof(temp), "Interface: %s\n", member->interface);
    strncat(message_buffer, temp, sizeof(message_buffer) - strlen(message_buffer) - 1);
    
    if (reason && strlen(reason) > 0) {
        snprintf(temp, sizeof(temp), "Reason: %s\n", reason);
        strncat(message_buffer, temp, sizeof(message_buffer) - strlen(message_buffer) - 1);
    }
    
    if (metrics) {
        strncat(message_buffer, "\nLast Known Metrics:\n", sizeof(message_buffer) - strlen(message_buffer) - 1);
        char metrics_str[512];
        event_builder_format_metrics(metrics, member->class, metrics_str, sizeof(metrics_str));
        strncat(message_buffer, metrics_str, sizeof(message_buffer) - strlen(message_buffer) - 1);
    }
    
    strncpy(event->message, message_buffer, sizeof(event->message) - 1);
    
    // Set other fields
    event->type = NOTIFICATION_TYPE_CRITICAL_ERROR;
    event->priority = NOTIFICATION_PRIORITY_HIGH;
    event->timestamp = now;
    strncpy(event->member_name, member->name, sizeof(event->member_name) - 1);
    
    // Add details JSON
    snprintf(event->details_json, sizeof(event->details_json),
             "{\"event_type\":\"member_down\",\"member\":\"%s\",\"interface\":\"%s\",\"class\":\"%s\",\"reason\":\"%s\"}",
             member->name, member->interface, member->class, reason ? reason : "");
    
    return 0;
}

// Create member up notification event
int event_builder_create_member_up_event(event_builder_t* builder,
                                        const network_member_t* member,
                                        const event_metrics_t* metrics,
                                        notification_event_t* event) {
    if (!builder || !builder->initialized || !member || !event) {
        return -1;
    }
    
    memset(event, 0, sizeof(notification_event_t));
    
    // Generate ID
    time_t now = time(NULL);
    snprintf(event->id, sizeof(event->id), "member_up_%s_%ld", member->name, now);
    
    // Get emoji for member class
    const char* emoji = event_builder_get_member_emoji(member->class);
    
    // Create title
    char class_title[64];
    strncpy(class_title, member->class, sizeof(class_title) - 1);
    class_title[0] = toupper(class_title[0]); // Capitalize first letter
    
    snprintf(event->title, sizeof(event->title), "%s %s Connection Restored", emoji, class_title);
    
    // Create message
    char message_buffer[2048];
    snprintf(message_buffer, sizeof(message_buffer), "%s connection is back online\n\n", class_title);
    
    char temp[256];
    snprintf(temp, sizeof(temp), "Member: %s\n", member->name);
    strncat(message_buffer, temp, sizeof(message_buffer) - strlen(message_buffer) - 1);
    
    snprintf(temp, sizeof(temp), "Interface: %s\n", member->interface);
    strncat(message_buffer, temp, sizeof(message_buffer) - strlen(message_buffer) - 1);
    
    if (metrics) {
        strncat(message_buffer, "\nCurrent Quality:\n", sizeof(message_buffer) - strlen(message_buffer) - 1);
        char metrics_str[512];
        event_builder_format_metrics(metrics, member->class, metrics_str, sizeof(metrics_str));
        strncat(message_buffer, metrics_str, sizeof(message_buffer) - strlen(message_buffer) - 1);
    }
    
    strncpy(event->message, message_buffer, sizeof(event->message) - 1);
    
    // Set other fields
    event->type = NOTIFICATION_TYPE_CRITICAL_ERROR;
    event->priority = NOTIFICATION_PRIORITY_NORMAL;
    event->timestamp = now;
    strncpy(event->member_name, member->name, sizeof(event->member_name) - 1);
    
    // Add details JSON
    snprintf(event->details_json, sizeof(event->details_json),
             "{\"event_type\":\"member_up\",\"member\":\"%s\",\"interface\":\"%s\",\"class\":\"%s\"}",
             member->name, member->interface, member->class);
    
    return 0;
}

// Create predictive warning notification event
int event_builder_create_predictive_event(event_builder_t* builder,
                                         const network_member_t* member,
                                         const char* prediction,
                                         double confidence,
                                         const event_metrics_t* metrics,
                                         notification_event_t* event) {
    if (!builder || !builder->initialized || !member || !prediction || !event) {
        return -1;
    }
    
    memset(event, 0, sizeof(notification_event_t));
    
    // Generate ID
    time_t now = time(NULL);
    snprintf(event->id, sizeof(event->id), "predictive_%s_%ld", member->name, now);
    
    // Create title
    strncpy(event->title, "🔮 Predictive Warning", sizeof(event->title) - 1);
    
    // Create message
    char message_buffer[2048];
    snprintf(message_buffer, sizeof(message_buffer), "Potential issue predicted for %s\n\n", member->name);
    
    char temp[256];
    snprintf(temp, sizeof(temp), "Prediction: %s\n", prediction);
    strncat(message_buffer, temp, sizeof(message_buffer) - strlen(message_buffer) - 1);
    
    snprintf(temp, sizeof(temp), "Confidence: %.1f%%\n", confidence * 100.0);
    strncat(message_buffer, temp, sizeof(message_buffer) - strlen(message_buffer) - 1);
    
    snprintf(temp, sizeof(temp), "Member: %s (%s)\n", member->name, member->class);
    strncat(message_buffer, temp, sizeof(message_buffer) - strlen(message_buffer) - 1);
    
    if (metrics) {
        strncat(message_buffer, "\nCurrent Metrics:\n", sizeof(message_buffer) - strlen(message_buffer) - 1);
        char metrics_str[512];
        event_builder_format_metrics(metrics, member->class, metrics_str, sizeof(metrics_str));
        strncat(message_buffer, metrics_str, sizeof(message_buffer) - strlen(message_buffer) - 1);
    }
    
    strncat(message_buffer, "\nRecommendation: Monitor connection closely", sizeof(message_buffer) - strlen(message_buffer) - 1);
    
    strncpy(event->message, message_buffer, sizeof(event->message) - 1);
    
    // Set other fields
    event->type = NOTIFICATION_TYPE_SYSTEM_HEALTH;
    event->priority = NOTIFICATION_PRIORITY_NORMAL;
    event->timestamp = now;
    strncpy(event->member_name, member->name, sizeof(event->member_name) - 1);
    
    // Add details JSON
    snprintf(event->details_json, sizeof(event->details_json),
             "{\"event_type\":\"predictive\",\"member\":\"%s\",\"prediction\":\"%s\",\"confidence\":%.3f}",
             member->name, prediction, confidence);
    
    return 0;
}

// Create critical error notification event
int event_builder_create_critical_error_event(event_builder_t* builder,
                                             const char* component,
                                             const char* error_message,
                                             const char* details_json,
                                             notification_event_t* event) {
    if (!builder || !builder->initialized || !component || !error_message || !event) {
        return -1;
    }
    
    memset(event, 0, sizeof(notification_event_t));
    
    // Generate ID
    time_t now = time(NULL);
    snprintf(event->id, sizeof(event->id), "critical_%ld", now);
    
    // Create title
    strncpy(event->title, "🚨 CRITICAL: System Error", sizeof(event->title) - 1);
    
    // Create message
    char message_buffer[2048];
    snprintf(message_buffer, sizeof(message_buffer), "Critical error in %s\n\n", component);
    
    char temp[512];
    snprintf(temp, sizeof(temp), "Error: %s\n", error_message);
    strncat(message_buffer, temp, sizeof(message_buffer) - strlen(message_buffer) - 1);
    
    if (details_json && strlen(details_json) > 0) {
        strncat(message_buffer, "\nDetails: See context data\n", sizeof(message_buffer) - strlen(message_buffer) - 1);
    }
    
    strncat(message_buffer, "\nImmediate attention required!", sizeof(message_buffer) - strlen(message_buffer) - 1);
    
    strncpy(event->message, message_buffer, sizeof(event->message) - 1);
    
    // Set other fields
    event->type = NOTIFICATION_TYPE_SYSTEM_HEALTH;
    event->priority = NOTIFICATION_PRIORITY_EMERGENCY;
    event->timestamp = now;
    
    // Add details JSON
    if (details_json && strlen(details_json) > 0) {
        strncpy(event->details_json, details_json, sizeof(event->details_json) - 1);
    } else {
        snprintf(event->details_json, sizeof(event->details_json),
                 "{\"event_type\":\"critical_error\",\"component\":\"%s\",\"error\":\"%s\"}",
                 component, error_message);
    }
    
    return 0;
}

// Create test notification event
int event_builder_create_test_event(event_builder_t* builder,
                                   notification_event_t* event) {
    if (!builder || !builder->initialized || !event) {
        return -1;
    }
    
    memset(event, 0, sizeof(notification_event_t));
    
    // Generate ID
    time_t now = time(NULL);
    snprintf(event->id, sizeof(event->id), "test_%ld", now);
    
    // Create test notification
    strncpy(event->title, "🧪 Test Notification", sizeof(event->title) - 1);
    strncpy(event->message, "This is a test notification from autonomy.\n\nIf you receive this, notifications are working correctly!", sizeof(event->message) - 1);
    
    event->type = NOTIFICATION_TYPE_STATUS_UPDATE;
    event->priority = NOTIFICATION_PRIORITY_NORMAL;
    event->timestamp = now;
    
    // Add details JSON
    strncpy(event->details_json, "{\"event_type\":\"test\",\"test\":true}", sizeof(event->details_json) - 1);
    
    return 0;
}

// Create custom notification event
int event_builder_create_custom_event(event_builder_t* builder,
                                     notification_type_t type,
                                     const char* title,
                                     const char* message,
                                     notification_priority_t priority,
                                     notification_event_t* event) {
    if (!builder || !builder->initialized || !title || !message || !event) {
        return -1;
    }
    
    memset(event, 0, sizeof(notification_event_t));
    
    // Generate ID
    time_t now = time(NULL);
    snprintf(event->id, sizeof(event->id), "custom_%ld", now);
    
    // Set fields
    strncpy(event->title, title, sizeof(event->title) - 1);
    strncpy(event->message, message, sizeof(event->message) - 1);
    event->type = type;
    event->priority = priority;
    event->timestamp = now;
    
    // Add details JSON
    snprintf(event->details_json, sizeof(event->details_json),
             "{\"event_type\":\"custom\",\"type\":\"%s\",\"priority\":%d}",
             notification_type_to_string(type), (int)priority);
    
    return 0;
}
