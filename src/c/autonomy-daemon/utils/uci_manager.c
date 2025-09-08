#include "uci_manager.h"
#include "../core/types.h"
#include "../utils/logx.h"
#include <uci.h>
#include <libtlt_uci.h> 
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <libtlt_uci.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Global UCI context
static struct uci_context *g_uci_ctx = NULL;
static bool g_uci_initialized = false; // Use configurable setting // Use configurable setting

// Configuration package name
#define UCI_PACKAGE "autonomy"

// Default configuration values
static const autonomy_config_t DEFAULT_CONFIG = {
    .config_file = "/etc/config/autonomy",
    .daemon_mode = true,
    .debug_mode = false,
    .log_level = 2, // info
    .log_file = "/var/log/autonomy.log",
    .pid_file_timeout = 30,
    
    // Network settings
    .network_check_interval = 30,
    .failover_timeout = 60,
    .auto_failover = true,
    .min_interface_health = 50,
    .mwan3_integration = true,
    
    // GPS settings
    .gps_update_interval = 10,
    .gps_timeout = 30,
    .gps_fusion = true,
    .gps_cache_timeout = 300,
    .min_gps_accuracy = 10.0,
    
    // Starlink settings
    .starlink_check_interval = 30,
    .starlink_health_monitoring = true,
    .starlink_host = "192.168.100.1", // Fallback only
    .starlink_port = 9200,
    .starlink_timeout = 10,
    
    // System monitoring
    .system_check_interval = 60,
    .resource_monitoring = true,
    .service_monitoring = true,
    .alert_threshold = 80,
    
    // Notifications
    .notifications_enabled = true,
    .email_from = "",
    .email_to = "",
    .email_smtp = "",
    .webhook_url = "",
    
    // Snow detection settings
    .snow_detection_enabled = true,
    .snow_detection_samples = 5,
    .snow_obstruction_threshold = 0.05,
    .snow_snr_degradation_threshold = 0.02,
    .snow_temperature_threshold = 2.0,
    .snow_verification_time = 300,
    .snow_melt_timeout = 1800,
    .snow_weather_api_key = ""
};

