#!/bin/bash
# Build simple VUCI packages using the existing SDK
# This avoids toolchain issues by building minimal packages

set -e

echo "========================================="
echo "BUILDING SIMPLE VUCI PACKAGES (OPTIMIZED)"
echo "========================================="
echo ""
echo "Build optimizations enabled:"
echo "  • Parallel compilation with $(nproc) cores"
echo "  • Increased make job threads"
echo "  • Minimal package dependencies"
echo ""

# Use existing SDK
SDK_DIR="$HOME/rutos-sdk"
if [ ! -d "$SDK_DIR" ]; then
    echo "Error: SDK not found at $SDK_DIR"
    exit 1
fi

cd "$SDK_DIR"
echo "Working in: $(pwd)"

# Create ultra-simple packages that don't need toolchain
echo ""
echo "Creating ultra-simple VUCI packages..."

# Remove old simple packages
rm -rf package/vuci-app-simple-api package/vuci-app-simple-ui

# Create simple API package
mkdir -p package/vuci-app-simple-api/files/usr/lib/lua/api/services
cat > package/vuci-app-simple-api/files/usr/lib/lua/api/services/simple.lua << 'EOF'
-- Simple API Service
local M = {}

function M.test()
    return {
        success = true,
        message = "Simple API working",
        timestamp = os.time()
    }
end

return M
EOF

cat > package/vuci-app-simple-api/Makefile << 'EOF'
include $(TOPDIR)/rules.mk

PKG_NAME:=vuci-app-simple-api
PKG_VERSION:=1.0
PKG_RELEASE:=7
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/vuci-app-simple-api
  SECTION:=vuci
  CATEGORY:=VUCI
  TITLE:=Simple VUCI API
  DEPENDS:=+lua
endef

define Package/vuci-app-simple-api/description
  Simple VUCI API service
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
EOF

# Create simple UI package
mkdir -p package/vuci-app-simple-ui/files/usr/share/vuci/menu.d
mkdir -p package/vuci-app-simple-ui/files/www/simple

cat > package/vuci-app-simple-ui/files/usr/share/vuci/menu.d/simple.json << 'EOF'
{"services/simple":{"title":"Simple","index":400,"view":"services/Simple"}}
EOF

cat > package/vuci-app-simple-ui/files/www/simple/index.html << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <title>Simple VUCI App</title>
    <style>
        body { font-family: Arial, sans-serif; padding: 20px; }
        .container { max-width: 800px; margin: 0 auto; }
        button { background: #007bff; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; }
        button:hover { background: #0056b3; }
        pre { background: #f5f5f5; padding: 10px; border-radius: 5px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Simple VUCI Application</h1>
        <p>Version: 1.0-7</p>
        <p>This is a minimal VUCI application built with the SDK.</p>
        
        <button onclick="testAPI()">Test API</button>
        <div id="result"></div>
    </div>
    
    <script>
        function testAPI() {
            fetch('/api/simple/test')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('result').innerHTML = '<pre>' + JSON.stringify(data, null, 2) + '</pre>';
                })
                .catch(error => {
                    document.getElementById('result').innerHTML = '<p style="color: red;">Error: ' + error + '</p>';
                });
        }
    </script>
</body>
</html>
EOF

cat > package/vuci-app-simple-ui/Makefile << 'EOF'
include $(TOPDIR)/rules.mk

PKG_NAME:=vuci-app-simple-ui
PKG_VERSION:=1.0
PKG_RELEASE:=7
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/vuci-app-simple-ui
  SECTION:=vuci
  CATEGORY:=VUCI
  TITLE:=Simple VUCI UI
  DEPENDS:=+vuci-app-simple-api
endef

define Package/vuci-app-simple-ui/description
  Simple VUCI UI
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
    # Create symlinks for proper paths
    mkdir -p /overlay/root/upper/usr/share/vuci/menu.d 2>/dev/null
    ln -sf /usr/local/usr/share/vuci/menu.d/simple.json /overlay/root/upper/usr/share/vuci/menu.d/simple.json 2>/dev/null
    
    # Restart web server
    /etc/init.d/uhttpd restart 2>/dev/null
}
exit 0
endef

$(eval $(call BuildPackage,vuci-app-simple-ui))
EOF

# Create output directory
mkdir -p bin/simple-packages

# Build packages using make
echo ""
echo "Building packages with make..."

# Determine optimal thread count
NUM_CORES=$(nproc)
NUM_THREADS=$((NUM_CORES * 2))  # Use 2x cores for better throughput
echo "System has $NUM_CORES cores, using $NUM_THREADS threads for building"
echo ""

