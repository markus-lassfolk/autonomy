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

o = s:option(Value, "poll_interval_ms", translate("Poll Interval (ms)"))
o.datatype = "uinteger"
o.default = "2000"
o.rmempty = false

o = s:option(Value, "min_uptime_s", translate("Minimum Uptime (seconds)"))
o.datatype = "uinteger"
o.default = "30"
o.rmempty = false

o = s:option(Value, "cooldown_s", translate("Cooldown Period (seconds)"))
o.datatype = "uinteger"
o.default = "30"
o.rmempty = false

-- Starlink section
s = m:section(TypedSection, "starlink", translate("Starlink Configuration"))
s.anonymous = true

o = s:option(Value, "host", translate("Starlink Host"))
o.default = "192.168.100.1"
o.rmempty = false

o = s:option(Value, "port", translate("Starlink Port"))
o.datatype = "port"
o.default = "9200"
o.rmempty = false

o = s:option(Value, "timeout_s", translate("Timeout (seconds)"))
o.datatype = "uinteger"
o.default = "5"
o.rmempty = false

o = s:option(Value, "health_threshold", translate("Health Threshold (%)"))
o.datatype = "uinteger"
o.default = "80"
o.rmempty = false

return m





