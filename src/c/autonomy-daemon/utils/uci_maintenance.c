#include "../notifications/notification_manager.h"
#include "uci_maintenance.h"
#include "../core/types.h"
#include "../shared/utils/string_utils.h"
#include "../notifications/notification_types.h"
#include "../shared/utils/string_utils.h"
#include "../shared/logging/logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>
#include <math.h>
#include <fcntl.h>

// Global UCI maintenance instance
static uci_maintenance_t g_uci_maintenance;

// Forward declarations
int check_parse_errors(uci_maintenance_result_t *result\n"\n"\n"\n"\n"\n"\n"\n");
static int validate_critical_sections(uci_maintenance_result_t *result\n"\n"\n"\n"\n"\n"\n"\n");
int check_uci_corruption(uci_maintenance_result_t *result\n"\n"\n"\n"\n"\n"\n"\n");
int check_unwanted_config_files(uci_maintenance_result_t *result\n"\n"\n"\n"\n"\n"\n"\n");
static int fix_issues(uci_maintenance_result_t *result\n"\n"\n"\n"\n"\n"\n"\n");
static int verify_fixes(uci_maintenance_result_t *result\n"\n"\n"\n"\n"\n"\n"\n");
static int create_uci_backup(const char *backup_path\n"\n"\n"\n"\n"\n"\n"\n");
static int restore_uci_backup(const char *backup_path\n"\n"\n"\n"\n"\n"\n"\n");
int remove_unwanted_files(void\n"\n"\n"\n"\n"\n"\n"\n");
static void send_notification(const char *type, const char *message\n"\n"\n"\n"\n"\n"\n"\n");

/**
 * Initialize UCI maintenance
 */
