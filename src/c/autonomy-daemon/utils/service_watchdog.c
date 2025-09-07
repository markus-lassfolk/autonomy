#include "service_watchdog.h"
#include "../core/types.h"
#include "../notifications/notification_manager.h"
#include "../notifications/notification_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>
#include <math.h>
#include <fcntl.h>

// Global service watchdog instance
static service_watchdog_t g_service_watchdog;

// Forward declarations
int check_service_status(const char *service);
int check_process_running(const char *service);
int check_init_script(const char *service);
static int has_recent_activity(const char *service);
int restart_service(const char *service, const char *reason);
static int kill_service(const char *service);
static void send_notification(const char *type, const char *message);

/**
 * Initialize service watchdog
 */
int service_watchdog_init(void) {
    memset(&g_service_watchdog, 0, sizeof(service_watchdog_t));
    
    // Set default configuration
    g_service_watchdog.config.enabled = true;
    g_service_watchdog.config.service_timeout = 1800; // 30 minutes
    g_service_watchdog.config.auto_restart = true;
    g_service_watchdog.config.max_restart_attempts = 3;
    g_service_watchdog.config.restart_cooldown = 300; // 5 minutes
    
    // Set default services to monitor
    strcpy(g_service_watchdog.config.services_to_monitor[0], "nlbwmon");
    strcpy(g_service_watchdog.config.services_to_monitor[1], "mdcollectd");
    strcpy(g_service_watchdog.config.services_to_monitor[2], "connchecker");
    strcpy(g_service_watchdog.config.services_to_monitor[3], "network");
    g_service_watchdog.config.services_count = 4;
    
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
    
    g_service_watchdog.stats.last_check_time = time(NULL);
    
    for (int i = 0; i < g_service_watchdog.config.services_count; i++) {
        const char *service = g_service_watchdog.config.services_to_monitor[i];
        
        if (check_service(service) != 0) {
            fprintf(stderr, "Service check failed for %s\n", service);
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
    int running = check_service_status(service);
    if (!running) {
        fprintf(stderr, "Service %s not running\n", service);
        return restart_service(service, "not running");
    }
    
    // Check if service has recent log activity
    int active = has_recent_activity(service);
    if (!active) {
        fprintf(stderr, "Service %s appears hung (no recent activity)\n", service);
        return restart_service(service, "no recent activity");
    }
    
    fprintf(stderr, "Service %s healthy\n", service);
    return AUTONOMY_SUCCESS;
}

/**
 * Check service status using systemctl or init.d
 */
int check_service_status(const char *service) {
    // Try different methods to check service status
    int running = check_service_status_method(service);
    if (running) return true;
    
    running = check_process_running(service);
    if (running) return true;
    
    running = check_init_script(service);
    return running;
}

/**
 * Check service status using systemctl/init.d
 */
int check_service_status_method(const char *service) {
    char command[256];
    
    // Try systemctl first
    snprintf(command, sizeof(command), "systemctl is-active %s > /dev/null 2>&1", service);
    int exit_code = system(command);
    if (exit_code == 0) {
        return true;
    }
    
    // Try init.d script
    snprintf(command, sizeof(command), "/etc/init.d/%s status > /dev/null 2>&1", service);
    exit_code = system(command);
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
    snprintf(command, sizeof(command), "pgrep -f %s > /dev/null 2>&1", service);
    
    int exit_code = system(command);
    return (exit_code == 0);
}

/**
 * Check init script status
 */
int check_init_script(const char *service) {
    char script_path[256];
    snprintf(script_path, sizeof(script_path), "/etc/init.d/%s", service);
    
    struct stat st;
    if (stat(script_path, &st) != 0) {
        return false;
    }
    
    // Check if script is executable
    if (!(st.st_mode & S_IXUSR)) {
        return false;
    }
    
    // Try to run status command
    char command[256];
    snprintf(command, sizeof(command), "%s status > /dev/null 2>&1", script_path);
    
    int exit_code = system(command);
    return (exit_code == 0);
}

/**
 * Check if service has recent activity
 */
static int has_recent_activity(const char *service) {
    // Check for recent log entries
    char command[256];
    snprintf(command, sizeof(command), 
            "logread | grep -i %s | tail -1 | awk '{print $1, $2, $3}' | xargs -I {} date -d '{}' +%%s", 
            service);
    
    FILE *pipe = popen(command, "r");
    if (!pipe) {
        return false;
    }
    
    char buffer[64];
    time_t last_log_time = 0;
    if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        last_log_time = atol(buffer);
    }
    pclose(pipe);
    
    if (last_log_time == 0) {
        return false;
    }
    
    time_t now = time(NULL);
    time_t time_diff = now - last_log_time;
    
    // Consider service active if it had activity within the timeout period
    return (time_diff < g_service_watchdog.config.service_timeout);
}

/**
 * Restart a service
 */
int restart_service(const char *service, const char *reason) {
    if (!g_service_watchdog.config.auto_restart) {
        fprintf(stderr, "Auto-restart disabled for %s\n", service);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Check restart cooldown
    time_t now = time(NULL);
    if (g_service_watchdog.stats.last_restart_time > 0) {
        time_t time_since_restart = now - g_service_watchdog.stats.last_restart_time;
        if (time_since_restart < g_service_watchdog.config.restart_cooldown) {
            fprintf(stderr, "Service restart cooldown active for %s\n", service);
            return AUTONOMY_ERROR_SYSTEM;
        }
    }
    
    // Check restart attempts
    if (g_service_watchdog.stats.services_restarted >= g_service_watchdog.config.max_restart_attempts) {
        fprintf(stderr, "Maximum restart attempts reached for %s\n", service);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    fprintf(stderr, "Restarting service %s (reason: %s)\n", service, reason);
    
    char command[256];
    
    // Try systemctl restart first
    snprintf(command, sizeof(command), "systemctl restart %s > /dev/null 2>&1", service);
    int exit_code = system(command);
    
    if (exit_code != 0) {
        // Try init.d script
        snprintf(command, sizeof(command), "/etc/init.d/%s restart > /dev/null 2>&1", service);
        exit_code = system(command);
    }
    
    if (exit_code == 0) {
        g_service_watchdog.stats.services_restarted++;
        g_service_watchdog.stats.last_restart_time = now;
        
        send_notification("fix", "Service restarted successfully");
        fprintf(stderr, "Service %s restarted successfully\n", service);
        return AUTONOMY_SUCCESS;
    } else {
        // If restart fails, try to kill and restart
        fprintf(stderr, "Service restart failed for %s, attempting kill and restart\n", service);
        return kill_service(service);
    }
}

/**
 * Kill a service and restart it
 */
static int kill_service(const char *service) {
    fprintf(stderr, "Killing service %s\n", service);
    
    char command[256];
    
    // Try to kill the service process
    snprintf(command, sizeof(command), "pkill -f %s", service);
    int exit_code = system(command);
    
    if (exit_code == 0) {
        g_service_watchdog.stats.services_killed++;
        
        // Wait a moment for process to terminate
        sleep(2);
        
        // Try to restart
        return restart_service(service, "killed and restarting");
    } else {
        fprintf(stderr, "Failed to kill service %s\n", service);
        return AUTONOMY_ERROR_SYSTEM;
    }
}

/**
 * Send notification via notification manager
 */
static void send_notification(const char *type, const char *message) {
    // Create notification event
    notification_event_t event = {0};
    strcpy(event.title, "Service Watchdog Alert");
    strncpy(event.message, message, sizeof(event.message) - 1);
    event.message[sizeof(event.message) - 1] = '\0';
    event.type = NOTIFICATION_TYPE_SYSTEM_HEALTH;
    event.timestamp = time(NULL);
    
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
        notification_manager_send_default(event.type, event.title, event.message);
    } else {
        // Fallback to stderr logging
        fprintf(stderr, "SERVICE WATCHDOG NOTIFICATION [%s]: %s\n", type, message);
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
        strcpy(status->services_to_monitor[i], g_service_watchdog.config.services_to_monitor[i]);
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
    memset(&g_service_watchdog.stats, 0, sizeof(service_watchdog_stats_t));
    return AUTONOMY_SUCCESS;
}

/**
 * Cleanup service watchdog
 */
void service_watchdog_cleanup(void) {
    // Nothing to cleanup for this module
}
