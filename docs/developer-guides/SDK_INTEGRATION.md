# RUTOS SDK Integration Guide

## Overview

This guide covers the integration of the autonomy daemon with the Teltonika RUTOS SDK, including package creation, VuCI web interface development, and professional deployment procedures.

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Package Structure](#package-structure)
3. [Building with RUTOS SDK](#building-with-rutos-sdk)
4. [VuCI Web Interface](#vuci-web-interface)
5. [API/RPCD Integration](#apirpcd-integration)
6. [System Integration](#system-integration)
7. [Testing and Validation](#testing-and-validation)
8. [Deployment](#deployment)
9. [Troubleshooting](#troubleshooting)

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

```text
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

### VuCI API Package (`vuci-app-autonomy-api/`)

The VuCI API package provides backend services:

```text
vuci-app-autonomy-api/
├── Makefile                    # VuCI API build configuration
└── files/
    ├── usr/lib/lua/api/services/
    │   └── autonomy.lua        # API service file
    └── usr/share/rpcd/acl.d/
        └── autonomy.json       # RPCD access control
```

### VuCI UI Package (`vuci-app-autonomy-ui/`)

The VuCI UI package provides the web interface:

```text
vuci-app-autonomy-ui/
├── Makefile                    # VuCI UI build configuration
├── files/
│   ├── usr/share/vuci/menu.d/
│   │   └── autonomy.json       # Menu configuration
│   └── www/assets/
│       └── app.autonomy.app-1.0.0.js.gz  # Compiled Vue.js
└── src/
    └── src/views/services/
        ├── Autonomy.vue        # Main view component
        └── AutonomyEdit.vue    # Edit form component
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
cp -r vuci-app-autonomy-api /path/to/rutos-sdk/package/feeds/vuci/
cp -r vuci-app-autonomy-ui /path/to/rutos-sdk/package/feeds/vuci/

# Add to package selection
echo "CONFIG_PACKAGE_autonomy=y" >> /path/to/rutos-sdk/.config
echo "CONFIG_PACKAGE_vuci-app-autonomy-api=m" >> /path/to/rutos-sdk/.config
echo "CONFIG_PACKAGE_vuci-app-autonomy-ui=m" >> /path/to/rutos-sdk/.config
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
make package/vuci-app-autonomy-api/compile V=s
make package/vuci-app-autonomy-ui/compile V=s
```

### 4. Generate IPK Files

```bash
# Build IPK packages
make package/autonomy/install V=s
make package/vuci-app-autonomy-api/install V=s
make package/vuci-app-autonomy-ui/install V=s

# Find generated IPK files
find bin/packages/ -name "*autonomy*.ipk"
```

## VuCI Web Interface

### Architecture

The VuCI interface consists of:

1. **API Service** (`/usr/local/usr/lib/lua/api/services/autonomy.lua`)
   - Provides backend API for web interface
   - Communicates with autonomy daemon via ubus
   - Handles UCI configuration management

2. **RPCD ACL** (`/usr/local/usr/share/rpcd/acl.d/autonomy.json`)
   - Defines access control for API endpoints
   - Controls permissions for read/write operations

3. **Menu Configuration** (`/usr/local/usr/share/vuci/menu.d/autonomy.json`)
   - Defines web interface menu structure
   - Links to Vue.js components

4. **Vue.js Components** (`/usr/local/www/assets/app.autonomy.app-*.js.gz`)
   - Compiled Vue.js application
   - Provides user interface functionality

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

## API/RPCD Integration

### API Service Structure

The API service file (`autonomy.lua`) must export a `handle` function:

```lua
local M = {}

function M.get_status()
    return {
        status = "active",
        version = "1.0.0",
        uptime = os.time()
    }
end

function M.get_config()
    local uci = require("uci")
    local cursor = uci.cursor()
    
    local config = {}
    cursor:foreach("autonomy", "main", function(s)
        config[s[".name"]] = s
    end)
    
    return config
end

function M.set_config(section, option, value)
    local uci = require("uci")
    local cursor = uci.cursor()
    
    cursor:set("autonomy", section, option, value)
    cursor:commit("autonomy")
    
    return {success = true}
end

function M.handle(method, path, query, body)
    if path:match("/status") then
        return M.get_status()
    elseif path:match("/config") then
        return M.get_config()
    elseif path:match("/config/set") then
        return M.set_config(query.section, query.option, query.value)
    else
        return {error = "Unknown endpoint", path = path}
    end
end

return M
```

### RPCD ACL Configuration

The ACL file (`autonomy.json`) defines API permissions:

```json
{
  "services/autonomy": {
    "description": "Autonomy App permissions",
    "read": {
      "api": {
        "/autonomy/status": ["*"],
        "/autonomy/config": ["*"]
      }
    },
    "write": {
      "api": [
        "/autonomy/config/set"
      ]
    }
  }
}
```

### Menu Configuration

The menu file (`autonomy.json`) defines the web interface structure:

```json
{
  "services/autonomy": {
    "title": "Autonomy",
    "index": 500,
    "view": "services/Autonomy",
    "acls": ["services/autonomy"]
  }
}
```

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
   # Install packages
   opkg install autonomy_1.0.0-1_all.ipk
   opkg install vuci-app-autonomy-api_1.0.0-1_all.ipk
   opkg install vuci-app-autonomy-ui_1.0.0-1_all.ipk
   
   # Verify installation
   ls -la /usr/sbin/autonomyd
   ls -la /etc/init.d/autonomy
   ls -la /usr/local/usr/lib/lua/api/services/autonomy.lua
   ls -la /usr/local/usr/share/vuci/menu.d/autonomy.json
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

3. **API Testing**:

   ```bash
   # Test API endpoints
   ubus call api get '{"path":"/autonomy/status"}'
   ubus call api get '{"path":"/autonomy/config"}'
   ```

4. **Mobile Testing**:
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
   opkg install vuci-app-autonomy-api
   opkg install vuci-app-autonomy-ui
   ```

### Firmware Integration

For firmware-level integration:

1. **Add to SDK Configuration**:

   ```bash
   # Enable packages in firmware
   echo "CONFIG_PACKAGE_autonomy=y" >> .config
   echo "CONFIG_PACKAGE_vuci-app-autonomy-api=y" >> .config
   echo "CONFIG_PACKAGE_vuci-app-autonomy-ui=y" >> .config
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
   
   # Verify API service
   ls -la /usr/local/usr/lib/lua/api/services/autonomy.lua
   
   # Check ACL configuration
   ls -la /usr/local/usr/share/rpcd/acl.d/autonomy.json
   
   # Check menu configuration
   ls -la /usr/local/usr/share/vuci/menu.d/autonomy.json
   
   # Check web assets
   ls -la /usr/local/www/assets/app.autonomy.app-*.js.gz
   
   # Restart services
   /etc/init.d/rpcd restart
   /etc/init.d/uhttpd restart
   ```

4. **IPK Installation Issues**:

   ```bash
   # Check IPK format
   file autonomy.ipk
   
   # Verify IPK structure
   ar t autonomy.ipk
   
   # Check for gzip compression
   gunzip -c autonomy.ipk > temp.ipk
   ar t temp.ipk
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
   
   # Test API endpoints
   ubus call api get '{"path":"/autonomy/status"}'
   
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
5. **API Security**: Use RPCD ACL for access control

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
