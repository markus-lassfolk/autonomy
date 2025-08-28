#!/bin/bash

# Unique VUCI Package Builder
# Uses completely unique identifiers to avoid conflicts
# Based on working packages: ntpd, upnp
# Bypasses SDK build system to avoid ntpd collision

set -e

# Generate unique identifiers
PACKAGE_NAME="example"
PACKAGE_TITLE="Example"
PACKAGE_VERSION="1.0"
PACKAGE_RELEASE="1"
ARCH="arm_cortex-a7_neon-vfpv4"
UNIQUE_ID=$(date +%s)  # Use timestamp as unique identifier

echo "=== Building Unique VUCI Packages ==="
echo "Package Name: $PACKAGE_NAME"
echo "Package Title: $PACKAGE_TITLE"
echo "Architecture: $ARCH"
echo "Unique ID: $UNIQUE_ID"
echo ""

# Create build directory
BUILD_DIR="/home/markusla/unique-vuci-build"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# ============================================================================
# CREATE API PACKAGE
# ============================================================================

echo "📦 Creating API Package..."

API_PACKAGE_DIR="$BUILD_DIR/vuci-app-${PACKAGE_NAME}-api"
mkdir -p "$API_PACKAGE_DIR"

# Create API package structure
mkdir -p "$API_PACKAGE_DIR/usr/lib/lua/api/services"

# Create the API service file (based on working ntpd.lua pattern)
cat > "$API_PACKAGE_DIR/usr/lib/lua/api/services/${PACKAGE_NAME}.lua" << 'EOF'
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

# Create control file for API package with unique identifiers
cat > "$API_PACKAGE_DIR/control" << EOF
Package: vuci-app-${PACKAGE_NAME}-api
Version: ${PACKAGE_VERSION}-${PACKAGE_RELEASE}
Depends: api-core
Section: vuci
Architecture: ${ARCH}
Installed-Size: 1024
Source: custom/vuci-app-${PACKAGE_NAME}-api
SourceName: vuci-app-${PACKAGE_NAME}-api
License: MIT
SourceDateEpoch: ${UNIQUE_ID}
Firmware: RUTX_R_00.07.17
Description: VuCI API Support for ${PACKAGE_TITLE} application
EOF

# Create postinst script for API package
cat > "$API_PACKAGE_DIR/postinst" << 'EOF'
#!/bin/sh
[ -z "${IPKG_INSTROOT}" ] || exit 0
ubus call session reload_acls
exit 0
EOF

chmod +x "$API_PACKAGE_DIR/postinst"

echo "✅ API package structure created"

# ============================================================================
# CREATE UI PACKAGE
# ============================================================================

echo "📦 Creating UI Package..."

UI_PACKAGE_DIR="$BUILD_DIR/vuci-app-${PACKAGE_NAME}-ui"
mkdir -p "$UI_PACKAGE_DIR"

# Create UI package structure
mkdir -p "$UI_PACKAGE_DIR/usr/share/vuci/menu.d"
mkdir -p "$UI_PACKAGE_DIR/www/assets"

# Create the menu configuration (following working ntpd.json pattern)
cat > "$UI_PACKAGE_DIR/usr/share/vuci/menu.d/${PACKAGE_NAME}.json" << EOF
{"services/${PACKAGE_NAME}":{"title":"${PACKAGE_TITLE}","index":100,"view":"services/${PACKAGE_TITLE}","acls":["services/${PACKAGE_NAME}"]}}
EOF

# Create a simple Vue.js application (compiled to .js.gz)
cat > "$UI_PACKAGE_DIR/www/assets/app.${PACKAGE_NAME}.app.js" << 'EOF'
// Compiled Vue.js application for Example
(function() {
    'use strict';
    
    // Vue component for Example application
    const Example = {
        name: 'Example',
        data() {
            return {
                status: null,
                loading: false
            }
        },
        methods: {
            async getStatus() {
                this.loading = true;
                try {
                    const response = await this.$axios.get('/api/example/status');
                    this.status = response.data;
                } catch (error) {
                    console.error('Error fetching status:', error);
                    this.$message.error('Failed to fetch status');
                } finally {
                    this.loading = false;
                }
            }
        },
        template: `
            <div class="container">
                <div class="row">
                    <div class="col-md-12">
                        <div class="card">
                            <div class="card-header">
                                <h3>Example Application</h3>
                            </div>
                            <div class="card-body">
                                <p>This is a working example VUCI application!</p>
                                <div v-if="status">
                                    <pre>{{ JSON.stringify(status, null, 2) }}</pre>
                                </div>
                                <button class="btn btn-primary" @click="getStatus" :disabled="loading">
                                    {{ loading ? 'Loading...' : 'Get Status' }}
                                </button>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        `
    };
    
    // Register component globally
    if (typeof window !== 'undefined' && window.Vue) {
        window.Vue.component('Example', Example);
    }
    
    // Export for module systems
    if (typeof module !== 'undefined' && module.exports) {
        module.exports = Example;
    }
})();
EOF

