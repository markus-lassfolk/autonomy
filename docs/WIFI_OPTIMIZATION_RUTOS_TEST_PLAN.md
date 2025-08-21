# WiFi Optimization RUTOS Hardware Test Plan

## 🎯 **Test Objectives**

Validate the fully integrated WiFi optimization system on actual RUTOS hardware to ensure:
1. ✅ **GPS-based movement triggers** work correctly
2. ✅ **Nightly/weekly scheduler** executes as configured  
3. ✅ **Channel optimization** improves WiFi performance
4. ✅ **System integration** works with main daemon
5. ✅ **Error handling** gracefully handles all failure modes

---

## 🔧 **Prerequisites**

### **Hardware Requirements**
- RUTOS device (RUTX50 recommended)
- Active WiFi interfaces (2.4GHz and 5GHz)
- GPS capability (for movement detection)
- SSH access to device

### **Software Requirements**
- Latest autonomy daemon with WiFi optimization (`autonomyd-linux-arm-new`)
- UCI configuration access
- System logs access (`logread`)

### **Network Environment**
- Multiple WiFi networks visible (for channel optimization testing)
- Ability to move device physically (for GPS testing)
- Stable power supply during testing

---

## 📋 **Test Procedures**

### **Phase 1: Basic Integration Test**

#### **Test 1.1: Daemon Startup with WiFi Optimization**
```bash
# SSH to RUTOS device
ssh -i "C:\path\to\your\ssh\key" root@your-router-ip

# Deploy new daemon
scp -i "C:\path\to\your\ssh\key" autonomyd-linux-arm-new root@your-router-ip:/tmp/autonomyd-new

# Enable WiFi optimization
uci set autonomy.main.wifi_optimization_enabled='1'
uci commit autonomy

# Test daemon startup
/tmp/autonomyd-new -config /etc/config/autonomy -foreground -verbose
```

**Expected Results:**
- ✅ Daemon starts without errors
- ✅ WiFi optimization services initialize
- ✅ GPS-WiFi integration starts (if GPS available)
- ✅ WiFi scheduler starts
- ✅ System management includes WiFi health checks

**Success Criteria:**
```
INFO: WiFi optimizer created enabled=true
INFO: GPS-WiFi manager created movement_threshold=100
INFO: WiFi scheduler created nightly_enabled=true weekly_enabled=false
INFO: WiFi optimization services started successfully
```

---

### **Phase 2: Configuration Validation**

#### **Test 2.1: UCI Configuration Loading**
```bash
# Check all WiFi settings are loaded
uci show autonomy | grep wifi

# Verify default values
uci get autonomy.main.wifi_optimization_enabled
uci get autonomy.main.wifi_movement_threshold
uci get autonomy.main.wifi_nightly_optimization
uci get autonomy.main.wifi_nightly_time
```

**Expected Results:**
- ✅ All 23 WiFi UCI options are available
- ✅ Default values match documentation
- ✅ Configuration changes are applied without restart

#### **Test 2.2: Configuration Hot Reload**
```bash
# Change configuration while daemon running
uci set autonomy.main.wifi_min_improvement='20'
uci set autonomy.main.wifi_nightly_time='02:30'
uci commit autonomy

# Send SIGHUP to reload config (if supported)
killall -HUP autonomyd-new

# Monitor logs for config reload
logread -f | grep -i "wifi\|config"
```

**Expected Results:**
- ✅ Configuration reloads without daemon restart
- ✅ New settings take effect immediately
- ✅ No errors in logs

---

### **Phase 3: Scheduled Optimization Testing**

#### **Test 3.1: Nightly Optimization**
```bash
# Configure nightly optimization for immediate testing
uci set autonomy.main.wifi_nightly_optimization='1'
uci set autonomy.main.wifi_nightly_time='$(date -d "+2 minutes" +%H:%M)'
uci set autonomy.main.wifi_nightly_window='5'  # 5 minute window
uci commit autonomy

# Monitor logs for scheduled execution
logread -f | grep -i "nightly\|scheduled\|wifi.*optimization"
```

**Expected Results:**
- ✅ Scheduler calculates next execution time correctly
- ✅ Optimization triggers within the specified window
- ✅ Logs show "nightly_scheduled" trigger
- ✅ Channel optimization completes successfully

**Success Log Pattern:**
```json
{
  "level": "info",
  "msg": "Starting WiFi channel optimization",
  "trigger": "nightly_scheduled",
  "scheduled_time": "02:32",
  "execution_window_min": 5
}
```

#### **Test 3.2: Weekly Optimization**
```bash
# Enable weekly optimization for current day
CURRENT_DAY=$(date +%A | tr '[:upper:]' '[:lower:]')
uci set autonomy.main.wifi_weekly_optimization='1'
uci set autonomy.main.wifi_weekly_days="$CURRENT_DAY"
uci set autonomy.main.wifi_weekly_time='$(date -d "+3 minutes" +%H:%M)'
uci commit autonomy

# Monitor for weekly execution
logread -f | grep -i "weekly\|scheduled"
```

