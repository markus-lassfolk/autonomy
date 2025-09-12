#ifndef CELLULAR_COLLECTOR_H
#define CELLULAR_COLLECTOR_H

#include "../../core/types.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// Cellular network types
typedef enum {
    CELLULAR_NETWORK_TYPE_UNKNOWN = 0,
    CELLULAR_NETWORK_TYPE_GSM,
    CELLULAR_NETWORK_TYPE_2G,
    CELLULAR_NETWORK_TYPE_3G,
    CELLULAR_NETWORK_TYPE_UMTS,
    CELLULAR_NETWORK_TYPE_LTE,
    CELLULAR_NETWORK_TYPE_5G,
    CELLULAR_NETWORK_TYPE_CDMA,
    CELLULAR_NETWORK_TYPE_MAX
} cellular_network_type_t;

// Cellular connection states
typedef enum {
    CELLULAR_STATE_DISCONNECTED = 0,
    CELLULAR_STATE_CONNECTING,
    CELLULAR_STATE_CONNECTED,
    CELLULAR_STATE_RECONNECTING,
    CELLULAR_STATE_ERROR,
    CELLULAR_STATE_UNKNOWN,
    CELLULAR_STATE_SEARCHING,
    CELLULAR_STATE_DENIED,
    CELLULAR_STATE_MAX
} cellular_connection_state_t;

// Cellular roaming types
typedef enum {
    ROAMING_TYPE_NONE = 0,
    ROAMING_TYPE_NATIONAL,
    ROAMING_TYPE_INTERNATIONAL,
    ROAMING_TYPE_DOMESTIC,
    ROAMING_TYPE_MAX
} cellular_roaming_type_t;

// Cellular modem types
typedef enum {
    MODEM_TYPE_UNKNOWN = 0,
    MODEM_TYPE_QMI,
    MODEM_TYPE_MBIM,
    MODEM_TYPE_NCM,
    MODEM_TYPE_PPP,
    MODEM_TYPE_MAX
} cellular_modem_type_t;

// Cellular SIM status
typedef enum {
    CELLULAR_SIM_STATUS_UNKNOWN = 0,
    CELLULAR_SIM_STATUS_READY,
    CELLULAR_SIM_STATUS_PIN_REQUIRED,
    CELLULAR_SIM_STATUS_PUK_REQUIRED,
    CELLULAR_SIM_STATUS_ERROR,
    CELLULAR_SIM_STATUS_MAX
} cellular_sim_status_t;

// Comprehensive cellular information
typedef struct {
    // Signal metrics
    int rsrp;                           // Reference Signal Received Power (dBm)
    int rsrq;                           // Reference Signal Received Quality (dB)
    int sinr;                           // Signal to Interference plus Noise Ratio (dB)
    int rssi;                           // Received Signal Strength Indicator (dBm)
    bool has_rsrp;
    bool has_rsrq;
    bool has_sinr;
    bool has_rssi;
    
    // Network information
    cellular_network_type_t network_type;
    char operator_name[64];
    char band[16];
    char cell_id[32];
    bool roaming;
    cellular_roaming_type_t roaming_type;
    char home_operator[64];
    
    // Multi-SIM support
    int sim_slot;
    int sim_count;
    int active_sim;
    char sim_status[32];
    
    // Device information
    char manufacturer[64];
    char model[64];
    char imei[32];
    char iccid[32];
    char mcc[8];
    char mnc[8];
    
    // Connection details
    cellular_modem_type_t modem_type;
    char ip_address[16];
    char gateway[16];
    char dns_servers[256];
    cellular_connection_state_t connection_state;
    
    // Quality metrics
    double signal_quality;              // 0-100 score
    double stability_score;             // 0-100 stability score
    double predictive_risk;             // 0-1 risk of failure
    
    // Advanced metrics
    char tac[16];                       // Tracking Area Code
    int lac;                            // Location Area Code
    int earfcn;                         // E-UTRA Absolute Radio Frequency Channel Number
    int pci;                            // Physical Cell ID
    bool has_cell_info;                 // Whether cell info is available
    
    // Data usage
    uint64_t tx_bytes;
    uint64_t rx_bytes;
    bool has_data_usage;
    
    // Temperature and power
    double temperature;
    int power_level;
    bool has_temperature;
    bool has_power_level;
    
    // Stability metrics
    int cell_changes;                   // Cell changes in monitoring window
    double signal_variance;             // Signal strength variance
    double throughput_kbps;             // Calculated throughput
    
    // Collection metadata
    time_t timestamp;
    char modem_device[64];              // e.g., "/dev/ttyUSB0"
    char interface_name[32];            // e.g., "mob1s1a1"
} cellular_info_t;