int uci_maintenance_init(void) {
    memset(&g_uci_maintenance, 0, sizeof(uci_maintenance_t)\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Initialize statistics
    g_uci_maintenance.stats.last_check_time = 0;
    g_uci_maintenance.stats.issues_found = 0;
    g_uci_maintenance.stats.issues_fixed = 0;
    g_uci_maintenance.stats.backups_created = 0;
    g_uci_maintenance.stats.last_backup_time = 0;
    
    return AUTONOMY_SUCCESS;
}

/**
 * Perform comprehensive UCI maintenance
 */
int uci_maintenance_perform_maintenance(uci_maintenance_result_t *result) {
    if (!result) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Initialize result structure
    memset(result, 0, sizeof(uci_maintenance_result_t)\n"\n"\n"\n"\n"\n"\n"\n");
    result->issues_found = malloc(0\n"\n"\n"\n"\n"\n"\n"\n");
    result->issues_fixed = malloc(0\n"\n"\n"\n"\n"\n"\n"\n");
    result->issues_found_count = 0;
    result->issues_fixed_count = 0;
    result->success = false;
    
    g_uci_maintenance.stats.last_check_time = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Step 1: Create backup (disabled - using external backup system)
    // Backup functionality disabled to avoid filling up storage
    // External backup via Teltonika RMS is used instead
    fprintf(stderr, "UCI backup skipped - using external backup system\n"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Step 2: Check for parse errors
    if (check_parse_errors(result) != 0) {
        fprintf(stderr, "Failed to check UCI parse errors\n"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Step 3: Validate critical sections
    if (validate_critical_sections(result) != 0) {
        fprintf(stderr, "Failed to validate critical sections\n"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Step 4: Check for corruption
    if (check_uci_corruption(result) != 0) {
        fprintf(stderr, "Failed to check UCI corruption\n"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Step 4.5: Check for unwanted files in /etc/config/
    if (check_unwanted_config_files(result) != 0) {
        fprintf(stderr, "Failed to check unwanted config files\n"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Step 5: Attempt to fix issues
    if (fix_issues(result) != 0) {
        fprintf(stderr, "Failed to fix UCI issues\n"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Step 6: Verify fixes
    if (verify_fixes(result) != 0) {
        fprintf(stderr, "Failed to verify UCI fixes\n"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    result->success = (result->issues_found_count == 0 || result->issues_fixed_count > 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    g_uci_maintenance.stats.issues_found += result->issues_found_count;
    g_uci_maintenance.stats.issues_fixed += result->issues_fixed_count;
    
    fprintf(stderr, "UCI maintenance completed: issues_found=%d, issues_fixed=%d, success=%s\n",
            result->issues_found_count, result->issues_fixed_count, 
            result->success ? "true" : "false"\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

/**
 * Check for UCI parse errors
 */
int check_parse_errors(uci_maintenance_result_t *result) {
    // Test UCI configuration by trying to read all sections
    const char *test_sections[] = {"network", "mwan3", "system", "firewall"};
    
    for (int i = 0; i < sizeof(test_sections) / sizeof(test_sections[0]); i++) {
        char command[256];
        snprintf(command, sizeof(command), "uci show %s > /dev/null 2>&1", test_sections[i]\n"\n"\n"\n"\n"\n"\n"\n");
        
        int exit_code = system(command\n"\n"\n"\n"\n"\n"\n"\n");
        if (exit_code != 0) {
            // Parse error detected
            uci_issue_t *issue = malloc(sizeof(uci_issue_t)\n"\n"\n"\n"\n"\n"\n"\n");
            if (issue) {
                safe_strncpy(issue->type, "parse_error", sizeof(issue->type)\n"\n"\n"\n"\n"\n"\n"\n");
                issue->type[sizeof(issue->type) - 1] = '\0';
                safe_strncpy(issue->section, test_sections[i], sizeof(issue->section)\n"\n"\n"\n"\n"\n"\n"\n");
                issue->section[sizeof(issue->section) - 1] = '\0';
                snprintf(issue->description, sizeof(issue->description) - 1,
                        "UCI parse error in %s section", test_sections[i]\n"\n"\n"\n"\n"\n"\n"\n");
                safe_strncpy(issue->severity, "critical", sizeof(issue->severity)\n"\n"\n"\n"\n"\n"\n"\n");
                issue->severity[sizeof(issue->severity) - 1] = '\0';
                issue->can_auto_fix = true;
                issue->timestamp = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
                
                // Add to issues found
                result->issues_found = realloc(result->issues_found, 
                                            (result->issues_found_count + 1) * sizeof(uci_issue_t*)\n"\n"\n"\n"\n"\n"\n"\n");
                result->issues_found[result->issues_found_count++] = issue;
            }
        }
    }
    
    return AUTONOMY_SUCCESS;
}

/**
 * Validate critical UCI sections
 */
static int validate_critical_sections(uci_maintenance_result_t *result) {
    const char *critical_sections[] = {"network", "mwan3", "system", "firewall"};
    
    for (int i = 0; i < sizeof(critical_sections) / sizeof(critical_sections[0]); i++) {
        char command[256];
        snprintf(command, sizeof(command), "uci get %s > /dev/null 2>&1", critical_sections[i]\n"\n"\n"\n"\n"\n"\n"\n");
        
        int exit_code = system(command\n"\n"\n"\n"\n"\n"\n"\n");
        if (exit_code != 0) {
            // Missing critical section
            uci_issue_t *issue = malloc(sizeof(uci_issue_t)\n"\n"\n"\n"\n"\n"\n"\n");
            if (issue) {
                safe_strncpy(issue->type, "missing_section", sizeof(issue->type)\n"\n"\n"\n"\n"\n"\n"\n");
                issue->type[sizeof(issue->type) - 1] = '\0';
                safe_strncpy(issue->section, critical_sections[i], sizeof(issue->section)\n"\n"\n"\n"\n"\n"\n"\n");
                issue->section[sizeof(issue->section) - 1] = '\0';
                snprintf(issue->description, sizeof(issue->description) - 1,
                        "Critical UCI section %s is missing", critical_sections[i]\n"\n"\n"\n"\n"\n"\n"\n");
                safe_strncpy(issue->severity, "critical", sizeof(issue->severity)\n"\n"\n"\n"\n"\n"\n"\n");
                issue->severity[sizeof(issue->severity) - 1] = '\0';
                issue->can_auto_fix = false; // Cannot auto-fix missing sections
                issue->timestamp = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
                
                // Add to issues found
                result->issues_found = realloc(result->issues_found, 
                                            (result->issues_found_count + 1) * sizeof(uci_issue_t*)\n"\n"\n"\n"\n"\n"\n"\n");
                result->issues_found[result->issues_found_count++] = issue;
            }
        }
    }
    
    return AUTONOMY_SUCCESS;
}

/**
 * Check for UCI corruption
 */
int check_uci_corruption(uci_maintenance_result_t *result) {
    // Check for corrupted UCI files by looking for common corruption patterns
    const char *config_dir = "/etc/config";
    DIR *dir = opendir(config_dir\n"\n"\n"\n"\n"\n"\n"\n");
    if (!dir) {
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) { // Regular file
            char file_path[512];
            snprintf(file_path, sizeof(file_path), "%s/%s", config_dir, entry->d_name\n"\n"\n"\n"\n"\n"\n"\n");
            
            // Check file size (corrupted files are often very large or very small)
            struct stat st;
            if (stat(file_path, &st) == 0) {
                if (st.st_size > 1024 * 1024) { // Larger than 1MB
                    uci_issue_t *issue = malloc(sizeof(uci_issue_t)\n"\n"\n"\n"\n"\n"\n"\n");
                    if (issue) {
                        safe_strncpy(issue->type, "corruption", sizeof(issue->type)\n"\n"\n"\n"\n"\n"\n"\n");
                        issue->type[sizeof(issue->type) - 1] = '\0';
                        safe_strncpy(issue->section, entry->d_name, sizeof(issue->section)\n"\n"\n"\n"\n"\n"\n"\n");
                        issue->section[sizeof(issue->section) - 1] = '\0';
                        // Truncate filename if too long to prevent buffer overflow
                        char truncated_name[64];
                        safe_strncpy(truncated_name, entry->d_name, sizeof(truncated_name)\n"\n"\n"\n"\n"\n"\n"\n");
                        truncated_name[sizeof(truncated_name) - 1] = '\0';
                        
                        snprintf(issue->description, sizeof(issue->description) - 1,
                                "UCI file %s appears corrupted (size: %lld bytes)", 
                                truncated_name, (long long)st.st_size\n"\n"\n"\n"\n"\n"\n"\n");
                        safe_strncpy(issue->severity, "critical", sizeof(issue->severity)\n"\n"\n"\n"\n"\n"\n"\n");
                        issue->severity[sizeof(issue->severity) - 1] = '\0';
                        issue->can_auto_fix = true;
                        issue->timestamp = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
                        
                        // Add to issues found
                        result->issues_found = realloc(result->issues_found, 
                                                    (result->issues_found_count + 1) * sizeof(uci_issue_t*)\n"\n"\n"\n"\n"\n"\n"\n");
                        result->issues_found[result->issues_found_count++] = issue;
                    }
                }
            }
        }
    }
    closedir(dir\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

/**
 * Check for unwanted config files
 */
int check_unwanted_config_files(uci_maintenance_result_t *result) {
    const char *config_dir = "/etc/config";
    DIR *dir = opendir(config_dir\n"\n"\n"\n"\n"\n"\n"\n");
    if (!dir) {
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) { // Regular file
            const char *filename = entry->d_name;
            
            // Check for unwanted file patterns
            if (strstr(filename, ".backup") || 
                strstr(filename, ".tmp") ||
                strstr(filename, ".old") ||
                strstr(filename, "~") ||
                strstr(filename, ".swp")) {
                
                uci_issue_t *issue = malloc(sizeof(uci_issue_t)\n"\n"\n"\n"\n"\n"\n"\n");
                if (issue) {
                    safe_strncpy(issue->type, "unwanted_file", sizeof(issue->type)\n"\n"\n"\n"\n"\n"\n"\n");
                    issue->type[sizeof(issue->type) - 1] = '\0';
                    safe_strncpy(issue->section, filename, sizeof(issue->section)\n"\n"\n"\n"\n"\n"\n"\n");
                    issue->section[sizeof(issue->section) - 1] = '\0';
                    // Truncate filename if too long to prevent buffer overflow
                    char truncated_filename[64];
                    safe_strncpy(truncated_filename, filename, sizeof(truncated_filename)\n"\n"\n"\n"\n"\n"\n"\n");
                    truncated_filename[sizeof(truncated_filename) - 1] = '\0';
                    
                    snprintf(issue->description, sizeof(issue->description) - 1,
                            "Unwanted file %s found in /etc/config", truncated_filename\n"\n"\n"\n"\n"\n"\n"\n");
                    safe_strncpy(issue->severity, "warning", sizeof(issue->severity)\n"\n"\n"\n"\n"\n"\n"\n");
                    issue->severity[sizeof(issue->severity) - 1] = '\0';
                    issue->can_auto_fix = true;
                    issue->timestamp = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
                    
                    // Add to issues found
                    result->issues_found = realloc(result->issues_found, 
                                                (result->issues_found_count + 1) * sizeof(uci_issue_t*)\n"\n"\n"\n"\n"\n"\n"\n");
                    result->issues_found[result->issues_found_count++] = issue;
                }
            }
        }
    }
    closedir(dir\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

/**
 * Attempt to fix UCI issues
 */
static int fix_issues(uci_maintenance_result_t *result) {
    for (int i = 0; i < result->issues_found_count; i++) {
        uci_issue_t *issue = result->issues_found[i];
        
        if (!issue->can_auto_fix) {
            continue;
        }
        
        if (strcmp(issue->type, "parse_error") == 0) {
            // Try to fix parse errors by reloading the section
            char command[256];
            snprintf(command, sizeof(command), "uci reload %s", issue->section\n"\n"\n"\n"\n"\n"\n"\n");
            int exit_code = system(command\n"\n"\n"\n"\n"\n"\n"\n");
            
            if (exit_code == 0) {
                // Issue fixed
                uci_issue_t *fixed_issue = malloc(sizeof(uci_issue_t)\n"\n"\n"\n"\n"\n"\n"\n");
                if (fixed_issue) {
                    memcpy(fixed_issue, issue, sizeof(uci_issue_t)\n"\n"\n"\n"\n"\n"\n"\n");
                    fixed_issue->timestamp = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
                    
                    // Add to issues fixed
                    result->issues_fixed = realloc(result->issues_fixed, 
                                                (result->issues_fixed_count + 1) * sizeof(uci_issue_t*)\n"\n"\n"\n"\n"\n"\n"\n");
                    result->issues_fixed[result->issues_fixed_count++] = fixed_issue;
                }
            }
        } else if (strcmp(issue->type, "corruption") == 0) {
            // Try to fix corruption by removing the corrupted file
            char file_path[512];
            snprintf(file_path, sizeof(file_path), "/etc/config/%s", issue->section\n"\n"\n"\n"\n"\n"\n"\n");
            
            if (unlink(file_path) == 0) {
                // Issue fixed
                uci_issue_t *fixed_issue = malloc(sizeof(uci_issue_t)\n"\n"\n"\n"\n"\n"\n"\n");
                if (fixed_issue) {
                    memcpy(fixed_issue, issue, sizeof(uci_issue_t)\n"\n"\n"\n"\n"\n"\n"\n");
                    fixed_issue->timestamp = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
                    
                    // Add to issues fixed
                    result->issues_fixed = realloc(result->issues_fixed, 
                                                (result->issues_fixed_count + 1) * sizeof(uci_issue_t*)\n"\n"\n"\n"\n"\n"\n"\n");
                    result->issues_fixed[result->issues_fixed_count++] = fixed_issue;
                }
            }
        } else if (strcmp(issue->type, "unwanted_file") == 0) {
            // Remove unwanted files
            char file_path[512];
            snprintf(file_path, sizeof(file_path), "/etc/config/%s", issue->section\n"\n"\n"\n"\n"\n"\n"\n");
            
            if (unlink(file_path) == 0) {
                // Issue fixed
                uci_issue_t *fixed_issue = malloc(sizeof(uci_issue_t)\n"\n"\n"\n"\n"\n"\n"\n");
                if (fixed_issue) {
                    memcpy(fixed_issue, issue, sizeof(uci_issue_t)\n"\n"\n"\n"\n"\n"\n"\n");
                    fixed_issue->timestamp = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
                    
                    // Add to issues fixed
                    result->issues_fixed = realloc(result->issues_fixed, 
                                                (result->issues_fixed_count + 1) * sizeof(uci_issue_t*)\n"\n"\n"\n"\n"\n"\n"\n");
                    result->issues_fixed[result->issues_fixed_count++] = fixed_issue;
                }
            }
        }
    }
    
    return AUTONOMY_SUCCESS;
}

/**
 * Verify that fixes worked
 */
static int verify_fixes(uci_maintenance_result_t *result) {
    // Re-run parse error checks to verify fixes
    for (int i = 0; i < result->issues_fixed_count; i++) {
        uci_issue_t *fixed_issue = result->issues_fixed[i];
        
        if (strcmp(fixed_issue->type, "parse_error") == 0) {
            char command[256];
            snprintf(command, sizeof(command), "uci show %s > /dev/null 2>&1", fixed_issue->section\n"\n"\n"\n"\n"\n"\n"\n");
            
            int exit_code = system(command\n"\n"\n"\n"\n"\n"\n"\n");
            if (exit_code != 0) {
                // Fix didn't work, remove from fixed list
                free(fixed_issue\n"\n"\n"\n"\n"\n"\n"\n");
                for (int j = i; j < result->issues_fixed_count - 1; j++) {
                    result->issues_fixed[j] = result->issues_fixed[j + 1];
                }
                result->issues_fixed_count--;
                i--; // Re-check this index
            }
        }
    }
    
    return AUTONOMY_SUCCESS;
}

/**
 * Create UCI backup
 */
static int create_uci_backup(const char *backup_path) {
    if (!backup_path) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Create backup directory if it doesn't exist
    char backup_dir[256];
    safe_strncpy(backup_dir, backup_path, sizeof(backup_dir)\n"\n"\n"\n"\n"\n"\n"\n");
    char *last_slash = strrchr(backup_dir, '/'\n"\n"\n"\n"\n"\n"\n"\n");
    if (last_slash) {
        *last_slash = '\0';
        struct stat st = {0};
        if (stat(backup_dir, &st) == -1) {
            mkdir(backup_dir, 0755\n"\n"\n"\n"\n"\n"\n"\n");
        }
    }
    
    // Create backup using UCI export
    char uci_cmd[512];
    snprintf(uci_cmd, sizeof(uci_cmd), "uci export > %s", backup_path\n"\n"\n"\n"\n"\n"\n"\n");
    
    int result = system(uci_cmd\n"\n"\n"\n"\n"\n"\n"\n");
    if (result == 0) {
        printf("INFO: "UCI backup created successfully", "path", backup_path\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_SUCCESS;
    } else {
        printf("ERROR: "Failed to create UCI backup", "path", backup_path, "result", result\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_SYSTEM;
    }
}

/**
 * Restore UCI backup
 */
static int restore_uci_backup(const char *backup_path) {
    if (!backup_path) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Check if backup file exists
    struct stat st;
    if (stat(backup_path, &st) != 0) {
        printf("ERROR: "Backup file does not exist", "path", backup_path\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    // Create a temporary backup before restoring
    char temp_backup[256];
    snprintf(temp_backup, sizeof(temp_backup), "%s.pre_restore", backup_path\n"\n"\n"\n"\n"\n"\n"\n");
    
    int backup_result = create_uci_backup(temp_backup\n"\n"\n"\n"\n"\n"\n"\n");
    if (backup_result != AUTONOMY_SUCCESS) {
        printf("WARN: "Failed to create pre-restore backup", "path", temp_backup\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Restore from backup using UCI import
    char uci_cmd[512];
    snprintf(uci_cmd, sizeof(uci_cmd), "uci import < %s", backup_path\n"\n"\n"\n"\n"\n"\n"\n");
    
    int result = system(uci_cmd\n"\n"\n"\n"\n"\n"\n"\n");
    if (result == 0) {
        // Commit the changes
        system("uci commit"\n"\n"\n"\n"\n"\n"\n"\n");
        printf("INFO: "UCI backup restored successfully", "path", backup_path\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_SUCCESS;
    } else {
        printf("ERROR: "Failed to restore UCI backup", "path", backup_path, "result", result\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Try to restore from the temporary backup if available
        if (backup_result == AUTONOMY_SUCCESS) {
            printf("INFO: "Attempting to restore from pre-restore backup"\n"\n"\n"\n"\n"\n"\n"\n");
            snprintf(uci_cmd, sizeof(uci_cmd), "uci import < %s", temp_backup\n"\n"\n"\n"\n"\n"\n"\n");
            system(uci_cmd\n"\n"\n"\n"\n"\n"\n"\n");
            system("uci commit"\n"\n"\n"\n"\n"\n"\n"\n");
        }
        
        return AUTONOMY_ERROR_SYSTEM;
    }
}

/**
 * Remove unwanted files
 */
int remove_unwanted_files(void) {
    const char *config_dir = "/etc/config";
    DIR *dir = opendir(config_dir\n"\n"\n"\n"\n"\n"\n"\n");
    if (!dir) {
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    int removed_count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) { // Regular file
            const char *filename = entry->d_name;
            
            // Check for unwanted file patterns
            if (strstr(filename, ".backup") || 
                strstr(filename, ".tmp") ||
                strstr(filename, ".old") ||
                strstr(filename, "~") ||
                strstr(filename, ".swp")) {
                
                char file_path[512];
                snprintf(file_path, sizeof(file_path), "%s/%s", config_dir, filename\n"\n"\n"\n"\n"\n"\n"\n");
                
                if (unlink(file_path) == 0) {
                    removed_count++;
                }
            }
        }
    }
    closedir(dir\n"\n"\n"\n"\n"\n"\n"\n");
    
    return removed_count;
}

/**
 * Send notification via notification manager
 */
static void send_notification(const char *type, const char *message) {
    // Create notification event
    notification_event_t event = {0};
    strcpy(event.title, "UCI Maintenance Alert"\n"\n"\n"\n"\n"\n"\n"\n");
    safe_strncpy(event.message, message, sizeof(event.message)\n"\n"\n"\n"\n"\n"\n"\n");
    event.message[sizeof(event.message) - 1] = '\0';
    event.type = NOTIFICATION_TYPE_SYSTEM_HEALTH;
    event.timestamp = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Determine priority based on type
    if (strcmp(type, "critical") == 0) {
        event.priority = NOTIFICATION_PRIORITY_HIGH;
    } else if (strcmp(type, "warning") == 0) {
        event.priority = NOTIFICATION_PRIORITY_HIGH;
    } else {
        event.priority = NOTIFICATION_PRIORITY_NORMAL;
    }
    
    // Send via notification manager if available
    if (notification_manager_is_initialized()) {
        notification_manager_send_default(event.type, event.title, event.message\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        // Fallback to stderr logging
        fprintf(stderr, "UCI MAINTENANCE NOTIFICATION [%s]: %s\n", type, message\n"\n"\n"\n"\n"\n"\n"\n");
    }
}

/**
 * Get UCI maintenance status
 */
int uci_maintenance_get_status(uci_maintenance_status_t *status) {
    if (!status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    status->last_check_time = g_uci_maintenance.stats.last_check_time;
    status->issues_found = g_uci_maintenance.stats.issues_found;
    status->issues_fixed = g_uci_maintenance.stats.issues_fixed;
    status->backups_created = g_uci_maintenance.stats.backups_created;
    status->last_backup_time = g_uci_maintenance.stats.last_backup_time;
    
    return AUTONOMY_SUCCESS;
}

/**
 * Reset UCI maintenance
 */
int uci_maintenance_reset(void) {
    memset(&g_uci_maintenance.stats, 0, sizeof(uci_maintenance_stats_t)\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_SUCCESS;
}

/**
 * Cleanup UCI maintenance
 */
void uci_maintenance_cleanup(void) {
    // Nothing to cleanup for this module
}
