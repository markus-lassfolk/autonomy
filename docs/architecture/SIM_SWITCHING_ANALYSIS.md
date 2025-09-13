# RUTX50 SIM Switching Analysis and Implementation Guide

## Executive Summary

This document analyzes the RUTX50's SIM switching capabilities, both built-in RUTOS
features and potential custom implementations using the autonomy-daemon codebase. The
analysis covers automatic SIM switching based on data limits, roaming detection, and
geo-positioning.

## RUTOS Built-in SIM Switching Capabilities

### ✅ **Data Limit-Based SIM Switching**

RUTOS **already includes** automatic SIM switching based on data limits:

- **Location**: Network → Mobile → SIM Switch in web interface
- **Feature**: "On data limit" option
- **Configuration**: Set specific data thresholds for each SIM card
- **Status**: ✅ **Available and Ready to Use**

### ✅ **Roaming-Based SIM Switching**

RUTOS **already includes** automatic SIM switching when roaming is detected:

- **Location**: Network → Mobile → SIM Switch in web interface  
- **Feature**: "On roaming" option
- **Functionality**: Automatically switches to alternative SIM upon detecting roaming
- **Status**: ✅ **Available and Ready to Use**

### ❌ **Geo-Position-Based SIM Switching**

RUTOS **does NOT natively support** SIM switching based on geographic location:

- **Status**: ❌ **Not Available in RUTOS**
- **Requirement**: Custom development needed
- **Complexity**: High - requires GPS integration and custom logic

## Current Autonomy-Daemon Codebase Analysis

### Existing Cellular Infrastructure

The codebase already has comprehensive cellular monitoring capabilities:

#### 1. **Cellular Data Collection** (`cellular_collector.c`)

- **Multi-method collection**: UBUS, gsmctl, AT commands
- **Signal metrics**: RSRP, RSRQ, SINR, RSSI
- **Network info**: Operator, band, cell ID, roaming status
- **Multi-SIM support**: `sim_slot`, `sim_count`, `active_sim` fields
- **Quality scoring**: Signal quality, stability, predictive risk

#### 2. **Data Usage Monitoring** (`metered_manager.c`)

- **Real-time usage tracking**: Interface statistics from `/proc/net/dev`
- **Database persistence**: SQLite storage for historical data
- **Threshold monitoring**: Warning, critical, and hard limits
- **Roaming detection**: Via UBUS GSM service integration

#### 3. **GPS Integration** (`gps_rutos.c`)

- **RUTOS GPS integration**: Direct access to router's GPS data
- **Location services**: Latitude, longitude, accuracy, satellites
- **Real-time monitoring**: Continuous GPS data collection
- **Multiple sources**: RUTOS, Starlink, external GPS support

#### 4. **Network Failover System** (`network_failover.c`)

- **Interface management**: Multi-interface failover capabilities
- **Health monitoring**: Continuous interface health assessment
- **Automatic switching**: Configurable failover triggers
- **Recovery management**: Automatic failback when conditions improve

### Key Data Structures

```c
// Multi-SIM support already exists
typedef struct {
    int sim_slot;           // Current SIM slot
    int sim_count;          // Total SIM count
    int active_sim;         // Currently active SIM
    char sim_status[32];    // SIM status
    bool roaming;           // Roaming status
    // ... other fields
} cellular_info_t;
```

## Implementation Recommendations

### 1. **Leverage Existing RUTOS Features** (Immediate)

**For Data Limits and Roaming:**

- ✅ **Use RUTOS built-in features** - no custom code needed
- Configure via web interface: Network → Mobile → SIM Switch
- Enable "On data limit" and "On roaming" options
- Set appropriate thresholds for your use case

### 2. **Custom Geo-Position SIM Switching** (Development Required)

#### **Implementation Approach:**

#### A. Extend Cellular Collector

```c
// Add to cellular_collector.h
typedef struct {
    double latitude;
    double longitude;
    char geo_region[64];
    bool geo_switching_enabled;
    double geo_switch_radius_km;
} geo_switching_config_t;
```

#### B. Create SIM Switching Manager

```c
// New file: sim_switching_manager.c
typedef struct {
    bool enabled;
    int current_sim;
    int target_sim;
    sim_switch_reason_t reason;
    time_t last_switch;
    geo_switching_config_t geo_config;
} sim_switching_manager_t;
```

**C. Integration Points:**

1. **GPS Integration**: Use existing `gps_rutos.c` for location data
2. **Cellular Monitoring**: Extend `cellular_collector.c` for SIM status
3. **Network Management**: Integrate with `network_failover.c` for interface switching
4. **Notifications**: Use existing notification system for switch alerts

