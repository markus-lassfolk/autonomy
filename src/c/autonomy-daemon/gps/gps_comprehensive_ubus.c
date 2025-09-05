#include "gps_comprehensive_ubus.h"
#include "gps_comprehensive.h"
#include "gps_fusion_engine.h"
#include "logx.h"
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// Helper function to add standardized GPS data to blob
static void add_gps_data_to_blob(struct blob_buf *bb, const char *name, 
                                 const standardized_gps_data_t *gps_data) {
    void *gps_table = blobmsg_open_table(bb, name);
    
    blobmsg_add_double(bb, "latitude", gps_data->latitude);
    blobmsg_add_double(bb, "longitude", gps_data->longitude);
    blobmsg_add_double(bb, "altitude", gps_data->altitude);
    blobmsg_add_double(bb, "accuracy", gps_data->accuracy);
    blobmsg_add_double(bb, "vertical_accuracy", gps_data->vertical_accuracy);
    blobmsg_add_double(bb, "confidence", gps_data->confidence);
    blobmsg_add_string(bb, "fix_type", gps_fix_type_to_string(gps_data->fix_type));
    blobmsg_add_string(bb, "fix_quality", gps_fix_quality_to_string(gps_data->fix_quality));
    blobmsg_add_u8(bb, "valid", gps_data->valid);
    
    blobmsg_add_u32(bb, "satellites_used", gps_data->satellites_used);
    blobmsg_add_u32(bb, "satellites_visible", gps_data->satellites_visible);
    blobmsg_add_double(bb, "hdop", gps_data->hdop);
    blobmsg_add_double(bb, "vdop", gps_data->vdop);
    blobmsg_add_double(bb, "pdop", gps_data->pdop);
    
    blobmsg_add_double(bb, "speed", gps_data->speed);
    blobmsg_add_double(bb, "heading", gps_data->heading);
    blobmsg_add_double(bb, "climb", gps_data->climb);
    
    blobmsg_add_u32(bb, "timestamp", (uint32_t)gps_data->timestamp);
    blobmsg_add_double(bb, "age_seconds", gps_data->age_seconds);
    blobmsg_add_double(bb, "collection_duration_ms", gps_data->collection_duration_ms);
    
    blobmsg_add_string(bb, "source", gps_data->source);
    blobmsg_add_string(bb, "source_type", gps_source_type_to_string(gps_data->source_type));
    blobmsg_add_u32(bb, "source_priority", gps_data->source_priority);
    blobmsg_add_u8(bb, "from_cache", gps_data->from_cache);
    
    if (strlen(gps_data->raw_nmea) > 0) {
        blobmsg_add_string(bb, "raw_nmea", gps_data->raw_nmea);
    }
    if (strlen(gps_data->raw_json) > 0) {
        blobmsg_add_string(bb, "raw_json", gps_data->raw_json);
    }
    
    blobmsg_close_table(bb, gps_table);
}

// Helper function to add source health to blob
static void add_source_health_to_blob(struct blob_buf *bb, const gps_source_health_t *health) {
    void *health_table = blobmsg_open_table(bb, NULL);
    
    blobmsg_add_string(bb, "source", health->source_name);
    blobmsg_add_string(bb, "type", gps_source_type_to_string(health->source_type));
    blobmsg_add_u8(bb, "available", health->available);
    blobmsg_add_u8(bb, "healthy", health->healthy);
    blobmsg_add_double(bb, "health_score", health->health_score);
    
    blobmsg_add_u64(bb, "total_collections", health->total_collections);
    blobmsg_add_u64(bb, "successful_collections", health->successful_collections);
    blobmsg_add_u64(bb, "failed_collections", health->failed_collections);
    blobmsg_add_double(bb, "success_rate", health->success_rate);
    
    blobmsg_add_double(bb, "avg_collection_time_ms", health->average_collection_time_ms);
    blobmsg_add_double(bb, "avg_accuracy", health->average_accuracy);
    blobmsg_add_double(bb, "avg_confidence", health->average_confidence);
    
    blobmsg_add_u32(bb, "consecutive_failures", health->consecutive_failures);
    blobmsg_add_u32(bb, "consecutive_successes", health->consecutive_successes);
    blobmsg_add_u32(bb, "last_success", (uint32_t)health->last_success);
    blobmsg_add_u32(bb, "last_failure", (uint32_t)health->last_failure);
    
    blobmsg_add_double(bb, "best_accuracy", health->best_accuracy);
    blobmsg_add_double(bb, "worst_accuracy", health->worst_accuracy);
    blobmsg_add_u32(bb, "first_seen", (uint32_t)health->first_seen);
    blobmsg_add_u32(bb, "last_seen", (uint32_t)health->last_seen);
    
    blobmsg_close_table(bb, health_table);
}

