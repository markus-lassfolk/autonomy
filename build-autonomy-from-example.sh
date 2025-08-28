#!/bin/bash
set -e

echo "=== Building Autonomy Packages from Example Foundation ==="

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="/tmp/autonomy-from-example-$$"
VERSION="1.0"
ARCHITECTURE="arm_cortex-a7_neon-vfpv4"

echo "Project root: $SCRIPT_DIR"
echo "Build directory: $BUILD_DIR"

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Create API package structure
echo "Creating autonomy API package from example foundation..."
mkdir -p "api-package/usr/lib/lua/api/services"

# Create the config file with autonomy-specific functionality
cat > "api-package/usr/lib/lua/api/services/config_autonomy.lua" << 'EOF'
uaQ
local ConfigService = require("api/ConfigService")

local Autonomy = ConfigService:new({
	-- delete = false,          -- Disable deletion of UCI sections
	-- create = false,          -- Disable creation of UCI sections
	-- general_section = "main",-- General UCI section name
	-- anonymous = true,        -- Create UCI anonymous sections
	-- increment_name = true,   -- Create UCI sections with numeric incremental names
})

local ConfigAutonomy = Autonomy:section(
	"autonomy", -- UCI config name
	"autonomy"  -- UCI section type
)
ConfigAutonomy:make_primary()
ConfigAutonomy.default_options.id.maxlength = 32

function ConfigAutonomy:create_defaults(sid)
	-- Default values to be added with every creation
	return {
		enabled = "1",
		check_interval = "30",
		starlink_enabled = "1",
		cellular_enabled = "1",
		gps_enabled = "1",
		log_level = "info",
		failover_threshold = "3"
	}
end

	local opt_enabled = ConfigAutonomy:option("enabled")
		opt_enabled.maxlength = 1
		function opt_enabled:validate(value)
			return self.dt:is_bool(value)
		end

	local opt_check_interval = ConfigAutonomy:option("check_interval")
		opt_check_interval.maxlength = 3
		function opt_check_interval:validate(value)
			local num = tonumber(value)
			return num and num >= 10 and num <= 300
		end

	local opt_starlink_enabled = ConfigAutonomy:option("starlink_enabled")
		opt_starlink_enabled.maxlength = 1
		function opt_starlink_enabled:validate(value)
			return self.dt:is_bool(value)
		end

	local opt_cellular_enabled = ConfigAutonomy:option("cellular_enabled")
		opt_cellular_enabled.maxlength = 1
		function opt_cellular_enabled:validate(value)
			return self.dt:is_bool(value)
		end

	local opt_gps_enabled = ConfigAutonomy:option("gps_enabled")
		opt_gps_enabled.maxlength = 1
		function opt_gps_enabled:validate(value)
			return self.dt:is_bool(value)
		end

	local opt_log_level = ConfigAutonomy:option("log_level")
		opt_log_level.maxlength = 10
		function opt_log_level:validate(value)
			return self.dt:check_array(value, { "debug", "info", "warn", "error" })
		end

	local opt_failover_threshold = ConfigAutonomy:option("failover_threshold")
		opt_failover_threshold.maxlength = 2
		function opt_failover_threshold:validate(value)
			local num = tonumber(value)
			return num and num >= 1 and num <= 10
		end

return Autonomy
EOF

# Create the function file with autonomy-specific functionality
cat > "api-package/usr/lib/lua/api/services/function_autonomy.lua" << 'EOF'
uaQ
local FunctionService = require("api/FunctionService")

local Autonomy = FunctionService:new()

-- GET /api/autonomy_f/status
function Autonomy:GET_TYPE_status()
	local status = {
		running = false,
		uptime = '',
		last_check = '',
		issues = {}
	}
	
	-- Check if autonomy service is running
	local handle = io.popen("ps | grep autonomysysmgmt | grep -v grep")
	if handle then
		local result = handle:read("*a")
		handle:close()
		status.running = result and result ~= ""
	end
	
	-- Get uptime if running
	if status.running then
		handle = io.popen("ps -o etime= -p $(pgrep autonomysysmgmt) 2>/dev/null")
		if handle then
			local result = handle:read("*a")
			handle:close()
			status.uptime = result and result:gsub("^%s*(.-)%s*$", "%1") or "Unknown"
		end
	end
	
	-- Get last check time
	handle = io.popen("tail -1 /var/log/autonomy/autonomy.log 2>/dev/null | cut -d' ' -f1-3")
	if handle then
		local result = handle:read("*a")
		handle:close()
		status.last_check = result and result:gsub("^%s*(.-)%s*$", "%1") or "Never"
	end
	
	-- Check for issues
	if not status.running then
		table.insert(status.issues, "Service not running")
	end
	
	return self:ResponseOK(status)
