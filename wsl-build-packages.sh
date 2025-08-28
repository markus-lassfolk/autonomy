#!/bin/bash
# Build VUCI packages using SDK-RUTOS WSL
# This script creates and builds the packages in the proper SDK environment

set -e

SDK_DIR="/mnt/wsl/SDK/src/rutos-ipq40xx-rutx-sdk"
OUTPUT_DIR="/mnt/wsl/SDK/work/packages"

echo "========================================="
echo "VUCI PACKAGE BUILD SCRIPT"
echo "========================================="
echo ""
echo "SDK: $SDK_DIR"
echo "Output: $OUTPUT_DIR"
echo ""

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Change to SDK directory
cd "$SDK_DIR"

# Clean up old packages
echo "Cleaning old packages..."
rm -rf package/vuci-app-simple-api package/vuci-app-simple-ui

# ===== CREATE API PACKAGE =====
echo "Creating API package..."
mkdir -p package/vuci-app-simple-api/files/usr/lib/lua/api/services

cat > package/vuci-app-simple-api/files/usr/lib/lua/api/services/simple.lua << 'EOFLUA'
-- Simple API Service
local M = {}

function M.test()
    return {
        success = true,
        message = "Simple API working",
        timestamp = os.time()
    }
end

function M.get_status()
    return {
        status = "active", 
        message = "Example API is working!",
        version = "1.0-9"
    }
end

function M.get_config()
    return {
        enabled = true,
        port = 8088,
        description = "Simple VUCI Example"
    }
end

function M.handle(method, path, query, body)
    if path:match("/test") then
        return M.test()
    elseif path:match("/status") then
        return M.get_status()
    elseif path:match("/config") then
        return M.get_config()
    else
        return {error = "Unknown endpoint", path = path}
    end
end

return M
EOFLUA

cat > package/vuci-app-simple-api/Makefile << 'EOFMAKE'
include $(TOPDIR)/rules.mk

PKG_NAME:=vuci-app-simple-api
PKG_VERSION:=1.0
PKG_RELEASE:=9
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/vuci-app-simple-api
  SECTION:=vuci
  CATEGORY:=VUCI
  TITLE:=Simple VUCI API
  DEPENDS:=+lua
endef

define Package/vuci-app-simple-api/description
  Simple VUCI API service for testing
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Configure
endef

define Build/Compile
endef

define Package/vuci-app-simple-api/install
	$(INSTALL_DIR) $(1)/usr/lib/lua/api/services
	$(INSTALL_DATA) ./files/usr/lib/lua/api/services/simple.lua $(1)/usr/lib/lua/api/services/
endef

$(eval $(call BuildPackage,vuci-app-simple-api))
EOFMAKE

# ===== CREATE UI PACKAGE =====
echo "Creating UI package..."
mkdir -p package/vuci-app-simple-ui/files/usr/share/vuci/menu.d
mkdir -p package/vuci-app-simple-ui/files/www/simple

cat > package/vuci-app-simple-ui/files/usr/share/vuci/menu.d/simple.json << 'EOFJSON'
{"services/simple":{"title":"Simple","index":400,"view":"services/Simple","acls":["services/simple"]}}
EOFJSON

