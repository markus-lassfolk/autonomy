#!/bin/bash

# Build OpenWrt Package for opkg Installation
# This script builds the autonomy package for OpenWrt

set -e

echo "=== Building OpenWrt Package for opkg ==="

# Configuration
PACKAGE_NAME="autonomy"
PACKAGE_VERSION="1.0.0"
ARCHITECTURE="x86_64"
BUILD_DIR="/mnt/d/GitCursor/autonomy/build-openwrt"
PACKAGE_DIR="$BUILD_DIR/packages"

# Create build directories
mkdir -p $BUILD_DIR
mkdir -p $PACKAGE_DIR

echo "Building package: $PACKAGE_NAME-$PACKAGE_VERSION"

# Build the Go binary
echo "Building Go binary..."
cd /mnt/d/GitCursor/autonomy
go build -ldflags="-s -w" -o $BUILD_DIR/autonomysysmgmt ./cmd/autonomysysmgmt
go build -ldflags="-s -w" -o $BUILD_DIR/autonomyd ./cmd/autonomyd

# Create package structure
PACKAGE_ROOT="$BUILD_DIR/$PACKAGE_NAME"
mkdir -p $PACKAGE_ROOT/{usr/bin,etc/init.d,etc/config,usr/lib/lua/luci/controller,usr/lib/lua/luci/model/cbi/autonomy,usr/lib/lua/luci/view/autonomy,CONTROL}

