#ifndef STARLINK_SNOW_DETECTION_UBUS_H
#define STARLINK_SNOW_DETECTION_UBUS_H

#include "starlink_snow_detection.h"
#include <libubox/blobmsg.h>
#include <libubus.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// UBUS policy definitions for snow detection methods
enum {
    SNOW_DETECTION_STATUS_UNSPEC,
    SNOW_DETECTION_STATUS_ENABLED,
    SNOW_DETECTION_STATUS_HEATING_ACTIVE,
    SNOW_DETECTION_STATUS_CONSECUTIVE_SAMPLES,
    SNOW_DETECTION_STATUS_TOTAL_DETECTIONS,
    SNOW_DETECTION_STATUS_SUCCESSFUL_MELTS,
    SNOW_DETECTION_STATUS_FALSE_POSITIVES,
    SNOW_DETECTION_STATUS_LAST_CLEAR_TIME,
    SNOW_DETECTION_STATUS_HEATING_START_TIME,
    SNOW_DETECTION_STATUS_HEATING_DURATION,
    SNOW_DETECTION_STATUS_CONTEXT,
    __SNOW_DETECTION_STATUS_MAX
};

static const struct blobmsg_policy snow_detection_status_policy[] = {
    [SNOW_DETECTION_STATUS_ENABLED] = { .name = "enabled", .type = BLOBMSG_TYPE_BOOL },
    [SNOW_DETECTION_STATUS_HEATING_ACTIVE] = { .name = "heating_active", .type = BLOBMSG_TYPE_BOOL },
    [SNOW_DETECTION_STATUS_CONSECUTIVE_SAMPLES] = { .name = "consecutive_samples", .type = BLOBMSG_TYPE_INT32 },
    [SNOW_DETECTION_STATUS_TOTAL_DETECTIONS] = { .name = "total_detections", .type = BLOBMSG_TYPE_INT32 },
    [SNOW_DETECTION_STATUS_SUCCESSFUL_MELTS] = { .name = "successful_melts", .type = BLOBMSG_TYPE_INT32 },
    [SNOW_DETECTION_STATUS_FALSE_POSITIVES] = { .name = "false_positives", .type = BLOBMSG_TYPE_INT32 },
    [SNOW_DETECTION_STATUS_LAST_CLEAR_TIME] = { .name = "last_clear_time", .type = BLOBMSG_TYPE_INT32 },
    [SNOW_DETECTION_STATUS_HEATING_START_TIME] = { .name = "heating_start_time", .type = BLOBMSG_TYPE_INT32 },
    [SNOW_DETECTION_STATUS_HEATING_DURATION] = { .name = "heating_duration", .type = BLOBMSG_TYPE_INT32 },
    [SNOW_DETECTION_STATUS_CONTEXT] = { .name = "context", .type = BLOBMSG_TYPE_TABLE },
};

enum {
    SNOW_DETECTION_CONFIG_UNSPEC,
    SNOW_DETECTION_CONFIG_ENABLED,
    SNOW_DETECTION_CONFIG_DETECTION_SAMPLES,
    SNOW_DETECTION_CONFIG_OBSTRUCTION_THRESHOLD,
    SNOW_DETECTION_CONFIG_SNR_DEGRADATION_THRESHOLD,
    SNOW_DETECTION_CONFIG_TEMPERATURE_THRESHOLD,
    SNOW_DETECTION_CONFIG_VERIFICATION_TIME,
    SNOW_DETECTION_CONFIG_MELT_TIMEOUT,
    __SNOW_DETECTION_CONFIG_MAX
};

