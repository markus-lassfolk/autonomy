#include "overlay_management.h"
#include "../core/types.h"
// Simple notification function for overlay management
static void send_overlay_notification(const char* level, const char* message) {
    printf("OVERLAY MANAGEMENT [%s]: %s\n", level, message);
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <fcntl.h>

// Global overlay management instance
static overlay_management_t g_overlay_manager;

// Forward declarations
static int get_overlay_usage(void);
int perform_cleanup(void);
int perform_emergency_cleanup(void);
int64_t cleanup_stale_backups(void);
int64_t cleanup_old_logs(void);
int64_t cleanup_temp_files(void);
int64_t cleanup_maintenance_logs(void);
int64_t cleanup_all_backups(void);
int64_t cleanup_all_logs(void);
int64_t cleanup_all_temp_files(void);
int64_t cleanup_system_cache(void);
int64_t remove_file_recursive(const char *path);
static int64_t get_file_size(const char *path);
static int is_file_older_than(const char *path, int days);
// send_overlay_notification is now implemented as send_overlay_notification above

/**
 * Initialize overlay management
 */
int overlay_management_init(void) {
    memset(&g_overlay_manager, 0, sizeof(overlay_management_t));
    
    // Set default configuration
    g_overlay_manager.config.enabled = true;
    g_overlay_manager.config.overlay_space_threshold = 80;
    g_overlay_manager.config.overlay_critical_threshold = 90;
    g_overlay_manager.config.cleanup_retention_days = 7;
    g_overlay_manager.config.notifications_enabled = true;
    g_overlay_manager.config.notify_on_fixes = true;
    g_overlay_manager.config.notify_on_critical = true;
    
    // Initialize statistics
    g_overlay_manager.stats.last_check_time = 0;
    g_overlay_manager.stats.cleanup_count = 0;
    g_overlay_manager.stats.emergency_cleanup_count = 0;
    g_overlay_manager.stats.total_bytes_freed = 0;
    g_overlay_manager.stats.last_cleanup_time = 0;
    
    return AUTONOMY_SUCCESS;
}

/**
 * Check overlay space and perform cleanup if needed
 */
int overlay_management_check(void) {
    if (!g_overlay_manager.config.enabled) {
        return AUTONOMY_SUCCESS;
    }
    
    int usage = get_overlay_usage();
    if (usage == 0) {
        // Skip if monitoring read-only filesystem
        return AUTONOMY_SUCCESS;
    }
    
    g_overlay_manager.stats.last_check_time = time(NULL);
    
    // Only monitor when usage is above reasonable threshold (50%)
    if (usage < 50) {
        return AUTONOMY_SUCCESS;
    }
    
    if (usage >= g_overlay_manager.config.overlay_critical_threshold) {
        // Critical space usage - perform emergency cleanup
        if (g_overlay_manager.config.notifications_enabled && g_overlay_manager.config.notify_on_critical) {
            char message[256];
            snprintf(message, sizeof(message), "Critical overlay space usage: %d%%", usage);
            send_overlay_notification("critical", message);
        }
        
        int result = perform_emergency_cleanup();
        if (result == AUTONOMY_SUCCESS) {
            g_overlay_manager.stats.emergency_cleanup_count++;
        }
        return result;
    } else if (usage >= g_overlay_manager.config.overlay_space_threshold) {
        // High space usage - perform routine cleanup
        if (g_overlay_manager.config.notifications_enabled && g_overlay_manager.config.notify_on_fixes) {
            char message[256];
            snprintf(message, sizeof(message), "High overlay space usage: %d%%", usage);
            send_overlay_notification("warning", message);
        }
        
        int result = perform_cleanup();
        if (result == AUTONOMY_SUCCESS) {
            g_overlay_manager.stats.cleanup_count++;
        }
        return result;
    }
    
    return AUTONOMY_SUCCESS;
}

/**
 * Get overlay filesystem usage percentage
 */
static int get_overlay_usage(void) {
    struct statvfs fs_info;
    
    if (statvfs("/overlay", &fs_info) != 0) {
        return 0; // Cannot access overlay
    }
    
    // Check if this is a read-only filesystem
    if (fs_info.f_flag & ST_RDONLY) {
        return 0; // Read-only filesystem
    }
    
    // Calculate usage percentage
    unsigned long total_blocks = fs_info.f_blocks;
    unsigned long free_blocks = fs_info.f_bavail;
    unsigned long used_blocks = total_blocks - free_blocks;
    
    if (total_blocks == 0) {
        return 0;
    }
    
    int usage_percent = (int)((used_blocks * 100) / total_blocks);
    return usage_percent;
}

/**
 * Perform routine cleanup of stale files
 */
int perform_cleanup(void) {
    int64_t total_freed = 0;
    
    // Cleanup stale backup files
    int64_t freed = cleanup_stale_backups();
    if (freed > 0) total_freed += freed;
    
    // Cleanup old log files
    freed = cleanup_old_logs();
    if (freed > 0) total_freed += freed;
    
    // Cleanup temporary files
    freed = cleanup_temp_files();
    if (freed > 0) total_freed += freed;
    
    // Cleanup maintenance logs
    freed = cleanup_maintenance_logs();
    if (freed > 0) total_freed += freed;
    
    if (total_freed > 0) {
        g_overlay_manager.stats.total_bytes_freed += total_freed;
        g_overlay_manager.stats.last_cleanup_time = time(NULL);
        
        if (g_overlay_manager.config.notifications_enabled && g_overlay_manager.config.notify_on_fixes) {
            char message[256];
            snprintf(message, sizeof(message), "Overlay cleanup completed: freed %lld bytes", (long long)total_freed);
            send_overlay_notification("fix", message);
        }
    }
    
    return AUTONOMY_SUCCESS;
}

/**
 * Perform aggressive cleanup for critical space situations
 */
int perform_emergency_cleanup(void) {
    int64_t total_freed = 0;
    
    // More aggressive cleanup for emergency situations
    int64_t freed = cleanup_all_backups();
    if (freed > 0) total_freed += freed;
    
    freed = cleanup_all_logs();
    if (freed > 0) total_freed += freed;
    
    freed = cleanup_all_temp_files();
    if (freed > 0) total_freed += freed;
    
    freed = cleanup_system_cache();
    if (freed > 0) total_freed += freed;
    
    if (total_freed > 0) {
        g_overlay_manager.stats.total_bytes_freed += total_freed;
        g_overlay_manager.stats.last_cleanup_time = time(NULL);
        
        if (g_overlay_manager.config.notifications_enabled && g_overlay_manager.config.notify_on_critical) {
            char message[256];
            snprintf(message, sizeof(message), "Emergency overlay cleanup completed: freed %lld bytes", (long long)total_freed);
            send_overlay_notification("critical", message);
        }
    }
    
    return AUTONOMY_SUCCESS;
}

/**
 * Cleanup stale backup files
 */
int64_t cleanup_stale_backups(void) {
    int64_t total_freed = 0;
    const char *backup_dirs[] = {"/etc/config", "/root", "/tmp"};
    
    for (int i = 0; i < sizeof(backup_dirs) / sizeof(backup_dirs[0]); i++) {
        DIR *dir = opendir(backup_dirs[i]);
        if (!dir) continue;
        
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".backup") || 
                strstr(entry->d_name, ".bak") ||
                strstr(entry->d_name, ".old")) {
                
                char full_path[512];
                snprintf(full_path, sizeof(full_path), "%s/%s", backup_dirs[i], entry->d_name);
                
                if (is_file_older_than(full_path, g_overlay_manager.config.cleanup_retention_days)) {
                    int64_t freed = remove_file_recursive(full_path);
                    if (freed > 0) total_freed += freed;
                }
            }
        }
        closedir(dir);
    }
    
    return total_freed;
}

