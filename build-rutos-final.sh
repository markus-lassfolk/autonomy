#!/bin/bash
set -e

echo "=== Building Final RUTOS Autonomy Package with Real ARM Binaries ==="

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
BUILD_DIR="/tmp/autonomy-final"
PACKAGE_NAME="autonomy"
VERSION="1.0.0"
ARCHITECTURE="arm_cortex-a7_neon-vfpv4"

echo "Building in: $BUILD_DIR"
echo "Architecture: $ARCHITECTURE"
echo "Version: $VERSION"

# Clean and create build directory
echo "Cleaning build directory..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Create package structure
echo "Creating package structure..."
AUTONOMY_PACKAGE_DIR="$BUILD_DIR/$PACKAGE_NAME"
mkdir -p "$AUTONOMY_PACKAGE_DIR/usr/local/bin"
mkdir -p "$AUTONOMY_PACKAGE_DIR/usr/local/etc/autonomy"
mkdir -p "$AUTONOMY_PACKAGE_DIR/usr/local/share/autonomy-ui"
mkdir -p "$AUTONOMY_PACKAGE_DIR/etc/init.d"
mkdir -p "$AUTONOMY_PACKAGE_DIR/etc/config"

# Copy real ARM binaries
echo "Copying real ARM binaries..."
if [ -f "$PROJECT_ROOT/bin/autonomyd-arm" ]; then
    cp "$PROJECT_ROOT/bin/autonomyd-arm" "$AUTONOMY_PACKAGE_DIR/usr/local/bin/autonomyd"
    echo "Copied autonomyd ARM binary (19MB)"
else
    echo "Error: autonomyd-arm binary not found!"
    exit 1
fi

if [ -f "$PROJECT_ROOT/bin/autonomysysmgmt-arm" ]; then
    cp "$PROJECT_ROOT/bin/autonomysysmgmt-arm" "$AUTONOMY_PACKAGE_DIR/usr/local/bin/autonomysysmgmt"
    echo "Copied autonomysysmgmt ARM binary (13MB)"
else
    echo "Error: autonomysysmgmt-arm binary not found!"
    exit 1
fi

chmod +x "$AUTONOMY_PACKAGE_DIR/usr/local/bin/autonomyd"
chmod +x "$AUTONOMY_PACKAGE_DIR/usr/local/bin/autonomysysmgmt"

# Copy package files
echo "Copying package files..."
if [ -d "$PROJECT_ROOT/package/autonomy/files" ]; then
    cp -r "$PROJECT_ROOT/package/autonomy/files"/* "$AUTONOMY_PACKAGE_DIR/usr/local/etc/autonomy/"
    echo "Copied package files"
else
    echo "Warning: Package files directory not found"
fi

# Copy web UI files
echo "Copying web UI files..."
if [ -d "$PROJECT_ROOT/vuci-app-autonomy-ui/src" ]; then
    cp -r "$PROJECT_ROOT/vuci-app-autonomy-ui/src"/* "$AUTONOMY_PACKAGE_DIR/usr/local/share/autonomy-ui/"
    echo "Copied web UI files"
else
    echo "Warning: Web UI directory not found"
fi

# Create control file
echo "Creating control file..."
mkdir -p "$AUTONOMY_PACKAGE_DIR/CONTROL"

cat > "$AUTONOMY_PACKAGE_DIR/CONTROL/control" << EOF
Package: $PACKAGE_NAME
Version: $VERSION
Depends: uci, mwan3, ubus
Architecture: $ARCHITECTURE
Installed-Size: 32768
Description: Autonomous networking system for RUTOS devices
 Provides intelligent network failover, GPS tracking, and monitoring
 with Starlink integration and cellular failover capabilities.
 Includes web UI and comprehensive configuration management.
 Built with real ARM binaries for optimal performance.
EOF

# Create postinst script
echo "Creating postinst script..."
cat > "$AUTONOMY_PACKAGE_DIR/CONTROL/postinst" << 'EOF'
#!/bin/sh
# Post-installation script for autonomy package

# Set executable permissions
chmod +x /usr/local/bin/autonomyd
chmod +x /usr/local/bin/autonomysysmgmt

# Create necessary directories
mkdir -p /etc/autonomy
mkdir -p /var/log/autonomy
mkdir -p /var/lib/autonomy

# Set up default configuration if not exists
if [ ! -f /etc/config/autonomy ]; then
    cp /usr/local/etc/autonomy/autonomy.config /etc/config/autonomy
fi

# Enable and start service
/etc/init.d/autonomy enable
/etc/init.d/autonomy start

echo "Autonomy package installed successfully!"
echo "Real ARM binaries available at: /usr/local/bin/"
echo "Web interface available at: http://your-router-ip/cgi-bin/luci/admin/autonomy"
echo "Configuration available at: /usr/local/etc/autonomy/"
EOF

chmod +x "$AUTONOMY_PACKAGE_DIR/CONTROL/postinst"

# Create prerm script
echo "Creating prerm script..."
cat > "$AUTONOMY_PACKAGE_DIR/CONTROL/prerm" << 'EOF'
#!/bin/sh
# Pre-removal script for autonomy package

# Stop and disable service
/etc/init.d/autonomy stop
/etc/init.d/autonomy disable

echo "Autonomy service stopped and disabled."
EOF

chmod +x "$AUTONOMY_PACKAGE_DIR/CONTROL/prerm"

# Create IPK package in RUTOS format
echo "Creating IPK package..."
cd "$AUTONOMY_PACKAGE_DIR"

# Create data.tar.gz
echo "Creating data.tar.gz..."
tar -czf data.tar.gz usr/ etc/

# Create control.tar.gz
echo "Creating control.tar.gz..."
tar -czf control.tar.gz CONTROL/

# Create debian-binary
echo "Creating debian-binary..."
echo "2.0" > debian-binary

# Create the final IPK as a gzipped tar archive (RUTOS format)
echo "Creating final IPK file..."
tar -czf "${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}.ipk" debian-binary control.tar.gz data.tar.gz

# Move IPK to project root
mv "${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}.ipk" "$PROJECT_ROOT/"

echo "IPK package created successfully!"
echo "Package: ${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}.ipk"

# Show package size
PACKAGE_SIZE=$(du -h "${PROJECT_ROOT}/${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}.ipk" | cut -f1)
echo "Package size: $PACKAGE_SIZE"

# Clean up
cd "$PROJECT_ROOT"
rm -rf "$BUILD_DIR"

echo "Build completed successfully!"





