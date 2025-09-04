# 📡 Metered Mode Integration Guide

## Overview

The RUTOS Starlink Failover system now includes comprehensive **automatic metered connection signaling** that seamlessly integrates with mwan3 failover and data usage monitoring. This sophisticated system automatically informs client devices about connection cost characteristics through standards-compliant WiFi vendor elements and DHCP signaling, helping them optimize their data usage behavior without manual intervention.

## ✨ Key Features

### Automatic Failover Integration
- **Seamless mwan3 Integration**: Automatically detects failover events and applies appropriate metered signaling
- **5-Minute Stability Delay**: Prevents rapid mode changes during network instability
- **Intelligent Interface Classification**: Automatically detects data-limited connections vs unlimited connections
- **WiFi STA Detection**: Identifies mobile hotspot/tethering scenarios for appropriate signaling

### Progressive Data Usage Signaling
- **Real-time Monitoring**: Continuously monitors data usage against configured limits
- **Progressive Warnings**: Escalates from `restricted` → `near-cap` → `over-cap` based on actual usage
- **Hysteresis Protection**: Prevents rapid state changes with configurable margin (default 5%)
- **Multiple Data Sources**: Supports nlbw, vnstat, /proc/net/dev, and ubus interface statistics

### Standards-Compliant Client Signaling
- **Windows/Linux**: Microsoft Network Cost IE with progressive warnings
- **Android**: DHCP Option 43 ANDROID_METERED signaling  
- **Apple**: Undocumented vendor element for iOS/macOS compatibility
- **Client Management**: Configurable gentle/force reconnection methods

## 🔧 Information Elements Explained

### Microsoft Network Cost IE
**Format**: `DD 08 00 50 F2 11 <CostLevel> 00 <CostFlags> 00`

| Mode | Cost Level | Flags | Hex | Client Behavior |
|------|------------|-------|-----|-----------------|
| `restricted` | Fixed (02) | None (00) | `DD080050F211020000` | Basic metered awareness |
| `near-cap` | Fixed (02) | ApproachingDataLimit (08) | `DD080050F211020800` | Reduced background activity |
| `over-cap` | Fixed (02) | OverDataLimit (01) | `DD080050F211020100` | Minimal background activity |

### Microsoft Tethering Identifier IE  
**Format**: `DD 0E 00 50 F2 12 00 2B 00 06 <AP_MAC>`
- Used for `tethered-no-limit` mode
- Identifies connection as mobile hotspot
- Embeds actual AP MAC address

### Apple Vendor IE
**Format**: `DD 0A 00 17 F2 06 01 01 03 01 00 00`
- Static element for Apple device compatibility
- Applied to all metered modes (no customization options known)

## 🚀 Installation & Setup

### Automatic Installation
The metered mode system is **automatically included** with the RUTOS Starlink Failover daemon. No separate installation is required.

### Enable Metered Mode
Add to your `/etc/config/autonomy` configuration:

```uci
config autonomy 'main'
    # ... existing configuration ...
    
    # Enable metered mode signaling
    option metered_mode_enabled '1'
    
    # Configure thresholds (optional - defaults shown)
    option data_limit_warning_threshold '80'     # 80% = near-cap mode
    option data_limit_critical_threshold '95'    # 95% = over-cap mode
    option data_usage_hysteresis_margin '5'      # 5% hysteresis margin
    option metered_stability_delay '300'         # 5 minutes stability delay
    option metered_client_reconnect_method 'gentle'  # gentle or force
    option metered_mode_debug '0'                # Enable debug logging
```

### Configure Data Limits
Set data limits for your interfaces using UCI:

```bash
# Set 10GB monthly limit for cellular interface
uci set network.mob1.data_limit="10GB"

# Set 5GB daily limit for USB modem  
uci set network.wwan0.data_limit_daily_bytes="5368709120"

# Commit changes
uci commit network
```

## ⚙️ Advanced Configuration

