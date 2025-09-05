#include "emergency_detector.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

// Global emergency detector instance
static emergency_detector_t g_emergency_detector;
static bool g_emergency_detector_initialized = false;

// Initialize emergency detector
static int emergency_detector_init(const emergency_detector_config_t* config) {
    if (g_emergency_detector_initialized) {
        return 0; // Already initialized
    }
    
    if (!config) {
        return -1;
    }
    
    memset(&g_emergency_detector, 0, sizeof(emergency_detector_t));
    
    // Copy configuration
    g_emergency_detector.config = *config;
    
    // Initialize mutex
    g_emergency_detector.mutex = malloc(sizeof(pthread_mutex_t));
    if (!g_emergency_detector.mutex) {
        return -1;
    }
    
    pthread_mutex_init(g_emergency_detector.mutex, NULL);
    
    // Initialize incident tracking
    g_emergency_detector.active_incidents = malloc(config->max_active_incidents * sizeof(active_incident_t));
    if (!g_emergency_detector.active_incidents) {
        pthread_mutex_destroy(g_emergency_detector.mutex);
        free(g_emergency_detector.mutex);
        return -1;
    }
    
    g_emergency_detector.max_active_incidents = config->max_active_incidents;
    g_emergency_detector.active_incidents_count = 0;
    
    // Initialize failure records
    g_emergency_detector.recent_failures = malloc(config->max_failure_records * sizeof(failure_record_t));
    if (!g_emergency_detector.recent_failures) {
        free(g_emergency_detector.active_incidents);
        pthread_mutex_destroy(g_emergency_detector.mutex);
        free(g_emergency_detector.mutex);
        return -1;
    }
    
    g_emergency_detector.max_failure_records = config->max_failure_records;
    g_emergency_detector.failure_records_count = 0;
    
    g_emergency_detector_initialized = true;
    return 0;
}

// Clean up emergency detector
static void emergency_detector_cleanup(void) {
    if (!g_emergency_detector_initialized) return;
    
    if (g_emergency_detector.mutex) {
        pthread_mutex_destroy(g_emergency_detector.mutex);
        free(g_emergency_detector.mutex);
    }
    
    if (g_emergency_detector.active_incidents) {
        free(g_emergency_detector.active_incidents);
    }
    
    if (g_emergency_detector.recent_failures) {
        free(g_emergency_detector.recent_failures);
    }
    
    g_emergency_detector.active_incidents = NULL;
    g_emergency_detector.recent_failures = NULL;
    g_emergency_detector.mutex = NULL;
    g_emergency_detector.active_incidents_count = 0;
    g_emergency_detector.max_active_incidents = 0;
    g_emergency_detector.failure_records_count = 0;
    g_emergency_detector.max_failure_records = 0;
    
    g_emergency_detector_initialized = false;
}

// Check system health emergency
static emergency_level_t check_system_health_emergency(const system_health_state_t* health) {
    if (!health || !g_emergency_detector_initialized) {
        return EMERGENCY_LEVEL_NONE;
    }
    
    const emergency_thresholds_t* thresholds = &g_emergency_detector.config.emergency_thresholds;
    
    // Critical system conditions
    if (health->cpu_usage >= thresholds->cpu_usage_emergency ||
        health->memory_usage >= thresholds->memory_usage_emergency ||
        health->temperature >= thresholds->temperature_emergency ||
        health->disk_usage >= thresholds->disk_usage_emergency) {
        return EMERGENCY_LEVEL_CRITICAL;
    }
    
    // High emergency conditions (80% of critical thresholds)
    if (health->cpu_usage >= thresholds->cpu_usage_emergency * 0.8 ||
        health->memory_usage >= thresholds->memory_usage_emergency * 0.8 ||
        health->temperature >= thresholds->temperature_emergency * 0.8 ||
        health->disk_usage >= thresholds->disk_usage_emergency * 0.8) {
        return EMERGENCY_LEVEL_HIGH;
    }
    
    // Medium emergency conditions (60% of critical thresholds)
    if (health->cpu_usage >= thresholds->cpu_usage_emergency * 0.6 ||
        health->memory_usage >= thresholds->memory_usage_emergency * 0.6 ||
        health->temperature >= thresholds->temperature_emergency * 0.6 ||
        health->disk_usage >= thresholds->disk_usage_emergency * 0.6) {
        return EMERGENCY_LEVEL_MEDIUM;
    }
    
    return EMERGENCY_LEVEL_NONE;
}

