#include "starlink_types.h"
#include "starlink_modules.h"
#include "starlink_obstruction.h"
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <fcntl.h>
#include <sys/socket.h>

// Starlink UBUS method handlers
int autonomy_starlink_status(struct ubus_context *uctx, struct ubus_object *obj,
                            struct ubus_request_data *req, const char *method,
                            struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Get Starlink status
    starlink_collection_result_t result;
    if (starlink_collect_data(&result) == 0 && result.success) {
        // Add basic status
        blobmsg_add_string(&bb, "status", "connected");
        blobmsg_add_u32(&bb, "timestamp", (uint32_t)result.collection_time);
        
        // Add device info
        void *device_info = blobmsg_open_table(&bb, "device_info");
        blobmsg_add_string(&bb, "id", result.status.device_info.id);
        blobmsg_add_string(&bb, "hardware_version", result.status.device_info.hardware_version);
        blobmsg_add_string(&bb, "software_version", result.status.device_info.software_version);
        blobmsg_add_string(&bb, "country_code", result.status.device_info.country_code);
        blobmsg_add_u32(&bb, "generation_number", result.status.device_info.generation_number);
        blobmsg_add_u32(&bb, "uptime_s", (uint32_t)result.status.device_state.uptime_s);
        blobmsg_close_table(&bb, device_info);
        
        // Add GPS info
        void *gps_info = blobmsg_open_table(&bb, "gps");
        blobmsg_add_u8(&bb, "valid", result.status.gps_stats.gps_valid);
        blobmsg_add_u32(&bb, "satellites", result.status.gps_stats.gps_sats);
        blobmsg_add_double(&bb, "lat", result.status.device_info.lat);
        blobmsg_add_double(&bb, "lon", result.status.device_info.lon);
        blobmsg_close_table(&bb, gps_info);
        
        // Add network performance
        void *network = blobmsg_open_table(&bb, "network");
        blobmsg_add_double(&bb, "ping_latency_ms", result.status.network_perf.pop_ping_latency_ms);
        blobmsg_add_double(&bb, "downlink_mbps", result.status.network_perf.downlink_throughput_bps / 1000000.0);
        blobmsg_add_double(&bb, "uplink_mbps", result.status.network_perf.uplink_throughput_bps / 1000000.0);
        blobmsg_add_double(&bb, "drop_rate", result.status.network_perf.pop_ping_drop_rate);
        blobmsg_close_table(&bb, network);
        
        // Add signal quality
        void *signal = blobmsg_open_table(&bb, "signal");
        blobmsg_add_double(&bb, "snr_db", result.status.signal_quality.snr_db);
        blobmsg_add_u8(&bb, "above_noise_floor", result.status.signal_quality.is_snr_above_noise_floor);
        blobmsg_add_u8(&bb, "persistently_low", result.status.signal_quality.is_snr_persistently_low);
        blobmsg_close_table(&bb, signal);
        
        // Add positioning
        void *positioning = blobmsg_open_table(&bb, "positioning");
        blobmsg_add_double(&bb, "azimuth_deg", result.status.positioning.boresight_azimuth_deg);
        blobmsg_add_double(&bb, "elevation_deg", result.status.positioning.boresight_elevation_deg);
        blobmsg_close_table(&bb, positioning);
        
        // Add health status
        void *health = blobmsg_open_table(&bb, "health");
        blobmsg_add_u32(&bb, "overall_score", result.health.overall_score);
        blobmsg_add_string(&bb, "status", result.health.status);
        blobmsg_add_u8(&bb, "is_healthy", result.health.is_healthy);
        blobmsg_close_table(&bb, health);
        
    } else {
        // Error status
        blobmsg_add_string(&bb, "status", "disconnected");
        blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
        blobmsg_add_string(&bb, "error", result.error_message);
    }
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_starlink_health(struct ubus_context *uctx, struct ubus_object *obj,
                             struct ubus_request_data *req, const char *method,
                             struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Get Starlink health
    starlink_health_t health;
    if (starlink_get_health(&health) == 0) {
        blobmsg_add_string(&bb, "result", "health_check_completed");
        blobmsg_add_u32(&bb, "overall_score", health.overall_score);
        blobmsg_add_string(&bb, "status", health.status);
        blobmsg_add_u8(&bb, "is_healthy", health.is_healthy);
        blobmsg_add_u32(&bb, "last_check", health.last_check);
        blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
        
        if (strlen(health.error_message) > 0) {
            blobmsg_add_string(&bb, "error_message", health.error_message);
        }
    } else {
        blobmsg_add_string(&bb, "result", "health_check_failed");
        blobmsg_add_string(&bb, "error", "Unable to retrieve Starlink health");
        blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    }
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_starlink_location(struct ubus_context *uctx, struct ubus_object *obj,
                              struct ubus_request_data *req, const char *method,
                              struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Get Starlink location
    starlink_lla_position_t location;
    if (starlink_get_location(&location) == 0) {
        blobmsg_add_string(&bb, "result", "location_retrieved");
        blobmsg_add_double(&bb, "latitude", location.lat);
        blobmsg_add_double(&bb, "longitude", location.lon);
        blobmsg_add_double(&bb, "altitude", location.alt);
        blobmsg_add_string(&bb, "source", "starlink");
        blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    } else {
        blobmsg_add_string(&bb, "result", "location_failed");
        blobmsg_add_string(&bb, "error", "Unable to retrieve Starlink location");
        blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    }
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_starlink_collector_stats(struct ubus_context *uctx, struct ubus_object *obj,
                                     struct ubus_request_data *req, const char *method,
                                     struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Get collector statistics
    int cache_hits, cache_misses, errors, successes;
    starlink_get_collector_stats(&cache_hits, &cache_misses, &errors, &successes);
    
    blobmsg_add_string(&bb, "result", "stats_retrieved");
    
    void *stats = blobmsg_open_table(&bb, "statistics");
    blobmsg_add_u32(&bb, "cache_hits", cache_hits);
    blobmsg_add_u32(&bb, "cache_misses", cache_misses);
    blobmsg_add_u32(&bb, "errors", errors);
    blobmsg_add_u32(&bb, "successes", successes);
    
    // Calculate cache hit rate
    int total_requests = cache_hits + cache_misses;
    float hit_rate = (total_requests > 0) ? ((float)cache_hits / total_requests) * 100.0 : 0.0;
    blobmsg_add_double(&bb, "cache_hit_rate_percent", hit_rate);
    
    // Calculate success rate
    int total_attempts = successes + errors;
    float success_rate = (total_attempts > 0) ? ((float)successes / total_attempts) * 100.0 : 0.0;
    blobmsg_add_double(&bb, "success_rate_percent", success_rate);
    blobmsg_close_table(&bb, stats);
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_starlink_force_collect(struct ubus_context *uctx, struct ubus_object *obj,
                                   struct ubus_request_data *req, const char *method,
                                   struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Force immediate collection
    starlink_collection_result_t result;
    if (starlink_force_collect(&result) == 0 && result.success) {
        blobmsg_add_string(&bb, "result", "forced_collection_completed");
        blobmsg_add_u32(&bb, "collection_time", (uint32_t)result.collection_time);
        blobmsg_add_u32(&bb, "health_score", result.health.overall_score);
        blobmsg_add_string(&bb, "health_status", result.health.status);
        blobmsg_add_u8(&bb, "is_healthy", result.health.is_healthy);
    } else {
        blobmsg_add_string(&bb, "result", "forced_collection_failed");
        blobmsg_add_string(&bb, "error", result.error_message);
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

// Starlink obstruction analysis UBUS methods

int autonomy_starlink_obstruction_status(struct ubus_context *uctx, struct ubus_object *obj,
                                        struct ubus_request_data *req, const char *method,
                                        struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Get obstruction analysis status
    starlink_obstruction_status_t status;
    if (starlink_obstruction_get_status(&status) == 0) {
        blobmsg_add_string(&bb, "result", "status_retrieved");
        blobmsg_add_u8(&bb, "enabled", status.enabled);
        blobmsg_add_u32(&bb, "pattern_count", status.pattern_count);
        blobmsg_add_u32(&bb, "max_patterns", status.max_patterns);
        blobmsg_add_u32(&bb, "active_match_count", status.active_match_count);
        blobmsg_add_u32(&bb, "max_active_matches", status.max_active_matches);
        blobmsg_add_u32(&bb, "total_observations", status.total_observations);
        blobmsg_add_u32(&bb, "last_analysis", (uint32_t)status.last_analysis);
    } else {
        blobmsg_add_string(&bb, "result", "status_failed");
        blobmsg_add_string(&bb, "error", "Unable to retrieve obstruction status");
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_starlink_obstruction_patterns(struct ubus_context *uctx, struct ubus_object *obj,
                                          struct ubus_request_data *req, const char *method,
                                          struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Get environmental patterns
    starlink_environmental_pattern_t patterns[100];
    int pattern_count = starlink_obstruction_get_patterns(patterns, 100);
    
    if (pattern_count >= 0) {
        blobmsg_add_string(&bb, "result", "patterns_retrieved");
        blobmsg_add_u32(&bb, "pattern_count", pattern_count);
        
        void *patterns_array = blobmsg_open_array(&bb, "patterns");
        for (int i = 0; i < pattern_count; i++) {
            void *pattern = blobmsg_open_table(&bb, NULL);
            blobmsg_add_string(&bb, "id", patterns[i].id);
            blobmsg_add_string(&bb, "name", patterns[i].name);
            blobmsg_add_string(&bb, "description", patterns[i].description);
            blobmsg_add_double(&bb, "latitude", patterns[i].latitude);
            blobmsg_add_double(&bb, "longitude", patterns[i].longitude);
            blobmsg_add_double(&bb, "confidence", patterns[i].confidence);
            blobmsg_add_u32(&bb, "sample_count", patterns[i].sample_count);
            blobmsg_add_u32(&bb, "first_seen", (uint32_t)patterns[i].first_seen);
            blobmsg_add_u32(&bb, "last_seen", (uint32_t)patterns[i].last_seen);
            blobmsg_close_table(&bb, pattern);
        }
        blobmsg_close_array(&bb, patterns_array);
    } else {
        blobmsg_add_string(&bb, "result", "patterns_failed");
        blobmsg_add_string(&bb, "error", "Unable to retrieve patterns");
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_starlink_obstruction_matches(struct ubus_context *uctx, struct ubus_object *obj,
                                         struct ubus_request_data *req, const char *method,
                                         struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Get active matches
    starlink_active_match_t matches[10];
    int match_count = starlink_obstruction_get_active_matches(matches, 10);
    
    if (match_count >= 0) {
        blobmsg_add_string(&bb, "result", "matches_retrieved");
        blobmsg_add_u32(&bb, "match_count", match_count);
        
        void *matches_array = blobmsg_open_array(&bb, "active_matches");
        for (int i = 0; i < match_count; i++) {
            void *match = blobmsg_open_table(&bb, NULL);
            blobmsg_add_string(&bb, "pattern_id", matches[i].pattern_id);
            blobmsg_add_string(&bb, "pattern_name", matches[i].pattern_name);
            blobmsg_add_u32(&bb, "start_time", (uint32_t)matches[i].start_time);
            blobmsg_add_u32(&bb, "last_update", (uint32_t)matches[i].last_update);
            blobmsg_add_double(&bb, "similarity", matches[i].similarity);
            blobmsg_add_double(&bb, "confidence", matches[i].confidence);
            blobmsg_add_u32(&bb, "sample_count", matches[i].sample_count);
            blobmsg_close_table(&bb, match);
        }
        blobmsg_close_array(&bb, matches_array);
    } else {
        blobmsg_add_string(&bb, "result", "matches_failed");
        blobmsg_add_string(&bb, "error", "Unable to retrieve matches");
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_starlink_obstruction_config(struct ubus_context *uctx, struct ubus_object *obj,
                                        struct ubus_request_data *req, const char *method,
                                        struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Get obstruction configuration
    starlink_obstruction_config_t config;
    if (starlink_obstruction_get_config(&config) == 0) {
        blobmsg_add_string(&bb, "result", "config_retrieved");
        blobmsg_add_u8(&bb, "enabled", config.enabled);
        blobmsg_add_u32(&bb, "max_patterns", config.max_patterns);
        blobmsg_add_u32(&bb, "min_observations_to_learn", config.min_observations_to_learn);
        blobmsg_add_double(&bb, "pattern_similarity_threshold", config.pattern_similarity_threshold);
        blobmsg_add_double(&bb, "location_radius_meters", config.location_radius_meters);
        blobmsg_add_u32(&bb, "max_active_matches", config.max_active_matches);
        blobmsg_add_u32(&bb, "match_timeout_minutes", config.match_timeout_minutes);
        blobmsg_add_u32(&bb, "history_size", config.history_size);
    } else {
        blobmsg_add_string(&bb, "result", "config_failed");
        blobmsg_add_string(&bb, "error", "Unable to retrieve configuration");
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_starlink_obstruction_reset(struct ubus_context *uctx, struct ubus_object *obj,
                                       struct ubus_request_data *req, const char *method,
                                       struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Reset obstruction analysis
    if (starlink_obstruction_reset() == 0) {
        blobmsg_add_string(&bb, "result", "reset_completed");
        blobmsg_add_string(&bb, "message", "Obstruction analysis has been reset");
    } else {
        blobmsg_add_string(&bb, "result", "reset_failed");
        blobmsg_add_string(&bb, "error", "Unable to reset obstruction analysis");
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}
