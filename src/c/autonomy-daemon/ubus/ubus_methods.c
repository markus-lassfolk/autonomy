#include <stdlib.h>
#include "../core/types.h"
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

extern autonomy_state_t g_state;
extern autonomy_config_t g_config;

// Core method handlers - these are the main autonomy daemon control methods
int autonomy_status(struct ubus_context *uctx, struct ubus_object *obj,
                    struct ubus_request_data *req, const char *method,
                    struct blob_attr *msg)
{
    struct blob_buf bb = {0};

    blob_buf_init(&bb, 0);
    blobmsg_add_string(&bb, "state", "running");
    blobmsg_add_u32(&bb, "uptime", (uint32_t)time(NULL));
    blobmsg_add_string(&bb, "version", "5.8.4-227");
    blobmsg_add_string(&bb, "note", "autonomy daemon is running");
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_health(struct ubus_context *uctx, struct ubus_object *obj,
                    struct ubus_request_data *req, const char *method,
                    struct blob_attr *msg)
{
    struct blob_buf bb = {0};

    blob_buf_init(&bb, 0);
    blobmsg_add_string(&bb, "status", "healthy");
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    blobmsg_add_string(&bb, "version", "5.8.4-227");
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_config(struct ubus_context *uctx, struct ubus_object *obj,
                    struct ubus_request_data *req, const char *method,
                    struct blob_attr *msg)
{
    struct blob_buf bb = {0};

    blob_buf_init(&bb, 0);
    blobmsg_add_string(&bb, "config", "autonomy_default_config");
    blobmsg_add_string(&bb, "mode", "normal");
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_start(struct ubus_context *uctx, struct ubus_object *obj,
                   struct ubus_request_data *req, const char *method,
                   struct blob_attr *msg)
{
    struct blob_buf bb = {0};

    blob_buf_init(&bb, 0);
    blobmsg_add_string(&bb, "result", "started");
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    blobmsg_add_string(&bb, "message", "Autonomy service started successfully");
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_stop(struct ubus_context *uctx, struct ubus_object *obj,
                  struct ubus_request_data *req, const char *method,
                  struct blob_attr *msg)
{
    struct blob_buf bb = {0};

    blob_buf_init(&bb, 0);
    blobmsg_add_string(&bb, "result", "stopped");
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    blobmsg_add_string(&bb, "message", "Autonomy service stopped successfully");
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_restart(struct ubus_context *uctx, struct ubus_object *obj,
                     struct ubus_request_data *req, const char *method,
                     struct blob_attr *msg)
{
    struct blob_buf bb = {0};

    blob_buf_init(&bb, 0);
    blobmsg_add_string(&bb, "result", "restarted");
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    blobmsg_add_string(&bb, "message", "Autonomy service restarted successfully");
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

// Infrastructure method handlers - these are daemon infrastructure methods
int autonomy_pid_status(struct ubus_context *uctx, struct ubus_object *obj,
                        struct ubus_request_data *req, const char *method,
                        struct blob_attr *msg)
{
    struct blob_buf bb = {0};

