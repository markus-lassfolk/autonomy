#!/bin/sh

echo "=== Installing Example API on RUTOS Device ==="

# Create the example API Lua file
echo "Creating example API..."
cat > /usr/libexec/rpcd/example.lua << 'EOF'
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

# Make the file executable
chmod +x /usr/libexec/rpcd/example.lua

# Create VUCI menu configuration
echo "Creating VUCI menu configuration..."
mkdir -p /usr/share/vuci/menu.d

cat > /usr/share/vuci/menu.d/example.json << 'EOF'
{
    "name": "Example",
    "icon": "icon-example",
    "path": "example",
    "order": 100,
    "permissions": ["example"]
}
EOF

# Create example UI directory and files
echo "Creating example UI..."
mkdir -p /www/views/example

# Create the example UI HTML file
cat > /www/views/example/index.html << 'EOF'
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

# Restart services
echo "Restarting services..."
/etc/init.d/rpcd restart
/etc/init.d/uhttpd restart

# Test the installation
echo "Testing installation..."

# Check if API is available
echo "Checking if example API is available..."
ubus list | grep example

# Test API calls
echo "Testing API calls..."
ubus call example status
ubus call example info

echo ""
echo "=== INSTALLATION COMPLETE ==="
echo "Example API has been installed and configured!"
echo ""
echo "You can now:"
echo "1. Access the UI at: http://192.168.80.1/example/"
echo "2. Test the API: ubus call example status"
echo "3. The app should now appear in the Web Package Manager"
echo "4. The 'services/Example' error should be resolved"




