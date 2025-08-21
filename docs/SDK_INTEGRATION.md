# RUTOS SDK Integration Guide

## Overview

This guide covers the integration of the autonomy daemon with the Teltonika RUTOS SDK, including package creation, VuCI web interface development, and professional deployment procedures.

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Package Structure](#package-structure)
3. [Building with RUTOS SDK](#building-with-rutos-sdk)
4. [VuCI Web Interface](#vuci-web-interface)
5. [System Integration](#system-integration)
6. [Testing and Validation](#testing-and-validation)
7. [Deployment](#deployment)
8. [Troubleshooting](#troubleshooting)

## Prerequisites

### Required Software

- **RUTOS SDK**: Teltonika RUTX50 SDK (located at `J:\GithubCursor\rutos-ipq40xx-rutx-sdk`)
- **Build Environment**: Linux or WSL with required build tools
- **Go Toolchain**: Go 1.19+ for building the autonomy daemon
- **OpenWrt Build System**: Standard OpenWrt build environment

### SDK Setup

1. **Clone/Setup RUTOS SDK**:
   ```bash
   # Navigate to SDK directory
   cd /path/to/rutos-ipq40xx-rutx-sdk
   
   # Initialize build environment
   source scripts/env.sh
   ```

2. **Verify SDK Environment**:
   ```bash
   # Check SDK version
   cat .config | grep CONFIG_VERSION_NUMBER
   
   # Verify target architecture
   cat .config | grep CONFIG_TARGET_ARCH
   ```

## Package Structure

### Main Package (`package/autonomy/`)

The autonomy daemon package follows OpenWrt package conventions:

```
package/autonomy/
├── Makefile                    # Package build configuration
├── files/                      # Package files
│   ├── autonomy.init           # Init script
│   ├── autonomy.config         # Default UCI configuration
│   ├── autonomy.watchdog.example # Watchdog configuration
│   ├── autonomyctl             # Control script
│   ├── 99-autonomy-defaults    # UCI defaults script
│   └── README.md               # Package documentation
```

### VuCI App (`vuci-app-autonomy/`)

The VuCI web interface package:

```
vuci-app-autonomy/
├── Makefile                    # VuCI app build configuration
├── root/                       # Web interface files
│   ├── usr/libexec/rpcd/autonomy # RPC daemon plugin
│   ├── usr/lib/lua/luci/controller/autonomy.lua # Controller
│   └── usr/lib/lua/luci/view/autonomy/ # View templates
```

## Building with RUTOS SDK

### 1. Prepare Source Code

```bash
# Clone autonomy repository
git clone https://github.com/autonomy/autonomy.git
cd autonomy

# Build Go binary for target architecture
make cross-compile
```

### 2. Integrate with SDK

```bash
# Copy packages to SDK
cp -r package/autonomy /path/to/rutos-sdk/package/
cp -r vuci-app-autonomy /path/to/rutos-sdk/package/

# Add to package selection
echo "CONFIG_PACKAGE_autonomy=y" >> /path/to/rutos-sdk/.config
echo "CONFIG_PACKAGE_vuci-app-autonomy=y" >> /path/to/rutos-sdk/.config
```

### 3. Build Packages

```bash
# Navigate to SDK
cd /path/to/rutos-sdk

# Update package feeds
./scripts/feeds update -a
./scripts/feeds install -a

# Build packages
make package/autonomy/compile V=s
make package/vuci-app-autonomy/compile V=s
```

### 4. Generate IPK Files

```bash
# Build IPK packages
make package/autonomy/install V=s
make package/vuci-app-autonomy/install V=s

# Find generated IPK files
find bin/packages/ -name "*autonomy*.ipk"
```

## VuCI Web Interface

### Architecture

The VuCI interface consists of:

1. **RPC Daemon Plugin** (`/usr/libexec/rpcd/autonomy`)
   - Provides backend API for web interface
   - Communicates with autonomy daemon via ubus
   - Handles UCI configuration management

2. **LuCI Controller** (`/usr/lib/lua/luci/controller/autonomy.lua`)
   - Defines web interface routes
   - Handles HTTP requests and responses
   - Integrates with LuCI framework

3. **View Templates** (`/usr/lib/lua/luci/view/autonomy/`)
   - HTML templates for web pages
   - JavaScript for real-time updates
   - CSS for styling and responsive design

### Key Features

- **Real-time Monitoring**: Live status updates every 5 seconds
- **Service Control**: Start/stop/restart autonomy daemon
- **Configuration Management**: Edit UCI configuration via web interface
- **Resource Monitoring**: CPU, memory, and disk usage
- **Interface Status**: Visual representation of network interfaces
- **Logs Viewer**: Real-time log display with filtering

### Development Guidelines

1. **Responsive Design**: Ensure mobile-friendly interface
2. **Native Look & Feel**: Match RUTOS web interface styling
3. **Performance**: Optimize for resource-constrained devices
4. **Accessibility**: Follow web accessibility guidelines

## System Integration

### Init Script Integration

The autonomy daemon integrates with OpenWrt's procd system:

```bash
# Service management
/etc/init.d/autonomy start
/etc/init.d/autonomy stop
/etc/init.d/autonomy restart
/etc/init.d/autonomy status
```

### UCI Configuration

Configuration is managed through UCI system:

```bash
# View configuration
uci show autonomy

# Modify settings
uci set autonomy.main.enable='1'
uci set autonomy.main.log_level='info'
uci commit autonomy

# Reload service
/etc/init.d/autonomy reload
```

### User Management

The package creates a dedicated user for security:

```bash
# User: autonomy (UID: 1000)
# Group: autonomy (GID: 1000)
# Home: /var/lib/autonomy
# Shell: /bin/false
```

## Testing and Validation

### Package Testing

1. **Installation Test**:
   ```bash
   # Install package
   opkg install autonomy_1.0.0-1_all.ipk
   
   # Verify installation
   ls -la /usr/sbin/autonomyd
   ls -la /etc/init.d/autonomy
   ```

2. **Configuration Test**:
   ```bash
   # Test configuration
   /etc/init.d/autonomy test
   
   # Verify UCI configuration
   uci show autonomy
   ```

3. **Service Test**:
   ```bash
   # Start service
   /etc/init.d/autonomy start
   
   # Check status
   /etc/init.d/autonomy status
   
   # Test ubus interface
   ubus call autonomy status
   ```

### VuCI Interface Testing

1. **Web Interface Access**:
   - Navigate to RUTOS web interface
   - Check for "Autonomy" menu item
   - Verify all pages load correctly

2. **Functionality Test**:
   - Test service control buttons
   - Verify real-time updates
   - Test configuration changes
   - Check log viewing

3. **Mobile Testing**:
   - Test responsive design
   - Verify touch interactions
   - Check performance on mobile devices

## Deployment

### Package Distribution

1. **Create Package Repository**:
   ```bash
   # Set up package feed
   mkdir -p /var/www/autonomy-feed
   cp *.ipk /var/www/autonomy-feed/
   
   # Generate Packages file
   cd /var/www/autonomy-feed
   opkg-make-index . > Packages
   ```

2. **Configure Package Feed**:
   ```bash
   # Add to /etc/opkg/customfeeds.conf
   src/gz autonomy-feed http://your-server/autonomy-feed
   ```

3. **Install via Package Manager**:
   ```bash
   # Update package lists
   opkg update
   
   # Install packages
   opkg install autonomy
   opkg install vuci-app-autonomy
   ```

### Firmware Integration

For firmware-level integration:

1. **Add to SDK Configuration**:
   ```bash
   # Enable packages in firmware
   echo "CONFIG_PACKAGE_autonomy=y" >> .config
   echo "CONFIG_PACKAGE_vuci-app-autonomy=y" >> .config
   ```

2. **Build Firmware**:
   ```bash
   # Build complete firmware
   make V=s
   ```

3. **Flash Firmware**:
   ```bash
   # Flash to device
   # Follow RUTOS firmware update procedure
   ```

## Troubleshooting

### Common Issues

1. **Package Build Failures**:
   ```bash
   # Check dependencies
   make package/autonomy/compile V=s 2>&1 | grep -i error
   
   # Verify Go toolchain
   go version
   ```

2. **Service Start Failures**:
   ```bash
   # Check logs
   logread | grep autonomy
   
   # Verify binary
   file /usr/sbin/autonomyd
   
   # Check permissions
   ls -la /usr/sbin/autonomyd
   ```

3. **VuCI Interface Issues**:
   ```bash
   # Check RPC daemon
   rpcd -i
   
   # Verify LuCI installation
   opkg list-installed | grep luci
   
   # Check web server logs
   logread | grep uhttpd
   ```

### Debug Procedures

1. **Enable Debug Logging**:
   ```bash
   uci set autonomy.main.log_level='debug'
   uci commit autonomy
   /etc/init.d/autonomy restart
   ```

2. **Check System Resources**:
   ```bash
   # Monitor resource usage
   top -p $(cat /var/run/autonomyd.pid)
   
   # Check memory usage
   cat /proc/$(cat /var/run/autonomyd.pid)/status
   ```

3. **Network Connectivity**:
   ```bash
   # Test ubus communication
   ubus list | grep autonomy
   ubus call autonomy status
   
   # Check network interfaces
   ip link show
   ip addr show
   ```

### Performance Optimization

1. **Memory Usage**:
   - Monitor memory consumption
   - Optimize telemetry storage
   - Implement cleanup procedures

2. **CPU Usage**:
   - Profile decision engine
   - Optimize polling intervals
   - Use efficient algorithms

3. **Network Efficiency**:
   - Minimize API calls
   - Implement caching
   - Optimize telemetry publishing

## Best Practices

### Security

1. **User Isolation**: Run as dedicated user
2. **File Permissions**: Restrict access to sensitive files
3. **Network Security**: Validate all network inputs
4. **Configuration Security**: Sanitize UCI inputs

### Reliability

1. **Error Handling**: Comprehensive error handling
2. **Recovery Procedures**: Automatic recovery mechanisms
3. **Monitoring**: Health checks and alerts
4. **Logging**: Structured logging for debugging

### Maintainability

1. **Code Organization**: Clear package structure
2. **Documentation**: Comprehensive inline documentation
3. **Testing**: Automated test procedures
4. **Version Control**: Proper version management

## Support

For additional support:

- **Documentation**: Check `/usr/share/autonomy/` for local documentation
- **Logs**: Review `/var/log/autonomyd.log` for detailed information
- **Configuration**: Use `autonomyctl config` for configuration help
- **Community**: Join the autonomy community for support

---

**Last Updated**: 2025-08-20
**Version**: 1.0.0

