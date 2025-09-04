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
ConfigAutonomy.default_options.id.maxlength = 32 -- Default id option can also have validations

function ConfigAutonomy:create_defaults(sid)
	-- Default values to be added with every creation
	return {
		enabled = "1",
		check_interval = "30",
		starlink_enabled = "1",
		cellular_enabled = "1",
		gps_enabled = "1"
	}
end

	local opt_enabled = ConfigAutonomy:option("enabled")
		opt_enabled.maxlength = 1
		function opt_enabled:validate(value)
			return self.dt:is_bool(value)
		end

	local opt_check_interval = ConfigAutonomy:option("check_interval")
		opt_check_interval.maxlength = 10
		function opt_check_interval:validate(value)
			local num = tonumber(value)
			return num and num >= 5 and num <= 3600
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
		function opt_log_level:validate(value)
			return self.dt:check_array(value, { "debug", "info", "warn", "error" })
		end

	local opt_failover_threshold = ConfigAutonomy:option("failover_threshold")
		opt_failover_threshold.maxlength = 10
		function opt_failover_threshold:validate(value)
			local num = tonumber(value)
			return num and num >= 1 and num <= 10
		end

return Autonomy