// Check network health emergency
static emergency_level_t check_network_health_emergency(const network_health_state_t* health) {
    if (!health || !g_emergency_detector_initialized) {
        return EMERGENCY_LEVEL_NONE;
    }
    
    const emergency_thresholds_t* thresholds = &g_emergency_detector.config.emergency_thresholds;
    
    // Critical network conditions
    if (!health->primary_interface_up && health->backup_interfaces_up == 0) {
        return EMERGENCY_LEVEL_CRITICAL; // Complete network failure
    }
    
    if (health->average_packet_loss >= thresholds->packet_loss_emergency ||
        health->average_latency >= thresholds->latency_emergency) {
        return EMERGENCY_LEVEL_CRITICAL;
    }
    
    // High emergency conditions
    if (!health->primary_interface_up ||
        health->backup_interfaces_up < health->total_interfaces / 2) {
        return EMERGENCY_LEVEL_HIGH;
    }
    
    if (health->average_packet_loss >= thresholds->packet_loss_emergency * 0.6 ||
        health->average_latency >= thresholds->latency_emergency * 0.6) {
        return EMERGENCY_LEVEL_HIGH;
    }
    
    // Medium emergency conditions
    if (health->average_packet_loss >= thresholds->packet_loss_emergency * 0.3 ||
        health->average_latency >= thresholds->latency_emergency * 0.3) {
        return EMERGENCY_LEVEL_MEDIUM;
    }
    
    return EMERGENCY_LEVEL_NONE;
}

// Check cascading failures
static emergency_level_t check_cascading_failures(void) {
    if (!g_emergency_detector_initialized) {
        return EMERGENCY_LEVEL_NONE;
    }
    
    const emergency_thresholds_t* thresholds = &g_emergency_detector.config.emergency_thresholds;
    time_t now = time(NULL);
    
    pthread_mutex_lock(g_emergency_detector.mutex);
    
    // Count recent high-severity incidents
    int recent_high_severity = 0;
    for (int i = 0; i < g_emergency_detector.active_incidents_count; i++) {
        active_incident_t* incident = &g_emergency_detector.active_incidents[i];
        if (incident->severity >= (int)EMERGENCY_LEVEL_HIGH &&
            (now - incident->start_time) < 600) { // 10 minutes
            recent_high_severity++;
        }
    }
    
    if (recent_high_severity >= thresholds->cascading_failure_count) {
        pthread_mutex_unlock(g_emergency_detector.mutex);
        return EMERGENCY_LEVEL_CRITICAL;
    }
    
    // Count failures in the last minute
    int recent_failures = 0;
    time_t cutoff = now - 60; // 1 minute ago
    for (int i = 0; i < g_emergency_detector.failure_records_count; i++) {
        failure_record_t* failure = &g_emergency_detector.recent_failures[i];
        if (failure->timestamp > cutoff) {
            recent_failures++;
        }
    }
    
    pthread_mutex_unlock(g_emergency_detector.mutex);
    
    double failure_rate = (double)recent_failures;
    if (failure_rate >= thresholds->failure_rate_emergency) {
        return EMERGENCY_LEVEL_HIGH;
    }
    
    if (failure_rate >= thresholds->failure_rate_emergency * 0.6) {
        return EMERGENCY_LEVEL_MEDIUM;
    }
    
    return EMERGENCY_LEVEL_NONE;
}

// Check alert-specific emergency conditions
static emergency_level_t check_alert_specific_emergency(notification_type_t alert_type, const char* details_json) {
    if (!g_emergency_detector_initialized || !details_json) {
        return EMERGENCY_LEVEL_NONE;
    }
    
    const emergency_thresholds_t* thresholds = &g_emergency_detector.config.emergency_thresholds;
    
    switch (alert_type) {
        case NOTIFICATION_TYPE_SYSTEM_HEALTH:
            // Parse temperature from JSON details
            if (strstr(details_json, "temperature")) {
                double temperature = 0.0;
                if (sscanf(details_json, "{\"temperature\":%lf", &temperature) == 1) {
                    if (temperature >= thresholds->temperature_emergency) {
                        return EMERGENCY_LEVEL_CRITICAL;
                    }
                    if (temperature >= thresholds->temperature_emergency * 0.8) {
                        return EMERGENCY_LEVEL_HIGH;
                    }
                }
            }
            break;
            
        case NOTIFICATION_TYPE_FAILOVER:
            // Check for multiple rapid failovers
            if (strstr(details_json, "recent_failover_count")) {
                int failover_count = 0;
                if (sscanf(details_json, "{\"recent_failover_count\":%d", &failover_count) == 1) {
                    if (failover_count >= 3) {
                        return EMERGENCY_LEVEL_CRITICAL;
                    }
                    if (failover_count >= 2) {
                        return EMERGENCY_LEVEL_HIGH;
                    }
                }
            }
            break;
            
        case NOTIFICATION_TYPE_DATA_LIMIT:
            // Check for critical data usage
            if (strstr(details_json, "usage_percentage")) {
                double usage_percentage = 0.0;
                if (sscanf(details_json, "{\"usage_percentage\":%lf", &usage_percentage) == 1) {
                    if (usage_percentage >= 95.0) {
                        return EMERGENCY_LEVEL_CRITICAL;
                    }
                    if (usage_percentage >= 90.0) {
                        return EMERGENCY_LEVEL_HIGH;
                    }
                }
            }
            break;
            
        default:
            break;
    }
    
    return EMERGENCY_LEVEL_NONE;
}

