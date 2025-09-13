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

```text
package/autonomy/
├── Makefile                 # Package build configuration
├── files/                   # Package files
│   ├── autonomy.json       # VuCI configuration
│   ├── autonomy.init       # Init script
│   └── autonomy.config     # UCI configuration
└── src/                    # Source code
    ├── autonomy-daemon/    # Main daemon
    ├── autonomy-api/       # Lua API
    └── autonomy-ui/        # Web interface
```

### Package Makefile

```makefile
include $(TOPDIR)/rules.mk

# Load version information from centralized VERSION file
include $(CURDIR)/../VERSION

PKG_NAME:=autonomy
PKG_VERSION:=$(AUTONOMY_VERSION)
PKG_RELEASE:=$(AUTONOMY_VERSION_BUILD)

PKG_LICENSE:=GPL-2.0
PKG_LICENSE_FILES:=LICENSE

PKG_MAINTAINER:=Autonomy Team <autonomy@example.com>

include $(INCLUDE_DIR)/package.mk

define Package/autonomy
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Autonomy Network Management System
  DEPENDS:=+libubox +libubus +libuci +mwan3 +procd
  URL:=https://github.com/markus-lassfolk/autonomy
endef

define Package/autonomy/description
  Intelligent multi-interface failover system with Starlink tracking
  and predictive analytics for RUTOS routers.
endef

define Build/Compile
  # Build C components
  $(MAKE) -C $(PKG_BUILD_DIR)/src/c/autonomy-daemon \
    CC="$(TARGET_CC)" \
    CFLAGS="$(TARGET_CFLAGS)" \
    LDFLAGS="$(TARGET_LDFLAGS)"
  
  # Build Starlink tracking
  $(MAKE) -C $(PKG_BUILD_DIR)/src/c/starlink-tracking \
    CC="$(TARGET_CC)" \
    CFLAGS="$(TARGET_CFLAGS)" \
    LDFLAGS="$(TARGET_LDFLAGS)"
endef

define Package/autonomy/install
  # Install daemon
  $(INSTALL_DIR) $(1)/usr/sbin
  $(INSTALL_BIN) $(PKG_BUILD_DIR)/src/c/autonomy-daemon/autonomy-daemon $(1)/usr/sbin/
  
  # Install init script
  $(INSTALL_DIR) $(1)/etc/init.d
  $(INSTALL_BIN) $(PKG_BUILD_DIR)/files/autonomy.init $(1)/etc/init.d/autonomy
  
  # Install configuration
  $(INSTALL_DIR) $(1)/etc/config
  $(INSTALL_DATA) $(PKG_BUILD_DIR)/files/autonomy.config $(1)/etc/config/autonomy
  
  # Install VuCI files
  $(INSTALL_DIR) $(1)/usr/share/vuci/menu.d
  $(INSTALL_DATA) $(PKG_BUILD_DIR)/files/autonomy.json $(1)/usr/share/vuci/menu.d/
  
  # Install Lua API
  $(INSTALL_DIR) $(1)/usr/lib/lua/api/services
  $(INSTALL_DATA) $(PKG_BUILD_DIR)/src/api/autonomy-api/*.lua $(1)/usr/lib/lua/api/services/
endef

$(eval $(call BuildPackage,autonomy))
```

## Building with RUTOS SDK

### 1. Prepare Build Environment

```bash
# Navigate to SDK
cd /mnt/wsl/SDK/rutos-ipq40xx-rutx-sdk

# Source environment
source scripts/env.sh

# Verify environment
echo $STAGING_DIR
echo $TARGET_CC
```

### 2. Copy Package Source

```bash
# Copy autonomy package to SDK
cp -r /mnt/s/autonomy /mnt/wsl/SDK/rutos-ipq40xx-rutx-sdk/package/feeds/autonomy/

# Or create symlink for development
ln -sf /mnt/s/autonomy /mnt/wsl/SDK/rutos-ipq40xx-rutx-sdk/package/feeds/autonomy/
```

### 3. Build Package

```bash
# Clean previous builds
make package/feeds/autonomy/autonomy/clean

# Build package
make package/feeds/autonomy/autonomy/compile V=s

# Check build results
ls -la bin/packages/arm_cortex-a7_neon-vfpv4/autonomy/
```

### 4. Build Individual Components

```bash
# Build autonomy daemon only
make package/feeds/autonomy/autonomy-daemon/compile V=s

# Build Starlink tracking only
make package/feeds/autonomy/starlink-tracking/compile V=s

# Build API only
make package/feeds/autonomy/autonomy-api/compile V=s
```