### Additional UCI Settings
All metered mode configuration is handled through UCI. You can customize additional settings:

```bash
# Restart autonomy daemon after configuration changes
uci commit autonomy
/etc/init.d/autonomy restart
```

## 📱 Usage Examples

### Manual Control via ubus
```bash
# Check current metered mode status
ubus call autonomy status | jq '.metered'

# Force immediate metered mode evaluation
ubus call autonomy action '{"cmd":"recheck_metered"}'

# View data usage for specific interface
ubus call autonomy data_usage '{"interface":"mob1"}'

# Check current member status and scores
ubus call autonomy members | jq '.[] | select(.name=="mob1")'
```

### Configuration Management
```bash
# View current metered mode configuration
uci show autonomy | grep metered

# Enable/disable metered mode
uci set autonomy.main.metered_mode_enabled='1'
uci commit autonomy

# Adjust thresholds
uci set autonomy.main.data_limit_warning_threshold='75'
uci set autonomy.main.data_limit_critical_threshold='90'
uci commit autonomy

# Restart daemon to apply changes
/etc/init.d/autonomy restart
```

## 🔄 Automatic Operation Flow

### Seamless mwan3 Integration
1. **Failover Detection**: Automatic detection via controller callback system
2. **Stability Delay**: Configurable delay (default 5 minutes) prevents rapid changes during network instability
3. **Interface Classification**: Intelligent detection of data-limited vs unlimited connections
4. **Mode Selection**: Automatic mode selection based on interface type and current usage:
   - **Unlimited connections** (Starlink, LAN) → `off`
   - **Data-limited connections** → `restricted` (escalates based on usage)
   - **WiFi STA mode** → `tethered-no-limit`

### Real-time Data Usage Monitoring
1. **Continuous Monitoring**: Every 5 minutes, monitors current data usage via multiple sources
2. **Progressive Escalation**: Automatic escalation based on usage thresholds:
   - **< 80%** → `restricted` (basic metered signaling)
   - **80-95%** → `near-cap` (approaching data limit warning)
   - **> 95%** → `over-cap` (over data limit warning)
3. **Hysteresis Protection**: Configurable margin (default 5%) prevents rapid state changes
4. **Multiple Data Sources**: Supports nlbw, vnstat, /proc/net/dev, and ubus statistics

### Standards-Compliant Client Signaling
1. **Vendor Element Generation**: Creates appropriate IEEE 802.11 vendor elements
2. **DHCP Configuration**: Updates DHCP Option 43 for Android devices
3. **WiFi Configuration Update**: Applies vendor elements to all AP interfaces
4. **Client Reconnection**: Configurable gentle or force reconnection methods
5. **Immediate Effect**: Clients receive new metered status on next connection/beacon

## 🔍 Monitoring and Troubleshooting

### Log Monitoring
```bash
# View metered mode activity in autonomy daemon logs
logread | grep -i metered

# View failover events that trigger metered mode changes
logread | grep "Switching network interface"

# View data usage monitoring
logread | grep "data usage"

# Enable debug logging in UCI configuration
uci set autonomy.main.metered_mode_debug='1'
uci commit autonomy
/etc/init.d/autonomy restart
```

### Service Status
```bash
# Check autonomy daemon status
/etc/init.d/autonomy status

# View current metered mode status via ubus
ubus call autonomy status | grep -A 10 metered

# Restart autonomy daemon
/etc/init.d/autonomy restart
```

### Manual Testing & Status
```bash
# Check current metered mode status
ubus call autonomy status | jq '.metered'

# View current vendor elements on WiFi interfaces
for iface in $(iw dev | awk '/Interface/{print $2}'); do
    echo "=== $iface ==="
    hostapd_cli -i "$iface" get_config | grep vendor_elements
done

# Check DHCP android configuration
uci show dhcp | grep -E "android|metered"

# Test data usage calculation for specific interface
ubus call autonomy data_usage '{"interface":"mob1"}'

# Force immediate metered mode evaluation
ubus call autonomy action '{"cmd":"recheck_metered"}'
```

