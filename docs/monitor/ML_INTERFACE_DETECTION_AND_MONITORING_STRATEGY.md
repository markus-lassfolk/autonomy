# ML Interface Detection and Monitoring Strategy

## Overview

This document explains how the ML monitoring system detects network interface types, determines monitoring suitability, and leverages MWAN3 ping information for intelligent monitoring strategies.

## 🔍 Interface Type Detection

### Detection Logic Flow

```c
interface_type_t ml_monitor_map_interface_type(const network_interface_t *interface) {
    // Priority 1: Starlink Detection
    if (interface->is_starlink) {
        return INTERFACE_TYPE_STARLINK;
    }
    
    // Priority 2: Type-based Detection
    if (strcmp(interface->type, "cellular") == 0) {
        return INTERFACE_TYPE_CELLULAR;
    } else if (strcmp(interface->type, "wifi") == 0) {
        return INTERFACE_TYPE_WIFI;
    } else if (strcmp(interface->type, "ethernet") == 0) {
        return INTERFACE_TYPE_LAN;
    }
    
    return INTERFACE_TYPE_UNKNOWN;
}
```

### Detection Methods by Interface Type

#### 1. **Starlink Detection**
```bash
# Network Discovery Logic:
- IP Range Detection: 100.64.0.0/10 (CGNAT range used by Starlink)
- Route Analysis: Default route through Starlink gateway
- Device Identification: Physical device characteristics
- UCI Configuration: Starlink-specific settings

# Result: interface->is_starlink = true
```

#### 2. **Cellular Detection**
```bash
# Network Discovery Logic:
- Protocol Detection: proto="wwan" in network.interface
- Modem Field: presence of "modem" field in UCI config
- Interface Naming: qmimux*, wwan*, etc.
- UBUS Services: mobifd.modem* services

# Enhanced Cellular Info via AT Commands:
- AT+CSQ: Signal strength and quality
- AT+COPS?: Network operator and technology
- AT+QENG="servingcell": LTE metrics (RSRP, RSRQ, SINR)
```

#### 3. **WiFi Detection**
```bash
# Network Discovery Logic:
- Device Type: external=true in network.device
- Wireless Config: presence in wireless UCI section
- Interface Naming: wlan*, ath*, etc.
- Physical Detection: wireless device enumeration

# Enhanced WiFi Info via iw commands:
- iw dev: Interface details
- iw scan: Available networks
- iwinfo: Signal strength and quality
```

#### 4. **LAN/Ethernet Detection**
```bash
# Network Discovery Logic:
- Device Type: devtype="ethernet" in network.device
- Physical Detection: /sys/class/net/*/type = 1 (Ethernet)
- Switch Detection: DSA/switch port analysis
- Cable Detection: ethtool link status

# Enhanced LAN Info via ethtool:
- ethtool eth0: Link speed, duplex, cable status
- /sys/class/net/*/statistics/: Interface statistics
```

## 🎯 ML Monitoring Suitability

### Suitability Criteria

```c
bool ml_monitor_is_interface_suitable_for_ml(const network_interface_t *interface) {
    // Must be operational
    if (!interface->up || !interface->enabled) return false;
    
    // Must be MWAN3-relevant (indicates failover importance)
    if (!interface->mwan3_tracking_enabled) return false;
    
    // Must be a known type for ML
    if (ml_monitor_map_interface_type(interface) == INTERFACE_TYPE_UNKNOWN) return false;
    
    // Must have reasonable health
    if (interface->health_score < 50.0) return false;
    
    return true;
}
```

### Why MWAN3 Tracking Matters

- **Failover Relevance**: Only interfaces tracked by MWAN3 are used for failover
- **Resource Efficiency**: Avoids monitoring interfaces that won't impact connectivity
- **Real-world Usage**: Focuses on interfaces actually configured for redundancy
- **Integration**: Leverages existing MWAN3 health monitoring

## 🚀 MWAN3 Integration and Monitoring Strategies

### MWAN3 Ping Information Detection

