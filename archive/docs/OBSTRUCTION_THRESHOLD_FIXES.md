# Configuration Fixes - Obstruction Threshold and MWAN3 UCI Path

## Issues Identified

### 1. Obstruction Threshold Too Sensitive
**Problem:** Current threshold of 0.001% (0.1%) causing false positives
- User has 0 outages in 12 hours with good obstruction map
- Current obstruction: 0.416% triggering unnecessary failovers
- Threshold was too conservative for real-world conditions

**Solution:** Updated to 3% (0.03) threshold
- Based on field experience and Starlink's actual tolerance
- 0-1% excellent, 1-3% good, 3-5% acceptable, 5%+ poor
- Reduces false positives while maintaining protection against real issues

### 2. MWAN3 UCI Path Hardcoded
**Problem:** Script using hardcoded `mwan3.starlink.metric` path
- Error: `uci: Invalid argument` when trying to set metric
- Should use configured `MWAN_MEMBER` variable instead

**Solution:** Updated to use dynamic member configuration
- Changed from `mwan3.starlink.metric` to `mwan3.${MWAN_MEMBER}.metric`
- Now respects user's MWAN3 configuration in config file
- Works with any member name (member1, member2, starlink, etc.)

## Files Modified

### config/config.unified.template.sh
```bash
# OLD - Too sensitive
export OBSTRUCTION_THRESHOLD="0.001"  # 0.1%

# NEW - Realistic threshold
export OBSTRUCTION_THRESHOLD="0.03"   # 3%
```

### Starlink-RUTOS-Failover/starlink_monitor_unified-rutos.sh
```bash
# OLD - Hardcoded path
current_metric=$(uci get mwan3.starlink.metric 2>/dev/null || echo "10")
uci set mwan3.starlink.metric=$new_metric

# NEW - Dynamic member path
current_metric=$(uci get "mwan3.${MWAN_MEMBER}.metric" 2>/dev/null || echo "10")
uci set mwan3.${MWAN_MEMBER}.metric=$new_metric
```

## Expected Results

### Obstruction Threshold Fix
- **No more false positives** from normal obstruction levels (0.1-0.5%)
- **Still triggers** on significant obstructions (3%+) that actually impact connectivity
- **Better user experience** - failover only when really needed

### MWAN3 UCI Fix
- **UCI commands will succeed** instead of failing with "Invalid argument"
- **Failover will work** properly when quality thresholds are exceeded
- **Configuration flexibility** - works with any MWAN member name

## Testing Recommendation

1. **Update configuration** by re-running install script or manually updating config
2. **Test with current conditions**: With your 0.416% obstruction, monitor should now report good quality
3. **Verify MWAN3 paths**: Check your actual MWAN3 config with `uci show mwan3 | grep member`
4. **Monitor behavior**: Should see fewer false failovers but still respond to real issues

## Obstruction Threshold Guidelines

| Threshold | Percentage | Use Case |
|-----------|------------|----------|
| 0.01 | 1% | Very sensitive - critical applications |
| 0.02 | 2% | Sensitive - good balance |
| **0.03** | **3%** | **Recommended - realistic default** |
| 0.04 | 4% | Moderate - reduces false positives |
| 0.05 | 5% | Conservative - only major obstructions |

Based on your field experience (0 outages with 0.416% obstruction), the new 3% threshold should work much better while still providing protection against real connectivity issues.
