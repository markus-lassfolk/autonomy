#include "channel_intelligence.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>

// Global channel intelligence instance
static channel_intelligence_t g_channel_intelligence;
static bool g_channel_intelligence_initialized = false;

// Forward declarations
static double get_default_channel_effectiveness(notification_channel_t channel);
static time_t get_default_channel_response_time(notification_channel_t channel);
static double calculate_context_adjustment(notification_channel_t channel, notification_type_t alert_type, 
                                          notification_priority_t priority, const system_state_t* system_state);
static double calculate_time_adjustment(notification_channel_t channel, const system_state_t* system_state);
static double calculate_priority_adjustment(notification_channel_t channel, notification_priority_t priority);
static void generate_score_reason(notification_channel_t channel, double final_score, 
                                 double context_adj, double time_adj, double priority_adj,
                                 char* reason, size_t max_size);
static int compare_channel_scores(const void* a, const void* b);

// Initialize channel intelligence
int channel_intelligence_init(const channel_intelligence_config_t* config) {
    if (g_channel_intelligence_initialized) {
        return 0; // Already initialized
    }
    
    if (!config) {
        return -1;
    }
    
    memset(&g_channel_intelligence, 0, sizeof(channel_intelligence_t));
    
    // Copy configuration
    g_channel_intelligence.config = *config;
    
    // Initialize mutex
    g_channel_intelligence.mutex = malloc(sizeof(pthread_mutex_t));
    if (!g_channel_intelligence.mutex) {
        return -1;
    }
    
    pthread_mutex_init(g_channel_intelligence.mutex, NULL);
    
    // Initialize channel effectiveness data
    if (config->max_channel_effectiveness_entries > 0) {
        g_channel_intelligence.channel_effectiveness = malloc(config->max_channel_effectiveness_entries * sizeof(channel_effectiveness_t));
        if (!g_channel_intelligence.channel_effectiveness) {
            pthread_mutex_destroy(g_channel_intelligence.mutex);
            free(g_channel_intelligence.mutex);
            return -1;
        }
        
        g_channel_intelligence.max_channel_effectiveness_entries = config->max_channel_effectiveness_entries;
        g_channel_intelligence.channel_effectiveness_count = 0;
        
        // Initialize default effectiveness data for all channels
        notification_channel_t channels[] = {
            NOTIFICATION_CHANNEL_PUSHOVER, NOTIFICATION_CHANNEL_EMAIL, NOTIFICATION_CHANNEL_SLACK,
            NOTIFICATION_CHANNEL_DISCORD, NOTIFICATION_CHANNEL_TELEGRAM, NOTIFICATION_CHANNEL_WEBHOOK,
            NOTIFICATION_CHANNEL_SMS, NOTIFICATION_CHANNEL_SYSLOG, NOTIFICATION_CHANNEL_UBUS
        };
        
        for (int i = 0; i < 9 && g_channel_intelligence.channel_effectiveness_count < config->max_channel_effectiveness_entries; i++) {
            channel_effectiveness_t* eff = &g_channel_intelligence.channel_effectiveness[g_channel_intelligence.channel_effectiveness_count];
            eff->channel = channels[i];
            eff->effectiveness_score = get_default_channel_effectiveness(channels[i]);
            eff->average_response_time_seconds = get_default_channel_response_time(channels[i]);
            eff->response_rate = 0.85; // Default response rate
            eff->total_sent = 0;
            eff->total_successful = 0;
            eff->last_updated = time(NULL);
            g_channel_intelligence.channel_effectiveness_count++;
        }
    }
    
    // Initialize statistics
    g_channel_intelligence.total_selections = 0;
    g_channel_intelligence.intelligent_selections = 0;
    
    g_channel_intelligence_initialized = true;
    return 0;
}

// Clean up channel intelligence
void channel_intelligence_cleanup(void) {
    if (!g_channel_intelligence_initialized) return;
    
    if (g_channel_intelligence.mutex) {
        pthread_mutex_destroy(g_channel_intelligence.mutex);
        free(g_channel_intelligence.mutex);
    }
    
    if (g_channel_intelligence.channel_effectiveness) {
        free(g_channel_intelligence.channel_effectiveness);
    }
    
    g_channel_intelligence.channel_effectiveness = NULL;
    g_channel_intelligence.mutex = NULL;
    g_channel_intelligence.channel_effectiveness_count = 0;
    g_channel_intelligence.max_channel_effectiveness_entries = 0;
    g_channel_intelligence.total_selections = 0;
    g_channel_intelligence.intelligent_selections = 0;
    
    g_channel_intelligence_initialized = false;
}

