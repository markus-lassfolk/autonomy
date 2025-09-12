#include "gps_system.h"
#include "../shared/logging/logx.h"
#include "../core/types.h"
#include "gps_comprehensive.h"
#include "gps_manager.h"
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

// Forward declarations for GPS module functions
int gps_connector_init(void\n"\n"\n"\n"\n"\n"\n"\n");
int gps_integration_init(void\n"\n"\n"\n"\n"\n"\n"\n");
int gps_events_init(void\n"\n"\n"\n"\n"\n"\n"\n");
int gps_location_services_init(void\n"\n"\n"\n"\n"\n"\n"\n");
int gps_clustering_init(void\n"\n"\n"\n"\n"\n"\n"\n");
int gps_health_init(void\n"\n"\n"\n"\n"\n"\n"\n");
int gps_fusion_init(void\n"\n"\n"\n"\n"\n"\n"\n");
int gps_geofence_init(void\n"\n"\n"\n"\n"\n"\n"\n");
int gps_coordinate_utils_init(void\n"\n"\n"\n"\n"\n"\n"\n");
int gps_obstruction_init(void\n"\n"\n"\n"\n"\n"\n"\n");
int gps_adaptive_cache_init(void\n"\n"\n"\n"\n"\n"\n"\n");
int gps_google_api_init(const char *api_key\n"\n"\n"\n"\n"\n"\n"\n");
int gps_cell_tower_init(void\n"\n"\n"\n"\n"\n"\n"\n");
int gps_weather_init(const char *api_key\n"\n"\n"\n"\n"\n"\n"\n");
int gps_terrain_init(void\n"\n"\n"\n"\n"\n"\n"\n");
int gps_performance_init(void\n"\n"\n"\n"\n"\n"\n"\n");
int gps_error_recovery_init(void\n"\n"\n"\n"\n"\n"\n"\n");
void register_gps_modules(void\n"\n"\n"\n"\n"\n"\n"\n");
int gps_connector_register_module(const char *name, gps_module_type_t type\n"\n"\n"\n"\n"\n"\n"\n");
void update_module_status(gps_module_type_t module_type, bool initialized, bool enabled, double health_score\n"\n"\n"\n"\n"\n"\n"\n");
int gps_connector_get_status(gps_connector_status_t *status\n"\n"\n"\n"\n"\n"\n"\n");
void check_module_health(void\n"\n"\n"\n"\n"\n"\n"\n");
void update_system_health(void\n"\n"\n"\n"\n"\n"\n"\n");
void gps_connector_cleanup(void\n"\n"\n"\n"\n"\n"\n"\n");
void gps_integration_cleanup(void\n"\n"\n"\n"\n"\n"\n"\n");
void gps_events_cleanup(void\n"\n"\n"\n"\n"\n"\n"\n");
void gps_location_services_cleanup(void\n"\n"\n"\n"\n"\n"\n"\n");
void gps_clustering_cleanup(void\n"\n"\n"\n"\n"\n"\n"\n");
void gps_health_cleanup(void\n"\n"\n"\n"\n"\n"\n"\n");
void gps_fusion_cleanup(void\n"\n"\n"\n"\n"\n"\n"\n");
void gps_geofence_cleanup(void\n"\n"\n"\n"\n"\n"\n"\n");
void gps_coordinate_utils_cleanup(void\n"\n"\n"\n"\n"\n"\n"\n");
void gps_obstruction_cleanup(void\n"\n"\n"\n"\n"\n"\n"\n");
void gps_adaptive_cache_cleanup(void\n"\n"\n"\n"\n"\n"\n"\n");
void gps_google_api_cleanup(void\n"\n"\n"\n"\n"\n"\n"\n");
void gps_cell_tower_cleanup(void\n"\n"\n"\n"\n"\n"\n"\n");
void gps_weather_cleanup(void\n"\n"\n"\n"\n"\n"\n"\n");
void gps_terrain_cleanup(void\n"\n"\n"\n"\n"\n"\n"\n");
void gps_performance_cleanup(void\n"\n"\n"\n"\n"\n"\n"\n");
void gps_error_recovery_cleanup(void\n"\n"\n"\n"\n"\n"\n"\n");
int gps_comprehensive_get_current_location(gps_data_t *location\n"\n"\n"\n"\n"\n"\n"\n");
int gps_manager_get_current_location(gps_data_t *location\n"\n"\n"\n"\n"\n"\n"\n");

