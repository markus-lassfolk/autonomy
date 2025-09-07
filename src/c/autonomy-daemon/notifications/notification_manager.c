#include "notification_manager.h"
#include "../core/types.h"
#include "../utils/logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>

// Global notification manager state
static bool g_notification_manager_initialized = false;
static pthread_mutex_t g_notification_mutex = PTHREAD_MUTEX_INITIALIZER;
static notification_config_t g_notification_config = {0};
static notification_stats_t g_notification_stats = {0};

// Forward declarations
static int send_to_syslog(notification_type_t type, const char* title, const char* message);
static const char* get_type_string(notification_type_t type);
static const char* get_priority_string(notification_priority_t priority);

// Initialize notification manager
int notification_manager_init(const notification_config_t* config)
{
    if (g_notification_manager_initialized) {
        LOGX_WARN_MSG("Notification manager already initialized");
        return AUTONOMY_SUCCESS;
    }

    if (!config) {
        LOGX_ERROR_MSG("Notification config cannot be NULL");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&g_notification_mutex);

    g_notification_config = *config;
    memset(&g_notification_stats, 0, sizeof(notification_stats_t));
    g_notification_stats.init_time = time(NULL);

    g_notification_manager_initialized = true;

    pthread_mutex_unlock(&g_notification_mutex);

    LOGX_INFO_MSG("Notification manager initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Cleanup notification manager
void notification_manager_cleanup(void)
{
    if (!g_notification_manager_initialized) {
        return;
    }

    pthread_mutex_lock(&g_notification_mutex);
    
    g_notification_manager_initialized = false;
    memset(&g_notification_config, 0, sizeof(notification_config_t));
    memset(&g_notification_stats, 0, sizeof(notification_stats_t));
    
    pthread_mutex_unlock(&g_notification_mutex);

    LOGX_INFO_MSG("Notification manager cleaned up");
}

// Check if notification manager is initialized
bool notification_manager_is_initialized(void)
{
    return g_notification_manager_initialized;
}

// Send notification with default settings
int notification_manager_send_default(notification_type_t type, const char* title, const char* message)
{
    if (!g_notification_manager_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }

    if (!title || !message) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&g_notification_mutex);

    // For now, send to syslog as the primary notification method
    int result = send_to_syslog(type, title, message);

    // Update statistics
    if (result == AUTONOMY_SUCCESS) {
        g_notification_stats.successful_notifications++;
    } else {
        g_notification_stats.failed_notifications++;
    }
    g_notification_stats.total_notifications++;
    g_notification_stats.last_notification = time(NULL);

    pthread_mutex_unlock(&g_notification_mutex);

    LOGX_INFO_MSG("Sent notification: type=%s, title='%s'", 
                  get_type_string(type), title);

    return result;
}

// Start notification worker (simplified - no worker thread for now)
int notification_manager_start_worker(void)
{
    if (!g_notification_manager_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }

    LOGX_INFO_MSG("Notification worker started (simplified mode)");
    return AUTONOMY_SUCCESS;
}

// Stop notification worker
void notification_manager_stop_worker(void)
{
    LOGX_INFO_MSG("Notification worker stopped");
}

// Get notification manager status
void notification_manager_get_status(notification_manager_status_t* status)
{
    if (!status) {
        return;
    }

    pthread_mutex_lock(&g_notification_mutex);

    memset(status, 0, sizeof(notification_manager_status_t));
    status->initialized = g_notification_manager_initialized;
    status->worker_running = g_notification_manager_initialized; // Simplified
    status->last_notification = g_notification_stats.last_notification;
    status->total_notifications = g_notification_stats.total_notifications;

    pthread_mutex_unlock(&g_notification_mutex);
}

// Get notification statistics
void notification_manager_get_stats(notification_stats_t* stats)
{
    if (!stats) {
        return;
    }

    pthread_mutex_lock(&g_notification_mutex);
    *stats = g_notification_stats;
    pthread_mutex_unlock(&g_notification_mutex);
}

// Reset statistics
void notification_manager_reset_stats(void)
{
    if (!g_notification_manager_initialized) {
        return;
    }

    pthread_mutex_lock(&g_notification_mutex);
    memset(&g_notification_stats, 0, sizeof(notification_stats_t));
    g_notification_stats.init_time = time(NULL);
    pthread_mutex_unlock(&g_notification_mutex);

    LOGX_INFO_MSG("Notification statistics reset");
}

// Get notification manager instance (simplified)
notification_manager_t* notification_manager_get_instance(void)
{
    // Return NULL for simplified implementation
    return NULL;
}

// Specialized notification functions
int notification_manager_send_failover(const char* title, const char* message, const char* member_name)
{
    char full_message[512];
    snprintf(full_message, sizeof(full_message), "[FAILOVER %s] %s", member_name, message);
    return notification_manager_send_default(NOTIFICATION_TYPE_CRITICAL, title, full_message);
}

int notification_manager_send_failback(const char* title, const char* message, const char* member_name)
{
    char full_message[512];
    snprintf(full_message, sizeof(full_message), "[FAILBACK %s] %s", member_name, message);
    return notification_manager_send_default(NOTIFICATION_TYPE_INFO, title, full_message);
}

int notification_manager_send_member_down(const char* title, const char* message, const char* member_name)
{
    char full_message[512];
    snprintf(full_message, sizeof(full_message), "[MEMBER DOWN %s] %s", member_name, message);
    return notification_manager_send_default(NOTIFICATION_TYPE_ERROR, title, full_message);
}

int notification_manager_send_member_up(const char* title, const char* message, const char* member_name)
{
    char full_message[512];
    snprintf(full_message, sizeof(full_message), "[MEMBER UP %s] %s", member_name, message);
    return notification_manager_send_default(NOTIFICATION_TYPE_INFO, title, full_message);
}

int notification_manager_send_critical_error(const char* title, const char* message)
{
    return notification_manager_send_default(NOTIFICATION_TYPE_CRITICAL, title, message);
}

int notification_manager_send_recovery(const char* title, const char* message)
{
    return notification_manager_send_default(NOTIFICATION_TYPE_INFO, title, message);
}

int notification_manager_send_status_update(const char* title, const char* message)
{
    return notification_manager_send_default(NOTIFICATION_TYPE_INFO, title, message);
}

int notification_manager_send_predictive(const char* title, const char* message, const char* member_name)
{
    char full_message[512];
    snprintf(full_message, sizeof(full_message), "[PREDICTIVE %s] %s", member_name, message);
    return notification_manager_send_default(NOTIFICATION_TYPE_WARNING, title, full_message);
}

// Data limit notification functions
int notification_manager_send_data_limit_failover(const char* title, const char* message, const char* member_name)
{
    char full_message[512];
    snprintf(full_message, sizeof(full_message), "[DATA LIMIT FAILOVER %s] %s", member_name, message);
    return notification_manager_send_default(NOTIFICATION_TYPE_WARNING, title, full_message);
}

int notification_manager_send_data_limit_failback(const char* title, const char* message, const char* member_name)
{
    char full_message[512];
    snprintf(full_message, sizeof(full_message), "[DATA LIMIT FAILBACK %s] %s", member_name, message);
    return notification_manager_send_default(NOTIFICATION_TYPE_INFO, title, full_message);
}

int notification_manager_send_data_limit_daily_80(const char* title, const char* message)
{
    return notification_manager_send_default(NOTIFICATION_TYPE_WARNING, title, message);
}

int notification_manager_send_data_limit_daily_100(const char* title, const char* message)
{
    return notification_manager_send_default(NOTIFICATION_TYPE_CRITICAL, title, message);
}

int notification_manager_send_data_limit_monthly_80(const char* title, const char* message)
{
    return notification_manager_send_default(NOTIFICATION_TYPE_WARNING, title, message);
}

int notification_manager_send_data_limit_monthly_95(const char* title, const char* message)
{
    return notification_manager_send_default(NOTIFICATION_TYPE_CRITICAL, title, message);
}

int notification_manager_send_data_limit_exceeded(const char* title, const char* message)
{
    return notification_manager_send_default(NOTIFICATION_TYPE_CRITICAL, title, message);
}

int notification_manager_send_data_limit_reset(const char* title, const char* message)
{
    return notification_manager_send_default(NOTIFICATION_TYPE_INFO, title, message);
}

int notification_manager_send_data_usage_spike(const char* title, const char* message)
{
    return notification_manager_send_default(NOTIFICATION_TYPE_WARNING, title, message);
}

// Send notification with full parameters (simplified)
int notification_manager_send(notification_type_t type, const char* title, const char* message,
                             notification_priority_t priority, const char* member_name,
                             notification_location_t* location)
{
    // For simplified implementation, ignore priority and location
    if (member_name) {
        char full_message[512];
        snprintf(full_message, sizeof(full_message), "[%s] %s", member_name, message);
        return notification_manager_send_default(type, title, full_message);
    } else {
        return notification_manager_send_default(type, title, message);
    }
}

// Static helper functions

// Send notification to syslog
static int send_to_syslog(notification_type_t type, const char* title, const char* message)
{
    const char* type_str = get_type_string(type);
    
    // Use RUTOS built-in syslog functionality
    LOGX_INFO_MSG("[NOTIFICATION %s] %s: %s", type_str, title, message);
    
    return AUTONOMY_SUCCESS;
}

// Get type string
static const char* get_type_string(notification_type_t type)
{
    switch (type) {
        case NOTIFICATION_TYPE_INFO: return "INFO";
        case NOTIFICATION_TYPE_WARNING: return "WARNING";
        case NOTIFICATION_TYPE_ERROR: return "ERROR";
        case NOTIFICATION_TYPE_CRITICAL: return "CRITICAL";
        case NOTIFICATION_TYPE_HEALTH: return "HEALTH";
        case NOTIFICATION_TYPE_SECURITY: return "SECURITY";
        case NOTIFICATION_TYPE_SYSTEM_ALERT: return "SYSTEM_ALERT";
        default: return "UNKNOWN";
    }
}

// Get priority string
static const char* get_priority_string(notification_priority_t priority)
{
    switch (priority) {
        case NOTIFICATION_PRIORITY_LOW: return "LOW";
        case NOTIFICATION_PRIORITY_MEDIUM: return "MEDIUM";
        case NOTIFICATION_PRIORITY_HIGH: return "HIGH";
        case NOTIFICATION_PRIORITY_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}