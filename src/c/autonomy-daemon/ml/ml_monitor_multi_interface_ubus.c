#include <stdlib.h>
#include "ml_monitor_multi_interface.h"
#include "ml_monitor_ubus.h"
#include "../utils/logx.h"
#include <libubox/blobmsg_json.h>
#include <json-c/json.h>
#include <string.h>

// Multi-Interface UBUS Methods

// UBUS method: get_multi_interface_status
int ml_monitor_ubus_get_multi_interface_status(struct ubus_context *ctx, struct ubus_object *obj,
                                              struct ubus_request_data *req, const char *method,
                                              struct blob_attr *msg) {
    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    
    multi_interface_ml_system_t *system = ml_monitor_get_multi_interface_system();
    
    if (!system) {
        blobmsg_add_string(&b, "error", "Multi-interface ML system not initialized");
        blobmsg_add_u32(&b, "code", ML_MONITOR_MULTI_ERROR_NOT_FOUND);
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_OK;
    }
    
    blobmsg_add_string(&b, "system", "Multi-Interface ML Intelligence");
    blobmsg_add_u32(&b, "interface_count", system->interface_count);
    blobmsg_add_u8(&b, "continuous_monitoring", system->failover_intelligence.continuous_monitoring_during_failover);
    blobmsg_add_u8(&b, "predictive_failback", system->failover_intelligence.enable_predictive_failback);
    blobmsg_add_u8(&b, "duration_prediction", system->failover_intelligence.enable_outage_duration_prediction);
    blobmsg_add_u8(&b, "dynamic_weights", system->mwan3_integration.enable_dynamic_weight_updates);
    
    // Interface details
    void *interfaces_array = blobmsg_open_array(&b, "interfaces");
    for (int i = 0; i < system->interface_count; i++) {
        interface_ml_model_t *model = &system->interface_models[i];
        
        void *interface_table = blobmsg_open_table(&b, NULL);
        blobmsg_add_string(&b, "interface_id", model->interface_id);
        blobmsg_add_u32(&b, "type", model->type);
        blobmsg_add_u8(&b, "active", model->active);
        blobmsg_add_double(&b, "typical_latency_ms", model->performance.typical_latency_ms);
        blobmsg_add_double(&b, "typical_throughput_mbps", model->performance.typical_throughput_mbps);
        blobmsg_add_double(&b, "reliability", model->performance.typical_reliability);
        blobmsg_add_u32(&b, "predictions_made", model->performance.total_predictions);
        blobmsg_add_double(&b, "accuracy", model->performance.accuracy);
        blobmsg_close_table(&b, interface_table);
    }
    blobmsg_close_array(&b, interfaces_array);
    
    // MWAN3 integration status
    void *mwan3_table = blobmsg_open_table(&b, "mwan3_integration");
    blobmsg_add_u8(&b, "dynamic_weights_enabled", system->mwan3_integration.enable_dynamic_weight_updates);
    blobmsg_add_u8(&b, "auto_apply", system->mwan3_integration.auto_apply_weight_changes);
    blobmsg_add_double(&b, "sensitivity", system->mwan3_integration.weight_adjustment_sensitivity);
    blobmsg_add_u32(&b, "update_interval", system->mwan3_integration.weight_update_interval_seconds);
    blobmsg_close_table(&b, mwan3_table);
    
    blobmsg_add_u32(&b, "timestamp", time(NULL));
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// UBUS method: predict_interface_outage
int ml_monitor_ubus_predict_interface_outage(struct ubus_context *ctx, struct ubus_object *obj,
                                            struct ubus_request_data *req, const char *method,
                                            struct blob_attr *msg) {
    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    
    // Parse interface_id from request
    struct blob_attr *tb[1];
    static const struct blobmsg_policy policy[1] = {
        [0] = { .name = "interface_id", .type = BLOBMSG_TYPE_STRING },
    };
    
    blobmsg_parse(policy, 1, tb, blob_data(msg), blob_len(msg));
    
    if (!tb[0]) {
        blobmsg_add_string(&b, "error", "Missing interface_id parameter");
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_OK;
    }
    
    const char *interface_id = blobmsg_get_string(tb[0]);
    multi_interface_ml_system_t *system = ml_monitor_get_multi_interface_system();
    
    if (!system) {
        blobmsg_add_string(&b, "error", "Multi-interface system not initialized");
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_OK;
    }
    
    // Get predictions
    uint8_t outage_prob, performance_score, confidence;
    int result = ml_monitor_predict_interface_performance(system, interface_id,
                                                        &outage_prob, &performance_score, &confidence);
    
    if (result == ML_MONITOR_MULTI_SUCCESS) {
        blobmsg_add_string(&b, "interface_id", interface_id);
        blobmsg_add_u32(&b, "outage_probability", outage_prob);
        blobmsg_add_u32(&b, "performance_score", performance_score);
        blobmsg_add_u32(&b, "confidence", confidence);
        
        // Get outage duration prediction
        outage_duration_prediction_t duration_pred;
        if (ml_monitor_predict_outage_duration(system, interface_id, &duration_pred) == ML_MONITOR_MULTI_SUCCESS) {
            void *duration_table = blobmsg_open_table(&b, "duration_prediction");
            blobmsg_add_u32(&b, "expected_duration_seconds", duration_pred.expected_duration_seconds);
            blobmsg_add_u8(&b, "very_short_prob", duration_pred.very_short_probability);
            blobmsg_add_u8(&b, "short_prob", duration_pred.short_probability);
            blobmsg_add_u8(&b, "medium_prob", duration_pred.medium_probability);
            blobmsg_add_u8(&b, "long_prob", duration_pred.long_probability);
            blobmsg_add_u8(&b, "recommend_failover", duration_pred.recommend_failover);
            blobmsg_add_string(&b, "reasoning", duration_pred.reasoning);
            blobmsg_close_table(&b, duration_table);
        }
        
        // Get failback readiness
        failback_readiness_t failback;
        if (ml_monitor_assess_failback_readiness(system, interface_id, &failback) == ML_MONITOR_MULTI_SUCCESS) {
            void *failback_table = blobmsg_open_table(&b, "failback_readiness");
            blobmsg_add_u32(&b, "recommended_delay_seconds", failback.recommended_failback_delay);
            blobmsg_add_u32(&b, "success_probability", failback.failback_success_probability);
            blobmsg_add_u32(&b, "confidence", failback.failback_confidence);
            blobmsg_add_double(&b, "immediate_risk", failback.risk_of_immediate_failback);
            blobmsg_add_double(&b, "delayed_risk", failback.risk_of_delayed_failback);
            blobmsg_close_table(&b, failback_table);
        }
        
        blobmsg_add_u32(&b, "timestamp", time(NULL));
        blobmsg_add_string(&b, "status", "prediction_successful");
    } else {
        blobmsg_add_string(&b, "error", "Failed to get interface prediction");
        blobmsg_add_u32(&b, "code", result);
    }
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// UBUS method: update_mwan3_weights
int ml_monitor_ubus_update_mwan3_weights(struct ubus_context *ctx, struct ubus_object *obj,
                                        struct ubus_request_data *req, const char *method,
                                        struct blob_attr *msg) {
    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    
    multi_interface_ml_system_t *system = ml_monitor_get_multi_interface_system();
    
    if (!system) {
        blobmsg_add_string(&b, "error", "Multi-interface system not initialized");
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_OK;
    }
    
    // Update MWAN3 weights based on ML predictions
    int result = ml_monitor_update_mwan3_weights(system);
    
    if (result == ML_MONITOR_MULTI_SUCCESS) {
        blobmsg_add_string(&b, "status", "mwan3_weights_updated");
        blobmsg_add_u32(&b, "interfaces_updated", system->interface_count);
        
        // Show current weights
        void *weights_array = blobmsg_open_array(&b, "current_weights");
        for (int i = 0; i < system->mwan3_integration.mwan3_interface_count; i++) {
            void *weight_table = blobmsg_open_table(&b, NULL);
            blobmsg_add_string(&b, "interface", system->mwan3_integration.mwan3_interfaces[i].interface_name);
            blobmsg_add_u32(&b, "base_weight", system->mwan3_integration.mwan3_interfaces[i].base_weight);
            blobmsg_add_u32(&b, "ml_adjustment", system->mwan3_integration.mwan3_interfaces[i].ml_weight_adjustment);
            blobmsg_add_u32(&b, "current_weight", system->mwan3_integration.mwan3_interfaces[i].current_weight);
            blobmsg_add_double(&b, "ml_reliability", system->mwan3_integration.mwan3_interfaces[i].ml_reliability_score);
            blobmsg_close_table(&b, weight_table);
        }
        blobmsg_close_array(&b, weights_array);
        
        blobmsg_add_u32(&b, "timestamp", time(NULL));
        
        LOGX_INFO_MSG("MWAN3 weights updated via UBUS");
    } else {
        blobmsg_add_string(&b, "error", "Failed to update MWAN3 weights");
        blobmsg_add_u32(&b, "code", result);
    }
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// UBUS method: add_interface_monitoring
int ml_monitor_ubus_add_interface_monitoring(struct ubus_context *ctx, struct ubus_object *obj,
                                            struct ubus_request_data *req, const char *method,
                                            struct blob_attr *msg) {
    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    
    // Parse parameters
    struct blob_attr *tb[2];
    static const struct blobmsg_policy policy[2] = {
        [0] = { .name = "interface_id", .type = BLOBMSG_TYPE_STRING },
        [1] = { .name = "interface_type", .type = BLOBMSG_TYPE_INT32 },
    };
    
    blobmsg_parse(policy, 2, tb, blob_data(msg), blob_len(msg));
    
    if (!tb[0] || !tb[1]) {
        blobmsg_add_string(&b, "error", "Missing interface_id or interface_type parameter");
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_OK;
    }
    
    const char *interface_id = blobmsg_get_string(tb[0]);
    interface_type_t type = (interface_type_t)blobmsg_get_u32(tb[1]);
    
    multi_interface_ml_system_t *system = ml_monitor_get_multi_interface_system();
    if (!system) {
        blobmsg_add_string(&b, "error", "Multi-interface system not initialized");
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_OK;
    }
    
    // Add interface
    int result = ml_monitor_add_interface(system, interface_id, type);
    
    if (result == ML_MONITOR_MULTI_SUCCESS) {
        blobmsg_add_string(&b, "status", "interface_added");
        blobmsg_add_string(&b, "interface_id", interface_id);
        blobmsg_add_u32(&b, "interface_type", type);
        blobmsg_add_u32(&b, "total_interfaces", system->interface_count);
        
        const char* type_names[] = {"Starlink", "Cellular", "WiFi", "LAN", "Unknown"};
        LOGX_INFO_MSG("Added interface %s (%s) to ML monitoring", interface_id, type_names[type]);
    } else {
        blobmsg_add_string(&b, "error", "Failed to add interface");
        blobmsg_add_u32(&b, "code", result);
    }
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// UBUS method: validate_failover_prediction
int ml_monitor_ubus_validate_failover_prediction(struct ubus_context *ctx, struct ubus_object *obj,
                                                 struct ubus_request_data *req, const char *method,
                                                 struct blob_attr *msg) {
    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    
    // Parse parameters
    struct blob_attr *tb[3];
    static const struct blobmsg_policy policy[3] = {
        [0] = { .name = "interface_id", .type = BLOBMSG_TYPE_STRING },
        [1] = { .name = "actual_outage", .type = BLOBMSG_TYPE_BOOL },
        [2] = { .name = "duration_seconds", .type = BLOBMSG_TYPE_INT32 },
    };
    
    blobmsg_parse(policy, 3, tb, blob_data(msg), blob_len(msg));
    
    if (!tb[0] || !tb[1]) {
        blobmsg_add_string(&b, "error", "Missing required parameters");
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_OK;
    }
    
    const char *interface_id = blobmsg_get_string(tb[0]);
    bool actual_outage = blobmsg_get_bool(tb[1]);
    uint32_t duration_seconds = tb[2] ? blobmsg_get_u32(tb[2]) : 0;
    
    multi_interface_ml_system_t *system = ml_monitor_get_multi_interface_system();
    if (!system) {
        blobmsg_add_string(&b, "error", "Multi-interface system not initialized");
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_OK;
    }
    
    // Validate prediction
    int result = ml_monitor_validate_failover_prediction(system, interface_id, actual_outage, duration_seconds);
    
    if (result == ML_MONITOR_MULTI_SUCCESS) {
        blobmsg_add_string(&b, "status", "prediction_validated");
        blobmsg_add_string(&b, "interface_id", interface_id);
        blobmsg_add_u8(&b, "actual_outage", actual_outage);
        blobmsg_add_u32(&b, "duration_seconds", duration_seconds);
        blobmsg_add_u32(&b, "validation_timestamp", time(NULL));
        
        LOGX_INFO_MSG("Failover prediction validated for %s: outage=%s, duration=%u seconds",
                 interface_id, actual_outage ? "yes" : "no", duration_seconds);
    } else {
        blobmsg_add_string(&b, "error", "Failed to validate prediction");
        blobmsg_add_u32(&b, "code", result);
    }
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}