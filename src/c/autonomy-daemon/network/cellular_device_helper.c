#include "cellular_device_helper.h"
#include "../utils/secure_exec.h"
#include "../utils/logx.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

// Common cellular device paths to check
static const char* CELLULAR_DEVICE_PATHS[] = {
    "/dev/ttyUSB0", "/dev/ttyUSB1", "/dev/ttyUSB2", "/dev/ttyUSB3",
    "/dev/ttyACM0", "/dev/ttyACM1", "/dev/ttyACM2", "/dev/ttyACM3",
    "/dev/cdc-wdm0", "/dev/cdc-wdm1", "/dev/cdc-wdm2",
    NULL
};

// Discover cellular devices dynamically
int discover_cellular_devices(cellular_device_info_t *devices, int max_devices, int *actual_count) {
    if (!devices || !actual_count || max_devices <= 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    *actual_count = 0;
    
    // Check common device paths
    for (int i = 0; CELLULAR_DEVICE_PATHS[i] != NULL && *actual_count < max_devices; i++) {
        const char *device_path = CELLULAR_DEVICE_PATHS[i];
        
        if (is_cellular_device(device_path)) {
            cellular_device_info_t *device = &devices[*actual_count];
            
            // Initialize device info
            memset(device, 0, sizeof(cellular_device_info_t));
            strncpy(device->device_path, device_path, sizeof(device->device_path) - 1);
            device->is_available = true;
            
            // Try to get signal strength to verify device is working
            int rssi, ber;
            if (get_signal_strength(device_path, &rssi, &ber) == AUTONOMY_SUCCESS) {
                device->signal_strength_dbm = -113 + (rssi * 2);
                device->signal_quality = (rssi * 100) / 31;
            }
            
            // Try to get operator name
            get_network_operator(device_path, device->operator_name, sizeof(device->operator_name));
            
            (*actual_count)++;
            LOGX_INFO("📱 Discovered cellular device: %s", device_path);
        }
    }
    
    LOGX_INFO("📱 Cellular device discovery complete: found %d devices", *actual_count);
    return AUTONOMY_SUCCESS;
}

// Get cellular device path for a specific interface
int get_cellular_device_path(const char *interface_name, char *device_path, size_t path_size) {
    if (!interface_name || !device_path || path_size == 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Discover available devices
    cellular_device_info_t devices[8];
    int device_count;
    
    int result = discover_cellular_devices(devices, 8, &device_count);
    if (result != AUTONOMY_SUCCESS || device_count == 0) {
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    // For now, return the first available device
    // In the future, we could match interface names to specific devices
    strncpy(device_path, devices[0].device_path, path_size - 1);
    device_path[path_size - 1] = '\0';
    
    LOGX_DEBUG("📱 Using cellular device %s for interface %s", device_path, interface_name);
    return AUTONOMY_SUCCESS;
}

// Get signal strength from cellular device
int get_signal_strength(const char *device_path, int *rssi, int *ber) {
    if (!device_path || !rssi || !ber) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    exec_result_t result;
    int ret = secure_cellular_at_command(device_path, "AT+CSQ", &result);
    
    if (ret != AUTONOMY_SUCCESS || !result.success) {
        LOGX_DEBUG("📱 Failed to get signal strength from %s: %s", device_path, result.error);
        return ret;
    }
    
    // Parse result: +CSQ: rssi,ber
    char *csq_line = strstr(result.output, "+CSQ:");
    if (csq_line && sscanf(csq_line, "+CSQ: %d,%d", rssi, ber) == 2) {
        LOGX_DEBUG("📱 Signal strength from %s: RSSI=%d, BER=%d", device_path, *rssi, *ber);
        return AUTONOMY_SUCCESS;
    }
    
    return AUTONOMY_ERROR_PARSE_FAILED;
}

// Get signal strength using dynamic device discovery
int get_signal_strength_dynamic(int *rssi, int *ber) {
    if (!rssi || !ber) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Discover devices
    cellular_device_info_t devices[8];
    int device_count;
    
    int result = discover_cellular_devices(devices, 8, &device_count);
    if (result != AUTONOMY_SUCCESS || device_count == 0) {
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    // Try each device until we get a successful reading
    for (int i = 0; i < device_count; i++) {
        if (get_signal_strength(devices[i].device_path, rssi, ber) == AUTONOMY_SUCCESS) {
            return AUTONOMY_SUCCESS;
        }
    }
    
    return AUTONOMY_ERROR_NOT_FOUND;
}

// Get network operator information
int get_network_operator(const char *device_path, char *operator_name, size_t name_size) {
    if (!device_path || !operator_name || name_size == 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    exec_result_t result;
    int ret = secure_cellular_at_command(device_path, "AT+COPS?", &result);
    
    if (ret != AUTONOMY_SUCCESS || !result.success) {
        return ret;
    }
    
    // Parse result: +COPS: mode,format,"operator"
    char *cops_line = strstr(result.output, "+COPS:");
    if (cops_line) {
        char *start = strchr(cops_line, '"');
        if (start) {
            start++;
            char *end = strchr(start, '"');
            if (end) {
                size_t len = end - start;
                if (len < name_size) {
                    strncpy(operator_name, start, len);
                    operator_name[len] = '\0';
                    return AUTONOMY_SUCCESS;
                }
            }
        }
    }
    
    return AUTONOMY_ERROR_PARSE_FAILED;
}

// Get LTE metrics (RSRP, RSRQ, SINR)
int get_lte_metrics(const char *device_path, int *rsrp, int *rsrq, int *sinr) {
    if (!device_path || !rsrp || !rsrq || !sinr) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    exec_result_t result;
    int ret = secure_cellular_at_command(device_path, "AT+QENG=\"servingcell\"", &result);
    
    if (ret != AUTONOMY_SUCCESS || !result.success) {
        return ret;
    }
    
    // Parse LTE serving cell info
    char *lte_line = strstr(result.output, "LTE");
    if (lte_line) {
        // Parse comma-separated values to extract RSRP, RSRQ, SINR
        char *token = strtok(lte_line, ",");
        int field = 0;
        while (token && field < 20) {
            if (field == 11) { // RSRP field
                *rsrp = atoi(token);
            } else if (field == 12) { // RSRQ field
                *rsrq = atoi(token);
            } else if (field == 13) { // SINR field
                *sinr = atoi(token);
            }
            token = strtok(NULL, ",");
            field++;
        }
        
        if (*rsrp != 0 || *rsrq != 0 || *sinr != 0) {
            return AUTONOMY_SUCCESS;
        }
    }
    
    return AUTONOMY_ERROR_PARSE_FAILED;
}

// Check if device is a cellular modem
bool is_cellular_device(const char *device_path) {
    if (!device_path) return false;
    
    // Check if device exists and is accessible
    if (access(device_path, R_OK | W_OK) != 0) {
        return false;
    }
    
    // Try a simple AT command to verify it's a modem
    exec_result_t result;
    int ret = secure_cellular_at_command(device_path, "AT", &result);
    
    // If we get "OK" response, it's likely a cellular modem
    return (ret == AUTONOMY_SUCCESS && result.success && strstr(result.output, "OK"));
}
