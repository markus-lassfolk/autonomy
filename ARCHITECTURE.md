# autonomy ARCHITECTURE
**Technical Architecture and System Design**

> This file contains the technical architecture and system design for autonomy.
> For current status, see `STATUS.md`.
> For implementation roadmap, see `ROADMAP.md`.

**Last Updated**: 2025-08-20 22:15 UTC

---

## 🏗️ SYSTEM ARCHITECTURE

### **High-Level Overview**
autonomy is a Go-based multi-interface failover daemon that provides reliable, autonomous, and resource-efficient network failover management for RutOS/OpenWrt routers.

**Core Architecture**:
```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Discovery     │    │   Collectors    │    │  Decision       │
│   & Member      │───▶│   (Starlink,    │───▶│  Engine &       │
│   Management    │    │    Cellular,    │    │  Predictive     │
│                 │    │    WiFi, GPS)   │    │  Logic          │
└─────────────────┘    └─────────────────┘    └─────────────────┘
                                │                        │
                                ▼                        ▼
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Telemetry     │    │   Controller    │    │   ubus API      │
│   Store &       │◀───│   (mwan3/       │◀───│   & CLI         │
│   Events        │    │    netifd)      │    │   Interface     │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

### **Component Responsibilities**

#### **Discovery & Member Management**
- Discovers network interfaces from mwan3 configuration
- Classifies members by type (Starlink, Cellular, WiFi, LAN)
- Maps mwan3 members to netifd interfaces
- Handles configuration changes and member updates

#### **Collectors**
- **Starlink Collector**: Native gRPC client for Starlink API
- **Cellular Collector**: Multi-SIM support with RSRP/RSRQ/SINR metrics
- **WiFi Collector**: Advanced WiFi analysis with RSSI-weighted scoring
- **GPS Collector**: Multi-source GPS collection (RUTOS, Starlink, Cellular, OpenCellID)

#### **Decision Engine**
- Calculates health scores (instant, EWMA, final)
- Implements predictive failover logic
- Manages hysteresis and cooldown periods
- Triggers failover decisions based on thresholds

#### **Controller**
- Manages mwan3 policy updates
- Provides netifd fallback for systems without mwan3
- Updates route metrics and interface weights
- Handles failover execution and verification

#### **Telemetry Store**
- RAM-based ring buffer storage
- Automatic cleanup and retention management
- Event logging and historical data
- Memory usage optimization

#### **ubus API & CLI**
- RPC interface for system control
- Status monitoring and configuration
- CLI wrapper for operational control
- Real-time system information

---

## 📦 PACKAGE STRUCTURE

```
/cmd/autonomyd/           # Main daemon entry point
/pkg/                     # Internal packages
  collector/              # Data collection providers
    starlink.go          # Starlink API integration
    cellular.go          # Cellular metrics collection
    wifi.go              # WiFi analysis and optimization
    enhanced_cellular_stability.go  # Enhanced cellular monitoring
  decision/               # Decision making and scoring
    engine.go            # Main decision engine
    predictive.go        # Predictive failover logic
    enhanced_cellular_scoring.go  # Enhanced cellular scoring
  controller/             # Network control
    controller.go        # mwan3/netifd integration
    hybrid_controller.go # Hybrid control strategies
  telem/                  # Telemetry storage
    store.go             # Ring buffer storage
  logx/                   # Structured logging
    logger.go            # JSON logging framework
  uci/                    # Configuration management
    config.go            # UCI configuration handling
    config_manager.go    # Configuration management
  ubus/                   # RPC interface
    server.go            # ubus server implementation
    client.go            # ubus client utilities
  gps/                    # GPS and location services
    enhanced_cell_cache.go  # OpenCellID caching
    cellular_fusion.go   # Location fusion algorithms
    opencellid_source.go # OpenCellID integration
  wifi/                   # WiFi optimization
    enhanced_scanner.go  # RUTOS-native WiFi scanning
    optimizer.go         # Channel optimization
    scheduler.go         # Optimization scheduling
  metered/                # Data limit detection
    enhanced_rutos_data_limits.go  # RUTOS-native data limits
    data_usage.go        # Data usage monitoring
  notifications/          # Notification system
    manager.go           # Notification management
    pushover.go          # Pushover integration
  sysmgmt/                # System management
    components.go        # System component management
    wifi_manager.go      # WiFi system integration
  mqtt/                   # Telemetry publishing
    client.go            # MQTT client implementation
  performance/            # Performance monitoring
    profiler.go          # Performance profiling
  security/               # Security features
    auditor.go           # Security auditing
