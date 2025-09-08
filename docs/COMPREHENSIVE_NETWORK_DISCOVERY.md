# Comprehensive Network Discovery Implementation

## Overview

This document describes the comprehensive network discovery system implemented for the autonomy daemon. The system provides detailed interface identification, MWAN3 integration, and enhanced failover capabilities.

## Features

### 1. Enhanced Interface Identification

The system now identifies network interfaces using multiple methods instead of relying solely on naming conventions:

- **Cellular Interfaces**: Detected via `proto: "wwan"` and `modem` field in network.interface
- **WiFi Interfaces**: Detected via `external: true` in network.device and wireless configuration
- **Ethernet Interfaces**: Detected via `devtype: "ethernet"` in network.device
- **Starlink Connections**: Detected via IP range (100.64.0.0/10) and route analysis
- **VPN Interfaces**: Detected via interface name patterns (wg_, tun, tap) and protocol types

### 2. MWAN3 Integration

The system now properly integrates with MWAN3 for failover management:

- **MWAN3 Tracking**: Only interfaces tracked by MWAN3 are considered for failover
- **Status Monitoring**: Real-time MWAN3 status (online, offline, standby) is tracked
- **Metric Integration**: MWAN3 metrics are used for interface prioritization

### 3. Comprehensive Interface Information

Each interface now provides detailed information:

#### Basic Information
- Device name (eth0, wlan0-1, etc.)
- Friendly name as seen in RUTOS UI
- Interface type (ethernet, wifi, cellular, starlink, vpn)
- Subtype (sim, wireguard, etc.)

#### Network Configuration
- IP address, gateway, DNS servers
- Protocol (static, dhcp, wwan, etc.)
- Physical device information
- Route metrics

#### MWAN3 Information
- MWAN3 interface name
- Tracking status (enabled/disabled)
- Availability in MWAN3
- Current MWAN3 status
- MWAN3 metric values

#### Specialized Information
- **Cellular**: Modem model, SIM ID, operator, signal strength
- **WiFi**: SSID, band (2.4G/5G), mode (ap/sta), encryption
- **Starlink**: Dish ID, dish name, Starlink IP
- **VPN**: VPN type, connection name

## Usage

### UBUS API

The new comprehensive interface information is available via UBUS:

```bash
# Get detailed interface information
ubus call autonomy.network interfaces_detailed
```

### Example Output

```json
{
    "interfaces": [
        {
            "name": "eth1",
            "friendly_name": "WAN",
            "type": "starlink",
            "mwan3_name": "wan",
            "mwan3_tracking_enabled": true,
            "mwan3_status": "online",
            "is_starlink": true,
            "starlink_dish_id": "dish_100.76.91.229",
            "starlink_ip": "100.76.91.229",
            "ip_address": "100.76.91.229",
            "up": true,
            "health_score": 95.0
        },
        {
            "name": "qmimux0",
            "friendly_name": "Mobile 1",
            "type": "cellular",
            "subtype": "sim",
            "mwan3_name": "mob1s1a1",
            "mwan3_tracking_enabled": true,
            "mwan3_status": "standby",
            "modem_model": "RG501Q-EU",
            "sim_id": "1",
            "ip_address": "10.49.24.161",
            "up": true,
            "health_score": 88.0
        }
    ],
    "interface_count": 2,
    "timestamp": 1757338756
}
```

## Configuration

### New Configuration Options

- `include_vpn_in_failover`: Whether to include VPN interfaces in failover logic (default: false)

### MWAN3 Integration

The system automatically detects MWAN3 availability and integrates with it when present. No additional configuration is required.

## Failover Logic

The enhanced failover logic now includes:

1. **MWAN3 Filtering**: Only interfaces tracked by MWAN3 are considered
2. **VPN Filtering**: VPN interfaces are excluded unless specifically configured
3. **Health-based Selection**: Interfaces are selected based on health scores
4. **Status Validation**: Only up and available interfaces are considered

### Failover Criteria

An interface is included in failover if:
- It's enabled for failover
- It's tracked by MWAN3 (`mwan3_tracking_enabled: true`)
- It's available in MWAN3 (`mwan3_available: true`)
- It's up and running (`up: true`)
- It's not a VPN interface (unless `include_vpn_in_failover: true`)
- It meets the minimum health threshold

## Implementation Details

### Files Modified/Created

1. **`src/c/autonomy-daemon/core/types.h`**
   - Extended `network_interface_t` structure with comprehensive fields

2. **`src/c/autonomy-daemon/network/network_discovery_comprehensive.c`**
   - New comprehensive discovery implementation
   - MWAN3 integration functions
   - Interface type detection logic

3. **`src/c/autonomy-daemon/network/network_discovery_comprehensive.h`**
   - Header file for comprehensive discovery

4. **`src/c/autonomy-daemon/network/network_ubus.c`**
   - Added `autonomy_network_interfaces_detailed` method

5. **`src/c/autonomy-daemon/network/network_failover.c`**
   - Enhanced failover logic with MWAN3 filtering

6. **`src/c/autonomy-daemon/core/autonomy-daemon.c`**
   - Registered new UBUS method

7. **`src/c/autonomy-daemon/core/autonomy_modules.h`**
   - Added function declaration

### Key Functions

- `get_comprehensive_interface_info()`: Main discovery function
- `should_include_in_failover()`: MWAN3 filtering logic
- `detect_starlink_connections()`: Starlink detection
- `detect_vpn_connections()`: VPN detection
- `get_mwan3_interface_info()`: MWAN3 integration

## Testing

A test script is provided to verify the implementation:

```bash
./src/c/autonomy-daemon/testing/test_comprehensive_network_discovery.sh
```

## Benefits

1. **Reliable Interface Identification**: No longer dependent on naming conventions
2. **MWAN3 Integration**: Proper integration with RUTOS MWAN3 system
3. **Comprehensive Information**: Detailed interface metadata for better decision making
4. **Enhanced Failover**: More intelligent failover logic with proper filtering
5. **Starlink Detection**: Automatic detection of Starlink connections
6. **VPN Awareness**: Proper handling of VPN interfaces in failover logic

## Future Enhancements

1. **Real-time Updates**: Implement real-time interface status updates
2. **Performance Metrics**: Add bandwidth and latency monitoring
3. **Configuration UI**: Web interface for configuration management
4. **Advanced Starlink Features**: Integration with Starlink API for dish management
5. **Cellular Operator Detection**: Enhanced cellular operator identification
