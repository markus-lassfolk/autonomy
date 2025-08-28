#!/bin/bash
# Manual build script that creates properly structured VUCI packages
# This works around SDK toolchain issues while still creating correct packages

set -e

echo "========================================="
echo "MANUAL VUCI PACKAGE BUILD (Proper Structure)"
echo "========================================="
echo ""

# Configuration
BUILD_DIR="/home/markusla/vuci-packages"
WORK_DIR="/tmp/vuci-manual-build-$$"
VERSION="1.0"
RELEASE="4"
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
        version = "1.0.4"
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

# Create postinst
cat > "$API_DIR/postinst" << 'EOF'
#!/bin/sh
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
    </style>
</head>
<body>
    <div class="container">
        <h1>Example VUCI Application</h1>
        <div class="status">
            <h3>Status</h3>
            <p>Package Version: 1.0-4</p>
            <p>This is a test page for the Example VUCI application.</p>
            <p>If you see this page, the package was installed successfully!</p>
        </div>
        
        <div class="status">
            <h3>Test API</h3>
            <button onclick="testAPI()">Test API Endpoint</button>
            <div id="api-result"></div>
        </div>
    </div>
    
    <script>
        function testAPI() {
            fetch('/api/example/test')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('api-result').innerHTML = 
                        '<pre>' + JSON.stringify(data, null, 2) + '</pre>';
                })
                .catch(error => {
                    document.getElementById('api-result').innerHTML = 
                        '<p style="color: red;">Error: ' + error + '</p>';
                });
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
Description: Example VUCI UI
EOF

# Create postinst that adds a redirect
cat > "$UI_DIR/postinst" << 'EOF'
#!/bin/sh
[ -z "$IPKG_INSTROOT" ] && {
    # Clear any cache
    rm -rf /tmp/luci-* /tmp/vuci-* 2>/dev/null || true
    
    # Create a symlink for the HTML fallback
    ln -sf /www/vuci-app-example/index.html /www/example.html 2>/dev/null || true
    
    # Restart web server
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
# STEP 3: Summary
# ============================================
echo ""
echo "========================================="
echo "BUILD COMPLETE!"
echo "========================================="
echo ""
echo "Packages created in: $BUILD_DIR"
ls -la "$BUILD_DIR"/*.ipk

echo ""
echo "Key features of these packages:"
echo "  ✓ Menu file in correct location (/usr/share/vuci/menu.d/)"
echo "  ✓ API service in standard location"
echo "  ✓ HTML fallback page for testing"
echo "  ✓ Proper package dependencies"
echo "  ✓ Clean installation scripts"
echo ""
echo "Note: These packages include:"
echo "  - Uncompiled Vue component (for reference)"
echo "  - HTML fallback page (accessible directly)"
echo "  - Proper menu registration"
echo ""
echo "The Vue component won't work without webpack compilation,"
echo "but the HTML page provides a way to test the API."
echo ""

# Clean up
rm -rf "$WORK_DIR"


