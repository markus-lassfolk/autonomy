#include "starlink_comprehensive_ubus.h"
#include "../starlink/starlink_comprehensive.h"
#include "../core/types.h"
#include "../shared/logging/logx.h"
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>

// Helper function to add comprehensive GPS data to blob
void add_starlink_gps_data_to_blob(struct blob_buf *bb, const char *name, 
                                          const starlink_comprehensive_gps_t *gps_data) {
    void *gps_table = blobmsg_open_table(bb, name\n"\n"\n"\n"\n"\n"\n"\n");
    
    blobmsg_add_double(bb, "latitude", gps_data->latitude\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "longitude", gps_data->longitude\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "altitude", gps_data->altitude\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "accuracy", gps_data->accuracy\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "horizontal_speed_mps", gps_data->horizontal_speed_mps\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "vertical_speed_mps", gps_data->vertical_speed_mps\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(bb, "gps_source", gps_data->gps_source\n"\n"\n"\n"\n"\n"\n"\n");
    
    blobmsg_add_u8(bb, "gps_valid", gps_data->gps_valid\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(bb, "gps_satellites", gps_data->gps_satellites\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(bb, "no_sats_after_ttff", gps_data->no_sats_after_ttff\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(bb, "inhibit_gps", gps_data->inhibit_gps\n"\n"\n"\n"\n"\n"\n"\n");
    
    blobmsg_add_u8(bb, "location_enabled", gps_data->location_enabled\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "uncertainty_meters", gps_data->uncertainty_meters\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(bb, "uncertainty_meters_valid", gps_data->uncertainty_meters_valid\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "gps_time_s", gps_data->gps_time_s\n"\n"\n"\n"\n"\n"\n"\n");
    
    blobmsg_add_u8(bb, "valid", gps_data->valid\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "confidence", gps_data->confidence\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(bb, "quality_score", gps_data->quality_score\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(bb, "data_sources", gps_data->data_sources\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(bb, "collected_at", (uint32_t)gps_data->collected_at\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "collection_ms", gps_data->collection_ms\n"\n"\n"\n"\n"\n"\n"\n");
    
    blobmsg_close_table(bb, gps_table\n"\n"\n"\n"\n"\n"\n"\n");
}

// Helper function to add events analysis to blob
void add_events_analysis_to_blob(struct blob_buf *bb, const char *name,
                                        const starlink_events_outages_analysis_t *analysis) {
    void *analysis_table = blobmsg_open_table(bb, name\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Add events array
    void *events_array = blobmsg_open_array(bb, "events"\n"\n"\n"\n"\n"\n"\n"\n");
    for (int i = 0; i < analysis->event_count; i++) {
        const starlink_event_t* event = &analysis->events[i];
        
        void *event_table = blobmsg_open_table(bb, NULL\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(bb, "severity", starlink_event_severity_to_string(event->severity)\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(bb, "reason", starlink_event_reason_to_string(event->reason)\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(bb, "start_timestamp_ns", event->start_timestamp_ns\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(bb, "duration_ns", event->duration_ns\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(bb, "message", event->message\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u8(bb, "ongoing", event->ongoing\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u32(bb, "recorded_at", (uint32_t)event->recorded_at\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_close_table(bb, event_table\n"\n"\n"\n"\n"\n"\n"\n");
    }
    blobmsg_close_array(bb, events_array\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Add outages array
    void *outages_array = blobmsg_open_array(bb, "outages"\n"\n"\n"\n"\n"\n"\n"\n");
    for (int i = 0; i < analysis->outage_count; i++) {
        const starlink_outage_t* outage = &analysis->outages[i];
        
        void *outage_table = blobmsg_open_table(bb, NULL\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(bb, "cause", starlink_outage_cause_to_string(outage->cause)\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(bb, "start_timestamp_ns", outage->start_timestamp_ns\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(bb, "duration_ns", outage->duration_ns\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u8(bb, "did_switch", outage->did_switch\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(bb, "cause_description", outage->cause_description\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u32(bb, "recorded_at", (uint32_t)outage->recorded_at\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_close_table(bb, outage_table\n"\n"\n"\n"\n"\n"\n"\n");
    }
    blobmsg_close_array(bb, outages_array\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Add analysis summary
    void *summary_table = blobmsg_open_table(bb, "analysis"\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(bb, "critical_events_24h", analysis->critical_events_24h\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(bb, "warning_events_24h", analysis->warning_events_24h\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(bb, "total_outages_24h", analysis->total_outages_24h\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "avg_outage_duration_s", analysis->avg_outage_duration_s\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "outage_frequency_per_hour", analysis->outage_frequency_per_hour\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(bb, "primary_cause", starlink_outage_cause_to_string(analysis->primary_cause)\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(bb, "outage_pattern_detected", analysis->outage_pattern_detected\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(bb, "event_escalation_detected", analysis->event_escalation_detected\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_double(bb, "stability_score", analysis->stability_score\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(bb, "last_analysis", (uint32_t)analysis->last_analysis\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_close_table(bb, summary_table\n"\n"\n"\n"\n"\n"\n"\n");
    
    blobmsg_close_table(bb, analysis_table\n"\n"\n"\n"\n"\n"\n"\n");
}

// Get comprehensive Starlink status
int starlink_comprehensive_ubus_get_status(struct ubus_context *ctx, struct ubus_object *obj,
                                          struct ubus_request_data *req, const char *method,
                                          struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!starlink_comprehensive_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Comprehensive Starlink collector not initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }
    
    starlink_comprehensive_status_t status;
    if (starlink_comprehensive_collect_all(&status) == AUTONOMY_SUCCESS) {
        blobmsg_add_u8(&bb, "success", 1\n"\n"\n"\n"\n"\n"\n"\n");
        
        void *comprehensive_table = blobmsg_open_table(&bb, "comprehensive_status"\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Add device information
        void *device_table = blobmsg_open_table(&bb, "device_info"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "id", status.device_info.id\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "hardware_version", status.device_info.hardware_version\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "software_version", status.device_info.software_version\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "country_code", status.device_info.country_code\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u32(&bb, "generation_number", status.device_info.generation_number\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "uptime_s", status.device_state.uptime_s\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_close_table(&bb, device_table\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Add comprehensive GPS data
        add_starlink_gps_data_to_blob(&bb, "gps_data", &status.gps_data\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Add network performance
        void *network_table = blobmsg_open_table(&bb, "network_performance"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(&bb, "pop_ping_latency_ms", status.pop_ping_latency_ms\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(&bb, "downlink_throughput_bps", status.downlink_throughput_bps\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(&bb, "uplink_throughput_bps", status.uplink_throughput_bps\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_close_table(&bb, network_table\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Add obstruction statistics
        void *obstruction_table = blobmsg_open_table(&bb, "obstruction_stats"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u8(&bb, "currently_obstructed", status.obstruction_stats.currently_obstructed\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(&bb, "fraction_obstructed", status.obstruction_stats.fraction_obstructed\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u32(&bb, "last24h_obstructed_s", status.obstruction_stats.last24h_obstructed_s\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(&bb, "avg_prolonged_obstruction_interval_s", 
                          status.obstruction_stats.avg_prolonged_obstruction_interval_s\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_close_table(&bb, obstruction_table\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Add events analysis
        add_events_analysis_to_blob(&bb, "events_analysis", &status.events_analysis\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Add health scores
        void *health_table = blobmsg_open_table(&bb, "health_scores"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(&bb, "overall_health", status.overall_health_score\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(&bb, "gps_quality", status.gps_quality_score\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(&bb, "network_quality", status.network_quality_score\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(&bb, "stability", status.stability_score\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_close_table(&bb, health_table\n"\n"\n"\n"\n"\n"\n"\n");
        
        blobmsg_add_u32(&bb, "last_update", (uint32_t)status.last_update\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(&bb, "collection_duration_ms", status.collection_duration_ms\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "collection_status", status.collection_status\n"\n"\n"\n"\n"\n"\n"\n");
        
        blobmsg_close_table(&bb, comprehensive_table\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Failed to collect comprehensive Starlink data"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return UBUS_STATUS_OK;
}

// Get comprehensive Starlink GPS data
int starlink_comprehensive_ubus_get_gps_data(struct ubus_context *ctx, struct ubus_object *obj,
                                            struct ubus_request_data *req, const char *method,
                                            struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!starlink_comprehensive_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Comprehensive Starlink collector not initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }
    
    starlink_comprehensive_gps_t gps_data;
    if (starlink_comprehensive_collect_gps(&gps_data) == AUTONOMY_SUCCESS) {
        blobmsg_add_u8(&bb, "success", 1\n"\n"\n"\n"\n"\n"\n"\n");
        add_starlink_gps_data_to_blob(&bb, "gps_data", &gps_data\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Failed to collect Starlink GPS data"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return UBUS_STATUS_OK;
}

// Get Starlink events and outages analysis
int starlink_comprehensive_ubus_get_events_analysis(struct ubus_context *ctx, struct ubus_object *obj,
                                                   struct ubus_request_data *req, const char *method,
                                                   struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!starlink_comprehensive_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Comprehensive Starlink collector not initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }
    
    starlink_events_outages_analysis_t analysis;
    if (starlink_comprehensive_analyze_events(&analysis) == AUTONOMY_SUCCESS) {
        blobmsg_add_u8(&bb, "success", 1\n"\n"\n"\n"\n"\n"\n"\n");
        add_events_analysis_to_blob(&bb, "events_analysis", &analysis\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Failed to analyze Starlink events and outages"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return UBUS_STATUS_OK;
}

// Get Starlink stability score and health metrics
int starlink_comprehensive_ubus_get_stability(struct ubus_context *ctx, struct ubus_object *obj,
                                             struct ubus_request_data *req, const char *method,
                                             struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!starlink_comprehensive_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Comprehensive Starlink collector not initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }
    
    blobmsg_add_u8(&bb, "success", 1\n"\n"\n"\n"\n"\n"\n"\n");
    
    starlink_comprehensive_status_t status;
    if (starlink_comprehensive_get_status(&status) == AUTONOMY_SUCCESS) {
        void *stability_table = blobmsg_open_table(&bb, "stability"\n"\n"\n"\n"\n"\n"\n"\n");
        
        blobmsg_add_double(&bb, "stability_score", status.stability_score\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(&bb, "health_score", status.overall_health_score\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(&bb, "gps_quality_score", status.gps_quality_score\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(&bb, "network_quality_score", status.network_quality_score\n"\n"\n"\n"\n"\n"\n"\n");
        
        blobmsg_add_u32(&bb, "recent_events", status.events_analysis.event_count\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u32(&bb, "recent_outages", status.events_analysis.outage_count\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u8(&bb, "pattern_detected", status.events_analysis.outage_pattern_detected\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u8(&bb, "escalation_detected", status.events_analysis.event_escalation_detected\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Determine primary issue and recommendation
        const char* primary_issue = "none";
        const char* recommendation = "stable";
        
        if (status.events_analysis.critical_events_24h > 0) {
            primary_issue = "critical_events";
            recommendation = "monitor_closely";
        } else if (status.events_analysis.outage_pattern_detected) {
            primary_issue = "outage_pattern";
            recommendation = "investigate_cause";
        } else if (status.obstruction_stats.fraction_obstructed > 0.1) {
            primary_issue = "obstruction";
            recommendation = "check_dish_alignment";
        } else if (status.stability_score < 0.7) {
            primary_issue = "low_stability";
            recommendation = "monitor_events";
        }
        
        blobmsg_add_string(&bb, "primary_issue", primary_issue\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "recommendation", recommendation\n"\n"\n"\n"\n"\n"\n"\n");
        
        blobmsg_close_table(&bb, stability_table\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return UBUS_STATUS_OK;
}

// Get comprehensive Starlink statistics
int starlink_comprehensive_ubus_get_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                              struct ubus_request_data *req, const char *method,
                                              struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!starlink_comprehensive_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Comprehensive Starlink collector not initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }
    
    blobmsg_add_u8(&bb, "success", 1\n"\n"\n"\n"\n"\n"\n"\n");
    
    uint64_t total_collections, successful_collections, failed_collections;
    double avg_collection_time_ms, avg_gps_confidence, avg_stability_score;
    
    if (starlink_comprehensive_get_statistics(&total_collections, &successful_collections,
                                             &failed_collections, &avg_collection_time_ms,
                                             &avg_gps_confidence, &avg_stability_score) == AUTONOMY_SUCCESS) {
        
        void *stats_table = blobmsg_open_table(&bb, "statistics"\n"\n"\n"\n"\n"\n"\n"\n");
        
        blobmsg_add_u64(&bb, "total_collections", total_collections\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "successful_collections", successful_collections\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "failed_collections", failed_collections\n"\n"\n"\n"\n"\n"\n"\n");
        
        double success_rate = total_collections > 0 ? 
            (double)successful_collections / total_collections : 0.0;
        blobmsg_add_double(&bb, "collection_success_rate", success_rate\n"\n"\n"\n"\n"\n"\n"\n");
        
        void *api_calls_table = blobmsg_open_table(&bb, "api_calls"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "get_location", g_starlink_comprehensive.api_calls_location\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "get_status", g_starlink_comprehensive.api_calls_status\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "get_diagnostics", g_starlink_comprehensive.api_calls_diagnostics\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u64(&bb, "get_history", g_starlink_comprehensive.api_calls_history\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_close_table(&bb, api_calls_table\n"\n"\n"\n"\n"\n"\n"\n");
        
        void *performance_table = blobmsg_open_table(&bb, "performance"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(&bb, "avg_collection_time_ms", avg_collection_time_ms\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(&bb, "avg_gps_confidence", avg_gps_confidence\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_double(&bb, "avg_stability_score", avg_stability_score\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_close_table(&bb, performance_table\n"\n"\n"\n"\n"\n"\n"\n");
        
        blobmsg_close_table(&bb, stats_table\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return UBUS_STATUS_OK;
}

// Force comprehensive collection
int starlink_comprehensive_ubus_force_collection(struct ubus_context *ctx, struct ubus_object *obj,
                                                struct ubus_request_data *req, const char *method,
                                                struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!starlink_comprehensive_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Starlink comprehensive collector not initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }
    
    // Force collection of all data
    starlink_comprehensive_status_t status;
    if (starlink_comprehensive_collect_all(&status) == AUTONOMY_SUCCESS) {
        blobmsg_add_u8(&bb, "success", 1\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "message", "Comprehensive collection completed"\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL)\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Failed to force comprehensive collection"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return UBUS_STATUS_OK;
}

// Get comprehensive configuration
int starlink_comprehensive_ubus_get_config(struct ubus_context *ctx, struct ubus_object *obj,
                                          struct ubus_request_data *req, const char *method,
                                          struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!starlink_comprehensive_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Starlink comprehensive collector not initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }
    
    blobmsg_add_u8(&bb, "success", 1\n"\n"\n"\n"\n"\n"\n"\n");
    
    void *config_table = blobmsg_open_table(&bb, "config"\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(&bb, "enabled", true); // Placeholder
    blobmsg_add_string(&bb, "host", "192.168.100.1"); // Placeholder
    blobmsg_add_u32(&bb, "port", 9200); // Placeholder
    blobmsg_add_u32(&bb, "timeout_seconds", 30); // Placeholder
    blobmsg_add_u32(&bb, "collection_interval_s", 60); // Placeholder
    blobmsg_add_u8(&bb, "collect_location", true); // Placeholder
    blobmsg_add_u8(&bb, "collect_status", true); // Placeholder
    blobmsg_add_u8(&bb, "collect_diagnostics", true); // Placeholder
    blobmsg_add_u8(&bb, "collect_history", true); // Placeholder
    blobmsg_add_u8(&bb, "enable_events_analysis", true); // Placeholder
    blobmsg_add_u8(&bb, "enable_outages_analysis", true); // Placeholder
    blobmsg_add_u32(&bb, "max_events", 50); // Placeholder
    blobmsg_add_u32(&bb, "max_outages", 20); // Placeholder
    blobmsg_add_u32(&bb, "analysis_window_hours", 24); // Placeholder
    blobmsg_add_double(&bb, "min_gps_confidence", 0.5); // Placeholder
    blobmsg_add_double(&bb, "min_network_quality", 0.7); // Placeholder
    blobmsg_add_double(&bb, "min_stability_score", 0.6); // Placeholder
    blobmsg_add_u8(&bb, "enable_health_monitoring", true); // Placeholder
    blobmsg_add_u32(&bb, "health_check_interval_s", 300); // Placeholder
    blobmsg_add_u32(&bb, "max_consecutive_failures", 3); // Placeholder
    blobmsg_close_table(&bb, config_table\n"\n"\n"\n"\n"\n"\n"\n");
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return UBUS_STATUS_OK;
}

// Perform comprehensive health check
int starlink_comprehensive_ubus_health_check(struct ubus_context *ctx, struct ubus_object *obj,
                                            struct ubus_request_data *req, const char *method,
                                            struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!starlink_comprehensive_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0\n"\n"\n"\n"\n"\n"\n"\n");
        blobmsg_add_string(&bb, "error", "Starlink comprehensive collector not initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
        blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
        return UBUS_STATUS_OK;
    }
    
    blobmsg_add_u8(&bb, "success", 1\n"\n"\n"\n"\n"\n"\n"\n");
    
    void *health_table = blobmsg_open_table(&bb, "health"\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(&bb, "initialized", starlink_comprehensive_is_initialized()\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u8(&bb, "healthy", true); // Placeholder
    blobmsg_add_string(&bb, "status", "operational"); // Placeholder
    blobmsg_add_double(&bb, "stability_score", starlink_comprehensive_get_stability_score()\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL)\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_close_table(&bb, health_table\n"\n"\n"\n"\n"\n"\n"\n");
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    return UBUS_STATUS_OK;
}

// UBUS method definitions
const struct ubus_method starlink_comprehensive_ubus_methods[] = {
    UBUS_METHOD_NOARG("get_comprehensive_status", starlink_comprehensive_ubus_get_status),
    UBUS_METHOD_NOARG("get_comprehensive_gps", starlink_comprehensive_ubus_get_gps_data),
    UBUS_METHOD_NOARG("get_events_analysis", starlink_comprehensive_ubus_get_events_analysis),
    UBUS_METHOD_NOARG("get_stability", starlink_comprehensive_ubus_get_stability),
    UBUS_METHOD_NOARG("get_comprehensive_stats", starlink_comprehensive_ubus_get_statistics),
    UBUS_METHOD_NOARG("force_comprehensive_collection", starlink_comprehensive_ubus_force_collection),
    UBUS_METHOD_NOARG("get_comprehensive_config", starlink_comprehensive_ubus_get_config),
    UBUS_METHOD_NOARG("comprehensive_health_check", starlink_comprehensive_ubus_health_check),
};

const int starlink_comprehensive_ubus_methods_count = ARRAY_SIZE(starlink_comprehensive_ubus_methods\n"\n"\n"\n"\n"\n"\n"\n");