end

-- GET /api/autonomy_f/logs
function Autonomy:GET_TYPE_logs()
	local logs = {}
	
	-- Read recent logs
	local handle = io.popen("tail -50 /var/log/autonomy/autonomy.log 2>/dev/null")
	if handle then
		for line in handle:lines() do
			table.insert(logs, line)
		end
		handle:close()
	end
	
	if #logs == 0 then
		table.insert(logs, "No logs available")
	end
	
	return self:ResponseOK(logs)
end

-- GET /api/autonomy_f/overview
function Autonomy:GET_TYPE_overview()
	local overview = {
		service_status = "Unknown",
		starlink_status = "Unknown",
		cellular_status = "Unknown",
		gps_status = "Unknown",
		last_check = "Never",
		issues = 0
	}
	
	-- Check service status
	local handle = io.popen("ps | grep autonomysysmgmt | grep -v grep")
	if handle then
		local result = handle:read("*a")
		handle:close()
		overview.service_status = result and result ~= "" and "Running" or "Stopped"
	end
	
	-- Check Starlink status
	handle = io.popen("ubus call network.interface.wan status 2>/dev/null | grep -q 'up' && echo 'Connected' || echo 'Disconnected'")
	if handle then
		local result = handle:read("*a")
		handle:close()
		overview.starlink_status = result and result:gsub("^%s*(.-)%s*$", "%1") or "Unknown"
	end
	
	-- Check cellular status
	handle = io.popen("ubus call network.interface.wwan status 2>/dev/null | grep -q 'up' && echo 'Connected' || echo 'Disconnected'")
	if handle then
		local result = handle:read("*a")
		handle:close()
		overview.cellular_status = result and result:gsub("^%s*(.-)%s*$", "%1") or "Unknown"
	end
	
	-- Check GPS status
	handle = io.popen("ubus call gpsd status 2>/dev/null | grep -q 'connected' && echo 'Connected' || echo 'Disconnected'")
	if handle then
		local result = handle:read("*a")
		handle:close()
		overview.gps_status = result and result:gsub("^%s*(.-)%s*$", "%1") or "Unknown"
	end
	
	-- Count issues
	if overview.service_status == "Stopped" then
		overview.issues = overview.issues + 1
	end
	if overview.starlink_status == "Disconnected" then
		overview.issues = overview.issues + 1
	end
	if overview.cellular_status == "Disconnected" then
		overview.issues = overview.issues + 1
	end
	
	return self:ResponseOK(overview)
end

-- POST /api/autonomy_f/actions/restart
function Autonomy:RestartAction()
	local result = os.execute("/etc/init.d/autonomy restart")
	
	if result then
		return self:ResponseOK({
			result = "Autonomy service restarted successfully",
			message = "Service restart command executed"
		})
	else
		return self:ResponseError("Failed to restart autonomy service")
	end
end

-- Register the restart action
local restart_action = Autonomy:action("restart", Autonomy.RestartAction)

	local name = restart_action:option("name")
		name.require = false
		name.maxlength = 256

return Autonomy
EOF

# Create API package control file
cat > "api-package/control" << EOF
Package: vuci-app-autonomy-api
Version: $VERSION
Depends: uci, ubus, api-core
Architecture: $ARCHITECTURE
Installed-Size: 1024
Description: VuCI API Support for Autonomy APP (Based on Example Foundation)
EOF

# Create API package postinst script
cat > "api-package/postinst" << 'EOF'
#!/bin/sh
[ "${IPKG_NO_SCRIPT}" = "1" ] && exit 0
[ -s ${IPKG_INSTROOT}/lib/functions.sh ] || exit 0
. ${IPKG_INSTROOT}/lib/functions.sh
default_postinst $0 $@
ubus call session reload_acls
exit 0
EOF
chmod +x "api-package/postinst"

