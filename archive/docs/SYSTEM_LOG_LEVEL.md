# 📊 System Log Level Control for RUTOS Systems

## 🧠 What is System Log Level Control?

System log level control is a **global setting** that determines what types of log messages are recorded system-wide on OpenWRT/RUTOS systems. It affects **all system services** including kernel messages, service logs, and application output.

### How It Works:

* **Centralized Control** – One setting affects all logging across the system
* **Hierarchical Filtering** – Higher numbers include all lower-level messages
* **Performance Impact** – Lower levels reduce CPU overhead and storage usage
* **Flash Protection** – Reduces write cycles on flash storage systems

In short: **System log level control provides the most efficient way to reduce log noise across your entire router.**

---

## 🎚️ Understanding Log Levels

OpenWRT/RUTOS uses standard syslog levels with this hierarchy:

| Level | Name | Description | Includes |
| ----- | ---- | ----------- | -------- |
| **7** | `debug` | Detailed debugging information | Everything (very verbose) |
| **6** | `info` | General informational messages | info + notice + warning + error + critical |
| **5** | `notice` | Normal but significant events | notice + warning + error + critical |
| **4** | `warning` | Warning conditions that should be noted | warning + error + critical |
| **3** | `error` | Error conditions that affect functionality | error + critical |
| **2** | `critical` | Critical conditions requiring immediate action | critical only |

### 🎯 Recommended Level for Production: **4 (warning)**

Setting log level to `4` suppresses:
* ✅ **Routine notice messages** (like DHCP renewals, interface status)
* ✅ **Informational chatter** (service startups, configuration loads)
* ✅ **Debug output** (detailed protocol information)
* ❌ **Keeps important warnings and errors** for troubleshooting

---

## 🚫 What Messages Get Suppressed at Level 4?

### Typical Notice Messages (Level 5) That Disappear:
* `udhcpc: sending discover`
* `udhcpc: sending select`
* `udhcpc: lease obtained`
* `kernel: br-lan: port X(interface) entered forwarding state`
* `netifd: Interface 'lan' is now up`
* `dnsmasq[1234]: started, version 2.xx`

### Typical Info Messages (Level 6) That Disappear:
* Service startup confirmations
* Configuration file loading messages
* Network interface state changes
* Protocol negotiation details

### 🔥 What You KEEP at Level 4:
* **All error messages** - Service failures, connection problems
* **All warning messages** - Configuration issues, deprecated settings
* **All critical messages** - System failures, hardware problems

---

## ⚙️ Automated Configuration with RUTOS Starlink Failover

The RUTOS Starlink Failover system includes automated system log level optimization.

### 🔧 Configuration Options

Add these settings to your `/etc/starlink-config/config.sh`:

```bash
# System-wide log level control
export MAINTENANCE_SYSTEM_LOGLEVEL_ENABLED="true" # Enable system log level optimization (true/false)
export SYSTEM_LOG_LEVEL="4"                       # System log level: 7=debug, 6=info, 5=notice, 4=warning, 3=error, 2=critical
```

### 🚀 How It Works

1. **Automatic Detection**: Checks current system log level every 6 hours and 10 minutes after reboot
2. **UCI Configuration**: Uses OpenWRT's UCI system to safely modify `/etc/config/system`
3. **Service Restart**: Automatically restarts logging service only when changes are made
4. **Persistent Settings**: Configuration survives reboots and system updates
5. **Validation**: Ensures log level is within valid range (2-7)

### 📊 Monitoring

The system maintenance script logs all system log level optimization activities:

* **Found**: When sub-optimal log levels are detected
* **Fixed**: When log levels are successfully optimized  
* **Failed**: When optimization attempts fail

Check the maintenance log at `/var/log/system-maintenance.log` for details.

---

## 🔧 Manual Configuration (Advanced)

If you prefer manual control:

### Option 1: Using UCI Commands (Recommended)

```bash
# Set log level to warning (4) - recommended for production
uci set system.@system[0].log_level='4'

# Commit changes
uci commit system

# Restart logging system
/etc/init.d/log restart
```

### Option 2: Direct Config File Edit

Edit `/etc/config/system` and find the `config system` section:

**Before**:
```bash
config system
    option hostname 'RUTX50'
    option zonename 'Europe/Helsinki'
    # ... other options
```

