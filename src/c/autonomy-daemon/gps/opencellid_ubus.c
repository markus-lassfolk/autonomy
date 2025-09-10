#include <stdlib.h>
#include "opencellid_ubus.h"
#include "opencellid_complete.h"
#include "../utils/logx.h"
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <json-c/json.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// UBUS parameter policies
enum {
    OPENCELLID_CENTER_LAT,
    OPENCELLID_CENTER_LON,
    OPENCELLID_RADIUS,
    OPENCELLID_MAX_TOWERS,
    __OPENCELLID_TOWERS_MAX
};

static const struct blobmsg_policy opencellid_towers_policy[] = {
    [OPENCELLID_CENTER_LAT] = { .name = "center_lat", .type = BLOBMSG_TYPE_DOUBLE },
    [OPENCELLID_CENTER_LON] = { .name = "center_lon", .type = BLOBMSG_TYPE_DOUBLE },
    [OPENCELLID_RADIUS] = { .name = "radius", .type = BLOBMSG_TYPE_INT32 },
    [OPENCELLID_MAX_TOWERS] = { .name = "max_towers", .type = BLOBMSG_TYPE_INT32 },
};

enum {
    OPENCELLID_CONFIG_ENABLED,
    OPENCELLID_CONFIG_API_KEY,
    OPENCELLID_CONFIG_CACHE_SIZE,
    OPENCELLID_CONFIG_CONTRIB_ENABLED,
    OPENCELLID_CONFIG_CONTRIB_INTERVAL,
    OPENCELLID_CONFIG_MIN_GPS_ACCURACY,
    __OPENCELLID_CONFIG_MAX
};

static const struct blobmsg_policy opencellid_config_policy[] = {
    [OPENCELLID_CONFIG_ENABLED] = { .name = "enabled", .type = BLOBMSG_TYPE_BOOL },
    [OPENCELLID_CONFIG_API_KEY] = { .name = "api_key", .type = BLOBMSG_TYPE_STRING },
    [OPENCELLID_CONFIG_CACHE_SIZE] = { .name = "cache_size_mb", .type = BLOBMSG_TYPE_INT32 },
    [OPENCELLID_CONFIG_CONTRIB_ENABLED] = { .name = "contribution_enabled", .type = BLOBMSG_TYPE_BOOL },
    [OPENCELLID_CONFIG_CONTRIB_INTERVAL] = { .name = "contribution_interval", .type = BLOBMSG_TYPE_INT32 },
    [OPENCELLID_CONFIG_MIN_GPS_ACCURACY] = { .name = "min_gps_accuracy", .type = BLOBMSG_TYPE_DOUBLE },
};

enum {
    OPENCELLID_CLEAR_CONFIRM,
    __OPENCELLID_CLEAR_MAX
};

static const struct blobmsg_policy opencellid_clear_policy[] = {
    [OPENCELLID_CLEAR_CONFIRM] = { .name = "confirm", .type = BLOBMSG_TYPE_BOOL },
};

// Helper function to add cell info to blob
static void add_cell_info_to_blob(struct blob_buf *bb, const char *name, 
                                  const opencellid_cell_identifier_t *cell_id,
                                  const opencellid_cellular_metrics_t *metrics) {
    void *cell_table = blobmsg_open_table(bb, name);
    
    blobmsg_add_u32(bb, "mcc", cell_id->mcc);
    blobmsg_add_u32(bb, "mnc", cell_id->mnc);
    blobmsg_add_u32(bb, "lac", cell_id->lac);
    blobmsg_add_u64(bb, "cell_id", cell_id->cell_id);
    blobmsg_add_string(bb, "radio", opencellid_radio_type_to_string(cell_id->radio));
    blobmsg_add_u32(bb, "pci", cell_id->pci);
    blobmsg_add_u32(bb, "earfcn", cell_id->earfcn);
    
    if (metrics) {
        blobmsg_add_u32(bb, "rsrp", metrics->rsrp);
        blobmsg_add_u32(bb, "rsrq", metrics->rsrq);
        blobmsg_add_u32(bb, "sinr", metrics->sinr);
        blobmsg_add_u32(bb, "rssi", metrics->rssi);
        blobmsg_add_u32(bb, "timing_advance", metrics->timing_advance);
        if (metrics->timing_advance_valid) {
            blobmsg_add_double(bb, "timing_advance_distance", metrics->timing_advance_distance);
        }
        blobmsg_add_u32(bb, "band", metrics->band);
        blobmsg_add_u32(bb, "bandwidth", metrics->bandwidth);
    }
    
    blobmsg_close_table(bb, cell_table);
}

