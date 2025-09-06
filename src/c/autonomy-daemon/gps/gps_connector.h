#ifndef GPS_CONNECTOR_H
#define GPS_CONNECTOR_H

#include "../core/types.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

// Note: gps_module_type_t is defined in ../core/types.h

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

// Note: gps_connector_status_t is defined in ../core/types.h

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
