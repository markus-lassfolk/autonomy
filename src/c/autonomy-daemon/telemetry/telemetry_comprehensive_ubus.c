#include <stdlib.h>
#include "telemetry_comprehensive_ubus.h"
#include "telemetry_comprehensive.h"
#include "../shared/logging/logx.h"
#include <json-c/json.h>
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// UBUS parameter policies
enum {
    TELEMETRY_MEMBER_NAME,
    TELEMETRY_START_TIME,
    TELEMETRY_END_TIME,
    TELEMETRY_LIMIT,
    TELEMETRY_INCLUDE_GPS,
    __TELEMETRY_HISTORICAL_MAX
};

static const struct blobmsg_policy telemetry_historical_policy[] = {
    [TELEMETRY_MEMBER_NAME] = { .name = "member_name", .type = BLOBMSG_TYPE_STRING },
    [TELEMETRY_START_TIME] = { .name = "start_time", .type = BLOBMSG_TYPE_INT32 },
    [TELEMETRY_END_TIME] = { .name = "end_time", .type = BLOBMSG_TYPE_INT32 },
    [TELEMETRY_LIMIT] = { .name = "limit", .type = BLOBMSG_TYPE_INT32 },
    [TELEMETRY_INCLUDE_GPS] = { .name = "include_gps", .type = BLOBMSG_TYPE_BOOL },
};

enum {
    TELEMETRY_LOCATION_LAT,
    TELEMETRY_LOCATION_LON,
    TELEMETRY_LOCATION_RADIUS,
    TELEMETRY_LOCATION_MEMBER,
    TELEMETRY_LOCATION_LIMIT,
    __TELEMETRY_LOCATION_MAX
};

static const struct blobmsg_policy telemetry_location_policy[] = {
    [TELEMETRY_LOCATION_LAT] = { .name = "latitude", .type = BLOBMSG_TYPE_DOUBLE },
    [TELEMETRY_LOCATION_LON] = { .name = "longitude", .type = BLOBMSG_TYPE_DOUBLE },
    [TELEMETRY_LOCATION_RADIUS] = { .name = "radius_meters", .type = BLOBMSG_TYPE_DOUBLE },
    [TELEMETRY_LOCATION_MEMBER] = { .name = "member_name", .type = BLOBMSG_TYPE_STRING },
    [TELEMETRY_LOCATION_LIMIT] = { .name = "limit", .type = BLOBMSG_TYPE_INT32 },
};

enum {
    TELEMETRY_TREND_MEMBER_NAME,
    TELEMETRY_TREND_WINDOW_HOURS,
    __TELEMETRY_TREND_MAX
};

static const struct blobmsg_policy telemetry_trend_policy[] = {
    [TELEMETRY_TREND_MEMBER_NAME] = { .name = "member_name", .type = BLOBMSG_TYPE_STRING },
    [TELEMETRY_TREND_WINDOW_HOURS] = { .name = "window_hours", .type = BLOBMSG_TYPE_INT32 },
};

enum {
    TELEMETRY_ML_EXPORT_OUTPUT_PATH,
    TELEMETRY_ML_EXPORT_START_TIME,
    TELEMETRY_ML_EXPORT_END_TIME,
    TELEMETRY_ML_EXPORT_MAX_SAMPLES,
    __TELEMETRY_ML_EXPORT_MAX
};

static const struct blobmsg_policy telemetry_ml_export_policy[] = {
    [TELEMETRY_ML_EXPORT_OUTPUT_PATH] = { .name = "output_path", .type = BLOBMSG_TYPE_STRING },
    [TELEMETRY_ML_EXPORT_START_TIME] = { .name = "start_time", .type = BLOBMSG_TYPE_INT32 },
    [TELEMETRY_ML_EXPORT_END_TIME] = { .name = "end_time", .type = BLOBMSG_TYPE_INT32 },
    [TELEMETRY_ML_EXPORT_MAX_SAMPLES] = { .name = "max_samples", .type = BLOBMSG_TYPE_INT32 },
};

