# autonomy Watchdog & Failsafe Operations Guide

## Overview

The autonomy Watchdog & Failsafe System is an independent sidecar process that monitors the health of the autonomy daemon and the overall system, providing automatic recovery and external notifications when issues are detected.

## Architecture

```
[RUTOS]
  autonomyd (main) ──writes──> /tmp/autonomyd.health
       ▲                                   ▲
       │                                   │
     procd (respawn)                 starwatch (sidecar)
                                       │   │
                         ┌──────────────┘   └──────────────┐
                         │                                 │
                 Self‑heal actions                 Webhook (HMAC)
                 (restart, hold‑down,             ↳ server creates GitHub issues
                 ubus/rpcd restart, logs)
```

## Components

### 1. Heartbeat Writer (autonomyd)
- **Location**: `/tmp/autonomyd.health`
- **Frequency**: Every 10 seconds
- **Format**: JSON with RFC3339Z timestamps
- **Content**: Uptime, version, status, memory usage, goroutines, device ID

### 2. Watchdog Sidecar (starwatch)
- **Location**: `/usr/sbin/starwatch`
- **Execution**: Every 6 minutes via cron
- **Dependencies**: None (independent of UCI/ubus)
- **Configuration**: `/etc/autonomy/watch.conf`

### 3. Crash Loop Detection
- **Location**: `/tmp/autonomyd.restarts`
- **Threshold**: ≥3 restarts in 10 minutes
- **Action**: 30-minute hold-down period
- **Notification**: Critical alert with diagnostic bundle

## Configuration

### File Locations
- **Watchdog Script**: `/usr/sbin/starwatch`
- **Cron Job**: `/etc/cron.d/starwatch`
- **Configuration**: `/etc/autonomy/watch.conf`
- **Heartbeat**: `/tmp/autonomyd.health`
- **Restart Log**: `/tmp/autonomyd.restarts`
- **Diagnostics**: `/tmp/autonomy_diagnostics/`
- **Queue**: `/tmp/starwatch.queue/`

### Key Parameters

#### Heartbeat Monitoring
```bash
HEARTBEAT_STALE_SEC=60              # Seconds before heartbeat is stale
CRASH_LOOP_THRESHOLD=3              # Restarts to trigger crash loop
CRASH_LOOP_WINDOW_SEC=600           # Time window (10 minutes)
COOLDOWN_MINUTES=30                 # Hold-down period
```

#### System Health Thresholds
```bash
DISK_WARN_PERCENT=95                # Warning threshold for overlay
DISK_CRIT_PERCENT=98                # Critical threshold for overlay
MIN_MEM_MB=32                       # Minimum available memory
LOAD_CRIT_PER_CORE=3.0             # Critical load per core
SLOW_UBUS_MS=1500                   # Slow ubus response threshold
```

#### Notification Settings
```bash
NOTIFY_COOLDOWN_MIN=15              # Minutes between notifications
REBOOT_ON_HARD_HANG=0               # Enable auto-reboot (0=disabled)
HANG_MINUTES_BEFORE_REBOOT=20       # Minutes before reboot
```

## Scenarios & Responses

### Scenario 1: Daemon Down or Stale Heartbeat
**Detection**: Process not running OR heartbeat >60s old
**Response**:
1. Record restart timestamp
2. Check for crash loop (≥3 restarts/10min)
3. If crash loop detected:
   - Engage 30-minute hold-down
   - Create diagnostic bundle
   - Send critical webhook
4. Restart daemon
5. Send warning webhook

### Scenario 2: System Degraded
**Detection**: High disk usage, low memory, high load
**Response**:
- **Disk ≥98%**: Critical alert + log pruning
- **Disk ≥95%**: Warning alert + log pruning
- **Memory <32MB**: Warning alert
- **Load >3.0/core**: Warning alert

### Scenario 3: ubus/rpcd Issues
**Detection**: ubus not responding OR slow response
**Response**:
1. Restart ubus and rpcd services
2. Send warning webhook
3. Monitor for slow responses (>1.5s)

### Scenario 4: Deep Hang (Optional)
**Detection**: Persistent high load + low memory
**Response**:
1. Start hang timer
2. After 20 minutes: reboot system
3. Send post-reboot notification

## Monitoring & Troubleshooting

### Health Check Commands
```bash
# Check daemon status
/etc/init.d/autonomy status

# Check heartbeat
cat /tmp/autonomyd.health

# Check restart history
cat /tmp/autonomyd.restarts

# Check watchdog logs
logread | grep starwatch

# Check diagnostic bundles
ls -la /tmp/autonomy_diagnostics/

# Check notification queue
ls -la /tmp/starwatch.queue/
```

### Log Analysis
```bash
# Watch watchdog activity
logread -f | grep starwatch

# Check for crash loops
grep "Crash loop detected" /var/log/messages

# Check for system degradation
grep "High disk usage\|Low memory\|High load" /var/log/messages
```

### Diagnostic Bundle Contents
Each diagnostic bundle (`autonomy_diag_YYYYMMDD_HHMMSS.tgz`) contains:
- System information (uname, memory, load)
- Disk usage and process list
- Network interfaces and routing
- MWAN3 status
- Recent logs (logread/dmesg)
- autonomy heartbeat and restart history
- Configuration files

