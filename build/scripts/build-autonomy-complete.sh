#!/bin/bash

# Autonomy Complete System Build Script
# Builds both daemon and UI together with proper SDK toolchain

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
SDK_DIR="/mnt/wsl/SDK"

echo -e "${BLUE}🚀 Building Autonomy Complete System${NC}"
echo "=================================="
echo "Project Root: $PROJECT_ROOT"
echo "Build Dir: $BUILD_DIR"
echo "SDK Dir: $SDK_DIR"
echo ""

# Check if SDK is available
if [ ! -d "$SDK_DIR" ]; then
    echo -e "${RED}❌ SDK directory not found: $SDK_DIR${NC}"
    echo "Please ensure the SDK is properly mounted"
    exit 1
fi

# Check if we're in the right directory
if [ ! -f "$PROJECT_ROOT/VERSION" ]; then
    echo -e "${RED}❌ VERSION file not found in project root${NC}"
    echo "Please run this script from the project root or ensure VERSION file exists"
    exit 1
fi

# Load version information
source "$PROJECT_ROOT/VERSION"
echo -e "${GREEN}📦 Building version: $AUTONOMY_VERSION-$AUTONOMY_VERSION_BUILD${NC}"
echo ""

# Clean previous builds
echo -e "${YELLOW}🧹 Cleaning previous builds...${NC}"
rm -rf "$BUILD_DIR/packages/utils/tlt-autonomy-complete"
rm -rf "$SDK_DIR/rutos-ipq40xx-rutx-sdk/build_dir/target-arm_cortex-a7+neon-vfpv4_musl_eabi/tlt-autonomy-complete"

# Create build directory structure
echo -e "${YELLOW}📁 Creating build directory structure...${NC}"
mkdir -p "$BUILD_DIR/packages/utils/tlt-autonomy-complete"

# Copy Makefile
echo -e "${YELLOW}📋 Copying build configuration...${NC}"
cp "$PROJECT_ROOT/build/packages/utils/tlt-autonomy-complete/Makefile" "$BUILD_DIR/packages/utils/tlt-autonomy-complete/" 2>/dev/null || echo "Makefile already exists or will be created by SDK"

# Process UI files with SDK toolchain
echo -e "${YELLOW}🎨 Processing UI files with SDK toolchain...${NC}"

# Create processed UI directory
UI_PROCESSED_DIR="$BUILD_DIR/packages/utils/tlt-autonomy-complete/ui-processed"
mkdir -p "$UI_PROCESSED_DIR"

