# System Maintenance Schedule and Actions

## ⏰ **Maintenance Schedule**

### **Primary Schedule**
- **Frequency**: Every **5 minutes** (default)
- **Configurable via**: UCI `autonomy.system_management.check_interval`
- **Maximum execution time**: 30 seconds per cycle
- **Auto-fix enabled**: Yes (can be disabled)

### **Integration with Main Loop**
The maintenance runs alongside:
- **Decision Engine**: Every 5 seconds (metrics collection, failover decisions)
- **Discovery**: Every 30 seconds (interface discovery)
- **Cleanup**: Every 5 minutes (telemetry cleanup)

## 🔍 **What the System Monitors and Fixes**

### **1. Overlay Space Management**
**What it monitors:**
- Overlay filesystem usage percentage
- Available space in `/overlay`
- Critical space thresholds

**Actions taken:**
- **Warning threshold (80%)**: Log warnings, send notifications
- **Critical threshold (90%)**: 
  - Clean up old log files
  - Remove temporary files
  - Clean up old database entries
  - Restart services if needed
- **Cleanup retention**: Remove files older than 7 days

**Notification example:**
```
⚠️ Overlay Space Critical
Usage: 92% (23.4 MB used of 25.6 MB)
Actions taken:
• Cleaned 15 old log files (2.1 MB freed)
• Removed temp files (0.8 MB freed)
```

### **2. Service Watchdog**
**What it monitors:**
- Critical services status: `mwan3`, `network`, `firewall`, `dnsmasq`
- Service response times
- Service crash detection

**Actions taken:**
- **Service down**: Restart the service
- **Service hanging**: Kill and restart
- **Multiple failures**: Escalate to system reboot (if configured)

**Notification example:**
```
🔧 Service Watchdog Alert
Service: mwan3
Status: Restarted (was not responding)
Downtime: 45 seconds
```

### **3. Log Flood Detection**
**What it monitors:**
- Log entry rates (entries per hour)
- Specific error patterns
- Disk space consumption by logs

**Actions taken:**
- **Flood detected**: Rotate logs immediately
- **Pattern matching**: Identify and suppress spam
- **Disk protection**: Archive or compress large logs

**Thresholds:**
- **Warning**: >1000 entries/hour
- **Critical**: >5000 entries/hour

### **4. Time Drift Correction**
**What it monitors:**
- System time vs NTP servers
- Time drift magnitude
- NTP synchronization status

**Actions taken:**
- **Small drift (<30s)**: Gradual adjustment
- **Large drift (>30s)**: Force NTP sync
- **NTP failure**: Try alternative servers
- **Critical drift**: System time reset

### **5. Network Interface Stabilization**
**What it monitors:**
- Interface flapping (up/down cycles)
- Connection stability
- Interface error rates

**Actions taken:**
- **Flapping detected**: Increase interface timeouts
- **Persistent issues**: Restart network service
- **Hardware issues**: Log for manual intervention

**Flapping threshold**: >10 state changes per hour

### **6. Starlink Script Health**
**What it monitors:**
- Starlink monitoring script activity
- API response times
- Data collection success rates

**Actions taken:**
- **Script inactive**: Restart monitoring
- **API failures**: Reset connections
- **Data corruption**: Clear cache and restart

### **7. Database Health**
**What it monitors:**
- Database file integrity
- Database size and growth
- Query performance
- Corruption detection

**Actions taken:**
- **Corruption detected**: Rebuild database
- **Size issues**: Archive old data
- **Performance**: Optimize queries, vacuum database
- **Age management**: Remove data older than configured limit

### **8. UCI Configuration Health** ⭐ **NEW**
**What it monitors:**
- UCI parse errors
- Corrupted configuration files
- Missing critical sections (`network`, `mwan3`, `system`, `firewall`)
- **Unwanted files**: `.backup`, `.tmp`, `.old`, editor temp files

**Actions taken:**
- **Parse errors**: 
  - Try `uci revert` and `uci commit`
  - Clear UCI cache
  - Reload configuration
- **Corrupted files**: Restore from backup
- **Missing sections**: Alert for manual intervention
- **Unwanted files**: Move to `/tmp/uci_unwanted_files/` with timestamp
- **Backup creation**: Automatic backup before any fixes

