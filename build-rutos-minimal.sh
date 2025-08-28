#!/bin/bash
set -e

echo "=== Building Minimal RUTOS Autonomy Package ==="

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
BUILD_DIR="/tmp/autonomy-minimal"
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

# Create simple binary placeholder
echo "Creating binary placeholder..."
cat > "$AUTONOMY_PACKAGE_DIR/usr/local/bin/autonomyd" << 'EOF'
#!/bin/sh
echo "Autonomy daemon placeholder"
echo "Version: 1.0.0"
echo "Status: Installed successfully"
echo "Architecture: arm_cortex-a7_neon-vfpv4"
EOF

chmod +x "$AUTONOMY_PACKAGE_DIR/usr/local/bin/autonomyd"

# Create control file with NO dependencies
echo "Creating control file..."
mkdir -p "$AUTONOMY_PACKAGE_DIR/CONTROL"

cat > "$AUTONOMY_PACKAGE_DIR/CONTROL/control" << EOF
Package: $PACKAGE_NAME
Version: $VERSION
Depends: 
Architecture: $ARCHITECTURE
Installed-Size: 1024
Description: Autonomous networking system for RUTOS devices
 Simple test package for autonomy system.
EOF

# Create simple postinst script
echo "Creating postinst script..."
cat > "$AUTONOMY_PACKAGE_DIR/CONTROL/postinst" << 'EOF'
#!/bin/sh
echo "Autonomy package installed successfully!"
echo "Binary available at: /usr/local/bin/autonomyd"
EOF

chmod +x "$AUTONOMY_PACKAGE_DIR/CONTROL/postinst"

# Create IPK package
echo "Creating IPK package..."
cd "$AUTONOMY_PACKAGE_DIR"

# Create data.tar.gz
echo "Creating data.tar.gz..."
tar -czf data.tar.gz usr/

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

