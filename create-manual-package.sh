#!/bin/bash
set -e

echo "=== Creating Manual VUCI Package ==="

# Create package directories
API_PKG_DIR="vuci-app-example-api-manual"
UI_PKG_DIR="vuci-app-example-ui-manual"

echo "Creating manual packages..."

# Clean and create directories
rm -rf "$API_PKG_DIR" "$UI_PKG_DIR"
mkdir -p "$API_PKG_DIR/files/usr/libexec/rpcd"
mkdir -p "$API_PKG_DIR/files/etc/init.d"
mkdir -p "$UI_PKG_DIR/files/usr/share/vuci/menu.d"
mkdir -p "$UI_PKG_DIR/files/www/views/example"

# Create API package files
echo "Creating API package files..."

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

chmod +x "$API_PKG_DIR/files/etc/init.d/example"

# Create API control file
cat > "$API_PKG_DIR/control" << 'EOF'
Package: vuci-app-example-api
Version: 1.0
Depends: rpcd, rpcd-mod-file, rpcd-mod-iwinfo
Section: net
Architecture: all
Installed-Size: 1024
Description: Example VUCI API for testing package deployment.
EOF

# Create UI package files
echo "Creating UI package files..."

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

        window.onload = function() {
            getStatus();
            getInfo();
        };
    </script>
</body>
</html>
EOF

# Create UI control file
cat > "$UI_PKG_DIR/control" << 'EOF'
Package: vuci-app-example-ui
Version: 1.0
Depends: vuci-app-example-api, vuci-ui-core
Section: net
Architecture: all
Installed-Size: 2048
Description: Example VUCI UI for testing package deployment.
EOF

# Create IPK packages manually
echo "Creating IPK packages..."

# Create API package
cd "$API_PKG_DIR"
tar -czf data.tar.gz files/
tar -czf control.tar.gz control/
echo "2.0" > debian-binary
ar -r ../vuci-app-example-api-manual_1.0_arm_cortex-a7_neon-vfpv4.ipk debian-binary control.tar.gz data.tar.gz
cd ..

# Create UI package
cd "$UI_PKG_DIR"
tar -czf data.tar.gz files/
tar -czf control.tar.gz control/
echo "2.0" > debian-binary
ar -r ../vuci-app-example-ui-manual_1.0_arm_cortex-a7_neon-vfpv4.ipk debian-binary control.tar.gz data.tar.gz
cd ..

echo "Manual packages created successfully!"
echo ""
echo "Created packages:"
echo "- vuci-app-example-api-manual_1.0_arm_cortex-a7_neon-vfpv4.ipk"
echo "- vuci-app-example-ui-manual_1.0_arm_cortex-a7_neon-vfpv4.ipk"
echo ""
echo "To deploy:"
echo "1. Copy packages to device:"
echo "   scp vuci-app-example-api-manual_1.0_arm_cortex-a7_neon-vfpv4.ipk root@192.168.80.1:/tmp/"
echo "   scp vuci-app-example-ui-manual_1.0_arm_cortex-a7_neon-vfpv4.ipk root@192.168.80.1:/tmp/"
echo ""
echo "2. Install packages:"
echo "   ssh root@192.168.80.1 'opkg install /tmp/vuci-app-example-api-manual_1.0_arm_cortex-a7_neon-vfpv4.ipk'"
echo "   ssh root@192.168.80.1 'opkg install /tmp/vuci-app-example-ui-manual_1.0_arm_cortex-a7_neon-vfpv4.ipk'"
echo ""
echo "3. Restart services:"
echo "   ssh root@192.168.80.1 '/etc/init.d/rpcd restart && /etc/init.d/uhttpd restart'"
echo ""
echo "4. Access the UI at:"
echo "   http://192.168.80.1/example/"




