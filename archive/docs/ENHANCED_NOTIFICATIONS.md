# Enhanced Pushover Notification System

<!-- Version: 2.6.0 | Updated: 2025-07-24 -->

**Version:** 2.6.0 | **Updated:** 2025-07-24

## Overview

The system maintenance script now provides real-time Pushover notifications for every maintenance action,
giving you complete visibility into your RUTX50's health.

## Notification Types

### ✅ FIXED Notifications

- **When**: A problem is successfully resolved
- **Priority**: Normal (0)
- **Sound**: Magic
- **Example**: "✅ Created missing /var/lock directory"
- **Details**: Shows the exact solution that was applied

### ❌ FAILED Notifications

- **When**: A fix attempt fails
- **Priority**: High (1) - requires attention
- **Sound**: Siren
- **Example**: "❌ Fix Failed: Failed to create /var/lock directory"
- **Details**: Shows what was attempted and suggests manual intervention

### 🚨 CRITICAL Notifications

- **When**: Critical system issues are detected
- **Priority**: Emergency (2) - immediate attention required
- **Sound**: Alien
- **Example**: "🚨 CRITICAL: System reboot scheduled"
- **Details**: Indicates immediate attention is required

### ⚠️ FOUND Notifications (Optional)

- **When**: Issues are detected but not yet fixed
- **Priority**: Low (-1) or Normal (0)
- **Sound**: Pushover
- **Example**: "⚠️ Issue Detected: High memory usage detected"
- **Details**: Shows what needs attention

## Configuration Options

### Basic Configuration (config.template.sh)

```bash
# Notification control (recommended for most users)
MAINTENANCE_NOTIFY_ON_FIXES="true"        # Get notified when issues are fixed ✅
MAINTENANCE_NOTIFY_ON_FAILURES="true"     # Get notified when fixes fail ❌
MAINTENANCE_NOTIFY_ON_CRITICAL="true"     # Get notified of critical issues 🚨
MAINTENANCE_NOTIFY_ON_FOUND="false"       # Don't get notified of every issue found (reduces noise)

# Timing and limits
MAINTENANCE_CRITICAL_THRESHOLD=1          # Send critical alerts immediately
MAINTENANCE_NOTIFICATION_COOLDOWN=1800    # 30 minutes between critical summaries
MAINTENANCE_MAX_NOTIFICATIONS_PER_RUN=10  # Maximum notifications per maintenance run

# Priorities (Pushover priority levels)
MAINTENANCE_PRIORITY_FIXED=0              # Normal priority for fixes
MAINTENANCE_PRIORITY_FAILED=1             # High priority for failures
MAINTENANCE_PRIORITY_CRITICAL=2           # Emergency priority for critical issues
MAINTENANCE_PRIORITY_FOUND=0              # Normal priority for found issues
```

### Advanced Configuration (config.advanced.template.sh)

```bash
# More aggressive monitoring for power users
MAINTENANCE_NOTIFY_ON_FIXES=true        # Get notified of all fixes
MAINTENANCE_NOTIFY_ON_FAILURES=true     # Get notified of all failures
MAINTENANCE_NOTIFY_ON_CRITICAL=true     # Get notified of all critical issues
MAINTENANCE_NOTIFY_ON_FOUND=true        # Get notified of issues found (more verbose)

# More frequent updates
MAINTENANCE_CRITICAL_THRESHOLD=1          # Immediate critical alerts
MAINTENANCE_NOTIFICATION_COOLDOWN=900     # 15 minutes between summaries
MAINTENANCE_MAX_NOTIFICATIONS_PER_RUN=15  # Higher notification limit

# Granular priorities
MAINTENANCE_PRIORITY_FOUND=-1             # Low priority for found issues (reduces notification interruptions)
```

## Notification Examples

### Successful Database Fix

```text
Title: ✅ System Fixed - RUTX50
Message: ✅ Fixed database optimization spam

         Fixed at: 2025-07-20 15:30:22
         Solution: Reset databases: uci.db logd.db. Restarted: logd ubus. Backup: /tmp/maintenance_backup_20250720_153022
```

