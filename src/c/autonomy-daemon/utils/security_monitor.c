#include "security_monitor.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdbool.h>
#include <math.h>
#include <sys/socket.h>

// Global security monitor instance
static security_monitor_t g_security_monitor;
static bool g_security_monitor_initialized = false;

// Forward declarations
int perform_file_integrity_check(security_scan_result_t* result);
int perform_network_security_check(security_scan_result_t* result);
int perform_access_control_check(security_scan_result_t* result);
int perform_configuration_check(security_scan_result_t* result);
static int perform_threat_detection(security_scan_result_t* result);
void update_security_events(const char* event_type, const char* description, 
                                  const char* source, const char* target, threat_level_t level);
static char* generate_event_id(void);

// Initialize security monitor
int security_monitor_init(const security_monitor_config_t* config) {
    if (g_security_monitor_initialized) {
        return 0; // Already initialized
    }
    
    memset(&g_security_monitor, 0, sizeof(security_monitor_t));
    
    // Set configuration
    if (config) {
        g_security_monitor.config = *config;
    } else {
        // Default configuration
        g_security_monitor.config.enabled = true;
        g_security_monitor.config.scan_interval_seconds = 300; // 5 minutes
        g_security_monitor.config.enable_file_integrity = true;
        g_security_monitor.config.enable_network_monitoring = true;
        g_security_monitor.config.enable_access_control = true;
        g_security_monitor.config.enable_configuration_check = true;
        g_security_monitor.config.enable_threat_detection = true;
    }
    
    // Initialize mutex
    g_security_monitor.mutex = malloc(sizeof(pthread_mutex_t));
    if (!g_security_monitor.mutex) {
        return -1;
    }
    
    pthread_mutex_init(g_security_monitor.mutex, NULL);
    
    // Initialize security events
    g_security_monitor.event_count = 0;
    g_security_monitor.event_index = 0;
    
    g_security_monitor_initialized = true;
    return 0;
}

// Clean up security monitor
void security_monitor_cleanup(void) {
    if (!g_security_monitor_initialized) return;
    
    if (g_security_monitor.mutex) {
        pthread_mutex_destroy(g_security_monitor.mutex);
        free(g_security_monitor.mutex);
    }
    
    g_security_monitor.mutex = NULL;
    g_security_monitor_initialized = false;
}

// Perform security scan
static int security_monitor_perform_scan(void) {
    if (!g_security_monitor_initialized || !g_security_monitor.config.enabled) {
        return -1;
    }
    
    pthread_mutex_lock(g_security_monitor.mutex);
    
    security_scan_result_t result;
    memset(&result, 0, sizeof(security_scan_result_t));
    
    int vulnerabilities_found = 0;
    int critical_vulnerabilities = 0;
    
    // Perform various security checks
    if (g_security_monitor.config.enable_file_integrity) {
        if (perform_file_integrity_check(&result) == 0) {
            result.file_integrity_check = true;
        }
    }
    
    if (g_security_monitor.config.enable_network_monitoring) {
        if (perform_network_security_check(&result) == 0) {
            result.network_security_check = true;
        }
    }
    
    if (g_security_monitor.config.enable_access_control) {
        if (perform_access_control_check(&result) == 0) {
            result.access_control_check = true;
        }
    }
    
    if (g_security_monitor.config.enable_configuration_check) {
        if (perform_configuration_check(&result) == 0) {
            result.configuration_check = true;
        }
    }
    
    if (g_security_monitor.config.enable_threat_detection) {
        if (perform_threat_detection(&result) == 0) {
            // Count vulnerabilities
            for (int i = 0; i < g_security_monitor.event_count; i++) {
                if (g_security_monitor.security_events[i].threat_level == THREAT_LEVEL_CRITICAL) {
                    critical_vulnerabilities++;
                } else if (g_security_monitor.security_events[i].threat_level >= THREAT_LEVEL_LOW) {
                    vulnerabilities_found++;
                }
            }
        }
    }
    
    // Update scan results
    result.vulnerabilities_found = vulnerabilities_found;
    result.critical_vulnerabilities = critical_vulnerabilities;
    result.scan_timestamp = time(NULL);
    
    // Generate scan summary
    snprintf(result.scan_summary, sizeof(result.scan_summary),
             "Security scan completed: %d vulnerabilities found (%d critical)",
             vulnerabilities_found, critical_vulnerabilities);
    
    // Update last scan results
    g_security_monitor.last_scan = result;
    
    // Update statistics
    g_security_monitor.last_scan_time = time(NULL);
    g_security_monitor.scan_count++;
    g_security_monitor.threat_detections = vulnerabilities_found;
    g_security_monitor.critical_threats = critical_vulnerabilities;
    
    pthread_mutex_unlock(g_security_monitor.mutex);
    
    return 0;
}