/scripts/                 # Build and deployment
  build.sh               # Cross-compilation script
  autonomy.init          # Init script for procd
  autonomyctl            # CLI wrapper
/configs/                 # Configuration examples
  autonomy.example       # Basic configuration
  autonomy.enhanced_wifi.example  # Enhanced WiFi config
/docs/                    # Documentation
  API_REFERENCE.md       # API documentation
  WIFI_OPTIMIZATION_COMPLETE.md  # WiFi optimization guide
  ENHANCED_DATA_LIMIT_DETECTION.md  # Data limit detection guide
```

---

## 🔧 CONFIGURATION SYSTEM

### **UCI Configuration Structure**
File: `/etc/config/autonomy`

```uci
# Main configuration section
config autonomy 'main'
    option enable '1'
    option use_mwan3 '1'
    option poll_interval_ms '1500'
    option history_window_s '600'
    option retention_hours '24'
    option max_ram_mb '16'
    option data_cap_mode 'balanced'
    option predictive '1'
    option switch_margin '10'
    option min_uptime_s '20'
    option cooldown_s '20'
    option metrics_listener '0'
    option health_listener '1'
    option log_level 'info'
    option log_file ''

    # Fail/restore thresholds
    option fail_threshold_loss '5'
    option fail_threshold_latency '1200'
    option fail_min_duration_s '10'
    option restore_threshold_loss '1'
    option restore_threshold_latency '800'
    option restore_min_duration_s '30'

    # Notifications
    option pushover_token ''
    option pushover_user ''

    # Telemetry publishing
    option mqtt_broker ''
    option mqtt_topic 'autonomy/status'

# Member-specific overrides
config member 'starlink_any'
    option detect 'auto'
    option class 'starlink'
    option weight '100'
    option min_uptime_s '30'
    option cooldown_s '20'

config member 'cellular_any'
    option detect 'auto'
    option class 'cellular'
    option weight '80'
    option prefer_roaming '0'
    option metered '1'
    option min_uptime_s '20'
    option cooldown_s '20'

# Enhanced WiFi configuration
config wifi 'optimization'
    option enabled '1'
    option scan_interval_minutes '60'
    option optimization_schedule 'nightly'
    option rssi_weighting '1'
    option channel_overlap_detection '1'
    option regulatory_domain 'auto'

# Enhanced data limit detection
config data_limits 'monitoring'
    option enabled '1'
    option rutos_native '1'
    option fallback_strategy '1'
    option warning_threshold '80'
    option critical_threshold '95'
    option sms_warning_detection '1'

# GPS and location services
config gps 'location'
    option enabled '1'
    option movement_threshold_m '500'
    option clustering_enabled '1'
    option opencellid_enabled '1'
    option cache_size_mb '25'
    option contribution_enabled '1'
```

### **Configuration Validation**
- All numeric options validated with sane ranges
- String normalization and validation
- Automatic defaults for missing options
- Backward compatibility with legacy configurations

---

## 🔌 API INTERFACES

### **ubus API Methods**

#### **Core Status Methods**
```json
// Get system status
ubus call autonomy status
{
  "state": "primary|backup|degraded",
  "current": "wan_starlink",
  "rank": [
    {"name": "wan_starlink", "class": "starlink", "final": 88.4, "eligible": true},
    {"name": "wan_cell", "class": "cellular", "final": 76.2, "eligible": true}
  ],
  "last_event": {"ts": "2025-08-13T12:34:56Z", "type": "failover", "reason": "predictive"},
  "config": {"predictive": true, "use_mwan3": true, "switch_margin": 10}
}

