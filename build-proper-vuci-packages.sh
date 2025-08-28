#!/bin/bash

# Comprehensive VUCI Package Builder
# Based on working packages: ntpd, upnp
# Follows exact SDK patterns and file structures

set -e

SDK_DIR="/home/markusla/rutos-sdk"
PACKAGE_NAME="example"
PACKAGE_TITLE="Example"

echo "=== Building Proper VUCI Packages ==="
echo "Package Name: $PACKAGE_NAME"
echo "Package Title: $PACKAGE_TITLE"
echo "SDK Directory: $SDK_DIR"
echo ""

# Verify SDK exists
if [ ! -d "$SDK_DIR" ]; then
    echo "❌ ERROR: SDK directory not found: $SDK_DIR"
    exit 1
fi

# Verify required SDK files exist
if [ ! -f "$SDK_DIR/package/feeds/vuci/api.mk" ]; then
    echo "❌ ERROR: api.mk not found in SDK"
    exit 1
fi

if [ ! -f "$SDK_DIR/package/feeds/vuci/app.mk" ]; then
    echo "❌ ERROR: app.mk not found in SDK"
    exit 1
fi

echo "✅ SDK verification passed"
echo ""

# Clean previous builds
echo "🧹 Cleaning previous builds..."
rm -rf "$SDK_DIR/package/feeds/vuci/vuci-app-${PACKAGE_NAME}-api"
rm -rf "$SDK_DIR/package/feeds/vuci/vuci-app-${PACKAGE_NAME}-ui"
echo "✅ Cleanup completed"
echo ""

# ============================================================================
# CREATE API PACKAGE
# ============================================================================

echo "📦 Creating API Package..."
API_PACKAGE_DIR="$SDK_DIR/package/feeds/vuci/vuci-app-${PACKAGE_NAME}-api"
mkdir -p "$API_PACKAGE_DIR"

# Create API package Makefile
cat > "$API_PACKAGE_DIR/Makefile" << EOF
include \$(TOPDIR)/rules.mk

APP_TITLE:=VuCI API Support for ${PACKAGE_TITLE} application

include ../api.mk

# call BuildPackage - OpenWrt buildroot signature
EOF

# Create API package files directory structure
mkdir -p "$API_PACKAGE_DIR/files/usr/lib/lua/api/services"

# Create the API service file (based on working ntpd.lua pattern)
cat > "$API_PACKAGE_DIR/files/usr/lib/lua/api/services/${PACKAGE_NAME}.lua" << 'EOF'
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

echo "✅ API package structure created"
echo ""

# ============================================================================
# CREATE UI PACKAGE
# ============================================================================

echo "📦 Creating UI Package..."
UI_PACKAGE_DIR="$SDK_DIR/package/feeds/vuci/vuci-app-${PACKAGE_NAME}-ui"
mkdir -p "$UI_PACKAGE_DIR"

# Create UI package Makefile
cat > "$UI_PACKAGE_DIR/Makefile" << EOF
include \$(TOPDIR)/rules.mk

APP_TITLE:=VuCI UI Support for ${PACKAGE_TITLE} application

include ../app.mk

# call BuildPackage - OpenWrt buildroot signature
EOF

# Create UI package files directory structure
mkdir -p "$UI_PACKAGE_DIR/files/usr/share/vuci/menu.d"

# Create the menu configuration (following working ntpd.json pattern)
cat > "$UI_PACKAGE_DIR/files/usr/share/vuci/menu.d/${PACKAGE_NAME}.json" << EOF
{"services/${PACKAGE_NAME}":{"title":"${PACKAGE_TITLE}","index":100,"view":"services/${PACKAGE_TITLE}","acls":["services/${PACKAGE_NAME}"]}}
EOF

# Create Vue.js source structure
mkdir -p "$UI_PACKAGE_DIR/src/src/views/services"

# Create the Vue component (following VUCI patterns)
cat > "$UI_PACKAGE_DIR/src/src/views/services/${PACKAGE_TITLE}.vue" << 'EOF'
<template>
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
            <button class="btn btn-primary" @click="getStatus">Get Status</button>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script>
