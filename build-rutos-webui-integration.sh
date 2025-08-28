#!/bin/bash
set -e

echo "=== Building RUTOS Autonomy Package with Web UI Integration ==="

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
BUILD_DIR="/tmp/autonomy-webui"
PACKAGE_NAME="autonomy"
VERSION="1.0.0"
ARCHITECTURE="arm_cortex-a7_neon-vfpv4"

echo "Building in: $BUILD_DIR"
echo "Architecture: $ARCHITECTURE"
echo "Version: $VERSION"

# Clean and create build directory
echo "Cleaning build directory..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Create package structure
echo "Creating package structure..."
AUTONOMY_PACKAGE_DIR="$BUILD_DIR/$PACKAGE_NAME"
mkdir -p "$AUTONOMY_PACKAGE_DIR/usr/local/bin"
mkdir -p "$AUTONOMY_PACKAGE_DIR/usr/local/etc/autonomy"
mkdir -p "$AUTONOMY_PACKAGE_DIR/usr/local/share/autonomy-ui"
mkdir -p "$AUTONOMY_PACKAGE_DIR/etc/init.d"
mkdir -p "$AUTONOMY_PACKAGE_DIR/etc/config"
mkdir -p "$AUTONOMY_PACKAGE_DIR/etc/uci-defaults"
mkdir -p "$AUTONOMY_PACKAGE_DIR/usr/lib/lua/luci/controller/admin"
mkdir -p "$AUTONOMY_PACKAGE_DIR/usr/lib/lua/luci/model/cbi/admin_autonomy"
mkdir -p "$AUTONOMY_PACKAGE_DIR/usr/lib/lua/luci/view/admin_autonomy"
mkdir -p "$AUTONOMY_PACKAGE_DIR/usr/share/rpcd/acl.d"

# Copy real ARM binaries
echo "Copying real ARM binaries..."
if [ -f "$PROJECT_ROOT/bin/autonomyd-arm" ]; then
    cp "$PROJECT_ROOT/bin/autonomyd-arm" "$AUTONOMY_PACKAGE_DIR/usr/local/bin/autonomyd"
    echo "Copied autonomyd ARM binary (19MB)"
else
    echo "Error: autonomyd-arm binary not found!"
    exit 1
fi

if [ -f "$PROJECT_ROOT/bin/autonomysysmgmt-arm" ]; then
    cp "$PROJECT_ROOT/bin/autonomysysmgmt-arm" "$AUTONOMY_PACKAGE_DIR/usr/local/bin/autonomysysmgmt"
    echo "Copied autonomysysmgmt ARM binary (13MB)"
else
    echo "Error: autonomysysmgmt-arm binary not found!"
    exit 1
fi

chmod +x "$AUTONOMY_PACKAGE_DIR/usr/local/bin/autonomyd"
chmod +x "$AUTONOMY_PACKAGE_DIR/usr/local/bin/autonomysysmgmt"

