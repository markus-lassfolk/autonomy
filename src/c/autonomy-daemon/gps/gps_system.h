#ifndef GPS_SYSTEM_H
#define GPS_SYSTEM_H

#include "../core/types.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

// GPS system constants
#define GPS_MAX_MODULES 20

// Note: gps_module_type_t is defined in ../core/types.h

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
 * Get current GPS location
 * @param location Location structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_get_current_location(gps_data_t *location);

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