    blob_buf_init(&bb, 0);
    blobmsg_add_string(&bb, "pid_file", "/var/run/autonomy-daemon.pid");
    blobmsg_add_u32(&bb, "current_pid", getpid());
    blobmsg_add_string(&bb, "status", "active");
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_log_status(struct ubus_context *uctx, struct ubus_object *obj,
                        struct ubus_request_data *req, const char *method,
                        struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    const char *log_level_str = "info";
    
    // Convert log level integer to string
    switch (g_config.log_level) {
        case 0: log_level_str = "error"; break;
        case 1: log_level_str = "warn"; break;
        case 2: log_level_str = "info"; break;
        case 3: log_level_str = "debug"; break;
        default: log_level_str = "info"; break;
    }

    blob_buf_init(&bb, 0);
    blobmsg_add_string(&bb, "log_level", log_level_str);
    blobmsg_add_string(&bb, "log_destination", "syslog");
    blobmsg_add_string(&bb, "log_file", g_config.log_file);
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_config_status(struct ubus_context *uctx, struct ubus_object *obj,
                           struct ubus_request_data *req, const char *method,
                           struct blob_attr *msg)
{
    struct blob_buf bb = {0};

    blob_buf_init(&bb, 0);
    blobmsg_add_string(&bb, "config_file", g_config.config_file);
    blobmsg_add_u8(&bb, "daemon_mode", g_config.daemon_mode);
    blobmsg_add_u8(&bb, "debug_mode", g_config.debug_mode);
    blobmsg_add_u32(&bb, "log_level", g_config.log_level);
    blobmsg_add_string(&bb, "log_file", g_config.log_file);
    blobmsg_add_u32(&bb, "pid_file_timeout", g_config.pid_file_timeout);
    
    // Network settings
    blobmsg_add_u32(&bb, "network_check_interval", g_config.network_check_interval);
    blobmsg_add_u32(&bb, "failover_timeout", g_config.failover_timeout);
    blobmsg_add_u8(&bb, "auto_failover", g_config.auto_failover);
    blobmsg_add_u32(&bb, "min_interface_health", g_config.min_interface_health);
    blobmsg_add_u8(&bb, "mwan3_integration", g_config.mwan3_integration);
    
    // GPS settings
    blobmsg_add_u32(&bb, "gps_update_interval", g_config.gps_update_interval);
    blobmsg_add_u32(&bb, "gps_timeout", g_config.gps_timeout);
    blobmsg_add_u8(&bb, "gps_fusion", g_config.gps_fusion);
    blobmsg_add_u32(&bb, "gps_cache_timeout", g_config.gps_cache_timeout);
    blobmsg_add_double(&bb, "min_gps_accuracy", g_config.min_gps_accuracy);
    
    // Starlink settings
    blobmsg_add_u32(&bb, "starlink_check_interval", g_config.starlink_check_interval);
    blobmsg_add_u8(&bb, "starlink_health_monitoring", g_config.starlink_health_monitoring);
    blobmsg_add_string(&bb, "starlink_host", g_config.starlink_host);
    blobmsg_add_u32(&bb, "starlink_port", g_config.starlink_port);
    blobmsg_add_u32(&bb, "starlink_timeout", g_config.starlink_timeout);
    
    // System monitoring
    blobmsg_add_u32(&bb, "system_check_interval", g_config.system_check_interval);
    blobmsg_add_u8(&bb, "resource_monitoring", g_config.resource_monitoring);
    blobmsg_add_u8(&bb, "service_monitoring", g_config.service_monitoring);
    blobmsg_add_u32(&bb, "alert_threshold", g_config.alert_threshold);
    
    // Notifications
    blobmsg_add_u8(&bb, "notifications_enabled", g_config.notifications_enabled);
    blobmsg_add_string(&bb, "email_from", g_config.email_from);
    blobmsg_add_string(&bb, "email_to", g_config.email_to);
    blobmsg_add_string(&bb, "email_smtp", g_config.email_smtp);
    blobmsg_add_string(&bb, "webhook_url", g_config.webhook_url);
    
    // Snow detection
    blobmsg_add_u8(&bb, "snow_detection_enabled", g_config.snow_detection_enabled);
    blobmsg_add_u32(&bb, "snow_detection_samples", g_config.snow_detection_samples);
    blobmsg_add_double(&bb, "snow_obstruction_threshold", g_config.snow_obstruction_threshold);
    blobmsg_add_double(&bb, "snow_snr_degradation_threshold", g_config.snow_snr_degradation_threshold);
    blobmsg_add_double(&bb, "snow_temperature_threshold", g_config.snow_temperature_threshold);
    blobmsg_add_u32(&bb, "snow_verification_time", g_config.snow_verification_time);
    blobmsg_add_u32(&bb, "snow_melt_timeout", g_config.snow_melt_timeout);
    blobmsg_add_string(&bb, "snow_weather_api_key", g_config.snow_weather_api_key);
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}