## 📊 Client Device Behavior

### Windows 10/11
- **Restricted**: Shows "Metered connection" in network settings
- **Near-cap**: Reduces Windows Update downloads, cloud sync
- **Over-cap**: Minimal background activity, user warnings

### Android
- **All modes**: Shows "Limited connection" notification
- **Behavior**: Reduces app background data, limits large downloads

### iOS/macOS  
- **Behavior**: May reduce background app refresh, iCloud sync
- **Note**: Apple's implementation is less documented

### Linux NetworkManager
- **Restricted**: Sets connection as metered in NetworkManager
- **Applications**: Depends on app implementation of metered awareness

## 🛠️ Advanced Customization

### Custom Interface Classification
The Go implementation automatically classifies interfaces based on UCI configuration and interface properties. You can customize data limits per interface:

```bash
# Set custom data limits for specific interfaces
uci set network.satellite_wan.data_limit=""           # No limits for Starlink
uci set network.cellular_backup.data_limit="50GB"     # 50GB limit for cellular backup
uci set network.mobile_hotspot.data_limit="10GB"      # 10GB limit for mobile hotspot

# Commit changes
uci commit network
/etc/init.d/autonomy restart
```

### Custom Notification Integration
The system integrates with the existing Pushover notification system. You can configure notifications in the main autonomy configuration:

```bash
# Configure Pushover notifications for metered mode events
uci set autonomy.main.pushover_token='your_app_token'
uci set autonomy.main.pushover_user='your_user_key'

# Commit changes
uci commit autonomy
/etc/init.d/autonomy restart
```

## 🔒 Security Considerations

### Information Disclosure
- Vendor elements are broadcast in WiFi beacons (visible to all nearby devices)
- DHCP options are only sent to connected clients
- No sensitive data is exposed (only cost/limit status)

### Client Trust
- Clients may ignore metered signaling (not enforced)
- Malicious clients can still consume data regardless of signaling
- Consider implementing actual traffic shaping for enforcement

### Network Isolation
- Metered signaling doesn't provide network isolation
- Consider separate SSIDs for different user classes if needed

## 📈 Performance Impact

### System Resources
- **CPU**: Minimal impact (periodic checks every 5 minutes)
- **Memory**: < 1MB additional RAM usage
- **Storage**: Integrated into main daemon (no additional files)

### Network Impact
- **WiFi**: Slightly larger beacon frames (additional vendor elements)
- **DHCP**: Minimal additional option data
- **Client Reconnections**: Brief disruption during mode changes

### Optimization Tips
- Use `gentle` reconnection method for minimal disruption
- Increase check intervals if system resources are constrained
- Disable debug logging in production environments

## 🆘 Troubleshooting Guide

### Common Issues

#### Metered Mode Not Applied
```bash
# Check if metered mode is enabled
uci get autonomy.main.metered_mode_enabled

# Check autonomy daemon status
/etc/init.d/autonomy status

# Check daemon logs for metered mode activity
logread | grep -i metered

# Force immediate metered mode evaluation
ubus call autonomy action '{"cmd":"recheck_metered"}'
```

#### Clients Not Recognizing Metered Status
```bash
# Check vendor elements in beacon
for iface in $(iw dev | awk '/Interface/{print $2}'); do
    echo "=== $iface ==="
    hostapd_cli -i "$iface" get_config | grep vendor_elements
done

# Check DHCP configuration
uci show dhcp | grep -E "android|metered"

# Check current metered mode status
ubus call autonomy status | jq '.metered'
```

#### Data Usage Not Detected
```bash
# Test data usage calculation for specific interface
ubus call autonomy data_usage '{"interface":"mob1"}'

# Check available monitoring tools
which nlbw vnstat

# Check interface data limits configuration
uci show network | grep data_limit

# Check autonomy logs for data usage monitoring
logread | grep "data usage"
```

