#!/bin/bash
# Manual build script with debug logging for opkg installation
# This helps identify why files aren't being installed to expected locations

set -e

echo "========================================="
echo "MANUAL VUCI PACKAGE BUILD (Debug Version)"
echo "========================================="
echo ""

# Configuration
BUILD_DIR="/home/markusla/vuci-packages"
WORK_DIR="/tmp/vuci-manual-build-$$"
VERSION="1.0"
RELEASE="5"
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
# STEP 1: Create Simple Example API Package
# ============================================
echo "Creating Example API package..."

API_PKG="vuci-app-example-api"
API_DIR="$WORK_DIR/$API_PKG"

mkdir -p "$API_DIR/data/usr/lib/lua/api/services"

# Create the API service
cat > "$API_DIR/data/usr/lib/lua/api/services/example.lua" << 'EOF'
-- Example API Service for VUCI
local M = {}

-- Simple test endpoint
function M.test()
    return {
        success = true,
        message = "Example API is working!",
        timestamp = os.time(),
        version = "1.0.5"
    }
end

-- Status endpoint
function M.status()
    return {
        running = true,
        uptime = os.time(),
        memory = collectgarbage("count")
    }
end

-- Configuration endpoint
function M.config()
    return {
        enabled = true,
        interval = 60,
        debug = false
    }
end

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
Description: Example VUCI API service
EOF

# Create postinst with debug logging
cat > "$API_DIR/postinst" << 'EOF'
#!/bin/sh
echo "==== vuci-app-example-api postinst ===="
echo "IPKG_INSTROOT: $IPKG_INSTROOT"
echo "Package files should be installed to:"
echo "  /usr/lib/lua/api/services/example.lua"
echo "Checking if file exists:"
ls -la /usr/lib/lua/api/services/example.lua 2>/dev/null || echo "  File not found!"
echo "==== End postinst ===="
exit 0
EOF
chmod +x "$API_DIR/postinst"

# Create preinst for debugging
cat > "$API_DIR/preinst" << 'EOF'
#!/bin/sh
echo "==== vuci-app-example-api preinst ===="
echo "Preparing to install API package..."
echo "Target directory: /usr/lib/lua/api/services/"
mkdir -p /usr/lib/lua/api/services/ 2>/dev/null || true
echo "==== End preinst ===="
exit 0
EOF
chmod +x "$API_DIR/preinst"

echo "2.0" > "$API_DIR/debian-binary"

# Build API package
cd "$API_DIR"
tar -czf control.tar.gz --owner=root --group=root control postinst preinst
tar -czf data.tar.gz --owner=root --group=root -C data .

# Debug: Show contents of data.tar.gz
echo "API package data contents:"
tar -tzf data.tar.gz | head -10

tar -cf "../${API_PKG}_${VERSION}-${RELEASE}_${ARCH}.tar" debian-binary control.tar.gz data.tar.gz
gzip -c "../${API_PKG}_${VERSION}-${RELEASE}_${ARCH}.tar" > "$BUILD_DIR/${API_PKG}_${VERSION}-${RELEASE}_${ARCH}.ipk"
rm "../${API_PKG}_${VERSION}-${RELEASE}_${ARCH}.tar"

echo "✓ API package created"

# ============================================
# STEP 2: Create Simple Example UI Package
# ============================================
echo ""
echo "Creating Example UI package..."

UI_PKG="vuci-app-example-ui"
UI_DIR="$WORK_DIR/$UI_PKG"

# Create directories
mkdir -p "$UI_DIR/data/usr/share/vuci/menu.d"
mkdir -p "$UI_DIR/data/www/vuci-app-example"

# Create menu file (correct location)
cat > "$UI_DIR/data/usr/share/vuci/menu.d/example.json" << 'EOF'
{
  "services/example": {
    "title": "Example",
    "index": 300,
    "view": "services/Example"
  }
}
EOF