## VuCI Web Interface

### 1. Menu Configuration (`autonomy.json`)

```json
{
  "autonomy": {
    "title": "Autonomy",
    "order": 10,
    "action": {
      "type": "view",
      "path": "autonomy/overview"
    },
    "depends": {
      "acl": ["autonomy"]
    }
  }
}
```

### 2. View Structure

```text
src/web/autonomy-ui/
├── views/
│   ├── overview.html
│   ├── configuration.html
│   ├── monitoring.html
│   └── logs.html
├── js/
│   ├── autonomy.js
│   └── components/
└── css/
    └── autonomy.css
```

### 3. JavaScript Integration

```javascript
// autonomy.js
'use strict';

return L.Class.extend({
    __name__: 'autonomy',
    
    load: function() {
        return Promise.all([
            L.resolveDefault(callAutonomyStatus(), {}),
            L.resolveDefault(callAutonomyConfig(), {})
        ]);
    },
    
    render: function(data) {
        const status = data[0];
        const config = data[1];
        
        return E('div', { class: 'cbi-section' }, [
            E('h3', 'Autonomy Status'),
            E('div', { class: 'cbi-section-node' }, [
                E('p', `Status: ${status.running ? 'Running' : 'Stopped'}`),
                E('p', `Version: ${status.version}`),
                E('p', `Uptime: ${status.uptime}`)
            ])
        ]);
    }
});

function callAutonomyStatus() {
    return L.resolveDefault(L.rpc.declare({
        object: 'autonomy',
        method: 'status',
        expect: { status: {} }
    })(), {});
}

function callAutonomyConfig() {
    return L.resolveDefault(L.rpc.declare({
        object: 'autonomy',
        method: 'get_config',
        expect: { config: {} }
    })(), {});
}
```

## API/RPCD Integration

### 1. UBUS Method Registration

```c
// ubus_methods.c
#include <libubox/blobmsg_json.h>
#include <libubus.h>

static const struct ubus_method autonomy_methods[] = {
    UBUS_METHOD("status", autonomy_status, status_policy),
    UBUS_METHOD("get_config", autonomy_get_config, config_policy),
    UBUS_METHOD("set_config", autonomy_set_config, set_config_policy),
    UBUS_METHOD("restart", autonomy_restart, restart_policy),
    UBUS_METHOD("get_logs", autonomy_get_logs, logs_policy),
};

static struct ubus_object_type autonomy_object_type =
    UBUS_OBJECT_TYPE("autonomy", autonomy_methods);

static struct ubus_object autonomy_object = {
    .name = "autonomy",
    .type = &autonomy_object_type,
    .methods = autonomy_methods,
    .n_methods = ARRAY_SIZE(autonomy_methods),
};

int autonomy_ubus_init(void) {
    int ret = ubus_add_object(ctx, &autonomy_object);
    if (ret) {
        syslog(LOG_ERR, "Failed to add autonomy ubus object: %s", ubus_strerror(ret));
        return ret;
    }
    
    return 0;
}
```

### 2. RPCD Integration

```lua
-- autonomy-api.lua
local rpc = require "luci.rpc"
local ubus = require "ubus"

local function autonomy_status()
    local conn = ubus.connect()
    if not conn then
        return nil, "Failed to connect to ubus"
    end
    
    local status = conn:call("autonomy", "status", {})
    conn:close()
    
    return status
end

local function autonomy_get_config()
    local conn = ubus.connect()
    if not conn then
        return nil, "Failed to connect to ubus"
    end
    
    local config = conn:call("autonomy", "get_config", {})
    conn:close()
    
    return config
end

return {
    status = autonomy_status,
    get_config = autonomy_get_config,
}
```

## System Integration

### 1. Init Script (`autonomy.init`)

```bash
#!/bin/sh /etc/rc.common

START=99
STOP=10

USE_PROCD=1

PROG=/usr/sbin/autonomy-daemon
CONFIG_FILE=/etc/config/autonomy

start_service() {
    procd_open_instance
    procd_set_param command "$PROG" -c "$CONFIG_FILE"
    procd_set_param respawn
    procd_set_param limits core="unlimited"
    procd_set_param limits nofile="65536 65536"
    procd_set_param user root
    procd_set_param group root
    procd_set_param stdout 1
    procd_set_param stderr 1
    procd_close_instance
}

reload_service() {
    ubus call autonomy restart
}

service_triggers() {
    procd_add_reload_trigger "autonomy"
}
```