// Get default channel effectiveness
static double get_default_channel_effectiveness(notification_channel_t channel) {
    switch (channel) {
        case NOTIFICATION_CHANNEL_PUSHOVER:
            return 0.95; // Highest for mobile alerts
        case NOTIFICATION_CHANNEL_TELEGRAM:
            return 0.92; // High for instant messaging
        case NOTIFICATION_CHANNEL_SLACK:
            return 0.90; // High for team communication
        case NOTIFICATION_CHANNEL_DISCORD:
            return 0.88; // Good for community alerts
        case NOTIFICATION_CHANNEL_EMAIL:
            return 0.85; // Good for detailed notifications
        case NOTIFICATION_CHANNEL_WEBHOOK:
            return 0.80; // Variable depending on integration
        case NOTIFICATION_CHANNEL_SMS:
            return 0.87; // High for mobile alerts
        case NOTIFICATION_CHANNEL_SYSLOG:
            return 0.70; // System logging
        case NOTIFICATION_CHANNEL_UBUS:
            return 0.75; // System integration
        default:
            return 0.50; // Default moderate effectiveness
    }
}

// Get default channel response time
static time_t get_default_channel_response_time(notification_channel_t channel) {
    switch (channel) {
        case NOTIFICATION_CHANNEL_PUSHOVER:
            return 30; // 30 seconds - very fast mobile notifications
        case NOTIFICATION_CHANNEL_TELEGRAM:
            return 45; // 45 seconds - fast instant messaging
        case NOTIFICATION_CHANNEL_SLACK:
        case NOTIFICATION_CHANNEL_DISCORD:
            return 60; // 1 minute - fast team communication
        case NOTIFICATION_CHANNEL_EMAIL:
            return 300; // 5 minutes - slower email checking
        case NOTIFICATION_CHANNEL_WEBHOOK:
            return 120; // 2 minutes - variable webhook processing
        case NOTIFICATION_CHANNEL_SMS:
            return 60; // 1 minute - mobile SMS
        case NOTIFICATION_CHANNEL_SYSLOG:
        case NOTIFICATION_CHANNEL_UBUS:
            return 5; // 5 seconds - immediate system logging
        default:
            return 120; // Default 2 minutes
    }
}

// Calculate context-based score adjustments
static double calculate_context_adjustment(notification_channel_t channel, notification_type_t alert_type, 
                                          notification_priority_t priority, const system_state_t* system_state) {
    (void)priority; // May be used in the future
    (void)system_state; // May be used for system state context
    
    double adjustment = 0.0;
    
    // Alert type specific preferences
    switch (alert_type) {
        case NOTIFICATION_TYPE_FAILOVER:
        case NOTIFICATION_TYPE_SYSTEM_HEALTH:
            // Critical alerts prefer fast channels
            switch (channel) {
                case NOTIFICATION_CHANNEL_PUSHOVER:
                case NOTIFICATION_CHANNEL_TELEGRAM:
                    adjustment += 0.2; // Prefer instant notifications
                    break;
                case NOTIFICATION_CHANNEL_SLACK:
                case NOTIFICATION_CHANNEL_DISCORD:
                    adjustment += 0.1; // Good for team alerts
                    break;
                case NOTIFICATION_CHANNEL_EMAIL:
                    adjustment -= 0.1; // Less preferred for urgent alerts
                    break;
                default:
                    break;
            }
            break;
            
        case NOTIFICATION_TYPE_DATA_LIMIT:
        case NOTIFICATION_TYPE_PREDICTIVE:  // NOTIFICATION_TYPE_OBSTRUCTION not defined
            // Informational alerts can use any channel
            switch (channel) {
                case NOTIFICATION_CHANNEL_EMAIL:
                    adjustment += 0.1; // Good for detailed information
                    break;
                case NOTIFICATION_CHANNEL_SLACK:
                    adjustment += 0.05; // Good for team awareness
                    break;
                default:
                    break;
            }
            break;
            
        default:
            break;
    }
    
    return adjustment;
}