**Expected Results:**
- ✅ Weekly scheduler recognizes current day
- ✅ Optimization triggers at specified time
- ✅ Logs show "weekly_scheduled" trigger

---

### **Phase 4: GPS Movement Detection Testing**

#### **Test 4.1: GPS Availability Check**
```bash
# Check GPS status
gpsctl -s
cat /dev/gps0  # If available

# Check GPS in autonomy logs
logread | grep -i gps | tail -10

# Verify GPS accuracy
logread -f | grep -i "gps.*accuracy\|location"
```

**Expected Results:**
- ✅ GPS device is accessible
- ✅ GPS coordinates are being collected
- ✅ GPS accuracy meets threshold (50m default)

#### **Test 4.2: Movement Detection**
```bash
# Record initial position
logread | grep -i "gps.*lat\|location" | tail -1

# Physically move device >100 meters
# Wait for GPS to update (may take 1-2 minutes)

# Monitor for movement detection
logread -f | grep -i "movement\|stationary\|distance"
```

**Expected Results:**
- ✅ Movement is detected when >100m threshold exceeded
- ✅ Stationary timer starts after movement stops
- ✅ Optimization triggers after 30 minutes stationary

**Success Log Pattern:**
```json
{
  "level": "info",
  "msg": "Starting WiFi channel optimization",
  "trigger": "movement_detected",
  "gps_lat": 45.123456,
  "gps_lon": -122.654321,
  "movement_distance_m": 150.5,
  "stationary_time_min": 32
}
```

#### **Test 4.3: GPS Accuracy Filtering**
```bash
# Monitor GPS accuracy in logs
logread -f | grep -i "gps.*accuracy\|threshold"

# Test with poor GPS signal (indoor/covered area)
# Verify optimization is skipped with poor accuracy
```

**Expected Results:**
- ✅ Optimization skipped when GPS accuracy > 50m threshold
- ✅ Logs show accuracy filtering messages

---

### **Phase 5: Channel Optimization Validation**

#### **Test 5.1: WiFi Interface Detection**
```bash
# Check WiFi interfaces
iwconfig
iw dev

# Monitor interface detection in logs
logread -f | grep -i "interface.*detect\|wifi.*interface"
```

**Expected Results:**
- ✅ Both 2.4GHz and 5GHz interfaces detected
- ✅ Interface classification correct (band detection)

#### **Test 5.2: Channel Scanning and Selection**
```bash
# Trigger manual optimization (if ubus API available)
# Otherwise wait for scheduled/movement trigger

# Monitor optimization process
logread -f | grep -i "channel\|optimization\|scan"
```

**Expected Results:**
- ✅ RF environment scanning completes
- ✅ Channel scoring algorithm runs
- ✅ Optimal channels selected for both bands
- ✅ Channel width optimization (HT20/VHT40/VHT80)

**Success Log Pattern:**
```json
{
  "level": "info",
  "msg": "Optimal channel plan determined",
  "channel_24": 6,
  "score_24": 85,
  "channel_5": 149,
  "score_5": 92,
  "width_5": "VHT80",
  "total_score": 88.5,
  "improvement": 23.2
}
```

#### **Test 5.3: Channel Application**
```bash
# Monitor UCI changes during optimization
logread -f | grep -i "uci\|wireless\|commit"

# Verify actual channel changes
iwconfig
uci show wireless | grep -E "channel|htmode"
```

**Expected Results:**
- ✅ UCI wireless configuration updated
- ✅ Channels applied to interfaces
- ✅ WiFi interfaces restart with new settings
- ✅ Improvement threshold respected (min 15 points)

---

### **Phase 6: Error Handling and Edge Cases**

#### **Test 6.1: No GPS Available**
```bash
# Disable GPS or test without GPS hardware
# Monitor graceful degradation
logread -f | grep -i "gps.*not.*available\|gps.*integration"
```

**Expected Results:**
- ✅ Daemon starts successfully without GPS
- ✅ Only scheduled optimization available
- ✅ No GPS-related errors in logs

#### **Test 6.2: WiFi Interface Issues**
```bash
# Temporarily disable WiFi interfaces
ifconfig wlan0 down
ifconfig wlan1 down

# Monitor error handling
logread -f | grep -i "wifi.*interface\|interface.*down"
```

**Expected Results:**
- ✅ Graceful handling of missing interfaces
- ✅ Optimization skipped with appropriate logging
- ✅ No daemon crashes or errors

