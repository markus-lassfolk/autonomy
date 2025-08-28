#!/bin/bash
# Build VUCI packages manually using SDK structure but avoiding make system issues
# This creates properly formatted IPK packages

set -e

echo "========================================="
echo "MANUAL VUCI PACKAGE BUILD (SDK Structure)"
echo "========================================="
echo ""

# Configuration
SDK_BASE="/mnt/wsl/SDK"
OUTPUT_DIR="$SDK_BASE/work/packages"
VERSION="1.0"
RELEASE="9"
ARCH="arm_cortex-a7_neon-vfpv4"

mkdir -p "$OUTPUT_DIR"
cd "$SDK_BASE/work"

echo "Working directory: $(pwd)"
echo "Output directory: $OUTPUT_DIR"
echo ""

# ============================================
# Build API Package
# ============================================
echo "Building VUCI API package..."

API_DIR="vuci-app-example-api-build"
rm -rf "$API_DIR"
mkdir -p "$API_DIR/data/usr/lib/lua/api/services"

# Create API service
cat > "$API_DIR/data/usr/lib/lua/api/services/example.lua" << 'EOF'
-- Example API Service for VUCI
-- Built with SDK on /mnt/wsl/SDK
local M = {}

-- Main status function
function M.get_status()
    return {
        success = true,
        status = "running",
        message = "Example API is working!",
        timestamp = os.time(),
        version = "1.0.9",
        sdk_path = "/mnt/wsl/SDK"
    }
end

-- Test endpoint
function M.test()
    return {
        success = true,
        test = "passed",
        data = "Hello from VUCI API",
        build = "SDK Manual Build"
    }
end

-- Configuration endpoint
function M.get_config()
    local uci = require("uci")
    local cursor = uci.cursor()
    
    return {
        enabled = true,
        interval = 60,
        debug = false,
        uci_available = (cursor ~= nil)
    }
end

-- Handle HTTP requests (for CGI mode)
function M.handle(method, path, query, data)
    if path == "/status" or path == "/api/example/status" then
        return M.get_status()
    elseif path == "/test" or path == "/api/example/test" then
        return M.test()
    elseif path == "/config" or path == "/api/example/config" then
        return M.get_config()
    else
        return { error = "Unknown endpoint", path = path }
    end
end

return M
EOF

# Create control file
cat > "$API_DIR/control" << EOF
Package: vuci-app-example-api
Version: ${VERSION}-${RELEASE}
Depends: libc, lua
Section: vuci
Architecture: ${ARCH}
Installed-Size: 4096
Description: Example VUCI API Service (SDK Build)
EOF

# Create postinst script
cat > "$API_DIR/postinst" << 'EOF'
#!/bin/sh
[ -z "${IPKG_INSTROOT}" ] && {
    echo "Installing Example API..."
    
    # Create symlinks for API access
    mkdir -p /usr/lib/lua/api/services 2>/dev/null || true
    ln -sf /usr/local/usr/lib/lua/api/services/example.lua /usr/lib/lua/api/services/example.lua 2>/dev/null || true
    
    # Create CGI wrapper for direct HTTP access
    mkdir -p /www/cgi-bin 2>/dev/null || true
    cat > /www/cgi-bin/example-api << 'SCRIPT'
#!/usr/bin/lua
local api = require("api.services.example")
local json = require("luci.jsonc") or require("json")

print("Content-Type: application/json")
print("")

local method = os.getenv("REQUEST_METHOD") or "GET"
local path = os.getenv("PATH_INFO") or "/status"
local result = api.handle(method, path, "", nil)

print(json.stringify and json.stringify(result) or json.encode(result))
SCRIPT
    chmod +x /www/cgi-bin/example-api 2>/dev/null || true
    
    echo "Example API installed successfully"
}
exit 0
EOF
chmod +x "$API_DIR/postinst"

echo "2.0" > "$API_DIR/debian-binary"

# Build API IPK
cd "$API_DIR"
tar -czf control.tar.gz --owner=root --group=root control postinst
tar -czf data.tar.gz --owner=root --group=root -C data .
tar -cf ../example-api.tar debian-binary control.tar.gz data.tar.gz
gzip -c ../example-api.tar > "$OUTPUT_DIR/vuci-app-example-api_${VERSION}-${RELEASE}_${ARCH}.ipk"
cd ..
rm -f example-api.tar

