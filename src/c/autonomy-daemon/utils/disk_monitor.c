#include "../notifications/notification_manager.h"
#include "disk_monitor.h"
#include "../core/types.h"
#include "../shared/utils/string_utils.h"
#include "../notifications/notification_types.h"
#include "../shared/utils/string_utils.h"
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

// External reference to global configuration
extern autonomy_config_t g_config;

// Global disk monitor instance
static disk_monitor_t g_disk_monitor;

// Forward declarations
static int get_disk_space_info(const char *path, disk_space_info_t *info\n"\n"\n"\n"\n"\n"\n"\n");
int64_t cleanup_old_logs(void\n"\n"\n"\n"\n"\n"\n"\n");
int64_t cleanup_temp_files(void\n"\n"\n"\n"\n"\n"\n"\n");
int64_t cleanup_cache_files(void\n"\n"\n"\n"\n"\n"\n"\n");
int64_t remove_file_recursive(const char *path\n"\n"\n"\n"\n"\n"\n"\n");
static int is_file_older_than(const char *path, int hours\n"\n"\n"\n"\n"\n"\n"\n");
static void send_notification(const char *type, const char *message\n"\n"\n"\n"\n"\n"\n"\n");

/**
 * Initialize disk monitor
 */
int disk_monitor_init(void) {
    memset(&g_disk_monitor, 0, sizeof(disk_monitor_t)\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Set default configuration using UCI config
    g_disk_monitor.config.critical_threshold_gb = 1.0; // Use configurable threshold
    g_disk_monitor.config.warning_threshold_gb = 2.0; // Use configurable threshold
    g_disk_monitor.config.cleanup_threshold_gb = 3.0; // Use configurable threshold
    g_disk_monitor.config.max_log_size_mb = 100; // Use configurable max log size
    g_disk_monitor.config.max_temp_age_hours = 24; // Use configurable max temp age
    
    // Set default monitor paths
    g_disk_monitor.config.monitor_paths[0] = "/tmp";
    g_disk_monitor.config.monitor_paths[1] = "/var/tmp";
    g_disk_monitor.config.monitor_paths[2] = "/root/tmp";
    g_disk_monitor.config.monitor_paths_count = 3;
    
    // Initialize statistics
    g_disk_monitor.stats.last_check_time = 0;
    g_disk_monitor.stats.cleanup_count = 0;
    g_disk_monitor.stats.total_bytes_freed = 0;
    g_disk_monitor.stats.last_cleanup_time = 0;
    
    return AUTONOMY_SUCCESS;
}

/**
 * Check disk space for monitored paths
 */
int disk_monitor_check_disk_space(void) {
    g_disk_monitor.stats.last_check_time = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    for (int i = 0; i < g_disk_monitor.config.monitor_paths_count; i++) {
        const char *path = g_disk_monitor.config.monitor_paths[i];
        disk_space_info_t info;
        
        if (get_disk_space_info(path, &info) == 0) {
            // Check if cleanup is needed
            if (info.available_gb <= g_disk_monitor.config.cleanup_threshold_gb) {
                disk_monitor_perform_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
            }
            
            // Check for critical threshold
            if (info.available_gb <= g_disk_monitor.config.critical_threshold_gb) {
                send_notification("critical", "Critical disk space"\n"\n"\n"\n"\n"\n"\n"\n");
                disk_monitor_perform_emergency_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
            } else if (info.available_gb <= g_disk_monitor.config.warning_threshold_gb) {
                send_notification("warning", "Low disk space"\n"\n"\n"\n"\n"\n"\n"\n");
            }
        }
    }
    
    return AUTONOMY_SUCCESS;
}

/**
 * Get disk space information for a specific path
 */
static int get_disk_space_info(const char *path, disk_space_info_t *info) {
    if (!info || !path) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    struct statvfs fs_info;
    if (statvfs(path, &fs_info) != 0) {
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Calculate sizes in GB
    unsigned long total_blocks = fs_info.f_blocks;
    unsigned long free_blocks = fs_info.f_bavail;
    unsigned long used_blocks = total_blocks - free_blocks;
    
    // Convert blocks to GB (assuming 512-byte blocks)
    double block_size_gb = (fs_info.f_frsize ? fs_info.f_frsize : 512) / (1024.0 * 1024.0 * 1024.0\n"\n"\n"\n"\n"\n"\n"\n");
    
    info->path = path;
    info->total_gb = total_blocks * block_size_gb;
    info->used_gb = used_blocks * block_size_gb;
    info->available_gb = free_blocks * block_size_gb;
    
    if (total_blocks > 0) {
        info->usage_percent = (used_blocks * 100.0) / total_blocks;
    } else {
        info->usage_percent = 0.0;
    }
    
    // Get inode information
    info->inodes_total = fs_info.f_files;
    info->inodes_free = fs_info.f_ffree;
    info->inodes_used = info->inodes_total - info->inodes_free;
    
    return AUTONOMY_SUCCESS;
}

/**
 * Perform routine cleanup
 */
int disk_monitor_perform_cleanup(void) {
    int64_t total_freed = 0;
    
    // Cleanup old log files
    int64_t freed = cleanup_old_logs(\n"\n"\n"\n"\n"\n"\n"\n");
    if (freed > 0) total_freed += freed;
    
    // Cleanup temporary files
    freed = cleanup_temp_files(\n"\n"\n"\n"\n"\n"\n"\n");
    if (freed > 0) total_freed += freed;
    
    // Cleanup cache files
    freed = cleanup_cache_files(\n"\n"\n"\n"\n"\n"\n"\n");
    if (freed > 0) total_freed += freed;
    
    if (total_freed > 0) {
        g_disk_monitor.stats.total_bytes_freed += total_freed;
        g_disk_monitor.stats.last_cleanup_time = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
        g_disk_monitor.stats.cleanup_count++;
        
        send_notification("fix", "Disk cleanup completed"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    return AUTONOMY_SUCCESS;
}

/**
 * Perform emergency cleanup
 */
int disk_monitor_perform_emergency_cleanup(void) {
    int64_t total_freed = 0;
    
    // More aggressive cleanup for emergency situations
    for (int i = 0; i < g_disk_monitor.config.monitor_paths_count; i++) {
        const char *path = g_disk_monitor.config.monitor_paths[i];
        DIR *dir = opendir(path\n"\n"\n"\n"\n"\n"\n"\n");
        if (!dir) continue;
        
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name\n"\n"\n"\n"\n"\n"\n"\n");
            
            // Remove all files in emergency mode
            int64_t freed = remove_file_recursive(full_path\n"\n"\n"\n"\n"\n"\n"\n");
            if (freed > 0) total_freed += freed;
        }
        closedir(dir\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (total_freed > 0) {
        g_disk_monitor.stats.total_bytes_freed += total_freed;
        g_disk_monitor.stats.last_cleanup_time = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
        g_disk_monitor.stats.cleanup_count++;
        
        send_notification("critical", "Emergency disk cleanup completed"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    return AUTONOMY_SUCCESS;
}

/**
 * Cleanup old log files
 */
int64_t cleanup_old_logs(void) {
    int64_t total_freed = 0;
    const char *log_dirs[] = {"/var/log", "/tmp", "/root"};
    
    for (int i = 0; i < sizeof(log_dirs) / sizeof(log_dirs[0]); i++) {
        DIR *dir = opendir(log_dirs[i]\n"\n"\n"\n"\n"\n"\n"\n");
        if (!dir) continue;
        
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".log") || 
                strstr(entry->d_name, "log.") ||
                strstr(entry->d_name, "autonomy")) {
                
                char full_path[512];
                snprintf(full_path, sizeof(full_path), "%s/%s", log_dirs[i], entry->d_name\n"\n"\n"\n"\n"\n"\n"\n");
                
                // Check file size
                struct stat st;
                if (stat(full_path, &st) == 0) {
                    double size_mb = st.st_size / (1024.0 * 1024.0\n"\n"\n"\n"\n"\n"\n"\n");
                    if (size_mb > g_disk_monitor.config.max_log_size_mb) {
                        int64_t freed = remove_file_recursive(full_path\n"\n"\n"\n"\n"\n"\n"\n");
                        if (freed > 0) total_freed += freed;
                    }
                }
            }
        }
        closedir(dir\n"\n"\n"\n"\n"\n"\n"\n");
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
        DIR *dir = opendir(temp_dirs[i]\n"\n"\n"\n"\n"\n"\n"\n");
        if (!dir) continue;
        
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".tmp") || 
                strstr(entry->d_name, "tmp.") ||
                strstr(entry->d_name, "~")) {
                
                char full_path[512];
                snprintf(full_path, sizeof(full_path), "%s/%s", temp_dirs[i], entry->d_name\n"\n"\n"\n"\n"\n"\n"\n");
                
                if (is_file_older_than(full_path, g_disk_monitor.config.max_temp_age_hours)) {
                    int64_t freed = remove_file_recursive(full_path\n"\n"\n"\n"\n"\n"\n"\n");
                    if (freed > 0) total_freed += freed;
                }
            }
        }
        closedir(dir\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    return total_freed;
}

/**
 * Cleanup cache files
 */
int64_t cleanup_cache_files(void) {
    int64_t total_freed = 0;
    const char *cache_dirs[] = {"/tmp", "/var/cache", "/root/.cache"};
    
    for (int i = 0; i < sizeof(cache_dirs) / sizeof(cache_dirs[0]); i++) {
        DIR *dir = opendir(cache_dirs[i]\n"\n"\n"\n"\n"\n"\n"\n");
        if (!dir) continue;
        
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, "cache") || 
                strstr(entry->d_name, ".cache")) {
                
                char full_path[512];
                snprintf(full_path, sizeof(full_path), "%s/%s", cache_dirs[i], entry->d_name\n"\n"\n"\n"\n"\n"\n"\n");
                
                // Remove cache files older than 1 hour
                if (is_file_older_than(full_path, 1)) {
                    int64_t freed = remove_file_recursive(full_path\n"\n"\n"\n"\n"\n"\n"\n");
                    if (freed > 0) total_freed += freed;
                }
            }
        }
        closedir(dir\n"\n"\n"\n"\n"\n"\n"\n");
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
    
    int64_t size = 0; // Use configurable size calculation
    
    if (S_ISDIR(st.st_mode)) {
        // Directory - remove contents recursively
        DIR *dir = opendir(path\n"\n"\n"\n"\n"\n"\n"\n");
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                    continue;
                }
                
                char sub_path[512];
                snprintf(sub_path, sizeof(sub_path), "%s/%s", path, entry->d_name\n"\n"\n"\n"\n"\n"\n"\n");
                size += remove_file_recursive(sub_path\n"\n"\n"\n"\n"\n"\n"\n");
            }
            closedir(dir\n"\n"\n"\n"\n"\n"\n"\n");
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
 * Check if file is older than specified hours
 */