```c
// MWAN3 Configuration Analysis
static void get_mwan3_ping_info(network_interface_t *interface) {
    // 1. UBUS Query: mwan3 status for real-time ping results
    // 2. UCI Analysis: /etc/config/mwan3 for ping configuration
    // 3. Status Extraction: ping interval, success rate, tracking IPs
    
    interface->real_time_metrics.mwan3_ping_active = true;
    interface->real_time_metrics.mwan3_ping_interval = 5; // seconds
    interface->real_time_metrics.mwan3_ping_success_rate = 95; // %
}
```

### Intelligent Monitoring Strategies

#### Strategy 1: **MWAN3-Based Monitoring** (Cellular Cost-Conscious)
```c
// For cellular interfaces with frequent MWAN3 pings
if (interface->type == "cellular" && 
    interface->real_time_metrics.mwan3_ping_interval <= 10) {
    
    strategy = ML_MONITORING_STRATEGY_MWAN3_BASED;
    // Use MWAN3 ping results, avoid additional data costs
    // Monitor: Signal strength, RSRP, RSRQ (no data cost)
    // Frequency: Match MWAN3 interval (5-10 seconds)
}
```

#### Strategy 2: **Full ML Monitoring** (No-Cost Interfaces)
```c
// For Starlink, WiFi, LAN with no data costs
if (interface->type == "starlink" || interface->type == "wifi" || interface->type == "ethernet") {
    strategy = ML_MONITORING_STRATEGY_FULL;
    // Perform our own ping tests for ML accuracy
    // Frequency: 1 second for streaming protection
}
```

#### Strategy 3: **Hybrid Monitoring** (Complementary)
```c
// When MWAN3 pings very frequently, complement with ML analysis
if (interface->real_time_metrics.mwan3_ping_interval <= 5) {
    strategy = ML_MONITORING_STRATEGY_HYBRID;
    // Use MWAN3 results + less frequent ML analysis
    // Frequency: MWAN3 interval * 2
}
```

#### Strategy 4: **Minimal Monitoring** (Cost-Sensitive)
```c
// For cellular without frequent MWAN3 pings
if (interface->type == "cellular" && 
    interface->real_time_metrics.mwan3_ping_interval > 10) {
    
    strategy = ML_MONITORING_STRATEGY_MINIMAL;
    // Focus on modem metrics (no data cost)
    // Monitor: SNR, RSRP, signal quality via AT commands
    // Frequency: 5 minutes
}
```

## 📊 Enhanced Metrics Integration

### Real-time Metrics Structure

```c
struct {
    uint32_t ping_latency_ms;           // Current ping latency
    uint8_t ping_success_rate;          // Success rate over last minute
    uint32_t consecutive_ping_failures; // Current failure streak
    bool mwan3_ping_active;             // MWAN3 is pinging this interface
    uint16_t mwan3_ping_interval;       // MWAN3 ping frequency (seconds)
    uint8_t mwan3_ping_success_rate;    // MWAN3's ping success rate
    time_t last_mwan3_ping;             // Last MWAN3 ping timestamp
} real_time_metrics;
```

### Performance History Tracking

```c
struct {
    uint16_t latency_history[60];       // Last 60 minutes of latency
    uint8_t loss_history[60];           // Last 60 minutes of packet loss
    uint8_t health_history[60];         // Last 60 minutes of health scores
    double latency_trend;               // Trend analysis (-1 to 1)
    double loss_trend;                  // Loss trend (-1 to 1)
    double health_trend;                // Health trend (-1 to 1)
} performance_history;
```

### Enhanced Cellular Metrics

```c
struct {
    char operator_name[32];             // Network operator (AT+COPS?)
    char network_technology[16];        // 2G/3G/4G/5G
    int signal_strength_dbm;            // Signal strength in dBm (AT+CSQ)
    char cell_id[16];                   // Current cell tower ID
    int signal_quality;                 // Signal quality 0-100
    int rsrp_dbm;                       // Reference Signal Received Power
    int rsrq_db;                        // Reference Signal Received Quality
    int sinr_db;                        // Signal-to-Interference Ratio
} enhanced_cellular_info;
```