# Create a simple HTML file as a fallback
cat > "$UI_DIR/data/www/vuci-app-example/index.html" << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <title>Example VUCI App</title>
    <style>
        body { font-family: Arial, sans-serif; padding: 20px; }
        .container { max-width: 800px; margin: 0 auto; }
        .status { background: #f0f0f0; padding: 15px; border-radius: 5px; margin: 10px 0; }
        button { background: #007bff; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; }
        button:hover { background: #0056b3; }
        pre { background: #f8f9fa; padding: 10px; border-radius: 3px; overflow-x: auto; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Example VUCI Application</h1>
        <div class="status">
            <h3>Status</h3>
            <p>Package Version: 1.0-5 (Debug)</p>
            <p>This is a test page for the Example VUCI application.</p>
            <p>If you see this page, the UI package was installed successfully!</p>
        </div>
        
        <div class="status">
            <h3>Test API</h3>
            <button onclick="testAPI()">Test API Endpoint</button>
            <div id="api-result"></div>
        </div>
        
        <div class="status">
            <h3>Debug Information</h3>
            <button onclick="checkFiles()">Check Installed Files</button>
            <div id="file-result"></div>
        </div>
    </div>
    
    <script>
        function testAPI() {
            document.getElementById('api-result').innerHTML = '<p>Testing API...</p>';
            fetch('/api/example/test')
                .then(response => {
                    if (!response.ok) throw new Error('HTTP ' + response.status);
                    return response.json();
                })
                .then(data => {
                    document.getElementById('api-result').innerHTML = 
                        '<pre>' + JSON.stringify(data, null, 2) + '</pre>';
                })
                .catch(error => {
                    document.getElementById('api-result').innerHTML = 
                        '<p style="color: red;">Error: ' + error + '</p>' +
                        '<p>The API endpoint may not be properly configured.</p>';
                });
        }
        
        function checkFiles() {
            var files = [
                '/usr/share/vuci/menu.d/example.json',
                '/usr/lib/lua/api/services/example.lua',
                '/www/vuci-app-example/index.html'
            ];
            
            var result = '<h4>Expected files:</h4><ul>';
            files.forEach(function(file) {
                result += '<li>' + file + '</li>';
            });
            result += '</ul>';
            result += '<p>Check router logs for actual installation paths.</p>';
            
            document.getElementById('file-result').innerHTML = result;
        }
    </script>
</body>
</html>
EOF

# Create a simple Vue component (uncompiled, for reference)
mkdir -p "$UI_DIR/data/usr/share/vuci/components"
cat > "$UI_DIR/data/usr/share/vuci/components/Example.vue" << 'EOF'
<template>
  <div class="example-app">
    <h2>Example Application</h2>
    <p>This is the Example VUCI component.</p>
    <p>Status: {{ status }}</p>
    <button @click="testAPI">Test API</button>
    <div v-if="apiResult">{{ apiResult }}</div>
  </div>
</template>

<script>
export default {
  name: 'Example',
  data() {
    return {
      status: 'Ready',
      apiResult: null
    }
  },
  methods: {
    async testAPI() {
      try {
        const response = await fetch('/api/example/test')
        const data = await response.json()
        this.apiResult = JSON.stringify(data, null, 2)
      } catch (error) {
        this.apiResult = 'Error: ' + error.message
      }
    }
  }
}
</script>
EOF

# Create control file
cat > "$UI_DIR/control" << EOF
Package: ${UI_PKG}
Version: ${VERSION}-${RELEASE}
Depends: libc, vuci-ui-core, ${API_PKG}
Section: vuci
Architecture: ${ARCH}
Installed-Size: 4096
Description: Example VUCI UI (Debug version)
EOF

# Create postinst with extensive debug logging
cat > "$UI_DIR/postinst" << 'EOF'
#!/bin/sh
echo "==== vuci-app-example-ui postinst ===="
echo "IPKG_INSTROOT: $IPKG_INSTROOT"
echo ""
echo "Expected installation paths:"
echo "  Menu: /usr/share/vuci/menu.d/example.json"
echo "  HTML: /www/vuci-app-example/index.html"
echo "  Vue:  /usr/share/vuci/components/Example.vue"
echo ""
echo "Checking actual installation:"
echo ""
echo "1. Menu file:"
if [ -f /usr/share/vuci/menu.d/example.json ]; then
    echo "   FOUND: /usr/share/vuci/menu.d/example.json"
    echo "   Contents:"
    cat /usr/share/vuci/menu.d/example.json
else
    echo "   NOT FOUND in /usr/share/vuci/menu.d/"
    echo "   Checking other locations:"
    find / -name "example.json" 2>/dev/null | head -5
fi
echo ""
echo "2. HTML file:"
if [ -f /www/vuci-app-example/index.html ]; then
    echo "   FOUND: /www/vuci-app-example/index.html"
else
    echo "   NOT FOUND in /www/vuci-app-example/"
    echo "   Checking other locations:"
    find / -name "index.html" -path "*/vuci-app-example/*" 2>/dev/null | head -5
fi
echo ""
echo "3. Vue component:"
if [ -f /usr/share/vuci/components/Example.vue ]; then
    echo "   FOUND: /usr/share/vuci/components/Example.vue"
else
    echo "   NOT FOUND in /usr/share/vuci/components/"
fi
echo ""
echo "Creating symlink for HTML fallback..."
ln -sf /www/vuci-app-example/index.html /www/example.html 2>/dev/null || echo "   Failed to create symlink"
echo ""
echo "Listing all installed package files:"
echo "(This shows where opkg actually placed the files)"
opkg files ${UI_PKG} 2>/dev/null || echo "   Could not list files"
echo ""
[ -z "$IPKG_INSTROOT" ] && {
    echo "Clearing cache..."
    rm -rf /tmp/luci-* /tmp/vuci-* 2>/dev/null || true
    
    echo "Restarting web server..."
    /etc/init.d/uhttpd restart 2>/dev/null || true
    
    echo ""
    echo "========================================="
    echo "Example VUCI App installed!"
    echo "========================================="
    echo ""
    echo "The app should appear under Services -> Example"
    echo "Fallback HTML page: http://router-ip/vuci-app-example/"
    echo ""
}
echo "==== End postinst ===="
exit 0
EOF
chmod +x "$UI_DIR/postinst"

# Create preinst for debugging
cat > "$UI_DIR/preinst" << 'EOF'
#!/bin/sh
echo "==== vuci-app-example-ui preinst ===="
echo "Preparing to install UI package..."
echo "Creating target directories:"
mkdir -p /usr/share/vuci/menu.d/ 2>/dev/null && echo "  Created /usr/share/vuci/menu.d/" || echo "  Failed to create /usr/share/vuci/menu.d/"
mkdir -p /www/vuci-app-example/ 2>/dev/null && echo "  Created /www/vuci-app-example/" || echo "  Failed to create /www/vuci-app-example/"
mkdir -p /usr/share/vuci/components/ 2>/dev/null && echo "  Created /usr/share/vuci/components/" || echo "  Failed to create /usr/share/vuci/components/"
echo "==== End preinst ===="
exit 0
EOF
chmod +x "$UI_DIR/preinst"

echo "2.0" > "$UI_DIR/debian-binary"

# Build UI package
cd "$UI_DIR"
tar -czf control.tar.gz --owner=root --group=root control postinst preinst
tar -czf data.tar.gz --owner=root --group=root -C data .

# Debug: Show contents of data.tar.gz
echo "UI package data contents:"
tar -tzf data.tar.gz | head -15

tar -cf "../${UI_PKG}_${VERSION}-${RELEASE}_${ARCH}.tar" debian-binary control.tar.gz data.tar.gz
gzip -c "../${UI_PKG}_${VERSION}-${RELEASE}_${ARCH}.tar" > "$BUILD_DIR/${UI_PKG}_${VERSION}-${RELEASE}_${ARCH}.ipk"
rm "../${UI_PKG}_${VERSION}-${RELEASE}_${ARCH}.tar"

echo "✓ UI package created"

# ============================================
# STEP 3: Summary
# ============================================
echo ""
echo "========================================="
echo "BUILD COMPLETE! (Debug Version)"
echo "========================================="
echo ""
echo "Packages created in: $BUILD_DIR"
ls -la "$BUILD_DIR"/*.ipk

echo ""
echo "Debug features added:"
echo "  ✓ Extensive logging in pre/post install scripts"
echo "  ✓ File existence checks during installation"
echo "  ✓ Package file listing after installation"
echo "  ✓ Debug buttons in HTML interface"
echo ""
echo "Installation will show detailed logs about:"
echo "  - Where files are being installed"
echo "  - Which files are found/missing"
echo "  - Any permission or path issues"
echo ""

# Clean up
rm -rf "$WORK_DIR"


