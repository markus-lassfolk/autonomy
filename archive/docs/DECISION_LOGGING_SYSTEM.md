# Starlink Decision Logging System

## Overview

The enhanced Starlink monitoring script now includes a comprehensive decision logging system that tracks **every evaluation, decision, and action** the monitoring script makes. This provides complete visibility into:

- **Why** failovers were triggered
- **What** metrics led to each decision  
- **When** actions were taken and their results
- **How** the system is performing over time

## Decision Log Format

The decision log is stored as a CSV file with the following columns:

| Column | Description | Example Values |
|--------|-------------|----------------|
| `timestamp` | When the decision was made | `2025-01-15 14:30:25` |
| `decision_type` | Type of decision | `evaluation`, `soft_failover`, `hard_failover`, `restore`, `maintenance` |
| `trigger_reason` | Why the decision was made | `quality_degraded`, `multiple_critical_issues`, `quality_improved` |
| `quality_factors` | Summary of failing metrics | `lat:1,loss:0,obs:1,snr:0` |
| `latency_ms` | Current latency in milliseconds | `185` |
| `packet_loss_pct` | Current packet loss percentage | `3.2` |
| `obstruction_pct` | Current obstruction percentage | `0.08` |
| `snr_db` | Signal-to-noise ratio | `12.5` |
| `current_metric` | MWAN3 metric before action | `1` |
| `new_metric` | MWAN3 metric after action | `20` |
| `action_taken` | What action was performed | `metric_increase`, `metric_restore`, `no_action` |
| `action_result` | Result of the action | `success`, `failed`, `completed` |
| `gps_context` | GPS information (if enabled) | `active:45.123,-93.456` |
| `cellular_context` | Cellular information (if enabled) | `signal:-85dbm` |
| `additional_notes` | Extra context and details | `Quality factors: 2, Previous metric: 1` |

## Decision Types

### 1. **Evaluation** (`evaluation`)
- **Purpose**: Records quality assessments that don't trigger actions
- **When**: Every monitoring cycle when connection quality is checked
- **Examples**: 
  - `quality_good` - All metrics within thresholds
  - `single_issue_weak_cellular` - One issue but cellular backup isn't strong enough
  - `obstruction_detected_acceptable` - Obstruction present but within acceptable limits

### 2. **Soft Failover** (`soft_failover`)
- **Purpose**: Records standard failover due to quality issues
- **Triggers**: Single quality metric exceeds threshold
- **Examples**:
  - `single_quality_issue` - High latency OR packet loss OR obstruction
  - `dual_quality_issues` - Two metrics failing simultaneously

### 3. **Hard Failover** (`hard_failover`)
- **Purpose**: Records emergency failover due to critical issues
- **Triggers**: Multiple critical quality metrics failing
- **Examples**:
  - `multiple_critical_issues` - 3+ quality factors failing
  - `emergency_obstruction` - Obstruction 3x normal threshold

### 4. **Restore** (`restore`)
- **Purpose**: Records restoration of primary connection
- **Triggers**: Quality improves and metric is elevated
- **Examples**:
  - `quality_improved` - Metrics back within acceptable ranges
  - `metric_elevated` - Quality unknown but metric needs reset

### 5. **Maintenance** (`maintenance`)
- **Purpose**: Records system operations and status checks
- **Examples**:
  - `monitoring_cycle_start` - Beginning of monitoring cycle
  - `api_communication` - Starlink API status
  - `failover_execution` - Actual failover command execution

## Configuration

### Enable Decision Logging

In your configuration file (`config.unified.template.sh`):

```bash
# Enable comprehensive decision logging
export ENABLE_ENHANCED_FAILOVER_LOGGING="true"

# Log directory (decision log will be: ${LOG_DIR}/failover_decisions.csv)
export LOG_DIR="/etc/starlink-logs"
```

### Log File Location

The decision log is automatically created at:
```
${LOG_DIR}/failover_decisions.csv
```

Default location: `/etc/starlink-logs/failover_decisions.csv`

## Analysis Tools

### 1. Decision Log Analyzer

Provides comprehensive analysis of decision patterns:

```bash
# Analyze last 24 hours
./scripts/analyze-decision-log-rutos.sh

# Analyze last 12 hours with custom log file
./scripts/analyze-decision-log-rutos.sh /tmp/decisions.csv 12

# Show all decisions from last 48 hours
./scripts/analyze-decision-log-rutos.sh /etc/starlink-logs/failover_decisions.csv 48 true
```

**Output includes:**
- Decision type distribution
- Action success/failure rates  
- Trigger reason analysis
- Quality metrics trends
- Recent metric changes
- Monitoring recommendations

### 2. Real-Time Decision Viewer

Watch decisions as they happen:

```bash
# Watch with 5 second refresh
./scripts/watch-decisions-rutos.sh

# Watch with 2 second refresh
./scripts/watch-decisions-rutos.sh /etc/starlink-logs/failover_decisions.csv 2
```

