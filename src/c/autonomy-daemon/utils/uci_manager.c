#include "uci_manager.h"
#include <time.h>
#include "../utils/logx.h"
#include <uci.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <math.h>

// Global UCI context
static struct uci_context *g_uci_ctx = NULL;
static bool g_uci_initialized = false;

// Default configuration values
static const autonomy_config_t DEFAULT_CONFIG = {
    .config_file = "/etc/config/autonomy",
    .daemon_mode = true,
    .debug_mode = false,
    .log_level = LOGX_LEVEL_INFO,
    .log_file = "/var/log/autonomy.log",
    .pid_file_timeout = 30,
    .network_check_interval = 30,
    .failover_timeout = 60,
    .auto_failover = true,
    .min_interface_health = 70,
    .mwan3_integration = true,
    .gps_update_interval = 60,
    .gps_timeout = 30,
    .gps_fusion = true,
    .gps_cache_timeout = 300,
    .min_gps_accuracy = 10.0f,
    .starlink_host = "192.168.100.1",
    .starlink_port = 9000,
    .starlink_timeout = 10,
    .starlink_check_interval = 60,
    .starlink_health_monitoring = true,
    .system_check_interval = 300,
    .resource_monitoring = true,
    .service_monitoring = true,
    .alert_threshold = 80,
    .notifications_enabled = true,
    .webhook_url = "",
    .email_smtp = "",
    .email_from = "",
    .email_to = ""
};

