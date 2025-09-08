#ifndef CELLULAR_DEVICE_HELPER_H
#define CELLULAR_DEVICE_HELPER_H

#include "../core/types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get cellular device path dynamically for AT commands
 * @param device_path Buffer to store the device path
 * @param path_size Size of the device_path buffer
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int get_dynamic_cellular_device_path(char *device_path, size_t path_size);

/**
 * Execute AT command with dynamic device path
 * @param at_command AT command to execute
 * @param response Buffer to store the response
 * @param response_size Size of the response buffer
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int execute_at_command_dynamic(const char *at_command, char *response, size_t response_size);

/**
 * Get signal strength using dynamic device path
 * @param rssi Pointer to store RSSI value
 * @param ber Pointer to store BER value
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int get_signal_strength_dynamic(int *rssi, int *ber);

/**
 * Get operator information using dynamic device path
 * @param operator_name Buffer to store operator name
 * @param name_size Size of the operator_name buffer
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int get_operator_info_dynamic(char *operator_name, size_t name_size);

#ifdef __cplusplus
}
#endif

#endif // CELLULAR_DEVICE_HELPER_H