**Shows:**
- Last 15 decisions with color coding
- Current MWAN3 metric status
- Decision statistics
- Real-time updates

## Understanding Decision Logic

### Quality Factor Analysis

The system tracks these quality factors:
- **lat:1** - Latency exceeds threshold
- **loss:1** - Packet loss exceeds threshold  
- **obs:1** - Obstruction exceeds threshold
- **snr:1** - Signal-to-noise ratio below threshold

### Decision Logic Examples

```bash
# Example 1: Single issue, no failover
decision_type: evaluation
trigger_reason: single_issue_weak_cellular
quality_factors: lat:1,loss:0,obs:0,snr:0
action_taken: no_action

# Example 2: Multiple issues, failover triggered
decision_type: soft_failover  
trigger_reason: dual_quality_issues
quality_factors: lat:1,loss:1,obs:0,snr:0
action_taken: metric_increase
action_result: success

# Example 3: Quality restored
decision_type: restore
trigger_reason: quality_improved
action_taken: metric_restore
action_result: success
```

## Troubleshooting with Decision Logs

### High Failover Frequency

If you see frequent failovers:

1. **Check trigger reasons**: Are thresholds too sensitive?
2. **Analyze quality trends**: Is one metric consistently problematic?
3. **Review restore patterns**: Are restores happening appropriately?

```bash
# Look for patterns
./scripts/analyze-decision-log-rutos.sh | grep -A 5 "TRIGGER REASON"
```

### Failed Actions

If you see failed actions:

1. **Check system permissions**: Can the script modify MWAN3 configuration?
2. **Verify MWAN3 setup**: Are interface names correct?
3. **Review error messages**: Check additional_notes field

```bash
# Find failed actions
grep "failed" /etc/starlink-logs/failover_decisions.csv
```

### Understanding Quality Issues

To understand why failovers occur:

1. **Check quality metrics trends**: What values trigger decisions?
2. **Compare with thresholds**: Are thresholds appropriate for your connection?
3. **Look at decision context**: GPS/cellular factors

```bash
# See quality trends
./scripts/analyze-decision-log-rutos.sh | grep -A 10 "QUALITY METRICS"
```

## Log Maintenance

### Automatic Cleanup

The decision log grows over time. Consider periodic cleanup:

```bash
# Keep last 30 days only
find /etc/starlink-logs -name "failover_decisions.csv" -exec head -1 {} \; > /tmp/header.csv
tail -n +2 /etc/starlink-logs/failover_decisions.csv | awk -v cutoff="$(date -d '30 days ago' '+%Y-%m-%d')" '$1 >= cutoff' >> /tmp/header.csv
mv /tmp/header.csv /etc/starlink-logs/failover_decisions.csv
```

### Log Rotation

Add to your log rotation script:

```bash
# Rotate decision log weekly
if [ -f "/etc/starlink-logs/failover_decisions.csv" ]; then
    mv "/etc/starlink-logs/failover_decisions.csv" "/etc/starlink-logs/failover_decisions.csv.$(date +%Y%m%d)"
    echo "timestamp,decision_type,trigger_reason,quality_factors,latency_ms,packet_loss_pct,obstruction_pct,snr_db,current_metric,new_metric,action_taken,action_result,gps_context,cellular_context,additional_notes" > "/etc/starlink-logs/failover_decisions.csv"
fi
```

## Benefits

### For Troubleshooting
- **Root cause analysis**: Understand exactly why failovers occur
- **Performance optimization**: Identify patterns and adjust thresholds
- **System validation**: Verify monitoring logic is working correctly

### For Monitoring
- **Historical analysis**: Track system behavior over time
- **Trend identification**: Spot recurring issues or improvements
- **Decision auditing**: Complete audit trail of all system decisions

### For Optimization
- **Threshold tuning**: Use actual data to optimize sensitivity
- **Pattern recognition**: Identify environmental or usage patterns
- **System refinement**: Improve decision logic based on real-world data

## Example Analysis Workflow

1. **Daily Review**: Check decision summary for patterns
   ```bash
   ./scripts/analyze-decision-log-rutos.sh | head -20
   ```

2. **Weekly Deep Dive**: Analyze quality trends and patterns  
   ```bash
   ./scripts/analyze-decision-log-rutos.sh /etc/starlink-logs/failover_decisions.csv 168 true
   ```

3. **Issue Investigation**: When problems occur, check decision context
   ```bash
   grep -A 5 -B 5 "failed\|hard_failover" /etc/starlink-logs/failover_decisions.csv
   ```

4. **Performance Tuning**: Use metrics trends to adjust thresholds
   ```bash
   ./scripts/analyze-decision-log-rutos.sh | grep -A 10 "QUALITY METRICS TRENDS"
   ```

The decision logging system transforms the monitoring script from a "black box" into a fully transparent system where every decision is documented, analyzed, and available for optimization.
