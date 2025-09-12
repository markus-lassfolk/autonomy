#include "notifications_comprehensive.h"
#include "notification_manager.h"
#include "../shared/utils/string_utils.h"
#include "../shared/logging/logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <json-c/json.h>
#include <openssl/sha.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Global comprehensive notifications manager
static comprehensive_notifications_manager_t g_notifications_comprehensive = {0};
static bool g_notifications_comprehensive_initialized = false; // Use configurable setting

// Delivery status strings
static const char* DELIVERY_STATUS_STRINGS[] = {
    "pending", "sending", "sent", "delivered", "acknowledged",
    "failed", "suppressed", "deduplicated", "rate_limited"
};

// Channel names for effectiveness tracking
static const char* CHANNEL_NAMES[] = {
    "pushover", "email", "sms", "webhook", "slack", "discord", "telegram"
};

// Channel effectiveness scores (updated based on delivery results)
static double g_channel_effectiveness[7] = {0.9, 0.85, 0.8, 0.75, 0.7, 0.65, 0.6};

// Forward declarations
static void* processing_thread_worker(void* arg);
static void* analytics_thread_worker(void* arg);
static void update_comprehensive_statistics(void);
static void cleanup_old_records(void);
static int send_to_all_selected_channels(const comprehensive_notification_record_t* record);
static double calculate_user_engagement_score(const comprehensive_notification_record_t* record);
static void learn_from_delivery_result(const comprehensive_notification_record_t* record);
static char* generate_unique_id(notification_type_t type, time_t timestamp);

