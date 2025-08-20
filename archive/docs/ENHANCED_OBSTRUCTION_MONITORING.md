# Enhanced Obstruction Monitoring for Starlink Failover

## Overview

The enhanced obstruction monitoring system uses multiple metrics from Starlink's API to make intelligent failover decisions, reducing false positives while maintaining reliable detection of actual connectivity issues.

## Understanding Obstruction Metrics

### Available Starlink Obstruction Data

Your Starlink provides these obstruction statistics:

```json
"obstructionStats": {
   "fractionObstructed": 0.004166088,           // Current sky coverage obstruction (0.42%)
   "validS": 53349,                             // Seconds of valid measurement data (~14.8 hours)
   "avgProlongedObstructionIntervalS": "NaN",   // Average duration of long obstructions
   "timeObstructed": 0.00037113513,             // Fraction of time actually obstructed (0.000037%)
   "patchesValid": 7201                         // Number of valid sky coverage measurements
}
```

### What Each Metric Means

1. **fractionObstructed (Current Obstruction)**
   - **What**: Percentage of sky coverage currently blocked
   - **Your value**: 0.42% (very good)
   - **Range**: 0-100%, lower is better
   - **Usage**: Primary trigger for obstruction detection

2. **timeObstructed (Historical Obstruction)**
   - **What**: Actual percentage of time the dish was obstructed over the measurement period
   - **Your value**: 0.000037% (excellent - virtually no actual obstruction time)
   - **Range**: 0-100%, lower is better
   - **Usage**: Validates whether current reading represents real impact

3. **validS (Data Age)**
   - **What**: How long Starlink has been collecting this obstruction data
   - **Your value**: 53,349 seconds (14.8 hours)
   - **Range**: 0-∞ seconds, more is better for accuracy
   - **Usage**: Determines reliability of historical analysis

4. **avgProlongedObstructionIntervalS (Obstruction Duration)**
   - **What**: Average length of significant obstruction events
   - **Your value**: "NaN" (no prolonged obstructions detected)
   - **Range**: 0-∞ seconds, shorter/none is better
   - **Usage**: Identifies whether obstructions cause service disruption

5. **patchesValid (Measurement Quality)**
   - **What**: Number of valid sky coverage measurement points
   - **Your value**: 7,201 (good data quality)
   - **Range**: 0-∞, higher is better for accuracy
   - **Usage**: Validates measurement reliability

## Enhanced Analysis Logic

### Traditional vs. Enhanced Monitoring

**Traditional (Simple) Approach:**
- If `fractionObstructed > threshold` → trigger failover
- Problem: Causes false positives for temporary/harmless obstructions

**Enhanced (Intelligent) Approach:**
- Analyzes multiple factors before deciding
- Considers historical impact, not just current reading
- Reduces false positives while maintaining sensitivity

### Decision Matrix

The enhanced system considers these factors:

| Factor | Threshold | Your Status | Impact |
|--------|-----------|-------------|--------|
| Current Obstruction | 3.0% | 0.42% ✅ | Below threshold |
| Historical Time Obstructed | 1.0% | 0.000037% ✅ | Virtually no impact |
| Prolonged Obstruction Duration | 30s | None ✅ | No service disruption |
| Data Age | 1 hour | 14.8 hours ✅ | Reliable data |
| Measurement Quality | 1000 patches | 7,201 ✅ | High quality |

**Result for your connection**: No failover needed - obstruction is temporary/harmless

## Configuration Options

### Basic Settings

```bash
# Simple threshold (percentage as decimal)
export OBSTRUCTION_THRESHOLD="0.03"  # 3%

# Enable intelligent analysis
export ENABLE_INTELLIGENT_OBSTRUCTION="true"
```

### Advanced Settings

