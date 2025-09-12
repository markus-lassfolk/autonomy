#include "starlink_snow_detection_ubus.h"
#include "starlink_snow_detection.h"
#include "../shared/logging/logx.h"
#include "../core/types.h"
#include <libubox/blobmsg_json.h>
#include <libubox/uloop.h>
#include <libubus.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>

// UBUS service name
#define SNOW_DETECTION_UBUS_SERVICE "starlink.snow_detection"

// UBUS method names
#define SNOW_DETECTION_METHOD_STATUS "status"
#define SNOW_DETECTION_METHOD_CONFIG "config"
#define SNOW_DETECTION_METHOD_ENABLE "enable"
#define SNOW_DETECTION_METHOD_DISABLE "disable"
#define SNOW_DETECTION_METHOD_FORCE_CHECK "force_check"
#define SNOW_DETECTION_METHOD_START_HEATING "start_heating"
#define SNOW_DETECTION_METHOD_STOP_HEATING "stop_heating"
#define SNOW_DETECTION_METHOD_STATISTICS "statistics"
#define SNOW_DETECTION_METHOD_RESET_STATS "reset_stats"
#define SNOW_DETECTION_METHOD_SET_CONFIG "set_config"
#define SNOW_DETECTION_METHOD_GET_CONFIG "get_config"

// Global UBUS context
static struct ubus_context *g_ubus_ctx = NULL;
static struct ubus_object g_snow_detection_object;
static bool g_ubus_initialized = false;
static pthread_mutex_t g_ubus_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations
static int ubus_snow_detection_status(struct ubus_context *ctx, struct ubus_object *obj,
                                     struct ubus_request_data *req, const char *method,
                                     struct blob_attr *msg);
static int ubus_snow_detection_config(struct ubus_context *ctx, struct ubus_object *obj,
                                     struct ubus_request_data *req, const char *method,
                                     struct blob_attr *msg);
static int ubus_snow_detection_enable(struct ubus_context *ctx, struct ubus_object *obj,
                                     struct ubus_request_data *req, const char *method,
                                     struct blob_attr *msg);
static int ubus_snow_detection_disable(struct ubus_context *ctx, struct ubus_object *obj,
                                      struct ubus_request_data *req, const char *method,
                                      struct blob_attr *msg);
static int ubus_snow_detection_force_check(struct ubus_context *ctx, struct ubus_object *obj,
                                          struct ubus_request_data *req, const char *method,
                                          struct blob_attr *msg);
static int ubus_snow_detection_start_heating(struct ubus_context *ctx, struct ubus_object *obj,
                                            struct ubus_request_data *req, const char *method,
                                            struct blob_attr *msg);
static int ubus_snow_detection_stop_heating(struct ubus_context *ctx, struct ubus_object *obj,
                                           struct ubus_request_data *req, const char *method,
                                           struct blob_attr *msg);
static int ubus_snow_detection_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                         struct ubus_request_data *req, const char *method,
                                         struct blob_attr *msg);
static int ubus_snow_detection_reset_stats(struct ubus_context *ctx, struct ubus_object *obj,
                                          struct ubus_request_data *req, const char *method,
                                          struct blob_attr *msg);
static int ubus_snow_detection_set_config(struct ubus_context *ctx, struct ubus_object *obj,
                                         struct ubus_request_data *req, const char *method,
                                         struct blob_attr *msg);
static int ubus_snow_detection_get_config(struct ubus_context *ctx, struct ubus_object *obj,
                                         struct ubus_request_data *req, const char *method,
                                         struct blob_attr *msg);

