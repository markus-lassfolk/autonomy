# SNR Logic and MWAN3 Service Handling Fixes

## Overview
Fixed two critical issues that were causing unnecessary failovers and service instability:

1. **Incorrect SNR failover logic** - triggering failover on perfectly good connections
2. **MWAN3 service polling without proper delays** - causing rapid file access errors

## Problem Analysis

### Issue 1: Incorrect SNR Logic
**Problem**: The script was triggering failover on excellent connections due to misinterpreted SNR readyStates.

**Your Connection Data**:
- Latency: **26.5ms** (threshold 80ms) ✅ **EXCELLENT**
- Packet Loss: **0%** (threshold 5%) ✅ **PERFECT**  
- Obstruction: **0.4%** (threshold 5%) ✅ **EXCELLENT**
- SNR readyStates: `above_noise_floor=false`, `persistently_low=false`

**Flawed Logic**:
```bash
if [ "$snr_above_noise" = "false" ] || [ "$snr_persistently_low" = "true" ]; then
    snr_poor=1  # WRONG - triggering on above_noise=false
```

**Problem**: `above_noise_floor=false` doesn't necessarily mean "bad signal" - it could mean:
- Signal measurement not available
- Different measurement methodology
- Transitional state

**The Real Indicator**: `persistently_low=true` is the reliable indicator of poor SNR.

### Issue 2: MWAN3 Service Polling
**Problem**: After `mwan3 reload`, the script continued without waiting for service initialization, causing:
```
cat: can't open '/tmp/run/mwan3/active_wan': No such file or directory
```

## Solutions Implemented

### 1. Corrected SNR Logic

**New Logic**:
```bash
# Primary check: Only fail on persistently low SNR
if [ "$snr_persistently_low" = "true" ]; then
    snr_poor=1
    log_warning "Poor SNR detected: persistently low signal"
elif [ "$snr_above_noise" = "false" ] && [ "$current_snr" != "unknown" ]; then
    # Secondary check: If above_noise=false AND numeric SNR < 3dB
    if [ "$current_snr" -lt 3 ]; then
        snr_poor=1
        log_warning "Poor SNR detected: Very low SNR ($current_snr dB)"
    else
        log_debug "SNR acceptable despite above_noise=false (SNR >= 3dB)"
    fi
else
    log_debug "SNR is good (persistently_low=false, readyStates normal)"
fi
```

**Benefits**:
- **Prevents false positives**: Won't failover on good connections
- **Conservative approach**: Only triggers on genuine signal problems
- **Dual validation**: Uses both readyStates AND numeric thresholds
- **Proper logging**: Clear warnings only when actually needed

### 2. Enhanced MWAN3 Service Handling

**Added Features**:
```bash
# Wait for mwan3 service to settle
sleep 3

# Verify service readiness with timeout
mwan3_ready=0
for i in 1 2 3 4 5; do
    if [ -f "/var/run/mwan3.pid" ] || [ -d "/var/run/mwan3" ] || [ -f "/tmp/run/mwan3.pid" ]; then
        mwan3_ready=1
        log_debug "MWAN3 service appears ready (attempt $i/5)"
        break
    else
        log_debug "MWAN3 service not ready yet (attempt $i/5), waiting..."
        sleep 1
    fi
done
```

**Benefits**:
- **Service stability**: Waits for mwan3 to fully initialize
- **Error prevention**: Reduces file access errors from rapid polling
- **Graceful handling**: Continues operation even if status files aren't found
- **Debugging**: Clear logging of service readiness checks

## Impact on Your System

### Before Fix
- **Unnecessary failovers** on excellent connections (26ms latency, 0% loss)
- **Service errors** due to rapid mwan3 file polling
- **Poor user experience** with false positive alerts

### After Fix
- **Accurate failover decisions** based on real signal problems
- **Stable service operations** with proper initialization delays
- **Reliable notifications** only when genuinely needed

## Expected Behavior Changes

### SNR Evaluation
**Your Connection (after fix)**:
- `above_noise_floor=false` + `persistently_low=false` = **GOOD** ✅
- Numeric SNR `5.0dB` > `3dB` threshold = **ACCEPTABLE** ✅
- **Result**: No unnecessary failover

### Service Reliability
- **3-second delay** after mwan3 reload
- **5-attempt verification** of service readiness
- **Graceful continuation** if status files not found
- **Reduced error messages** from file access issues

## Testing the Fixes

### Test SNR Logic
```bash
# Test with your current connection (should NOT trigger failover)
DEBUG=1 CONFIG_FILE=/etc/starlink-config/config.sh /usr/local/starlink-monitor/scripts/starlink_monitor_unified-rutos.sh

# Look for these logs:
# [DEBUG] SNR DECISION LOGIC: SNR is good (persistently_low=false, readyStates normal)
# [INFO] 🔍 DECISION: Evaluated connection quality - connection_stable
```

### Test Service Handling
```bash
# Watch for proper service handling logs:
# [DEBUG] Waiting for mwan3 service to initialize...
# [DEBUG] MWAN3 service appears ready (attempt 1/5)
```

## Configuration Impact

### SNR Thresholds
- **Conservative approach**: Only triggers on `persistently_low=true`
- **Backup threshold**: `< 3dB` for very low signals with `above_noise=false`
- **No changes needed** to your configuration

### Service Timing
- **Built-in delays**: 3-second initial wait + 1-second verification intervals
- **Timeout protection**: Max 8 seconds total wait time
- **Automatic continuation**: Proceeds even if verification fails

This fix ensures your excellent connection (26ms latency, 0% loss, 0.4% obstruction) won't trigger unnecessary failovers while maintaining protection against genuine signal degradation.