```bash
# Minimum data age for reliable analysis
export OBSTRUCTION_MIN_DATA_HOURS="1"  # Require 1+ hours of data

# Historical impact threshold
export OBSTRUCTION_HISTORICAL_THRESHOLD="1.0"  # 1% time obstructed

# Prolonged obstruction threshold  
export OBSTRUCTION_PROLONGED_THRESHOLD="30"  # 30+ second average duration
```

## Real-World Examples

### Your Connection (Excellent)
```
Current: 0.42% (below 3% threshold)
Historical: 0.000037% time obstructed (below 1% threshold)
Prolonged: None detected
Assessment: No failover - temporary/harmless obstruction
```

### Problematic Connection Example
```
Current: 4.5% (above 3% threshold)
Historical: 2.3% time obstructed (above 1% threshold)
Prolonged: 45s average duration (above 30s threshold)
Assessment: Failover recommended - actual service impact
```

### False Positive Example (Avoided)
```
Current: 5.2% (above 3% threshold)
Historical: 0.1% time obstructed (below 1% threshold)
Prolonged: None detected
Assessment: No failover - likely temporary obstruction (cloud, bird, etc.)
```

## Benefits

### Reduced False Positives
- **Traditional**: Would fail over on your 0.42% reading if threshold was 0.1%
- **Enhanced**: Recognizes 0.000037% historical impact = no real problem

### Maintained Sensitivity
- Still triggers on genuinely problematic connections
- Emergency threshold (3x normal) catches severe obstructions immediately
- Prolonged obstruction detection catches service-disrupting events

### Better Data Quality
- Validates measurement reliability before making decisions
- Requires sufficient data history for intelligent analysis
- Falls back to simple mode when data is insufficient

## Monitoring Output

### Normal Operation
```
[INFO] Obstruction detected but within acceptable parameters
[INFO]   Current: 0.42% (threshold: 3.0%)
[INFO]   Historical: 0.000037% over 14.8h (threshold: 1.0%)
[INFO]   Assessment: Temporary/acceptable obstruction - no failover needed
```

### Failover Triggered
```
[WARN] Intelligent obstruction analysis: FAILOVER RECOMMENDED
[WARN]   Current: 4.5% > 3.0%
[WARN]   Historical: 2.3% (threshold: 1.0%)
[WARN]   Prolonged avg: 45s (threshold: 30s)
[WARN]   Data period: 6.2h, patches: 4521
```

## Troubleshooting

### If You Want More Sensitivity
```bash
# Lower the current obstruction threshold
export OBSTRUCTION_THRESHOLD="0.02"  # 2% instead of 3%

# Lower the historical threshold
export OBSTRUCTION_HISTORICAL_THRESHOLD="0.5"  # 0.5% instead of 1%
```

### If You Get Too Many False Positives
```bash
# Raise the current obstruction threshold
export OBSTRUCTION_THRESHOLD="0.05"  # 5% instead of 3%

# Require more historical impact
export OBSTRUCTION_HISTORICAL_THRESHOLD="2.0"  # 2% instead of 1%
```

### Disable Enhanced Analysis
```bash
# Fall back to simple threshold checking
export ENABLE_INTELLIGENT_OBSTRUCTION="false"
```

## Technical Implementation

The enhanced monitoring extracts these additional fields from Starlink API:

```bash
obstruction_time_pct=$(echo "$status_data" | jq -r '.dishGetStatus.obstructionStats.timeObstructed')
obstruction_valid_s=$(echo "$status_data" | jq -r '.dishGetStatus.obstructionStats.validS')  
obstruction_avg_prolonged=$(echo "$status_data" | jq -r '.dishGetStatus.obstructionStats.avgProlongedObstructionIntervalS')
obstruction_patches_valid=$(echo "$status_data" | jq -r '.dishGetStatus.obstructionStats.patchesValid')
```

These metrics enable intelligent analysis that considers:
- Whether obstructions actually impact service
- How long obstructions typically last
- Quality and age of the measurement data
- Historical patterns vs. current snapshots

This results in more reliable failover decisions that better reflect actual connectivity quality.
