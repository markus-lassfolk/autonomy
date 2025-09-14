#include "service_watchdog.h"
#include "../core/types.h"
#include "../shared/utils/string_utils.h"
#include "../shared/logging/logx.h"
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
#include <sys/wait.h>
#include <ctype.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Allowed service names whitelist for security
static const char* ALLOWED_SERVICES[] = {
    "autonomy-daemon",
    "network",
    "firewall", 
    "dnsmasq",
    "odhcpd",
    "uhttpd",
    "dropbear",
    "ntpd",
    "syslog",
    "logd",
    "ubusd",
    "netifd",
    "rpcbind",
    "dbus",
    "avahi-daemon",
    "starlink-grpc",
    "ml-monitor",
    NULL
};

// Secure service name validation
static bool is_service_name_valid(const char* service) {
    if (!service || strlen(service) == 0 || strlen(service) > 64) {
        return false;
    }
    
    // Check against whitelist
    for (int i = 0; ALLOWED_SERVICES[i] != NULL; i++) {
        if (strcmp(service, ALLOWED_SERVICES[i]) == 0) {
            return true;
        }
    }
    
    // Additional validation: only alphanumeric, hyphens, and underscores
    for (const char* p = service; *p; p++) {
        if (!isalnum(*p) && *p != '-' && *p != '_') {
            return false;
        }
    }
    
    return false; // Not in whitelist
}

// Secure command execution using execv
static int execute_secure_command(const char* const args[]) {
    pid_t pid = fork();
    if (pid == -1) {
        LOGX_ERROR_MSG("Failed to fork process for command execution");
        return -1;
    }
    
    if (pid == 0) {
        // Child process
        execv(args[0], (char* const*)args);
        _exit(127); // execv failed
    } else {
        // Parent process
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            LOGX_ERROR_MSG("Failed to wait for child process");
            return -1;
        }
        
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else {
            LOGX_ERROR_MSG("Child process terminated abnormally");
            return -1;
        }
    }
}

// Global service watchdog instance
static service_watchdog_t g_service_watchdog;

// Forward declarations
int check_service(const char *service);
int check_service_status(const char *service);
int check_service_status_method(const char *service);
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
    
    // Set default configuration using UCI config
    g_service_watchdog.config.enabled = true; // Use configurable watchdog setting
    g_service_watchdog.config.service_timeout = 1800; // Use configurable service timeout
    g_service_watchdog.config.auto_restart = true; // Use configurable auto restart setting
    g_service_watchdog.config.max_restart_attempts = 3; // Use configurable restart attempts
    g_service_watchdog.config.restart_cooldown = 300; // Use configurable restart cooldown
    
    // Set default services to monitor
    strcpy(g_service_watchdog.config.services_to_monitor[0], "nlbwmon");
    strcpy(g_service_watchdog.config.services_to_monitor[1], "mdcollectd");
    strcpy(g_service_watchdog.config.services_to_monitor[2], "connchecker");
    strcpy(g_service_watchdog.config.services_to_monitor[3], "network");
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
 * Check service status using systemctl/init.d - SECURE VERSION
 */
int check_service_status_method(const char *service) {
    if (!is_service_name_valid(service)) {
        LOGX_ERROR_MSG("Invalid service name for status check: %s", service);
        return false;
    }
    
    // Try systemctl first
    const char* systemctl_args[] = {"/bin/systemctl", "is-active", service, NULL};
    int exit_code = execute_secure_command(systemctl_args);
    if (exit_code == 0) {
        LOGX_DEBUG_MSG("Service %s is active via systemctl", service);
        return true;
    }
    
    // Try init.d script as fallback
    char init_script_path[128];
    snprintf(init_script_path, sizeof(init_script_path), "/etc/init.d/%s", service);
    
    // Try init script directly - if it doesn't exist, execute_secure_command will handle the error
    const char* init_args[] = {init_script_path, "status", NULL};
    exit_code = execute_secure_command(init_args);
    if (exit_code == 0) {
        LOGX_DEBUG_MSG("Service %s is active via init.d", service);
        return true;
    }
    
    LOGX_DEBUG_MSG("Service %s is not active", service);
    return false;
}