// UBUS method definitions
static const struct ubus_method snow_detection_methods[] = {
    UBUS_METHOD(SNOW_DETECTION_METHOD_STATUS, ubus_snow_detection_status, snow_detection_status_policy),
    UBUS_METHOD(SNOW_DETECTION_METHOD_CONFIG, ubus_snow_detection_config, snow_detection_config_policy),
    UBUS_METHOD(SNOW_DETECTION_METHOD_ENABLE, ubus_snow_detection_enable, snow_detection_enable_policy),
    UBUS_METHOD(SNOW_DETECTION_METHOD_DISABLE, ubus_snow_detection_disable, snow_detection_disable_policy),
    UBUS_METHOD(SNOW_DETECTION_METHOD_FORCE_CHECK, ubus_snow_detection_force_check, snow_detection_force_check_policy),
    UBUS_METHOD(SNOW_DETECTION_METHOD_START_HEATING, ubus_snow_detection_start_heating, snow_detection_start_heating_policy),
    UBUS_METHOD(SNOW_DETECTION_METHOD_STOP_HEATING, ubus_snow_detection_stop_heating, snow_detection_stop_heating_policy),
    UBUS_METHOD(SNOW_DETECTION_METHOD_STATISTICS, ubus_snow_detection_statistics, snow_detection_statistics_policy),
    UBUS_METHOD(SNOW_DETECTION_METHOD_RESET_STATS, ubus_snow_detection_reset_stats, snow_detection_reset_stats_policy),
    UBUS_METHOD(SNOW_DETECTION_METHOD_SET_CONFIG, ubus_snow_detection_set_config, snow_detection_set_config_policy),
    UBUS_METHOD(SNOW_DETECTION_METHOD_GET_CONFIG, ubus_snow_detection_get_config, snow_detection_get_config_policy),
};

// UBUS object type
static struct ubus_object_type snow_detection_object_type = 
    UBUS_OBJECT_TYPE(SNOW_DETECTION_UBUS_SERVICE, snow_detection_methods);

