#include "../core/types.h"
#include "network_discovery_comprehensive.h"
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <time.h>
#include <string.h>

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

// Comprehensive network interface information method
int autonomy_network_interfaces_detailed(struct ubus_context *uctx, struct ubus_object *obj,
                                       struct ubus_request_data *req, const char *method,
                                       struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    // Get comprehensive interface information
    network_interface_t interfaces[MAX_INTERFACES];
    int interface_count = 0;
    
    // Call our enhanced discovery function
    int ret = get_comprehensive_interface_info(interfaces, &interface_count);
    if (ret != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&bb, "error", "Failed to get interface information");
        blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
        ubus_send_reply(uctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    // Add interfaces array
    void *interfaces_array = blobmsg_open_array(&bb, "interfaces");
    for (int i = 0; i < interface_count; i++) {
        void *iface = blobmsg_open_table(&bb, NULL);
        
        // Basic info
        blobmsg_add_string(&bb, "name", interfaces[i].name);
        blobmsg_add_string(&bb, "friendly_name", interfaces[i].friendly_name);
        blobmsg_add_string(&bb, "type", interfaces[i].type);
        blobmsg_add_string(&bb, "subtype", interfaces[i].subtype);
        
        // Network configuration
        blobmsg_add_u8(&bb, "up", interfaces[i].up);
        blobmsg_add_string(&bb, "ip_address", interfaces[i].ip_address);
        blobmsg_add_string(&bb, "gateway", interfaces[i].gateway);
        blobmsg_add_string(&bb, "mac_address", interfaces[i].mac_address);
        blobmsg_add_u32(&bb, "mtu", interfaces[i].mtu);
        blobmsg_add_u32(&bb, "metric", interfaces[i].metric);
        blobmsg_add_string(&bb, "dns_servers", interfaces[i].dns_servers);
        blobmsg_add_string(&bb, "protocol", interfaces[i].protocol);
        blobmsg_add_string(&bb, "device", interfaces[i].device);
        
        // MWAN3 info
        blobmsg_add_string(&bb, "mwan3_name", interfaces[i].mwan3_name);
        blobmsg_add_u8(&bb, "mwan3_tracking_enabled", interfaces[i].mwan3_tracking_enabled);
        blobmsg_add_u8(&bb, "mwan3_available", interfaces[i].mwan3_available);
        blobmsg_add_string(&bb, "mwan3_status", interfaces[i].mwan3_status);
        blobmsg_add_u32(&bb, "mwan3_metric", interfaces[i].mwan3_metric);
        
        // VPN info
        blobmsg_add_u8(&bb, "is_vpn", interfaces[i].is_vpn);
        blobmsg_add_string(&bb, "vpn_type", interfaces[i].vpn_type);
        blobmsg_add_string(&bb, "vpn_name", interfaces[i].vpn_name);
        
        // Starlink info
        blobmsg_add_u8(&bb, "is_starlink", interfaces[i].is_starlink);
        blobmsg_add_string(&bb, "starlink_dish_id", interfaces[i].starlink_dish_id);
        blobmsg_add_string(&bb, "starlink_dish_name", interfaces[i].starlink_dish_name);
        blobmsg_add_string(&bb, "starlink_ip", interfaces[i].starlink_ip);
        
        // Cellular info
        blobmsg_add_string(&bb, "modem_model", interfaces[i].modem_model);
        blobmsg_add_string(&bb, "sim_id", interfaces[i].sim_id);
        blobmsg_add_string(&bb, "operator", interfaces[i].operator);
        blobmsg_add_u32(&bb, "signal_strength", interfaces[i].signal_strength);
        blobmsg_add_string(&bb, "modem_id", interfaces[i].modem_id);
        
        // WiFi info
        blobmsg_add_string(&bb, "ssid", interfaces[i].ssid);
        blobmsg_add_string(&bb, "wifi_band", interfaces[i].wifi_band);
        blobmsg_add_string(&bb, "wifi_mode", interfaces[i].wifi_mode);
        blobmsg_add_string(&bb, "wifi_encryption", interfaces[i].wifi_encryption);
        
        // Health info
        blobmsg_add_double(&bb, "latency", interfaces[i].latency);
        blobmsg_add_double(&bb, "packet_loss", interfaces[i].packet_loss);
        blobmsg_add_double(&bb, "health_score", interfaces[i].health_score);
        
        // Statistics
        blobmsg_add_u64(&bb, "rx_bytes", interfaces[i].rx_bytes);
        blobmsg_add_u64(&bb, "tx_bytes", interfaces[i].tx_bytes);
        blobmsg_add_u64(&bb, "rx_packets", interfaces[i].rx_packets);
        blobmsg_add_u64(&bb, "tx_packets", interfaces[i].tx_packets);
        blobmsg_add_u64(&bb, "rx_errors", interfaces[i].rx_errors);
        blobmsg_add_u64(&bb, "tx_errors", interfaces[i].tx_errors);
        
        // Timestamps
        blobmsg_add_u32(&bb, "last_check", (uint32_t)interfaces[i].last_check);
        blobmsg_add_u32(&bb, "last_seen", (uint32_t)interfaces[i].last_seen);
        blobmsg_add_u32(&bb, "last_health_check", (uint32_t)interfaces[i].last_health_check);
        
        blobmsg_close_table(&bb, iface);
    }
    blobmsg_close_array(&bb, interfaces_array);
    
    blobmsg_add_u32(&bb, "interface_count", interface_count);
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}
