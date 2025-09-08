#include "ml_monitor_ubus.h"
#include "ml_monitor.h"
#include "../utils/logx.h"
#include <libubox/blobmsg_json.h>
#include <json-c/json.h>
#include <string.h>

// UBUS policy definitions
enum {
    ML_CONFIG_ENABLED,
    ML_CONFIG_COLLECTION_INTERVAL,
    ML_CONFIG_PREDICTION_HORIZON,
    ML_CONFIG_MAX_OBSERVATIONS,
    ML_CONFIG_LEARNING_RATE,
    ML_CONFIG_CONFIDENCE_THRESHOLD,
    ML_CONFIG_MOBILE_MODE_ENABLED,
    ML_CONFIG_AUTO_TUNING_ENABLED,
    __ML_CONFIG_MAX
};

static const struct blobmsg_policy ml_config_policy[__ML_CONFIG_MAX] = {
    [ML_CONFIG_ENABLED] = { .name = "enabled", .type = BLOBMSG_TYPE_BOOL },
    [ML_CONFIG_COLLECTION_INTERVAL] = { .name = "collection_interval_seconds", .type = BLOBMSG_TYPE_INT32 },
    [ML_CONFIG_PREDICTION_HORIZON] = { .name = "prediction_horizon_minutes", .type = BLOBMSG_TYPE_INT32 },
    [ML_CONFIG_MAX_OBSERVATIONS] = { .name = "max_observations", .type = BLOBMSG_TYPE_INT32 },
    [ML_CONFIG_LEARNING_RATE] = { .name = "learning_rate", .type = BLOBMSG_TYPE_INT32 },
    [ML_CONFIG_CONFIDENCE_THRESHOLD] = { .name = "confidence_threshold", .type = BLOBMSG_TYPE_INT32 },
    [ML_CONFIG_MOBILE_MODE_ENABLED] = { .name = "mobile_mode_enabled", .type = BLOBMSG_TYPE_BOOL },
    [ML_CONFIG_AUTO_TUNING_ENABLED] = { .name = "auto_tuning_enabled", .type = BLOBMSG_TYPE_BOOL },
};

// UBUS object and methods
static struct ubus_object ml_monitor_object;

