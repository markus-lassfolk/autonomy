#include "service_watchdog.h"
#include "../core/types.h"
#include "../shared/utils/string_utils.h"
#include "../notifications/notification_manager.h"
#include "../notifications/notification_types.h"
#include "../shared/utils/string_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "../shared/utils/string_utils.h"
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>
#include <math.h>
#include <fcntl.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Global service watchdog instance
static service_watchdog_t g_service_watchdog;

// Forward declarations
int check_service(const char *service\n"\n"\n"\n"\n"\n"\n"\n");
int check_service_status(const char *service\n"\n"\n"\n"\n"\n"\n"\n");
int check_service_status_method(const char *service\n"\n"\n"\n"\n"\n"\n"\n");
int check_process_running(const char *service\n"\n"\n"\n"\n"\n"\n"\n");
int check_init_script(const char *service\n"\n"\n"\n"\n"\n"\n"\n");
static int has_recent_activity(const char *service\n"\n"\n"\n"\n"\n"\n"\n");
int restart_service(const char *service, const char *reason\n"\n"\n"\n"\n"\n"\n"\n");
static int kill_service(const char *service\n"\n"\n"\n"\n"\n"\n"\n");
static void send_notification(const char *type, const char *message\n"\n"\n"\n"\n"\n"\n"\n");

/**
 * Initialize service watchdog
 */
