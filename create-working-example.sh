#!/bin/bash
set -e

echo "=== Creating Working VUCI Example Package ==="

# Configuration
SDK_DIR="/home/markusla/rutos-sdk"
VUCI_DIR="$SDK_DIR/package/feeds/vuci"
API_PKG_NAME="vuci-app-example-api"
UI_PKG_NAME="vuci-app-example-ui"

echo "SDK directory: $SDK_DIR"
echo "VUCI directory: $VUCI_DIR"

# Check if SDK exists
if [ ! -d "$SDK_DIR" ]; then
    echo "ERROR: SDK not found at $SDK_DIR"
    exit 1
fi

# Create API package directory
API_PKG_DIR="$VUCI_DIR/$API_PKG_NAME"
echo "Creating API package at: $API_PKG_DIR"

# Clean and create API package directory
rm -rf "$API_PKG_DIR"
mkdir -p "$API_PKG_DIR/files/usr/libexec/rpcd"
mkdir -p "$API_PKG_DIR/files/etc/init.d"

# Create API Makefile
cat > "$API_PKG_DIR/Makefile" << 'EOF'
include $(TOPDIR)/rules.mk

PKG_NAME:=vuci-app-example-api
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_MAINTAINER:=Autonomy Team
PKG_LICENSE:=MIT

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Example VUCI API
  DEPENDS:=+rpcd +rpcd-mod-file +rpcd-mod-iwinfo
endef

define Package/$(PKG_NAME)/description
  Example VUCI API for testing package deployment.
endef

define Build/Compile
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/libexec/rpcd
	$(INSTALL_BIN) ./files/example.lua $(1)/usr/libexec/rpcd/
	
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/example.init $(1)/etc/init.d/example
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
EOF

# Create API Lua file
cat > "$API_PKG_DIR/files/usr/libexec/rpcd/example.lua" << 'EOF'
local ubus = require "ubus"
local json = require "luci.jsonc"

local function example_status()
    return {
        status = "running",
        timestamp = os.time(),
        version = "1.0.0",
        message = "Hello from example package!",
        uptime = os.time() - 1700000000
    }
end

local function example_info()
    return {
        name = "example",
        description = "A working example VUCI application",
        author = "Autonomy Team",
        version = "1.0.0",
        features = {"status", "info", "config"}
    }
end

local function example_config()
    return {
        enabled = true,
        port = 8080,
        timeout = 30,
        settings = {
            debug = false,
            log_level = "info"
        }
    }
end

local function example_set_config(config)
    -- In a real app, you would save this to UCI
    return {
        success = true,
        message = "Configuration updated",
        config = config
    }
end

local ubus_conn = ubus.connect()
if not ubus_conn then
    error("Failed to connect to ubus")
end

ubus_conn:add({
    example = {
        status = example_status,
        info = example_info,
        config = example_config,
        set_config = example_set_config
    }
})

ubus_conn:listen()
EOF

# Create API init script
cat > "$API_PKG_DIR/files/etc/init.d/example" << 'EOF'
#!/bin/sh /etc/rc.common

START=99
STOP=10

start() {
    echo "Starting example API service..."
    /etc/init.d/rpcd restart
}

stop() {
    echo "Stopping example API service..."
}

reload() {
    echo "Reloading example API service..."
    /etc/init.d/rpcd restart
}
EOF

# Make init script executable
chmod +x "$API_PKG_DIR/files/etc/init.d/example"

# Create UI package directory
UI_PKG_DIR="$VUCI_DIR/$UI_PKG_NAME"
echo "Creating UI package at: $UI_PKG_DIR"

# Clean and create UI package directory
rm -rf "$UI_PKG_DIR"
mkdir -p "$UI_PKG_DIR/files/usr/share/vuci/menu.d"
mkdir -p "$UI_PKG_DIR/files/www/views/example"

# Create UI Makefile using the SDK pattern
cat > "$UI_PKG_DIR/Makefile" << 'EOF'
include $(TOPDIR)/rules.mk

APP_TITLE:=VuCI UI Support for Example

include ../app.mk

# call BuildPackage - OpenWrt buildroot signature
EOF

# Create VUCI menu configuration
cat > "$UI_PKG_DIR/files/usr/share/vuci/menu.d/example.json" << 'EOF'
{
    "name": "Example",
    "icon": "icon-example",
    "path": "example",
    "order": 100,
    "permissions": ["example"]
}
EOF

