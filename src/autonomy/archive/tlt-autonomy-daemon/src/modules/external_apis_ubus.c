#include "external_apis_ubus.h"
#include "external_apis.h"
#include "logx.h"
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <json-c/json.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// UBUS parameter policies
enum {
    EXTERNAL_API_LAT,
    EXTERNAL_API_LON,
    __EXTERNAL_API_COORDS_MAX
};

static const struct blobmsg_policy external_api_coords_policy[] = {
    [EXTERNAL_API_LAT] = { .name = "latitude", .type = BLOBMSG_TYPE_DOUBLE },
    [EXTERNAL_API_LON] = { .name = "longitude", .type = BLOBMSG_TYPE_DOUBLE },
};

enum {
    EXTERNAL_API_CONFIG_API,
    EXTERNAL_API_CONFIG_ENABLED,
    EXTERNAL_API_CONFIG_API_KEY,
    EXTERNAL_API_CONFIG_TIMEOUT,
    EXTERNAL_API_CONFIG_MAX_REQUESTS_HOUR,
    __EXTERNAL_API_CONFIG_MAX
};

static const struct blobmsg_policy external_api_config_policy[] = {
    [EXTERNAL_API_CONFIG_API] = { .name = "api", .type = BLOBMSG_TYPE_STRING },
    [EXTERNAL_API_CONFIG_ENABLED] = { .name = "enabled", .type = BLOBMSG_TYPE_BOOL },
    [EXTERNAL_API_CONFIG_API_KEY] = { .name = "api_key", .type = BLOBMSG_TYPE_STRING },
    [EXTERNAL_API_CONFIG_TIMEOUT] = { .name = "timeout_seconds", .type = BLOBMSG_TYPE_INT32 },
    [EXTERNAL_API_CONFIG_MAX_REQUESTS_HOUR] = { .name = "max_requests_per_hour", .type = BLOBMSG_TYPE_INT32 },
};

// Get elevation data for coordinates
int external_apis_ubus_get_elevation(struct ubus_context *ctx, struct ubus_object *obj,
                                    struct ubus_request_data *req, const char *method,
                                    struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    if (!external_apis_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "External APIs not initialized");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    struct blob_attr *tb[__EXTERNAL_API_COORDS_MAX];
    blobmsg_parse(external_api_coords_policy, __EXTERNAL_API_COORDS_MAX, tb, blob_data(msg), blob_len(msg));
    
    if (!tb[EXTERNAL_API_LAT] || !tb[EXTERNAL_API_LON]) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "Missing latitude or longitude parameters");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    double latitude = blobmsg_get_double(tb[EXTERNAL_API_LAT]);
    double longitude = blobmsg_get_double(tb[EXTERNAL_API_LON]);
    
    external_elevation_data_t elevation_data;
    if (external_apis_get_elevation(latitude, longitude, &elevation_data) == AUTONOMY_SUCCESS) {
        blobmsg_add_u8(&bb, "success", 1);
        
        void *elevation_table = blobmsg_open_table(&bb, "elevation");
        blobmsg_add_double(&bb, "elevation", elevation_data.elevation);
        blobmsg_add_double(&bb, "resolution", elevation_data.resolution);
        blobmsg_add_string(&bb, "source", elevation_data.source);
        blobmsg_add_u32(&bb, "timestamp", (uint32_t)elevation_data.timestamp);
        blobmsg_close_table(&bb, elevation_table);
    } else {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "Failed to get elevation data");
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return UBUS_STATUS_OK;
}