// Check temporal emergency patterns
static emergency_level_t check_temporal_emergency(const system_state_t* system_state) {
    if (!system_state || !g_emergency_detector_initialized) {
        return EMERGENCY_LEVEL_NONE;
    }
    
    const emergency_thresholds_t* thresholds = &g_emergency_detector.config.emergency_thresholds;
    time_t now = time(NULL);
    
    // Check for sustained high load
    if (system_state->system_health.cpu_usage > thresholds->cpu_usage_emergency * 0.7 &&
        (now - system_state->system_health.timestamp) > 300) { // 5 minutes
        return EMERGENCY_LEVEL_HIGH;
    }
    
    // Check for rapid state changes
    if (system_state->state_change_count > thresholds->rapid_state_change_threshold) {
        return EMERGENCY_LEVEL_MEDIUM;
    }
    
    return EMERGENCY_LEVEL_NONE;
}

// Detect emergency conditions
emergency_level_t emergency_detector_detect_emergency(const system_state_t* system_state,
                                                    notification_type_t alert_type,
                                                    const char* details_json) {
    if (!g_emergency_detector_initialized || !system_state) {
        return EMERGENCY_LEVEL_NONE;
    }
    
    if (!g_emergency_detector.config.emergency_detection_enabled) {
        return EMERGENCY_LEVEL_NONE;
    }
    
    emergency_level_t max_level = EMERGENCY_LEVEL_NONE;
    
    // Check system health emergencies
    emergency_level_t system_level = check_system_health_emergency(&system_state->system_health);
    if (system_level > max_level) {
        max_level = system_level;
    }
    
    // Check network health emergencies
    emergency_level_t network_level = check_network_health_emergency(&system_state->network_health);
    if (network_level > max_level) {
        max_level = network_level;
    }
    
    // Check cascading failure patterns
    emergency_level_t cascade_level = check_cascading_failures();
    if (cascade_level > max_level) {
        max_level = cascade_level;
    }
    
    // Check alert-specific emergency conditions
    emergency_level_t alert_level = check_alert_specific_emergency(alert_type, details_json);
    if (alert_level > max_level) {
        max_level = alert_level;
    }
    
    // Check temporal emergency patterns
    emergency_level_t temporal_level = check_temporal_emergency(system_state);
    if (temporal_level > max_level) {
        max_level = temporal_level;
    }
    
    // Log emergency detection
    if (max_level > EMERGENCY_LEVEL_NONE) {
        // Note: In a real implementation, this would use the logging system
        printf("EMERGENCY: Level %d detected for alert type %d\n", max_level, alert_type);
    }
    
    return max_level;
}

// Add active incident
static int emergency_detector_add_incident(const active_incident_t* incident) {
    if (!g_emergency_detector_initialized || !incident) {
        return -1;
    }
    
    pthread_mutex_lock(g_emergency_detector.mutex);
    
    if (g_emergency_detector.active_incidents_count >= g_emergency_detector.max_active_incidents) {
        // Remove oldest incident to make room
        for (int i = 0; i < g_emergency_detector.max_active_incidents - 1; i++) {
            g_emergency_detector.active_incidents[i] = g_emergency_detector.active_incidents[i + 1];
        }
        g_emergency_detector.active_incidents_count--;
    }
    
    int index = g_emergency_detector.active_incidents_count;
    g_emergency_detector.active_incidents[index] = *incident;
    g_emergency_detector.active_incidents_count++;
    
    pthread_mutex_unlock(g_emergency_detector.mutex);
    return 0;
}

// Add failure record
static int emergency_detector_add_failure(const failure_record_t* failure) {
    if (!g_emergency_detector_initialized || !failure) {
        return -1;
    }
    
    pthread_mutex_lock(g_emergency_detector.mutex);
    
    if (g_emergency_detector.failure_records_count >= g_emergency_detector.max_failure_records) {
        // Remove oldest failure to make room
        for (int i = 0; i < g_emergency_detector.max_failure_records - 1; i++) {
            g_emergency_detector.recent_failures[i] = g_emergency_detector.recent_failures[i + 1];
        }
        g_emergency_detector.failure_records_count--;
    }
    
    int index = g_emergency_detector.failure_records_count;
    g_emergency_detector.recent_failures[index] = *failure;
    g_emergency_detector.failure_records_count++;
    
    pthread_mutex_unlock(g_emergency_detector.mutex);
    return 0;
}

// Get emergency detector status
static void emergency_detector_get_status(emergency_detector_status_t* status) {
    if (!status || !g_emergency_detector_initialized) return;
    
    pthread_mutex_lock(g_emergency_detector.mutex);
    
    status->enabled = g_emergency_detector.config.emergency_detection_enabled;
    status->active_incidents_count = g_emergency_detector.active_incidents_count;
    status->max_active_incidents = g_emergency_detector.max_active_incidents;
    status->failure_records_count = g_emergency_detector.failure_records_count;
    status->max_failure_records = g_emergency_detector.max_failure_records;
    
    pthread_mutex_unlock(g_emergency_detector.mutex);
}

// Check if emergency detector is initialized
static bool emergency_detector_is_initialized(void) {
    return g_emergency_detector_initialized;
}

// Get emergency detector instance
static emergency_detector_t* emergency_detector_get_instance(void) {
    return g_emergency_detector_initialized ? &g_emergency_detector : NULL;
}
