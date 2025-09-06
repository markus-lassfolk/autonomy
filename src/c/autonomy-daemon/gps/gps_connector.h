#ifndef GPS_CONNECTOR_H
#define GPS_CONNECTOR_H

#include "../core/types.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// GPS module types
typedef enum {
    GPS_MODULE_TYPE_UNKNOWN = 0,
    GPS_MODULE_TYPE_INTEGRATION,
    GPS_MODULE_TYPE_MANAGER,
    GPS_MODULE_TYPE_RUTOS,
    GPS_MODULE_TYPE_STARLINK,
    GPS_MODULE_TYPE_CONFIDENCE,
    GPS_MODULE_TYPE_ACCURACY,
    GPS_MODULE_TYPE_NMEA,
    GPS_MODULE_TYPE_MOVEMENT,
    GPS_MODULE_TYPE_CLUSTERING,
    GPS_MODULE_TYPE_HEALTH,
    GPS_MODULE_TYPE_FUSION,
    GPS_MODULE_TYPE_GEOFENCE,
    GPS_MODULE_TYPE_EVENTS,
    GPS_MODULE_TYPE_LOCATION_SERVICES,
    GPS_MODULE_TYPE_COORDINATE_UTILS,
    GPS_MODULE_TYPE_OBSTRUCTION,
    GPS_MODULE_TYPE_ADAPTIVE_CACHE,
    GPS_MODULE_TYPE_GOOGLE_API,
    GPS_MODULE_TYPE_CELL_TOWER,
    GPS_MODULE_TYPE_WEATHER,
    GPS_MODULE_TYPE_TERRAIN,
    GPS_MODULE_TYPE_PERFORMANCE,
    GPS_MODULE_TYPE_ERROR_RECOVERY
} gps_module_type_t;

// GPS connector module
typedef struct {
    bool active;                        // Whether module is active
    int module_id;                      // Unique module identifier
    char name[64];                      // Module name
    gps_module_type_t module_type;      // Type of GPS module
    bool enabled;                        // Whether module is enabled
    time_t last_operation;              // Last operation timestamp
    int operation_count;                // Total operation count
    double health_score;                // Module health score (0-100)
    int error_count;                    // Total error count
    time_t last_error;                  // Last error timestamp
} gps_connector_module_t;

// GPS connector configuration
typedef struct {
    bool enabled;                       // Enable/disable connector
    int max_modules;                    // Maximum GPS modules
    int check_interval;                 // Connector check interval
    int health_timeout;                 // Module health timeout
    double health_threshold;            // Module health threshold
} gps_connector_config_t;

// GPS connector status
typedef struct {
    bool enabled;                       // Connector enabled
    int module_count;                   // Total modules
    int active_modules;                 // Active modules
    int total_operations;               // Total operations
    time_t last_check;                  // Last check timestamp
    double system_health;               // Overall system health
    int active_module_count;            // Number of active modules
    gps_connector_module_t modules[20]; // GPS modules
} gps_connector_status_t;

// GPS connector system state
typedef struct {
    bool enabled;                       // Connector enabled
    int max_modules;                    // Maximum modules
    int check_interval;                 // Check interval
    int health_timeout;                 // Health timeout
    double health_threshold;            // Health threshold
    
    // State
    int module_count;                   // Module count
    int active_modules;                 // Active modules
    int total_operations;               // Total operations
    time_t last_check;                  // Last check
    double system_health;               // System health
    
    // GPS modules
    gps_connector_module_t modules[20]; // GPS modules
} gps_connector_t;

// Function prototypes

/**
 * Initialize GPS connector system
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_connector_init(void);

/**
 * Register GPS module
 * @param name Module name
 * @param module_type Type of GPS module
 * @return Module ID on success, error code on failure
 */
int gps_connector_register_module(const char *name, gps_module_type_t module_type);

/**
 * Update module operation
 * @param module_id Module ID
 * @param operation_successful Whether operation was successful
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_connector_update_module_operation(int module_id, bool operation_successful);

/**
 * Get connector status
 * @param status Status structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_connector_get_status(gps_connector_status_t *status);

/**
 * Get connector configuration
 * @param config Configuration structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_connector_get_config(gps_connector_config_t *config);

/**
 * Set connector configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_connector_set_config(const gps_connector_config_t *config);

/**
 * Enable/disable connector
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_connector_set_enabled(bool enabled);

/**
 * Enable/disable specific module
 * @param module_id Module ID
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_connector_set_module_enabled(int module_id, bool enabled);

/**
 * Unregister module
 * @param module_id Module ID to unregister
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_connector_unregister_module(int module_id);

/**
 * Reset connector
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_connector_reset(void);

/**
 * Cleanup connector
 */
void gps_connector_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_CONNECTOR_H
