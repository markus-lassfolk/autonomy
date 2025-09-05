#!/usr/bin/env lua

-- Autonomy API for RPCD
-- This file provides the main API interface for the Autonomy system

local json = require("cjson")
local ubus = require("ubus")

-- Initialize ubus connection
local conn = ubus.connect()
if not conn then
    error("Failed to connect to ubus")
end

-- Helper function to execute commands
local function execute_command(cmd)
    local handle = io.popen(cmd .. " 2>&1")
    if not handle then
        return nil, "Failed to execute command"
    end
    
    local result = handle:read("*a")
    local exit_code = handle:close()
    
    return result, exit_code
end

-- Helper function to parse JSON safely
local function safe_json_decode(str)
    if not str or str == "" then
        return nil
    end
    
    local success, data = pcall(json.decode, str)
    if success then
        return data
    else
        return nil
    end
end

-- Get system status
local function get_system_status()
    local status = {
        running = false,
        uptime = "",
        last_check = os.date("%Y-%m-%d %H:%M:%S"),
        issues = 0,
        version = "1.0.0"
    }
    
    -- Check if autonomy daemon is running
    local result, exit_code = execute_command("pgrep autonomyd")
    if exit_code == 0 then
        status.running = true
    end
    
    -- Try to get status from ubus
    local ubus_result = conn:call("autonomy", "status", {})
    if ubus_result then
        status.ubus_status = ubus_result
    end
    
    return status
end

-- Get Starlink status
local function get_starlink_status()
    local status = {
        status = "unknown",
        latency = 0,
        obstruction = 0,
        snr = 0,
        uptime = 0
    }
    
    -- Try to get status from ubus
    local ubus_result = conn:call("autonomy", "starlink_status", {})
    if ubus_result then
        status = ubus_result
    end
    
    return status
end

-- Get GPS status
local function get_gps_status()
    local status = {
        status = "unknown",
        fix = false,
        latitude = 0,
        longitude = 0,
        altitude = 0,
        satellites = 0
    }
    
    -- Try to get status from ubus
    local ubus_result = conn:call("autonomy", "gps_status", {})
    if ubus_result then
        status = ubus_result
    end
    
    return status
end

-- Get network status
local function get_network_status()
    local status = {
        status = "unknown",
        active_interface = "unknown",
        interfaces = {}
    }
    
    -- Try to get status from ubus
    local ubus_result = conn:call("autonomy", "network_status", {})
    if ubus_result then
        status = ubus_result
    end
    
    return status
end

-- Get configuration
local function get_config()
    local config = {}
    
    -- Try to get config from ubus
    local ubus_result = conn:call("autonomy", "config", {})
    if ubus_result then
        config = ubus_result
    end
    
    return config
end

-- Get logs
local function get_logs(log_type, lines)
    lines = lines or 50
    local logs = {}
    
    local log_files = {
        autonomy = "/var/log/autonomy/autonomy.log",
        gps = "/var/log/autonomy/gps.log",
        starlink = "/var/log/autonomy/starlink.log",
        network = "/var/log/autonomy/network.log"
    }
    
    local log_file = log_files[log_type] or log_files.autonomy
    local file = io.open(log_file, "r")
    if file then
        local all_lines = {}
        for line in file:lines() do
            table.insert(all_lines, line)
        end
        file:close()
        
        -- Return last N lines
        local start = math.max(1, #all_lines - lines)
        for i = start, #all_lines do
            table.insert(logs, all_lines[i])
        end
    end
    
    return logs
end

-- Main API handler
local function handle_request(method, path, data)
    local parts = {}
    for part in path:gmatch("[^/]+") do
        table.insert(parts, part)
    end
    
    if #parts == 0 then
        return {error = "Invalid endpoint", code = 404}
    end
    
    local endpoint = parts[1]
    local sub_endpoint = parts[2]
    
    if endpoint == "status" then
        return {success = true, data = get_system_status()}
        
    elseif endpoint == "starlink" then
        return {success = true, data = get_starlink_status()}
        
    elseif endpoint == "gps" then
        return {success = true, data = get_gps_status()}
        
    elseif endpoint == "network" then
        return {success = true, data = get_network_status()}
        
    elseif endpoint == "config" then
        if method == "GET" then
            return {success = true, data = get_config()}
        elseif method == "POST" and data then
            -- Try to set config via ubus
            local result = conn:call("autonomy", "set_config", {config = data})
            return {success = true, data = result}
        end
        
    elseif endpoint == "logs" then
        if sub_endpoint then
            local lines = data and data.lines or 50
            return {success = true, data = get_logs(sub_endpoint, lines)}
        else
            return {error = "Log type required", code = 400}
        end
        
    elseif endpoint == "health-check" and method == "POST" then
        local result = conn:call("autonomy", "health", {})
        return {success = true, data = result}
        
    elseif endpoint == "restart" and method == "POST" then
        local result = conn:call("autonomy", "restart", {})
        return {success = true, data = result}
        
    else
        return {error = "Endpoint not found", code = 404}
    end
end

-- Export the handler function for rpcd
return {
    handle = handle_request
}