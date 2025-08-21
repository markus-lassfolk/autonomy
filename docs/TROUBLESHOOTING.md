# Troubleshooting Guide

## Common Issues and Solutions

### Service Won't Start

**Symptoms:**
- Service fails to start with error messages
- `autonomy` process not running
- No ubus API available

**Diagnosis:**
```bash
# Check service status
/etc/init.d/autonomy status

# Check logs
logread | grep autonomy

# Check configuration
uci show autonomy
```

**Solutions:**

1. **Configuration Error:**
   ```bash
   # Reset configuration
   uci delete autonomy
   uci commit autonomy
   /etc/init.d/autonomy restart
   ```

2. **Missing Dependencies:**
   ```bash
   # Install required packages
   opkg update
   opkg install uci ubus mwan3
   ```

3. **Permission Issues:**
   ```bash
   # Fix permissions
   chmod +x /usr/sbin/autonomyd
   chmod +x /etc/init.d/autonomy
   ```

### Starlink Integration Issues

**Symptoms:**
- Starlink not detected
- API connection failures
- No obstruction data

**Diagnosis:**
```bash
# Check Starlink API connectivity
ubus call autonomy interfaces

# Test API endpoint
curl -s http://192.168.100.1:9200/status
```

**Solutions:**

1. **API Endpoint Unreachable:**
   ```bash
   # Verify Starlink IP
   ping 192.168.100.1
   
   # Update API endpoint if needed
   uci set autonomy.starlink.api_endpoint="grpc://192.168.100.1:9200"
   uci commit autonomy
   ```

2. **Authentication Issues:**
   ```bash
   # Check Starlink authentication
   uci set autonomy.starlink.auth_enabled=0
   uci commit autonomy
   ```

3. **Network Connectivity:**
   ```bash
   # Ensure proper network setup
   ip route add 192.168.100.0/24 dev starlink
   ```

### Cellular Failover Issues

**Symptoms:**
- Cellular interface not detected
- Failover not working
- Poor signal quality

**Diagnosis:**
```bash
# Check cellular interface
ip link show wwan0

# Check signal strength
ubus call autonomy interfaces

# Check cellular configuration
uci show autonomy.cellular
```

**Solutions:**

1. **Interface Not Found:**
   ```bash
   # Update interface name
   uci set autonomy.cellular.interface="wwan1"
   uci commit autonomy
   ```

2. **APN Configuration:**
   ```bash
   # Set correct APN
   uci set autonomy.cellular.apn="internet"
   uci set autonomy.cellular.username=""
   uci set autonomy.cellular.password=""
   uci commit autonomy
   ```

3. **Signal Quality:**
   ```bash
   # Adjust thresholds
   uci set autonomy.cellular.signal_threshold=-90
   uci commit autonomy
   ```

### GPS Issues

**Symptoms:**
- No GPS data
- Inaccurate location
- GPS source not working

**Diagnosis:**
```bash
# Check GPS status
ubus call autonomy gps location

# Check GPS configuration
uci show autonomy.gps
```

**Solutions:**

1. **GPS Source Configuration:**
   ```bash
   # Set GPS source
   uci set autonomy.gps.source="starlink"
   uci commit autonomy
   ```

2. **External GPS Device:**
   ```bash
   # Configure external GPS
   uci set autonomy.gps.source="external"
   uci set autonomy.gps.device="/dev/ttyUSB0"
   uci set autonomy.gps.baud_rate="9600"
   uci commit autonomy
   ```

3. **Cellular Fallback:**
   ```bash
   # Enable cellular fallback
   uci set autonomy.gps.fallback_to_cellular=1
   uci commit autonomy
   ```

### Performance Issues

**Symptoms:**
- High CPU usage
- High memory usage
- Slow response times

**Diagnosis:**
```bash
# Check performance metrics
ubus call autonomy metrics

# Check system resources
top
free -m
```

**Solutions:**

1. **Reduce Polling Frequency:**
   ```bash
   # Increase polling intervals
   uci set autonomy.config.poll_interval_ms=3000
   uci set autonomy.starlink.health_check_interval=60
   uci commit autonomy
   ```

2. **Memory Optimization:**
   ```bash
   # Reduce memory usage
   uci set autonomy.config.max_memory_mb=32
   uci set autonomy.telemetry.buffer_size=500
   uci commit autonomy
   ```

