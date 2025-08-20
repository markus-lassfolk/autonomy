# CRITICAL BUG FIX: Runaway Metric Increases in Unified Script

## 🐛 **The Problem**

The `starlink_monitor_unified-rutos.sh` script had **faulty incremental failover logic** that caused runaway metric increases:

### **Before (BROKEN):**
```bash
# Keeps adding 10 each failover: 1→11→21→31→...→311
new_metric=$((current_metric + 10))
```

### **After (FIXED):**
```bash
# Always sets to configured METRIC_BAD value (e.g., 20)
new_metric="${METRIC_BAD:-20}"
```

## ⚠️ **Impact**

- **User's Starlink metric**: 311 (extremely high priority = lowest preference)
- **Cause**: Multiple failover cycles incrementally increased metric by 10 each time
- **Result**: Starlink completely deprioritized, all traffic on cellular

## 🔧 **Fixes Applied**

### **1. Fixed Failover Logic**
- Changed incremental increase to fixed `METRIC_BAD` value
- Prevents runaway metric increases from repeated failovers

### **2. Fixed Restore Logic**  
- Use configured `METRIC_GOOD` instead of hardcoded `10`
- Use configured `METRIC_GOOD` for comparison instead of hardcoded `10`

### **3. Fixed Function Name**
- Changed `restore_starlink()` call to `restore_primary()` (correct function name)

## 🚨 **Immediate Actions Required**

### **1. Reset Starlink Metric (URGENT)**
```bash
# Reset to good priority immediately
uci set mwan3.starlink.metric=1
uci commit mwan3
/etc/init.d/mwan3 reload
```

### **2. Update Script (IMPORTANT)**
```bash
# Use the fixed unified script or switch to starlink_monitor-rutos.sh
# The standard starlink_monitor-rutos.sh doesn't have this bug
```

### **3. Check Configuration**
```bash
# Verify your configuration has proper values:
export METRIC_GOOD="1"    # Lowest number = highest priority
export METRIC_BAD="20"    # Higher number = lower priority (failover state)
```

## 📊 **Why This Happened**

1. **Different Logic**: The unified script used different failover logic than other scripts
2. **No Bounds Checking**: No maximum limit on metric increases
3. **Incremental Design**: Intended to handle multiple failover levels but caused runaway increases

## ✅ **Prevention Measures**

### **Fixed Code Now Includes:**
- **Bounded metric values** using configuration variables
- **Consistent failover logic** across all scripts  
- **Proper function naming** and calls
- **Configuration-driven values** instead of hardcoded numbers

## 🎯 **Recommended Actions**

1. **Immediate**: Reset Starlink metric to 1
2. **Short-term**: Use fixed unified script or switch to `starlink_monitor-rutos.sh`
3. **Long-term**: Monitor metric values to ensure they stay within expected ranges (1-20)

## 📋 **Monitoring Command**
```bash
# Check current metrics regularly:
uci show mwan3 | grep metric
```

Expected output:
```
mwan3.starlink.metric='1'     # Good (primary)
mwan3.sim_telia.metric='2'    # Backup 1  
mwan3.sim_roaming.metric='4'  # Backup 2
```

---

**The bug has been fixed in the repository. Users with high Starlink metrics should reset them immediately and update their scripts.**
