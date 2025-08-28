#!/bin/bash
# Download and setup a clean RUTOS SDK
# This gets a fresh SDK without the configuration issues

set -e

echo "========================================="
echo "DOWNLOADING CLEAN RUTOS SDK"
echo "========================================="
echo ""

# Configuration
SDK_DIR="$HOME/rutos-sdk-clean"
SDK_TARBALL="$HOME/rutos-sdk.tar.gz"

# Clean up old SDK
if [ -d "$SDK_DIR" ]; then
    echo "Removing old SDK directory..."
    rm -rf "$SDK_DIR"
fi

mkdir -p "$SDK_DIR"

# Check if we have a local copy of the SDK tarball
if [ -f "$SDK_TARBALL" ]; then
    echo "Using existing SDK tarball: $SDK_TARBALL"
else
    echo "Downloading RUTOS SDK..."
    echo "Trying Teltonika wiki..."
    wget -q --show-progress -O "$SDK_TARBALL" "https://wiki.teltonika-networks.com/gpl/RUTX_R_GPL_00.07.11.2.tar.gz" || {
        echo "Wiki download failed, trying firmware site..."
        wget -q --show-progress -O "$SDK_TARBALL" "https://firmware.teltonika-networks.com/gpl/RUTX_R_GPL_00.07.11.2.tar.gz" || {
            echo "Download failed. Manual download may be required."
            echo "Please download the SDK from:"
            echo "  https://wiki.teltonika-networks.com/view/RUTX11_GPL"
            exit 1
        }
    }
fi

# Extract SDK
echo ""
echo "Extracting SDK to $SDK_DIR..."
cd "$SDK_DIR"
tar -xzf "$SDK_TARBALL" --strip-components=1

# Check if extraction was successful
if [ ! -f "Makefile" ] || [ ! -d "package" ]; then
    echo "Error: SDK extraction failed or unexpected structure"
    ls -la
    exit 1
fi

echo ""
echo "SDK extracted successfully"

# Copy VUCI examples to package directory
echo ""
echo "Setting up VUCI examples..."
if [ -d "vuci-examples" ]; then
    echo "Found vuci-examples directory"
    
    # Create our own simple example packages
    mkdir -p package/vuci-app-simple-api
    mkdir -p package/vuci-app-simple-ui
    
    # Create simple API package
    cat > package/vuci-app-simple-api/Makefile << 'EOF'
include $(TOPDIR)/rules.mk

PKG_NAME:=vuci-app-simple-api
PKG_VERSION:=1.0
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/vuci-app-simple-api
  SECTION:=vuci
  CATEGORY:=VUCI
  TITLE:=Simple VUCI API
  DEPENDS:=+lua
endef

define Package/vuci-app-simple-api/description
  Simple VUCI API service
endef

define Build/Compile
endef

define Package/vuci-app-simple-api/install
	$(INSTALL_DIR) $(1)/usr/lib/lua/api/services
	echo "return {test = function() return {success=true} end}" > $(1)/usr/lib/lua/api/services/simple.lua
endef

$(eval $(call BuildPackage,vuci-app-simple-api))
EOF

    # Create simple UI package
    cat > package/vuci-app-simple-ui/Makefile << 'EOF'
include $(TOPDIR)/rules.mk

PKG_NAME:=vuci-app-simple-ui
PKG_VERSION:=1.0
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/vuci-app-simple-ui
  SECTION:=vuci
  CATEGORY:=VUCI
  TITLE:=Simple VUCI UI
  DEPENDS:=+vuci-app-simple-api
endef

define Package/vuci-app-simple-ui/description
  Simple VUCI UI
endef

define Build/Compile
endef

define Package/vuci-app-simple-ui/install
	$(INSTALL_DIR) $(1)/usr/share/vuci/menu.d
	echo '{"services/simple":{"title":"Simple","view":"services/Simple"}}' > $(1)/usr/share/vuci/menu.d/simple.json
	$(INSTALL_DIR) $(1)/www/simple
	echo '<html><body><h1>Simple VUCI App</h1></body></html>' > $(1)/www/simple/index.html
endef

$(eval $(call BuildPackage,vuci-app-simple-ui))
EOF

    echo "Simple example packages created"
else
    echo "vuci-examples not found, creating simple packages anyway"
fi

# Create a minimal config that avoids circular dependencies
echo ""
echo "Creating minimal build configuration..."
cat > .config << 'EOF'
CONFIG_TARGET_ipq40xx=y
CONFIG_TARGET_ipq40xx_generic=y
CONFIG_TARGET_ipq40xx_generic_Default=y
CONFIG_PACKAGE_vuci-app-simple-api=y
CONFIG_PACKAGE_vuci-app-simple-ui=y
EOF

# Run defconfig to expand the minimal config
echo ""
echo "Expanding configuration..."
make defconfig

echo ""
echo "========================================="
echo "CLEAN SDK READY"
echo "========================================="
echo ""
echo "SDK location: $SDK_DIR"
echo ""
echo "To build the simple packages:"
echo "  cd $SDK_DIR"
echo "  make package/vuci-app-simple-api/compile V=s"
echo "  make package/vuci-app-simple-ui/compile V=s"
echo ""
echo "Or build everything (takes time):"
echo "  make -j\$(nproc)"
echo ""