// Calculate time-based score adjustments
static double calculate_time_adjustment(notification_channel_t channel, const system_state_t* system_state) {
    (void)system_state; // May be used for user presence in the future
    
    double adjustment = 0.0;
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    int hour = tm_info->tm_hour;
    
    // Time of day preferences
    if (hour >= 22 || hour <= 6) { // Night time
        switch (channel) {
            case NOTIFICATION_CHANNEL_PUSHOVER:
            case NOTIFICATION_CHANNEL_TELEGRAM:
                adjustment -= 0.2; // Reduce intrusive notifications at night
                break;
            case NOTIFICATION_CHANNEL_EMAIL:
                adjustment += 0.1; // Email is less intrusive
                break;
            default:
                break;
        }
    } else if (hour >= 9 && hour <= 17) { // Business hours
        switch (channel) {
            case NOTIFICATION_CHANNEL_SLACK:
            case NOTIFICATION_CHANNEL_DISCORD:
                adjustment += 0.1; // Team channels more relevant
                break;
            case NOTIFICATION_CHANNEL_EMAIL:
                adjustment += 0.05; // Professional communication
                break;
            default:
                break;
        }
    }
    
    return adjustment;
}

// Calculate priority-based score adjustments
static double calculate_priority_adjustment(notification_channel_t channel, notification_priority_t priority) {
    double adjustment = 0.0;
    
    switch (priority) {
        case NOTIFICATION_PRIORITY_EMERGENCY:
            // Emergency: prefer all fast, reliable channels
            switch (channel) {
                case NOTIFICATION_CHANNEL_PUSHOVER:
                case NOTIFICATION_CHANNEL_TELEGRAM:
                    adjustment += 0.3; // Highest preference for instant channels
                    break;
                case NOTIFICATION_CHANNEL_SLACK:
                case NOTIFICATION_CHANNEL_DISCORD:
                    adjustment += 0.2; // High preference for team channels
                    break;
                case NOTIFICATION_CHANNEL_EMAIL:
                case NOTIFICATION_CHANNEL_WEBHOOK:
                    adjustment += 0.1; // Include all channels for emergency
                    break;
                default:
                    break;
            }
            break;
            
        case NOTIFICATION_PRIORITY_HIGH:
            // High priority: prefer fast channels
            switch (channel) {
                case NOTIFICATION_CHANNEL_PUSHOVER:
                case NOTIFICATION_CHANNEL_TELEGRAM:
                    adjustment += 0.2;
                    break;
                case NOTIFICATION_CHANNEL_SLACK:
                    adjustment += 0.1;
                    break;
                default:
                    break;
            }
            break;
            
        case NOTIFICATION_PRIORITY_LOW:
        case NOTIFICATION_PRIORITY_LOWEST:
            // Low priority: prefer less intrusive channels
            switch (channel) {
                case NOTIFICATION_CHANNEL_EMAIL:
                    adjustment += 0.1;
                    break;
                case NOTIFICATION_CHANNEL_PUSHOVER:
                case NOTIFICATION_CHANNEL_TELEGRAM:
                    adjustment -= 0.1;
                    break;
                default:
                    break;
            }
            break;
            
        default:
            break;
    }
    
    return adjustment;
}

// Generate score reason
static void generate_score_reason(notification_channel_t channel, double final_score, 
                                 double context_adj, double time_adj, double priority_adj,
                                 char* reason, size_t max_size) {
    double base_effectiveness = final_score - context_adj - time_adj - priority_adj;
    
    snprintf(reason, max_size, "Base: %.2f", base_effectiveness);
    
    if (context_adj != 0.0) {
        char temp[64];
        snprintf(temp, sizeof(temp), ", Context: %+.2f", context_adj);
        strncat(reason, temp, max_size - strlen(reason) - 1);
    }
    
    if (time_adj != 0.0) {
        char temp[64];
        snprintf(temp, sizeof(temp), ", Time: %+.2f", time_adj);
        strncat(reason, temp, max_size - strlen(reason) - 1);
    }
    
    if (priority_adj != 0.0) {
        char temp[64];
        snprintf(temp, sizeof(temp), ", Priority: %+.2f", priority_adj);
        strncat(reason, temp, max_size - strlen(reason) - 1);
    }
}