// Get member information
ubus call autonomy members
[
  {
    "name": "wan_starlink",
    "class": "starlink",
    "iface": "wan_starlink",
    "eligible": true,
    "score": {"instant": 87.2, "ewma": 89.1, "final": 88.5},
    "metrics": {"lat_ms": 53, "loss_pct": 0.3, "obstruction_pct": 1.4},
    "last_update": "2025-08-13T12:34:56Z"
  }
]

// Get telemetry data
ubus call autonomy metrics '{"name": "wan_cell"}'
{
  "name": "wan_cell",
  "samples": [
    {"ts": "2025-08-13T12:33:12Z", "lat_ms": 73, "loss_pct": 1.5, "rsrp": -95}
  ]
}
```

#### **Enhanced Feature APIs**
```json
// WiFi optimization status
ubus call autonomy wifi_status
{
  "enabled": true,
  "last_optimization": "2025-01-20T22:00:00Z",
  "current_channel": 36,
  "channel_rating": "⭐⭐⭐⭐⭐",
  "interference_level": "low",
  "next_optimization": "2025-01-21T02:00:00Z"
}

// Data limit status
ubus call autonomy data_limit_status
{
  "interfaces": [
    {
      "name": "mob1s1a1",
      "status": "🟢 ok",
      "usage_percent": 45.2,
      "data_used_mb": 2048,
      "data_limit_mb": 5000,
      "period_reset": "2025-02-01T00:00:00Z",
      "sms_warning": false
    }
  ]
}

// GPS and location information
ubus call autonomy gps
{
  "location": {"lat": 59.48007, "lon": 18.279852, "alt": 9.2},
  "accuracy_m": 5,
  "source": "rutos",
  "movement_detected": false,
  "last_update": "2025-01-20T22:15:00Z"
}
```

#### **Action Methods**
```json
// Manual failover
ubus call autonomy action '{"cmd": "failover", "name": "wan_cell"}'

// Restore primary
ubus call autonomy action '{"cmd": "restore"}'

// Recheck members
ubus call autonomy action '{"cmd": "recheck"}'

// Optimize WiFi
ubus call autonomy optimize_wifi '{"dry_run": false}'
```

### **CLI Interface**
```bash
# Status and monitoring
autonomyctl status
autonomyctl members
autonomyctl metrics <name>
autonomyctl history <name> [since_s]
autonomyctl events [limit]

# Control actions
autonomyctl failover
autonomyctl restore
autonomyctl recheck
autonomyctl setlog <debug|info|warn|error>

# Enhanced features
autonomyctl wifi_status
autonomyctl data_limit_status
autonomyctl gps
```

---

## 🔄 DATA FLOW

### **Main Processing Loop**
```go
// Core processing loop (tick ~1.0–1.5s)
tick := time.NewTicker(cfg.PollInterval)
for {
  select {
  case <-tick.C:
    // 1. Discover/refresh members
    members := discover()
    
    // 2. Collect metrics per member
    for m := range members {
      metrics[m] = collectors[m.class].Collect(ctx, m)
    }
    
    // 3. Update scores
    for m := range members {
      score[m] = scorer.Update(m, metrics[m])
    }
    
    // 4. Evaluate switch conditions
    top := rank(scores, eligible(members))
    if shouldSwitch(current, top, scores, windows, cfg) {
      controller.Switch(current, top)
      events.Add(SwitchEvent{...})
      current = top
    }
    
  case <-reload:
    cfg = loadConfig()
  }
}
```

### **Data Collection Flow**
1. **Discovery**: Parse mwan3 config, classify members
2. **Collection**: Gather metrics from each collector
3. **Processing**: Calculate scores and trends
4. **Decision**: Evaluate failover conditions
5. **Action**: Execute network changes via controller
6. **Storage**: Store telemetry and events
7. **Publishing**: Send updates via MQTT and ubus

---

## 🎯 DECISION ENGINE

### **Scoring Algorithm**
```go
// Instant score calculation (0..100)
score = clamp(0, 100,
    base_weight
  - w_lat * norm(lat_ms,  L_ok, L_bad)
  - w_loss* norm(loss_%,  P_ok, P_bad)
  - w_jit * norm(jitter,  J_ok, J_bad)
  - w_obs * norm(obstruct, O_ok, O_bad)        # starlink only
  - penalties(class, roaming, weak_signal, ...)
  + bonuses(class, strong_radio, ...)
)

