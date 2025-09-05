local M = {}

function M.get_config()
    local uci = require("uci")
    local cursor = uci.cursor()
    local config = {}
    
    -- Get main configuration
    cursor:foreach("autonomy", "main", function(s)
        config.main = s
    end)
    
    -- Get other sections
    cursor:foreach("autonomy", "starlink", function(s)
        config.starlink = s
    end)
    
    cursor:foreach("autonomy", "gps", function(s)
        config.gps = s
    end)
    
    cursor:foreach("autonomy", "notifications", function(s)
        config.notifications = s
    end)
    
    cursor:foreach("autonomy", "maintenance", function(s)
        config.maintenance = s
    end)
    
    return config
end

function M.set_config(section, option, value)
    local uci = require("uci")
    local cursor = uci.cursor()
    
    cursor:set("autonomy", section, option, value)
    cursor:commit("autonomy")
    
    return { success = true }
end

function M.handle(method, path, query, body)
    if method == "GET" and path:match("/config") then
        return M.get_config()
    elseif method == "POST" and path:match("/config") then
        local data = json.decode(body or "{}")
        if data.section and data.option and data.value then
            return M.set_config(data.section, data.option, data.value)
        else
            return { error = "Missing section, option, or value" }
        end
    else
        return { error = "Unknown endpoint", path = path }
    end
end

return M

