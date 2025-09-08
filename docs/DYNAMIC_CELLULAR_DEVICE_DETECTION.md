# Dynamic Cellular Device Detection

## Overview

This document describes the dynamic cellular device detection system that replaces hardcoded device paths (like `/dev/ttyUSB2`) with intelligent device discovery based on network interface information.

## Problem

Previously, the system used hardcoded device paths for cellular modems:

```c
// OLD: Hardcoded device path
snprintf(cmd, sizeof(cmd), "echo 'AT+CSQ' | microcom -t 1000 /dev/ttyUSB2 2>/dev/null | grep '+CSQ:' | head -1");
```

This approach has several issues:
- Device paths can change between reboots
- Different hardware configurations use different device paths
- No way to handle multiple cellular modems
- Not portable across different systems

## Solution

The new system dynamically detects cellular device paths using:

1. **Network Discovery Integration**: Uses the comprehensive network discovery system to identify cellular interfaces
2. **Device Path Detection**: Automatically detects the correct device path for each cellular interface
3. **UCI Configuration**: Falls back to UCI configuration if automatic detection fails
4. **Multiple Device Support**: Can handle multiple cellular modems with different device paths

## Implementation

### 1. Enhanced Network Interface Structure

The `network_interface_t` structure now includes:

```c
typedef struct {
    // ... other fields ...
    
    // Cellular specific
    char cellular_device_path[64];      // Cellular device path (e.g., "/dev/ttyUSB2")
    
    // ... other fields ...
} network_interface_t;
```

### 2. Dynamic Device Detection

The system automatically detects cellular device paths by:

1. **Checking Common Device Paths**: Tests common modem device paths:
   - `/dev/ttyUSB0`, `/dev/ttyUSB1`, `/dev/ttyUSB2`, `/dev/ttyUSB3`
   - `/dev/ttyACM0`, `/dev/ttyACM1`, `/dev/ttyACM2`, `/dev/ttyACM3`
   - `/dev/cdc-wdm0`, `/dev/cdc-wdm1`, `/dev/cdc-wdm2`
   - `/dev/ttyS0`, `/dev/ttyS1`, `/dev/ttyS2`, `/dev/ttyS3`

2. **AT Command Verification**: Sends AT commands to verify the device is a cellular modem:
   ```bash
   echo 'AT' | timeout 2 microcom -t 1000 /dev/ttyUSB2 2>/dev/null | grep -q 'OK'
   ```

3. **UCI Configuration Fallback**: Checks UCI network configuration for device paths

4. **Default Fallback**: Uses `/dev/ttyUSB2` as a last resort

### 3. Helper Functions

#### `get_dynamic_cellular_device_path()`
```c
int get_dynamic_cellular_device_path(char *device_path, size_t path_size);
```
Gets the device path for the first available cellular interface.

#### `execute_at_command_dynamic()`
```c
int execute_at_command_dynamic(const char *at_command, char *response, size_t response_size);
```
Executes AT commands using the dynamically detected device path.

#### `get_signal_strength_dynamic()`
```c
int get_signal_strength_dynamic(int *rssi, int *ber);
```
Gets signal strength using dynamic device detection.

#### `get_operator_info_dynamic()`
```c
int get_operator_info_dynamic(char *operator_name, size_t name_size);
```
Gets operator information using dynamic device detection.

## Usage Examples

### Basic Device Path Detection

```c
#include "cellular_device_helper.h"

char device_path[64];
int ret = get_dynamic_cellular_device_path(device_path, sizeof(device_path));
if (ret == AUTONOMY_SUCCESS) {
    printf("Cellular device path: %s\n", device_path);
} else {
    printf("Failed to detect cellular device\n");
}
```

### AT Command Execution

```c
#include "cellular_device_helper.h"

char response[256];
int ret = execute_at_command_dynamic("AT+CSQ", response, sizeof(response));
if (ret == AUTONOMY_SUCCESS) {
    printf("AT+CSQ response: %s\n", response);
}
```

### Signal Strength Detection

```c
#include "cellular_device_helper.h"

int rssi, ber;
int ret = get_signal_strength_dynamic(&rssi, &ber);
if (ret == AUTONOMY_SUCCESS) {
    printf("Signal strength: RSSI=%d, BER=%d\n", rssi, ber);
}
```

### Operator Information

```c
#include "cellular_device_helper.h"

char operator_name[64];
int ret = get_operator_info_dynamic(operator_name, sizeof(operator_name));
if (ret == AUTONOMY_SUCCESS) {
    printf("Operator: %s\n", operator_name);
}
```

## Migration Guide

### Before (Hardcoded)

```c
// OLD: Hardcoded device path
char command[256];
snprintf(command, sizeof(command), 
         "echo 'AT+CSQ' | microcom -t 1000 /dev/ttyUSB2 2>/dev/null | grep '+CSQ:' | head -1");
FILE *fp = popen(command, "r");
// ... process response ...
```

### After (Dynamic)

```c
// NEW: Dynamic device detection
#include "cellular_device_helper.h"

int rssi, ber;
int ret = get_signal_strength_dynamic(&rssi, &ber);
if (ret == AUTONOMY_SUCCESS) {
    // Use rssi and ber values
    printf("Signal strength: RSSI=%d, BER=%d\n", rssi, ber);
}
```

## UBUS Integration

The dynamic device paths are also available via the UBUS API:

```bash
# Get comprehensive interface information including device paths
ubus call autonomy.network interfaces_detailed
```

Example response:
```json
{
    "interfaces": [
        {
            "name": "qmimux0",
            "type": "cellular",
            "cellular_device_path": "/dev/ttyUSB2",
            "modem_model": "RG501Q-EU",
            "sim_id": "1",
            "operator": "Telia",
            "signal_strength": 25
        }
    ]
}
```

## Benefits

1. **Automatic Detection**: No need to manually configure device paths
2. **Hardware Agnostic**: Works with different modem types and configurations
3. **Multiple Modem Support**: Can handle multiple cellular modems
4. **Robust Fallbacks**: Multiple fallback mechanisms ensure reliability
5. **Easy Migration**: Simple API for replacing hardcoded paths
6. **Debugging Support**: Comprehensive logging for troubleshooting

## Configuration

The system automatically detects device paths without requiring configuration. However, you can influence the detection by:

1. **UCI Configuration**: Set device paths in UCI network configuration
2. **Device Permissions**: Ensure proper permissions on device files
3. **Modem Initialization**: Ensure modems are properly initialized before detection

## Troubleshooting

### Device Not Found
- Check if modem is properly connected and initialized
- Verify device permissions (`ls -la /dev/ttyUSB*`)
- Check UCI network configuration
- Review system logs for detection errors

### AT Commands Failing
- Verify modem is responding to AT commands
- Check if `microcom` or `gsmctl` is available
- Ensure proper device permissions
- Test with manual AT commands

### Multiple Modems
- The system will use the first detected cellular interface
- Use the UBUS API to see all detected interfaces
- Configure specific interfaces via UCI if needed

## Future Enhancements

1. **Priority-based Selection**: Select modems based on signal strength or other criteria
2. **Hot-plug Support**: Dynamic detection when modems are added/removed
3. **Configuration UI**: Web interface for modem configuration
4. **Advanced AT Commands**: Support for more complex AT command sequences
5. **Modem-specific Optimization**: Optimized detection for specific modem models
