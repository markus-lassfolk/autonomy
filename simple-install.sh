#!/bin/sh

echo "Creating overlay directories..."
mkdir -p /overlay/root/upper/usr/libexec/rpcd
mkdir -p /overlay/root/upper/usr/share/vuci/menu.d
mkdir -p /overlay/root/upper/www/views/example

echo "Creating API file..."
cat > /overlay/root/upper/usr/libexec/rpcd/example.lua << 'EOF'
local ubus = require "ubus"
local ubus_conn = ubus.connect()
if not ubus_conn then error("Failed to connect to ubus") end

ubus_conn:add({
    example = {
        status = function() return {status="running", message="Hello from example!"} end,
        info = function() return {name="example", version="1.0.0"} end
    }
})

ubus_conn:listen()
EOF

echo "Creating menu config..."
cat > /overlay/root/upper/usr/share/vuci/menu.d/example.json << 'EOF'
{"name":"Example","path":"example","order":100}
EOF

echo "Creating UI file..."
cat > /overlay/root/upper/www/views/example/index.html << 'EOF'
<!DOCTYPE html>
<html><head><title>Example</title></head>
<body><h1>Example App Works!</h1><p>This is a working VUCI application.</p></body>
</html>
EOF

echo "Creating symlinks..."
ln -sf /usr/local/usr/libexec/rpcd/example.lua /usr/libexec/rpcd/example.lua
ln -sf /usr/local/usr/share/vuci/menu.d/example.json /usr/share/vuci/menu.d/example.json
ln -sf /usr/local/www/views/example /www/views/example

echo "Restarting services..."
/etc/init.d/rpcd restart
/etc/init.d/uhttpd restart

echo "Testing..."
ubus list | grep example
echo "Installation complete!"




