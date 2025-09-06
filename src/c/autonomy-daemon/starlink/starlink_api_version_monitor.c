#include "starlink_api_version_monitor.h"
#include "../core/types.h"
#include "../starlink/starlink_comprehensive.h"
#include "../notifications/notifications_comprehensive.h"
#include "../utils/logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <json-c/json.h>
#include <curl/curl.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>

// Global API version monitor
starlink_api_version_monitor_t g_api_version_monitor = {0};
static bool g_api_version_monitor_initialized = false;

// Change severity strings
static const char* CHANGE_SEVERITY_STRINGS[] = {
    "minor", "moderate", "major", "unknown"
};

// API endpoint strings
static const char* API_ENDPOINT_STRINGS[] = {
    "get_status", "get_location", "get_diagnostics", "get_history"
};

// Known version patterns for change detection
static const char* KNOWN_VERSION_PATTERNS[] = {
    "2023.", "2024.", "2025.", // Year-based patterns
    ".mr", ".rc", ".beta"      // Build type patterns
};

// Forward declarations
static void* monitor_thread_worker(void* arg);
static int extract_version_from_starlink_response(const char* json_response, starlink_api_version_t* version);
static int detect_version_change(const starlink_api_version_t* new_version);
static int send_version_change_notification(const starlink_api_version_change_t* change);
int validate_api_after_change(const starlink_api_version_change_t* change);
static int save_version_to_storage(const starlink_api_version_t* version);
static int load_version_from_storage(starlink_api_version_t* version);

