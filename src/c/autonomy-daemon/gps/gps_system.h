#ifndef GPS_SYSTEM_H
#define GPS_SYSTEM_H

#include "types.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// GPS system constants
#define GPS_MAX_MODULES 20

// GPS module types (matching gps_connector.h)
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
    GPS_MODULE_TYPE_LOCATION_SERVICES
} gps_module_type_t;

// GPS module status
typedef struct {
    gps_module_type_t module_type;      // Type of GPS module
    bool initialized;                    // Whether module is initialized
    bool enabled;                        // Whether module is enabled
    double health_score;                 // Module health score (0-100)
    time_t last_operation;               // Last operation timestamp
    int error_count;                     // Total error count
} gps_module_status_t;

// GPS system configuration
typedef struct {
    bool enabled;                        // Enable/disable GPS system
    int init_timeout;                    // Initialization timeout in seconds
    int health_check_interval;           // Health check interval in seconds
    double min_health;                   // Minimum system health threshold
} gps_system_config_t;

// GPS system status
typedef struct {
    bool enabled;                        // GPS system enabled
    bool init_complete;                  // Whether initialization is complete
    time_t init_start_time;              // Initialization start timestamp
    time_t init_complete_time;           // Initialization completion timestamp
    int module_count;                    // Total modules
    int active_modules;                  // Active modules
    double system_health;                // Overall system health
    time_t last_health_check;            // Last health check timestamp
    int active_module_count;             // Number of active modules
    gps_module_status_t module_status[GPS_MAX_MODULES]; // Module status
} gps_system_status_t;

// GPS system state
typedef struct {
    bool enabled;                        // GPS system enabled
    int init_timeout;                    // Initialization timeout
    int health_check_interval;           // Health check interval
    double min_health;                   // Minimum health threshold
    
    // State
    time_t init_start_time;              // Initialization start time
    bool init_complete;                  // Initialization complete
    time_t init_complete_time;           // Initialization complete time
    int module_count;                    // Module count
    int active_modules;                  // Active modules
    double system_health;                // System health
    time_t last_health_check;            // Last health check
    
    // Module status
    gps_module_status_t module_status[GPS_MAX_MODULES]; // Module status
} gps_system_t;

// Function prototypes

/**
 * Initialize GPS system
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_system_init(void);

/**
 * Perform GPS system health check
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_system_health_check(void);

/**
 * Get GPS system status
 * @param status Status structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_system_get_status(gps_system_status_t *status);

/**
 * Get GPS system configuration
 * @param config Configuration structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_system_get_config(gps_system_config_t *config);

/**
 * Set GPS system configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_system_set_config(const gps_system_config_t *config);

/**
 * Enable/disable GPS system
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_system_set_enabled(bool enabled);

/**
 * Reset GPS system
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_system_reset(void);

/**
 * Cleanup GPS system
 */
void gps_system_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_SYSTEM_H