/**
 * Cleanup old log files
 */
int64_t cleanup_old_logs(void) {
    int64_t total_freed = 0;
    const char *log_dirs[] = {"/var/log", "/tmp", "/root"};
    
    for (int i = 0; i < sizeof(log_dirs) / sizeof(log_dirs[0]); i++) {
        DIR *dir = opendir(log_dirs[i]);
        if (!dir) continue;
        
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".log") || 
                strstr(entry->d_name, "log.") ||
                strstr(entry->d_name, "autonomy")) {
                
                char full_path[512];
                snprintf(full_path, sizeof(full_path), "%s/%s", log_dirs[i], entry->d_name);
                
                if (is_file_older_than(full_path, g_overlay_manager.config.cleanup_retention_days)) {
                    int64_t freed = remove_file_recursive(full_path);
                    if (freed > 0) total_freed += freed;
                }
            }
        }
        closedir(dir);
    }
    
    return total_freed;
}

/**
 * Cleanup temporary files
 */
int64_t cleanup_temp_files(void) {
    int64_t total_freed = 0;
    const char *temp_dirs[] = {"/tmp", "/var/tmp", "/root/tmp"};
    
    for (int i = 0; i < sizeof(temp_dirs) / sizeof(temp_dirs[0]); i++) {
        DIR *dir = opendir(temp_dirs[i]);
        if (!dir) continue;
        
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".tmp") || 
                strstr(entry->d_name, "tmp.") ||
                strstr(entry->d_name, "~")) {
                
                char full_path[512];
                snprintf(full_path, sizeof(full_path), "%s/%s", temp_dirs[i], entry->d_name);
                
                if (is_file_older_than(full_path, 1)) { // Remove temp files older than 1 day
                    int64_t freed = remove_file_recursive(full_path);
                    if (freed > 0) total_freed += freed;
                }
            }
        }
        closedir(dir);
    }
    
    return total_freed;
}

