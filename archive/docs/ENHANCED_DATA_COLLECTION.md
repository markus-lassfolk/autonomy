# Enhanced Data Collection Features

## Overview

The RUTOS Data Collection Library has been enhanced with comprehensive GPS diagnostics and Starlink health monitoring capabilities based on the rich data available from the `get_diagnostics` API endpoint.

## New Functions

### Enhanced GPS Collection

#### `collect_gps_data_enhanced()`

Collects GPS data with additional diagnostic information from Starlink's `get_diagnostics` API.

**Output Format**: `lat,lon,alt,accuracy,source,uncertainty_meters,gps_time_s,utc_offset_s`

**New Fields from get_diagnostics**:
- `uncertainty_meters`: GPS accuracy uncertainty in meters (from `uncertaintyMeters`)
- `gps_time_s`: GPS time in seconds (from `gpsTimeS`)
- `utc_offset_s`: UTC offset in seconds (from `utcOffsetS`)

**API Priority**:
1. `get_diagnostics` (most comprehensive, includes uncertainty data)
2. `get_location` (high precision coordinates)
3. RUTOS `gpsctl` (fallback for local GPS)

### Health Monitoring

#### `check_starlink_health()`

Comprehensive health assessment using `get_diagnostics` data.

**Output Format**: `overall,hardware_test,dl_bw_reason,ul_bw_reason,thermal_throttle,thermal_shutdown,roaming`

**Monitored Parameters**:

1. **Hardware Self-Test** (`hardwareSelfTest`)
   - Expected: `"PASSED"`
   - **CRITICAL**: Any other value indicates hardware failure
   - **Failover Trigger**: Immediate

2. **Bandwidth Restrictions**
   - `dlBandwidthRestrictedReason` - Downlink restrictions
   - `ulBandwidthRestrictedReason` - Uplink restrictions
   - Expected: `"NO_LIMIT"`
   - **DEGRADED**: Any restriction indicates performance impact
   - **Examples**: Data cap exceeded, network congestion, policy limits

3. **Thermal Alerts**
   - `alerts.thermalThrottle` - Performance reduced due to heat
   - `alerts.thermalShutdown` - Risk of emergency shutdown
   - **CRITICAL**: Thermal shutdown imminent
   - **DEGRADED**: Thermal throttling active

4. **Roaming Alert** (`alerts.roaming`)
   - Indicates device operating outside home region
   - **DEGRADED**: May impact performance or cost

#### `should_trigger_failover(health_status)`

Automated failover decision logic based on health assessment.

**Failover Triggers**:
- `critical` status (hardware failure, thermal shutdown)
- `unknown` status (diagnostics unavailable)

**No Failover**:
- `healthy` status (all systems normal)
- `degraded` status (monitoring closely, not critical yet)

## Health Status Levels

### HEALTHY
- Hardware self-test: `PASSED`
- No bandwidth restrictions
- No thermal issues
- No critical alerts

### DEGRADED
- Thermal throttling active
- Bandwidth restrictions present
- Roaming alerts
- Performance impacted but functional

### CRITICAL
- Hardware self-test failed
- Thermal shutdown imminent
- System failure likely
- **Immediate failover recommended**

### UNKNOWN
- Diagnostics unavailable
- API communication failed
- **Failover recommended as precaution**

## Implementation Examples

### Basic Enhanced GPS Collection
```bash
# Enable GPS logging
ENABLE_GPS_LOGGING="true"

# Collect enhanced GPS data
gps_data=$(collect_gps_data_enhanced)
echo "GPS: $gps_data"

# Parse individual components
lat=$(echo "$gps_data" | cut -d',' -f1)
uncertainty=$(echo "$gps_data" | cut -d',' -f6)
echo "Position: $lat with ${uncertainty}m uncertainty"
```

### Health Monitoring with Failover
```bash
# Check Starlink health
health_status=$(check_starlink_health)
echo "Health: $health_status"

# Determine if failover needed
if should_trigger_failover "$health_status"; then
    echo "🚨 FAILOVER TRIGGERED: Switching to cellular backup"
    # Trigger failover logic here
else
    echo "✅ System healthy, continuing with Starlink"
fi
```

### Integration with Monitoring Scripts
```bash
# In monitoring scripts
health_data=$(check_starlink_health)
overall_status=$(echo "$health_data" | cut -d',' -f1)

case "$overall_status" in
    critical)
        log_error "CRITICAL: Starlink health failure detected"
        trigger_immediate_failover
        ;;
    degraded)
        log_warning "DEGRADED: Starlink performance impacted"
        increase_monitoring_frequency
        ;;
    healthy)
        log_debug "Starlink health normal"
        ;;
esac
```

## API Requirements

### Required Tools
- `grpcurl` - Starlink API communication
- `jq` - JSON parsing

### Environment Variables
- `GRPCURL_CMD` - Path to grpcurl binary
- `JQ_CMD` - Path to jq binary  
- `STARLINK_IP` - Starlink dish IP (default: 192.168.100.1)
- `STARLINK_PORT` - Starlink API port (default: 9200)

### Configuration
- `ENABLE_GPS_LOGGING` - Enable GPS collection
- `ENABLE_HEALTH_MONITORING` - Enable health checks
- `GPS_PRIMARY_SOURCE` - Preferred GPS source (starlink/rutos)

## Benefits

### Enhanced GPS Accuracy
- **Uncertainty measurements** help assess GPS reliability
- **Multiple source prioritization** ensures best available data
- **Timing information** enables temporal correlation

### Proactive Health Monitoring
- **Hardware failure detection** before total failure
- **Thermal protection** prevents damage
- **Performance degradation alerts** for quality issues
- **Automated failover decisions** reduce downtime

### Operational Intelligence
- **Bandwidth restriction awareness** for capacity planning
- **Environmental monitoring** for deployment decisions
- **Comprehensive diagnostics** for troubleshooting

## Migration from Basic Functions

The enhanced functions are **backwards compatible** - existing scripts using `collect_gps_data()` continue to work unchanged.

**Migration Path**:
1. Add enhanced functions alongside existing ones
2. Update monitoring scripts to use enhanced data
3. Implement health-based failover logic
4. Gradually replace basic functions where more data is needed

## Testing

Use the test script to verify enhanced functionality:
```bash
./test-enhanced-data-collection.sh
```

This validates:
- Enhanced GPS collection with diagnostics
- Health monitoring and status assessment
- Failover decision logic
- Backwards compatibility with existing functions
