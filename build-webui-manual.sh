#!/bin/bash
set -e

echo "=== Building Autonomy Web UI Packages (Manual Method) ==="

# Set paths
AUTONOMY_DIR="/mnt/j/GithubCursor/autonomy"
BUILD_DIR="$AUTONOMY_DIR/build-webui-manual"

echo "Building in: $BUILD_DIR"

# Create build directory
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Create API package structure
echo "Creating API package..."
API_PKG_DIR="$BUILD_DIR/vuci-app-autonomy-api"
mkdir -p "$API_PKG_DIR/control"
mkdir -p "$API_PKG_DIR/data/usr/libexec/autonomy-api"
mkdir -p "$API_PKG_DIR/data/etc/init.d"
mkdir -p "$API_PKG_DIR/data/etc/config"

# Create API control file
cat > "$API_PKG_DIR/control/control" << 'EOF'
Package: vuci-app-autonomy-api
Version: 1.0.0
Depends: autonomy, luci-base, luci-compat, uhttpd
Section: net
Architecture: all
Installed-Size: 1024
Description: VuCI API for Autonomy System
 API backend for the Autonomy autonomous networking system.
 Provides REST API endpoints for monitoring, configuration, and control.
EOF

# Copy API files
cp "$AUTONOMY_DIR/vuci-app-autonomy-api/src/autonomy-api.lua" "$API_PKG_DIR/data/usr/libexec/autonomy-api/"
cp "$AUTONOMY_DIR/vuci-app-autonomy-api/src/autonomy-api.init" "$API_PKG_DIR/data/etc/init.d/autonomy-api"
cp "$AUTONOMY_DIR/vuci-app-autonomy-api/src/autonomy-api.config" "$API_PKG_DIR/data/etc/config/autonomy-api"

# Create UI package structure
echo "Creating UI package..."
UI_PKG_DIR="$BUILD_DIR/vuci-app-autonomy-ui"
mkdir -p "$UI_PKG_DIR/control"
mkdir -p "$UI_PKG_DIR/data/usr/share/autonomy-ui"

# Create UI control file
cat > "$UI_PKG_DIR/control/control" << 'EOF'
Package: vuci-app-autonomy-ui
Version: 1.0.0
Depends: vuci-app-autonomy-api, luci-base, luci-compat
Section: net
Architecture: all
Installed-Size: 2048
Description: VuCI UI for Autonomy System
 Comprehensive web UI for the Autonomy autonomous networking system.
 Provides monitoring, configuration, and control interface.
EOF

# Copy UI files
cp -r "$AUTONOMY_DIR/vuci-app-autonomy-ui/src/"* "$UI_PKG_DIR/data/usr/share/autonomy-ui/"

# Create IPK packages
echo "Creating IPK packages..."

# Create API IPK
cd "$API_PKG_DIR"
tar -czf control.tar.gz -C control .
tar -czf data.tar.gz -C data .
echo "2.0" > debian-binary
ar r vuci-app-autonomy-api_1.0.0_all.ipk debian-binary control.tar.gz data.tar.gz
mv vuci-app-autonomy-api_1.0.0_all.ipk "$AUTONOMY_DIR/"

# Create UI IPK
cd "$UI_PKG_DIR"
tar -czf control.tar.gz -C control .
tar -czf data.tar.gz -C data .
echo "2.0" > debian-binary
ar r vuci-app-autonomy-ui_1.0.0_all.ipk debian-binary control.tar.gz data.tar.gz
mv vuci-app-autonomy-ui_1.0.0_all.ipk "$AUTONOMY_DIR/"

# Clean up
cd "$AUTONOMY_DIR"
rm -rf "$BUILD_DIR"

echo "IPK packages created successfully!"
echo "Packages in $AUTONOMY_DIR:"
ls -la *.ipk

echo "Build completed successfully!"
