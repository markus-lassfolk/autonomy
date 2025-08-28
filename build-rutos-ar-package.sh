#!/bin/bash
set -e

echo "=== Building RUTOS Autonomy Package with AR Format ==="

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
BUILD_DIR="/tmp/autonomy-rutos-package"
PACKAGE_NAME="autonomy"
VERSION="1.0.0"
ARCHITECTURE="arm_cortex-a7_neon-vfpv4"

echo "Building in: $BUILD_DIR"
echo "Architecture: $ARCHITECTURE"
echo "Version: $VERSION"

# Verify package structure exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Package structure not found at $BUILD_DIR"
    echo "Please run the package preparation steps first"
    exit 1
fi

# Create IPK package in standard OpenWrt AR format
echo "Creating IPK package..."
cd "$BUILD_DIR"

# Create data.tar.gz
echo "Creating data.tar.gz..."
tar -czf data.tar.gz files/usr/ files/etc/

# Create control file
echo "Creating control file..."
mkdir -p CONTROL

cat > CONTROL/control << EOF
Package: $PACKAGE_NAME
Version: $VERSION
Depends: uci, mwan3, ubus, luci-base, luci-compat
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
cat > CONTROL/postinst << 'EOF'
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

# Run UCI defaults
/etc/uci-defaults/70-autonomy

# Restart LuCI to pick up new menu items
/etc/init.d/rpcd restart
/etc/init.d/uhttpd restart

# Enable and start service
/etc/init.d/autonomy enable
/etc/init.d/autonomy start

echo "Autonomy package installed successfully!"
echo "Web interface available at: http://your-router-ip/cgi-bin/luci/admin/autonomy"
echo "Configuration available at: /usr/local/etc/autonomy/"
exit 0
EOF

chmod +x CONTROL/postinst

# Create prerm script
echo "Creating prerm script..."
cat > CONTROL/prerm << 'EOF'
#!/bin/sh
# Pre-removal script for autonomy package

# Stop and disable service
/etc/init.d/autonomy stop
/etc/init.d/autonomy disable

# Remove from UCI tracking
uci -q delete ucitrack.@autonomy[-1] 2>/dev/null || true
uci commit ucitrack

# Restart LuCI
/etc/init.d/rpcd restart
/etc/init.d/uhttpd restart

echo "Autonomy service stopped and disabled."
exit 0
EOF

chmod +x CONTROL/prerm

# Create control.tar.gz
echo "Creating control.tar.gz..."
tar -czf control.tar.gz CONTROL/

# Create debian-binary
echo "Creating debian-binary..."
echo "2.0" > debian-binary

# Create the final IPK using AR format (standard OpenWrt)
echo "Creating final IPK file using AR format..."
ar rcs "${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}_ar.ipk" debian-binary control.tar.gz data.tar.gz

# Move IPK to project root
mv "${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}_ar.ipk" "$PROJECT_ROOT/"

echo "IPK package created successfully!"
echo "Package: ${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}_ar.ipk"

# Show package size
PACKAGE_SIZE=$(du -h "${PROJECT_ROOT}/${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}_ar.ipk" | cut -f1)
echo "Package size: $PACKAGE_SIZE"

# Clean up temporary files
rm -f data.tar.gz control.tar.gz debian-binary
rm -rf CONTROL

cd "$PROJECT_ROOT"

echo "Build completed successfully!"
echo ""
echo "This package uses the standard OpenWrt AR format and should be compatible with RUTOS!"
echo "Install it using: opkg install ${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}_ar.ipk"





