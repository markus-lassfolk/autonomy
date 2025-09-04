# WSL-based OpenWrt Testing Guide

## Overview

This guide provides a comprehensive approach to testing OpenWrt packages using Windows Subsystem for Linux (WSL). This method is much easier to work with than Docker and provides a dedicated Linux environment for testing.

## 🎯 **Why WSL is Better Than Docker for OpenWrt Testing**

### **Advantages of WSL:**
- ✅ **Native Linux environment** - No container networking issues
- ✅ **Easy file access** - Direct access to Windows files via `/mnt/c/`
- ✅ **Persistent environment** - Your setup stays between sessions
- ✅ **Better performance** - Native Linux performance, not container overhead
- ✅ **Easy debugging** - Standard Linux tools and debugging
- ✅ **Network access** - Full network stack access
- ✅ **Package management** - Use standard Linux package managers

### **Compared to Docker:**
- ❌ Docker networking can be complex
- ❌ File mounting and permissions issues
- ❌ Container isolation makes debugging harder
- ❌ Performance overhead from containerization
- ❌ Network access limitations

## 🚀 **Quick Start**

### **1. Check WSL Availability**

```powershell
# Check if WSL is available
powershell -ExecutionPolicy Bypass -File test/wsl-openwrt-setup.ps1 -Action "check"
```

### **2. Create WSL Instance**

**Option A: Alpine Linux (Lightweight - Recommended)**
```powershell
# Create lightweight Alpine Linux instance
powershell -ExecutionPolicy Bypass -File test/wsl-openwrt-setup.ps1 -Action "1"
```

**Option B: Ubuntu (Full-featured)**
```powershell
# Create full Ubuntu instance
powershell -ExecutionPolicy Bypass -File test/wsl-openwrt-setup.ps1 -Action "2"
```

### **3. Access Your WSL Environment**

```powershell
# Start WSL shell
powershell -ExecutionPolicy Bypass -File test/wsl-openwrt-setup.ps1 -Action "5"
```

## 📋 **Step-by-Step Setup**

### **Step 1: Install WSL (if not already installed)**

```powershell
# Install WSL
wsl --install

# Restart your computer if prompted
```

### **Step 2: Create Dedicated WSL Instance**

```powershell
# Run the setup script
powershell -ExecutionPolicy Bypass -File test/wsl-openwrt-setup.ps1
```

Choose option 1 (Alpine) or 2 (Ubuntu) from the menu.

### **Step 3: Mount Your Project Directory**

```powershell
# Mount your project to WSL
powershell -ExecutionPolicy Bypass -File test/wsl-openwrt-setup.ps1 -Action "6"
```

### **Step 4: Build and Test Packages**

```powershell
# Build OpenWrt packages
powershell -ExecutionPolicy Bypass -File test/wsl-openwrt-setup.ps1 -Action "4"
```

## 🧪 **Testing Your Packages in WSL**

### **Accessing WSL Environment**

```bash
# Start WSL shell
wsl -d openwrt-test

# Your project is available at /workspace
cd /workspace
ls -la
```

### **Building Packages**

```bash
# Navigate to your project
cd /workspace

# Build OpenWrt packages
powershell -ExecutionPolicy Bypass -File build-openwrt-package.ps1 -Architecture "x86_64"
```

### **Testing Package Installation**

```bash
# Navigate to build directory
cd /workspace/build-openwrt

# List available packages
ls -la *.ipk

# Install autonomy package
opkg install autonomy_1.0.0_x86_64.ipk

# Install LuCI web interface
opkg install luci-app-autonomy_1.0.0_all.ipk

# Test service
/etc/init.d/autonomy start
/etc/init.d/autonomy status

# Test ubus interface
ubus call autonomy status
```

### **Testing Web Interface**

```bash
# Start web server (if needed)
/etc/init.d/uhttpd start

# Check if LuCI is accessible
curl http://localhost/cgi-bin/luci/admin/autonomy
```

## 🔧 **WSL Environment Features**

### **Mock OpenWrt Environment**

The WSL setup includes:

- **Mock UCI configuration**: `/etc/config/system`, `/etc/config/network`, `/etc/config/mwan3`
- **Mock commands**: `ubus`, `uci`, `opkg` (with basic functionality)
- **Test directories**: `/etc/config`, `/var/log`, `/tmp/autonomy`

### **Network Testing**

```bash
# Create test network interfaces
sudo ip link add test-wan1 type dummy
sudo ip link add test-wan2 type dummy
sudo ip link set test-wan1 up
sudo ip link set test-wan2 up

# Configure test interfaces
sudo ip addr add 192.168.1.1/24 dev test-wan1
sudo ip addr add 192.168.2.1/24 dev test-wan2
```

### **Package Testing**

```bash
# Test package dependencies
opkg list-installed | grep -E "(luci|mwan3|ubus|uci)"

# Test service management
/etc/init.d/autonomy start
/etc/init.d/autonomy stop
/etc/init.d/autonomy restart
/etc/init.d/autonomy status

# Test configuration
uci show autonomy
uci set autonomy.test.enabled=1
uci commit autonomy
```

