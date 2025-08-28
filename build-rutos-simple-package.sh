#!/bin/bash
set -e

echo "=== Building Simple RUTOS Autonomy Package for Package Manager ==="

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
BUILD_DIR="/tmp/autonomy-simple"
PACKAGE_NAME="luci-app-autonomy"
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
mkdir -p "$AUTONOMY_PACKAGE_DIR/usr/lib/lua/luci/controller/admin"
mkdir -p "$AUTONOMY_PACKAGE_DIR/usr/lib/lua/luci/model/cbi/admin_autonomy"
mkdir -p "$AUTONOMY_PACKAGE_DIR/usr/lib/lua/luci/view/admin_autonomy"
mkdir -p "$AUTONOMY_PACKAGE_DIR/etc/uci-defaults"

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

return m
EOF

# Create LuCI view template
echo "Creating LuCI view template..."
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

# Create UCI defaults for automatic configuration
echo "Creating UCI defaults..."
cat > "$AUTONOMY_PACKAGE_DIR/etc/uci-defaults/70-luci-app-autonomy" << 'EOF'
#!/bin/sh

# Add autonomy to services menu
uci -q batch <<-EOF >/dev/null
	delete ucitrack.@autonomy[-1]
	add ucitrack autonomy
	set ucitrack.@autonomy[-1].init=autonomy
	commit ucitrack
EOF

exit 0
EOF

chmod +x "$AUTONOMY_PACKAGE_DIR/etc/uci-defaults/70-luci-app-autonomy"

# Create control file
echo "Creating control file..."
mkdir -p "$AUTONOMY_PACKAGE_DIR/CONTROL"

cat > "$AUTONOMY_PACKAGE_DIR/CONTROL/control" << EOF
Package: $PACKAGE_NAME
Version: $VERSION
Depends: luci-base, luci-compat, uci, mwan3, ubus
Architecture: $ARCHITECTURE
Installed-Size: 1024
Description: LuCI web interface for Autonomy
 Provides web interface for autonomous networking system
 with Starlink integration and cellular failover capabilities.
 Requires autonomy package to be installed separately.
EOF

# Create postinst script
echo "Creating postinst script..."
cat > "$AUTONOMY_PACKAGE_DIR/CONTROL/postinst" << 'EOF'
#!/bin/sh
# Post-installation script for luci-app-autonomy

# Run UCI defaults
/etc/uci-defaults/70-luci-app-autonomy

# Restart LuCI to pick up new menu items
/etc/init.d/rpcd restart
/etc/init.d/uhttpd restart

echo "LuCI Autonomy web interface installed successfully!"
echo "Web interface available at: http://your-router-ip/cgi-bin/luci/admin/autonomy"
EOF

chmod +x "$AUTONOMY_PACKAGE_DIR/CONTROL/postinst"

# Create prerm script
echo "Creating prerm script..."
cat > "$AUTONOMY_PACKAGE_DIR/CONTROL/prerm" << 'EOF'
#!/bin/sh
# Pre-removal script for luci-app-autonomy

# Remove from UCI tracking
uci -q delete ucitrack.@autonomy[-1] 2>/dev/null || true
uci commit ucitrack

# Restart LuCI
/etc/init.d/rpcd restart
/etc/init.d/uhttpd restart

echo "LuCI Autonomy web interface removed."
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
tar -czf "${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}.ipk" debian-binary control.tar.gz data.tar.gz

# Move IPK to project root
mv "${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}.ipk" "$PROJECT_ROOT/"

echo "IPK package created successfully!"
echo "Package: ${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}.ipk"

# Show package size
PACKAGE_SIZE=$(du -h "${PROJECT_ROOT}/${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}.ipk" | cut -f1)
echo "Package size: $PACKAGE_SIZE"

# Clean up
cd "$PROJECT_ROOT"
rm -rf "$BUILD_DIR"

echo "Build completed successfully!"





