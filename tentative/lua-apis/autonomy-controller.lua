module("luci.controller.admin.autonomy", package.seeall)

function index()
    if not nixio.fs.access("/etc/config/autonomy") then
        return
    end

    local page = entry({"admin", "autonomy"}, alias("admin", "autonomy", "overview"), _("Autonomy"), 60)
    page.dependent = true
    page.acl_depends = { "luci-app-autonomy" }

    entry({"admin", "autonomy", "overview"}, template("admin_autonomy/overview"), _("Overview"), 1)
    entry({"admin", "autonomy", "status"}, call("action_status")).leaf = true
    entry({"admin", "autonomy", "config"}, cbi("admin_autonomy/config"), _("Configuration"), 2)
    entry({"admin", "autonomy", "logs"}, template("admin_autonomy/logs"), _("Logs"), 3)
    entry({"admin", "autonomy", "starlink"}, template("admin_autonomy/starlink"), _("Starlink"), 4)
end

function action_status()
    local sys = require "luci.sys"
    local status = {
        running = false,
        starlink = {},
        system = {}
    }

    -- Check if autonomy daemon is running
    status.running = sys.process.list()["autonomyd"] ~= nil

    -- Get Starlink status (if available)
    local starlink_status = sys.exec("/usr/local/bin/autonomysysmgmt -check -dry-run 2>/dev/null | grep -E 'latency_ms|obstruction_pct|uptime_hours' | tail -3")
    if starlink_status and starlink_status ~= "" then
        status.starlink = starlink_status
    end

    -- Get system status
    local system_status = sys.exec("/usr/local/bin/autonomysysmgmt -check -dry-run 2>/dev/null | grep -E 'issues_found|duration' | tail -2")
    if system_status and system_status ~= "" then
        status.system = system_status
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(status)
end






