local M = {}

function M.get_status()
    local status = {
        daemon_running = false,
        uptime = 0,
        interfaces = {},
        last_check = os.time()
    }
    
    -- Check if daemon is running
    local handle = io.popen("ps | grep autonomyd | grep -v grep")
    if handle then
        local result = handle:read("*a")
        handle:close()
        status.daemon_running = result ~= ""
    end
    
    -- Get uptime if running
    if status.daemon_running then
        handle = io.popen("ps -o etime= -p $(pgrep autonomyd) 2>/dev/null")
        if handle then
            local uptime = handle:read("*a"):match("^%s*(.-)%s*$")
            handle:close()
            status.uptime = uptime or "unknown"
        end
    end
    
    -- Get interface status
    local uci = require("uci")
    local cursor = uci.cursor()
    cursor:foreach("network", "interface", function(s)
        if s.ifname then
            table.insert(status.interfaces, {
                name = s[".name"],
                device = s.ifname,
                proto = s.proto or "none"
            })
        end
    end)
    
    return status
end

function M.start_daemon()
    os.execute("/etc/init.d/autonomy start")
    return { success = true, message = "Daemon started" }
end

function M.stop_daemon()
    os.execute("/etc/init.d/autonomy stop")
    return { success = true, message = "Daemon stopped" }
end

function M.restart_daemon()
    os.execute("/etc/init.d/autonomy restart")
    return { success = true, message = "Daemon restarted" }
end

function M.handle(method, path, query, body)
    if method == "GET" and path:match("/status") then
        return M.get_status()
    elseif method == "POST" and path:match("/start") then
        return M.start_daemon()
    elseif method == "POST" and path:match("/stop") then
        return M.stop_daemon()
    elseif method == "POST" and path:match("/restart") then
        return M.restart_daemon()
    else
        return { error = "Unknown endpoint", path = path }
    end
end

return M

