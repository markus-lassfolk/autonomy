#!/bin/bash

# Create a proper VUCI example package following the SDK pattern
# Based on the structure of vuci-app-bluetooth-api and vuci-app-bluetooth-ui

set -e

SDK_DIR="/home/markusla/rutos-sdk"
PACKAGE_NAME="example"

echo "=== Creating Proper VUCI Example Package ==="

# Create API package directory
API_PACKAGE_DIR="$SDK_DIR/package/feeds/vuci/vuci-app-${PACKAGE_NAME}-api"
mkdir -p "$API_PACKAGE_DIR"

echo "Creating API package in: $API_PACKAGE_DIR"

# Create API package Makefile
cat > "$API_PACKAGE_DIR/Makefile" << 'EOF'
include $(TOPDIR)/rules.mk

APP_TITLE:=VuCI API Support for Example application

include ../api.mk

# call BuildPackage - OpenWrt buildroot signature
EOF

# Create API package files directory structure
mkdir -p "$API_PACKAGE_DIR/files/usr/lib/lua/api/services"

# Create the API service file
cat > "$API_PACKAGE_DIR/files/usr/lib/lua/api/services/example.lua" << 'EOF'
local FunctionService = require("api/FunctionService")

local Example = FunctionService:new()

-- GET /api/example/status
function Example:GET_TYPE_status()
    return self:ResponseOK({
        status = "running",
        timestamp = os.time(),
        version = "1.0.0",
        message = "Hello from example VUCI API!"
    })
end

-- GET /api/example/info
function Example:GET_TYPE_info()
    return self:ResponseOK({
        name = "example",
        description = "A working example VUCI API service",
        author = "Autonomy Team",
        version = "1.0.0",
        features = {"status", "info", "config"}
    })
end

-- GET /api/example/config
function Example:GET_TYPE_config()
    return self:ResponseOK({
        enabled = true,
        port = 8080,
        timeout = 30,
        settings = {
            debug = false,
            log_level = "info"
        }
    })
end

-- POST /api/example/actions/set_config
function Example:SetConfigAction()
    return self:ResponseOK({
        success = true,
        message = "Configuration updated",
        config = self.arguments.data
    })
end

local set_config_action = Example:action("set_config", Example.SetConfigAction)

local enabled = set_config_action:option("enabled")
enabled.require = false
enabled.type = "boolean"

local port = set_config_action:option("port")
port.require = false
port.type = "number"
port.min = 1
port.max = 65535

return Example
EOF

# Create UI package directory
UI_PACKAGE_DIR="$SDK_DIR/package/feeds/vuci/vuci-app-${PACKAGE_NAME}-ui"
mkdir -p "$UI_PACKAGE_DIR"

echo "Creating UI package in: $UI_PACKAGE_DIR"

# Create UI package Makefile
cat > "$UI_PACKAGE_DIR/Makefile" << 'EOF'
include $(TOPDIR)/rules.mk

APP_TITLE:=VuCI UI Support for Example application

include ../app.mk

# call BuildPackage - OpenWrt buildroot signature
EOF

# Create UI package files directory structure
mkdir -p "$UI_PACKAGE_DIR/files/usr/share/vuci/menu.d"
mkdir -p "$UI_PACKAGE_DIR/files/www/views/example"

# Create the menu configuration
cat > "$UI_PACKAGE_DIR/files/usr/share/vuci/menu.d/example.json" << 'EOF'
{
  "services/example": {
    "title": "Example",
    "index": 100,
    "view": "services/Example",
    "acls": ["services/example"]
  }
}
EOF

# Create the UI view
cat > "$UI_PACKAGE_DIR/files/www/views/example/index.html" << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <title>Example Application</title>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="stylesheet" href="/assets/css/vuci.css">
</head>
<body>
    <div class="container">
        <div class="row">
            <div class="col-md-12">
                <div class="card">
                    <div class="card-header">
                        <h3>Example Application</h3>
                    </div>
                    <div class="card-body">
                        <p>This is a working example VUCI application!</p>
                        <div id="status"></div>
                        <button class="btn btn-primary" onclick="getStatus()">Get Status</button>
                    </div>
                </div>
            </div>
        </div>
    </div>

    <script>
        function getStatus() {
            fetch('/api/example/status')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('status').innerHTML = 
                        '<pre>' + JSON.stringify(data, null, 2) + '</pre>';
                })
                .catch(error => {
                    document.getElementById('status').innerHTML = 
                        '<div class="alert alert-danger">Error: ' + error.message + '</div>';
                });
        }
    </script>
</body>
</html>
EOF

echo "=== Package Structure Created ==="
echo "API Package: $API_PACKAGE_DIR"
echo "UI Package: $UI_PACKAGE_DIR"
echo ""
echo "Files created:"
echo "- API service: $API_PACKAGE_DIR/files/usr/lib/lua/api/services/example.lua"
echo "- Menu config: $UI_PACKAGE_DIR/files/usr/share/vuci/menu.d/example.json"
echo "- UI view: $UI_PACKAGE_DIR/files/www/views/example/index.html"
echo ""
echo "Next steps:"
echo "1. Build the packages: cd $SDK_DIR && make package/vuci-app-example-api/compile && make package/vuci-app-example-ui/compile"
echo "2. Find the .ipk files in $SDK_DIR/bin/"
echo "3. Install the packages on your RUTOS device"
echo "4. Access the UI at: https://192.168.80.1/services/example"