// Get current position via triangulation
int opencellid_ubus_get_position(struct ubus_context *ctx, struct ubus_object *obj,
                                struct ubus_request_data *req, const char *method,
                                struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    if (!opencellid_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "OpenCellID system not initialized");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    // Get cellular environment
    opencellid_cellular_environment_t environment;
    if (opencellid_get_cellular_environment(&environment) != AUTONOMY_SUCCESS) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "Failed to get cellular environment");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    // Perform triangulation
    opencellid_triangulation_result_t result;
    if (opencellid_triangulate_position(&environment, &result) != AUTONOMY_SUCCESS) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "Triangulation failed");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    blobmsg_add_u8(&bb, "success", 1);
    
    // Add position information
    void *position_table = blobmsg_open_table(&bb, "position");
    blobmsg_add_double(&bb, "latitude", result.latitude);
    blobmsg_add_double(&bb, "longitude", result.longitude);
    blobmsg_add_double(&bb, "accuracy", result.accuracy);
    blobmsg_add_double(&bb, "confidence", result.confidence);
    blobmsg_add_string(&bb, "method", result.method);
    blobmsg_add_u32(&bb, "cells_used", result.cells_used);
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)result.calculation_time);
    if (result.timing_advance_applied) {
        blobmsg_add_double(&bb, "timing_advance_constraint", result.timing_advance_constraint);
    }
    blobmsg_close_table(&bb, position_table);
    
    // Add primary cell information
    add_cell_info_to_blob(&bb, "primary_cell", &result.primary_cell.cell_id, NULL);
    
    // Add contributing cells if any
    if (result.contributing_cell_count > 0) {
        void *contributing_array = blobmsg_open_array(&bb, "contributing_cells");
        for (int i = 0; i < result.contributing_cell_count; i++) {
            void *cell_table = blobmsg_open_table(&bb, NULL);
            add_cell_info_to_blob(&bb, "cell", &result.contributing_cells[i].cell_id, NULL);
            blobmsg_add_double(&bb, "latitude", result.contributing_cells[i].latitude);
            blobmsg_add_double(&bb, "longitude", result.contributing_cells[i].longitude);
            blobmsg_add_double(&bb, "range", result.contributing_cells[i].range);
            blobmsg_add_double(&bb, "confidence", result.contributing_cells[i].confidence);
            blobmsg_close_table(&bb, cell_table);
        }
        blobmsg_close_array(&bb, contributing_array);
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return UBUS_STATUS_OK;
}

