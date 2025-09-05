#ifndef STARLINK_API_VERSION_MONITOR_H
#define STARLINK_API_VERSION_MONITOR_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// API version change severity levels
typedef enum {
    API_VERSION_CHANGE_MINOR = 0,      // Minor version change (patch updates)
    API_VERSION_CHANGE_MODERATE,       // Moderate change (minor version bump)
    API_VERSION_CHANGE_MAJOR,          // Major change (major version bump)
    API_VERSION_CHANGE_UNKNOWN,        // Unknown change pattern
    API_VERSION_CHANGE_MAX
} api_version_change_severity_t;

// API endpoint monitoring
typedef enum {
    STARLINK_API_ENDPOINT_GET_STATUS = 0,
    STARLINK_API_ENDPOINT_GET_LOCATION,
    STARLINK_API_ENDPOINT_GET_DIAGNOSTICS,
    STARLINK_API_ENDPOINT_GET_HISTORY,
    STARLINK_API_ENDPOINT_MAX
} starlink_api_endpoint_t;

// API version information
typedef struct {
    char software_version[64];             // Current software version (e.g., "2023.26.0.mr7526")
    char hardware_version[32];             // Hardware version
    char software_part_number[32];         // Software part number
    int generation_number;                 // Generation number
    int boot_count;                        // Boot count
    
    // Version parsing
    int major_version;                     // Parsed major version
    int minor_version;                     // Parsed minor version
    int patch_version;                     // Parsed patch version
    char build_identifier[16];             // Build identifier (e.g., "mr7526")
    
    // Metadata
    time_t first_detected;                 // When first detected
    time_t last_seen;                      // When last seen
    bool is_current;                       // Whether this is the current version
} starlink_api_version_t;

// API version change record
typedef struct {
    char change_id[64];                    // Unique change ID
    time_t detected_at;                    // When change was detected
    starlink_api_endpoint_t endpoint;      // Which endpoint detected the change
    
    // Version change details
    starlink_api_version_t old_version;   // Previous version
    starlink_api_version_t new_version;   // New version
    api_version_change_severity_t severity; // Change severity
    
    // Impact assessment
    bool breaking_change_suspected;       // Whether breaking changes are suspected
    bool notification_sent;               // Whether notification was sent
    char impact_assessment[512];          // Impact assessment description
    char recommended_actions[512];        // Recommended actions
    
    // Validation results
    bool api_still_functional;            // Whether API still works
    char validation_errors[1024];         // Validation error details
    time_t validation_performed_at;       // When validation was performed
} starlink_api_version_change_t;

// API version monitor configuration
typedef struct {
    bool enabled;                          // Enable version monitoring
    int check_interval_s;                  // Check interval in seconds
    bool notify_on_minor_changes;          // Notify on minor version changes
    bool notify_on_moderate_changes;       // Notify on moderate changes
    bool notify_on_major_changes;          // Notify on major changes
    bool notify_on_unknown_changes;        // Notify on unknown patterns
    
    // Validation configuration
    bool perform_validation_on_change;     // Validate API after version change
    int validation_timeout_s;              // Validation timeout
    int max_validation_retries;           // Max validation retries
    
    // Storage configuration
    int max_version_history;               // Max versions to keep in history
    int max_change_records;                // Max change records to keep
    char version_storage_file[256];        // File to store version history
    
    // Notification configuration
    bool send_immediate_notifications;     // Send immediate notifications
    bool send_summary_notifications;       // Send daily summary notifications
    int summary_notification_hour;         // Hour for summary notifications (0-23)
} starlink_api_version_monitor_config_t;

// API version monitor statistics
typedef struct {
    uint64_t total_version_checks;         // Total version checks performed
    uint64_t version_changes_detected;     // Total version changes detected
    uint64_t minor_changes;                // Minor version changes
    uint64_t moderate_changes;             // Moderate version changes
    uint64_t major_changes;                // Major version changes
    uint64_t unknown_changes;              // Unknown change patterns
    
    uint64_t notifications_sent;           // Notifications sent for version changes
    uint64_t validation_attempts;          // API validation attempts
    uint64_t validation_failures;          // API validation failures
    
    double average_check_time_ms;          // Average version check time
    time_t last_version_check;             // Last version check time
    time_t last_version_change;            // Last version change detected
    time_t stats_start_time;               // When statistics started
} starlink_api_version_monitor_stats_t;

