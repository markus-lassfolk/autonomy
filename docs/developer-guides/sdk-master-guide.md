# MASTER GUIDE — Building RutOS (Teltonika) Packages & VuCI Apps (RUTX 7.17.1)

This guide consolidates our internal notes, quick sheets, and integration write-ups into a
single, end-to-end reference for **building RutOS-compliant packages**, registering
**UBUS/RPCD** APIs, and shipping **VuCI** web apps that appear and function correctly in the
**Package Manager** and **WebUI**.

> Scope: RUTX family on RutOS **00.07.17.1**; adapt variables for other device lines
> (RUT9/TRBx) and versions.  
> Audience: developers building packages and VuCI apps for production.

---

## 0) What's different on RutOS vs "vanilla" OpenWrt

- **IPK format is strict**: A valid IPK is an **ar** archive with `debian-binary`,
  `control.tar.gz`, `data.tar.gz` — and **must be gzip-compressed** for `opkg` to accept it
  (when crafting manually).  
- **Install prefix reality**: Third-party/user packages end up under the **`/usr/local` overlay**
  at runtime (e.g., menus → `/usr/local/usr/share/vuci/menu.d`, APIs →
  `/usr/local/usr/lib/lua/api/services`). The SDK/Makefile installs to `/usr/...` paths, which
  get remapped via overlay. Plan your paths accordingly.  
- **WebUI technology**: RutOS uses **VuCI** (Vue-based, LuCI-like). UI is shipped as
  **compiled, gzipped JS bundles** under `/www/assets/` with the naming
  `app.<name>.app-<version|hash>.js.gz`. Views must match menu `view` paths exactly
  (case-sensitive).  

---

## 1) SDK Setup (Host)

The SDK flow below mirrors our **Developer QuickSheet**; use WSL or native Linux.  

### 1.1 Host prerequisites (Ubuntu/WSL) — **[Host]**

```bash
sudo apt-get update -y
sudo apt-get install -y \
  binutils binutils-gold bison build-essential bzip2 ca-certificates curl cmake \
  default-jdk device-tree-compiler devscripts file flex g++ gawk gcc gettext git \
  gnupg gperf help2man jq libc6-dev libffi-dev libexpat1-dev libncurses-dev \
  libpcre3-dev libsqlite3-dev libssl-dev libxml-parser-perl lz4 liblz4-dev \
  libzstd-dev make patch pkg-config psmisc python-is-python3 python3 python3-dev \
  python3-setuptools python3-yaml rsync ruby sharutils subversion swig \
  u-boot-tools unzip uuid-dev vim-common wget zip zlib1g-dev time dos2unix
# (Optional) Node 20.x if you will compile VuCI locally
curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
sudo apt-get install -y nodejs
```

### 1.2 Obtain and initialize the SDK — **\[Host]**

```bash
SDK_URL="https://firmware.teltonika-networks.com/7.17.1/RUTX/RUTX_R_GPL_00.07.17.1.tar.gz"
SDK_DIR="/mnt/wsl/SDK/rutos-ipq40xx-rutx-sdk"

# Download and extract
wget -O /tmp/rutos-sdk.tar.gz "$SDK_URL"
mkdir -p "$(dirname "$SDK_DIR")"
tar -xzf /tmp/rutos-sdk.tar.gz -C "$(dirname "$SDK_DIR")"
mv "$(dirname "$SDK_DIR")/rutos-ipq40xx-rutx-sdk" "$SDK_DIR"

# Initialize build environment
cd "$SDK_DIR"
source scripts/env.sh

# Verify
echo "SDK: $SDK_DIR"
echo "Target: $(grep CONFIG_TARGET_ARCH .config)"
echo "Toolchain: $TARGET_CC"
```

---

## 2) Package Structure & Makefile

### 2.1 Directory layout — **[Host]**

```text
package/feeds/autonomy/
├── autonomy/                    # Main package
│   ├── Makefile
│   ├── files/
│   │   ├── autonomy.json       # VuCI menu
│   │   ├── autonomy.init       # procd init script
│   │   └── autonomy.config     # UCI config template
│   └── src/
│       ├── autonomy-daemon/    # C daemon
│       ├── autonomy-api/       # Lua API
│       └── autonomy-ui/        # VuCI views
├── autonomy-daemon/            # Separate daemon package
├── autonomy-api/               # Separate API package
└── autonomy-ui/                # Separate UI package
```

### 2.2 Main package Makefile — **[Host]**

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
  $(INSTALL_BIN) $(PKG_BUILD_DIR)/files/autonomy.init $(1)/usr/sbin/autonomy
  
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

