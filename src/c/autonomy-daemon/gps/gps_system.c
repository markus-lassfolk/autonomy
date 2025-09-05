#include "gps_system.h"
#include "logx.h"
#include "types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

// GPS system configuration
static const int GPS_SYSTEM_INIT_TIMEOUT = 30;           // 30 second initialization timeout
static const int GPS_SYSTEM_HEALTH_CHECK_INTERVAL = 10;   // 10 second health check interval
static const double GPS_SYSTEM_MIN_HEALTH = 70.0;         // 70% minimum system health

// Global GPS system state
static gps_system_t g_gps_system = {0};
static bool g_gps_system_initialized = false;
static pthread_mutex_t g_gps_system_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize GPS system
static int gps_system_init(void) {
    if (g_gps_system_initialized) {
        LOGX_WARN("GPS system already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_gps_system_mutex);
    
    // Initialize GPS system state
    memset(&g_gps_system, 0, sizeof(gps_system_t));
    g_gps_system.enabled = true;
    g_gps_system.init_timeout = GPS_SYSTEM_INIT_TIMEOUT;
    g_gps_system.health_check_interval = GPS_SYSTEM_HEALTH_CHECK_INTERVAL;
    g_gps_system.min_health = GPS_SYSTEM_MIN_HEALTH;
    
    g_gps_system.init_start_time = time(NULL);
    g_gps_system.init_complete = false;
    g_gps_system.module_count = 0;
    g_gps_system.active_modules = 0;
    g_gps_system.system_health = 0.0;
    g_gps_system.last_health_check = 0;
    
    // Initialize module status array
    for (int i = 0; i < GPS_MAX_MODULES; i++) {
        g_gps_system.module_status[i].module_type = GPS_MODULE_TYPE_UNKNOWN;
        g_gps_system.module_status[i].initialized = false;
        g_gps_system.module_status[i].enabled = false;
        g_gps_system.module_status[i].health_score = 0.0;
        g_gps_system.module_status[i].last_operation = 0;
        g_gps_system.module_status[i].error_count = 0;
    }
    
    pthread_mutex_unlock(&g_gps_system_mutex);
    
    LOGX_INFO("GPS system initialization started");
    
    // Initialize GPS connector first
    int result = gps_connector_init();
    if (result != AUTONOMY_SUCCESS) {
        LOGX_ERROR("Failed to initialize GPS connector: %d", result);
        return result;
    }
    
    // Initialize GPS integration
    result = gps_integration_init();
    if (result != AUTONOMY_SUCCESS) {
        LOGX_ERROR("Failed to initialize GPS integration: %d", result);
        return result;
    }
    
    // Initialize GPS events
    result = gps_events_init();
    if (result != AUTONOMY_SUCCESS) {
        LOGX_ERROR("Failed to initialize GPS events: %d", result);
        return result;
    }
    
    // Initialize GPS location services
    result = gps_location_services_init();
    if (result != AUTONOMY_SUCCESS) {
        LOGX_ERROR("Failed to initialize GPS location services: %d", result);
        return result;
    }
    
    // Initialize GPS clustering
    result = gps_clustering_init();
    if (result != AUTONOMY_SUCCESS) {
        LOGX_ERROR("Failed to initialize GPS clustering: %d", result);
        return result;
    }
    
    // Initialize GPS health monitoring
    result = gps_health_init();
    if (result != AUTONOMY_SUCCESS) {
        LOGX_ERROR("Failed to initialize GPS health monitoring: %d", result);
        return result;
    }
    
    // Initialize GPS fusion
    result = gps_fusion_init();
    if (result != AUTONOMY_SUCCESS) {
        LOGX_ERROR("Failed to initialize GPS fusion: %d", result);
        return result;
    }
    
    // Initialize GPS geofencing
    result = gps_geofence_init();
    if (result != AUTONOMY_SUCCESS) {
        LOGX_ERROR("Failed to initialize GPS geofencing: %d", result);
        return result;
    }
    
    // Initialize GPS coordinate utilities
    result = gps_coordinate_utils_init();
    if (result != AUTONOMY_SUCCESS) {
        LOGX_ERROR("Failed to initialize GPS coordinate utilities: %d", result);
        return result;
    }
    
    // Initialize GPS obstruction analysis
    result = gps_obstruction_init();
    if (result != AUTONOMY_SUCCESS) {
        LOGX_ERROR("Failed to initialize GPS obstruction analysis: %d", result);
        return result;
    }
    
    // Initialize GPS adaptive cache
    result = gps_adaptive_cache_init();
    if (result != AUTONOMY_SUCCESS) {
        LOGX_ERROR("Failed to initialize GPS adaptive cache: %d", result);
        return result;
    }
    
    // Initialize GPS Google API (with placeholder API key)
    result = gps_google_api_init("YOUR_GOOGLE_API_KEY_HERE");
    if (result != AUTONOMY_SUCCESS) {
        LOGX_ERROR("Failed to initialize GPS Google API: %d", result);
        return result;
    }
    
    // Initialize GPS Cell Tower positioning
    result = gps_cell_tower_init();
    if (result != AUTONOMY_SUCCESS) {
        LOGX_ERROR("Failed to initialize GPS Cell Tower positioning: %d", result);
        return result;
    }
    
    // Initialize GPS Weather integration (with placeholder API key)
    result = gps_weather_init("YOUR_WEATHER_API_KEY_HERE");
    if (result != AUTONOMY_SUCCESS) {
        LOGX_ERROR("Failed to initialize GPS Weather integration: %d", result);
        return result;
    }
    
    // Initialize GPS Terrain analysis
    result = gps_terrain_init();
    if (result != AUTONOMY_SUCCESS) {
        LOGX_ERROR("Failed to initialize GPS Terrain analysis: %d", result);
        return result;
    }
    
    // Initialize GPS Performance tracking
    result = gps_performance_init();
    if (result != AUTONOMY_SUCCESS) {
        LOGX_ERROR("Failed to initialize GPS Performance tracking: %d", result);
        return result;
    }
    
    // Initialize GPS Error recovery
    result = gps_error_recovery_init();
    if (result != AUTONOMY_SUCCESS) {
        LOGX_ERROR("Failed to initialize GPS Error recovery: %d", result);
        return result;
    }
    
    // Register all modules with the connector
    register_gps_modules();
    
    // Mark initialization as complete
    pthread_mutex_lock(&g_gps_system_mutex);
    g_gps_system.init_complete = true;
    g_gps_system.init_complete_time = time(NULL);
    pthread_mutex_unlock(&g_gps_system_mutex);
    
    LOGX_INFO("GPS system initialization completed successfully in %ld seconds", 
               g_gps_system.init_complete_time - g_gps_system.init_start_time);
    
    g_gps_system_initialized = true;
    return AUTONOMY_SUCCESS;
}

// Register GPS modules with the connector
static void register_gps_modules(void) {
    // Register GPS integration module
    int integration_id = gps_connector_register_module("GPS Integration", GPS_MODULE_TYPE_INTEGRATION);
    if (integration_id > 0) {
        update_module_status(GPS_MODULE_TYPE_INTEGRATION, true, true, 100.0);
        LOGX_INFO("Registered GPS Integration module with ID: %d", integration_id);
    }
    
    // Register GPS events module
    int events_id = gps_connector_register_module("GPS Events", GPS_MODULE_TYPE_EVENTS);
    if (events_id > 0) {
        update_module_status(GPS_MODULE_TYPE_EVENTS, true, true, 100.0);
        LOGX_INFO("Registered GPS Events module with ID: %d", events_id);
    }
    
    // Register GPS location services module
    int location_services_id = gps_connector_register_module("GPS Location Services", GPS_MODULE_TYPE_LOCATION_SERVICES);
    if (location_services_id > 0) {
        update_module_status(GPS_MODULE_TYPE_LOCATION_SERVICES, true, true, 100.0);
        LOGX_INFO("Registered GPS Location Services module with ID: %d", location_services_id);
    }
    
    // Register GPS clustering module
    int clustering_id = gps_connector_register_module("GPS Clustering", GPS_MODULE_TYPE_CLUSTERING);
    if (clustering_id > 0) {
        update_module_status(GPS_MODULE_TYPE_CLUSTERING, true, true, 100.0);
        LOGX_INFO("Registered GPS Clustering module with ID: %d", clustering_id);
    }
    
    // Register GPS health monitoring module
    int health_id = gps_connector_register_module("GPS Health", GPS_MODULE_TYPE_HEALTH);
    if (health_id > 0) {
        update_module_status(GPS_MODULE_TYPE_HEALTH, true, true, 100.0);
        LOGX_INFO("Registered GPS Health module with ID: %d", health_id);
    }
    
    // Register GPS fusion module
    int fusion_id = gps_connector_register_module("GPS Fusion", GPS_MODULE_TYPE_FUSION);
    if (fusion_id > 0) {
        update_module_status(GPS_MODULE_TYPE_FUSION, true, true, 100.0);
        LOGX_INFO("Registered GPS Fusion module with ID: %d", fusion_id);
    }
    
    // Register GPS geofencing module
    int geofence_id = gps_connector_register_module("GPS Geofencing", GPS_MODULE_TYPE_GEOFENCE);
    if (geofence_id > 0) {
        update_module_status(GPS_MODULE_TYPE_GEOFENCE, true, true, 100.0);
        LOGX_INFO("Registered GPS Geofencing module with ID: %d", geofence_id);
    }
    
    // Register GPS coordinate utilities module
    int coordinate_utils_id = gps_connector_register_module("GPS Coordinate Utils", GPS_MODULE_TYPE_COORDINATE_UTILS);
    if (coordinate_utils_id > 0) {
        update_module_status(GPS_MODULE_TYPE_COORDINATE_UTILS, true, true, 100.0);
        LOGX_INFO("Registered GPS Coordinate Utils module with ID: %d", coordinate_utils_id);
    }
    
    // Register GPS obstruction analysis module
    int obstruction_id = gps_connector_register_module("GPS Obstruction Analysis", GPS_MODULE_TYPE_OBSTRUCTION);
    if (obstruction_id > 0) {
        update_module_status(GPS_MODULE_TYPE_OBSTRUCTION, true, true, 100.0);
        LOGX_INFO("Registered GPS Obstruction Analysis module with ID: %d", obstruction_id);
    }
    
    // Register GPS adaptive cache module
    int adaptive_cache_id = gps_connector_register_module("GPS Adaptive Cache", GPS_MODULE_TYPE_ADAPTIVE_CACHE);
    if (adaptive_cache_id > 0) {
        update_module_status(GPS_MODULE_TYPE_ADAPTIVE_CACHE, true, true, 100.0);
        LOGX_INFO("Registered GPS Adaptive Cache module with ID: %d", adaptive_cache_id);
    }
    
    // Register GPS Google API module
    int google_api_id = gps_connector_register_module("GPS Google API", GPS_MODULE_TYPE_GOOGLE_API);
    if (google_api_id > 0) {
        update_module_status(GPS_MODULE_TYPE_GOOGLE_API, true, true, 100.0);
        LOGX_INFO("Registered GPS Google API module with ID: %d", google_api_id);
    }
    
    // Register GPS Cell Tower positioning module
    int cell_tower_id = gps_connector_register_module("GPS Cell Tower", GPS_MODULE_TYPE_CELL_TOWER);
    if (cell_tower_id > 0) {
        update_module_status(GPS_MODULE_TYPE_CELL_TOWER, true, true, 100.0);
        LOGX_INFO("Registered GPS Cell Tower positioning module with ID: %d", cell_tower_id);
    }
    
    // Register GPS Weather integration module
    int weather_id = gps_connector_register_module("GPS Weather", GPS_MODULE_TYPE_WEATHER);
    if (weather_id > 0) {
        update_module_status(GPS_MODULE_TYPE_WEATHER, true, true, 100.0);
        LOGX_INFO("Registered GPS Weather integration module with ID: %d", weather_id);
    }
    
    // Register GPS Terrain analysis module
    int terrain_id = gps_connector_register_module("GPS Terrain", GPS_MODULE_TYPE_TERRAIN);
    if (terrain_id > 0) {
        update_module_status(GPS_MODULE_TYPE_TERRAIN, true, true, 100.0);
        LOGX_INFO("Registered GPS Terrain analysis module with ID: %d", terrain_id);
    }
    
    // Register GPS Performance tracking module
    int performance_id = gps_connector_register_module("GPS Performance", GPS_MODULE_TYPE_PERFORMANCE);
    if (performance_id > 0) {
        update_module_status(GPS_MODULE_TYPE_PERFORMANCE, true, true, 100.0);
        LOGX_INFO("Registered GPS Performance tracking module with ID: %d", performance_id);
    }
    
    // Register GPS Error recovery module
    int error_recovery_id = gps_connector_register_module("GPS Error Recovery", GPS_MODULE_TYPE_ERROR_RECOVERY);
    if (error_recovery_id > 0) {
        update_module_status(GPS_MODULE_TYPE_ERROR_RECOVERY, true, true, 100.0);
        LOGX_INFO("Registered GPS Error recovery module with ID: %d", error_recovery_id);
    }
    
    LOGX_INFO("Registered %d GPS modules with the connector", g_gps_system.module_count);
}

// Update module status
static void update_module_status(gps_module_type_t module_type, bool initialized, bool enabled, double health_score) {
    pthread_mutex_lock(&g_gps_system_mutex);
    
    // Find module in status array
    for (int i = 0; i < GPS_MAX_MODULES; i++) {
        if (g_gps_system.module_status[i].module_type == GPS_MODULE_TYPE_UNKNOWN) {
            // Add new module
            g_gps_system.module_status[i].module_type = module_type;
            g_gps_system.module_status[i].initialized = initialized;
            g_gps_system.module_status[i].enabled = enabled;
            g_gps_system.module_status[i].health_score = health_score;
            g_gps_system.module_status[i].last_operation = time(NULL);
            g_gps_system.module_status[i].error_count = 0;
            
            g_gps_system.module_count++;
            if (enabled) {
                g_gps_system.active_modules++;
            }
            break;
        } else if (g_gps_system.module_status[i].module_type == module_type) {
            // Update existing module
            g_gps_system.module_status[i].initialized = initialized;
            g_gps_system.module_status[i].enabled = enabled;
            g_gps_system.module_status[i].health_score = health_score;
            g_gps_system.module_status[i].last_operation = time(NULL);
            break;
        }
    }
    
    pthread_mutex_unlock(&g_gps_system_mutex);
}

// Perform GPS system health check
static int gps_system_health_check(void) {
    if (!g_gps_system_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_gps_system_mutex);
    
    time_t now = time(NULL);
    
    // Check if enough time has passed since last health check
    if ((now - g_gps_system.last_health_check) < g_gps_system.health_check_interval) {
        pthread_mutex_unlock(&g_gps_system_mutex);
        return AUTONOMY_SUCCESS;
    }
    
    g_gps_system.last_health_check = now;
    
    // Get connector status
    gps_connector_status_t connector_status;
    int result = gps_connector_get_status(&connector_status);
    if (result == AUTONOMY_SUCCESS) {
        g_gps_system.system_health = connector_status.system_health;
    }
    
    // Check individual module health
    check_module_health();
    
    // Update overall system health
    update_system_health();
    
    pthread_mutex_unlock(&g_gps_system_mutex);
    
    LOGX_DEBUG("GPS system health check completed - System health: %.1f%%", g_gps_system.system_health);
    
    return AUTONOMY_SUCCESS;
}

// Check individual module health
static void check_module_health(void) {
    for (int i = 0; i < GPS_MAX_MODULES; i++) {
        if (g_gps_system.module_status[i].module_type == GPS_MODULE_TYPE_UNKNOWN) {
            continue;
        }
        
        gps_module_status_t *module = &g_gps_system.module_status[i];
        time_t now = time(NULL);
        
        // Check if module is stale
        if (module->last_operation > 0 && 
            (now - module->last_operation) > 300) {  // 5 minutes
            module->health_score *= 0.9;  // Reduce health score
            LOGX_WARN("GPS module %d is stale (last operation: %ld seconds ago)", 
                      module->module_type, now - module->last_operation);
        }
        
        // Disable module if health is too low
        if (module->health_score < g_gps_system.min_health && module->enabled) {
            module->enabled = false;
            g_gps_system.active_modules--;
            LOGX_WARN("GPS module %d disabled due to poor health (score: %.1f)", 
                      module->module_type, module->health_score);
        }
    }
}

// Update overall system health
static void update_system_health(void) {
    double total_health = 0.0;
    int active_count = 0;
    
    for (int i = 0; i < GPS_MAX_MODULES; i++) {
        if (g_gps_system.module_status[i].module_type != GPS_MODULE_TYPE_UNKNOWN && 
            g_gps_system.module_status[i].enabled) {
            total_health += g_gps_system.module_status[i].health_score;
            active_count++;
        }
    }
    
    if (active_count > 0) {
        g_gps_system.system_health = total_health / active_count;
    } else {
        g_gps_system.system_health = 0.0;
    }
}

// Get GPS system status
static int gps_system_get_status(gps_system_status_t *status) {
    if (!g_gps_system_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_gps_system_mutex);
    
    status->enabled = g_gps_system.enabled;
    status->init_complete = g_gps_system.init_complete;
    status->init_start_time = g_gps_system.init_start_time;
    status->init_complete_time = g_gps_system.init_complete_time;
    status->module_count = g_gps_system.module_count;
    status->active_modules = g_gps_system.active_modules;
    status->system_health = g_gps_system.system_health;
    status->last_health_check = g_gps_system.last_health_check;
    
    // Copy module status information
    int active_modules = 0;
    for (int i = 0; i < GPS_MAX_MODULES && active_modules < GPS_MAX_MODULES; i++) {
        if (g_gps_system.module_status[i].module_type != GPS_MODULE_TYPE_UNKNOWN) {
            memcpy(&status->module_status[active_modules], &g_gps_system.module_status[i], 
                   sizeof(gps_module_status_t));
            active_modules++;
        }
    }
    status->active_module_count = active_modules;
    
    pthread_mutex_unlock(&g_gps_system_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get GPS system configuration
static int gps_system_get_config(gps_system_config_t *config) {
    if (!g_gps_system_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_gps_system_mutex);
    
    config->enabled = g_gps_system.enabled;
    config->init_timeout = g_gps_system.init_timeout;
    config->health_check_interval = g_gps_system.health_check_interval;
    config->min_health = g_gps_system.min_health;
    
    pthread_mutex_unlock(&g_gps_system_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set GPS system configuration
static int gps_system_set_config(const gps_system_config_t *config) {
    if (!g_gps_system_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_gps_system_mutex);
    
    g_gps_system.enabled = config->enabled;
    g_gps_system.init_timeout = config->init_timeout;
    g_gps_system.health_check_interval = config->health_check_interval;
    g_gps_system.min_health = config->min_health;
    
    pthread_mutex_unlock(&g_gps_system_mutex);
    
    LOGX_INFO("GPS system configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable GPS system
static int gps_system_set_enabled(bool enabled) {
    if (!g_gps_system_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_gps_system_mutex);
    g_gps_system.enabled = enabled;
    pthread_mutex_unlock(&g_gps_system_mutex);
    
    LOGX_INFO("GPS system %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Reset GPS system
static int gps_system_reset(void) {
    if (!g_gps_system_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_gps_system_mutex);
    
    g_gps_system.init_complete = false;
    g_gps_system.module_count = 0;
    g_gps_system.active_modules = 0;
    g_gps_system.system_health = 0.0;
    g_gps_system.last_health_check = 0;
    
    // Clear all module status
    for (int i = 0; i < GPS_MAX_MODULES; i++) {
        g_gps_system.module_status[i].module_type = GPS_MODULE_TYPE_UNKNOWN;
        g_gps_system.module_status[i].initialized = false;
        g_gps_system.module_status[i].enabled = false;
        g_gps_system.module_status[i].health_score = 0.0;
        g_gps_system.module_status[i].last_operation = 0;
        g_gps_system.module_status[i].error_count = 0;
    }
    
    pthread_mutex_unlock(&g_gps_system_mutex);
    
    LOGX_INFO("GPS system reset");
    return AUTONOMY_SUCCESS;
}

// Cleanup GPS system
static void gps_system_cleanup(void) {
    if (!g_gps_system_initialized) {
        return;
    }
    
    // Cleanup all GPS modules
    gps_connector_cleanup();
    gps_integration_cleanup();
    gps_events_cleanup();
    gps_location_services_cleanup();
    gps_clustering_cleanup();
    gps_health_cleanup();
    gps_fusion_cleanup();
    gps_geofence_cleanup();
    gps_coordinate_utils_cleanup();
    gps_obstruction_cleanup();
    gps_adaptive_cache_cleanup();
    gps_google_api_cleanup();
    gps_cell_tower_cleanup();
    gps_weather_cleanup();
    gps_terrain_cleanup();
    gps_performance_cleanup();
    gps_error_recovery_cleanup();
    
    pthread_mutex_destroy(&g_gps_system_mutex);
    g_gps_system_initialized = false;
    
    LOGX_INFO("GPS system cleaned up");
}