# Copy package files
echo "Copying package files..."
if [ -d "$PROJECT_ROOT/package/autonomy/files" ]; then
    cp -r "$PROJECT_ROOT/package/autonomy/files"/* "$AUTONOMY_PACKAGE_DIR/usr/local/etc/autonomy/"
    echo "Copied package files"
else
    echo "Warning: Package files directory not found"
fi

# Copy web UI files
echo "Copying web UI files..."
if [ -d "$PROJECT_ROOT/vuci-app-autonomy-ui/src" ]; then
    cp -r "$PROJECT_ROOT/vuci-app-autonomy-ui/src"/* "$AUTONOMY_PACKAGE_DIR/usr/local/share/autonomy-ui/"
    echo "Copied web UI files"
else
    echo "Warning: Web UI directory not found"
fi

# Create LuCI controller for web interface
echo "Creating LuCI controller..."
cat > "$AUTONOMY_PACKAGE_DIR/usr/lib/lua/luci/controller/admin/autonomy.lua" << 'EOF'
module("luci.controller.admin.autonomy", package.seeall)

function index()
    if not nixio.fs.access("/etc/config/autonomy") then
        return
    end

    local page = entry({"admin", "autonomy"}, alias("admin", "autonomy", "overview"), _("Autonomy"), 60)
    page.dependent = true
    page.acl_depends = { "luci-app-autonomy" }

    entry({"admin", "autonomy", "overview"}, template("admin_autonomy/overview"), _("Overview"), 1)
    entry({"admin", "autonomy", "status"}, call("action_status")).leaf = true
    entry({"admin", "autonomy", "config"}, cbi("admin_autonomy/config"), _("Configuration"), 2)
    entry({"admin", "autonomy", "logs"}, template("admin_autonomy/logs"), _("Logs"), 3)
    entry({"admin", "autonomy", "starlink"}, template("admin_autonomy/starlink"), _("Starlink"), 4)
end

function action_status()
    local sys = require "luci.sys"
    local status = {
        running = false,
        starlink = {},
        system = {}
    }

    -- Check if autonomy daemon is running
    status.running = sys.process.list()["autonomyd"] ~= nil

    -- Get Starlink status (if available)
    local starlink_status = sys.exec("/usr/local/bin/autonomysysmgmt -check -dry-run 2>/dev/null | grep -E 'latency_ms|obstruction_pct|uptime_hours' | tail -3")
    if starlink_status and starlink_status ~= "" then
        status.starlink = starlink_status
    end

    -- Get system status
    local system_status = sys.exec("/usr/local/bin/autonomysysmgmt -check -dry-run 2>/dev/null | grep -E 'issues_found|duration' | tail -2")
    if system_status and system_status ~= "" then
        status.system = system_status
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(status)
end
EOF

# Create LuCI model for configuration
echo "Creating LuCI model..."
cat > "$AUTONOMY_PACKAGE_DIR/usr/lib/lua/luci/model/cbi/admin_autonomy/config.lua" << 'EOF'
local m, s, o

m = Map("autonomy", translate("Autonomy Configuration"), translate("Configure autonomous networking system"))

s = m:section(TypedSection, "main", translate("Main Configuration"))
s.anonymous = true

o = s:option(Flag, "enable", translate("Enable Autonomy"))
o.default = "1"
o.rmempty = false

o = s:option(Flag, "use_mwan3", translate("Use mwan3 for failover"))
o.default = "1"
o.rmempty = false

o = s:option(ListValue, "log_level", translate("Log Level"))
o:value("debug", translate("Debug"))
o:value("info", translate("Info"))
o:value("warn", translate("Warning"))
o:value("error", translate("Error"))
o:value("trace", translate("Trace"))
o.default = "info"
o.rmempty = false

o = s:option(Value, "poll_interval_ms", translate("Poll Interval (ms)"))
o.datatype = "uinteger"
o.default = "2000"
o.rmempty = false

o = s:option(Value, "min_uptime_s", translate("Minimum Uptime (seconds)"))
o.datatype = "uinteger"
o.default = "30"
o.rmempty = false

o = s:option(Value, "cooldown_s", translate("Cooldown Period (seconds)"))
o.datatype = "uinteger"
o.default = "30"
o.rmempty = false

-- Starlink section
s = m:section(TypedSection, "starlink", translate("Starlink Configuration"))
s.anonymous = true

o = s:option(Value, "host", translate("Starlink Host"))
o.default = "192.168.100.1"
o.rmempty = false

o = s:option(Value, "port", translate("Starlink Port"))
o.datatype = "port"
o.default = "9200"
o.rmempty = false

o = s:option(Value, "timeout_s", translate("Timeout (seconds)"))
o.datatype = "uinteger"
o.default = "5"
o.rmempty = false

o = s:option(Value, "health_threshold", translate("Health Threshold (%)"))
o.datatype = "uinteger"
o.default = "80"
o.rmempty = false

return m
EOF

# Create LuCI view templates
echo "Creating LuCI view templates..."
mkdir -p "$AUTONOMY_PACKAGE_DIR/usr/lib/lua/luci/view/admin_autonomy"

cat > "$AUTONOMY_PACKAGE_DIR/usr/lib/lua/luci/view/admin_autonomy/overview.htm" << 'EOF'
<%+header%>

<h2><%:Autonomy Overview%></h2>

<div class="cbi-section">
    <div class="cbi-section-node">
        <h3><%:System Status%></h3>
        <div id="status-container">
            <p><%:Loading status...%></p>
        </div>
    </div>
    
    <div class="cbi-section-node">
        <h3><%:Starlink Status%></h3>
        <div id="starlink-container">
            <p><%:Loading Starlink data...%></p>
        </div>
    </div>
    
    <div class="cbi-section-node">
        <h3><%:System Health%></h3>
        <div id="health-container">
            <p><%:Loading health data...%></p>
        </div>
    </div>
</div>

<script type="text/javascript">
function updateStatus() {
    XHR.get('<%=luci.dispatcher.build_url("admin", "autonomy", "status")%>', null, function(x, data) {
        var status = JSON.parse(data);
        
        var statusHtml = '<p><strong>Daemon Status:</strong> ';
        statusHtml += status.running ? '<span style="color: green;">Running</span>' : '<span style="color: red;">Stopped</span>';
        statusHtml += '</p>';
        
        document.getElementById('status-container').innerHTML = statusHtml;
        
        if (status.starlink) {
            document.getElementById('starlink-container').innerHTML = '<pre>' + status.starlink + '</pre>';
        }
        
        if (status.system) {
            document.getElementById('health-container').innerHTML = '<pre>' + status.system + '</pre>';
        }
    });
}

// Update status every 10 seconds
updateStatus();
setInterval(updateStatus, 10000);
</script>

<%+footer%>
EOF

cat > "$AUTONOMY_PACKAGE_DIR/usr/lib/lua/luci/view/admin_autonomy/logs.htm" << 'EOF'
<%+header%>

<h2><%:Autonomy Logs%></h2>

<div class="cbi-section">
    <div class="cbi-section-node">
        <h3><%:Recent Logs%></h3>
        <div id="logs-container">
            <p><%:Loading logs...%></p>
        </div>
        <button class="btn" onclick="refreshLogs()"><%:Refresh%></button>
    </div>
</div>

<script type="text/javascript">
function refreshLogs() {
    XHR.get('<%=luci.dispatcher.build_url("admin", "autonomy", "logs")%>', null, function(x, data) {
        document.getElementById('logs-container').innerHTML = '<pre>' + data + '</pre>';
    });
}

refreshLogs();
</script>

<%+footer%>
EOF

cat > "$AUTONOMY_PACKAGE_DIR/usr/lib/lua/luci/view/admin_autonomy/starlink.htm" << 'EOF'
<%+header%>

<h2><%:Starlink Status%></h2>

<div class="cbi-section">
    <div class="cbi-section-node">
        <h3><%:Connection Status%></h3>
        <div id="starlink-status">
            <p><%:Loading Starlink status...%></p>
        </div>
        <button class="btn" onclick="refreshStarlink()"><%:Refresh%></button>
    </div>
</div>

<script type="text/javascript">
function refreshStarlink() {
    XHR.get('<%=luci.dispatcher.build_url("admin", "autonomy", "status")%>', null, function(x, data) {
        var status = JSON.parse(data);
        if (status.starlink) {
            document.getElementById('starlink-status').innerHTML = '<pre>' + status.starlink + '</pre>';
        }
    });
}

refreshStarlink();
</script>

<%+footer%>
EOF

# Create UCI defaults for automatic configuration
echo "Creating UCI defaults..."
cat > "$AUTONOMY_PACKAGE_DIR/etc/uci-defaults/70-autonomy" << 'EOF'
#!/bin/sh

# Add autonomy to services menu
uci -q batch <<-EOF >/dev/null
	delete ucitrack.@autonomy[-1]
	add ucitrack autonomy
	set ucitrack.@autonomy[-1].init=autonomy
	commit ucitrack
EOF

# Enable autonomy service (will be created during installation)
# /etc/init.d/autonomy enable

exit 0
EOF

chmod +x "$AUTONOMY_PACKAGE_DIR/etc/uci-defaults/70-autonomy"

# Create ACL for LuCI access
echo "Creating ACL configuration..."
cat > "$AUTONOMY_PACKAGE_DIR/usr/share/rpcd/acl.d/luci-app-autonomy.json" << 'EOF'
{
    "luci-app-autonomy": {
        "description": "Grant access to Autonomy configuration",
        "read": {
            "ubus": {
                "service": ["list", "get"],
                "system": ["info"]
            },
            "uci": ["autonomy"]
        },
        "write": {
            "ubus": {
                "service": ["add", "delete", "restart"],
                "system": ["reboot"]
            },
            "uci": ["autonomy"]
        }
    }
}
EOF

# Create control file
echo "Creating control file..."
mkdir -p "$AUTONOMY_PACKAGE_DIR/CONTROL"

cat > "$AUTONOMY_PACKAGE_DIR/CONTROL/control" << EOF
Package: $PACKAGE_NAME
Version: $VERSION
Depends: uci, mwan3, ubus, luci-base, luci-compat
Architecture: $ARCHITECTURE
Installed-Size: 32768
Description: Autonomous networking system for RUTOS devices
 Provides intelligent network failover, GPS tracking, and monitoring
 with Starlink integration and cellular failover capabilities.
 Includes web UI integration with LuCI and comprehensive configuration management.
 Built with real ARM binaries for optimal performance.
EOF

# Create postinst script
echo "Creating postinst script..."
cat > "$AUTONOMY_PACKAGE_DIR/CONTROL/postinst" << 'EOF'
#!/bin/sh
# Post-installation script for autonomy package

# Set executable permissions
chmod +x /usr/local/bin/autonomyd
chmod +x /usr/local/bin/autonomysysmgmt

# Create necessary directories
mkdir -p /etc/autonomy
mkdir -p /var/log/autonomy
mkdir -p /var/lib/autonomy

# Set up default configuration if not exists
if [ ! -f /etc/config/autonomy ]; then
    cp /usr/local/etc/autonomy/autonomy.config /etc/config/autonomy
fi

# Run UCI defaults
/etc/uci-defaults/70-autonomy

# Restart LuCI to pick up new menu items
/etc/init.d/rpcd restart
/etc/init.d/uhttpd restart

# Enable and start service
/etc/init.d/autonomy enable
/etc/init.d/autonomy start

echo "Autonomy package installed successfully!"
echo "Web interface available at: http://your-router-ip/cgi-bin/luci/admin/autonomy"
echo "Configuration available at: /usr/local/etc/autonomy/"
EOF

chmod +x "$AUTONOMY_PACKAGE_DIR/CONTROL/postinst"

# Create prerm script
echo "Creating prerm script..."
cat > "$AUTONOMY_PACKAGE_DIR/CONTROL/prerm" << 'EOF'
#!/bin/sh
# Pre-removal script for autonomy package

# Stop and disable service
/etc/init.d/autonomy stop
/etc/init.d/autonomy disable

# Remove from UCI tracking
uci -q delete ucitrack.@autonomy[-1] 2>/dev/null || true
uci commit ucitrack

# Restart LuCI
/etc/init.d/rpcd restart
/etc/init.d/uhttpd restart

echo "Autonomy service stopped and disabled."
EOF

chmod +x "$AUTONOMY_PACKAGE_DIR/CONTROL/prerm"

# Create IPK package in RUTOS format
echo "Creating IPK package..."
cd "$AUTONOMY_PACKAGE_DIR"

# Create data.tar.gz
echo "Creating data.tar.gz..."
tar -czf data.tar.gz usr/ etc/

# Create control.tar.gz
echo "Creating control.tar.gz..."
tar -czf control.tar.gz CONTROL/

# Create debian-binary
echo "Creating debian-binary..."
echo "2.0" > debian-binary

# Create the final IPK as a gzipped tar archive (RUTOS format)
echo "Creating final IPK file..."
tar -czf "${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}_webui.ipk" debian-binary control.tar.gz data.tar.gz

# Move IPK to project root
mv "${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}_webui.ipk" "$PROJECT_ROOT/"

echo "IPK package created successfully!"
echo "Package: ${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}_webui.ipk"

# Show package size
PACKAGE_SIZE=$(du -h "${PROJECT_ROOT}/${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}_webui.ipk" | cut -f1)
echo "Package size: $PACKAGE_SIZE"

# Clean up
cd "$PROJECT_ROOT"
rm -rf "$BUILD_DIR"

echo "Build completed successfully!"