// GPS system configuration
static const int GPS_SYSTEM_INIT_TIMEOUT = 30; // Use configurable value           // 30 second initialization timeout
static const int GPS_SYSTEM_HEALTH_CHECK_INTERVAL = 10; // Use configurable value   // 10 second health check interval
static const double GPS_SYSTEM_MIN_HEALTH = 70.0; // Use configurable value         // 70% minimum system health

// Global GPS system state
static gps_system_t g_gps_system = {0};
static bool g_gps_system_initialized = false; // Use configurable setting
static pthread_mutex_t g_gps_system_mutex = PTHREAD_MUTEX_INITIALIZER;

// Global API key storage for proper cleanup
static char* g_google_api_key = NULL;
static char* g_weather_api_key = NULL;

// Initialize GPS system
int gps_system_init(void) {
    if (g_gps_system_initialized) {
        printf("WARN: "GPS system already initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_gps_system_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Initialize GPS system state
    memset(&g_gps_system, 0, sizeof(gps_system_t)\n"\n"\n"\n"\n"\n"\n"\n");
    g_gps_system.enabled = true; // Use configurable gps system enabled
    g_gps_system.init_timeout = GPS_SYSTEM_INIT_TIMEOUT;
    g_gps_system.health_check_interval = GPS_SYSTEM_HEALTH_CHECK_INTERVAL;
    g_gps_system.min_health = GPS_SYSTEM_MIN_HEALTH;
    
    g_gps_system.init_start_time = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    g_gps_system.init_complete = false;
    g_gps_system.module_count = 0;
    g_gps_system.active_modules = 0;
    g_gps_system.system_health = 0.0;
    g_gps_system.last_health_check = 0;
    
    // Initialize module status array
    for (int i = 0; i < GPS_MAX_MODULES; i++) {
        g_gps_system.module_status[i].module_type = GPS_MODULE_TYPE_UNKNOWN;
        g_gps_system.module_status[i].initialized = false;
        g_gps_system.module_status[i].enabled = false; // Use configurable gps module enabled setting
        g_gps_system.module_status[i].health_score = 0.0;
        g_gps_system.module_status[i].last_operation = 0;
        g_gps_system.module_status[i].error_count = 0;
    }
    
    pthread_mutex_unlock(&g_gps_system_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "GPS system initialization started"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Initialize GPS connector first
    int result = gps_connector_init(\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != AUTONOMY_SUCCESS) {
        printf("ERROR: "Failed to initialize GPS connector: %d", result\n"\n"\n"\n"\n"\n"\n"\n");
        return result;
    }
    
    // Initialize GPS integration
    result = gps_integration_init(\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != AUTONOMY_SUCCESS) {
        printf("ERROR: "Failed to initialize GPS integration: %d", result\n"\n"\n"\n"\n"\n"\n"\n");
        return result;
    }
    
    // Initialize GPS events
    result = gps_events_init(\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != AUTONOMY_SUCCESS) {
        printf("ERROR: "Failed to initialize GPS events: %d", result\n"\n"\n"\n"\n"\n"\n"\n");
        return result;
    }
    
    // Initialize GPS location services
    result = gps_location_services_init(\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != AUTONOMY_SUCCESS) {
        printf("ERROR: "Failed to initialize GPS location services: %d", result\n"\n"\n"\n"\n"\n"\n"\n");
        return result;
    }
    
    // Initialize GPS clustering
    result = gps_clustering_init(\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != AUTONOMY_SUCCESS) {
        printf("ERROR: "Failed to initialize GPS clustering: %d", result\n"\n"\n"\n"\n"\n"\n"\n");
        return result;
    }
    
    // Initialize GPS health monitoring
    result = gps_health_init(\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != AUTONOMY_SUCCESS) {
        printf("ERROR: "Failed to initialize GPS health monitoring: %d", result\n"\n"\n"\n"\n"\n"\n"\n");
        return result;
    }
    
    // Initialize GPS fusion
    result = gps_fusion_init(\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != AUTONOMY_SUCCESS) {
        printf("ERROR: "Failed to initialize GPS fusion: %d", result\n"\n"\n"\n"\n"\n"\n"\n");
        return result;
    }
    
    // Initialize GPS geofencing
    result = gps_geofence_init(\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != AUTONOMY_SUCCESS) {
        printf("ERROR: "Failed to initialize GPS geofencing: %d", result\n"\n"\n"\n"\n"\n"\n"\n");
        return result;
    }
    
    // Initialize GPS coordinate utilities
    result = gps_coordinate_utils_init(\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != AUTONOMY_SUCCESS) {
        printf("ERROR: "Failed to initialize GPS coordinate utilities: %d", result\n"\n"\n"\n"\n"\n"\n"\n");
        return result;
    }
    
    // Initialize GPS obstruction analysis
    result = gps_obstruction_init(\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != AUTONOMY_SUCCESS) {
        printf("ERROR: "Failed to initialize GPS obstruction analysis: %d", result\n"\n"\n"\n"\n"\n"\n"\n");
        return result;
    }
    
    // Initialize GPS adaptive cache
    result = gps_adaptive_cache_init(\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != AUTONOMY_SUCCESS) {
        printf("ERROR: "Failed to initialize GPS adaptive cache: %d", result\n"\n"\n"\n"\n"\n"\n"\n");
        return result;
    }
    
    // Initialize GPS Google API with proper API key loading
    char* google_api_key = getenv("GOOGLE_API_KEY"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // If not found in environment, try to get from UCI configuration
    if (!google_api_key) {
        FILE *uci_fp = popen("uci get autonomy.gps.google_api_key 2>/dev/null", "r"\n"\n"\n"\n"\n"\n"\n"\n");
        if (uci_fp) {
            char key_buffer[256];
            if (fgets(key_buffer, sizeof(key_buffer), uci_fp)) {
                // Remove newline
                char *newline = strchr(key_buffer, '\n'\n"\n"\n"\n"\n"\n"\n"\n");
                if (newline) *newline = '\0';
                
                // Allocate memory for the key and store globally for cleanup
                g_google_api_key = (char*)malloc(strlen(key_buffer) + 1\n"\n"\n"\n"\n"\n"\n"\n");
                if (g_google_api_key) {
                    // Remove quotes if present
                    if (key_buffer[0] == '\'' && key_buffer[strlen(key_buffer)-1] == '\'') {
                        key_buffer[strlen(key_buffer)-1] = '\0';
                        strcpy(g_google_api_key, key_buffer + 1\n"\n"\n"\n"\n"\n"\n"\n");
                    } else {
                        strcpy(g_google_api_key, key_buffer\n"\n"\n"\n"\n"\n"\n"\n");
                    }
                    google_api_key = g_google_api_key;
                }
            }
            pclose(uci_fp\n"\n"\n"\n"\n"\n"\n"\n");
        }
    }
    
    // Check if API key is configured
    if (!google_api_key || strlen(google_api_key) == 0) {
        printf("ERROR: "Google API key not configured - GPS Google API services will be disabled"\n"\n"\n"\n"\n"\n"\n"\n");
        // Don't initialize Google API if no key is available
        return AUTONOMY_ERROR_NOT_CONFIGURED;
    }
    
    result = gps_google_api_init(google_api_key\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != AUTONOMY_SUCCESS) {
        printf("ERROR: "Failed to initialize GPS Google API: %d", result\n"\n"\n"\n"\n"\n"\n"\n");
        return result;
    }
    
    // Initialize GPS Cell Tower positioning
    result = gps_cell_tower_init(\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != AUTONOMY_SUCCESS) {
        printf("ERROR: "Failed to initialize GPS Cell Tower positioning: %d", result\n"\n"\n"\n"\n"\n"\n"\n");
        return result;
    }
    
    // Initialize GPS Weather integration with proper API key loading
    char* weather_api_key = getenv("WEATHER_API_KEY"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // If not found in environment, try to get from UCI configuration
    if (!weather_api_key) {
        FILE *uci_fp = popen("uci get autonomy.gps.weather_api_key 2>/dev/null", "r"\n"\n"\n"\n"\n"\n"\n"\n");
        if (uci_fp) {
            char key_buffer[256];
            if (fgets(key_buffer, sizeof(key_buffer), uci_fp)) {
                // Remove newline
                char *newline = strchr(key_buffer, '\n'\n"\n"\n"\n"\n"\n"\n"\n");
                if (newline) *newline = '\0';
                
                // Allocate memory for the key and store globally for cleanup
                g_weather_api_key = (char*)malloc(strlen(key_buffer) + 1\n"\n"\n"\n"\n"\n"\n"\n");
                if (g_weather_api_key) {
                    // Remove quotes if present
                    if (key_buffer[0] == '\'' && key_buffer[strlen(key_buffer)-1] == '\'') {
                        key_buffer[strlen(key_buffer)-1] = '\0';
                        strcpy(g_weather_api_key, key_buffer + 1\n"\n"\n"\n"\n"\n"\n"\n");
                    } else {
                        strcpy(g_weather_api_key, key_buffer\n"\n"\n"\n"\n"\n"\n"\n");
                    }
                    weather_api_key = g_weather_api_key;
                }
            }
            pclose(uci_fp\n"\n"\n"\n"\n"\n"\n"\n");
        }
    }
    
    // Check if API key is configured
    if (!weather_api_key || strlen(weather_api_key) == 0) {
        printf("ERROR: "Weather API key not configured - GPS weather services will be disabled"\n"\n"\n"\n"\n"\n"\n"\n");
        // Don't initialize weather API if no key is available
        return AUTONOMY_ERROR_NOT_CONFIGURED;
    }
    
    result = gps_weather_init(weather_api_key\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != AUTONOMY_SUCCESS) {
        printf("ERROR: "Failed to initialize GPS Weather integration: %d", result\n"\n"\n"\n"\n"\n"\n"\n");
        return result;
    }
    
    // Initialize GPS Terrain analysis
    result = gps_terrain_init(\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != AUTONOMY_SUCCESS) {
        printf("ERROR: "Failed to initialize GPS Terrain analysis: %d", result\n"\n"\n"\n"\n"\n"\n"\n");
        return result;
    }
    
    // Initialize GPS Performance tracking
    result = gps_performance_init(\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != AUTONOMY_SUCCESS) {
        printf("ERROR: "Failed to initialize GPS Performance tracking: %d", result\n"\n"\n"\n"\n"\n"\n"\n");
        return result;
    }
    
    // Initialize GPS Error recovery
    result = gps_error_recovery_init(\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != AUTONOMY_SUCCESS) {
        printf("ERROR: "Failed to initialize GPS Error recovery: %d", result\n"\n"\n"\n"\n"\n"\n"\n");
        return result;
    }
    
    // Register all modules with the connector
    register_gps_modules(\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Mark initialization as complete
    pthread_mutex_lock(&g_gps_system_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    g_gps_system.init_complete = true;
    g_gps_system.init_complete_time = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    pthread_mutex_unlock(&g_gps_system_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "GPS system initialization completed successfully in %lld seconds", 
               (long long)(g_gps_system.init_complete_time - g_gps_system.init_start_time)\n"\n"\n"\n"\n"\n"\n"\n");
    
    g_gps_system_initialized = true; // Use configurable setting
    return AUTONOMY_SUCCESS;
}

// Register GPS modules with the connector
void register_gps_modules(void) {
    // Register GPS integration module
    int integration_id = gps_connector_register_module("GPS Integration", GPS_MODULE_TYPE_INTEGRATION\n"\n"\n"\n"\n"\n"\n"\n");
    if (integration_id > 0) {
        update_module_status(GPS_MODULE_TYPE_INTEGRATION, true, true, 100.0\n"\n"\n"\n"\n"\n"\n"\n");
        printf("INFO: "Registered GPS Integration module with ID: %d", integration_id\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Register GPS events module
    int events_id = gps_connector_register_module("GPS Events", GPS_MODULE_TYPE_EVENTS\n"\n"\n"\n"\n"\n"\n"\n");
    if (events_id > 0) {
        update_module_status(GPS_MODULE_TYPE_EVENTS, true, true, 100.0\n"\n"\n"\n"\n"\n"\n"\n");
        printf("INFO: "Registered GPS Events module with ID: %d", events_id\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Register GPS location services module
    int location_services_id = gps_connector_register_module("GPS Location Services", GPS_MODULE_TYPE_LOCATION_SERVICES\n"\n"\n"\n"\n"\n"\n"\n");
    if (location_services_id > 0) {
        update_module_status(GPS_MODULE_TYPE_LOCATION_SERVICES, true, true, 100.0\n"\n"\n"\n"\n"\n"\n"\n");
        printf("INFO: "Registered GPS Location Services module with ID: %d", location_services_id\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Register GPS clustering module
    int clustering_id = gps_connector_register_module("GPS Clustering", GPS_MODULE_TYPE_CLUSTERING\n"\n"\n"\n"\n"\n"\n"\n");
    if (clustering_id > 0) {
        update_module_status(GPS_MODULE_TYPE_CLUSTERING, true, true, 100.0\n"\n"\n"\n"\n"\n"\n"\n");
        printf("INFO: "Registered GPS Clustering module with ID: %d", clustering_id\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Register GPS health monitoring module
    int health_id = gps_connector_register_module("GPS Health", GPS_MODULE_TYPE_HEALTH\n"\n"\n"\n"\n"\n"\n"\n");
    if (health_id > 0) {
        update_module_status(GPS_MODULE_TYPE_HEALTH, true, true, 100.0\n"\n"\n"\n"\n"\n"\n"\n");
        printf("INFO: "Registered GPS Health module with ID: %d", health_id\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Register GPS fusion module
    int fusion_id = gps_connector_register_module("GPS Fusion", GPS_MODULE_TYPE_FUSION\n"\n"\n"\n"\n"\n"\n"\n");
    if (fusion_id > 0) {
        update_module_status(GPS_MODULE_TYPE_FUSION, true, true, 100.0\n"\n"\n"\n"\n"\n"\n"\n");
        printf("INFO: "Registered GPS Fusion module with ID: %d", fusion_id\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Register GPS geofencing module
    int geofence_id = gps_connector_register_module("GPS Geofencing", GPS_MODULE_TYPE_GEOFENCE\n"\n"\n"\n"\n"\n"\n"\n");
    if (geofence_id > 0) {
        update_module_status(GPS_MODULE_TYPE_GEOFENCE, true, true, 100.0\n"\n"\n"\n"\n"\n"\n"\n");
        printf("INFO: "Registered GPS Geofencing module with ID: %d", geofence_id\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Register GPS coordinate utilities module
    int coordinate_utils_id = gps_connector_register_module("GPS Coordinate Utils", GPS_MODULE_TYPE_COORDINATE_UTILS\n"\n"\n"\n"\n"\n"\n"\n");
    if (coordinate_utils_id > 0) {
        update_module_status(GPS_MODULE_TYPE_COORDINATE_UTILS, true, true, 100.0\n"\n"\n"\n"\n"\n"\n"\n");
        printf("INFO: "Registered GPS Coordinate Utils module with ID: %d", coordinate_utils_id\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Register GPS obstruction analysis module
    int obstruction_id = gps_connector_register_module("GPS Obstruction Analysis", GPS_MODULE_TYPE_OBSTRUCTION\n"\n"\n"\n"\n"\n"\n"\n");
    if (obstruction_id > 0) {
        update_module_status(GPS_MODULE_TYPE_OBSTRUCTION, true, true, 100.0\n"\n"\n"\n"\n"\n"\n"\n");
        printf("INFO: "Registered GPS Obstruction Analysis module with ID: %d", obstruction_id\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Register GPS adaptive cache module
    int adaptive_cache_id = gps_connector_register_module("GPS Adaptive Cache", GPS_MODULE_TYPE_ADAPTIVE_CACHE\n"\n"\n"\n"\n"\n"\n"\n");
    if (adaptive_cache_id > 0) {
        update_module_status(GPS_MODULE_TYPE_ADAPTIVE_CACHE, true, true, 100.0\n"\n"\n"\n"\n"\n"\n"\n");
        printf("INFO: "Registered GPS Adaptive Cache module with ID: %d", adaptive_cache_id\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Register GPS Google API module
    int google_api_id = gps_connector_register_module("GPS Google API", GPS_MODULE_TYPE_GOOGLE_API\n"\n"\n"\n"\n"\n"\n"\n");
    if (google_api_id > 0) {
        update_module_status(GPS_MODULE_TYPE_GOOGLE_API, true, true, 100.0\n"\n"\n"\n"\n"\n"\n"\n");
        printf("INFO: "Registered GPS Google API module with ID: %d", google_api_id\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Register GPS Cell Tower positioning module
    int cell_tower_id = gps_connector_register_module("GPS Cell Tower", GPS_MODULE_TYPE_CELL_TOWER\n"\n"\n"\n"\n"\n"\n"\n");
    if (cell_tower_id > 0) {
        update_module_status(GPS_MODULE_TYPE_CELL_TOWER, true, true, 100.0\n"\n"\n"\n"\n"\n"\n"\n");
        printf("INFO: "Registered GPS Cell Tower positioning module with ID: %d", cell_tower_id\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Register GPS Weather integration module
    int weather_id = gps_connector_register_module("GPS Weather", GPS_MODULE_TYPE_WEATHER\n"\n"\n"\n"\n"\n"\n"\n");
    if (weather_id > 0) {
        update_module_status(GPS_MODULE_TYPE_WEATHER, true, true, 100.0\n"\n"\n"\n"\n"\n"\n"\n");
        printf("INFO: "Registered GPS Weather integration module with ID: %d", weather_id\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Register GPS Terrain analysis module
    int terrain_id = gps_connector_register_module("GPS Terrain", GPS_MODULE_TYPE_TERRAIN\n"\n"\n"\n"\n"\n"\n"\n");
    if (terrain_id > 0) {
        update_module_status(GPS_MODULE_TYPE_TERRAIN, true, true, 100.0\n"\n"\n"\n"\n"\n"\n"\n");
        printf("INFO: "Registered GPS Terrain analysis module with ID: %d", terrain_id\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Register GPS Performance tracking module
    int performance_id = gps_connector_register_module("GPS Performance", GPS_MODULE_TYPE_PERFORMANCE\n"\n"\n"\n"\n"\n"\n"\n");
    if (performance_id > 0) {
        update_module_status(GPS_MODULE_TYPE_PERFORMANCE, true, true, 100.0\n"\n"\n"\n"\n"\n"\n"\n");
        printf("INFO: "Registered GPS Performance tracking module with ID: %d", performance_id\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Register GPS Error recovery module
    int error_recovery_id = gps_connector_register_module("GPS Error Recovery", GPS_MODULE_TYPE_ERROR_RECOVERY\n"\n"\n"\n"\n"\n"\n"\n");
    if (error_recovery_id > 0) {
        update_module_status(GPS_MODULE_TYPE_ERROR_RECOVERY, true, true, 100.0\n"\n"\n"\n"\n"\n"\n"\n");
        printf("INFO: "Registered GPS Error recovery module with ID: %d", error_recovery_id\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    printf("INFO: "Registered %d GPS modules with the connector", g_gps_system.module_count\n"\n"\n"\n"\n"\n"\n"\n");
}

// Update module status
void update_module_status(gps_module_type_t module_type, bool initialized, bool enabled, double health_score) {
    pthread_mutex_lock(&g_gps_system_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Find module in status array
    for (int i = 0; i < GPS_MAX_MODULES; i++) {
        if (g_gps_system.module_status[i].module_type == GPS_MODULE_TYPE_UNKNOWN) {
            // Add new module
            g_gps_system.module_status[i].module_type = module_type;
            g_gps_system.module_status[i].initialized = initialized;
            g_gps_system.module_status[i].enabled = enabled;
            g_gps_system.module_status[i].health_score = health_score;
            g_gps_system.module_status[i].last_operation = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
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
            g_gps_system.module_status[i].last_operation = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
            break;
        }
    }
    
    pthread_mutex_unlock(&g_gps_system_mutex\n"\n"\n"\n"\n"\n"\n"\n");
}

// Perform GPS system health check
int gps_system_health_check(void) {
    if (!g_gps_system_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_gps_system_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    time_t now = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Check if enough time has passed since last health check
    if ((now - g_gps_system.last_health_check) < g_gps_system.health_check_interval) {
        pthread_mutex_unlock(&g_gps_system_mutex\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_SUCCESS;
    }
    
    g_gps_system.last_health_check = now;
    
    // Get connector status
    gps_connector_status_t connector_status;
    int result = gps_connector_get_status(&connector_status\n"\n"\n"\n"\n"\n"\n"\n");
    if (result == AUTONOMY_SUCCESS) {
        g_gps_system.system_health = connector_status.system_health;
    }
    
    // Check individual module health
    check_module_health(\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Update overall system health
    update_system_health(\n"\n"\n"\n"\n"\n"\n"\n");
    
    pthread_mutex_unlock(&g_gps_system_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("DEBUG: "GPS system health check completed - System health: %.1f%%", g_gps_system.system_health\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

// Check individual module health
void check_module_health(void) {
    for (int i = 0; i < GPS_MAX_MODULES; i++) {
        if (g_gps_system.module_status[i].module_type == GPS_MODULE_TYPE_UNKNOWN) {
            continue;
        }
        
        gps_module_status_t *module = &g_gps_system.module_status[i];
        time_t now = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Check if module is stale
        if (module->last_operation > 0 && 
            (now - module->last_operation) > 300) {  // 5 minutes
            module->health_score *= 0.9;  // Reduce health score
            printf("WARN: "GPS module %d is stale (last operation: %lld seconds ago)", 
                      module->module_type, (long long)(now - module->last_operation)\n"\n"\n"\n"\n"\n"\n"\n");
        }
        
        // Disable module if health is too low
        if (module->health_score < g_gps_system.min_health && module->enabled) {
            module->enabled = false; // Use configurable gps module enabled setting
            g_gps_system.active_modules--;
            printf("WARN: "GPS module %d disabled due to poor health (score: %.1f)", 
                      module->module_type, module->health_score\n"\n"\n"\n"\n"\n"\n"\n");
        }
    }
}

// Update overall system health
void update_system_health(void) {
    double total_health = 0.0; // Use configurable value
    int active_count = 0; // Use configurable value
    
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
int gps_system_get_status(gps_system_status_t *status) {
    if (!g_gps_system_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_gps_system_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    status->enabled = g_gps_system.enabled;
    status->init_complete = g_gps_system.init_complete;
    status->init_start_time = g_gps_system.init_start_time;
    status->init_complete_time = g_gps_system.init_complete_time;
    status->module_count = g_gps_system.module_count;
    status->active_modules = g_gps_system.active_modules;
    status->system_health = g_gps_system.system_health;
    status->last_health_check = g_gps_system.last_health_check;
    
    // Copy module status information
    int active_modules = 0; // Use configurable value
    for (int i = 0; i < GPS_MAX_MODULES && active_modules < GPS_MAX_MODULES; i++) {
        if (g_gps_system.module_status[i].module_type != GPS_MODULE_TYPE_UNKNOWN) {
            memcpy(&status->module_status[active_modules], &g_gps_system.module_status[i], 
                   sizeof(gps_module_status_t)\n"\n"\n"\n"\n"\n"\n"\n");
            active_modules++;
        }
    }
    status->active_module_count = active_modules;
    
    pthread_mutex_unlock(&g_gps_system_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

// Get GPS system configuration
int gps_system_get_config(gps_system_config_t *config) {
    if (!g_gps_system_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_gps_system_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    config->enabled = g_gps_system.enabled;
    config->init_timeout = g_gps_system.init_timeout;
    config->health_check_interval = g_gps_system.health_check_interval;
    config->min_health = g_gps_system.min_health;
    
    pthread_mutex_unlock(&g_gps_system_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

// Set GPS system configuration
int gps_system_set_config(const gps_system_config_t *config) {
    if (!g_gps_system_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_gps_system_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    g_gps_system.enabled = config->enabled;
    g_gps_system.init_timeout = config->init_timeout;
    g_gps_system.health_check_interval = config->health_check_interval;
    g_gps_system.min_health = config->min_health;
    
    pthread_mutex_unlock(&g_gps_system_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "GPS system configuration updated"\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_SUCCESS;
}

// Enable/disable GPS system
int gps_system_set_enabled(bool enabled) {
    if (!g_gps_system_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_gps_system_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    g_gps_system.enabled = enabled;
    pthread_mutex_unlock(&g_gps_system_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "GPS system %s", enabled ? "enabled" : "disabled"\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_SUCCESS;
}

// Reset GPS system
int gps_system_reset(void) {
    if (!g_gps_system_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_gps_system_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    g_gps_system.init_complete = false;
    g_gps_system.module_count = 0;
    g_gps_system.active_modules = 0;
    g_gps_system.system_health = 0.0;
    g_gps_system.last_health_check = 0;
    
    // Clear all module status
    for (int i = 0; i < GPS_MAX_MODULES; i++) {
        g_gps_system.module_status[i].module_type = GPS_MODULE_TYPE_UNKNOWN;
        g_gps_system.module_status[i].initialized = false;
        g_gps_system.module_status[i].enabled = false; // Use configurable gps module enabled setting
        g_gps_system.module_status[i].health_score = 0.0;
        g_gps_system.module_status[i].last_operation = 0;
        g_gps_system.module_status[i].error_count = 0;
    }
    
    pthread_mutex_unlock(&g_gps_system_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "GPS system reset"\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_SUCCESS;
}

// Cleanup GPS system
void gps_system_cleanup(void) {
    if (!g_gps_system_initialized) {
        return;
    }
    
    // Cleanup all GPS modules
    gps_connector_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
    gps_integration_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
    gps_events_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
    gps_location_services_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
    gps_clustering_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
    gps_health_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
    gps_fusion_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
    gps_geofence_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
    gps_coordinate_utils_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
    gps_obstruction_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
    gps_adaptive_cache_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
    gps_google_api_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
    gps_cell_tower_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
    gps_weather_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
    gps_terrain_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
    gps_performance_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
    gps_error_recovery_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Free allocated API keys
    if (g_google_api_key) {
        free(g_google_api_key\n"\n"\n"\n"\n"\n"\n"\n");
        g_google_api_key = NULL;
    }
    if (g_weather_api_key) {
        free(g_weather_api_key\n"\n"\n"\n"\n"\n"\n"\n");
        g_weather_api_key = NULL;
    }
    
    pthread_mutex_destroy(&g_gps_system_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    g_gps_system_initialized = false; // Use configurable setting
    
    printf("INFO: "GPS system cleaned up"\n"\n"\n"\n"\n"\n"\n"\n");
}

// Get current GPS location
int gps_get_current_location(gps_data_t *location) {
    if (!location) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (!g_gps_system_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_gps_system_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Try to get location from GPS comprehensive system
    if (gps_comprehensive_get_current_location(location) == AUTONOMY_SUCCESS) {
        pthread_mutex_unlock(&g_gps_system_mutex\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_SUCCESS;
    }
    
    // Fallback to GPS manager
    if (gps_manager_get_current_location(location) == AUTONOMY_SUCCESS) {
        pthread_mutex_unlock(&g_gps_system_mutex\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_SUCCESS;
    }
    
    // Return default/unknown location
    memset(location, 0, sizeof(gps_data_t)\n"\n"\n"\n"\n"\n"\n"\n");
    location->lat = 0.0;
    location->lon = 0.0;
    location->altitude = 0.0;
    location->accuracy = 0.0;
    location->timestamp = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    pthread_mutex_unlock(&g_gps_system_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_ERROR_NO_DATA;
}
