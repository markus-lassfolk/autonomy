#!/bin/bash
set -e

echo "=== Building RUTOS Autonomy Package (Linux Method) ==="

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
BUILD_DIR="/tmp/autonomy-build-linux"
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

# Copy binaries (create placeholders for now)
echo "Creating binary placeholders..."
cat > "$AUTONOMY_PACKAGE_DIR/usr/local/bin/autonomyd" << 'EOF'
#!/bin/sh
echo "Autonomy daemon placeholder - needs ARM binary"
echo "This is a placeholder for the autonomy daemon"
echo "Please build ARM binaries and replace this file"
EOF

cat > "$AUTONOMY_PACKAGE_DIR/usr/local/bin/autonomysysmgmt" << 'EOF'
#!/bin/sh
echo "Autonomy system management placeholder - needs ARM binary"
echo "This is a placeholder for the autonomy system management"
echo "Please build ARM binaries and replace this file"
EOF

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
Depends: uci, mwan3, ubus, gpsctl, gsmctl
Architecture: $ARCHITECTURE
Installed-Size: 20480
Description: Autonomous networking system for RUTOS devices
 Provides intelligent network failover, GPS tracking, and monitoring
 with Starlink integration and cellular failover capabilities.
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
echo "Web interface available at: http://your-router-ip/cgi-bin/luci/admin/autonomy"
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

# Create IPK package
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

# Create IPK file
echo "Creating IPK file..."
ar rcs "${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}.ipk" debian-binary control.tar.gz data.tar.gz

# Move IPK to project root
mv "${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}.ipk" "$PROJECT_ROOT/"

echo "IPK package created successfully!"
echo "Package: ${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}.ipk"

# Clean up
cd "$PROJECT_ROOT"
rm -rf "$BUILD_DIR"

echo "Build completed successfully!"
