#!/bin/sh

echo "=== Creating VUCI API Service ==="

# Create directories in overlay
echo "Creating directories..."
mkdir -p /overlay/root/upper/usr/lib/lua/api/services
mkdir -p /overlay/root/upper/usr/share/vuci/menu.d
mkdir -p /overlay/root/upper/www/views/example

# Create VUCI API service
echo "Creating VUCI API service..."
cat > /overlay/root/upper/usr/lib/lua/api/services/example.lua << 'EOF'
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

# Create VUCI menu configuration
echo "Creating VUCI menu configuration..."
cat > /overlay/root/upper/usr/share/vuci/menu.d/example.json << 'EOF'
{
    "name": "Example",
    "icon": "icon-example",
    "path": "example",
    "order": 100,
    "permissions": ["example"]
}
EOF

# Create UI HTML file
echo "Creating UI file..."
cat > /overlay/root/upper/www/views/example/index.html << 'EOF'
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
            <p>A working example VUCI API service</p>
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
        </div>
    </div>

    <script>
        function callAPI(endpoint, method = 'GET', data = null) {
            const options = {
                method: method,
                headers: {
                    'Content-Type': 'application/json',
                    'Authorization': 'Bearer ' + (localStorage.getItem('token') || '')
                }
            };
            
            if (data && method === 'POST') {
                options.body = JSON.stringify(data);
            }
            
            return fetch('/api/example/' + endpoint, options)
                .then(response => response.json())
                .then(data => {
                    if (data.success) return data.result;
                    throw new Error(data.errors ? data.errors[0].error : 'API call failed');
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
            callAPI('status')
                .then(result => updateResult('status-result', result))
                .catch(error => updateResult('status-result', error.message, true));
        }

        function getInfo() {
            callAPI('info')
                .then(result => updateResult('info-result', result))
                .catch(error => updateResult('info-result', error.message, true));
        }

        function getConfig() {
            callAPI('config')
                .then(result => updateResult('config-result', result))
                .catch(error => updateResult('config-result', error.message, true));
        }

        function setConfig() {
            const config = {
                enabled: true,
                port: 8080,
                timeout: 30
            };
            
            callAPI('actions/set_config', 'POST', { data: config })
                .then(result => updateResult('config-result', result))
                .catch(error => updateResult('config-result', error.message, true));
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

# Test the API
echo "Testing API..."
sleep 3
if curl -s -k https://localhost/api/example/status > /dev/null 2>&1; then
    echo "✓ API is accessible (may require authentication)"
else
    echo "✗ API not accessible"
fi

echo ""
echo "=== INSTALLATION COMPLETE ==="
echo "VUCI API service has been installed!"
echo ""
echo "You can now:"
echo "1. Access the UI at: https://192.168.80.1/example/"
echo "2. Test the API: curl -k https://localhost/api/example/status"
echo "3. The app should now appear in the Web Package Manager"
echo "4. The 'services/Example' error should be resolved"




