# Hybrid Weight System Documentation

## 🎯 Overview

The **Hybrid Weight System** is a revolutionary approach to network failover that **respects your MWAN3 configuration** while adding intelligent monitoring and temporary adjustments. Unlike the old system that overrode all your carefully configured weights, this system works **with** MWAN3, not against it.

## 🤔 The Problem with the Old System

### ❌ Old Approach (Override Everything)
```bash
# Your MWAN3 Configuration
starlink_m1:  weight 100
cellular1_m1: weight 85  
cellular2_m1: weight 84
cellular3_m1: weight 83
# ... carefully configured priorities

# What autonomy Did (BAD!)
starlink_m1:  weight 100  ← Selected interface
cellular1_m1: weight 10   ← Everything else forced to 10
cellular2_m1: weight 10   ← Your configuration ignored
cellular3_m1: weight 10   ← User loses control
```

**Problems:**
- 👤 **User loses control** - Your careful weight configuration is ignored
- 🐛 **Hard to debug** - Two conflicting weight systems
- ⚙️ **Unnecessary complexity** - Duplicates MWAN3 functionality
- 🔄 **Poor integration** - Fights against MWAN3 instead of working with it

## ✅ The New Hybrid Approach

### 🎯 Respects Your Configuration
```bash
# Your MWAN3 Configuration (PRESERVED!)
starlink_m1:  weight 100  ← Your preference
cellular1_m1: weight 85   ← Your preference  
cellular2_m1: weight 84   ← Your preference
cellular3_m1: weight 83   ← Your preference

# What Hybrid System Does (GOOD!)
# Normal operation: Uses your weights exactly as configured
# Intelligent adjustments: Only when conditions warrant
# Temporary changes: Automatically restored after conditions improve
```

## 🧠 How It Works

### 1. **Normal Operation**
- Uses your MWAN3 weights exactly as configured
- No interference with your priority system
- MWAN3 handles failover based on your preferences + health checks

### 2. **Intelligent Monitoring**
- Monitors Starlink obstructions, outages, dish health
- Tracks cellular signal strength, roaming status
- Analyzes latency, packet loss, performance trends
- Detects emergency situations

### 3. **Temporary Adjustments**
- **Penalties**: Reduce weight when issues detected
- **Boosts**: Increase weight when conditions are excellent
- **Emergency Overrides**: Critical situations only
- **Automatic Restoration**: Return to user weights when conditions improve

### 4. **Configurable Behavior**
- Full control via UCI configuration
- Enable/disable each feature independently
- Adjust thresholds and durations
- Emergency-only mode available

## ⚙️ Configuration

### UCI Configuration Options

```bash
# Enable hybrid weight system
uci set autonomy.main.respect_user_weights='1'      # Respect MWAN3 weights
uci set autonomy.main.dynamic_adjustment='1'        # Allow intelligent adjustments
uci set autonomy.main.emergency_override='1'        # Allow emergency overrides
uci set autonomy.main.only_emergency_override='1'   # Only override in emergencies
uci set autonomy.main.restore_timeout_s='300'       # Restore after 5 minutes

# Configure adjustment behavior
uci set autonomy.main.minimal_adjustment_points='10'           # Small adjustments (+/-10)
uci set autonomy.main.temporary_boost_points='20'             # Moderate boosts (+20)
uci set autonomy.main.temporary_adjustment_duration_s='300'    # 5 minutes
uci set autonomy.main.emergency_adjustment_duration_s='900'    # 15 minutes

# Configure intelligent thresholds
uci set autonomy.main.starlink_obstruction_threshold='10.0'    # 10% obstruction
uci set autonomy.main.cellular_signal_threshold='-110.0'      # -110 dBm
uci set autonomy.main.latency_degradation_threshold='500.0'    # 500ms
uci set autonomy.main.loss_threshold='5.0'                    # 5% packet loss

uci commit autonomy
```

### Configuration Modes

#### 🎯 **Full Hybrid Mode** (Recommended)
```bash
uci set autonomy.main.respect_user_weights='1'
uci set autonomy.main.dynamic_adjustment='1'
uci set autonomy.main.emergency_override='1'
```
- Respects your MWAN3 weights
- Applies intelligent adjustments when needed
- Allows emergency overrides in critical situations

#### 🛡️ **Conservative Mode**
```bash
uci set autonomy.main.respect_user_weights='1'
uci set autonomy.main.dynamic_adjustment='1'
uci set autonomy.main.emergency_override='0'
```
- Respects your MWAN3 weights
- Only minor adjustments, no emergency overrides
- Maximum respect for user configuration

#### 🚨 **Emergency Only Mode**
```bash
uci set autonomy.main.respect_user_weights='1'
uci set autonomy.main.dynamic_adjustment='0'
uci set autonomy.main.emergency_override='1'
uci set autonomy.main.only_emergency_override='1'
```
- Respects your MWAN3 weights
- No routine adjustments
- Only intervenes in true emergencies

#### ⚡ **Legacy Mode** (Not Recommended)
```bash
uci set autonomy.main.respect_user_weights='0'
```
- Disables hybrid system
- Falls back to old override behavior
- Only use if you have issues with hybrid system

## 🎯 Example Scenarios

### Scenario 1: Normal Operation
```
Your MWAN3 Config:
  starlink_m1:  100
  cellular1_m1: 85
  cellular2_m1: 84

Hybrid System:
  ✅ Uses weights exactly as configured
  ✅ MWAN3 handles failover naturally
  ✅ No interference from autonomy
```

