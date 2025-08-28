#!/bin/bash
set -e

echo "=== Building RUTOS Autonomy Packages (Simple Format) ==="

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="/tmp/autonomy-simple-build-$$"
PACKAGE_NAME="autonomy"
VERSION="1.0"
ARCHITECTURE="arm_cortex-a7_neon-vfpv4"

echo "Project root: $SCRIPT_DIR"
echo "Build directory: $BUILD_DIR"

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Create API package structure
echo "Creating API package..."
mkdir -p "api-package/usr/lib/lua/api/services"

# Copy API files
cp "$SCRIPT_DIR/vuci-app-autonomy-api/files/usr/lib/lua/api/services/config_autonomy.lua" "api-package/usr/lib/lua/api/services/"
cp "$SCRIPT_DIR/vuci-app-autonomy-api/files/usr/lib/lua/api/services/function_autonomy.lua" "api-package/usr/lib/lua/api/services/"

# Create API package control file
cat > "api-package/control" << EOF
Package: vuci-app-autonomy-api
Version: $VERSION
Depends: uci, ubus
Architecture: $ARCHITECTURE
Installed-Size: 1024
Description: VuCI API Support for Autonomy APP
EOF

# Build API package using gzipped tar format (like our working packages)
echo "Building API package..."
cd "api-package"
tar -czf control.tar.gz control
tar -czf data.tar.gz usr/
echo "2.0" > debian-binary
tar -czf "vuci-app-autonomy-api_${VERSION}_${ARCHITECTURE}.ipk" debian-binary control.tar.gz data.tar.gz
cd ..

# Create UI package structure
echo "Creating UI package..."
mkdir -p "ui-package/usr/share/vuci/menu.d"
mkdir -p "ui-package/www/assets"

# Copy UI files
cp "$SCRIPT_DIR/vuci-app-autonomy-ui/files/usr/share/vuci/menu.d/autonomy.json" "ui-package/usr/share/vuci/menu.d/"

# Create UI package control file
cat > "ui-package/control" << EOF
Package: vuci-app-autonomy-ui
Version: $VERSION
Depends: vuci-app-autonomy-api, uci, ubus
Architecture: $ARCHITECTURE
Installed-Size: 2048
Description: VuCI UI Support for Autonomy APP
EOF

# Build UI package using gzipped tar format
echo "Building UI package..."
cd "ui-package"
tar -czf control.tar.gz control
tar -czf data.tar.gz usr/ www/
echo "2.0" > debian-binary
tar -czf "vuci-app-autonomy-ui_${VERSION}_${ARCHITECTURE}.ipk" debian-binary control.tar.gz data.tar.gz
cd ..

# Copy packages to project directory
echo "Copying packages to project directory..."
cp "api-package/vuci-app-autonomy-api_${VERSION}_${ARCHITECTURE}.ipk" "$SCRIPT_DIR/"
cp "ui-package/vuci-app-autonomy-ui_${VERSION}_${ARCHITECTURE}.ipk" "$SCRIPT_DIR/"

echo "Build completed successfully!"
echo ""
echo "Packages created:"
echo "- API: $SCRIPT_DIR/vuci-app-autonomy-api_${VERSION}_${ARCHITECTURE}.ipk"
echo "- UI: $SCRIPT_DIR/vuci-app-autonomy-ui_${VERSION}_${ARCHITECTURE}.ipk"
echo ""
echo "To install on device:"
echo "1. Copy packages to device:"
echo "   scp $SCRIPT_DIR/vuci-app-autonomy-*.ipk root@192.168.80.1:/tmp/"
echo ""
echo "2. Install packages:"
echo "   ssh root@192.168.80.1 'opkg install /tmp/vuci-app-autonomy-api_${VERSION}_${ARCHITECTURE}.ipk'"
echo "   ssh root@192.168.80.1 'opkg install /tmp/vuci-app-autonomy-ui_${VERSION}_${ARCHITECTURE}.ipk'"
echo ""
echo "3. Restart services:"
echo "   ssh root@192.168.80.1 '/etc/init.d/rpcd restart && /etc/init.d/uhttpd restart'"

# Cleanup
rm -rf "$BUILD_DIR"





