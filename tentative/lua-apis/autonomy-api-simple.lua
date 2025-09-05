#!/usr/bin/env lua

-- Simple API server for Autonomy system
-- This version doesn't depend on luci modules

local cgi = require("cgi")
local json = require("cjson")

-- Helper functions
local function json_response(data, status)
    status = status or 200
    print("Content-Type: application/json")
    print("Status: " .. status)
    print("")
    print(json.encode(data))
end

local function error_response(message, status)
    status = status or 500
    json_response({error = message}, status)
end

local function success_response(data)
    json_response({success = true, data = data})
end

-- Execute command and return result
local function execute_command(cmd)
    local handle = io.popen(cmd .. " 2>&1")
    if not handle then
        return nil, "Failed to execute command"
    end
    
    local result = handle:read("*a")
    local exit_code = handle:close()
    
    return result, exit_code
end

-- Get system status
local function get_system_status()
    local status = {
        status = "unknown",
        lastCheck = os.time(),
        version = "1.0.0"
    }
    
    -- Check if autonomy daemon is running
    local result, exit_code = execute_command("pgrep autonomyd")
    if exit_code == 0 then
        status.status = "healthy"
        status.daemon_running = true
    else
        status.status = "error"
        status.daemon_running = false
    end
    
    -- Get system uptime
    local uptime_file = io.open("/proc/uptime", "r")
    if uptime_file then
        local uptime_seconds = uptime_file:read("*n")
        status.uptime_seconds = uptime_seconds
        uptime_file:close()
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
    
    -- Try to get status from autonomy daemon
    local result, exit_code = execute_command("/usr/local/bin/autonomyctl starlink")
    if exit_code == 0 and result then
        -- Parse the output (assuming JSON format)
        local success, data = pcall(json.decode, result)
        if success and data then
            status = data
        end
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
    
    -- Try to get GPS status from autonomy daemon
    local result, exit_code = execute_command("/usr/local/bin/autonomyctl gps")
    if exit_code == 0 and result then
        local success, data = pcall(json.decode, result)
        if success and data then
            status = data
        end
    end
    
    return status
end

-- Get network status
local function get_network_status()
    local status = {
        status = "unknown",
        activeInterface = "unknown"
    }
    
    -- Get active interface from mwan3
    local result, exit_code = execute_command("ubus call mwan3 status")
    if exit_code == 0 and result then
        local success, data = pcall(json.decode, result)
        if success and data then
            -- Find the active interface
            for interface, info in pairs(data) do
                if info.status == "online" then
                    status.activeInterface = interface
                    status.status = "healthy"
                    break
                end
            end
        end
    end
    
    return status
end

-- Get log content
local function get_log_content(log_type)
    local log_files = {
        autonomy = "/var/log/autonomy/autonomy.log",
        gps = "/var/log/autonomy/gps.log",
        opencellid = "/var/log/autonomy/opencellid.log",
        health = "/var/log/autonomy/health.log"
    }
    
    local log_file = log_files[log_type]
    if not log_file then
        return nil, "Invalid log type"
    end
    
    local file = io.open(log_file, "r")
    if not file then
        return "", "Log file not found"
    end
    
    local content = file:read("*a")
    file:close()
    
    return content
end

-- Main request handler
local function handle_request()
    local method = os.getenv("REQUEST_METHOD") or "GET"
    local path = os.getenv("PATH_INFO") or ""
    
    -- Parse path
    local parts = {}
    for part in path:gmatch("[^/]+") do
        table.insert(parts, part)
    end
    
    if #parts == 0 then
        return error_response("Invalid endpoint", 404)
    end
    
    local endpoint = parts[1]
    local sub_endpoint = parts[2]
    
    if endpoint == "status" then
        return success_response(get_system_status())
        
    elseif endpoint == "starlink" then
        return success_response(get_starlink_status())
        
    elseif endpoint == "gps" then
        return success_response(get_gps_status())
        
    elseif endpoint == "network" then
        return success_response(get_network_status())
        
    elseif endpoint == "health-check" and method == "POST" then
        local result, exit_code = execute_command("/usr/local/bin/autonomyctl health-check")
        if exit_code == 0 then
            return success_response({message = "Health check completed"})
        else
            return error_response("Health check failed", 500)
        end
        
    elseif endpoint == "opencellid" and sub_endpoint == "submit" and method == "POST" then
        local result, exit_code = execute_command("/usr/local/bin/autonomyctl opencellid-submit")
        if exit_code == 0 then
            return success_response({message = "Data submitted to OpenCELLID"})
        else
            return error_response("OpenCELLID submission failed", 500)
        end
        
    elseif endpoint == "logs" then
        if sub_endpoint then
            local content, error = get_log_content(sub_endpoint)
            if error then
                return error_response(error, 404)
            end
            return success_response({content = content})
        else
            return error_response("Log type required", 400)
        end
        
    else
        return error_response("Endpoint not found", 404)
    end
end

-- Main execution
local success, result = pcall(handle_request)
if not success then
    error_response("Internal server error: " .. tostring(result), 500)
end