// Initialize UBUS service
int starlink_snow_detection_ubus_init(void) {
    if (g_ubus_initialized) {
        LOGX_WARN_MSG("Snow detection UBUS service already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_ubus_mutex);
    
    // Initialize UBUS context
    g_ubus_ctx = ubus_connect(NULL);
    if (!g_ubus_ctx) {
        LOGX_ERROR_MSG("Failed to connect to UBUS");
        pthread_mutex_unlock(&g_ubus_mutex);
        return AUTONOMY_ERROR_API_FAILED;
    }
    
    // Initialize snow detection system first
    int result = starlink_snow_detection_init();
    if (result != AUTONOMY_SUCCESS) {
        LOGX_ERROR_MSG("Failed to initialize snow detection system", "result", result);
        ubus_free(g_ubus_ctx);
        g_ubus_ctx = NULL;
        pthread_mutex_unlock(&g_ubus_mutex);
        return result;
    }
    
    // Set up UBUS object
    g_snow_detection_object.name = SNOW_DETECTION_UBUS_SERVICE;
    g_snow_detection_object.type = &snow_detection_object_type;
    g_snow_detection_object.methods = snow_detection_methods;
    g_snow_detection_object.n_methods = ARRAY_SIZE(snow_detection_methods);
    
    // Add object to UBUS
    int ubus_result = ubus_add_object(g_ubus_ctx, &g_snow_detection_object);
    if (ubus_result != 0) {
        LOGX_ERROR_MSG("Failed to add snow detection object to UBUS", "result", ubus_result);
        starlink_snow_detection_cleanup();
        ubus_free(g_ubus_ctx);
        g_ubus_ctx = NULL;
        pthread_mutex_unlock(&g_ubus_mutex);
        return AUTONOMY_ERROR_API_FAILED;
    }
    
    g_ubus_initialized = true;
    pthread_mutex_unlock(&g_ubus_mutex);
    
    LOGX_INFO_MSG("Snow detection UBUS service initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Get status via UBUS
static int ubus_snow_detection_status(struct ubus_context *ctx, struct ubus_object *obj,
                                     struct ubus_request_data *req, const char *method,
                                     struct blob_attr *msg) {
    struct blob_buf b = {0};
    starlink_snow_detection_status_t status;
    
    blob_buf_init(&b, 0);
    
    // Get status from snow detection system
    int result = starlink_snow_detection_get_status(&status);
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&b, "error", "Failed to get snow detection status");
        blobmsg_add_u32(&b, "result", result);
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_UNKNOWN_ERROR;
    }
    
    // Add status information
    blobmsg_add_string(&b, "status", "success");
    blobmsg_add_u8(&b, "enabled", status.enabled);
    blobmsg_add_u8(&b, "heating_active", status.is_heating_active);
    blobmsg_add_u32(&b, "consecutive_obstruction_samples", status.consecutive_obstruction_samples);
    blobmsg_add_u32(&b, "total_detections", status.total_detections);
    blobmsg_add_u32(&b, "successful_melts", status.successful_melts);
    blobmsg_add_u32(&b, "false_positives", status.false_positives);
    blobmsg_add_u32(&b, "last_clear_time", status.last_clear_time);
    blobmsg_add_u32(&b, "heating_start_time", status.heating_start_time);
    blobmsg_add_u32(&b, "heating_duration", status.heating_duration);
    
    // Add context information
    void *context_table = blobmsg_open_table(&b, "context");
    blobmsg_add_u8(&b, "is_stationary", status.context.is_stationary);
    blobmsg_add_u8(&b, "is_winter_season", status.context.is_winter_season);
    blobmsg_add_u8(&b, "snow_forecast_active", status.context.snow_forecast_active);
    blobmsg_add_double(&b, "obstruction_increase_rate", status.context.obstruction_increase_rate);
    blobmsg_add_double(&b, "snr_degradation_rate", status.context.snr_degradation_rate);
    blobmsg_add_double(&b, "temperature", status.context.temperature);
    blobmsg_add_double(&b, "humidity", status.context.humidity);
    blobmsg_add_u32(&b, "last_clear_time", status.context.last_clear_time);
    blobmsg_add_u32(&b, "consecutive_obstruction_samples", status.context.consecutive_obstruction_samples);
    blobmsg_close_table(&b, context_table);
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// Get configuration via UBUS
static int ubus_snow_detection_config(struct ubus_context *ctx, struct ubus_object *obj,
                                     struct ubus_request_data *req, const char *method,
                                     struct blob_attr *msg) {
    struct blob_buf b = {0};
    starlink_snow_detection_config_t config;
    
    blob_buf_init(&b, 0);
    
    // Get configuration from snow detection system
    int result = starlink_snow_detection_get_config(&config);
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&b, "error", "Failed to get snow detection configuration");
        blobmsg_add_u32(&b, "result", result);
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_UNKNOWN_ERROR;
    }
    
    // Add configuration information
    blobmsg_add_string(&b, "status", "success");
    blobmsg_add_u8(&b, "enabled", config.enabled);
    blobmsg_add_u32(&b, "detection_samples", config.detection_samples);
    blobmsg_add_double(&b, "obstruction_threshold", config.obstruction_threshold);
    blobmsg_add_double(&b, "snr_degradation_threshold", config.snr_degradation_threshold);
    blobmsg_add_double(&b, "temperature_threshold", config.temperature_threshold);
    blobmsg_add_u32(&b, "verification_time", config.verification_time);
    blobmsg_add_u32(&b, "melt_timeout", config.melt_timeout);
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// Enable snow detection via UBUS
static int ubus_snow_detection_enable(struct ubus_context *ctx, struct ubus_object *obj,
                                     struct ubus_request_data *req, const char *method,
                                     struct blob_attr *msg) {
    struct blob_buf b = {0};
    
    blob_buf_init(&b, 0);
    
    // Enable snow detection system
    int result = starlink_snow_detection_set_enabled(true);
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&b, "error", "Failed to enable snow detection");
        blobmsg_add_u32(&b, "result", result);
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_UNKNOWN_ERROR;
    }
    
    blobmsg_add_string(&b, "status", "success");
    blobmsg_add_string(&b, "message", "Snow detection enabled");
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// Disable snow detection via UBUS
static int ubus_snow_detection_disable(struct ubus_context *ctx, struct ubus_object *obj,
                                      struct ubus_request_data *req, const char *method,
                                      struct blob_attr *msg) {
    struct blob_buf b = {0};
    
    blob_buf_init(&b, 0);
    
    // Disable snow detection system
    int result = starlink_snow_detection_set_enabled(false);
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&b, "error", "Failed to disable snow detection");
        blobmsg_add_u32(&b, "result", result);
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_UNKNOWN_ERROR;
    }
    
    blobmsg_add_string(&b, "status", "success");
    blobmsg_add_string(&b, "message", "Snow detection disabled");
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// Force check via UBUS
static int ubus_snow_detection_force_check(struct ubus_context *ctx, struct ubus_object *obj,
                                          struct ubus_request_data *req, const char *method,
                                          struct blob_attr *msg) {
    struct blob_buf b = {0};
    
    blob_buf_init(&b, 0);
    
    // Force snow detection check
    int result = starlink_snow_detection_force_check();
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&b, "error", "Failed to force snow detection check");
        blobmsg_add_u32(&b, "result", result);
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_UNKNOWN_ERROR;
    }
    
    blobmsg_add_string(&b, "status", "success");
    blobmsg_add_string(&b, "message", "Snow detection check completed");
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// Start heating via UBUS
static int ubus_snow_detection_start_heating(struct ubus_context *ctx, struct ubus_object *obj,
                                            struct ubus_request_data *req, const char *method,
                                            struct blob_attr *msg) {
    struct blob_buf b = {0};
    
    blob_buf_init(&b, 0);
    
    // Start heating manually
    int result = starlink_snow_detection_start_heating_manual();
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&b, "error", "Failed to start heating");
        blobmsg_add_u32(&b, "result", result);
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_UNKNOWN_ERROR;
    }
    
    blobmsg_add_string(&b, "status", "success");
    blobmsg_add_string(&b, "message", "Heating started manually");
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// Stop heating via UBUS
static int ubus_snow_detection_stop_heating(struct ubus_context *ctx, struct ubus_object *obj,
                                           struct ubus_request_data *req, const char *method,
                                           struct blob_attr *msg) {
    struct blob_buf b = {0};
    
    blob_buf_init(&b, 0);
    
    // Stop heating manually
    int result = starlink_snow_detection_stop_heating_manual();
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&b, "error", "Failed to stop heating");
        blobmsg_add_u32(&b, "result", result);
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_UNKNOWN_ERROR;
    }
    
    blobmsg_add_string(&b, "status", "success");
    blobmsg_add_string(&b, "message", "Heating stopped manually");
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// Get statistics via UBUS
static int ubus_snow_detection_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                         struct ubus_request_data *req, const char *method,
                                         struct blob_attr *msg) {
    struct blob_buf b = {0};
    starlink_snow_detection_stats_t stats;
    
    blob_buf_init(&b, 0);
    
    // Get statistics from snow detection system
    int result = starlink_snow_detection_get_statistics(&stats);
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&b, "error", "Failed to get snow detection statistics");
        blobmsg_add_u32(&b, "result", result);
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_UNKNOWN_ERROR;
    }
    
    // Add statistics information
    blobmsg_add_string(&b, "status", "success");
    blobmsg_add_u32(&b, "total_detections", stats.total_detections);
    blobmsg_add_u32(&b, "successful_melts", stats.successful_melts);
    blobmsg_add_u32(&b, "false_positives", stats.false_positives);
    blobmsg_add_u32(&b, "prewarm_actions", stats.prewarm_actions);
    blobmsg_add_u32(&b, "melt_actions", stats.melt_actions);
    blobmsg_add_u32(&b, "verify_actions", stats.verify_actions);
    blobmsg_add_double(&b, "average_melt_time", stats.average_melt_time);
    blobmsg_add_double(&b, "detection_accuracy", stats.detection_accuracy);
    blobmsg_add_u32(&b, "last_detection", stats.last_detection);
    blobmsg_add_u32(&b, "last_successful_melt", stats.last_successful_melt);
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// Reset statistics via UBUS
static int ubus_snow_detection_reset_stats(struct ubus_context *ctx, struct ubus_object *obj,
                                          struct ubus_request_data *req, const char *method,
                                          struct blob_attr *msg) {
    struct blob_buf b = {0};
    
    blob_buf_init(&b, 0);
    
    // Reset statistics
    int result = starlink_snow_detection_reset_statistics();
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&b, "error", "Failed to reset statistics");
        blobmsg_add_u32(&b, "result", result);
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_UNKNOWN_ERROR;
    }
    
    blobmsg_add_string(&b, "status", "success");
    blobmsg_add_string(&b, "message", "Statistics reset successfully");
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// Set configuration via UBUS
static int ubus_snow_detection_set_config(struct ubus_context *ctx, struct ubus_object *obj,
                                         struct ubus_request_data *req, const char *method,
                                         struct blob_attr *msg) {
    struct blob_buf b = {0};
    starlink_snow_detection_config_t config;
    struct blob_attr *tb[__SNOW_DETECTION_CONFIG_MAX];
    
    blob_buf_init(&b, 0);
    
    // Parse message
    blobmsg_parse(snow_detection_config_policy, ARRAY_SIZE(tb), tb, blob_data(msg), blob_len(msg));
    
    // Get current configuration first
    int result = starlink_snow_detection_get_config(&config);
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&b, "error", "Failed to get current configuration");
        blobmsg_add_u32(&b, "result", result);
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_UNKNOWN_ERROR;
    }
    
    // Update configuration with provided values
    if (tb[SNOW_DETECTION_CONFIG_ENABLED]) {
        config.enabled = blobmsg_get_bool(tb[SNOW_DETECTION_CONFIG_ENABLED]);
    }
    if (tb[SNOW_DETECTION_CONFIG_DETECTION_SAMPLES]) {
        config.detection_samples = blobmsg_get_u32(tb[SNOW_DETECTION_CONFIG_DETECTION_SAMPLES]);
    }
    if (tb[SNOW_DETECTION_CONFIG_OBSTRUCTION_THRESHOLD]) {
        config.obstruction_threshold = blobmsg_get_double(tb[SNOW_DETECTION_CONFIG_OBSTRUCTION_THRESHOLD]);
    }
    if (tb[SNOW_DETECTION_CONFIG_SNR_DEGRADATION_THRESHOLD]) {
        config.snr_degradation_threshold = blobmsg_get_double(tb[SNOW_DETECTION_CONFIG_SNR_DEGRADATION_THRESHOLD]);
    }
    if (tb[SNOW_DETECTION_CONFIG_TEMPERATURE_THRESHOLD]) {
        config.temperature_threshold = blobmsg_get_double(tb[SNOW_DETECTION_CONFIG_TEMPERATURE_THRESHOLD]);
    }
    if (tb[SNOW_DETECTION_CONFIG_VERIFICATION_TIME]) {
        config.verification_time = blobmsg_get_u32(tb[SNOW_DETECTION_CONFIG_VERIFICATION_TIME]);
    }
    if (tb[SNOW_DETECTION_CONFIG_MELT_TIMEOUT]) {
        config.melt_timeout = blobmsg_get_u32(tb[SNOW_DETECTION_CONFIG_MELT_TIMEOUT]);
    }
    
    // Set new configuration
    result = starlink_snow_detection_set_config(&config);
    if (result != AUTONOMY_SUCCESS) {
        blobmsg_add_string(&b, "error", "Failed to set configuration");
        blobmsg_add_u32(&b, "result", result);
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_UNKNOWN_ERROR;
    }
    
    blobmsg_add_string(&b, "status", "success");
    blobmsg_add_string(&b, "message", "Configuration updated successfully");
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// Get configuration via UBUS (alias for config method)
static int ubus_snow_detection_get_config(struct ubus_context *ctx, struct ubus_object *obj,
                                         struct ubus_request_data *req, const char *method,
                                         struct blob_attr *msg) {
    return ubus_snow_detection_config(ctx, obj, req, method, msg);
}

// Cleanup UBUS service
void starlink_snow_detection_ubus_cleanup(void) {
    if (!g_ubus_initialized) {
        return;
    }
    
    pthread_mutex_lock(&g_ubus_mutex);
    
    // Remove object from UBUS
    if (g_ubus_ctx) {
        ubus_remove_object(g_ubus_ctx, &g_snow_detection_object);
        ubus_free(g_ubus_ctx);
        g_ubus_ctx = NULL;
    }
    
    // Cleanup snow detection system
    starlink_snow_detection_cleanup();
    
    g_ubus_initialized = false;
    pthread_mutex_unlock(&g_ubus_mutex);
    
    LOGX_INFO_MSG("Snow detection UBUS service cleaned up");
}

// Get UBUS context (for external use)
struct ubus_context *starlink_snow_detection_get_ubus_context(void) {
    return g_ubus_ctx;
}

// Check if UBUS service is initialized
bool starlink_snow_detection_ubus_is_initialized(void) {
    return g_ubus_initialized;
}
