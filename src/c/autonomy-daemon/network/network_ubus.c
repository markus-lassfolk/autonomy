#include "autonomy_types.h"
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <time.h>
#include <string.h>
#include <math.h>

extern struct autonomy_state g_state;

// Network management method handlers
int autonomy_network_status(struct ubus_context *uctx, struct ubus_object *obj,
                           struct ubus_request_data *req, const char *method,
                           struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    blobmsg_add_string(&bb, "active_interface", g_state.active_interface);
    blobmsg_add_u8(&bb, "failover_enabled", g_state.failover_enabled);
    blobmsg_add_double(&bb, "network_health_score", g_state.network_health_score);
    blobmsg_add_u32(&bb, "interface_count", g_state.interface_count);
    blobmsg_add_u32(&bb, "last_network_check", g_state.last_network_check);
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_network_interfaces(struct ubus_context *uctx, struct ubus_object *obj,
                               struct ubus_request_data *req, const char *method,
                               struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Add interfaces array
    void *interfaces = blobmsg_open_array(&bb, "interfaces");
    for (int i = 0; i < g_state.interface_count; i++) {
        void *iface = blobmsg_open_table(&bb, NULL);
        blobmsg_add_string(&bb, "name", g_state.interfaces[i].name);
        blobmsg_add_string(&bb, "type", g_state.interfaces[i].type);
        blobmsg_add_u8(&bb, "enabled", g_state.interfaces[i].enabled);
        blobmsg_add_double(&bb, "latency", g_state.interfaces[i].latency);
        blobmsg_add_double(&bb, "loss", g_state.interfaces[i].loss);
        blobmsg_add_u32(&bb, "signal_strength", g_state.interfaces[i].signal_strength);
        blobmsg_add_u32(&bb, "bandwidth", g_state.interfaces[i].bandwidth);
        blobmsg_add_u32(&bb, "health_score", g_state.interfaces[i].health_score);
        blobmsg_add_string(&bb, "status", g_state.interfaces[i].status);
        blobmsg_close_table(&bb, iface);
    }
    blobmsg_close_array(&bb, interfaces);
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_network_health_check(struct ubus_context *uctx, struct ubus_object *obj,
                                 struct ubus_request_data *req, const char *method,
                                 struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    // Perform the health check
    perform_network_health_check();
    
    blob_buf_init(&bb, 0);
    blobmsg_add_string(&bb, "result", "health_check_completed");
    blobmsg_add_double(&bb, "network_health_score", g_state.network_health_score);
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_network_failover(struct ubus_context *uctx, struct ubus_object *obj,
                             struct ubus_request_data *req, const char *method,
                             struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    // Simple failover logic - find the best interface
    int best_interface = -1;
    int best_score = -1;
    
    for (int i = 0; i < g_state.interface_count; i++) {
        if (g_state.interfaces[i].enabled && g_state.interfaces[i].health_score > best_score) {
            best_score = g_state.interfaces[i].health_score;
            best_interface = i;
        }
    }
    
    if (best_interface >= 0) {
        strcpy(g_state.active_interface, g_state.interfaces[best_interface].name);
        g_state.last_failover = time(NULL);
    }
    
    blob_buf_init(&bb, 0);
    blobmsg_add_string(&bb, "result", "failover_completed");
    blobmsg_add_string(&bb, "new_active_interface", g_state.active_interface);
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}