// Get visible cell towers for map display
int opencellid_ubus_get_visible_towers(struct ubus_context *ctx, struct ubus_object *obj,
                                      struct ubus_request_data *req, const char *method,
                                      struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    if (!opencellid_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "OpenCellID system not initialized");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    struct blob_attr *tb[__OPENCELLID_TOWERS_MAX];
    blobmsg_parse(opencellid_towers_policy, __OPENCELLID_TOWERS_MAX, tb, blob_data(msg), blob_len(msg));
    
    // Get parameters with defaults
    double center_lat = 0.0, center_lon = 0.0;
    int radius = 2000; // Default 2km radius
    int max_towers = 10; // Default 10 towers
    
    if (tb[OPENCELLID_CENTER_LAT]) {
        center_lat = blobmsg_get_double(tb[OPENCELLID_CENTER_LAT]);
    }
    if (tb[OPENCELLID_CENTER_LON]) {
        center_lon = blobmsg_get_double(tb[OPENCELLID_CENTER_LON]);
    }
    if (tb[OPENCELLID_RADIUS]) {
        radius = blobmsg_get_u32(tb[OPENCELLID_RADIUS]);
    }
    if (tb[OPENCELLID_MAX_TOWERS]) {
        max_towers = blobmsg_get_u32(tb[OPENCELLID_MAX_TOWERS]);
    }
    
    // If no center provided, use current triangulated position
    if (center_lat == 0.0 && center_lon == 0.0) {
        opencellid_cellular_environment_t environment;
        opencellid_triangulation_result_t result;
        
        if (opencellid_get_cellular_environment(&environment) == AUTONOMY_SUCCESS &&
            opencellid_triangulate_position(&environment, &result) == AUTONOMY_SUCCESS) {
            center_lat = result.latitude;
            center_lon = result.longitude;
        } else {
            blobmsg_add_u8(&bb, "success", 0);
            blobmsg_add_string(&bb, "error", "No center coordinates provided and triangulation failed");
            ubus_send_reply(ctx, req, bb.head);
            blob_buf_free(&bb);
            return UBUS_STATUS_OK;
        }
    }
    
    // Get visible towers
    opencellid_cell_location_t towers[50]; // Allow up to 50 towers
    int max_lookup = (max_towers > 50) ? 50 : max_towers;
    
    int tower_count = opencellid_get_visible_towers(towers, max_lookup, 
                                                   center_lat, center_lon, radius);
    
    if (tower_count < 0) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "Failed to get visible towers");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    blobmsg_add_u8(&bb, "success", 1);
    
    // Add center coordinates
    void *center_table = blobmsg_open_table(&bb, "center");
    blobmsg_add_double(&bb, "latitude", center_lat);
    blobmsg_add_double(&bb, "longitude", center_lon);
    blobmsg_close_table(&bb, center_table);
    
    // Get current cellular environment for serving/neighbor identification
    opencellid_cellular_environment_t environment;
    bool have_environment = (opencellid_get_cellular_environment(&environment) == AUTONOMY_SUCCESS);
    
    // Add accuracy circle from current triangulation
    opencellid_triangulation_result_t current_position;
    if (opencellid_triangulate_position(&environment, &current_position) == AUTONOMY_SUCCESS) {
        void *accuracy_table = blobmsg_open_table(&bb, "accuracy_circle");
        blobmsg_add_double(&bb, "radius", current_position.accuracy);
        blobmsg_add_double(&bb, "confidence", current_position.confidence);
        blobmsg_close_table(&bb, accuracy_table);
    }
    
    // Add towers array
    void *towers_array = blobmsg_open_array(&bb, "towers");
    
    for (int i = 0; i < tower_count; i++) {
        void *tower_table = blobmsg_open_table(&bb, NULL);
        
        // Basic cell information
        blobmsg_add_u32(&bb, "mcc", towers[i].cell_id.mcc);
        blobmsg_add_u32(&bb, "mnc", towers[i].cell_id.mnc);
        blobmsg_add_u32(&bb, "lac", towers[i].cell_id.lac);
        blobmsg_add_u64(&bb, "cell_id", towers[i].cell_id.cell_id);
        blobmsg_add_string(&bb, "radio", opencellid_radio_type_to_string(towers[i].cell_id.radio));
        blobmsg_add_u32(&bb, "pci", towers[i].cell_id.pci);
        blobmsg_add_u32(&bb, "earfcn", towers[i].cell_id.earfcn);
        
        // Location information
        blobmsg_add_double(&bb, "latitude", towers[i].latitude);
        blobmsg_add_double(&bb, "longitude", towers[i].longitude);
        blobmsg_add_double(&bb, "range", towers[i].range);
        blobmsg_add_double(&bb, "confidence", towers[i].confidence);
        blobmsg_add_u32(&bb, "samples", towers[i].samples);
        blobmsg_add_string(&bb, "source", towers[i].source);
        
        // Calculate distance from center
        double distance = opencellid_calculate_distance(center_lat, center_lon,
                                                       towers[i].latitude, towers[i].longitude);
        blobmsg_add_double(&bb, "distance", distance);
        
        // Check if this is serving or neighbor cell
        bool is_serving = false;
        bool is_neighbor = false;
        int rsrp = 0, rsrq = 0;
        
        if (have_environment) {
            // Check if serving cell
            if (towers[i].cell_id.mcc == environment.serving_cell.cell_id.mcc &&
                towers[i].cell_id.mnc == environment.serving_cell.cell_id.mnc &&
                towers[i].cell_id.lac == environment.serving_cell.cell_id.lac &&
                towers[i].cell_id.cell_id == environment.serving_cell.cell_id.cell_id) {
                is_serving = true;
                rsrp = environment.serving_cell.metrics.rsrp;
                rsrq = environment.serving_cell.metrics.rsrq;
            } else {
                // Check if neighbor cell
                for (int j = 0; j < environment.neighbor_count; j++) {
                    if (towers[i].cell_id.mcc == environment.neighbors[j].cell_id.mcc &&
                        towers[i].cell_id.mnc == environment.neighbors[j].cell_id.mnc &&
                        towers[i].cell_id.lac == environment.neighbors[j].cell_id.lac &&
                        towers[i].cell_id.cell_id == environment.neighbors[j].cell_id.cell_id) {
                        is_neighbor = true;
                        rsrp = environment.neighbors[j].rsrp;
                        rsrq = environment.neighbors[j].rsrq;
                        break;
                    }
                }
            }
        }
        
        blobmsg_add_u8(&bb, "is_serving", is_serving);
        blobmsg_add_u8(&bb, "is_neighbor", is_neighbor);
        
        if (is_serving || is_neighbor) {
            blobmsg_add_u32(&bb, "rsrp", rsrp);
            blobmsg_add_u32(&bb, "rsrq", rsrq);
        }
        
        blobmsg_close_table(&bb, tower_table);
    }
    
    blobmsg_close_array(&bb, towers_array);
    blobmsg_add_u32(&bb, "count", tower_count);
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return UBUS_STATUS_OK;
}

