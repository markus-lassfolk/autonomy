#ifndef GPS_STARLINK_H
#define GPS_STARLINK_H

#include "../core/types.h"
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
static void* starlink_gps_monitor_thread(void *arg);
static bool extract_gps_from_starlink_api(void);
static bool parse_gps_from_response(const char *response);
static void calculate_gps_reliability(void);

// Starlink GPS configuration
typedef struct {
    bool enabled;                    // Enable/disable Starlink GPS
    int update_interval;             // Update interval in seconds
    int timeout;                     // API timeout in seconds
    char starlink_ip[16];            // Starlink dish IP address
    int starlink_port;               // Starlink dish port
} gps_starlink_config_t;

// Starlink GPS status
typedef struct {
    bool enabled;                    // Starlink GPS enabled
    int update_interval;             // Current update interval
    int timeout;                     // Current timeout
    time_t last_update;              // Last update timestamp
    int total_updates;               // Total updates performed
    int successful_updates;          // Successful updates
    int failed_updates;              // Failed updates
    char starlink_ip[16];            // Current Starlink IP
    int starlink_port;               // Current Starlink port
} gps_starlink_status_t;

// Starlink GPS system state
typedef struct {
    bool enabled;                    // Starlink GPS enabled
    int update_interval;             // Update interval in seconds
    int timeout;                     // API timeout in seconds
    time_t last_update;              // Last update timestamp
    int total_updates;               // Total updates performed
    int successful_updates;          // Successful updates
    int failed_updates;              // Failed updates
    char starlink_ip[16];            // Starlink dish IP address
    int starlink_port;               // Starlink dish port
    gps_data_t gps_data;             // Current GPS data
} gps_starlink_t;

// Function prototypes

/**
 * Initialize Starlink GPS system
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_starlink_init(void);

/**
 * Check if Starlink GPS is initialized
 * @return true if initialized, false otherwise
 */
bool gps_starlink_is_initialized(void);

/**
 * Start Starlink GPS monitoring thread
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_starlink_start_monitoring(void);

/**
 * Stop Starlink GPS monitoring
 */
void gps_starlink_stop_monitoring(void);

/**
 * Extract GPS data from Starlink dish
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_starlink_extract_data(void);

/**
 * Get Starlink GPS data
 * @param gps_data GPS data structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_starlink_get_data(gps_data_t *gps_data);

/**
 * Get Starlink GPS status
 * @param status Status structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_starlink_get_status(gps_starlink_status_t *status);

/**
 * Set Starlink GPS configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_starlink_set_config(const gps_starlink_config_t *config);

/**
 * Enable/disable Starlink GPS
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_starlink_set_enabled(bool enabled);

/**
 * Check if GPS data is recent
 * @param max_age_seconds Maximum age in seconds
 * @return True if data is recent, false otherwise
 */
bool gps_starlink_is_data_recent(int max_age_seconds);

/**
 * Check if GPS data meets accuracy requirements
 * @param required_accuracy Required accuracy in meters
 * @return True if accuracy meets requirements, false otherwise
 */
bool gps_starlink_meets_accuracy(double required_accuracy);

/**
 * Force immediate GPS update
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_starlink_force_update(void);

/**
 * Cleanup Starlink GPS system
 */
void gps_starlink_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_STARLINK_H