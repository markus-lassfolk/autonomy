# Starlink Monitor Troubleshooting Guide

## Current Issues and Solutions

Based on your debug output, I've identified several key issues that need to be addressed:

### 1. UCI Configuration Error

**Problem**: The monitoring script is using hardcoded `mwan3.starlink.metric` instead of the configured `MWAN_MEMBER` variable.

**Error Message**:
```
[DEBUG] Command: uci set mwan3.starlink.metric=20
uci: Invalid argument
[ERROR] Command failed: Set mwan3 metric to 20 (exit code: 1)
```

**Root Cause**: The script has hardcoded UCI commands that don't match your MWAN3 configuration.

**Solution**:
```bash
# 1. Diagnose the issue
./scripts/fix-mwan-configuration-rutos.sh diagnose

# 2. Fix the monitor script
./scripts/fix-mwan-configuration-rutos.sh fix-script

# 3. Test the fix
./scripts/fix-mwan-configuration-rutos.sh test
```

### 4. Missing Unified Logging

**Problem**: No centralized logging for monitoring activities - difficult to track what's happening across all scripts.

**Current State**: 
- Individual scripts may log to different locations
- No central log file for all Starlink monitoring
- Syslog integration not working due to missing LOG_TAG
- Cron jobs run silently with no activity tracking

**Solution**: Set up unified logging system:

```bash
# Set up unified logging (part of comprehensive fix)
./scripts/fix-configuration-variables-rutos.sh setup-logging

# Test unified logging
./scripts/fix-configuration-variables-rutos.sh test-logging
```

**After setup, you'll have**:
- **Central log file**: `/etc/starlink-logs/starlink_monitor.log`
- **Syslog integration**: All entries tagged with "StarlinkMonitor" 
- **Log rotation**: Automatic cleanup and compression
- **All scripts log to same place**: Monitor, logger, maintenance scripts

### 5. Missing Cron Job Logging

**Current State**: Cron jobs run silently with no logging or status tracking.

**Solution**:
```bash
# 1. Set up enhanced cron monitoring
./scripts/setup-enhanced-cron-monitoring-rutos.sh setup

# 2. Check cron status
./scripts/enhanced-cron-logging-rutos.sh status

# 3. Run health check
./scripts/enhanced-cron-logging-rutos.sh health
```

### 3. Configuration Variable Name Mismatches

**Problem**: Variable name mismatches between config file and monitoring script:
- Config has `PUSHOVER_ENABLED="1"` but script expects `ENABLE_PUSHOVER`
- Missing feature flag variables showing as "UNSET"
- `LOG_TAG` not set, preventing syslog integration

**Current Config Variables vs Expected**:
```bash
# Your config has:
export PUSHOVER_ENABLED="1"              # Script expects: ENABLE_PUSHOVER
export PUSHOVER_TOKEN="your_token"       # ✓ Correct
export PUSHOVER_USER="your_user"         # ✓ Correct

# Missing variables causing "UNSET" warnings:
# ENABLE_GPS_TRACKING: UNSET
# ENABLE_CELLULAR_TRACKING: UNSET  
# ENABLE_ENHANCED_FAILOVER: UNSET
# LOG_TAG: UNSET
```

**Impact**: 
- Pushover notifications may not work despite being configured
- Features show as disabled even when they should work
- No syslog integration due to missing LOG_TAG

**Solution**: Use the automated fix tool:

```bash
# 1. Analyze configuration issues
./scripts/fix-configuration-variables-rutos.sh analyze

# 2. Run comprehensive fix (recommended)
./scripts/fix-configuration-variables-rutos.sh fix

# 3. Test the fixes
./scripts/fix-configuration-variables-rutos.sh test
```

**What the fix does**:
- Maps `PUSHOVER_ENABLED` to `ENABLE_PUSHOVER` automatically
- Adds missing feature flag variables with proper defaults
- Sets up `LOG_TAG="StarlinkMonitor"` for syslog integration
- Creates unified logging system for all scripts

## Enhanced Monitoring Features

### 1. Comprehensive Cron Logging

The new enhanced logging system provides:

- **Individual Log Files**: Each cron execution gets its own timestamped log file
- **Status Tracking**: Success/failure tracking with detailed statistics
- **Health Monitoring**: Automatic detection of repeated failures
- **Log Management**: Automatic cleanup and rotation

**Usage**:
```bash
# Check current cron status
enhanced-cron-logging-rutos.sh status

# View recent job executions
enhanced-cron-logging-rutos.sh health

# Manual cleanup of old logs
enhanced-cron-logging-rutos.sh cleanup
```

### 2. Error Detection and Alerting

The system now provides:

- **Real-time Error Detection**: Immediate notification of script failures
- **Failure Pattern Analysis**: Detection of recurring issues
- **System Integration**: Integration with system logging (syslog)
- **Health Thresholds**: Configurable alerting thresholds

### 3. Log File Locations

After setting up enhanced monitoring:

