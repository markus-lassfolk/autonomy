#!/bin/bash
# Fresh SDK setup script - downloads and configures RUTOS SDK properly
# This will get the toolchain working

set -e

echo "========================================="
echo "FRESH RUTOS SDK SETUP"
echo "========================================="
echo ""

# Configuration
SDK_DIR="$HOME/rutos-sdk-fresh"
SDK_URL="https://wiki.teltonika-networks.com/gpl/RUTX_R_GPL_00.07.11.2.tar.gz"
ARCH="arm_cortex-a7_neon-vfpv4"

echo "SDK will be installed to: $SDK_DIR"
echo ""

# Clean up old SDK if exists
if [ -d "$SDK_DIR" ]; then
    echo "Removing old SDK directory..."
    rm -rf "$SDK_DIR"
fi

# Create SDK directory
echo "Creating SDK directory..."
mkdir -p "$SDK_DIR"
cd "$SDK_DIR"

# Download SDK
echo "Downloading RUTOS SDK..."
echo "This may take a few minutes..."
wget -q --show-progress -O rutos-sdk.tar.gz "$SDK_URL" || {
    echo "Failed to download SDK from $SDK_URL"
    echo "Trying alternative URL..."
    SDK_URL="https://firmware.teltonika-networks.com/gpl/RUTX_R_GPL_00.07.11.2.tar.gz"
    wget -q --show-progress -O rutos-sdk.tar.gz "$SDK_URL" || {
        echo "Failed to download SDK. Please check your internet connection."
        exit 1
    }
}

# Extract SDK
echo ""
echo "Extracting SDK..."
tar -xzf rutos-sdk.tar.gz
rm rutos-sdk.tar.gz

# Find the extracted directory
SDK_EXTRACTED=$(find . -maxdepth 1 -type d -name "RUTX_R_*" | head -1)
if [ -z "$SDK_EXTRACTED" ]; then
    echo "Error: Could not find extracted SDK directory"
    exit 1
fi

# Move contents to current directory
echo "Organizing SDK files..."
mv "$SDK_EXTRACTED"/* .
rmdir "$SDK_EXTRACTED"

# Copy vuci-examples to package directory
echo ""
echo "Setting up VUCI examples..."
if [ -d "vuci-examples" ]; then
    cp -r vuci-examples/vuci-app-example-api package/
    cp -r vuci-examples/vuci-app-example-ui package/
    echo "VUCI examples copied to package directory"
else
    echo "Warning: vuci-examples not found"
fi

# Update feeds
echo ""
echo "Updating package feeds..."
./scripts/feeds update -a
./scripts/feeds install -a

# Create minimal config
echo ""
echo "Creating minimal configuration..."
cat > .config << 'EOF'
CONFIG_TARGET_ipq40xx=y
CONFIG_TARGET_ipq40xx_generic=y
CONFIG_TARGET_MULTI_PROFILE=y
CONFIG_TARGET_DEVICE_ipq40xx_generic_DEVICE_teltonika_rutx=y
CONFIG_TARGET_DEVICE_PACKAGES_ipq40xx_generic_DEVICE_teltonika_rutx=""
CONFIG_DEVEL=y
CONFIG_TOOLCHAINOPTS=y
CONFIG_SDK=y
CONFIG_PACKAGE_vuci-app-example-api=m
CONFIG_PACKAGE_vuci-app-example-ui=m
EOF

# Run defconfig
echo "Running defconfig..."
make defconfig

# Download and prepare toolchain
echo ""
echo "Downloading and preparing toolchain..."
echo "This will take 10-20 minutes on first run..."
make -j$(nproc) tools/install
make -j$(nproc) toolchain/install

echo ""
echo "========================================="
echo "SDK SETUP COMPLETE!"
echo "========================================="
echo ""
echo "SDK location: $SDK_DIR"
echo ""
echo "To build VUCI packages:"
echo "  cd $SDK_DIR"
echo "  make package/vuci-app-example-api/compile V=s"
echo "  make package/vuci-app-example-ui/compile V=s"
echo ""
echo "Built packages will be in:"
echo "  $SDK_DIR/bin/packages/$ARCH/vuci/"
echo ""


