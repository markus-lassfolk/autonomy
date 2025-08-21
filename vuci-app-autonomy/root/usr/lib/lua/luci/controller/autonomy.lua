module("luci.controller.autonomy", package.seeall)

function index()
    if not nixio.fs.access("/etc/config/autonomy") then
        return
    end

    local page = entry({"admin", "network", "autonomy"}, alias("admin", "network", "autonomy", "status"), _("Autonomy"), 60)
    page.dependent = true
    page.acl_depends = { "luci-app-autonomy" }

    entry({"admin", "network", "autonomy"}, alias("admin", "network", "autonomy", "status"), _("Autonomy"), 60).dependent = false
    entry({"admin", "network", "autonomy", "status"}, template("autonomy/status"), _("Status"), 10)
    entry({"admin", "network", "autonomy", "config"}, template("autonomy/config"), _("Configuration"), 20)
    entry({"admin", "network", "autonomy", "interfaces"}, template("autonomy/interfaces"), _("Interfaces"), 30)
    entry({"admin", "network", "autonomy", "telemetry"}, template("autonomy/telemetry"), _("Telemetry"), 40)
    entry({"admin", "network", "autonomy", "logs"}, template("autonomy/logs"), _("Logs"), 50)
    entry({"admin", "network", "autonomy", "monitoring"}, template("autonomy/monitoring"), _("Monitoring"), 60)
    
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
    entry({"admin", "network", "autonomy", "api", "historical_data"}, call("action_historical_data")).leaf = true
    entry({"admin", "network", "autonomy", "api", "alerts"}, call("action_alerts")).leaf = true
end

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
    local rpc = require "luci.http.protocol"
    local data = rpc.call("autonomy", "resources", {})
    
    luci.http.prepare_content("application/json")
    luci.http.write_json(data)
end

function action_monitoring()
    luci.template.render("autonomy/monitoring")
end

function action_historical_data()
    local rpc = require "luci.http.protocol"
    local data = rpc.call("autonomy", "historical_data", {})
    
    luci.http.prepare_content("application/json")
    luci.http.write_json(data)
end

function action_alerts()
    local rpc = require "luci.http.protocol"
    local data = rpc.call("autonomy", "alerts", {})
    
    luci.http.prepare_content("application/json")
    luci.http.write_json(data)
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

