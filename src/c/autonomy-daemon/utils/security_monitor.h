#ifndef SECURITY_MONITOR_H
#define SECURITY_MONITOR_H

#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>

// Security threat levels
typedef enum {
    THREAT_LEVEL_LOW,
    THREAT_LEVEL_MEDIUM,
    THREAT_LEVEL_HIGH,
    THREAT_LEVEL_CRITICAL
} threat_level_t;

// Security event
typedef struct {
    char event_id[64];
    threat_level_t threat_level;
    char event_type[64];
    char description[256];
    char source[128];
    char target[128];
    time_t timestamp;
    bool acknowledged;
    char mitigation[256];
} security_event_t;

// Security scan result
typedef struct {
    bool file_integrity_check;
    bool network_security_check;
    bool access_control_check;
    bool configuration_check;
    int vulnerabilities_found;
    int critical_vulnerabilities;
    char scan_summary[512];
    time_t scan_timestamp;
} security_scan_result_t;

// Security monitor configuration
typedef struct {
    bool enabled;
    int scan_interval_seconds;
    bool enable_file_integrity;
    bool enable_network_monitoring;
    bool enable_access_control;
    bool enable_configuration_check;
    bool enable_threat_detection;
} security_monitor_config_t;

// Security monitor structure
typedef struct {
    security_monitor_config_t config;
    
    // Current scan results
    security_scan_result_t last_scan;
    
    // Security events
    security_event_t security_events[100];
    int event_count;
    int event_index;
    
    // Statistics
    time_t last_scan_time;
    int scan_count;
    int threat_detections;
    int critical_threats;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} security_monitor_t;

// Initialize security monitor
int security_monitor_init(const security_monitor_config_t* config);

// Clean up security monitor
void security_monitor_cleanup(void);

// Perform security scan
int security_monitor_perform_scan(void);

// Get security scan results
int security_monitor_get_scan_results(security_scan_result_t* results);

// Get security events
int security_monitor_get_events(security_event_t* events, int max_events);

// Acknowledge security event
int security_monitor_acknowledge_event(const char* event_id);

// Report security threat
int security_monitor_report_threat(threat_level_t level, const char* description, 
                                  const char* source, const char* target);

// Get security monitor status
void security_monitor_get_status(security_monitor_t* status);

// Check if security monitor is initialized
bool security_monitor_is_initialized(void);

// Get security monitor instance
security_monitor_t* security_monitor_get_instance(void);

#endif // SECURITY_MONITOR_H
