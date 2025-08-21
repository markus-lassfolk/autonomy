# Troubleshooting Guide

## Common Issues and Solutions

### Installation Issues

#### Binary Download Failures

**Problem**: grpcurl or jq download fails

```bash
Error: Failed to download grpcurl
```

**Solution**:

1. Check internet connectivity
2. Verify architecture (should be ARMv7 for RUTX50)
3. Manual download:

   ```bash
   wget https://github.com/fullstorydev/grpcurl/releases/download/v1.9.3/grpcurl_1.9.3_linux_armv7.tar.gz
   ```

#### Permission Errors

**Problem**: Permission denied when installing

```bash
Permission denied: /etc/hotplug.d/iface/99-pushover_notify
```

**Solution**:

```bash
# Run as root
sudo ./install.sh
# Or manually fix permissions
chmod 755 /etc/hotplug.d/iface/99-pushover_notify
```

### Configuration Issues

#### UCI Configuration Errors

**Problem**: mwan3 member not found

```bash
Warning: mwan3 member 'member1' not found
```

**Solution**:

1. Check existing members:

   ```bash
   uci show mwan3 | grep member
   ```

2. Update config.sh with correct member name
3. Configure mwan3 if needed:

   ```bash
   uci set mwan3.member1=member
   uci set mwan3.member1.interface='wan'
   uci commit mwan3
   ```

#### Invalid Thresholds

**Problem**: Monitor behaves unexpectedly

```bash
Warning: Zero thresholds may cause issues
```

**Solution**:

1. Review threshold values in config.sh
2. Use realistic values:

   ```bash
   PACKET_LOSS_THRESHOLD=0.05    # 5%
   OBSTRUCTION_THRESHOLD=0.001   # 0.1%
   LATENCY_THRESHOLD_MS=150      # 150ms
   ```

### Network Issues

#### Starlink API Unreachable

**Problem**: Cannot connect to Starlink API

```bash
ERROR: Failed to get data from API. Dish may be unreachable.
```

**Solution**:

1. Check Starlink is in Bypass Mode
2. Verify static route:

   ```bash
   ip route show | grep 192.168.100.1
   ```

3. Test connectivity:

   ```bash
   ping -c 1 192.168.100.1
   ```

4. Check firewall rules:

   ```bash
   iptables -L | grep 192.168.100.1
   ```

#### RUTOS API Issues

**Problem**: Cannot authenticate with RUTOS API

```bash
HTTP 401 Unauthorized
```

**Solution**:

1. Verify credentials in config.sh
2. Check API is enabled:

   ```bash
   uci show uhttpd | grep rpc
   ```

3. Test authentication:

   ```bash
   curl -u username:password http://your-router-ip/cgi-bin/luci/rpc/uci
   ```

### Notification Issues

#### Pushover Not Working

**Problem**: No notifications received

```bash
Warning: Pushover not configured, skipping notification
```

**Solution**:

1. Verify Pushover credentials in config.sh
2. Test notification:

   ```bash
   /etc/hotplug.d/iface/99-pushover_notify test
   ```

3. Check logs:

   ```bash
   tail -f /var/log/notifications.log
   ```

#### Rate Limiting Active

**Problem**: Too many notifications

```bash
Rate limit active for soft_failover (120s ago)
```

**Solution**:

1. This is normal behavior (prevents spam)
2. Adjust rate limit if needed in notification script
3. Check for underlying issues causing frequent failovers

### Monitoring Issues

#### High CPU Usage

**Problem**: System becomes slow

```bash
# Check process usage
top | grep starlink
```

**Solution**:

1. Reduce monitoring frequency in cron
2. Check for infinite loops in scripts
3. Monitor log file sizes:

   ```bash
   du -h /var/log/starlink_monitor_*.log
   ```

#### Memory Issues

**Problem**: Out of memory errors

```bash
Cannot allocate memory
```

**Solution**:

1. Enable log rotation
2. Clean up state files:

   ```bash
   rm -f /tmp/run/starlink_monitor.*
   ```

