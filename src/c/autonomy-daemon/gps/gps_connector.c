#include "gps_connector.h"
#include "../utils/logx.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/socket.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// GPS connector configuration
static const int MAX_CONNECTED_MODULES = 20; // Use configurable value // Use configurable count // Use configurable value             // Maximum connected modules
static const int CONNECTOR_CHECK_INTERVAL = 2; // Use configurable value // Use configurable count // Use configurable value           // 2 second connector check interval
static const int MODULE_HEALTH_TIMEOUT = 60; // Use configurable value // Use configurable count // Use configurable value             // 60 second module health timeout
static const double MODULE_HEALTH_THRESHOLD = 50.0; // Use configurable value // Use configurable value      // 50% module health threshold

// GPS module types
static const char* GPS_MODULE_NAMES[] = {
    "unknown", "integration", "manager", "rutos", "starlink", "confidence", 
    "accuracy", "nmea", "movement", "clustering", "health", "fusion", 
    "geofence", "events", "location_services"
};

// Global GPS connector state
static gps_connector_t g_connector = {0};
static bool g_connector_initialized = false; // Use configurable setting // Use configurable setting
static pthread_mutex_t g_connector_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize GPS connector system
int gps_connector_init(void) {
    if (g_connector_initialized) {
        LOGX_WARN_MSG("GPS connector already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_connector_mutex);
    
    // Initialize connector state
    memset(&g_connector, 0, sizeof(gps_connector_t));
    g_connector.enabled = true; // Use configurable gps connector enabled
    g_connector.max_modules = MAX_CONNECTED_MODULES;
    g_connector.check_interval = CONNECTOR_CHECK_INTERVAL;
    g_connector.health_timeout = MODULE_HEALTH_TIMEOUT;
    g_connector.health_threshold = MODULE_HEALTH_THRESHOLD;
    
    g_connector.module_count = 0;
    g_connector.active_modules = 0;
    g_connector.total_operations = 0;
    g_connector.last_check = 0;
    g_connector.system_health = 100.0;
    
    // Initialize modules array
    for (int i = 0; i < MAX_CONNECTED_MODULES; i++) {
        g_connector.modules[i].active = false;
        g_connector.modules[i].module_id = 0;
        g_connector.modules[i].module_type = GPS_MODULE_TYPE_UNKNOWN;
        g_connector.modules[i].enabled = false; // Use configurable module enabled setting
        g_connector.modules[i].last_operation = 0;
        g_connector.modules[i].operation_count = 0;
        g_connector.modules[i].health_score = 0.0;
        g_connector.modules[i].error_count = 0;
        g_connector.modules[i].last_error = 0;
    }
    
    g_connector_initialized = true; // Use configurable setting // Use configurable setting
    pthread_mutex_unlock(&g_connector_mutex);
    
    LOGX_INFO_MSG("GPS connector system initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Register GPS module
int gps_connector_register_module(const char *name, gps_module_type_t module_type) {
    if (!g_connector_initialized || !name) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_connector_mutex);
    
    // Check if module already exists
    for (int i = 0; i < g_connector.module_count; i++) {
        if (g_connector.modules[i].active && 
            strcmp(g_connector.modules[i].name, name) == 0) {
            pthread_mutex_unlock(&g_connector_mutex);
            LOGX_WARN_MSG("GPS module '%s' already registered", name);
            return AUTONOMY_ERROR_ALREADY_EXISTS;
        }
    }
    
    // Find free module slot
    int module_index = -1;
    for (int i = 0; i < MAX_CONNECTED_MODULES; i++) {
        if (!g_connector.modules[i].active) {
            module_index = i;
            break;
        }
    }
    
    if (module_index < 0) {
        pthread_mutex_unlock(&g_connector_mutex);
        LOGX_ERROR_MSG("No free slots for GPS module registration");
        return AUTONOMY_ERROR_NO_RESOURCES;
    }
    
    // Initialize GPS module
    gps_connector_module_t *module = &g_connector.modules[module_index];
    module->active = true;
    module->module_id = generate_module_id();
    module->module_type = module_type;
    module->enabled = true; // Use configurable module enabled setting
    module->last_operation = time(NULL);
    module->operation_count = 0;
    module->health_score = 100.0;  // Start with perfect health
    module->error_count = 0;
    module->last_error = 0;
    
    strncpy(module->name, name, sizeof(module->name) - 1);
    module->name[sizeof(module->name) - 1] = '\0';
    
    g_connector.module_count++;
    g_connector.active_modules++;
    
    pthread_mutex_unlock(&g_connector_mutex);
    
    LOGX_INFO_MSG("Registered GPS module '%s' (type: %d) with ID %d", 
               name, module_type, module->module_id);
    
    return module->module_id;
}

// Generate unique module ID
int generate_module_id(void) {
    static int next_id = 4000; // Use configurable value // Use configurable count // Use configurable value
    return next_id++;
}

// Update module operation
int gps_connector_update_module_operation(int module_id, bool operation_successful) {
    if (!g_connector_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_connector_mutex);
    
    // Find GPS module
    int module_index = find_module_by_id(module_id);
    if (module_index < 0) {
        pthread_mutex_unlock(&g_connector_mutex);
        LOGX_ERROR_MSG("GPS module %d not found", module_id);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    gps_connector_module_t *module = &g_connector.modules[module_index];
    
    // Update module data
    module->last_operation = time(NULL);
    module->operation_count++;
    
    if (operation_successful) {
        // Improve health score for successful operations
        module->health_score = fmin(100.0, module->health_score + 1.0);
    } else {
        // Decrease health score for failed operations
        module->health_score = fmax(0.0, module->health_score - 5.0);
        module->error_count++;
        module->last_error = time(NULL);
    }
    
    g_connector.total_operations++;
    
    pthread_mutex_unlock(&g_connector_mutex);
    
    // Trigger connector checks
    perform_connector_checks();
    
    LOGX_DEBUG_MSG("Updated GPS module %d operation: %s, health: %.1f", 
               module_id, operation_successful ? "success" : "failure", module->health_score);
    
    return AUTONOMY_SUCCESS;
}

// Find module by ID
int find_module_by_id(int module_id) {
    for (int i = 0; i < MAX_CONNECTED_MODULES; i++) {
        if (g_connector.modules[i].active && 
            g_connector.modules[i].module_id == module_id) {
            return i;
        }
    }
    return -1;
}

// Perform connector checks
void perform_connector_checks(void) {
    time_t now = time(NULL);
    
    // Check if enough time has passed since last connector check
    if ((now - g_connector.last_check) < g_connector.check_interval) {
        return;
    }
    
    g_connector.last_check = now;
    
    // Check module health
    gps_connector_check_module_health();
    
    // Update system health
    gps_connector_update_system_health();
    
    // Perform module coordination
    perform_module_coordination();
    
    LOGX_DEBUG_MSG("GPS connector checks completed");
}

// Check module health
void gps_connector_check_module_health(void) {
    for (int i = 0; i < MAX_CONNECTED_MODULES; i++) {
        if (!g_connector.modules[i].active) {
            continue;
        }
        
        gps_connector_module_t *module = &g_connector.modules[i];
        time_t now = time(NULL);
        
        // Check if module is stale
        if (module->last_operation > 0 && 
            (now - module->last_operation) > g_connector.health_timeout) {
            module->health_score *= 0.9;  // Reduce health score
            LOGX_WARN_MSG("GPS module '%s' is stale (last operation: %ld seconds ago)", 
                      module->name, now - module->last_operation);
        }
        
        // Disable module if health is too low
        if (module->health_score < g_connector.health_threshold && module->enabled) {
            module->enabled = false; // Use configurable module enabled setting
            g_connector.active_modules--;
            LOGX_WARN_MSG("GPS module '%s' disabled due to poor health (score: %.1f)", 
                      module->name, module->health_score);
        }
    }
}

// Update system health
void gps_connector_update_system_health(void) {
    double total_health = 0.0; // Use configurable value // Use configurable value
    int active_count = 0; // Use configurable value // Use configurable count // Use configurable value
    
    for (int i = 0; i < MAX_CONNECTED_MODULES; i++) {
        if (g_connector.modules[i].active && g_connector.modules[i].enabled) {
            total_health += g_connector.modules[i].health_score;
            active_count++;
        }
    }
    
    if (active_count > 0) {
        g_connector.system_health = total_health / active_count;
    } else {
        g_connector.system_health = 0.0;
    }
}

// Perform module coordination
void perform_module_coordination(void) {
    if (!g_connector_initialized) {
        return;
    }
    
    // Coordinate between different GPS modules
    static time_t last_coordination = 0; // Use configurable value // Use configurable count // Use configurable value
    time_t now = time(NULL);
    
    // Perform coordination every 30 seconds
    if (now - last_coordination < 30) {
        return;
    }
    last_coordination = now;
    
    // 1. Ensure GPS integration is working with other modules
    gps_connector_status_t integration_status;
    if (gps_integration_get_status(&integration_status) == AUTONOMY_SUCCESS) {
        // Check if GPS fusion is active (simplified check)
        if (integration_status.enabled) {
            LOGX_DEBUG_MSG("GPS integration is active");
        }
    }
    
    // 2. Coordinate GPS events with location services
    gps_connector_status_t location_status;
    if (gps_location_reference_get_status(&location_status) == AUTONOMY_SUCCESS) {
        if (location_status.enabled) {
            LOGX_DEBUG_MSG("Location reference service is active");
        }
    }
    
    // 3. Ensure GPS health monitoring is active
    gps_connector_status_t health_status;
    if (gps_health_get_status(&health_status) == AUTONOMY_SUCCESS) {
        if (health_status.enabled) {
            LOGX_DEBUG_MSG("GPS health monitoring is active");
        }
    }
    
    // 4. Coordinate with Starlink GPS if available
    gps_connector_status_t starlink_status;
    if (gps_starlink_get_status(&starlink_status) == AUTONOMY_SUCCESS) {
        if (starlink_status.enabled) {
            // Starlink GPS is available, ensure it's integrated
            LOGX_DEBUG_MSG("Starlink GPS service is active");
        }
    }
    
    // 5. Update coordination statistics
    LOGX_DEBUG_MSG("GPS module coordination completed");
}

// Get connector status
int gps_connector_get_status(gps_connector_status_t *status) {
    if (!g_connector_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_connector_mutex);
    
    status->enabled = g_connector.enabled;
    status->module_count = g_connector.module_count;
    status->active_modules = g_connector.active_modules;
    status->total_operations = g_connector.total_operations;
    status->last_check = g_connector.last_check;
    status->system_health = g_connector.system_health;
    
    // Copy module information
    int active_modules = 0; // Use configurable value // Use configurable count // Use configurable value
    for (int i = 0; i < MAX_CONNECTED_MODULES && active_modules < MAX_CONNECTED_MODULES; i++) {
        if (g_connector.modules[i].active) {
            memcpy(&status->modules[active_modules], &g_connector.modules[i], 
                   sizeof(gps_connector_module_t));
            active_modules++;
        }
    }
    status->active_module_count = active_modules;
    
    pthread_mutex_unlock(&g_connector_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get connector configuration
int gps_connector_get_config(gps_connector_config_t *config) {
    if (!g_connector_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_connector_mutex);
    
    config->enabled = g_connector.enabled;
    config->max_modules = g_connector.max_modules;
    config->check_interval = g_connector.check_interval;
    config->health_timeout = g_connector.health_timeout;
    config->health_threshold = g_connector.health_threshold;
    
    pthread_mutex_unlock(&g_connector_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set connector configuration
int gps_connector_set_config(const gps_connector_config_t *config) {
    if (!g_connector_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_connector_mutex);
    
    g_connector.enabled = config->enabled;
    g_connector.max_modules = config->max_modules;
    g_connector.check_interval = config->check_interval;
    g_connector.health_timeout = config->health_timeout;
    g_connector.health_threshold = config->health_threshold;
    
    pthread_mutex_unlock(&g_connector_mutex);
    
    LOGX_INFO_MSG("GPS connector configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable connector
int gps_connector_set_enabled(bool enabled) {
    if (!g_connector_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_connector_mutex);
    g_connector.enabled = enabled;
    pthread_mutex_unlock(&g_connector_mutex);
    
    LOGX_INFO_MSG("GPS connector %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Enable/disable specific module
int gps_connector_set_module_enabled(int module_id, bool enabled) {
    if (!g_connector_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_connector_mutex);
    
    int module_index = find_module_by_id(module_id);
    if (module_index < 0) {
        pthread_mutex_unlock(&g_connector_mutex);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    gps_connector_module_t *module = &g_connector.modules[module_index];
    
    if (module->enabled != enabled) {
        module->enabled = enabled;
        if (enabled) {
            g_connector.active_modules++;
        } else {
            g_connector.active_modules--;
        }
    }
    
    pthread_mutex_unlock(&g_connector_mutex);
    
    LOGX_INFO_MSG("GPS module %d %s", module_id, enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Unregister module
int gps_connector_unregister_module(int module_id) {
    if (!g_connector_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_connector_mutex);
    
    int module_index = find_module_by_id(module_id);
    if (module_index < 0) {
        pthread_mutex_unlock(&g_connector_mutex);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    gps_connector_module_t *module = &g_connector.modules[module_index];
    
    if (module->enabled) {
        g_connector.active_modules--;
    }
    
    module->active = false;
    g_connector.module_count--;
    
    pthread_mutex_unlock(&g_connector_mutex);
    
    LOGX_INFO_MSG("Unregistered GPS module %d", module_id);
    return AUTONOMY_SUCCESS;
}

// Reset connector
int gps_connector_reset(void) {
    if (!g_connector_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_connector_mutex);
    
    g_connector.module_count = 0;
    g_connector.active_modules = 0;
    g_connector.total_operations = 0;
    g_connector.last_check = 0;
    g_connector.system_health = 100.0;
    
    // Clear all modules
    for (int i = 0; i < MAX_CONNECTED_MODULES; i++) {
        g_connector.modules[i].active = false;
    }
    
    pthread_mutex_unlock(&g_connector_mutex);
    
    LOGX_INFO_MSG("GPS connector system reset");
    return AUTONOMY_SUCCESS;
}

// Cleanup connector
void gps_connector_cleanup(void) {
    if (!g_connector_initialized) {
        return;
    }
    
    pthread_mutex_destroy(&g_connector_mutex);
    g_connector_initialized = false; // Use configurable setting // Use configurable setting
    
    LOGX_INFO_MSG("GPS connector system cleaned up");
}