// Initialize UCI system
int uci_manager_init(void) {
    if (g_uci_initialized) {
        LOGX_WARN_MSG("UCI manager already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    // Create UCI context
    g_uci_ctx = uci_alloc_context();
    if (!g_uci_ctx) {
        LOGX_ERROR_MSG("Failed to allocate UCI context");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Set UCI search path
    uci_set_confdir(g_uci_ctx, "/etc/config");
    
    g_uci_initialized = true;
    LOGX_INFO_MSG("UCI manager initialized successfully");
    
    return AUTONOMY_SUCCESS;
}

// Load configuration from UCI
static int uci_manager_load_config(autonomy_config_t *config) {
    if (!g_uci_initialized || !config) {
        LOGX_ERROR_MSG("UCI manager not initialized or invalid config pointer");
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    // Start with default configuration
    memcpy(config, &DEFAULT_CONFIG, sizeof(autonomy_config_t));
    
    // Load UCI configuration
    struct uci_package *pkg = NULL;
    int ret = uci_load(g_uci_ctx, "autonomy", &pkg);
    if (ret != UCI_OK) {
        LOGX_WARN_MSG("Failed to load UCI package 'autonomy', using defaults");
        return AUTONOMY_SUCCESS; // Return success with defaults
    }
    
    // Parse general section
    struct uci_section *general = uci_lookup_section(g_uci_ctx, pkg, "general");
    if (general) {
        const char *value;
        
        value = uci_lookup_option_string(g_uci_ctx, general, "daemon_mode");
        if (value) {
            config->daemon_mode = (strcmp(value, "1") == 0 || strcmp(value, "true") == 0);
        }
        
        value = uci_lookup_option_string(g_uci_ctx, general, "debug_mode");
        if (value) {
            config->debug_mode = (strcmp(value, "1") == 0 || strcmp(value, "true") == 0);
        }
        
        value = uci_lookup_option_string(g_uci_ctx, general, "log_level");
        if (value) {
            if (strcmp(value, "trace") == 0) config->log_level = LOGX_LEVEL_TRACE;
            else if (strcmp(value, "debug") == 0) config->log_level = LOGX_LEVEL_DEBUG;
            else if (strcmp(value, "info") == 0) config->log_level = LOGX_LEVEL_INFO;
            else if (strcmp(value, "warn") == 0) config->log_level = LOGX_LEVEL_WARN;
            else if (strcmp(value, "error") == 0) config->log_level = LOGX_LEVEL_ERROR;
            else if (strcmp(value, "fatal") == 0) config->log_level = LOGX_LEVEL_FATAL;
        }
        
        value = uci_lookup_option_string(g_uci_ctx, general, "log_file");
        if (value) {
            strncpy(config->log_file, value, sizeof(config->log_file) - 1);
            config->log_file[sizeof(config->log_file) - 1] = '\0';
        }
        
        value = uci_lookup_option_string(g_uci_ctx, general, "pid_file_timeout");
        if (value) {
            config->pid_file_timeout = atoi(value);
        }
    }
    
    // Parse network section
    struct uci_section *network = uci_lookup_section(g_uci_ctx, pkg, "network");
    if (network) {
        const char *value;
        
        value = uci_lookup_option_string(g_uci_ctx, network, "check_interval");
        if (value) {
            config->network_check_interval = atoi(value);
        }
        
        value = uci_lookup_option_string(g_uci_ctx, network, "failover_timeout");
        if (value) {
            config->failover_timeout = atoi(value);
        }
        
        value = uci_lookup_option_string(g_uci_ctx, network, "auto_failover");
        if (value) {
            config->auto_failover = (strcmp(value, "1") == 0 || strcmp(value, "true") == 0);
        }
        
        value = uci_lookup_option_string(g_uci_ctx, network, "min_interface_health");
        if (value) {
            config->min_interface_health = atoi(value);
        }
        
        value = uci_lookup_option_string(g_uci_ctx, network, "mwan3_integration");
        if (value) {
            config->mwan3_integration = (strcmp(value, "1") == 0 || strcmp(value, "true") == 0);
        }
    }
    
    // Parse GPS section
    struct uci_section *gps = uci_lookup_section(g_uci_ctx, pkg, "gps");
    if (gps) {
        const char *value;
        
        value = uci_lookup_option_string(g_uci_ctx, gps, "update_interval");
        if (value) {
            config->gps_update_interval = atoi(value);
        }
        
        value = uci_lookup_option_string(g_uci_ctx, gps, "timeout");
        if (value) {
            config->gps_timeout = atoi(value);
        }
        
        value = uci_lookup_option_string(g_uci_ctx, gps, "fusion");
        if (value) {
            config->gps_fusion = (strcmp(value, "1") == 0 || strcmp(value, "true") == 0);
        }
        
        value = uci_lookup_option_string(g_uci_ctx, gps, "cache_timeout");
        if (value) {
            config->gps_cache_timeout = atoi(value);
        }
        
        value = uci_lookup_option_string(g_uci_ctx, gps, "min_accuracy");
        if (value) {
            config->min_gps_accuracy = atof(value);
        }
    }
    
    // Parse Starlink section
    struct uci_section *starlink = uci_lookup_section(g_uci_ctx, pkg, "starlink");
    if (starlink) {
        const char *value;
        
        value = uci_lookup_option_string(g_uci_ctx, starlink, "host");
        if (value) {
            strncpy(config->starlink_host, value, sizeof(config->starlink_host) - 1);
            config->starlink_host[sizeof(config->starlink_host) - 1] = '\0';
        }
        
        value = uci_lookup_option_string(g_uci_ctx, starlink, "port");
        if (value) {
            config->starlink_port = atoi(value);
        }
        
        value = uci_lookup_option_string(g_uci_ctx, starlink, "timeout");
        if (value) {
            config->starlink_timeout = atoi(value);
        }
        
        value = uci_lookup_option_string(g_uci_ctx, starlink, "check_interval");
        if (value) {
            config->starlink_check_interval = atoi(value);
        }
        
        value = uci_lookup_option_string(g_uci_ctx, starlink, "health_monitoring");
        if (value) {
            config->starlink_health_monitoring = (strcmp(value, "1") == 0 || strcmp(value, "true") == 0);
        }
    }
    
    // Parse system section
    struct uci_section *system = uci_lookup_section(g_uci_ctx, pkg, "system");
    if (system) {
        const char *value;
        
        value = uci_lookup_option_string(g_uci_ctx, system, "check_interval");
        if (value) {
            config->system_check_interval = atoi(value);
        }
        
        value = uci_lookup_option_string(g_uci_ctx, system, "resource_monitoring");
        if (value) {
            config->resource_monitoring = (strcmp(value, "1") == 0 || strcmp(value, "true") == 0);
        }
        
        value = uci_lookup_option_string(g_uci_ctx, system, "service_monitoring");
        if (value) {
            config->service_monitoring = (strcmp(value, "1") == 0 || strcmp(value, "true") == 0);
        }
        
        value = uci_lookup_option_string(g_uci_ctx, system, "alert_threshold");
        if (value) {
            config->alert_threshold = atoi(value);
        }
    }
    
    // Parse notifications section
    struct uci_section *notifications = uci_lookup_section(g_uci_ctx, pkg, "notifications");
    if (notifications) {
        const char *value;
        
        value = uci_lookup_option_string(g_uci_ctx, notifications, "enabled");
        if (value) {
            config->notifications_enabled = (strcmp(value, "1") == 0 || strcmp(value, "true") == 0);
        }
        
        value = uci_lookup_option_string(g_uci_ctx, notifications, "webhook_url");
        if (value) {
            strncpy(config->webhook_url, value, sizeof(config->webhook_url) - 1);
            config->webhook_url[sizeof(config->webhook_url) - 1] = '\0';
        }
        
        value = uci_lookup_option_string(g_uci_ctx, notifications, "email_smtp");
        if (value) {
            strncpy(config->email_smtp, value, sizeof(config->email_smtp) - 1);
            config->email_smtp[sizeof(config->email_smtp) - 1] = '\0';
        }
        
        value = uci_lookup_option_string(g_uci_ctx, notifications, "email_from");
        if (value) {
            strncpy(config->email_from, value, sizeof(config->email_from) - 1);
            config->email_from[sizeof(config->email_from) - 1] = '\0';
        }
        
        value = uci_lookup_option_string(g_uci_ctx, notifications, "email_to");
        if (value) {
            strncpy(config->email_to, value, sizeof(config->email_to) - 1);
            config->email_to[sizeof(config->email_to) - 1] = '\0';
        }
    }
    
    // Unload package
    uci_unload(g_uci_ctx, pkg);
    
    LOGX_INFO_MSG("Configuration loaded successfully from UCI");
    return AUTONOMY_SUCCESS;
}

// Save configuration to UCI
static int uci_manager_save_config(const autonomy_config_t *config) {
    if (!g_uci_initialized || !config) {
        LOGX_ERROR_MSG("UCI manager not initialized or invalid config pointer");
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    // Create new package
    struct uci_package *pkg = NULL;
    int ret = uci_new_package(g_uci_ctx, "autonomy", &pkg);
    if (ret != UCI_OK) {
        LOGX_ERROR_MSG("Failed to create new UCI package");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Create general section
    struct uci_section *general = uci_add_section(g_uci_ctx, pkg, "general", "general");
    if (general) {
        uci_set(g_uci_ctx, general, "daemon_mode", config->daemon_mode ? "1" : "0");
        uci_set(g_uci_ctx, general);
        uci_set(g_uci_ctx, general, "log_level", get_log_level_string(config->log_level));
        uci_set(g_uci_ctx, general);
        uci_set(g_uci_ctx, general, "pid_file_timeout", int_to_string(config->pid_file_timeout));
    }
    
    // Create network section
    struct uci_section *network = uci_add_section(g_uci_ctx, pkg);
    if (network) {
        uci_set(g_uci_ctx, network, "check_interval", int_to_string(config->network_check_interval));
        uci_set(g_uci_ctx, network));
        uci_set(g_uci_ctx, network, "auto_failover", config->auto_failover ? "1" : "0");
        uci_set(g_uci_ctx, network));
        uci_set(g_uci_ctx, network, "mwan3_integration", config->mwan3_integration ? "1" : "0");
    }
    
    // Create GPS section
    struct uci_section *gps = uci_add_section(g_uci_ctx, pkg);
    if (gps) {
        uci_set(g_uci_ctx, gps, "update_interval", int_to_string(config->gps_update_interval));
        uci_set(g_uci_ctx, gps));
        uci_set(g_uci_ctx, gps, "fusion", config->gps_fusion ? "1" : "0");
        uci_set(g_uci_ctx, gps));
        uci_set(g_uci_ctx, gps, "min_accuracy", float_to_string(config->min_gps_accuracy));
    }
    
    // Create Starlink section
    struct uci_section *starlink = uci_add_section(g_uci_ctx, pkg);
    if (starlink) {
        uci_set(g_uci_ctx, starlink, "host", config->starlink_host);
        uci_set(g_uci_ctx, starlink));
        uci_set(g_uci_ctx, starlink, "timeout", int_to_string(config->starlink_timeout));
        uci_set(g_uci_ctx, starlink));
        uci_set(g_uci_ctx, starlink, "health_monitoring", config->starlink_health_monitoring ? "1" : "0");
    }
    
    // Create system section
    struct uci_section *system = uci_add_section(g_uci_ctx, pkg);
    if (system) {
        uci_set(g_uci_ctx, system, "check_interval", int_to_string(config->system_check_interval));
        uci_set(g_uci_ctx, system);
        uci_set(g_uci_ctx, system, "service_monitoring", config->service_monitoring ? "1" : "0");
        uci_set(g_uci_ctx, system));
    }
    
    // Create notifications section
    struct uci_section *notifications = uci_add_section(g_uci_ctx, pkg, "notifications", "notifications");
    if (notifications) {
        uci_set(g_uci_ctx, notifications, "enabled", config->notifications_enabled ? "1" : "0");
        uci_set(g_uci_ctx, notifications);
        uci_set(g_uci_ctx, notifications, "email_smtp", config->email_smtp);
        uci_set(g_uci_ctx, notifications);
        uci_set(g_uci_ctx, notifications, "email_to", config->email_to);
    }
    
    // Save package
    ret = uci_save(g_uci_ctx);
    if (ret != UCI_OK) {
        LOGX_ERROR_MSG("Failed to save UCI package");
        uci_unload(g_uci_ctx);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Commit changes
    ret = uci_commit(g_uci_ctx, &pkg, false);
    if (ret != UCI_OK) {
        LOGX_ERROR_MSG("Failed to commit UCI changes");
        uci_unload(g_uci_ctx, pkg);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    uci_unload(g_uci_ctx, pkg);
    
    LOGX_INFO_MSG("Configuration saved successfully to UCI");
    return AUTONOMY_SUCCESS;
}

// Helper functions
static const char* get_log_level_string(logx_level_t level) {
    switch (level) {
        case LOGX_LEVEL_TRACE: return "trace";
        case LOGX_LEVEL_DEBUG: return "debug";
        case LOGX_LEVEL_INFO: return "info";
        case LOGX_LEVEL_WARN: return "warn";
        case LOGX_LEVEL_ERROR: return "error";
        case LOGX_LEVEL_FATAL: return "fatal";
        default: return "info";
    }
}

static char* int_to_string(int value) {
    static char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", value);
    return buffer;
}

static char* float_to_string(float value) {
    static char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.2f", value);
    return buffer;
}

// Validate configuration
static int uci_manager_validate_config(const autonomy_config_t *config) {
    if (!config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Validate timeouts
    if (config->network_check_interval < 5 || config->network_check_interval > 3600) {
        LOGX_WARN_MSG("Invalid network_check_interval: %d, using default", config->network_check_interval);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (config->failover_timeout < 10 || config->failover_timeout > 300) {
        LOGX_WARN_MSG("Invalid failover_timeout: %d, using default", config->failover_timeout);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (config->gps_update_interval < 10 || config->gps_update_interval > 3600) {
        LOGX_WARN_MSG("Invalid gps_update_interval: %d, using default", config->gps_update_interval);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (config->starlink_check_interval < 10 || config->starlink_check_interval > 3600) {
        LOGX_WARN_MSG("Invalid starlink_check_interval: %d, using default", config->starlink_check_interval);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (config->system_check_interval < 60 || config->system_check_interval > 3600) {
        LOGX_WARN_MSG("Invalid system_check_interval: %d, using default", config->system_check_interval);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Validate thresholds
    if (config->min_interface_health < 0 || config->min_interface_health > 100) {
        LOGX_WARN_MSG("Invalid min_interface_health: %d, using default", config->min_interface_health);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (config->alert_threshold < 0 || config->alert_threshold > 100) {
        LOGX_WARN_MSG("Invalid alert_threshold: %d, using default", config->alert_threshold);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (config->min_gps_accuracy < 0.1f || config->min_gps_accuracy > 1000.0f) {
        LOGX_WARN_MSG("Invalid min_gps_accuracy: %.2f, using default", config->min_gps_accuracy);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    return AUTONOMY_SUCCESS;
}

// Get default configuration
const autonomy_config_t* uci_manager_get_default_config(void) {
    return &DEFAULT_CONFIG;
}

// Check if UCI is available
static bool uci_manager_is_available(void) {
    return g_uci_initialized && g_uci_ctx != NULL;
}

// Cleanup UCI system
void uci_manager_cleanup(void) {
    if (g_uci_ctx) {
        uci_free_context(g_uci_ctx);
        g_uci_ctx = NULL;
    }
    g_uci_initialized = false;
    LOGX_INFO_MSG("UCI manager cleaned up");
}