/**
 * Cleanup maintenance logs
 */
int64_t cleanup_maintenance_logs(void) {
    int64_t total_freed = 0;
    const char *maintenance_logs[] = {"/var/log/maintenance.log", "/tmp/maintenance.log"};
    
    for (int i = 0; i < sizeof(maintenance_logs) / sizeof(maintenance_logs[0]); i++) {
        if (access(maintenance_logs[i], F_OK) == 0) {
            if (is_file_older_than(maintenance_logs[i], g_overlay_manager.config.cleanup_retention_days)) {
                int64_t freed = remove_file_recursive(maintenance_logs[i]);
                if (freed > 0) total_freed += freed;
            }
        }
    }
    
    return total_freed;
}

/**
 * Cleanup all backup files (emergency mode)
 */
int64_t cleanup_all_backups(void) {
    int64_t total_freed = 0;
    const char *backup_dirs[] = {"/etc/config", "/root", "/tmp"};
    
    for (int i = 0; i < sizeof(backup_dirs) / sizeof(backup_dirs[0]); i++) {
        DIR *dir = opendir(backup_dirs[i]);
        if (!dir) continue;
        
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".backup") || 
                strstr(entry->d_name, ".bak") ||
                strstr(entry->d_name, ".old")) {
                
                char full_path[512];
                snprintf(full_path, sizeof(full_path), "%s/%s", backup_dirs[i], entry->d_name);
                
                int64_t freed = remove_file_recursive(full_path);
                if (freed > 0) total_freed += freed;
            }
        }
        closedir(dir);
    }
    
    return total_freed;
}

/**
 * Cleanup all log files (emergency mode)
 */
int64_t cleanup_all_logs(void) {
    int64_t total_freed = 0;
    const char *log_dirs[] = {"/var/log", "/tmp", "/root"};
    
    for (int i = 0; i < sizeof(log_dirs) / sizeof(log_dirs[0]); i++) {
        DIR *dir = opendir(log_dirs[i]);
        if (!dir) continue;
        
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".log") || 
                strstr(entry->d_name, "log.") ||
                strstr(entry->d_name, "autonomy")) {
                
                char full_path[512];
                snprintf(full_path, sizeof(full_path), "%s/%s", log_dirs[i], entry->d_name);
                
                int64_t freed = remove_file_recursive(full_path);
                if (freed > 0) total_freed += freed;
            }
        }
        closedir(dir);
    }
    
    return total_freed;
}

/**
 * Cleanup all temporary files (emergency mode)
 */
int64_t cleanup_all_temp_files(void) {
    int64_t total_freed = 0;
    const char *temp_dirs[] = {"/tmp", "/var/tmp", "/root/tmp"};
    
    for (int i = 0; i < sizeof(temp_dirs) / sizeof(temp_dirs[0]); i++) {
        DIR *dir = opendir(temp_dirs[i]);
        if (!dir) continue;
        
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".tmp") || 
                strstr(entry->d_name, "tmp.") ||
                strstr(entry->d_name, "~")) {
                
                char full_path[512];
                snprintf(full_path, sizeof(full_path), "%s/%s", temp_dirs[i], entry->d_name);
                
                int64_t freed = remove_file_recursive(full_path);
                if (freed > 0) total_freed += freed;
            }
        }
        closedir(dir);
    }
    
    return total_freed;
}

/**
 * Cleanup system cache (emergency mode)
 */
int64_t cleanup_system_cache(void) {
    int64_t total_freed = 0;
    const char *cache_dirs[] = {"/tmp", "/var/cache", "/root/.cache"};
    
    for (int i = 0; i < sizeof(cache_dirs) / sizeof(cache_dirs[0]); i++) {
        DIR *dir = opendir(cache_dirs[i]);
        if (!dir) continue;
        
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, "cache") || 
                strstr(entry->d_name, ".cache")) {
                
                char full_path[512];
                snprintf(full_path, sizeof(full_path), "%s/%s", cache_dirs[i], entry->d_name);
                
                int64_t freed = remove_file_recursive(full_path);
                if (freed > 0) total_freed += freed;
            }
        }
        closedir(dir);
    }
    
    return total_freed;
}

/**
 * Remove file or directory recursively
 */
