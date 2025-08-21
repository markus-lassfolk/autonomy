# VuCI Web Interface Development Guide

## Overview

This guide covers the development of the VuCI (Vu+ Configuration Interface) web interface for the autonomy daemon, providing a native RUTOS look & feel with real-time monitoring and configuration capabilities.

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Development Environment](#development-environment)
3. [Component Structure](#component-structure)
4. [RPC Daemon Plugin](#rpc-daemon-plugin)
5. [LuCI Controller](#luci-controller)
6. [View Templates](#view-templates)
7. [JavaScript Integration](#javascript-integration)
8. [Styling and UI](#styling-and-ui)
9. [Testing and Debugging](#testing-and-debugging)
10. [Deployment](#deployment)

## Architecture Overview

### VuCI Architecture

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Web Browser   │    │   LuCI/VuCI     │    │   RPC Daemon    │
│                 │    │   Framework     │    │   Plugin        │
├─────────────────┤    ├─────────────────┤    ├─────────────────┤
│ HTML Templates  │◄──►│ LuCI Controller │◄──►│ autonomy RPC    │
│ JavaScript      │    │ View System     │    │ Methods         │
│ CSS Styling     │    │ Route Handler   │    │ UCI Interface   │
└─────────────────┘    └─────────────────┘    └─────────────────┘
                                │                        │
                                ▼                        ▼
                       ┌─────────────────┐    ┌─────────────────┐
                       │   ubus System   │    │   UCI System    │
                       │                 │    │                 │
                       │ autonomy daemon │    │ /etc/config/    │
                       │ status/config   │    │ autonomy        │
                       └─────────────────┘    └─────────────────┘
```

### Data Flow

1. **User Request**: Browser requests page via HTTP
2. **Route Resolution**: LuCI controller handles route
3. **RPC Call**: Controller calls RPC daemon plugin
4. **ubus Communication**: RPC plugin communicates with autonomy daemon
5. **Data Processing**: Process and format data
6. **Response**: Return JSON/HTML to browser
7. **UI Update**: JavaScript updates interface

## Development Environment

### Prerequisites

- **RUTOS SDK**: Teltonika RUTX50 SDK
- **Lua**: Lua 5.1+ for RPC daemon and controller
- **JavaScript**: ES6+ for frontend development
- **CSS**: Modern CSS with responsive design
- **HTML**: Semantic HTML5 markup

### Setup Development Environment

```bash
# Clone autonomy repository
git clone https://github.com/autonomy/autonomy.git
cd autonomy

# Set up VuCI development structure
mkdir -p vuci-app-autonomy/root/usr/lib/lua/luci/view/autonomy
mkdir -p vuci-app-autonomy/root/usr/libexec/rpcd
mkdir -p vuci-app-autonomy/root/usr/lib/lua/luci/controller

# Install development dependencies
opkg update
opkg install luci-base luci-compat rpcd rpcd-mod-file rpcd-mod-iwinfo
```

## Component Structure

### Package Structure

```
vuci-app-autonomy/
├── Makefile                                    # Package build configuration
├── root/                                       # Root filesystem overlay
│   ├── usr/libexec/rpcd/autonomy              # RPC daemon plugin
│   ├── usr/lib/lua/luci/controller/autonomy.lua # LuCI controller
│   └── usr/lib/lua/luci/view/autonomy/        # View templates
│       ├── status.htm                         # Status page
│       ├── config.htm                         # Configuration page
│       ├── interfaces.htm                     # Interfaces page
│       ├── telemetry.htm                      # Telemetry page
│       ├── logs.htm                           # Logs page
│       └── resources.htm                      # Resources page
```

### File Descriptions

- **Makefile**: Package build configuration and dependencies
- **autonomy**: RPC daemon plugin providing backend API
- **autonomy.lua**: LuCI controller defining routes and handlers
- ***.htm**: HTML templates with embedded JavaScript and CSS

## RPC Daemon Plugin

### Plugin Structure

The RPC daemon plugin (`/usr/libexec/rpcd/autonomy`) provides the backend API:

```lua
#!/usr/bin/lua
-- autonomy RPC daemon plugin for VuCI

local rpcd = require "rpcd"
local ubus = require "ubus"
local json = require "luci.jsonc"
local util = require "luci.util"

local M = {}

-- Initialize ubus connection
local ubus_conn = ubus.connect()

-- Helper function to call autonomy service
local function call_autonomy(method, args)
    if not ubus_conn then
        return { error = "ubus connection not available" }
    end
    
    local result = ubus_conn:call("autonomy", method, args or {})
    if not result then
        return { error = "autonomy service not available" }
    end
    
    return result
end

-- RPC methods
function M.status()
    return call_autonomy("status")
end

function M.config()
    return call_autonomy("config")
end

function M.interfaces()
    return call_autonomy("interfaces")
end

function M.telemetry()
    return call_autonomy("telemetry")
end

function M.health()
    return call_autonomy("health")
end

function M.reload()
    return call_autonomy("reload")
end

function M.service_status()
    -- Get service status information
    local status = {
        running = false,
        pid = nil,
        uptime = nil,
        memory = nil
    }
    
    -- Check if service is running
    local pid = util.exec("cat /var/run/autonomyd.pid 2>/dev/null")
    if pid and pid:match("^%d+$") then
        status.running = true
        status.pid = tonumber(pid)
        
        -- Get process info
        local ps_output = util.exec("ps -o pid,ppid,etime,pcpu,pmem,comm -p " .. pid)
        if ps_output then
            -- Parse process information
            for line in ps_output:gmatch("[^\r\n]+") do
                if line:match("^%s*" .. pid) then
                    local parts = {}
                    for part in line:gmatch("%S+") do
                        table.insert(parts, part)
                    end
                    if #parts >= 5 then
                        status.uptime = parts[3]
                        status.cpu = parts[4]
                        status.memory = parts[5]
                    end
                    break
                end
            end
        end
    end
    
    return status
end

function M.logs(lines)
    lines = lines or 50
    local log_file = "/var/log/autonomyd.log"
    
    if not util.file_exists(log_file) then
        return { error = "Log file not found" }
    end
    
    local cmd = string.format("tail -n %d %s", lines, log_file)
    local output = util.exec(cmd)
    
    return { logs = output or "" }
end

function M.resources()
    local resources = {}
    
    -- CPU usage
    local cpu_info = util.exec("top -bn1 | grep 'Cpu(s)' | awk '{print $2}' | cut -d'%' -f1")
    resources.cpu = tonumber(cpu_info) or 0
    
    -- Memory usage
    local mem_info = util.exec("free | grep Mem | awk '{printf \"%.1f\", $3/$2 * 100.0}'")
    resources.memory = tonumber(mem_info) or 0
    
    -- Disk usage
    local disk_info = util.exec("df / | tail -1 | awk '{print $5}' | cut -d'%' -f1")
    resources.disk = tonumber(disk_info) or 0
    
    -- Network interfaces
    resources.interfaces = {}
    local ifaces = util.exec("ip link show | grep '^[0-9]' | awk '{print $2}' | cut -d':' -f1")
    if ifaces then
        for iface in ifaces:gmatch("[^\r\n]+") do
            local rx_bytes = util.exec(string.format("cat /sys/class/net/%s/statistics/rx_bytes 2>/dev/null", iface)) or "0"
            local tx_bytes = util.exec(string.format("cat /sys/class/net/%s/statistics/tx_bytes 2>/dev/null", iface)) or "0"
            
            table.insert(resources.interfaces, {
                name = iface,
                rx_bytes = tonumber(rx_bytes) or 0,
                tx_bytes = tonumber(tx_bytes) or 0
            })
        end
    end
    
    return resources
end

-- UCI configuration management
function M.get_uci_config()
    local config = {}
    local uci = require "uci"
    local cursor = uci.cursor()
    
    -- Read autonomy configuration sections
    cursor:foreach("autonomy", "autonomy", function(section)
        if section[".name"] == "main" then
            config.main = section
        end
    end)
    
    cursor:foreach("autonomy", "thresholds", function(section)
        config.thresholds = config.thresholds or {}
        config.thresholds[section[".name"]] = section
    end)
    
    cursor:foreach("autonomy", "interface", function(section)
        config.interfaces = config.interfaces or {}
        config.interfaces[section[".name"]] = section
    end)
    
    return config
end

function M.set_uci_config(config)
    local uci = require "uci"
    local cursor = uci.cursor()
    
    -- Update configuration sections
    if config.main then
        for key, value in pairs(config.main) do
            if key:sub(1, 1) ~= "." then
                cursor:set("autonomy", "main", key, value)
            end
        end
    end
    
    if config.thresholds then
        for section_name, section_data in pairs(config.thresholds) do
            for key, value in pairs(section_data) do
                if key:sub(1, 1) ~= "." then
                    cursor:set("autonomy", section_name, key, value)
                end
            end
        end
    end
    
    if config.interfaces then
        for section_name, section_data in pairs(config.interfaces) do
            for key, value in pairs(section_data) do
                if key:sub(1, 1) ~= "." then
                    cursor:set("autonomy", section_name, key, value)
                end
            end
        end
    end
    
    -- Commit changes
    cursor:commit("autonomy")
    
    return { success = true }
end

-- Register RPC methods
local methods = {
    status = M.status,
    config = M.config,
    interfaces = M.interfaces,
    telemetry = M.telemetry,
    health = M.health,
    reload = M.reload,
    service_status = M.service_status,
    logs = M.logs,
    resources = M.resources,
    get_uci_config = M.get_uci_config,
    set_uci_config = M.set_uci_config
}

-- Start RPC daemon
rpcd.run(methods)
```

### Key Features

- **ubus Integration**: Direct communication with autonomy daemon
- **UCI Management**: Read/write UCI configuration
- **System Monitoring**: CPU, memory, disk, and network statistics
- **Service Control**: Start/stop/restart autonomy service
- **Log Management**: Real-time log viewing and filtering
- **Error Handling**: Comprehensive error handling and reporting

## LuCI Controller

### Controller Structure

The LuCI controller (`/usr/lib/lua/luci/controller/autonomy.lua`) defines the web interface routes:

```lua
module("luci.controller.autonomy", package.seeall)

function index()
    if not nixio.fs.access("/etc/config/autonomy") then
        return
    end

    local page = entry({"admin", "network", "autonomy"}, alias("admin", "network", "autonomy", "status"), _("Autonomy"), 60)
    page.dependent = true
    page.acl_depends = { "luci-app-autonomy" }

    -- Page routes
    entry({"admin", "network", "autonomy", "status"}, template("autonomy/status"), _("Status"), 10).leaf = true
    entry({"admin", "network", "autonomy", "config"}, template("autonomy/config"), _("Configuration"), 20).leaf = true
    entry({"admin", "network", "autonomy", "interfaces"}, template("autonomy/interfaces"), _("Interfaces"), 30).leaf = true
    entry({"admin", "network", "autonomy", "telemetry"}, template("autonomy/telemetry"), _("Telemetry"), 40).leaf = true
    entry({"admin", "network", "autonomy", "logs"}, template("autonomy/logs"), _("Logs"), 50).leaf = true
    entry({"admin", "network", "autonomy", "resources"}, template("autonomy/resources"), _("Resources"), 60).leaf = true

    -- API endpoints
    entry({"admin", "network", "autonomy", "api", "status"}, call("action_status")).leaf = true
    entry({"admin", "network", "autonomy", "api", "config"}, call("action_config")).leaf = true
    entry({"admin", "network", "autonomy", "api", "interfaces"}, call("action_interfaces")).leaf = true
    entry({"admin", "network", "autonomy", "api", "telemetry"}, call("action_telemetry")).leaf = true
    entry({"admin", "network", "autonomy", "api", "logs"}, call("action_logs")).leaf = true
    entry({"admin", "network", "autonomy", "api", "resources"}, call("action_resources")).leaf = true
    entry({"admin", "network", "autonomy", "api", "service_status"}, call("action_service_status")).leaf = true
    entry({"admin", "network", "autonomy", "api", "reload"}, call("action_reload")).leaf = true
    entry({"admin", "network", "autonomy", "api", "get_uci_config"}, call("action_get_uci_config")).leaf = true
    entry({"admin", "network", "autonomy", "api", "set_uci_config"}, call("action_set_uci_config")).leaf = true
end

-- API action handlers
function action_status()
    local rpc = require "luci.rpcc"
    local result = rpc.call("autonomy", "status", {})
    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end

function action_config()
    local rpc = require "luci.rpcc"
    local result = rpc.call("autonomy", "config", {})
    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end

function action_interfaces()
    local rpc = require "luci.rpcc"
    local result = rpc.call("autonomy", "interfaces", {})
    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end

function action_telemetry()
    local rpc = require "luci.rpcc"
    local result = rpc.call("autonomy", "telemetry", {})
    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end

function action_logs()
    local rpc = require "luci.rpcc"
    local lines = luci.http.formvalue("lines") or 50
    local result = rpc.call("autonomy", "logs", {lines = tonumber(lines)})
    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end

function action_resources()
    local rpc = require "luci.rpcc"
    local result = rpc.call("autonomy", "resources", {})
    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end

function action_service_status()
    local rpc = require "luci.rpcc"
    local result = rpc.call("autonomy", "service_status", {})
    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end

function action_reload()
    local rpc = require "luci.rpcc"
    local result = rpc.call("autonomy", "reload", {})
    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end

function action_get_uci_config()
    local rpc = require "luci.rpcc"
    local result = rpc.call("autonomy", "get_uci_config", {})
    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end

function action_set_uci_config()
    local rpc = require "luci.rpcc"
    local config = luci.http.formvalue("config")
    if config then
        config = luci.jsonc.parse(config)
        local result = rpc.call("autonomy", "set_uci_config", {config = config})
        luci.http.prepare_content("application/json")
        luci.http.write_json(result)
    else
        luci.http.status(400, "Bad Request")
        luci.http.prepare_content("application/json")
        luci.http.write_json({error = "No configuration provided"})
    end
end
```

### Route Structure

- **Main Entry**: `/admin/network/autonomy` → Status page
- **Status Page**: `/admin/network/autonomy/status` → Real-time monitoring
- **Configuration**: `/admin/network/autonomy/config` → UCI configuration
- **Interfaces**: `/admin/network/autonomy/interfaces` → Interface management
- **Telemetry**: `/admin/network/autonomy/telemetry` → Telemetry data
- **Logs**: `/admin/network/autonomy/logs` → Log viewer
- **Resources**: `/admin/network/autonomy/resources` → System resources

### API Endpoints

- **GET** `/admin/network/autonomy/api/status` → Get autonomy status
- **GET** `/admin/network/autonomy/api/config` → Get configuration
- **GET** `/admin/network/autonomy/api/interfaces` → Get interface status
- **GET** `/admin/network/autonomy/api/telemetry` → Get telemetry data
- **GET** `/admin/network/autonomy/api/logs` → Get logs
- **GET** `/admin/network/autonomy/api/resources` → Get system resources
- **GET** `/admin/network/autonomy/api/service_status` → Get service status
- **POST** `/admin/network/autonomy/api/reload` → Reload configuration
- **GET** `/admin/network/autonomy/api/get_uci_config` → Get UCI configuration
- **POST** `/admin/network/autonomy/api/set_uci_config` → Set UCI configuration

## View Templates

### Template Structure

View templates are HTML files with embedded JavaScript and CSS:

```html
<%+header%>

<script type="text/javascript" src="<%=resource%>/cbi.js"></script>
<script type="text/javascript" src="<%=resource%>/autonomy.js"></script>

<div class="cbi-map">
    <div class="cbi-map-descr">
        <h2><%:Page Title%></h2>
        <p><%:Page description%></p>
    </div>

    <div class="cbi-section">
        <!-- Page content -->
    </div>
</div>

<style>
/* CSS styles */
</style>

<script type="text/javascript">
// JavaScript functionality
</script>

<%+footer%>
```

### Key Template Features

1. **LuCI Integration**: Uses LuCI header/footer and resource system
2. **Responsive Design**: Mobile-friendly CSS Grid and Flexbox
3. **Real-time Updates**: JavaScript polling for live data
4. **Interactive Elements**: Buttons, forms, and dynamic content
5. **Error Handling**: Graceful error display and recovery

### Template Best Practices

1. **Semantic HTML**: Use proper HTML5 semantic elements
2. **Accessibility**: Include ARIA labels and keyboard navigation
3. **Performance**: Optimize JavaScript and CSS loading
4. **Mobile First**: Design for mobile devices first
5. **Progressive Enhancement**: Ensure basic functionality without JavaScript

## JavaScript Integration

### Core JavaScript Functions

```javascript
// Global variables
let updateInterval;
let serviceStatus = 'unknown';

// Initialize the page
document.addEventListener('DOMContentLoaded', function() {
    loadStatus();
    startAutoRefresh();
});

// Load status data
function loadStatus() {
    // Load service status
    fetch('/admin/network/autonomy/api/service_status')
        .then(response => response.json())
        .then(data => updateServiceStatus(data))
        .catch(error => console.error('Error loading service status:', error));

    // Load autonomy status
    fetch('/admin/network/autonomy/api/status')
        .then(response => response.json())
        .then(data => updateAutonomyStatus(data))
        .catch(error => console.error('Error loading autonomy status:', error));

    // Load interfaces
    fetch('/admin/network/autonomy/api/interfaces')
        .then(response => response.json())
        .then(data => updateInterfaces(data))
        .catch(error => console.error('Error loading interfaces:', error));

    // Load resources
    fetch('/admin/network/autonomy/api/resources')
        .then(response => response.json())
        .then(data => updateResources(data))
        .catch(error => console.error('Error loading resources:', error));
}

// Update service status
function updateServiceStatus(data) {
    const statusElement = document.getElementById('service-status');
    const startBtn = document.getElementById('start-service');
    const stopBtn = document.getElementById('stop-service');
    const restartBtn = document.getElementById('restart-service');

    if (data.running) {
        statusElement.textContent = 'Running';
        statusElement.className = 'status-indicator running';
        startBtn.style.display = 'none';
        stopBtn.style.display = 'inline-block';
        restartBtn.style.display = 'inline-block';
        serviceStatus = 'running';
    } else {
        statusElement.textContent = 'Stopped';
        statusElement.className = 'status-indicator stopped';
        startBtn.style.display = 'inline-block';
        stopBtn.style.display = 'none';
        restartBtn.style.display = 'none';
        serviceStatus = 'stopped';
    }

    if (data.pid) {
        statusElement.textContent += ` (PID: ${data.pid})`;
    }
}

// Update interfaces
function updateInterfaces(data) {
    if (data.error) {
        console.error('Interfaces error:', data.error);
        return;
    }

    const container = document.getElementById('interfaces-container');
    container.innerHTML = '';

    if (data.interfaces && Array.isArray(data.interfaces)) {
        data.interfaces.forEach(iface => {
            const card = createInterfaceCard(iface);
            container.appendChild(card);
        });
    }
}

// Create interface card
function createInterfaceCard(iface) {
    const card = document.createElement('div');
    card.className = `interface-card ${iface.active ? 'active' : ''} ${iface.enabled ? '' : 'disabled'}`;

    const statusClass = iface.enabled ? (iface.up ? 'up' : 'down') : 'disabled';
    const statusText = iface.enabled ? (iface.up ? 'UP' : 'DOWN') : 'DISABLED';

    card.innerHTML = `
        <div class="interface-header">
            <span class="interface-name">${iface.name}</span>
            <span class="interface-status ${statusClass}">${statusText}</span>
        </div>
        <div class="interface-metrics">
            <div class="metric-item">
                <span class="metric-label">Type:</span>
                <span class="metric-value">${iface.type || 'Unknown'}</span>
            </div>
            <div class="metric-item">
                <span class="metric-label">Priority:</span>
                <span class="metric-value">${iface.priority || 'N/A'}</span>
            </div>
            <div class="metric-item">
                <span class="metric-label">Latency:</span>
                <span class="metric-value">${iface.latency ? iface.latency + 'ms' : 'N/A'}</span>
            </div>
            <div class="metric-item">
                <span class="metric-label">Loss:</span>
                <span class="metric-value">${iface.loss ? iface.loss + '%' : 'N/A'}</span>
            </div>
        </div>
    `;

    return card;
}

// Update resources
function updateResources(data) {
    if (data.error) {
        console.error('Resources error:', data.error);
        return;
    }

    // Update CPU usage
    if (data.cpu !== undefined) {
        const cpuElement = document.getElementById('cpu-usage');
        const cpuText = document.getElementById('cpu-text');
        cpuElement.style.width = data.cpu + '%';
        cpuText.textContent = data.cpu.toFixed(1) + '%';
        
        if (data.cpu > 80) {
            cpuElement.className = 'progress-fill danger';
        } else if (data.cpu > 60) {
            cpuElement.className = 'progress-fill warning';
        } else {
            cpuElement.className = 'progress-fill';
        }
    }

    // Update memory usage
    if (data.memory !== undefined) {
        const memElement = document.getElementById('memory-usage');
        const memText = document.getElementById('memory-text');
        memElement.style.width = data.memory + '%';
        memText.textContent = data.memory.toFixed(1) + '%';
        
        if (data.memory > 80) {
            memElement.className = 'progress-fill danger';
        } else if (data.memory > 60) {
            memElement.className = 'progress-fill warning';
        } else {
            memElement.className = 'progress-fill';
        }
    }

    // Update disk usage
    if (data.disk !== undefined) {
        const diskElement = document.getElementById('disk-usage');
        const diskText = document.getElementById('disk-text');
        diskElement.style.width = data.disk + '%';
        diskText.textContent = data.disk.toFixed(1) + '%';
        
        if (data.disk > 90) {
            diskElement.className = 'progress-fill danger';
        } else if (data.disk > 80) {
            diskElement.className = 'progress-fill warning';
        } else {
            diskElement.className = 'progress-fill';
        }
    }
}

// Service control functions
document.getElementById('start-service').addEventListener('click', function() {
    if (confirm('Start the autonomy service?')) {
        fetch('/admin/network/autonomy/api/reload', {
            method: 'POST'
        }).then(() => {
            setTimeout(loadStatus, 1000);
        });
    }
});

document.getElementById('stop-service').addEventListener('click', function() {
    if (confirm('Stop the autonomy service?')) {
        fetch('/admin/network/autonomy/api/reload', {
            method: 'POST'
        }).then(() => {
            setTimeout(loadStatus, 1000);
        });
    }
});

document.getElementById('restart-service').addEventListener('click', function() {
    if (confirm('Restart the autonomy service?')) {
        fetch('/admin/network/autonomy/api/reload', {
            method: 'POST'
        }).then(() => {
            setTimeout(loadStatus, 1000);
        });
    }
});

// Auto-refresh
function startAutoRefresh() {
    updateInterval = setInterval(loadStatus, 5000); // Update every 5 seconds
}

function stopAutoRefresh() {
    if (updateInterval) {
        clearInterval(updateInterval);
    }
}

// Cleanup on page unload
window.addEventListener('beforeunload', stopAutoRefresh);
```

### JavaScript Best Practices

1. **Error Handling**: Comprehensive error handling with user feedback
2. **Performance**: Efficient DOM manipulation and event handling
3. **Memory Management**: Proper cleanup of intervals and event listeners
4. **User Experience**: Smooth animations and responsive interactions
5. **Accessibility**: Keyboard navigation and screen reader support

## Styling and UI

### CSS Framework

The VuCI interface uses a custom CSS framework designed to match RUTOS styling:

```css
/* Status indicators */
.status-indicator {
    padding: 4px 8px;
    border-radius: 4px;
    font-weight: bold;
    margin-right: 10px;
}

.status-indicator.running {
    background-color: #d4edda;
    color: #155724;
    border: 1px solid #c3e6cb;
}

.status-indicator.stopped {
    background-color: #f8d7da;
    color: #721c24;
    border: 1px solid #f5c6cb;
}

.status-indicator.loading {
    background-color: #fff3cd;
    color: #856404;
    border: 1px solid #ffeaa7;
}

/* Resource monitoring */
.resource-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
    gap: 15px;
    margin-top: 10px;
}

.resource-item {
    display: flex;
    flex-direction: column;
    align-items: center;
}

.progress-bar {
    width: 100%;
    height: 20px;
    background-color: #e9ecef;
    border-radius: 10px;
    overflow: hidden;
    margin-bottom: 5px;
}

.progress-fill {
    height: 100%;
    background: linear-gradient(90deg, #28a745, #20c997);
    transition: width 0.3s ease;
}

.progress-fill.warning {
    background: linear-gradient(90deg, #ffc107, #fd7e14);
}

.progress-fill.danger {
    background: linear-gradient(90deg, #dc3545, #e83e8c);
}

/* Interface cards */
.interfaces-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
    gap: 15px;
    margin-top: 10px;
}

.interface-card {
    border: 1px solid #dee2e6;
    border-radius: 8px;
    padding: 15px;
    background-color: #f8f9fa;
}

.interface-card.active {
    border-color: #28a745;
    background-color: #d4edda;
}

.interface-card.disabled {
    border-color: #6c757d;
    background-color: #e9ecef;
    opacity: 0.6;
}

.interface-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 10px;
}

.interface-status {
    padding: 2px 6px;
    border-radius: 3px;
    font-size: 12px;
    font-weight: bold;
}

.interface-status.up {
    background-color: #28a745;
    color: white;
}

.interface-status.down {
    background-color: #dc3545;
    color: white;
}

.interface-status.disabled {
    background-color: #6c757d;
    color: white;
}

/* Buttons */
.btn {
    padding: 6px 12px;
    border: 1px solid #007bff;
    background-color: #007bff;
    color: white;
    border-radius: 4px;
    cursor: pointer;
    margin-left: 5px;
    font-size: 14px;
}

.btn:hover {
    background-color: #0056b3;
    border-color: #0056b3;
}

.btn.danger {
    background-color: #dc3545;
    border-color: #dc3545;
}

.btn.danger:hover {
    background-color: #c82333;
    border-color: #bd2130;
}

.btn.success {
    background-color: #28a745;
    border-color: #28a745;
}

.btn.success:hover {
    background-color: #218838;
    border-color: #1e7e34;
}

/* Responsive design */
@media (max-width: 768px) {
    .resource-grid {
        grid-template-columns: 1fr;
    }
    
    .interfaces-grid {
        grid-template-columns: 1fr;
    }
    
    .interface-metrics {
        grid-template-columns: 1fr;
    }
}
```

### Design Principles

1. **Consistency**: Match RUTOS web interface styling
2. **Responsiveness**: Mobile-first design approach
3. **Accessibility**: High contrast and readable fonts
4. **Performance**: Optimized CSS with minimal reflows
5. **Maintainability**: Modular CSS with clear naming conventions

## Testing and Debugging

### Development Testing

1. **Local Testing**:
   ```bash
   # Install development packages
   opkg install luci-base luci-compat rpcd rpcd-mod-file
   
   # Test RPC daemon
   rpcd -i
   
   # Test LuCI controller
   luci-reload
   ```

2. **Browser Testing**:
   - Test on multiple browsers (Chrome, Firefox, Safari)
   - Test responsive design on mobile devices
   - Verify JavaScript functionality
   - Check accessibility features

3. **API Testing**:
   ```bash
   # Test RPC methods
   ubus call autonomy status
   ubus call autonomy config
   ubus call autonomy interfaces
   
   # Test web API endpoints
   curl http://router/admin/network/autonomy/api/status
   curl http://router/admin/network/autonomy/api/resources
   ```

### Debug Procedures

1. **Enable Debug Logging**:
   ```bash
   # Enable debug logging
   uci set autonomy.main.log_level='debug'
   uci commit autonomy
   /etc/init.d/autonomy restart
   
   # Monitor logs
   tail -f /var/log/autonomyd.log
   ```

2. **Check RPC Daemon**:
   ```bash
   # Check RPC daemon status
   rpcd -i
   
   # Test RPC methods
   rpcd -i autonomy status
   rpcd -i autonomy config
   ```

3. **Check Web Interface**:
   ```bash
   # Check LuCI installation
   opkg list-installed | grep luci
   
   # Check web server logs
   logread | grep uhttpd
   
   # Test web interface access
   curl -I http://router/admin/network/autonomy/status
   ```

### Performance Testing

1. **Resource Usage**:
   ```bash
   # Monitor CPU usage
   top -p $(cat /var/run/autonomyd.pid)
   
   # Monitor memory usage
   cat /proc/$(cat /var/run/autonomyd.pid)/status
   
   # Monitor network usage
   iftop -i eth0
   ```

2. **Load Testing**:
   ```bash
   # Test API endpoints under load
   ab -n 1000 -c 10 http://router/admin/network/autonomy/api/status
   
   # Test web interface performance
   ab -n 100 -c 5 http://router/admin/network/autonomy/status
   ```

## Deployment

### Package Building

1. **Build VuCI Package**:
   ```bash
   # Navigate to SDK
   cd /path/to/rutos-sdk
   
   # Build VuCI package
   make package/vuci-app-autonomy/compile V=s
   make package/vuci-app-autonomy/install V=s
   ```

2. **Generate IPK**:
   ```bash
   # Find generated IPK
   find bin/packages/ -name "*vuci-app-autonomy*.ipk"
   
   # Copy to distribution directory
   cp bin/packages/*/luci/vuci-app-autonomy*.ipk /var/www/autonomy-feed/
   ```

### Installation

1. **Install Package**:
   ```bash
   # Update package lists
   opkg update
   
   # Install VuCI package
   opkg install vuci-app-autonomy
   ```

2. **Verify Installation**:
   ```bash
   # Check package installation
   opkg list-installed | grep vuci-app-autonomy
   
   # Check file installation
   ls -la /usr/lib/lua/luci/controller/autonomy.lua
   ls -la /usr/libexec/rpcd/autonomy
   ls -la /usr/lib/lua/luci/view/autonomy/
   ```

3. **Test Web Interface**:
   ```bash
   # Reload LuCI
   luci-reload
   
   # Access web interface
   # Navigate to http://router/admin/network/autonomy/
   ```

### Configuration

1. **Enable Web Interface**:
   ```bash
   # Enable autonomy service
   uci set autonomy.main.enable='1'
   uci commit autonomy
   
   # Start service
   /etc/init.d/autonomy start
   ```

2. **Configure Access Control**:
   ```bash
   # Set up user permissions
   uci set luci.main.mediaurlbase='/luci-static/resources'
   uci commit luci
   ```

3. **Test Functionality**:
   ```bash
   # Test service control
   /etc/init.d/autonomy status
   
   # Test API endpoints
   ubus call autonomy status
   
   # Test web interface
   # Access http://router/admin/network/autonomy/status
   ```

## Best Practices

### Development

1. **Code Organization**: Clear separation of concerns
2. **Error Handling**: Comprehensive error handling
3. **Documentation**: Inline documentation and comments
4. **Testing**: Automated and manual testing procedures
5. **Version Control**: Proper version management

### Security

1. **Input Validation**: Validate all user inputs
2. **Access Control**: Implement proper access controls
3. **Data Sanitization**: Sanitize all data outputs
4. **Secure Communication**: Use HTTPS for sensitive data

### Performance

1. **Optimization**: Optimize JavaScript and CSS
2. **Caching**: Implement appropriate caching strategies
3. **Resource Management**: Efficient resource usage
4. **Monitoring**: Continuous performance monitoring

### User Experience

1. **Responsive Design**: Mobile-friendly interface
2. **Accessibility**: Follow accessibility guidelines
3. **Intuitive Navigation**: Clear and logical navigation
4. **Feedback**: Provide user feedback for actions

---

**Last Updated**: 2025-08-20
**Version**: 1.0.0

