#ifndef INTELLIGENCE_ENGINE_H
#define INTELLIGENCE_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "notification_types.h"

// Intelligence engine for smart notification decisions

// Intelligence configuration
typedef struct {
    bool enable_machine_learning;
    bool enable_pattern_recognition;
    bool enable_predictive_analytics;
    int learning_window_days;
    float confidence_threshold;
    int max_patterns;
} intelligence_config_t;

// Pattern structure
typedef struct {
    char pattern_id[64];
    char description[256];
    int frequency;
    float confidence;
    time_t first_seen;
    time_t last_seen;
    char conditions[512];
} notification_pattern_t;

// Function declarations
int intelligence_engine_init(const intelligence_config_t *config);
void intelligence_engine_cleanup(void);
int intelligence_engine_analyze_notification(const notification_t *notification);
int intelligence_engine_learn_from_delivery(const char *notification_id, bool success);
int intelligence_engine_get_patterns(notification_pattern_t *patterns, int max_patterns);
int intelligence_engine_predict_delivery_success(const notification_t *notification, float *confidence);

#endif // INTELLIGENCE_ENGINE_H
