#ifndef CELLULAR_DEVICE_HELPER_H
#define CELLULAR_DEVICE_HELPER_H

#include "../core/types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Cellular device information
typedef struct {
    char device_path[64];           // Device path (e.g., /dev/ttyUSB2)
    char modem_model[64];           // Modem model
    char sim_id[16];                // SIM ID
    char operator_name[32];         // Network operator
    bool is_available;              // Device is available and responsive
    int signal_strength_dbm;        // Signal strength in dBm
    int signal_quality;             // Signal quality (0-100)
} cellular_device_info_t;

/**
 * Discover cellular devices dynamically
 * @param devices Array to store discovered devices
 * @param max_devices Maximum number of devices to discover
 * @param actual_count Actual number of devices found
 * @return AUTONOMY_SUCCESS on success
 */
int discover_cellular_devices(cellular_device_info_t *devices, int max_devices, int *actual_count);

/**
 * Get cellular device path for a specific interface
 * @param interface_name Interface name (e.g., "qmimux0")
 * @param device_path Output buffer for device path
 * @param path_size Size of device_path buffer
 * @return AUTONOMY_SUCCESS on success
 */
int get_cellular_device_path(const char *interface_name, char *device_path, size_t path_size);

/**
 * Get signal strength from cellular device
 * @param device_path Device path
 * @param rssi Output RSSI value
 * @param ber Output BER value
 * @return AUTONOMY_SUCCESS on success
 */
int get_signal_strength(const char *device_path, int *rssi, int *ber);

/**
 * Get signal strength using dynamic device discovery
 * @param rssi Output RSSI value
 * @param ber Output BER value
 * @return AUTONOMY_SUCCESS on success
 */
int get_signal_strength_dynamic(int *rssi, int *ber);

/**
 * Get network operator information
 * @param device_path Device path
 * @param operator_name Output buffer for operator name
 * @param name_size Size of operator_name buffer
 * @return AUTONOMY_SUCCESS on success
 */
int get_network_operator(const char *device_path, char *operator_name, size_t name_size);

/**
 * Get LTE metrics (RSRP, RSRQ, SINR)
 * @param device_path Device path
 * @param rsrp Output RSRP value
 * @param rsrq Output RSRQ value
 * @param sinr Output SINR value
 * @return AUTONOMY_SUCCESS on success
 */
int get_lte_metrics(const char *device_path, int *rsrp, int *rsrq, int *sinr);

/**
 * Check if device is a cellular modem
 * @param device_path Device path to check
 * @return true if device is a cellular modem
 */
bool is_cellular_device(const char *device_path);

#ifdef __cplusplus
}
#endif

#endif // CELLULAR_DEVICE_HELPER_H
