#include "ml_monitor.h"
#include "../utils/logx.h"
#include "../utils/uci_manager.h"
#include <uci.h>
#include <string.h>
#include <stdlib.h>

// Helper function to set UCI options using proper UCI API
static int set_uci_option(struct uci_context *ctx, const char *package, const char *section, const char *option, const char *value) {
    struct uci_ptr ptr;
    memset(&ptr, 0, sizeof(ptr));
    ptr.package = package;
    ptr.section = section;
    ptr.option = option;
    
    // Use uci_lookup_ptr to find the option
    if (uci_lookup_ptr(ctx, &ptr, NULL, true) != UCI_OK) {
        LOGX_ERROR_MSG("Failed to lookup UCI option: %s.%s.%s", package, section, option);
        return -1;
    }
    
    // Set the value
    if (uci_set(ctx, &ptr) != UCI_OK) {
        LOGX_ERROR_MSG("Failed to set UCI option: %s.%s.%s", package, section, option);
        return -1;
    }
    
    return 0;
}

// UCI configuration section name
#define UCI_ML_MONITOR_PACKAGE "autonomy"
#define UCI_ML_MONITOR_SECTION "ml_monitor"

// UCI option names
#define UCI_OPT_ENABLED                           "enabled"
#define UCI_OPT_COLLECTION_INTERVAL_SECONDS       "collection_interval_seconds"
#define UCI_OPT_PREDICTION_HORIZON_MINUTES        "prediction_horizon_minutes"
#define UCI_OPT_MAX_OBSERVATIONS                  "max_observations"
#define UCI_OPT_LEARNING_RATE                     "learning_rate"
#define UCI_OPT_CONFIDENCE_THRESHOLD              "confidence_threshold"
#define UCI_OPT_PATTERN_LIBRARY_SIZE              "pattern_library_size"
#define UCI_OPT_NEURAL_NETWORK_SIZE               "neural_network_size"
#define UCI_OPT_SKY_GRID_AZIMUTH_RESOLUTION       "sky_grid_azimuth_resolution"
#define UCI_OPT_SKY_GRID_ELEVATION_RESOLUTION     "sky_grid_elevation_resolution"
#define UCI_OPT_SKY_GRID_LEARNING_RATE            "sky_grid_learning_rate"
#define UCI_OPT_MOBILE_MODE_ENABLED               "mobile_mode_enabled"
#define UCI_OPT_LOCATION_CHANGE_THRESHOLD_METERS  "location_change_threshold_meters"
#define UCI_OPT_STATIONARY_TIME_THRESHOLD_MINUTES "stationary_time_threshold_minutes"
#define UCI_OPT_AUTO_TUNING_ENABLED               "auto_tuning_enabled"
#define UCI_OPT_PERFORMANCE_EVALUATION_INTERVAL_HOURS "performance_evaluation_interval_hours"
#define UCI_OPT_MEMORY_LIMIT_KB                   "memory_limit_kb"
#define UCI_OPT_STORAGE_PATH                      "storage_path"
#define UCI_OPT_USE_MEMORY_MAPPED_STORAGE         "use_memory_mapped_storage"
#define UCI_OPT_STORAGE_SYNC_INTERVAL_MINUTES     "storage_sync_interval_minutes"
#define UCI_OPT_DEBUG_LOGGING_ENABLED             "debug_logging_enabled"
#define UCI_OPT_SAVE_RAW_OBSERVATIONS             "save_raw_observations"
#define UCI_OPT_DEBUG_LOG_PATH                    "debug_log_path"

// Helper function to get string option
static const char* uci_get_string_option(struct uci_context *ctx, struct uci_section *s, const char *option, const char *default_value) {
    const char *value = uci_lookup_option_string(ctx, s, option);
    return value ? value : default_value;
}

// Helper function to get integer option
static int uci_get_int_option(struct uci_context *ctx, struct uci_section *s, const char *option, int default_value) {
    const char *value = uci_lookup_option_string(ctx, s, option);
    return value ? atoi(value) : default_value;
}