**Unwanted file patterns detected:**
```
.backup, .bak, .tmp, .temp, .old, .orig, .save
~, .swp, .swo, # (editor files)
Files with invalid UCI naming (dots, spaces, special chars)
```

## 📊 **Maintenance Statistics Tracking**

The system tracks:
- **Issues found**: Count per maintenance cycle
- **Issues fixed**: Successful repairs
- **Notifications sent**: Alert volume
- **Last check time**: Monitoring continuity
- **Execution time**: Performance monitoring

## 🔔 **Notification System**

### **Notification Triggers**
- **Critical issues found**: High priority alerts
- **Successful fixes**: Normal priority confirmations
- **Failed fixes**: High priority manual intervention needed
- **System health summaries**: Periodic status updates

### **Notification Examples**

**UCI Issue Fixed:**
```
🔧 UCI Configuration Maintenance
UCI maintenance completed:

📊 Issues found: 2
✅ Issues fixed: 2
💾 Backup created: /tmp/uci_emergency_backup_20250817_140532.tar.gz

🔍 Issues detected:
🔧 mwan3: Unwanted file 'mwan3.backup.' moved to backup
🔧 network: UCI parse error fixed
```

**Critical System Issue:**
```
🚨 System Maintenance Alert
Critical issues detected:

❌ Overlay space: 95% full
❌ Service mwan3: Not responding
✅ Cleaned 25 MB of old files
✅ Restarted mwan3 service

Manual intervention may be required.
```

## ⚙️ **Configuration Options**

### **Enable/Disable Components**
```bash
# Disable specific maintenance components
uci set autonomy.system_management.service_watchdog_enabled='0'
uci set autonomy.system_management.log_flood_enabled='0'
uci set autonomy.system_management.time_drift_enabled='0'
uci commit autonomy
```

### **Adjust Thresholds**
```bash
# Change overlay space thresholds
uci set autonomy.system_management.overlay_space_threshold='70'
uci set autonomy.system_management.overlay_critical_threshold='85'

# Change check interval (in seconds)
uci set autonomy.system_management.check_interval='300'  # 5 minutes
uci commit autonomy
```

### **Notification Settings**
```bash
# Configure notifications
uci set autonomy.system_management.notifications_enabled='1'
uci set autonomy.system_management.notify_on_fixes='1'
uci set autonomy.system_management.notify_on_critical='1'
uci commit autonomy
```

## 🛡️ **Safety Features**

### **Dry Run Mode**
- Test mode that detects issues but doesn't fix them
- Useful for testing maintenance logic
- Enabled via configuration

### **Execution Time Limits**
- Maximum 30 seconds per maintenance cycle
- Prevents maintenance from blocking main operations
- Timeout protection for stuck operations

### **Backup Before Changes**
- Automatic backups before any system changes
- UCI configuration backed up before repairs
- Rollback capability for failed fixes

### **Rate Limiting**
- Cooldown periods between notifications
- Maximum notifications per maintenance run
- Prevents notification spam

## 📈 **Monitoring the Maintenance System**

### **Log Entries**
The system logs all maintenance activities:
```
INFO System health check completed issues_found=2 issues_fixed=2 duration=1.2s
WARN UCI configuration errors detected health={"status":"error","errors":["Parse error"]}
INFO Fixed unwanted file file=/etc/config/mwan3.backup. backup=/tmp/uci_unwanted_files/mwan3.backup._20250817_140532
```

### **Health Check Status**
You can monitor maintenance health via:
- **Daemon logs**: Real-time maintenance activity
- **Pushover notifications**: Critical issues and fixes
- **UCI configuration**: `autonomy.system_management.*`

## 🎯 **Summary**

The system maintenance runs **every 5 minutes** and provides comprehensive monitoring and automatic repair of:

1. **System resources** (disk space, memory)
2. **Critical services** (network, firewall, mwan3)
3. **Configuration integrity** (UCI health, unwanted files)
4. **System stability** (time sync, interface flapping)
5. **Data integrity** (databases, logs)

**Total monitoring coverage**: 8 major system areas with automatic detection, repair, backup, and notification capabilities.