static const struct blobmsg_policy snow_detection_config_policy[] = {
    [SNOW_DETECTION_CONFIG_ENABLED] = { .name = "enabled", .type = BLOBMSG_TYPE_BOOL },
    [SNOW_DETECTION_CONFIG_DETECTION_SAMPLES] = { .name = "detection_samples", .type = BLOBMSG_TYPE_INT32 },
    [SNOW_DETECTION_CONFIG_OBSTRUCTION_THRESHOLD] = { .name = "obstruction_threshold", .type = BLOBMSG_TYPE_DOUBLE },
    [SNOW_DETECTION_CONFIG_SNR_DEGRADATION_THRESHOLD] = { .name = "snr_degradation_threshold", .type = BLOBMSG_TYPE_DOUBLE },
    [SNOW_DETECTION_CONFIG_TEMPERATURE_THRESHOLD] = { .name = "temperature_threshold", .type = BLOBMSG_TYPE_DOUBLE },
    [SNOW_DETECTION_CONFIG_VERIFICATION_TIME] = { .name = "verification_time", .type = BLOBMSG_TYPE_INT32 },
    [SNOW_DETECTION_CONFIG_MELT_TIMEOUT] = { .name = "melt_timeout", .type = BLOBMSG_TYPE_INT32 },
};

// Simple policies for methods that don't require parameters
static const struct blobmsg_policy snow_detection_enable_policy[] = {
    { .name = "enabled", .type = BLOBMSG_TYPE_BOOL },
};

static const struct blobmsg_policy snow_detection_disable_policy[] = {
    { .name = "disabled", .type = BLOBMSG_TYPE_BOOL },
};

static const struct blobmsg_policy snow_detection_force_check_policy[] = {
    { .name = "force_check", .type = BLOBMSG_TYPE_BOOL },
};

static const struct blobmsg_policy snow_detection_start_heating_policy[] = {
    { .name = "start_heating", .type = BLOBMSG_TYPE_BOOL },
};

static const struct blobmsg_policy snow_detection_stop_heating_policy[] = {
    { .name = "stop_heating", .type = BLOBMSG_TYPE_BOOL },
};

static const struct blobmsg_policy snow_detection_statistics_policy[] = {
    { .name = "statistics", .type = BLOBMSG_TYPE_BOOL },
};

static const struct blobmsg_policy snow_detection_reset_stats_policy[] = {
    { .name = "reset_stats", .type = BLOBMSG_TYPE_BOOL },
};

static const struct blobmsg_policy snow_detection_set_config_policy[] = {
    [SNOW_DETECTION_CONFIG_ENABLED] = { .name = "enabled", .type = BLOBMSG_TYPE_BOOL },
    [SNOW_DETECTION_CONFIG_DETECTION_SAMPLES] = { .name = "detection_samples", .type = BLOBMSG_TYPE_INT32 },
    [SNOW_DETECTION_CONFIG_OBSTRUCTION_THRESHOLD] = { .name = "obstruction_threshold", .type = BLOBMSG_TYPE_DOUBLE },
    [SNOW_DETECTION_CONFIG_SNR_DEGRADATION_THRESHOLD] = { .name = "snr_degradation_threshold", .type = BLOBMSG_TYPE_DOUBLE },
    [SNOW_DETECTION_CONFIG_TEMPERATURE_THRESHOLD] = { .name = "temperature_threshold", .type = BLOBMSG_TYPE_DOUBLE },
    [SNOW_DETECTION_CONFIG_VERIFICATION_TIME] = { .name = "verification_time", .type = BLOBMSG_TYPE_INT32 },
    [SNOW_DETECTION_CONFIG_MELT_TIMEOUT] = { .name = "melt_timeout", .type = BLOBMSG_TYPE_INT32 },
};

static const struct blobmsg_policy snow_detection_get_config_policy[] = {
    { .name = "get_config", .type = BLOBMSG_TYPE_BOOL },
};

// API Functions

// Initialization and cleanup
int starlink_snow_detection_ubus_init(void);
void starlink_snow_detection_ubus_cleanup(void);

// Utility functions
struct ubus_context *starlink_snow_detection_get_ubus_context(void);
bool starlink_snow_detection_ubus_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif // STARLINK_SNOW_DETECTION_UBUS_H
