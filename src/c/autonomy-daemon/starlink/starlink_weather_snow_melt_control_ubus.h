#ifndef STARLINK_WEATHER_SNOW_MELT_CONTROL_UBUS_H
#define STARLINK_WEATHER_SNOW_MELT_CONTROL_UBUS_H

#include "starlink_weather_snow_melt_control.h"
#include <libubox/blobmsg_json.h>
#include <libubox/blobmsg.h>
#include <libubus.h>

#ifdef __cplusplus
extern "C" {
#endif

// UBUS object name
#define STARLINK_WEATHER_SNOW_MELT_UBUS_OBJECT "starlink.weather_snow_melt"

// UBUS method names
#define STARLINK_WEATHER_SNOW_MELT_UBUS_GET_STATUS "get_status"
#define STARLINK_WEATHER_SNOW_MELT_UBUS_GET_CONFIG "get_config"
#define STARLINK_WEATHER_SNOW_MELT_UBUS_SET_CONFIG "set_config"
#define STARLINK_WEATHER_SNOW_MELT_UBUS_SET_ENABLED "set_enabled"
#define STARLINK_WEATHER_SNOW_MELT_UBUS_SET_MODE "set_mode"
#define STARLINK_WEATHER_SNOW_MELT_UBUS_FORCE_UPDATE "force_update"
#define STARLINK_WEATHER_SNOW_MELT_UBUS_GET_STATISTICS "get_statistics"
#define STARLINK_WEATHER_SNOW_MELT_UBUS_RESET_STATISTICS "reset_statistics"

// UBUS method policies
enum {
    STARLINK_WEATHER_SNOW_MELT_UBUS_POLICY_GET_STATUS,
    STARLINK_WEATHER_SNOW_MELT_UBUS_POLICY_GET_CONFIG,
    STARLINK_WEATHER_SNOW_MELT_UBUS_POLICY_SET_CONFIG,
    STARLINK_WEATHER_SNOW_MELT_UBUS_POLICY_SET_ENABLED,
    STARLINK_WEATHER_SNOW_MELT_UBUS_POLICY_SET_MODE,
    STARLINK_WEATHER_SNOW_MELT_UBUS_POLICY_FORCE_UPDATE,
    STARLINK_WEATHER_SNOW_MELT_UBUS_POLICY_GET_STATISTICS,
    STARLINK_WEATHER_SNOW_MELT_UBUS_POLICY_RESET_STATISTICS,
    __STARLINK_WEATHER_SNOW_MELT_UBUS_POLICY_MAX
};

// UBUS method arguments
enum {
    STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_ENABLED,
    STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_MODE,
    STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_TEMPERATURE_THRESHOLD,
    STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_WEATHER_CHECK_INTERVAL,
    STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_PREHEAT_DURATION,
    STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_USE_FORECAST,
    STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_FORECAST_HOURS_AHEAD,
    STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_WEATHER_API_KEY,
    STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_STARLINK_HOST,
    STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_STARLINK_PORT,
    STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_DEBUG_MODE,
    __STARLINK_WEATHER_SNOW_MELT_UBUS_ARG_MAX
};

// UBUS response fields
enum {
    STARLINK_WEATHER_SNOW_MELT_UBUS_RESPONSE_SUCCESS,
    STARLINK_WEATHER_SNOW_MELT_UBUS_RESPONSE_ERROR,
    STARLINK_WEATHER_SNOW_MELT_UBUS_RESPONSE_MESSAGE,
    STARLINK_WEATHER_SNOW_MELT_UBUS_RESPONSE_STATUS,
    STARLINK_WEATHER_SNOW_MELT_UBUS_RESPONSE_CONFIG,
    STARLINK_WEATHER_SNOW_MELT_UBUS_RESPONSE_STATISTICS,
    __STARLINK_WEATHER_SNOW_MELT_UBUS_RESPONSE_MAX
};

// Function prototypes

/**
 * Initialize UBUS interface for weather-based snow melt control
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_weather_snow_melt_ubus_init(void);

/**
 * Cleanup UBUS interface for weather-based snow melt control
 */
void starlink_weather_snow_melt_ubus_cleanup(void);

/**
 * UBUS method handlers
 */
int starlink_weather_snow_melt_ubus_get_status(struct ubus_context *ctx, struct ubus_object *obj,
                                               struct ubus_request_data *req, const char *method,
                                               struct blob_attr *msg);

int starlink_weather_snow_melt_ubus_get_config(struct ubus_context *ctx, struct ubus_object *obj,
                                               struct ubus_request_data *req, const char *method,
                                               struct blob_attr *msg);

int starlink_weather_snow_melt_ubus_set_config(struct ubus_context *ctx, struct ubus_object *obj,
                                               struct ubus_request_data *req, const char *method,
                                               struct blob_attr *msg);

int starlink_weather_snow_melt_ubus_set_enabled(struct ubus_context *ctx, struct ubus_object *obj,
                                                struct ubus_request_data *req, const char *method,
                                                struct blob_attr *msg);

int starlink_weather_snow_melt_ubus_set_mode(struct ubus_context *ctx, struct ubus_object *obj,
                                             struct ubus_request_data *req, const char *method,
                                             struct blob_attr *msg);

int starlink_weather_snow_melt_ubus_force_update(struct ubus_context *ctx, struct ubus_object *obj,
                                                 struct ubus_request_data *req, const char *method,
                                                 struct blob_attr *msg);

int starlink_weather_snow_melt_ubus_get_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                                   struct ubus_request_data *req, const char *method,
                                                   struct blob_attr *msg);

int starlink_weather_snow_melt_ubus_reset_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                                     struct ubus_request_data *req, const char *method,
                                                     struct blob_attr *msg);

#ifdef __cplusplus
}
#endif

#endif // STARLINK_WEATHER_SNOW_MELT_CONTROL_UBUS_H