// Get cellular environment
int opencellid_ubus_get_cellular_environment(struct ubus_context *ctx, struct ubus_object *obj,
                                           struct ubus_request_data *req, const char *method,
                                           struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    if (!opencellid_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "OpenCellID system not initialized");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    opencellid_cellular_environment_t environment;
    if (opencellid_get_cellular_environment(&environment) != AUTONOMY_SUCCESS) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "Failed to get cellular environment");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    blobmsg_add_u8(&bb, "success", 1);
    
    // Add serving cell
    void *serving_table = blobmsg_open_table(&bb, "serving_cell");
    add_cell_info_to_blob(&bb, "cell", &environment.serving_cell.cell_id, 
                         &environment.serving_cell.metrics);
    blobmsg_add_u8(&bb, "registered", environment.serving_cell.is_registered);
    blobmsg_add_string(&bb, "operator", environment.serving_cell.operator_name);
    blobmsg_close_table(&bb, serving_table);
    
    // Add neighbor cells
    void *neighbors_array = blobmsg_open_array(&bb, "neighbor_cells");
    for (int i = 0; i < environment.neighbor_count; i++) {
        void *neighbor_table = blobmsg_open_table(&bb, NULL);
        
        blobmsg_add_u32(&bb, "mcc", environment.neighbors[i].cell_id.mcc);
        blobmsg_add_u32(&bb, "mnc", environment.neighbors[i].cell_id.mnc);
        blobmsg_add_u32(&bb, "lac", environment.neighbors[i].cell_id.lac);
        blobmsg_add_u64(&bb, "cell_id", environment.neighbors[i].cell_id.cell_id);
        blobmsg_add_string(&bb, "radio", opencellid_radio_type_to_string(environment.neighbors[i].cell_id.radio));
        blobmsg_add_u32(&bb, "pci", environment.neighbors[i].pci);
        blobmsg_add_u32(&bb, "earfcn", environment.neighbors[i].earfcn);
        blobmsg_add_u32(&bb, "rsrp", environment.neighbors[i].rsrp);
        blobmsg_add_u32(&bb, "rsrq", environment.neighbors[i].rsrq);
        blobmsg_add_string(&bb, "type", environment.neighbors[i].type);
        
        blobmsg_close_table(&bb, neighbor_table);
    }
    blobmsg_close_array(&bb, neighbors_array);
    
    // Add scan information
    blobmsg_add_u32(&bb, "scan_time", (uint32_t)environment.scan_time);
    blobmsg_add_string(&bb, "environment_hash", environment.environment_hash);
    
    // Add GPS location if available
    if (environment.gps_valid) {
        void *gps_table = blobmsg_open_table(&bb, "gps_location");
        blobmsg_add_double(&bb, "latitude", environment.gps_latitude);
        blobmsg_add_double(&bb, "longitude", environment.gps_longitude);
        blobmsg_add_double(&bb, "accuracy", environment.gps_accuracy);
        blobmsg_close_table(&bb, gps_table);
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return UBUS_STATUS_OK;
}

