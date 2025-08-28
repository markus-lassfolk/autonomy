#!/bin/bash
# Build VUCI packages with proper API integration and postinst scripts
# This version creates symlinks automatically and fixes API routing

set -e

echo "========================================="
echo "WORKING VUCI PACKAGE BUILD"
echo "========================================="
echo ""

# Configuration
BUILD_DIR="/home/markusla/vuci-packages"
WORK_DIR="/tmp/vuci-working-$$"
VERSION="1.0"
RELEASE="6"
ARCH="arm_cortex-a7_neon-vfpv4"

# Create work directory
rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"
mkdir -p "$BUILD_DIR"

cd "$WORK_DIR"

echo "Build directory: $WORK_DIR"
echo "Output directory: $BUILD_DIR"
echo ""

# ============================================
# STEP 1: Create Working API Package
# ============================================
echo "Creating API package with proper routing..."

API_PKG="vuci-app-example-api"
API_DIR="$WORK_DIR/$API_PKG"

# Create the correct directory structure
mkdir -p "$API_DIR/data/usr/lib/lua/api/services"

# Create the API service with proper routing
cat > "$API_DIR/data/usr/lib/lua/api/services/example.lua" << 'EOF'
-- Example API Service for VUCI with proper routing
local M = {}

-- Main handler function that routes requests
function M.handle(method, path, query, data)
    -- Log the request for debugging
    local log = io.open("/tmp/example-api.log", "a")
    if log then
        log:write(string.format("[%s] Method: %s, Path: %s\n", os.date(), method, path))
        log:close()
    end
    
    -- Route based on path
    if path == "/api/example/test" then
        return M.test()
    elseif path == "/api/example/status" then
        return M.status()
    elseif path == "/api/example/config" then
        if method == "GET" then
            return M.get_config()
        elseif method == "POST" then
            return M.set_config(data)
        end
    end
    
    -- Default response for unknown paths
    return {
        error = "Unknown API endpoint",
        path = path,
        method = method
    }
end

-- Test endpoint
function M.test()
    return {
        success = true,
        message = "Example API is working!",
        timestamp = os.time(),
        version = "1.0.6",
        method = "test"
    }
end

-- Status endpoint
function M.status()
    return {
        running = true,
        uptime = os.time(),
        memory = collectgarbage("count"),
        method = "status"
    }
end

-- Get configuration
function M.get_config()
    return {
        enabled = true,
        interval = 60,
        debug = false,
        method = "get_config"
    }
end

-- Set configuration
function M.set_config(data)
    return {
        success = true,
        message = "Configuration updated",
        data = data,
        method = "set_config"
    }
end

-- Make functions accessible
return M
EOF

# Create control file
cat > "$API_DIR/control" << EOF
Package: ${API_PKG}
Version: ${VERSION}-${RELEASE}
Depends: libc, lua
Section: vuci
Architecture: ${ARCH}
Installed-Size: 2048
Description: Example VUCI API service with routing
EOF

# Create postinst that creates symlinks
cat > "$API_DIR/postinst" << 'EOF'
#!/bin/sh
echo "==== Installing Example API ===="
[ -z "$IPKG_INSTROOT" ] && {
    # Create symlink from /usr/local to expected location
    mkdir -p /overlay/root/upper/usr/lib/lua/api/services/ 2>/dev/null
    ln -sf /usr/local/usr/lib/lua/api/services/example.lua /overlay/root/upper/usr/lib/lua/api/services/example.lua 2>/dev/null
    
    # Also try direct symlink (some systems)
    ln -sf /usr/local/usr/lib/lua/api/services/example.lua /usr/lib/lua/api/services/example.lua 2>/dev/null || true
    
    echo "API symlinks created"
}
exit 0
EOF
chmod +x "$API_DIR/postinst"

echo "2.0" > "$API_DIR/debian-binary"

# Build API package
cd "$API_DIR"
tar -czf control.tar.gz --owner=root --group=root control postinst
tar -czf data.tar.gz --owner=root --group=root -C data .
tar -cf "../${API_PKG}_${VERSION}-${RELEASE}_${ARCH}.tar" debian-binary control.tar.gz data.tar.gz
gzip -c "../${API_PKG}_${VERSION}-${RELEASE}_${ARCH}.tar" > "$BUILD_DIR/${API_PKG}_${VERSION}-${RELEASE}_${ARCH}.ipk"
rm "../${API_PKG}_${VERSION}-${RELEASE}_${ARCH}.tar"

echo "✓ API package created"

# ============================================
# STEP 2: Create Working UI Package
# ============================================
echo ""
echo "Creating UI package with working HTML and proper paths..."

