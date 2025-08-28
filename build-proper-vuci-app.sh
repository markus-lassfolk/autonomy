#!/bin/bash

# Proper VUCI App Builder
# Creates a VUCI-compatible Vue.js application following the exact pattern of working apps

set -e

# Configuration
PACKAGE_NAME="autonomy"
PACKAGE_VERSION="1.0-1"
ARCH="arm_cortex-a7_neon-vfpv4"
BUILD_DIR="/home/markusla/proper-vuci-build"
TIMESTAMP=$(date +%s)

echo "Building Proper VUCI Application"
echo "Package: $PACKAGE_NAME"
echo "Version: $PACKAGE_VERSION"
echo "Architecture: $ARCH"
echo "Build Directory: $BUILD_DIR"

# Clean and create build directory
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "Creating package structure..."

# Create API package structure
mkdir -p "api-package/data/usr/local/usr/lib/lua/api/services"
mkdir -p "api-package/control"

# Create UI package structure  
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

# API Package - Lua service file (proper VUCI API pattern)
cat > "api-package/data/usr/local/usr/lib/lua/api/services/${PACKAGE_NAME}.lua" << 'EOF'
-- Autonomy API Service
-- Following VUCI API pattern

local function autonomy_status()
    local status = {
        timestamp = os.time(),
        version = "1.0.0",
        status = "running",
        uptime = 3600,
        connections = 5,
        enabled = "1"
    }
    return status
end

local function autonomy_config()
    local config = {
        enabled = "1",
        interval = "30",
        log_level = "info",
        auto_failover = "1"
    }
    return config
end

-- VUCI API endpoints
local function get_status()
    return { success = true, data = autonomy_status() }
end

local function get_config()
    return { success = true, data = autonomy_config() }
end

-- Register with rpcd
local rpcd = require "rpcd"
local json = require "luci.jsonc"

rpcd.register("autonomy_status", function()
    return json.stringify(get_status())
end)

rpcd.register("autonomy_config", function()
    return json.stringify(get_config())
end)

-- Initialize
print("Autonomy API service loaded")
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

# UI Package - Menu configuration
cat > "ui-package/data/usr/local/usr/share/vuci/menu.d/${PACKAGE_NAME}.json" << EOF
{"services/${PACKAGE_NAME}":{"title":"Autonomy","index":50,"view":"services/Autonomy","acls":["services/${PACKAGE_NAME}"]}}
EOF

# UI Package - Proper VUCI Vue.js application (following NTPD/UPNP pattern)
cat > "ui-package/data/usr/local/www/assets/app.${PACKAGE_NAME}.app-${TIMESTAMP}.js" << 'EOF'
import{_ as v}from"./index-2025-08-08-3a282822359.js";import{s as a,v as y,x as _,y as o,z as i}from"./vendor-2025-08-08-3a282822359.js";const w={data(){return{status:{},config:{},loading:!1}},methods:{loadStatus(){return this.$axios.get("/api/autonomy_status").then(({data:e})=>{this.status=e.data}).catch(()=>{this.$message.error("Failed to load autonomy status")})},loadConfig(){return this.$axios.get("/api/autonomy_config").then(({data:e})=>{this.config=e.data}).catch(()=>{this.$message.error("Failed to load autonomy config")})},afterLoad(){this.loadStatus();this.loadConfig()}}};function I(e,s,c,d,r,u){const l=a("vuci-form-item-switch"),h=a("vuci-form-item-input"),p=a("vuci-named-section"),n=a("vuci-form");return _(),y(n,{config:"autonomy","after-load":u.afterLoad},{default:o(({uciData:m})=>[i(p,{title:e.$t("Autonomy Status"),help:e.$t("Autonomy system status and configuration."),endpoints:[{endpoint:"autonomy/status"},{endpoint:"autonomy/config"}],"uci-data":m,"data-key":"autonomy"},{default:o(({s:t})=>[i(l,{"uci-section":t,name:"enabled",label:e.$t("Enable"),help:e.$t("Enable Autonomy system."),initial:"1",rmempty:!1},null,8,["uci-section","label","help"]),i(h,{"uci-section":t,name:"interval",label:e.$t("Check Interval"),help:e.$t("Interval in seconds between health checks."),placeholder:"30",rules:"uinteger",initial:"30"},null,8,["uci-section","label","help","placeholder"]),i(h,{"uci-section":t,name:"log_level",label:e.$t("Log Level"),help:e.$t("Logging level for autonomy system."),placeholder:"info",initial:"info"},null,8,["uci-section","label","help","placeholder"])]),_:2},1032,["title","help","uci-data"])]),_:1},8,["after-load"])}const k=v(w,[["render",I]]);export{k as default};
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
    
    # Create tar archive containing IPK components
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
echo "Ready for deployment!"


