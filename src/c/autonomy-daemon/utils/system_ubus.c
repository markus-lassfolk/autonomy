#include "../core/types.h"
#include "system_management.h"
#include <libubus.h>
#include <libubox/blobmsg.h>
#include <libubox/blobmsg_json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// System status UBUS method
int autonomy_system_status(struct ubus_context *uctx, struct ubus_object *obj,
                          struct ubus_request_data *req, const char *method,
                          struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Get system information
    unsigned long total_mem, available_mem;
    unsigned long uptime;
    double load1, load5, load15;
    
    get_system_memory_usage(&total_mem, &available_mem);
    uptime = get_system_uptime();
    get_system_load_average(&load1, &load5, &load15);
    
    // Add system status information
    blobmsg_add_string(&bb, "status", "operational");
    blobmsg_add_u32(&bb, "uptime", (uint32_t)uptime);
    blobmsg_add_u64(&bb, "total_memory", total_mem);
    blobmsg_add_u64(&bb, "available_memory", available_mem);
    blobmsg_add_double(&bb, "load_1min", (float)load1);
    blobmsg_add_double(&bb, "load_5min", (float)load5);
    blobmsg_add_double(&bb, "load_15min", (float)load15);
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

// System health check UBUS method
int autonomy_system_health_check(struct ubus_context *uctx, struct ubus_object *obj,
                                struct ubus_request_data *req, const char *method,
                                struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Perform health check
    if (perform_system_health_check() == 0) {
        const system_health_t *health = get_system_health_status();
        
        blobmsg_add_string(&bb, "status", "healthy");
        blobmsg_add_u32(&bb, "overall_score", health->overall_score);
        blobmsg_add_string(&bb, "status", health->status);
        blobmsg_add_u8(&bb, "starlink_health", health->starlink_health);
        blobmsg_add_u8(&bb, "uci_health", health->uci_health);
        blobmsg_add_u8(&bb, "overlay_health", health->overlay_health);
        blobmsg_add_u8(&bb, "services_health", health->services_health);
        blobmsg_add_u8(&bb, "network_health", health->network_health);
        blobmsg_add_u8(&bb, "database_health", health->database_health);
        blobmsg_add_u8(&bb, "time_health", health->time_health);
        blobmsg_add_u8(&bb, "logs_health", health->logs_health);
    } else {
        blobmsg_add_string(&bb, "status", "error");
        blobmsg_add_string(&bb, "error", "Failed to perform health check");
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

// System health details UBUS method
int autonomy_system_health_details(struct ubus_context *uctx, struct ubus_object *obj,
                                  struct ubus_request_data *req, const char *method,
                                  struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Get detailed health information
    const system_health_t *health = get_system_health_status();
    
    // Add detailed health information
    blobmsg_add_u32(&bb, "overall_score", health->overall_score);
    blobmsg_add_string(&bb, "status", health->status);
    
    // Component health details
    struct blob_attr *components = blobmsg_open_table(&bb, "components");
    blobmsg_add_u8(&bb, "starlink", health->starlink_health);
    blobmsg_add_u8(&bb, "uci", health->uci_health);
    blobmsg_add_u8(&bb, "overlay", health->overlay_health);
    blobmsg_add_u8(&bb, "services", health->services_health);
    blobmsg_add_u8(&bb, "network", health->network_health);
    blobmsg_add_u8(&bb, "database", health->database_health);
    blobmsg_add_u8(&bb, "time", health->time_health);
    blobmsg_add_u8(&bb, "logs", health->logs_health);
    blobmsg_close_table(&bb, components);
    
    // System resource information
    unsigned long total_mem, available_mem;
    double load1, load5, load15;
    
    get_system_memory_usage(&total_mem, &available_mem);
    get_system_load_average(&load1, &load5, &load15);
    
    struct blob_attr *resources = blobmsg_open_table(&bb, "resources");
    blobmsg_add_u64(&bb, "total_memory", total_mem);
    blobmsg_add_u64(&bb, "available_memory", available_mem);
    blobmsg_add_double(&bb, "load_1min", (float)load1);
    blobmsg_add_double(&bb, "load_5min", (float)load5);
    blobmsg_add_double(&bb, "load_15min", (float)load15);
    blobmsg_close_table(&bb, resources);
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

// System maintenance UBUS method
int autonomy_system_maintenance(struct ubus_context *uctx, struct ubus_object *obj,
                               struct ubus_request_data *req, const char *method,
                               struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // For now, just acknowledge the maintenance request
    // In a real implementation, this would perform actual maintenance tasks
    blobmsg_add_string(&bb, "status", "maintenance_requested");
    blobmsg_add_string(&bb, "message", "System maintenance initiated");
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

// System restart services UBUS method
int autonomy_system_restart_services(struct ubus_context *uctx, struct ubus_object *obj,
                                    struct ubus_request_data *req, const char *method,
                                    struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // For now, just acknowledge the restart request
    // In a real implementation, this would restart specified services
    blobmsg_add_string(&bb, "status", "restart_requested");
    blobmsg_add_string(&bb, "message", "Service restart initiated");
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}
