#!/bin/bash

# TELTONIKA-SPECIFIC VUCI Package Build Script
# Uses the correct paths for RUTOS installation

set -e

echo "========================================="
echo "TELTONIKA VUCI PACKAGE BUILD SCRIPT"
echo "========================================="

# Configuration
PACKAGE_NAME="example"
PACKAGE_NAME_CAPITALIZED="Example"
VERSION="1.0"
RELEASE="2"
ARCH="arm_cortex-a7_neon-vfpv4"
BUILD_DIR="/tmp/vuci-teltonika-$$"

# Clean build directory
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

echo "Build directory: $BUILD_DIR"

# ============================================
# STEP 1: Create Simple Working JavaScript
# ============================================
echo ""
echo "Creating simplified JavaScript component..."

# Create a simple, working JavaScript file that VUCI can load
# This matches the pattern of working apps like NTPD and UPNP
mkdir -p "$BUILD_DIR/js"

cat > "$BUILD_DIR/js/app.example.app-2025-08-08-3a282822359.js" << 'EOF'
// Example VUCI Application - Simplified for compatibility
(function() {
    'use strict';
    
    // Define the Vue component
    const ExampleComponent = {
        name: 'Example',
        template: `
            <div class="example-app" style="padding: 20px;">
                <h2>{{ title }}</h2>
                
                <div v-if="loading" style="text-align: center; padding: 40px;">
                    Loading...
                </div>
                
                <div v-else>
                    <div style="margin-bottom: 30px; padding: 20px; border: 1px solid #ddd; border-radius: 4px;">
                        <h3>Status</h3>
                        <p>Application is working!</p>
                        <p>Version: 1.0</p>
                        <p>Time: {{ currentTime }}</p>
                    </div>
                    
                    <div style="margin-bottom: 30px; padding: 20px; border: 1px solid #ddd; border-radius: 4px;">
                        <h3>API Test</h3>
                        <button @click="testApi" :disabled="testing" style="padding: 8px 16px; background: #007bff; color: white; border: none; border-radius: 4px; cursor: pointer;">
                            {{ testing ? 'Testing...' : 'Test API' }}
                        </button>
                        <div v-if="apiResponse" style="margin-top: 10px; padding: 10px; background: #f5f5f5; border-radius: 4px;">
                            <pre>{{ apiResponse }}</pre>
                        </div>
                    </div>
                </div>
            </div>
        `,
        data() {
            return {
                title: 'Example Application',
                loading: false,
                testing: false,
                apiResponse: null,
                currentTime: new Date().toLocaleString()
            }
        },
        methods: {
            async testApi() {
                this.testing = true;
                this.apiResponse = null;
                
                try {
                    // Try to call our API
                    const response = await fetch('/api/example_f/test');
                    const data = await response.json();
                    this.apiResponse = JSON.stringify(data, null, 2);
                } catch (error) {
                    this.apiResponse = 'Error: ' + error.message;
                }
                
                this.testing = false;
            }
        },
        mounted() {
            console.log('Example app mounted successfully');
            // Update time every second
            setInterval(() => {
                this.currentTime = new Date().toLocaleString();
            }, 1000);
        }
    };
    
    // Register with Vue if available
    if (typeof Vue !== 'undefined') {
        Vue.component('Example', ExampleComponent);
    }
    
    // Export for module systems
    if (typeof module !== 'undefined' && module.exports) {
        module.exports = ExampleComponent;
    }
    
    // AMD support
    if (typeof define === 'function' && define.amd) {
        define([], function() {
            return ExampleComponent;
        });
    }
    
    // Global export as fallback
    if (typeof window !== 'undefined') {
        window.ExampleComponent = ExampleComponent;
    }
    
    // VUCI-specific registration
    if (typeof window !== 'undefined' && window.__VUCI_APPS__) {
        window.__VUCI_APPS__['services/Example'] = ExampleComponent;
    }
})();

// Export default for ES6 modules
export default window.ExampleComponent || {};
EOF

# Compress the JavaScript
gzip -c "$BUILD_DIR/js/app.example.app-2025-08-08-3a282822359.js" > "$BUILD_DIR/js/app.example.app-2025-08-08-3a282822359.js.gz"

# ============================================
# STEP 2: Create API Package
# ============================================
echo ""
echo "Creating API package..."

