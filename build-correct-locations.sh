#!/bin/bash

# Correct Locations IPK Package Builder
# Places files in proper locations that VUCI expects

set -e

# Configuration
PACKAGE_NAME="autonomy"
PACKAGE_VERSION="1.0-1"
ARCH="arm_cortex-a7_neon-vfpv4"
BUILD_DIR="/home/markusla/correct-locations-build"
TIMESTAMP=$(date +%s)

echo "Building Correct Locations IPK Packages"
echo "Package: $PACKAGE_NAME"
echo "Version: $PACKAGE_VERSION"
echo "Architecture: $ARCH"
echo "Build Directory: $BUILD_DIR"

# Clean and create build directory
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "Creating package structure..."

# Create API package structure - CORRECTED PATHS
mkdir -p "api-package/data/usr/local/usr/lib/lua/api/services"
mkdir -p "api-package/control"

# Create UI package structure - CORRECTED PATHS  
mkdir -p "ui-package/data/usr/local/usr/share/vuci/menu.d"
mkdir -p "ui-package/data/usr/local/www/assets"
mkdir -p "ui-package/control"

echo "Creating API package files..."

# API Package - Control file
cat > "api-package/control/control" << EOF
Package: vuci-app-${PACKAGE_NAME}-api
Version: ${PACKAGE_VERSION}
Depends: rpcd, rpcd-mod-file, rpcd-mod-iwinfo, rpcd-mod-rpcsys
Section: vuci
Architecture: ${ARCH}
Installed-Size: 2048
Source: vuci-app-${PACKAGE_NAME}-api
SourceName: vuci-app-${PACKAGE_NAME}-api
SourceDateEpoch: ${TIMESTAMP}
Description: Autonomy API service for RUTOS
 This package provides the API backend for the Autonomy VUCI application.
EOF

# API Package - Postinst script
cat > "api-package/control/postinst" << 'EOF'
#!/bin/sh
# Post-installation script for vuci-app-autonomy-api
echo "Installing Autonomy API service..."
exit 0
EOF
chmod +x "api-package/control/postinst"

# API Package - Lua service file
cat > "api-package/data/usr/local/usr/lib/lua/api/services/${PACKAGE_NAME}.lua" << 'EOF'
-- Autonomy API Service
-- Based on working ntpd.lua pattern

local function autonomy_status()
    local status = {
        timestamp = os.time(),
        version = "1.0.0",
        status = "running",
        uptime = 3600,
        connections = 5
    }
    return status
end

local function autonomy_config()
    local config = {
        enabled = true,
        interval = 30,
        log_level = "info"
    }
    return config
end

-- Register functions
local function register()
    local rpc = require "luci.http"
    local json = require "luci.jsonc"
    
    -- Status endpoint
    rpc.register("autonomy_status", function()
        return json.stringify(autonomy_status())
    end)
    
    -- Config endpoint  
    rpc.register("autonomy_config", function()
        return json.stringify(autonomy_config())
    end)
end

-- Initialize
register()
EOF

echo "Creating UI package files..."

# UI Package - Control file
cat > "ui-package/control/control" << EOF
Package: vuci-app-${PACKAGE_NAME}-ui
Version: ${PACKAGE_VERSION}
Depends: vuci-app-${PACKAGE_NAME}-api
Section: vuci
Architecture: ${ARCH}
Installed-Size: 4096
Source: vuci-app-${PACKAGE_NAME}-ui
SourceName: vuci-app-${PACKAGE_NAME}-ui
SourceDateEpoch: ${TIMESTAMP}
Description: Autonomy UI for RUTOS
 This package provides the web interface for the Autonomy VUCI application.
EOF

# UI Package - Postinst script
cat > "ui-package/control/postinst" << 'EOF'
#!/bin/sh
# Post-installation script for vuci-app-autonomy-ui
echo "Installing Autonomy UI..."
exit 0
EOF
chmod +x "ui-package/control/postinst"

# UI Package - Menu configuration - CORRECTED PATH
cat > "ui-package/data/usr/local/usr/share/vuci/menu.d/${PACKAGE_NAME}.json" << EOF
{"services/${PACKAGE_NAME}":{"title":"Autonomy","index":50,"view":"services/Autonomy","acls":["services/${PACKAGE_NAME}"]}}
EOF

# UI Package - Vue.js application - CORRECTED PATH
cat > "ui-package/data/usr/local/www/assets/app.${PACKAGE_NAME}.app-${TIMESTAMP}.js" << 'EOF'
// Autonomy Vue.js Application
// Compiled Vue 3 application for VUCI

