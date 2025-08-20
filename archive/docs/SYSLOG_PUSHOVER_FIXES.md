# Syslog and Pushover Notification Fixes

## Overview
Fixed two critical issues affecting operational visibility:
1. **Missing human-readable syslog messages** for failover events
2. **Missing Pushover notifications** during actual failovers

## Problems Identified

### 1. Syslog Integration Missing
- **Issue**: Library logging functions only output to console/stderr, not to syslog
- **Impact**: System administrators couldn't see failover events in standard syslog tools
- **Root Cause**: `_log_message()` function in `scripts/lib/rutos-logging.sh` only used `printf`

### 2. Pushover Function Not Available
- **Issue**: Main monitoring script called `send_pushover_notification()` but function was undefined
- **Impact**: No Pushover notifications sent during actual failover events
- **Root Cause**: Function exists in other scripts but not loaded/available in monitoring script

## Solutions Implemented

### 1. Enhanced Syslog Integration

**File**: `scripts/lib/rutos-logging.sh`

**Changes**:
- Modified `_log_message()` to include syslog integration with `logger` command
- Added syslog priority parameter with appropriate levels:
  - `daemon.info` for INFO, SUCCESS, STEP messages
  - `daemon.warn` for WARNING messages  
  - `daemon.err` for ERROR messages
  - `daemon.debug` for DEBUG and TRACE messages
- Uses `LOG_TAG` or script name for syslog identification

**Result**: All log messages now appear in both console output AND system syslog

### 2. Robust Pushover Notification Function

**File**: `Starlink-RUTOS-Failover/starlink_monitor_unified-rutos.sh`

**Added Features**:
- Complete `send_pushover_notification()` function with multiple fallback strategies
- Integrates with existing `placeholder-utils.sh` if available
- Direct API calls as fallback when utilities unavailable
- Placeholder detection to avoid sending notifications with unconfigured values
- Comprehensive error handling and logging

**Function Logic**:
1. Try `safe_send_notification()` from placeholder utilities (recommended)
2. Fallback to direct Pushover API calls if credentials configured
3. Skip gracefully if not configured or using placeholder values
4. Log all attempts and results for debugging

## Testing the Fixes

### Test Syslog Integration
```bash
# Run a script that uses the logging library
./scripts/install-rutos.sh --dry-run

# Check syslog for messages
tail -f /var/log/syslog | grep -E "(RutosScript|StarlinkMonitor)"
```

### Test Pushover Notifications
```bash
# Set proper Pushover credentials
export PUSHOVER_TOKEN="your_actual_token"
export PUSHOVER_USER="your_actual_user_key"

# Run monitoring script or trigger a test notification
./Starlink-RUTOS-Failover/starlink_monitor_unified-rutos.sh
```

## Configuration Requirements

### For Syslog
- No additional configuration required
- Uses system `logger` command (available on all RUTOS devices)
- Automatically uses `LOG_TAG` variable or script name for identification

### For Pushover
- Set `PUSHOVER_TOKEN` and `PUSHOVER_USER` environment variables
- Or configure in your `.env` file or configuration script
- Function automatically detects and skips placeholder values

## Benefits

### Improved Operational Visibility
- **Syslog Integration**: Failover events now visible in standard system logs
- **Human-Readable Messages**: Rich context including metrics, GPS, cellular data
- **Proper Log Levels**: Different message types use appropriate syslog priorities

### Reliable Notifications
- **Multiple Fallback Strategies**: Works with or without placeholder utilities
- **Graceful Degradation**: Continues operation even if notifications fail
- **Debug Information**: Comprehensive logging of notification attempts

### System Integration
- **Standard Tools**: Works with existing syslog infrastructure
- **Monitoring Integration**: Messages can be forwarded to external monitoring systems
- **Searchable Logs**: Standardized format for log analysis tools

## Examples of Enhanced Logging

### Syslog Output
```
Jan 15 10:30:15 router StarlinkMonitor: [ERROR] 🚨 DECISION: Hard failover triggered - quality_degraded
Jan 15 10:30:15 router StarlinkMonitor: [INFO] 📊 METRICS: Latency=250ms (threshold 150ms), Loss=5.2% (threshold 2%), Obstruction=0.5% (threshold 0.001%)
Jan 15 10:30:15 router StarlinkMonitor: [INFO] 🎯 ACTION: metric_increase → success (metric: 1 → 10)
```

### Console Output (with colors)
```
[ERROR] [2025-01-15 10:30:15] 🚨 DECISION: Hard failover triggered - quality_degraded
[INFO] [2025-01-15 10:30:15] 📊 METRICS: Latency=250ms (threshold 150ms), Loss=5.2% (threshold 2%), Obstruction=0.5% (threshold 0.001%)
[INFO] [2025-01-15 10:30:15] 🎯 ACTION: metric_increase → success (metric: 1 → 10)
```

## Impact on Debugging

### Before Fix
- No syslog visibility of failover events
- Had to check script output directly
- Missing Pushover notifications during real failures

### After Fix  
- Full syslog integration with proper priorities
- Human-readable messages with rich context
- Reliable Pushover notifications with fallback strategies
- Comprehensive debugging information

This enhancement significantly improves the operational visibility and reliability of the RUTOS Starlink failover system.