// Compare function for sorting channel scores
static int compare_channel_scores(const void* a, const void* b) {
    const channel_score_t* score_a = (const channel_score_t*)a;
    const channel_score_t* score_b = (const channel_score_t*)b;
    
    if (score_a->score > score_b->score) return -1;
    if (score_a->score < score_b->score) return 1;
    return 0;
}

// Calculate channel score
void channel_intelligence_calculate_channel_score(notification_channel_t channel,
                                                 notification_type_t alert_type,
                                                 notification_priority_t priority,
                                                 const system_state_t* system_state,
                                                 const char* base_data_json,
                                                 channel_score_t* score) {
    if (!score) return;
    
    memset(score, 0, sizeof(channel_score_t));
    score->channel = channel;
    
    // Get base effectiveness
    score->effectiveness = channel_intelligence_get_channel_effectiveness(channel);
    score->response_time_seconds = channel_intelligence_get_channel_response_time(channel);
    score->score = score->effectiveness;
    
    // Calculate adjustments
    double context_adj = calculate_context_adjustment(channel, alert_type, priority, system_state);
    double time_adj = calculate_time_adjustment(channel, system_state);
    double priority_adj = calculate_priority_adjustment(channel, priority);
    
    // Apply adjustments
    score->score += context_adj + time_adj + priority_adj;
    
    // Clamp score to valid range
    if (score->score < 0.0) {
        score->score = 0.0;
    } else if (score->score > 1.0) {
        score->score = 1.0;
    }
    
    // Generate reason
    generate_score_reason(channel, score->score, context_adj, time_adj, priority_adj,
                         score->reason, sizeof(score->reason));
}

// Score all available channels
int channel_intelligence_score_all_channels(notification_type_t alert_type,
                                           notification_priority_t priority,
                                           const system_state_t* system_state,
                                           const char* base_data_json,
                                           channel_score_t* channel_scores,
                                           int max_scores) {
    if (!g_channel_intelligence_initialized || !channel_scores || max_scores <= 0) {
        return -1;
    }
    
    notification_channel_t channels[] = {
        NOTIFICATION_CHANNEL_PUSHOVER, NOTIFICATION_CHANNEL_EMAIL, NOTIFICATION_CHANNEL_SLACK,
        NOTIFICATION_CHANNEL_DISCORD, NOTIFICATION_CHANNEL_TELEGRAM, NOTIFICATION_CHANNEL_WEBHOOK,
        NOTIFICATION_CHANNEL_SMS, NOTIFICATION_CHANNEL_SYSLOG, NOTIFICATION_CHANNEL_UBUS
    };
    
    int channel_count = (max_scores < 9) ? max_scores : 9;
    
    // Calculate scores for all channels
    for (int i = 0; i < channel_count; i++) {
        channel_intelligence_calculate_channel_score(channels[i], alert_type, priority, 
                                                    system_state, base_data_json, &channel_scores[i]);
    }
    
    // Sort by score (highest first)
    qsort(channel_scores, channel_count, sizeof(channel_score_t), compare_channel_scores);
    
    return channel_count;
}