// Get security scan results
static int security_monitor_get_scan_results(security_scan_result_t* results) {
    if (!g_security_monitor_initialized || !results) {
        return -1;
    }
    
    pthread_mutex_lock(g_security_monitor.mutex);
    *results = g_security_monitor.last_scan;
    pthread_mutex_unlock(g_security_monitor.mutex);
    
    return 0;
}

// Get security events
static int security_monitor_get_events(security_event_t* events, int max_events) {
    if (!g_security_monitor_initialized || !events || max_events <= 0) {
        return -1;
    }
    
    pthread_mutex_lock(g_security_monitor.mutex);
    
    int count = 0;
    int index = g_security_monitor.event_index;
    
    for (int i = 0; i < g_security_monitor.event_count && count < max_events; i++) {
        int event_index = (index - i + 100) % 100;
        if (g_security_monitor.security_events[event_index].timestamp > 0) {
            events[count] = g_security_monitor.security_events[event_index];
            count++;
        }
    }
    
    pthread_mutex_unlock(g_security_monitor.mutex);
    
    return count;
}

// Acknowledge security event
static int security_monitor_acknowledge_event(const char* event_id) {
    if (!g_security_monitor_initialized || !event_id) {
        return -1;
    }
    
    pthread_mutex_lock(g_security_monitor.mutex);
    
    for (int i = 0; i < g_security_monitor.event_count; i++) {
        if (strcmp(g_security_monitor.security_events[i].event_id, event_id) == 0) {
            g_security_monitor.security_events[i].acknowledged = true;
            pthread_mutex_unlock(g_security_monitor.mutex);
            return 0;
        }
    }
    
    pthread_mutex_unlock(g_security_monitor.mutex);
    return -1;
}

// Report security threat
int security_monitor_report_threat(threat_level_t level, const char* description, 
                                  const char* source, const char* target) {
    if (!g_security_monitor_initialized || !description) {
        return -1;
    }
    
    update_security_events("threat_detected", description, source, target, level);
    
    return 0;
}

// Perform file integrity check
int perform_file_integrity_check(security_scan_result_t* result) {
    if (!result) return -1;
    
    // This is a simplified file integrity check
    // In a real system, you'd check file hashes, permissions, etc.
    
    // Check critical system files
    const char* critical_files[] = {
        "/etc/passwd",
        "/etc/shadow",
        "/etc/sudoers",
        "/etc/ssh/sshd_config"
    };
    
    int critical_file_count = sizeof(critical_files) / sizeof(critical_files[0]);
    
    for (int i = 0; i < critical_file_count; i++) {
        struct stat st;
        if (stat(critical_files[i], &st) == 0) {
            // Check file permissions
            if ((st.st_mode & S_IRWXU) != S_IRUSR) {
                // Report security issue
                update_security_events("file_permission", 
                                     "Critical file has insecure permissions",
                                     critical_files[i], "system", THREAT_LEVEL_HIGH);
            }
        }
    }
    
    return 0;
}

