#include "starlink_weather_snow_melt_control_ubus.h"
#include "starlink_weather_snow_melt_control.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

// UBUS method policies
static const struct blobmsg_policy starlink_weather_snow_melt_ubus_policy[__STARLINK_WEATHER_SNOW_MELT_UBUS_POLICY_MAX] = {
    [STARLINK_WEATHER_SNOW_MELT_UBUS_POLICY_GET_STATUS] = { .name = "get_status", .type = BLOBMSG_TYPE_BOOL },
    [STARLINK_WEATHER_SNOW_MELT_UBUS_POLICY_GET_CONFIG] = { .name = "get_config", .type = BLOBMSG_TYPE_BOOL },
    [STARLINK_WEATHER_SNOW_MELT_UBUS_POLICY_SET_CONFIG] = { .name = "set_config", .type = BLOBMSG_TYPE_TABLE },
    [STARLINK_WEATHER_SNOW_MELT_UBUS_POLICY_SET_ENABLED] = { .name = "enabled", .type = BLOBMSG_TYPE_BOOL },
    [STARLINK_WEATHER_SNOW_MELT_UBUS_POLICY_SET_MODE] = { .name = "mode", .type = BLOBMSG_TYPE_STRING },
    [STARLINK_WEATHER_SNOW_MELT_UBUS_POLICY_FORCE_UPDATE] = { .name = "force_update", .type = BLOBMSG_TYPE_BOOL },
    [STARLINK_WEATHER_SNOW_MELT_UBUS_POLICY_GET_STATISTICS] = { .name = "get_statistics", .type = BLOBMSG_TYPE_BOOL },
    [STARLINK_WEATHER_SNOW_MELT_UBUS_POLICY_RESET_STATISTICS] = { .name = "reset_statistics", .type = BLOBMSG_TYPE_BOOL },
};

// UBUS method arguments
static const struct blobmsg_policy starlink_weather_snow_melt_ubus_args[__STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_MAX] = {
    [STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_ENABLED] = { .name = "enabled", .type = BLOBMSG_TYPE_BOOL },
    [STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_MODE] = { .name = "mode", .type = BLOBMSG_TYPE_STRING },
    [STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_TEMPERATURE_THRESHOLD] = { .name = "temperature_threshold", .type = BLOBMSG_TYPE_DOUBLE },
    [STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_WEATHER_CHECK_INTERVAL] = { .name = "weather_check_interval", .type = BLOBMSG_TYPE_INT32 },
    [STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_PREHEAT_DURATION] = { .name = "preheat_duration", .type = BLOBMSG_TYPE_INT32 },
    [STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_USE_FORECAST] = { .name = "use_forecast", .type = BLOBMSG_TYPE_BOOL },
    [STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_FORECAST_HOURS_AHEAD] = { .name = "forecast_hours_ahead", .type = BLOBMSG_TYPE_INT32 },
    [STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_WEATHER_API_KEY] = { .name = "weather_api_key", .type = BLOBMSG_TYPE_STRING },
    [STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_STARLINK_HOST] = { .name = "starlink_host", .type = BLOBMSG_TYPE_STRING },
    [STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_STARLINK_PORT] = { .name = "starlink_port", .type = BLOBMSG_TYPE_INT32 },
    [STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_DEBUG_MODE] = { .name = "debug_mode", .type = BLOBMSG_TYPE_BOOL },
};

