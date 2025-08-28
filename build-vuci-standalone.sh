#!/bin/bash

# Build a standalone VUCI package that should work independently
# This attempts to create a package that VUCI can dynamically load

set -e

echo "========================================="
echo "STANDALONE VUCI PACKAGE BUILD"
echo "========================================="

# Configuration
PACKAGE_NAME="example"
VERSION="1.0"
RELEASE="3"
ARCH="arm_cortex-a7_neon-vfpv4"
BUILD_DIR="/tmp/vuci-standalone-$$"

# Clean build directory
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

echo "Build directory: $BUILD_DIR"

# ============================================
# STEP 1: Create a self-registering JavaScript module
# ============================================
echo ""
echo "Creating self-registering JavaScript module..."

mkdir -p "$BUILD_DIR/js"

# Create a module that can be dynamically imported
cat > "$BUILD_DIR/js/app.example.app-2025-08-08-3a282822359.js" << 'EOF'
// VUCI Standalone Application - Example
// This module self-registers with VUCI when loaded

(function(global) {
    'use strict';
    
    console.log('[Example] Module loading...');
    
    // The Vue component
    const ExampleComponent = {
        name: 'Example',
        template: `
            <div class="example-app p-4">
                <h2>Example Application (Standalone)</h2>
                
                <div class="card mb-3">
                    <div class="card-body">
                        <h5 class="card-title">Status</h5>
                        <p>Module loaded successfully!</p>
                        <p>Time: {{ currentTime }}</p>
                        <p>Package Version: 1.0-3</p>
                    </div>
                </div>
                
                <div class="card mb-3">
                    <div class="card-body">
                        <h5 class="card-title">Test API</h5>
                        <button @click="testApi" :disabled="testing" class="btn btn-primary">
                            {{ testing ? 'Testing...' : 'Test API Endpoint' }}
                        </button>
                        <div v-if="apiResponse" class="mt-3">
                            <pre class="bg-light p-2">{{ apiResponse }}</pre>
                        </div>
                    </div>
                </div>
            </div>
        `,
        data() {
            return {
                currentTime: new Date().toLocaleString(),
                testing: false,
                apiResponse: null
            }
        },
        methods: {
            async testApi() {
                this.testing = true;
                this.apiResponse = null;
                
                try {
                    const response = await fetch('/api/example/test');
                    if (!response.ok) {
                        throw new Error(`HTTP ${response.status}`);
                    }
                    const data = await response.json();
                    this.apiResponse = JSON.stringify(data, null, 2);
                } catch (error) {
                    this.apiResponse = 'Error: ' + error.message;
                }
                
                this.testing = false;
            }
        },
        mounted() {
            console.log('[Example] Component mounted');
            this.timer = setInterval(() => {
                this.currentTime = new Date().toLocaleString();
            }, 1000);
        },
        unmounted() {
            if (this.timer) {
                clearInterval(this.timer);
            }
        }
    };
    
    // Try multiple registration methods
    
    // Method 1: Register as a Vue component
    if (typeof Vue !== 'undefined' && Vue.component) {
        console.log('[Example] Registering with Vue.component');
        Vue.component('Example', ExampleComponent);
        Vue.component('services-example', ExampleComponent);
    }
    
    // Method 2: Register with VUCI app registry
    if (typeof window !== 'undefined') {
        // Create app registry if it doesn't exist
        window.__VUCI_APPS__ = window.__VUCI_APPS__ || {};
        window.__VUCI_COMPONENTS__ = window.__VUCI_COMPONENTS__ || {};
        
        // Register under multiple possible paths
        console.log('[Example] Registering with VUCI registries');
        window.__VUCI_APPS__['services/Example'] = ExampleComponent;
        window.__VUCI_APPS__['Example'] = ExampleComponent;
        window.__VUCI_COMPONENTS__['Example'] = ExampleComponent;
        
        // Also try to register with the router if available
        if (window.$router && window.$router.addRoute) {
            console.log('[Example] Adding route dynamically');
            window.$router.addRoute({
                path: '/services/example',
                name: 'services-example',
                component: ExampleComponent
            });
        }
    }
    
    // Method 3: Export for ES6 modules
    if (typeof module !== 'undefined' && module.exports) {
        module.exports = ExampleComponent;
    }
    
    // Method 4: AMD/RequireJS
    if (typeof define === 'function' && define.amd) {
        define('services/Example', [], function() {
            return ExampleComponent;
        });
    }
    
    // Method 5: Store in a global location VUCI might check
    if (typeof window !== 'undefined') {
        window.ExampleComponent = ExampleComponent;
        
        // Try to find and patch the VUCI loader
        if (window.__vite__) {
            console.log('[Example] Detected Vite environment');
            // Register with Vite's module system
            window.__vite__.modules = window.__vite__.modules || {};
            window.__vite__.modules['./app.example.app-2025-08-08-3a282822359.js'] = {
                default: ExampleComponent
            };
        }
    }
    
    console.log('[Example] Module registration complete');
    
    // Return the component for dynamic imports
    return ExampleComponent;
    
})(typeof window !== 'undefined' ? window : global);

// ES6 export
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
mkdir -p "$API_DIR/data/usr/lib/lua/api/services"

# Create a simple API service that should work
cat > "$API_DIR/data/usr/lib/lua/api/services/example.lua" << 'EOF'
-- Example API Service
local M = {}

function M.test()
    return {
        success = true,
        message = "API endpoint working",
        timestamp = os.time(),
        version = "1.0-3"
    }
end

-- Handle HTTP requests
function M.handle(method, path, query, data)
    if path == "/api/example/test" then
        return M.test()
    end
    return { error = "Not found" }
