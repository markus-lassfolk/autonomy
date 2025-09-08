#ifndef NETWORK_FAILOVER_SECURE_H
#define NETWORK_FAILOVER_SECURE_H

#include "../core/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Secure MWAN3 interface status update
 * @param interface_name Name of the interface
 * @param status Status to set (online, offline, standby)
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int secure_mwan3_set_status(const char *interface_name, const char *status);

/**
 * Secure interface bring up
 * @param interface_name Name of the interface to bring up
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int secure_interface_up(const char *interface_name);

/**
 * Secure interface bring down
 * @param interface_name Name of the interface to bring down
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int secure_interface_down(const char *interface_name);

/**
 * Secure route addition
 * @param target Target network/IP
 * @param gateway Gateway IP
 * @param interface Interface name
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int secure_route_add(const char *target, const char *gateway, const char *interface);

/**
 * Secure route deletion
 * @param target Target network/IP
 * @param gateway Gateway IP (optional)
 * @param interface Interface name (optional)
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int secure_route_del(const char *target, const char *gateway, const char *interface);

/**
 * Secure network reload
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int secure_network_reload(void);

/**
 * Secure UCI network configuration set
 * @param section UCI section name
 * @param option UCI option name
 * @param value Value to set
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int secure_uci_network_set(const char *section, const char *option, const char *value);

/**
 * Secure UCI network configuration commit
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int secure_uci_network_commit(void);

#ifdef __cplusplus
}
#endif

#endif // NETWORK_FAILOVER_SECURE_H