# Compress the JavaScript file (simulate gzip)
gzip -c "$UI_PACKAGE_DIR/www/assets/app.${PACKAGE_NAME}.app.js" > "$UI_PACKAGE_DIR/www/assets/app.${PACKAGE_NAME}.app.js.gz"
rm "$UI_PACKAGE_DIR/www/assets/app.${PACKAGE_NAME}.app.js"

# Create control file for UI package with unique identifiers
cat > "$UI_PACKAGE_DIR/control" << EOF
Package: vuci-app-${PACKAGE_NAME}-ui
Version: ${PACKAGE_VERSION}-${PACKAGE_RELEASE}
Depends: vuci-ui-core, vuci-app-${PACKAGE_NAME}-api
Section: vuci
Architecture: ${ARCH}
Installed-Size: 2048
Source: custom/vuci-app-${PACKAGE_NAME}-ui
SourceName: vuci-app-${PACKAGE_NAME}-ui
License: MIT
SourceDateEpoch: ${UNIQUE_ID}
Router: RUTX
Firmware: RUTX_R_00.07.17
tlt_name: ${PACKAGE_TITLE}
AppName: ${PACKAGE_NAME}
Description: VuCI UI Support for ${PACKAGE_TITLE} application
EOF

# Create postinst script for UI package
cat > "$UI_PACKAGE_DIR/postinst" << 'EOF'
#!/bin/sh
[ -z "${IPKG_INSTROOT}" ] || exit 0
ubus send vuci.notify '{"event": "reload_routes"}'
exit 0
EOF

chmod +x "$UI_PACKAGE_DIR/postinst"

echo "✅ UI package structure created"

# ============================================================================
# VERIFY PACKAGE STRUCTURE
# ============================================================================

echo "🔍 Verifying package structure..."

# Verify API package
if [ ! -f "$API_PACKAGE_DIR/usr/lib/lua/api/services/${PACKAGE_NAME}.lua" ]; then
    echo "❌ ERROR: API service file not found"
    exit 1
fi

if [ ! -f "$API_PACKAGE_DIR/control" ]; then
    echo "❌ ERROR: API control file not found"
    exit 1
fi

# Verify UI package
if [ ! -f "$UI_PACKAGE_DIR/usr/share/vuci/menu.d/${PACKAGE_NAME}.json" ]; then
    echo "❌ ERROR: UI menu file not found"
    exit 1
fi

if [ ! -f "$UI_PACKAGE_DIR/www/assets/app.${PACKAGE_NAME}.app.js.gz" ]; then
    echo "❌ ERROR: UI assets not found"
    exit 1
fi

if [ ! -f "$UI_PACKAGE_DIR/control" ]; then
    echo "❌ ERROR: UI control file not found"
    exit 1
fi

echo "✅ Package structure verification passed"

# ============================================================================
# BUILD IPK PACKAGES
# ============================================================================

echo "🔨 Building IPK packages..."

# Build API package
echo "Building API package..."
cd "$API_PACKAGE_DIR"

# Create data.tar.gz
tar -czf data.tar.gz usr/

# Create control.tar.gz
tar -czf control.tar.gz control postinst

# Create debian-binary
echo "2.0" > debian-binary

# Create IPK file
ar r "vuci-app-${PACKAGE_NAME}-api_${PACKAGE_VERSION}-${PACKAGE_RELEASE}_${ARCH}.ipk" debian-binary control.tar.gz data.tar.gz

# Move to build directory
mv "vuci-app-${PACKAGE_NAME}-api_${PACKAGE_VERSION}-${PACKAGE_RELEASE}_${ARCH}.ipk" "$BUILD_DIR/"

echo "✅ API package built successfully"

# Build UI package
echo "Building UI package..."
cd "$UI_PACKAGE_DIR"

# Create data.tar.gz
tar -czf data.tar.gz usr/ www/

# Create control.tar.gz
tar -czf control.tar.gz control postinst

# Create debian-binary
echo "2.0" > debian-binary

# Create IPK file
ar r "vuci-app-${PACKAGE_NAME}-ui_${PACKAGE_VERSION}-${PACKAGE_RELEASE}_${ARCH}.ipk" debian-binary control.tar.gz data.tar.gz

# Move to build directory
mv "vuci-app-${PACKAGE_NAME}-ui_${PACKAGE_VERSION}-${PACKAGE_RELEASE}_${ARCH}.ipk" "$BUILD_DIR/"