UI_PKG="vuci-app-example-ui"
UI_DIR="$WORK_DIR/$UI_PKG"

# Create directories
mkdir -p "$UI_DIR/data/usr/share/vuci/menu.d"
mkdir -p "$UI_DIR/data/www/vuci-app-example"

# Create menu file
cat > "$UI_DIR/data/usr/share/vuci/menu.d/example.json" << 'EOF'
{
  "services/example": {
    "title": "Example",
    "index": 300,
    "view": "services/Example"
  }
}
EOF

# Create a working HTML file with proper API calls
cat > "$UI_DIR/data/www/vuci-app-example/index.html" << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <title>Example VUCI App</title>
    <style>
        body { font-family: Arial, sans-serif; padding: 20px; background: #f5f5f5; }
        .container { max-width: 900px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #333; border-bottom: 2px solid #007bff; padding-bottom: 10px; }
        .status { background: #f8f9fa; padding: 20px; border-radius: 5px; margin: 20px 0; border-left: 4px solid #007bff; }
        .success { border-left-color: #28a745; background: #d4edda; }
        .error { border-left-color: #dc3545; background: #f8d7da; }
        button { background: #007bff; color: white; padding: 12px 24px; border: none; border-radius: 5px; cursor: pointer; margin: 5px; font-size: 16px; }
        button:hover { background: #0056b3; }
        button:disabled { background: #6c757d; cursor: not-allowed; }
        pre { background: #f8f9fa; padding: 15px; border-radius: 5px; overflow-x: auto; border: 1px solid #dee2e6; }
        .spinner { display: inline-block; width: 20px; height: 20px; border: 3px solid rgba(255,255,255,.3); border-radius: 50%; border-top-color: white; animation: spin 1s ease-in-out infinite; }
        @keyframes spin { to { transform: rotate(360deg); } }
        .test-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 20px; margin: 20px 0; }
        .test-card { background: #f8f9fa; padding: 15px; border-radius: 5px; }
        .test-card h3 { margin-top: 0; color: #495057; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🚀 Example VUCI Application</h1>
        
        <div class="status success">
            <h3>✅ Package Status</h3>
            <p><strong>Version:</strong> 1.0-6 (Working)</p>
            <p><strong>HTML Interface:</strong> Loaded Successfully</p>
            <p>This page confirms the UI package is installed and accessible.</p>
        </div>
        
        <div class="test-grid">
            <div class="test-card">
                <h3>🔧 Test API - Direct</h3>
                <button onclick="testAPIDirect()">Test Direct API</button>
                <div id="direct-result"></div>
            </div>
            
            <div class="test-card">
                <h3>📡 Test API - Proxy</h3>
                <button onclick="testAPIProxy()">Test via Proxy</button>
                <div id="proxy-result"></div>
            </div>
            
            <div class="test-card">
                <h3>🔍 Check API Routes</h3>
                <button onclick="checkRoutes()">Check All Routes</button>
                <div id="routes-result"></div>
            </div>
            
            <div class="test-card">
                <h3>📊 System Status</h3>
                <button onclick="getStatus()">Get Status</button>
                <div id="status-result"></div>
            </div>
        </div>
        
        <div class="status">
            <h3>📝 API Response Log</h3>
            <pre id="api-log">No API calls yet...</pre>
        </div>
        
        <div class="status">
            <h3>🐛 Debug Information</h3>
            <button onclick="debugInfo()">Show Debug Info</button>
            <div id="debug-result"></div>
        </div>
    </div>
    
    <script>
        // Helper function to make API calls
        async function callAPI(endpoint, method = 'GET', data = null) {
            const logElement = document.getElementById('api-log');
            const timestamp = new Date().toLocaleTimeString();
            
            try {
                logElement.textContent = `[${timestamp}] Calling ${endpoint}...\n`;
                
                const options = {
                    method: method,
                    headers: {
                        'Content-Type': 'application/json',
                    }
                };
                
                if (data) {
                    options.body = JSON.stringify(data);
                }
                
                const response = await fetch(endpoint, options);
                const responseText = await response.text();
                
                logElement.textContent += `[${timestamp}] Response Status: ${response.status}\n`;
                logElement.textContent += `[${timestamp}] Response Headers: ${response.headers.get('content-type')}\n`;
                logElement.textContent += `[${timestamp}] Response Body:\n${responseText}\n`;
                
                if (response.headers.get('content-type')?.includes('application/json')) {
                    return JSON.parse(responseText);
                }
                return { raw: responseText, status: response.status };
            } catch (error) {
                logElement.textContent += `[${timestamp}] Error: ${error.message}\n`;
                throw error;
            }
        }
        
        // Test direct API call
        async function testAPIDirect() {
            const resultDiv = document.getElementById('direct-result');
            resultDiv.innerHTML = '<div class="spinner"></div> Testing...';
            
            try {
                const data = await callAPI('/api/example/test');
                resultDiv.innerHTML = '<pre>' + JSON.stringify(data, null, 2) + '</pre>';
            } catch (error) {
                resultDiv.innerHTML = '<p style="color: red;">Error: ' + error.message + '</p>';
            }
        }
        
        // Test via proxy/CGI
        async function testAPIProxy() {
            const resultDiv = document.getElementById('proxy-result');
            resultDiv.innerHTML = '<div class="spinner"></div> Testing...';
            
            try {
                const data = await callAPI('/cgi-bin/luci/api/example/test');
                resultDiv.innerHTML = '<pre>' + JSON.stringify(data, null, 2) + '</pre>';
            } catch (error) {
                resultDiv.innerHTML = '<p style="color: red;">Error: ' + error.message + '</p>';
            }
        }
        
        // Check all possible routes
        async function checkRoutes() {
            const resultDiv = document.getElementById('routes-result');
            resultDiv.innerHTML = '<div class="spinner"></div> Checking...';
            
            const routes = [
                '/api/example/test',
                '/api/example/status',
                '/api/example/config',
                '/ubus',
                '/cgi-bin/luci/api/example/test'
            ];
            
            let results = [];
            for (const route of routes) {
                try {
                    const response = await fetch(route);
                    results.push(`${route}: ${response.status}`);
                } catch (error) {
                    results.push(`${route}: Failed`);
                }
            }
            
            resultDiv.innerHTML = '<pre>' + results.join('\n') + '</pre>';
        }
        
        // Get system status
        async function getStatus() {
            const resultDiv = document.getElementById('status-result');
            resultDiv.innerHTML = '<div class="spinner"></div> Loading...';
            
            try {
                const data = await callAPI('/api/example/status');
                resultDiv.innerHTML = '<pre>' + JSON.stringify(data, null, 2) + '</pre>';
            } catch (error) {
                resultDiv.innerHTML = '<p style="color: red;">Error: ' + error.message + '</p>';
            }
        }
        
        // Show debug information
        function debugInfo() {
            const resultDiv = document.getElementById('debug-result');
            
            const info = {
                'Current URL': window.location.href,
                'Protocol': window.location.protocol,
                'Host': window.location.host,
                'User Agent': navigator.userAgent,
                'Cookies': document.cookie || 'None',
                'Local Time': new Date().toString()
            };
            
            resultDiv.innerHTML = '<pre>' + JSON.stringify(info, null, 2) + '</pre>';
        }
        
        // Auto-load debug info on page load
        window.onload = function() {
            debugInfo();
        };
    </script>
</body>
</html>
EOF

# Create control file
cat > "$UI_DIR/control" << EOF
Package: ${UI_PKG}
Version: ${VERSION}-${RELEASE}
Depends: libc, vuci-ui-core, ${API_PKG}
Section: vuci
Architecture: ${ARCH}
Installed-Size: 8192
Description: Example VUCI UI with working HTML
EOF

# Create postinst that creates all necessary symlinks
cat > "$UI_DIR/postinst" << 'EOF'
#!/bin/sh
echo "==== Installing Example UI ===="
[ -z "$IPKG_INSTROOT" ] && {
    # Create symlink for menu
    mkdir -p /overlay/root/upper/usr/share/vuci/menu.d/ 2>/dev/null
    ln -sf /usr/local/usr/share/vuci/menu.d/example.json /overlay/root/upper/usr/share/vuci/menu.d/example.json 2>/dev/null
    
    # Create symlink for HTML access
    ln -sf /usr/local/www/vuci-app-example/index.html /overlay/root/upper/www/example.html 2>/dev/null
    
    # Also create in /www directly if possible
    ln -sf /usr/local/www/vuci-app-example/index.html /www/example.html 2>/dev/null || true
    
    # Clear cache
    rm -rf /tmp/luci-* /tmp/vuci-* 2>/dev/null || true
    
    # Restart web server
    /etc/init.d/uhttpd restart 2>/dev/null || true
    
    echo "UI symlinks created"
    echo ""
    echo "========================================="
    echo "Example VUCI App installed!"
    echo "========================================="
    echo "Access the app at:"
    echo "  http://192.168.80.1/vuci-app-example/"
    echo "  http://192.168.80.1/example.html (if symlink works)"
    echo ""
}
exit 0
EOF
chmod +x "$UI_DIR/postinst"

echo "2.0" > "$UI_DIR/debian-binary"

# Build UI package
cd "$UI_DIR"
tar -czf control.tar.gz --owner=root --group=root control postinst
tar -czf data.tar.gz --owner=root --group=root -C data .
tar -cf "../${UI_PKG}_${VERSION}-${RELEASE}_${ARCH}.tar" debian-binary control.tar.gz data.tar.gz
gzip -c "../${UI_PKG}_${VERSION}-${RELEASE}_${ARCH}.tar" > "$BUILD_DIR/${UI_PKG}_${VERSION}-${RELEASE}_${ARCH}.ipk"
rm "../${UI_PKG}_${VERSION}-${RELEASE}_${ARCH}.tar"

echo "✓ UI package created"

# ============================================
# STEP 3: Create API Registration Package
# ============================================
echo ""
echo "Creating API registration helper..."

REG_PKG="vuci-app-example-register"
REG_DIR="$WORK_DIR/$REG_PKG"

mkdir -p "$REG_DIR/data/etc/init.d"

# Create init script to register API
cat > "$REG_DIR/data/etc/init.d/example-api" << 'EOF'
#!/bin/sh /etc/rc.common

START=99
STOP=10

start() {
    echo "Starting Example API registration..."
    
    # Create API handler script if needed
    mkdir -p /www/cgi-bin 2>/dev/null
    
    cat > /www/cgi-bin/example-api << 'SCRIPT'
#!/usr/bin/lua
-- CGI handler for Example API
local json = require("json")

-- Get request info
local method = os.getenv("REQUEST_METHOD") or "GET"
local path = os.getenv("PATH_INFO") or ""
local query = os.getenv("QUERY_STRING") or ""

-- Load our API module
local ok, api = pcall(require, "api.services.example")

-- Send headers
print("Content-Type: application/json")
print("")

if ok and api and api.handle then
    local result = api.handle(method, path, query, nil)
    print(json.encode(result))
else
    print(json.encode({error = "API module not loaded", ok = ok}))
end
SCRIPT
    
    chmod +x /www/cgi-bin/example-api 2>/dev/null
    
    echo "Example API registered"
}

stop() {
    echo "Stopping Example API..."
    rm -f /www/cgi-bin/example-api 2>/dev/null
}

restart() {
    stop
    start
}
EOF
chmod +x "$REG_DIR/data/etc/init.d/example-api"

# Create control file
cat > "$REG_DIR/control" << EOF
Package: ${REG_PKG}
Version: ${VERSION}-${RELEASE}
Depends: libc, ${API_PKG}
Section: vuci
Architecture: ${ARCH}
Installed-Size: 1024
Description: Example API registration helper
EOF

# Create postinst
cat > "$REG_DIR/postinst" << 'EOF'
#!/bin/sh
[ -z "$IPKG_INSTROOT" ] && {
    /etc/init.d/example-api enable
    /etc/init.d/example-api start
}
exit 0
EOF
chmod +x "$REG_DIR/postinst"

echo "2.0" > "$REG_DIR/debian-binary"

# Build registration package
cd "$REG_DIR"
tar -czf control.tar.gz --owner=root --group=root control postinst
tar -czf data.tar.gz --owner=root --group=root -C data .
tar -cf "../${REG_PKG}_${VERSION}-${RELEASE}_${ARCH}.tar" debian-binary control.tar.gz data.tar.gz
gzip -c "../${REG_PKG}_${VERSION}-${RELEASE}_${ARCH}.tar" > "$BUILD_DIR/${REG_PKG}_${VERSION}-${RELEASE}_${ARCH}.ipk"
rm "../${REG_PKG}_${VERSION}-${RELEASE}_${ARCH}.tar"

echo "✓ Registration package created"

# ============================================
# STEP 4: Summary
# ============================================
echo ""
echo "========================================="
echo "BUILD COMPLETE!"
echo "========================================="
echo ""
echo "Packages created in: $BUILD_DIR"
ls -la "$BUILD_DIR"/*_${VERSION}-${RELEASE}_*.ipk

echo ""
echo "This build includes:"
echo "  ✓ API package with proper routing"
echo "  ✓ UI package with working HTML interface"
echo "  ✓ Registration helper for API endpoints"
echo "  ✓ Automatic symlink creation in postinst"
echo "  ✓ Debug logging and testing interface"
echo ""
echo "The HTML interface includes:"
echo "  - Multiple API test methods"
echo "  - Debug information display"
echo "  - Route checking functionality"
echo "  - Real-time API response logging"
echo ""

# Clean up
rm -rf "$WORK_DIR"