export default {
  name: 'Example',
  data() {
    return {
      status: null
    }
  },
  methods: {
    async getStatus() {
      try {
        const response = await this.$axios.get('/api/example/status')
        this.status = response.data
      } catch (error) {
        console.error('Error fetching status:', error)
        this.$message.error('Failed to fetch status')
      }
    }
  }
}
</script>

<style scoped>
.container {
  padding: 20px;
}
</style>
EOF

echo "✅ UI package structure created"
echo ""

# ============================================================================
# VERIFY PACKAGE STRUCTURE
# ============================================================================

echo "🔍 Verifying package structure..."

# Verify API package
echo "Checking API package structure..."
if [ ! -f "$API_PACKAGE_DIR/Makefile" ]; then
    echo "❌ ERROR: API Makefile not found"
    exit 1
fi

if [ ! -f "$API_PACKAGE_DIR/files/usr/lib/lua/api/services/${PACKAGE_NAME}.lua" ]; then
    echo "❌ ERROR: API service file not found"
    exit 1
fi

# Verify UI package
echo "Checking UI package structure..."
if [ ! -f "$UI_PACKAGE_DIR/Makefile" ]; then
    echo "❌ ERROR: UI Makefile not found"
    exit 1
fi

if [ ! -f "$UI_PACKAGE_DIR/files/usr/share/vuci/menu.d/${PACKAGE_NAME}.json" ]; then
    echo "❌ ERROR: UI menu file not found"
    exit 1
fi

if [ ! -f "$UI_PACKAGE_DIR/src/src/views/services/${PACKAGE_TITLE}.vue" ]; then
    echo "❌ ERROR: UI Vue component not found"
    exit 1
fi

echo "✅ Package structure verification passed"
echo ""

# ============================================================================
# BUILD PACKAGES
# ============================================================================

echo "🔨 Building packages..."

# Build API package
echo "Building API package..."
cd "$SDK_DIR"
make package/vuci-app-${PACKAGE_NAME}-api/clean
make package/vuci-app-${PACKAGE_NAME}-api/compile

if [ $? -ne 0 ]; then
    echo "❌ ERROR: API package build failed"
    exit 1
fi

echo "✅ API package built successfully"

# Build UI package
echo "Building UI package..."
make package/vuci-app-${PACKAGE_NAME}-ui/clean
make package/vuci-app-${PACKAGE_NAME}-ui/compile

if [ $? -ne 0 ]; then
    echo "❌ ERROR: UI package build failed"
    exit 1
fi

echo "✅ UI package built successfully"
echo ""

# ============================================================================
# FIND AND VERIFY BUILT PACKAGES
# ============================================================================

echo "🔍 Finding built packages..."

# Find API package
API_PACKAGE=$(find "$SDK_DIR/bin" -name "vuci-app-${PACKAGE_NAME}-api_*.ipk" | head -1)
if [ -z "$API_PACKAGE" ]; then
    echo "❌ ERROR: API package not found in bin directory"
    exit 1
fi

# Find UI package
UI_PACKAGE=$(find "$SDK_DIR/bin" -name "vuci-app-${PACKAGE_NAME}-ui_*.ipk" | head -1)
if [ -z "$UI_PACKAGE" ]; then
    echo "❌ ERROR: UI package not found in bin directory"
    exit 1
fi

echo "✅ Found API package: $(basename "$API_PACKAGE")"
echo "✅ Found UI package: $(basename "$UI_PACKAGE")"
echo ""

# ============================================================================
# VERIFY PACKAGE CONTENTS
# ============================================================================

echo "🔍 Verifying package contents..."

# Create temporary directory for extraction
TEMP_DIR="/tmp/vuci-package-verification"
rm -rf "$TEMP_DIR"
mkdir -p "$TEMP_DIR"

