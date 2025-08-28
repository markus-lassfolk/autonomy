#!/bin/bash
set -e

echo "=== Building VUCI Example App ==="

# Configuration
SDK_DIR="/home/markusla/rutos-sdk"
VUCI_DIR="$SDK_DIR/package/feeds/vuci"
PKG_NAME="vuci-app-example-simple"

echo "SDK directory: $SDK_DIR"
echo "VUCI directory: $VUCI_DIR"

# Check if SDK exists
if [ ! -d "$SDK_DIR" ]; then
    echo "ERROR: SDK not found at $SDK_DIR"
    exit 1
fi

# Create package directory
PKG_DIR="$VUCI_DIR/$PKG_NAME"
echo "Creating package at: $PKG_DIR"

# Clean and create package directory
rm -rf "$PKG_DIR"
mkdir -p "$PKG_DIR/files/usr/libexec/rpcd"
mkdir -p "$PKG_DIR/files/www"
mkdir -p "$PKG_DIR/files/etc/init.d"

# Create Makefile using the SDK pattern
cat > "$PKG_DIR/Makefile" << 'EOF'
include $(TOPDIR)/rules.mk

APP_TITLE:=VuCI UI Support for Example Simple

include ../app.mk

# call BuildPackage - OpenWrt buildroot signature
EOF

# Create Lua API
cat > "$PKG_DIR/files/usr/libexec/rpcd/example.lua" << 'EOF'
local ubus = require "ubus"

local function example_status()
    return {
        status = "running",
        timestamp = os.time(),
        version = "1.0.0",
        message = "Hello from example package!"
    }
end

local function example_info()
    return {
        name = "example-simple",
        description = "A simple example VUCI application",
        author = "Autonomy Team",
        version = "1.0.0"
    }
end

local ubus_conn = ubus.connect()
if not ubus_conn then
    error("Failed to connect to ubus")
end

ubus_conn:add({
    example = {
        status = example_status,
        info = example_info
    }
})

ubus_conn:listen()
EOF

# Create HTML UI
cat > "$PKG_DIR/files/www/index.html" << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <title>Example Simple App</title>
    <meta charset="utf-8">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .container { max-width: 800px; margin: 0 auto; }
        .box { background: #f5f5f5; padding: 20px; border-radius: 5px; margin: 20px 0; }
        button { background: #007cba; color: white; border: none; padding: 10px 20px; border-radius: 3px; cursor: pointer; }
        pre { background: #f8f8f8; padding: 10px; border-radius: 3px; overflow-x: auto; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Example Simple App</h1>
        <p>This is a simple example VUCI application to test package deployment.</p>
        
        <div class="box">
            <h2>Status</h2>
            <button onclick="getStatus()">Get Status</button>
            <pre id="status-result">Click button to get status...</pre>
        </div>
        
        <div class="box">
            <h2>Info</h2>
            <button onclick="getInfo()">Get Info</button>
            <pre id="info-result">Click button to get info...</pre>
        </div>
    </div>

    <script>
        function callUbus(method, params) {
            return fetch('/ubus', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    method: 'call',
                    params: ['00000000000000000000000000000000', 'example', method, params || {}]
                })
            })
            .then(response => response.json())
            .then(data => {
                if (data[0] === 0) return data[1];
                throw new Error('Ubus call failed: ' + data[1]);
            });
        }

        function getStatus() {
            document.getElementById('status-result').textContent = 'Loading...';
            callUbus('status')
                .then(result => {
                    document.getElementById('status-result').textContent = JSON.stringify(result, null, 2);
                })
                .catch(error => {
                    document.getElementById('status-result').textContent = 'Error: ' + error.message;
                });
        }

        function getInfo() {
            document.getElementById('info-result').textContent = 'Loading...';
            callUbus('info')
                .then(result => {
                    document.getElementById('info-result').textContent = JSON.stringify(result, null, 2);
                })
                .catch(error => {
                    document.getElementById('info-result').textContent = 'Error: ' + error.message;
                });
        }

        window.onload = function() {
            getStatus();
            getInfo();
        };
    </script>
</body>
</html>
EOF

# Create init script
cat > "$PKG_DIR/files/etc/init.d/example" << 'EOF'
#!/bin/sh /etc/rc.common

START=99
STOP=10

start() {
    echo "Starting example service..."
    /etc/init.d/rpcd restart
}

stop() {
    echo "Stopping example service..."
}

reload() {
    echo "Reloading example service..."
    /etc/init.d/rpcd restart
}
EOF

# Make init script executable
chmod +x "$PKG_DIR/files/etc/init.d/example"

echo "Package created successfully!"

# Change to SDK directory
cd "$SDK_DIR"

# Try to build the package
echo "Building package..."

# First, let's try to build just our package
make package/vuci-app-example-simple/clean
make package/vuci-app-example-simple/compile V=s

# Find built package
echo "Looking for built package..."
PACKAGE=$(find "$SDK_DIR/bin" -name "vuci-app-example-simple_*.ipk" | head -1)

if [ -n "$PACKAGE" ]; then
    echo "Package built successfully!"
    echo "Package: $PACKAGE"
    
    # Copy to current directory
    cp "$PACKAGE" ./
    echo "Package copied to: $(pwd)/$(basename "$PACKAGE")"
    
    echo ""
    echo "=== DEPLOYMENT INSTRUCTIONS ==="
    echo "1. Copy package to device:"
    echo "   scp $(basename "$PACKAGE") root@192.168.80.1:/tmp/"
    echo ""
    echo "2. Install package:"
    echo "   ssh root@192.168.80.1 'opkg install /tmp/$(basename "$PACKAGE")'"
    echo ""
    echo "3. Restart services:"
    echo "   ssh root@192.168.80.1 '/etc/init.d/rpcd restart && /etc/init.d/uhttpd restart'"
    echo ""
    echo "4. Access the UI at:"
    echo "   http://192.168.80.1/example/"
    echo ""
    echo "5. Test the API:"
    echo "   ssh root@192.168.80.1 'ubus call example status'"
    echo "   ssh root@192.168.80.1 'ubus call example info'"
else
    echo "ERROR: Failed to find built package"
    echo "Let's try a different approach..."
    
    # Try building with menuconfig
    echo "Trying to build with menuconfig..."
    make menuconfig
    
    # Try building again
    make package/vuci-app-example-simple/compile V=s
    
    PACKAGE=$(find "$SDK_DIR/bin" -name "vuci-app-example-simple_*.ipk" | head -1)
    if [ -n "$PACKAGE" ]; then
        echo "Package built successfully on second attempt!"
        echo "Package: $PACKAGE"
        cp "$PACKAGE" ./
        echo "Package copied to: $(pwd)/$(basename "$PACKAGE")"
    else
        echo "ERROR: Still failed to build package"
        exit 1
    fi
fi

echo "Build completed successfully!"




