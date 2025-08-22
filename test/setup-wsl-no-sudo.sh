#!/bin/bash

# WSL OpenWrt Testing Environment Setup - No Sudo Required
# This script sets up a user-space development environment

set -e

echo "=== Setting up user-space OpenWrt testing environment ==="

# Create user-space directories
echo "Creating user-space directories..."
mkdir -p ~/autonomy/{bin,config,logs,tmp,etc/config}
mkdir -p ~/go/{bin,src,pkg}
mkdir -p ~/workspace

# Create user-space OpenWrt config
echo "Creating user-space OpenWrt configuration..."
cat > ~/autonomy/etc/config/system << 'EOF'
config system
    option hostname "openwrt-test"
    option timezone "UTC"
EOF

cat > ~/autonomy/etc/config/network << 'EOF'
config interface "loopback"
    option ifname "lo"
    option proto "static"
    option ipaddr "127.0.0.1"
EOF

cat > ~/autonomy/etc/config/mwan3 << 'EOF'
config globals "globals"
    option mmx_mask "0x3F00"
    option local_source "lan"

config interface "wan"
    option enabled "1"
    option family "ipv4"
    option track_method "ping"
    option track_ip "8.8.8.8"
    option reliability "1"
    option count "1"
    option timeout "2"
    option interval "5"
    option down "3"
    option up "3"
EOF

# Create user-space mock commands
echo "Creating user-space mock commands..."
cat > ~/autonomy/bin/ubus << 'EOF'
#!/bin/bash
echo "Mock ubus - $*"
# Return success for most commands
case "$1" in
    "version")
        echo "ubus version 2023-01-01"
        ;;
    "list")
        echo "autonomy"
        echo "system"
        echo "network"
        ;;
    "call")
        if [ "$2" = "autonomy" ] && [ "$3" = "status" ]; then
            echo '{"status": "running", "version": "1.0.0"}'
        else
            echo '{"result": "success"}'
        fi
        ;;
    *)
        echo '{"result": "success"}'
        ;;
esac
EOF

cat > ~/autonomy/bin/uci << 'EOF'
#!/bin/bash
echo "Mock uci - $*"
# Return success for most commands
case "$1" in
    "show")
        if [ "$2" = "system" ]; then
            echo "system.@system[0].hostname='openwrt-test'"
            echo "system.@system[0].timezone='UTC'"
        else
            echo "config.@config[0].name='test'"
        fi
        ;;
    "get")
        echo "test_value"
        ;;
    "set")
        echo "Configuration updated"
        ;;
    "commit")
        echo "Configuration committed"
        ;;
    *)
        echo "Configuration updated"
        ;;
esac
EOF

cat > ~/autonomy/bin/opkg << 'EOF'
#!/bin/bash
echo "Mock opkg - $*"
# Return success for most commands
case "$1" in
    "list-installed")
        echo "autonomy - 1.0.0-1"
        echo "luci-app-autonomy - 1.0.0-1"
        ;;
    "install")
        echo "Installing $2..."
        echo "Package $2 successfully installed"
        ;;
    "remove")
        echo "Removing $2..."
        echo "Package $2 successfully removed"
        ;;
    *)
        echo "Operation completed successfully"
        ;;
esac
EOF

# Make mock commands executable
chmod +x ~/autonomy/bin/ubus
chmod +x ~/autonomy/bin/uci
chmod +x ~/autonomy/bin/opkg

# Set up environment variables
echo "Setting up environment variables..."
cat >> ~/.bashrc << 'EOF'

# OpenWrt Testing Environment
export GOPATH=$HOME/go
export PATH=$PATH:$GOPATH/bin
export GO111MODULE=on
export AUTONOMY_CONFIG=$HOME/autonomy/etc/config
export AUTONOMY_LOGS=$HOME/autonomy/logs
export PATH=$HOME/autonomy/bin:$PATH

# Aliases for convenience
alias autonomy-test="cd ~/workspace && go test ./..."
alias autonomy-build="cd ~/workspace && go build ./cmd/autonomysysmgmt"
alias autonomy-run="cd ~/workspace && ./autonomysysmgmt --config $AUTONOMY_CONFIG/autonomy"
alias ubus-test="$HOME/autonomy/bin/ubus"
alias uci-test="$HOME/autonomy/bin/uci"
alias opkg-test="$HOME/autonomy/bin/opkg"
EOF

# Create a simple test script
cat > ~/autonomy/test-environment.sh << 'EOF'
#!/bin/bash

echo "=== OpenWrt Testing Environment Status ==="
echo "User: $(whoami)"
echo "Home: $HOME"
echo "Go Path: $GOPATH"
echo "Autonomy Config: $AUTONOMY_CONFIG"
echo "Autonomy Logs: $AUTONOMY_LOGS"

echo ""
echo "=== Testing Mock Commands ==="
echo "ubus version:"
~/autonomy/bin/ubus version

echo ""
echo "uci show system:"
~/autonomy/bin/uci show system

echo ""
echo "opkg list-installed:"
~/autonomy/bin/opkg list-installed

echo ""
echo "=== Configuration Files ==="
echo "System config:"
cat ~/autonomy/etc/config/system

echo ""
echo "Network config:"
cat ~/autonomy/etc/config/network

echo ""
echo "=== Environment Ready! ==="
echo "You can now test without sudo!"
echo "Use: source ~/.bashrc to reload environment"
EOF

chmod +x ~/autonomy/test-environment.sh

echo "=== Setup Complete! ==="
echo "User-space OpenWrt testing environment is ready."
echo ""
echo "To activate the environment:"
echo "1. source ~/.bashrc"
echo "2. ~/autonomy/test-environment.sh"
echo ""
echo "Available commands:"
echo "- ubus-test: Test ubus commands"
echo "- uci-test: Test uci commands"
echo "- opkg-test: Test opkg commands"
echo "- autonomy-test: Run Go tests"
echo "- autonomy-build: Build autonomy binary"
echo "- autonomy-run: Run autonomy system"