3. Check for memory leaks:

   ```bash
   free -h
   ```

### Failover Issues

#### Failover Not Triggering

**Problem**: Poor connection but no failover

```bash
DEBUG INFO: Loss Check: triggered=0, Obstruction Check: triggered=0, Latency Check: triggered=0
```

**Solution**:

1. Check threshold values are appropriate
2. Verify API data is being received
3. Review logs for errors:

   ```bash
   logread | grep StarlinkMonitor
   ```

#### Constant Flapping

**Problem**: Rapid switching between connections

```bash
STATE CHANGE: Quality is BELOW threshold
STATE CHANGE: Stability threshold met
```

**Solution**:

1. Increase stability checks required
2. Adjust thresholds to be less sensitive
3. Check for network interference

### Log Analysis

#### Common Log Patterns

**Normal Operation**:

```text
INFO: Current state: up, Stability: 0, Metric: 1
DEBUG: Metrics - Loss: 0.02, Obstruction: 0.000, Latency: 45ms
INFO: Connection quality remains good
```

**Quality Degradation**:

```text
WARN: Quality degraded below threshold: [High Latency: 200ms]
INFO: Performing soft failover - setting metric to 10
INFO: Soft failover completed successfully
```

**Recovery**:

```text
INFO: Quality recovered - stability check 3/5
INFO: Stability threshold met - performing failback
INFO: Soft failback completed successfully
```

#### Log Locations

- System logs: `logread | grep StarlinkMonitor`
- Daily logs: `/var/log/starlink_monitor_YYYY-MM-DD.log`
- Notifications: `/var/log/notifications.log`

### Performance Analysis

#### Monitoring Performance Data

```bash
# View recent performance
tail -20 /root/starlink_performance_log.csv

# Analyze with awk
awk -F',' '$3 > 100 { print $1, $3 }' /root/starlink_performance_log.csv
```

#### Threshold Optimization

1. Collect data for at least a week
2. Analyze patterns:

   ```bash
   # Find 95th percentile latency
   tail -n +2 /root/starlink_performance_log.csv | cut -d',' -f3 | sort -n | awk '{a[NR]=$0} END {print a[int(NR*0.95)]}'
   ```

3. Set thresholds above normal operation

### Recovery Procedures

#### System Recovery

1. Stop monitoring:

   ```bash
   crontab -r
   ```

2. Reset network:

   ```bash
   mwan3 restart
   /etc/init.d/network restart
   ```

3. Restart monitoring:

   ```bash
   crontab -e
   # Add cron entries back
   ```

#### Configuration Recovery

1. Restore from backup:

   ```bash
   cp /root/config.sh.backup /root/config.sh
   ```

2. Validate configuration:

   ```bash
   /root/starlink-monitor/scripts/validate-config.sh
   ```

### Emergency Procedures

#### Disable Monitoring

```bash
# Stop all monitoring
crontab -r
# Or remove specific entries
crontab -e
# Comment out starlink lines
```

#### Reset to Defaults

```bash
# Reset mwan3 metrics
uci set mwan3.member1.metric='1'
uci commit mwan3
mwan3 restart
```

### Getting Help

#### Debug Information Collection

```bash
# System info
uname -a
cat /etc/openwrt_version

# Network config
uci show network | grep -E "(wan|interface)"
uci show mwan3 | grep -E "(member|metric)"

# Current status
logread | grep -E "(StarlinkMonitor|PushoverNotifier)" | tail -20
```

#### Support Channels

1. Check [GitHub Issues](https://github.com/markus-lassfolk/autonomy/issues)
2. Create new issue with debug information
3. Include system specifications and error logs

### Prevention

#### Regular Maintenance

1. Monitor system health weekly
2. Review logs for errors
3. Update thresholds based on performance data
4. Keep system updated

#### Backup Strategy

1. Backup configuration files
2. Export mwan3 configuration
3. Document custom changes
4. Test recovery procedures

---

**Remember**: Most issues are related to configuration or network setup. Always start with the basics before diving into
complex troubleshooting.