#### **Implementation Steps:**

1. **Create SIM Switching Module**

   ```c
   // sim_switching_manager.c
   int sim_switching_init(void);
   int sim_switching_set_geo_config(const geo_switching_config_t* config);
   int sim_switching_check_geo_conditions(void);
   int sim_switching_perform_switch(int target_sim, sim_switch_reason_t reason);
   ```

2. **Extend GPS Integration**

   ```c
   // Add to gps_rutos.c
   int gps_rutos_get_current_location(gps_location_t* location);
   bool gps_rutos_is_in_geo_region(const geo_switching_config_t* config);
   ```

3. **Add UBUS Commands**

   ```c
   // Add SIM switching UBUS methods
   int autonomy_sim_switch_geo(struct ubus_context *uctx, struct ubus_object *obj, ...);
   int autonomy_sim_switch_manual(struct ubus_context *uctx, struct ubus_object *obj, ...);
   ```

4. **Configuration Integration**

   ```c
   // Add to autonomy.config
   sim_switching_enabled=true
   sim_switching_geo_enabled=true
   sim_switching_geo_latitude=40.7128
   sim_switching_geo_longitude=-74.0060
   sim_switching_geo_radius_km=50.0
   ```

### 3. **Enhanced Features** (Future Development)

#### **A. Smart SIM Selection**

- **Cost-based switching**: Switch to cheaper SIM in specific regions
- **Coverage optimization**: Choose SIM with best signal in area
- **Time-based rules**: Different SIMs for different times of day
- **Load balancing**: Distribute usage across multiple SIMs

#### **B. Advanced Monitoring**

- **SIM health tracking**: Monitor individual SIM performance
- **Usage analytics**: Track usage patterns per SIM
- **Predictive switching**: Anticipate needs based on location patterns
- **Cost optimization**: Minimize roaming charges automatically

## Technical Implementation Details

### **SIM Switching Commands**

The implementation would use RUTOS system commands:

```bash
# Switch to SIM 1
gsmctl -S 1

# Switch to SIM 2  
gsmctl -S 2

# Check current SIM
gsmctl -s

# Get SIM status
gsmctl -i
```

### **UBUS Integration**

```c
// Example UBUS method for SIM switching
static int autonomy_sim_switch_geo(struct ubus_context *uctx, struct ubus_object *obj,
                                   struct ubus_request_data *req, const char *method,
                                   struct blob_attr *msg) {
    // Get current GPS location
    gps_location_t location;
    if (gps_rutos_get_current_location(&location) != AUTONOMY_SUCCESS) {
        return UBUS_STATUS_UNKNOWN_ERROR;
    }
    
    // Check if location matches geo-switching criteria
    if (should_switch_sim_by_location(&location)) {
        int target_sim = determine_best_sim_for_location(&location);
        return perform_sim_switch(target_sim, SIM_SWITCH_REASON_GEO);
    }
    
    return UBUS_STATUS_OK;
}
```

### **Configuration Example**

```ini
# autonomy.config additions
[sim_switching]
enabled=true
geo_switching_enabled=true
check_interval=60

[sim_switching.geo]
# Home region - use SIM 1
home_latitude=40.7128
home_longitude=-74.0060
home_radius_km=100.0
home_sim=1

# Travel region - use SIM 2  
travel_latitude=51.5074
travel_longitude=-0.1278
travel_radius_km=200.0
travel_sim=2

# Roaming region - use SIM 2 (cheaper roaming)
roaming_sim=2
```

## Conclusion and Recommendations

### **Immediate Actions (No Development Required):**

1. ✅ **Enable RUTOS built-in SIM switching** for data limits and roaming
2. ✅ **Configure thresholds** appropriate for your use case
3. ✅ **Test existing functionality** before considering custom development

### **Custom Development (If Geo-Position Switching Needed):**

1. **Assess requirements**: Determine if geo-position switching is truly necessary
2. **Leverage existing code**: Build upon cellular_collector, gps_rutos, and network_failover
3. **Implement incrementally**: Start with basic geo-switching, add advanced features later
4. **Consider alternatives**: Evaluate if time-based or manual switching meets needs

### **Development Effort Estimate:**

- **Basic geo-position switching**: 2-3 weeks
- **Advanced features**: 4-6 weeks  
- **Testing and integration**: 1-2 weeks

### **Risk Assessment:**

- **Low risk**: Using existing RUTOS features
- **Medium risk**: Custom geo-position implementation
- **Mitigation**: Thorough testing, fallback mechanisms, gradual rollout

The existing autonomy-daemon codebase provides an excellent foundation for implementing custom SIM switching features, with comprehensive cellular monitoring, GPS integration, and network management already in place.