(function() {
    'use strict';
    
    // Vue 3 component for Autonomy
    const AutonomyApp = {
        name: 'AutonomyApp',
        data() {
            return {
                status: {},
                config: {},
                loading: false
            }
        },
        methods: {
            async loadStatus() {
                this.loading = true;
                try {
                    const response = await this.$axios.get('/api/autonomy_status');
                    this.status = response.data;
                } catch (error) {
                    console.error('Failed to load status:', error);
                } finally {
                    this.loading = false;
                }
            },
            async loadConfig() {
                try {
                    const response = await this.$axios.get('/api/autonomy_config');
                    this.config = response.data;
                } catch (error) {
                    console.error('Failed to load config:', error);
                }
            }
        },
        mounted() {
            this.loadStatus();
            this.loadConfig();
        },
        template: `
            <div class="autonomy-app">
                <h2>Autonomy Status</h2>
                <div v-if="loading" class="loading">Loading...</div>
                <div v-else>
                    <p><strong>Version:</strong> {{ status.version }}</p>
                    <p><strong>Status:</strong> {{ status.status }}</p>
                    <p><strong>Uptime:</strong> {{ status.uptime }} seconds</p>
                    <p><strong>Connections:</strong> {{ status.connections }}</p>
                </div>
            </div>
        `
    };
    
    // Register component
    if (typeof window !== 'undefined' && window.Vue) {
        window.Vue.component('autonomy-app', AutonomyApp);
    }
})();
EOF

# Compress the Vue.js file to .js.gz
gzip -c "ui-package/data/usr/local/www/assets/app.${PACKAGE_NAME}.app-${TIMESTAMP}.js" > "ui-package/data/usr/local/www/assets/app.${PACKAGE_NAME}.app-${TIMESTAMP}.js.gz"
rm "ui-package/data/usr/local/www/assets/app.${PACKAGE_NAME}.app-${TIMESTAMP}.js"

echo "Creating IPK packages (Tar Format)..."

# Function to create IPK package in tar format
create_ipk_tar() {
    local package_dir="$1"
    local package_name="$2"
    
    echo "Creating $package_name..."
    
    # Create debian-binary
    echo "2.0" > "$BUILD_DIR/$package_dir/debian-binary"
    
    # Create tar archives
    cd "$BUILD_DIR/$package_dir/control"
    tar -czf "../control.tar.gz" --owner=root --group=root control postinst
    cd "$BUILD_DIR/$package_dir"
    
    cd "$BUILD_DIR/$package_dir/data"
    tar -czf "../data.tar.gz" --owner=root --group=root .
    cd "$BUILD_DIR/$package_dir"
    
    # Create tar archive containing IPK components (like working ntpd package)
    tar -cf "${package_name}_${PACKAGE_VERSION}_${ARCH}.ipk" debian-binary control.tar.gz data.tar.gz
    
    # Compress the tar archive with gzip
    gzip -c "${package_name}_${PACKAGE_VERSION}_${ARCH}.ipk" > "${package_name}_${PACKAGE_VERSION}_${ARCH}.ipk.gz"
    
    # Rename to .ipk extension for opkg
    mv "${package_name}_${PACKAGE_VERSION}_${ARCH}.ipk.gz" "${package_name}_${PACKAGE_VERSION}_${ARCH}.ipk"
    
    # Verify package
    echo "Verifying $package_name..."
    file "${package_name}_${PACKAGE_VERSION}_${ARCH}.ipk"
    
    # Move to build directory
    mv "${package_name}_${PACKAGE_VERSION}_${ARCH}.ipk" "$BUILD_DIR/"
    
    cd "$BUILD_DIR"
}

# Create API package
create_ipk_tar "api-package" "vuci-app-${PACKAGE_NAME}-api"

# Create UI package  
create_ipk_tar "ui-package" "vuci-app-${PACKAGE_NAME}-ui"

echo "Package creation complete!"
echo ""
echo "Created packages:"
ls -la *.ipk
echo ""
echo "Package verification:"
for pkg in *.ipk; do
    echo "$pkg: $(file "$pkg")"
done

echo ""
echo "Package structure verification:"
for pkg in *.ipk; do
    echo "--- $pkg ---"
    gunzip -c "$pkg" > "temp_${pkg}.tar"
    tar -tf "temp_${pkg}.tar"
    rm "temp_${pkg}.tar"
    echo ""
done

echo ""
echo "Ready for deployment!"