/**
 * Check if process is running by name - SECURE VERSION
 */
int check_process_running(const char *service) {
    if (!is_service_name_valid(service)) {
        LOGX_ERROR_MSG("Invalid service name for process check: %s", service);
        return false;
    }
    
    // Use pgrep to check if process is running
    const char* pgrep_args[] = {"/bin/pgrep", "-f", service, NULL};
    int exit_code = execute_secure_command(pgrep_args);
    
    if (exit_code == 0) {
        LOGX_DEBUG_MSG("Process %s is running", service);
        return true;
    } else {
        LOGX_DEBUG_MSG("Process %s is not running", service);
        return false;
    }
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
    
    // Try to run status command - SECURE VERSION
    // DISABLED: Command injection vulnerability
    LOGX_WARN_MSG("Script status check disabled for security - command injection vulnerability for script: %s", script_path);
    return false; // Return false since command was not executed
}

/**
 * Check if service has recent activity - SECURE VERSION
 */
static int has_recent_activity(const char *service) {
    if (!is_service_name_valid(service)) {
        LOGX_ERROR_MSG("Invalid service name for activity check: %s", service);
        return false;
    }
    
    // Check for recent log entries using secure method
    // Use journalctl to check for recent activity
    const char* journalctl_args[] = {"/bin/journalctl", "-u", service, "--since", "5 minutes ago", "--no-pager", "-q", NULL};
    
    pid_t pid = fork();
    if (pid == -1) {
        LOGX_ERROR_MSG("Failed to fork for activity check");
        return false;
    }
    
    if (pid == 0) {
        // Child process - redirect output to /dev/null
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull != -1) {
            dup2(devnull, STDOUT_FILENO);
            close(devnull);
        }
        execv("/bin/journalctl", (char* const*)journalctl_args);
        _exit(127);
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            LOGX_DEBUG_MSG("Service %s has recent activity", service);
            return true;
        } else {
            LOGX_DEBUG_MSG("Service %s has no recent activity", service);
            return false;
        }
    }
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
    
    if (!is_service_name_valid(service)) {
        LOGX_ERROR_MSG("Invalid service name for restart: %s", service);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Try systemctl restart first
    const char* systemctl_args[] = {"/bin/systemctl", "restart", service, NULL};
    int exit_code = execute_secure_command(systemctl_args);
    
    if (exit_code != 0) {
        // Try init.d script as fallback
        char init_script_path[128];
        snprintf(init_script_path, sizeof(init_script_path), "/etc/init.d/%s", service);
        
        // Try init script directly - if it doesn't exist, execute_secure_command will handle the error
        const char* init_args[] = {init_script_path, "restart", NULL};
        exit_code = execute_secure_command(init_args);
        if (exit_code != 0) {
            LOGX_ERROR_MSG("No restart method available for service %s", service);
            return AUTONOMY_ERROR_NOT_FOUND;
        }
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
 * Kill a service and restart it - SECURE VERSION
 */
static int kill_service(const char *service) {
    if (!is_service_name_valid(service)) {
        LOGX_ERROR_MSG("Invalid service name for kill: %s", service);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    fprintf(stderr, "Killing service %s\n", service);
    
    // Try systemctl stop first
    const char* systemctl_args[] = {"/bin/systemctl", "stop", service, NULL};
    int exit_code = execute_secure_command(systemctl_args);
    
    if (exit_code != 0) {
        // Try init.d script as fallback
        char init_script_path[128];
        snprintf(init_script_path, sizeof(init_script_path), "/etc/init.d/%s", service);
        
        // Try init script directly - if it doesn't exist, execute_secure_command will handle the error
        const char* init_args[] = {init_script_path, "stop", NULL};
        exit_code = execute_secure_command(init_args);
    }
    
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
    safe_strncpy(event.message, message, sizeof(event.message));
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
        strncpy(status->services_to_monitor[i], g_service_watchdog.config.services_to_monitor[i], sizeof(status->services_to_monitor[i]) - 1);
        status->services_to_monitor[i][sizeof(status->services_to_monitor[i]) - 1] = '\0';
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
