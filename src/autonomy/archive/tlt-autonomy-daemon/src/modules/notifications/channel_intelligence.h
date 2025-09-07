#ifndef CHANNEL_INTELLIGENCE_H
#define CHANNEL_INTELLIGENCE_H

#include "notification_types.h"
#include "emergency_detector.h"
#include <stdbool.h>
#include <time.h>

// Channel score
typedef struct {
    notification_channel_t channel;
    double score;
    char reason[256];
    double effectiveness;
    time_t response_time_seconds;
} channel_score_t;

// Channel effectiveness data
typedef struct {
    notification_channel_t channel;
    double effectiveness_score;
    time_t average_response_time_seconds;
    double response_rate;
    int total_sent;
    int total_successful;
    time_t last_updated;
} channel_effectiveness_t;

// Channel intelligence configuration
typedef struct {
    bool channel_intelligence_enabled;
    bool learning_enabled;
    double confidence_threshold;
    int max_channel_effectiveness_entries;
    bool respect_user_preferences;
    bool respect_business_hours;
    bool respect_quiet_hours;
} channel_intelligence_config_t;

// Channel intelligence status
typedef struct {
    bool enabled;
    bool learning_enabled;
    int channel_effectiveness_count;
    int max_channel_effectiveness_entries;
    int total_selections;
    int intelligent_selections;
    double selection_accuracy;
} channel_intelligence_status_t;

// Channel intelligence structure
typedef struct {
    channel_intelligence_config_t config;
    
    // Channel effectiveness data
    channel_effectiveness_t* channel_effectiveness;
    int max_channel_effectiveness_entries;
    int channel_effectiveness_count;
    
    // Statistics
    int total_selections;
    int intelligent_selections;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} channel_intelligence_t;

// Initialize channel intelligence
int channel_intelligence_init(const channel_intelligence_config_t* config);

// Clean up channel intelligence
void channel_intelligence_cleanup(void);

// Select optimal channels for notification
int channel_intelligence_select_optimal_channels(notification_type_t alert_type,
                                                notification_priority_t priority,
                                                const system_state_t* system_state,
                                                const char* base_data_json,
                                                notification_channel_t* selected_channels,
                                                int max_channels);

// Score all available channels
int channel_intelligence_score_all_channels(notification_type_t alert_type,
                                           notification_priority_t priority,
                                           const system_state_t* system_state,
                                           const char* base_data_json,
                                           channel_score_t* channel_scores,
                                           int max_scores);

// Calculate channel score
void channel_intelligence_calculate_channel_score(notification_channel_t channel,
                                                 notification_type_t alert_type,
                                                 notification_priority_t priority,
                                                 const system_state_t* system_state,
                                                 const char* base_data_json,
                                                 channel_score_t* score);

// Update channel effectiveness data
int channel_intelligence_update_effectiveness(notification_channel_t channel,
                                             bool was_successful,
                                             time_t response_time_seconds);

// Get channel effectiveness
double channel_intelligence_get_channel_effectiveness(notification_channel_t channel);

// Get channel response time
time_t channel_intelligence_get_channel_response_time(notification_channel_t channel);

// Get channel intelligence status
void channel_intelligence_get_status(channel_intelligence_status_t* status);

// Get channel intelligence statistics
void channel_intelligence_get_stats(char* stats_json, size_t max_size);

// Check if channel intelligence is initialized
bool channel_intelligence_is_initialized(void);

// Get channel intelligence instance
channel_intelligence_t* channel_intelligence_get_instance(void);

#endif // CHANNEL_INTELLIGENCE_H