# Build API package
echo "Building autonomy API package..."
cd "api-package"
tar -czf control.tar.gz control postinst
tar -czf data.tar.gz usr/
echo "2.0" > debian-binary
tar -czf "vuci-app-autonomy-api_${VERSION}_${ARCHITECTURE}.ipk" debian-binary control.tar.gz data.tar.gz
cd ..

# Create UI package structure
echo "Creating autonomy UI package from example foundation..."
mkdir -p "ui-package/usr/share/vuci/menu.d"
mkdir -p "ui-package/www/assets"
mkdir -p "ui-package/www/views/services"

# Create menu file
cat > "ui-package/usr/share/vuci/menu.d/autonomy.json" << 'EOF'
{
  "services/autonomy": {
    "title": "Autonomy",
    "index": 50,
    "view": "services/Autonomy",
    "acls": ["services/autonomy"]
  }
}
EOF

# Create Vue 3 component (based on example foundation)
cat > "ui-package/www/assets/app.autonomy.app-1.0.js.gz" << 'EOF'
'use strict';
(function () {
    'use strict';
    
    // Vue 3 component for Autonomy (based on example foundation)
    const { createApp, ref, onMounted } = Vue;
    
    const AutonomyApp = {
        setup() {
            const status = ref({
                running: false,
                uptime: '',
                last_check: '',
                issues: []
            });
            
            const overview = ref({
                service_status: 'Unknown',
                starlink_status: 'Unknown',
                cellular_status: 'Unknown',
                gps_status: 'Unknown',
                last_check: 'Never',
                issues: 0
            });
            
            const loadStatus = async () => {
                try {
                    const response = await fetch('/api/autonomy_f/status');
                    const data = await response.json();
                    status.value = data;
                } catch (error) {
                    console.error('Failed to load status:', error);
                    status.value = {
                        running: false,
                        uptime: '',
                        last_check: '',
                        issues: ['Failed to load status']
                    };
                }
            };
            
            const loadOverview = async () => {
                try {
                    const response = await fetch('/api/autonomy_f/overview');
                    const data = await response.json();
                    overview.value = data;
                } catch (error) {
                    console.error('Failed to load overview:', error);
                }
            };
            
            const restartService = async () => {
                try {
                    await fetch('/api/autonomy_f/actions/restart', { method: 'POST' });
                    await loadStatus();
                    await loadOverview();
                } catch (error) {
                    console.error('Failed to restart service:', error);
                }
            };
            
            onMounted(() => {
                loadStatus();
                loadOverview();
            });
            
            return {
                status,
                overview,
                loadStatus,
                loadOverview,
                restartService
            };
        },
        template: `
            <div>
                <div class="card">
                    <div class="card-header">
                        <h5>Autonomy Status</h5>
                    </div>
                    <div class="card-body">
                        <div class="row">
                            <div class="col-md-6">
                                <strong>Service Status:</strong>
                                <span :class="{'text-success': status.running, 'text-danger': !status.running}">
                                    {{ status.running ? 'Running' : 'Stopped' }}
                                </span>
                            </div>
                            <div class="col-md-6">
                                <strong>Uptime:</strong> {{ status.uptime || 'N/A' }}
                            </div>
                        </div>
                        <div class="row mt-2">
                            <div class="col-12">
                                <strong>Last Check:</strong> {{ status.last_check || 'N/A' }}
                            </div>
                        </div>
                        <div class="row mt-2" v-if="status.issues && status.issues.length > 0">
                            <div class="col-12">
                                <strong>Issues:</strong>
                                <ul class="list-unstyled">
                                    <li v-for="issue in status.issues" :key="issue" class="text-danger">{{ issue }}</li>
                                </ul>
                            </div>
                        </div>
                        <div class="row mt-3">
                            <div class="col-12">
                                <button class="btn btn-primary me-2" @click="loadStatus">Refresh Status</button>
                                <button class="btn btn-warning" @click="restartService">Restart Service</button>
                            </div>
                        </div>
                    </div>
                </div>
                
                <div class="card mt-3">
                    <div class="card-header">
                        <h5>System Overview</h5>
                    </div>
                    <div class="card-body">
                        <div class="row">
                            <div class="col-md-3">
                                <strong>Starlink:</strong>
                                <span :class="{'text-success': overview.starlink_status === 'Connected', 'text-danger': overview.starlink_status === 'Disconnected'}">
                                    {{ overview.starlink_status }}
                                </span>
                            </div>
                            <div class="col-md-3">
                                <strong>Cellular:</strong>
                                <span :class="{'text-success': overview.cellular_status === 'Connected', 'text-danger': overview.cellular_status === 'Disconnected'}">
                                    {{ overview.cellular_status }}
                                </span>
                            </div>
                            <div class="col-md-3">
                                <strong>GPS:</strong>
                                <span :class="{'text-success': overview.gps_status === 'Connected', 'text-danger': overview.gps_status === 'Disconnected'}">
                                    {{ overview.gps_status }}
                                </span>
                            </div>
                            <div class="col-md-3">
                                <strong>Issues:</strong>
                                <span :class="{'text-success': overview.issues === 0, 'text-warning': overview.issues > 0}">
                                    {{ overview.issues }}
                                </span>
                            </div>
                        </div>
                        <div class="row mt-2">
                            <div class="col-12">
                                <button class="btn btn-primary" @click="loadOverview">Refresh Overview</button>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        `
    };
    
    // Register the component
    if (typeof window !== 'undefined' && window.Vue) {
        window.Vue.createApp(AutonomyApp).mount('#autonomy-app');
    }
})();
EOF