# Set make options for faster building
export MAKEFLAGS="-j$NUM_THREADS"

# Try to build with make (may fail due to toolchain)
echo "Building API package with $NUM_THREADS threads..."
make -j$NUM_THREADS package/vuci-app-simple-api/{clean,compile} V=s 2>&1 | tail -20

echo ""
echo "Building UI package with $NUM_THREADS threads..."
make -j$NUM_THREADS package/vuci-app-simple-ui/{clean,compile} V=s 2>&1 | tail -20

# Check if packages were built
if [ -f "bin/packages/arm_cortex-a7_neon-vfpv4/vuci/vuci-app-simple-api"*.ipk ] || [ -f "bin/packages/ipq40xx/vuci/vuci-app-simple-api"*.ipk ]; then
    echo ""
    echo "Packages built successfully with make!"
    # Copy to simple-packages directory
    find bin/packages -name "vuci-app-simple*.ipk" -exec cp {} bin/simple-packages/ \;
else
    echo ""
    echo "Make build failed or packages not found, creating IPKs manually..."
    
    # Build API IPK manually
    cd package/vuci-app-simple-api
    mkdir -p ipk-build/data
    cp -r files/* ipk-build/data/
    
    cat > ipk-build/control << EOF
Package: vuci-app-simple-api
Version: 1.0-7
Depends: libc, lua
Section: vuci
Architecture: arm_cortex-a7_neon-vfpv4
Installed-Size: 1024
Description: Simple VUCI API service
EOF
    
    cat > ipk-build/postinst << 'EOF'
#!/bin/sh
exit 0
EOF
    chmod +x ipk-build/postinst
    
    echo "2.0" > ipk-build/debian-binary
    
    cd ipk-build
    tar -czf control.tar.gz control postinst
    tar -czf data.tar.gz -C data .
    tar -cf ../simple-api.tar debian-binary control.tar.gz data.tar.gz
    gzip -c ../simple-api.tar > "$SDK_DIR/bin/simple-packages/vuci-app-simple-api_1.0-7_arm_cortex-a7_neon-vfpv4.ipk"
    cd "$SDK_DIR"
    
    # Build UI IPK manually
    cd package/vuci-app-simple-ui
    mkdir -p ipk-build/data
    cp -r files/* ipk-build/data/
    
    cat > ipk-build/control << EOF
Package: vuci-app-simple-ui
Version: 1.0-7
Depends: libc, vuci-app-simple-api
Section: vuci
Architecture: arm_cortex-a7_neon-vfpv4
Installed-Size: 2048
Description: Simple VUCI UI
EOF
    
    cat > ipk-build/postinst << 'EOF'
#!/bin/sh
[ -z "${IPKG_INSTROOT}" ] && {
    mkdir -p /overlay/root/upper/usr/share/vuci/menu.d 2>/dev/null
    ln -sf /usr/local/usr/share/vuci/menu.d/simple.json /overlay/root/upper/usr/share/vuci/menu.d/simple.json 2>/dev/null
    /etc/init.d/uhttpd restart 2>/dev/null
}
exit 0
EOF
    chmod +x ipk-build/postinst
    
    echo "2.0" > ipk-build/debian-binary
    
    cd ipk-build
    tar -czf control.tar.gz control postinst
    tar -czf data.tar.gz -C data .
    tar -cf ../simple-ui.tar debian-binary control.tar.gz data.tar.gz
    gzip -c ../simple-ui.tar > "$SDK_DIR/bin/simple-packages/vuci-app-simple-ui_1.0-7_arm_cortex-a7_neon-vfpv4.ipk"
    cd "$SDK_DIR"
fi

echo ""
echo "========================================="
echo "BUILD COMPLETE"
echo "========================================="
echo ""
echo "Packages location:"
ls -la bin/simple-packages/*.ipk 2>/dev/null || ls -la bin/packages/*/vuci/*.ipk 2>/dev/null || echo "No packages found"
echo ""
echo "Copy packages to main directory for easy access:"
mkdir -p ~/vuci-packages
cp bin/simple-packages/*.ipk ~/vuci-packages/ 2>/dev/null || cp bin/packages/*/vuci/vuci-app-simple*.ipk ~/vuci-packages/ 2>/dev/null || true
echo ""
echo "Final packages:"
ls -la ~/vuci-packages/vuci-app-simple*.ipk 2>/dev/null || echo "No packages found in ~/vuci-packages/"
echo ""
echo "Build time: $SECONDS seconds"