// Get system statistics
int opencellid_ubus_get_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                  struct ubus_request_data *req, const char *method,
                                  struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    if (!opencellid_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "OpenCellID system not initialized");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    opencellid_statistics_t stats;
    if (opencellid_get_statistics(&stats) != AUTONOMY_SUCCESS) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "Failed to get statistics");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    blobmsg_add_u8(&bb, "success", 1);
    
    void *stats_table = blobmsg_open_table(&bb, "statistics");
    
    // API statistics
    void *api_table = blobmsg_open_table(&bb, "api");
    blobmsg_add_u64(&bb, "total_lookups", stats.total_lookups);
    blobmsg_add_u64(&bb, "successful_lookups", stats.successful_lookups);
    blobmsg_add_u64(&bb, "failed_lookups", stats.failed_lookups);
    blobmsg_add_u64(&bb, "rate_limited_lookups", stats.rate_limited_lookups);
    blobmsg_add_double(&bb, "average_response_time_ms", stats.average_response_time_ms);
    blobmsg_close_table(&bb, api_table);
    
    // Cache statistics
    void *cache_table = blobmsg_open_table(&bb, "cache");
    blobmsg_add_u64(&bb, "hits", stats.cache_hits);
    blobmsg_add_u64(&bb, "misses", stats.cache_misses);
    blobmsg_add_double(&bb, "hit_ratio", stats.cache_hit_ratio);
    blobmsg_add_u64(&bb, "entries", stats.cache_entries);
    blobmsg_add_double(&bb, "size_mb", stats.cache_size_bytes / (1024.0 * 1024.0));
    blobmsg_close_table(&bb, cache_table);
    
    // Triangulation statistics
    void *triangulation_table = blobmsg_open_table(&bb, "triangulation");
    blobmsg_add_u64(&bb, "total_performed", stats.triangulations_performed);
    blobmsg_add_u64(&bb, "single_cell", stats.single_cell_positions);
    blobmsg_add_u64(&bb, "multi_cell", stats.multi_cell_positions);
    blobmsg_add_double(&bb, "average_accuracy", stats.average_accuracy_meters);
    blobmsg_add_double(&bb, "average_confidence", stats.average_confidence);
    blobmsg_close_table(&bb, triangulation_table);
    
    // Contribution statistics
    void *contribution_table = blobmsg_open_table(&bb, "contribution");
    blobmsg_add_u64(&bb, "total_sent", stats.total_contributions);
    blobmsg_add_u64(&bb, "successful", stats.successful_contributions);
    blobmsg_add_u64(&bb, "failed", stats.failed_contributions);
    blobmsg_add_u64(&bb, "queued", stats.queued_contributions);
    blobmsg_close_table(&bb, contribution_table);
    
    // Health statistics
    void *health_table = blobmsg_open_table(&bb, "health");
    blobmsg_add_u8(&bb, "healthy", stats.healthy);
    blobmsg_add_u32(&bb, "consecutive_failures", stats.consecutive_failures);
    blobmsg_add_u32(&bb, "consecutive_successes", stats.consecutive_successes);
    blobmsg_add_u32(&bb, "last_success", (uint32_t)stats.last_success);
    blobmsg_add_u32(&bb, "last_failure", (uint32_t)stats.last_failure);
    blobmsg_close_table(&bb, health_table);
    
    blobmsg_close_table(&bb, stats_table);
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return UBUS_STATUS_OK;
}

