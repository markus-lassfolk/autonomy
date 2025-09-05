#include "ubus_monitor.h"
#include "types.h"
#include "notifications/notification_manager.h"
#include "notifications/notification_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <errno.h>

// Global UBUS monitor instance
static ubus_monitor_t g_ubus_monitor;

// Forward declarations
static int test_ubus_response(void);
static int check_rpcd_status(void);
static int check_ubus_socket(void);
static int count_ubus_services(void);
static int restart_rpcd_service(void);
static int check_critical_services(void);
static void send_notification(const char *type, const char *message);

/**
 * Initialize UBUS monitor
 */
static int ubus_monitor_init(void) {
    memset(&g_ubus_monitor, 0, sizeof(ubus_monitor_t));
    
    // Set default configuration
    g_ubus_monitor.config.enabled = true;
    g_ubus_monitor.config.check_interval = 300; // 5 minutes
    g_ubus_monitor.config.max_fix_attempts = 3;
    g_ubus_monitor.config.auto_fix = true;
    g_ubus_monitor.config.restart_timeout = 30;
    g_ubus_monitor.config.min_services_expected = 20;
    
    // Set default critical services
    strcpy(g_ubus_monitor.config.critical_services[0], "system");
    strcpy(g_ubus_monitor.config.critical_services[1], "uci");
    strcpy(g_ubus_monitor.config.critical_services[2], "network");
    strcpy(g_ubus_monitor.config.critical_services[3], "service");
    g_ubus_monitor.config.critical_services_count = 4;
    
    // Initialize state
    g_ubus_monitor.fix_attempts = 0;
    g_ubus_monitor.last_fix_time = 0;
    
    return AUTONOMY_SUCCESS;
}

/**
 * Check UBUS health
 */
static int ubus_monitor_check_ubus_health(ubus_health_info_t *info) {
    if (!info) {
        return AUTONOMY_ERROR_INVALID_PARAMETER;
    }
    
    // Initialize info structure
    memset(info, 0, sizeof(ubus_health_info_t));
    info->last_check_time = time(NULL);
    info->fix_attempts = g_ubus_monitor.fix_attempts;
    
    if (g_ubus_monitor.last_fix_time > 0) {
        info->last_fix_time = g_ubus_monitor.last_fix_time;
    }
    
    // Check if UBUS is responding
    int ubus_responding = test_ubus_response();
    info->ubus_responding = ubus_responding;
    
    // Check if rpcd is running
    int rpcd_running = check_rpcd_status();
    info->rpcd_running = rpcd_running;
    
    // Check if UBUS socket exists
    int socket_exists = check_ubus_socket();
    info->ubus_socket_exists = socket_exists;
    
    // Count available services
    int services_count = count_ubus_services();
    info->services_count = services_count;
    
    // Check if health is good
    bool health_good = ubus_responding && rpcd_running && socket_exists && 
                      (services_count >= g_ubus_monitor.config.min_services_expected);
    
    // If health is poor and auto-fix is enabled, attempt to fix
    if (!health_good && g_ubus_monitor.config.auto_fix) {
        if (g_ubus_monitor.fix_attempts < g_ubus_monitor.config.max_fix_attempts) {
            if (restart_rpcd_service() == AUTONOMY_SUCCESS) {
                g_ubus_monitor.fix_attempts++;
                g_ubus_monitor.last_fix_time = time(NULL);
                info->last_fix_time = g_ubus_monitor.last_fix_time;
                
                send_notification("fix", "UBUS health restored by restarting rpcd");
            } else {
                strcpy(info->last_error, "Failed to restart rpcd service");
            }
        } else {
            strcpy(info->last_error, "Maximum fix attempts reached");
        }
    }
    
    return AUTONOMY_SUCCESS;
}

/**
 * Test UBUS response
 */
static int test_ubus_response(void) {
    // Try to connect to UBUS socket
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        return false;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, "/var/run/ubus.sock");
    
    // Set timeout
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    int result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    close(sock);
    
    return (result == 0);
}

/**
 * Check rpcd service status
 */
static int check_rpcd_status(void) {
    // Check if rpcd process is running
    char command[256];
    snprintf(command, sizeof(command), "pgrep rpcd > /dev/null 2>&1");
    
    int exit_code = system(command);
    return (exit_code == 0);
}

/**
 * Check if UBUS socket exists
 */
