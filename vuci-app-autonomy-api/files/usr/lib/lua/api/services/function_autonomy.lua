local FunctionService = require("api/FunctionService")

local Autonomy = FunctionService:new()

-- GET /api/autonomy_f/status
function Autonomy:GET_TYPE_status()
	local status = {
		running = false,
		uptime = "",
		last_check = "",
		issues = 0,
		starlink = {},
		cellular = {},
		gps = {}
	}

	-- Check if autonomy daemon is running
	local handle = io.popen("ps | grep autonomyd | grep -v grep")
	if handle then
		status.running = handle:read("*a") ~= ""
		handle:close()
	end

	-- Get system status from autonomysysmgmt
	local handle = io.popen("/usr/local/bin/autonomysysmgmt -check -dry-run 2>/dev/null")
	if handle then
		local output = handle:read("*a")
		handle:close()
		
		-- Parse JSON output for status information
		if output and output ~= "" then
			-- Extract basic status info
			for line in output:gmatch("[^\r\n]+") do
				if line:match("issues_found") then
					local issues = line:match('"issues_found":(%d+)')
					if issues then
						status.issues = tonumber(issues)
					end
				end
				if line:match("duration") then
					local duration = line:match('"duration":(%d+)')
					if duration then
						status.uptime = string.format("%.1f seconds", tonumber(duration) / 1000000000)
					end
				end
			end
		end
	end

	status.last_check = os.date("%Y-%m-%d %H:%M:%S")

	return self:ResponseOK(status)
end

-- GET /api/autonomy_f/logs
function Autonomy:GET_TYPE_logs()
	local logs = {}
	
	-- Read autonomy logs
	local log_file = "/var/log/autonomy/autonomy.log"
	local handle = io.open(log_file, "r")
	if handle then
		for line in handle:lines() do
			table.insert(logs, line)
		end
		handle:close()
	end

	-- Return last 100 lines
	local start = math.max(1, #logs - 100)
	local recent_logs = {}
	for i = start, #logs do
		table.insert(recent_logs, logs[i])
	end

	return self:ResponseOK(recent_logs)
end

-- GET /api/autonomy_f/overview
function Autonomy:GET_TYPE_overview()
	local overview = {
		running = false,
		uptime = "",
		last_check = "",
		issues = 0,
		services = {
			starlink = false,
			cellular = false,
			gps = false
		}
	}

	-- Check if autonomy daemon is running
	local handle = io.popen("ps | grep autonomyd | grep -v grep")
	if handle then
		overview.running = handle:read("*a") ~= ""
		handle:close()
	end

	-- Get basic status
	local handle = io.popen("/usr/local/bin/autonomysysmgmt -check -dry-run 2>/dev/null")
	if handle then
		local output = handle:read("*a")
		handle:close()
		
		if output and output ~= "" then
			for line in output:gmatch("[^\r\n]+") do
				if line:match("issues_found") then
					local issues = line:match('"issues_found":(%d+)')
					if issues then
						overview.issues = tonumber(issues)
					end
				end
				if line:match("duration") then
					local duration = line:match('"duration":(%d+)')
					if duration then
						overview.uptime = string.format("%.1f seconds", tonumber(duration) / 1000000000)
					end
				end
			end
		end
	end

	overview.last_check = os.date("%Y-%m-%d %H:%M:%S")

	return self:ResponseOK(overview)
end

-- POST /api/autonomy_f/actions/restart
function Autonomy:RestartAction()
	-- For error handling use error functions:
	--      self:add_critical_error(100, "Example error message", "Source")
	--      self:add_error(100, "Example error message", "Source")

	-- Restart autonomy service
	local handle = io.popen("/etc/init.d/autonomy restart 2>&1")
	local result = handle:read("*a")
	handle:close()

	if result:match("error") or result:match("failed") then
		return self:ResponseError(500, "Failed to restart autonomy service")
	end

	return self:ResponseOK({
		result = "Autonomy service restarted successfully",
		message = result
	})
end

-- POST /api/autonomy_f/actions/restart
local restart_action = Autonomy:action("restart", Autonomy.RestartAction)

return Autonomy





