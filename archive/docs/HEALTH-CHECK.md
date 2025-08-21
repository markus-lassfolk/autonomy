# Health Check System Documentation

This document describes the comprehensive health monitoring system for the RUTOS Starlink Failover solution.

## Overview

The health check system provides automated monitoring and diagnostic capabilities for the Starlink failover solution
running on RUTX50 routers. It orchestrates all individual test scripts and provides a unified view of system health.

## Main Health Check Script

**Location**: `scripts/health-check.sh`

The main health check script provides comprehensive system monitoring with multiple operational modes.

### Quick Start

```bash
# Run comprehensive health check
/root/starlink-monitor/scripts/health-check.sh

# Quick health check (skip time-consuming tests)
/root/starlink-monitor/scripts/health-check.sh --quick

# Debug mode for troubleshooting
DEBUG=1 /root/starlink-monitor/scripts/health-check.sh
```

### Health Check Features

#### 1. System Resource Monitoring

- **Disk Space**: Monitors available disk space and warns if low
- **Memory Usage**: Checks RAM utilization and swap usage
- **CPU Load**: Monitors system load averages
- **System Uptime**: Tracks system uptime and load statistics

#### 2. Network Connectivity Testing

- **Internet Connectivity**: Tests connection to external servers (8.8.8.8)
- **DNS Resolution**: Verifies DNS functionality (google.com)
- **Starlink Device**: Tests connectivity to Starlink dish (192.168.100.1)
- **gRPC API**: Validates Starlink gRPC API accessibility

#### 3. Configuration Health

- **Configuration Validation**: Runs complete config validation
- **Placeholder Detection**: Identifies unconfigured optional features
- **Critical Settings**: Ensures essential configuration is present
- **Template Compatibility**: Checks for outdated configuration templates

#### 4. Monitoring System Health

- **Log Freshness**: Checks if monitoring logs are recent
- **Process Status**: Verifies monitoring processes are running
- **Cron Schedule**: Validates monitoring is scheduled in crontab
- **State Files**: Checks for proper state file management

#### 5. Integrated Test Execution

- **Pushover Testing**: Tests notification system (if configured)
- **Monitoring Tests**: Validates core monitoring functionality
- **System Status**: Comprehensive system overview
- **Connectivity Tests**: Network and device connectivity validation

## Health Check Modes

### 1. Full Health Check (Default)

```bash
/root/starlink-monitor/scripts/health-check.sh
```

- Runs all health checks
- Executes all integrated tests
- Provides comprehensive system overview
- Most thorough but takes longest time

### 2. Quick Health Check

```bash
/root/starlink-monitor/scripts/health-check.sh --quick
```

- Skips time-consuming integrated tests
- Focuses on essential system checks
- Faster execution for routine monitoring
- Good for automated scheduling

### 3. Component-Specific Checks

```bash
# Test only network connectivity
/root/starlink-monitor/scripts/health-check.sh --connectivity

# Test only monitoring system
/root/starlink-monitor/scripts/health-check.sh --monitoring

# Test only configuration
/root/starlink-monitor/scripts/health-check.sh --config

# Test only system resources
/root/starlink-monitor/scripts/health-check.sh --resources
```

## Health Status Indicators

### Status Levels

- **✅ HEALTHY**: Component is functioning normally
- **⚠️ WARNING**: Non-critical issues that should be addressed
- **❌ CRITICAL**: Critical issues requiring immediate attention
- **❓ UNKNOWN**: Unable to determine component status

### Health Scoring

The system provides an overall health score based on:

- **Critical Issues**: Heavily impact score (major deduction)
- **Warnings**: Moderately impact score (minor deduction)
- **Healthy Components**: Contribute positively to score
- **Unknown Status**: Minor negative impact

## Sample Health Check Output