### Failed Service Restart

```text
Title: ❌ Fix Failed - RUTX50
Message: ❌ Fix Failed: Failed to restart lighttpd service

         Failed at: 2025-07-20 15:35:10
         Attempted: /etc/init.d/lighttpd restart

         Manual intervention may be required.
```

### Critical System Issue

```text
Title: 🚨 CRITICAL Issue - RUTX50
Message: 🚨 CRITICAL: System reboot scheduled

         Detected at: 2025-07-20 15:40:55
         Action: Persistent critical issues: 3

         IMMEDIATE ATTENTION REQUIRED!
```

## Smart Features

### 📱 Notification Management

- **Rate Limiting**: Maximum notifications per run prevents spam
- **Priority Levels**: Important issues get emergency priority with retry
- **Cooldown**: Critical summary notifications respect cooldown periods
- **Smart Sounds**: Different sounds for different issue types

### 🔄 High Priority Handling

- **Emergency Priority** (2): Retries every minute for 1 hour
- **High Priority** (1): Requires acknowledgment
- **Normal Priority** (0): Standard delivery
- **Low Priority** (-1): Quiet delivery

### 📊 Logging Integration

- All notifications are logged to system log
- Success/failure of notification delivery is tracked
- Notification counts are tracked per maintenance run

## Recommended Settings

### Conservative (Minimal Notifications)

```bash
MAINTENANCE_NOTIFY_ON_FIXES="false"       # Don't notify on routine fixes
MAINTENANCE_NOTIFY_ON_FAILURES="true"     # Only notify on failures
MAINTENANCE_NOTIFY_ON_CRITICAL="true"     # Always notify on critical issues
MAINTENANCE_NOTIFY_ON_FOUND="false"       # Don't notify on found issues
MAINTENANCE_MAX_NOTIFICATIONS_PER_RUN=5   # Limit notifications
```

### Balanced (Recommended)

```bash
MAINTENANCE_NOTIFY_ON_FIXES="true"        # Know when issues are fixed
MAINTENANCE_NOTIFY_ON_FAILURES="true"     # Know when fixes fail
MAINTENANCE_NOTIFY_ON_CRITICAL="true"     # Know about critical issues
MAINTENANCE_NOTIFY_ON_FOUND="false"       # Don't get notified of every scan result
MAINTENANCE_MAX_NOTIFICATIONS_PER_RUN=10  # Reasonable limit
```

### Comprehensive (Power Users)

```bash
MAINTENANCE_NOTIFY_ON_FIXES="true"        # Know about all fixes
MAINTENANCE_NOTIFY_ON_FAILURES="true"     # Know about all failures
MAINTENANCE_NOTIFY_ON_CRITICAL="true"     # Know about all critical issues
MAINTENANCE_NOTIFY_ON_FOUND="true"        # Know about all issues found
MAINTENANCE_MAX_NOTIFICATIONS_PER_RUN=15  # Higher limit for comprehensive monitoring
```

## Troubleshooting

### No Notifications Received

1. Check `MAINTENANCE_PUSHOVER_ENABLED="true"`
2. Verify `PUSHOVER_TOKEN` and `PUSHOVER_USER` are set
3. Check notification limits: `MAINTENANCE_MAX_NOTIFICATIONS_PER_RUN`
4. Review logs: `/var/log/system-maintenance.log`

### Too Many Notifications

1. Set `MAINTENANCE_NOTIFY_ON_FOUND="false"`
2. Reduce `MAINTENANCE_MAX_NOTIFICATIONS_PER_RUN`
3. Use conservative notification settings
4. Increase `MAINTENANCE_NOTIFICATION_COOLDOWN`

### Missing Critical Notifications

1. Verify `MAINTENANCE_NOTIFY_ON_CRITICAL="true"`
2. Check `MAINTENANCE_PRIORITY_CRITICAL=2` for emergency priority
3. Ensure `MAINTENANCE_CRITICAL_THRESHOLD=1` for immediate alerts

This enhanced notification system keeps you informed about your system's health in real-time while
preventing notification spam through intelligent rate limiting and priority management.