# Process HTML files (minify and optimize)
echo "  Processing HTML files..."
for html_file in "$PROJECT_ROOT/src/web/autonomy-ui"/*.html; do
    if [ -f "$html_file" ]; then
        filename=$(basename "$html_file")
        echo "    Processing: $filename"
        
        # Use SDK's HTML minifier if available, otherwise copy as-is
        if command -v html-minifier &> /dev/null; then
            html-minifier --collapse-whitespace --remove-comments --minify-css --minify-js \
                "$html_file" > "$UI_PROCESSED_DIR/$filename"
        else
            cp "$html_file" "$UI_PROCESSED_DIR/$filename"
        fi
    fi
done

# Process JavaScript files (minify and optimize)
echo "  Processing JavaScript files..."
for js_file in "$PROJECT_ROOT/src/web/autonomy-ui"/*.js; do
    if [ -f "$js_file" ]; then
        filename=$(basename "$js_file")
        echo "    Processing: $filename"
        
        # Use SDK's JavaScript minifier if available, otherwise copy as-is
        if command -v uglifyjs &> /dev/null; then
            uglifyjs "$js_file" --compress --mangle -o "$UI_PROCESSED_DIR/$filename"
        else
            cp "$js_file" "$UI_PROCESSED_DIR/$filename"
        fi
    fi
done

# Process VUCI integration files
echo "  Processing VUCI integration..."
mkdir -p "$UI_PROCESSED_DIR/vuci"
if [ -f "$PROJECT_ROOT/src/web/autonomy-ui/ui/data/usr/share/vuci/menu.d/autonomy.json" ]; then
    cp "$PROJECT_ROOT/src/web/autonomy-ui/ui/data/usr/share/vuci/menu.d/autonomy.json" \
       "$UI_PROCESSED_DIR/vuci/"
fi

# Create web server configuration
echo "  Creating web server configuration..."
mkdir -p "$UI_PROCESSED_DIR/config"
cat > "$UI_PROCESSED_DIR/config/autonomy-ui" << 'EOF'
#!/bin/sh
# Autonomy UI Web Server Configuration

# Enable UBUS support for API calls
uci set uhttpd.main.ubus_prefix="/ubus"
uci set uhttpd.main.ubus_timeout=30

# Add Autonomy UI location
uci add_list uhttpd.main.location="/autonomy"
uci add_list uhttpd.main.location="/autonomy/"

# Set document root for Autonomy UI
uci set uhttpd.main.home="/www/autonomy"

# Commit changes
uci commit uhttpd

# Restart uhttpd
/etc/init.d/uhttpd restart
EOF

chmod +x "$UI_PROCESSED_DIR/config/autonomy-ui"

# Create setup script
echo "  Creating setup script..."
cat > "$UI_PROCESSED_DIR/setup.sh" << 'EOF'
#!/bin/sh
# Autonomy Complete System Setup Script

echo "Setting up Autonomy Complete System..."

# Start the daemon
/etc/init.d/autonomy start

# Configure web interface
/etc/init.d/autonomy-ui

# Set proper permissions
chmod 644 /www/autonomy/*.html
chmod 644 /www/autonomy/*.js
chmod 644 /usr/share/vuci/menu.d/autonomy.json

# Get router IP
ROUTER_IP=$(uci get network.lan.ipaddr 2>/dev/null || echo "192.168.1.1")

echo "Autonomy Complete System setup complete!"
echo ""
echo "Access the dashboard at:"
echo "  http://$ROUTER_IP/autonomy"
echo "  http://$ROUTER_IP/autonomy.html"
echo ""
echo "Or through VUCI menu: Services > Autonomy"
echo ""
echo "Daemon status: /etc/init.d/autonomy status"
echo "UI status: /etc/init.d/autonomy-ui status"
EOF

chmod +x "$UI_PROCESSED_DIR/setup.sh"

echo -e "${GREEN}✅ UI processing complete${NC}"
echo ""

# Build the package using SDK
echo -e "${YELLOW}🔨 Building package with SDK toolchain...${NC}"
cd "$SDK_DIR/rutos-ipq40xx-rutx-sdk"

# Clean previous build
make package/feeds/autonomy/tlt-autonomy-complete/clean

# Build the package
make package/feeds/autonomy/tlt-autonomy-complete/compile V=s

# Check if build was successful
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ Build successful!${NC}"
    
    # Find the generated IPK file
    IPK_FILE=$(find bin/packages/arm_cortex-a7_neon-vfpv4/autonomy/ -name "tlt-autonomy-complete_*.ipk" | head -1)
    
    if [ -n "$IPK_FILE" ]; then
        echo -e "${GREEN}📦 Package created: $IPK_FILE${NC}"
        
        # Copy to project build directory
        cp "$IPK_FILE" "$PROJECT_ROOT/build/packages/"
        
        # Get package info
        PACKAGE_SIZE=$(du -h "$IPK_FILE" | cut -f1)
        PACKAGE_NAME=$(basename "$IPK_FILE")
        
        echo ""
        echo -e "${GREEN}🎉 Autonomy Complete System Build Summary${NC}"
        echo "=========================================="
        echo "Package: $PACKAGE_NAME"
        echo "Size: $PACKAGE_SIZE"
        echo "Location: $PROJECT_ROOT/build/packages/$PACKAGE_NAME"
        echo ""
        echo -e "${BLUE}📋 Installation Instructions:${NC}"
        echo "1. Copy the IPK file to your RUTOS device"
        echo "2. Install: opkg install $PACKAGE_NAME"
        echo "3. Setup: /usr/bin/autonomy-setup"
        echo "4. Access: http://[router-ip]/autonomy"
        echo ""
        echo -e "${BLUE}🔧 Features Included:${NC}"
        echo "• Intelligent multi-interface failover daemon"
        echo "• Enhanced web dashboard with real-time monitoring"
        echo "• ML-powered network predictions"
        echo "• Starlink integration and tracking"
        echo "• GPS monitoring and location services"
        echo "• Comprehensive system health monitoring"
        echo "• VUCI integration for easy management"
        echo "• Configuration management interface"
        echo "• Log viewing and analysis tools"
        echo ""
        
    else
        echo -e "${RED}❌ IPK file not found after build${NC}"
        exit 1
    fi
    
else
    echo -e "${RED}❌ Build failed!${NC}"
    echo "Check the build log for details"
    exit 1
fi

echo -e "${GREEN}🚀 Build process completed successfully!${NC}"