// Helper function to get boolean option
static bool uci_get_bool_option(struct uci_context *ctx, struct uci_section *s, const char *option, bool default_value) {
    const char *value = uci_lookup_option_string(ctx, s, option);
    if (!value) return default_value;
    
    return (strcmp(value, "1") == 0 || 
            strcmp(value, "true") == 0 || 
            strcmp(value, "yes") == 0 || 
            strcmp(value, "on") == 0);
}

// Load ML monitor configuration from UCI
int ml_monitor_load_config_from_uci(ml_monitor_config_t *config) {
    if (!config) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    LOGX_INFO_MSG("Loading ML monitor configuration from UCI");
    
    // Initialize with defaults first
    ml_monitor_config_init_defaults(config);
    
    struct uci_context *ctx = uci_alloc_context();
    if (!ctx) {
        LOGX_ERROR_MSG("Failed to allocate UCI context");
        return ML_MONITOR_ERROR_UCI_FAILED;
    }
    
    struct uci_package *pkg = NULL;
    int ret = uci_load(ctx, UCI_ML_MONITOR_PACKAGE, &pkg);
    if (ret != UCI_OK) {
        LOGX_WARN_MSG("Failed to load UCI package '%s', using defaults", UCI_ML_MONITOR_PACKAGE);
        uci_free_context(ctx);
        return ML_MONITOR_SUCCESS; // Use defaults
    }
    
    struct uci_section *s = uci_lookup_section(ctx, pkg, UCI_ML_MONITOR_SECTION);
    if (!s) {
        LOGX_WARN_MSG("ML monitor section not found in UCI config, using defaults");
        uci_free_context(ctx);
        return ML_MONITOR_SUCCESS; // Use defaults
    }
    
    // Load configuration values
    config->enabled = uci_get_bool_option(ctx, s, UCI_OPT_ENABLED, config->enabled);
    config->collection_interval_seconds = uci_get_int_option(ctx, s, UCI_OPT_COLLECTION_INTERVAL_SECONDS, config->collection_interval_seconds);
    config->prediction_horizon_minutes = uci_get_int_option(ctx, s, UCI_OPT_PREDICTION_HORIZON_MINUTES, config->prediction_horizon_minutes);
    config->max_observations = uci_get_int_option(ctx, s, UCI_OPT_MAX_OBSERVATIONS, config->max_observations);
    
    // Learning parameters
    config->learning_rate = uci_get_int_option(ctx, s, UCI_OPT_LEARNING_RATE, config->learning_rate);
    config->confidence_threshold = uci_get_int_option(ctx, s, UCI_OPT_CONFIDENCE_THRESHOLD, config->confidence_threshold);
    config->pattern_library_size = uci_get_int_option(ctx, s, UCI_OPT_PATTERN_LIBRARY_SIZE, config->pattern_library_size);
    config->neural_network_size = uci_get_int_option(ctx, s, UCI_OPT_NEURAL_NETWORK_SIZE, config->neural_network_size);
    
    // Sky grid settings
    config->sky_grid_azimuth_resolution = uci_get_int_option(ctx, s, UCI_OPT_SKY_GRID_AZIMUTH_RESOLUTION, config->sky_grid_azimuth_resolution);
    config->sky_grid_elevation_resolution = uci_get_int_option(ctx, s, UCI_OPT_SKY_GRID_ELEVATION_RESOLUTION, config->sky_grid_elevation_resolution);
    config->sky_grid_learning_rate = uci_get_int_option(ctx, s, UCI_OPT_SKY_GRID_LEARNING_RATE, config->sky_grid_learning_rate);
    
    // Mobile optimization
    config->mobile_mode_enabled = uci_get_bool_option(ctx, s, UCI_OPT_MOBILE_MODE_ENABLED, config->mobile_mode_enabled);
    config->location_change_threshold_meters = uci_get_int_option(ctx, s, UCI_OPT_LOCATION_CHANGE_THRESHOLD_METERS, config->location_change_threshold_meters);
    config->stationary_time_threshold_minutes = uci_get_int_option(ctx, s, UCI_OPT_STATIONARY_TIME_THRESHOLD_MINUTES, config->stationary_time_threshold_minutes);
    
    // Performance tuning
    config->auto_tuning_enabled = uci_get_bool_option(ctx, s, UCI_OPT_AUTO_TUNING_ENABLED, config->auto_tuning_enabled);
    config->performance_evaluation_interval_hours = uci_get_int_option(ctx, s, UCI_OPT_PERFORMANCE_EVALUATION_INTERVAL_HOURS, config->performance_evaluation_interval_hours);
    config->memory_limit_kb = uci_get_int_option(ctx, s, UCI_OPT_MEMORY_LIMIT_KB, config->memory_limit_kb);
    
    // Storage settings
    const char *storage_path = uci_get_string_option(ctx, s, UCI_OPT_STORAGE_PATH, config->storage_path);
    strncpy(config->storage_path, storage_path, sizeof(config->storage_path) - 1);
    config->use_memory_mapped_storage = uci_get_bool_option(ctx, s, UCI_OPT_USE_MEMORY_MAPPED_STORAGE, config->use_memory_mapped_storage);
    config->storage_sync_interval_minutes = uci_get_int_option(ctx, s, UCI_OPT_STORAGE_SYNC_INTERVAL_MINUTES, config->storage_sync_interval_minutes);
    
    // Debug settings
    config->debug_logging_enabled = uci_get_bool_option(ctx, s, UCI_OPT_DEBUG_LOGGING_ENABLED, config->debug_logging_enabled);
    config->save_raw_observations = uci_get_bool_option(ctx, s, UCI_OPT_SAVE_RAW_OBSERVATIONS, config->save_raw_observations);
    const char *debug_log_path = uci_get_string_option(ctx, s, UCI_OPT_DEBUG_LOG_PATH, config->debug_log_path);
    strncpy(config->debug_log_path, debug_log_path, sizeof(config->debug_log_path) - 1);
    
    uci_free_context(ctx);
    
    LOGX_INFO_MSG("ML monitor configuration loaded from UCI successfully");
    LOGX_DEBUG_MSG("ML monitor enabled: %s, collection interval: %d seconds", 
              config->enabled ? "yes" : "no", config->collection_interval_seconds);
    
    return ML_MONITOR_SUCCESS;
}