// Final score blending
final = 0.30*instant + 0.50*ewma + 0.20*window_avg
```

### **Predictive Logic**
- **Trend Analysis**: Linear regression on metrics history
- **Pattern Detection**: Cyclic and deteriorating patterns
- **Anomaly Detection**: Statistical baseline analysis
- **Class-Specific Triggers**: Starlink obstruction, cellular roaming, WiFi degradation

### **Hysteresis Management**
- **Fail Window**: Sustained "bad" before failover
- **Restore Window**: Sustained "good" before failback
- **Cooldown Period**: Minimum time between switches
- **Switch Margin**: Minimum score difference for switching

---

## 🔧 INTEGRATION POINTS

### **mwan3 Integration**
- **Policy Management**: Update member weights and policies
- **Status Monitoring**: Check member status and health
- **Configuration**: Read and write mwan3 configuration
- **Reload**: Trigger mwan3 configuration reload

### **netifd Fallback**
- **Route Metrics**: Update route metrics for interface preference
- **Interface Control**: Manage interface up/down states
- **Status Queries**: Check interface status via ubus
- **Configuration**: Read network configuration

### **UCI Integration**
- **Configuration**: Read/write UCI configuration
- **Validation**: Validate configuration options
- **Defaults**: Apply sensible defaults
- **Reload**: Handle configuration changes

### **ubus Integration**
- **Service Registration**: Register as ubus service
- **Method Handlers**: Implement RPC methods
- **Event Publishing**: Publish status updates
- **Client Communication**: Communicate with other services

---

## 📊 PERFORMANCE CHARACTERISTICS

### **Resource Usage Targets**
- **Binary Size**: ≤12 MB stripped
- **Memory Usage**: ≤25 MB RSS steady state
- **CPU Usage**: ≤5% on low-end ARM when healthy
- **Network**: Minimal probing on metered links

### **Response Times**
- **Failover Time**: <5 seconds
- **API Response**: <1 second
- **Decision Time**: <1 second
- **Collection Time**: <2 seconds

### **Scalability**
- **Member Count**: Support ≥10 members
- **Concurrent Operations**: Handle multiple API calls
- **Data Retention**: Configurable retention periods
- **Memory Management**: Automatic cleanup and optimization

---

## 🔒 SECURITY CONSIDERATIONS

### **Access Control**
- **Local Admin Only**: CLI and ubus methods require local admin
- **Network Isolation**: Metrics/health endpoints bound to 127.0.0.1
- **Secret Management**: Store secrets only in UCI configuration
- **Audit Logging**: Log all administrative actions

### **Data Protection**
- **No Persistent Storage**: Telemetry stored in RAM by default
- **Encrypted Communication**: Use HTTPS for external APIs
- **Secure Configuration**: Validate all configuration inputs
- **Error Handling**: Never expose sensitive information in errors

---

## 🚨 FAILURE MODES & RECOVERY

### **Graceful Degradation**
- **Starlink API Down**: Rely on reachability metrics
- **ubus/mwan3 Missing**: Fall back to netifd
- **ICMP Blocked**: Use TCP/UDP connect timing
- **Config Invalid**: Apply defaults and continue

### **Recovery Mechanisms**
- **Automatic Restart**: procd respawn configuration
- **Health Checks**: Continuous health monitoring
- **Error Recovery**: Exponential backoff for failures
- **State Persistence**: Maintain state across restarts

### **Monitoring & Alerting**
- **Health Endpoints**: /healthz endpoint for monitoring
- **Structured Logging**: JSON logs for analysis
- **Event Tracking**: Comprehensive event history
- **Performance Metrics**: Real-time performance monitoring

---

**For current status and progress, see `STATUS.md`**
**For implementation roadmap, see `ROADMAP.md`**
**For detailed specifications, see `PROJECT_INSTRUCTION.md`**