// UBUS method: status
int ml_monitor_ubus_status(struct ubus_context *ctx, struct ubus_object *obj,
                          struct ubus_request_data *req, const char *method,
                          struct blob_attr *msg) {
    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    
    ml_monitor_t *monitor = ml_monitor_get_instance();
    
    if (monitor) {
        blobmsg_add_u8(&b, "initialized", monitor->initialized);
        blobmsg_add_u8(&b, "running", monitor->running);
        blobmsg_add_u32(&b, "last_collection", monitor->last_collection);
        blobmsg_add_u32(&b, "last_prediction", monitor->last_prediction);
        
        if (monitor->state) {
            blobmsg_add_u32(&b, "total_observations", monitor->state->total_observations);
            blobmsg_add_u32(&b, "location_changes", monitor->state->location_changes);
            blobmsg_add_u32(&b, "storage_size_kb", monitor->storage_size / 1024);
            
            // Data integration status
            void *integration_table = blobmsg_open_table(&b, "data_integration");
            blobmsg_add_u8(&b, "starlink_available", 1); // Would check actual availability
            blobmsg_add_u8(&b, "gps_available", 1);      // Would check actual availability
            blobmsg_add_u8(&b, "weather_available", 1);  // Would check actual availability
            blobmsg_add_string(&b, "integration_status", "Phase 3 - Advanced Sky Grid & Sliding Window");
            blobmsg_close_table(&b, integration_table);
            
            // Location learning status
            location_learner_t *learner = &monitor->state->models.location_learner;
            void *location_table = blobmsg_open_table(&b, "location_learning");
            blobmsg_add_double(&b, "current_lat", learner->current_lat_e7 / 10000000.0);
            blobmsg_add_double(&b, "current_lon", learner->current_lon_e7 / 10000000.0);
            blobmsg_add_u32(&b, "observations_here", learner->observations_here);
            blobmsg_add_u32(&b, "learned_percentage", learner->profile.learned);
            blobmsg_add_u32(&b, "location_history_count", learner->history_count);
            blobmsg_close_table(&b, location_table);
            
            // Sky grid status
            compact_sky_grid_t *grid = &monitor->state->models.sky_grid;
            void *sky_table = blobmsg_open_table(&b, "sky_grid");
            blobmsg_add_double(&b, "learned_lat", grid->learned_lat_e7 / 10000000.0);
            blobmsg_add_double(&b, "learned_lon", grid->learned_lon_e7 / 10000000.0);
            blobmsg_add_u32(&b, "learning_hours", grid->learning_time_hours);
            blobmsg_add_u32(&b, "last_update", grid->last_update);
            blobmsg_close_table(&b, sky_table);
            
            // Performance metrics
            void *perf_table = blobmsg_open_table(&b, "performance");
            performance_monitor_t *perf = &monitor->state->models.performance;
            blobmsg_add_u32(&b, "predictions_made", perf->predictions_made);
            blobmsg_add_u32(&b, "predictions_correct", perf->predictions_correct);
            blobmsg_add_u32(&b, "false_positives", perf->false_positives);
            blobmsg_add_u32(&b, "false_negatives", perf->false_negatives);
            blobmsg_add_u8(&b, "accuracy_pct", perf->metrics.accuracy_pct);
            blobmsg_add_u8(&b, "precision_pct", perf->metrics.precision_pct);
            blobmsg_add_u8(&b, "recall_pct", perf->metrics.recall_pct);
            blobmsg_add_u32(&b, "memory_peak_kb", perf->resources.memory_peak_kb);
            blobmsg_close_table(&b, perf_table);
        }
        
        blobmsg_add_string(&b, "status", "active");
    } else {
        blobmsg_add_string(&b, "status", "not_initialized");
        blobmsg_add_u8(&b, "initialized", 0);
        blobmsg_add_u8(&b, "running", 0);
    }
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// UBUS method: start
int ml_monitor_ubus_start(struct ubus_context *ctx, struct ubus_object *obj,
                         struct ubus_request_data *req, const char *method,
                         struct blob_attr *msg) {
    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    
    ml_monitor_t *monitor = ml_monitor_get_instance();
    
    if (!monitor) {
        // Initialize ML monitor with default configuration
        ml_monitor_config_t config;
        ml_monitor_load_config_from_uci(&config);
        
        monitor = ml_monitor_init(&config);
        if (!monitor) {
            blobmsg_add_string(&b, "error", "Failed to initialize ML monitor");
            blobmsg_add_u32(&b, "code", ML_MONITOR_ERROR_NOT_INITIALIZED);
            ubus_send_reply(ctx, req, b.head);
            blob_buf_free(&b);
            return UBUS_STATUS_OK;
        }
    }
    
    int result = ml_monitor_start(monitor);
    
    if (result == ML_MONITOR_SUCCESS) {
        blobmsg_add_string(&b, "status", "started");
        blobmsg_add_u32(&b, "code", result);
    } else {
        blobmsg_add_string(&b, "error", "Failed to start ML monitor");
        blobmsg_add_u32(&b, "code", result);
    }
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// UBUS method: stop
int ml_monitor_ubus_stop(struct ubus_context *ctx, struct ubus_object *obj,
                        struct ubus_request_data *req, const char *method,
                        struct blob_attr *msg) {
    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    
    ml_monitor_t *monitor = ml_monitor_get_instance();
    
    if (!monitor) {
        blobmsg_add_string(&b, "error", "ML monitor not initialized");
        blobmsg_add_u32(&b, "code", ML_MONITOR_ERROR_NOT_INITIALIZED);
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_OK;
    }
    
    int result = ml_monitor_stop(monitor);
    
    if (result == ML_MONITOR_SUCCESS) {
        blobmsg_add_string(&b, "status", "stopped");
        blobmsg_add_u32(&b, "code", result);
    } else {
        blobmsg_add_string(&b, "error", "Failed to stop ML monitor");
        blobmsg_add_u32(&b, "code", result);
    }
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// UBUS method: restart
int ml_monitor_ubus_restart(struct ubus_context *ctx, struct ubus_object *obj,
                           struct ubus_request_data *req, const char *method,
                           struct blob_attr *msg) {
    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    
    ml_monitor_t *monitor = ml_monitor_get_instance();
    
    if (monitor) {
        ml_monitor_stop(monitor);
    }
    
    // Reload configuration
    ml_monitor_config_t config;
    int load_result = ml_monitor_load_config_from_uci(&config);
    
    if (load_result != ML_MONITOR_SUCCESS) {
        blobmsg_add_string(&b, "error", "Failed to load configuration");
        blobmsg_add_u32(&b, "code", load_result);
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_OK;
    }
    
    if (!monitor) {
        monitor = ml_monitor_init(&config);
        if (!monitor) {
            blobmsg_add_string(&b, "error", "Failed to initialize ML monitor");
            blobmsg_add_u32(&b, "code", ML_MONITOR_ERROR_NOT_INITIALIZED);
            ubus_send_reply(ctx, req, b.head);
            blob_buf_free(&b);
            return UBUS_STATUS_OK;
        }
    } else {
        ml_monitor_update_config(monitor, &config);
    }
    
    int start_result = ml_monitor_start(monitor);
    
    if (start_result == ML_MONITOR_SUCCESS) {
        blobmsg_add_string(&b, "status", "restarted");
        blobmsg_add_u32(&b, "code", start_result);
    } else {
        blobmsg_add_string(&b, "error", "Failed to restart ML monitor");
        blobmsg_add_u32(&b, "code", start_result);
    }
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// UBUS method: get_config
int ml_monitor_ubus_get_config(struct ubus_context *ctx, struct ubus_object *obj,
                              struct ubus_request_data *req, const char *method,
                              struct blob_attr *msg) {
    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    
    ml_monitor_config_t config;
    int result = ml_monitor_load_config_from_uci(&config);
    
    if (result != ML_MONITOR_SUCCESS) {
        blobmsg_add_string(&b, "error", "Failed to load configuration");
        blobmsg_add_u32(&b, "code", result);
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_OK;
    }
    
    // Add configuration to response
    blobmsg_add_u8(&b, "enabled", config.enabled);
    blobmsg_add_u32(&b, "collection_interval_seconds", config.collection_interval_seconds);
    blobmsg_add_u32(&b, "prediction_horizon_minutes", config.prediction_horizon_minutes);
    blobmsg_add_u32(&b, "max_observations", config.max_observations);
    blobmsg_add_u32(&b, "learning_rate", config.learning_rate);
    blobmsg_add_u32(&b, "confidence_threshold", config.confidence_threshold);
    blobmsg_add_u32(&b, "pattern_library_size", config.pattern_library_size);
    blobmsg_add_u32(&b, "neural_network_size", config.neural_network_size);
    blobmsg_add_u32(&b, "sky_grid_azimuth_resolution", config.sky_grid_azimuth_resolution);
    blobmsg_add_u32(&b, "sky_grid_elevation_resolution", config.sky_grid_elevation_resolution);
    blobmsg_add_u32(&b, "sky_grid_learning_rate", config.sky_grid_learning_rate);
    blobmsg_add_u8(&b, "mobile_mode_enabled", config.mobile_mode_enabled);
    blobmsg_add_u32(&b, "location_change_threshold_meters", config.location_change_threshold_meters);
    blobmsg_add_u32(&b, "stationary_time_threshold_minutes", config.stationary_time_threshold_minutes);
    blobmsg_add_u8(&b, "auto_tuning_enabled", config.auto_tuning_enabled);
    blobmsg_add_u32(&b, "performance_evaluation_interval_hours", config.performance_evaluation_interval_hours);
    blobmsg_add_u32(&b, "memory_limit_kb", config.memory_limit_kb);
    blobmsg_add_string(&b, "storage_path", config.storage_path);
    blobmsg_add_u8(&b, "use_memory_mapped_storage", config.use_memory_mapped_storage);
    blobmsg_add_u32(&b, "storage_sync_interval_minutes", config.storage_sync_interval_minutes);
    blobmsg_add_u8(&b, "debug_logging_enabled", config.debug_logging_enabled);
    blobmsg_add_u8(&b, "save_raw_observations", config.save_raw_observations);
    blobmsg_add_string(&b, "debug_log_path", config.debug_log_path);
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// UBUS method: set_config
int ml_monitor_ubus_set_config(struct ubus_context *ctx, struct ubus_object *obj,
                              struct ubus_request_data *req, const char *method,
                              struct blob_attr *msg) {
    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    
    struct blob_attr *tb[__ML_CONFIG_MAX];
    blobmsg_parse(ml_config_policy, __ML_CONFIG_MAX, tb, blob_data(msg), blob_len(msg));
    
    // Load current configuration
    ml_monitor_config_t config;
    int result = ml_monitor_load_config_from_uci(&config);
    
    if (result != ML_MONITOR_SUCCESS) {
        blobmsg_add_string(&b, "error", "Failed to load current configuration");
        blobmsg_add_u32(&b, "code", result);
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_OK;
    }
    
    // Update configuration with provided values
    if (tb[ML_CONFIG_ENABLED]) {
        config.enabled = blobmsg_get_bool(tb[ML_CONFIG_ENABLED]);
    }
    if (tb[ML_CONFIG_COLLECTION_INTERVAL]) {
        config.collection_interval_seconds = blobmsg_get_u32(tb[ML_CONFIG_COLLECTION_INTERVAL]);
    }
    if (tb[ML_CONFIG_PREDICTION_HORIZON]) {
        config.prediction_horizon_minutes = blobmsg_get_u32(tb[ML_CONFIG_PREDICTION_HORIZON]);
    }
    if (tb[ML_CONFIG_MAX_OBSERVATIONS]) {
        config.max_observations = blobmsg_get_u32(tb[ML_CONFIG_MAX_OBSERVATIONS]);
    }
    if (tb[ML_CONFIG_LEARNING_RATE]) {
        config.learning_rate = blobmsg_get_u32(tb[ML_CONFIG_LEARNING_RATE]);
    }
    if (tb[ML_CONFIG_CONFIDENCE_THRESHOLD]) {
        config.confidence_threshold = blobmsg_get_u32(tb[ML_CONFIG_CONFIDENCE_THRESHOLD]);
    }
    if (tb[ML_CONFIG_MOBILE_MODE_ENABLED]) {
        config.mobile_mode_enabled = blobmsg_get_bool(tb[ML_CONFIG_MOBILE_MODE_ENABLED]);
    }
    if (tb[ML_CONFIG_AUTO_TUNING_ENABLED]) {
        config.auto_tuning_enabled = blobmsg_get_bool(tb[ML_CONFIG_AUTO_TUNING_ENABLED]);
    }
    
    // Validate configuration
    result = ml_monitor_validate_config(&config);
    if (result != ML_MONITOR_SUCCESS) {
        blobmsg_add_string(&b, "error", "Invalid configuration");
        blobmsg_add_u32(&b, "code", result);
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_OK;
    }
    
    // Save configuration
    result = ml_monitor_save_config_to_uci(&config);
    if (result != ML_MONITOR_SUCCESS) {
        blobmsg_add_string(&b, "error", "Failed to save configuration");
        blobmsg_add_u32(&b, "code", result);
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_OK;
    }
    
    // Update running instance if available
    ml_monitor_t *monitor = ml_monitor_get_instance();
    if (monitor) {
        ml_monitor_update_config(monitor, &config);
    }
    
    blobmsg_add_string(&b, "status", "configuration_updated");
    blobmsg_add_u32(&b, "code", ML_MONITOR_SUCCESS);
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// UBUS method: get_predictions
int ml_monitor_ubus_get_predictions(struct ubus_context *ctx, struct ubus_object *obj,
                                   struct ubus_request_data *req, const char *method,
                                   struct blob_attr *msg) {
    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    
    ml_monitor_t *monitor = ml_monitor_get_instance();
    
    if (!monitor || !monitor->running) {
        blobmsg_add_string(&b, "error", "ML monitor not running");
        blobmsg_add_u32(&b, "code", ML_MONITOR_ERROR_NOT_RUNNING);
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_OK;
    }
    
    // Get current predictions (15 minutes ahead, 15-second intervals)
    uint8_t probabilities[60];
    uint8_t confidence;
    
    // Try enhanced predictions first (Phase 3), fall back to basic predictions
    int result = ml_monitor_predict_next_15_minutes_enhanced(monitor, probabilities, &confidence);
    if (result != ML_MONITOR_SUCCESS) {
        result = ml_monitor_predict_next_15_minutes(monitor, probabilities, &confidence);
    }
    
    if (result == ML_MONITOR_SUCCESS) {
        blobmsg_add_u8(&b, "confidence", confidence);
        blobmsg_add_u32(&b, "prediction_horizon_minutes", 15);
        blobmsg_add_u32(&b, "interval_seconds", 15);
        
        void *array = blobmsg_open_array(&b, "probabilities");
        for (int i = 0; i < 60; i++) {
            blobmsg_add_u32(&b, NULL, probabilities[i]);
        }
        blobmsg_close_array(&b, array);
        
        blobmsg_add_u32(&b, "timestamp", time(NULL));
    } else {
        blobmsg_add_string(&b, "error", "Failed to generate predictions");
        blobmsg_add_u32(&b, "code", result);
    }
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// UBUS method: get_statistics
int ml_monitor_ubus_get_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                  struct ubus_request_data *req, const char *method,
                                  struct blob_attr *msg) {
    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    
    ml_monitor_t *monitor = ml_monitor_get_instance();
    
    if (!monitor) {
        blobmsg_add_string(&b, "error", "ML monitor not initialized");
        blobmsg_add_u32(&b, "code", ML_MONITOR_ERROR_NOT_INITIALIZED);
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_OK;
    }
    
    if (monitor->state) {
        ml_persistent_state_t *state = monitor->state;
        
        // General statistics
        blobmsg_add_u32(&b, "total_observations", state->total_observations);
        blobmsg_add_u32(&b, "location_changes", state->location_changes);
        blobmsg_add_u32(&b, "recent_observations_count", state->recent.count);
        blobmsg_add_u32(&b, "hourly_observations_count", state->hourly.count);
        blobmsg_add_u32(&b, "daily_observations_count", state->daily.count);
        
        // Pattern matcher statistics
        pattern_matcher_t *pm = &state->models.pattern_matcher;
        void *pm_table = blobmsg_open_table(&b, "pattern_matcher");
        blobmsg_add_u32(&b, "pattern_count", pm->count);
        blobmsg_add_u32(&b, "max_patterns", pm->max_patterns);
        blobmsg_add_u32(&b, "total_predictions", pm->total_predictions);
        blobmsg_add_u32(&b, "correct_predictions", pm->correct_predictions);
        blobmsg_add_double(&b, "accuracy", pm->accuracy);
        blobmsg_close_table(&b, pm_table);
        
        // Neural network statistics
        tiny_nn_t *nn = &state->models.neural_network;
        void *nn_table = blobmsg_open_table(&b, "neural_network");
        blobmsg_add_u32(&b, "update_count", nn->update_count);
        blobmsg_add_u32(&b, "learning_rate", nn->learning_rate);
        blobmsg_close_table(&b, nn_table);
        
        // Sky grid statistics
        compact_sky_grid_t *sg = &state->models.sky_grid;
        void *sg_table = blobmsg_open_table(&b, "sky_grid");
        blobmsg_add_u32(&b, "last_update", sg->last_update);
        blobmsg_add_u32(&b, "learned_lat_e7", sg->learned_lat_e7);
        blobmsg_add_u32(&b, "learned_lon_e7", sg->learned_lon_e7);
        blobmsg_add_u32(&b, "learning_time_hours", sg->learning_time_hours);
        blobmsg_close_table(&b, sg_table);
        
        // Location learner statistics
        location_learner_t *ll = &state->models.location_learner;
        void *ll_table = blobmsg_open_table(&b, "location_learner");
        blobmsg_add_u32(&b, "current_lat_e7", ll->current_lat_e7);
        blobmsg_add_u32(&b, "current_lon_e7", ll->current_lon_e7);
        blobmsg_add_u32(&b, "arrival_time", ll->arrival_time);
        blobmsg_add_u32(&b, "observations_here", ll->observations_here);
        blobmsg_add_u32(&b, "history_count", ll->history_count);
        blobmsg_add_u32(&b, "typical_snr", ll->profile.typical_snr);
        blobmsg_add_u32(&b, "typical_latency", ll->profile.typical_latency);
        blobmsg_add_u32(&b, "obstruction_level", ll->profile.obstruction_level);
        blobmsg_add_u32(&b, "reliability_score", ll->profile.reliability_score);
        blobmsg_add_u32(&b, "learned", ll->profile.learned);
        blobmsg_close_table(&b, ll_table);
        
        // Performance statistics
        performance_monitor_t *perf = &state->models.performance;
        void *perf_table = blobmsg_open_table(&b, "performance");
        blobmsg_add_u32(&b, "predictions_made", perf->predictions_made);
        blobmsg_add_u32(&b, "predictions_correct", perf->predictions_correct);
        blobmsg_add_u32(&b, "false_positives", perf->false_positives);
        blobmsg_add_u32(&b, "false_negatives", perf->false_negatives);
        blobmsg_add_u32(&b, "accuracy_pct", perf->metrics.accuracy_pct);
        blobmsg_add_u32(&b, "precision_pct", perf->metrics.precision_pct);
        blobmsg_add_u32(&b, "recall_pct", perf->metrics.recall_pct);
        blobmsg_add_u32(&b, "last_evaluation", perf->metrics.last_evaluation);
        blobmsg_add_u32(&b, "cpu_cycles_used", perf->resources.cpu_cycles_used);
        blobmsg_add_u32(&b, "memory_peak_kb", perf->resources.memory_peak_kb);
        blobmsg_add_u32(&b, "storage_used_kb", perf->resources.storage_used_kb);
        blobmsg_close_table(&b, perf_table);
    }
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// UBUS method: reset_learning
int ml_monitor_ubus_reset_learning(struct ubus_context *ctx, struct ubus_object *obj,
                                  struct ubus_request_data *req, const char *method,
                                  struct blob_attr *msg) {
    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    
    ml_monitor_t *monitor = ml_monitor_get_instance();
    
    if (!monitor) {
        blobmsg_add_string(&b, "error", "ML monitor not initialized");
        blobmsg_add_u32(&b, "code", ML_MONITOR_ERROR_NOT_INITIALIZED);
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_OK;
    }
    
    if (monitor->state) {
        // Reset learning models
        ml_monitor_sky_grid_reset_soft(&monitor->state->models.sky_grid);
        
        // Reset pattern matcher
        monitor->state->models.pattern_matcher.count = 0;
        monitor->state->models.pattern_matcher.total_predictions = 0;
        monitor->state->models.pattern_matcher.correct_predictions = 0;
        monitor->state->models.pattern_matcher.accuracy = 0.0;
        
        // Reset neural network update count
        monitor->state->models.neural_network.update_count = 0;
        
        // Reset performance monitor
        memset(&monitor->state->models.performance, 0, sizeof(performance_monitor_t));
        
        // Sync storage
        ml_monitor_sync_storage(monitor);
        
        blobmsg_add_string(&b, "status", "learning_reset");
        blobmsg_add_u32(&b, "code", ML_MONITOR_SUCCESS);
        
        LOGX_INFO("ML monitor learning data reset via UBUS");
    } else {
        blobmsg_add_string(&b, "error", "ML monitor state not available");
        blobmsg_add_u32(&b, "code", ML_MONITOR_ERROR_STORAGE_FAILED);
    }
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// UBUS method: export_data
int ml_monitor_ubus_export_data(struct ubus_context *ctx, struct ubus_object *obj,
                               struct ubus_request_data *req, const char *method,
                               struct blob_attr *msg) {
    struct blob_buf b = {};
    blob_buf_init(&b, 0);
    
    ml_monitor_t *monitor = ml_monitor_get_instance();
    
    if (!monitor || !monitor->state) {
        blobmsg_add_string(&b, "error", "ML monitor not initialized or no data available");
        blobmsg_add_u32(&b, "code", ML_MONITOR_ERROR_NOT_INITIALIZED);
        ubus_send_reply(ctx, req, b.head);
        blob_buf_free(&b);
        return UBUS_STATUS_OK;
    }
    
    // Export basic statistics (in a real implementation, this would export more data)
    blobmsg_add_string(&b, "export_format", "json");
    blobmsg_add_u32(&b, "export_timestamp", time(NULL));
    blobmsg_add_u32(&b, "total_observations", monitor->state->total_observations);
    blobmsg_add_u32(&b, "storage_size", monitor->storage_size);
    blobmsg_add_string(&b, "storage_path", monitor->config.storage_path);
    
    // Note: In a full implementation, this would export observation data,
    // learned patterns, neural network weights, etc. to a file or return
    // the data in the response
    
    blobmsg_add_string(&b, "status", "export_completed");
    blobmsg_add_u32(&b, "code", ML_MONITOR_SUCCESS);
    
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
    
    return UBUS_STATUS_OK;
}

// UBUS method definitions
static const struct ubus_method ml_monitor_methods[] = {
    UBUS_METHOD_NOARG(ML_MONITOR_UBUS_METHOD_STATUS, ml_monitor_ubus_status),
    UBUS_METHOD_NOARG(ML_MONITOR_UBUS_METHOD_START, ml_monitor_ubus_start),
    UBUS_METHOD_NOARG(ML_MONITOR_UBUS_METHOD_STOP, ml_monitor_ubus_stop),
    UBUS_METHOD_NOARG(ML_MONITOR_UBUS_METHOD_RESTART, ml_monitor_ubus_restart),
    UBUS_METHOD_NOARG(ML_MONITOR_UBUS_METHOD_GET_CONFIG, ml_monitor_ubus_get_config),
    UBUS_METHOD(ML_MONITOR_UBUS_METHOD_SET_CONFIG, ml_monitor_ubus_set_config, ml_config_policy),
    UBUS_METHOD_NOARG(ML_MONITOR_UBUS_METHOD_GET_PREDICTIONS, ml_monitor_ubus_get_predictions),
    UBUS_METHOD_NOARG(ML_MONITOR_UBUS_METHOD_GET_STATISTICS, ml_monitor_ubus_get_statistics),
    UBUS_METHOD_NOARG(ML_MONITOR_UBUS_METHOD_RESET_LEARNING, ml_monitor_ubus_reset_learning),
    UBUS_METHOD_NOARG(ML_MONITOR_UBUS_METHOD_EXPORT_DATA, ml_monitor_ubus_export_data),
};

// UBUS object type
static struct ubus_object_type ml_monitor_object_type =
    UBUS_OBJECT_TYPE(ML_MONITOR_UBUS_OBJECT, ml_monitor_methods);

// UBUS object
static struct ubus_object ml_monitor_object = {
    .name = ML_MONITOR_UBUS_OBJECT,
    .type = &ml_monitor_object_type,
    .methods = ml_monitor_methods,
    .n_methods = ARRAY_SIZE(ml_monitor_methods),
};

// Initialize UBUS interface
int ml_monitor_ubus_init(struct ubus_context *ctx) {
    if (!ctx) return -1;
    
    LOGX_INFO("Initializing ML monitor UBUS interface");
    
    return ML_MONITOR_SUCCESS;
}

// Add UBUS object
int ml_monitor_ubus_add_object(struct ubus_context *ctx) {
    if (!ctx) return -1;
    
    int ret = ubus_add_object(ctx, &ml_monitor_object);
    if (ret) {
        LOGX_ERROR("Failed to add ML monitor UBUS object: %s", ubus_strerror(ret));
        return -1;
    }
    
    LOGX_INFO("ML monitor UBUS object added successfully");
    return 0;
}

// Remove UBUS object
void ml_monitor_ubus_remove_object(struct ubus_context *ctx) {
    if (!ctx) return;
    
    ubus_remove_object(ctx, &ml_monitor_object);
    LOGX_INFO("ML monitor UBUS object removed");
}

// Cleanup UBUS interface
void ml_monitor_ubus_cleanup(struct ubus_context *ctx) {
    if (!ctx) return;
    
    ml_monitor_ubus_remove_object(ctx);
    LOGX_INFO("ML monitor UBUS interface cleaned up");
}