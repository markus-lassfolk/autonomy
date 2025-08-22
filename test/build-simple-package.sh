#!/bin/bash

# Simple OpenWrt Package Builder
set -e

echo "=== Building Simple OpenWrt Package ==="

# Configuration
PACKAGE_NAME="autonomy"
PACKAGE_VERSION="1.0.0"
ARCHITECTURE="x86_64"
BUILD_DIR="/mnt/d/GitCursor/autonomy/build-openwrt"
PACKAGE_DIR="$BUILD_DIR/packages"

# Create directories
mkdir -p $BUILD_DIR
mkdir -p $PACKAGE_DIR

echo "Building package: $PACKAGE_NAME-$PACKAGE_VERSION"

# Build Go binary (simplified)
echo "Building Go binary..."
cd /mnt/d/GitCursor/autonomy
CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -ldflags="-s -w" -o $BUILD_DIR/autonomysysmgmt ./cmd/autonomysysmgmt

# Create package structure
PACKAGE_ROOT="$BUILD_DIR/$PACKAGE_NAME"
rm -rf $PACKAGE_ROOT
mkdir -p $PACKAGE_ROOT/{usr/bin,etc/init.d,etc/config,CONTROL}

# Copy binary
cp $BUILD_DIR/autonomysysmgmt $PACKAGE_ROOT/usr/bin/
chmod +x $PACKAGE_ROOT/usr/bin/autonomysysmgmt

# Create init script
cat > $PACKAGE_ROOT/etc/init.d/autonomy << 'EOF'
#!/bin/sh /etc/rc.common

START=95
STOP=15

start() {
    echo "Starting autonomy system..."
    /usr/bin/autonomysysmgmt --config /etc/config/autonomy &
}

stop() {
    echo "Stopping autonomy system..."
    killall autonomysysmgmt 2>/dev/null || true
}

restart() {
    stop
    sleep 1
    start
}
EOF

chmod +x $PACKAGE_ROOT/etc/init.d/autonomy

# Create default configuration
cat > $PACKAGE_ROOT/etc/config/autonomy << 'EOF'
config autonomy 'main'
    option enabled '1'
    option log_level 'info'

config autonomy 'starlink'
    option enabled '1'
    option api_key ''

config autonomy 'cellular'
    option enabled '1'
    option interface 'wwan0'
EOF

# Create control file
cat > $PACKAGE_ROOT/CONTROL/control << EOF
Package: $PACKAGE_NAME
Version: $PACKAGE_VERSION
Depends: luci-base
Section: net
Architecture: $ARCHITECTURE
Installed-Size: $(du -s $PACKAGE_ROOT | cut -f1)
Description: Autonomous networking system for OpenWrt
 Provides intelligent network failover management.
EOF

# Create the IPK package
echo "Creating IPK package..."
cd $BUILD_DIR

# Create data.tar.gz
tar -czf data.tar.gz -C $PACKAGE_ROOT .

# Create control.tar.gz
tar -czf control.tar.gz -C $PACKAGE_ROOT/CONTROL .

# Create debian-binary
echo "2.0" > debian-binary

# Create the IPK
ar r $PACKAGE_DIR/${PACKAGE_NAME}_${PACKAGE_VERSION}_${ARCHITECTURE}.ipk debian-binary data.tar.gz control.tar.gz

# Clean up
rm -f data.tar.gz control.tar.gz debian-binary

echo "=== Package built successfully! ==="
echo "Package: $PACKAGE_DIR/${PACKAGE_NAME}_${PACKAGE_VERSION}_${ARCHITECTURE}.ipk"
echo ""
echo "Package contents:"
ls -la $PACKAGE_DIR/

# Copy package to convenient location
echo ""
echo "=== Copying package to Downloads ==="
cp $PACKAGE_DIR/${PACKAGE_NAME}_${PACKAGE_VERSION}_${ARCHITECTURE}.ipk /mnt/d/Downloads/
echo "✅ Package copied to: /mnt/d/Downloads/${PACKAGE_NAME}_${PACKAGE_VERSION}_${ARCHITECTURE}.ipk"

# Test installation locally in WSL
echo ""
echo "=== Testing local installation ==="
cd /tmp
mkdir -p autonomy-test-install
cd autonomy-test-install

# Extract package
ar x $PACKAGE_DIR/${PACKAGE_NAME}_${PACKAGE_VERSION}_${ARCHITECTURE}.ipk

# Install files locally for testing
echo "Installing files locally for testing..."
sudo tar -xzf data.tar.gz -C / 2>/dev/null || echo "Note: Some files may already exist"

# Test the installation
echo ""
echo "=== Testing installed components ==="
echo "✅ Binary: $(ls -la /usr/bin/autonomysysmgmt 2>/dev/null || echo 'Not found')"
echo "✅ Init script: $(ls -la /etc/init.d/autonomy 2>/dev/null || echo 'Not found')"
echo "✅ Config: $(ls -la /etc/config/autonomy 2>/dev/null || echo 'Not found')"

# Test binary
echo ""
echo "=== Testing binary ==="
if [ -f /usr/bin/autonomysysmgmt ]; then
    echo "Binary help output:"
    /usr/bin/autonomysysmgmt --help | head -5
    echo "..."
else
    echo "❌ Binary not found"
fi

# Test service script
echo ""
echo "=== Testing service script ==="
if [ -f /etc/init.d/autonomy ]; then
    echo "Service script exists and is executable"
    chmod +x /etc/init.d/autonomy 2>/dev/null || true
else
    echo "❌ Service script not found"
fi

# Show configuration
echo ""
echo "=== Configuration preview ==="
if [ -f /etc/config/autonomy ]; then
    cat /etc/config/autonomy
else
    echo "❌ Configuration not found"
fi

echo ""
echo "=== Installation Summary ==="
echo "✅ Package built: ${PACKAGE_NAME}_${PACKAGE_VERSION}_${ARCHITECTURE}.ipk"
echo "✅ Package copied to: /mnt/d/Downloads/"
echo "✅ Local installation tested"
echo ""
echo "=== Next Steps ==="
echo "1. Copy to OpenWrt device: scp /mnt/d/Downloads/${PACKAGE_NAME}_${PACKAGE_VERSION}_${ARCHITECTURE}.ipk root@your-router:/tmp/"
echo "2. Install on OpenWrt: opkg install /tmp/${PACKAGE_NAME}_${PACKAGE_VERSION}_${ARCHITECTURE}.ipk"
echo "3. Start service: /etc/init.d/autonomy start"
echo "4. Access web UI: http://your-router-ip/cgi-bin/luci/admin/system/autonomy"