### 2. UCI Configuration (`autonomy.config`)

```bash
config autonomy 'main'
    option enabled '1'
    option log_level 'info'
    option config_file '/etc/config/autonomy'
    option pid_file '/var/run/autonomy.pid'

config autonomy 'starlink'
    option enabled '1'
    option host '192.168.100.1'
    option port '9200'
    option timeout '30'

config autonomy 'cellular'
    option enabled '1'
    option monitor_interval '30'
    option failover_threshold '3'

config autonomy 'notifications'
    option enabled '1'
    option discord_webhook ''
    option slack_webhook ''
    option email_smtp ''
```

## Testing and Validation

### 1. Package Testing

```bash
# Test package installation
opkg install autonomy_1.0.0-1_arm_cortex-a7_neon-vfpv4.ipk

# Verify installation
opkg list-installed | grep autonomy

# Check service status
/etc/init.d/autonomy status

# Test UBUS methods
ubus call autonomy status
ubus call autonomy get_config
```

### 2. Integration Testing

```bash
# Test VuCI integration
# Navigate to web interface and verify menu appears

# Test API endpoints
curl -X POST http://192.168.1.1/cgi-bin/luci/rpc/autonomy \
  -H "Content-Type: application/json" \
  -d '{"method": "status", "params": []}'

# Test configuration changes
uci set autonomy.main.enabled='0'
uci commit autonomy
/etc/init.d/autonomy reload
```

### 3. Performance Testing

```bash
# Monitor resource usage
top -p $(pgrep autonomy-daemon)

# Check memory usage
cat /proc/$(pgrep autonomy-daemon)/status

# Monitor network connections
netstat -an | grep autonomy
```

## Deployment

### 1. Production Build

```bash
# Clean build environment
make distclean

# Configure for production
make menuconfig
# Select: Network -> autonomy

# Build complete firmware
make V=s

# Build packages only
make package/feeds/autonomy/autonomy/compile V=s
```

### 2. Package Distribution

```bash
# Create package repository
mkdir -p /var/www/html/autonomy/packages/arm_cortex-a7_neon-vfpv4/
cp bin/packages/arm_cortex-a7_neon-vfpv4/autonomy/*.ipk /var/www/html/autonomy/packages/arm_cortex-a7_neon-vfpv4/

# Generate package index
cd /var/www/html/autonomy/packages/arm_cortex-a7_neon-vfpv4/
opkg-make-index . > Packages
gzip Packages
```

### 3. Remote Installation

```bash
# Add repository to router
echo "src/gz autonomy http://your-server.com/autonomy/packages/arm_cortex-a7_neon-vfpv4/" >> /etc/opkg.conf

# Update package list
opkg update

# Install autonomy
opkg install autonomy
```

## Troubleshooting

### Common Build Issues

1. **Missing Dependencies**:

   ```bash
   # Install required packages
   sudo apt-get install build-essential libssl-dev libncurses-dev
   ```

2. **Cross-compilation Errors**:

   ```bash
   # Verify toolchain
   $TARGET_CC --version
   $TARGET_STRIP --version
   ```

3. **Package Build Failures**:

   ```bash
   # Check build logs
   tail -f logs/package.log
   
   # Clean and rebuild
   make package/feeds/autonomy/autonomy/clean
   make package/feeds/autonomy/autonomy/compile V=s
   ```

### Runtime Issues

1. **Service Won't Start**:

   ```bash
   # Check logs
   logread | grep autonomy
   
   # Test manually
   /usr/sbin/autonomy-daemon -c /etc/config/autonomy -d
   ```

2. **UBUS Registration Failed**:

   ```bash
   # Check UBUS status
   ubus list | grep autonomy
   
   # Restart UBUS
   /etc/init.d/ubus restart
   ```

3. **VuCI Integration Issues**:

   ```bash
   # Check menu files
   ls -la /usr/share/vuci/menu.d/
   
   # Restart web interface
   /etc/init.d/uhttpd restart
   ```

## Best Practices

1. **Version Management**: Use centralized version file for all packages
2. **Error Handling**: Implement comprehensive error handling and logging
3. **Resource Management**: Monitor memory and CPU usage
4. **Security**: Validate all inputs and sanitize outputs
5. **Testing**: Test on multiple RUTOS versions and devices
6. **Documentation**: Keep documentation updated with code changes

This guide provides a comprehensive foundation for integrating the autonomy system with the RUTOS SDK and deploying it in production environments.
