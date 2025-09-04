# autonomy User Guide

A comprehensive guide for installing, configuring, and using the autonomy intelligent multi-interface failover system on RutOS and OpenWrt routers.

## Table of Contents

1. [Overview](#overview)
2. [System Requirements](#system-requirements)
3. [Installation](#installation)
4. [Basic Configuration](#basic-configuration)
5. [Advanced Configuration](#advanced-configuration)
6. [Using autonomy](#using-autonomy)
7. [Monitoring and Alerts](#monitoring-and-alerts)
8. [Troubleshooting](#troubleshooting)
9. [FAQ](#faq)

## Overview

autonomy is an intelligent multi-interface failover system that automatically manages network connections between Starlink, cellular (multi-SIM), Wi-Fi, and LAN uplinks. It uses predictive algorithms to ensure you never experience network outages or degradation.

### Key Benefits

- **Zero Downtime**: Automatic failover between network interfaces
- **Intelligent Prediction**: Anticipates issues before they cause problems
- **Multi-Interface Support**: Starlink, Cellular, Wi-Fi, and LAN
- **Easy Management**: Simple configuration and monitoring
- **Resource Efficient**: Minimal impact on router performance

## System Requirements

### Hardware Requirements

- **Router**: RutOS or OpenWrt compatible router
- **RAM**: Minimum 64MB available RAM
- **Storage**: 16MB free space
- **CPU**: ARM Cortex-A7 or better (for optimal performance)

### Software Requirements

- **Operating System**: RutOS 21.02+ or OpenWrt 21.02+
- **Required Packages**:
  - `mwan3` - Multi-WAN load balancing
  - `ubus` - System bus (usually pre-installed)
  - `procd` - Process management (usually pre-installed)

### Network Interfaces

- **Starlink**: Dishy connected via Ethernet
- **Cellular**: USB modem or built-in cellular module
- **Wi-Fi**: Client mode or tethering capability
- **LAN**: Ethernet uplink (optional)

## Installation

### Method 1: Package Installation (Recommended)

```bash
# Update package lists
opkg update

# Install autonomy
opkg install autonomy

# Enable and start the service
/etc/init.d/autonomy enable
/etc/init.d/autonomy start
```

### Method 2: Manual Installation

```bash
# Download the latest release
wget https://github.com/your-repo/autonomy/releases/latest/download/autonomy_1.0.0_arm_cortex-a7.ipk

# Install the package
opkg install autonomy_1.0.0_arm_cortex-a7.ipk

# Enable and start the service
/etc/init.d/autonomy enable
/etc/init.d/autonomy start
```

### Method 3: Building from Source

```bash
# Install build dependencies
opkg install git gcc make

# Clone the repository
git clone https://github.com/your-repo/autonomy.git
cd autonomy

# Build for ARM
export CGO_ENABLED=0
GOOS=linux GOARCH=arm GOARM=7 go build -ldflags "-s -w" -o autonomyd ./cmd/autonomyd

# Install
cp autonomyd /usr/sbin/
chmod 755 /usr/sbin/autonomyd

# Copy configuration
cp configs/autonomy.example /etc/config/autonomy

# Copy init script
cp scripts/autonomy.init /etc/init.d/autonomy
chmod 755 /etc/init.d/autonomy
```

## Basic Configuration

### Initial Setup

1. **Create Basic Configuration**:

```bash
# Create configuration file
touch /etc/config/autonomy
```

2. **Add Basic Settings**:

```uci
config autonomy 'main'
    option enable '1'
    option use_mwan3 '1'
    option poll_interval_ms '1500'
    option predictive '1'
    option switch_margin '10'
    option log_level 'info'
```

3. **Configure Network Interfaces**:

```uci
config autonomy 'interfaces'
    option starlink 'wan'
    option cellular 'wwan0'
    option wifi 'wlan0'
    option lan 'eth0'
```

### Interface-Specific Configuration

#### Starlink Configuration

```uci
config autonomy 'starlink'
    option enabled '1'
    option priority '100'
    option health_check_interval '30'
    option obstruction_threshold '10'
    option gps_enabled '1'
```

#### Cellular Configuration

```uci
config autonomy 'cellular'
    option enabled '1'
    option priority '80'
    option signal_threshold '-85'
    option data_limit_enabled '1'
    option data_limit_gb '50'
```

#### Wi-Fi Configuration

```uci
config autonomy 'wifi'
    option enabled '1'
    option priority '60'
    option rssi_threshold '-70'
    option channel_optimization '1'
```

## Advanced Configuration

### Predictive Failover Settings

```uci
config autonomy 'predictive'
    option enabled '1'
    option lookback_period '300'
    option trend_analysis '1'
    option pattern_recognition '1'
    option switch_margin '15'
```

### GPS and Location Services

```uci
config autonomy 'gps'
    option enabled '1'
    option source 'starlink'
    option update_interval '60'
    option movement_detection '1'
    option geofencing '1'
```

### Monitoring and Alerts

```uci
config autonomy 'monitoring'
    option health_check_interval '30'
    option performance_monitoring '1'
    option resource_usage_tracking '1'
    option alert_threshold '80'
```

### Notification Configuration

```uci
config autonomy 'notifications'
    option enabled '1'
    option email_enabled '1'
    option pushover_enabled '1'
    option webhook_enabled '1'
    option alert_level 'warning'
```

## Using autonomy

### Command Line Interface

autonomy provides a comprehensive command-line interface for monitoring and control:

#### Basic Commands

```bash
# Check overall status
autonomyctl status

# View all network members
autonomyctl members

# View detailed member information
autonomyctl members --detailed

# Check system health
autonomyctl health

# View telemetry data
autonomyctl telemetry
```

#### Advanced Commands

```bash
# Manual failover to specific interface
autonomyctl failover --interface starlink

# View GPS location and status
autonomyctl gps

# Check notification status
autonomyctl notifications

# Export configuration
autonomyctl config export

# Validate configuration
autonomyctl config validate
```

### Web Interface (LuCI)

If you have the LuCI web interface installed:

1. **Install LuCI App**:
```bash
opkg install luci-app-autonomy
```

2. **Access Interface**:
   - Open your browser and navigate to `http://your-router-ip`
   - Login with your router credentials
   - Navigate to **Services** → **autonomy**

3. **Available Features**:
   - Real-time status monitoring
   - Configuration management
   - Performance metrics
   - Alert management
   - System health overview

### Monitoring Dashboard

The web interface provides:

- **Network Status**: Real-time status of all interfaces
- **Health Scores**: Performance metrics for each interface
- **Failover History**: Log of recent failover events
- **Performance Metrics**: Detailed performance data
- **Alert Management**: Configure and manage notifications

## Monitoring and Alerts

### Real-Time Monitoring

autonomy provides comprehensive monitoring capabilities:

#### Health Metrics

- **Interface Health**: Overall health score (0-100)
- **Signal Strength**: RSSI for wireless interfaces
- **Latency**: Response time measurements
- **Packet Loss**: Connection quality metrics
- **Data Usage**: Cellular data consumption

#### Performance Metrics

- **CPU Usage**: System resource utilization
- **Memory Usage**: RAM consumption
- **Network Throughput**: Bandwidth utilization
- **Error Rates**: Connection error statistics

### Alert System

autonomy can send alerts via multiple channels:

#### Email Alerts

```uci
config autonomy 'email'
    option enabled '1'
    option smtp_server 'smtp.gmail.com'
    option smtp_port '587'
    option username 'your-email@gmail.com'
    option password 'your-app-password'
    option recipients 'admin@example.com'
```

#### Pushover Alerts

```uci
config autonomy 'pushover'
    option enabled '1'
    option api_token 'your-api-token'
    option user_key 'your-user-key'
```

#### Webhook Alerts

```uci
config autonomy 'webhook'
    option enabled '1'
    option url 'https://your-webhook-url.com/autonomy'
    option method 'POST'
    option headers 'Content-Type: application/json'
```

### Alert Levels

- **Info**: General information and status updates
- **Warning**: Potential issues detected
- **Critical**: Immediate attention required
- **Emergency**: System failure or outage

## Troubleshooting

### Common Issues

#### Service Won't Start

```bash
# Check service status
/etc/init.d/autonomy status

# View error logs
logread | grep autonomy

# Check configuration
autonomyctl config validate
```

**Common Solutions**:
- Verify configuration syntax
- Check required packages are installed
- Ensure proper permissions
- Verify network interface names

#### No Network Members Discovered

```bash
# Check mwan3 configuration
ubus call mwan3 status

# Verify interface discovery
autonomyctl members --debug

# Check network configuration
ip link show
```

**Common Solutions**:
- Verify mwan3 is properly configured
- Check interface names match configuration
- Ensure interfaces are enabled
- Verify network connectivity

#### GPS Not Working

```bash
# Check GPS status
autonomyctl gps

# Verify GPS hardware
ubus call system board

# Check GPS configuration
cat /etc/config/autonomy | grep gps
```

**Common Solutions**:
- Verify GPS hardware is connected
- Check GPS configuration settings
- Ensure Starlink dish is properly connected
- Verify GPS permissions

#### Notifications Not Sending

```bash
# Check notification status
autonomyctl notifications

# Test notification configuration
autonomyctl notifications test

# View notification logs
logread | grep notification
```

**Common Solutions**:
- Verify notification service credentials
- Check network connectivity
- Ensure notification services are enabled
- Verify alert thresholds are configured

### Debug Mode

Enable debug logging for detailed troubleshooting:

```bash
# Enable debug mode
uci set autonomy.main.log_level='debug'
uci commit autonomy

# Restart service
/etc/init.d/autonomy restart

# View debug logs
logread -f | grep autonomy
```

### Performance Issues

If you experience performance issues:

```bash
# Check system resources
autonomyctl health

# Monitor resource usage
top | grep autonomy

# Check memory usage
free -h

# View performance metrics
autonomyctl metrics
```

**Optimization Tips**:
- Increase poll interval for better performance
- Disable unused features
- Optimize notification frequency
- Monitor system resource usage

## FAQ

### General Questions

**Q: What is the difference between autonomy and mwan3?**
A: mwan3 provides basic load balancing and failover. autonomy adds intelligent prediction, advanced monitoring, and automated optimization.

**Q: Can I use autonomy without mwan3?**
A: No, autonomy requires mwan3 for interface management and failover coordination.

**Q: How much RAM does autonomy use?**
A: Typically 8-16MB RAM, depending on configuration and monitoring features enabled.

**Q: Does autonomy work with all routers?**
A: autonomy works with any RutOS or OpenWrt compatible router with sufficient resources.

### Configuration Questions

**Q: How do I change the failover priority?**
A: Use the `priority` option in interface-specific configuration sections. Higher numbers = higher priority.

**Q: Can I disable predictive failover?**
A: Yes, set `option predictive '0'` in the main configuration section.

**Q: How do I configure custom health checks?**
A: Use the `health_check_interval` and `health_check_method` options in interface configurations.

**Q: Can I use multiple cellular interfaces?**
A: Yes, autonomy supports multiple cellular interfaces with independent configuration.

### Troubleshooting Questions

**Q: Why isn't my Starlink interface being detected?**
A: Check that the Starlink dish is properly connected and the interface name matches your configuration.

**Q: How do I reset autonomy to factory defaults?**
A: Remove the configuration file and restart: `rm /etc/config/autonomy && /etc/init.d/autonomy restart`

**Q: Can I backup my autonomy configuration?**
A: Yes, use `autonomyctl config export > autonomy-backup.conf`

**Q: How do I update autonomy?**
A: Use `opkg update && opkg upgrade autonomy` or download the latest package manually.

### Advanced Questions

**Q: Can I integrate autonomy with external monitoring systems?**
A: Yes, autonomy provides webhook notifications and MQTT integration for external systems.

**Q: How does the predictive algorithm work?**
A: autonomy analyzes historical performance data and current metrics to predict potential issues before they occur.

**Q: Can I customize the health scoring algorithm?**
A: Yes, you can adjust scoring weights and thresholds in the configuration.

**Q: Does autonomy support IPv6?**
A: Yes, autonomy fully supports IPv6 networks and dual-stack configurations.

---

For additional support and documentation, visit:
- [Project Documentation](https://github.com/your-repo/autonomy/docs)
- [Configuration Reference](CONFIGURATION.md)
- [API Reference](API_REFERENCE.md)
- [Troubleshooting Guide](TROUBLESHOOTING.md)