API_DIR="$BUILD_DIR/api-package"
# CRITICAL: Use /usr/local paths as discovered from working packages
mkdir -p "$API_DIR/data/usr/local/usr/lib/lua/api/services"

# Create simplified API service
cat > "$API_DIR/data/usr/local/usr/lib/lua/api/services/example_f.lua" << 'EOF'
-- Example Function Service for VUCI
-- Simplified to work without complex dependencies

local M = {}

-- Simple test function
function M.test()
    return {
        success = true,
        message = "API is working!",
        timestamp = os.time(),
        data = {
            version = "1.0",
            status = "operational",
            uptime = io.popen("uptime"):read("*a")
        }
    }
end

-- Handle GET requests
function M.GET(path, query)
    if path == "test" then
        return M.test()
    end
    return { error = "Unknown endpoint" }
end

-- Handle POST requests  
function M.POST(path, data)
    return {
        success = true,
        message = "POST received",
        path = path,
        data = data
    }
end

return M
EOF

# Create control file for API
cat > "$API_DIR/control" << EOF
Package: vuci-app-${PACKAGE_NAME}-api
Version: ${VERSION}-${RELEASE}
Depends: libc
Section: vuci
Architecture: ${ARCH}
Installed-Size: 2048
Description: Example VUCI API service (Teltonika paths)
EOF

# Create postinst for API
cat > "$API_DIR/postinst" << 'EOF'
#!/bin/sh
[ -z "$IPKG_INSTROOT" ] && {
    /etc/init.d/uhttpd restart 2>/dev/null || true
}
exit 0
EOF
chmod +x "$API_DIR/postinst"

# Create debian-binary
echo "2.0" > "$API_DIR/debian-binary"

# Build API package
cd "$API_DIR"
tar -czf control.tar.gz --owner=root --group=root control postinst
tar -czf data.tar.gz --owner=root --group=root -C data .
tar -cf "../vuci-app-${PACKAGE_NAME}-api_${VERSION}-${RELEASE}_${ARCH}.tar" debian-binary control.tar.gz data.tar.gz
gzip -c "../vuci-app-${PACKAGE_NAME}-api_${VERSION}-${RELEASE}_${ARCH}.tar" > "../vuci-app-${PACKAGE_NAME}-api_${VERSION}-${RELEASE}_${ARCH}.ipk"
rm "../vuci-app-${PACKAGE_NAME}-api_${VERSION}-${RELEASE}_${ARCH}.tar"

echo "API package created: vuci-app-${PACKAGE_NAME}-api_${VERSION}-${RELEASE}_${ARCH}.ipk"

# ============================================
# STEP 3: Create UI Package with Teltonika Paths
# ============================================
echo ""
echo "Creating UI package with Teltonika-specific paths..."

UI_DIR="$BUILD_DIR/ui-package"

# CRITICAL: Use the paths that Teltonika's RUTOS expects
# Menu goes to /usr/local/share/vuci/menu.d/ (NOT /usr/share!)
mkdir -p "$UI_DIR/data/usr/local/share/vuci/menu.d"

# Assets go to /www/assets/ (where other VUCI apps are)
mkdir -p "$UI_DIR/data/www/assets"

# Copy compiled JavaScript to assets
cp "$BUILD_DIR/js/app.example.app-2025-08-08-3a282822359.js.gz" "$UI_DIR/data/www/assets/"

# Create menu configuration
# CRITICAL: The view must match the JavaScript component name
cat > "$UI_DIR/data/usr/local/share/vuci/menu.d/example.json" << EOF
{"services/example":{"title":"Example","index":50,"view":"services/Example","acls":["services/example"]}}
EOF

# Create ACL file in the correct location
mkdir -p "$UI_DIR/data/usr/local/share/rpcd/acl.d"
cat > "$UI_DIR/data/usr/local/share/rpcd/acl.d/example.json" << 'EOF'
{
    "services/example": {
        "description": "Example application access",
        "read": {
            "ubus": {
                "example": ["*"]
            },
            "uci": {
                "example": ["*"]
            }
        },
        "write": {
            "ubus": {
                "example": ["*"]
            },
            "uci": {
                "example": ["*"]
            }
        }
    }
}
EOF