// Cellular collector configuration
typedef struct {
    bool enabled;
    char modem_device[64];
    char interface_name[32];
    int collection_interval;
    int timeout_seconds;
    bool enable_stability_monitoring;
    bool enable_predictive_analysis;
    int stability_window_size;
    double stability_threshold;
    int max_cell_changes;
    double signal_variance_threshold;
} cellular_collector_config_t;

// Cellular collector statistics
typedef struct {
    int total_collections;
    int successful_collections;
    int failed_collections;
    time_t last_collection;
    double average_rsrp;
    double average_rsrq;
    double average_sinr;
    int cell_change_count;
    double stability_score;
} cellular_collector_stats_t;

// Cellular collector structure
typedef struct {
    cellular_collector_config_t config;
    cellular_collector_stats_t stats;
    cellular_info_t last_info;
    
    // Stability tracking
    int rsrp_history[100];
    int rsrq_history[100];
    int sinr_history[100];
    int history_count;
    int history_index;
    
    // Cell change tracking
    char last_cell_id[32];
    time_t last_cell_change;
    
    // Thread safety
    pthread_mutex_t mutex;
} cellular_collector_t;

// Function prototypes

/**
 * Initialize cellular collector
 * @param config Collector configuration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int cellular_collector_init(const cellular_collector_config_t* config);

/**
 * Cleanup cellular collector
 */
void cellular_collector_cleanup(void);

/**
 * Collect cellular metrics
 * @param info Structure to fill with cellular information
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int cellular_collector_collect(cellular_info_t* info);

/**
 * Get cellular collector statistics
 * @param stats Structure to fill with statistics
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int cellular_collector_get_stats(cellular_collector_stats_t* stats);

/**
 * Get cellular collector configuration
 * @param config Structure to fill with configuration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int cellular_collector_get_config(cellular_collector_config_t* config);

/**
 * Set cellular collector configuration
 * @param config New configuration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int cellular_collector_set_config(const cellular_collector_config_t* config);

/**
 * Enable/disable cellular collector
 * @param enabled Whether to enable collection
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int cellular_collector_set_enabled(bool enabled);

/**
 * Reset cellular collector statistics
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int cellular_collector_reset_stats(void);

/**
 * Force immediate collection
 * @param info Structure to fill with cellular information
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int cellular_collector_force_collect(cellular_info_t* info);

/**
 * Check if cellular collector is initialized
 * @return true if initialized, false otherwise
 */
bool cellular_collector_is_initialized(void);

/**
 * Get cellular signal quality score (0-100)
 * @param info Cellular information
 * @return Quality score (0-100)
 */
double cellular_collector_calculate_quality_score(const cellular_info_t* info);

/**
 * Calculate stability score based on signal variance
 * @param info Cellular information
 * @return Stability score (0-100)
 */
double cellular_collector_calculate_stability_score(const cellular_info_t* info);

/**
 * Calculate predictive risk score
 * @param info Cellular information
 * @return Risk score (0-1, higher means higher risk)
 */
double cellular_collector_calculate_predictive_risk(const cellular_info_t* info);

#ifdef __cplusplus
}
#endif

#endif // CELLULAR_COLLECTOR_H