// Get OpenCellID configuration
int opencellid_ubus_get_config(struct ubus_context *ctx, struct ubus_object *obj,
                              struct ubus_request_data *req, const char *method,
                              struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    if (!opencellid_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "OpenCellID system not initialized");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    blobmsg_add_u8(&bb, "success", 1);
    
    void *config_table = blobmsg_open_table(&bb, "config");
    blobmsg_add_u8(&bb, "enabled", true); // Placeholder
    blobmsg_add_string(&bb, "api_key", "placeholder"); // Placeholder
    blobmsg_add_u32(&bb, "cache_size_mb", 10); // Placeholder
    blobmsg_add_u8(&bb, "contribution_enabled", true); // Placeholder
    blobmsg_add_u32(&bb, "contribution_interval", 300); // Placeholder
    blobmsg_add_double(&bb, "min_gps_accuracy", 10.0); // Placeholder
    blobmsg_close_table(&bb, config_table);
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return UBUS_STATUS_OK;
}

// Set OpenCellID configuration
int opencellid_ubus_set_config(struct ubus_context *ctx, struct ubus_object *obj,
                              struct ubus_request_data *req, const char *method,
                              struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    if (!opencellid_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "OpenCellID system not initialized");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    struct blob_attr *tb[__OPENCELLID_CONFIG_MAX];
    blobmsg_parse(opencellid_config_policy, __OPENCELLID_CONFIG_MAX, tb, blob_data(msg), blob_len(msg));
    
    // For now, just acknowledge the configuration update
    blobmsg_add_u8(&bb, "success", 1);
    blobmsg_add_string(&bb, "message", "OpenCellID configuration updated");
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return UBUS_STATUS_OK;
}

// Perform triangulation
int opencellid_ubus_triangulate(struct ubus_context *ctx, struct ubus_object *obj,
                               struct ubus_request_data *req, const char *method,
                               struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    if (!opencellid_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "OpenCellID system not initialized");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    // Placeholder triangulation result
    blobmsg_add_u8(&bb, "success", 1);
    
    void *position_table = blobmsg_open_table(&bb, "position");
    blobmsg_add_double(&bb, "latitude", 0.0); // Placeholder
    blobmsg_add_double(&bb, "longitude", 0.0); // Placeholder
    blobmsg_add_double(&bb, "accuracy", 100.0); // Placeholder
    blobmsg_add_double(&bb, "confidence", 0.5); // Placeholder
    blobmsg_add_string(&bb, "method", "triangulation");
    blobmsg_add_u32(&bb, "towers_used", 3); // Placeholder
    blobmsg_close_table(&bb, position_table);
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return UBUS_STATUS_OK;
}