# Create control file for UI
cat > "$UI_DIR/control" << EOF
Package: vuci-app-${PACKAGE_NAME}-ui
Version: ${VERSION}-${RELEASE}
Depends: libc, vuci-app-${PACKAGE_NAME}-api, vuci-ui-core
Section: vuci
Architecture: ${ARCH}
Installed-Size: 4096
Description: Example VUCI UI application (Teltonika paths)
EOF

# Create postinst for UI
cat > "$UI_DIR/postinst" << 'EOF'
#!/bin/sh
[ -z "$IPKG_INSTROOT" ] && {
    # Clear any cache
    rm -rf /tmp/luci-*
    
    # Create symlink if needed for menu discovery
    if [ -f /usr/local/share/vuci/menu.d/example.json ] && [ ! -f /usr/share/vuci/menu.d/example.json ]; then
        mkdir -p /usr/share/vuci/menu.d
        ln -sf /usr/local/share/vuci/menu.d/example.json /usr/share/vuci/menu.d/example.json
    fi
    
    # Restart web server
    /etc/init.d/uhttpd restart 2>/dev/null || true
}
exit 0
EOF
chmod +x "$UI_DIR/postinst"

# Create postrm to clean up
cat > "$UI_DIR/postrm" << 'EOF'
#!/bin/sh
[ -z "$IPKG_INSTROOT" ] && {
    rm -f /usr/share/vuci/menu.d/example.json
    rm -f /www/assets/app.example.*.js.gz
}
exit 0
EOF
chmod +x "$UI_DIR/postrm"

# Create debian-binary
echo "2.0" > "$UI_DIR/debian-binary"

# Build UI package
cd "$UI_DIR"
tar -czf control.tar.gz --owner=root --group=root control postinst postrm
tar -czf data.tar.gz --owner=root --group=root -C data .
tar -cf "../vuci-app-${PACKAGE_NAME}-ui_${VERSION}-${RELEASE}_${ARCH}.tar" debian-binary control.tar.gz data.tar.gz
gzip -c "../vuci-app-${PACKAGE_NAME}-ui_${VERSION}-${RELEASE}_${ARCH}.tar" > "../vuci-app-${PACKAGE_NAME}-ui_${VERSION}-${RELEASE}_${ARCH}.ipk"
rm "../vuci-app-${PACKAGE_NAME}-ui_${VERSION}-${RELEASE}_${ARCH}.tar"

echo "UI package created: vuci-app-${PACKAGE_NAME}-ui_${VERSION}-${RELEASE}_${ARCH}.ipk"

# ============================================
# STEP 4: Copy packages to current directory
# ============================================
echo ""
echo "Copying packages to current directory..."

cp "$BUILD_DIR/vuci-app-${PACKAGE_NAME}-api_${VERSION}-${RELEASE}_${ARCH}.ipk" .
cp "$BUILD_DIR/vuci-app-${PACKAGE_NAME}-ui_${VERSION}-${RELEASE}_${ARCH}.ipk" .

echo ""
echo "========================================="
echo "BUILD COMPLETE!"
echo "========================================="
echo ""
echo "Packages created:"
echo "  - vuci-app-${PACKAGE_NAME}-api_${VERSION}-${RELEASE}_${ARCH}.ipk"
echo "  - vuci-app-${PACKAGE_NAME}-ui_${VERSION}-${RELEASE}_${ARCH}.ipk"
echo ""
echo "Key improvements (Teltonika-specific):"
echo "  ✓ Menu files in /usr/local/share/vuci/menu.d/ (Teltonika path)"
echo "  ✓ JavaScript in /www/assets/ (standard VUCI location)"
echo "  ✓ API in /usr/local/usr/lib/lua/api/services/ (overlay path)"
echo "  ✓ Symlink created from /usr/share to /usr/local/share for menu"
echo "  ✓ Simplified JavaScript that should work with VUCI"
echo ""
echo "To deploy:"
echo "  1. Copy packages to router: scp *.ipk root@192.168.80.1:/tmp/"
echo "  2. Install: opkg install /tmp/vuci-app-example-*.ipk"
echo "  3. Access at: http://192.168.80.1 -> Services -> Example"
echo ""

# Clean up build directory
rm -rf "$BUILD_DIR"