// Get comprehensive GPS status
int gps_comprehensive_ubus_get_status(struct ubus_context *ctx, struct ubus_object *obj,
                                     struct ubus_request_data *req, const char *method,
                                     struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    if (!gps_comprehensive_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "Comprehensive GPS collector not initialized");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    blobmsg_add_u8(&bb, "success", 1);
    
    // Get current position from comprehensive collector
    standardized_gps_data_t current_gps;
    if (gps_comprehensive_collect_best(&current_gps) == AUTONOMY_SUCCESS) {
        add_gps_data_to_blob(&bb, "current_position", &current_gps);
    } else {
        void *position_table = blobmsg_open_table(&bb, "current_position");
        blobmsg_add_u8(&bb, "available", 0);
        blobmsg_add_string(&bb, "error", "No GPS data available");
        blobmsg_close_table(&bb, position_table);
    }
    
    // Get fusion data if available
    if (gps_fusion_engine_is_initialized()) {
        gps_fusion_statistics_t fusion_stats;
        if (gps_fusion_engine_get_statistics(&fusion_stats) == AUTONOMY_SUCCESS) {
            void *fusion_table = blobmsg_open_table(&bb, "fusion_data");
            blobmsg_add_u64(&bb, "total_fusions", fusion_stats.total_fusions);
            blobmsg_add_u64(&bb, "successful_fusions", fusion_stats.successful_fusions);
            blobmsg_add_u64(&bb, "failed_fusions", fusion_stats.failed_fusions);
            blobmsg_add_double(&bb, "avg_fusion_time_ms", fusion_stats.average_fusion_time_ms);
            blobmsg_add_double(&bb, "avg_sources_per_fusion", fusion_stats.average_sources_per_fusion);
            blobmsg_add_double(&bb, "avg_fusion_accuracy", fusion_stats.average_fusion_accuracy);
            blobmsg_add_double(&bb, "avg_fusion_confidence", fusion_stats.average_fusion_confidence);
            blobmsg_add_u32(&bb, "last_fusion", (uint32_t)fusion_stats.last_fusion);
            blobmsg_close_table(&bb, fusion_table);
        }
    }
    
    // Get movement status
    gps_movement_state_t movement_state;
    if (gps_comprehensive_get_movement_state(&movement_state) == AUTONOMY_SUCCESS) {
        void *movement_table = blobmsg_open_table(&bb, "movement");
        blobmsg_add_u8(&bb, "is_moving", movement_state.is_moving);
        blobmsg_add_u8(&bb, "was_moving", movement_state.was_moving);
        blobmsg_add_u32(&bb, "movement_start", (uint32_t)movement_state.movement_start);
        blobmsg_add_u32(&bb, "stationary_start", (uint32_t)movement_state.stationary_start);
        blobmsg_add_double(&bb, "last_latitude", movement_state.last_latitude);
        blobmsg_add_double(&bb, "last_longitude", movement_state.last_longitude);
        blobmsg_add_double(&bb, "total_distance_m", movement_state.total_distance_m);
        blobmsg_add_double(&bb, "current_speed_ms", movement_state.current_speed_ms);
        blobmsg_add_double(&bb, "average_speed_ms", movement_state.average_speed_ms);
        blobmsg_add_double(&bb, "max_speed_ms", movement_state.max_speed_ms);
        blobmsg_add_u32(&bb, "movement_events", movement_state.movement_events);
        blobmsg_add_u32(&bb, "last_movement_event", (uint32_t)movement_state.last_movement_event);
        blobmsg_close_table(&bb, movement_table);
    }
    
    // Get source health for all sources
    gps_source_health_t source_health[GPS_SOURCE_MAX];
    int health_count = gps_comprehensive_get_all_source_health(source_health, GPS_SOURCE_MAX);
    
    if (health_count > 0) {
        void *sources_array = blobmsg_open_array(&bb, "source_health");
        for (int i = 0; i < health_count; i++) {
            add_source_health_to_blob(&bb, &source_health[i]);
        }
        blobmsg_close_array(&bb, sources_array);
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return UBUS_STATUS_OK;
}

// Collect GPS data from best source
int gps_comprehensive_ubus_collect_best(struct ubus_context *ctx, struct ubus_object *obj,
                                       struct ubus_request_data *req, const char *method,
                                       struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    if (!gps_comprehensive_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "Comprehensive GPS collector not initialized");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    standardized_gps_data_t gps_data;
    if (gps_comprehensive_collect_best(&gps_data) == AUTONOMY_SUCCESS) {
        blobmsg_add_u8(&bb, "success", 1);
        add_gps_data_to_blob(&bb, "gps_data", &gps_data);
    } else {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "GPS collection failed");
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return UBUS_STATUS_OK;
}

// Collect from all sources and perform fusion
int gps_comprehensive_ubus_collect_all_and_fuse(struct ubus_context *ctx, struct ubus_object *obj,
                                               struct ubus_request_data *req, const char *method,
                                               struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    if (!gps_comprehensive_is_initialized() || !gps_fusion_engine_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "GPS comprehensive system not initialized");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    gps_fusion_result_t fusion_result;
    if (gps_comprehensive_collect_all_and_fuse(&fusion_result) == AUTONOMY_SUCCESS) {
        blobmsg_add_u8(&bb, "success", 1);
        
        void *result_table = blobmsg_open_table(&bb, "fusion_result");
        
        // Add fused data
        add_gps_data_to_blob(&bb, "fused_data", &fusion_result.fused_data);
        
        // Add individual source data
        void *sources_array = blobmsg_open_array(&bb, "source_data");
        for (int i = 0; i < fusion_result.sources_used; i++) {
            add_gps_data_to_blob(&bb, NULL, &fusion_result.source_data[i]);
        }
        blobmsg_close_array(&bb, sources_array);
        
        blobmsg_add_u32(&bb, "sources_used", fusion_result.sources_used);
        blobmsg_add_string(&bb, "fusion_method", fusion_result.fusion_method);
        blobmsg_add_double(&bb, "fusion_confidence", fusion_result.fusion_confidence);
        blobmsg_add_u32(&bb, "fusion_time", (uint32_t)fusion_result.fusion_time);
        blobmsg_add_string(&bb, "fusion_reasoning", fusion_result.fusion_reasoning);
        
        blobmsg_close_table(&bb, result_table);
    } else {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "GPS fusion failed");
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return UBUS_STATUS_OK;
}

// Get GPS source health status
int gps_comprehensive_ubus_get_source_health(struct ubus_context *ctx, struct ubus_object *obj,
                                           struct ubus_request_data *req, const char *method,
                                           struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    if (!gps_comprehensive_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "Comprehensive GPS collector not initialized");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    blobmsg_add_u8(&bb, "success", 1);
    
    gps_source_health_t source_health[GPS_SOURCE_MAX];
    int health_count = gps_comprehensive_get_all_source_health(source_health, GPS_SOURCE_MAX);
    
    if (health_count > 0) {
        void *sources_array = blobmsg_open_array(&bb, "sources");
        for (int i = 0; i < health_count; i++) {
            add_source_health_to_blob(&bb, &source_health[i]);
        }
        blobmsg_close_array(&bb, sources_array);
    } else {
        blobmsg_add_string(&bb, "message", "No GPS sources available");
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return UBUS_STATUS_OK;
}

// Get movement detection status
int gps_comprehensive_ubus_get_movement_status(struct ubus_context *ctx, struct ubus_object *obj,
                                             struct ubus_request_data *req, const char *method,
                                             struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    if (!gps_comprehensive_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "Comprehensive GPS collector not initialized");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    blobmsg_add_u8(&bb, "success", 1);
    
    gps_movement_state_t movement_state;
    if (gps_comprehensive_get_movement_state(&movement_state) == AUTONOMY_SUCCESS) {
        void *movement_table = blobmsg_open_table(&bb, "movement");
        
        blobmsg_add_u8(&bb, "is_moving", movement_state.is_moving);
        blobmsg_add_u8(&bb, "was_moving", movement_state.was_moving);
        blobmsg_add_u32(&bb, "movement_start", (uint32_t)movement_state.movement_start);
        blobmsg_add_u32(&bb, "stationary_start", (uint32_t)movement_state.stationary_start);
        blobmsg_add_double(&bb, "last_latitude", movement_state.last_latitude);
        blobmsg_add_double(&bb, "last_longitude", movement_state.last_longitude);
        blobmsg_add_double(&bb, "total_distance_m", movement_state.total_distance_m);
        blobmsg_add_double(&bb, "current_speed_ms", movement_state.current_speed_ms);
        blobmsg_add_double(&bb, "average_speed_ms", movement_state.average_speed_ms);
        blobmsg_add_double(&bb, "max_speed_ms", movement_state.max_speed_ms);
        blobmsg_add_u32(&bb, "movement_events", movement_state.movement_events);
        blobmsg_add_u32(&bb, "last_movement_event", (uint32_t)movement_state.last_movement_event);
        
        blobmsg_close_table(&bb, movement_table);
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return UBUS_STATUS_OK;
}

// Get GPS fusion engine statistics
int gps_comprehensive_ubus_get_fusion_stats(struct ubus_context *ctx, struct ubus_object *obj,
                                          struct ubus_request_data *req, const char *method,
                                          struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    if (!gps_fusion_engine_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "GPS fusion engine not initialized");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    blobmsg_add_u8(&bb, "success", 1);
    
    gps_fusion_statistics_t stats;
    if (gps_fusion_engine_get_statistics(&stats) == AUTONOMY_SUCCESS) {
        void *stats_table = blobmsg_open_table(&bb, "fusion_stats");
        
        blobmsg_add_u64(&bb, "total_fusions", stats.total_fusions);
        blobmsg_add_u64(&bb, "successful_fusions", stats.successful_fusions);
        blobmsg_add_u64(&bb, "failed_fusions", stats.failed_fusions);
        blobmsg_add_u64(&bb, "outliers_detected", stats.outliers_detected);
        blobmsg_add_u64(&bb, "consensus_failures", stats.consensus_failures);
        
        blobmsg_add_double(&bb, "avg_fusion_time_ms", stats.average_fusion_time_ms);
        blobmsg_add_double(&bb, "avg_sources_per_fusion", stats.average_sources_per_fusion);
        blobmsg_add_double(&bb, "avg_fusion_accuracy", stats.average_fusion_accuracy);
        blobmsg_add_double(&bb, "avg_fusion_confidence", stats.average_fusion_confidence);
        
        void *method_table = blobmsg_open_table(&bb, "method_usage");
        for (int i = 0; i < GPS_FUSION_METHOD_MAX; i++) {
            blobmsg_add_u64(&bb, gps_fusion_method_to_string((gps_fusion_method_t)i), 
                           stats.method_usage[i]);
        }
        blobmsg_close_table(&bb, method_table);
        
        blobmsg_add_u32(&bb, "last_fusion", (uint32_t)stats.last_fusion);
        blobmsg_add_u32(&bb, "stats_reset_time", (uint32_t)stats.stats_reset_time);
        
        blobmsg_close_table(&bb, stats_table);
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return UBUS_STATUS_OK;
}

// Get comprehensive GPS statistics
int gps_comprehensive_ubus_get_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                        struct ubus_request_data *req, const char *method,
                                        struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    if (!gps_comprehensive_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "Comprehensive GPS collector not initialized");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    blobmsg_add_u8(&bb, "success", 1);
    
    uint64_t total_collections, successful_collections, failed_collections;
    uint64_t fusion_operations, movement_detections;
    
    if (gps_comprehensive_get_statistics(&total_collections, &successful_collections, 
                                       &failed_collections, &fusion_operations, 
                                       &movement_detections) == AUTONOMY_SUCCESS) {
        
        void *stats_table = blobmsg_open_table(&bb, "comprehensive_stats");
        
        blobmsg_add_u64(&bb, "total_collections", total_collections);
        blobmsg_add_u64(&bb, "successful_collections", successful_collections);
        blobmsg_add_u64(&bb, "failed_collections", failed_collections);
        blobmsg_add_u64(&bb, "fusion_operations", fusion_operations);
        blobmsg_add_u64(&bb, "movement_detections", movement_detections);
        
        double success_rate = total_collections > 0 ? 
            (double)successful_collections / total_collections : 0.0;
        blobmsg_add_double(&bb, "collection_success_rate", success_rate);
        
        blobmsg_close_table(&bb, stats_table);
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return UBUS_STATUS_OK;
}

// Additional UBUS method implementations would continue here...

// UBUS method definitions
const struct ubus_method gps_comprehensive_ubus_methods[] = {
    UBUS_METHOD_NOARG("get_comprehensive_status", gps_comprehensive_ubus_get_status),
    UBUS_METHOD_NOARG("collect_best_gps", gps_comprehensive_ubus_collect_best),
    UBUS_METHOD_NOARG("collect_all_and_fuse", gps_comprehensive_ubus_collect_all_and_fuse),
    UBUS_METHOD_NOARG("get_source_health", gps_comprehensive_ubus_get_source_health),
    UBUS_METHOD_NOARG("get_movement_status", gps_comprehensive_ubus_get_movement_status),
    UBUS_METHOD_NOARG("get_fusion_stats", gps_comprehensive_ubus_get_fusion_stats),
    UBUS_METHOD_NOARG("get_comprehensive_stats", gps_comprehensive_ubus_get_statistics),
    UBUS_METHOD_NOARG("force_collection", gps_comprehensive_ubus_force_collection),
    UBUS_METHOD_NOARG("reset_statistics", gps_comprehensive_ubus_reset_statistics),
    UBUS_METHOD_NOARG("comprehensive_health_check", gps_comprehensive_ubus_health_check),
};

const int gps_comprehensive_ubus_methods_count = ARRAY_SIZE(gps_comprehensive_ubus_methods);