#### Daemon Not Starting
```bash
# Check autonomy daemon logs
logread | grep autonomy

# Check configuration syntax
uci show autonomy

# Check if daemon binary exists
ls -la /usr/sbin/autonomyd

# Manual daemon start with debug
/usr/sbin/autonomyd -config /etc/config/autonomy
```

### Debug Mode
Enable comprehensive debugging:

```bash
# Enable debug logging in UCI
uci set autonomy.main.metered_mode_debug='1'
uci set autonomy.main.log_level='debug'
uci commit autonomy

# Restart daemon
/etc/init.d/autonomy restart

# Monitor debug logs
logread -f | grep -E "autonomy|metered"
```

## 📚 References

- [Microsoft Network Cost API](https://docs.microsoft.com/en-us/windows/win32/api/netlistmgr/)
- [Android Metered Connection Detection](https://developer.android.com/training/basics/network-ops/reading-network-state)
- [IEEE 802.11 Vendor-Specific Information Elements](https://standards.ieee.org/standard/802_11-2020.html)
- [OpenWrt UCI Configuration System](https://openwrt.org/docs/guide-user/base-system/uci)
- [RUTOS Documentation](https://wiki.teltonika-networks.com/view/RUTOS)

## 🏗️ Implementation Architecture

### Core Components

#### Metered Mode Manager (`pkg/metered/manager.go`)
- **Central orchestration** of all metered mode operations
- **Configuration management** from UCI settings
- **Failover event handling** via controller callbacks
- **Mode state management** with stability delays and hysteresis

#### Data Usage Monitor (`pkg/metered/data_usage.go`)
- **Multi-source data collection** (nlbw, vnstat, /proc/net/dev, ubus)
- **Intelligent limit parsing** (supports GB, MB, KB units)
- **Usage percentage calculation** with configurable periods
- **Threshold-based escalation** logic

#### WiFi Management (`pkg/metered/wifi.go`)
- **Vendor element generation** for Microsoft Network Cost and Tethering IEs
- **Apple compatibility** with undocumented vendor elements
- **DHCP Option 43** configuration for Android devices
- **Client reconnection** with gentle/force options

### Integration Points

#### Controller Integration
- **Failover callbacks** automatically trigger metered mode evaluation
- **5-minute stability delay** prevents rapid changes during network instability
- **Interface classification** determines appropriate signaling mode

#### Main Daemon Integration
- **5-minute monitoring cycle** for data usage and pending changes
- **Seamless startup** with automatic configuration loading
- **Error handling** with graceful degradation when components unavailable

#### UCI Configuration System
- **Complete integration** with existing autonomy configuration
- **Validation and defaults** for all metered mode settings
- **Hot-reload support** for configuration changes

### Standards Compliance

#### IEEE 802.11 Vendor Elements
- **Microsoft Network Cost IE** (DD 08 00 50 F2 11) with progressive cost levels
- **Microsoft Tethering Identifier IE** (DD 0E 00 50 F2 12) with AP MAC embedding
- **Apple Vendor IE** (DD 0A 00 17 F2 06) for iOS/macOS compatibility

#### DHCP Standards
- **Option 43** with vendor-specific information for Android devices
- **Vendor class targeting** specifically for "Android" devices
- **Automatic cleanup** of conflicting configurations

### Performance Characteristics
- **Minimal CPU impact**: 5-minute monitoring cycles
- **Low memory usage**: < 1MB additional RAM
- **Network efficiency**: Vendor elements add ~30 bytes to beacon frames
- **Storage minimal**: Integrated into main daemon binary

---

*This comprehensive metered mode integration provides enterprise-grade, standards-compliant automatic signaling that seamlessly integrates with your RUTOS Starlink failover system. The Go-based implementation is fully integrated into the main autonomy daemon, requiring no additional scripts or services. All configuration is managed through UCI, and all control is available via ubus commands, ensuring clients are always appropriately informed about connection cost characteristics through multiple complementary signaling methods, all without requiring manual intervention.*
