#!/bin/bash
set -e

echo "=== Building Autonomy Web UI Packages ==="

# Set SDK path
SDK_PATH="/mnt/j/GithubCursor/rutos-ipq40xx-rutx-sdk"
cd "$SDK_PATH"

echo "Current directory: $(pwd)"
echo "SDK path: $SDK_PATH"

# Check if packages exist
if [ ! -d "package/base/vuci-app-autonomy-api" ]; then
    echo "ERROR: vuci-app-autonomy-api package not found"
    exit 1
fi

if [ ! -d "package/base/vuci-app-autonomy-ui" ]; then
    echo "ERROR: vuci-app-autonomy-ui package not found"
    exit 1
fi

echo "Packages found, starting build..."

# Clean previous builds
echo "Cleaning previous builds..."
make clean 2>/dev/null || true

# Build API package with FORCE=1 to override case-sensitive filesystem requirement
echo "Building vuci-app-autonomy-api..."
make package/vuci-app-autonomy-api/compile V=s FORCE=1

if [ $? -ne 0 ]; then
    echo "ERROR: Failed to build vuci-app-autonomy-api package"
    exit 1
fi

# Build UI package with FORCE=1 to override case-sensitive filesystem requirement
echo "Building vuci-app-autonomy-ui..."
make package/vuci-app-autonomy-ui/compile V=s FORCE=1

if [ $? -ne 0 ]; then
    echo "ERROR: Failed to build vuci-app-autonomy-ui package"
    exit 1
fi

# Find built packages
echo "Looking for built packages..."
API_PKG=$(find bin/packages -name "*vuci-app-autonomy-api*.ipk" | head -1)
UI_PKG=$(find bin/packages -name "*vuci-app-autonomy-ui*.ipk" | head -1)

if [ -z "$API_PKG" ] || [ -z "$UI_PKG" ]; then
    echo "ERROR: Failed to find built packages"
    echo "Available packages in bin/packages/:"
    ls -la bin/packages/ || echo "bin/packages/ directory not found"
    exit 1
fi

echo "Packages built successfully:"
echo "  API: $(basename $API_PKG)"
echo "  UI: $(basename $UI_PKG)"

# Copy packages to autonomy project directory
AUTONOMY_DIR="/mnt/j/GithubCursor/autonomy"
echo "Copying packages to: $AUTONOMY_DIR"

cp "$API_PKG" "$AUTONOMY_DIR/"
cp "$UI_PKG" "$AUTONOMY_DIR/"

echo "Build completed successfully!"
echo "Packages copied to: $AUTONOMY_DIR"
ls -la "$AUTONOMY_DIR"/*.ipk
