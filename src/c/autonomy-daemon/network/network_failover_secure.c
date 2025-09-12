#include "network_failover_secure.h"
#include "../utils/secure_exec.h"
#include "../shared/logging/logx.h"
#include "../core/types.h"
#include <string.h>
#include <stdio.h>

// Forward declarations
static bool is_valid_ip_address(const char *ip);

// Secure MWAN3 interface status update
int secure_mwan3_set_status(const char *interface_name, const char *status) {
    if (!interface_name || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Validate status values
    if (strcmp(status, "online") != 0 && 
        strcmp(status, "offline") != 0 && 
        strcmp(status, "standby") != 0) {
        LOGX_ERROR_MSG("Invalid MWAN3 status: %s", status);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Build UBUS command arguments
    char ubus_args[512];
    snprintf(ubus_args, sizeof(ubus_args), 
             "call mwan3 set_status '{\"interface\":\"%s\",\"status\":\"%s\"}'", 
             interface_name, status);
    
    exec_result_t result;
    int ret = secure_exec_command("ubus", &result);
    if (ret != AUTONOMY_SUCCESS) {
        LOGX_ERROR_MSG("Failed to execute UBUS command: %s", result.error);
        return ret;
    }
    
    if (!result.success) {
        LOGX_WARN_MSG("MWAN3 status update failed for interface %s: %s", 
                     interface_name, result.error);
        return AUTONOMY_ERROR_NETWORK;
    }
    
    LOGX_DEBUG_MSG("MWAN3 status updated successfully for interface %s to %s", 
                  interface_name, status);
    return AUTONOMY_SUCCESS;
}

// Secure interface bring up
int secure_interface_up(const char *interface_name) {
    if (!interface_name) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Validate interface name (basic security check)
    if (strlen(interface_name) > 32 || strstr(interface_name, "..") || 
        strstr(interface_name, "/") || strstr(interface_name, ";")) {
        LOGX_ERROR_MSG("Invalid interface name: %s", interface_name);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    char command[256];
    snprintf(command, sizeof(command), "ifup %s", interface_name);
    
    exec_result_t result;
    int ret = secure_exec_command(command, &result);
    if (ret != AUTONOMY_SUCCESS) {
        LOGX_ERROR_MSG("Failed to bring up interface %s: %s", interface_name, result.error);
        return ret;
    }
    
    if (!result.success) {
        LOGX_WARN_MSG("Interface %s bring up failed: %s", interface_name, result.error);
        return AUTONOMY_ERROR_NETWORK;
    }
    
    LOGX_DEBUG_MSG("Interface %s brought up successfully", interface_name);
    return AUTONOMY_SUCCESS;
}

// Secure interface bring down
int secure_interface_down(const char *interface_name) {
    if (!interface_name) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Validate interface name (basic security check)
    if (strlen(interface_name) > 32 || strstr(interface_name, "..") || 
        strstr(interface_name, "/") || strstr(interface_name, ";")) {
        LOGX_ERROR_MSG("Invalid interface name: %s", interface_name);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    char command[256];
    snprintf(command, sizeof(command), "ifdown %s", interface_name);
    
    exec_result_t result;
    int ret = secure_exec_command(command, &result);
    if (ret != AUTONOMY_SUCCESS) {
        LOGX_ERROR_MSG("Failed to bring down interface %s: %s", interface_name, result.error);
        return ret;
    }
    
    if (!result.success) {
        LOGX_WARN_MSG("Interface %s bring down failed: %s", interface_name, result.error);
        return AUTONOMY_ERROR_NETWORK;
    }
    
    LOGX_DEBUG_MSG("Interface %s brought down successfully", interface_name);
    return AUTONOMY_SUCCESS;
}

// Secure route management
int secure_route_add(const char *target, const char *gateway, const char *interface) {
    if (!target || !gateway || !interface) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Validate IP addresses and interface name
    if (!is_valid_ip_address(target) || !is_valid_ip_address(gateway)) {
        LOGX_ERROR_MSG("Invalid IP address: target=%s, gateway=%s", target, gateway);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (strlen(interface) > 32 || strstr(interface, "..") || 
        strstr(interface, "/") || strstr(interface, ";")) {
        LOGX_ERROR_MSG("Invalid interface name: %s", interface);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    char command[512];
    snprintf(command, sizeof(command), "ip route add %s via %s dev %s", 
             target, gateway, interface);
    
    exec_result_t result;
    int ret = secure_exec_command(command, &result);
    if (ret != AUTONOMY_SUCCESS) {
        LOGX_ERROR_MSG("Failed to add route: %s", result.error);
        return ret;
    }
    
    if (!result.success) {
        LOGX_WARN_MSG("Route addition failed: %s", result.error);
        return AUTONOMY_ERROR_NETWORK;
    }
    
    LOGX_DEBUG_MSG("Route added successfully: %s via %s dev %s", target, gateway, interface);
    return AUTONOMY_SUCCESS;
}

// Secure route deletion
int secure_route_del(const char *target, const char *gateway, const char *interface) {
    if (!target) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Validate IP address
    if (!is_valid_ip_address(target)) {
        LOGX_ERROR_MSG("Invalid IP address: %s", target);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    char command[512];
    if (gateway && interface) {
        snprintf(command, sizeof(command), "ip route del %s via %s dev %s", 
                 target, gateway, interface);
    } else {
        snprintf(command, sizeof(command), "ip route del %s", target);
    }
    
    exec_result_t result;
    int ret = secure_exec_command(command, &result);
    if (ret != AUTONOMY_SUCCESS) {
        LOGX_ERROR_MSG("Failed to delete route: %s", result.error);
        return ret;
    }
    
    if (!result.success) {
        LOGX_WARN_MSG("Route deletion failed: %s", result.error);
        return AUTONOMY_ERROR_NETWORK;
    }
    
    LOGX_DEBUG_MSG("Route deleted successfully: %s", target);
    return AUTONOMY_SUCCESS;
}

// Secure network reload
int secure_network_reload(void) {
    exec_result_t result;
    int ret = secure_exec_command("/etc/init.d/network reload", &result);
    if (ret != AUTONOMY_SUCCESS) {
        LOGX_ERROR_MSG("Failed to reload network: %s", result.error);
        return ret;
    }
    
    if (!result.success) {
        LOGX_WARN_MSG("Network reload failed: %s", result.error);
        return AUTONOMY_ERROR_NETWORK;
    }
    
    LOGX_DEBUG_MSG("Network reloaded successfully");
    return AUTONOMY_SUCCESS;
}

// Secure UCI network configuration
int secure_uci_network_set(const char *section, const char *option, const char *value) {
    if (!section || !option || !value) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Validate section and option names
    if (strlen(section) > 64 || strlen(option) > 64 || strlen(value) > 256) {
        LOGX_ERROR_MSG("Section, option, or value too long");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Check for dangerous characters
    if (strstr(section, "..") || strstr(option, "..") || strstr(value, "..") ||
        strstr(section, ";") || strstr(option, ";") || strstr(value, ";")) {
        LOGX_ERROR_MSG("Invalid characters in UCI parameters");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    char uci_args[512];
    snprintf(uci_args, sizeof(uci_args), "set %s.%s=%s", section, option, value);
    
    exec_result_t result;
    int ret = secure_uci_command(uci_args, &result);
    if (ret != AUTONOMY_SUCCESS) {
        LOGX_ERROR_MSG("Failed to set UCI network config: %s", result.error);
        return ret;
    }
    
    if (!result.success) {
        LOGX_WARN_MSG("UCI network config set failed: %s", result.error);
        return AUTONOMY_ERROR_NETWORK;
    }
    
    LOGX_DEBUG_MSG("UCI network config set successfully: %s.%s=%s", section, option, value);
    return AUTONOMY_SUCCESS;
}

// Secure UCI network commit
int secure_uci_network_commit(void) {
    exec_result_t result;
    int ret = secure_uci_command("commit network", &result);
    if (ret != AUTONOMY_SUCCESS) {
        LOGX_ERROR_MSG("Failed to commit UCI network config: %s", result.error);
        return ret;
    }
    
    if (!result.success) {
        LOGX_WARN_MSG("UCI network config commit failed: %s", result.error);
        return AUTONOMY_ERROR_NETWORK;
    }
    
    LOGX_DEBUG_MSG("UCI network config committed successfully");
    return AUTONOMY_SUCCESS;
}

// Validate IP address format
static bool is_valid_ip_address(const char *ip) {
    if (!ip) return false;
    
    // Basic IPv4 validation
    int a, b, c, d;
    if (sscanf(ip, "%d.%d.%d.%d", &a, &b, &c, &d) == 4) {
        return (a >= 0 && a <= 255 && b >= 0 && b <= 255 && 
                c >= 0 && c <= 255 && d >= 0 && d <= 255);
    }
    
    // Basic IPv6 validation (simplified)
    if (strchr(ip, ':') != NULL) {
        return (strlen(ip) <= 39); // Max IPv6 length
    }
    
    return false;
}
