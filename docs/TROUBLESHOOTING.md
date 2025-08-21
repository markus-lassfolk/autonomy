# autonomy Troubleshooting Guide

A comprehensive guide for diagnosing and resolving issues with the autonomy intelligent multi-interface failover system.

## Table of Contents

1. [Quick Diagnostic Steps](#quick-diagnostic-steps)
2. [Common Issues](#common-issues)
3. [Service Problems](#service-problems)
4. [Network Issues](#network-issues)
5. [Configuration Problems](#configuration-problems)
6. [Performance Issues](#performance-issues)
7. [GPS and Location Issues](#gps-and-location-issues)
8. [Notification Problems](#notification-problems)
9. [Advanced Diagnostics](#advanced-diagnostics)
10. [Recovery Procedures](#recovery-procedures)

## Quick Diagnostic Steps

### Initial Assessment

When troubleshooting autonomy, start with these quick diagnostic steps:

```bash
# 1. Check if the service is running
/etc/init.d/autonomy status

# 2. Check basic system status
autonomyctl status

# 3. Verify network interfaces
autonomyctl members

# 4. Check recent logs
logread | grep autonomy | tail -20

# 5. Verify configuration
autonomyctl config validate
```

### Emergency Commands

If the system is not responding:

```bash
# Force restart the service
/etc/init.d/autonomy restart

# Check for stuck processes
ps aux | grep autonomy

# Kill any stuck processes
pkill -f autonomyd

# Clear lock files
rm -f /var/run/autonomy.pid
```

## Common Issues

### Issue: Service Won't Start

#### Symptoms
- `autonomyd: command not found`
- Service fails to start with error messages
- No autonomy processes running

#### Diagnostic Steps

```bash
# Check if binary exists
which autonomyd
ls -la /usr/sbin/autonomyd

# Check file permissions
ls -la /usr/sbin/autonomyd
ls -la /etc/init.d/autonomy

# Check dependencies
opkg list-installed | grep mwan3
opkg list-installed | grep ubus

# Check system resources
free -h
df -h
```

#### Solutions

1. **Binary Missing**:
```bash
# Reinstall the package
opkg remove autonomy
opkg install autonomy

# Or manually install
wget https://github.com/your-repo/autonomy/releases/latest/download/autonomy_1.0.0_arm_cortex-a7.ipk
opkg install autonomy_1.0.0_arm_cortex-a7.ipk
```

2. **Permission Issues**:
```bash
# Fix permissions
chmod 755 /usr/sbin/autonomyd
chmod 755 /etc/init.d/autonomy
chown root:root /usr/sbin/autonomyd
chown root:root /etc/init.d/autonomy
```

3. **Missing Dependencies**:
```bash
# Install required packages
opkg update
opkg install mwan3 ubus procd

# Check for missing libraries
ldd /usr/sbin/autonomyd
```

### Issue: No Network Members Discovered

#### Symptoms
- Empty member list in `autonomyctl members`
- No interfaces being monitored
- Failover not working

#### Diagnostic Steps

```bash
# Check mwan3 configuration
ubus call mwan3 status

# Verify network interfaces
ip link show
ip addr show

# Check mwan3 members
ubus call mwan3 status | grep -A 10 "members"

# Check autonomy interface discovery
autonomyctl members --debug
```

#### Solutions

1. **mwan3 Not Configured**:
```bash
# Basic mwan3 setup
uci set mwan3.default=interface
uci set mwan3.default.enabled=1
uci set mwan3.default.family=ipv4
uci set mwan3.default.track_method=ping
uci set mwan3.default.track_ip=8.8.8.8
uci commit mwan3
/etc/init.d/mwan3 restart
```

2. **Interface Names Mismatch**:
```bash
# Check current interface names
ip link show

# Update autonomy configuration
uci set autonomy.interfaces.starlink=wan
uci set autonomy.interfaces.cellular=wwan0
uci set autonomy.interfaces.wifi=wlan0
uci commit autonomy
/etc/init.d/autonomy restart
```

3. **Interfaces Not Enabled**:
```bash
# Enable interfaces in mwan3
uci set mwan3.wan=interface
uci set mwan3.wan.enabled=1
uci set mwan3.wan.family=ipv4
uci commit mwan3
/etc/init.d/mwan3 restart
```

### Issue: High CPU Usage

#### Symptoms
- System becomes unresponsive
- High CPU usage in `top` output
- Slow response to commands

#### Diagnostic Steps

```bash
# Check CPU usage
top -n 1 | grep autonomy

# Check memory usage
ps aux | grep autonomy

# Check system load
uptime

# Monitor resource usage over time
watch -n 5 'ps aux | grep autonomy'
```

#### Solutions

1. **Optimize Polling Intervals**:
```bash
# Increase polling intervals
uci set autonomy.main.poll_interval_ms=3000
uci set autonomy.starlink.health_check_interval=60
uci set autonomy.cellular.health_check_interval=60
uci commit autonomy
/etc/init.d/autonomy restart
```

2. **Disable Unused Features**:
```bash
# Disable features you don't need
uci set autonomy.gps.enabled=0
uci set autonomy.predictive.enabled=0
uci set autonomy.notifications.enabled=0
uci commit autonomy
/etc/init.d/autonomy restart
```

3. **Resource Limits**:
```bash
# Set resource limits
uci set autonomy.performance.max_cpu_percent=20
uci set autonomy.performance.max_memory_mb=64
uci commit autonomy
/etc/init.d/autonomy restart
```

## Service Problems

### Issue: Service Crashes Frequently

#### Symptoms
- Service stops unexpectedly
- Frequent restarts in logs
- Error messages in system logs

#### Diagnostic Steps

```bash
# Check crash logs
logread | grep -i crash
logread | grep -i segfault

# Check system logs
dmesg | grep -i autonomy

# Check for memory issues
free -h
cat /proc/meminfo | grep -i memavailable
```

#### Solutions

1. **Memory Issues**:
```bash
# Increase swap space
dd if=/dev/zero of=/swapfile bs=1M count=256
chmod 600 /swapfile
mkswap /swapfile
swapon /swapfile

# Add to fstab for persistence
echo "/swapfile none swap sw 0 0" >> /etc/fstab
```

2. **Configuration Errors**:
```bash
# Validate configuration
autonomyctl config validate

# Reset to default configuration
cp /etc/config/autonomy.example /etc/config/autonomy
/etc/init.d/autonomy restart
```

3. **Dependency Issues**:
```bash
# Reinstall dependencies
opkg update
opkg install --force-reinstall mwan3 ubus procd

# Check library dependencies
ldd /usr/sbin/autonomyd
```

### Issue: Service Won't Stop

#### Symptoms
- `stop` command hangs
- Process remains running after stop
- Can't restart the service

#### Diagnostic Steps

```bash
# Check process status
ps aux | grep autonomy

# Check for child processes
pstree -p | grep autonomy

# Check for lock files
ls -la /var/run/autonomy*

# Check system resources
top -n 1
```

#### Solutions

1. **Force Kill Process**:
```bash
# Kill all autonomy processes
pkill -f autonomyd
pkill -f starwatch

# Wait and verify
sleep 5
ps aux | grep autonomy

# Clear lock files
rm -f /var/run/autonomy.pid
rm -f /var/run/starwatch.pid
```

2. **System Resource Issues**:
```bash
# Check disk space
df -h

# Check memory
free -h

# Check system load
uptime
```

## Network Issues

### Issue: Failover Not Working

#### Symptoms
- Interfaces don't switch when primary fails
- Manual failover doesn't work
- No failover events in logs

#### Diagnostic Steps

```bash
# Check current active interface
autonomyctl status

# Check interface health scores
autonomyctl members --detailed

# Check mwan3 status
ubus call mwan3 status

# Check failover history
autonomyctl decisions --limit 10
```

#### Solutions

1. **mwan3 Configuration Issues**:
```bash
# Verify mwan3 configuration
cat /etc/config/mwan3

# Test mwan3 manually
ubus call mwan3 status

# Restart mwan3
/etc/init.d/mwan3 restart
```

2. **Interface Priority Issues**:
```bash
# Check interface priorities
autonomyctl members

# Set proper priorities
uci set autonomy.starlink.priority=100
uci set autonomy.cellular.priority=80
uci set autonomy.wifi.priority=60
uci commit autonomy
/etc/init.d/autonomy restart
```

3. **Health Check Issues**:
```bash
# Test health checks manually
ping -c 3 8.8.8.8
ping -c 3 1.1.1.1

# Check health check configuration
uci show autonomy | grep health
```

### Issue: Starlink Interface Not Detected

#### Symptoms
- Starlink not in member list
- No Starlink metrics available
- Starlink health score shows 0

#### Diagnostic Steps

```bash
# Check Starlink connection
ip link show | grep -i starlink
ip addr show | grep -i starlink

# Check Starlink API
curl -s http://192.168.100.1/status

# Check Starlink configuration
uci show autonomy | grep starlink

# Test Starlink connectivity
ping -c 3 192.168.100.1
```

#### Solutions

1. **Starlink Dish Not Connected**:
```bash
# Check physical connection
ip link show

# Check Starlink dish status
curl -s http://192.168.100.1/status | jq .

# Restart network interface
ifdown wan
ifup wan
```

2. **Starlink API Issues**:
```bash
# Check Starlink API access
curl -s http://192.168.100.1/status

# Update Starlink configuration
uci set autonomy.starlink.api_enabled=1
uci set autonomy.starlink.api_timeout=30
uci commit autonomy
/etc/init.d/autonomy restart
```

3. **Network Configuration**:
```bash
# Check network configuration
cat /etc/config/network | grep -A 10 "config interface 'wan'"

# Update interface configuration
uci set network.wan.proto=dhcp
uci set network.wan.device=eth0
uci commit network
/etc/init.d/network restart
```

### Issue: Cellular Interface Problems

#### Symptoms
- Cellular not detected or working
- Poor cellular signal
- Cellular data limits not working

#### Diagnostic Steps

```bash
# Check cellular interface
ip link show | grep -i wwan
ip addr show | grep -i wwan

# Check cellular signal
ubus call network.device status '{"name":"wwan0"}'

# Check cellular configuration
uci show network | grep wwan

# Test cellular connectivity
ping -c 3 8.8.8.8 -I wwan0
```

#### Solutions

1. **Cellular Modem Issues**:
```bash
# Check USB modem
lsusb
dmesg | grep -i usb

# Restart cellular interface
ifdown wwan0
ifup wwan0

# Check cellular logs
logread | grep -i wwan
```

2. **Signal Issues**:
```bash
# Check signal strength
ubus call network.device status '{"name":"wwan0"}' | jq '.signal'

# Adjust signal thresholds
uci set autonomy.cellular.signal_threshold=-90
uci commit autonomy
/etc/init.d/autonomy restart
```

3. **Data Limit Issues**:
```bash
# Check data limit configuration
uci show autonomy | grep data_limit

# Enable data limit monitoring
uci set autonomy.cellular.data_limit_enabled=1
uci set autonomy.cellular.data_limit_gb=50
uci commit autonomy
/etc/init.d/autonomy restart
```

## Configuration Problems

### Issue: Configuration Validation Errors

#### Symptoms
- Service fails to start with config errors
- Configuration validation fails
- UCI syntax errors

#### Diagnostic Steps

```bash
# Validate configuration
autonomyctl config validate

# Check UCI syntax
uci show autonomy

# Check for syntax errors
cat /etc/config/autonomy

# Test UCI commands
uci get autonomy.main.enable
```

#### Solutions

1. **UCI Syntax Errors**:
```bash
# Fix common syntax errors
# Remove any trailing spaces or invalid characters
sed -i 's/[[:space:]]*$//' /etc/config/autonomy

# Validate UCI syntax
uci show autonomy

# Commit changes
uci commit autonomy
```

2. **Missing Required Options**:
```bash
# Add missing required options
uci set autonomy.main.enable=1
uci set autonomy.main.use_mwan3=1
uci set autonomy.main.poll_interval_ms=1500
uci commit autonomy
```

3. **Invalid Values**:
```bash
# Check for invalid values
uci get autonomy.main.poll_interval_ms

# Set valid values
uci set autonomy.main.poll_interval_ms=1500
uci set autonomy.main.log_level=info
uci commit autonomy
```

### Issue: Configuration Not Applied

#### Symptoms
- Changes not taking effect
- Old configuration still active
- Service not picking up new settings

#### Diagnostic Steps

```bash
# Check current configuration
uci show autonomy

# Check if service is running
/etc/init.d/autonomy status

# Check configuration file
cat /etc/config/autonomy

# Verify configuration is loaded
autonomyctl config show
```

#### Solutions

1. **Service Not Restarted**:
```bash
# Restart service to apply changes
/etc/init.d/autonomy restart

# Verify changes applied
autonomyctl config show
```

2. **Configuration Not Committed**:
```bash
# Commit UCI changes
uci commit autonomy

# Restart service
/etc/init.d/autonomy restart
```

3. **Configuration File Issues**:
```bash
# Check file permissions
ls -la /etc/config/autonomy

# Fix permissions if needed
chmod 644 /etc/config/autonomy
chown root:root /etc/config/autonomy

# Restart service
/etc/init.d/autonomy restart
```

## Performance Issues

### Issue: High Memory Usage

#### Symptoms
- Growing memory usage over time
- System becomes slow
- Out of memory errors

#### Diagnostic Steps

```bash
# Check memory usage
free -h
ps aux | grep autonomy

# Check memory over time
watch -n 10 'free -h'

# Check for memory leaks
cat /proc/meminfo | grep -i mem
```

#### Solutions

1. **Memory Leak**:
```bash
# Restart service to clear memory
/etc/init.d/autonomy restart

# Monitor memory usage
watch -n 30 'ps aux | grep autonomy'

# Set memory limits
uci set autonomy.performance.max_memory_mb=64
uci commit autonomy
/etc/init.d/autonomy restart
```

2. **High Telemetry Data**:
```bash
# Clean old telemetry data
autonomyctl telemetry cleanup --older-than 7d

# Reduce telemetry retention
uci set autonomy.performance.telemetry_retention_days=7
uci commit autonomy
/etc/init.d/autonomy restart
```

3. **System Resource Issues**:
```bash
# Check system resources
top -n 1
df -h

# Optimize system performance
uci set autonomy.performance.gc_interval=300
uci commit autonomy
/etc/init.d/autonomy restart
```

### Issue: Slow Response Times

#### Symptoms
- Commands take long to respond
- Interface switching is slow
- System feels sluggish

#### Diagnostic Steps

```bash
# Check system load
uptime
top -n 1

# Check disk I/O
iostat -x 1 5

# Check network latency
ping -c 10 8.8.8.8

# Check autonomy performance
autonomyctl health
```

#### Solutions

1. **High System Load**:
```bash
# Check what's using CPU
top -n 1

# Optimize polling intervals
uci set autonomy.main.poll_interval_ms=3000
uci set autonomy.starlink.health_check_interval=60
uci commit autonomy
/etc/init.d/autonomy restart
```

2. **Disk I/O Issues**:
```bash
# Check disk usage
df -h

# Clean log files
logrotate -f /etc/logrotate.d/autonomy

# Move logs to RAM disk
uci set system.@system[0].log_file=/tmp/log/messages
uci commit system
/etc/init.d/system restart
```

3. **Network Latency**:
```bash
# Check network performance
ping -c 10 8.8.8.8

# Optimize health check targets
uci set autonomy.main.health_check_targets='8.8.8.8,1.1.1.1'
uci commit autonomy
/etc/init.d/autonomy restart
```

## GPS and Location Issues

### Issue: GPS Not Working

#### Symptoms
- No GPS coordinates available
- GPS status shows "unavailable"
- Location-based features not working

#### Diagnostic Steps

```bash
# Check GPS status
autonomyctl gps

# Check GPS configuration
uci show autonomy | grep gps

# Check GPS hardware
ubus call system board

# Test GPS connectivity
ping -c 3 192.168.100.1
```

#### Solutions

1. **GPS Hardware Issues**:
```bash
# Check GPS hardware connection
lsusb | grep -i gps
dmesg | grep -i gps

# Restart GPS interface
ifdown gps
ifup gps
```

2. **GPS Configuration Issues**:
```bash
# Enable GPS
uci set autonomy.gps.enabled=1
uci set autonomy.gps.source=starlink
uci set autonomy.gps.update_interval=60
uci commit autonomy
/etc/init.d/autonomy restart
```

3. **Starlink GPS Issues**:
```bash
# Check Starlink GPS
curl -s http://192.168.100.1/status | jq '.gps'

# Update GPS source
uci set autonomy.gps.source=starlink
uci commit autonomy
/etc/init.d/autonomy restart
```

### Issue: Location Services Not Working

#### Symptoms
- Geofencing not working
- Movement detection not working
- Location-based alerts not triggering

#### Diagnostic Steps

```bash
# Check location services
autonomyctl gps --detailed

# Check geofencing configuration
uci show autonomy | grep geofence

# Check movement detection
autonomyctl gps --movement
```

#### Solutions

1. **Geofencing Configuration**:
```bash
# Enable geofencing
uci set autonomy.gps.geofencing=1
uci set autonomy.gps.geofence_radius=1000
uci commit autonomy
/etc/init.d/autonomy restart
```

2. **Movement Detection**:
```bash
# Enable movement detection
uci set autonomy.gps.movement_detection=1
uci set autonomy.gps.movement_threshold=100
uci commit autonomy
/etc/init.d/autonomy restart
```

## Notification Problems

### Issue: Notifications Not Sending

#### Symptoms
- No alerts received
- Notification status shows errors
- Test notifications fail

#### Diagnostic Steps

```bash
# Check notification status
autonomyctl notifications

# Test notifications
autonomyctl notifications test

# Check notification logs
logread | grep -i notification

# Check notification configuration
uci show autonomy | grep notification
```

#### Solutions

1. **Email Notifications**:
```bash
# Check email configuration
uci show autonomy | grep email

# Test email connectivity
echo "test" | mail -s "test" your-email@example.com

# Update email settings
uci set autonomy.email.enabled=1
uci set autonomy.email.smtp_server=smtp.gmail.com
uci set autonomy.email.smtp_port=587
uci set autonomy.email.username=your-email@gmail.com
uci set autonomy.email.password=your-app-password
uci commit autonomy
/etc/init.d/autonomy restart
```

2. **Pushover Notifications**:
```bash
# Check Pushover configuration
uci show autonomy | grep pushover

# Test Pushover API
curl -s -F "token=your-api-token" -F "user=your-user-key" -F "message=test" https://api.pushover.net/1/messages.json

# Update Pushover settings
uci set autonomy.pushover.enabled=1
uci set autonomy.pushover.api_token=your-api-token
uci set autonomy.pushover.user_key=your-user-key
uci commit autonomy
/etc/init.d/autonomy restart
```

3. **Webhook Notifications**:
```bash
# Check webhook configuration
uci show autonomy | grep webhook

# Test webhook connectivity
curl -X POST -H "Content-Type: application/json" -d '{"test":"data"}' https://your-webhook-url.com

# Update webhook settings
uci set autonomy.webhook.enabled=1
uci set autonomy.webhook.url=https://your-webhook-url.com
uci set autonomy.webhook.method=POST
uci commit autonomy
/etc/init.d/autonomy restart
```

## Advanced Diagnostics

### Comprehensive Diagnostic Script

```bash
#!/bin/sh
# Save as /usr/local/bin/autonomy-diagnostics.sh

DIAG_FILE="/tmp/autonomy-diagnostics-$(date +%Y%m%d_%H%M%S).txt"

echo "autonomy Comprehensive Diagnostics Report" > $DIAG_FILE
echo "Generated: $(date)" >> $DIAG_FILE
echo "================================================" >> $DIAG_FILE

# System Information
echo "=== SYSTEM INFORMATION ===" >> $DIAG_FILE
uname -a >> $DIAG_FILE
cat /etc/os-release >> $DIAG_FILE
echo "" >> $DIAG_FILE

# Hardware Information
echo "=== HARDWARE INFORMATION ===" >> $DIAG_FILE
cat /proc/cpuinfo | grep "model name" | head -1 >> $DIAG_FILE
free -h >> $DIAG_FILE
df -h >> $DIAG_FILE
echo "" >> $DIAG_FILE

# Network Information
echo "=== NETWORK INFORMATION ===" >> $DIAG_FILE
ip link show >> $DIAG_FILE
ip addr show >> $DIAG_FILE
ip route show >> $DIAG_FILE
echo "" >> $DIAG_FILE

# Service Status
echo "=== SERVICE STATUS ===" >> $DIAG_FILE
/etc/init.d/autonomy status >> $DIAG_FILE
/etc/init.d/starwatch status >> $DIAG_FILE
/etc/init.d/mwan3 status >> $DIAG_FILE
echo "" >> $DIAG_FILE

# Process Information
echo "=== PROCESS INFORMATION ===" >> $DIAG_FILE
ps aux | grep autonomy >> $DIAG_FILE
ps aux | grep starwatch >> $DIAG_FILE
echo "" >> $DIAG_FILE

# Configuration
echo "=== CONFIGURATION ===" >> $DIAG_FILE
cat /etc/config/autonomy >> $DIAG_FILE
echo "" >> $DIAG_FILE

# autonomy Status
echo "=== autonomy STATUS ===" >> $DIAG_FILE
autonomyctl status >> $DIAG_FILE
echo "" >> $DIAG_FILE

# Network Members
echo "=== NETWORK MEMBERS ===" >> $DIAG_FILE
autonomyctl members >> $DIAG_FILE
echo "" >> $DIAG_FILE

# Health Information
echo "=== HEALTH INFORMATION ===" >> $DIAG_FILE
autonomyctl health >> $DIAG_FILE
echo "" >> $DIAG_FILE

# Recent Logs
echo "=== RECENT LOGS ===" >> $DIAG_FILE
logread | grep autonomy | tail -100 >> $DIAG_FILE
echo "" >> $DIAG_FILE

# Performance Metrics
echo "=== PERFORMANCE METRICS ===" >> $DIAG_FILE
autonomyctl metrics >> $DIAG_FILE
echo "" >> $DIAG_FILE

# GPS Information
echo "=== GPS INFORMATION ===" >> $DIAG_FILE
autonomyctl gps >> $DIAG_FILE
echo "" >> $DIAG_FILE

# Notification Status
echo "=== NOTIFICATION STATUS ===" >> $DIAG_FILE
autonomyctl notifications >> $DIAG_FILE
echo "" >> $DIAG_FILE

echo "Diagnostics saved to $DIAG_FILE"
echo "Please attach this file when reporting issues."
```

### Performance Monitoring

```bash
#!/bin/sh
# Save as /usr/local/bin/autonomy-performance.sh

echo "autonomy Performance Report"
echo "=========================="
echo "Generated: $(date)"
echo ""

# CPU Usage
echo "CPU Usage:"
top -n 1 | grep autonomy
echo ""

# Memory Usage
echo "Memory Usage:"
ps aux | grep autonomy | grep -v grep
echo ""

# System Load
echo "System Load:"
uptime
echo ""

# Disk Usage
echo "Disk Usage:"
df -h /var/log /var/lib/autonomy
echo ""

# Network Performance
echo "Network Performance:"
autonomyctl metrics --performance
echo ""

# Interface Health
echo "Interface Health:"
autonomyctl members --detailed
echo ""
```

## Recovery Procedures

### Complete System Reset

If all else fails, you can perform a complete system reset:

```bash
#!/bin/sh
# Save as /usr/local/bin/autonomy-reset.sh

echo "WARNING: This will reset autonomy to factory defaults!"
echo "All configuration and data will be lost!"
read -p "Are you sure? (y/N): " confirm

if [ "$confirm" != "y" ]; then
    echo "Reset cancelled."
    exit 1
fi

echo "Performing complete autonomy reset..."

# Stop services
/etc/init.d/autonomy stop
/etc/init.d/starwatch stop

# Remove configuration
rm -f /etc/config/autonomy

# Remove data
rm -rf /var/lib/autonomy

# Remove logs
rm -f /var/log/autonomy.log

# Remove lock files
rm -f /var/run/autonomy.pid
rm -f /var/run/starwatch.pid

# Restore default configuration
cp /etc/config/autonomy.example /etc/config/autonomy

# Restart services
/etc/init.d/autonomy start
/etc/init.d/starwatch start

echo "autonomy reset completed."
echo "Please reconfigure the system."
```

### Configuration Recovery

To recover from a backup configuration:

```bash
#!/bin/sh
# Save as /usr/local/bin/autonomy-recover.sh

BACKUP_FILE=$1

if [ -z "$BACKUP_FILE" ]; then
    echo "Usage: autonomy-recover.sh <backup_file>"
    exit 1
fi

if [ ! -f "$BACKUP_FILE" ]; then
    echo "Backup file not found: $BACKUP_FILE"
    exit 1
fi

echo "Recovering autonomy configuration from $BACKUP_FILE"

# Stop services
/etc/init.d/autonomy stop
/etc/init.d/starwatch stop

# Backup current configuration
cp /etc/config/autonomy /etc/config/autonomy.recovery.backup.$(date +%Y%m%d_%H%M%S)

# Restore from backup
cp "$BACKUP_FILE" /etc/config/autonomy

# Validate configuration
autonomyctl config validate

# Restart services
/etc/init.d/autonomy start
/etc/init.d/starwatch start

echo "Configuration recovery completed."
```

---

## Getting Help

If you're still experiencing issues after following this troubleshooting guide:

1. **Collect Diagnostics**: Run the comprehensive diagnostic script
2. **Check Documentation**: Review the [User Guide](USER_GUIDE.md) and [API Reference](API_REFERENCE.md)
3. **Search Issues**: Check existing issues in the project repository
4. **Create Issue**: Provide detailed information including:
   - System information
   - Configuration files
   - Diagnostic output
   - Steps to reproduce
   - Expected vs actual behavior

### Emergency Contacts

- **Critical Issues**: [Your emergency contact]
- **General Support**: [Your support contact]
- **Documentation**: [Your documentation URL]

---

**Last Updated**: 2025-01-20 15:30 UTC
**Version**: 1.0.0