```text
=== RUTOS STARLINK FAILOVER HEALTH CHECK ===
Health Check Version: 1.0.2
Timestamp: 2025-07-15 14:30:15

=== SYSTEM RESOURCE CHECKS ===
✅ HEALTHY      | Disk Space              | 15.2GB available (85% free)
✅ HEALTHY      | Memory Usage            | 512MB/1GB used (51% used)
✅ HEALTHY      | System Load             | 0.15 (low load)
✅ HEALTHY      | System Uptime           | up 5 days, 12:30

=== NETWORK CONNECTIVITY CHECKS ===
✅ HEALTHY      | Internet Connectivity   | Can reach 8.8.8.8
✅ HEALTHY      | DNS Resolution         | Can resolve google.com
✅ HEALTHY      | Starlink Device        | Can reach 192.168.100.1
⚠️  WARNING     | gRPC API               | gRPC tools not available

=== CONFIGURATION HEALTH CHECKS ===
✅ HEALTHY      | Configuration          | Configuration validation passed
⚠️  WARNING     | Pushover Config        | Pushover using placeholder values
✅ HEALTHY      | Template Version       | Using current template version

=== MONITORING SYSTEM CHECKS ===
✅ HEALTHY      | Log Freshness          | Logs updated 5 minutes ago
✅ HEALTHY      | Monitor Script         | Script exists and is executable
✅ HEALTHY      | Cron Schedule          | Monitoring scheduled in crontab
✅ HEALTHY      | State Files            | State directory accessible

=== INTEGRATED TEST RESULTS ===
⚠️  WARNING     | Pushover Test          | Skipped (placeholder values)
✅ HEALTHY      | Monitoring Test        | All connectivity tests passed
✅ HEALTHY      | System Status          | System status retrieved successfully

=== HEALTH CHECK SUMMARY ===
✅ HEALTHY:     12 checks
⚠️  WARNING:     3 checks
❌ CRITICAL:     0 checks
❓ UNKNOWN:      0 checks
──────────────────────
📊 TOTAL:       15 checks

🎉 OVERALL STATUS: HEALTHY
📈 HEALTH SCORE: 80%

=== RECOMMENDATIONS ===
⚠️  Configure Pushover notifications for alerts
⚠️  Install grpcurl for enhanced Starlink API testing
ℹ️  System is operating normally with minor configuration gaps
```

## Log Freshness Monitoring

The health check system monitors log freshness to detect stale monitoring:

### Configurable Thresholds

- **CRITICAL**: No logs in 60+ minutes (monitoring likely failed)
- **WARNING**: No logs in 30-60 minutes (monitoring may be delayed)
- **HEALTHY**: Recent logs within 30 minutes

### Log Locations Monitored

- Main monitoring logs
- System logs
- Error logs
- State files

### Custom Thresholds

```bash
# Set custom log freshness threshold (in minutes)
LOG_FRESHNESS_THRESHOLD=45 /root/starlink-monitor/scripts/health-check.sh
```

## System Resource Monitoring

### Disk Space Monitoring

```bash
# Critical: < 1GB free space
# Warning: < 2GB free space
# Healthy: > 2GB free space
```

### Memory Monitoring

```bash
# Critical: > 90% memory usage
# Warning: > 80% memory usage
# Healthy: < 80% memory usage
```

### CPU Load Monitoring

```bash
# Critical: Load > 2.0
# Warning: Load > 1.0
# Healthy: Load < 1.0
```

## Integration with Other Scripts

The health check system integrates with all existing test scripts:

### Test Script Integration

- **test-pushover.sh**: Notification system testing
- **test-monitoring.sh**: Core monitoring functionality
- **system-status.sh**: System overview
- **validate-config.sh**: Configuration validation

### Quiet Mode Support

All integrated test scripts support `--quiet` mode for clean health check output:

```bash
# Scripts run in quiet mode during health checks
/root/starlink-monitor/scripts/test-pushover.sh --quiet
/root/starlink-monitor/scripts/system-status.sh --quiet
```

## Automated Health Monitoring

### Cron Integration

Add health checks to crontab for automated monitoring:

```bash
# Run health check every hour
0 * * * * /root/starlink-monitor/scripts/health-check.sh --quick > /var/log/health-check.log 2>&1

# Run comprehensive health check daily
0 6 * * * /root/starlink-monitor/scripts/health-check.sh > /var/log/health-check-daily.log 2>&1
```

### Health Check Notifications

Integrate with notification system for automated alerts:

```bash
# Health check with notification on critical issues
/root/starlink-monitor/scripts/health-check.sh && echo "Health check passed" || echo "Health check failed - investigate immediately"
```

## Exit Codes

The health check script uses standard exit codes:

- **0**: All checks passed (healthy)
- **1**: Warnings found (investigate)
- **2**: Critical issues found (immediate attention required)
- **3**: No checks performed (configuration error)

### Using Exit Codes in Scripts