---

## 3) VuCI Web Interface

### 3.1 Menu configuration — **[Host]**

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

### 3.2 View structure — **[Host]**

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

### 3.3 JavaScript integration — **[Host]**

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

---

## 4) UBUS/RPCD Integration

### 4.1 UBUS method registration — **[Host]**

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

### 4.2 RPCD integration — **[Host]**

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

---

## 5) Building & Testing

### 5.1 Build package — **[Host]**

```bash
# Clean previous builds
make package/feeds/autonomy/autonomy/clean

# Build package
make package/feeds/autonomy/autonomy/compile V=s

# Check build results
ls -la bin/packages/arm_cortex-a7_neon-vfpv4/autonomy/
```

### 5.2 Test installation — **[Device]**

```bash
# Install package
opkg install autonomy_1.0.0-1_arm_cortex-a7_neon-vfpv4.ipk

# Verify installation
opkg list-installed | grep autonomy

# Check service status
/etc/init.d/autonomy status

# Test UBUS methods
ubus call autonomy status
ubus call autonomy get_config
```

### 5.3 Test VuCI integration — **[Device]**

```bash
# Check menu files
ls -la /usr/local/usr/share/vuci/menu.d/

# Restart web interface
/etc/init.d/uhttpd restart

# Navigate to web interface and verify menu appears
```

---

## 6) Deployment & Distribution

### 6.1 Create package repository — **[Host]**

```bash
# Create repository structure
mkdir -p /var/www/html/autonomy/packages/arm_cortex-a7_neon-vfpv4/

# Copy packages
cp bin/packages/arm_cortex-a7_neon-vfpv4/autonomy/*.ipk /var/www/html/autonomy/packages/arm_cortex-a7_neon-vfpv4/

# Generate package index
cd /var/www/html/autonomy/packages/arm_cortex-a7_neon-vfpv4/
opkg-make-index . > Packages
gzip Packages
```

### 6.2 Remote installation — **[Device]**

```bash
# Add repository to router
echo "src/gz autonomy http://your-server.com/autonomy/packages/arm_cortex-a7_neon-vfpv4/" >> /etc/opkg.conf

# Update package list
opkg update

# Install autonomy
opkg install autonomy
```

---

## 7) Troubleshooting

### 7.1 Common build issues — **[Host]**

```bash
# Missing dependencies
sudo apt-get install build-essential libssl-dev libncurses-dev

# Cross-compilation errors
$TARGET_CC --version
$TARGET_STRIP --version

# Package build failures
tail -f logs/package.log
make package/feeds/autonomy/autonomy/clean
make package/feeds/autonomy/autonomy/compile V=s
```

### 7.2 Runtime issues — **[Device]**

```bash
# Service won't start
logread | grep autonomy
/usr/sbin/autonomy-daemon -c /etc/config/autonomy -d

# UBUS registration failed
ubus list | grep autonomy
/etc/init.d/ubus restart

# VuCI integration issues
ls -la /usr/local/usr/share/vuci/menu.d/
/etc/init.d/uhttpd restart
```

---

## 8) Best Practices

1. **Version Management**: Use centralized version file for all packages
2. **Error Handling**: Implement comprehensive error handling and logging
3. **Resource Management**: Monitor memory and CPU usage
4. **Security**: Validate all inputs and sanitize outputs
5. **Testing**: Test on multiple RUTOS versions and devices
6. **Documentation**: Keep documentation updated with code changes

---

## 9) Quick Reference

### Build Commands

```bash
# Build all autonomy packages
make package/feeds/autonomy/autonomy/compile V=s

# Build specific package
make package/feeds/autonomy/autonomy-daemon/compile V=s

# Clean build
make package/feeds/autonomy/autonomy/clean
```

### Installation Commands

```bash
# Install package
opkg install autonomy_1.0.0-1_arm_cortex-a7_neon-vfpv4.ipk

# Remove package
opkg remove autonomy

# Check status
opkg list-installed | grep autonomy
```

### Service Commands

```bash
# Start service
/etc/init.d/autonomy start

# Stop service
/etc/init.d/autonomy stop

# Restart service
/etc/init.d/autonomy restart

# Check status
/etc/init.d/autonomy status
```

### UBUS Commands

```bash
# List methods
ubus list | grep autonomy

# Call method
ubus call autonomy status
ubus call autonomy get_config
ubus call autonomy restart
```

This master guide provides everything needed to build, package, and deploy RutOS applications with VuCI integration!
