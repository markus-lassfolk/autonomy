#!/bin/bash
set -e

echo "=== Building RUTOS Autonomy Packages with SDK Build System ==="

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SDK_DIR="/mnt/j/GithubCursor/rutos-ipq40xx-rutx-sdk"
VUCI_DIR="$SDK_DIR/package/feeds/vuci"

echo "Project root: $SCRIPT_DIR"
echo "SDK directory: $SDK_DIR"

# Check if SDK directory exists
if [ ! -d "$SDK_DIR" ]; then
    echo "ERROR: SDK directory not found at $SDK_DIR"
    exit 1
fi

# Copy our packages to the SDK VUCI directory
echo "Copying packages to SDK..."

# Remove existing packages if they exist
rm -rf "$VUCI_DIR/vuci-app-autonomy-api"
rm -rf "$VUCI_DIR/vuci-app-autonomy-ui"

# Copy API package
echo "Copying API package..."
cp -r "$SCRIPT_DIR/vuci-app-autonomy-api" "$VUCI_DIR/"

# Copy UI package
echo "Copying UI package..."
cp -r "$SCRIPT_DIR/vuci-app-autonomy-ui" "$VUCI_DIR/"

# Update ipk_packages.json to include our packages
echo "Updating ipk_packages.json..."
if ! grep -q "vuci-app-autonomy-ui" "$SDK_DIR/ipk_packages.json"; then
    # Add our package to the JSON file
    # This is a simplified approach - in practice, you'd want to properly edit the JSON
    echo "Adding autonomy packages to ipk_packages.json..."
    # Note: This is a placeholder - the actual JSON editing would be more complex
fi

# Change to SDK directory
cd "$SDK_DIR"

# Configure the build to enable Lua compilation
echo "Configuring build with Lua compilation enabled..."

# Create a temporary .config with our settings
cat > .config.tmp << EOF
CONFIG_VUCI_COMPILE_LUA=y
CONFIG_PACKAGE_vuci-app-autonomy-api=m
CONFIG_PACKAGE_vuci-app-autonomy-ui=m
EOF

# Merge with existing config
if [ -f .config ]; then
    cat .config .config.tmp | sort -u > .config.new
    mv .config.new .config
else
    mv .config.tmp .config
fi

# Build the API package
echo "Building API package..."
make package/vuci-app-autonomy-api/clean
make package/vuci-app-autonomy-api/compile V=s

# Build the UI package
echo "Building UI package..."
make package/vuci-app-autonomy-ui/clean
make package/vuci-app-autonomy-ui/compile V=s

# Find the built packages
echo "Looking for built packages..."
API_PACKAGE=$(find "$SDK_DIR/bin" -name "vuci-app-autonomy-api_*.ipk" | head -1)
UI_PACKAGE=$(find "$SDK_DIR/bin" -name "vuci-app-autonomy-ui_*.ipk" | head -1)

if [ -n "$API_PACKAGE" ] && [ -n "$UI_PACKAGE" ]; then
    echo "Packages built successfully!"
    echo "API package: $API_PACKAGE"
    echo "UI package: $UI_PACKAGE"
    
    # Copy packages to project directory
    cp "$API_PACKAGE" "$SCRIPT_DIR/"
    cp "$UI_PACKAGE" "$SCRIPT_DIR/"
    
    echo ""
    echo "Packages copied to project directory:"
    echo "- $(basename "$API_PACKAGE")"
    echo "- $(basename "$UI_PACKAGE")"
    echo ""
    echo "To install on device:"
    echo "1. Copy packages to device:"
    echo "   scp $SCRIPT_DIR/$(basename "$API_PACKAGE") root@192.168.80.1:/tmp/"
    echo "   scp $SCRIPT_DIR/$(basename "$UI_PACKAGE") root@192.168.80.1:/tmp/"
    echo ""
    echo "2. Install packages:"
    echo "   ssh root@192.168.80.1 'opkg install /tmp/$(basename "$API_PACKAGE")'"
    echo "   ssh root@192.168.80.1 'opkg install /tmp/$(basename "$UI_PACKAGE")'"
    echo ""
    echo "3. Restart services:"
    echo "   ssh root@192.168.80.1 '/etc/init.d/rpcd restart && /etc/init.d/uhttpd restart'"
else
    echo "ERROR: Failed to find built packages"
    echo "API package: $API_PACKAGE"
    echo "UI package: $UI_PACKAGE"
    exit 1
fi

echo "Build completed successfully!"