end

return M
EOF

# Create control file
cat > "$API_DIR/control" << EOF
Package: vuci-app-${PACKAGE_NAME}-api
Version: ${VERSION}-${RELEASE}
Depends: libc
Section: vuci
Architecture: ${ARCH}
Installed-Size: 1024
Description: Example VUCI API service (standalone)
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
tar -cf "../vuci-app-${PACKAGE_NAME}-api_${VERSION}-${RELEASE}_${ARCH}.tar" debian-binary control.tar.gz data.tar.gz
gzip -c "../vuci-app-${PACKAGE_NAME}-api_${VERSION}-${RELEASE}_${ARCH}.tar" > "../vuci-app-${PACKAGE_NAME}-api_${VERSION}-${RELEASE}_${ARCH}.ipk"
rm "../vuci-app-${PACKAGE_NAME}-api_${VERSION}-${RELEASE}_${ARCH}.tar"

# ============================================
# STEP 3: Create UI Package
# ============================================
echo ""
echo "Creating UI package..."

UI_DIR="$BUILD_DIR/ui-package"

# Try multiple possible locations for maximum compatibility
mkdir -p "$UI_DIR/data/www/assets"
mkdir -p "$UI_DIR/data/usr/share/vuci/menu.d"
mkdir -p "$UI_DIR/data/usr/local/share/vuci/menu.d"

# Copy JavaScript to assets
cp "$BUILD_DIR/js/app.example.app-2025-08-08-3a282822359.js.gz" "$UI_DIR/data/www/assets/"

# Create menu in both locations
cat > "$UI_DIR/data/usr/share/vuci/menu.d/example.json" << 'EOF'
{"services/example":{"title":"Example","index":300,"view":"services/Example","acls":["services/example"]}}
EOF

cat > "$UI_DIR/data/usr/local/share/vuci/menu.d/example.json" << 'EOF'
{"services/example":{"title":"Example","index":300,"view":"services/Example","acls":["services/example"]}}
EOF

# Create a loader script that tries to inject our module
mkdir -p "$UI_DIR/data/www/assets/loaders"
cat > "$UI_DIR/data/www/assets/loaders/example-loader.js" << 'EOF'
// Loader to ensure our module is available
(function() {
    console.log('[Example Loader] Attempting to load example module...');
    
    // Try to dynamically import our module
    if (typeof import === 'function') {
        import('../app.example.app-2025-08-08-3a282822359.js')
            .then(function(module) {
                console.log('[Example Loader] Module loaded successfully');
            })
            .catch(function(error) {
                console.error('[Example Loader] Failed to load module:', error);
            });
    }
})();
EOF

# Create control file
cat > "$UI_DIR/control" << EOF
Package: vuci-app-${PACKAGE_NAME}-ui
Version: ${VERSION}-${RELEASE}
Depends: libc, vuci-ui-core
Section: vuci
Architecture: ${ARCH}
Installed-Size: 8192
Description: Example VUCI UI (standalone)
EOF

# Create postinst that tries to register our module
cat > "$UI_DIR/postinst" << 'EOF'
#!/bin/sh
[ -z "$IPKG_INSTROOT" ] && {
    # Clear cache
    rm -rf /tmp/luci-* /tmp/vuci-*
    
    # Try to inject our module into the index if possible
    if [ -f /www/assets/index-*.js.gz ]; then
        echo "Checking VUCI index for dynamic import support..."
        # This is a placeholder - in reality we'd need to patch the index
    fi
    
    # Restart web server
    /etc/init.d/uhttpd restart 2>/dev/null || true
}
exit 0
EOF
chmod +x "$UI_DIR/postinst"

echo "2.0" > "$UI_DIR/debian-binary"

# Build UI package
cd "$UI_DIR"
tar -czf control.tar.gz --owner=root --group=root control postinst
tar -czf data.tar.gz --owner=root --group=root -C data .
tar -cf "../vuci-app-${PACKAGE_NAME}-ui_${VERSION}-${RELEASE}_${ARCH}.tar" debian-binary control.tar.gz data.tar.gz
gzip -c "../vuci-app-${PACKAGE_NAME}-ui_${VERSION}-${RELEASE}_${ARCH}.tar" > "../vuci-app-${PACKAGE_NAME}-ui_${VERSION}-${RELEASE}_${ARCH}.ipk"
rm "../vuci-app-${PACKAGE_NAME}-ui_${VERSION}-${RELEASE}_${ARCH}.tar"

# ============================================
# Copy packages
# ============================================
echo ""
echo "Copying packages..."
cp "$BUILD_DIR/vuci-app-${PACKAGE_NAME}-api_${VERSION}-${RELEASE}_${ARCH}.ipk" .
cp "$BUILD_DIR/vuci-app-${PACKAGE_NAME}-ui_${VERSION}-${RELEASE}_${ARCH}.ipk" .

echo ""
echo "========================================="
echo "BUILD COMPLETE!"
echo "========================================="
echo ""
echo "This build attempts to create a standalone package that:"
echo "  ✓ Self-registers with VUCI when loaded"
echo "  ✓ Uses multiple registration methods"
echo "  ✓ Installs to multiple possible locations"
echo "  ✓ Includes debugging output"
echo ""
echo "The real issue is that VUCI's index.js needs to know about our module."
echo "Without modifying the SDK build process, this is the best we can do."
echo ""
echo "To test:"
echo "  1. Install packages on router"
echo "  2. Check browser console for [Example] messages"
echo "  3. Look for registration attempts"
echo ""

rm -rf "$BUILD_DIR"