# Create main UI view
cat > "$UI_PKG_DIR/files/www/views/example/index.html" << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <title>Example App</title>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { 
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; 
            margin: 0; 
            padding: 20px; 
            background: #f5f5f5; 
        }
        .container { 
            max-width: 1200px; 
            margin: 0 auto; 
            background: white; 
            border-radius: 8px; 
            box-shadow: 0 2px 10px rgba(0,0,0,0.1); 
            overflow: hidden; 
        }
        .header { 
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); 
            color: white; 
            padding: 30px; 
            text-align: center; 
        }
        .content { 
            padding: 30px; 
        }
        .card { 
            background: #f8f9fa; 
            border: 1px solid #e9ecef; 
            border-radius: 6px; 
            padding: 20px; 
            margin: 20px 0; 
        }
        .card h3 { 
            margin-top: 0; 
            color: #495057; 
        }
        button { 
            background: #007bff; 
            color: white; 
            border: none; 
            padding: 10px 20px; 
            border-radius: 4px; 
            cursor: pointer; 
            font-size: 14px; 
            margin: 5px; 
        }
        button:hover { 
            background: #0056b3; 
        }
        .success { 
            background: #d4edda; 
            border-color: #c3e6cb; 
            color: #155724; 
        }
        .error { 
            background: #f8d7da; 
            border-color: #f5c6cb; 
            color: #721c24; 
        }
        pre { 
            background: #f8f9fa; 
            border: 1px solid #e9ecef; 
            border-radius: 4px; 
            padding: 15px; 
            overflow-x: auto; 
            font-size: 12px; 
            line-height: 1.4; 
        }
        .status-indicator {
            display: inline-block;
            width: 10px;
            height: 10px;
            border-radius: 50%;
            margin-right: 8px;
        }
        .status-running { background: #28a745; }
        .status-stopped { background: #dc3545; }
        .status-loading { background: #ffc107; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🚀 Example VUCI Application</h1>
            <p>A working example package for RUTOS deployment testing</p>
        </div>
        
        <div class="content">
            <div class="card">
                <h3>📊 Status</h3>
                <button onclick="getStatus()">Get Status</button>
                <div id="status-result">
                    <span class="status-indicator status-loading"></span>
                    Click button to get status...
                </div>
            </div>
            
            <div class="card">
                <h3>ℹ️ Information</h3>
                <button onclick="getInfo()">Get Info</button>
                <div id="info-result">
                    <span class="status-indicator status-loading"></span>
                    Click button to get info...
                </div>
            </div>
            
            <div class="card">
                <h3>⚙️ Configuration</h3>
                <button onclick="getConfig()">Get Config</button>
                <button onclick="setConfig()">Set Config</button>
                <div id="config-result">
                    <span class="status-indicator status-loading"></span>
                    Click button to get configuration...
                </div>
            </div>
            
            <div class="card">
                <h3>🔧 System Info</h3>
                <button onclick="getSystemInfo()">Get System Info</button>
                <div id="system-result">
                    <span class="status-indicator status-loading"></span>
                    Click button to get system info...
                </div>
            </div>
        </div>
    </div>

    <script>
        function callUbus(method, params) {
            return fetch('/ubus', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    method: 'call',
                    params: ['00000000000000000000000000000000', 'example', method, params || {}]
                })
            })
            .then(response => response.json())
            .then(data => {
                if (data[0] === 0) return data[1];
                throw new Error('Ubus call failed: ' + data[1]);
            });
        }

        function updateResult(elementId, result, isError = false) {
            const element = document.getElementById(elementId);
            const indicator = element.querySelector('.status-indicator');
            
            if (isError) {
                indicator.className = 'status-indicator status-stopped';
                element.innerHTML = `<span class="status-indicator status-stopped"></span><pre class="error">${result}</pre>`;
            } else {
                indicator.className = 'status-indicator status-running';
                element.innerHTML = `<span class="status-indicator status-running"></span><pre class="success">${JSON.stringify(result, null, 2)}</pre>`;
            }
        }

        function getStatus() {
            callUbus('status')
                .then(result => updateResult('status-result', result))
                .catch(error => updateResult('status-result', error.message, true));
        }

        function getInfo() {
            callUbus('info')
                .then(result => updateResult('info-result', result))
                .catch(error => updateResult('info-result', error.message, true));
        }

        function getConfig() {
            callUbus('config')
                .then(result => updateResult('config-result', result))
                .catch(error => updateResult('config-result', error.message, true));
        }

        function setConfig() {
            const config = {
                enabled: true,
                port: 8080,
                timeout: 30,
                settings: {
                    debug: true,
                    log_level: "debug"
                }
            };
            
            callUbus('set_config', config)
                .then(result => updateResult('config-result', result))
                .catch(error => updateResult('config-result', error.message, true));
        }

        function getSystemInfo() {
            // Get system info from ubus
            fetch('/ubus', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    method: 'call',
                    params: ['00000000000000000000000000000000', 'system', 'info', {}]
                })
            })
            .then(response => response.json())
            .then(data => {
                if (data[0] === 0) {
                    updateResult('system-result', data[1]);
                } else {
                    updateResult('system-result', 'System info not available', true);
                }
            })
            .catch(error => updateResult('system-result', error.message, true));
        }

        // Auto-load status on page load
        window.onload = function() {
            getStatus();
            getInfo();
        };
    </script>
