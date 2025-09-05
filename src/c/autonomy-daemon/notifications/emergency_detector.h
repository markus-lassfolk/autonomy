#ifndef EMERGENCY_DETECTOR_H
#define EMERGENCY_DETECTOR_H

#include "notification_types.h"
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

// Emergency levels
typedef enum {
    EMERGENCY_LEVEL_NONE = 0,
    EMERGENCY_LEVEL_LOW,
    EMERGENCY_LEVEL_MEDIUM,
    EMERGENCY_LEVEL_HIGH,
    EMERGENCY_LEVEL_CRITICAL
} emergency_level_t;

// System health state
typedef struct {
    double cpu_usage;
    double memory_usage;
    double temperature;
    double disk_usage;
    time_t timestamp;
} system_health_state_t;

// Network health state
typedef struct {
    bool primary_interface_up;
    int backup_interfaces_up;
    int total_interfaces;
    double average_packet_loss;
    double average_latency;
} network_health_state_t;

// Active incident
typedef struct {
    char id[64];
    char description[256];
    int severity;
    time_t start_time;
    time_t last_updated;
} active_incident_t;

// Failure record
typedef struct {
    char component[128];
    char description[256];
    time_t timestamp;
    int severity;
} failure_record_t;

// System state
typedef struct {
    system_health_state_t system_health;
    network_health_state_t network_health;
    int state_change_count;
} system_state_t;

// Emergency thresholds
typedef struct {
    double cpu_usage_emergency;
    double memory_usage_emergency;
    double temperature_emergency;
    double disk_usage_emergency;
    double packet_loss_emergency;
    double latency_emergency;
    int cascading_failure_count;
    double failure_rate_emergency;
    int rapid_state_change_threshold;
} emergency_thresholds_t;

// Emergency detector configuration
typedef struct {
    bool emergency_detection_enabled;
    emergency_thresholds_t emergency_thresholds;
    int max_active_incidents;
    int max_failure_records;
} emergency_detector_config_t;

// Emergency detector status
typedef struct {
    bool enabled;
    int active_incidents_count;
    int max_active_incidents;
    int failure_records_count;
    int max_failure_records;
} emergency_detector_status_t;

// Emergency detector structure
typedef struct {
    emergency_detector_config_t config;
    
    // Incident tracking
    active_incident_t* active_incidents;
    int max_active_incidents;
    int active_incidents_count;
    
    // Failure records
    failure_record_t* recent_failures;
    int max_failure_records;
    int failure_records_count;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} emergency_detector_t;

// Initialize emergency detector
int emergency_detector_init(const emergency_detector_config_t* config);

// Clean up emergency detector
void emergency_detector_cleanup(void);

// Detect emergency conditions
emergency_level_t emergency_detector_detect_emergency(const system_state_t* system_state,
                                                    notification_type_t alert_type,
                                                    const char* details_json);

// Add active incident
int emergency_detector_add_incident(const active_incident_t* incident);

// Add failure record
int emergency_detector_add_failure(const failure_record_t* failure);

// Get emergency detector status
void emergency_detector_get_status(emergency_detector_status_t* status);

// Check if emergency detector is initialized
bool emergency_detector_is_initialized(void);

// Get emergency detector instance
emergency_detector_t* emergency_detector_get_instance(void);

#endif // EMERGENCY_DETECTOR_H