// Main API version monitor structure
typedef struct {
    starlink_api_version_monitor_config_t config; // Configuration
    starlink_api_version_monitor_stats_t stats;   // Statistics
    
    // Version tracking
    starlink_api_version_t* version_history;      // Version history
    int version_history_count;             // Number of versions in history
    int version_history_index;             // Current index in history
    
    starlink_api_version_change_t* change_records; // Change records
    int change_records_count;              // Number of change records
    int change_records_index;              // Current index in change records
    
    starlink_api_version_t current_version; // Current API version
    bool current_version_valid;            // Whether current version is valid
    
    // Threading
    pthread_mutex_t mutex;                 // Main mutex
    pthread_t monitor_thread;              // Monitoring thread
    bool thread_running;                   // Thread status
    
    // State
    bool initialized;                      // Initialization status
    time_t last_check;                     // Last check time
} starlink_api_version_monitor_t;

// Function prototypes

/**
 * Initialize Starlink API version monitor
 * @param config Monitor configuration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_api_version_monitor_init(const starlink_api_version_monitor_config_t* config);

/**
 * Cleanup Starlink API version monitor
 */
void starlink_api_version_monitor_cleanup(void);

/**
 * Check for API version changes
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_api_version_monitor_check_version(void);

/**
 * Get current Starlink API version
 * @param version Version structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_api_version_monitor_get_current_version(starlink_api_version_t* version);

/**
 * Get API version change history
 * @param changes Array to store change records
 * @param max_changes Maximum changes to return
 * @return Number of changes returned, or negative error code
 */
int starlink_api_version_monitor_get_change_history(starlink_api_version_change_t* changes, int max_changes);

/**
 * Get API version monitor statistics
 * @param stats Statistics structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_api_version_monitor_get_statistics(starlink_api_version_monitor_stats_t* stats);

/**
 * Force immediate version check
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_api_version_monitor_force_check(void);

/**
 * Validate API functionality after version change
 * @param endpoint API endpoint to validate
 * @return AUTONOMY_SUCCESS if API works, error code if broken
 */
int starlink_api_version_monitor_validate_endpoint(starlink_api_endpoint_t endpoint);

/**
 * Check if API version monitor is initialized
 * @return true if initialized, false otherwise
 */
bool starlink_api_version_monitor_is_initialized(void);

// Utility functions

/**
 * Parse Starlink software version string
 * @param version_str Version string (e.g., "2023.26.0.mr7526")
 * @param version Version structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_parse_software_version(const char* version_str, starlink_api_version_t* version);

/**
 * Compare two API versions
 * @param version1 First version
 * @param version2 Second version
 * @return 0 if equal, >0 if version1 > version2, <0 if version1 < version2
 */
int starlink_compare_api_versions(const starlink_api_version_t* version1, const starlink_api_version_t* version2);

/**
 * Determine change severity between versions
 * @param old_version Old version
 * @param new_version New version
 * @return Change severity level
 */
api_version_change_severity_t starlink_determine_change_severity(const starlink_api_version_t* old_version,
                                                               const starlink_api_version_t* new_version);

/**
 * Convert API version to string
 * @param version API version
 * @param buffer Buffer to store string (min 64 bytes)
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_api_version_to_string(const starlink_api_version_t* version, char* buffer);

/**
 * Convert change severity to string
 * @param severity Change severity
 * @return Severity string
 */
const char* starlink_api_version_change_severity_to_string(api_version_change_severity_t severity);

/**
 * Convert API endpoint to string
 * @param endpoint API endpoint
 * @return Endpoint string
 */
const char* starlink_api_endpoint_to_string(starlink_api_endpoint_t endpoint);

/**
 * Generate impact assessment for version change
 * @param change Version change record
 * @param assessment_buffer Buffer to store assessment (min 512 bytes)
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_generate_impact_assessment(const starlink_api_version_change_t* change, char* assessment_buffer);

/**
 * Generate recommended actions for version change
 * @param change Version change record
 * @param actions_buffer Buffer to store actions (min 512 bytes)
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_generate_recommended_actions(const starlink_api_version_change_t* change, char* actions_buffer);

#ifdef __cplusplus
}
#endif

#endif // STARLINK_API_VERSION_MONITOR_H