**After**:
```bash
config system
    option hostname 'RUTX50'
    option zonename 'Europe/Helsinki'
    option log_level '4'    # Add this line
    # ... other options
```

Then restart:
```bash
/etc/init.d/log restart
```

---

## 🎯 Choosing the Right Log Level

### For Different Use Cases:

| Scenario | Recommended Level | Reason |
| -------- | ---------------- | ------ |
| **Production Router** | `4` (warning) | Optimal balance - keeps errors, drops noise |
| **Development/Testing** | `6` (info) | More detail for troubleshooting |
| **Debugging Issues** | `7` (debug) | Maximum verbosity for problem analysis |
| **Minimal Logging** | `3` (error) | Only log actual problems |
| **Critical Only** | `2` (critical) | Absolute minimum logging |

### 🚨 When NOT to Lower Log Level:

* During initial router setup
* When troubleshooting network issues
* When debugging service problems
* When monitoring for security issues
* During firmware updates or changes

---

## 🏥 Troubleshooting

### Check Current Settings

```bash
# View current log level
uci get system.@system[0].log_level

# Check all system settings
uci show system.@system[0]

# View recent system logs
logread | tail -20
```

### Verify Optimization Status

```bash
# Run maintenance check manually
/usr/local/starlink-monitor/scripts/system-maintenance-rutos.sh check

# Run with debug output
DEBUG=1 /usr/local/starlink-monitor/scripts/system-maintenance-rutos.sh check
```

### Check Maintenance Logs

```bash
# View recent maintenance activity
tail -f /var/log/system-maintenance.log

# Look for log level related entries
grep -i "log level\|log_level" /var/log/system-maintenance.log
```

### Test Log Level Changes

```bash
# Generate test messages at different levels
logger -p daemon.debug "Debug message test"
logger -p daemon.info "Info message test"  
logger -p daemon.notice "Notice message test"
logger -p daemon.warning "Warning message test"
logger -p daemon.err "Error message test"

# Check which ones appear in logs
logread | tail -10
```

---

## 📈 Impact Analysis

### Before Optimization (Level 6 - info):
```
Jul 28 12:34:56 RUTX50 udhcpc[1234]: sending discover
Jul 28 12:34:56 RUTX50 udhcpc[1234]: sending select for 192.168.1.100
Jul 28 12:34:56 RUTX50 udhcpc[1234]: lease obtained, lease time 3600
Jul 28 12:34:56 RUTX50 kernel: br-lan: port 1(eth0.1) entered forwarding state
Jul 28 12:34:57 RUTX50 netifd: Interface 'lan' is now up
Jul 28 12:34:57 RUTX50 dnsmasq[5678]: started, version 2.85
```

### After Optimization (Level 4 - warning):
```
(Only errors and warnings appear - routine messages suppressed)
```

### Storage Impact:
* **Reduced log file sizes** by 60-80% in typical scenarios
* **Less flash wear** on embedded systems
* **Faster log searches** due to reduced volume
* **Better performance** due to less logging overhead

---

## 🔄 Dynamic Log Level Control

For temporary debugging, you can temporarily increase verbosity:

```bash
# Temporarily increase to debug level
uci set system.@system[0].log_level='7'
uci commit system
/etc/init.d/log restart

# ... do your debugging ...

# Restore production level
uci set system.@system[0].log_level='4'
uci commit system
/etc/init.d/log restart
```

---

## 🤝 Integration with Other Optimizations

System log level control works great with other logging optimizations:

1. **DNSMASQ Optimization** - Disables specific DHCP/DNS logging
2. **HOSTAPD Optimization** - Reduces WiFi access point logging
3. **System Log Level** - Reduces ALL system-wide logging

**Best Practice**: Use all three together for maximum log noise reduction while preserving important error information.

---

## ✅ Summary

* **System log level** is the most effective way to reduce logging system-wide
* **Level 4 (warning)** is recommended for production systems
* **Suppresses routine notices** like DHCP renewals and service status updates
* **Preserves all errors and warnings** for troubleshooting
* The RUTOS Starlink Failover system can automatically optimize this setting
* **Configuration is persistent** and survives reboots
* Can be **temporarily adjusted** for debugging purposes

For questions about the automated system log level optimization, check the main RUTOS Starlink Failover documentation or maintenance logs.
