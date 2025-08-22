# RUTX50 Production Deployment Guide

**Version:** 3.0.0 | **Updated:** 2025-08-22

This guide provides step-by-step instructions for deploying the Autonomy networking system on a RUTX50 router with comprehensive Starlink failover, GPS integration, and intelligent monitoring.

## 📋 Pre-deployment Checklist

### Hardware Configuration Verified

- ✅ **RUTX50** router with latest firmware (`RUTX_R_00.07.15.2` or newer)
- ✅ **Starlink dish** in Bypass Mode connected to WAN port
- ✅ **Dual SIM setup** (Primary: Telia, Backup: Roaming SIM)
- ✅ **GPS enabled** and functioning
- ✅ **Network interfaces** properly configured
- ✅ **Adequate storage** (minimum 50MB free space)

### Current Network Setup Analysis

Your configuration shows the following setup that we'll enhance:

```bash
# Interface Priority (from your mwan3 config)
member1 (wan)        - Starlink     - metric=1 (highest priority)
member3 (mob1s1a1)   - SIM Telia    - metric=2 (primary cellular)
member4 (mob1s2a1)   - SIM Roaming  - metric=4 (backup cellular)
```

## 🚀 Enhanced Deployment Steps

### 1. Install Autonomy System

```bash
# Download the Autonomy binary for ARM
curl -fL https://github.com/markus-lassfolk/autonomy/releases/latest/download/autonomy-armv7.tar.gz -o autonomy-armv7.tar.gz

# Extract to /usr/local/bin
tar -xzf autonomy-armv7.tar.gz -C /usr/local/bin/
chmod +x /usr/local/bin/autonomysysmgmt
chmod +x /usr/local/bin/autonomyctl

# Create system directories
mkdir -p /etc/autonomy
mkdir -p /var/log/autonomy
mkdir -p /var/lib/autonomy
```

### 2. Configure UCI Integration

```bash
# Create UCI configuration
cat > /etc/config/autonomy << 'EOF'
config autonomy 'main'
    option enabled '1'
    option log_level 'info'
    option data_dir '/var/lib/autonomy'
    option log_dir '/var/log/autonomy'

config autonomy 'starlink'
    option enabled '1'
    option api_endpoint 'https://192.168.100.1:9200'
    option oauth_token 'your-starlink-token'
    option health_check_interval '30'
    option failover_threshold '3'

config autonomy 'cellular'
    option primary_interface 'mob1s1a1'
    option backup_interface 'mob1s2a1'
    option signal_threshold '-85'
    option data_limit_check '1'

config autonomy 'gps'
    option enabled '1'
    option sources 'rutos,starlink,cellular'
    option cache_ttl '300'
    option accuracy_threshold '50'

config autonomy 'notifications'
    option pushover_token 'your-pushover-token'
    option pushover_user 'your-pushover-user'
    option enable_alerts '1'
    option alert_threshold 'warning'
EOF

# Commit UCI configuration
uci commit autonomy
```

### 3. Enhanced mwan3 Configuration

Optimize the health checks for better failover performance:

```bash
# Enhanced Starlink monitoring
uci delete mwan3.@condition[1]  # Remove existing wan condition
uci add mwan3 condition
uci set mwan3.@condition[-1].interface='wan'
uci set mwan3.@condition[-1].track_method='ping'
uci set mwan3.@condition[-1].track_ip='1.0.0.1' '8.8.8.8' '1.1.1.1'
uci set mwan3.@condition[-1].reliability='2'      # Require 2/3 to succeed
uci set mwan3.@condition[-1].timeout='1'
uci set mwan3.@condition[-1].interval='5'         # Check every 5 seconds
uci set mwan3.@condition[-1].count='3'
uci set mwan3.@condition[-1].family='ipv4'
uci set mwan3.@condition[-1].up='2'               # 2 successful checks to mark up
uci set mwan3.@condition[-1].down='3'             # 3 failed checks to mark down

# Enhanced recovery settings
uci set mwan3.wan.recovery_wait='15'              # Wait 15s before recovery

# Commit changes
uci commit mwan3
mwan3 restart
```

### 4. Create Systemd Service

```bash
# Create systemd service file
cat > /etc/systemd/system/autonomy.service << 'EOF'
[Unit]
Description=Autonomy Networking System
After=network.target
Wants=network.target

[Service]
Type=simple
User=root
ExecStart=/usr/local/bin/autonomysysmgmt
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal
SyslogIdentifier=autonomy

# Environment variables
Environment=AUTONOMY_CONFIG=/etc/config/autonomy
Environment=AUTONOMY_LOG_LEVEL=info

[Install]
WantedBy=multi-user.target
EOF

# Enable and start service
systemctl daemon-reload
systemctl enable autonomy
systemctl start autonomy
```

