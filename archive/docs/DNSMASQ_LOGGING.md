# 🌐 DNSMASQ Logging Control for RUTOS Systems

## 🧠 What is `dnsmasq` and What Does it Do?

`dnsmasq` is a **lightweight DNS forwarder and DHCP server** that's essential for router operations. On OpenWRT/RUTOS systems, it handles:

### Key Responsibilities:

* **DHCP Server** – Assigns IP addresses to connected devices
* **DNS Forwarding** – Forwards DNS queries to upstream servers
* **DNS Caching** – Caches DNS responses for faster lookups
* **Local Name Resolution** – Resolves local hostnames (e.g., router.local)
* **Network Boot** – PXE/TFTP support for network booting
* **DHCP Options** – Custom DHCP options and static leases

In short: **Without `dnsmasq`, your router wouldn't be able to assign IP addresses to devices or resolve domain names.**

---

## 🧾 What Are These `dnsmasq-dhcp` Logs?

These log entries show `dnsmasq`'s DHCP and DNS activity:

### DHCP Logs:
* **DHCPREQUEST**: A device is asking to renew or request an IP
* **DHCPACK**: The router acknowledges and grants the IP
* **DHCPDISCOVER**: A device is looking for available DHCP servers
* **DHCPOFFER**: The router offers an IP address to a device

### DNS Logs:
* **Query logs**: Show DNS requests from clients
* **Response logs**: Show DNS responses and caching activity

Example log entries:
```
dnsmasq-dhcp[1234]: DHCPREQUEST(br-lan) 192.168.1.100 aa:bb:cc:dd:ee:ff
dnsmasq-dhcp[1234]: DHCPACK(br-lan) 192.168.1.100 aa:bb:cc:dd:ee:ff smartphone
```

This shows: interface (`br-lan`), IP (`192.168.1.100`), MAC (`aa:bb:cc:dd:ee:ff`), and hostname (`smartphone`).

---

## 🔍 Should You Disable DHCP/DNS Logging?

### ✅ You *can* suppress them if:

* You don't need to audit client DHCP activity
* You don't use these logs for troubleshooting network issues
* You want to reduce log churn and prevent flash wear
* Your network is stable with predictable device behavior

### 🚫 You should *keep minimal logging* if:

* You're diagnosing IP conflicts or rogue devices
* You need to track device presence/usage patterns
* You're troubleshooting DNS resolution issues
* You want historical network activity data

---

## 🛡️ Best Practices for Production Logging

For a stable, deployed router:

| Log Type | Setting | Use Case |
| -------- | ------- | -------- |
| DHCP Logging | `logdhcp=0` | Disable routine lease renewals (reduces noise) |
| DNS Query Logging | `logqueries=0` | Disable DNS request logging (reduces noise) |
| Error Logging | Always enabled | Keep critical error messages |

This approach:
* Eliminates repetitive lease renewal messages
* Stops DNS query spam from frequent lookups
* Preserves important error and warning messages
* Reduces log storage requirements

---

## 🚨 What Errors Should You Still Watch For?

Even with reduced logging, you'll still see important messages:

### ❗ Critical Issues
* `DHCP packet received on interface with no address`
* `DHCP range exhausted`
* `DNS server failures`
* `Configuration errors`

### ⚠️ Warnings Worth Monitoring
* `Duplicate IP address detected`
* `DHCP lease conflicts`
* `DNS forwarding failures`
* `Interface down/up events`

These indicate real network problems that need attention.

---

## ⚙️ Automated Configuration with RUTOS Starlink Failover

The RUTOS Starlink Failover system includes automated dnsmasq logging optimization through the system maintenance script.

### 🔧 Configuration Options

Add these settings to your `/etc/starlink-config/config.sh`:

```bash
# DHCP dnsmasq logging control
export MAINTENANCE_DNSMASQ_LOGGING_ENABLED="true" # Enable dnsmasq logging optimization (true/false)
export DNSMASQ_LOG_DHCP="0"                       # DHCP logging: 0=disabled, 1=enabled (suppress routine lease renewals)
export DNSMASQ_LOG_QUERIES="0"                    # DNS query logging: 0=disabled, 1=enabled (suppress DNS queries)
```

### 🚀 How It Works