// Save ML monitor configuration to UCI
int ml_monitor_save_config_to_uci(const ml_monitor_config_t *config) {
    if (!config) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    LOGX_INFO_MSG("Saving ML monitor configuration to UCI");
    
    struct uci_context *ctx = uci_alloc_context();
    if (!ctx) {
        LOGX_ERROR_MSG("Failed to allocate UCI context");
        return ML_MONITOR_ERROR_UCI_FAILED;
    }
    
    struct uci_package *pkg = NULL;
    int ret = uci_load(ctx, UCI_ML_MONITOR_PACKAGE, &pkg);
    if (ret != UCI_OK) {
        LOGX_ERROR_MSG("Failed to load UCI package '%s'", UCI_ML_MONITOR_PACKAGE);
        uci_free_context(ctx);
        return ML_MONITOR_ERROR_UCI_FAILED;
    }
    
    // Get or create the ML monitor section
    struct uci_section *s = uci_lookup_section(ctx, pkg, UCI_ML_MONITOR_SECTION);
    if (!s) {
        // Create section if it doesn't exist
        struct uci_ptr ptr;
        memset(&ptr, 0, sizeof(ptr));
        ptr.package = UCI_ML_MONITOR_PACKAGE;
        ptr.section = UCI_ML_MONITOR_SECTION;
        ptr.value = "ml_monitor";
        
        if (uci_set(ctx, &ptr) != UCI_OK) {
            LOGX_ERROR_MSG("Failed to create UCI section");
            uci_free_context(ctx);
            return ML_MONITOR_ERROR_UCI_FAILED;
        }
        
        s = ptr.s;
    }
    
    
    // Set all configuration options
    char buffer[256];
    
    set_uci_option(ctx, UCI_ML_MONITOR_PACKAGE, UCI_ML_MONITOR_SECTION, UCI_OPT_ENABLED, config->enabled ? "1" : "0");
    
    snprintf(buffer, sizeof(buffer), "%d", config->collection_interval_seconds);
    set_uci_option(ctx, UCI_ML_MONITOR_PACKAGE, UCI_ML_MONITOR_SECTION, UCI_OPT_COLLECTION_INTERVAL_SECONDS, buffer);
    
    snprintf(buffer, sizeof(buffer), "%d", config->prediction_horizon_minutes);
    set_uci_option(ctx, UCI_ML_MONITOR_PACKAGE, UCI_ML_MONITOR_SECTION, UCI_OPT_PREDICTION_HORIZON_MINUTES, buffer);
    
    snprintf(buffer, sizeof(buffer), "%d", config->max_observations);
    set_uci_option(ctx, UCI_ML_MONITOR_PACKAGE, UCI_ML_MONITOR_SECTION, UCI_OPT_MAX_OBSERVATIONS, buffer);
    
    snprintf(buffer, sizeof(buffer), "%d", config->learning_rate);
    set_uci_option(ctx, UCI_ML_MONITOR_PACKAGE, UCI_ML_MONITOR_SECTION, UCI_OPT_LEARNING_RATE, buffer);
    
    snprintf(buffer, sizeof(buffer), "%d", config->confidence_threshold);
    set_uci_option(ctx, UCI_ML_MONITOR_PACKAGE, UCI_ML_MONITOR_SECTION, UCI_OPT_CONFIDENCE_THRESHOLD, buffer);
    
    snprintf(buffer, sizeof(buffer), "%d", config->pattern_library_size);
    set_uci_option(ctx, UCI_ML_MONITOR_PACKAGE, UCI_ML_MONITOR_SECTION, UCI_OPT_PATTERN_LIBRARY_SIZE, buffer);
    
    snprintf(buffer, sizeof(buffer), "%d", config->neural_network_size);
    set_uci_option(ctx, UCI_ML_MONITOR_PACKAGE, UCI_ML_MONITOR_SECTION, UCI_OPT_NEURAL_NETWORK_SIZE, buffer);
    
    snprintf(buffer, sizeof(buffer), "%d", config->sky_grid_azimuth_resolution);
    set_uci_option(ctx, UCI_ML_MONITOR_PACKAGE, UCI_ML_MONITOR_SECTION, UCI_OPT_SKY_GRID_AZIMUTH_RESOLUTION, buffer);
    
    snprintf(buffer, sizeof(buffer), "%d", config->sky_grid_elevation_resolution);
    set_uci_option(ctx, UCI_ML_MONITOR_PACKAGE, UCI_ML_MONITOR_SECTION, UCI_OPT_SKY_GRID_ELEVATION_RESOLUTION, buffer);
    
    snprintf(buffer, sizeof(buffer), "%d", config->sky_grid_learning_rate);
    set_uci_option(ctx, UCI_ML_MONITOR_PACKAGE, UCI_ML_MONITOR_SECTION, UCI_OPT_SKY_GRID_LEARNING_RATE, buffer);
    
    set_uci_option(ctx, UCI_ML_MONITOR_PACKAGE, UCI_ML_MONITOR_SECTION, UCI_OPT_MOBILE_MODE_ENABLED, config->mobile_mode_enabled ? "1" : "0");
    
    snprintf(buffer, sizeof(buffer), "%d", config->location_change_threshold_meters);
    set_uci_option(ctx, UCI_ML_MONITOR_PACKAGE, UCI_ML_MONITOR_SECTION, UCI_OPT_LOCATION_CHANGE_THRESHOLD_METERS, buffer);
    
    snprintf(buffer, sizeof(buffer), "%d", config->stationary_time_threshold_minutes);
    set_uci_option(ctx, UCI_ML_MONITOR_PACKAGE, UCI_ML_MONITOR_SECTION, UCI_OPT_STATIONARY_TIME_THRESHOLD_MINUTES, buffer);
    
    set_uci_option(ctx, UCI_ML_MONITOR_PACKAGE, UCI_ML_MONITOR_SECTION, UCI_OPT_AUTO_TUNING_ENABLED, config->auto_tuning_enabled ? "1" : "0");
    
    snprintf(buffer, sizeof(buffer), "%d", config->performance_evaluation_interval_hours);
    set_uci_option(ctx, UCI_ML_MONITOR_PACKAGE, UCI_ML_MONITOR_SECTION, UCI_OPT_PERFORMANCE_EVALUATION_INTERVAL_HOURS, buffer);
    
    snprintf(buffer, sizeof(buffer), "%d", config->memory_limit_kb);
    set_uci_option(ctx, UCI_ML_MONITOR_PACKAGE, UCI_ML_MONITOR_SECTION, UCI_OPT_MEMORY_LIMIT_KB, buffer);
    
    set_uci_option(ctx, UCI_ML_MONITOR_PACKAGE, UCI_ML_MONITOR_SECTION, UCI_OPT_STORAGE_PATH, config->storage_path);
    set_uci_option(ctx, UCI_ML_MONITOR_PACKAGE, UCI_ML_MONITOR_SECTION, UCI_OPT_USE_MEMORY_MAPPED_STORAGE, config->use_memory_mapped_storage ? "1" : "0");
    
    snprintf(buffer, sizeof(buffer), "%d", config->storage_sync_interval_minutes);
    set_uci_option(ctx, UCI_ML_MONITOR_PACKAGE, UCI_ML_MONITOR_SECTION, UCI_OPT_STORAGE_SYNC_INTERVAL_MINUTES, buffer);
    
    set_uci_option(ctx, UCI_ML_MONITOR_PACKAGE, UCI_ML_MONITOR_SECTION, UCI_OPT_DEBUG_LOGGING_ENABLED, config->debug_logging_enabled ? "1" : "0");
    set_uci_option(ctx, UCI_ML_MONITOR_PACKAGE, UCI_ML_MONITOR_SECTION, UCI_OPT_SAVE_RAW_OBSERVATIONS, config->save_raw_observations ? "1" : "0");
    set_uci_option(ctx, UCI_ML_MONITOR_PACKAGE, UCI_ML_MONITOR_SECTION, UCI_OPT_DEBUG_LOG_PATH, config->debug_log_path);
    
    
    // Commit changes
    if (uci_commit(ctx, &pkg, false) != UCI_OK) {
        LOGX_ERROR_MSG("Failed to commit UCI changes");
        uci_free_context(ctx);
        return ML_MONITOR_ERROR_UCI_FAILED;
    }
    
    uci_free_context(ctx);
    
    LOGX_INFO_MSG("ML monitor configuration saved to UCI successfully");
    return ML_MONITOR_SUCCESS;
}