## 📊 **Advanced Testing Scenarios**

### **1. Network Failover Testing**

```bash
# Setup test network
sudo ip link add test-wan1 type dummy
sudo ip link add test-wan2 type dummy
sudo ip link set test-wan1 up
sudo ip link set test-wan2 up

# Configure mwan3 for testing
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

### **2. Performance Testing**

```bash
# Monitor resource usage
top -p $(pgrep autonomyd)

# Test response times
time ubus call autonomy status

# Test concurrent requests
for i in {1..10}; do ubus call autonomy status & done
```

### **3. Configuration Testing**

```bash
# Test configuration validation
/etc/init.d/autonomy test

# Test configuration changes
uci set autonomy.main.enabled=0
uci commit autonomy
/etc/init.d/autonomy restart

# Test configuration rollback
uci revert autonomy
/etc/init.d/autonomy restart
```

## 🔍 **Debugging in WSL**

### **Log Analysis**

```bash
# View system logs
dmesg | grep autonomy

# View service logs
journalctl -u autonomy -f

# View application logs
tail -f /var/log/autonomy/autonomy.log
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

### **Process Debugging**

```bash
# Check running processes
ps aux | grep autonomy

# Check open files
lsof -p $(pgrep autonomyd)

# Check memory usage
cat /proc/$(pgrep autonomyd)/status | grep VmRSS
```

## 🚀 **Production Testing**

### **Building Custom OpenWrt Images**

```bash
# Setup OpenWrt Image Builder
cd /workspace
wget https://downloads.openwrt.org/releases/22.03.5/targets/x86/64/openwrt-imagebuilder-22.03.5-x86-64.Linux-x86_64.tar.xz
tar -xf openwrt-imagebuilder-22.03.5-x86-64.Linux-x86_64.tar.xz
cd openwrt-imagebuilder-22.03.5-x86-64.Linux-x86_64

# Build custom image with your packages
make image PACKAGES="luci luci-base luci-compat mwan3 ubus uci autonomy"
```

### **Package Distribution Testing**

```bash
# Create package repository
mkdir -p /workspace/autonomy-feed
cp /workspace/build-openwrt/*.ipk /workspace/autonomy-feed/
cd /workspace/autonomy-feed

# Generate package index
opkg-make-index . > Packages

# Test package installation from repository
echo "src/gz autonomy-feed file:///workspace/autonomy-feed" >> /etc/opkg/customfeeds.conf
opkg update
opkg install autonomy luci-app-autonomy
```

## 📝 **WSL Management Commands**

### **WSL Instance Management**

```powershell
# List WSL instances
wsl -l -v

# Start specific instance
wsl -d openwrt-test

# Stop instance
wsl --terminate openwrt-test

# Remove instance
wsl --unregister openwrt-test

# Export instance (backup)
wsl --export openwrt-test openwrt-test-backup.tar

# Import instance
wsl --import openwrt-test-restore C:\wsl\openwrt-test-restore openwrt-test-backup.tar
```

### **File System Access**

```bash
# Access Windows files from WSL
cd /mnt/c/Users/YourUsername/Documents

# Access WSL files from Windows
# Navigate to: \\wsl$\openwrt-test\home\username
```

## 🎯 **Best Practices**

### **1. Use Dedicated WSL Instance**
- Keep your OpenWrt testing separate from other WSL instances
- Use descriptive names like `openwrt-test` or `autonomy-dev`

### **2. Regular Backups**
```powershell
# Create regular backups
wsl --export openwrt-test openwrt-test-$(Get-Date -Format "yyyyMMdd").tar
```

### **3. Version Control**
```bash
# Keep your project in version control
cd /workspace
git add .
git commit -m "Test results and configuration"
```

### **4. Clean Testing**
```bash
# Reset environment for clean testing
/etc/init.d/autonomy stop
uci revert autonomy
rm -rf /var/log/autonomy/*
```

## 🔧 **Troubleshooting**

### **Common Issues**

**1. WSL Not Starting**
```powershell
# Restart WSL service
wsl --shutdown
wsl --start
```

**2. Package Installation Fails**
```bash
# Check dependencies
opkg list-installed | grep -E "(luci|mwan3|ubus|uci)"

# Install missing packages
sudo apt-get update
sudo apt-get install package-name
```

**3. Network Issues**
```bash
# Check WSL network
ip addr show
ping 8.8.8.8

# Restart network service
sudo service networking restart
```

**4. Permission Issues**
```bash
# Fix file permissions
sudo chown -R $USER:$USER /workspace
chmod +x /workspace/build-openwrt/*.ipk
```

## 📚 **Next Steps**

1. **Set up your WSL environment** using the provided script
2. **Build and test your packages** in the WSL environment
3. **Validate functionality** against the testing checklist
4. **Deploy to real hardware** when testing is complete
5. **Monitor performance** in production environment

For additional support, check the project documentation or create an issue on GitHub.