# Copy binaries
cp $BUILD_DIR/autonomysysmgmt $PACKAGE_ROOT/usr/bin/
cp $BUILD_DIR/autonomyd $PACKAGE_ROOT/usr/bin/
chmod +x $PACKAGE_ROOT/usr/bin/*

# Create init script
cat > $PACKAGE_ROOT/etc/init.d/autonomy << 'EOF'
#!/bin/sh /etc/rc.common

START=95
STOP=15
USE_PROCD=1

start_service() {
    procd_open_instance
    procd_set_param command /usr/bin/autonomysysmgmt
    procd_set_param respawn
    procd_set_param stdout 1
    procd_set_param stderr 1
    procd_close_instance
}

stop_service() {
    killall autonomysysmgmt 2>/dev/null || true
}
EOF

chmod +x $PACKAGE_ROOT/etc/init.d/autonomy

# Create default configuration
cat > $PACKAGE_ROOT/etc/config/autonomy << 'EOF'
config autonomy 'main'
    option enabled '1'
    option log_level 'info'
    option config_path '/etc/config'

config autonomy 'starlink'
    option enabled '1'
    option api_key ''
    option health_check_interval '30'

config autonomy 'cellular'
    option enabled '1'
    option interface 'wwan0'
    option health_check_interval '15'

config autonomy 'wifi'
    option enabled '1'
    option interface 'wlan0'
    option health_check_interval '10'

config autonomy 'gps'
    option enabled '1'
    option device '/dev/ttyUSB0'
    option baud_rate '9600'
EOF

# Create LuCI controller
cat > $PACKAGE_ROOT/usr/lib/lua/luci/controller/autonomy.lua << 'EOF'
module("luci.controller.autonomy", package.seeall)

function index()
    entry({"admin", "system", "autonomy"}, cbi("autonomy/config"), _("Autonomy"), 60)
    entry({"admin", "system", "autonomy", "status"}, call("action_status")).leaf = true
    entry({"admin", "system", "autonomy", "start"}, call("action_start")).leaf = true
    entry({"admin", "system", "autonomy", "stop"}, call("action_stop")).leaf = true
    entry({"admin", "system", "autonomy", "restart"}, call("action_restart")).leaf = true
end

function action_status()
    local sys = require "luci.sys"
    local status = {
        running = sys.process.list()["autonomysysmgmt"] ~= nil,
        version = "1.0.0"
    }
    luci.http.prepare_content("application/json")
    luci.http.write_json(status)
end

function action_start()
    local sys = require "luci.sys"
    sys.init.start("autonomy")
    luci.http.prepare_content("application/json")
    luci.http.write_json({status = "started"})
end

function action_stop()
    local sys = require "luci.sys"
    sys.init.stop("autonomy")
    luci.http.prepare_content("application/json")
    luci.http.write_json({status = "stopped"})
end

function action_restart()
    local sys = require "luci.sys"
    sys.init.restart("autonomy")
    luci.http.prepare_content("application/json")
    luci.http.write_json({status = "restarted"})
end
EOF

# Create LuCI model
cat > $PACKAGE_ROOT/usr/lib/lua/luci/model/cbi/autonomy/config.lua << 'EOF'
local m, s

m = Map("autonomy", translate("Autonomy System Configuration"), translate("Configure the autonomous networking system"))

s = m:section(TypedSection, "main", translate("General Settings"))
s.anonymous = true

enabled = s:option(Flag, "enabled", translate("Enable Autonomy System"))
enabled.default = 1

log_level = s:option(ListValue, "log_level", translate("Log Level"))
log_level:value("debug", translate("Debug"))
log_level:value("info", translate("Info"))
log_level:value("warn", translate("Warning"))
log_level:value("error", translate("Error"))
log_level.default = "info"

s = m:section(TypedSection, "starlink", translate("Starlink Configuration"))
s.anonymous = true

starlink_enabled = s:option(Flag, "enabled", translate("Enable Starlink"))
starlink_enabled.default = 1

api_key = s:option(Value, "api_key", translate("API Key"))
api_key.password = true

s = m:section(TypedSection, "cellular", translate("Cellular Configuration"))
s.anonymous = true

cellular_enabled = s:option(Flag, "enabled", translate("Enable Cellular"))
cellular_enabled.default = 1

interface = s:option(Value, "interface", translate("Interface"))
interface.default = "wwan0"

return m
EOF

# Create LuCI view
cat > $PACKAGE_ROOT/usr/lib/lua/luci/view/autonomy/status.htm << 'EOF'
<%+header%>
<h2><%:Autonomy System Status%></h2>
<div class="cbi-section">
    <div class="cbi-value">
        <label class="cbi-value-title"><%:Status%></label>
        <div class="cbi-value-field">
            <span id="autonomy-status">Loading...</span>
        </div>
    </div>
    <div class="cbi-value">
        <label class="cbi-value-title"><%:Version%></label>
        <div class="cbi-value-field">
            <span id="autonomy-version">1.0.0</span>
        </div>
    </div>
</div>
<div class="cbi-section">
    <div class="cbi-value">
        <label class="cbi-value-title"><%:Actions%></label>
        <div class="cbi-value-field">
            <input type="button" class="btn" value="<%:Start%>" onclick="startAutonomy()" />
            <input type="button" class="btn" value="<%:Stop%>" onclick="stopAutonomy()" />
            <input type="button" class="btn" value="<%:Restart%>" onclick="restartAutonomy()" />
        </div>
    </div>
</div>

<script type="text/javascript">
function updateStatus() {
    XHR.get('<%=luci.dispatcher.build_url("admin", "system", "autonomy", "status")%>', null, function(x, data) {
        var status = JSON.parse(data);
        document.getElementById('autonomy-status').textContent = status.running ? 'Running' : 'Stopped';
    });
}

function startAutonomy() {
    XHR.get('<%=luci.dispatcher.build_url("admin", "system", "autonomy", "start")%>', null, function(x, data) {
        updateStatus();
    });
}

function stopAutonomy() {
    XHR.get('<%=luci.dispatcher.build_url("admin", "system", "autonomy", "stop")%>', null, function(x, data) {
        updateStatus();
    });
}

function restartAutonomy() {
    XHR.get('<%=luci.dispatcher.build_url("admin", "system", "autonomy", "restart")%>', null, function(x, data) {
        updateStatus();
    });
}

updateStatus();
setInterval(updateStatus, 5000);
</script>
<%+footer%>
EOF

# Create control file
cat > $PACKAGE_ROOT/CONTROL/control << EOF
Package: $PACKAGE_NAME
Version: $PACKAGE_VERSION
Depends: luci-base, mwan3, ubus, uci
Section: net
Architecture: $ARCHITECTURE
Installed-Size: $(du -s $PACKAGE_ROOT | cut -f1)
Description: Autonomous networking system for OpenWrt
 Provides intelligent network failover management with Starlink integration,
 cellular failover, GPS tracking, and advanced monitoring capabilities.
EOF

# Create postinst script
cat > $PACKAGE_ROOT/CONTROL/postinst << 'EOF'
#!/bin/sh
[ -n "${IPKG_INSTROOT}" ] || {
    ( . /etc/uci-defaults/luci-autonomy ) && rm -f /etc/uci-defaults/luci-autonomy
    exit 0
}
EOF

chmod +x $PACKAGE_ROOT/CONTROL/postinst

# Create uci-defaults
mkdir -p $PACKAGE_ROOT/etc/uci-defaults
cat > $PACKAGE_ROOT/etc/uci-defaults/luci-autonomy << 'EOF'
#!/bin/sh

uci -q batch <<-EOF >/dev/null
	delete ucitrack.@autonomy[-1]
	add ucitrack autonomy
	set ucitrack.@autonomy[-1].init=autonomy
	commit ucitrack
EOF

rm -f /tmp/luci-indexcache
exit 0
EOF

chmod +x $PACKAGE_ROOT/etc/uci-defaults/luci-autonomy

# Create the IPK package
echo "Creating IPK package..."
cd $BUILD_DIR

# Create data.tar.gz
tar -czf data.tar.gz -C $PACKAGE_ROOT .

# Create control.tar.gz
tar -czf control.tar.gz -C $PACKAGE_ROOT/CONTROL .

# Create debian-binary
echo "2.0" > debian-binary

# Create the IPK
ar r $PACKAGE_DIR/${PACKAGE_NAME}_${PACKAGE_VERSION}_${ARCHITECTURE}.ipk debian-binary data.tar.gz control.tar.gz

# Clean up
rm -f data.tar.gz control.tar.gz debian-binary

echo "=== Package built successfully! ==="
echo "Package: $PACKAGE_DIR/${PACKAGE_NAME}_${PACKAGE_VERSION}_${ARCHITECTURE}.ipk"
echo ""
echo "To install:"
echo "opkg install $PACKAGE_DIR/${PACKAGE_NAME}_${PACKAGE_VERSION}_${ARCHITECTURE}.ipk"
echo ""
echo "Package contents:"
echo "- Binary: /usr/bin/autonomysysmgmt"
echo "- Binary: /usr/bin/autonomyd"
echo "- Init script: /etc/init.d/autonomy"
echo "- Config: /etc/config/autonomy"
echo "- LuCI integration: /usr/lib/lua/luci/controller/autonomy.lua"
echo "- LuCI model: /usr/lib/lua/luci/model/cbi/autonomy/config.lua"
echo "- LuCI view: /usr/lib/lua/luci/view/autonomy/status.htm"
