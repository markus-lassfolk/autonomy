#!/bin/bash
# autonomy-install.sh - Automated installation script for RUTOS Autonomy System
# Version: 1.0.0
# Compatible with: RUTOS RUTX50 and similar devices

set -e

echo "=========================================="
echo "Autonomy System Installation for RUTOS"
echo "=========================================="
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Error: This script must be run as root"
    echo "Please run: sudo $0"
    exit 1
fi

# Check if we're on a RUTOS device
if [ ! -f "/etc/os-release" ] || ! grep -q "RUTOS" /etc/os-release; then
    echo "Warning: This script is designed for RUTOS devices"
    echo "Continue anyway? (y/N)"
    read -r response
    if [[ ! "$response" =~ ^[Yy]$ ]]; then
        echo "Installation cancelled."
        exit 1
    fi
fi

echo "Starting Autonomy System installation..."
echo ""

# Create temporary directory
TEMP_DIR="/tmp/autonomy-install-$$"
mkdir -p "$TEMP_DIR"
cd "$TEMP_DIR"

echo "Step 1: Downloading autonomy package..."
# Note: Replace with actual download URL when hosting
echo "Please download the autonomy package manually and place it in the current directory"
echo "Package name: autonomy_1.0.0_arm_cortex-a7_neon-vfpv4_correct.ipk"
echo ""
echo "Press Enter when the package is ready..."
read -r

# Check if package exists
if [ ! -f "autonomy_1.0.0_arm_cortex-a7_neon-vfpv4_correct.ipk" ]; then
    echo "Error: Package file not found!"
    echo "Please ensure autonomy_1.0.0_arm_cortex-a7_neon-vfpv4_correct.ipk is in the current directory"
    exit 1
fi

echo "Step 2: Extracting package..."
tar -xzf autonomy_1.0.0_arm_cortex-a7_neon-vfpv4_correct.ipk
tar -xzf data.tar.gz

echo "Step 3: Installing binaries..."
mkdir -p /usr/local/bin
cp -r usr/local/bin/* /usr/local/bin/
chmod +x /usr/local/bin/*

echo "Step 4: Installing configuration files..."
mkdir -p /usr/local/etc
cp -r usr/local/etc/* /usr/local/etc/

echo "Step 5: Installing web UI components..."
mkdir -p /usr/local/share
cp -r usr/local/share/* /usr/local/share/

echo "Step 6: Installing service scripts..."
mkdir -p /etc/init.d
cp -r etc/init.d/* /etc/init.d/
chmod +x /etc/init.d/autonomy

echo "Step 7: Installing LuCI web interface..."
mkdir -p /usr/local/lib/lua/luci/controller/admin
mkdir -p /usr/local/lib/lua/luci/model/cbi/admin_autonomy
mkdir -p /usr/local/lib/lua/luci/view/admin_autonomy
cp -r usr/lib/lua/luci/controller/admin/* /usr/local/lib/lua/luci/controller/admin/
cp -r usr/lib/lua/luci/model/cbi/admin_autonomy/* /usr/local/lib/lua/luci/model/cbi/admin_autonomy/
cp -r usr/lib/lua/luci/view/admin_autonomy/* /usr/local/lib/lua/luci/view/admin_autonomy/

echo "Step 8: Installing UCI defaults..."
mkdir -p /etc/uci-defaults
cp -r etc/uci-defaults/* /etc/uci-defaults/
chmod +x /etc/uci-defaults/70-autonomy

echo "Step 9: Installing ACL configuration..."
mkdir -p /usr/share/rpcd/acl.d
cp -r usr/share/rpcd/acl.d/* /usr/share/rpcd/acl.d/

echo "Step 10: Setting up configuration..."
if [ ! -f /etc/config/autonomy ]; then
    cp /usr/local/etc/autonomy/autonomy.config /etc/config/autonomy
    echo "Created default configuration at /etc/config/autonomy"
fi

echo "Step 11: Running UCI defaults..."
/etc/uci-defaults/70-autonomy

echo "Step 12: Restarting services..."
/etc/init.d/rpcd restart
/etc/init.d/uhttpd restart

echo "Step 13: Enabling and starting autonomy service..."
/etc/init.d/autonomy enable
/etc/init.d/autonomy start

echo ""
echo "=========================================="
echo "Installation Completed Successfully!"
echo "=========================================="
echo ""
echo "Autonomy System is now installed and running!"
echo ""
echo "Access the web interface at:"
echo "  http://$(hostname -I | awk '{print $1}')/cgi-bin/luci/admin/autonomy"
echo ""
echo "System Status:"
echo "  Service: /etc/init.d/autonomy {start|stop|restart|status}"
echo "  Configuration: /etc/config/autonomy"
echo "  Logs: /var/log/autonomy/"
echo "  Binaries: /usr/local/bin/autonomyd, /usr/local/bin/autonomysysmgmt"
echo ""
echo "To verify installation, run:"
echo "  /usr/local/bin/autonomysysmgmt -check -dry-run"
echo ""

# Clean up
cd /
rm -rf "$TEMP_DIR"

echo "Installation script completed!"
echo "The autonomy system is ready for use."





