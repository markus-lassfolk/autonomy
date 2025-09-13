# 🚧 OpenWrt Support Status

## ⚠️ **Current Status: Limited Support**

**Important**: The Autonomy system is primarily designed and tested for **RUTOS** (Teltonika's customized OpenWrt). While it's built on OpenWrt foundations, **full functionality is not guaranteed on vanilla OpenWrt** due to missing RUTOS-specific components.

## 🎯 **What This Means**

- ✅ **Basic failover functionality** should work
- ⚠️ **Advanced features** may have limited functionality
- ❌ **Some features** will not work without RUTOS components
- 🧪 **Testing needed** on vanilla OpenWrt installations

## 🔍 **RUTOS vs OpenWrt Differences**

### RUTOS-Specific Components We Depend On

#### 1. **Cellular Management (`mobiled` service)**

**RUTOS Has**:

```bash
ubus call mobiled signal      # Real-time signal data
ubus call mobiled cell_info   # Cell tower information  
ubus call mobiled operator    # Operator details
```

**OpenWrt May Have**:

- Basic `qmi` tools
- Manual AT command access
- Limited cellular integration

**Impact**:

- ❌ **Cellular monitoring** may be severely limited
- ❌ **Signal quality analysis** may not work
- ❌ **Automatic SIM switching** likely won't work

#### 2. **GPS Services (`gpsctl`, `gsmctl`)**

**RUTOS Has**:

```bash
gpsctl -i                     # GPS information
gsmctl -A AT+CGNSINF          # GPS via cellular modem
ubus call gps location        # GPS via ubus
```

**OpenWrt May Have**:

- `gpsd` daemon (if installed)
- Manual GPS device access (`/dev/ttyUSB*`)
- Limited GPS integration

**Impact**:

- ⚠️ **GPS functionality** may be limited to basic coordinates
- ❌ **RUTOS GPS source** will not work
- ⚠️ **Multi-source GPS fusion** may fall back to fewer sources

#### 3. **Network Interface Management**

**RUTOS Has**:

- Enhanced `mwan3` integration
- Teltonika-specific network naming
- Custom network interface detection

**OpenWrt Has**:

- Standard `mwan3` package
- Standard OpenWrt network naming
- Basic network interface management

**Impact**:

- ⚠️ **Interface auto-discovery** may need manual configuration
- ⚠️ **Network naming** may not match expectations
- ✅ **Basic failover** should work with manual setup

#### 4. **Web Interface (VuCI vs LuCI)**

**RUTOS Has**:

- VuCI (Vue.js-based interface)
- Teltonika-specific styling
- Custom package manager integration

**OpenWrt Has**:

- LuCI (Lua-based interface)  
- Different styling and layout
- Standard OpenWrt package management

**Impact**:

- ❌ **VuCI web interface** will not work
- ❌ **Package manager integration** will not work
- 🔧 **Alternative web interface** would need development

## 🚨 **Features That May Not Work on OpenWrt**

### ❌ **Definitely Won't Work**

1. **Enhanced Cellular Monitoring**
   - Relies on `ubus mobiled` service
   - RUTOS-specific signal quality APIs
   - Automatic SIM management

2. **RUTOS GPS Integration**  
   - `gpsctl` and `gsmctl` commands
   - RUTOS-specific GPS ubus calls
   - Cellular modem GPS integration

3. **VuCI Web Interface**
   - Vue.js-based web interface
   - RUTOS package manager integration
   - Teltonika-specific styling

4. **Advanced Network Discovery**
   - RUTOS-specific interface naming
   - Teltonika network configurations
   - Enhanced mwan3 integration

### ⚠️ **Limited Functionality**

1. **WiFi Optimization**
   - Basic optimization should work
   - RUTOS-specific enhancements may not work
   - Channel analysis may be limited

2. **GPS System**
   - Starlink GPS should work (if Starlink present)
   - OpenCellID fallback should work
   - Multi-source fusion will be limited

3. **Notification System**
   - External notifications (Pushover, email) should work
   - System integration may be limited

4. **Basic Network Failover**
   - Standard mwan3 integration should work
   - Advanced features may not work

### ✅ **Should Work**

1. **Starlink Integration**
   - Direct gRPC API communication
   - Obstruction analysis
   - Satellite tracking

2. **External API Integration**
   - Space-Track API
   - OpenCellID API  
   - Pushover notifications
   - Email notifications

3. **Basic Monitoring**
   - Network connectivity checks
   - Basic health monitoring
   - Log collection

## 🛠️ **OpenWrt Compatibility Checklist**

Before attempting to use Autonomy on OpenWrt, verify these components:

### Required Packages

```bash
# Check if these packages are installed
opkg list-installed | grep -E "mwan3|ubus|uci|curl|json"

# Install missing packages
opkg update
opkg install mwan3 ubus uci libcurl4 libjson-c
```

### Required Services

```bash
# Check if these services exist
ubus list | grep -E "network|system"

# Check for cellular services (likely missing)
ubus list | grep -E "mobiled|cellular|gsm"

# Check for GPS services (likely missing)  
ls /dev/tty* | grep -E "USB|GPS"
```

### Network Configuration

```bash
# Verify mwan3 is configured
uci show mwan3

# Check network interfaces
uci show network
```

## 🔧 **Potential Workarounds**

### 1. **Cellular Monitoring Fallback**

```bash
# Manual AT command access
echo "AT+CSQ" > /dev/ttyUSB2
cat /dev/ttyUSB2

# QMI tools (if available)
qmicli -d /dev/cdc-wdm0 --nas-get-signal-strength
```

### 2. **GPS Fallback**

```bash
# Install gpsd
opkg install gpsd

# Use external GPS sources only
uci set autonomy.gps.sources='starlink,opencellid'
```

### 3. **Manual Interface Configuration**

```bash
# Manually define interfaces instead of auto-discovery
uci set autonomy.interfaces.auto_discovery='0'
uci set autonomy.interfaces.wan_interface='wan'
uci set autonomy.interfaces.cellular_interface='wwan'
```

## 🧪 **Testing on OpenWrt**

### Minimal Test Configuration

```bash
# Disable RUTOS-specific features
uci set autonomy.cellular.enabled='0'          # Disable cellular monitoring
uci set autonomy.gps.sources='starlink'        # Starlink GPS only
uci set autonomy.interfaces.auto_discovery='0'  # Manual interface config

# Enable basic features only
uci set autonomy.main.enabled='1'
uci set autonomy.starlink.enabled='1'
uci set autonomy.predictive.enabled='0'        # Disable ML features
uci commit autonomy
```

### Test Basic Functionality

```bash
# Test service startup
/etc/init.d/autonomy start

# Test ubus API
ubus call autonomy status

# Check logs
logread | grep autonomy
```

## 📋 **OpenWrt Support Roadmap**

### Phase 1: Basic Compatibility (Future)

- [ ] Replace `mobiled` calls with QMI/AT commands
- [ ] Add gpsd integration for GPS
- [ ] Improve interface auto-discovery for vanilla OpenWrt
- [ ] Create OpenWrt-specific configuration templates

### Phase 2: Full OpenWrt Support (Future)

- [ ] LuCI web interface (alternative to VuCI)
- [ ] OpenWrt package manager integration
- [ ] OpenWrt-specific documentation
- [ ] Comprehensive testing on multiple OpenWrt versions

### Phase 3: Cross-Platform (Future)

- [ ] Automatic platform detection
- [ ] Runtime feature adaptation
- [ ] Universal configuration system

## ⚡ **Quick Compatibility Check**

Run this script to check OpenWrt compatibility:

```bash
#!/bin/bash
echo "=== OpenWrt Compatibility Check ==="

# Check OpenWrt version
if [ -f /etc/openwrt_release ]; then
    echo "✅ OpenWrt detected:"
    cat /etc/openwrt_release
else
    echo "❌ Not OpenWrt or missing release info"
fi

# Check required packages
echo -e "\n=== Required Packages ==="
for pkg in mwan3 ubus uci curl; do
    if opkg list-installed | grep -q "^$pkg "; then
        echo "✅ $pkg installed"
    else
        echo "❌ $pkg missing"
    fi
done

# Check RUTOS-specific services
echo -e "\n=== RUTOS-Specific Services ==="
if ubus list | grep -q mobiled; then
    echo "✅ mobiled service available"
else
    echo "❌ mobiled service missing (cellular monitoring limited)"
fi

if command -v gpsctl >/dev/null 2>&1; then
    echo "✅ gpsctl available"
else
    echo "❌ gpsctl missing (RUTOS GPS unavailable)"
fi

if command -v gsmctl >/dev/null 2>&1; then
    echo "✅ gsmctl available"
else
    echo "❌ gsmctl missing (cellular GPS unavailable)"
fi

echo -e "\n=== Recommendation ==="
echo "For full functionality, use RUTOS firmware."
echo "For basic failover on OpenWrt, disable cellular and GPS features."
```

## 📞 **Support and Feedback**

### Current Support Level

- ✅ **RUTOS**: Full support and testing
- ⚠️ **OpenWrt**: Community support only
- 🔧 **Development**: Contributions welcome for OpenWrt compatibility

### Getting Help

- **RUTOS Issues**: Full support via standard channels
- **OpenWrt Issues**: Community support, contributions welcome
- **Compatibility Reports**: Please report what works/doesn't work

### Contributing OpenWrt Support

If you're interested in improving OpenWrt compatibility:

1. Test current functionality and report results
2. Identify specific missing components
3. Contribute code for OpenWrt-specific adaptations
4. Help with testing and validation

## 📚 **Related Documentation**

- [RUTOS Production Guide](../tutorials/rutx50-production-guide.md) - Full RUTOS setup
- [OpenWrt Testing Guide](../tutorials/WSL_OPENWRT_TESTING_GUIDE.md) - Development testing
- [Configuration Guide](../tutorials/configuration-guide.md) - Manual configuration
- [Troubleshooting Guide](troubleshooting-guide.md) - Common issues

---

**Bottom Line**: Use RUTOS for full functionality. OpenWrt support is experimental and requires manual configuration with limited features.