// UBUS response fields
static const struct blobmsg_policy starlink_weather_snow_melt_ubus_response[__STARLINK_WEATHER_SNOW_MELT_UBUS_RESPONSE_MAX] = {
    [STARLINK_WEATHER_SNOW_MELT_UBUS_RESPONSE_SUCCESS] = { .name = "success", .type = BLOBMSG_TYPE_BOOL },
    [STARLINK_WEATHER_SNOW_MELT_UBUS_RESPONSE_ERROR] = { .name = "error", .type = BLOBMSG_TYPE_INT32 },
    [STARLINK_WEATHER_SNOW_MELT_UBUS_RESPONSE_MESSAGE] = { .name = "message", .type = BLOBMSG_TYPE_STRING },
    [STARLINK_WEATHER_SNOW_MELT_UBUS_RESPONSE_STATUS] = { .name = "status", .type = BLOBMSG_TYPE_TABLE },
    [STARLINK_WEATHER_SNOW_MELT_UBUS_RESPONSE_CONFIG] = { .name = "config", .type = BLOBMSG_TYPE_TABLE },
    [STARLINK_WEATHER_SNOW_MELT_UBUS_RESPONSE_STATISTICS] = { .name = "statistics", .type = BLOBMSG_TYPE_TABLE },
};

// UBUS methods
static const struct ubus_method starlink_weather_snow_melt_ubus_methods[] = {
    UBUS_METHOD(STARLINK_WEATHER_SNOW_MELT_UBUS_GET_STATUS, starlink_weather_snow_melt_ubus_get_status, starlink_weather_snow_melt_ubus_policy),
    UBUS_METHOD(STARLINK_WEATHER_SNOW_MELT_UBUS_GET_CONFIG, starlink_weather_snow_melt_ubus_get_config, starlink_weather_snow_melt_ubus_policy),
    UBUS_METHOD(STARLINK_WEATHER_SNOW_MELT_UBUS_SET_CONFIG, starlink_weather_snow_melt_ubus_set_config, starlink_weather_snow_melt_ubus_args),
    UBUS_METHOD(STARLINK_WEATHER_SNOW_MELT_UBUS_SET_ENABLED, starlink_weather_snow_melt_ubus_set_enabled, starlink_weather_snow_melt_ubus_args),
    UBUS_METHOD(STARLINK_WEATHER_SNOW_MELT_UBUS_SET_MODE, starlink_weather_snow_melt_ubus_set_mode, starlink_weather_snow_melt_ubus_args),
    UBUS_METHOD(STARLINK_WEATHER_SNOW_MELT_UBUS_FORCE_UPDATE, starlink_weather_snow_melt_ubus_force_update, starlink_weather_snow_melt_ubus_policy),
    UBUS_METHOD(STARLINK_WEATHER_SNOW_MELT_UBUS_GET_STATISTICS, starlink_weather_snow_melt_ubus_get_statistics, starlink_weather_snow_melt_ubus_policy),
    UBUS_METHOD(STARLINK_WEATHER_SNOW_MELT_UBUS_RESET_STATISTICS, starlink_weather_snow_melt_ubus_reset_statistics, starlink_weather_snow_melt_ubus_policy),
};

// UBUS object type
static struct ubus_object_type starlink_weather_snow_melt_ubus_object_type = 
    UBUS_OBJECT_TYPE(STARLINK_WEATHER_SNOW_MELT_UBUS_OBJECT, starlink_weather_snow_melt_ubus_methods);

// UBUS object
static struct ubus_object starlink_weather_snow_melt_ubus_object = {
    .name = STARLINK_WEATHER_SNOW_MELT_UBUS_OBJECT,
    .type = &starlink_weather_snow_melt_ubus_object_type,
    .methods = starlink_weather_snow_melt_ubus_methods,
    .n_methods = ARRAY_SIZE(starlink_weather_snow_melt_ubus_methods),
};

