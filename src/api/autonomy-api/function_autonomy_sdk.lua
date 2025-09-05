local FunctionService = require("api/FunctionService")

local Autonomy = FunctionService:new()

-- GET /api/autonomy_f/status
function Autonomy:GET_TYPE_status()
    local status = {}
    local handle = io.popen("pgrep -f autonomyd")
    if handle then
        local result = handle:read("*a")
        handle:close()
        status.daemon_running = (result and result ~= "")
        status.status = status.daemon_running and "running" or "stopped"
    else
        status.daemon_running = false
        status.status = "stopped"
    end
    return self:ResponseOK(status)
end

-- GET /api/autonomy_f/config
function Autonomy:GET_TYPE_config()
    local uci = require "uci"
    local ctx = uci.cursor()
    local config = {}
    ctx:foreach("autonomy", "main", function(s)
        for k, v in pairs(s) do
            if type(v) == "string" then
                config[k] = v
            end
        end
    end)
    return self:ResponseOK({config = config})
end

function Autonomy:StartAction()
    os.execute("/etc/init.d/autonomy start")
    return self:ResponseOK({
        ok = true,
        message = "Service started"
    })
end

function Autonomy:StopAction()
    os.execute("/etc/init.d/autonomy stop")
    return self:ResponseOK({
        ok = true,
        message = "Service stopped"
    })
end

function Autonomy:RestartAction()
    os.execute("/etc/init.d/autonomy restart")
    return self:ResponseOK({
        ok = true,
        message = "Service restarted"
    })
end

-- POST /api/autonomy_f/actions/start
local start_action = Autonomy:action("start", Autonomy.StartAction)

-- POST /api/autonomy_f/actions/stop
local stop_action = Autonomy:action("stop", Autonomy.StopAction)

-- POST /api/autonomy_f/actions/restart
local restart_action = Autonomy:action("restart", Autonomy.RestartAction)

return Autonomy
