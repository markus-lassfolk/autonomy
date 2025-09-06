#ifndef UCI_MAINTENANCE_H
#define UCI_MAINTENANCE_H

#include "../core/types.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

// UCI issue types
typedef struct {
    char type[32];                          // Issue type (parse_error, corruption, missing_section, unwanted_file)
    char section[64];                       // UCI section or file path
    char description[256];                  // Human-readable description
    char severity[16];                      // Severity (critical, warning, info)
    bool can_auto_fix;                      // Whether we can automatically fix this
    time_t timestamp;                       // When the issue was detected
} uci_issue_t;

// UCI maintenance result
typedef struct {
    uci_issue_t **issues_found;             // Array of issues found
    int issues_found_count;                 // Number of issues found
    uci_issue_t **issues_fixed;             // Array of issues fixed
    int issues_fixed_count;                 // Number of issues fixed
    bool backup_created;                    // Whether backup was created
    char backup_path[256];                  // Path to backup file
    bool system_restart;                    // Whether system restart is needed
    bool success;                           // Whether maintenance was successful
    char error_message[256];                // Error message if failed
} uci_maintenance_result_t;

// UCI maintenance statistics
typedef struct {
    time_t last_check_time;                 // Last check time
    int issues_found;                       // Total issues found
    int issues_fixed;                       // Total issues fixed
    int backups_created;                    // Number of backups created
    time_t last_backup_time;                // Last backup time
} uci_maintenance_stats_t;

// UCI maintenance status
typedef struct {
    time_t last_check_time;                 // Last check time
    int issues_found;                       // Total issues found
    int issues_fixed;                       // Total issues fixed
    int backups_created;                    // Number of backups created
    time_t last_backup_time;                // Last backup time
} uci_maintenance_status_t;

// Main UCI maintenance structure
typedef struct {
    uci_maintenance_stats_t stats;          // Statistics
} uci_maintenance_t;

// Function prototypes

/**
 * Initialize UCI maintenance
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int uci_maintenance_init(void);

/**
 * Perform comprehensive UCI maintenance
 * @param result Result structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int uci_maintenance_perform_maintenance(uci_maintenance_result_t *result);

/**
 * Get UCI maintenance status
 * @param status Status structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int uci_maintenance_get_status(uci_maintenance_status_t *status);

/**
 * Reset UCI maintenance
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int uci_maintenance_reset(void);

/**
 * Cleanup UCI maintenance
 */
void uci_maintenance_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // UCI_MAINTENANCE_H