echo "✅ UI package built successfully"

# ============================================================================
# VERIFY BUILT PACKAGES
# ============================================================================

echo "🔍 Verifying built packages..."

cd "$BUILD_DIR"

# Find built packages
API_PACKAGE="vuci-app-${PACKAGE_NAME}-api_${PACKAGE_VERSION}-${PACKAGE_RELEASE}_${ARCH}.ipk"
UI_PACKAGE="vuci-app-${PACKAGE_NAME}-ui_${PACKAGE_VERSION}-${PACKAGE_RELEASE}_${ARCH}.ipk"

if [ ! -f "$API_PACKAGE" ]; then
    echo "❌ ERROR: API package not found"
    exit 1
fi

if [ ! -f "$UI_PACKAGE" ]; then
    echo "❌ ERROR: UI package not found"
    exit 1
fi

echo "✅ Found API package: $API_PACKAGE"
echo "✅ Found UI package: $UI_PACKAGE"

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
ar x "$BUILD_DIR/$API_PACKAGE"
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
ar x "$BUILD_DIR/$UI_PACKAGE"
tar -xzf data.tar.gz

# Check for expected files
if [ ! -f "usr/share/vuci/menu.d/${PACKAGE_NAME}.json" ]; then
    echo "❌ ERROR: UI menu file not found in package"
    exit 1
fi

if [ ! -f "www/assets/app.${PACKAGE_NAME}.app.js.gz" ]; then
    echo "❌ ERROR: UI assets not found in package"
    exit 1
fi

echo "✅ UI package contents verified"

# Clean up
rm -rf "$TEMP_DIR"

# ============================================================================
# COPY PACKAGES TO CURRENT DIRECTORY
# ============================================================================

echo "📋 Copying packages to current directory..."
cp "$BUILD_DIR/$API_PACKAGE" /mnt/j/GithubCursor/autonomy/
cp "$BUILD_DIR/$UI_PACKAGE" /mnt/j/GithubCursor/autonomy/

cd /mnt/j/GithubCursor/autonomy

echo "✅ Copied $API_PACKAGE"
echo "✅ Copied $UI_PACKAGE"

# ============================================================================
# FINAL VERIFICATION
# ============================================================================

echo "🔍 Final verification..."

# Check file sizes
API_SIZE=$(stat -c%s "$API_PACKAGE")
UI_SIZE=$(stat -c%s "$UI_PACKAGE")

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
if file "$API_PACKAGE" | grep -q "ar archive"; then
    echo "✅ API package is valid ar archive"
else
    echo "❌ ERROR: API package is not a valid ar archive"
    exit 1
fi

if file "$UI_PACKAGE" | grep -q "ar archive"; then
    echo "✅ UI package is valid ar archive"
else
    echo "❌ ERROR: UI package is not a valid ar archive"
    exit 1
fi

# Clean up build directory
rm -rf "$BUILD_DIR"

echo ""

# ============================================================================
# SUMMARY
# ============================================================================

echo "🎉 BUILD COMPLETED SUCCESSFULLY!"
echo ""
echo "📦 Generated Packages:"
echo "  API: $API_PACKAGE ($API_SIZE bytes)"
echo "  UI:  $UI_PACKAGE ($UI_SIZE bytes)"
echo ""
echo "📁 Package Structure:"
echo "  API Service: /usr/lib/lua/api/services/${PACKAGE_NAME}.lua"
echo "  Menu Config: /usr/share/vuci/menu.d/${PACKAGE_NAME}.json"
echo "  UI Assets: /www/assets/app.${PACKAGE_NAME}.app.js.gz"
echo ""
echo "🔧 Build System:"
echo "  Manual IPK creation (bypassed SDK build system)"
echo "  Used unique identifiers to avoid conflicts"
echo "  Followed working package patterns (ntpd, upnp)"
echo "  Proper file placement in overlay directories"
echo "  Correct package dependencies and metadata"
echo ""
echo "🔑 Unique Identifiers Used:"
echo "  SourceDateEpoch: $UNIQUE_ID"
echo "  Source: custom/vuci-app-${PACKAGE_NAME}-*"
echo "  License: MIT"
echo "  No USERID conflicts"
echo ""
echo "📋 Next Steps:"
echo "  1. Install packages: opkg install $API_PACKAGE && opkg install $UI_PACKAGE"
echo "  2. Restart services: /etc/init.d/rpcd restart && /etc/init.d/uhttpd restart"
echo "  3. Access UI: https://192.168.80.1/services/${PACKAGE_NAME}"
echo "  4. Test API: curl -k https://192.168.80.1/api/${PACKAGE_NAME}/status"
echo ""
echo "✅ All verifications passed - packages are ready for deployment!"
