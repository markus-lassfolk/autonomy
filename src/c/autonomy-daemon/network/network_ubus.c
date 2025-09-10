#include <stdlib.h>
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
    extern int get_enhanced_comprehensive_interface_info(network_interface_t *interfaces, int *count);
    int ret = get_enhanced_comprehensive_interface_info(interfaces, &interface_count);
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
        blobmsg_add_string(&bb, "cellular_device_path", interfaces[i].cellular_device_path);
        
        // WiFi info
        blobmsg_add_string(&bb, "ssid", interfaces[i].ssid);
        blobmsg_add_string(&bb, "wifi_band", interfaces[i].wifi_band);
        blobmsg_add_string(&bb, "wifi_mode", interfaces[i].wifi_mode);
        blobmsg_add_string(&bb, "wifi_encryption", interfaces[i].wifi_encryption);
        
        // Health info
        blobmsg_add_double(&bb, "latency", interfaces[i].latency);
        blobmsg_add_double(&bb, "packet_loss", interfaces[i].packet_loss);
        blobmsg_add_double(&bb, "health_score", interfaces[i].health_score);
        
        // Enhanced real-time metrics
        void *realtime_table = blobmsg_open_table(&bb, "real_time_metrics");
        blobmsg_add_u32(&bb, "ping_latency_ms", interfaces[i].real_time_metrics.ping_latency_ms);
        blobmsg_add_u32(&bb, "ping_success_rate", interfaces[i].real_time_metrics.ping_success_rate);
        blobmsg_add_u32(&bb, "consecutive_ping_failures", interfaces[i].real_time_metrics.consecutive_ping_failures);
        blobmsg_add_u32(&bb, "total_ping_tests", interfaces[i].real_time_metrics.total_ping_tests);
        blobmsg_add_u32(&bb, "successful_pings", interfaces[i].real_time_metrics.successful_pings);
        blobmsg_add_u32(&bb, "ping_jitter_ms", interfaces[i].real_time_metrics.ping_jitter_ms);
        blobmsg_add_u32(&bb, "ping_min_ms", interfaces[i].real_time_metrics.ping_min_ms);
        blobmsg_add_u32(&bb, "ping_max_ms", interfaces[i].real_time_metrics.ping_max_ms);
        blobmsg_add_u8(&bb, "mwan3_ping_active", interfaces[i].real_time_metrics.mwan3_ping_active);
        blobmsg_add_u32(&bb, "mwan3_ping_interval", interfaces[i].real_time_metrics.mwan3_ping_interval);
        blobmsg_add_u32(&bb, "last_mwan3_ping", (uint32_t)interfaces[i].real_time_metrics.last_mwan3_ping);
        blobmsg_add_u32(&bb, "mwan3_ping_success_rate", interfaces[i].real_time_metrics.mwan3_ping_success_rate);
        blobmsg_close_table(&bb, realtime_table);
        
        // Performance history trends
        if (interfaces[i].performance_history.history_count >= 10) {
            void *trends_table = blobmsg_open_table(&bb, "performance_trends");
            blobmsg_add_double(&bb, "latency_trend", interfaces[i].performance_history.latency_trend);
            blobmsg_add_double(&bb, "loss_trend", interfaces[i].performance_history.loss_trend);
            blobmsg_add_double(&bb, "health_trend", interfaces[i].performance_history.health_trend);
            blobmsg_add_u32(&bb, "history_count", interfaces[i].performance_history.history_count);
            blobmsg_add_u32(&bb, "history_start_time", (uint32_t)interfaces[i].performance_history.history_start_time);
            blobmsg_close_table(&bb, trends_table);
        }
        
        // Enhanced cellular information
        if (strcmp(interfaces[i].type, "cellular") == 0 && 
            interfaces[i].enhanced_cellular_info.signal_strength_dbm != 0) {
            void *cellular_table = blobmsg_open_table(&bb, "enhanced_cellular_info");
            blobmsg_add_string(&bb, "operator_name", interfaces[i].enhanced_cellular_info.operator_name);
            blobmsg_add_string(&bb, "network_technology", interfaces[i].enhanced_cellular_info.network_technology);
            blobmsg_add_u32(&bb, "signal_strength_dbm", interfaces[i].enhanced_cellular_info.signal_strength_dbm);
            blobmsg_add_string(&bb, "cell_id", interfaces[i].enhanced_cellular_info.cell_id);
            blobmsg_add_u32(&bb, "signal_quality", interfaces[i].enhanced_cellular_info.signal_quality);
            blobmsg_add_u32(&bb, "rsrp_dbm", interfaces[i].enhanced_cellular_info.rsrp_dbm);
            blobmsg_add_u32(&bb, "rsrq_db", interfaces[i].enhanced_cellular_info.rsrq_db);
            blobmsg_add_u32(&bb, "sinr_db", interfaces[i].enhanced_cellular_info.sinr_db);
            blobmsg_close_table(&bb, cellular_table);
        }
        
        // Statistics
        blobmsg_add_u64(&bb, "rx_bytes", interfaces[i].rx_bytes);
        blobmsg_add_u64(&bb, "tx_bytes", interfaces[i].tx_bytes);
        blobmsg_add_u64(&bb, "rx_packets", interfaces[i].rx_packets);
        blobmsg_add_u64(&bb, "tx_packets", interfaces[i].tx_packets);
        blobmsg_add_u64(&bb, "rx_errors", interfaces[i].rx_errors);
        blobmsg_add_u64(&bb, "tx_errors", interfaces[i].tx_errors);
        
        // ML monitoring recommendations
        void *ml_table = blobmsg_open_table(&bb, "ml_monitoring_recommendations");
        
        // Get monitoring strategy recommendations
        extern int ml_monitor_get_monitoring_frequency_recommendation(const network_interface_t *interface);
        extern bool ml_monitor_should_use_mwan3_ping_results(const network_interface_t *interface);
        extern bool ml_monitor_is_interface_suitable_for_ml(const network_interface_t *interface);
        
        int recommended_frequency = ml_monitor_get_monitoring_frequency_recommendation(&interfaces[i]);
        bool use_mwan3_pings = ml_monitor_should_use_mwan3_ping_results(&interfaces[i]);
        bool suitable_for_ml = ml_monitor_is_interface_suitable_for_ml(&interfaces[i]);
        
        blobmsg_add_u32(&bb, "recommended_monitoring_frequency_seconds", recommended_frequency);
        blobmsg_add_u8(&bb, "use_mwan3_ping_results", use_mwan3_pings);
        blobmsg_add_u8(&bb, "suitable_for_ml_monitoring", suitable_for_ml);
        
        // Add monitoring strategy explanation
        const char *strategy_explanation;
        if (strcmp(interfaces[i].type, "cellular") == 0) {
            if (use_mwan3_pings) {
                strategy_explanation = "Using MWAN3 ping results to minimize cellular data costs";
            } else {
                strategy_explanation = "Minimal monitoring focusing on modem metrics (no data cost)";
            }
        } else if (strcmp(interfaces[i].type, "starlink") == 0 || 
                   strcmp(interfaces[i].type, "wifi") == 0 || 
                   strcmp(interfaces[i].type, "ethernet") == 0) {
            if (use_mwan3_pings) {
                strategy_explanation = "Hybrid monitoring complementing MWAN3 pings";
            } else {
                strategy_explanation = "Full ML monitoring (no data cost concerns)";
            }
        } else {
            strategy_explanation = "Standard monitoring approach";
        }
        blobmsg_add_string(&bb, "monitoring_strategy", strategy_explanation);
        
        blobmsg_close_table(&bb, ml_table);
        
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