// Validate ML monitor configuration
int ml_monitor_validate_config(const ml_monitor_config_t *config) {
    if (!config) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    // Validate ranges
    if (config->collection_interval_seconds < 1 || config->collection_interval_seconds > 3600) {
        LOGX_ERROR_MSG("Invalid collection interval: %d (must be 1-3600 seconds)", config->collection_interval_seconds);
        return ML_MONITOR_ERROR_CONFIG_FAILED;
    }
    
    if (config->prediction_horizon_minutes < 1 || config->prediction_horizon_minutes > 120) {
        LOGX_ERROR_MSG("Invalid prediction horizon: %d (must be 1-120 minutes)", config->prediction_horizon_minutes);
        return ML_MONITOR_ERROR_CONFIG_FAILED;
    }
    
    if (config->max_observations < 100 || config->max_observations > 100000) {
        LOGX_ERROR_MSG("Invalid max observations: %d (must be 100-100000)", config->max_observations);
        return ML_MONITOR_ERROR_CONFIG_FAILED;
    }
    
    if (config->learning_rate > 255) {
        LOGX_ERROR_MSG("Invalid learning rate: %d (must be 0-255)", config->learning_rate);
        return ML_MONITOR_ERROR_CONFIG_FAILED;
    }
    
    if (config->confidence_threshold > 255) {
        LOGX_ERROR_MSG("Invalid confidence threshold: %d (must be 0-255)", config->confidence_threshold);
        return ML_MONITOR_ERROR_CONFIG_FAILED;
    }
    
    if (config->pattern_library_size < 10 || config->pattern_library_size > 10000) {
        LOGX_ERROR_MSG("Invalid pattern library size: %d (must be 10-10000)", config->pattern_library_size);
        return ML_MONITOR_ERROR_CONFIG_FAILED;
    }
    
    if (config->neural_network_size < 1 || config->neural_network_size > 100) {
        LOGX_ERROR_MSG("Invalid neural network size: %d (must be 1-100 KB)", config->neural_network_size);
        return ML_MONITOR_ERROR_CONFIG_FAILED;
    }
    
    if (config->sky_grid_azimuth_resolution < 1 || config->sky_grid_azimuth_resolution > 10) {
        LOGX_ERROR_MSG("Invalid sky grid azimuth resolution: %d (must be 1-10 degrees)", config->sky_grid_azimuth_resolution);
        return ML_MONITOR_ERROR_CONFIG_FAILED;
    }
    
    if (config->sky_grid_elevation_resolution < 1 || config->sky_grid_elevation_resolution > 10) {
        LOGX_ERROR_MSG("Invalid sky grid elevation resolution: %d (must be 1-10 degrees)", config->sky_grid_elevation_resolution);
        return ML_MONITOR_ERROR_CONFIG_FAILED;
    }
    
    if (config->sky_grid_learning_rate > 255) {
        LOGX_ERROR_MSG("Invalid sky grid learning rate: %d (must be 0-255)", config->sky_grid_learning_rate);
        return ML_MONITOR_ERROR_CONFIG_FAILED;
    }
    
    if (config->location_change_threshold_meters < 10 || config->location_change_threshold_meters > 10000) {
        LOGX_ERROR_MSG("Invalid location change threshold: %d (must be 10-10000 meters)", config->location_change_threshold_meters);
        return ML_MONITOR_ERROR_CONFIG_FAILED;
    }
    
    if (config->stationary_time_threshold_minutes < 1 || config->stationary_time_threshold_minutes > 1440) {
        LOGX_ERROR_MSG("Invalid stationary time threshold: %d (must be 1-1440 minutes)", config->stationary_time_threshold_minutes);
        return ML_MONITOR_ERROR_CONFIG_FAILED;
    }
    
    if (config->performance_evaluation_interval_hours < 1 || config->performance_evaluation_interval_hours > 168) {
        LOGX_ERROR_MSG("Invalid performance evaluation interval: %d (must be 1-168 hours)", config->performance_evaluation_interval_hours);
        return ML_MONITOR_ERROR_CONFIG_FAILED;
    }
    
    if (config->memory_limit_kb < 100 || config->memory_limit_kb > 10240) {
        LOGX_ERROR_MSG("Invalid memory limit: %d (must be 100-10240 KB)", config->memory_limit_kb);
        return ML_MONITOR_ERROR_CONFIG_FAILED;
    }
    
    if (config->storage_sync_interval_minutes < 1 || config->storage_sync_interval_minutes > 60) {
        LOGX_ERROR_MSG("Invalid storage sync interval: %d (must be 1-60 minutes)", config->storage_sync_interval_minutes);
        return ML_MONITOR_ERROR_CONFIG_FAILED;
    }
    
    // Validate paths
    if (strlen(config->storage_path) == 0) {
        LOGX_ERROR_MSG("Storage path cannot be empty");
        return ML_MONITOR_ERROR_CONFIG_FAILED;
    }
    
    if (strlen(config->debug_log_path) == 0) {
        LOGX_ERROR_MSG("Debug log path cannot be empty");
        return ML_MONITOR_ERROR_CONFIG_FAILED;
    }
    
    LOGX_DEBUG_MSG("ML monitor configuration validation passed");
    return ML_MONITOR_SUCCESS;
}