```bash
# Cron execution logs
/etc/starlink-logs/cron/starlink_monitor_YYYYMMDD_HHMMSS.log
/etc/starlink-logs/cron/starlink_logger_YYYYMMDD_HHMMSS.log

# Status tracking
/etc/starlink-logs/cron/job_status.log

# Health reports
/etc/starlink-logs/cron/health_report_YYYYMMDD_HHMMSS.txt

# System logs
/var/log/messages (search for "starlink-*")
```

## Immediate Action Plan

### Step 1: Fix Configuration Variable Mismatches

```bash
# Navigate to the scripts directory
cd /usr/local/starlink-monitor/scripts

# Analyze configuration issues
./fix-configuration-variables-rutos.sh analyze

# Run comprehensive fix (adds missing variables and unified logging)
./fix-configuration-variables-rutos.sh fix

# Test the configuration fixes
./fix-configuration-variables-rutos.sh test
```

### Step 2: Fix UCI Configuration Issue

```bash
# Run UCI diagnosis
./fix-mwan-configuration-rutos.sh diagnose

# If issues found, fix the monitor script
./fix-mwan-configuration-rutos.sh fix-script

# Test the fix
DEBUG=1 CONFIG_FILE=/etc/starlink-config/config.sh /usr/local/starlink-monitor/Starlink-RUTOS-Failover/starlink_monitor_unified-rutos.sh
```

### Step 3: Set Up Enhanced Cron Monitoring

```bash
# Install enhanced cron monitoring
./setup-enhanced-cron-monitoring-rutos.sh setup

# Check status
./enhanced-cron-logging-rutos.sh status

# Verify cron jobs are working
tail -f /etc/starlink-logs/cron/job_status.log
```

### Step 4: Verify All Fixes

```bash
# Test the monitoring script manually (should show no more "UNSET" variables)
DEBUG=1 CONFIG_FILE=/etc/starlink-config/config.sh /usr/local/starlink-monitor/Starlink-RUTOS-Failover/starlink_monitor_unified-rutos.sh

# Check for UCI errors (should be gone)
# Check that configuration variables are no longer "UNSET"
# Verify Pushover mapping works: PUSHOVER_ENABLED=1 should show ENABLE_PUSHOVER=true

# Check unified logging is working
tail -5 /etc/starlink-logs/starlink_monitor.log

# Check syslog integration
grep "StarlinkMonitor" /var/log/messages

# Wait a few minutes and check cron logs
ls -la /etc/starlink-logs/cron/
cat /etc/starlink-logs/cron/job_status.log
```

## Ongoing Monitoring

### Daily Commands

```bash
# Check cron job health
enhanced-cron-logging-rutos.sh status

# View recent execution logs
tail -5 /etc/starlink-logs/cron/job_status.log

# Check for system alerts
grep "starlink-" /var/log/messages
```

### Weekly Commands

```bash
# Run comprehensive health check
enhanced-cron-logging-rutos.sh health

# Review log file usage
du -sh /etc/starlink-logs/

# Check for configuration issues
fix-mwan-configuration-rutos.sh diagnose
```

## Common Issues and Solutions

### Issue: "uci: Invalid argument"
**Cause**: Hardcoded UCI commands not matching your MWAN3 configuration  
**Solution**: Run `fix-mwan-configuration-rutos.sh fix-script`

### Issue: "UNSET" configuration variables
**Cause**: Missing variable definitions in config file  
**Solution**: Add explicit variable definitions to config.sh

### Issue: No cron job visibility
**Cause**: No logging or status tracking  
**Solution**: Install enhanced cron monitoring system

### Issue: High obstruction warnings
**Cause**: Physical obstructions or very sensitive threshold  
**Solution**: Check dish positioning or adjust `OBSTRUCTION_THRESHOLD`

### Issue: Frequent failovers
**Cause**: Thresholds too sensitive for your environment  
**Solution**: Adjust `LATENCY_THRESHOLD`, `PACKET_LOSS_THRESHOLD` values

## Configuration Optimization

### For Stable Connections
```bash
export LATENCY_THRESHOLD="150"        # More tolerant of latency spikes
export PACKET_LOSS_THRESHOLD="8"      # Allow higher packet loss
export OBSTRUCTION_THRESHOLD="0.005"  # Less sensitive to minor obstructions
```

### For Sensitive Applications
```bash
export LATENCY_THRESHOLD="80"         # Strict latency requirements
export PACKET_LOSS_THRESHOLD="3"      # Low packet loss tolerance
export OBSTRUCTION_THRESHOLD="0.001"  # Very sensitive to obstructions
```

## Getting Help

If issues persist after following this guide:

1. **Collect Debug Information**:
   ```bash
   # Run diagnosis
   fix-mwan-configuration-rutos.sh diagnose > debug_info.txt
   enhanced-cron-logging-rutos.sh health >> debug_info.txt
   
   # Show recent logs
   tail -20 /etc/starlink-logs/cron/job_status.log >> debug_info.txt
   ```

2. **Check System Status**:
   ```bash
   # Show MWAN3 configuration
   uci show mwan3 >> debug_info.txt
   
   # Show current cron jobs
   crontab -l >> debug_info.txt
   ```

3. **Review the debug_info.txt file** for comprehensive system state information.

This enhanced monitoring and troubleshooting system should resolve your current issues and provide much better visibility into the Starlink monitoring system's operation.