// Get weather data for coordinates
int external_apis_ubus_get_weather(struct ubus_context *ctx, struct ubus_object *obj,
                                  struct ubus_request_data *req, const char *method,
                                  struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    if (!external_apis_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "External APIs not initialized");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    struct blob_attr *tb[__EXTERNAL_API_COORDS_MAX];
    blobmsg_parse(external_api_coords_policy, __EXTERNAL_API_COORDS_MAX, tb, blob_data(msg), blob_len(msg));
    
    if (!tb[EXTERNAL_API_LAT] || !tb[EXTERNAL_API_LON]) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "Missing latitude or longitude parameters");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    double latitude = blobmsg_get_double(tb[EXTERNAL_API_LAT]);
    double longitude = blobmsg_get_double(tb[EXTERNAL_API_LON]);
    
    external_weather_data_t weather_data;
    if (external_apis_get_weather(latitude, longitude, &weather_data) == AUTONOMY_SUCCESS) {
        blobmsg_add_u8(&bb, "success", 1);
        
        void *weather_table = blobmsg_open_table(&bb, "weather");
        blobmsg_add_double(&bb, "temperature_celsius", weather_data.temperature_celsius);
        blobmsg_add_double(&bb, "humidity_percent", weather_data.humidity_percent);
        blobmsg_add_double(&bb, "pressure_hpa", weather_data.pressure_hpa);
        blobmsg_add_double(&bb, "wind_speed_ms", weather_data.wind_speed_ms);
        blobmsg_add_double(&bb, "wind_direction_deg", weather_data.wind_direction_deg);
        blobmsg_add_double(&bb, "visibility_km", weather_data.visibility_km);
        blobmsg_add_string(&bb, "description", weather_data.description);
        blobmsg_add_string(&bb, "icon", weather_data.icon);
        blobmsg_add_string(&bb, "source", weather_data.source);
        blobmsg_add_u32(&bb, "timestamp", (uint32_t)weather_data.timestamp);
        blobmsg_close_table(&bb, weather_table);
    } else {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "Failed to get weather data");
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return UBUS_STATUS_OK;
}

// Get reverse geocoding for coordinates
int external_apis_ubus_get_reverse_geocoding(struct ubus_context *ctx, struct ubus_object *obj,
                                           struct ubus_request_data *req, const char *method,
                                           struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    if (!external_apis_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "External APIs not initialized");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    struct blob_attr *tb[__EXTERNAL_API_COORDS_MAX];
    blobmsg_parse(external_api_coords_policy, __EXTERNAL_API_COORDS_MAX, tb, blob_data(msg), blob_len(msg));
    
    if (!tb[EXTERNAL_API_LAT] || !tb[EXTERNAL_API_LON]) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "Missing latitude or longitude parameters");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    double latitude = blobmsg_get_double(tb[EXTERNAL_API_LAT]);
    double longitude = blobmsg_get_double(tb[EXTERNAL_API_LON]);
    
    external_location_data_t location_data;
    if (external_apis_get_reverse_geocoding(latitude, longitude, &location_data) == AUTONOMY_SUCCESS) {
        blobmsg_add_u8(&bb, "success", 1);
        
        void *location_table = blobmsg_open_table(&bb, "location");
        blobmsg_add_double(&bb, "latitude", location_data.latitude);
        blobmsg_add_double(&bb, "longitude", location_data.longitude);
        blobmsg_add_double(&bb, "accuracy", location_data.accuracy);
        blobmsg_add_double(&bb, "confidence", location_data.confidence);
        blobmsg_add_string(&bb, "formatted_address", location_data.formatted_address);
        blobmsg_add_string(&bb, "country", location_data.country);
        blobmsg_add_string(&bb, "region", location_data.region);
        blobmsg_add_string(&bb, "city", location_data.city);
        blobmsg_add_string(&bb, "postal_code", location_data.postal_code);
        blobmsg_add_string(&bb, "source", location_data.source);
        blobmsg_add_u32(&bb, "timestamp", (uint32_t)location_data.timestamp);
        blobmsg_close_table(&bb, location_table);
    } else {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "Failed to get reverse geocoding data");
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return UBUS_STATUS_OK;
}

// Get external API statistics
int external_apis_ubus_get_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                     struct ubus_request_data *req, const char *method,
                                     struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    if (!external_apis_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "External APIs not initialized");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    blobmsg_add_u8(&bb, "success", 1);
    
    external_api_statistics_t stats_array[EXTERNAL_API_MAX];
    int api_count = external_apis_get_all_statistics(stats_array, EXTERNAL_API_MAX);
    
    if (api_count > 0) {
        void *apis_array = blobmsg_open_array(&bb, "apis");
        
        for (int i = 0; i < api_count; i++) {
            void *api_table = blobmsg_open_table(&bb, NULL);
            
            blobmsg_add_string(&bb, "api", external_api_type_to_string((external_api_type_t)i));
            blobmsg_add_string(&bb, "status", external_api_status_to_string(stats_array[i].status));
            blobmsg_add_u64(&bb, "total_requests", stats_array[i].total_requests);
            blobmsg_add_u64(&bb, "successful_requests", stats_array[i].successful_requests);
            blobmsg_add_u64(&bb, "failed_requests", stats_array[i].failed_requests);
            blobmsg_add_double(&bb, "success_rate", stats_array[i].success_rate);
            blobmsg_add_double(&bb, "avg_response_time_ms", stats_array[i].average_response_time_ms);
            blobmsg_add_u32(&bb, "requests_this_hour", stats_array[i].requests_this_hour);
            blobmsg_add_u32(&bb, "requests_this_day", stats_array[i].requests_this_day);
            blobmsg_add_double(&bb, "cost_this_month", stats_array[i].cost_this_month);
            blobmsg_add_u32(&bb, "consecutive_failures", stats_array[i].consecutive_failures);
            blobmsg_add_u32(&bb, "last_success", (uint32_t)stats_array[i].last_success);
            
            blobmsg_close_table(&bb, api_table);
        }
        
        blobmsg_close_array(&bb, apis_array);
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return UBUS_STATUS_OK;
}