### Scenario 2: Starlink Obstruction
```
Condition: Starlink obstruction 15% (threshold: 10%)

Your MWAN3 Config:
  starlink_m1:  100
  cellular1_m1: 85
  cellular2_m1: 84

Hybrid Adjustment:
  starlink_m1:  80  ← Temporary penalty (-20)
  cellular1_m1: 85  ← Unchanged
  cellular2_m1: 84  ← Unchanged

Result: Cellular becomes preferred until obstruction clears
Auto-restore: After 5 minutes, starlink_m1 returns to 100
```

### Scenario 3: Excellent Cellular Signal
```
Condition: Cellular1 signal -65dBm (excellent) + Starlink issues

Your MWAN3 Config:
  starlink_m1:  100
  cellular1_m1: 85
  cellular2_m1: 84

Hybrid Adjustment:
  starlink_m1:  100 ← Unchanged (or penalized if issues)
  cellular1_m1: 95  ← Temporary boost (+10)
  cellular2_m1: 84  ← Unchanged

Result: Cellular1 gets priority due to excellent conditions
Auto-restore: After 10 minutes, cellular1_m1 returns to 85
```

### Scenario 4: Emergency Situation
```
Condition: All interfaces down except cellular2

Your MWAN3 Config:
  starlink_m1:  100 (DOWN)
  cellular1_m1: 85  (DOWN)
  cellular2_m1: 84  (UP)

Emergency Override:
  starlink_m1:  100 ← Unchanged (down)
  cellular1_m1: 85  ← Unchanged (down)
  cellular2_m1: 100 ← Emergency boost to ensure priority

Result: Cellular2 gets maximum priority in emergency
Auto-restore: After 15 minutes, cellular2_m1 returns to 84
```

## 📊 Monitoring and Debugging

### Weight Status Commands
```bash
# View current weight status
autonomyctl status weights

# View active adjustments
autonomyctl status adjustments

# Restore all weights to user configuration
autonomyctl restore weights

# View weight history
autonomyctl logs weights
```

### Log Messages
```
INFO: Hybrid weight system initialized, loaded 8 MWAN3 weights
INFO: Applied penalty: starlink_m1 weight 100 → 80 (obstruction: 15.2%)
INFO: Applied boost: cellular1_m1 weight 85 → 95 (excellent signal: -65dBm)
INFO: Weight adjustment expired, restored: starlink_m1 100
INFO: Emergency override: cellular2_m1 weight 84 → 100 (critical situation)
```

## 🔧 Troubleshooting

### Issue: Weights Not Being Respected
```bash
# Check if hybrid system is enabled
uci get autonomy.main.respect_user_weights

# Check if MWAN3 weights were loaded
autonomyctl status weights

# Verify MWAN3 configuration
uci show mwan3 | grep weight
```

### Issue: Too Many Adjustments
```bash
# Increase thresholds to reduce sensitivity
uci set autonomy.main.starlink_obstruction_threshold='20.0'  # Less sensitive
uci set autonomy.main.cellular_signal_threshold='-120.0'    # Less sensitive

# Disable dynamic adjustments
uci set autonomy.main.dynamic_adjustment='0'

# Enable emergency-only mode
uci set autonomy.main.only_emergency_override='1'
```

### Issue: Adjustments Not Working
```bash
# Check if dynamic adjustment is enabled
uci get autonomy.main.dynamic_adjustment

# Check adjustment thresholds
uci show autonomy.main | grep threshold

# View recent logs
logread | grep autonomy | grep weight
```

## 🎉 Benefits Summary

### ✅ For Users
- 👤 **Full Control**: Your MWAN3 weights are respected
- 🎯 **Predictable**: System behaves as you configured
- 🐛 **Easy Debugging**: Clear separation of user config vs system intelligence
- ⚙️ **Configurable**: Full control via UCI
- 🔄 **Automatic**: Intelligent adjustments when needed, restoration when conditions improve

### ✅ For System
- 🧠 **Intelligent**: Monitors real conditions and adjusts accordingly
- 🚀 **Efficient**: Only intervenes when necessary
- 🛡️ **Safe**: Automatic restoration prevents permanent changes
- 📊 **Observable**: Full logging and status reporting
- 🔧 **Maintainable**: Clean architecture, easy to understand and modify

## 🚀 Migration from Old System

### Step 1: Configure Your MWAN3 Weights
```bash
# Set your preferred priorities in MWAN3
uci set mwan3.starlink_m1.weight='100'
uci set mwan3.cellular1_m1.weight='85'
uci set mwan3.cellular2_m1.weight='84'
uci set mwan3.cellular3_m1.weight='83'
uci set mwan3.cellular4_m1.weight='82'
uci set mwan3.cellular5_m1.weight='81'
uci set mwan3.cellular6_m1.weight='80'
uci set mwan3.cellular7_m1.weight='79'
uci set mwan3.cellular8_m1.weight='78'
uci commit mwan3
```

### Step 2: Enable Hybrid System
```bash
# Enable hybrid weight system
uci set autonomy.main.respect_user_weights='1'
uci set autonomy.main.dynamic_adjustment='1'
uci set autonomy.main.emergency_override='1'
uci commit autonomy
```

### Step 3: Restart autonomy
```bash
/etc/init.d/autonomy restart
```

### Step 4: Verify Operation
```bash
# Check that your weights are loaded
autonomyctl status weights

# Monitor for a few minutes
autonomyctl logs --follow
```

## 🎯 Conclusion

The Hybrid Weight System represents a fundamental shift from **fighting against MWAN3** to **working with MWAN3**. It gives you:

1. **Full control** over your network priorities
2. **Intelligent monitoring** and adjustments when needed  
3. **Automatic restoration** to your preferences
4. **Complete configurability** via UCI
5. **Clear separation** between user config and system intelligence

This is the **best of both worlds**: your control + system intelligence.

---

**Ready to try it?** Run the test:
```bash
go run . -test-hybrid-weights
```