// Select optimal channels for notification
int channel_intelligence_select_optimal_channels(notification_type_t alert_type,
                                                notification_priority_t priority,
                                                const system_state_t* system_state,
                                                const char* base_data_json,
                                                notification_channel_t* selected_channels,
                                                int max_channels) {
    if (!g_channel_intelligence_initialized || !selected_channels || max_channels <= 0) {
        return -1;
    }
    
    if (!g_channel_intelligence.config.channel_intelligence_enabled) {
        // No intelligence - return default channels
        selected_channels[0] = NOTIFICATION_CHANNEL_PUSHOVER;
        return 1;
    }
    
    pthread_mutex_lock(g_channel_intelligence.mutex);
    g_channel_intelligence.total_selections++;
    pthread_mutex_unlock(g_channel_intelligence.mutex);
    
    // Score all channels
    channel_score_t channel_scores[9];
    int score_count = channel_intelligence_score_all_channels(alert_type, priority, system_state, 
                                                             base_data_json, channel_scores, 9);
    
    if (score_count <= 0) {
        // Fallback to default
        selected_channels[0] = NOTIFICATION_CHANNEL_PUSHOVER;
        return 1;
    }
    
    // Check for forced channel selection
    if (base_data_json && strstr(base_data_json, "\"force_all_channels\":true")) {
        int count = (max_channels < score_count) ? max_channels : score_count;
        for (int i = 0; i < count; i++) {
            selected_channels[i] = channel_scores[i].channel;
        }
        return count;
    }
    
    // Select channels based on priority and scores
    int selected_count = 0;
    
    switch (priority) {
        case NOTIFICATION_PRIORITY_EMERGENCY:
            // Emergency: use top 3 channels or all channels with score > 0.6
            for (int i = 0; i < score_count && selected_count < max_channels; i++) {
                if (i < 3 || channel_scores[i].score > 0.6) {
                    selected_channels[selected_count] = channel_scores[i].channel;
                    selected_count++;
                }
            }
            break;
            
        case NOTIFICATION_PRIORITY_HIGH:
            // High: use top 2 channels or channels with score > 0.7
            for (int i = 0; i < score_count && selected_count < max_channels; i++) {
                if (i < 2 || channel_scores[i].score > 0.7) {
                    selected_channels[selected_count] = channel_scores[i].channel;
                    selected_count++;
                }
            }
            break;
            
        case NOTIFICATION_PRIORITY_NORMAL:
            // Normal: use top channel or channels with score > 0.8
            for (int i = 0; i < score_count && selected_count < max_channels; i++) {
                if (i < 1 || channel_scores[i].score > 0.8) {
                    selected_channels[selected_count] = channel_scores[i].channel;
                    selected_count++;
                }
            }
            break;
            
        case NOTIFICATION_PRIORITY_LOW:
        case NOTIFICATION_PRIORITY_LOWEST:
            // Low: use only the top channel if score > 0.5
            if (score_count > 0 && channel_scores[0].score > 0.5 && selected_count < max_channels) {
                selected_channels[selected_count] = channel_scores[0].channel;
                selected_count++;
            }
            break;
    }
    
    // Ensure at least one channel is selected
    if (selected_count == 0 && max_channels > 0) {
        selected_channels[0] = channel_scores[0].channel;
        selected_count = 1;
    }
    
    // Update statistics
    if (selected_count > 1) {
        pthread_mutex_lock(g_channel_intelligence.mutex);
        g_channel_intelligence.intelligent_selections++;
        pthread_mutex_unlock(g_channel_intelligence.mutex);
    }
    
    return selected_count;
}

// Update channel effectiveness data
int channel_intelligence_update_effectiveness(notification_channel_t channel,
                                             bool was_successful,
                                             time_t response_time_seconds) {
    if (!g_channel_intelligence_initialized || !g_channel_intelligence.config.learning_enabled) {
        return -1;
    }
    
    pthread_mutex_lock(g_channel_intelligence.mutex);
    
    // Find channel effectiveness entry
    channel_effectiveness_t* eff = NULL;
    for (int i = 0; i < g_channel_intelligence.channel_effectiveness_count; i++) {
        if (g_channel_intelligence.channel_effectiveness[i].channel == channel) {
            eff = &g_channel_intelligence.channel_effectiveness[i];
            break;
        }
    }
    
    if (!eff) {
        pthread_mutex_unlock(g_channel_intelligence.mutex);
        return -1; // Channel not found
    }
    
    // Update statistics
    eff->total_sent++;
    if (was_successful) {
        eff->total_successful++;
    }
    
    // Update effectiveness score (exponential moving average)
    double new_success_rate = (double)eff->total_successful / eff->total_sent;
    double alpha = 0.3; // Learning rate
    eff->effectiveness_score = (1.0 - alpha) * eff->effectiveness_score + alpha * new_success_rate;
    
    // Update response time (exponential moving average)
    if (was_successful && response_time_seconds > 0) {
        eff->average_response_time_seconds = 
            (time_t)((1.0 - alpha) * eff->average_response_time_seconds + alpha * response_time_seconds);
    }
    
    // Update response rate
    eff->response_rate = new_success_rate;
    eff->last_updated = time(NULL);
    
    pthread_mutex_unlock(g_channel_intelligence.mutex);
    return 0;
}