// Perform network security check
int perform_network_security_check(security_scan_result_t* result) {
    if (!result) return -1;
    
    // This is a simplified network security check
    // In a real system, you'd check open ports, network connections, etc.
    
    // Check for common security issues
    // Simulate finding some issues
    update_security_events("network_security", 
                          "Unusual network activity detected",
                          "network", "system", THREAT_LEVEL_MEDIUM);
    
    return 0;
}

// Perform access control check
int perform_access_control_check(security_scan_result_t* result) {
    if (!result) return -1;
    
    // This is a simplified access control check
    // In a real system, you'd check user accounts, permissions, etc.
    
    // Check for unauthorized access attempts
    // Simulate finding some issues
    update_security_events("access_control", 
                          "Multiple failed login attempts detected",
                          "authentication", "system", THREAT_LEVEL_MEDIUM);
    
    return 0;
}

// Perform configuration check
int perform_configuration_check(security_scan_result_t* result) {
    if (!result) return -1;
    
    // This is a simplified configuration check
    // In a real system, you'd check configuration files, settings, etc.
    
    // Check for insecure configurations
    // Simulate finding some issues
    update_security_events("configuration", 
                          "Insecure configuration detected",
                          "config", "system", THREAT_LEVEL_LOW);
    
    return 0;
}

// Perform threat detection
static int perform_threat_detection(security_scan_result_t* result) {
    if (!result) return -1;
    
    // This is a simplified threat detection
    // In a real system, you'd use IDS/IPS, behavioral analysis, etc.
    
    // Simulate threat detection
    update_security_events("threat_detection", 
                          "Suspicious activity pattern detected",
                          "behavioral_analysis", "system", THREAT_LEVEL_HIGH);
    
    return 0;
}

// Update security events
void update_security_events(const char* event_type, const char* description, 
                                  const char* source, const char* target, threat_level_t level) {
    if (g_security_monitor.event_count >= 100) return;
    
    security_event_t* event = &g_security_monitor.security_events[g_security_monitor.event_index];
    
    // Generate unique event ID
    char* event_id = generate_event_id();
    strcpy(event->event_id, event_id);
    free(event_id);
    
    // Set event details
    event->threat_level = level;
    strcpy(event->event_type, event_type);
    strcpy(event->description, description);
    strcpy(event->source, source ? source : "unknown");
    strcpy(event->target, target ? target : "unknown");
    event->timestamp = time(NULL);
    event->acknowledged = false;
    
    // Set mitigation based on threat level
    switch (level) {
        case THREAT_LEVEL_CRITICAL:
            strcpy(event->mitigation, "Immediate action required - isolate system");
            break;
        case THREAT_LEVEL_HIGH:
            strcpy(event->mitigation, "Investigate and remediate within 1 hour");
            break;
        case THREAT_LEVEL_MEDIUM:
            strcpy(event->mitigation, "Review and address within 24 hours");
            break;
        case THREAT_LEVEL_LOW:
            strcpy(event->mitigation, "Monitor and address during next maintenance");
            break;
    }
    
    // Update event index
    g_security_monitor.event_index = (g_security_monitor.event_index + 1) % 100;
    if (g_security_monitor.event_count < 100) {
        g_security_monitor.event_count++;
    }
}

// Generate unique event ID
static char* generate_event_id(void) {
    char* event_id = malloc(64);
    if (!event_id) return NULL;
    
    time_t now = time(NULL);
    snprintf(event_id, 64, "SEC_%ld_%d", now, rand() % 10000);
    
    return event_id;
}

// Get security monitor status
void security_monitor_get_status(security_monitor_t* status) {
    if (!status || !g_security_monitor_initialized) return;
    
    pthread_mutex_lock(g_security_monitor.mutex);
    *status = g_security_monitor;
    pthread_mutex_unlock(g_security_monitor.mutex);
}

// Check if security monitor is initialized
bool security_monitor_is_initialized(void) {
    return g_security_monitor_initialized;
}

// Get security monitor instance
static security_monitor_t* security_monitor_get_instance(void) {
    return g_security_monitor_initialized ? &g_security_monitor : NULL;
}