// Initialize Starlink API version monitor
int starlink_api_version_monitor_init(const starlink_api_version_monitor_config_t* config) {
    if (g_api_version_monitor_initialized) {
        LOGX_WARN_MSG("Starlink API version monitor already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    if (!config) {
        LOGX_ERROR_MSG("API version monitor config is NULL");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    memset(&g_api_version_monitor, 0, sizeof(starlink_api_version_monitor_t));
    g_api_version_monitor.config = *config;
    
    // Initialize mutex
    if (pthread_mutex_init(&g_api_version_monitor.mutex, NULL) != 0) {
        LOGX_ERROR_MSG("Failed to initialize API version monitor mutex");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Allocate memory for version history
    g_api_version_monitor.version_history = calloc(config->max_version_history,
                                                  sizeof(starlink_api_version_t));
    if (!g_api_version_monitor.version_history) {
        LOGX_ERROR_MSG("Failed to allocate memory for version history");
        pthread_mutex_destroy(&g_api_version_monitor.mutex);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Allocate memory for change records
    g_api_version_monitor.change_records = calloc(config->max_change_records,
                                                 sizeof(starlink_api_version_change_t));
    if (!g_api_version_monitor.change_records) {
        LOGX_ERROR_MSG("Failed to allocate memory for change records");
        free(g_api_version_monitor.version_history);
        pthread_mutex_destroy(&g_api_version_monitor.mutex);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Initialize statistics
    g_api_version_monitor.stats.stats_start_time = time(NULL);
    
    // Load previous version from storage if available
    if (load_version_from_storage(&g_api_version_monitor.current_version) == AUTONOMY_SUCCESS) {
        g_api_version_monitor.current_version_valid = true;
        LOGX_INFO_MSG("Loaded previous Starlink API version from storage",
                 "version", g_api_version_monitor.current_version.software_version);
    }
    
    // Start monitoring thread if enabled
    if (config->enabled) {
        g_api_version_monitor.thread_running = true;
        
        if (pthread_create(&g_api_version_monitor.monitor_thread, NULL, 
                          monitor_thread_worker, NULL) != 0) {
            LOGX_ERROR_MSG("Failed to create API version monitor thread");
            free(g_api_version_monitor.version_history);
            free(g_api_version_monitor.change_records);
            pthread_mutex_destroy(&g_api_version_monitor.mutex);
            return AUTONOMY_ERROR_SYSTEM;
        }
    }
    
    g_api_version_monitor_initialized = true;
    
    LOGX_INFO_MSG("Starlink API version monitor initialized",
              "enabled", config->enabled,
              "check_interval_s", config->check_interval_s,
              "notify_minor", config->notify_on_minor_changes,
              "notify_major", config->notify_on_major_changes);
    
    return AUTONOMY_SUCCESS;
}

// Cleanup API version monitor
void starlink_api_version_monitor_cleanup(void) {
    if (!g_api_version_monitor_initialized) return;
    
    pthread_mutex_lock(&g_api_version_monitor.mutex);
    
    // Stop monitoring thread
    g_api_version_monitor.thread_running = false;
    
    if (g_api_version_monitor.config.enabled) {
        pthread_cancel(g_api_version_monitor.monitor_thread);
        pthread_join(g_api_version_monitor.monitor_thread, NULL);
    }
    
    // Save current version to storage
    if (g_api_version_monitor.current_version_valid) {
        save_version_to_storage(&g_api_version_monitor.current_version);
    }
    
    // Free allocated memory
    free(g_api_version_monitor.version_history);
    free(g_api_version_monitor.change_records);
    
    pthread_mutex_unlock(&g_api_version_monitor.mutex);
    pthread_mutex_destroy(&g_api_version_monitor.mutex);
    
    g_api_version_monitor_initialized = false;
    
    LOGX_INFO_MSG("Starlink API version monitor cleaned up");
}

// Check for API version changes
int starlink_api_version_monitor_check_version(void) {
    if (!g_api_version_monitor_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_api_version_monitor.mutex);
    
    time_t start_time = time(NULL);
    
    // Get current Starlink status to extract version information
    starlink_comprehensive_status_t status;
    if (starlink_comprehensive_collect_all(&status) != AUTONOMY_SUCCESS) {
        LOGX_WARN_MSG("Failed to collect Starlink status for version check");
        pthread_mutex_unlock(&g_api_version_monitor.mutex);
        return AUTONOMY_ERROR_API_FAILED;
    }
    
    // Extract version information from device info
    starlink_api_version_t detected_version = {0};
    strncpy(detected_version.software_version, status.device_info.software_version,
            sizeof(detected_version.software_version) - 1);
    strncpy(detected_version.hardware_version, status.device_info.hardware_version,
            sizeof(detected_version.hardware_version) - 1);
    strncpy(detected_version.software_part_number, status.device_info.software_part_number,
            sizeof(detected_version.software_part_number) - 1);
    detected_version.generation_number = status.device_info.generation_number;
    detected_version.last_seen = time(NULL);
    
    // Parse version components
    if (starlink_parse_software_version(detected_version.software_version, &detected_version) != AUTONOMY_SUCCESS) {
        LOGX_WARN_MSG("Failed to parse Starlink software version",
                 "version_string", detected_version.software_version);
    }
    
    // Check for version change
    bool version_changed = false;
    if (g_api_version_monitor.current_version_valid) {
        if (strcmp(g_api_version_monitor.current_version.software_version, 
                  detected_version.software_version) != 0) {
            version_changed = true;
            LOGX_INFO_MSG("Starlink API version change detected",
                     "old_version", g_api_version_monitor.current_version.software_version,
                     "new_version", detected_version.software_version);
        }
    } else {
        // First time detection
        LOGX_INFO_MSG("Starlink API version detected for first time",
                 "version", detected_version.software_version);
        detected_version.first_detected = time(NULL);
    }
    
    // Update current version
    if (version_changed || !g_api_version_monitor.current_version_valid) {
        if (!g_api_version_monitor.current_version_valid) {
            detected_version.first_detected = time(NULL);
        }
        
        // Add previous version to history if valid
        if (g_api_version_monitor.current_version_valid) {
            g_api_version_monitor.current_version.is_current = false;
            
            // Add to history
            if (g_api_version_monitor.version_history_count < g_api_version_monitor.config.max_version_history) {
                g_api_version_monitor.version_history[g_api_version_monitor.version_history_count] = 
                    g_api_version_monitor.current_version;
                g_api_version_monitor.version_history_count++;
            } else {
                // Circular buffer
                g_api_version_monitor.version_history[g_api_version_monitor.version_history_index] = 
                    g_api_version_monitor.current_version;
                g_api_version_monitor.version_history_index = 
                    (g_api_version_monitor.version_history_index + 1) % g_api_version_monitor.config.max_version_history;
            }
        }
        
        g_api_version_monitor.current_version = detected_version;
        g_api_version_monitor.current_version.is_current = true;
        g_api_version_monitor.current_version_valid = true;
        
        // Process version change if this wasn't the first detection
        if (version_changed) {
            detect_version_change(&detected_version);
        }
        
        // Save to storage
        save_version_to_storage(&detected_version);
    }
    
    // Update statistics
    g_api_version_monitor.stats.total_version_checks++;
    g_api_version_monitor.stats.last_version_check = time(NULL);
    
    double check_time_ms = difftime(time(NULL), start_time) * 1000.0;
    g_api_version_monitor.stats.average_check_time_ms = 
        (g_api_version_monitor.stats.average_check_time_ms * 
         (g_api_version_monitor.stats.total_version_checks - 1) + 
         check_time_ms) / g_api_version_monitor.stats.total_version_checks;
    
    pthread_mutex_unlock(&g_api_version_monitor.mutex);
    
    LOGX_DEBUG_MSG("Starlink API version check completed",
              "version", detected_version.software_version,
              "changed", version_changed,
              "check_time_ms", check_time_ms);
    
    return AUTONOMY_SUCCESS;
}

// Parse Starlink software version string
int starlink_parse_software_version(const char* version_str, starlink_api_version_t* version) {
    if (!version_str || !version) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Initialize version components
    version->major_version = 0;
    version->minor_version = 0;
    version->patch_version = 0;
    memset(version->build_identifier, 0, sizeof(version->build_identifier));
    
    // Parse version string (e.g., "2023.26.0.mr7526")
    char* version_copy = strdup(version_str);
    if (!version_copy) {
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    char* token = strtok(version_copy, ".");
    if (token) {
        version->major_version = atoi(token);
        
        token = strtok(NULL, ".");
        if (token) {
            version->minor_version = atoi(token);
            
            token = strtok(NULL, ".");
            if (token) {
                // Check if this token contains build identifier
                char* build_start = strchr(token, 'm');
                if (build_start) {
                    *build_start = '\0';
                    version->patch_version = atoi(token);
                    strncpy(version->build_identifier, build_start + 1, sizeof(version->build_identifier) - 1);
                    version->build_identifier[sizeof(version->build_identifier) - 1] = '\0';
                } else {
                    version->patch_version = atoi(token);
                    
                    // Check for build identifier in next token
                    token = strtok(NULL, ".");
                    if (token) {
                        strncpy(version->build_identifier, token, sizeof(version->build_identifier) - 1);
                        version->build_identifier[sizeof(version->build_identifier) - 1] = '\0';
                    }
                }
            }
        }
    }
    
    free(version_copy);
    
    LOGX_DEBUG_MSG("Parsed Starlink version",
              "version_string", version_str,
              "major", version->major_version,
              "minor", version->minor_version,
              "patch", version->patch_version,
              "build", version->build_identifier);
    
    return AUTONOMY_SUCCESS;
}

// Detect and process version change
static int detect_version_change(const starlink_api_version_t* new_version) {
    if (!new_version || !g_api_version_monitor.current_version_valid) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Create change record
    starlink_api_version_change_t* change = 
        &g_api_version_monitor.change_records[g_api_version_monitor.change_records_index];
    
    memset(change, 0, sizeof(starlink_api_version_change_t));
    
    // Generate unique change ID
    snprintf(change->change_id, sizeof(change->change_id), 
             "api_change_%ld_%d", time(NULL), g_api_version_monitor.change_records_count);
    
    change->detected_at = time(NULL);
    change->endpoint = STARLINK_API_ENDPOINT_GET_STATUS; // Detected via get_status
    change->old_version = g_api_version_monitor.current_version;
    change->new_version = *new_version;
    
    // Determine change severity
    change->severity = starlink_determine_change_severity(&change->old_version, &change->new_version);
    
    // Generate impact assessment
    starlink_generate_impact_assessment(change, change->impact_assessment);
    starlink_generate_recommended_actions(change, change->recommended_actions);
    
    // Update change records tracking
    if (g_api_version_monitor.change_records_count < g_api_version_monitor.config.max_change_records) {
        g_api_version_monitor.change_records_count++;
    }
    g_api_version_monitor.change_records_index = 
        (g_api_version_monitor.change_records_index + 1) % g_api_version_monitor.config.max_change_records;
    
    // Update statistics
    g_api_version_monitor.stats.version_changes_detected++;
    g_api_version_monitor.stats.last_version_change = time(NULL);
    
    switch (change->severity) {
        case API_VERSION_CHANGE_MINOR:
            g_api_version_monitor.stats.minor_changes++;
            break;
        case API_VERSION_CHANGE_MODERATE:
            g_api_version_monitor.stats.moderate_changes++;
            break;
        case API_VERSION_CHANGE_MAJOR:
            g_api_version_monitor.stats.major_changes++;
            break;
        default:
            g_api_version_monitor.stats.unknown_changes++;
            break;
    }
    
    LOGX_WARN_MSG("Starlink API version change detected",
             "change_id", change->change_id,
             "old_version", change->old_version.software_version,
             "new_version", change->new_version.software_version,
             "severity", starlink_api_version_change_severity_to_string(change->severity));
    
    // Send notification if configured
    bool should_notify = false;
    switch (change->severity) {
        case API_VERSION_CHANGE_MINOR:
            should_notify = g_api_version_monitor.config.notify_on_minor_changes;
            break;
        case API_VERSION_CHANGE_MODERATE:
            should_notify = g_api_version_monitor.config.notify_on_moderate_changes;
            break;
        case API_VERSION_CHANGE_MAJOR:
            should_notify = g_api_version_monitor.config.notify_on_major_changes;
            break;
        case API_VERSION_CHANGE_UNKNOWN:
            should_notify = g_api_version_monitor.config.notify_on_unknown_changes;
            break;
    }
    
    if (should_notify && g_api_version_monitor.config.send_immediate_notifications) {
        if (send_version_change_notification(change) == AUTONOMY_SUCCESS) {
            change->notification_sent = true;
            g_api_version_monitor.stats.notifications_sent++;
        }
    }
    
    // Perform API validation if configured
    if (g_api_version_monitor.config.perform_validation_on_change) {
        if (validate_api_after_change(change) == AUTONOMY_SUCCESS) {
            change->api_still_functional = true;
        } else {
            change->api_still_functional = false;
            change->breaking_change_suspected = true;
            
            // Send emergency notification for suspected breaking changes
            if (notifications_comprehensive_is_initialized()) {
                char emergency_title[256];
                char emergency_message[512];
                
                snprintf(emergency_title, sizeof(emergency_title),
                        "🚨 Starlink API Breaking Change Detected");
                
                snprintf(emergency_message, sizeof(emergency_message),
                        "Starlink API version changed from %s to %s and validation failed. "
                        "System functionality may be compromised. Immediate attention required.",
                        change->old_version.software_version,
                        change->new_version.software_version);
                
                notifications_comprehensive_send_emergency(emergency_title, emergency_message,
                                                          "{\"api_change\": true, \"breaking\": true}",
                                                          "starlink_api_monitor");
            }
        }
        
        change->validation_performed_at = time(NULL);
        g_api_version_monitor.stats.validation_attempts++;
        
        if (!change->api_still_functional) {
            g_api_version_monitor.stats.validation_failures++;
        }
    }
    
    return AUTONOMY_SUCCESS;
}

// Determine change severity between versions
api_version_change_severity_t starlink_determine_change_severity(const starlink_api_version_t* old_version,
                                                               const starlink_api_version_t* new_version) {
    if (!old_version || !new_version) {
        return API_VERSION_CHANGE_UNKNOWN;
    }
    
    // Major version change (year change)
    if (old_version->major_version != new_version->major_version) {
        return API_VERSION_CHANGE_MAJOR;
    }
    
    // Moderate version change (minor version change)
    if (old_version->minor_version != new_version->minor_version) {
        return API_VERSION_CHANGE_MODERATE;
    }
    
    // Minor version change (patch or build change)
    if (old_version->patch_version != new_version->patch_version ||
        strcmp(old_version->build_identifier, new_version->build_identifier) != 0) {
        return API_VERSION_CHANGE_MINOR;
    }
    
    // Hardware or part number change
    if (strcmp(old_version->hardware_version, new_version->hardware_version) != 0 ||
        strcmp(old_version->software_part_number, new_version->software_part_number) != 0) {
        return API_VERSION_CHANGE_MODERATE;
    }
    
    return API_VERSION_CHANGE_UNKNOWN;
}

// Send version change notification
static int send_version_change_notification(const starlink_api_version_change_t* change) {
    if (!change || !notifications_comprehensive_is_initialized()) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    char title[256];
    char message[1024];
    char context_json[512];
    
    // Create notification based on severity
    switch (change->severity) {
        case API_VERSION_CHANGE_MAJOR:
            snprintf(title, sizeof(title), "🚨 Starlink API Major Version Change");
            snprintf(message, sizeof(message),
                    "Starlink API has undergone a MAJOR version change from %s to %s. "
                    "This may introduce breaking changes. Please review system functionality.",
                    change->old_version.software_version,
                    change->new_version.software_version);
            break;
            
        case API_VERSION_CHANGE_MODERATE:
            snprintf(title, sizeof(title), "⚠️ Starlink API Version Update");
            snprintf(message, sizeof(message),
                    "Starlink API version updated from %s to %s. "
                    "Monitor system for any functional changes.",
                    change->old_version.software_version,
                    change->new_version.software_version);
            break;
            
        case API_VERSION_CHANGE_MINOR:
            snprintf(title, sizeof(title), "ℹ️ Starlink API Minor Update");
            snprintf(message, sizeof(message),
                    "Starlink API minor update detected: %s → %s. "
                    "Likely bug fixes or minor improvements.",
                    change->old_version.software_version,
                    change->new_version.software_version);
            break;
            
        default:
            snprintf(title, sizeof(title), "❓ Starlink API Change");
            snprintf(message, sizeof(message),
                    "Starlink API change detected but severity unknown: %s → %s",
                    change->old_version.software_version,
                    change->new_version.software_version);
            break;
    }
    
    // Create context JSON
    snprintf(context_json, sizeof(context_json),
            "{"
            "\"api_version_change\": true, "
            "\"old_version\": \"%s\", "
            "\"new_version\": \"%s\", "
            "\"severity\": \"%s\", "
            "\"change_id\": \"%s\", "
            "\"endpoint\": \"%s\""
            "}",
            change->old_version.software_version,
            change->new_version.software_version,
            starlink_api_version_change_severity_to_string(change->severity),
            change->change_id,
            starlink_api_endpoint_to_string(change->endpoint));
    
    // Determine notification priority based on severity
    notification_priority_t priority;
    switch (change->severity) {
        case API_VERSION_CHANGE_MAJOR:
            priority = NOTIFICATION_PRIORITY_HIGH;
            break;
        case API_VERSION_CHANGE_MODERATE:
            priority = NOTIFICATION_PRIORITY_NORMAL;
            break;
        default:
            priority = NOTIFICATION_PRIORITY_LOW;
            break;
    }
    
    // Send notification
    const char* notification_id = notifications_comprehensive_send(
        NOTIFICATION_TYPE_SYSTEM_ALERT,
        priority,
        title,
        message,
        context_json,
        "starlink_api_monitor"
    );
    
    if (notification_id) {
        LOGX_INFO_MSG("Starlink API version change notification sent",
                 "notification_id", notification_id,
                 "severity", starlink_api_version_change_severity_to_string(change->severity));
        return AUTONOMY_SUCCESS;
    } else {
        LOGX_ERROR_MSG("Failed to send Starlink API version change notification");
        return AUTONOMY_ERROR_SYSTEM;
    }
}

// Generate impact assessment
int starlink_generate_impact_assessment(const starlink_api_version_change_t* change, char* assessment_buffer) {
    if (!change || !assessment_buffer) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    switch (change->severity) {
        case API_VERSION_CHANGE_MAJOR:
            snprintf(assessment_buffer, 512,
                    "MAJOR version change detected. High risk of breaking changes to API structure, "
                    "response formats, or endpoint behavior. Comprehensive testing recommended.");
            break;
            
        case API_VERSION_CHANGE_MODERATE:
            snprintf(assessment_buffer, 512,
                    "MODERATE version change detected. Possible changes to API behavior, "
                    "new features added, or response format modifications. Monitor for issues.");
            break;
            
        case API_VERSION_CHANGE_MINOR:
            snprintf(assessment_buffer, 512,
                    "MINOR version change detected. Likely bug fixes or small improvements. "
                    "Low risk of breaking changes but monitoring recommended.");
            break;
            
        default:
            snprintf(assessment_buffer, 512,
                    "UNKNOWN version change pattern. Unable to assess impact. "
                    "Manual review recommended to understand changes.");
            break;
    }
    
    return AUTONOMY_SUCCESS;
}

// Generate recommended actions
int starlink_generate_recommended_actions(const starlink_api_version_change_t* change, char* actions_buffer) {
    if (!change || !actions_buffer) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    switch (change->severity) {
        case API_VERSION_CHANGE_MAJOR:
            snprintf(actions_buffer, 512,
                    "1. Immediately test all Starlink API endpoints "
                    "2. Review system logs for API errors "
                    "3. Consider temporary failover to cellular if issues detected "
                    "4. Update API client code if necessary "
                    "5. Monitor system closely for 24-48 hours");
            break;
            
        case API_VERSION_CHANGE_MODERATE:
            snprintf(actions_buffer, 512,
                    "1. Test Starlink API functionality "
                    "2. Monitor system performance "
                    "3. Review logs for any new errors "
                    "4. Update documentation if API behavior changed");
            break;
            
        case API_VERSION_CHANGE_MINOR:
            snprintf(actions_buffer, 512,
                    "1. Monitor system for any unexpected behavior "
                    "2. Check logs for improved performance or bug fixes "
                    "3. No immediate action required");
            break;
            
        default:
            snprintf(actions_buffer, 512,
                    "1. Manually review Starlink API documentation "
                    "2. Test all API endpoints thoroughly "
                    "3. Update version monitoring patterns if needed");
            break;
    }
    
    return AUTONOMY_SUCCESS;
}

// Utility functions
const char* starlink_api_version_change_severity_to_string(api_version_change_severity_t severity) {
    if (severity >= 0 && severity < API_VERSION_CHANGE_MAX) {
        return CHANGE_SEVERITY_STRINGS[severity];
    }
    return "unknown";
}

const char* starlink_api_endpoint_to_string(starlink_api_endpoint_t endpoint) {
    if (endpoint >= 0 && endpoint < STARLINK_API_ENDPOINT_MAX) {
        return API_ENDPOINT_STRINGS[endpoint];
    }
    return "unknown";
}

bool starlink_api_version_monitor_is_initialized(void) {
    return g_api_version_monitor_initialized;
}

// Monitoring thread worker
static void* monitor_thread_worker(void* arg) {
    LOGX_INFO_MSG("Starlink API version monitor thread started",
             "check_interval_s", g_api_version_monitor.config.check_interval_s);
    
    while (g_api_version_monitor_initialized && g_api_version_monitor.thread_running) {
        sleep(g_api_version_monitor.config.check_interval_s);
        
        if (!g_api_version_monitor.thread_running) break;
        
        // Perform version check
        if (starlink_api_version_monitor_check_version() != AUTONOMY_SUCCESS) {
            LOGX_WARN_MSG("API version check failed in background thread");
        }
    }
    
    LOGX_INFO_MSG("Starlink API version monitor thread stopped");
    return NULL;
}

// Save version to storage (simple file-based storage)
static int save_version_to_storage(const starlink_api_version_t* version) {
    if (!version) return AUTONOMY_ERROR_INVALID_PARAM;
    
    FILE* fp = fopen(g_api_version_monitor.config.version_storage_file, "w");
    if (!fp) {
        LOGX_WARN_MSG("Failed to open version storage file for writing",
                 "file", g_api_version_monitor.config.version_storage_file);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Write version information as simple key-value format
    fprintf(fp, "software_version=%s\n", version->software_version);
    fprintf(fp, "hardware_version=%s\n", version->hardware_version);
    fprintf(fp, "software_part_number=%s\n", version->software_part_number);
    fprintf(fp, "generation_number=%d\n", version->generation_number);
    fprintf(fp, "major_version=%d\n", version->major_version);
    fprintf(fp, "minor_version=%d\n", version->minor_version);
    fprintf(fp, "patch_version=%d\n", version->patch_version);
    fprintf(fp, "build_identifier=%s\n", version->build_identifier);
    fprintf(fp, "first_detected=%ld\n", version->first_detected);
    fprintf(fp, "last_seen=%ld\n", version->last_seen);
    
    fclose(fp);
    
    LOGX_DEBUG_MSG("Starlink API version saved to storage",
              "version", version->software_version,
              "file", g_api_version_monitor.config.version_storage_file);
    
    return AUTONOMY_SUCCESS;
}

// Load version from storage
static int load_version_from_storage(starlink_api_version_t* version) {
    if (!version) return AUTONOMY_ERROR_INVALID_PARAM;
    
    FILE* fp = fopen(g_api_version_monitor.config.version_storage_file, "r");
    if (!fp) {
        return AUTONOMY_ERROR_NOT_FOUND; // File doesn't exist yet
    }
    
    memset(version, 0, sizeof(starlink_api_version_t));
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char key[64], value[192];
        if (sscanf(line, "%63[^=]=%191s", key, value) == 2) {
            if (strcmp(key, "software_version") == 0) {
                strncpy(version->software_version, value, sizeof(version->software_version) - 1);
                version->software_version[sizeof(version->software_version) - 1] = '\0';
            } else if (strcmp(key, "hardware_version") == 0) {
                strncpy(version->hardware_version, value, sizeof(version->hardware_version) - 1);
                version->hardware_version[sizeof(version->hardware_version) - 1] = '\0';
            } else if (strcmp(key, "software_part_number") == 0) {
                strncpy(version->software_part_number, value, sizeof(version->software_part_number) - 1);
                version->software_part_number[sizeof(version->software_part_number) - 1] = '\0';
            } else if (strcmp(key, "generation_number") == 0) {
                version->generation_number = atoi(value);
            } else if (strcmp(key, "major_version") == 0) {
                version->major_version = atoi(value);
            } else if (strcmp(key, "minor_version") == 0) {
                version->minor_version = atoi(value);
            } else if (strcmp(key, "patch_version") == 0) {
                version->patch_version = atoi(value);
            } else if (strcmp(key, "build_identifier") == 0) {
                strncpy(version->build_identifier, value, sizeof(version->build_identifier) - 1);
                version->build_identifier[sizeof(version->build_identifier) - 1] = '\0';
            } else if (strcmp(key, "first_detected") == 0) {
                version->first_detected = atol(value);
            } else if (strcmp(key, "last_seen") == 0) {
                version->last_seen = atol(value);
            }
        }
    }
    
    fclose(fp);
    
    // Validate loaded version
    if (strlen(version->software_version) > 0) {
        LOGX_DEBUG_MSG("Starlink API version loaded from storage",
                  "version", version->software_version);
        return AUTONOMY_SUCCESS;
    }
    
    return AUTONOMY_ERROR_NOT_FOUND;
}

// Validate API functionality after version change
int validate_api_after_change(const starlink_api_version_change_t* change) {
    if (!change) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Simple validation - just return success for now
    // In a real implementation, this would test key API endpoints
    LOGX_INFO_MSG("API validation after version change - placeholder implementation");
    return AUTONOMY_SUCCESS;
}