#### **Test 6.3: UCI Configuration Errors**
```bash
# Test with invalid configuration
uci set autonomy.main.wifi_nightly_time='invalid'
uci set autonomy.main.wifi_movement_threshold='-100'
uci commit autonomy

# Monitor validation and defaults
logread -f | grep -i "invalid\|default\|validation"
```

**Expected Results:**
- ✅ Invalid values rejected or defaulted
- ✅ Warning messages for invalid configuration
- ✅ System continues with valid defaults

---

### **Phase 7: Performance and Integration**

#### **Test 7.1: System Resource Usage**
```bash
# Monitor CPU and memory usage
top -p $(pgrep autonomyd-new)
ps aux | grep autonomyd-new

# Check for memory leaks over time
# Run for several hours and monitor RSS
```

**Expected Results:**
- ✅ CPU usage <5% during normal operation
- ✅ Memory usage <25MB steady state
- ✅ No memory leaks over extended operation

#### **Test 7.2: Integration with Main Daemon Features**
```bash
# Test failover while WiFi optimization running
# Simulate network interface changes
# Monitor interaction between systems

logread -f | grep -E "failover|wifi|optimization|decision"
```

**Expected Results:**
- ✅ WiFi optimization doesn't interfere with failover
- ✅ System management health checks include WiFi
- ✅ No conflicts between subsystems

---

## 📊 **Test Results Documentation**

### **Test Report Template**
```
RUTOS WiFi Optimization Test Report
Date: [DATE]
Device: [RUTOS MODEL]
Firmware: [VERSION]
Test Duration: [HOURS]

PHASE 1 - BASIC INTEGRATION:
[ ] Daemon startup: PASS/FAIL
[ ] Component initialization: PASS/FAIL
[ ] Service startup: PASS/FAIL

PHASE 2 - CONFIGURATION:
[ ] UCI loading: PASS/FAIL
[ ] Hot reload: PASS/FAIL
[ ] Validation: PASS/FAIL

PHASE 3 - SCHEDULED OPTIMIZATION:
[ ] Nightly optimization: PASS/FAIL
[ ] Weekly optimization: PASS/FAIL
[ ] Scheduler accuracy: PASS/FAIL

PHASE 4 - GPS MOVEMENT:
[ ] GPS availability: PASS/FAIL
[ ] Movement detection: PASS/FAIL
[ ] Accuracy filtering: PASS/FAIL

PHASE 5 - CHANNEL OPTIMIZATION:
[ ] Interface detection: PASS/FAIL
[ ] Channel scanning: PASS/FAIL
[ ] Channel application: PASS/FAIL

PHASE 6 - ERROR HANDLING:
[ ] No GPS graceful: PASS/FAIL
[ ] Interface errors: PASS/FAIL
[ ] Config validation: PASS/FAIL

PHASE 7 - PERFORMANCE:
[ ] Resource usage: PASS/FAIL
[ ] System integration: PASS/FAIL

OVERALL RESULT: PASS/FAIL
NOTES: [DETAILED OBSERVATIONS]
```

---

## 🚀 **Quick Test Commands**

### **Rapid Deployment and Test**
```bash
# Complete test sequence
ssh -i "C:\path\to\your\ssh\key" root@your-router-ip "
  # Enable WiFi optimization
  uci set autonomy.main.wifi_optimization_enabled='1' &&
  uci set autonomy.main.wifi_nightly_optimization='1' &&
  uci set autonomy.main.wifi_nightly_time='$(date -d '+2 minutes' +%H:%M)' &&
  uci commit autonomy &&
  
  # Deploy and test
  echo 'WiFi optimization enabled, monitoring logs...' &&
  logread -f | grep -i wifi
"
```

### **Status Check Commands**
```bash
# Check WiFi optimization status
uci get autonomy.main.wifi_optimization_enabled
ps aux | grep autonomyd
logread | grep -i "wifi.*optimization" | tail -5

# Check GPS status
gpsctl -s 2>/dev/null || echo "GPS not available"

# Check WiFi interfaces
iwconfig 2>/dev/null | grep -E "wlan|IEEE"
```

---

## ✅ **Success Criteria Summary**

The WiFi optimization system passes testing if:

1. ✅ **Daemon Integration** - Starts and runs without errors
2. ✅ **Configuration** - All UCI settings work correctly
3. ✅ **Scheduling** - Nightly/weekly optimization executes on time
4. ✅ **GPS Integration** - Movement detection triggers optimization
5. ✅ **Channel Optimization** - Improves WiFi performance measurably
6. ✅ **Error Handling** - Graceful degradation in all failure modes
7. ✅ **Performance** - Minimal resource usage, no interference
8. ✅ **Logging** - Comprehensive, structured logs for troubleshooting

---

**This test plan provides comprehensive validation of the WiFi optimization system on actual RUTOS hardware. Execute these tests to verify production readiness!** 🎯
