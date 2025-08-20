# Multi-Connection Monitoring System

## Overview

The enhanced RUTOS Starlink Failover system now supports intelligent monitoring and failover across multiple connection types:

- **Multiple Cellular Modems** (up to 8 modems: mob1s1a1 through mob8s1a1)
- **Generic Internet Connections** (WiFi bridges, Ethernet, VPN tunnels)
- **Intelligent Health Scoring** based on performance metrics
- **Priority-based Failover Ordering** with user-configurable preferences

## Key Features

### 🔧 Multi-Cellular Modem Support
- Automatically discovers available cellular modems (mob1s1a1 through mob8s1a1)
- Tests performance of each modem individually
- Ranks modems by signal strength, latency, and operator quality
- Supports different cellular technologies (4G, 5G) with appropriate scoring

### 🌐 Generic Connection Monitoring
- Monitors WiFi bridge connections (camping site WiFi)
- Supports Ethernet uplinks (marina connections, fixed broadband)
- Handles network bridges and VPN tunnels
- Configurable connection types with different quality expectations

### 🏥 Intelligent Health Scoring
- **Latency Score** (0-100): Based on ping response times
- **Packet Loss Score** (0-100): Based on connectivity reliability  
- **Signal Score** (0-100): Signal strength for cellular, fixed for wired
- **Connection Type Score** (0-100): Preference weighting by connection type

### 📊 Smart Failover Logic
- Compares all available connections simultaneously
- Only triggers failover when alternative is significantly better
- Considers improvement thresholds to avoid unnecessary switching
- Maintains connection stability through intelligent scoring

## Configuration Examples

### Camping Scenario: WiFi Bridge + Cellular Backup
```bash
# Monitor campsite WiFi and cellular modems
ENABLE_MULTI_CONNECTION_MONITORING=true
GENERIC_CONNECTIONS="wlan0-campsite"
GENERIC_CONNECTION_TYPES="wifi"
CELLULAR_MODEMS="mob1s1a1,mob2s1a1"
CONNECTION_PRIORITY_ORDER="starlink,wifi,cellular"
```

### Marine/RV Scenario: Ethernet + Multiple Cellular
```bash
# Monitor marina ethernet and cellular array
GENERIC_CONNECTIONS="eth2-marina"
GENERIC_CONNECTION_TYPES="ethernet"
CELLULAR_MODEMS="mob1s1a1,mob2s1a1,mob3s1a1,mob4s1a1"
CONNECTION_PRIORITY_ORDER="starlink,ethernet,cellular"
```

### Maximum Redundancy: All Connection Types
```bash
# Monitor everything available
GENERIC_CONNECTIONS="wlan0,eth2,br-guest,tun0"
GENERIC_CONNECTION_TYPES="wifi,ethernet,bridge,vpn"
CELLULAR_MODEMS="mob1s1a1,mob2s1a1,mob3s1a1,mob4s1a1,mob5s1a1,mob6s1a1"
CONNECTION_PRIORITY_ORDER="starlink,ethernet,vpn,wifi,cellular"
```

## How It Works

### 1. Connection Discovery
The system automatically discovers available connections:
- Scans configured cellular interfaces (mob1s1a1-mob8s1a1)
- Checks generic interfaces for IP assignment and connectivity
- Logs discovered connections with their types and availability

### 2. Performance Testing
Each connection is tested individually:
- **Ping tests** measure latency, packet loss, and jitter
- **Signal strength** assessment for cellular connections
- **Interface status** verification (up/down, IP assigned)

### 3. Health Score Calculation
Weighted scoring system (configurable weights):
- **Latency Weight: 40%** - Response time quality
- **Loss Weight: 30%** - Connection reliability  
- **Signal Weight: 20%** - Signal/link quality
- **Type Weight: 10%** - Connection type preference

### 4. Intelligent Failover Decision
Multi-factor analysis determines optimal connection:
- **Primary Issues**: Count of problems with Starlink
- **Score Comparison**: Health score difference between connections
- **Improvement Threshold**: Required improvement percentage for failover
- **Priority Order**: User-defined preference for connection types

## Decision Matrix

| Primary Status | Best Alternative | Score Difference | Action |
|---------------|------------------|------------------|--------|
| Multiple Issues | Available | > Threshold | **Hard Failover** |
| Minor Issues | Available | > 2x Threshold | **Soft Failover** |
| Good | Better Available | > Threshold | **Stay on Primary** |
| Failed | Any Available | Any | **Emergency Failover** |

## Backward Compatibility

The system maintains full backward compatibility:
- **Legacy dual-connection settings** still work
- **Existing configuration files** require no changes
- **New features are opt-in** via configuration flags
- **Traditional single-connection analysis** available as fallback

## Performance Benefits

### Multi-Cellular Array
- **Cellular Provider Diversity**: Use modems from different carriers
- **Signal Quality Optimization**: Automatically select best signal
- **Load Distribution**: Potential for future load balancing features
- **Redundancy**: Up to 8 cellular backup connections

### Generic Connection Support
- **Local WiFi Utilization**: Leverage available WiFi networks
- **Wired Uplink Support**: Use marina/campground ethernet
- **Bridge Network Access**: Connect to shared network infrastructure
- **VPN Tunnel Backup**: Support encrypted backup connections

## Monitoring and Logging

### Enhanced Logging
- Connection discovery events
- Performance test results for all interfaces
- Health score calculations and comparisons
- Failover decision reasoning with detailed context

### Performance Tracking
- Historical performance data for each connection
- Signal strength trends for cellular modems
- Network quality assessment over time
- Connection reliability statistics

## Implementation Notes

### RUTOS Compatibility
- **Busybox shell compatible** - All POSIX sh syntax
- **RUTOS library integration** - Uses standardized logging and utilities
- **Resource efficient** - Minimal system impact during testing
- **Configurable intervals** - Adjustable monitoring frequency

### Network Interface Support
- **Automatic discovery** of mob1s1a1 through mob8s1a1 cellular interfaces
- **Generic interface support** for any named network interface
- **IP address validation** ensures interfaces are actually connected
- **Interface type detection** via configuration mapping

This enhanced monitoring system provides comprehensive connection awareness and intelligent failover capabilities while maintaining the simplicity and reliability of the original RUTOS Starlink Failover solution.