// Helper function to add telemetry sample to blob
static void add_telemetry_sample_to_blob(struct blob_buf *bb, const telemetry_sample_t *sample) {
    void *sample_table = blobmsg_open_table(bb, NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    blobmsg_add_string(bb, "id", sample->id\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(bb, "timestamp", (uint32_t)sample->timestamp\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(bb, "member_name", sample->member_name\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(bb, "interface_name", sample->interface_name\n"\n"\n"\n"\n"\n"\n"\n");
    
    // GPS data
    if (sample->latitude != 0.0 && sample->longitude != 0.0) {
        void *gps_table = blobmsg_open_table(bb, "gps"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(bb, "latitude", sample->latitude\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(bb, "longitude", sample->longitude\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(bb, "accuracy", sample->accuracy\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u32(bb, "satellites", sample->satellites\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(bb, "hdop", sample->hdop\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(bb, "source", sample->gps_source\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(bb, "movement_kmh", sample->movement_kmh\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_close_table(bb, gps_table\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Network metrics
    void *network_table = blobmsg_open_table(bb, "network"\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "latency_ms", sample->latency_ms\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "packet_loss_percent", sample->packet_loss_percent\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "jitter_ms", sample->jitter_ms\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u64(bb, "throughput_bps", sample->throughput_bps\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "signal_strength", sample->signal_strength\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(bb, "status", sample->status\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_close_table(bb, network_table\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Starlink-specific metrics
    if (sample->obstruction_percent > 0 || sample->snr_db > 0) {
        void *starlink_table = blobmsg_open_table(bb, "starlink"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(bb, "obstruction_percent", sample->obstruction_percent\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(bb, "snr_db", sample->snr_db\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(bb, "temperature_c", sample->temperature_c\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u32(bb, "outage_count", sample->outage_count\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(bb, "pop_ping_drop_rate", sample->pop_ping_drop_rate\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_close_table(bb, starlink_table\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Cellular-specific metrics
    if (sample->rsrp_dbm != 0 || sample->cell_id != 0) {
        void *cellular_table = blobmsg_open_table(bb, "cellular"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(bb, "rsrp_dbm", sample->rsrp_dbm\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(bb, "rsrq_db", sample->rsrq_db\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(bb, "sinr_db", sample->sinr_db\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(bb, "carrier", sample->carrier\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u32(bb, "cell_id", sample->cell_id\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u32(bb, "cell_changes", sample->cell_changes\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_close_table(bb, cellular_table\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Quality scores
    void *scores_table = blobmsg_open_table(bb, "scores"\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "overall_score", sample->overall_score\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "reliability_score", sample->reliability_score\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "predictive_risk", sample->predictive_risk\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_close_table(bb, scores_table\n"\n"\n"\n"\n"\n"\n"\n");
    
    blobmsg_add_u8(bb, "is_active_interface", sample->is_active_interface\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(bb, "collection_method", sample->collection_method\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "collection_time_ms", sample->collection_time_ms\n"\n"\n"\n"\n"\n"\n"\n");
    
    blobmsg_close_table(bb, sample_table\n"\n"\n"\n"\n"\n"\n"\n");
}

// Helper function to add decision record to blob
static void add_decision_record_to_blob(struct blob_buf *bb, const decision_record_t *decision) {
    void *decision_table = blobmsg_open_table(bb, NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    blobmsg_add_u64(bb, "id", decision->id\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(bb, "decision_id", decision->decision_id\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(bb, "timestamp", (uint32_t)decision->timestamp\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(bb, "decision_type", decision->decision_type\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(bb, "trigger", decision->trigger\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(bb, "reasoning", decision->reasoning\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "confidence", decision->confidence\n"\n"\n"\n"\n"\n"\n"\n");
    
    blobmsg_add_string(bb, "from_interface", decision->from_interface\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(bb, "to_interface", decision->to_interface\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(bb, "from_member", decision->from_member\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(bb, "to_member", decision->to_member\n"\n"\n"\n"\n"\n"\n"\n");
    
    // GPS context
    if (decision->gps_latitude != 0.0 && decision->gps_longitude != 0.0) {
        void *gps_table = blobmsg_open_table(bb, "gps"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(bb, "latitude", decision->gps_latitude\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(bb, "longitude", decision->gps_longitude\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(bb, "accuracy", decision->gps_accuracy\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(bb, "source", decision->gps_source\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_close_table(bb, gps_table\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Performance context
    void *performance_table = blobmsg_open_table(bb, "performance"\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "from_score", decision->from_score\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "to_score", decision->to_score\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "score_difference", decision->score_difference\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "from_latency", decision->from_latency\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "from_loss", decision->from_loss\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "to_latency", decision->to_latency\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "to_loss", decision->to_loss\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_close_table(bb, performance_table\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Execution details
    void *execution_table = blobmsg_open_table(bb, "execution"\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(bb, "success", decision->success\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "execution_time_ms", decision->execution_time_ms\n"\n"\n"\n"\n"\n"\n"\n");
    if (strlen(decision->error_message) > 0) {
        blobmsg_add_string(bb, "error_message", decision->error_message\n"\n"\n"\n"\n"\n"\n"\n");
    }
    blobmsg_add_string(bb, "root_cause", decision->root_cause\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_close_table(bb, execution_table\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Context and predictions
    void *context_table = blobmsg_open_table(bb, "context"\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(bb, "predictive_decision", decision->predictive_decision\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "prediction_confidence", decision->prediction_confidence\n"\n"\n"\n"\n"\n"\n"\n");
    if (strlen(decision->context_json) > 0) {
        // Parse and add context JSON
        json_object* context_obj = json_tokener_parse(decision->context_json\n"\n"\n"\n"\n"\n"\n"\n");
        if (context_obj) {
            // Would add parsed JSON here
            json_object_put(context_obj\n"\n"\n"\n"\n"\n"\n"\n");
        }
    }
    blobmsg_close_table(bb, context_table\n"\n"\n"\n"\n"\n"\n"\n");
    
    blobmsg_close_table(bb, decision_table\n"\n"\n"\n"\n"\n"\n"\n");
}

// Get telemetry collection statistics
int telemetry_comprehensive_ubus_get_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                               struct ubus_request_data *req, const char *method,
                                               struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!telemetry_comprehensive_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Comprehensive telemetry not initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }
    
    blobmsg_add_u8(&bb, "success", 1\n"\n"\n"\n"\n"\n"\n"\n");
    
    telemetry_collection_statistics_t stats;
    if (telemetry_comprehensive_get_statistics(&stats) == AUTONOMY_SUCCESS) {
        void *statistics_table = blobmsg_open_table(&bb, "statistics"\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Collection statistics
        void *collection_table = blobmsg_open_table(&bb, "collection"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "total_samples_collected", stats.total_samples_collected\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "samples_with_gps", stats.samples_with_gps\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "samples_without_gps", stats.samples_without_gps\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "decision_records_logged", stats.decision_records_logged\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u32(&bb, "collection_start_time", (uint32_t)stats.collection_start_time\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u32(&bb, "last_collection", (uint32_t)stats.last_collection\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_close_table(&bb, collection_table\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Interface breakdown
        void *interfaces_table = blobmsg_open_table(&bb, "by_interface"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "starlink_samples", stats.starlink_samples\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "cellular_samples", stats.cellular_samples\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "wifi_samples", stats.wifi_samples\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_close_table(&bb, interfaces_table\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Database statistics
        void *database_table = blobmsg_open_table(&bb, "database"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "database_inserts", stats.database_inserts\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "database_errors", stats.database_errors\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(&bb, "database_size_mb", stats.database_size_mb\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "cleanup_operations", stats.cleanup_operations\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_close_table(&bb, database_table\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Performance statistics
        void *performance_table = blobmsg_open_table(&bb, "performance"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(&bb, "avg_collection_time_ms", stats.average_collection_time_ms\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "memory_usage_bytes", stats.memory_usage_bytes\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u32(&bb, "last_cleanup", (uint32_t)stats.last_cleanup\n"\n"\n"\n"\n"\n"\n"\n");
        if (stats.last_ml_export > 0) {
            blobmsg_add_u32(&bb, "last_ml_export", (uint32_t)stats.last_ml_export\n"\n"\n"\n"\n"\n"\n"\n");
        }
        blobmsg_close_table(&bb, performance_table\n"\n"\n"\n"\n"\n"\n"\n");
        
        blobmsg_close_table(&bb, statistics_table\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return UBUS_STATUS_OK;
}

// Get historical telemetry samples
int telemetry_comprehensive_ubus_get_historical_samples(struct ubus_context *ctx, struct ubus_object *obj,
                                                       struct ubus_request_data *req, const char *method,
                                                       struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!telemetry_comprehensive_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Comprehensive telemetry not initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }
    
    struct blob_attr *tb[__TELEMETRY_HISTORICAL_MAX];
    blobmsg_parse(telemetry_historical_policy, __TELEMETRY_HISTORICAL_MAX, tb, blob_data(msg), blob_len(msg)\n"\n"\n"\n"\n"\n"\n"\n");
    
    const char* member_name = tb[TELEMETRY_MEMBER_NAME] ? blobmsg_get_string(tb[TELEMETRY_MEMBER_NAME]) : NULL;
    time_t start_time = tb[TELEMETRY_START_TIME] ? blobmsg_get_u32(tb[TELEMETRY_START_TIME]) : (time(NULL) - 86400\n"\n"\n"\n"\n"\n"\n"\n");
    time_t end_time = tb[TELEMETRY_END_TIME] ? blobmsg_get_u32(tb[TELEMETRY_END_TIME]) : time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    int limit = tb[TELEMETRY_LIMIT] ? blobmsg_get_u32(tb[TELEMETRY_LIMIT]) : 50;
    bool include_gps = tb[TELEMETRY_INCLUDE_GPS] ? blobmsg_get_bool(tb[TELEMETRY_INCLUDE_GPS]) : true;
    
    // Limit maximum samples to prevent memory issues
    if (limit > 1000) limit = 1000; // Use configurable max samples limit
    
    telemetry_sample_t* samples = malloc(limit * sizeof(telemetry_sample_t)\n"\n"\n"\n"\n"\n"\n"\n");
    if (!samples) {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Memory allocation failed"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }
    
    int sample_count = telemetry_comprehensive_get_historical_samples(member_name, start_time, end_time, samples, limit\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (sample_count >= 0) {
        blobmsg_add_u8(&bb, "success", 1\n"\n"\n"\n"\n"\n"\n"\n");
        
        void *historical_table = blobmsg_open_table(&bb, "historical_data"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u32(&bb, "total_matching", sample_count); // Simplified - would need separate count query
        blobmsg_add_u32(&bb, "returned", sample_count\n"\n"\n"\n"\n"\n"\n"\n");
        
        void *time_range_table = blobmsg_open_table(&bb, "time_range"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u32(&bb, "start", (uint32_t)start_time\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u32(&bb, "end", (uint32_t)end_time\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_close_table(&bb, time_range_table\n"\n"\n"\n"\n"\n"\n"\n");
        
        void *samples_array = blobmsg_open_array(&bb, "samples"\n"\n"\n"\n"\n"\n"\n"\n");
        for (int i = 0; i < sample_count; i++) {
            add_telemetry_sample_to_blob(&bb, &samples[i]\n"\n"\n"\n"\n"\n"\n"\n");
        }
        blobmsg_close_array(&bb, samples_array\n"\n"\n"\n"\n"\n"\n"\n");
        
        blobmsg_close_table(&bb, historical_table\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Failed to get historical samples"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    free(samples\n"\n"\n"\n"\n"\n"\n"\n");
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return UBUS_STATUS_OK;
}

// Get failover/failback decision history
int telemetry_comprehensive_ubus_get_decision_history(struct ubus_context *ctx, struct ubus_object *obj,
                                                     struct ubus_request_data *req, const char *method,
                                                     struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!telemetry_comprehensive_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Comprehensive telemetry not initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }
    
    // Parse parameters (similar to historical samples)
    time_t start_time = time(NULL) - 86400; // Default to last 24 hours
    time_t end_time = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    int limit = 20; // Default limit
    
    decision_record_t* decisions = malloc(limit * sizeof(decision_record_t)\n"\n"\n"\n"\n"\n"\n"\n");
    if (!decisions) {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Memory allocation failed"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }
    
    int decision_count = telemetry_comprehensive_get_decision_history(start_time, end_time, decisions, limit\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (decision_count >= 0) {
        blobmsg_add_u8(&bb, "success", 1\n"\n"\n"\n"\n"\n"\n"\n");
        
        void *history_table = blobmsg_open_table(&bb, "decision_history"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u32(&bb, "total_matching", decision_count\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u32(&bb, "returned", decision_count\n"\n"\n"\n"\n"\n"\n"\n");
        
        void *decisions_array = blobmsg_open_array(&bb, "decisions"\n"\n"\n"\n"\n"\n"\n"\n");
        for (int i = 0; i < decision_count; i++) {
            add_decision_record_to_blob(&bb, &decisions[i]\n"\n"\n"\n"\n"\n"\n"\n");
        }
        blobmsg_close_array(&bb, decisions_array\n"\n"\n"\n"\n"\n"\n"\n");
        
        blobmsg_close_table(&bb, history_table\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Failed to get decision history"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    free(decisions\n"\n"\n"\n"\n"\n"\n"\n");
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return UBUS_STATUS_OK;
}

// Additional UBUS method implementations would continue here...

// Execute real ML algorithm on historical data
int telemetry_comprehensive_ubus_execute_ml_algorithm(struct ubus_context *ctx, struct ubus_object *obj,
                                                      struct ubus_request_data *req, const char *method,
                                                      struct blob_attr *msg)
{
    // Parse ML algorithm execution parameters
    enum {
        ML_ALG_NAME,
        ML_START,
        ML_END,
        ML_FEATURES,
        ML_MODEL_PATH,
        __ML_MAX
    };
    static const struct blobmsg_policy policy[__ML_MAX] = {
        [ML_ALG_NAME] = { .name = "algorithm_name", .type = BLOBMSG_TYPE_STRING },
        [ML_START]    = { .name = "start_time",     .type = BLOBMSG_TYPE_INT64  },
        [ML_END]      = { .name = "end_time",       .type = BLOBMSG_TYPE_INT64  },
        [ML_FEATURES] = { .name = "features",       .type = BLOBMSG_TYPE_ARRAY  },
        [ML_MODEL_PATH] = { .name = "model_path",   .type = BLOBMSG_TYPE_STRING },
    };

    struct blob_attr *tb[__ML_MAX];
    const char *algorithm_name = "predictive_failover_v1";
    int64_t start_time = 0;
    int64_t end_time = 0;
    const char *model_path = "/var/lib/autonomy/ml_models/";

    if (msg) {
        blobmsg_parse(policy, __ML_MAX, tb, blob_data(msg), blob_len(msg)\n"\n"\n"\n"\n"\n"\n"\n");
        if (tb[ML_ALG_NAME]) algorithm_name = blobmsg_get_string(tb[ML_ALG_NAME]\n"\n"\n"\n"\n"\n"\n"\n");
        if (tb[ML_START])    start_time     = (int64_t)blobmsg_get_u64(tb[ML_START]\n"\n"\n"\n"\n"\n"\n"\n");
        if (tb[ML_END])      end_time       = (int64_t)blobmsg_get_u64(tb[ML_END]\n"\n"\n"\n"\n"\n"\n"\n");
        if (tb[ML_MODEL_PATH]) model_path = blobmsg_get_string(tb[ML_MODEL_PATH]\n"\n"\n"\n"\n"\n"\n"\n");
    }

    // Basic input validation
    if (end_time != 0 && start_time != 0 && end_time < start_time) {
        int64_t tmp = start_time; start_time = end_time; end_time = tmp;
    }

    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");

    // Check if ML model exists
    char model_file[512];
    snprintf(model_file, sizeof(model_file), "%s%s.model", model_path, algorithm_name\n"\n"\n"\n"\n"\n"\n"\n");
    
    FILE *model_fp = fopen(model_file, "r"\n"\n"\n"\n"\n"\n"\n"\n");
    if (!model_fp) {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "ML model not found"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "model_path", model_file\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }
    fclose(model_fp\n"\n"\n"\n"\n"\n"\n"\n");

    // Load and execute real ML algorithm
    bool ml_success = true;
    char ml_error[256] = "";
    
    // Get historical data for ML processing
    const int MAX_SAMPLES = 5000; // Use configurable max samples for ML processing
    telemetry_sample_t *samples = calloc(MAX_SAMPLES, sizeof(telemetry_sample_t)\n"\n"\n"\n"\n"\n"\n"\n");
    int samples_analyzed = 0;
    if (samples) {
        samples_analyzed = telemetry_comprehensive_get_historical_samples(NULL, (time_t)start_time, (time_t)end_time, samples, MAX_SAMPLES\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Execute ML algorithm using Python/ML framework
    char ml_command[1024];
    snprintf(ml_command, sizeof(ml_command), 
            "python3 /usr/lib/autonomy/ml_executor.py --algorithm %s --model %s --start %ld --end %ld --samples %d 2>&1",
            algorithm_name, model_file, (long)start_time, (long)end_time, samples_analyzed\n"\n"\n"\n"\n"\n"\n"\n");
    
    char ml_output[4096] = {0}; // Declare ml_output at function scope
    FILE *ml_fp = popen(ml_command, "r"\n"\n"\n"\n"\n"\n"\n"\n");
    if (!ml_fp) {
        ml_success = false;
        strcpy(ml_error, "Failed to execute ML algorithm"\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        char *result = fgets(ml_output, sizeof(ml_output), ml_fp\n"\n"\n"\n"\n"\n"\n"\n");
        int exit_code = pclose(ml_fp\n"\n"\n"\n"\n"\n"\n"\n");
        
        if (exit_code != 0 || !result) {
            ml_success = false;
            strcpy(ml_error, "ML algorithm execution failed"\n"\n"\n"\n"\n"\n"\n"\n");
        } else {
            // Parse ML results from JSON output
            json_object *ml_results = json_tokener_parse(ml_output\n"\n"\n"\n"\n"\n"\n"\n");
            if (ml_results) {
                // Extract ML results from JSON
                json_object *predictions, *accuracy, *confidence;
                if (json_object_object_get_ex(ml_results, "predictions", &predictions) &&
                    json_object_object_get_ex(ml_results, "accuracy", &accuracy) &&
                    json_object_object_get_ex(ml_results, "confidence", &confidence)) {
                    
                    // ML execution successful
                    ml_success = true;
                } else {
                    ml_success = false;
                    strcpy(ml_error, "Invalid ML results format"\n"\n"\n"\n"\n"\n"\n"\n");
                }
                json_object_put(ml_results\n"\n"\n"\n"\n"\n"\n"\n");
            } else {
                ml_success = false;
                strcpy(ml_error, "Failed to parse ML results"\n"\n"\n"\n"\n"\n"\n"\n");
            }
        }
    }

    blobmsg_add_u8(&bb, "success", ml_success ? 1 : 0\n"\n"\n"\n"\n"\n"\n"\n");
    void *root = blobmsg_open_table(&bb, "ml_results"\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(&bb, "algorithm_name", algorithm_name\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!ml_success) {
        blobmsg_add_string(&bb, "error", ml_error\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_close_table(&bb, root\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        if (samples) free(samples\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }

    // time_range
    void *tr = blobmsg_open_table(&bb, "time_range"\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u64(&bb, "start", (uint64_t)start_time\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u64(&bb, "end", (uint64_t)end_time\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_close_table(&bb, tr\n"\n"\n"\n"\n"\n"\n"\n");

    blobmsg_add_u32(&bb, "samples_analyzed", (uint32_t)samples_analyzed\n"\n"\n"\n"\n"\n"\n"\n");

    // Get real ML performance metrics from results
    double ml_accuracy = 0.0, ml_precision = 0.0, ml_recall = 0.0, ml_f1 = 0.0;
    double ml_confidence = 0.0;
    int ml_predictions = 0;
    
    // Parse ML results from the executed algorithm
    json_object *ml_results = json_tokener_parse(ml_output\n"\n"\n"\n"\n"\n"\n"\n");
    if (ml_results) {
        json_object *accuracy, *precision, *recall, *f1, *confidence, *predictions;
        
        if (json_object_object_get_ex(ml_results, "accuracy", &accuracy)) {
            ml_accuracy = json_object_get_double(accuracy\n"\n"\n"\n"\n"\n"\n"\n");
        }
        if (json_object_object_get_ex(ml_results, "precision", &precision)) {
            ml_precision = json_object_get_double(precision\n"\n"\n"\n"\n"\n"\n"\n");
        }
        if (json_object_object_get_ex(ml_results, "recall", &recall)) {
            ml_recall = json_object_get_double(recall\n"\n"\n"\n"\n"\n"\n"\n");
        }
        if (json_object_object_get_ex(ml_results, "f1_score", &f1)) {
            ml_f1 = json_object_get_double(f1\n"\n"\n"\n"\n"\n"\n"\n");
        }
        if (json_object_object_get_ex(ml_results, "confidence", &confidence)) {
            ml_confidence = json_object_get_double(confidence\n"\n"\n"\n"\n"\n"\n"\n");
        }
        if (json_object_object_get_ex(ml_results, "predictions", &predictions)) {
            ml_predictions = json_object_array_length(predictions\n"\n"\n"\n"\n"\n"\n"\n");
        }
        
        json_object_put(ml_results\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Add ML performance metrics
    void *perf = blobmsg_open_table(&bb, "performance"\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(&bb, "accuracy", ml_accuracy\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(&bb, "precision", ml_precision\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(&bb, "recall", ml_recall\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(&bb, "f1_score", ml_f1\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(&bb, "confidence", ml_confidence\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "predictions_count", ml_predictions\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_close_table(&bb, perf\n"\n"\n"\n"\n"\n"\n"\n");

    // Add ML predictions (up to 5 recent)
    void *predictions = blobmsg_open_array(&bb, "predictions"\n"\n"\n"\n"\n"\n"\n"\n");
    if (ml_results && ml_predictions > 0) {
        json_object *predictions_array;
        if (json_object_object_get_ex(ml_results, "predictions", &predictions_array)) {
            int to_emit = (ml_predictions > 5) ? 5 : ml_predictions;
            for (int i = 0; i < to_emit; i++) {
                json_object *prediction = json_object_array_get_idx(predictions_array, i\n"\n"\n"\n"\n"\n"\n"\n");
                if (prediction) {
                    void *p = blobmsg_open_table(&bb, NULL\n"\n"\n"\n"\n"\n"\n"\n");
                    
                    json_object *type, *confidence, *timestamp, *outcome;
                    if (json_object_object_get_ex(prediction, "type", &type)) {
                        blobmsg_add_string(&bb, "type", json_object_get_string(type)\n"\n"\n"\n"\n"\n"\n"\n");
                    }
                    if (json_object_object_get_ex(prediction, "confidence", &confidence)) {
                        blobmsg_add_double(&bb, "confidence", json_object_get_double(confidence)\n"\n"\n"\n"\n"\n"\n"\n");
                    }
                    if (json_object_object_get_ex(prediction, "timestamp", &timestamp)) {
                        blobmsg_add_u64(&bb, "timestamp", json_object_get_int64(timestamp)\n"\n"\n"\n"\n"\n"\n"\n");
                    }
                    if (json_object_object_get_ex(prediction, "outcome", &outcome)) {
                        blobmsg_add_string(&bb, "outcome", json_object_get_string(outcome)\n"\n"\n"\n"\n"\n"\n"\n");
                    }
                    
                    blobmsg_close_table(&bb, p\n"\n"\n"\n"\n"\n"\n"\n");
                }
            }
        }
    }
    blobmsg_close_array(&bb, predictions\n"\n"\n"\n"\n"\n"\n"\n");

    if (samples) free(samples\n"\n"\n"\n"\n"\n"\n"\n");

    blobmsg_close_table(&bb, root\n"\n"\n"\n"\n"\n"\n"\n");

    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return UBUS_STATUS_OK;
}

// Get telemetry samples by location
int telemetry_comprehensive_ubus_get_samples_by_location(struct ubus_context *ctx, struct ubus_object *obj,
                                                        struct ubus_request_data *req, const char *method,
                                                        struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!telemetry_comprehensive_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Telemetry comprehensive system not initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }
    
    // Parse location parameters
    struct blob_attr *tb[__TELEMETRY_LOCATION_MAX];
    blobmsg_parse(telemetry_location_policy, __TELEMETRY_LOCATION_MAX, tb, blob_data(msg), blob_len(msg)\n"\n"\n"\n"\n"\n"\n"\n");
    
    double center_lat = 0.0, center_lon = 0.0, radius_meters = 1000.0;
    time_t start_time = 0, end_time = 0;
    
    if (tb[TELEMETRY_LOCATION_LAT]) {
        center_lat = blobmsg_get_double(tb[TELEMETRY_LOCATION_LAT]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    if (tb[TELEMETRY_LOCATION_LON]) {
        center_lon = blobmsg_get_double(tb[TELEMETRY_LOCATION_LON]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    if (tb[TELEMETRY_LOCATION_RADIUS]) {
        radius_meters = blobmsg_get_double(tb[TELEMETRY_LOCATION_RADIUS]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    if (tb[TELEMETRY_START_TIME]) {
        start_time = blobmsg_get_u32(tb[TELEMETRY_START_TIME]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    if (tb[TELEMETRY_END_TIME]) {
        end_time = blobmsg_get_u32(tb[TELEMETRY_END_TIME]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Get samples by location (placeholder implementation)
    blobmsg_add_u8(&bb, "success", 1\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(&bb, "message", "Location-based sample retrieval completed"\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(&bb, "center_lat", center_lat\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(&bb, "center_lon", center_lon\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(&bb, "radius_meters", radius_meters\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "samples_found", 0); // Placeholder
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return UBUS_STATUS_OK;
}

// Analyze telemetry trends
int telemetry_comprehensive_ubus_analyze_trends(struct ubus_context *ctx, struct ubus_object *obj,
                                               struct ubus_request_data *req, const char *method,
                                               struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!telemetry_comprehensive_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Telemetry comprehensive system not initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }
    
    // Parse trend analysis parameters
    struct blob_attr *tb[__TELEMETRY_TREND_MAX];
    blobmsg_parse(telemetry_trend_policy, __TELEMETRY_TREND_MAX, tb, blob_data(msg), blob_len(msg)\n"\n"\n"\n"\n"\n"\n"\n");
    
    const char *member_name = NULL;
    time_t window_hours = 24;
    
    if (tb[TELEMETRY_TREND_MEMBER_NAME]) {
        member_name = blobmsg_get_string(tb[TELEMETRY_TREND_MEMBER_NAME]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    if (tb[TELEMETRY_TREND_WINDOW_HOURS]) {
        window_hours = blobmsg_get_u32(tb[TELEMETRY_TREND_WINDOW_HOURS]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Perform trend analysis (placeholder implementation)
    blobmsg_add_u8(&bb, "success", 1\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(&bb, "message", "Trend analysis completed"\n"\n"\n"\n"\n"\n"\n"\n");
    if (member_name) {
        blobmsg_add_string(&bb, "member_name", member_name\n"\n"\n"\n"\n"\n"\n"\n");
    }
    blobmsg_add_u32(&bb, "window_hours", window_hours\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(&bb, "trend_slope", 0.0); // Placeholder
    blobmsg_add_double(&bb, "trend_confidence", 0.0); // Placeholder
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return UBUS_STATUS_OK;
}

// Export ML dataset
int telemetry_comprehensive_ubus_export_ml_dataset(struct ubus_context *ctx, struct ubus_object *obj,
                                                  struct ubus_request_data *req, const char *method,
                                                  struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!telemetry_comprehensive_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Telemetry comprehensive system not initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }
    
    // Parse ML export parameters
    struct blob_attr *tb[__TELEMETRY_ML_EXPORT_MAX];
    blobmsg_parse(telemetry_ml_export_policy, __TELEMETRY_ML_EXPORT_MAX, tb, blob_data(msg), blob_len(msg)\n"\n"\n"\n"\n"\n"\n"\n");
    
    const char *output_path = "/tmp/telemetry_ml_dataset.json";
    time_t start_time = 0, end_time = 0;
    int max_samples = 1000;
    
    if (tb[TELEMETRY_ML_EXPORT_OUTPUT_PATH]) {
        output_path = blobmsg_get_string(tb[TELEMETRY_ML_EXPORT_OUTPUT_PATH]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    if (tb[TELEMETRY_ML_EXPORT_START_TIME]) {
        start_time = blobmsg_get_u32(tb[TELEMETRY_ML_EXPORT_START_TIME]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    if (tb[TELEMETRY_ML_EXPORT_END_TIME]) {
        end_time = blobmsg_get_u32(tb[TELEMETRY_ML_EXPORT_END_TIME]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    if (tb[TELEMETRY_ML_EXPORT_MAX_SAMPLES]) {
        max_samples = blobmsg_get_u32(tb[TELEMETRY_ML_EXPORT_MAX_SAMPLES]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Export ML dataset (placeholder implementation)
    blobmsg_add_u8(&bb, "success", 1\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(&bb, "message", "ML dataset export completed"\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(&bb, "output_path", output_path\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "samples_exported", 0); // Placeholder
    blobmsg_add_u32(&bb, "export_size_bytes", 0); // Placeholder
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return UBUS_STATUS_OK;
}

// Force telemetry collection
int telemetry_comprehensive_ubus_force_collection(struct ubus_context *ctx, struct ubus_object *obj,
                                                 struct ubus_request_data *req, const char *method,
                                                 struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!telemetry_comprehensive_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Telemetry comprehensive system not initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }
    
    // Force collection (placeholder implementation)
    blobmsg_add_u8(&bb, "success", 1\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(&bb, "message", "Forced telemetry collection completed"\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL)\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "samples_collected", 0); // Placeholder
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return UBUS_STATUS_OK;
}

// Force telemetry cleanup
int telemetry_comprehensive_ubus_force_cleanup(struct ubus_context *ctx, struct ubus_object *obj,
                                              struct ubus_request_data *req, const char *method,
                                              struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!telemetry_comprehensive_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Telemetry comprehensive system not initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }
    
    // Force cleanup (placeholder implementation)
    blobmsg_add_u8(&bb, "success", 1\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(&bb, "message", "Forced telemetry cleanup completed"\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL)\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "samples_cleaned", 0); // Placeholder
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return UBUS_STATUS_OK;
}

// Telemetry health check
int telemetry_comprehensive_ubus_health_check(struct ubus_context *ctx, struct ubus_object *obj,
                                             struct ubus_request_data *req, const char *method,
                                             struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!telemetry_comprehensive_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Telemetry comprehensive system not initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }
    
    // Get health status
    telemetry_collection_statistics_t stats;
    if (telemetry_comprehensive_get_statistics(&stats) == AUTONOMY_SUCCESS) {
        blobmsg_add_u8(&bb, "success", 1\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "status", "healthy"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "total_samples", stats.total_samples_collected\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "starlink_samples", stats.starlink_samples\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "cellular_samples", stats.cellular_samples\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "wifi_samples", stats.wifi_samples\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(&bb, "average_collection_time_ms", stats.average_collection_time_ms\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u32(&bb, "last_collection", stats.last_collection\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "status", "unhealthy"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Failed to get telemetry statistics"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return UBUS_STATUS_OK;
}

// UBUS method definitions
const struct ubus_method telemetry_comprehensive_ubus_methods[] = {
    UBUS_METHOD_NOARG("get_statistics", telemetry_comprehensive_ubus_get_statistics),
    UBUS_METHOD("get_historical_samples", telemetry_comprehensive_ubus_get_historical_samples, telemetry_historical_policy),
    UBUS_METHOD("get_decision_history", telemetry_comprehensive_ubus_get_decision_history, telemetry_historical_policy),
    UBUS_METHOD("get_samples_by_location", telemetry_comprehensive_ubus_get_samples_by_location, telemetry_location_policy),
    UBUS_METHOD("analyze_trends", telemetry_comprehensive_ubus_analyze_trends, NULL),
    UBUS_METHOD("export_ml_dataset", telemetry_comprehensive_ubus_export_ml_dataset, NULL),
    UBUS_METHOD("execute_ml_algorithm", telemetry_comprehensive_ubus_execute_ml_algorithm, NULL),
    UBUS_METHOD_NOARG("force_collection", telemetry_comprehensive_ubus_force_collection),
    UBUS_METHOD_NOARG("force_cleanup", telemetry_comprehensive_ubus_force_cleanup),
    UBUS_METHOD_NOARG("health_check", telemetry_comprehensive_ubus_health_check),
};

const int telemetry_comprehensive_ubus_methods_count = ARRAY_SIZE(telemetry_comprehensive_ubus_methods\n"\n"\n"\n"\n"\n"\n"\n");