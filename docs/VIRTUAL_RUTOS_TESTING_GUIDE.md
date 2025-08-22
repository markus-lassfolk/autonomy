# Virtual RUTOS Testing Guide (OpenWrt-based)

## Overview

This guide provides a comprehensive approach to testing RUTOS packages in a virtual environment before deploying to physical hardware. **RUTOS is based on OpenWrt**, which uses **BusyBox** as its core system utilities and is designed specifically for embedded devices and routers.

## 🎯 **Why Virtual RUTOS Testing is Essential**

### **Benefits of Virtual Testing:**
- ✅ **Safe Testing** - No risk to physical hardware
- ✅ **Rapid Iteration** - Quick setup and teardown
- ✅ **Cost Effective** - No need for multiple physical devices
- ✅ **Isolated Environment** - Clean testing without affecting production
- ✅ **OpenWrt/BusyBox Environment** - Test in the actual environment RUTOS uses
- ✅ **Pre-Deployment Validation** - Ensure packages work before physical deployment

### **RUTOS Architecture:**
- **Base System**: OpenWrt (Linux distribution for embedded devices)
- **Core Utilities**: BusyBox (minimal Unix tools)
- **Package Manager**: opkg (OpenWrt Package Manager)
- **Configuration**: UCI (Unified Configuration Interface)
- **RPC System**: ubus (OpenWrt's RPC system)
- **RUTOS Extensions**: gpsctl, gsmctl (Teltonika-specific commands)

### **Testing Scenarios:**
- Package installation and dependencies in OpenWrt environment
- RUTOS-specific commands (`gpsctl`, `gsmctl`)
- Service management and configuration via UCI
- Network failover and monitoring with mwan3
- GPS and cellular functionality
- Web interface integration (LuCI)

## 🚀 **Quick Start**

### **📋 Menu Options Explained:**

When you run the script, you'll see these options:

**1. Create OpenWrt-based RUTOS environment**
- **What it does**: Creates a new WSL instance that simulates OpenWrt/BusyBox
- **When to use**: FIRST TIME setup only
- **What you get**: Mock ubus, uci, opkg, gpsctl, gsmctl commands + OpenWrt directory structure

**2. Setup OpenWrt Image Builder**
- **What it does**: Downloads and configures OpenWrt Image Builder for building real firmware
- **When to use**: ADVANCED testing with actual OpenWrt firmware images
- **What you get**: Ability to build custom OpenWrt images with your packages included

**3. Test RUTOS packages in OpenWrt environment**
- **What it does**: Runs automated tests on your RUTOS packages
- **When to use**: VALIDATION - to check if your packages work correctly
- **What you get**: Test results for ubus, uci, opkg, gpsctl, gsmctl functionality

**4. Start OpenWrt RUTOS shell**
- **What it does**: Opens interactive shell in the OpenWrt RUTOS environment
- **When to use**: DAILY DEVELOPMENT and manual testing
- **What you get**: Interactive environment where you can test commands manually

**5. List WSL instances**
- **What it does**: Shows all available WSL instances and their status
- **When to use**: CHECK what environments are available and running
- **What you get**: Overview of your WSL setup

### **🎯 Typical Workflow:**

```powershell
# 1. FIRST TIME SETUP (one time only)
powershell -ExecutionPolicy Bypass -File test/setup-virtual-rutos-openwrt.ps1 -Action "1"

# 2. DAILY DEVELOPMENT (use this most often)
powershell -ExecutionPolicy Bypass -File test/setup-virtual-rutos-openwrt.ps1 -Action "4"

# 3. VALIDATION (when you want to test your packages)
powershell -ExecutionPolicy Bypass -File test/setup-virtual-rutos-openwrt.ps1 -Action "3"

# 4. CHECK STATUS (see what's available)
powershell -ExecutionPolicy Bypass -File test/setup-virtual-rutos-openwrt.ps1 -Action "5"
```

## 📋 **Step-by-Step Setup**

### **Step 1: Create OpenWrt-based RUTOS Environment**

```powershell
# Run the OpenWrt-based RUTOS setup script
powershell -ExecutionPolicy Bypass -File test/setup-virtual-rutos-openwrt.ps1
```

Choose option 1 to create the OpenWrt-based RUTOS environment.

### **Step 2: Access OpenWrt RUTOS Environment**

```bash
# Start OpenWrt RUTOS shell
wsl -d rutos-openwrt-test

# Your project is available at /workspace
cd /workspace
ls -la
```

### **Step 3: Test OpenWrt Environment**

```bash
# Test OpenWrt commands
ubus version
uci show system
opkg list-installed

# Test RUTOS-specific commands
gpsctl --status
gsmctl --status
```

## 🧪 **Testing Your RUTOS Packages**

### **Accessing OpenWrt RUTOS Environment**

```bash
# Start OpenWrt RUTOS shell
wsl -d rutos-openwrt-test

# Your project is available at /workspace
cd /workspace
ls -la
```

### **Building RUTOS Packages for OpenWrt**

```bash
# Navigate to your project
cd /workspace

# Build RUTOS packages for OpenWrt
powershell -ExecutionPolicy Bypass -File build-rutos-package-fixed.ps1 -Architecture "arm_cortex-a7_neon-vfpv4"
```

### **Testing Package Installation in OpenWrt**

```bash
# Navigate to build directory
cd /workspace/build

# List available packages
ls -la *.ipk

# Install autonomy package (OpenWrt style)
opkg install autonomy_1.0.0_arm_cortex-a7_neon-vfpv4.ipk

# Install LuCI web interface
opkg install luci-app-autonomy_1.0.0_all.ipk

# Test service (OpenWrt init script style)
/etc/init.d/autonomy start
/etc/init.d/autonomy status

# Test ubus interface
ubus call autonomy status
```

### **Testing RUTOS-Specific Commands**

```bash
# Test GPS functionality (RUTOS-specific)
gpsctl --status
gpsctl --location

# Test GSM functionality (RUTOS-specific)
gsmctl --status
gsmctl --signal

# Test UCI configuration (OpenWrt style)
uci show autonomy
uci set autonomy.main.enabled=1
uci commit autonomy
```

## 🔧 **OpenWrt-based RUTOS Environment Features**

### **OpenWrt/BusyBox Environment**

The OpenWrt-based RUTOS setup includes:

- **OpenWrt-style directory structure**: `/etc/config`, `/var/log`, `/tmp/autonomy`
- **OpenWrt configuration**: UCI-based system, network, and mwan3 configs
- **OpenWrt commands**: `ubus`, `uci`, `opkg` (simulating actual OpenWrt behavior)
- **RUTOS-specific commands**: `gpsctl`, `gsmctl` (Teltonika extensions)
- **OpenWrt init scripts**: `/etc/init.d/autonomy` with proper OpenWrt format
- **BusyBox utilities**: Minimal Unix tools as used in OpenWrt

### **OpenWrt-Specific Testing**

```bash
# Test OpenWrt commands
ubus version
uci show system
opkg list-installed

# Test RUTOS-specific commands
gpsctl --status
gpsctl --location
gsmctl --status
gsmctl --signal

# Test OpenWrt service management
/etc/init.d/autonomy start
/etc/init.d/autonomy status
/etc/init.d/autonomy stop
```

### **Network Testing in OpenWrt Environment**

```bash
# Create test network interfaces (OpenWrt style)
sudo ip link add test-wan1 type dummy
sudo ip link add test-wan2 type dummy
sudo ip link set test-wan1 up
sudo ip link set test-wan2 up

# Configure test interfaces
sudo ip addr add 192.168.1.1/24 dev test-wan1
sudo ip addr add 192.168.2.1/24 dev test-wan2

# Test mwan3 configuration
uci show mwan3
uci set mwan3.test_wan1=interface
uci set mwan3.test_wan1.enabled=1
uci commit mwan3
```

## 📊 **Advanced Testing Scenarios**

### **1. GPS and Location Testing (RUTOS-specific)**

```bash
# Test GPS functionality
gpsctl --status
gpsctl --location

# Test location-based features
ubus call autonomy gps_status
ubus call autonomy location_info

# Test GPS data collection
/etc/init.d/autonomy start
sleep 5
ubus call autonomy gps_data
```

### **2. Cellular and GSM Testing (RUTOS-specific)**

```bash
# Test GSM functionality
gsmctl --status
gsmctl --signal
gsmctl --operator

# Test cellular data
gsmctl --data-status
gsmctl --connection-status

# Test cellular failover
ubus call autonomy cellular_status
ubus call autonomy cellular_failover
```

### **3. Network Failover Testing (OpenWrt/mwan3)**

```bash
# Setup test network
sudo ip link add test-wan1 type dummy
sudo ip link add test-wan2 type dummy
sudo ip link set test-wan1 up
sudo ip link set test-wan2 up

# Configure mwan3 for testing (OpenWrt style)
uci set mwan3.test_wan1=interface
uci set mwan3.test_wan1.enabled=1
uci set mwan3.test_wan1.family=ipv4
uci set mwan3.test_wan1.track_method=ping
uci set mwan3.test_wan1.track_ip=8.8.8.8
uci commit mwan3

# Start autonomy service
/etc/init.d/autonomy start

# Test failover
sudo ip link set test-wan1 down
ubus call autonomy status
```

### **4. Performance Testing in OpenWrt Environment**

```bash
# Monitor resource usage
top -p $(pgrep autonomyd)

# Test response times
time ubus call autonomy status

# Test concurrent requests
for i in {1..10}; do ubus call autonomy status & done
```

## 🔍 **Debugging in OpenWrt RUTOS Environment**

### **Log Analysis**

```bash
# View system logs
dmesg | grep autonomy

# View service logs
logread | grep autonomy

# View application logs
tail -f /var/log/autonomy/autonomy.log
```

### **OpenWrt-Specific Debugging**

```bash
# Test OpenWrt commands
ubus version
uci show autonomy
uci show mwan3

# Test RUTOS commands
gpsctl --status
gpsctl --debug
gsmctl --status
gsmctl --debug
```

### **Network Debugging**

```bash
# Check network interfaces
ip link show
ip addr show

# Test connectivity
ping 8.8.8.8
traceroute google.com

# Check routing
ip route show
```

## 🚀 **OpenWrt Image Builder Setup**

### **Option 2: Full OpenWrt Image Builder**

For more realistic testing, you can set up a full OpenWrt Image Builder:

```powershell
# Setup OpenWrt Image Builder
powershell -ExecutionPolicy Bypass -File test/setup-virtual-rutos-openwrt.ps1 -Action "2"
```

This creates a complete OpenWrt build environment that matches RUTOS.

### **Building Custom OpenWrt Images**

```bash
# Navigate to OpenWrt Image Builder
cd /workspace/openwrt-imagebuilder-22.03.5-x86-64.Linux-x86_64

# Build custom image with RUTOS-style packages
make image PACKAGES="luci luci-base luci-compat mwan3 ubus uci busybox"

# Output files in bin/targets/x86/64/
ls -la bin/targets/x86/64/
```

## 📝 **OpenWrt RUTOS Management**

### **WSL Instance Management**

```powershell
# List WSL instances
wsl -l -v

# Start OpenWrt RUTOS
wsl -d rutos-openwrt-test

# Stop instance
wsl --terminate rutos-openwrt-test

# Remove instance
wsl --unregister rutos-openwrt-test

# Export instance (backup)
wsl --export rutos-openwrt-test rutos-openwrt-backup.tar

# Import instance
wsl --import rutos-openwrt-restore C:\wsl\rutos-openwrt-restore rutos-openwrt-backup.tar
```

### **File System Access**

```bash
# Access Windows files from OpenWrt RUTOS
cd /mnt/c/Users/YourUsername/Documents

# Access OpenWrt RUTOS files from Windows
# Navigate to: \\wsl$\rutos-openwrt-test\home\username
```

## 🎯 **Best Practices**

### **1. Use OpenWrt-based Environment**
- Test in the actual OpenWrt/BusyBox environment RUTOS uses
- Use OpenWrt-specific commands and configurations
- Follow OpenWrt conventions for services and configuration

### **2. Regular Testing Cycles**
```bash
# Reset environment for clean testing
/etc/init.d/autonomy stop
uci revert autonomy
rm -rf /var/log/autonomy/*
```

### **3. Version Control**
```bash
# Keep your project in version control
cd /workspace
git add .
git commit -m "OpenWrt RUTOS testing results and configuration"
```

### **4. Comprehensive Testing**
- Test all OpenWrt-specific commands (`ubus`, `uci`, `opkg`)
- Validate RUTOS-specific functionality (`gpsctl`, `gsmctl`)
- Test network failover scenarios with mwan3
- Verify web interface integration (LuCI)

## 🔧 **Troubleshooting**

### **Common Issues**

**1. OpenWrt RUTOS Not Starting**
```powershell
# Restart WSL service
wsl --shutdown
wsl --start
```

**2. Package Installation Fails**
```bash
# Check OpenWrt dependencies
opkg list-installed | grep -E "(luci|mwan3|ubus|uci)"

# Install missing packages
sudo apt-get update
sudo apt-get install package-name
```

**3. RUTOS Commands Not Working**
```bash
# Check if RUTOS commands are installed
which gpsctl
which gsmctl

# Reinstall RUTOS commands if needed
sudo bash -c 'echo "#!/bin/bash" > /usr/bin/gpsctl'
sudo bash -c 'echo "echo \"RUTOS gpsctl - \$*\"" >> /usr/bin/gpsctl'
sudo chmod +x /usr/bin/gpsctl
```

**4. OpenWrt Configuration Issues**
```bash
# Check OpenWrt configuration
uci show system
uci show network
uci show mwan3

# Reset configuration if needed
uci revert system
uci commit system
```

## 📚 **Next Steps**

1. **Set up your OpenWrt-based RUTOS environment** using the provided script
2. **Build and test your RUTOS packages** in the OpenWrt environment
3. **Validate OpenWrt-specific functionality** against the testing checklist
4. **Test RUTOS-specific features** in the OpenWrt environment
5. **Deploy to physical RUTOS hardware** when testing is complete
6. **Monitor performance** in production environment

## 🎯 **Testing Checklist**

### **Pre-Deployment Testing**
- [ ] Packages built successfully for ARM architecture
- [ ] Dependencies resolved for OpenWrt environment
- [ ] OpenWrt commands working (`ubus`, `uci`, `opkg`)
- [ ] RUTOS-specific commands working (`gpsctl`, `gsmctl`)
- [ ] Service starts and stops correctly (OpenWrt init script style)
- [ ] UCI configuration works properly
- [ ] ubus interface responds correctly

### **Functionality Testing**
- [ ] GPS functionality tested (RUTOS-specific)
- [ ] GSM functionality tested (RUTOS-specific)
- [ ] Network failover working (mwan3)
- [ ] Web interface accessible (LuCI)
- [ ] Logs generated correctly
- [ ] Performance meets requirements

### **Integration Testing**
- [ ] mwan3 integration functional
- [ ] UCI configuration changes take effect
- [ ] Service responds to configuration changes
- [ ] GPS data collection working
- [ ] Cellular failover triggers correctly

For additional support, check the project documentation or create an issue on GitHub.