// Perform health check for all external APIs
int external_apis_ubus_health_check(struct ubus_context *ctx, struct ubus_object *obj,
                                   struct ubus_request_data *req, const char *method,
                                   struct blob_attr *msg) {
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    if (!external_apis_is_initialized()) {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "External APIs not initialized");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return UBUS_STATUS_OK;
    }
    
    // Perform health check
    if (external_apis_health_check() == AUTONOMY_SUCCESS) {
        blobmsg_add_u8(&bb, "success", 1);
        
        // Count API statuses
        int healthy = 0, degraded = 0, failed = 0, disabled = 0;
        
        void *health_table = blobmsg_open_table(&bb, "health_check");
        void *api_health_array = blobmsg_open_array(&bb, "api_health");
        
        for (int i = 0; i < EXTERNAL_API_MAX; i++) {
            api_status_t status = external_apis_get_status((external_api_type_t)i);
            
            switch (status) {
                case API_STATUS_HEALTHY: healthy++; break;
                case API_STATUS_DEGRADED: degraded++; break;
                case API_STATUS_FAILED: failed++; break;
                case API_STATUS_DISABLED: disabled++; break;
                default: break;
            }
            
            void *api_table = blobmsg_open_table(&bb, NULL);
            blobmsg_add_string(&bb, "api", external_api_type_to_string((external_api_type_t)i));
            blobmsg_add_string(&bb, "status", external_api_status_to_string(status));
            
            external_api_statistics_t stats;
            if (external_apis_get_statistics((external_api_type_t)i, &stats) == AUTONOMY_SUCCESS) {
                blobmsg_add_double(&bb, "success_rate", stats.success_rate);
                blobmsg_add_double(&bb, "avg_response_time_ms", stats.average_response_time_ms);
                blobmsg_add_u32(&bb, "consecutive_failures", stats.consecutive_failures);
            }
            
            blobmsg_close_table(&bb, api_table);
        }
        
        blobmsg_close_array(&bb, api_health_array);
        
        // Overall health assessment
        const char* overall_health = "excellent";
        if (failed > 0) overall_health = "poor";
        else if (degraded > 1) overall_health = "fair";
        else if (degraded > 0) overall_health = "good";
        
        blobmsg_add_string(&bb, "overall_health", overall_health);
        blobmsg_add_u32(&bb, "apis_healthy", healthy);
        blobmsg_add_u32(&bb, "apis_degraded", degraded);
        blobmsg_add_u32(&bb, "apis_failed", failed);
        blobmsg_add_u32(&bb, "apis_disabled", disabled);
        
        blobmsg_close_table(&bb, health_table);
    } else {
        blobmsg_add_u8(&bb, "success", 0);
        blobmsg_add_string(&bb, "error", "Health check failed");
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    return UBUS_STATUS_OK;
}

// Additional UBUS method implementations would continue here...

// UBUS method definitions
const struct ubus_method external_apis_ubus_methods[] = {
    UBUS_METHOD("get_elevation", external_apis_ubus_get_elevation, external_api_coords_policy),
    UBUS_METHOD("get_weather", external_apis_ubus_get_weather, external_api_coords_policy),
    UBUS_METHOD("get_reverse_geocoding", external_apis_ubus_get_reverse_geocoding, external_api_coords_policy),
    UBUS_METHOD_NOARG("get_statistics", external_apis_ubus_get_statistics),
    UBUS_METHOD_NOARG("get_config", external_apis_ubus_get_config),
    UBUS_METHOD("configure", external_apis_ubus_configure, external_api_config_policy),
    UBUS_METHOD_NOARG("health_check", external_apis_ubus_health_check),
    UBUS_METHOD("reset_statistics", external_apis_ubus_reset_statistics, external_api_config_policy),
    UBUS_METHOD("test_connection", external_apis_ubus_test_connection, external_api_config_policy),
};

const int external_apis_ubus_methods_count = ARRAY_SIZE(external_apis_ubus_methods);