// Initialize UBUS interface for weather-based snow melt control
int starlink_weather_snow_melt_ubus_init(void) {
    printf("INFO: Initializing UBUS interface for weather-based snow melt control\n");
    
    // Get UBUS context
    extern struct ubus_context *g_ubus_ctx;
    if (!g_ubus_ctx) {
        printf("ERROR: UBUS context not available\n");
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    // Add object to UBUS
    int result = ubus_add_object(g_ubus_ctx, &starlink_weather_snow_melt_ubus_object);
    if (result != 0) {
        printf("ERROR: Failed to add snow melt control object to UBUS: %d\n", result);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    printf("INFO: UBUS interface for weather-based snow melt control initialized successfully\n");
    
    return AUTONOMY_SUCCESS;
}

// Cleanup UBUS interface for weather-based snow melt control
void starlink_weather_snow_melt_ubus_cleanup(void) {
    printf("INFO: Cleaning up UBUS interface for weather-based snow melt control\n");
    
    // Get UBUS context
    extern struct ubus_context *g_ubus_ctx;
    if (g_ubus_ctx) {
        ubus_remove_object(g_ubus_ctx, &starlink_weather_snow_melt_ubus_object);
    }
    
    printf("INFO: UBUS interface for weather-based snow melt control cleaned up\n");
}

// Helper function to convert snow melt mode string to enum
static snow_melt_mode_t string_to_snow_melt_mode(const char *mode_str) {
    if (!mode_str) return SNOW_MELT_OFF;
    
    if (strcmp(mode_str, "off") == 0 || strcmp(mode_str, "OFF") == 0) {
        return SNOW_MELT_OFF;
    } else if (strcmp(mode_str, "automatic") == 0 || strcmp(mode_str, "AUTOMATIC") == 0) {
        return SNOW_MELT_AUTOMATIC;
    } else if (strcmp(mode_str, "preheat") == 0 || strcmp(mode_str, "PREHEAT") == 0) {
        return SNOW_MELT_PREHEAT;
    } else if (strcmp(mode_str, "manual") == 0 || strcmp(mode_str, "MANUAL") == 0) {
        return SNOW_MELT_MANUAL;
    }
    
    return SNOW_MELT_OFF;
}

// Helper function to add status to blob
static void add_status_to_blob(struct blob_buf *bb, const starlink_weather_snow_melt_status_t *status) {
    void *status_table = blobmsg_open_table(bb, "status");
    
    blobmsg_add_u8(bb, "enabled", status->enabled);
    blobmsg_add_string(bb, "current_mode", starlink_weather_snow_melt_mode_to_string(status->current_mode));
    blobmsg_add_string(bb, "previous_mode", starlink_weather_snow_melt_mode_to_string(status->previous_mode));
    blobmsg_add_u32(bb, "last_mode_change", status->last_mode_change);
    blobmsg_add_u32(bb, "last_weather_check", status->last_weather_check);
    blobmsg_add_u32(bb, "preheat_start_time", status->preheat_start_time);
    blobmsg_add_u32(bb, "preheat_remaining_minutes", status->preheat_remaining_minutes);
    blobmsg_add_string(bb, "current_weather", starlink_weather_snow_melt_weather_condition_to_string(status->current_weather));
    blobmsg_add_string(bb, "forecast_weather", starlink_weather_snow_melt_weather_condition_to_string(status->forecast_weather));
    blobmsg_add_double(bb, "current_temperature", status->current_temperature);
    blobmsg_add_double(bb, "forecast_temperature", status->forecast_temperature);
    blobmsg_add_u8(bb, "precipitation_expected", status->precipitation_expected);
    blobmsg_add_u32(bb, "precipitation_probability", status->precipitation_probability);
    blobmsg_add_string(bb, "last_weather_description", status->last_weather_description);
    blobmsg_add_u32(bb, "last_weather_update", status->last_weather_update);
    
    blobmsg_close_table(bb, status_table);
}

// Helper function to add config to blob
static void add_config_to_blob(struct blob_buf *bb, const starlink_weather_snow_melt_config_t *config) {
    void *config_table = blobmsg_open_table(bb, "config");
    
    blobmsg_add_u8(bb, "enabled", config->enabled);
    blobmsg_add_double(bb, "temperature_threshold_celsius", config->temperature_threshold_celsius);
    blobmsg_add_u32(bb, "weather_check_interval_minutes", config->weather_check_interval_minutes);
    blobmsg_add_u32(bb, "preheat_duration_minutes", config->preheat_duration_minutes);
    blobmsg_add_u8(bb, "use_forecast", config->use_forecast);
    blobmsg_add_u32(bb, "forecast_hours_ahead", config->forecast_hours_ahead);
    blobmsg_add_string(bb, "weather_api_key", config->weather_api_key);
    blobmsg_add_string(bb, "starlink_host", config->starlink_host);
    blobmsg_add_u32(bb, "starlink_port", config->starlink_port);
    blobmsg_add_u8(bb, "debug_mode", config->debug_mode);
    
    blobmsg_close_table(bb, config_table);
}

// Helper function to add statistics to blob
static void add_statistics_to_blob(struct blob_buf *bb, const starlink_weather_snow_melt_stats_t *stats) {
    void *stats_table = blobmsg_open_table(bb, "statistics");
    
    blobmsg_add_u32(bb, "total_mode_changes", stats->total_mode_changes);
    blobmsg_add_u32(bb, "automatic_activations", stats->automatic_activations);
    blobmsg_add_u32(bb, "preheat_activations", stats->preheat_activations);
    blobmsg_add_u32(bb, "manual_activations", stats->manual_activations);
    blobmsg_add_u32(bb, "weather_checks_performed", stats->weather_checks_performed);
    blobmsg_add_u32(bb, "successful_weather_checks", stats->successful_weather_checks);
    blobmsg_add_u32(bb, "failed_weather_checks", stats->failed_weather_checks);
    blobmsg_add_double(bb, "average_weather_check_time_ms", stats->average_weather_check_time_ms);
    blobmsg_add_u32(bb, "last_automatic_activation", stats->last_automatic_activation);
    blobmsg_add_u32(bb, "last_preheat_activation", stats->last_preheat_activation);
    blobmsg_add_u32(bb, "last_manual_activation", stats->last_manual_activation);
    
    blobmsg_close_table(bb, stats_table);
}

// UBUS method: get_status
int starlink_weather_snow_melt_ubus_get_status(struct ubus_context *ctx, struct ubus_object *obj,
                                               struct ubus_request_data *req, const char *method,
                                               struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    starlink_weather_snow_melt_status_t status;
    int result = starlink_weather_snow_melt_control_get_status(&status);
    
    if (result == AUTONOMY_SUCCESS) {
        blobmsg_add_u8(&bb, "success", true);
        add_status_to_blob(&bb, &status);
    } else {
        blobmsg_add_u8(&bb, "success", false);
        blobmsg_add_u32(&bb, "error", result);
        blobmsg_add_string(&bb, "message", "Failed to get snow melt control status");
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    
    return 0;
}

// UBUS method: get_config
int starlink_weather_snow_melt_ubus_get_config(struct ubus_context *ctx, struct ubus_object *obj,
                                               struct ubus_request_data *req, const char *method,
                                               struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    starlink_weather_snow_melt_config_t config;
    int result = starlink_weather_snow_melt_control_get_config(&config);
    
    if (result == AUTONOMY_SUCCESS) {
        blobmsg_add_u8(&bb, "success", true);
        add_config_to_blob(&bb, &config);
    } else {
        blobmsg_add_u8(&bb, "success", false);
        blobmsg_add_u32(&bb, "error", result);
        blobmsg_add_string(&bb, "message", "Failed to get snow melt control configuration");
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    
    return 0;
}

// UBUS method: set_config
int starlink_weather_snow_melt_ubus_set_config(struct ubus_context *ctx, struct ubus_object *obj,
                                               struct ubus_request_data *req, const char *method,
                                               struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    struct blob_attr *tb[__STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_MAX];
    blobmsg_parse(starlink_weather_snow_melt_ubus_args, __STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_MAX, tb, blob_data(msg), blob_len(msg));
    
    starlink_weather_snow_melt_config_t config;
    int result = starlink_weather_snow_melt_control_get_config(&config);
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_u8(&bb, "success", false);
        blobmsg_add_u32(&bb, "error", result);
        blobmsg_add_string(&bb, "message", "Failed to get current configuration");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    // Update configuration with provided values
    if (tb[STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_ENABLED]) {
        config.enabled = blobmsg_get_bool(tb[STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_ENABLED]);
    }
    
    if (tb[STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_TEMPERATURE_THRESHOLD]) {
        config.temperature_threshold_celsius = blobmsg_get_double(tb[STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_TEMPERATURE_THRESHOLD]);
    }
    
    if (tb[STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_WEATHER_CHECK_INTERVAL]) {
        config.weather_check_interval_minutes = blobmsg_get_u32(tb[STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_WEATHER_CHECK_INTERVAL]);
    }
    
    if (tb[STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_PREHEAT_DURATION]) {
        config.preheat_duration_minutes = blobmsg_get_u32(tb[STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_PREHEAT_DURATION]);
    }
    
    if (tb[STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_USE_FORECAST]) {
        config.use_forecast = blobmsg_get_bool(tb[STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_USE_FORECAST]);
    }
    
    if (tb[STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_FORECAST_HOURS_AHEAD]) {
        config.forecast_hours_ahead = blobmsg_get_u32(tb[STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_FORECAST_HOURS_AHEAD]);
    }
    
    if (tb[STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_WEATHER_API_KEY]) {
        strncpy(config.weather_api_key, blobmsg_get_string(tb[STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_WEATHER_API_KEY]), 
                sizeof(config.weather_api_key) - 1);
    }
    
    if (tb[STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_STARLINK_HOST]) {
        strncpy(config.starlink_host, blobmsg_get_string(tb[STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_STARLINK_HOST]), 
                sizeof(config.starlink_host) - 1);
    }
    
    if (tb[STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_STARLINK_PORT]) {
        config.starlink_port = blobmsg_get_u32(tb[STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_STARLINK_PORT]);
    }
    
    if (tb[STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_DEBUG_MODE]) {
        config.debug_mode = blobmsg_get_bool(tb[STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_DEBUG_MODE]);
    }
    
    // Set the updated configuration
    result = starlink_weather_snow_melt_control_set_config(&config);
    
    if (result == AUTONOMY_SUCCESS) {
        blobmsg_add_u8(&bb, "success", true);
        blobmsg_add_string(&bb, "message", "Snow melt control configuration updated successfully");
    } else {
        blobmsg_add_u8(&bb, "success", false);
        blobmsg_add_u32(&bb, "error", result);
        blobmsg_add_string(&bb, "message", "Failed to update snow melt control configuration");
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    
    return 0;
}

// UBUS method: set_enabled
int starlink_weather_snow_melt_ubus_set_enabled(struct ubus_context *ctx, struct ubus_object *obj,
                                                struct ubus_request_data *req, const char *method,
                                                struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    struct blob_attr *tb[__STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_MAX];
    blobmsg_parse(starlink_weather_snow_melt_ubus_args, __STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_MAX, tb, blob_data(msg), blob_len(msg));
    
    if (!tb[STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_ENABLED]) {
        blobmsg_add_u8(&bb, "success", false);
        blobmsg_add_u32(&bb, "error", AUTONOMY_ERROR_INVALID_PARAM);
        blobmsg_add_string(&bb, "message", "Missing 'enabled' parameter");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    bool enabled = blobmsg_get_bool(tb[STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_ENABLED]);
    int result = starlink_weather_snow_melt_control_set_enabled(enabled);
    
    if (result == AUTONOMY_SUCCESS) {
        blobmsg_add_u8(&bb, "success", true);
        blobmsg_add_string(&bb, "message", enabled ? "Snow melt control enabled" : "Snow melt control disabled");
    } else {
        blobmsg_add_u8(&bb, "success", false);
        blobmsg_add_u32(&bb, "error", result);
        blobmsg_add_string(&bb, "message", "Failed to set snow melt control enabled state");
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    
    return 0;
}

// UBUS method: set_mode
int starlink_weather_snow_melt_ubus_set_mode(struct ubus_context *ctx, struct ubus_object *obj,
                                             struct ubus_request_data *req, const char *method,
                                             struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    struct blob_attr *tb[__STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_MAX];
    blobmsg_parse(starlink_weather_snow_melt_ubus_args, __STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_MAX, tb, blob_data(msg), blob_len(msg));
    
    if (!tb[STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_MODE]) {
        blobmsg_add_u8(&bb, "success", false);
        blobmsg_add_u32(&bb, "error", AUTONOMY_ERROR_INVALID_PARAM);
        blobmsg_add_string(&bb, "message", "Missing 'mode' parameter");
        ubus_send_reply(ctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    const char *mode_str = blobmsg_get_string(tb[STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_MODE]);
    snow_melt_mode_t mode = string_to_snow_melt_mode(mode_str);
    
    int result = starlink_weather_snow_melt_control_set_mode(mode);
    
    if (result == AUTONOMY_SUCCESS) {
        blobmsg_add_u8(&bb, "success", true);
        blobmsg_add_string(&bb, "message", "Snow melt mode set successfully");
    } else {
        blobmsg_add_u8(&bb, "success", false);
        blobmsg_add_u32(&bb, "error", result);
        blobmsg_add_string(&bb, "message", "Failed to set snow melt mode");
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    
    return 0;
}

// UBUS method: force_update
int starlink_weather_snow_melt_ubus_force_update(struct ubus_context *ctx, struct ubus_object *obj,
                                                 struct ubus_request_data *req, const char *method,
                                                 struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    int result = starlink_weather_snow_melt_control_force_update();
    
    if (result == AUTONOMY_SUCCESS) {
        blobmsg_add_u8(&bb, "success", true);
        blobmsg_add_string(&bb, "message", "Weather check and mode update completed");
    } else {
        blobmsg_add_u8(&bb, "success", false);
        blobmsg_add_u32(&bb, "error", result);
        blobmsg_add_string(&bb, "message", "Failed to perform weather check and mode update");
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    
    return 0;
}

// UBUS method: get_statistics
int starlink_weather_snow_melt_ubus_get_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                                   struct ubus_request_data *req, const char *method,
                                                   struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    starlink_weather_snow_melt_stats_t stats;
    int result = starlink_weather_snow_melt_control_get_statistics(&stats);
    
    if (result == AUTONOMY_SUCCESS) {
        blobmsg_add_u8(&bb, "success", true);
        add_statistics_to_blob(&bb, &stats);
    } else {
        blobmsg_add_u8(&bb, "success", false);
        blobmsg_add_u32(&bb, "error", result);
        blobmsg_add_string(&bb, "message", "Failed to get snow melt control statistics");
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    
    return 0;
}

// UBUS method: reset_statistics
int starlink_weather_snow_melt_ubus_reset_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                                     struct ubus_request_data *req, const char *method,
                                                     struct blob_attr *msg) {
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0);
    
    int result = starlink_weather_snow_melt_control_reset_statistics();
    
    if (result == AUTONOMY_SUCCESS) {
        blobmsg_add_u8(&bb, "success", true);
        blobmsg_add_string(&bb, "message", "Snow melt control statistics reset successfully");
    } else {
        blobmsg_add_u8(&bb, "success", false);
        blobmsg_add_u32(&bb, "error", result);
        blobmsg_add_string(&bb, "message", "Failed to reset snow melt control statistics");
    }
    
    ubus_send_reply(ctx, req, bb.head);
    blob_buf_free(&bb);
    
    return 0;
}
