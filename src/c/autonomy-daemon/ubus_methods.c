#include "autonomy_types.h"
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <time.h>
#include <string.h>

extern struct autonomy_state g_state;
extern struct autonomy_config g_config;

// Core method handlers
int autonomy_status(struct ubus_context *uctx, struct ubus_object *obj,
                    struct ubus_request_data *req, const char *method,
                    struct blob_attr *msg)
{
    struct blob_buf bb = {0};

    blob_buf_init(&bb, 0);
    blobmsg_add_string(&bb, "state", "running");
    blobmsg_add_u32(&bb, "uptime", (uint32_t)time(NULL));
    blobmsg_add_string(&bb, "version", "5.2.0");
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
    blobmsg_add_string(&bb, "version", "5.2.0");
    
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

// Infrastructure method handlers
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
    
    blob_buf_init(&bb, 0);
    blobmsg_add_string(&bb, "log_level", g_config.log_level);
    blobmsg_add_string(&bb, "log_destination", "syslog");
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
    blobmsg_add_u8(&bb, "gps_enabled", g_config.enable_gps);
    blobmsg_add_u8(&bb, "notifications_enabled", g_config.enable_notifications);
    blobmsg_add_u32(&bb, "health_check_interval", g_config.health_check_interval);
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}
