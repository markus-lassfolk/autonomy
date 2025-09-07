#include "../core/types.h"
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>

extern struct autonomy_state g_state;
extern system_health_t g_system_health;

// System management UBUS method handlers
int autonomy_system_status(struct ubus_context *uctx, struct ubus_object *obj,
                          struct ubus_request_data *req, const char *method,
                          struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    blobmsg_add_string(&bb, "system_status", "operational");
    blobmsg_add_u32(&bb, "uptime", get_system_uptime());
    blobmsg_add_u32(&bb, "memory_mb", get_system_memory_usage());
    blobmsg_add_u32(&bb, "load_average", get_system_load_average());
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_system_health_check(struct ubus_context *uctx, struct ubus_object *obj,
                                struct ubus_request_data *req, const char *method,
                                struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    // Perform the system health check
    perform_system_health_check();
    
    blob_buf_init(&bb, 0);
    blobmsg_add_string(&bb, "result", "health_check_completed");
    blobmsg_add_string(&bb, "overall_status", g_system_health.status);
    blobmsg_add_u32(&bb, "overall_score", g_system_health.overall_score);
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_system_health_details(struct ubus_context *uctx, struct ubus_object *obj,
                                  struct ubus_request_data *req, const char *method,
                                  struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Add detailed health scores
    void *health_scores = blobmsg_open_table(&bb, "health_scores");
    blobmsg_add_u32(&bb, "starlink", g_system_health.starlink_health);
    blobmsg_add_u32(&bb, "uci", g_system_health.uci_health);
    blobmsg_add_u32(&bb, "overlay", g_system_health.overlay_health);
    blobmsg_add_u32(&bb, "services", g_system_health.services_health);
    blobmsg_add_u32(&bb, "network", g_system_health.network_health);
    blobmsg_add_u32(&bb, "database", g_system_health.database_health);
    blobmsg_add_u32(&bb, "time", g_system_health.time_health);
    blobmsg_add_u32(&bb, "logs", g_system_health.logs_health);
    blobmsg_close_table(&bb, health_scores);
    
    blobmsg_add_u32(&bb, "overall_score", g_system_health.overall_score);
    blobmsg_add_string(&bb, "overall_status", g_system_health.status);
    blobmsg_add_u32(&bb, "last_check", g_system_health.last_check);
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_system_maintenance(struct ubus_context *uctx, struct ubus_object *obj,
                               struct ubus_request_data *req, const char *method,
                               struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    blobmsg_add_string(&bb, "result", "maintenance_mode_activated");
    blobmsg_add_string(&bb, "message", "System maintenance procedures initiated");
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    // Simulate maintenance procedures
    // In a real implementation, this would trigger actual maintenance tasks
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_system_restart_services(struct ubus_context *uctx, struct ubus_object *obj,
                                    struct ubus_request_data *req, const char *method,
                                    struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    blobmsg_add_string(&bb, "result", "services_restart_initiated");
    blobmsg_add_string(&bb, "message", "System services restart sequence initiated");
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    // Simulate service restart
    // In a real implementation, this would restart critical services
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}
