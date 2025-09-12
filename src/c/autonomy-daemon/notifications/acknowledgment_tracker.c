#include "acknowledgment_tracker.h"
#include <stdlib.h>
#include "../shared/utils/string_utils.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>

// Global acknowledgment tracker instance
static acknowledgment_tracker_t g_acknowledgment_tracker;
static bool g_acknowledgment_tracker_initialized = false;

// Forward declarations
static void* cleanup_thread(void* arg\n"\n"\n"\n"\n"\n"\n"\n");
static void cleanup_expired_acknowledgments(void\n"\n"\n"\n"\n"\n"\n"\n");
static bool is_acknowledgment_required(notification_type_t type\n"\n"\n"\n"\n"\n"\n"\n");
static int check_pending_limits(notification_type_t type, notification_priority_t priority\n"\n"\n"\n"\n"\n"\n"\n");
static void generate_acknowledgment_id(const notification_event_t* event, char* id, size_t max_size\n"\n"\n"\n"\n"\n"\n"\n");

// Initialize acknowledgment tracker
int acknowledgment_tracker_init(const acknowledgment_config_t* config) {
    if (g_acknowledgment_tracker_initialized) {
        return 0; // Already initialized
    }
    
    if (!config) {
        return -1;
    }
    
    memset(&g_acknowledgment_tracker, 0, sizeof(acknowledgment_tracker_t)\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Copy configuration
    g_acknowledgment_tracker.config = *config;
    
    // Initialize mutex
    g_acknowledgment_tracker.mutex = malloc(sizeof(pthread_mutex_t)\n"\n"\n"\n"\n"\n"\n"\n");
    if (!g_acknowledgment_tracker.mutex) {
        return -1;
    }
    
    pthread_mutex_init(g_acknowledgment_tracker.mutex, NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Initialize acknowledgment storage
    g_acknowledgment_tracker.acknowledgments = malloc(config->max_acknowledgments * sizeof(acknowledgment_t)\n"\n"\n"\n"\n"\n"\n"\n");
    if (!g_acknowledgment_tracker.acknowledgments) {
        pthread_mutex_destroy(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
        free(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
        return -1;
    }
    
    g_acknowledgment_tracker.max_acknowledgments = config->max_acknowledgments;
    g_acknowledgment_tracker.acknowledgment_count = 0;
    
    // Initialize statistics
    memset(&g_acknowledgment_tracker.stats, 0, sizeof(acknowledgment_stats_t)\n"\n"\n"\n"\n"\n"\n"\n");
    g_acknowledgment_tracker.stats.last_cleanup = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Start cleanup thread if enabled
    if (config->enabled) {
        g_acknowledgment_tracker.thread_running = true;
        if (pthread_create(&g_acknowledgment_tracker.cleanup_thread, NULL, cleanup_thread, NULL) != 0) {
            free(g_acknowledgment_tracker.acknowledgments\n"\n"\n"\n"\n"\n"\n"\n");
            pthread_mutex_destroy(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
            free(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
            return -1;
        }
    }
    
    g_acknowledgment_tracker_initialized = true;
    return 0;
}

// Clean up acknowledgment tracker
void acknowledgment_tracker_cleanup(void) {
    if (!g_acknowledgment_tracker_initialized) return;
    
    // Stop cleanup thread
    if (g_acknowledgment_tracker.config.enabled) {
        g_acknowledgment_tracker.thread_running = false;
        pthread_join(g_acknowledgment_tracker.cleanup_thread, NULL\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (g_acknowledgment_tracker.mutex) {
        pthread_mutex_destroy(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
        free(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (g_acknowledgment_tracker.acknowledgments) {
        free(g_acknowledgment_tracker.acknowledgments\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    g_acknowledgment_tracker.acknowledgments = NULL;
    g_acknowledgment_tracker.mutex = NULL;
    g_acknowledgment_tracker.acknowledgment_count = 0;
    g_acknowledgment_tracker.max_acknowledgments = 0; // Use configurable max acknowledgments
    
    g_acknowledgment_tracker_initialized = false;
}

// Cleanup thread
static void* cleanup_thread(void* arg) {
    (void)arg; // Unused parameter
    
    while (g_acknowledgment_tracker.thread_running) {
        cleanup_expired_acknowledgments(\n"\n"\n"\n"\n"\n"\n"\n");
        sleep((unsigned int)g_acknowledgment_tracker.config.cleanup_interval_seconds\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    return NULL;
}

// Cleanup expired acknowledgments
static void cleanup_expired_acknowledgments(void) {
    if (!g_acknowledgment_tracker_initialized) return;
    
    pthread_mutex_lock(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    time_t now = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    int expired_count = 0;
    
    for (int i = 0; i < g_acknowledgment_tracker.acknowledgment_count; i++) {
        acknowledgment_t* ack = &g_acknowledgment_tracker.acknowledgments[i];
        
        if (ack->expires_at < now && ack->status == ACKNOWLEDGMENT_STATUS_PENDING) {
            ack->status = ACKNOWLEDGMENT_STATUS_EXPIRED;
            expired_count++;
            
            printf("ACKNOWLEDGMENT: Acknowledgment %s expired\n", ack->id\n"\n"\n"\n"\n"\n"\n"\n");
        }
    }
    
    g_acknowledgment_tracker.stats.last_cleanup = now;
    g_acknowledgment_tracker.stats.expired_count += expired_count;
    
    pthread_mutex_unlock(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
}

// Check if acknowledgment is required for notification type
static bool is_acknowledgment_required(notification_type_t type) {
    for (int i = 0; i < g_acknowledgment_tracker.config.require_acknowledgment_count; i++) {
        if (g_acknowledgment_tracker.config.require_acknowledgment_types[i] == type) {
            return true;
        }
    }
    return false;
}

// Check pending limits
static int check_pending_limits(notification_type_t type, notification_priority_t priority) {
    int type_pending_count = 0;
    int priority_pending_count = 0;
    
    for (int i = 0; i < g_acknowledgment_tracker.acknowledgment_count; i++) {
        acknowledgment_t* ack = &g_acknowledgment_tracker.acknowledgments[i];
        
        if (ack->status == ACKNOWLEDGMENT_STATUS_PENDING) {
            if (ack->type == type) {
                type_pending_count++;
            }
            if (ack->priority == priority) {
                priority_pending_count++;
            }
        }
    }
    
    if (type_pending_count >= g_acknowledgment_tracker.config.max_pending_per_type) {
        return -1; // Type limit exceeded
    }
    
    if (priority_pending_count >= g_acknowledgment_tracker.config.max_pending_per_priority) {
        return -2; // Priority limit exceeded
    }
    
    return 0;
}

// Generate acknowledgment ID
static void generate_acknowledgment_id(const notification_event_t* event, char* id, size_t max_size) {
    time_t now = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    snprintf(id, max_size, "ack_%s_%lld", notification_type_to_string(event->type), now\n"\n"\n"\n"\n"\n"\n"\n");
}

// Create acknowledgment for notification
int acknowledgment_tracker_create_acknowledgment(const notification_event_t* event,
                                                notification_channel_t channels[],
                                                int channel_count,
                                                char* acknowledgment_id,
                                                size_t max_id_size) {
    if (!g_acknowledgment_tracker_initialized || !event || !acknowledgment_id) {
        return -1;
    }
    
    if (!g_acknowledgment_tracker.config.enabled) {
        return 0; // Tracking disabled
    }
    
    if (!is_acknowledgment_required(event->type)) {
        return 0; // No acknowledgment required
    }
    
    pthread_mutex_lock(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Check limits
    if (check_pending_limits(event->type, event->priority) != 0) {
        pthread_mutex_unlock(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
        return -1; // Limits exceeded
    }
    
    // Check if we have space
    if (g_acknowledgment_tracker.acknowledgment_count >= g_acknowledgment_tracker.max_acknowledgments) {
        pthread_mutex_unlock(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
        return -1; // No space
    }
    
    // Create acknowledgment
    int index = g_acknowledgment_tracker.acknowledgment_count;
    acknowledgment_t* ack = &g_acknowledgment_tracker.acknowledgments[index];
    
    generate_acknowledgment_id(event, ack->id, sizeof(ack->id)\n"\n"\n"\n"\n"\n"\n"\n");
    safe_strncpy(ack->notification_id, event->id, sizeof(ack->notification_id)\n"\n"\n"\n"\n"\n"\n"\n");
    ack->type = event->type;
    safe_strncpy(ack->message, event->message, sizeof(ack->message)\n"\n"\n"\n"\n"\n"\n"\n");
    ack->priority = event->priority;
    ack->status = ACKNOWLEDGMENT_STATUS_PENDING;
    ack->created_at = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    ack->acknowledged_at = 0;
    ack->acknowledged_by[0] = '\0';
    ack->expires_at = ack->created_at + g_acknowledgment_tracker.config.default_expiry_seconds;
    ack->resolved_at = 0;
    
    if (strlen(event->details_json) > 0) {
        safe_strncpy(ack->context_json, event->details_json, sizeof(ack->context_json)\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        ack->context_json[0] = '\0';
    }
    
    // Copy channels
    ack->channel_count = (channel_count < 8) ? channel_count : 8;
    for (int i = 0; i < ack->channel_count; i++) {
        ack->channels[i] = channels[i];
    }
    
    ack->auto_resolve = g_acknowledgment_tracker.config.auto_resolve_enabled;
    ack->auto_resolve_time_seconds = g_acknowledgment_tracker.config.auto_resolve_time_seconds;
    
    g_acknowledgment_tracker.acknowledgment_count++;
    g_acknowledgment_tracker.stats.total_acknowledgments++;
    g_acknowledgment_tracker.stats.pending_count++;
    
    // Copy ID to output
    strncpy(acknowledgment_id, ack->id, max_id_size - 1\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("ACKNOWLEDGMENT: Created acknowledgment %s for notification %s\n", ack->id, event->id\n"\n"\n"\n"\n"\n"\n"\n");
    
    pthread_mutex_unlock(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    return 0;
}

// Acknowledge notification
int acknowledgment_tracker_acknowledge(const char* acknowledgment_id, const char* acknowledged_by) {
    if (!g_acknowledgment_tracker_initialized || !acknowledgment_id || !acknowledged_by) {
        return -1;
    }
    
    if (!g_acknowledgment_tracker.config.enabled) {
        return -1; // Tracking disabled
    }
    
    pthread_mutex_lock(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Find acknowledgment
    acknowledgment_t* ack = NULL;
    for (int i = 0; i < g_acknowledgment_tracker.acknowledgment_count; i++) {
        if (strcmp(g_acknowledgment_tracker.acknowledgments[i].id, acknowledgment_id) == 0) {
            ack = &g_acknowledgment_tracker.acknowledgments[i];
            break;
        }
    }
    
    if (!ack) {
        pthread_mutex_unlock(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
        return -1; // Not found
    }
    
    if (ack->status != ACKNOWLEDGMENT_STATUS_PENDING) {
        pthread_mutex_unlock(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
        return -1; // Not pending
    }
    
    // Mark as acknowledged
    time_t now = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    ack->status = ACKNOWLEDGMENT_STATUS_ACKNOWLEDGED;
    ack->acknowledged_at = now;
    safe_strncpy(ack->acknowledged_by, acknowledged_by, sizeof(ack->acknowledged_by)\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Update statistics
    g_acknowledgment_tracker.stats.pending_count--;
    g_acknowledgment_tracker.stats.acknowledged_count++;
    
    // Update average response time
    time_t response_time = now - ack->created_at;
    if (g_acknowledgment_tracker.stats.acknowledged_count == 1) {
        g_acknowledgment_tracker.stats.average_response_time_seconds = response_time;
    } else {
        // Exponential moving average
        g_acknowledgment_tracker.stats.average_response_time_seconds = 
            (time_t)(g_acknowledgment_tracker.stats.average_response_time_seconds * 0.9 + response_time * 0.1\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Update acknowledgment rate
    g_acknowledgment_tracker.stats.acknowledgment_rate = 
        (double)g_acknowledgment_tracker.stats.acknowledged_count / g_acknowledgment_tracker.stats.total_acknowledgments;
    
    printf("ACKNOWLEDGMENT: Acknowledgment %s acknowledged by %s\n", acknowledgment_id, acknowledged_by\n"\n"\n"\n"\n"\n"\n"\n");
    
    pthread_mutex_unlock(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    return 0;
}

// Resolve acknowledgment
int acknowledgment_tracker_resolve(const char* acknowledgment_id) {
    if (!g_acknowledgment_tracker_initialized || !acknowledgment_id) {
        return -1;
    }
    
    if (!g_acknowledgment_tracker.config.enabled) {
        return -1; // Tracking disabled
    }
    
    pthread_mutex_lock(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Find acknowledgment
    acknowledgment_t* ack = NULL;
    for (int i = 0; i < g_acknowledgment_tracker.acknowledgment_count; i++) {
        if (strcmp(g_acknowledgment_tracker.acknowledgments[i].id, acknowledgment_id) == 0) {
            ack = &g_acknowledgment_tracker.acknowledgments[i];
            break;
        }
    }
    
    if (!ack) {
        pthread_mutex_unlock(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
        return -1; // Not found
    }
    
    if (ack->status == ACKNOWLEDGMENT_STATUS_RESOLVED) {
        pthread_mutex_unlock(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
        return 0; // Already resolved
    }
    
    // Mark as resolved
    acknowledgment_status_t old_status = ack->status;
    ack->status = ACKNOWLEDGMENT_STATUS_RESOLVED;
    ack->resolved_at = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Update statistics
    if (old_status == ACKNOWLEDGMENT_STATUS_PENDING) {
        g_acknowledgment_tracker.stats.pending_count--;
    } else if (old_status == ACKNOWLEDGMENT_STATUS_ACKNOWLEDGED) {
        g_acknowledgment_tracker.stats.acknowledged_count--;
    }
    g_acknowledgment_tracker.stats.resolved_count++;
    
    printf("ACKNOWLEDGMENT: Acknowledgment %s resolved\n", acknowledgment_id\n"\n"\n"\n"\n"\n"\n"\n");
    
    pthread_mutex_unlock(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    return 0;
}

// Get acknowledgment by ID
int acknowledgment_tracker_get_acknowledgment(const char* acknowledgment_id, acknowledgment_t* acknowledgment) {
    if (!g_acknowledgment_tracker_initialized || !acknowledgment_id || !acknowledgment) {
        return -1;
    }
    
    pthread_mutex_lock(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    for (int i = 0; i < g_acknowledgment_tracker.acknowledgment_count; i++) {
        if (strcmp(g_acknowledgment_tracker.acknowledgments[i].id, acknowledgment_id) == 0) {
            *acknowledgment = g_acknowledgment_tracker.acknowledgments[i];
            pthread_mutex_unlock(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
            return 0;
        }
    }
    
    pthread_mutex_unlock(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    return -1; // Not found
}

// Get pending acknowledgment for type
int acknowledgment_tracker_get_pending_for_type(notification_type_t type, acknowledgment_t* acknowledgment) {
    if (!g_acknowledgment_tracker_initialized || !acknowledgment) {
        return -1;
    }
    
    pthread_mutex_lock(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    time_t now = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    for (int i = 0; i < g_acknowledgment_tracker.acknowledgment_count; i++) {
        acknowledgment_t* ack = &g_acknowledgment_tracker.acknowledgments[i];
        
        if (ack->type == type && ack->status == ACKNOWLEDGMENT_STATUS_PENDING && ack->expires_at > now) {
            *acknowledgment = *ack;
            pthread_mutex_unlock(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
            return 0; // Found pending acknowledgment
        }
    }
    
    pthread_mutex_unlock(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    return -1; // No pending acknowledgment found
}

// Check if notification should be sent
bool acknowledgment_tracker_should_send_notification(const notification_event_t* event) {
    if (!g_acknowledgment_tracker_initialized || !event) {
        return true; // Send if not initialized
    }
    
    if (!g_acknowledgment_tracker.config.enabled) {
        return true; // Always send if tracking disabled
    }
    
    if (!is_acknowledgment_required(event->type)) {
        return true; // No acknowledgment required
    }
    
    // Check for pending acknowledgment
    acknowledgment_t pending_ack;
    if (acknowledgment_tracker_get_pending_for_type(event->type, &pending_ack) == 0) {
        printf("ACKNOWLEDGMENT: Notification suppressed due to pending acknowledgment %s\n", pending_ack.id\n"\n"\n"\n"\n"\n"\n"\n");
        return false; // Don't send if there's a pending acknowledgment
    }
    
    return true; // OK to send
}

// Get acknowledgment statistics
void acknowledgment_tracker_get_stats(acknowledgment_stats_t* stats) {
    if (!stats || !g_acknowledgment_tracker_initialized) return;
    
    pthread_mutex_lock(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    *stats = g_acknowledgment_tracker.stats;
    pthread_mutex_unlock(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
}

// Get acknowledgment tracker status
void acknowledgment_tracker_get_status(acknowledgment_tracker_status_t* status) {
    if (!status || !g_acknowledgment_tracker_initialized) return;
    
    pthread_mutex_lock(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    status->enabled = g_acknowledgment_tracker.config.enabled;
    status->total_acknowledgments = g_acknowledgment_tracker.stats.total_acknowledgments;
    status->pending_count = g_acknowledgment_tracker.stats.pending_count;
    status->acknowledged_count = g_acknowledgment_tracker.stats.acknowledged_count;
    status->expired_count = g_acknowledgment_tracker.stats.expired_count;
    status->resolved_count = g_acknowledgment_tracker.stats.resolved_count;
    status->max_acknowledgments = g_acknowledgment_tracker.max_acknowledgments;
    status->last_cleanup = g_acknowledgment_tracker.stats.last_cleanup;
    status->acknowledgment_rate = g_acknowledgment_tracker.stats.acknowledgment_rate;
    
    pthread_mutex_unlock(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
}

// List acknowledgments with filters
int acknowledgment_tracker_list_acknowledgments(acknowledgment_t* acknowledgments,
                                               int max_acknowledgments,
                                               acknowledgment_status_t status_filter,
                                               notification_type_t type_filter) {
    if (!g_acknowledgment_tracker_initialized || !acknowledgments || max_acknowledgments <= 0) {
        return -1;
    }
    
    pthread_mutex_lock(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    int count = 0;
    
    for (int i = 0; i < g_acknowledgment_tracker.acknowledgment_count && count < max_acknowledgments; i++) {
        acknowledgment_t* ack = &g_acknowledgment_tracker.acknowledgments[i];
        
        // Apply filters
        bool matches = true;
        
        if (status_filter != -1 && ack->status != status_filter) {
            matches = false;
        }
        
        if (type_filter != -1 && ack->type != type_filter) {
            matches = false;
        }
        
        if (matches) {
            acknowledgments[count] = *ack;
            count++;
        }
    }
    
    pthread_mutex_unlock(g_acknowledgment_tracker.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    return count;
}

// Check if acknowledgment tracker is initialized
bool acknowledgment_tracker_is_initialized(void) {
    return g_acknowledgment_tracker_initialized;
}

// Get acknowledgment tracker instance
acknowledgment_tracker_t* acknowledgment_tracker_get_instance(void) {
    return g_acknowledgment_tracker_initialized ? &g_acknowledgment_tracker : NULL;
}