### 5. GPS Integration Setup

Configure multi-source GPS integration:

```bash
# Enable GPS in UCI
uci set system.gps.enabled='1'
uci set system.gps.device='/dev/ttyUSB0'
uci set system.gps.baudrate='9600'
uci commit system

# Configure GPS sources in Autonomy
uci set autonomy.gps.rutos_enabled='1'
uci set autonomy.gps.starlink_enabled='1'
uci set autonomy.gps.cellular_enabled='1'
uci set autonomy.gps.fusion_algorithm='weighted_average'
uci commit autonomy
```

### 6. Cellular Monitoring Configuration

```bash
# Configure cellular interfaces
uci set autonomy.cellular.primary_operator='Telia'
uci set autonomy.cellular.backup_operator='Roaming'
uci set autonomy.cellular.signal_monitoring='1'
uci set autonomy.cellular.data_usage_tracking='1'
uci set autonomy.cellular.roaming_detection='1'
uci commit autonomy
```

### 7. Notification Setup

```bash
# Configure Pushover notifications
uci set autonomy.notifications.pushover_token='your-token-here'
uci set autonomy.notifications.pushover_user='your-user-key'
uci set autonomy.notifications.alert_levels='critical,warning,info'
uci set autonomy.notifications.enable_location_alerts='1'
uci commit autonomy
```

## 🔧 Advanced Configuration

### Decision Engine Tuning

```bash
# Configure hybrid weight system
uci set autonomy.decision.performance_weight='0.3'
uci set autonomy.decision.location_weight='0.2'
uci set autonomy.decision.cost_weight='0.2'
uci set autonomy.decision.reliability_weight='0.3'
uci set autonomy.decision.ml_enabled='1'
uci commit autonomy
```

### Predictive Failover

```bash
# Enable predictive features
uci set autonomy.predictive.enabled='1'
uci set autonomy.predictive.obstruction_detection='1'
uci set autonomy.predictive.trend_analysis='1'
uci set autonomy.predictive.warning_window='300'
uci commit autonomy
```

### Intelligent Caching

```bash
# Configure location caching
uci set autonomy.cache.location_ttl='300'
uci set autonomy.cache.cell_tower_ttl='3600'
uci set autonomy.cache.predictive_loading='1'
uci set autonomy.cache.geographic_clustering='1'
uci commit autonomy
```

## 📊 Monitoring and Verification

### Check Service Status

```bash
# Check service status
systemctl status autonomy

# Check logs
journalctl -u autonomy -f

# Check UCI configuration
uci show autonomy
```

### Test System Components

```bash
# Test Starlink integration
autonomyctl starlink status

# Test cellular monitoring
autonomyctl cellular status

# Test GPS location
autonomyctl gps location

# Test decision engine
autonomyctl decision status
```

### Verify Network Failover

```bash
# Monitor failover events
autonomyctl events --follow

# Check mwan3 status
mwan3 status

# Test manual failover
autonomyctl failover trigger --reason="test"
```

## 🛠️ Troubleshooting

### Common Issues

1. **Service won't start**: Check logs with `journalctl -u autonomy`
2. **GPS not working**: Verify device path and permissions
3. **Starlink API errors**: Check OAuth token and network connectivity
4. **Cellular monitoring issues**: Verify interface names and permissions

### Debug Mode

```bash
# Enable debug logging
uci set autonomy.main.log_level='debug'
uci commit autonomy
systemctl restart autonomy

# Monitor debug output
journalctl -u autonomy -f --no-pager
```

### Performance Monitoring

```bash
# Check resource usage
autonomyctl metrics system

# Monitor memory usage
autonomyctl metrics memory

# Check decision engine performance
autonomyctl metrics decision
```

## 📈 Performance Optimization

### Memory Usage

- **Target**: <25MB RAM usage
- **Optimization**: Enable ring buffer storage
- **Monitoring**: Use `autonomyctl metrics memory`

### CPU Usage

- **Target**: <5% CPU on low-end ARM
- **Optimization**: Adjust monitoring intervals
- **Tuning**: Use adaptive sampling

### Storage

- **Logs**: Rotate logs automatically
- **Cache**: Limit cache size and TTL
- **Telemetry**: Use efficient storage formats

## 🔒 Security Considerations

1. **API Tokens**: Store securely in UCI configuration
2. **Network Access**: Limit to necessary ports only
3. **Logging**: Avoid logging sensitive information
4. **Updates**: Regular security updates and patches

## 📞 Support

For issues and support:
- Check logs: `journalctl -u autonomy`
- System status: `autonomyctl status`
- Configuration: `uci show autonomy`
- Documentation: `/usr/share/doc/autonomy/`