```bash
#!/bin/sh
if /root/starlink-monitor/scripts/health-check.sh --quick; then
    echo "System is healthy"
else
    case $? in
        1) echo "System has warnings - investigate when convenient" ;;
        2) echo "System has critical issues - immediate attention required" ;;
        3) echo "Health check failed to run - check configuration" ;;
    esac
fi
```

## Debug Mode

Enable debug mode for detailed troubleshooting:

```bash
DEBUG=1 /root/starlink-monitor/scripts/health-check.sh
```

### Debug Output Includes

- Detailed execution flow
- Variable values and states
- Command execution traces
- File access and permission checks
- Network connectivity details
- Configuration parsing information

## Troubleshooting Common Issues

### 1. Health Check Script Not Found

```bash
# Verify installation
ls -la /root/starlink-monitor/scripts/health-check.sh

# Re-run installation if missing
curl -fL https://raw.githubusercontent.com/markus-lassfolk/autonomy/main/scripts/install.sh | sh
```

### 2. Permission Errors

```bash
# Fix script permissions
chmod +x /root/starlink-monitor/scripts/health-check.sh
```

### 3. Network Connectivity Issues

```bash
# Test basic connectivity
ping -c 1 8.8.8.8

# Test DNS resolution
nslookup google.com

# Test Starlink device
ping -c 1 192.168.100.1
```

### 4. Configuration Issues

```bash
# Run configuration validation
/root/starlink-monitor/scripts/validate-config.sh

# Check for placeholder values
grep -r "YOUR_" /root/starlink-monitor/config/
```

## Best Practices

### 1. Regular Health Checks

- Run health checks regularly (hourly quick checks, daily full checks)
- Monitor health check logs for trends
- Set up notifications for critical issues

### 2. Proactive Monitoring

- Monitor log freshness to detect stale monitoring
- Watch system resource usage trends
- Verify network connectivity regularly

### 3. Configuration Management

- Keep configuration up to date
- Replace placeholder values with real settings
- Use graceful degradation for optional features

### 4. Documentation

- Document any custom modifications
- Keep track of system changes
- Maintain troubleshooting notes

## Integration with Monitoring Systems

### Nagios Integration

```bash
# Nagios check command
define command{
    command_name    check_starlink_health
    command_line    /root/starlink-monitor/scripts/health-check.sh --quick
}
```

### Prometheus Integration

```bash
# Export health metrics
/root/starlink-monitor/scripts/health-check.sh --quick | grep "HEALTH SCORE" | awk '{print "starlink_health_score " $3}' | sed 's/%//' > /var/lib/prometheus/node-exporter/starlink_health.prom
```

### Custom Monitoring Integration

```bash
# JSON output for integration
/root/starlink-monitor/scripts/health-check.sh --json > /tmp/health-status.json
```

## Advanced Configuration

### Custom Health Check Thresholds

Create custom configuration file:

```bash
# /root/starlink-monitor/config/health-check.conf
DISK_SPACE_CRITICAL=1000000  # 1GB in KB
DISK_SPACE_WARNING=2000000   # 2GB in KB
MEMORY_CRITICAL=90           # 90% usage
MEMORY_WARNING=80            # 80% usage
LOAD_CRITICAL=2.0            # Load average 2.0
LOAD_WARNING=1.0             # Load average 1.0
LOG_FRESHNESS_CRITICAL=60    # 60 minutes
LOG_FRESHNESS_WARNING=30     # 30 minutes
```

### Health Check Customization

```bash
# Custom health check script
#!/bin/sh
# Source the main health check
. /root/starlink-monitor/scripts/health-check.sh

# Add custom checks
check_custom_service() {
    # Custom service check logic
    return 0
}

# Run custom checks
check_custom_service
```

## Conclusion

The health check system provides comprehensive monitoring capabilities for the RUTOS Starlink Failover solution. It
offers:

- **Comprehensive Coverage**: All system components monitored
- **Flexible Operation**: Multiple modes for different use cases
- **Clear Reporting**: Easy-to-understand status information
- **Integration Ready**: Works with external monitoring systems
- **Automated Operation**: Suitable for cron-based automation
- **Troubleshooting Support**: Debug mode for problem resolution

Regular use of the health check system ensures reliable operation of the Starlink failover solution and provides early
warning of potential issues.

---

_For additional information, see the main project documentation and testing guides._