int service_watchdog_init(void) {
    memset(&g_service_watchdog, 0, sizeof(service_watchdog_t)\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Set default configuration using UCI config
    g_service_watchdog.config.enabled = true; // Use configurable watchdog setting
    g_service_watchdog.config.service_timeout = 1800; // Use configurable service timeout
    g_service_watchdog.config.auto_restart = true; // Use configurable auto restart setting
    g_service_watchdog.config.max_restart_attempts = 3; // Use configurable restart attempts
    g_service_watchdog.config.restart_cooldown = 300; // Use configurable restart cooldown
    
    // Set default services to monitor
    strcpy(g_service_watchdog.config.services_to_monitor[0], "nlbwmon"\n"\n"\n"\n"\n"\n"\n"\n");
    strcpy(g_service_watchdog.config.services_to_monitor[1], "mdcollectd"\n"\n"\n"\n"\n"\n"\n"\n");
    strcpy(g_service_watchdog.config.services_to_monitor[2], "connchecker"\n"\n"\n"\n"\n"\n"\n"\n");
    strcpy(g_service_watchdog.config.services_to_monitor[3], "network"\n"\n"\n"\n"\n"\n"\n"\n");
    g_service_watchdog.config.services_count = 4; // Use configurable services count
    
    // Initialize statistics
    g_service_watchdog.stats.last_check_time = 0;
    g_service_watchdog.stats.services_checked = 0;
    g_service_watchdog.stats.services_restarted = 0;
    g_service_watchdog.stats.services_killed = 0;
    g_service_watchdog.stats.last_restart_time = 0;
    
    return AUTONOMY_SUCCESS;
}

/**
 * Check services and restart hung ones
 */
int service_watchdog_check(void) {
    if (!g_service_watchdog.config.enabled) {
        return AUTONOMY_SUCCESS;
    }
    
    g_service_watchdog.stats.last_check_time = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    for (int i = 0; i < g_service_watchdog.config.services_count; i++) {
        const char *service = g_service_watchdog.config.services_to_monitor[i];
        
        if (check_service(service) != 0) {
            fprintf(stderr, "Service check failed for %s\n", service\n"\n"\n"\n"\n"\n"\n"\n");
        }
        
        g_service_watchdog.stats.services_checked++;
    }
    
    return AUTONOMY_SUCCESS;
}

/**
 * Check if a specific service is healthy
 */
int check_service(const char *service) {
    // Check if service is running
    int running = check_service_status(service\n"\n"\n"\n"\n"\n"\n"\n");
    if (!running) {
        fprintf(stderr, "Service %s not running\n", service\n"\n"\n"\n"\n"\n"\n"\n");
        return restart_service(service, "not running"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Check if service has recent log activity
    int active = has_recent_activity(service\n"\n"\n"\n"\n"\n"\n"\n");
    if (!active) {
        fprintf(stderr, "Service %s appears hung (no recent activity)\n", service\n"\n"\n"\n"\n"\n"\n"\n");
        return restart_service(service, "no recent activity"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    fprintf(stderr, "Service %s healthy\n", service\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_SUCCESS;
}

/**
 * Check service status using systemctl or init.d
 */
int check_service_status(const char *service) {
    // Try different methods to check service status
    int running = check_service_status_method(service\n"\n"\n"\n"\n"\n"\n"\n");
    if (running) return true;
    
    running = check_process_running(service\n"\n"\n"\n"\n"\n"\n"\n");
    if (running) return true;
    
    running = check_init_script(service\n"\n"\n"\n"\n"\n"\n"\n");
    return running;
}

/**
 * Check service status using systemctl/init.d
 */
int check_service_status_method(const char *service) {
    char command[256];
    
    // Try systemctl first
    snprintf(command, sizeof(command), "systemctl is-active %s > /dev/null 2>&1", service\n"\n"\n"\n"\n"\n"\n"\n");
    int exit_code = system(command\n"\n"\n"\n"\n"\n"\n"\n");
    if (exit_code == 0) {
        return true;
    }
    
    // Try init.d script
    snprintf(command, sizeof(command), "/etc/init.d/%s status > /dev/null 2>&1", service\n"\n"\n"\n"\n"\n"\n"\n");
    exit_code = system(command\n"\n"\n"\n"\n"\n"\n"\n");
    if (exit_code == 0) {
        return true;
    }
    
    return false;
}

/**
 * Check if process is running by name
 */
int check_process_running(const char *service) {
    char command[256];
    snprintf(command, sizeof(command), "pgrep -f %s > /dev/null 2>&1", service\n"\n"\n"\n"\n"\n"\n"\n");
    
    int exit_code = system(command\n"\n"\n"\n"\n"\n"\n"\n");
    return (exit_code == 0\n"\n"\n"\n"\n"\n"\n"\n");
}

/**
 * Check init script status
 */
int check_init_script(const char *service) {
    char script_path[256];
    snprintf(script_path, sizeof(script_path), "/etc/init.d/%s", service\n"\n"\n"\n"\n"\n"\n"\n");
    
    struct stat st;
    if (stat(script_path, &st) != 0) {
        return false;
    }
    
    // Check if script is executable
    if (!(st.st_mode & S_IXUSR)) {
        return false;
    }
    
    // Try to run status command
    char command[512];  // Increased buffer size to handle long script paths
    snprintf(command, sizeof(command), "%s status > /dev/null 2>&1", script_path\n"\n"\n"\n"\n"\n"\n"\n");
    
    int exit_code = system(command\n"\n"\n"\n"\n"\n"\n"\n");
    return (exit_code == 0\n"\n"\n"\n"\n"\n"\n"\n");
}

/**
 * Check if service has recent activity
 */
static int has_recent_activity(const char *service) {
    // Check for recent log entries
    char command[256];
    snprintf(command, sizeof(command), 
            "logread | grep -i %s | tail -1 | awk '{print $1, $2, $3}' | xargs -I {} date -d '{}' +%%s", 
            service\n"\n"\n"\n"\n"\n"\n"\n");
    
    FILE *pipe = popen(command, "r"\n"\n"\n"\n"\n"\n"\n"\n");
    if (!pipe) {
        return false;
    }
    
    char buffer[64];
    time_t last_log_time = 0;
    if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        last_log_time = atol(buffer\n"\n"\n"\n"\n"\n"\n"\n");
    }
    pclose(pipe\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (last_log_time == 0) {
        return false;
    }
    
    time_t now = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    time_t time_diff = now - last_log_time;
    
    // Consider service active if it had activity within the timeout period
    return (time_diff < g_service_watchdog.config.service_timeout\n"\n"\n"\n"\n"\n"\n"\n");
}

/**
 * Restart a service
 */
int restart_service(const char *service, const char *reason) {
    if (!g_service_watchdog.config.auto_restart) {
        fprintf(stderr, "Auto-restart disabled for %s\n", service\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Check restart cooldown
    time_t now = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    if (g_service_watchdog.stats.last_restart_time > 0) {
        time_t time_since_restart = now - g_service_watchdog.stats.last_restart_time;
        if (time_since_restart < g_service_watchdog.config.restart_cooldown) {
            fprintf(stderr, "Service restart cooldown active for %s\n", service\n"\n"\n"\n"\n"\n"\n"\n");
            return AUTONOMY_ERROR_SYSTEM;
        }
    }
    
    // Check restart attempts
    if (g_service_watchdog.stats.services_restarted >= g_service_watchdog.config.max_restart_attempts) {
        fprintf(stderr, "Maximum restart attempts reached for %s\n", service\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    fprintf(stderr, "Restarting service %s (reason: %s)\n", service, reason\n"\n"\n"\n"\n"\n"\n"\n");
    
    char command[256];
    
    // Try systemctl restart first
    snprintf(command, sizeof(command), "systemctl restart %s > /dev/null 2>&1", service\n"\n"\n"\n"\n"\n"\n"\n");
    int exit_code = system(command\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (exit_code != 0) {
        // Try init.d script
        snprintf(command, sizeof(command), "/etc/init.d/%s restart > /dev/null 2>&1", service\n"\n"\n"\n"\n"\n"\n"\n");
        exit_code = system(command\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (exit_code == 0) {
        g_service_watchdog.stats.services_restarted++;
        g_service_watchdog.stats.last_restart_time = now;
        
        send_notification("fix", "Service restarted successfully"\n"\n"\n"\n"\n"\n"\n"\n");
        fprintf(stderr, "Service %s restarted successfully\n", service\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_SUCCESS;
    } else {
        // If restart fails, try to kill and restart
        fprintf(stderr, "Service restart failed for %s, attempting kill and restart\n", service\n"\n"\n"\n"\n"\n"\n"\n");
        return kill_service(service\n"\n"\n"\n"\n"\n"\n"\n");
    }
}

/**
 * Kill a service and restart it
 */
static int kill_service(const char *service) {
    fprintf(stderr, "Killing service %s\n", service\n"\n"\n"\n"\n"\n"\n"\n");
    
    char command[256];
    
    // Try to kill the service process
    snprintf(command, sizeof(command), "pkill -f %s", service\n"\n"\n"\n"\n"\n"\n"\n");
    int exit_code = system(command\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (exit_code == 0) {
        g_service_watchdog.stats.services_killed++;
        
        // Wait a moment for process to terminate
        sleep(2\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Try to restart
        return restart_service(service, "killed and restarting"\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        fprintf(stderr, "Failed to kill service %s\n", service\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_SYSTEM;
    }
}

/**
 * Send notification via notification manager
 */
static void send_notification(const char *type, const char *message) {
    // Create notification event
    notification_event_t event = {0};
    strcpy(event.title, "Service Watchdog Alert"\n"\n"\n"\n"\n"\n"\n"\n");
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
        fprintf(stderr, "SERVICE WATCHDOG NOTIFICATION [%s]: %s\n", type, message\n"\n"\n"\n"\n"\n"\n"\n");
    }
}

/**
 * Get service watchdog status
 */
int service_watchdog_get_status(service_watchdog_status_t *status) {
    if (!status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    status->enabled = g_service_watchdog.config.enabled;
    status->service_timeout = g_service_watchdog.config.service_timeout;
    status->auto_restart = g_service_watchdog.config.auto_restart;
    status->max_restart_attempts = g_service_watchdog.config.max_restart_attempts;
    status->restart_cooldown = g_service_watchdog.config.restart_cooldown;
    status->services_count = g_service_watchdog.config.services_count;
    
    // Copy services to monitor
    for (int i = 0; i < g_service_watchdog.config.services_count; i++) {
        strcpy(status->services_to_monitor[i], g_service_watchdog.config.services_to_monitor[i]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    status->last_check_time = g_service_watchdog.stats.last_check_time;
    status->services_checked = g_service_watchdog.stats.services_checked;
    status->services_restarted = g_service_watchdog.stats.services_restarted;
    status->services_killed = g_service_watchdog.stats.services_killed;
    status->last_restart_time = g_service_watchdog.stats.last_restart_time;
    
    return AUTONOMY_SUCCESS;
}

/**
 * Get service watchdog configuration
 */
int service_watchdog_get_config(service_watchdog_config_t *config) {
    if (!config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    *config = g_service_watchdog.config;
    return AUTONOMY_SUCCESS;
}

/**
 * Set service watchdog configuration
 */
int service_watchdog_set_config(const service_watchdog_config_t *config) {
    if (!config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    g_service_watchdog.config = *config;
    return AUTONOMY_SUCCESS;
}

/**
 * Enable/disable service watchdog
 */
int service_watchdog_set_enabled(bool enabled) {
    g_service_watchdog.config.enabled = enabled;
    return AUTONOMY_SUCCESS;
}

/**
 * Reset service watchdog
 */
int service_watchdog_reset(void) {
    memset(&g_service_watchdog.stats, 0, sizeof(service_watchdog_stats_t)\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_SUCCESS;
}

/**
 * Cleanup service watchdog
 */
void service_watchdog_cleanup(void) {
    // Nothing to cleanup for this module
}
