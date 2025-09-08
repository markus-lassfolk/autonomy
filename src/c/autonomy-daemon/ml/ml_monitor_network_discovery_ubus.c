#include "ml_monitor_network_discovery_integration.h"
#include "ml_monitor_ubus.h"
#include "../utils/logx.h"
#include <libubox/blobmsg_json.h>
#include <json-c/json.h>
#include <string.h>

// UBUS methods for network discovery integration

// UBUS method: get_discovered_interfaces
int ml_monitor_ubus_get_discovered_interfaces(struct ubus_context *ctx, struct ubus_object *obj,
                                             struct ubus_request_data *req, const char *method,
                                             struct blob_attr *msg) {
    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    
    // Get enhanced comprehensive interface information
    network_interface_t discovered_interfaces[MAX_INTERFACES];
    int interface_count = 0;
    
    extern int get_enhanced_comprehensive_interface_info(network_interface_t *interfaces, int *count);
    int discovery_result = get_enhanced_comprehensive_interface_info(discovered_interfaces, &interface_count);
    
    if (discovery_result == AUTONOMY_SUCCESS) {
        blobmsg_add_u32(&b, "total_interfaces_discovered", interface_count);
        
        // Add interface details
        void *interfaces_array = blobmsg_open_array(&b, "interfaces");
        
        for (int i = 0; i < interface_count; i++) {
            network_interface_t *interface = &discovered_interfaces[i];
            
            void *interface_table = blobmsg_open_table(&b, NULL);
            
            // Basic information
            blobmsg_add_string(&b, "name", interface->name);
            blobmsg_add_string(&b, "friendly_name", interface->friendly_name);
            blobmsg_add_string(&b, "type", interface->type);
            blobmsg_add_u8(&b, "up", interface->up);
            blobmsg_add_u8(&b, "enabled", interface->enabled);
            blobmsg_add_double(&b, "health_score", interface->health_score);
            
            // MWAN3 information
            blobmsg_add_string(&b, "mwan3_name", interface->mwan3_name);
            blobmsg_add_u8(&b, "mwan3_tracking_enabled", interface->mwan3_tracking_enabled);
            blobmsg_add_u8(&b, "mwan3_available", interface->mwan3_available);
            blobmsg_add_string(&b, "mwan3_status", interface->mwan3_status);
            
            // ML monitoring suitability
            interface_type_t ml_type = ml_monitor_map_interface_type(interface);
            bool suitable_for_ml = ml_monitor_is_interface_suitable_for_ml(interface);
            
            blobmsg_add_string(&b, "ml_type", ml_monitor_get_interface_type_string(ml_type));
            blobmsg_add_u8(&b, "suitable_for_ml", suitable_for_ml);
            
            // ML recommendations if suitable
            if (suitable_for_ml) {
                double reliability_score;
                int recommended_weight;
                bool recommend_failover;
                
                if (ml_monitor_get_interface_recommendations(interface->name, &reliability_score,
                                                           &recommended_weight, &recommend_failover) == ML_MONITOR_SUCCESS) {
                    void *ml_table = blobmsg_open_table(&b, "ml_recommendations");
                    blobmsg_add_double(&b, "reliability_score", reliability_score);
                    blobmsg_add_u32(&b, "recommended_mwan3_weight", recommended_weight);
                    blobmsg_add_u8(&b, "recommend_for_failover", recommend_failover);
                    blobmsg_close_table(&b, ml_table);
                }
            }
            
            // Interface-specific information
            if (interface->is_starlink) {
                void *starlink_table = blobmsg_open_table(&b, "starlink_info");
                blobmsg_add_string(&b, "dish_id", interface->starlink_dish_id);
                blobmsg_add_string(&b, "starlink_ip", interface->starlink_ip);
                blobmsg_close_table(&b, starlink_table);
            }
            
            if (strlen(interface->modem_model) > 0) {
                void *cellular_table = blobmsg_open_table(&b, "cellular_info");
                blobmsg_add_string(&b, "modem_model", interface->modem_model);
                blobmsg_add_string(&b, "sim_id", interface->sim_id);
                blobmsg_add_string(&b, "modem_id", interface->modem_id);
                blobmsg_close_table(&b, cellular_table);
            }
            
            if (strlen(interface->ssid) > 0) {
                void *wifi_table = blobmsg_open_table(&b, "wifi_info");
                blobmsg_add_string(&b, "ssid", interface->ssid);
                blobmsg_add_string(&b, "wifi_band", interface->wifi_band);
                blobmsg_add_string(&b, "wifi_mode", interface->wifi_mode);
                blobmsg_close_table(&b, wifi_table);
            }
            
            blobmsg_close_table(&b, interface_table);
        }
        
        blobmsg_close_array(&b, interfaces_array);
        
        blobmsg_add_u32(&b, "timestamp", time(NULL));
        blobmsg_add_string(&b, "status", "discovery_successful");
    } else {
        blobmsg_add_string(&b, "error", "Failed to get comprehensive interface info");
        blobmsg_add_u32(&b, "code", discovery_result);
    }
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// UBUS method: sync_with_network_discovery
int ml_monitor_ubus_sync_with_network_discovery(struct ubus_context *ctx, struct ubus_object *obj,
                                               struct ubus_request_data *req, const char *method,
                                               struct blob_attr *msg) {
    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    
    ml_monitor_t *monitor = ml_monitor_get_instance();
    
    if (!monitor) {
        blobmsg_add_string(&b, "error", "ML monitor not initialized");
        blobmsg_add_u32(&b, "code", ML_MONITOR_ERROR_NOT_INITIALIZED);
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_OK;
    }
    
    // Trigger sync with network discovery
    int sync_result = ml_monitor_sync_with_network_discovery(monitor);
    
    if (sync_result == ML_MONITOR_SUCCESS) {
        // Get updated interface count
        multi_interface_ml_system_t *multi_system = ml_monitor_get_multi_interface_system();
        
        blobmsg_add_string(&b, "status", "sync_completed");
        blobmsg_add_u32(&b, "interfaces_monitored", multi_system ? multi_system->interface_count : 0);
        blobmsg_add_u32(&b, "sync_timestamp", time(NULL));
        
        LOGX_INFO("Network discovery sync completed via UBUS");
    } else {
        blobmsg_add_string(&b, "error", "Failed to sync with network discovery");
        blobmsg_add_u32(&b, "code", sync_result);
    }
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// UBUS method: get_interface_ml_recommendations
int ml_monitor_ubus_get_interface_ml_recommendations(struct ubus_context *ctx, struct ubus_object *obj,
                                                    struct ubus_request_data *req, const char *method,
                                                    struct blob_attr *msg) {
    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    
    // Parse interface name from request
    struct blob_attr *tb[1];
    static const struct blobmsg_policy policy[1] = {
        [0] = { .name = "interface_name", .type = BLOBMSG_TYPE_STRING },
    };
    
    blobmsg_parse(policy, 1, tb, blob_data(msg), blob_len(msg));
    
    if (!tb[0]) {
        blobmsg_add_string(&b, "error", "Missing interface_name parameter");
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_OK;
    }
    
    const char *interface_name = blobmsg_get_string(tb[0]);
    
    // Get ML recommendations
    double reliability_score;
    int recommended_weight;
    bool recommend_failover;
    
    int result = ml_monitor_get_interface_recommendations(interface_name, &reliability_score,
                                                        &recommended_weight, &recommend_failover);
    
    if (result == ML_MONITOR_SUCCESS) {
        blobmsg_add_string(&b, "interface_name", interface_name);
        blobmsg_add_double(&b, "ml_reliability_score", reliability_score);
        blobmsg_add_u32(&b, "recommended_mwan3_weight", recommended_weight);
        blobmsg_add_u8(&b, "recommend_for_failover", recommend_failover);
        
        // Get enhanced interface info
        char friendly_name[64] = {0};
        char mwan3_name[32] = {0};
        bool mwan3_tracking;
        double health_score;
        interface_type_t ml_type;
        
        if (ml_monitor_get_enhanced_interface_info(interface_name, friendly_name, mwan3_name,
                                                 &mwan3_tracking, &health_score, &ml_type) == ML_MONITOR_SUCCESS) {
            blobmsg_add_string(&b, "friendly_name", friendly_name);
            blobmsg_add_string(&b, "mwan3_name", mwan3_name);
            blobmsg_add_u8(&b, "mwan3_tracking_enabled", mwan3_tracking);
            blobmsg_add_double(&b, "health_score", health_score);
            blobmsg_add_string(&b, "ml_type", ml_monitor_get_interface_type_string(ml_type));
        }
        
        blobmsg_add_u32(&b, "timestamp", time(NULL));
        blobmsg_add_string(&b, "status", "recommendations_available");
    } else {
        blobmsg_add_string(&b, "error", "Failed to get ML recommendations");
        blobmsg_add_u32(&b, "code", result);
    }
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}