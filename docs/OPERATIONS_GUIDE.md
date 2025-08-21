# autonomy Operations Guide

A comprehensive guide for system administrators and operators managing autonomy in production environments.

## Table of Contents

1. [Overview](#overview)
2. [System Architecture](#system-architecture)
3. [Operational Procedures](#operational-procedures)
4. [Monitoring and Alerting](#monitoring-and-alerting)
5. [Maintenance Procedures](#maintenance-procedures)
6. [Performance Tuning](#performance-tuning)
7. [Security Considerations](#security-considerations)
8. [Disaster Recovery](#disaster-recovery)
9. [Troubleshooting](#troubleshooting)

## Overview

This guide provides operational procedures and best practices for managing autonomy in production environments. It covers monitoring, maintenance, performance optimization, and troubleshooting procedures.

### Target Audience

- **System Administrators**: Responsible for router infrastructure
- **Network Operators**: Managing network connectivity and failover
- **DevOps Engineers**: Automating monitoring and deployment
- **Support Teams**: Troubleshooting and incident response

### Operational Responsibilities

- **24/7 Monitoring**: Continuous system health monitoring
- **Performance Optimization**: Tuning for optimal performance
- **Security Management**: Access control and threat detection
- **Backup and Recovery**: Configuration and data protection
- **Incident Response**: Troubleshooting and resolution

## System Architecture

### Component Overview

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   autonomyd     │    │   Starwatch     │    │   LuCI Web UI   │
│   (Main Daemon) │    │   (Watchdog)    │    │   (Management)  │
└─────────────────┘    └─────────────────┘    └─────────────────┘
         │                       │                       │
         └───────────────────────┼───────────────────────┘
                                 │
         ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
         │   mwan3         │    │   ubus          │    │   UCI Config    │
         │   (Load Bal.)   │    │   (System Bus)  │    │   (Settings)    │
         └─────────────────┘    └─────────────────┘    └─────────────────┘
```

### Key Components

#### autonomyd (Main Daemon)
- **Purpose**: Core failover logic and interface management
- **Process**: Runs as system service via procd
- **Configuration**: `/etc/config/autonomy`
- **Logs**: `/var/log/messages` (via syslog)

#### Starwatch (Watchdog)
- **Purpose**: Independent health monitoring and recovery
- **Process**: Separate daemon process
- **Configuration**: Built-in with minimal dependencies
- **Logs**: `/var/log/starwatch.log`

#### LuCI Web Interface
- **Purpose**: Web-based management and monitoring
- **Access**: HTTP/HTTPS on router IP
- **Configuration**: `/etc/config/luci-app-autonomy`
- **Logs**: Browser developer tools

### Data Flow

1. **Interface Discovery**: autonomyd discovers mwan3 members
2. **Health Monitoring**: Continuous health checks on all interfaces
3. **Decision Making**: Predictive algorithms determine optimal interface
4. **Failover Execution**: mwan3 interface priority changes
5. **Notification**: Alerts sent via configured channels
6. **Logging**: All events logged for audit and troubleshooting

## Operational Procedures

### Daily Operations

#### Morning Checks

```bash
# Check system status
autonomyctl status

# Verify all interfaces are healthy
autonomyctl members

# Check for any alerts or warnings
autonomyctl notifications

# Review overnight logs
logread | grep autonomy | tail -50

# Check system resources
autonomyctl health
```

#### Continuous Monitoring

```bash
# Real-time log monitoring
logread -f | grep autonomy

# Watch interface status changes
watch -n 5 'autonomyctl members'

# Monitor system performance
watch -n 10 'autonomyctl health'
```

#### Evening Checks

```bash
# Review daily performance
autonomyctl metrics --period daily

# Check failover history
autonomyctl decisions --limit 10

# Verify backup systems
autonomyctl config backup

# Review alert history
autonomyctl notifications --history
```

### Weekly Operations

#### Performance Review

```bash
# Generate weekly report
autonomyctl report --period weekly --format json > weekly_report.json

# Analyze failover patterns
autonomyctl decisions --period weekly --analysis

# Review resource usage trends
autonomyctl metrics --period weekly --trends
```

#### Configuration Review

```bash
# Validate current configuration
autonomyctl config validate

# Compare with backup
diff /etc/config/autonomy /etc/config/autonomy.backup

# Check for configuration drift
autonomyctl config audit
```

#### Maintenance Tasks

```bash
# Clean old logs
logrotate -f /etc/logrotate.d/autonomy

# Update telemetry data
autonomyctl telemetry cleanup

# Verify backup integrity
autonomyctl config verify-backup
```

### Monthly Operations

#### Comprehensive Review

```bash
# Generate monthly report
autonomyctl report --period monthly --format html > monthly_report.html

# Performance analysis
autonomyctl analytics --period monthly

# Security audit
autonomyctl security audit
```

#### System Optimization

```bash
# Performance tuning
autonomyctl optimize

# Configuration optimization
autonomyctl config optimize

# Resource cleanup
autonomyctl cleanup --all
```

## Monitoring and Alerting

### Monitoring Strategy

#### Key Metrics to Monitor

1. **Interface Health Scores**
   - Target: > 80 for primary interfaces
   - Alert: < 60 for any interface
   - Critical: < 30 for primary interface

2. **Failover Frequency**
   - Normal: < 5 per day
   - Warning: 5-10 per day
   - Critical: > 10 per day

3. **System Performance**
   - CPU Usage: < 20%
   - Memory Usage: < 50%
   - Disk Usage: < 80%

4. **Network Performance**
   - Latency: < 100ms for primary
   - Packet Loss: < 1%
   - Throughput: > 80% of rated capacity

#### Monitoring Tools

```bash
# Built-in monitoring
autonomyctl monitor --continuous

# External monitoring integration
curl -X POST http://router-ip/ubus -d '{"jsonrpc":"2.0","id":1,"method":"call","params":["00000000000000000000000000000000","autonomy","status",{}]}'

# SNMP monitoring (if configured)
snmpwalk -v2c -c public router-ip .1.3.6.1.4.1.12345.1
```

### Alert Configuration

#### Alert Levels

```uci
config autonomy 'alerts'
    option info_enabled '1'
    option warning_enabled '1'
    option critical_enabled '1'
    option emergency_enabled '1'
    
    # Thresholds
    option health_warning '70'
    option health_critical '50'
    option failover_warning '5'
    option failover_critical '10'
```

#### Alert Channels

```uci
# Email alerts
config autonomy 'email'
    option enabled '1'
    option smtp_server 'smtp.company.com'
    option smtp_port '587'
    option username 'alerts@company.com'
    option password 'secure-password'
    option recipients 'admin@company.com,ops@company.com'

# Slack integration
config autonomy 'slack'
    option enabled '1'
    option webhook_url 'https://hooks.slack.com/services/xxx/yyy/zzz'
    option channel '#network-alerts'
    option username 'autonomy Bot'

# PagerDuty integration
config autonomy 'pagerduty'
    option enabled '1'
    option api_key 'your-pagerduty-api-key'
    option service_id 'your-service-id'
```

### Dashboard Setup

#### Grafana Dashboard

```json
{
  "dashboard": {
    "title": "autonomy Monitoring",
    "panels": [
      {
        "title": "Interface Health",
        "type": "graph",
        "targets": [
          {
            "expr": "autonomy_interface_health_score",
            "legendFormat": "{{interface}}"
          }
        ]
      },
      {
        "title": "Failover Events",
        "type": "stat",
        "targets": [
          {
            "expr": "rate(autonomy_failover_events_total[1h])",
            "legendFormat": "Failovers/hour"
          }
        ]
      }
    ]
  }
}
```

## Maintenance Procedures

### Regular Maintenance

#### Log Management

```bash
# Configure log rotation
cat > /etc/logrotate.d/autonomy << EOF
/var/log/autonomy.log {
    daily
    rotate 7
    compress
    delaycompress
    missingok
    notifempty
    create 644 root root
    postrotate
        /etc/init.d/autonomy reload
    endscript
}
EOF

# Manual log cleanup
find /var/log -name "*autonomy*" -mtime +30 -delete
```

#### Configuration Backup

```bash
# Automated backup script
cat > /usr/local/bin/autonomy-backup.sh << 'EOF'
#!/bin/sh
BACKUP_DIR="/backup/autonomy"
DATE=$(date +%Y%m%d_%H%M%S)

mkdir -p $BACKUP_DIR

# Backup configuration
cp /etc/config/autonomy $BACKUP_DIR/autonomy_$DATE.conf

# Backup telemetry data
autonomyctl telemetry export > $BACKUP_DIR/telemetry_$DATE.json

# Clean old backups (keep 30 days)
find $BACKUP_DIR -name "*.conf" -mtime +30 -delete
find $BACKUP_DIR -name "*.json" -mtime +30 -delete
EOF

chmod +x /usr/local/bin/autonomy-backup.sh

# Add to crontab
echo "0 2 * * * /usr/local/bin/autonomy-backup.sh" >> /etc/crontabs/root
```

#### Performance Optimization

```bash
# Weekly optimization script
cat > /usr/local/bin/autonomy-optimize.sh << 'EOF'
#!/bin/sh

# Optimize configuration
autonomyctl config optimize

# Clean telemetry data
autonomyctl telemetry cleanup --older-than 30d

# Optimize system performance
autonomyctl optimize --aggressive

# Generate optimization report
autonomyctl optimize --report > /var/log/autonomy-optimization.log
EOF

chmod +x /usr/local/bin/autonomy-optimize.sh

# Add to crontab
echo "0 3 * * 0 /usr/local/bin/autonomy-optimize.sh" >> /etc/crontabs/root
```

### Emergency Procedures

#### Service Recovery

```bash
# Emergency restart procedure
emergency_restart() {
    echo "Emergency restart initiated at $(date)"
    
    # Stop all services
    /etc/init.d/autonomy stop
    /etc/init.d/starwatch stop
    
    # Wait for processes to terminate
    sleep 5
    
    # Kill any remaining processes
    pkill -f autonomyd
    pkill -f starwatch
    
    # Clear any lock files
    rm -f /var/run/autonomy.pid
    rm -f /var/run/starwatch.pid
    
    # Restart services
    /etc/init.d/autonomy start
    /etc/init.d/starwatch start
    
    # Verify recovery
    sleep 10
    autonomyctl status
}

# Manual failover procedure
manual_failover() {
    INTERFACE=$1
    
    if [ -z "$INTERFACE" ]; then
        echo "Usage: manual_failover <interface>"
        return 1
    fi
    
    echo "Manual failover to $INTERFACE at $(date)"
    
    # Execute failover
    autonomyctl failover --interface $INTERFACE --force
    
    # Verify failover
    sleep 5
    autonomyctl status
}
```

#### Configuration Recovery

```bash
# Configuration recovery procedure
recover_config() {
    BACKUP_FILE=$1
    
    if [ -z "$BACKUP_FILE" ]; then
        echo "Usage: recover_config <backup_file>"
        return 1
    fi
    
    echo "Recovering configuration from $BACKUP_FILE"
    
    # Stop services
    /etc/init.d/autonomy stop
    
    # Backup current config
    cp /etc/config/autonomy /etc/config/autonomy.recovery.backup
    
    # Restore from backup
    cp $BACKUP_FILE /etc/config/autonomy
    
    # Validate configuration
    autonomyctl config validate
    
    # Restart services
    /etc/init.d/autonomy start
    
    # Verify recovery
    sleep 10
    autonomyctl status
}
```

## Performance Tuning

### System Optimization

#### Resource Limits

```uci
config autonomy 'performance'
    option max_memory_mb '64'
    option max_cpu_percent '20'
    option gc_interval '300'
    option telemetry_retention_days '30'
    option log_retention_days '7'
```

#### Polling Optimization

```uci
config autonomy 'polling'
    option base_interval_ms '1500'
    option adaptive_polling '1'
    option min_interval_ms '500'
    option max_interval_ms '5000'
    option load_threshold '80'
```

#### Interface-Specific Tuning

```uci
# Starlink optimization
config autonomy 'starlink'
    option health_check_interval '30'
    option obstruction_check_interval '60'
    option gps_update_interval '300'
    option predictive_enabled '1'

# Cellular optimization
config autonomy 'cellular'
    option signal_check_interval '15'
    option data_usage_check_interval '300'
    option stability_analysis '1'
    option roaming_detection '1'

# WiFi optimization
config autonomy 'wifi'
    option channel_scan_interval '600'
    option rssi_check_interval '10'
    option interference_detection '1'
    option auto_optimization '1'
```

### Network Optimization

#### Bandwidth Management

```uci
config autonomy 'bandwidth'
    option metered_mode '1'
    option data_limit_enabled '1'
    option traffic_shaping '1'
    option qos_enabled '1'
    
    # Bandwidth limits
    option starlink_limit_mbps '100'
    option cellular_limit_mbps '50'
    option wifi_limit_mbps '25'
```

#### Quality of Service

```uci
config autonomy 'qos'
    option enabled '1'
    option priority_queues '4'
    option bandwidth_sharing 'fair'
    option latency_optimization '1'
    
    # Priority mapping
    option voice_priority '1'
    option video_priority '2'
    option data_priority '3'
    option bulk_priority '4'
```

## Security Considerations

### Access Control

#### User Management

```bash
# Create dedicated user for autonomy
adduser -D -s /bin/false autonomy
addgroup autonomy ubus

# Set proper permissions
chown autonomy:autonomy /etc/config/autonomy
chmod 600 /etc/config/autonomy
```

#### API Security

```uci
config autonomy 'security'
    option api_authentication '1'
    option api_encryption '1'
    option rate_limiting '1'
    option ip_whitelist '192.168.1.0/24'
    
    # API keys
    option api_key_required '1'
    option api_key_rotation_days '90'
```

#### Network Security

```uci
config autonomy 'network_security'
    option firewall_integration '1'
    option intrusion_detection '1'
    option ddos_protection '1'
    option traffic_monitoring '1'
```

### Threat Detection

#### Security Monitoring

```bash
# Security monitoring script
cat > /usr/local/bin/autonomy-security.sh << 'EOF'
#!/bin/sh

# Check for suspicious activity
autonomyctl security audit

# Monitor for brute force attempts
grep "authentication failed" /var/log/messages | wc -l

# Check for unusual failover patterns
autonomyctl decisions --analysis --security

# Generate security report
autonomyctl security report > /var/log/autonomy-security.log
EOF

chmod +x /usr/local/bin/autonomy-security.sh

# Add to crontab
echo "0 4 * * * /usr/local/bin/autonomy-security.sh" >> /etc/crontabs/root
```

## Disaster Recovery

### Backup Strategy

#### Configuration Backup

```bash
# Comprehensive backup script
cat > /usr/local/bin/autonomy-disaster-backup.sh << 'EOF'
#!/bin/sh
BACKUP_DIR="/backup/autonomy/disaster"
DATE=$(date +%Y%m%d_%H%M%S)

mkdir -p $BACKUP_DIR

# Full system backup
tar -czf $BACKUP_DIR/autonomy-full-$DATE.tar.gz \
    /etc/config/autonomy \
    /etc/init.d/autonomy \
    /usr/sbin/autonomyd \
    /usr/sbin/autonomyctl \
    /var/lib/autonomy

# Configuration only
cp /etc/config/autonomy $BACKUP_DIR/autonomy-config-$DATE.conf

# Telemetry data
autonomyctl telemetry export > $BACKUP_DIR/telemetry-$DATE.json

# System state
autonomyctl status > $BACKUP_DIR/status-$DATE.json
autonomyctl members > $BACKUP_DIR/members-$DATE.json

# Create recovery script
cat > $BACKUP_DIR/recover-$DATE.sh << 'RECOVERY_SCRIPT'
#!/bin/sh
echo "Disaster recovery script for backup $DATE"
echo "Run this script to restore autonomy configuration"
# Add recovery commands here
RECOVERY_SCRIPT

chmod +x $BACKUP_DIR/recover-$DATE.sh

# Clean old backups (keep 90 days)
find $BACKUP_DIR -name "*.tar.gz" -mtime +90 -delete
find $BACKUP_DIR -name "*.conf" -mtime +90 -delete
find $BACKUP_DIR -name "*.json" -mtime +90 -delete
find $BACKUP_DIR -name "recover-*.sh" -mtime +90 -delete
EOF

chmod +x /usr/local/bin/autonomy-disaster-backup.sh

# Add to crontab
echo "0 1 * * * /usr/local/bin/autonomy-disaster-backup.sh" >> /etc/crontabs/root
```

#### Recovery Procedures

```bash
# Disaster recovery procedure
disaster_recovery() {
    BACKUP_DATE=$1
    
    if [ -z "$BACKUP_DATE" ]; then
        echo "Usage: disaster_recovery <backup_date>"
        return 1
    fi
    
    echo "Starting disaster recovery from backup $BACKUP_DATE"
    
    # Stop all services
    /etc/init.d/autonomy stop
    /etc/init.d/starwatch stop
    
    # Restore from backup
    tar -xzf /backup/autonomy/disaster/autonomy-full-$BACKUP_DATE.tar.gz -C /
    
    # Restore configuration
    cp /backup/autonomy/disaster/autonomy-config-$BACKUP_DATE.conf /etc/config/autonomy
    
    # Validate configuration
    autonomyctl config validate
    
    # Restart services
    /etc/init.d/autonomy start
    /etc/init.d/starwatch start
    
    # Verify recovery
    sleep 30
    autonomyctl status
    
    echo "Disaster recovery completed"
}
```

### Business Continuity

#### Failover Testing

```bash
# Automated failover testing
cat > /usr/local/bin/autonomy-failover-test.sh << 'EOF'
#!/bin/sh
echo "Starting automated failover test at $(date)"

# Test each interface
for interface in starlink cellular wifi lan; do
    echo "Testing failover to $interface"
    
    # Trigger failover
    autonomyctl failover --interface $interface --test
    
    # Wait for stabilization
    sleep 30
    
    # Verify connectivity
    if ping -c 3 8.8.8.8 > /dev/null 2>&1; then
        echo "✓ Failover to $interface successful"
    else
        echo "✗ Failover to $interface failed"
    fi
    
    # Return to primary
    autonomyctl failover --interface starlink
    sleep 30
done

echo "Failover testing completed at $(date)"
EOF

chmod +x /usr/local/bin/autonomy-failover-test.sh

# Add to crontab (weekly testing)
echo "0 2 * * 0 /usr/local/bin/autonomy-failover-test.sh" >> /etc/crontabs/root
```

## Troubleshooting

### Diagnostic Procedures

#### System Diagnostics

```bash
# Comprehensive diagnostic script
cat > /usr/local/bin/autonomy-diagnostics.sh << 'EOF'
#!/bin/sh
DIAG_FILE="/tmp/autonomy-diagnostics-$(date +%Y%m%d_%H%M%S).txt"

echo "autonomy Diagnostics Report" > $DIAG_FILE
echo "Generated: $(date)" >> $DIAG_FILE
echo "========================================" >> $DIAG_FILE

# System information
echo "System Information:" >> $DIAG_FILE
uname -a >> $DIAG_FILE
cat /etc/os-release >> $DIAG_FILE
echo "" >> $DIAG_FILE

# Service status
echo "Service Status:" >> $DIAG_FILE
/etc/init.d/autonomy status >> $DIAG_FILE
/etc/init.d/starwatch status >> $DIAG_FILE
echo "" >> $DIAG_FILE

# Configuration
echo "Configuration:" >> $DIAG_FILE
cat /etc/config/autonomy >> $DIAG_FILE
echo "" >> $DIAG_FILE

# Network interfaces
echo "Network Interfaces:" >> $DIAG_FILE
ip link show >> $DIAG_FILE
ip addr show >> $DIAG_FILE
echo "" >> $DIAG_FILE

# mwan3 status
echo "mwan3 Status:" >> $DIAG_FILE
ubus call mwan3 status >> $DIAG_FILE
echo "" >> $DIAG_FILE

# autonomy status
echo "autonomy Status:" >> $DIAG_FILE
autonomyctl status >> $DIAG_FILE
echo "" >> $DIAG_FILE

# Recent logs
echo "Recent Logs:" >> $DIAG_FILE
logread | grep autonomy | tail -50 >> $DIAG_FILE
echo "" >> $DIAG_FILE

# Performance metrics
echo "Performance Metrics:" >> $DIAG_FILE
autonomyctl health >> $DIAG_FILE
autonomyctl metrics >> $DIAG_FILE
echo "" >> $DIAG_FILE

echo "Diagnostics saved to $DIAG_FILE"
EOF

chmod +x /usr/local/bin/autonomy-diagnostics.sh
```

#### Common Issues and Solutions

| Issue | Symptoms | Solution |
|-------|----------|----------|
| Service won't start | `autonomyd: command not found` | Check installation and permissions |
| No interfaces discovered | Empty member list | Verify mwan3 configuration |
| High CPU usage | System becomes unresponsive | Optimize polling intervals |
| Memory leaks | Growing memory usage | Restart service, check for bugs |
| Configuration errors | Service fails to start | Validate configuration syntax |
| Network connectivity issues | Failover not working | Check interface configuration |

### Performance Troubleshooting

#### Resource Monitoring

```bash
# Performance monitoring script
cat > /usr/local/bin/autonomy-performance.sh << 'EOF'
#!/bin/sh

echo "autonomy Performance Report"
echo "=========================="

# CPU usage
echo "CPU Usage:"
top -n 1 | grep autonomy

# Memory usage
echo "Memory Usage:"
ps aux | grep autonomy | grep -v grep

# Network performance
echo "Network Performance:"
autonomyctl metrics --performance

# System load
echo "System Load:"
uptime

# Disk usage
echo "Disk Usage:"
df -h /var/log /var/lib/autonomy
EOF

chmod +x /usr/local/bin/autonomy-performance.sh
```

---

For additional operational support:

- **Emergency Contacts**: [Your emergency contact information]
- **Escalation Procedures**: [Your escalation procedures]
- **Change Management**: [Your change management process]
- **Documentation**: [Links to additional documentation]

---

**Last Updated**: 2025-01-20 15:30 UTC
**Version**: 1.0.0
**Maintainer**: [Your contact information]
