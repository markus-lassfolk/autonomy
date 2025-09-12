#include "notification_manager.h"
#include "notification_types.h"
#include "../core/types.h"
#include "../shared/logging/logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>
#include <syslog.h>

// Global notification manager state
static bool g_notification_manager_initialized = false;
static pthread_mutex_t g_notification_mutex = PTHREAD_MUTEX_INITIALIZER;
static notification_config_t g_notification_config = {0};
static notification_stats_t g_notification_stats = {0};
static notification_manager_status_t g_notification_status = {0};

// Forward declarations
static int send_to_syslog(notification_type_t type, const char* title, const char* message\n"\n"\n"\n"\n"\n"\n"\n");
static const char* get_type_string(notification_type_t type\n"\n"\n"\n"\n"\n"\n"\n");
static const char* get_priority_string(notification_priority_t priority\n"\n"\n"\n"\n"\n"\n"\n");
static notification_priority_t get_default_priority(notification_type_t type\n"\n"\n"\n"\n"\n"\n"\n");
static bool should_send_notification(notification_type_t type\n"\n"\n"\n"\n"\n"\n"\n");

// Initialize notification manager
int notification_manager_init(const notification_config_t* config)
{
    if (g_notification_manager_initialized) {
        printf("WARN: "Notification manager already initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_SUCCESS;
    }

    if (!config) {
        printf("ERROR: "Notification config cannot be NULL"\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&g_notification_mutex\n"\n"\n"\n"\n"\n"\n"\n");

    g_notification_config = *config;
    memset(&g_notification_stats, 0, sizeof(notification_stats_t)\n"\n"\n"\n"\n"\n"\n"\n");
    memset(&g_notification_status, 0, sizeof(notification_manager_status_t)\n"\n"\n"\n"\n"\n"\n"\n");

    g_notification_manager_initialized = true;

    pthread_mutex_unlock(&g_notification_mutex\n"\n"\n"\n"\n"\n"\n"\n");

    printf("INFO: "Enterprise notification manager initialized successfully"\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_SUCCESS;
}

// Cleanup notification manager
void notification_manager_cleanup(void)
{
    if (!g_notification_manager_initialized) {
        return;
    }

    pthread_mutex_lock(&g_notification_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    g_notification_manager_initialized = false;
    pthread_mutex_unlock(&g_notification_mutex\n"\n"\n"\n"\n"\n"\n"\n");

    printf("INFO: "Enterprise notification manager cleaned up"\n"\n"\n"\n"\n"\n"\n"\n");
}

// Check if notification manager is initialized
bool notification_manager_is_initialized(void)
{
    return g_notification_manager_initialized;
}

// Send notification with default settings - CORE ENTERPRISE FUNCTION
int notification_manager_send_default(notification_type_t type, const char* title, const char* message)
{
    if (!g_notification_manager_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }

    if (!title || !message) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&g_notification_mutex\n"\n"\n"\n"\n"\n"\n"\n");

    // Send to syslog (primary notification method using RUTOS built-in)
    int result = send_to_syslog(type, title, message\n"\n"\n"\n"\n"\n"\n"\n");

    // Update enterprise statistics
    if (result == AUTONOMY_SUCCESS) {
        // Update priority-based statistics
        notification_priority_t priority = get_default_priority(type\n"\n"\n"\n"\n"\n"\n"\n");
        switch (priority) {
            case NOTIFICATION_PRIORITY_EMERGENCY:
                g_notification_stats.emergency_sent++;
                g_notification_stats.last_emergency_time = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
                break;
            case NOTIFICATION_PRIORITY_HIGH:
                g_notification_stats.high_sent++;
                break;
            case NOTIFICATION_PRIORITY_NORMAL:
                g_notification_stats.normal_sent++;
                break;
            case NOTIFICATION_PRIORITY_LOW:
                g_notification_stats.low_sent++;
                break;
            default:
                break;
        }
    }

    g_notification_stats.last_notification_time = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");

    pthread_mutex_unlock(&g_notification_mutex\n"\n"\n"\n"\n"\n"\n"\n");

    printf("INFO: "Sent enterprise notification: type=%s, title='%s'", 
                  get_type_string(type), title\n"\n"\n"\n"\n"\n"\n"\n");

    return result;
}

// Start/Stop worker functions
int notification_manager_start_worker(void) { return AUTONOMY_SUCCESS; }
void notification_manager_stop_worker(void) {}

// Get status and statistics
void notification_manager_get_status(notification_manager_status_t* status)
{
    if (!status) return;
    pthread_mutex_lock(&g_notification_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    *status = g_notification_status;
    pthread_mutex_unlock(&g_notification_mutex\n"\n"\n"\n"\n"\n"\n"\n");
}

void notification_manager_get_stats(notification_stats_t* stats)
{
    if (!stats) return;
    pthread_mutex_lock(&g_notification_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    *stats = g_notification_stats;
    pthread_mutex_unlock(&g_notification_mutex\n"\n"\n"\n"\n"\n"\n"\n");
}

void notification_manager_reset_stats(void)
{
    if (!g_notification_manager_initialized) return;
    pthread_mutex_lock(&g_notification_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    memset(&g_notification_stats, 0, sizeof(notification_stats_t)\n"\n"\n"\n"\n"\n"\n"\n");
    pthread_mutex_unlock(&g_notification_mutex\n"\n"\n"\n"\n"\n"\n"\n");
}

notification_manager_t* notification_manager_get_instance(void) { return NULL; }

// ENTERPRISE SPECIALIZED FUNCTIONS - All documented notification types
int notification_manager_send_failover(const char* title, const char* message, const char* member_name)
{
    char full_message[512];
    snprintf(full_message, sizeof(full_message), "[FAILOVER %s] %s", member_name, message\n"\n"\n"\n"\n"\n"\n"\n");
    return notification_manager_send_default(NOTIFICATION_TYPE_FAILOVER, title, full_message\n"\n"\n"\n"\n"\n"\n"\n");
}

int notification_manager_send_failback(const char* title, const char* message, const char* member_name)
{
    char full_message[512];
    snprintf(full_message, sizeof(full_message), "[FAILBACK %s] %s", member_name, message\n"\n"\n"\n"\n"\n"\n"\n");
    return notification_manager_send_default(NOTIFICATION_TYPE_FAILBACK, title, full_message\n"\n"\n"\n"\n"\n"\n"\n");
}

int notification_manager_send_member_down(const char* title, const char* message, const char* member_name)
{
    char full_message[512];
    snprintf(full_message, sizeof(full_message), "[MEMBER DOWN %s] %s", member_name, message\n"\n"\n"\n"\n"\n"\n"\n");
    return notification_manager_send_default(NOTIFICATION_TYPE_MEMBER_DOWN, title, full_message\n"\n"\n"\n"\n"\n"\n"\n");
}

int notification_manager_send_member_up(const char* title, const char* message, const char* member_name)
{
    char full_message[512];
    snprintf(full_message, sizeof(full_message), "[MEMBER UP %s] %s", member_name, message\n"\n"\n"\n"\n"\n"\n"\n");
    return notification_manager_send_default(NOTIFICATION_TYPE_MEMBER_UP, title, full_message\n"\n"\n"\n"\n"\n"\n"\n");
}

int notification_manager_send_predictive(const char* title, const char* message, const char* member_name)
{
    char full_message[512];
    snprintf(full_message, sizeof(full_message), "[PREDICTIVE %s] %s", member_name, message\n"\n"\n"\n"\n"\n"\n"\n");
    return notification_manager_send_default(NOTIFICATION_TYPE_PREDICTIVE, title, full_message\n"\n"\n"\n"\n"\n"\n"\n");
}

int notification_manager_send_critical_error(const char* title, const char* message)
{
    return notification_manager_send_default(NOTIFICATION_TYPE_CRITICAL_ERROR, title, message\n"\n"\n"\n"\n"\n"\n"\n");
}

int notification_manager_send_recovery(const char* title, const char* message)
{
    return notification_manager_send_default(NOTIFICATION_TYPE_RECOVERY, title, message\n"\n"\n"\n"\n"\n"\n"\n");
}

int notification_manager_send_status_update(const char* title, const char* message)
{
    return notification_manager_send_default(NOTIFICATION_TYPE_STATUS_UPDATE, title, message\n"\n"\n"\n"\n"\n"\n"\n");
}

// DATA LIMIT ENTERPRISE FUNCTIONS
int notification_manager_send_data_limit_failover(const char* title, const char* message, const char* member_name)
{
    char full_message[512];
    snprintf(full_message, sizeof(full_message), "[DATA LIMIT FAILOVER %s] %s", member_name, message\n"\n"\n"\n"\n"\n"\n"\n");
    return notification_manager_send_default(NOTIFICATION_TYPE_DATA_LIMIT_FAILOVER, title, full_message\n"\n"\n"\n"\n"\n"\n"\n");
}

int notification_manager_send_data_limit_failback(const char* title, const char* message, const char* member_name)
{
    char full_message[512];
    snprintf(full_message, sizeof(full_message), "[DATA LIMIT FAILBACK %s] %s", member_name, message\n"\n"\n"\n"\n"\n"\n"\n");
    return notification_manager_send_default(NOTIFICATION_TYPE_DATA_LIMIT_FAILBACK, title, full_message\n"\n"\n"\n"\n"\n"\n"\n");
}

int notification_manager_send_data_limit_daily_80(const char* title, const char* message)
{
    return notification_manager_send_default(NOTIFICATION_TYPE_DATA_LIMIT_DAILY_80, title, message\n"\n"\n"\n"\n"\n"\n"\n");
}

int notification_manager_send_data_limit_daily_100(const char* title, const char* message)
{
    return notification_manager_send_default(NOTIFICATION_TYPE_DATA_LIMIT_DAILY_100, title, message\n"\n"\n"\n"\n"\n"\n"\n");
}

int notification_manager_send_data_limit_monthly_80(const char* title, const char* message)
{
    return notification_manager_send_default(NOTIFICATION_TYPE_DATA_LIMIT_MONTHLY_80, title, message\n"\n"\n"\n"\n"\n"\n"\n");
}

int notification_manager_send_data_limit_monthly_95(const char* title, const char* message)
{
    return notification_manager_send_default(NOTIFICATION_TYPE_DATA_LIMIT_MONTHLY_95, title, message\n"\n"\n"\n"\n"\n"\n"\n");
}

int notification_manager_send_data_limit_exceeded(const char* title, const char* message)
{
    return notification_manager_send_default(NOTIFICATION_TYPE_DATA_LIMIT_EXCEEDED, title, message\n"\n"\n"\n"\n"\n"\n"\n");
}

int notification_manager_send_data_limit_reset(const char* title, const char* message)
{
    return notification_manager_send_default(NOTIFICATION_TYPE_DATA_LIMIT_RESET, title, message\n"\n"\n"\n"\n"\n"\n"\n");
}

int notification_manager_send_data_usage_spike(const char* title, const char* message)
{
    return notification_manager_send_default(NOTIFICATION_TYPE_DATA_USAGE_SPIKE, title, message\n"\n"\n"\n"\n"\n"\n"\n");
}

// Full parameter send function
int notification_manager_send(notification_type_t type, const char* title, const char* message,
                             notification_priority_t priority, const char* member_name)
{
    if (member_name) {
        char full_message[512];
        snprintf(full_message, sizeof(full_message), "[%s] %s", member_name, message\n"\n"\n"\n"\n"\n"\n"\n");
        return notification_manager_send_default(type, title, full_message\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        return notification_manager_send_default(type, title, message\n"\n"\n"\n"\n"\n"\n"\n");
    }
}

// HELPER FUNCTIONS

// Send to syslog using RUTOS built-in functionality
static int send_to_syslog(notification_type_t type, const char* title, const char* message)
{
    const char* type_str = get_type_string(type\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Use RUTOS built-in syslog with appropriate priority
    int syslog_priority;
    switch (type) {
        case NOTIFICATION_TYPE_CRITICAL_ERROR:
        case NOTIFICATION_TYPE_DATA_LIMIT_EXCEEDED:
            syslog_priority = LOG_CRIT;
            break;
        case NOTIFICATION_TYPE_ERROR:
        case NOTIFICATION_TYPE_MEMBER_DOWN:
            syslog_priority = LOG_ERR;
            break;
        case NOTIFICATION_TYPE_WARNING:
        case NOTIFICATION_TYPE_FAILOVER:
            syslog_priority = LOG_WARNING;
            break;
        default:
            syslog_priority = LOG_INFO;
            break;
    }
    
    openlog("autonomy-notifications", LOG_PID, LOG_DAEMON\n"\n"\n"\n"\n"\n"\n"\n");
    syslog(syslog_priority, "[%s] %s: %s", type_str, title, message\n"\n"\n"\n"\n"\n"\n"\n");
    closelog(\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

static bool should_send_notification(notification_type_t type) { return true; }

static notification_priority_t get_default_priority(notification_type_t type)
{
    switch (type) {
        case NOTIFICATION_TYPE_CRITICAL_ERROR:
        case NOTIFICATION_TYPE_DATA_LIMIT_EXCEEDED:
            return NOTIFICATION_PRIORITY_EMERGENCY;
        case NOTIFICATION_TYPE_FAILOVER:
        case NOTIFICATION_TYPE_MEMBER_DOWN:
            return NOTIFICATION_PRIORITY_HIGH;
        default:
            return NOTIFICATION_PRIORITY_NORMAL;
    }
}

static const char* get_type_string(notification_type_t type)
{
    switch (type) {
        case NOTIFICATION_TYPE_FAILOVER: return "FAILOVER";
        case NOTIFICATION_TYPE_FAILBACK: return "FAILBACK";
        case NOTIFICATION_TYPE_MEMBER_DOWN: return "MEMBER_DOWN";
        case NOTIFICATION_TYPE_MEMBER_UP: return "MEMBER_UP";
        case NOTIFICATION_TYPE_PREDICTIVE: return "PREDICTIVE";
        case NOTIFICATION_TYPE_CRITICAL_ERROR: return "CRITICAL_ERROR";
        case NOTIFICATION_TYPE_SYSTEM_HEALTH: return "SYSTEM_HEALTH";
        case NOTIFICATION_TYPE_RECOVERY: return "RECOVERY";
        case NOTIFICATION_TYPE_STATUS_UPDATE: return "STATUS_UPDATE";
        case NOTIFICATION_TYPE_INFO: return "INFO";
        case NOTIFICATION_TYPE_WARNING: return "WARNING";
        case NOTIFICATION_TYPE_ERROR: return "ERROR";
        case NOTIFICATION_TYPE_EMERGENCY: return "EMERGENCY";
        default: return "UNKNOWN";
    }
}

static const char* get_priority_string(notification_priority_t priority)
{
    switch (priority) {
        case NOTIFICATION_PRIORITY_LOWEST: return "LOWEST";
        case NOTIFICATION_PRIORITY_LOW: return "LOW";
        case NOTIFICATION_PRIORITY_NORMAL: return "NORMAL";
        case NOTIFICATION_PRIORITY_HIGH: return "HIGH";
        case NOTIFICATION_PRIORITY_EMERGENCY: return "EMERGENCY";
        default: return "UNKNOWN";
    }
}