#ifndef GPS_CELL_TOWER_H
#define GPS_CELL_TOWER_H

#include "../core/types.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// Cell network types
typedef enum {
    CELL_NETWORK_TYPE_UNKNOWN = 0,
    CELL_NETWORK_TYPE_GSM,
    CELL_NETWORK_TYPE_CDMA,
    CELL_NETWORK_TYPE_UMTS,
    CELL_NETWORK_TYPE_LTE,
    CELL_NETWORK_TYPE_5G,
    CELL_NETWORK_TYPE_WIFI,
    CELL_NETWORK_TYPE_BLUETOOTH,
    CELL_NETWORK_TYPE_MAX
} cell_network_type_t;

// Cell position methods
typedef enum {
    CELL_POSITION_METHOD_UNKNOWN = 0,
    CELL_POSITION_METHOD_SINGLE_TOWER,
    CELL_POSITION_METHOD_TRIANGULATION,
    CELL_POSITION_METHOD_MULTILATERATION
} cell_position_method_t;

// Cell tower information
typedef struct {
    cell_network_type_t network_type;   // Network type
    double signal_strength;             // Signal strength in dBm
    double distance;                    // Distance to tower in meters
    double lat;                         // Tower latitude
    double lon;                         // Tower longitude
    int cell_id;                        // Cell ID
    int lac;                            // Location Area Code
    int mcc;                            // Mobile Country Code
    int mnc;                            // Mobile Network Code
} gps_cell_tower_info_t;

// Cell tower record
typedef struct {
    bool active;                        // Whether tower is active
    int tower_id;                       // Unique tower identifier
    cell_network_type_t network_type;   // Network type
    double signal_strength;             // Signal strength in dBm
    double distance;                    // Distance to tower in meters
    time_t last_seen;                   // Last time tower was seen
    double lat;                         // Tower latitude
    double lon;                         // Tower longitude
    int cell_id;                        // Cell ID
    int lac;                            // Location Area Code
    int mcc;                            // Mobile Country Code
    int mnc;                            // Mobile Network Code
} gps_cell_tower_record_t;

// Cell position record
typedef struct {
    time_t timestamp;                   // Position timestamp
    double lat;                         // Latitude
    double lon;                         // Longitude
    double accuracy;                    // Accuracy in meters
    int tower_count;                    // Number of towers used
    cell_position_method_t method;      // Positioning method used
} gps_cell_position_record_t;

// Cell tower configuration
typedef struct {
    bool enabled;                       // Enable/disable positioning
    int max_towers;                     // Maximum towers to track
    double max_distance;                // Maximum tower distance
    double min_signal_strength;         // Minimum signal strength
    int position_update_interval;       // Position update interval
    int max_position_history;           // Maximum position history
} gps_cell_tower_config_t;

// Cell tower status
typedef struct {
    bool enabled;                       // Positioning enabled
    int tower_count;                    // Total tower count
    int active_towers;                  // Active tower count
    time_t last_position_update;        // Last position update
    int total_position_updates;         // Total position updates
    double current_lat;                 // Current latitude
    double current_lon;                 // Current longitude
    double current_accuracy;            // Current accuracy
    int active_tower_count;             // Number of active towers in info
    gps_cell_tower_record_t active_towers_info[50]; // Active tower information
} gps_cell_tower_status_t;

// Cell tower statistics
typedef struct {
    int total_towers;                   // Total towers
    int total_position_updates;         // Total position updates
    double current_accuracy;            // Current accuracy
    double total_signal_strength;       // Total signal strength
    double total_distance;              // Total distance
    double average_signal_strength;     // Average signal strength
    double average_distance;            // Average distance
    int tower_counts[CELL_NETWORK_TYPE_MAX]; // Tower counts by network type
} gps_cell_tower_stats_t;

// Cell tower system state
typedef struct {
    bool enabled;                       // Positioning enabled
    int max_towers;                     // Maximum towers
    double max_distance;                // Maximum distance
    double min_signal_strength;         // Minimum signal strength
    int position_update_interval;       // Update interval
    int max_position_history;           // Position history
    
    // State
    int tower_count;                    // Tower count
    int active_towers;                  // Active towers
    time_t last_position_update;        // Last update
    int total_position_updates;         // Total updates
    double current_lat;                 // Current latitude
    double current_lon;                 // Current longitude
    double current_accuracy;            // Current accuracy
    
    // Cell towers
    gps_cell_tower_record_t cell_towers[100]; // Cell tower records
    
    // Position history
    gps_cell_position_record_t position_history[1000]; // Position history
} gps_cell_tower_t;

// Function prototypes

/**
 * Initialize cell tower positioning
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_cell_tower_init(void);

/**
 * Add cell tower information
 * @param tower_info Cell tower information
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_cell_tower_add_tower(const gps_cell_tower_info_t *tower_info);

/**
 * Update cell tower positioning
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_cell_tower_update_position(void);

/**
 * Get cell tower position
 * @param lat Latitude (output)
 * @param lon Longitude (output)
 * @param accuracy Accuracy in meters (output)
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_cell_tower_get_position(double *lat, double *lon, double *accuracy);

/**
 * Get cell tower status
 * @param status Status structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_cell_tower_get_status(gps_cell_tower_status_t *status);

/**
 * Get cell tower configuration
 * @param config Configuration structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_cell_tower_get_config(gps_cell_tower_config_t *config);

/**
 * Set cell tower configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_cell_tower_set_config(const gps_cell_tower_config_t *config);

/**
 * Enable/disable cell tower positioning
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_cell_tower_set_enabled(bool enabled);

/**
 * Force position update
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_cell_tower_force_position_update(void);

/**
 * Get cell tower statistics
 * @param stats Statistics structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_cell_tower_get_statistics(gps_cell_tower_stats_t *stats);

/**
 * Reset cell tower positioning
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_cell_tower_reset(void);

/**
 * Cleanup cell tower positioning
 */
void gps_cell_tower_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_CELL_TOWER_H