static int check_ubus_socket(void) {
    struct stat st;
    return (stat("/var/run/ubus.sock", &st) == 0);
}

/**
 * Count available UBUS services
 */
static int count_ubus_services(void) {
    // Use ubus list command to count services
    char command[256];
    snprintf(command, sizeof(command), "ubus list | wc -l");
    
    FILE *pipe = popen(command, "r");
    if (!pipe) {
        return 0;
    }
    
    char buffer[32];
    int count = 0;
    if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        count = atoi(buffer);
    }
    
    pclose(pipe);
    return count;
}

/**
 * Restart rpcd service
 */
static int restart_rpcd_service(void) {
    // Restart rpcd service
    char command[256];
    snprintf(command, sizeof(command), "service rpcd restart > /dev/null 2>&1");
    
    int exit_code = system(command);
    if (exit_code == 0) {
        // Wait for service to start
        sleep(g_ubus_monitor.config.restart_timeout);
        
        // Verify service is running
        if (check_rpcd_status()) {
            return AUTONOMY_SUCCESS;
        }
    }
    
    return AUTONOMY_ERROR_SYSTEM_CALL_FAILED;
}

/**
 * Check critical services
 */
static int check_critical_services(void) {
    int available_count = 0;
    
    for (int i = 0; i < g_ubus_monitor.config.critical_services_count; i++) {
        char command[256];
        snprintf(command, sizeof(command), "ubus list | grep -q %s", 
                g_ubus_monitor.config.critical_services[i]);
        
        int exit_code = system(command);
        if (exit_code == 0) {
            available_count++;
        }
    }
    
    return available_count;
}

/**
 * Send notification via notification manager
 */
static void send_notification(const char *type, const char *message) {
    // Create notification event
    notification_event_t event = {0};
    strcpy(event.title, "UBUS Monitor Alert");
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
        fprintf(stderr, "UBUS MONITOR NOTIFICATION [%s]: %s\n", type, message);
    }
}

/**
 * Get UBUS monitor status
 */
static int ubus_monitor_get_status(ubus_monitor_status_t *status) {
    if (!status) {
        return AUTONOMY_ERROR_INVALID_PARAMETER;
    }
    
    status->enabled = g_ubus_monitor.config.enabled;
    status->check_interval = g_ubus_monitor.config.check_interval;
    status->max_fix_attempts = g_ubus_monitor.config.max_fix_attempts;
    status->auto_fix = g_ubus_monitor.config.auto_fix;
    status->restart_timeout = g_ubus_monitor.config.restart_timeout;
    status->min_services_expected = g_ubus_monitor.config.min_services_expected;
    status->critical_services_count = g_ubus_monitor.config.critical_services_count;
    
    // Copy critical services
    for (int i = 0; i < g_ubus_monitor.config.critical_services_count; i++) {
        strcpy(status->critical_services[i], g_ubus_monitor.config.critical_services[i]);
    }
    
    status->fix_attempts = g_ubus_monitor.fix_attempts;
    status->last_fix_time = g_ubus_monitor.last_fix_time;
    
    return AUTONOMY_SUCCESS;
}

/**
 * Get UBUS monitor configuration
 */
static int ubus_monitor_get_config(ubus_monitor_config_t *config) {
    if (!config) {
        return AUTONOMY_ERROR_INVALID_PARAMETER;
    }
    
    *config = g_ubus_monitor.config;
    return AUTONOMY_SUCCESS;
}

/**
 * Set UBUS monitor configuration
 */
static int ubus_monitor_set_config(const ubus_monitor_config_t *config) {
    if (!config) {
        return AUTONOMY_ERROR_INVALID_PARAMETER;
    }
    
    g_ubus_monitor.config = *config;
    return AUTONOMY_SUCCESS;
}

/**
 * Enable/disable UBUS monitor
 */
static int ubus_monitor_set_enabled(bool enabled) {
    g_ubus_monitor.config.enabled = enabled;
    return AUTONOMY_SUCCESS;
}

/**
 * Reset UBUS monitor
 */
static int ubus_monitor_reset(void) {
    g_ubus_monitor.fix_attempts = 0;
    g_ubus_monitor.last_fix_time = 0;
    return AUTONOMY_SUCCESS;
}

/**
 * Cleanup UBUS monitor
 */
static void ubus_monitor_cleanup(void) {
    // Nothing to cleanup for this module
}