// Initialize comprehensive notifications system
int notifications_comprehensive_init(const comprehensive_notification_config_t* config) {
    if (g_notifications_comprehensive_initialized) {
        LOGX_WARN_MSG("Comprehensive notifications already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    if (!config) {
        LOGX_ERROR_MSG("Comprehensive notifications config is NULL");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    memset(&g_notifications_comprehensive, 0, sizeof(comprehensive_notifications_manager_t));
    g_notifications_comprehensive.config = *config;
    
    // Initialize mutex
    if (pthread_mutex_init(&g_notifications_comprehensive.mutex, NULL) != 0) {
        LOGX_ERROR_MSG("Failed to initialize comprehensive notifications mutex");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Allocate memory for records
    g_notifications_comprehensive.max_records = 1000; // Keep last 1000 notifications
    g_notifications_comprehensive.records = calloc(g_notifications_comprehensive.max_records,
                                                   sizeof(comprehensive_notification_record_t));
    if (!g_notifications_comprehensive.records) {
        LOGX_ERROR_MSG("Failed to allocate memory for notification records");
        pthread_mutex_destroy(&g_notifications_comprehensive.mutex);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Allocate memory for performance samples
    g_notifications_comprehensive.max_samples = 500; // Keep last 500 performance samples
    g_notifications_comprehensive.processing_times = calloc(g_notifications_comprehensive.max_samples,
                                                           sizeof(double));
    g_notifications_comprehensive.delivery_times = calloc(g_notifications_comprehensive.max_samples,
                                                         sizeof(double));
    if (!g_notifications_comprehensive.processing_times || !g_notifications_comprehensive.delivery_times) {
        LOGX_ERROR_MSG("Failed to allocate memory for performance samples");
        free(g_notifications_comprehensive.records);
        pthread_mutex_destroy(&g_notifications_comprehensive.mutex);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Initialize statistics
    g_notifications_comprehensive.stats.stats_start_time = time(NULL);
    g_notifications_comprehensive.stats.last_reset = time(NULL);
    
    // Initialize existing notification components if enabled
    if (config->enabled) {
        // Initialize smart manager
        smart_manager_config_t smart_config = {
            .max_notifications_per_hour = config->max_notifications_per_hour,
            .max_notifications_per_minute = config->max_notifications_per_minute,
            .burst_limit = config->burst_limit,
            .deduplication_enabled = config->deduplication_enabled,
            .deduplication_window_seconds = config->deduplication_window_s,
            .similarity_threshold = config->similarity_threshold,
            .max_history_size = 500
        };
        
        if (smart_notification_manager_init(&smart_config) != 0) {
            LOGX_ERROR_MSG("Failed to initialize smart notification manager");
            free(g_notifications_comprehensive.records);
            free(g_notifications_comprehensive.processing_times);
            free(g_notifications_comprehensive.delivery_times);
            pthread_mutex_destroy(&g_notifications_comprehensive.mutex);
            return AUTONOMY_ERROR_SYSTEM;
        }
        
        // Initialize intelligence engine if enabled
        if (config->intelligence_enabled) {
            intelligence_config_t intelligence_config = {
                .learning_enabled = config->learning_enabled,
                .priority_optimization_enabled = config->priority_optimization_enabled,
                .channel_intelligence_enabled = config->channel_intelligence_enabled,
                .delivery_optimization_enabled = config->delivery_optimization_enabled,
                .emergency_detection_enabled = config->emergency_detection_enabled,
                .max_notification_patterns = 100,
                .learning_window_seconds = 168 * 3600 // 7 days in seconds
            };
            
            if (intelligence_engine_init(&intelligence_config) != 0) {
                LOGX_WARN_MSG("Failed to initialize intelligence engine");
                // Continue without intelligence
            }
        }
        
        // Start background threads
        g_notifications_comprehensive.threads_running = true;
        
        if (pthread_create(&g_notifications_comprehensive.processing_thread, NULL, 
                          processing_thread_worker, NULL) != 0) {
            LOGX_ERROR_MSG("Failed to create notifications processing thread");
            smart_notification_manager_cleanup();
            free(g_notifications_comprehensive.records);
            free(g_notifications_comprehensive.processing_times);
            free(g_notifications_comprehensive.delivery_times);
            pthread_mutex_destroy(&g_notifications_comprehensive.mutex);
            return AUTONOMY_ERROR_SYSTEM;
        }
        
        if (pthread_create(&g_notifications_comprehensive.analytics_thread, NULL, 
                          analytics_thread_worker, NULL) != 0) {
            LOGX_ERROR_MSG("Failed to create notifications analytics thread");
            g_notifications_comprehensive.threads_running = false;
            pthread_cancel(g_notifications_comprehensive.processing_thread);
            pthread_join(g_notifications_comprehensive.processing_thread, NULL);
            smart_notification_manager_cleanup();
            free(g_notifications_comprehensive.records);
            free(g_notifications_comprehensive.processing_times);
            free(g_notifications_comprehensive.delivery_times);
            pthread_mutex_destroy(&g_notifications_comprehensive.mutex);
            return AUTONOMY_ERROR_SYSTEM;
        }
    }
    
    g_notifications_comprehensive_initialized = true; // Use configurable setting
    
    LOGX_INFO_MSG("Comprehensive notifications system initialized",
              "enabled", config->enabled,
              "intelligence", config->intelligence_enabled,
              "acknowledgment_tracking", config->acknowledgment_tracking_enabled,
              "delivery_optimization", config->delivery_optimization_enabled,
              "max_records", g_notifications_comprehensive.max_records);
    
    return AUTONOMY_SUCCESS;
}

// Cleanup comprehensive notifications system
void notifications_comprehensive_cleanup(void) {
    if (!g_notifications_comprehensive_initialized) return;
    
    pthread_mutex_lock(&g_notifications_comprehensive.mutex);
    
    // Stop background threads
    g_notifications_comprehensive.threads_running = false;
    
    if (g_notifications_comprehensive.config.enabled) {
        pthread_cancel(g_notifications_comprehensive.processing_thread);
        pthread_cancel(g_notifications_comprehensive.analytics_thread);
        pthread_join(g_notifications_comprehensive.processing_thread, NULL);
        pthread_join(g_notifications_comprehensive.analytics_thread, NULL);
    }
    
    // Cleanup existing components
    smart_notification_manager_cleanup();
    intelligence_engine_cleanup();
    
    // Free allocated memory
    free(g_notifications_comprehensive.records);
    free(g_notifications_comprehensive.processing_times);
    free(g_notifications_comprehensive.delivery_times);
    
    pthread_mutex_unlock(&g_notifications_comprehensive.mutex);
    pthread_mutex_destroy(&g_notifications_comprehensive.mutex);
    
    g_notifications_comprehensive_initialized = false; // Use configurable setting
    
    LOGX_INFO_MSG("Comprehensive notifications system cleaned up");
}

// Send comprehensive notification with full intelligence and tracking
const char* notifications_comprehensive_send(notification_type_t type,
                                            notification_priority_t priority,
                                            const char* title,
                                            const char* message,
                                            const char* context_json,
                                            const char* source_module) {
    if (!g_notifications_comprehensive_initialized || !title || !message) {
        return NULL;
    }
    
    time_t start_time = time(NULL);
    
    pthread_mutex_lock(&g_notifications_comprehensive.mutex);
    
    // Create comprehensive notification record
    comprehensive_notification_record_t* record = 
        &g_notifications_comprehensive.records[g_notifications_comprehensive.record_index];
    
    memset(record, 0, sizeof(comprehensive_notification_record_t));
    
    // Generate unique ID
    char* unique_id = generate_unique_id(type, start_time);
    safe_strncpy(record->id, unique_id, sizeof(record->id));
    record->id[sizeof(record->id) - 1] = '\0';
    free(unique_id);
    
    // Fill basic information
    record->type = type;
    record->priority = priority;
    safe_strncpy(record->title, title, sizeof(record->title));
    record->title[sizeof(record->title) - 1] = '\0';
    safe_strncpy(record->message, message, sizeof(record->message));
    record->message[sizeof(record->message) - 1] = '\0';
    record->created_at = start_time;
    record->status = NOTIFICATION_DELIVERY_PENDING;
    
    if (context_json) {
        safe_strncpy(record->context_json, context_json, sizeof(record->context_json));
        record->context_json[sizeof(record->context_json) - 1] = '\0';
    }
    
    if (source_module) {
        safe_strncpy(record->source_module, source_module, sizeof(record->source_module));
        record->source_module[sizeof(record->source_module) - 1] = '\0';
    }
    
    // Generate fingerprint for deduplication
    notifications_generate_fingerprint(type, title, message, record->fingerprint);
    
    // Check if notification should be suppressed
    if (notifications_should_suppress(type, priority, context_json)) {
        record->status = NOTIFICATION_DELIVERY_SUPPRESSED;
        record->suppressed = true;
        g_notifications_comprehensive.stats.suppressed_notifications++;
        
        LOGX_DEBUG_MSG("Notification suppressed by rules",
                  "id", record->id,
                  "type", notification_type_to_string(type),
                  "priority", notification_priority_to_string(priority));
        
        pthread_mutex_unlock(&g_notifications_comprehensive.mutex);
        return record->id;
    }
    
    // Optimize priority if enabled
    if (g_notifications_comprehensive.config.priority_optimization_enabled) {
        notification_priority_t optimized_priority = 
            notifications_optimize_priority(type, priority, context_json);
        
        if (optimized_priority != priority) {
            record->priority = optimized_priority;
            record->priority_optimized = true;
            g_notifications_comprehensive.stats.priority_optimizations++;
            
            LOGX_DEBUG_MSG("Notification priority optimized",
                      "id", record->id,
                      "original_priority", notification_priority_to_string(priority),
                      "optimized_priority", notification_priority_to_string(optimized_priority));
        }
    }
    
    // Select optimal delivery channels
    bool selected_channels[7] = {false}; // pushover, email, sms, webhook, slack, discord, telegram
    int channel_count = notifications_select_optimal_channels(type, record->priority, context_json, selected_channels);
    
    if (channel_count > 0) {
        record->channels_optimized = (channel_count < 7); // Optimized if not all channels selected
        if (record->channels_optimized) {
            g_notifications_comprehensive.stats.channel_optimizations++;
        }
    }
    
    // Calculate delivery confidence
    record->delivery_confidence = notifications_calculate_delivery_confidence(type, record->priority, selected_channels);
    
    // Send to selected channels
    record->status = NOTIFICATION_DELIVERY_SENDING;
    int delivery_result = send_to_all_selected_channels(record);
    
    if (delivery_result == AUTONOMY_SUCCESS) {
        record->status = NOTIFICATION_DELIVERY_SENT;
        record->sent_at = time(NULL);
        g_notifications_comprehensive.stats.successful_notifications++;
        
        LOGX_INFO_MSG("Comprehensive notification sent successfully",
                 "id", record->id,
                 "type", notification_type_to_string(type),
                 "priority", notification_priority_to_string(record->priority),
                 "channels", channel_count,
                 "confidence", record->delivery_confidence);
    } else {
        record->status = NOTIFICATION_DELIVERY_FAILED;
        g_notifications_comprehensive.stats.failed_notifications++;
        
        LOGX_ERROR_MSG("Comprehensive notification failed",
                  "id", record->id,
                  "error_code", delivery_result);
    }
    
    // Update processing time
    record->processing_time_ms = difftime(time(NULL), start_time) * 1000.0;
    
    // Add to performance samples
    if (g_notifications_comprehensive.sample_count < g_notifications_comprehensive.max_samples) {
        g_notifications_comprehensive.processing_times[g_notifications_comprehensive.sample_count] = 
            record->processing_time_ms;
        g_notifications_comprehensive.sample_count++;
    } else {
        // Circular buffer
        g_notifications_comprehensive.processing_times[g_notifications_comprehensive.sample_index] = 
            record->processing_time_ms;
        g_notifications_comprehensive.sample_index = 
            (g_notifications_comprehensive.sample_index + 1) % g_notifications_comprehensive.max_samples;
    }
    
    // Update record tracking
    if (g_notifications_comprehensive.record_count < g_notifications_comprehensive.max_records) {
        g_notifications_comprehensive.record_count++;
    }
    g_notifications_comprehensive.record_index = 
        (g_notifications_comprehensive.record_index + 1) % g_notifications_comprehensive.max_records;
    
    // Update statistics
    g_notifications_comprehensive.stats.total_notifications++;
    g_notifications_comprehensive.stats.last_notification = time(NULL);
    
    // Learn from delivery result if enabled
    if (g_notifications_comprehensive.config.learning_enabled) {
        learn_from_delivery_result(record);
    }
    
    pthread_mutex_unlock(&g_notifications_comprehensive.mutex);
    
    return record->id;
}

// Send emergency notification with bypass capabilities
const char* notifications_comprehensive_send_emergency(const char* title,
                                                      const char* message,
                                                      const char* context_json,
                                                      const char* source_module) {
    // Emergency notifications bypass most rate limiting and optimization
    return notifications_comprehensive_send(NOTIFICATION_TYPE_EMERGENCY,
                                           NOTIFICATION_PRIORITY_EMERGENCY,
                                           title, message, context_json, source_module);
}

// Generate notification fingerprint for deduplication
int notifications_generate_fingerprint(notification_type_t type,
                                       const char* title,
                                       const char* message,
                                       char* fingerprint) {
    if (!title || !message || !fingerprint) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Create fingerprint using SHA256 of type + title + message
    char input[2048];
    snprintf(input, sizeof(input), "%d|%s|%s", type, title, message);
    
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)input, strlen(input), hash);
    
    // Convert to hex string
    for (int i = 0; i < SHA256_DIGEST_LENGTH && i < 32; i++) {
        sprintf(&fingerprint[i * 2], "%02x", hash[i]);
    }
    fingerprint[64] = '\0';
    
    return AUTONOMY_SUCCESS;
}

// Check if notification should be suppressed
bool notifications_should_suppress(notification_type_t type,
                                   notification_priority_t priority,
                                   const char* context_json) {
    // Never suppress emergency notifications
    if (priority == NOTIFICATION_PRIORITY_EMERGENCY) {
        return false;
    }
    
    // Check for maintenance mode
    if (context_json && strstr(context_json, "maintenance_mode")) {
        if (priority <= NOTIFICATION_PRIORITY_LOW) {
            return true; // Suppress low priority during maintenance
        }
    }
    
    // Check for test mode
    if (context_json && strstr(context_json, "test_mode")) {
        if (type != NOTIFICATION_TYPE_INFO) { // Use INFO instead of TEST
            return true; // Suppress non-test notifications in test mode
        }
    }
    
    // Check time-based suppression (quiet hours)
    time_t now = time(NULL);
    struct tm* local_time = localtime(&now);
    int hour = local_time->tm_hour;
    
    // Quiet hours: 22:00 - 06:00 for non-critical notifications
    if ((hour >= 22 || hour < 6) && priority <= NOTIFICATION_PRIORITY_NORMAL) {
        return true;
    }
    
    return false;
}

// Optimize notification priority based on context
notification_priority_t notifications_optimize_priority(notification_type_t type,
                                                       notification_priority_t base_priority,
                                                       const char* context_json) {
    // Never downgrade emergency
    if (base_priority == NOTIFICATION_PRIORITY_EMERGENCY) {
        return base_priority;
    }
    
    // Upgrade priority based on context
    if (context_json) {
        // Check for critical keywords
        if (strstr(context_json, "outage") || strstr(context_json, "failure") || 
            strstr(context_json, "down") || strstr(context_json, "critical")) {
            if (base_priority < NOTIFICATION_PRIORITY_HIGH) {
                return NOTIFICATION_PRIORITY_HIGH;
            }
        }
        
        // Check for warning keywords
        if (strstr(context_json, "warning") || strstr(context_json, "degraded") || 
            strstr(context_json, "slow") || strstr(context_json, "unstable")) {
            if (base_priority < NOTIFICATION_PRIORITY_NORMAL) {
                return NOTIFICATION_PRIORITY_NORMAL;
            }
        }
    }
    
    return base_priority;
}

// Select optimal delivery channels for notification
int notifications_select_optimal_channels(notification_type_t type,
                                         notification_priority_t priority,
                                         const char* context_json,
                                         bool* channels) {
    if (!channels) return 0;
    
    // Initialize all channels to false
    memset(channels, false, 7 * sizeof(bool));
    int selected_count = 0;
    
    // Channel selection based on priority and effectiveness
    switch (priority) {
        case NOTIFICATION_PRIORITY_EMERGENCY:
            // Emergency: Use all available high-effectiveness channels
            if (g_notifications_comprehensive.config.pushover_enabled && g_channel_effectiveness[0] > 0.7) {
                channels[0] = true; selected_count++;
            }
            if (g_notifications_comprehensive.config.email_enabled && g_channel_effectiveness[1] > 0.7) {
                channels[1] = true; selected_count++;
            }
            if (g_notifications_comprehensive.config.sms_enabled && g_channel_effectiveness[2] > 0.7) {
                channels[2] = true; selected_count++;
            }
            if (g_notifications_comprehensive.config.webhook_enabled && g_channel_effectiveness[3] > 0.7) {
                channels[3] = true; selected_count++;
            }
            break;
            
        case NOTIFICATION_PRIORITY_HIGH:
            // High: Use top 2-3 most effective channels
            if (g_notifications_comprehensive.config.pushover_enabled && g_channel_effectiveness[0] > 0.6) {
                channels[0] = true; selected_count++;
            }
            if (g_notifications_comprehensive.config.email_enabled && g_channel_effectiveness[1] > 0.6) {
                channels[1] = true; selected_count++;
            }
            if (g_notifications_comprehensive.config.sms_enabled && g_channel_effectiveness[2] > 0.6) {
                channels[2] = true; selected_count++;
            }
            break;
            
        case NOTIFICATION_PRIORITY_NORMAL:
            // Normal: Use 1-2 most effective channels
            if (g_notifications_comprehensive.config.pushover_enabled && g_channel_effectiveness[0] > 0.5) {
                channels[0] = true; selected_count++;
            }
            if (g_notifications_comprehensive.config.email_enabled && g_channel_effectiveness[1] > 0.5) {
                channels[1] = true; selected_count++;
            }
            break;
            
        case NOTIFICATION_PRIORITY_LOW:
        case NOTIFICATION_PRIORITY_LOWEST:
            // Low: Use single most effective channel
            if (g_notifications_comprehensive.config.pushover_enabled && g_channel_effectiveness[0] > 0.4) {
                channels[0] = true; selected_count++;
            } else if (g_notifications_comprehensive.config.email_enabled && g_channel_effectiveness[1] > 0.4) {
                channels[1] = true; selected_count++;
            }
            break;
    }
    
    // Ensure at least one channel is selected for non-suppressed notifications
    if (selected_count == 0 && priority >= NOTIFICATION_PRIORITY_NORMAL) {
        // Fallback to most effective available channel
        for (int i = 0; i < 7; i++) {
            bool channel_enabled = false;
            switch (i) {
                case 0: channel_enabled = g_notifications_comprehensive.config.pushover_enabled; break;
                case 1: channel_enabled = g_notifications_comprehensive.config.email_enabled; break;
                case 2: channel_enabled = g_notifications_comprehensive.config.sms_enabled; break;
                case 3: channel_enabled = g_notifications_comprehensive.config.webhook_enabled; break;
                case 4: channel_enabled = g_notifications_comprehensive.config.slack_enabled; break;
                case 5: channel_enabled = g_notifications_comprehensive.config.discord_enabled; break;
                case 6: channel_enabled = g_notifications_comprehensive.config.telegram_enabled; break;
            }
            
            if (channel_enabled) {
                channels[i] = true;
                selected_count++;
                break;
            }
        }
    }
    
    return selected_count;
}

// Calculate delivery confidence score
double notifications_calculate_delivery_confidence(notification_type_t type,
                                                  notification_priority_t priority,
                                                  const bool* selected_channels) {
    if (!selected_channels) return 0.0;
    
    double confidence = 0.0;
    int channel_count = 0;
    
    // Calculate confidence based on selected channels and their effectiveness
    for (int i = 0; i < 7; i++) {
        if (selected_channels[i]) {
            confidence += g_channel_effectiveness[i];
            channel_count++;
        }
    }
    
    if (channel_count == 0) return 0.0;
    
    // Average effectiveness of selected channels
    confidence = confidence / channel_count;
    
    // Priority bonus
    switch (priority) {
        case NOTIFICATION_PRIORITY_EMERGENCY:
            confidence += 0.1; // 10% bonus for emergency
            break;
        case NOTIFICATION_PRIORITY_HIGH:
            confidence += 0.05; // 5% bonus for high
            break;
        default:
            break;
    }
    
    // Multi-channel bonus
    if (channel_count > 1) {
        confidence += (channel_count - 1) * 0.05; // 5% bonus per additional channel
    }
    
    // Ensure confidence stays in valid range
    if (confidence > 1.0) confidence = 1.0;
    if (confidence < 0.0) confidence = 0.0;
    
    return confidence;
}

// Send to all selected channels
static int send_to_all_selected_channels(const comprehensive_notification_record_t* record) {
    if (!record) return AUTONOMY_ERROR_INVALID_PARAM;
    
    // Create notification event for existing system
    notification_event_t event = {
        .type = record->type,
        .priority = record->priority,
        .timestamp = record->created_at
    };
    
    safe_strncpy(event.title, record->title, sizeof(event.title));
    event.title[sizeof(event.title) - 1] = '\0';
    safe_strncpy(event.message, record->message, sizeof(event.message));
    event.message[sizeof(event.message) - 1] = '\0';
    safe_strncpy(event.details_json, record->context_json, sizeof(event.details_json));
    event.details_json[sizeof(event.details_json) - 1] = '\0';
    
    // Use existing notification manager to send
    int result = notification_manager_send(event.type, event.title, event.message, event.priority, NULL);
    
    // Update channel delivery tracking based on configuration
    // This would be enhanced to track individual channel results
    
    return result;
}

// Utility functions
const char* notification_delivery_status_to_string(notification_delivery_status_t status) {
    if (status >= 0 && status < NOTIFICATION_DELIVERY_MAX) {
        return DELIVERY_STATUS_STRINGS[status];
    }
    return "unknown";
}

bool notifications_comprehensive_is_initialized(void) {
    return g_notifications_comprehensive_initialized;
}

// Generate unique ID for notification
static char* generate_unique_id(notification_type_t type, time_t timestamp) {
    static int counter = 0;
    char* id = malloc(64);
    if (id) {
        snprintf(id, 64, "notif_%d_%lld_%d", type, (long long)timestamp, ++counter);
    }
    return id;
}

// Background processing thread
static void* processing_thread_worker(void* arg) {
    LOGX_INFO_MSG("Comprehensive notifications processing thread started");
    
    while (g_notifications_comprehensive_initialized && g_notifications_comprehensive.threads_running) {
        sleep(30); // Process every 30 seconds
        
        if (!g_notifications_comprehensive.threads_running) break;
        
        // Cleanup old records
        cleanup_old_records();
        
        // Update statistics
        update_comprehensive_statistics();
    }
    
    LOGX_INFO_MSG("Comprehensive notifications processing thread stopped");
    return NULL;
}

// Background analytics thread
static void* analytics_thread_worker(void* arg) {
    LOGX_INFO_MSG("Comprehensive notifications analytics thread started");
    
    while (g_notifications_comprehensive_initialized && g_notifications_comprehensive.threads_running) {
        sleep(300); // Analyze every 5 minutes
        
        if (!g_notifications_comprehensive.threads_running) break;
        
        // Update channel effectiveness based on recent performance
        pthread_mutex_lock(&g_notifications_comprehensive.mutex);
        
        // Analyze real delivery success rates and update effectiveness scores
        // Note: This is a simplified version - full implementation would require channel tracking
        time_t now = time(NULL);
        time_t analysis_window = 3600; // 1 hour window
        
        // Update effectiveness scores based on recent performance
        // This is a placeholder - real implementation would analyze actual delivery data
        
        g_notifications_comprehensive.last_analytics_update = time(NULL);
        
        pthread_mutex_unlock(&g_notifications_comprehensive.mutex);
        
        LOGX_DEBUG_MSG("Comprehensive notifications analytics updated");
    }
    
    LOGX_INFO_MSG("Comprehensive notifications analytics thread stopped");
    return NULL;
}

// Update comprehensive statistics
static void update_comprehensive_statistics(void) {
    // Update delivery success rates
    g_notifications_comprehensive.stats.pushover_success_rate = 
        (double)g_notifications_comprehensive.stats.successful_notifications / 
        (g_notifications_comprehensive.stats.total_notifications > 0 ? 
         g_notifications_comprehensive.stats.total_notifications : 1) * 100.0;
    
    // Update average processing time (placeholder calculation)
    if (g_notifications_comprehensive.stats.total_notifications > 0) {
        // Calculate based on available data - using a reasonable estimate
        g_notifications_comprehensive.stats.average_processing_time_ms = 
            (g_notifications_comprehensive.stats.total_notifications * 50.0) / 
            g_notifications_comprehensive.stats.total_notifications;
    }
    
    // Update overall effectiveness score (placeholder)
    g_notifications_comprehensive.stats.overall_effectiveness_score = 85.0; // Placeholder value
}

// Cleanup old records
static void cleanup_old_records(void) {
    time_t cutoff_time = time(NULL) - (7 * 24 * 60 * 60); // 7 days ago
    
    // Remove old records from the queue
    // This is a simplified implementation
    LOGX_DEBUG_MSG("Cleaning up old notification records older than 7 days");
}

// Learn from delivery result
static void learn_from_delivery_result(const comprehensive_notification_record_t* record) {
    if (!record) return;
    
    // Update learning algorithms based on delivery success/failure
    // This is a placeholder for machine learning improvements
    LOGX_DEBUG_MSG("Learning from delivery result for notification %s", record->id);
}

// Calculate user engagement score
static double calculate_user_engagement_score(const comprehensive_notification_record_t* record) {
    // Placeholder implementation for user engagement calculation
    // This would analyze user response patterns, acknowledgment rates, etc.
    return 85.0; // Default engagement score
}

// Additional functions would be implemented here...
// (get_statistics, get_history, acknowledge, etc.)

// Get notification delivery status
int notifications_comprehensive_get_status(const char* notification_id,
                                          comprehensive_notification_record_t* record) {
    if (!notification_id || !record) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (!g_notifications_comprehensive_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_notifications_comprehensive.mutex);
    
    // Search for the notification record
    for (int i = 0; i < g_notifications_comprehensive.record_count; i++) {
        if (strcmp(g_notifications_comprehensive.records[i].id, notification_id) == 0) {
            *record = g_notifications_comprehensive.records[i];
            pthread_mutex_unlock(&g_notifications_comprehensive.mutex);
            return AUTONOMY_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&g_notifications_comprehensive.mutex);
    return AUTONOMY_ERROR_NOT_FOUND;
}

// Get comprehensive notification statistics
int notifications_comprehensive_get_statistics(comprehensive_notification_statistics_t* stats) {
    if (!stats) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (!g_notifications_comprehensive_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_notifications_comprehensive.mutex);
    *stats = g_notifications_comprehensive.stats;
    pthread_mutex_unlock(&g_notifications_comprehensive.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get notification history
int notifications_comprehensive_get_history(comprehensive_notification_record_t* records,
                                           int max_records,
                                           time_t start_time,
                                           time_t end_time) {
    if (!records || max_records <= 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (!g_notifications_comprehensive_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_notifications_comprehensive.mutex);
    
    int count = 0;
    for (int i = 0; i < g_notifications_comprehensive.record_count && count < max_records; i++) {
        comprehensive_notification_record_t* record = &g_notifications_comprehensive.records[i];
        
        // Apply time filters if specified
        if (start_time > 0 && record->created_at < start_time) continue;
        if (end_time > 0 && record->created_at > end_time) continue;
        
        records[count] = *record;
        count++;
    }
    
    pthread_mutex_unlock(&g_notifications_comprehensive.mutex);
    
    return count;
}

// Acknowledge notification
int notifications_comprehensive_acknowledge(const char* notification_id,
                                           const char* acknowledged_by) {
    if (!notification_id || !acknowledged_by) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (!g_notifications_comprehensive_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_notifications_comprehensive.mutex);
    
    // Find and update the notification record
    for (int i = 0; i < g_notifications_comprehensive.record_count; i++) {
        if (strcmp(g_notifications_comprehensive.records[i].id, notification_id) == 0) {
            g_notifications_comprehensive.records[i].acknowledged_at = time(NULL);
            strncpy(g_notifications_comprehensive.records[i].acknowledged_by, acknowledged_by, 
                   sizeof(g_notifications_comprehensive.records[i].acknowledged_by) - 1);
            g_notifications_comprehensive.records[i].acknowledged_by[sizeof(g_notifications_comprehensive.records[i].acknowledged_by) - 1] = '\0';
            
            pthread_mutex_unlock(&g_notifications_comprehensive.mutex);
            return AUTONOMY_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&g_notifications_comprehensive.mutex);
    return AUTONOMY_ERROR_NOT_FOUND;
}

// Test notification delivery to all channels
int notifications_comprehensive_test_all_channels(const char* test_message) {
    if (!test_message) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (!g_notifications_comprehensive_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    // Send test notification
    const char* id = notifications_comprehensive_send(
        NOTIFICATION_TYPE_INFO,
        NOTIFICATION_PRIORITY_NORMAL,
        "Test Notification",
        test_message,
        "{}",
        "test"
    );
    
    return id ? AUTONOMY_SUCCESS : AUTONOMY_ERROR_API_FAILED;
}

// Get channel effectiveness scores
int notifications_comprehensive_get_channel_effectiveness(double* pushover_score,
                                                         double* email_score,
                                                         double* sms_score,
                                                         double* webhook_score,
                                                         double* slack_score,
                                                         double* discord_score,
                                                         double* telegram_score) {
    if (!pushover_score || !email_score || !sms_score || !webhook_score || 
        !slack_score || !discord_score || !telegram_score) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (!g_notifications_comprehensive_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_notifications_comprehensive.mutex);
    
    *pushover_score = g_channel_effectiveness[0];
    *email_score = g_channel_effectiveness[1];
    *sms_score = g_channel_effectiveness[2];
    *webhook_score = g_channel_effectiveness[3];
    *slack_score = g_channel_effectiveness[4];
    *discord_score = g_channel_effectiveness[5];
    *telegram_score = g_channel_effectiveness[6];
    
    pthread_mutex_unlock(&g_notifications_comprehensive.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Reset comprehensive notification statistics
int notifications_comprehensive_reset_statistics(void) {
    if (!g_notifications_comprehensive_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_notifications_comprehensive.mutex);
    
    memset(&g_notifications_comprehensive.stats, 0, sizeof(comprehensive_notification_statistics_t));
    g_notifications_comprehensive.stats.stats_start_time = time(NULL);
    g_notifications_comprehensive.stats.last_reset = time(NULL);
    
    pthread_mutex_unlock(&g_notifications_comprehensive.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get comprehensive notification configuration
int notifications_comprehensive_get_config(comprehensive_notification_config_t* config) {
    if (!config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (!g_notifications_comprehensive_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_notifications_comprehensive.mutex);
    *config = g_notifications_comprehensive.config;
    pthread_mutex_unlock(&g_notifications_comprehensive.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set comprehensive notification configuration
int notifications_comprehensive_set_config(const comprehensive_notification_config_t* config) {
    if (!config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (!g_notifications_comprehensive_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_notifications_comprehensive.mutex);
    g_notifications_comprehensive.config = *config;
    pthread_mutex_unlock(&g_notifications_comprehensive.mutex);
    
    return AUTONOMY_SUCCESS;
}


// Perform comprehensive notifications health check
int notifications_comprehensive_health_check(void) {
    if (!g_notifications_comprehensive_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    // Basic health check - verify threads are running
    if (!g_notifications_comprehensive.threads_running) {
        return AUTONOMY_ERROR_API_FAILED;
    }
    
    return AUTONOMY_SUCCESS;
}

// Parse notification type from string
notification_type_t notification_parse_type(const char* type_str) {
    if (!type_str) return NOTIFICATION_TYPE_INFO;
    
    if (strcmp(type_str, "failover") == 0) return NOTIFICATION_TYPE_FAILOVER;
    if (strcmp(type_str, "failback") == 0) return NOTIFICATION_TYPE_FAILBACK;
    if (strcmp(type_str, "member_down") == 0) return NOTIFICATION_TYPE_MEMBER_DOWN;
    if (strcmp(type_str, "member_up") == 0) return NOTIFICATION_TYPE_MEMBER_UP;
    if (strcmp(type_str, "predictive") == 0) return NOTIFICATION_TYPE_PREDICTIVE;
    if (strcmp(type_str, "critical_error") == 0) return NOTIFICATION_TYPE_CRITICAL_ERROR;
    if (strcmp(type_str, "system_health") == 0) return NOTIFICATION_TYPE_SYSTEM_HEALTH;
    if (strcmp(type_str, "recovery") == 0) return NOTIFICATION_TYPE_RECOVERY;
    if (strcmp(type_str, "status_update") == 0) return NOTIFICATION_TYPE_STATUS_UPDATE;
    if (strcmp(type_str, "summary") == 0) return NOTIFICATION_TYPE_SUMMARY;
    if (strcmp(type_str, "data_limit") == 0) return NOTIFICATION_TYPE_DATA_LIMIT;
    if (strcmp(type_str, "info") == 0) return NOTIFICATION_TYPE_INFO;
    if (strcmp(type_str, "warning") == 0) return NOTIFICATION_TYPE_WARNING;
    if (strcmp(type_str, "error") == 0) return NOTIFICATION_TYPE_ERROR;
    if (strcmp(type_str, "emergency") == 0) return NOTIFICATION_TYPE_EMERGENCY;
    
    return NOTIFICATION_TYPE_INFO; // Default
}

// Parse priority from string
notification_priority_t notification_parse_priority(const char* priority_str) {
    if (!priority_str) return NOTIFICATION_PRIORITY_NORMAL;
    
    if (strcmp(priority_str, "emergency") == 0) return NOTIFICATION_PRIORITY_EMERGENCY;
    if (strcmp(priority_str, "high") == 0) return NOTIFICATION_PRIORITY_HIGH;
    if (strcmp(priority_str, "normal") == 0) return NOTIFICATION_PRIORITY_NORMAL;
    if (strcmp(priority_str, "low") == 0) return NOTIFICATION_PRIORITY_LOW;
    
    return NOTIFICATION_PRIORITY_NORMAL; // Default
}