// Get channel effectiveness
double channel_intelligence_get_channel_effectiveness(notification_channel_t channel) {
    if (!g_channel_intelligence_initialized) {
        return get_default_channel_effectiveness(channel);
    }
    
    pthread_mutex_lock(g_channel_intelligence.mutex);
    
    // Find channel effectiveness entry
    for (int i = 0; i < g_channel_intelligence.channel_effectiveness_count; i++) {
        channel_effectiveness_t* eff = &g_channel_intelligence.channel_effectiveness[i];
        if (eff->channel == channel) {
            double effectiveness = eff->effectiveness_score;
            pthread_mutex_unlock(g_channel_intelligence.mutex);
            return effectiveness;
        }
    }
    
    pthread_mutex_unlock(g_channel_intelligence.mutex);
    return get_default_channel_effectiveness(channel);
}

// Get channel response time
time_t channel_intelligence_get_channel_response_time(notification_channel_t channel) {
    if (!g_channel_intelligence_initialized) {
        return get_default_channel_response_time(channel);
    }
    
    pthread_mutex_lock(g_channel_intelligence.mutex);
    
    // Find channel effectiveness entry
    for (int i = 0; i < g_channel_intelligence.channel_effectiveness_count; i++) {
        channel_effectiveness_t* eff = &g_channel_intelligence.channel_effectiveness[i];
        if (eff->channel == channel) {
            time_t response_time = eff->average_response_time_seconds;
            pthread_mutex_unlock(g_channel_intelligence.mutex);
            return response_time;
        }
    }
    
    pthread_mutex_unlock(g_channel_intelligence.mutex);
    return get_default_channel_response_time(channel);
}

// Get channel intelligence status
void channel_intelligence_get_status(channel_intelligence_status_t* status) {
    if (!status || !g_channel_intelligence_initialized) return;
    
    pthread_mutex_lock(g_channel_intelligence.mutex);
    
    status->enabled = g_channel_intelligence.config.channel_intelligence_enabled;
    status->learning_enabled = g_channel_intelligence.config.learning_enabled;
    status->channel_effectiveness_count = g_channel_intelligence.channel_effectiveness_count;
    status->max_channel_effectiveness_entries = g_channel_intelligence.max_channel_effectiveness_entries;
    status->total_selections = g_channel_intelligence.total_selections;
    status->intelligent_selections = g_channel_intelligence.intelligent_selections;
    
    // Calculate selection accuracy
    status->selection_accuracy = (g_channel_intelligence.total_selections > 0) ? 
        (double)g_channel_intelligence.intelligent_selections / g_channel_intelligence.total_selections : 0.0;
    
    pthread_mutex_unlock(g_channel_intelligence.mutex);
}

// Get channel intelligence statistics
void channel_intelligence_get_stats(char* stats_json, size_t max_size) {
    if (!stats_json || max_size == 0 || !g_channel_intelligence_initialized) return;
    
    pthread_mutex_lock(g_channel_intelligence.mutex);
    
    snprintf(stats_json, max_size,
             "{"
             "\"intelligence_enabled\":%s,"
             "\"learning_enabled\":%s,"
             "\"total_selections\":%d,"
             "\"intelligent_selections\":%d,"
             "\"selection_accuracy\":%.3f,"
             "\"channel_effectiveness_count\":%d"
             "}",
             g_channel_intelligence.config.channel_intelligence_enabled ? "true" : "false",
             g_channel_intelligence.config.learning_enabled ? "true" : "false",
             g_channel_intelligence.total_selections,
             g_channel_intelligence.intelligent_selections,
             g_channel_intelligence.total_selections > 0 ? 
                (double)g_channel_intelligence.intelligent_selections / g_channel_intelligence.total_selections : 0.0,
             g_channel_intelligence.channel_effectiveness_count);
    
    pthread_mutex_unlock(g_channel_intelligence.mutex);
}

// Check if channel intelligence is initialized
bool channel_intelligence_is_initialized(void) {
    return g_channel_intelligence_initialized;
}

// Get channel intelligence instance
channel_intelligence_t* channel_intelligence_get_instance(void) {
    return g_channel_intelligence_initialized ? &g_channel_intelligence : NULL;
}