cat > package/vuci-app-simple-ui/files/www/simple/index.html << 'EOFHTML'
<!DOCTYPE html>
<html>
<head>
    <title>Simple VUCI App</title>
    <style>
        body { 
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
            padding: 20px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
        }
        .container {
            max-width: 800px;
            margin: 0 auto;
            background: white;
            padding: 30px;
            border-radius: 10px;
            box-shadow: 0 20px 40px rgba(0,0,0,0.1);
        }
        h1 { color: #333; }
        button {
            background: #667eea;
            color: white;
            padding: 12px 24px;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            margin: 5px;
            font-size: 16px;
            transition: all 0.3s;
        }
        button:hover {
            background: #764ba2;
            transform: translateY(-2px);
        }
        .status { 
            padding: 10px;
            margin: 10px 0;
            border-radius: 5px;
            background: #f0f0f0;
        }
        .success { background: #d4edda; color: #155724; }
        .error { background: #f8d7da; color: #721c24; }
        pre {
            background: #f5f5f5;
            padding: 15px;
            border-radius: 5px;
            overflow-x: auto;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🚀 Simple VUCI Application</h1>
        <div class="status">
            <strong>Version:</strong> 1.0-9<br>
            <strong>Status:</strong> <span id="status">Checking...</span>
        </div>
        
        <h2>API Tests</h2>
        <button onclick="testAPI('status')">Get Status</button>
        <button onclick="testAPI('test')">Test Endpoint</button>
        <button onclick="testAPI('config')">Get Config</button>
        <button onclick="testDirect()">Test Direct Port 8088</button>
        
        <div id="result"></div>
    </div>
    
    <script>
        // Check status on load
        window.onload = function() {
            checkAPIStatus();
        };
        
        async function checkAPIStatus() {
            const statusEl = document.getElementById('status');
            try {
                const response = await fetch('http://' + window.location.hostname + ':8088/status');
                if (response.ok) {
                    statusEl.innerHTML = '<span style="color: green;">✓ API Server Running on Port 8088</span>';
                } else {
                    statusEl.innerHTML = '<span style="color: orange;">⚠ API Server Not Responding</span>';
                }
            } catch (e) {
                statusEl.innerHTML = '<span style="color: red;">✗ API Server Offline</span>';
            }
        }
        
        async function testAPI(endpoint) {
            const resultDiv = document.getElementById('result');
            resultDiv.innerHTML = '<div class="status">Testing ' + endpoint + '...</div>';
            
            try {
                // Try multiple endpoints
                const endpoints = [
                    'http://' + window.location.hostname + ':8088/' + endpoint,
                    '/cgi-bin/example-api/' + endpoint,
                    '/api/simple/' + endpoint
                ];
                
                let success = false;
                let data = null;
                
                for (const url of endpoints) {
                    try {
                        const response = await fetch(url);
                        if (response.ok) {
                            data = await response.json();
                            success = true;
                            resultDiv.innerHTML = '<div class="status success"><strong>Success from ' + url + '</strong></div><pre>' + JSON.stringify(data, null, 2) + '</pre>';
                            break;
                        }
                    } catch (e) {
                        console.log('Failed: ' + url);
                    }
                }
                
                if (!success) {
                    resultDiv.innerHTML = '<div class="status error">All endpoints failed. API server may not be running.</div>';
                }
            } catch (error) {
                resultDiv.innerHTML = '<div class="status error">Error: ' + error.message + '</div>';
            }
        }
        
        async function testDirect() {
            const resultDiv = document.getElementById('result');
            resultDiv.innerHTML = '<div class="status">Testing direct port 8088...</div>';
            
            try {
                const response = await fetch('http://' + window.location.hostname + ':8088/status');
                const data = await response.json();
                resultDiv.innerHTML = '<div class="status success"><strong>Direct API Success!</strong></div><pre>' + JSON.stringify(data, null, 2) + '</pre>';
            } catch (error) {
                resultDiv.innerHTML = '<div class="status error">Port 8088 not responding. Run the API server fix script.</div>';
            }
        }
    </script>
</body>
</html>
EOFHTML

cat > package/vuci-app-simple-ui/Makefile << 'EOFMAKE'
include $(TOPDIR)/rules.mk

PKG_NAME:=vuci-app-simple-ui
PKG_VERSION:=1.0
PKG_RELEASE:=9
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/vuci-app-simple-ui
  SECTION:=vuci
  CATEGORY:=VUCI
  TITLE:=Simple VUCI UI
  DEPENDS:=+vuci-app-simple-api
endef

define Package/vuci-app-simple-ui/description
  Simple VUCI UI for testing
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Configure
endef

define Build/Compile
endef

define Package/vuci-app-simple-ui/install
	$(INSTALL_DIR) $(1)/usr/share/vuci/menu.d
	$(INSTALL_DATA) ./files/usr/share/vuci/menu.d/simple.json $(1)/usr/share/vuci/menu.d/
	$(INSTALL_DIR) $(1)/www/simple
	$(INSTALL_DATA) ./files/www/simple/index.html $(1)/www/simple/
endef

define Package/vuci-app-simple-ui/postinst
#!/bin/sh
[ -z "$$$${IPKG_INSTROOT}" ] && {
    echo "Creating symlinks for VUCI..."
    mkdir -p /overlay/root/upper/usr/share/vuci/menu.d 2>/dev/null
    ln -sf /usr/local/usr/share/vuci/menu.d/simple.json /overlay/root/upper/usr/share/vuci/menu.d/simple.json 2>/dev/null
    
    echo "Restarting web server..."
    /etc/init.d/uhttpd restart 2>/dev/null
    
    echo "Simple UI installed successfully!"
}
exit 0
endef

$(eval $(call BuildPackage,vuci-app-simple-ui))
EOFMAKE

# ===== BUILD PACKAGES =====
echo ""
echo "Building packages with make..."

# Try to build with make
echo "Building API package..."
if make -j$(nproc) package/vuci-app-simple-api/compile V=s 2>&1 | tail -20; then
    echo "API package built successfully"
else
    echo "API package build had issues"
fi

echo ""
echo "Building UI package..."
if make -j$(nproc) package/vuci-app-simple-ui/compile V=s 2>&1 | tail -20; then
    echo "UI package built successfully"
else
    echo "UI package build had issues"
fi

# Check if packages were built
echo ""
echo "Checking for built packages..."
if find bin/packages -name "vuci-app-simple*.ipk" 2>/dev/null | grep -q .; then
    echo "Packages found!"
    find bin/packages -name "vuci-app-simple*.ipk" -exec cp {} "$OUTPUT_DIR/" \;
else
    echo "No packages found, building manually..."
    
    # Manual build fallback
    cd package/vuci-app-simple-api
    mkdir -p build
    cp -r files/* build/
    cd build
    
    # Create control file
    mkdir -p CONTROL
    cat > CONTROL/control << EOF
Package: vuci-app-simple-api
Version: 1.0-9
Depends: libc, lua
Section: vuci
Architecture: arm_cortex-a7_neon-vfpv4
Installed-Size: 1024
Description: Simple VUCI API service
EOF
    
    # Create IPK
    cd ..
    tar -czf "$OUTPUT_DIR/vuci-app-simple-api_1.0-9_arm_cortex-a7_neon-vfpv4.ipk" -C build .
    
    # Build UI package
    cd "$SDK_DIR/package/vuci-app-simple-ui"
    mkdir -p build
    cp -r files/* build/
    cd build
    
    # Create control file
    mkdir -p CONTROL
    cat > CONTROL/control << EOF
Package: vuci-app-simple-ui
Version: 1.0-9
Depends: libc, vuci-app-simple-api
Section: vuci
Architecture: arm_cortex-a7_neon-vfpv4
Installed-Size: 2048
Description: Simple VUCI UI
EOF
    
    cat > CONTROL/postinst << 'EOF'
#!/bin/sh
[ -z "${IPKG_INSTROOT}" ] && {
    mkdir -p /overlay/root/upper/usr/share/vuci/menu.d 2>/dev/null
    ln -sf /usr/local/usr/share/vuci/menu.d/simple.json /overlay/root/upper/usr/share/vuci/menu.d/simple.json 2>/dev/null
    /etc/init.d/uhttpd restart 2>/dev/null
}
exit 0
EOF
    chmod +x CONTROL/postinst
    
    # Create IPK
    cd ..
    tar -czf "$OUTPUT_DIR/vuci-app-simple-ui_1.0-9_arm_cortex-a7_neon-vfpv4.ipk" -C build .
fi

echo ""
echo "========================================="
echo "BUILD COMPLETE"
echo "========================================="
echo ""
echo "Packages in $OUTPUT_DIR:"
ls -la "$OUTPUT_DIR"/vuci-app-simple*.ipk 2>/dev/null || echo "No packages found"
echo ""
echo "Next steps:"
echo "1. Deploy packages to router"
echo "2. Run API server fix on router"
echo "3. Access http://router-ip/simple/"


