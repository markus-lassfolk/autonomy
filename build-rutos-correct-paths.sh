#!/bin/bash
set -e

echo "=== Building RUTOS Autonomy Package with Correct File Paths ==="

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

# Create IPK package with correct file paths
echo "Creating IPK package..."
cd "$BUILD_DIR"

# Create a temporary directory with correct structure
TEMP_DATA_DIR="/tmp/autonomy-data-$$"
mkdir -p "$TEMP_DATA_DIR"

# Copy files with correct paths (without the 'files/' prefix)
echo "Creating correct file structure..."
cp -r files/usr "$TEMP_DATA_DIR/"
cp -r files/etc "$TEMP_DATA_DIR/"

# Create data.tar.gz with correct structure
echo "Creating data.tar.gz..."
cd "$TEMP_DATA_DIR"
tar -czf data.tar.gz usr/ etc/
cd "$BUILD_DIR"

# Create control file (minimal dependencies)
echo "Creating control file..."
cat > control << EOF
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
cat > postinst << 'EOF'
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

# Run UCI defaults (if exists)
if [ -f /etc/uci-defaults/70-autonomy ]; then
    /etc/uci-defaults/70-autonomy
fi

# Restart services (if they exist)
if [ -f /etc/init.d/rpcd ]; then
    /etc/init.d/rpcd restart
fi
if [ -f /etc/init.d/uhttpd ]; then
    /etc/init.d/uhttpd restart
fi

# Enable and start service
if [ -f /etc/init.d/autonomy ]; then
    /etc/init.d/autonomy enable
    /etc/init.d/autonomy start
fi

echo "Autonomy package installed successfully!"
echo "Configuration available at: /usr/local/etc/autonomy/"
exit 0
EOF

chmod +x postinst

# Create prerm script
echo "Creating prerm script..."
cat > prerm << 'EOF'
#!/bin/sh
# Pre-removal script for autonomy package

# Stop and disable service
if [ -f /etc/init.d/autonomy ]; then
    /etc/init.d/autonomy stop
    /etc/init.d/autonomy disable
fi

# Remove from UCI tracking
uci -q delete ucitrack.@autonomy[-1] 2>/dev/null || true
uci commit ucitrack

# Restart services (if they exist)
if [ -f /etc/init.d/rpcd ]; then
    /etc/init.d/rpcd restart
fi
if [ -f /etc/init.d/uhttpd ]; then
    /etc/init.d/uhttpd restart
fi

echo "Autonomy service stopped and disabled."
exit 0
EOF

chmod +x prerm

# Create control.tar.gz with files directly in root
echo "Creating control.tar.gz..."
tar -czf control.tar.gz control postinst prerm

# Create debian-binary
echo "Creating debian-binary..."
echo "2.0" > debian-binary

# Create signature file (required by RUTOS)
echo "Creating signature file..."
echo "Signature: This is a placeholder signature for autonomy package" > control+data.sig

# Copy the data.tar.gz from temp directory
cp "$TEMP_DATA_DIR/data.tar.gz" .

# Create the final IPK as a gzipped tar archive (RUTOS format)
echo "Creating final IPK file..."
tar -czf "${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}_correct_paths.ipk" debian-binary control.tar.gz data.tar.gz control+data.sig

# Move IPK to project root
mv "${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}_correct_paths.ipk" "$PROJECT_ROOT/"

echo "IPK package created successfully!"
echo "Package: ${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}_correct_paths.ipk"

# Show package size
PACKAGE_SIZE=$(du -h "${PROJECT_ROOT}/${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}_correct_paths.ipk" | cut -f1)
echo "Package size: $PACKAGE_SIZE"

# Clean up temporary files
rm -f data.tar.gz control.tar.gz debian-binary control+data.sig control postinst prerm
rm -rf "$TEMP_DATA_DIR"

cd "$PROJECT_ROOT"

echo "Build completed successfully!"
echo ""
echo "This package uses correct file paths and should install successfully!"
echo "Install it using: opkg install ${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}_correct_paths.ipk"