3. **Disable Unused Features:**
   ```bash
   # Disable unused monitoring
   uci set autonomy.wifi.enabled=0
   uci set autonomy.lan.enabled=0
   uci commit autonomy
   ```

### Notification Issues

**Symptoms:**
- Notifications not sent
- Webhook failures
- Rate limiting issues

**Diagnosis:**
```bash
# Check notification status
ubus call autonomy notifications status

# Test webhook
curl -X POST https://your-webhook.com/autonomy
```

**Solutions:**

1. **Webhook Configuration:**
   ```bash
   # Update webhook URL
   uci set autonomy.notifications.webhook_url="https://your-webhook.com/autonomy"
   uci set autonomy.notifications.webhook_timeout_ms=10000
   uci commit autonomy
   ```

2. **Rate Limiting:**
   ```bash
   # Adjust rate limits
   uci set autonomy.notifications.rate_limit_minutes=10
   uci set autonomy.notifications.max_notifications_per_hour=6
   uci commit autonomy
   ```

3. **Authentication:**
   ```bash
   # Add webhook authentication
   uci set autonomy.notifications.webhook_auth="Bearer your-token"
   uci commit autonomy
   ```

### Network Interface Issues

**Symptoms:**
- Interface not detected
- Wrong interface type
- Interface metrics not updating

**Diagnosis:**
```bash
# List all interfaces
ip link show

# Check interface status
ubus call autonomy interfaces

# Check mwan3 status
ubus call mwan3 status
```

**Solutions:**

1. **Interface Detection:**
   ```bash
   # Restart interface detection
   /etc/init.d/autonomy restart
   
   # Check interface configuration
   uci show network
   ```

2. **mwan3 Integration:**
   ```bash
   # Ensure mwan3 is running
   /etc/init.d/mwan3 restart
   
   # Check mwan3 configuration
   uci show mwan3
   ```

3. **Interface Metrics:**
   ```bash
   # Force metrics update
   ubus call autonomy control refresh
   ```

## Debug Mode

Enable debug mode for detailed logging:

```bash
# Enable debug logging
uci set autonomy.config.debug=1
uci set autonomy.config.log_level=debug
uci commit autonomy

# Restart service
/etc/init.d/autonomy restart

# Monitor logs
logread -f | grep autonomy
```

## Performance Monitoring

Monitor system performance:

```bash
# Check CPU usage
top -p $(pgrep autonomyd)

# Check memory usage
ps aux | grep autonomyd

# Check disk usage
df -h /var/lib/autonomy

# Check network usage
iftop -i starlink
```

## Log Analysis

Analyze logs for issues:

```bash
# Get recent logs
logread | grep autonomy | tail -50

# Search for errors
logread | grep autonomy | grep -i error

# Search for warnings
logread | grep autonomy | grep -i warn

# Monitor logs in real-time
logread -f | grep autonomy
```

## Configuration Validation

Validate configuration:

```bash
# Validate UCI configuration
uci show autonomy

# Test configuration
autonomyctl test-config

# Validate API endpoints
ubus call autonomy status
ubus call autonomy interfaces
```

## Recovery Procedures

### Complete Reset

If all else fails, perform a complete reset:

```bash
# Stop service
/etc/init.d/autonomy stop

# Remove configuration
uci delete autonomy
uci commit autonomy

# Remove data
rm -rf /var/lib/autonomy/*

# Reinstall package
opkg reinstall autonomy

# Start service
/etc/init.d/autonomy start
```

### Backup and Restore

```bash
# Backup configuration
cp /etc/config/autonomy /etc/config/autonomy.backup

# Backup data
tar -czf /tmp/autonomy-data-backup.tar.gz /var/lib/autonomy/

# Restore configuration
cp /etc/config/autonomy.backup /etc/config/autonomy

# Restore data
tar -xzf /tmp/autonomy-data-backup.tar.gz -C /
```

## Getting Help

If you're still experiencing issues:

1. **Check the logs** for specific error messages
2. **Review the configuration** for incorrect settings
3. **Test individual components** to isolate the issue
4. **Check system resources** for performance bottlenecks
5. **Consult the documentation** for configuration examples

### Useful Commands

```bash
# System information
uname -a
cat /etc/openwrt_release

# Network information
ip route show
ip link show

# Service status
/etc/init.d/autonomy status
ubus call autonomy status

# Configuration
uci show autonomy
autonomyctl show-config
```

## Next Steps

- [Configuration Guide](configuration.md)
- [API Reference](api.md)
- [Performance Tuning](performance.md)