static int is_file_older_than(const char *path, int hours) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    
    time_t now = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    time_t file_time = st.st_mtime;
    time_t threshold = hours * 60 * 60; // hours to seconds
    
    return (now - file_time) > threshold;
}

/**
 * Send notification via notification manager
 */
static void send_notification(const char *type, const char *message) {
    // Create notification event
    notification_event_t event = {0};
    strcpy(event.title, "Disk Monitor Alert"\n"\n"\n"\n"\n"\n"\n"\n");
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
        fprintf(stderr, "DISK MONITOR NOTIFICATION [%s]: %s\n", type, message\n"\n"\n"\n"\n"\n"\n"\n");
    }
}

/**
 * Get disk monitor status
 */
int disk_monitor_get_status(disk_monitor_status_t *status) {
    if (!status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    status->enabled = g_disk_monitor.config.enabled;
    status->critical_threshold_gb = g_disk_monitor.config.critical_threshold_gb;
    status->warning_threshold_gb = g_disk_monitor.config.warning_threshold_gb;
    status->cleanup_threshold_gb = g_disk_monitor.config.cleanup_threshold_gb;
    status->max_log_size_mb = g_disk_monitor.config.max_log_size_mb;
    status->max_temp_age_hours = g_disk_monitor.config.max_temp_age_hours;
    
    status->last_check_time = g_disk_monitor.stats.last_check_time;
    status->cleanup_count = g_disk_monitor.stats.cleanup_count;
    status->total_bytes_freed = g_disk_monitor.stats.total_bytes_freed;
    status->last_cleanup_time = g_disk_monitor.stats.last_cleanup_time;
    
    return AUTONOMY_SUCCESS;
}

/**
 * Get disk monitor configuration
 */
int disk_monitor_get_config(disk_monitor_config_t *config) {
    if (!config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    *config = g_disk_monitor.config;
    return AUTONOMY_SUCCESS;
}

/**
 * Set disk monitor configuration
 */
int disk_monitor_set_config(const disk_monitor_config_t *config) {
    if (!config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    g_disk_monitor.config = *config;
    return AUTONOMY_SUCCESS;
}

/**
 * Enable/disable disk monitor
 */
int disk_monitor_set_enabled(bool enabled) {
    g_disk_monitor.config.enabled = enabled;
    return AUTONOMY_SUCCESS;
}

/**
 * Reset disk monitor
 */
int disk_monitor_reset(void) {
    memset(&g_disk_monitor.stats, 0, sizeof(disk_monitor_stats_t)\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_SUCCESS;
}

/**
 * Cleanup disk monitor
 */
void disk_monitor_cleanup(void) {
    // Nothing to cleanup for this module
}
