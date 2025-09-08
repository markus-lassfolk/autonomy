#include "cellular_device_helper.h"
#include "network_discovery_comprehensive.h"
#include "../utils/logx.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

// Get cellular device path dynamically for AT commands
int get_dynamic_cellular_device_path(char *device_path, size_t path_size) {
    if (!device_path || path_size == 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Try to get device path from network discovery
    network_interface_t interfaces[MAX_INTERFACES];
    int interface_count = 0;
    
    int ret = get_comprehensive_interface_info(interfaces, &interface_count);
    if (ret == AUTONOMY_SUCCESS) {
        // Find the first cellular interface
        for (int i = 0; i < interface_count; i++) {
            if (strcmp(interfaces[i].type, "cellular") == 0 && 
                strlen(interfaces[i].cellular_device_path) > 0) {
                
                strncpy(device_path, interfaces[i].cellular_device_path, path_size - 1);
                device_path[path_size - 1] = '\0';
                LOGX_DEBUG_MSG("Using dynamic cellular device path: %s", device_path);
                return AUTONOMY_SUCCESS;
            }
        }
    }
    
    // Fallback to hardcoded default if discovery fails
    strncpy(device_path, "/dev/ttyUSB2", path_size - 1);
    device_path[path_size - 1] = '\0';
    LOGX_WARN_MSG("Using fallback cellular device path: %s", device_path);
    return AUTONOMY_ERROR_NOT_FOUND;
}

// Execute AT command with dynamic device path
int execute_at_command_dynamic(const char *at_command, char *response, size_t response_size) {
    if (!at_command || !response || response_size == 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    char device_path[64];
    int ret = get_dynamic_cellular_device_path(device_path, sizeof(device_path));
    if (ret != AUTONOMY_SUCCESS) {
        return ret;
    }
    
    // Build command with dynamic device path
    char command[512];
    snprintf(command, sizeof(command), 
             "echo '%s' | timeout 5 microcom -t 1000 %s 2>/dev/null", 
             at_command, device_path);
    
    LOGX_DEBUG_MSG("Executing AT command: %s on device: %s", at_command, device_path);
    
    FILE *fp = popen(command, "r");
    if (!fp) {
        LOGX_ERROR_MSG("Failed to execute AT command");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Read response
    size_t total_read = 0;
    char line[256];
    while (fgets(line, sizeof(line), fp) && total_read < response_size - 1) {
        size_t line_len = strlen(line);
        if (total_read + line_len < response_size) {
            strcpy(response + total_read, line);
            total_read += line_len;
        }
    }
    
    pclose(fp);
    response[total_read] = '\0';
    
    LOGX_DEBUG_MSG("AT command response: %s", response);
    return AUTONOMY_SUCCESS;
}

// Get signal strength using dynamic device path
int get_signal_strength_dynamic(int *rssi, int *ber) {
    if (!rssi || !ber) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    char response[256];
    int ret = execute_at_command_dynamic("AT+CSQ", response, sizeof(response));
    if (ret != AUTONOMY_SUCCESS) {
        return ret;
    }
    
    // Parse CSQ response (format: +CSQ: <rssi>,<ber>)
    char *csq_start = strstr(response, "+CSQ:");
    if (csq_start) {
        if (sscanf(csq_start, "+CSQ: %d,%d", rssi, ber) == 2) {
            LOGX_DEBUG_MSG("Signal strength: RSSI=%d, BER=%d", *rssi, *ber);
            return AUTONOMY_SUCCESS;
        }
    }
    
    LOGX_WARN_MSG("Failed to parse signal strength response: %s", response);
    return AUTONOMY_ERROR_PARSE;
}

// Get operator information using dynamic device path
int get_operator_info_dynamic(char *operator_name, size_t name_size) {
    if (!operator_name || name_size == 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    char response[256];
    int ret = execute_at_command_dynamic("AT+COPS?", response, sizeof(response));
    if (ret != AUTONOMY_SUCCESS) {
        return ret;
    }
    
    // Parse COPS response (format: +COPS: <mode>,<format>,<oper>)
    char *cops_start = strstr(response, "+COPS:");
    if (cops_start) {
        char *oper_start = strchr(cops_start, '"');
        if (oper_start) {
            oper_start++; // Skip opening quote
            char *oper_end = strchr(oper_start, '"');
            if (oper_end) {
                size_t oper_len = oper_end - oper_start;
                if (oper_len < name_size) {
                    strncpy(operator_name, oper_start, oper_len);
                    operator_name[oper_len] = '\0';
                    LOGX_DEBUG_MSG("Operator: %s", operator_name);
                    return AUTONOMY_SUCCESS;
                }
            }
        }
    }
    
    LOGX_WARN_MSG("Failed to parse operator response: %s", response);
    return AUTONOMY_ERROR_PARSE;
}
