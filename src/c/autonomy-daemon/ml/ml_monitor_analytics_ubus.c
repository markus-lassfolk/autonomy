#include <stdlib.h>
#include "ml_monitor_analytics.h"
#include "ml_monitor_ubus.h"
#include "ml_monitor_network_discovery_integration.h"
#include "../shared/logging/logx.h"
#include <libubox/blobmsg_json.h>
#include <json-c/json.h>
#include <string.h>

// UBUS methods for ML analytics and visualization

// UBUS method: get_ml_analytics_summary
int ml_monitor_ubus_get_analytics_summary(struct ubus_context *ctx, struct ubus_object *obj,
                                         struct ubus_request_data *req, const char *method,
                                         struct blob_attr *msg) {
    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    
    ml_analytics_data_t analytics_data;
    int result = ml_monitor_analytics_get_data(&analytics_data);
    
    if (result == ML_MONITOR_SUCCESS) {
        // Overall statistics
        blobmsg_add_u32(&b, "total_predictions", analytics_data.summary_stats.total_predictions);
        blobmsg_add_u32(&b, "correct_predictions", analytics_data.summary_stats.correct_predictions);
        blobmsg_add_double(&b, "overall_accuracy_pct", analytics_data.summary_stats.overall_accuracy_pct);
        blobmsg_add_u32(&b, "ml_triggered_actions", analytics_data.summary_stats.ml_triggered_actions);
        blobmsg_add_u32(&b, "successful_optimizations", analytics_data.summary_stats.successful_optimizations);
        blobmsg_add_u32(&b, "total_improvement_ms", analytics_data.summary_stats.total_improvement_ms);
        blobmsg_add_double(&b, "average_user_experience", analytics_data.summary_stats.average_user_experience);
        blobmsg_add_u32(&b, "stats_start_time", (uint32_t)analytics_data.summary_stats.stats_start_time);
        
        // Per-interface summary
        void *interfaces_array = blobmsg_open_array(&b, "interface_summaries");
        for (int i = 0; i < MAX_INTERFACES; i++) {
            if (!analytics_data.interface_summary[i].is_active) continue;
            
            void *iface_table = blobmsg_open_table(&b, NULL);
            blobmsg_add_string(&b, "interface_id", analytics_data.interface_summary[i].interface_id);
            blobmsg_add_u32(&b, "predictions_made", analytics_data.interface_summary[i].predictions_made);
            blobmsg_add_u32(&b, "predictions_correct", analytics_data.interface_summary[i].predictions_correct);
            blobmsg_add_double(&b, "accuracy_pct", analytics_data.interface_summary[i].accuracy_pct);
            blobmsg_add_double(&b, "current_score", analytics_data.interface_summary[i].current_score);
            blobmsg_add_double(&b, "best_score", analytics_data.interface_summary[i].best_score);
            blobmsg_add_double(&b, "worst_score", analytics_data.interface_summary[i].worst_score);
            blobmsg_add_u32(&b, "last_update", (uint32_t)analytics_data.interface_summary[i].last_update);
            blobmsg_close_table(&b, iface_table);
        }
        blobmsg_close_array(&b, interfaces_array);
        
        blobmsg_add_u32(&b, "timestamp", (uint32_t)time(NULL));
        blobmsg_add_string(&b, "status", "success");
    } else {
        blobmsg_add_string(&b, "error", "Failed to get analytics data");
        blobmsg_add_u32(&b, "code", result);
    }
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// UBUS method: get_interface_score_history
int ml_monitor_ubus_get_interface_score_history(struct ubus_context *ctx, struct ubus_object *obj,
                                               struct ubus_request_data *req, const char *method,
                                               struct blob_attr *msg) {
    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    
    // Parse parameters
    struct blob_attr *tb[2];
    static const struct blobmsg_policy policy[2] = {
        [0] = { .name = "interface_id", .type = BLOBMSG_TYPE_STRING },
        [1] = { .name = "hours", .type = BLOBMSG_TYPE_INT32 },
    };
    
    blobmsg_parse(policy, 2, tb, blob_data(msg), blob_len(msg));
    
    if (!tb[0]) {
        blobmsg_add_string(&b, "error", "Missing interface_id parameter");
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_OK;
    }
    
    const char *interface_id = blobmsg_get_string(tb[0]);
    uint32_t hours = tb[1] ? blobmsg_get_u32(tb[1]) : 24; // Default 24 hours
    
    // Get score history
    ml_interface_score_t scores[1440]; // Max 24 hours (1 per minute)
    uint32_t actual_count;
    uint32_t max_scores = fmin(1440, hours * 60); // 1 per minute
    
    int result = ml_monitor_analytics_get_interface_score_history(interface_id, scores, max_scores, &actual_count);
    
    if (result == ML_MONITOR_SUCCESS) {
        blobmsg_add_string(&b, "interface_id", interface_id);
        blobmsg_add_u32(&b, "hours_requested", hours);
        blobmsg_add_u32(&b, "data_points", actual_count);
        
        // Score history data
        void *history_array = blobmsg_open_array(&b, "score_history");
        for (uint32_t i = 0; i < actual_count; i++) {
            void *point_table = blobmsg_open_table(&b, NULL);
            blobmsg_add_u32(&b, "timestamp", (uint32_t)scores[i].timestamp);
            blobmsg_add_double(&b, "overall_score", scores[i].overall_score);
            blobmsg_add_double(&b, "accuracy_score", scores[i].accuracy_score);
            blobmsg_add_double(&b, "stability_score", scores[i].stability_score);
            blobmsg_add_double(&b, "performance_score", scores[i].performance_score);
            blobmsg_add_double(&b, "trend_score", scores[i].trend_score);
            
            // Score contributors
            void *contributors_table = blobmsg_open_table(&b, "score_contributors");
            blobmsg_add_double(&b, "latency_impact", scores[i].score_contributors.latency_impact);
            blobmsg_add_double(&b, "loss_impact", scores[i].score_contributors.loss_impact);
            blobmsg_add_double(&b, "signal_impact", scores[i].score_contributors.signal_impact);
            blobmsg_add_double(&b, "prediction_impact", scores[i].score_contributors.prediction_impact);
            blobmsg_add_double(&b, "stability_impact", scores[i].score_contributors.stability_impact);
            blobmsg_add_double(&b, "trend_impact", scores[i].score_contributors.trend_impact);
            blobmsg_close_table(&b, contributors_table);
            
            // Raw metrics
            blobmsg_add_u32(&b, "current_latency_ms", scores[i].current_latency_ms);
            blobmsg_add_u32(&b, "current_loss_pct", scores[i].current_loss_pct);
            blobmsg_add_u32(&b, "current_signal_dbm", scores[i].current_signal_dbm);
            blobmsg_add_u32(&b, "consecutive_stable_minutes", scores[i].consecutive_stable_minutes);
            
            blobmsg_close_table(&b, point_table);
        }
        blobmsg_close_array(&b, history_array);
        
        blobmsg_add_u32(&b, "timestamp", (uint32_t)time(NULL));
        blobmsg_add_string(&b, "status", "success");
    } else {
        blobmsg_add_string(&b, "error", "Failed to get score history");
        blobmsg_add_u32(&b, "code", result);
    }
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// UBUS method: get_prediction_accuracy_trends
int ml_monitor_ubus_get_accuracy_trends(struct ubus_context *ctx, struct ubus_object *obj,
                                       struct ubus_request_data *req, const char *method,
                                       struct blob_attr *msg) {
    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    
    // Parse parameters
    struct blob_attr *tb[2];
    static const struct blobmsg_policy policy[2] = {
        [0] = { .name = "interface_id", .type = BLOBMSG_TYPE_STRING },
        [1] = { .name = "hours", .type = BLOBMSG_TYPE_INT32 },
    };
    
    blobmsg_parse(policy, 2, tb, blob_data(msg), blob_len(msg));
    
    const char *interface_id = tb[0] ? blobmsg_get_string(tb[0]) : NULL;
    uint32_t hours = tb[1] ? blobmsg_get_u32(tb[1]) : 24; // Default 24 hours
    
    double accuracy_pct;
    int trend_direction;
    
    int result = ml_monitor_analytics_get_accuracy_trend(interface_id, hours, &accuracy_pct, &trend_direction);
    
    if (result == ML_MONITOR_SUCCESS) {
        if (interface_id) {
            blobmsg_add_string(&b, "interface_id", interface_id);
        } else {
            blobmsg_add_string(&b, "scope", "all_interfaces");
        }
        
        blobmsg_add_u32(&b, "hours_analyzed", hours);
        blobmsg_add_double(&b, "accuracy_pct", accuracy_pct);
        blobmsg_add_u32(&b, "trend_direction", trend_direction);
        
        const char *trend_description;
        if (trend_direction > 0) {
            trend_description = "improving";
        } else if (trend_direction < 0) {
            trend_description = "declining";
        } else {
            trend_description = "stable";
        }
        blobmsg_add_string(&b, "trend_description", trend_description);
        
        // Accuracy rating
        const char *accuracy_rating;
        if (accuracy_pct >= 95.0) {
            accuracy_rating = "excellent";
        } else if (accuracy_pct >= 90.0) {
            accuracy_rating = "very_good";
        } else if (accuracy_pct >= 80.0) {
            accuracy_rating = "good";
        } else if (accuracy_pct >= 70.0) {
            accuracy_rating = "fair";
        } else if (accuracy_pct >= 60.0) {
            accuracy_rating = "poor";
        } else {
            accuracy_rating = "very_poor";
        }
        blobmsg_add_string(&b, "accuracy_rating", accuracy_rating);
        
        blobmsg_add_u32(&b, "timestamp", (uint32_t)time(NULL));
        blobmsg_add_string(&b, "status", "success");
    } else {
        blobmsg_add_string(&b, "error", "Failed to get accuracy trends");
        blobmsg_add_u32(&b, "code", result);
    }
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// UBUS method: get_ml_impact_summary
int ml_monitor_ubus_get_impact_summary(struct ubus_context *ctx, struct ubus_object *obj,
                                      struct ubus_request_data *req, const char *method,
                                      struct blob_attr *msg) {
    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    
    // Parse parameters
    struct blob_attr *tb[1];
    static const struct blobmsg_policy policy[1] = {
        [0] = { .name = "hours", .type = BLOBMSG_TYPE_INT32 },
    };
    
    blobmsg_parse(policy, 1, tb, blob_data(msg), blob_len(msg));
    
    uint32_t hours = tb[0] ? blobmsg_get_u32(tb[0]) : 24; // Default 24 hours
    
    int32_t total_improvement_ms;
    double stability_improvement_pct;
    uint32_t actions_taken;
    
    int result = ml_monitor_analytics_get_impact_summary(hours, &total_improvement_ms, 
                                                        &stability_improvement_pct, &actions_taken);
    
    if (result == ML_MONITOR_SUCCESS) {
        blobmsg_add_u32(&b, "hours_analyzed", hours);
        blobmsg_add_u32(&b, "total_improvement_ms", total_improvement_ms);
        blobmsg_add_double(&b, "stability_improvement_pct", stability_improvement_pct);
        blobmsg_add_u32(&b, "ml_actions_taken", actions_taken);
        
        // Convert improvement to human-readable format
        if (total_improvement_ms > 0) {
            if (total_improvement_ms >= 60000) {
                double minutes = total_improvement_ms / 60000.0;
                blobmsg_add_double(&b, "improvement_minutes", minutes);
            } else if (total_improvement_ms >= 1000) {
                double seconds = total_improvement_ms / 1000.0;
                blobmsg_add_double(&b, "improvement_seconds", seconds);
            }
        }
        
        // Impact assessment
        const char *impact_assessment;
        if (total_improvement_ms > 300000) { // > 5 minutes
            impact_assessment = "major_improvement";
        } else if (total_improvement_ms > 60000) { // > 1 minute
            impact_assessment = "significant_improvement";
        } else if (total_improvement_ms > 10000) { // > 10 seconds
            impact_assessment = "moderate_improvement";
        } else if (total_improvement_ms > 0) {
            impact_assessment = "minor_improvement";
        } else if (total_improvement_ms == 0) {
            impact_assessment = "no_measurable_impact";
        } else {
            impact_assessment = "negative_impact";
        }
        blobmsg_add_string(&b, "impact_assessment", impact_assessment);
        
        // Stability assessment
        const char *stability_assessment;
        if (stability_improvement_pct >= 20.0) {
            stability_assessment = "major_stability_improvement";
        } else if (stability_improvement_pct >= 10.0) {
            stability_assessment = "significant_stability_improvement";
        } else if (stability_improvement_pct >= 5.0) {
            stability_assessment = "moderate_stability_improvement";
        } else if (stability_improvement_pct > 0.0) {
            stability_assessment = "minor_stability_improvement";
        } else {
            stability_assessment = "no_stability_change";
        }
        blobmsg_add_string(&b, "stability_assessment", stability_assessment);
        
        blobmsg_add_u32(&b, "timestamp", (uint32_t)time(NULL));
        blobmsg_add_string(&b, "status", "success");
    } else {
        blobmsg_add_string(&b, "error", "Failed to get impact summary");
        blobmsg_add_u32(&b, "code", result);
    }
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// UBUS method: get_current_interface_scores
int ml_monitor_ubus_get_current_interface_scores(struct ubus_context *ctx, struct ubus_object *obj,
                                                struct ubus_request_data *req, const char *method,
                                                struct blob_attr *msg) {
    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    
    // Get current interface information
    network_interface_t interfaces[MAX_INTERFACES];
    int interface_count = 0;
    
    extern int get_enhanced_comprehensive_interface_info(network_interface_t *interfaces, int *count);
    int discovery_result = get_enhanced_comprehensive_interface_info(interfaces, &interface_count);
    
    if (discovery_result == AUTONOMY_SUCCESS) {
        void *interfaces_array = blobmsg_open_array(&b, "interface_scores");
        
        for (int i = 0; i < interface_count; i++) {
            if (!ml_monitor_is_interface_suitable_for_ml(&interfaces[i])) continue;
            
            ml_interface_score_t score;
            int score_result = ml_monitor_analytics_calculate_interface_score(interfaces[i].name, &score);
            
            if (score_result == ML_MONITOR_SUCCESS) {
                void *iface_table = blobmsg_open_table(&b, NULL);
                
                blobmsg_add_string(&b, "interface_id", score.interface_id);
                blobmsg_add_string(&b, "interface_type", ml_monitor_get_interface_type_string(score.interface_type));
                blobmsg_add_double(&b, "overall_score", score.overall_score);
                blobmsg_add_double(&b, "accuracy_score", score.accuracy_score);
                blobmsg_add_double(&b, "stability_score", score.stability_score);
                blobmsg_add_double(&b, "performance_score", score.performance_score);
                blobmsg_add_double(&b, "trend_score", score.trend_score);
                
                // Score contributors (what's affecting the score)
                void *contributors_table = blobmsg_open_table(&b, "score_contributors");
                blobmsg_add_double(&b, "latency_impact", score.score_contributors.latency_impact);
                blobmsg_add_double(&b, "loss_impact", score.score_contributors.loss_impact);
                blobmsg_add_double(&b, "signal_impact", score.score_contributors.signal_impact);
                blobmsg_add_double(&b, "prediction_impact", score.score_contributors.prediction_impact);
                blobmsg_add_double(&b, "stability_impact", score.score_contributors.stability_impact);
                blobmsg_add_double(&b, "trend_impact", score.score_contributors.trend_impact);
                blobmsg_close_table(&b, contributors_table);
                
                // Current metrics
                void *metrics_table = blobmsg_open_table(&b, "current_metrics");
                blobmsg_add_u32(&b, "latency_ms", score.current_latency_ms);
                blobmsg_add_u32(&b, "loss_pct", score.current_loss_pct);
                blobmsg_add_u32(&b, "signal_dbm", score.current_signal_dbm);
                blobmsg_add_u32(&b, "stable_minutes", score.consecutive_stable_minutes);
                blobmsg_add_u32(&b, "recent_predictions_correct", score.recent_predictions_correct);
                blobmsg_close_table(&b, metrics_table);
                
                // Score rating
                const char *score_rating;
                if (score.overall_score >= 90.0) {
                    score_rating = "excellent";
                } else if (score.overall_score >= 80.0) {
                    score_rating = "very_good";
                } else if (score.overall_score >= 70.0) {
                    score_rating = "good";
                } else if (score.overall_score >= 60.0) {
                    score_rating = "fair";
                } else if (score.overall_score >= 50.0) {
                    score_rating = "poor";
                } else {
                    score_rating = "very_poor";
                }
                blobmsg_add_string(&b, "score_rating", score_rating);
                
                blobmsg_add_u32(&b, "timestamp", (uint32_t)score.timestamp);
                blobmsg_close_table(&b, iface_table);
            }
        }
        
        blobmsg_close_array(&b, interfaces_array);
        blobmsg_add_u32(&b, "interface_count", interface_count);
        blobmsg_add_u32(&b, "timestamp", (uint32_t)time(NULL));
        blobmsg_add_string(&b, "status", "success");
    } else {
        blobmsg_add_string(&b, "error", "Failed to get interface information");
        blobmsg_add_u32(&b, "code", discovery_result);
    }
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}