int64_t remove_file_recursive(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) {
        return 0;
    }
    
    int64_t size = 0;
    
    if (S_ISDIR(st.st_mode)) {
        // Directory - remove contents recursively
        DIR *dir = opendir(path);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                    continue;
                }
                
                char sub_path[512];
                snprintf(sub_path, sizeof(sub_path), "%s/%s", path, entry->d_name);
                size += remove_file_recursive(sub_path);
            }
            closedir(dir);
        }
        
        if (rmdir(path) == 0) {
            size += st.st_size;
        }
    } else {
        // Regular file
        size = st.st_size;
        if (unlink(path) == 0) {
            return size;
        }
    }
    
    return size;
}

/**
 * Get file size
 */
static int64_t get_file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return st.st_size;
    }
    return 0;
}

/**
 * Check if file is older than specified days
 */
static int is_file_older_than(const char *path, int days) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    
    time_t now = time(NULL);
    time_t file_time = st.st_mtime;
    time_t threshold = days * 24 * 60 * 60; // days to seconds
    
    return (now - file_time) > threshold;
}

/**
 * Send notification via notification manager
 */
static void send_overlay_notification(const char *type, const char *message) {
    // Create notification event
    notification_event_t event = {0};
    strcpy(event.title, "Overlay Management Alert");
    strncpy(event.message, message, sizeof(event.message) - 1);
    event.message[sizeof(event.message) - 1] = '\0';
    event.type = NOTIFICATION_TYPE_SYSTEM_HEALTH;
    event.timestamp = time(NULL);
    
    // Determine priority based on type
    if (strcmp(type, "critical") == 0) {
        event.priority = NOTIFICATION_PRIORITY_EMERGENCY;
    } else if (strcmp(type, "warning") == 0) {
        event.priority = NOTIFICATION_PRIORITY_HIGH;
    } else {
        event.priority = NOTIFICATION_PRIORITY_NORMAL;
    }
    
    // Send via notification manager if available
    if (notification_manager_is_initialized()) {
        notification_manager_send_default(event.type, event.title, event.message);
    } else {
        // Fallback to stderr logging
        fprintf(stderr, "OVERLAY NOTIFICATION [%s]: %s\n", type, message);
    }
}

/**
 * Get overlay management status
 */
int overlay_management_get_status(overlay_management_status_t *status) {
    if (!status) {
        return AUTONOMY_ERROR_INVALID_PARAMETER;
    }
    
    status->enabled = g_overlay_manager.config.enabled;
    status->overlay_space_threshold = g_overlay_manager.config.overlay_space_threshold;
    status->overlay_critical_threshold = g_overlay_manager.config.overlay_critical_threshold;
    status->cleanup_retention_days = g_overlay_manager.config.cleanup_retention_days;
    status->notifications_enabled = g_overlay_manager.config.notifications_enabled;
    status->notify_on_fixes = g_overlay_manager.config.notify_on_fixes;
    status->notify_on_critical = g_overlay_manager.config.notify_on_critical;
    
    status->last_check_time = g_overlay_manager.stats.last_check_time;
    status->cleanup_count = g_overlay_manager.stats.cleanup_count;
    status->emergency_cleanup_count = g_overlay_manager.stats.emergency_cleanup_count;
    status->total_bytes_freed = g_overlay_manager.stats.total_bytes_freed;
    status->last_cleanup_time = g_overlay_manager.stats.last_cleanup_time;
    
    return AUTONOMY_SUCCESS;
}

/**
 * Get overlay management configuration
 */
int overlay_management_get_config(overlay_management_config_t *config) {
    if (!config) {
        return AUTONOMY_ERROR_INVALID_PARAMETER;
    }
    
    *config = g_overlay_manager.config;
    return AUTONOMY_SUCCESS;
}

/**
 * Set overlay management configuration
 */
int overlay_management_set_config(const overlay_management_config_t *config) {
    if (!config) {
        return AUTONOMY_ERROR_INVALID_PARAMETER;
    }
    
    g_overlay_manager.config = *config;
    return AUTONOMY_SUCCESS;
}

/**
 * Enable/disable overlay management
 */
static int overlay_management_set_enabled(bool enabled) {
    g_overlay_manager.config.enabled = enabled;
    return AUTONOMY_SUCCESS;
}

/**
 * Reset overlay management
 */
static int overlay_management_reset(void) {
    memset(&g_overlay_manager.stats, 0, sizeof(overlay_management_stats_t));
    return AUTONOMY_SUCCESS;
}

/**
 * Cleanup overlay management
 */
void overlay_management_cleanup(void) {
    // Nothing to cleanup for this module
}