## Performance Targets

### Detection Times
- **Daemon down**: ≤2 minutes (cron interval)
- **Stale heartbeat**: ≤1 minute (60s threshold)
- **Crash loop**: ≤1 minute after 3rd restart
- **System degraded**: ≤6 minutes (next cron run)

### Recovery Actions
- **Service restart**: Immediate
- **Hold-down period**: 30 minutes
- **Log pruning**: Immediate
- **System reboot**: 20 minutes (if enabled)

### Notification Delivery
- **Webhook**: ≤60 seconds (with retry/queue)
- **Queue flush**: On next successful webhook
- **Cooldown**: 15 minutes per issue type

## Troubleshooting Guide

### Daemon Won't Start
1. Check logs: `logread | grep autonomyd`
2. Check configuration: `cat /etc/config/autonomy`
3. Check disk space: `df -h /overlay`
4. Check memory: `free -h`
5. Manual start: `/usr/sbin/autonomyd -config /etc/config/autonomy`

### Watchdog Not Running
1. Check cron: `crontab -l`
2. Check script: `ls -la /usr/sbin/starwatch`
3. Manual test: `/usr/sbin/starwatch`
4. Check logs: `logread | grep starwatch`

### No Notifications
1. Check webhook URL: `cat /etc/autonomy/watch.conf`
2. Check network: `ping -c 3 your.server`
3. Check queue: `ls -la /tmp/starwatch.queue/`
4. Test webhook: `curl -X POST $WEBHOOK_URL`

### False Positives
1. Adjust thresholds in `/etc/autonomy/watch.conf`
2. Increase cooldown periods
3. Check system baseline performance
4. Review diagnostic bundles for patterns

### High Resource Usage
1. Check diagnostic bundle size: `du -sh /tmp/autonomy_diagnostics/`
2. Prune old bundles: `find /tmp/autonomy_diagnostics/ -mtime +7 -delete`
3. Check queue size: `du -sh /tmp/starwatch.queue/`
4. Review cron frequency (currently 6 minutes)

## Maintenance

### Regular Tasks
- **Weekly**: Review diagnostic bundles
- **Monthly**: Update thresholds based on system performance
- **Quarterly**: Review webhook server logs
- **Annually**: Update device IDs and firmware versions

### Emergency Procedures
1. **Disable watchdog**: Create `/etc/autonomy/DISABLE_SIDEcar`
2. **Manual restart**: `/etc/init.d/autonomy restart`
3. **Clear crash loop**: `rm -f /tmp/autonomyd.restarts`
4. **Emergency reboot**: `reboot`

### Backup & Recovery
- **Configuration**: Backup `/etc/autonomy/watch.conf`
- **Diagnostics**: Archive `/tmp/autonomy_diagnostics/`
- **Logs**: Backup `/var/log/messages`
- **State**: Backup `/tmp/autonomyd.health` and `/tmp/autonomyd.restarts`

## Integration with External Systems

### Webhook Payload Format
```json
{
  "device_id": "rutx50-van-01",
  "fw": "RUTX_R_00.07.17",
  "severity": "critical|warn|info",
  "scenario": "daemon_down|daemon_hung|crash_loop|system_degraded|slow|post_reboot",
  "overlay_pct": 96,
  "mem_avail_mb": 12,
  "load1": 5.3,
  "ubus_ok": false,
  "actions": ["restart", "hold_down"],
  "note": "heartbeat stale; restarted",
  "ts": 1737388800
}
```

### HMAC Authentication
- **Header**: `X-Starwatch-Signature: sha256=<hex>`
- **Algorithm**: HMAC-SHA256
- **Key**: `WATCH_SECRET` from configuration
- **Validation**: Server must validate signature before processing

### GitHub Issue Creation
The webhook server should:
1. Validate HMAC signature
2. Deduplicate by device_id + scenario + time bucket
3. Create GitHub issue with appropriate labels
4. Attach diagnostic bundle or link to object storage
5. Return 2xx status for success

## Security Considerations

### Access Control
- **Webhook secret**: Use strong, unique secrets
- **File permissions**: 644 for config, 755 for scripts
- **Network access**: Limit webhook server access
- **Logging**: Avoid logging sensitive data

### Data Privacy
- **Diagnostic bundles**: May contain sensitive information
- **Heartbeat data**: Contains system metrics
- **Configuration**: May contain API keys
- **Retention**: Automatic cleanup after 7 days

### Best Practices
1. Use HTTPS for webhook URLs
2. Rotate webhook secrets regularly
3. Monitor webhook server access logs
4. Review diagnostic bundles before sharing
5. Implement rate limiting on webhook server

## Support & Escalation

### First Level (Automated)
- Watchdog handles most issues automatically
- Diagnostic bundles provide context
- Webhook notifications alert operators

### Second Level (Manual)
- Review diagnostic bundles
- Check system logs and metrics
- Adjust thresholds if needed
- Contact support with bundle attached

### Third Level (Escalation)
- System administrator intervention
- Hardware diagnostics
- Firmware updates
- Configuration review

---

**Last Updated**: 2025-01-20
**Version**: 1.0
**Maintainer**: autonomy Development Team