echo "✓ API package created: vuci-app-example-api_${VERSION}-${RELEASE}_${ARCH}.ipk"

# ============================================
# Build UI Package
# ============================================
echo ""
echo "Building VUCI UI package..."

UI_DIR="vuci-app-example-ui-build"
rm -rf "$UI_DIR"
mkdir -p "$UI_DIR/data/usr/share/vuci/menu.d"
mkdir -p "$UI_DIR/data/www/example"

# Create menu configuration
cat > "$UI_DIR/data/usr/share/vuci/menu.d/example.json" << 'EOF'
{
  "services/example": {
    "title": "Example",
    "index": 500,
    "view": "services/Example",
    "acls": ["services/example"]
  }
}
EOF

# Create enhanced HTML interface
cat > "$UI_DIR/data/www/example/index.html" << 'EOF'
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Example VUCI App - SDK Build</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { 
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        .container { max-width: 1200px; margin: 0 auto; }
        .header {
            background: white;
            border-radius: 15px;
            padding: 30px;
            margin-bottom: 30px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.1);
        }
        .header h1 { color: #333; margin-bottom: 10px; }
        .header .subtitle { color: #666; }
        .card {
            background: white;
            border-radius: 10px;
            padding: 25px;
            margin-bottom: 20px;
            box-shadow: 0 5px 15px rgba(0,0,0,0.08);
        }
        .card h2 {
            color: #333;
            margin-bottom: 20px;
            padding-bottom: 10px;
            border-bottom: 2px solid #f0f0f0;
        }
        .btn-group {
            display: flex;
            gap: 10px;
            flex-wrap: wrap;
            margin: 20px 0;
        }
        button {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            padding: 12px 24px;
            border-radius: 8px;
            cursor: pointer;
            font-size: 15px;
            font-weight: 500;
            transition: all 0.3s;
            box-shadow: 0 4px 15px rgba(102, 126, 234, 0.3);
        }
        button:hover {
            transform: translateY(-2px);
            box-shadow: 0 6px 20px rgba(102, 126, 234, 0.4);
        }
        button:active { transform: translateY(0); }
        button:disabled {
            background: #ccc;
            cursor: not-allowed;
            transform: none;
            box-shadow: none;
        }
        .status-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
            margin: 20px 0;
        }
        .status-item {
            background: #f8f9fa;
            padding: 15px;
            border-radius: 8px;
            border-left: 4px solid #667eea;
        }
        .status-item .label {
            color: #666;
            font-size: 12px;
            text-transform: uppercase;
            letter-spacing: 1px;
            margin-bottom: 5px;
        }
        .status-item .value {
            color: #333;
            font-size: 18px;
            font-weight: bold;
        }
        .success { color: #28a745; }
        .error { color: #dc3545; }
        .warning { color: #ffc107; }
        .info { color: #17a2b8; }
        pre {
            background: #2d3748;
            color: #48bb78;
            padding: 20px;
            border-radius: 8px;
            overflow-x: auto;
            font-family: 'Courier New', monospace;
            font-size: 14px;
            line-height: 1.5;
        }
        .spinner {
            display: inline-block;
            width: 20px;
            height: 20px;
            border: 3px solid rgba(102, 126, 234, 0.3);
            border-radius: 50%;
            border-top-color: #667eea;
            animation: spin 1s ease-in-out infinite;
        }
        @keyframes spin {
            to { transform: rotate(360deg); }
        }
        .result-box {
            background: #f8f9fa;
            border-radius: 8px;
            padding: 20px;
            margin: 20px 0;
            min-height: 100px;
        }
        .badge {
            display: inline-block;
            padding: 4px 8px;
            border-radius: 4px;
            font-size: 12px;
            font-weight: bold;
            margin: 0 5px;
        }
        .badge.success { background: #d4edda; color: #155724; }
        .badge.error { background: #f8d7da; color: #721c24; }
        .badge.info { background: #d1ecf1; color: #0c5460; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🚀 Example VUCI Application</h1>
            <div class="subtitle">
                SDK Build v1.0.9 | Built on /mnt/wsl/SDK
                <span class="badge success">ACTIVE</span>
                <span class="badge info">SDK</span>
            </div>
        </div>
        
        <div class="card">
            <h2>📊 System Status</h2>
            <div class="status-grid">
                <div class="status-item">
                    <div class="label">Package Version</div>
                    <div class="value">1.0-9</div>
                </div>
                <div class="status-item">
                    <div class="label">Build Type</div>
                    <div class="value">SDK Manual</div>
                </div>
                <div class="status-item">
                    <div class="label">API Status</div>
                    <div class="value success">Ready</div>
                </div>
                <div class="status-item">
                    <div class="label">Uptime</div>
                    <div class="value" id="uptime">0s</div>
                </div>
            </div>
        </div>
        
        <div class="card">
            <h2>🧪 API Testing</h2>
            <div class="btn-group">
                <button onclick="testAPI('status')">Get Status</button>
                <button onclick="testAPI('test')">Run Test</button>
                <button onclick="testAPI('config')">Get Config</button>
                <button onclick="testCGI()">Test CGI</button>
                <button onclick="checkAllRoutes()">Check All Routes</button>
                <button onclick="clearResults()">Clear</button>
            </div>
            <div class="result-box" id="result">
                <span class="info">Ready to test API endpoints...</span>
            </div>
        </div>
        
        <div class="card">
            <h2>📝 Response Log</h2>
            <pre id="log">Waiting for API calls...</pre>
        </div>
    </div>
    
    <script>
        let startTime = Date.now();
        let requestCount = 0;
        let successCount = 0;
        
        // Update uptime
        setInterval(() => {
            const uptime = Math.floor((Date.now() - startTime) / 1000);
            document.getElementById('uptime').textContent = uptime + 's';
        }, 1000);
        
        async function testAPI(endpoint) {
            requestCount++;
            const resultDiv = document.getElementById('result');
            const logDiv = document.getElementById('log');
            
            resultDiv.innerHTML = '<div class="spinner"></div> Testing API...';
            
            const paths = [
                `/api/example/${endpoint}`,
                `/cgi-bin/example-api/${endpoint}`,
                `/ubus/example.${endpoint}`,
                `/example/${endpoint}`
            ];
            
            let found = false;
            let results = [];
            
            for (const path of paths) {
                try {
                    const response = await fetch(path);
                    const text = await response.text();
                    
                    results.push({
                        path: path,
                        status: response.status,
                        ok: response.ok,
                        data: response.ok ? text : null
                    });
                    
                    if (response.ok) {
                        found = true;
                        successCount++;
                        try {
                            const json = JSON.parse(text);
                            resultDiv.innerHTML = '<span class="success">✓ API call successful!</span>';
                            logDiv.textContent = JSON.stringify({
                                endpoint: endpoint,
                                path: path,
                                response: json
                            }, null, 2);
                            break;
                        } catch (e) {
                            resultDiv.innerHTML = '<span class="warning">⚠ Response not JSON</span>';
                            logDiv.textContent = 'Response:\n' + text;
                        }
                    }
                } catch (error) {
                    results.push({
                        path: path,
                        error: error.message
                    });
                }
            }
            
            if (!found) {
                resultDiv.innerHTML = '<span class="error">✗ All paths failed</span>';
                logDiv.textContent = 'Tried paths:\n' + JSON.stringify(results, null, 2);
            }
        }
        
        async function testCGI() {
            const resultDiv = document.getElementById('result');
            const logDiv = document.getElementById('log');
            
            resultDiv.innerHTML = '<div class="spinner"></div> Testing CGI wrapper...';
            
            try {
                const response = await fetch('/cgi-bin/example-api');
                const text = await response.text();
                
                if (response.ok) {
                    resultDiv.innerHTML = '<span class="success">✓ CGI wrapper working!</span>';
                    logDiv.textContent = 'CGI Response:\n' + text;
                } else {
                    resultDiv.innerHTML = '<span class="error">✗ CGI failed: ' + response.status + '</span>';
                    logDiv.textContent = 'Error: HTTP ' + response.status;
                }
            } catch (error) {
                resultDiv.innerHTML = '<span class="error">✗ CGI error: ' + error.message + '</span>';
                logDiv.textContent = 'Error: ' + error.message;
            }
        }
        
        async function checkAllRoutes() {
            const resultDiv = document.getElementById('result');
            const logDiv = document.getElementById('log');
            
            resultDiv.innerHTML = '<div class="spinner"></div> Checking all routes...';
            
            const routes = [
                '/api/example/status',
                '/api/example/test',
                '/api/example/config',
                '/cgi-bin/example-api',
                '/www/example/',
                '/usr/local/www/example/'
            ];
            
            let results = {};
            for (const route of routes) {
                try {
                    const response = await fetch(route);
                    results[route] = response.status + ' ' + response.statusText;
                } catch (e) {
                    results[route] = 'Failed: ' + e.message;
                }
            }
            
            resultDiv.innerHTML = '<span class="info">Route check complete</span>';
            logDiv.textContent = 'Route Check Results:\n' + JSON.stringify(results, null, 2);
        }
        
        function clearResults() {
            document.getElementById('result').innerHTML = '<span class="info">Ready to test API endpoints...</span>';
            document.getElementById('log').textContent = 'Waiting for API calls...';
        }
        
        // Auto-check on load
        window.onload = () => {
            setTimeout(() => {
                checkAllRoutes();
            }, 1000);
        };
    </script>
</body>
</html>
EOF

# Create control file
cat > "$UI_DIR/control" << EOF
Package: vuci-app-example-ui
Version: ${VERSION}-${RELEASE}
Depends: libc, vuci-app-example-api
Section: vuci
Architecture: ${ARCH}
Installed-Size: 8192
Description: Example VUCI Web Interface (SDK Build)
EOF

# Create postinst script
cat > "$UI_DIR/postinst" << 'EOF'
#!/bin/sh
[ -z "${IPKG_INSTROOT}" ] && {
    echo "Installing Example UI..."
    
    # Create symlinks for menu
    mkdir -p /usr/share/vuci/menu.d 2>/dev/null || true
    ln -sf /usr/local/usr/share/vuci/menu.d/example.json /usr/share/vuci/menu.d/example.json 2>/dev/null || true
    
    # Create direct access symlink
    ln -sf /usr/local/www/example/index.html /www/example.html 2>/dev/null || true
    
    # Restart services
    /etc/init.d/uhttpd restart 2>/dev/null || true
    /etc/init.d/rpcd restart 2>/dev/null || true
    
    echo ""
    echo "========================================="
    echo "Example VUCI App Installed!"
    echo "========================================="
    echo "Access the app at:"
    echo "  http://router-ip/example/"
    echo "  http://router-ip/example.html"
    echo ""
}
exit 0
EOF
chmod +x "$UI_DIR/postinst"

echo "2.0" > "$UI_DIR/debian-binary"

# Build UI IPK
cd "$UI_DIR"
tar -czf control.tar.gz --owner=root --group=root control postinst
tar -czf data.tar.gz --owner=root --group=root -C data .
tar -cf ../example-ui.tar debian-binary control.tar.gz data.tar.gz
gzip -c ../example-ui.tar > "$OUTPUT_DIR/vuci-app-example-ui_${VERSION}-${RELEASE}_${ARCH}.ipk"
cd ..
rm -f example-ui.tar

echo "✓ UI package created: vuci-app-example-ui_${VERSION}-${RELEASE}_${ARCH}.ipk"

# Clean up build directories
rm -rf "$API_DIR" "$UI_DIR"

# ============================================
# Summary
# ============================================
echo ""
echo "========================================="
echo "BUILD COMPLETE!"
echo "========================================="
echo ""
echo "Packages created in: $OUTPUT_DIR"
ls -lh "$OUTPUT_DIR"/vuci-app-example*_${VERSION}-${RELEASE}_*.ipk
echo ""
echo "These packages include:"
echo "  ✓ Lua API service with multiple endpoints"
echo "  ✓ CGI wrapper for direct HTTP access"
echo "  ✓ Enhanced HTML interface with API testing"
echo "  ✓ Automatic symlink creation in postinst"
echo "  ✓ Service restart on installation"
echo ""
echo "To deploy:"
echo "  scp $OUTPUT_DIR/*.ipk root@router:/tmp/"
echo "  ssh root@router 'opkg install /tmp/vuci-app-example*.ipk'"
echo "  Access: http://router-ip/example/"
echo ""