# Extract and verify API package
echo "Verifying API package contents..."
cd "$TEMP_DIR"
mkdir api-package
cd api-package
ar x "$API_PACKAGE"
tar -xzf data.tar.gz

# Check for expected files
if [ ! -f "usr/lib/lua/api/services/${PACKAGE_NAME}.lua" ]; then
    echo "❌ ERROR: API service file not found in package"
    exit 1
fi

echo "✅ API package contents verified"

# Extract and verify UI package
echo "Verifying UI package contents..."
cd "$TEMP_DIR"
mkdir ui-package
cd ui-package
ar x "$UI_PACKAGE"
tar -xzf data.tar.gz

# Check for expected files
if [ ! -f "usr/share/vuci/menu.d/${PACKAGE_NAME}.json" ]; then
    echo "❌ ERROR: UI menu file not found in package"
    exit 1
fi

echo "✅ UI package contents verified"

# Clean up
rm -rf "$TEMP_DIR"

echo ""

# ============================================================================
# COPY PACKAGES TO CURRENT DIRECTORY
# ============================================================================

echo "📋 Copying packages to current directory..."
cp "$API_PACKAGE" .
cp "$UI_PACKAGE" .

API_PACKAGE_NAME=$(basename "$API_PACKAGE")
UI_PACKAGE_NAME=$(basename "$UI_PACKAGE")

echo "✅ Copied $API_PACKAGE_NAME"
echo "✅ Copied $UI_PACKAGE_NAME"
echo ""

# ============================================================================
# FINAL VERIFICATION
# ============================================================================

echo "🔍 Final verification..."

# Check file sizes
API_SIZE=$(stat -c%s "$API_PACKAGE_NAME")
UI_SIZE=$(stat -c%s "$UI_PACKAGE_NAME")

echo "API package size: $API_SIZE bytes"
echo "UI package size: $UI_SIZE bytes"

if [ $API_SIZE -lt 1000 ]; then
    echo "⚠️  WARNING: API package seems too small"
fi

if [ $UI_SIZE -lt 1000 ]; then
    echo "⚠️  WARNING: UI package seems too small"
fi

# Verify package structure
echo "Verifying package structure..."
if file "$API_PACKAGE_NAME" | grep -q "ar archive"; then
    echo "✅ API package is valid ar archive"
else
    echo "❌ ERROR: API package is not a valid ar archive"
    exit 1
fi

if file "$UI_PACKAGE_NAME" | grep -q "ar archive"; then
    echo "✅ UI package is valid ar archive"
else
    echo "❌ ERROR: UI package is not a valid ar archive"
    exit 1
fi

echo ""

# ============================================================================
# SUMMARY
# ============================================================================

echo "🎉 BUILD COMPLETED SUCCESSFULLY!"
echo ""
echo "📦 Generated Packages:"
echo "  API: $API_PACKAGE_NAME ($API_SIZE bytes)"
echo "  UI:  $UI_PACKAGE_NAME ($UI_SIZE bytes)"
echo ""
echo "📁 Package Structure:"
echo "  API Service: /usr/lib/lua/api/services/${PACKAGE_NAME}.lua"
echo "  Menu Config: /usr/share/vuci/menu.d/${PACKAGE_NAME}.json"
echo "  Vue Component: src/src/views/services/${PACKAGE_TITLE}.vue"
echo ""
echo "🔧 Build System:"
echo "  Used SDK api.mk and app.mk"
echo "  Followed working package patterns (ntpd, upnp)"
echo "  Proper file placement in overlay directories"
echo ""
echo "📋 Next Steps:"
echo "  1. Install packages: opkg install $API_PACKAGE_NAME && opkg install $UI_PACKAGE_NAME"
echo "  2. Restart services: /etc/init.d/rpcd restart && /etc/init.d/uhttpd restart"
echo "  3. Access UI: https://192.168.80.1/services/${PACKAGE_NAME}"
echo "  4. Test API: curl -k https://192.168.80.1/api/${PACKAGE_NAME}/status"
echo ""
echo "✅ All verifications passed - packages are ready for deployment!"