# Create HTML template
cat > "ui-package/www/views/services/autonomy.html" << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <title>Autonomy</title>
</head>
<body>
    <div id="autonomy-app"></div>
    <script src="/assets/app.autonomy.app-1.0.js.gz"></script>
</body>
</html>
EOF

# Create UI package control file
cat > "ui-package/control" << EOF
Package: vuci-app-autonomy-ui
Version: $VERSION
Depends: vuci-app-autonomy-api, uci, ubus
Architecture: $ARCHITECTURE
Installed-Size: 2048
Description: VuCI UI Support for Autonomy APP (Based on Example Foundation)
EOF

# Create UI package postinst script
cat > "ui-package/postinst" << 'EOF'
#!/bin/sh
[ "${IPKG_NO_SCRIPT}" = "1" ] && exit 0
[ -s ${IPKG_INSTROOT}/lib/functions.sh ] || exit 0
. ${IPKG_INSTROOT}/lib/functions.sh
default_postinst $0 $@
exit 0
EOF
chmod +x "ui-package/postinst"

# Build UI package
echo "Building autonomy UI package..."
cd "ui-package"
tar -czf control.tar.gz control postinst
tar -czf data.tar.gz usr/ www/
echo "2.0" > debian-binary
tar -czf "vuci-app-autonomy-ui_${VERSION}_${ARCHITECTURE}.ipk" debian-binary control.tar.gz data.tar.gz
cd ..

# Copy packages to project directory
echo "Copying packages to project directory..."
cp "api-package/vuci-app-autonomy-api_${VERSION}_${ARCHITECTURE}.ipk" "$SCRIPT_DIR/"
cp "ui-package/vuci-app-autonomy-ui_${VERSION}_${ARCHITECTURE}.ipk" "$SCRIPT_DIR/"

echo "Build completed successfully!"
echo ""
echo "Packages created:"
echo "- API: $SCRIPT_DIR/vuci-app-autonomy-api_${VERSION}_${ARCHITECTURE}.ipk"
echo "- UI: $SCRIPT_DIR/vuci-app-autonomy-ui_${VERSION}_${ARCHITECTURE}.ipk"
echo ""
echo "To install on device:"
echo "1. Copy packages to device:"
echo "   scp $SCRIPT_DIR/vuci-app-autonomy-*.ipk root@192.168.80.1:/tmp/"
echo ""
echo "2. Install packages:"
echo "   ssh root@192.168.80.1 'opkg install /tmp/vuci-app-autonomy-api_${VERSION}_${ARCHITECTURE}.ipk'"
echo "   ssh root@192.168.80.1 'opkg install /tmp/vuci-app-autonomy-ui_${VERSION}_${ARCHITECTURE}.ipk'"
echo ""
echo "3. Restart services:"
echo "   ssh root@192.168.80.1 '/etc/init.d/rpcd restart && /etc/init.d/uhttpd restart'"

# Cleanup
rm -rf "$BUILD_DIR"