1. **Automatic Detection**: The maintenance script checks current dnsmasq logging configuration every 6 hours and 10 minutes after reboot
2. **UCI Configuration**: Uses OpenWRT's UCI system to safely modify `/etc/config/dhcp`
3. **Service Restart**: Automatically restarts dnsmasq only when changes are made
4. **Persistent Settings**: Configuration survives reboots and system updates

### 📊 Monitoring

The system maintenance script logs all dnsmasq optimization activities:

* **Found**: When sub-optimal logging configurations are detected
* **Fixed**: When configurations are successfully optimized
* **Failed**: When optimization attempts fail

Check the maintenance log at `/var/log/system-maintenance.log` for details.

---

## 🔧 Manual Configuration (Advanced)

If you prefer manual control or need to customize beyond the automated system:

### Option 1: Using UCI Commands (Recommended)

```bash
# Disable DHCP logging (routine lease renewals)
uci set dhcp.@dnsmasq[0].logdhcp='0'

# Disable DNS query logging
uci set dhcp.@dnsmasq[0].logqueries='0'

# Commit changes
uci commit dhcp

# Restart dnsmasq
/etc/init.d/dnsmasq restart
```

### Option 2: Direct Config File Edit

Edit `/etc/config/dhcp` and find the `config dnsmasq` section:

**Before**:
```bash
config dnsmasq
    option domainneeded '1'
    option boguspriv '1'
    option filterwin2k '0'
    # ... other options
```

**After**:
```bash
config dnsmasq
    option domainneeded '1'
    option boguspriv '1'
    option filterwin2k '0'
    option logdhcp '0'      # Disable DHCP logging
    option logqueries '0'   # Disable DNS query logging
    # ... other options
```

Then restart:
```bash
/etc/init.d/dnsmasq restart
```

---

## 🏥 Troubleshooting

### Check Current Settings

```bash
# View current dnsmasq logging configuration
uci show dhcp.@dnsmasq[0] | grep -E "(logdhcp|logqueries)"

# Check if dnsmasq is running
ps | grep dnsmasq

# View recent dnsmasq logs
logread | grep dnsmasq | tail -20
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

# Look for dnsmasq-related entries
grep -i dnsmasq /var/log/system-maintenance.log
```

### Test DHCP/DNS Functionality

```bash
# Test DHCP lease renewal (from client)
dhclient -r && dhclient

# Test DNS resolution
nslookup google.com
dig @192.168.1.1 google.com

# Check DHCP leases
cat /var/dhcp.leases
```

---

## 🔄 Lease Time Optimization

If you prefer to keep logging but reduce frequency, consider increasing DHCP lease times:

```bash
# Set longer lease time (e.g., 4 hours instead of default)
uci set dhcp.lan.leasetime='4h'
uci commit dhcp
/etc/init.d/dnsmasq restart
```

Longer lease times mean:
* ✅ Less frequent renewal messages
* ✅ Reduced network traffic
* ❌ Slower detection of device disconnections
* ❌ Longer IP address hold times for disconnected devices

---

## 📈 Log Analysis

To understand your current logging volume:

```bash
# Count DHCP messages in the last hour
logread | grep "$(date +%b' '%d' '%H)" | grep -c "dnsmasq-dhcp"

# Most active devices (by MAC address)
logread | grep dnsmasq-dhcp | grep -o '[0-9a-f][0-9a-f]:[0-9a-f][0-9a-f]:[0-9a-f][0-9a-f]:[0-9a-f][0-9a-f]:[0-9a-f][0-9a-f]:[0-9a-f][0-9a-f]' | sort | uniq -c | sort -nr

# DNS query volume
logread | grep dnsmasq | grep -c "query"
```

---

## ✅ Summary

* `dnsmasq` handles DHCP and DNS for your router - it's essential for network operation
* **DHCP logging** can be safely disabled on stable networks to reduce log noise
* **DNS query logging** is usually unnecessary unless debugging resolution issues
* The RUTOS Starlink Failover system can automatically optimize these settings
* Manual configuration is available for advanced users
* **Critical errors will still be logged** even with optimized settings
* Configuration changes are persistent and survive reboots

For questions about the automated dnsmasq optimization system, check the main RUTOS Starlink Failover documentation or maintenance logs.