// Initialize UCI manager using Teltonika's library
int uci_manager_init(void) {
    if (g_uci_initialized) {
        LOGX_WARN_MSG("UCI manager already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    // Use standard OpenWrt UCI initialization
    g_uci_ctx = uci_alloc_context();
    if (!g_uci_ctx) {
        LOGX_ERROR_MSG("Failed to initialize UCI context using standard OpenWrt library");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    g_uci_initialized = true; // Use configurable setting // Use configurable setting
    LOGX_INFO_MSG("UCI manager initialized successfully using standard OpenWrt UCI");
    
    return AUTONOMY_SUCCESS;
}

// Cleanup UCI manager
void uci_manager_cleanup(void) {
    if (!g_uci_initialized) return;
    
    if (g_uci_ctx) {
        uci_cleanup(g_uci_ctx);
        g_uci_ctx = NULL;
    }
    
    g_uci_initialized = false; // Use configurable setting // Use configurable setting
    LOGX_INFO_MSG("UCI manager cleaned up");
}

// Load configuration from UCI using Teltonika library
int uci_manager_load_config(autonomy_config_t *config) {
    if (!g_uci_initialized || !config) {
        LOGX_ERROR_MSG("UCI manager not initialized or invalid config pointer");
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    LOGX_INFO_MSG("Loading configuration from UCI using Teltonika library");
    
    // Start with defaults
    *config = DEFAULT_CONFIG;
    
    // Load daemon settings using ucix_get_option functions
    char *value;
    
    // Daemon mode
    config->daemon_mode = ucix_get_option_int(g_uci_ctx, UCI_PACKAGE, "general", "daemon_mode", 1) != 0;
    
    // Debug mode
    config->debug_mode = ucix_get_option_int(g_uci_ctx, UCI_PACKAGE, "general", "debug_mode", 0) != 0;
    
    // Log level
    config->log_level = ucix_get_option_int(g_uci_ctx, UCI_PACKAGE, "general", "log_level", 2);
    
    // Log file
    value = ucix_get_option(g_uci_ctx, UCI_PACKAGE, "general", "log_file");
    if (value) {
        strncpy(config->log_file, value, sizeof(config->log_file) - 1);
        config->log_file[sizeof(config->log_file) - 1] = '\0';
        free(value);
    }
    
    // PID file timeout
    config->pid_file_timeout = ucix_get_option_int(g_uci_ctx, UCI_PACKAGE, "general", "pid_file_timeout", 30);
    
    // Network settings
    config->network_check_interval = ucix_get_option_int(g_uci_ctx, UCI_PACKAGE, "network", "check_interval", 30);
    config->failover_timeout = ucix_get_option_int(g_uci_ctx, UCI_PACKAGE, "network", "failover_timeout", 60);
    config->auto_failover = ucix_get_option_int(g_uci_ctx, UCI_PACKAGE, "network", "auto_failover", 1) != 0;
    config->min_interface_health = ucix_get_option_int(g_uci_ctx, UCI_PACKAGE, "network", "min_interface_health", 50);
    config->mwan3_integration = ucix_get_option_int(g_uci_ctx, UCI_PACKAGE, "network", "mwan3_integration", 1) != 0;
    
    // GPS settings
    config->gps_update_interval = ucix_get_option_int(g_uci_ctx, UCI_PACKAGE, "gps", "update_interval", 10);
    config->gps_timeout = ucix_get_option_int(g_uci_ctx, UCI_PACKAGE, "gps", "timeout", 30);
    config->gps_fusion = ucix_get_option_int(g_uci_ctx, UCI_PACKAGE, "gps", "fusion", 1) != 0;
    config->gps_cache_timeout = ucix_get_option_int(g_uci_ctx, UCI_PACKAGE, "gps", "cache_timeout", 300);
    
    // Min GPS accuracy (double value)
    value = ucix_get_option(g_uci_ctx, UCI_PACKAGE, "gps", "min_accuracy");
    if (value) {
        config->min_gps_accuracy = atof(value);
        free(value);
    }
    
    // Starlink settings
    config->starlink_check_interval = ucix_get_option_int(g_uci_ctx, UCI_PACKAGE, "starlink", "check_interval", 30);
    config->starlink_health_monitoring = ucix_get_option_int(g_uci_ctx, UCI_PACKAGE, "starlink", "health_monitoring", 1) != 0;
    config->starlink_port = ucix_get_option_int(g_uci_ctx, UCI_PACKAGE, "starlink", "port", 9200);
    config->starlink_timeout = ucix_get_option_int(g_uci_ctx, UCI_PACKAGE, "starlink", "timeout", 10);
    
    // Starlink host
    value = ucix_get_option(g_uci_ctx, UCI_PACKAGE, "starlink", "host");
    if (value) {
        strncpy(config->starlink_host, value, sizeof(config->starlink_host) - 1);
        config->starlink_host[sizeof(config->starlink_host) - 1] = '\0';
        free(value);
    }
    
    // System monitoring
    config->system_check_interval = ucix_get_option_int(g_uci_ctx, UCI_PACKAGE, "system", "check_interval", 60);
    config->resource_monitoring = ucix_get_option_int(g_uci_ctx, UCI_PACKAGE, "system", "resource_monitoring", 1) != 0;
    config->service_monitoring = ucix_get_option_int(g_uci_ctx, UCI_PACKAGE, "system", "service_monitoring", 1) != 0;
    config->alert_threshold = ucix_get_option_int(g_uci_ctx, UCI_PACKAGE, "system", "alert_threshold", 80);
    
    // Notifications
    config->notifications_enabled = ucix_get_option_int(g_uci_ctx, UCI_PACKAGE, "notifications", "enabled", 1) != 0;
    
    // Email settings
    value = ucix_get_option(g_uci_ctx, UCI_PACKAGE, "notifications", "email_from");
    if (value) {
        strncpy(config->email_from, value, sizeof(config->email_from) - 1);
        config->email_from[sizeof(config->email_from) - 1] = '\0';
        free(value);
    }
    
    value = ucix_get_option(g_uci_ctx, UCI_PACKAGE, "notifications", "email_to");
    if (value) {
        strncpy(config->email_to, value, sizeof(config->email_to) - 1);
        config->email_to[sizeof(config->email_to) - 1] = '\0';
        free(value);
    }
    
    value = ucix_get_option(g_uci_ctx, UCI_PACKAGE, "notifications", "email_smtp");
    if (value) {
        strncpy(config->email_smtp, value, sizeof(config->email_smtp) - 1);
        config->email_smtp[sizeof(config->email_smtp) - 1] = '\0';
        free(value);
    }
    
    value = ucix_get_option(g_uci_ctx, UCI_PACKAGE, "notifications", "webhook_url");
    if (value) {
        strncpy(config->webhook_url, value, sizeof(config->webhook_url) - 1);
        config->webhook_url[sizeof(config->webhook_url) - 1] = '\0';
        free(value);
    }
    
    // Snow detection settings
    config->snow_detection_enabled = ucix_get_option_int(g_uci_ctx, UCI_PACKAGE, "snow_detection", "enabled", 1) != 0;
    config->snow_detection_samples = ucix_get_option_int(g_uci_ctx, UCI_PACKAGE, "snow_detection", "detection_samples", 5);
    config->snow_verification_time = ucix_get_option_int(g_uci_ctx, UCI_PACKAGE, "snow_detection", "verification_time", 300);
    config->snow_melt_timeout = ucix_get_option_int(g_uci_ctx, UCI_PACKAGE, "snow_detection", "melt_timeout", 1800);
    
    // Snow detection thresholds (double values)
    value = ucix_get_option(g_uci_ctx, UCI_PACKAGE, "snow_detection", "obstruction_threshold");
    if (value) {
        config->snow_obstruction_threshold = atof(value);
        free(value);
    }
    
    value = ucix_get_option(g_uci_ctx, UCI_PACKAGE, "snow_detection", "snr_degradation_threshold");
    if (value) {
        config->snow_snr_degradation_threshold = atof(value);
        free(value);
    }
    
    value = ucix_get_option(g_uci_ctx, UCI_PACKAGE, "snow_detection", "temperature_threshold");
    if (value) {
        config->snow_temperature_threshold = atof(value);
        free(value);
    }
    
    // Weather API key
    value = ucix_get_option(g_uci_ctx, UCI_PACKAGE, "snow_detection", "weather_api_key");
    if (value) {
        strncpy(config->snow_weather_api_key, value, sizeof(config->snow_weather_api_key) - 1);
        config->snow_weather_api_key[sizeof(config->snow_weather_api_key) - 1] = '\0';
        free(value);
    }
    
    LOGX_INFO_MSG("Configuration loaded successfully from UCI using Teltonika library");
    return AUTONOMY_SUCCESS;
}

// Save configuration to UCI using Teltonika library
int uci_manager_save_config(const autonomy_config_t *config) {
    if (!g_uci_initialized || !config) {
        LOGX_ERROR_MSG("UCI manager not initialized or invalid config pointer");
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    LOGX_INFO_MSG("Saving configuration to UCI using Teltonika library");
    
    int ret;
    
    // Save daemon settings using ucix_add_option functions
    ret = ucix_add_option_int(g_uci_ctx, UCI_PACKAGE, "general", "daemon_mode", config->daemon_mode ? 1 : 0);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save daemon_mode to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    ret = ucix_add_option_int(g_uci_ctx, UCI_PACKAGE, "general", "debug_mode", config->debug_mode ? 1 : 0);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save debug_mode to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    ret = ucix_add_option_int(g_uci_ctx, UCI_PACKAGE, "general", "log_level", config->log_level);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save log_level to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    ret = ucix_add_option(g_uci_ctx, UCI_PACKAGE, "general", "log_file", config->log_file);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save log_file to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    ret = ucix_add_option_int(g_uci_ctx, UCI_PACKAGE, "general", "pid_file_timeout", config->pid_file_timeout);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save pid_file_timeout to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Save network settings
    ret = ucix_add_option_int(g_uci_ctx, UCI_PACKAGE, "network", "check_interval", config->network_check_interval);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save network check_interval to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    ret = ucix_add_option_int(g_uci_ctx, UCI_PACKAGE, "network", "failover_timeout", config->failover_timeout);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save failover_timeout to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    ret = ucix_add_option_int(g_uci_ctx, UCI_PACKAGE, "network", "auto_failover", config->auto_failover ? 1 : 0);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save auto_failover to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    ret = ucix_add_option_int(g_uci_ctx, UCI_PACKAGE, "network", "min_interface_health", config->min_interface_health);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save min_interface_health to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    ret = ucix_add_option_int(g_uci_ctx, UCI_PACKAGE, "network", "mwan3_integration", config->mwan3_integration ? 1 : 0);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save mwan3_integration to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Save GPS settings
    ret = ucix_add_option_int(g_uci_ctx, UCI_PACKAGE, "gps", "update_interval", config->gps_update_interval);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save GPS update_interval to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    ret = ucix_add_option_int(g_uci_ctx, UCI_PACKAGE, "gps", "timeout", config->gps_timeout);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save GPS timeout to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    ret = ucix_add_option_int(g_uci_ctx, UCI_PACKAGE, "gps", "fusion", config->gps_fusion ? 1 : 0);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save GPS fusion to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    ret = ucix_add_option_int(g_uci_ctx, UCI_PACKAGE, "gps", "cache_timeout", config->gps_cache_timeout);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save GPS cache_timeout to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Save GPS accuracy as string
    char accuracy_str[32];
    snprintf(accuracy_str, sizeof(accuracy_str), "%.2f", config->min_gps_accuracy);
    ret = ucix_add_option(g_uci_ctx, UCI_PACKAGE, "gps", "min_accuracy", accuracy_str);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save GPS min_accuracy to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Save Starlink settings
    ret = ucix_add_option_int(g_uci_ctx, UCI_PACKAGE, "starlink", "check_interval", config->starlink_check_interval);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save Starlink check_interval to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    ret = ucix_add_option_int(g_uci_ctx, UCI_PACKAGE, "starlink", "health_monitoring", config->starlink_health_monitoring ? 1 : 0);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save Starlink health_monitoring to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    ret = ucix_add_option(g_uci_ctx, UCI_PACKAGE, "starlink", "host", config->starlink_host);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save Starlink host to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    ret = ucix_add_option_int(g_uci_ctx, UCI_PACKAGE, "starlink", "port", config->starlink_port);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save Starlink port to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    ret = ucix_add_option_int(g_uci_ctx, UCI_PACKAGE, "starlink", "timeout", config->starlink_timeout);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save Starlink timeout to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Save system monitoring settings
    ret = ucix_add_option_int(g_uci_ctx, UCI_PACKAGE, "system", "check_interval", config->system_check_interval);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save system check_interval to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    ret = ucix_add_option_int(g_uci_ctx, UCI_PACKAGE, "system", "resource_monitoring", config->resource_monitoring ? 1 : 0);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save resource_monitoring to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    ret = ucix_add_option_int(g_uci_ctx, UCI_PACKAGE, "system", "service_monitoring", config->service_monitoring ? 1 : 0);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save service_monitoring to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    ret = ucix_add_option_int(g_uci_ctx, UCI_PACKAGE, "system", "alert_threshold", config->alert_threshold);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save alert_threshold to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Save notification settings
    ret = ucix_add_option_int(g_uci_ctx, UCI_PACKAGE, "notifications", "enabled", config->notifications_enabled ? 1 : 0);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save notifications enabled to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    ret = ucix_add_option(g_uci_ctx, UCI_PACKAGE, "notifications", "email_from", config->email_from);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save email_from to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    ret = ucix_add_option(g_uci_ctx, UCI_PACKAGE, "notifications", "email_to", config->email_to);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save email_to to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    ret = ucix_add_option(g_uci_ctx, UCI_PACKAGE, "notifications", "email_smtp", config->email_smtp);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save email_smtp to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    ret = ucix_add_option(g_uci_ctx, UCI_PACKAGE, "notifications", "webhook_url", config->webhook_url);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save webhook_url to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Save snow detection settings
    ret = ucix_add_option_int(g_uci_ctx, UCI_PACKAGE, "snow_detection", "enabled", config->snow_detection_enabled ? 1 : 0);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save snow_detection enabled to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    ret = ucix_add_option_int(g_uci_ctx, UCI_PACKAGE, "snow_detection", "detection_samples", config->snow_detection_samples);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save snow_detection_samples to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    ret = ucix_add_option_int(g_uci_ctx, UCI_PACKAGE, "snow_detection", "verification_time", config->snow_verification_time);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save snow_verification_time to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    ret = ucix_add_option_int(g_uci_ctx, UCI_PACKAGE, "snow_detection", "melt_timeout", config->snow_melt_timeout);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save snow_melt_timeout to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Save snow detection thresholds as strings
    char obstruction_str[32];
    snprintf(obstruction_str, sizeof(obstruction_str), "%.3f", config->snow_obstruction_threshold);
    ret = ucix_add_option(g_uci_ctx, UCI_PACKAGE, "snow_detection", "obstruction_threshold", obstruction_str);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save snow_obstruction_threshold to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    char snr_str[32];
    snprintf(snr_str, sizeof(snr_str), "%.3f", config->snow_snr_degradation_threshold);
    ret = ucix_add_option(g_uci_ctx, UCI_PACKAGE, "snow_detection", "snr_degradation_threshold", snr_str);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save snow_snr_degradation_threshold to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    char temp_str[32];
    snprintf(temp_str, sizeof(temp_str), "%.1f", config->snow_temperature_threshold);
    ret = ucix_add_option(g_uci_ctx, UCI_PACKAGE, "snow_detection", "temperature_threshold", temp_str);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save snow_temperature_threshold to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    ret = ucix_add_option(g_uci_ctx, UCI_PACKAGE, "snow_detection", "weather_api_key", config->snow_weather_api_key);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to save snow_weather_api_key to UCI");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Commit all changes using Teltonika's logged commit (includes logging)
    ret = ucix_logged_commit(g_uci_ctx, UCI_PACKAGE);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to commit UCI changes");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    LOGX_INFO_MSG("Configuration saved successfully to UCI using Teltonika library");
    return AUTONOMY_SUCCESS;
}

// Validate configuration
int uci_manager_validate_config(const autonomy_config_t *config) {
    if (!config) {
        LOGX_ERROR_MSG("Invalid config pointer");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Validate intervals are positive
    if (config->network_check_interval <= 0 || config->gps_update_interval <= 0 || 
        config->starlink_check_interval <= 0 || config->system_check_interval <= 0) {
        LOGX_ERROR_MSG("Check intervals must be positive");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Validate timeouts are positive
    if (config->failover_timeout <= 0 || config->gps_timeout <= 0 || config->starlink_timeout <= 0) {
        LOGX_ERROR_MSG("Timeouts must be positive");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Validate thresholds are in valid ranges
    if (config->min_interface_health < 0 || config->min_interface_health > 100) {
        LOGX_ERROR_MSG("Interface health threshold must be 0-100");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (config->alert_threshold < 0 || config->alert_threshold > 100) {
        LOGX_ERROR_MSG("Alert threshold must be 0-100");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Validate GPS accuracy is positive
    if (config->min_gps_accuracy <= 0.0) {
        LOGX_ERROR_MSG("GPS accuracy must be positive");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Validate Starlink port is in valid range
    if (config->starlink_port < 1 || config->starlink_port > 65535) {
        LOGX_ERROR_MSG("Starlink port must be 1-65535");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    LOGX_DEBUG_MSG("Configuration validation passed");
    return AUTONOMY_SUCCESS;
}

// Get default configuration
const autonomy_config_t* uci_manager_get_default_config(void) {
    return &DEFAULT_CONFIG;
}

// Check if UCI manager is available
bool uci_manager_is_available(void) {
    return g_uci_initialized && g_uci_ctx != NULL;
}

// Convert autonomy config to snow detection config
void uci_manager_convert_to_snow_config(const autonomy_config_t *autonomy_config, 
                                       starlink_snow_detection_config_t *snow_config) {
    if (!autonomy_config || !snow_config) {
        return;
    }
    
    snow_config->enabled = autonomy_config->snow_detection_enabled;
    snow_config->detection_samples = autonomy_config->snow_detection_samples;
    snow_config->obstruction_threshold = autonomy_config->snow_obstruction_threshold;
    snow_config->snr_degradation_threshold = autonomy_config->snow_snr_degradation_threshold;
    snow_config->temperature_threshold = autonomy_config->snow_temperature_threshold;
    snow_config->verification_time = autonomy_config->snow_verification_time;
    snow_config->melt_timeout = autonomy_config->snow_melt_timeout;
    strncpy(snow_config->weather_api_key, autonomy_config->snow_weather_api_key, 
            sizeof(snow_config->weather_api_key) - 1);
    snow_config->weather_api_key[sizeof(snow_config->weather_api_key) - 1] = '\0';
}

// Convert snow detection config to autonomy config
void uci_manager_convert_from_snow_config(const starlink_snow_detection_config_t *snow_config,
                                         autonomy_config_t *autonomy_config) {
    if (!snow_config || !autonomy_config) {
        return;
    }
    
    autonomy_config->snow_detection_enabled = snow_config->enabled;
    autonomy_config->snow_detection_samples = snow_config->detection_samples;
    autonomy_config->snow_obstruction_threshold = snow_config->obstruction_threshold;
    autonomy_config->snow_snr_degradation_threshold = snow_config->snr_degradation_threshold;
    autonomy_config->snow_temperature_threshold = snow_config->temperature_threshold;
    autonomy_config->snow_verification_time = snow_config->verification_time;
    autonomy_config->snow_melt_timeout = snow_config->melt_timeout;
    strncpy(autonomy_config->snow_weather_api_key, snow_config->weather_api_key,
            sizeof(autonomy_config->snow_weather_api_key) - 1);
    autonomy_config->snow_weather_api_key[sizeof(autonomy_config->snow_weather_api_key) - 1] = '\0';
}