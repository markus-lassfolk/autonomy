# 🛜 HOSTAPD Logging Control for RUTOS Systems

## 🧠 What is `hostapd` and What Does it Do?

`hostapd` stands for **Host Access Point Daemon**. It is a user-space service that allows a Linux-based system (like OpenWRT/RUTOS) to act as a **Wi-Fi access point** (AP) using supported wireless drivers and hardware.

### Key Responsibilities:

* **Beacon Management** – Sends beacon frames to advertise the AP.
* **Authentication** – Handles WPA/WPA2/WPA3 security (e.g., PSK/EAP).
* **Association Management** – Accepts or rejects client connections.
* **Encryption Setup** – Negotiates key material with clients.
* **802.11 Features** – Handles advanced options like:
  * 802.11k/v/r (roaming assistance)
  * 802.11w (management frame protection)
  * RADIUS integration for enterprise setups
* **DFS Handling** – Dynamic frequency selection for radar avoidance (in EU/US)

In short: **Without `hostapd`, your router wouldn't be able to broadcast or manage a secure Wi-Fi network.**

---

## 🔍 Should You Enable Logging for `hostapd`?

### ✳️ Yes, but **minimal logging is usually enough** unless debugging.

Here's what makes sense for a production router:

| Logging Level | Use Case                                        | Should Keep Enabled?    |
| ------------- | ----------------------------------------------- | ----------------------- |
| `error (1)`   | Serious issues like interface down, WPA failure | ✅ Yes                   |
| `warning (2)` | Non-fatal misconfig or retries                  | 🔶 Optional             |
| `info (3)`    | Status updates like station joins/leaves        | ❌ No (unless debugging) |
| `debug (>=4)` | Protocol-level or verbose tracing               | ❌ No (very noisy)       |

Your router right now logs at **level 2 with all modules**, which is **too chatty** for stable systems. Trimming it to **`logger_syslog=2` and `logger_syslog_level=1`** gives you useful error visibility without spam.

---

## 🚨 What Errors Should You Watch For?

Here are some **meaningful `hostapd` log entries** you *should* care about:

### ❗ Critical

* `Failed to initialize interface`
* `WPA authentication failed`
* `Could not set encryption`
* `Driver does not support configured mode`
* `Invalid beacon interval` (or related)

### ⚠️ Warnings (optional to monitor)

* `Failed to set beacon parameters` (often harmless, but persistent issues may indicate driver problems)
* `Client [MAC] deauthenticated due to timeout`
* `Ignoring STA [MAC] due to mismatch in capabilities`

These can hint at:

* Driver/firmware bugs
* Invalid regulatory domain settings
* DFS radar conflicts
* Overlapping/misconfigured radios
* Clients failing WPA handshake

---

## 🛡️ Best Practices for Production Logging

For a stable, deployed router:

```conf
logger_syslog=2           # Only log IEEE80211 module (main hostapd logic)
logger_syslog_level=1     # Only log errors
logger_stdout=2
logger_stdout_level=1
```

This lets you:

* Catch broken configs
* Spot failing associations or authentication issues
* Avoid log spam from retry loops and noisy drivers

If you're testing new features, debugging WiFi issues, or developing firmware — raise the level *temporarily*, but roll back when done.

---

## ⚙️ Automated Configuration with RUTOS Starlink Failover

The RUTOS Starlink Failover system includes automated hostapd logging optimization through the system maintenance script. This feature:

### 🔧 Configuration Options

Add these settings to your `/etc/starlink-config/config.sh`:

```bash
# WiFi hostapd logging control
export MAINTENANCE_HOSTAPD_LOGGING_ENABLED="true" # Enable hostapd logging optimization (true/false)
export HOSTAPD_LOGGER_SYSLOG="2"                  # Module to log: 2=IEEE80211 (main logic), 127=all modules
export HOSTAPD_LOGGER_SYSLOG_LEVEL="1"            # Log level: 1=error, 2=warning, 3=info, 4+=debug
export HOSTAPD_LOGGER_STDOUT="2"                  # Same as syslog for console output
export HOSTAPD_LOGGER_STDOUT_LEVEL="1"            # Same as syslog level for console output
```

### 🚀 How It Works

1. **Temporary Optimization**: The maintenance script checks and optimizes running hostapd configurations every 6 hours and 10 minutes after reboot
2. **Permanent Fix**: Applies a permanent patch to `/lib/netifd/wireless/mac80211.sh` so new configurations use optimized settings
3. **Smart Reloading**: Only reloads WiFi when changes are actually needed
4. **Automatic Application**: Runs after system reboots to ensure settings persist

### 📊 Monitoring

The system maintenance script logs all hostapd optimization activities:

* **Found**: When sub-optimal configurations are detected
* **Fixed**: When configurations are successfully optimized
* **Failed**: When optimization attempts fail

Check the maintenance log at `/var/log/system-maintenance.log` for details.

---

## 🔧 Manual Configuration (Advanced)

If you prefer manual control or need to customize beyond the automated system:

### Option 1: Temporary Fix (Runtime Only)

Apply to currently running configurations:

```bash
# Find and update hostapd configs
for config in /var/run/hostapd-phy*.conf; do
    if [ -f "$config" ]; then
        sed -i 's/logger_syslog=.*/logger_syslog=2/' "$config"
        sed -i 's/logger_syslog_level=.*/logger_syslog_level=1/' "$config"
        sed -i 's/logger_stdout=.*/logger_stdout=2/' "$config"
        sed -i 's/logger_stdout_level=.*/logger_stdout_level=1/' "$config"
    fi
done

# Reload WiFi to apply changes
wifi reload
```

### Option 2: Permanent Fix (System-Wide)

Edit `/lib/netifd/wireless/mac80211.sh` and find the `hostapd_common_add_log_config()` function.

**Before**:
```bash
append "$var" "logger_syslog=127" "$N"
append "$var" "logger_syslog_level=2" "$N"
append "$var" "logger_stdout=127" "$N"
append "$var" "logger_stdout_level=2" "$N"
```

**After**:
```bash
append "$var" "logger_syslog=2" "$N"
append "$var" "logger_syslog_level=1" "$N"
append "$var" "logger_stdout=2" "$N"
append "$var" "logger_stdout_level=1" "$N"
```

Then reload WiFi:
```bash
wifi reload
```

---

## 🏥 Troubleshooting

### Check Current Settings

```bash
# View current hostapd logging configuration
grep "logger_" /var/run/hostapd-phy*.conf

# Check if permanent fix is applied
grep -A 10 "hostapd_common_add_log_config" /lib/netifd/wireless/mac80211.sh
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

# Look for hostapd-related entries
grep -i hostapd /var/log/system-maintenance.log
```

---

## ✅ Summary

* `hostapd` is the daemon responsible for making your router function as a Wi-Fi AP.
* You **should keep minimal logging enabled** (at least errors).
* Look out for log lines about interface failures, WPA errors, and client failures.
* Disable info/debug logging unless you're actively troubleshooting.
* The RUTOS Starlink Failover system can automatically optimize these settings for you.
* Configuration persists across reboots and WiFi reloads when using the automated system.

For questions about the automated hostapd optimization system, check the main RUTOS Starlink Failover documentation or maintenance logs.