## 🎛️ UBUS API Integration

### Enhanced Interface Information

```bash
# Get comprehensive interface details with ML recommendations
ubus call autonomy.network interfaces_detailed

# Example Response:
{
  "interfaces": [
    {
      "name": "qmimux0",
      "type": "cellular",
      "mwan3_tracking_enabled": true,
      "real_time_metrics": {
        "mwan3_ping_active": true,
        "mwan3_ping_interval": 5,
        "mwan3_ping_success_rate": 95,
        "ping_latency_ms": 45,
        "ping_success_rate": 98
      },
      "enhanced_cellular_info": {
        "operator_name": "Verizon",
        "network_technology": "4G",
        "signal_strength_dbm": -85,
        "rsrp_dbm": -95,
        "rsrq_db": -10,
        "sinr_db": 15
      },
      "performance_trends": {
        "latency_trend": 0.1,
        "loss_trend": -0.05,
        "health_trend": 0.02
      },
      "ml_monitoring_recommendations": {
        "suitable_for_ml_monitoring": true,
        "recommended_monitoring_frequency_seconds": 5,
        "use_mwan3_ping_results": true,
        "monitoring_strategy": "Using MWAN3 ping results to minimize cellular data costs"
      }
    }
  ]
}
```

## 🧠 ML Integration Benefits

### 1. **Cost-Aware Monitoring**
- **Cellular**: Leverages MWAN3 pings to avoid data costs
- **No-Cost**: Full 1-second monitoring for streaming protection
- **Hybrid**: Intelligent combination based on MWAN3 frequency

### 2. **Enhanced Accuracy**
- **Real-time Metrics**: Uses actual ping results from MWAN3
- **Trend Analysis**: 60-minute performance history with linear regression
- **Multi-source Data**: Combines MWAN3, AT commands, system metrics

### 3. **Automatic Adaptation**
- **Interface Changes**: Detects new SIM cards, WiFi networks automatically
- **Configuration Updates**: Adapts to MWAN3 configuration changes
- **Performance Shifts**: Adjusts monitoring based on performance trends

### 4. **Resource Efficiency**
- **Selective Monitoring**: Only monitors MWAN3-relevant interfaces
- **Frequency Optimization**: Adapts monitoring frequency to MWAN3 settings
- **Data Conservation**: Minimizes cellular data usage while maintaining accuracy

## 🎯 Real-World Examples

### Example 1: Cellular Interface with MWAN3
```
Interface: qmimux0 (Cellular)
MWAN3: Pings every 5 seconds to 8.8.8.8
Strategy: Use MWAN3 ping results + modem metrics
ML Frequency: Every 5 seconds (matching MWAN3)
Data Cost: Zero (uses existing MWAN3 pings)
```

### Example 2: Starlink Interface
```
Interface: eth1 (Starlink)
MWAN3: Pings every 10 seconds
Strategy: Full ML monitoring for streaming protection
ML Frequency: Every 1 second (independent pings)
Data Cost: Zero (unlimited Starlink)
```

### Example 3: WiFi Backup
```
Interface: wlan0 (WiFi)
MWAN3: Not actively pinging (standby)
Strategy: Full monitoring when active
ML Frequency: Every 1 second when primary
Data Cost: Zero (WiFi)
```

## 🚀 Conclusion

The enhanced network discovery integration provides:

1. **Intelligent Interface Detection** using multiple detection methods
2. **Cost-Aware Monitoring Strategies** that leverage MWAN3 ping information
3. **Enhanced Metrics Collection** with cellular AT commands and performance trends
4. **Automatic Adaptation** to network configuration changes
5. **Resource Efficiency** by focusing on MWAN3-relevant interfaces

This creates a **production-ready, cost-conscious, and highly accurate** ML monitoring system that seamlessly integrates with existing RUTOS/MWAN3 infrastructure.