</body>
</html>
EOF

echo "Packages created successfully!"

# Change to SDK directory
cd "$SDK_DIR"

# Configure build
echo "Configuring build..."
cat > .config << EOF
CONFIG_TARGET_ar71xx=y
CONFIG_TARGET_ar71xx_generic=y
CONFIG_TARGET_ar71xx_generic_DEVICE_tl-wr841n-v8=y
CONFIG_PACKAGE_vuci-app-example-api=m
CONFIG_PACKAGE_vuci-app-example-ui=m
CONFIG_PACKAGE_rpcd=y
CONFIG_PACKAGE_rpcd-mod-file=y
CONFIG_PACKAGE_rpcd-mod-iwinfo=y
CONFIG_PACKAGE_uhttpd=y
CONFIG_PACKAGE_uhttpd-mod-ubus=y
EOF

# Build packages
echo "Building API package..."
make package/vuci-app-example-api/clean
make package/vuci-app-example-api/compile V=s

echo "Building UI package..."
make package/vuci-app-example-ui/clean
make package/vuci-app-example-ui/compile V=s

# Find built packages
echo "Looking for built packages..."
API_PACKAGE=$(find "$SDK_DIR/bin" -name "vuci-app-example-api_*.ipk" | head -1)
UI_PACKAGE=$(find "$SDK_DIR/bin" -name "vuci-app-example-ui_*.ipk" | head -1)

if [ -n "$API_PACKAGE" ] && [ -n "$UI_PACKAGE" ]; then
    echo "Packages built successfully!"
    echo "API package: $API_PACKAGE"
    echo "UI package: $UI_PACKAGE"
    
    # Copy packages to project directory
    cp "$API_PACKAGE" ./
    cp "$UI_PACKAGE" ./
    
    echo ""
    echo "Packages copied to project directory:"
    echo "- $(basename "$API_PACKAGE")"
    echo "- $(basename "$UI_PACKAGE")"
    echo ""
    echo "To install on device:"
    echo "1. Copy packages to device:"
    echo "   scp $(basename "$API_PACKAGE") root@192.168.80.1:/tmp/"
    echo "   scp $(basename "$UI_PACKAGE") root@192.168.80.1:/tmp/"
    echo ""
    echo "2. Install packages:"
    echo "   ssh root@192.168.80.1 'opkg install /tmp/$(basename "$API_PACKAGE")'"
    echo "   ssh root@192.168.80.1 'opkg install /tmp/$(basename "$UI_PACKAGE")'"
    echo ""
    echo "3. Restart services:"
    echo "   ssh root@192.168.80.1 '/etc/init.d/rpcd restart && /etc/init.d/uhttpd restart'"
    echo ""
    echo "4. Access the UI at:"
    echo "   http://192.168.80.1/example/"
    echo ""
    echo "5. Test the API:"
    echo "   ssh root@192.168.80.1 'ubus call example status'"
    echo "   ssh root@192.168.80.1 'ubus call example info'"
else
    echo "ERROR: Failed to find built packages"
    echo "API package: $API_PACKAGE"
    echo "UI package: $UI_PACKAGE"
    exit 1
fi

echo "Build completed successfully!"