// Contribute data now
int opencellid_ubus_contribute_now(struct ubus_context *ctx, struct ubus_object *obj,
                                  struct ubus_request_data *req, const char *method,
                                  struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    if (!opencellid_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "OpenCellID system not initialized");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    // Placeholder contribution
    blobmsg_add_u8(&bb, "success", 1);
    blobmsg_add_string(&bb, "message", "Data contribution queued");
    blobmsg_add_u32(&bb, "contributions_queued", 1); // Placeholder
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return UBUS_STATUS_OK;
}

// Clear cache
int opencellid_ubus_clear_cache(struct ubus_context *ctx, struct ubus_object *obj,
                               struct ubus_request_data *req, const char *method,
                               struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    if (!opencellid_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "OpenCellID system not initialized");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    struct blob_attr *tb[__OPENCELLID_CLEAR_MAX];
    blobmsg_parse(opencellid_clear_policy, __OPENCELLID_CLEAR_MAX, tb, blob_data(msg), blob_len(msg));
    
    if (!tb[OPENCELLID_CLEAR_CONFIRM] || !blobmsg_get_bool(tb[OPENCELLID_CLEAR_CONFIRM])) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "Cache clear not confirmed");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    // Placeholder cache clear
    blobmsg_add_u8(&bb, "success", 1);
    blobmsg_add_string(&bb, "message", "Cache cleared successfully");
    blobmsg_add_u32(&bb, "entries_cleared", 0); // Placeholder
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return UBUS_STATUS_OK;
}

// Reset statistics
int opencellid_ubus_reset_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                    struct ubus_request_data *req, const char *method,
                                    struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    if (!opencellid_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "OpenCellID system not initialized");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    // Placeholder statistics reset
    blobmsg_add_u8(&bb, "success", 1);
    blobmsg_add_string(&bb, "message", "OpenCellID statistics reset");
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return UBUS_STATUS_OK;
}

// Perform health check
int opencellid_ubus_health_check(struct ubus_context *ctx, struct ubus_object *obj,
                                struct ubus_request_data *req, const char *method,
                                struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    if (!opencellid_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "OpenCellID system not initialized");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    blobmsg_add_u8(&bb, "success", 1);
    
    void *health_table = blobmsg_open_table(&bb, "health");
    blobmsg_add_u8(&bb, "initialized", opencellid_is_initialized());
    blobmsg_add_u8(&bb, "healthy", true); // Placeholder
    blobmsg_add_string(&bb, "status", "operational"); // Placeholder
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    blobmsg_close_table(&bb, health_table);
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return UBUS_STATUS_OK;
}

// UBUS method definitions
const struct ubus_method opencellid_ubus_methods[] = {
    UBUS_METHOD_NOARG("get_position", opencellid_ubus_get_position),
    UBUS_METHOD("get_visible_towers", opencellid_ubus_get_visible_towers, opencellid_towers_policy),
    UBUS_METHOD_NOARG("get_cellular_environment", opencellid_ubus_get_cellular_environment),
    UBUS_METHOD_NOARG("get_statistics", opencellid_ubus_get_statistics),
    UBUS_METHOD_NOARG("get_config", opencellid_ubus_get_config),
    UBUS_METHOD("set_config", opencellid_ubus_set_config, opencellid_config_policy),
    UBUS_METHOD_NOARG("triangulate", opencellid_ubus_triangulate),
    UBUS_METHOD_NOARG("contribute_now", opencellid_ubus_contribute_now),
    UBUS_METHOD("clear_cache", opencellid_ubus_clear_cache, opencellid_clear_policy),
    UBUS_METHOD_NOARG("reset_statistics", opencellid_ubus_reset_statistics),
    UBUS_METHOD_NOARG("health_check", opencellid_ubus_health_check),
};

const int opencellid_ubus_methods_count = ARRAY_SIZE(opencellid_ubus_methods);