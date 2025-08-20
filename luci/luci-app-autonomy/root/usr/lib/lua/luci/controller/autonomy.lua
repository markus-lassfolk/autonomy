--[[
LuCI - autonomy Multi-Interface Failover Controller
Copyright (c) 2024 autonomy Team
Licensed under GPL-3.0-or-later
--]]

local uci = require "luci.model.uci".cursor()
local sys = require "luci.sys"
local util = require "luci.util"
local json = require "luci.jsonc"

module("luci.controller.autonomy", package.seeall)

function index()
    -- Main menu entry
    entry({"admin", "network", "autonomy"}, alias("admin", "network", "autonomy", "overview"), _("autonomy Failover"), 60)
    
    -- Overview page
    entry({"admin", "network", "autonomy", "overview"}, template("autonomy/overview"), _("Overview"), 1)
    
    -- Configuration page
    entry({"admin", "network", "autonomy", "config"}, cbi("autonomy/config"), _("Configuration"), 2)
    
    -- Members page
    entry({"admin", "network", "autonomy", "members"}, template("autonomy/members"), _("Members"), 3)
    
    -- Telemetry page
    entry({"admin", "network", "autonomy", "telemetry"}, template("autonomy/telemetry"), _("Telemetry"), 4)
    
    -- Logs page
    entry({"admin", "network", "autonomy", "logs"}, template("autonomy/logs"), _("Logs"), 5)
    
    -- Notifications page
    entry({"admin", "network", "autonomy", "notifications"}, template("autonomy/notifications"), _("Notifications"), 6)
    
    -- Adaptive Sampling page
    entry({"admin", "network", "autonomy", "adaptive"}, template("autonomy/adaptive"), _("Adaptive Sampling"), 7)
    
    -- Rule Engine page
    entry({"admin", "network", "autonomy", "rules"}, template("autonomy/rules"), _("Rule Engine"), 8)
    
    -- Analytics Dashboard page
    entry({"admin", "network", "autonomy", "analytics"}, template("autonomy/analytics"), _("Analytics"), 9)
    
    -- AJAX endpoints for dynamic data
    entry({"admin", "network", "autonomy", "status"}, call("action_status")).leaf = true
    entry({"admin", "network", "autonomy", "members_data"}, call("action_members_data")).leaf = true
    entry({"admin", "network", "autonomy", "telemetry_data"}, call("action_telemetry_data")).leaf = true
    entry({"admin", "network", "autonomy", "logs_data"}, call("action_logs_data")).leaf = true
    entry({"admin", "network", "autonomy", "control"}, call("action_control")).leaf = true
    entry({"admin", "network", "autonomy", "notifications_data"}, call("action_notifications_data")).leaf = true
    entry({"admin", "network", "autonomy", "adaptive_data"}, call("action_adaptive_data")).leaf = true
    entry({"admin", "network", "autonomy", "rules_data"}, call("action_rules_data")).leaf = true
    entry({"admin", "network", "autonomy", "audit_decisions"}, call("action_audit_decisions")).leaf = true
    entry({"admin", "network", "autonomy", "audit_patterns"}, call("action_audit_patterns")).leaf = true
    entry({"admin", "network", "autonomy", "audit_root_cause"}, call("action_audit_root_cause")).leaf = true
    entry({"admin", "network", "autonomy", "audit_stats"}, call("action_audit_stats")).leaf = true
    entry({"admin", "network", "autonomy", "send_notification"}, call("action_send_notification")).leaf = true
    entry({"admin", "network", "autonomy", "test_notification"}, call("action_test_notification")).leaf = true
    entry({"admin", "network", "autonomy", "analytics_data"}, call("action_analytics_data")).leaf = true
    entry({"admin", "network", "autonomy", "member_analytics"}, call("action_member_analytics")).leaf = true
end

-- Get overall status from autonomyd
function action_status()
    local status = {
        daemon_running = false,
        current_member = nil,
        total_members = 0,
        active_members = 0,
        last_switch = nil,
        uptime = nil,
        errors = {},
        adaptive_sampling = {},
        notifications = {}
    }
    
    -- Check if daemon is running
    local pid = sys.process.list()["autonomyd"]
    status.daemon_running = pid ~= nil
    
    if status.daemon_running then
        -- Get status via ubus
        local result = util.exec("ubus call autonomy status 2>/dev/null")
        if result and result ~= "" then
            local data = json.parse(result)
            if data then
                status.current_member = data.current_member
                status.total_members = data.total_members or 0
                status.active_members = data.active_members or 0
                status.last_switch = data.last_switch
                status.uptime = data.uptime
            end
        end
        
        -- Get errors if any
        local errors = util.exec("ubus call autonomy errors 2>/dev/null")
        if errors and errors ~= "" then
            local error_data = json.parse(errors)
            if error_data and error_data.errors then
                status.errors = error_data.errors
            end
        end
        
        -- Get adaptive sampling status
        local adaptive = util.exec("ubus call autonomy adaptive_sampling 2>/dev/null")
        if adaptive and adaptive ~= "" then
            local adaptive_data = json.parse(adaptive)
            if adaptive_data then
                status.adaptive_sampling = adaptive_data
            end
        end
        
        -- Get notification status
        local notifications = util.exec("ubus call autonomy notifications status 2>/dev/null")
        if notifications and notifications ~= "" then
            local notification_data = json.parse(notifications)
            if notification_data then
                status.notifications = notification_data
            end
        end
    end
    
    luci.http.prepare_content("application/json")
    luci.http.write_json(status)
end

-- Get members data
function action_members_data()
    local members = {}
    
    -- Get members list via ubus
    local result = util.exec("ubus call autonomy members 2>/dev/null")
    if result and result ~= "" then
        local data = json.parse(result)
        if data and data.members then
            members = data.members
        end
    end
    
    luci.http.prepare_content("application/json")
    luci.http.write_json(members)
end

-- Get telemetry data
function action_telemetry_data()
    local telemetry = {
        samples = {},
        events = {},
        health = {}
    }
    
    -- Get telemetry data via ubus
    local result = util.exec("ubus call autonomy telemetry 2>/dev/null")
    if result and result ~= "" then
        local data = json.parse(result)
        if data then
            telemetry.samples = data.samples or {}
            telemetry.events = data.events or {}
            telemetry.health = data.health or {}
        end
    end
    
    luci.http.prepare_content("application/json")
    luci.http.write_json(telemetry)
end

-- Get logs data
function action_logs_data()
    local logs = {}
    
    -- Get recent logs from autonomyd
    local result = util.exec("logread | grep autonomyd | tail -50 2>/dev/null")
    if result and result ~= "" then
        for line in result:gmatch("[^\r\n]+") do
            table.insert(logs, line)
        end
    end
    
    luci.http.prepare_content("application/json")
    luci.http.write_json(logs)
end

-- Get notifications data
function action_notifications_data()
    local notifications = {
        status = {},
        history = {},
        stats = {}
    }
    
    -- Get notification status
    local status_result = util.exec("ubus call autonomy notifications status 2>/dev/null")
    if status_result and status_result ~= "" then
        local status_data = json.parse(status_result)
        if status_data then
            notifications.status = status_data
        end
    end
    
    -- Get notification history
    local history_result = util.exec("ubus call autonomy notifications history 2>/dev/null")
    if history_result and history_result ~= "" then
        local history_data = json.parse(history_result)
        if history_data and history_data.history then
            notifications.history = history_data.history
        end
    end
    
    -- Get notification statistics
    local stats_result = util.exec("ubus call autonomy notifications stats 2>/dev/null")
    if stats_result and stats_result ~= "" then
        local stats_data = json.parse(stats_result)
        if stats_data then
            notifications.stats = stats_data.statistics or {}
        end
    end
    
    luci.http.prepare_content("application/json")
    luci.http.write_json(notifications)
end

-- Get adaptive sampling data
function action_adaptive_data()
    local adaptive = {
        status = {},
        config = {},
        performance = {}
    }
    
    -- Get adaptive sampling status
    local status_result = util.exec("ubus call autonomy adaptive_sampling 2>/dev/null")
    if status_result and status_result ~= "" then
        local status_data = json.parse(status_result)
        if status_data then
            adaptive.status = status_data
        end
    end
    
    -- Get adaptive sampling configuration
    local config_result = util.exec("ubus call autonomy adaptive_config 2>/dev/null")
    if config_result and config_result ~= "" then
        local config_data = json.parse(config_result)
        if config_data then
            adaptive.config = config_data
        end
    end
    
    luci.http.prepare_content("application/json")
    luci.http.write_json(adaptive)
end

-- Get rules data
function action_rules_data()
    local rules = {
        rules = {},
        execution_history = {},
        stats = {}
    }
    
    -- Get rules list
    local rules_result = util.exec("ubus call autonomy rules list 2>/dev/null")
    if rules_result and rules_result ~= "" then
        local rules_data = json.parse(rules_result)
        if rules_data and rules_data.rules then
            rules.rules = rules_data.rules
        end
    end
    
    -- Get execution history
    local history_result = util.exec("ubus call autonomy rules history 2>/dev/null")
    if history_result and history_result ~= "" then
        local history_data = json.parse(history_result)
        if history_data and history_data.history then
            rules.execution_history = history_data.history
        end
    end
    
    luci.http.prepare_content("application/json")
    luci.http.write_json(rules)
end

-- Send notification
function action_send_notification()
    local title = luci.http.formvalue("title")
    local message = luci.http.formvalue("message")
    local priority = luci.http.formvalue("priority") or "0"
    local channel = luci.http.formvalue("channel") or "pushover"
    
    local response = { success = false, message = "" }
    
    if title and message then
        local cmd = string.format('ubus call autonomy notifications send \'{"title":"%s","message":"%s","priority":%s,"channel":"%s"}\' 2>&1', 
            title:gsub('"', '\\"'), message:gsub('"', '\\"'), priority, channel)
        local result = util.exec(cmd)
        
        if result and result ~= "" then
            local data = json.parse(result)
            if data then
                response.success = data.success or false
                response.message = data.message or result
            else
                response.message = result
            end
        else
            response.message = "Failed to send notification"
        end
    else
        response.message = "Title and message are required"
    end
    
    luci.http.prepare_content("application/json")
    luci.http.write_json(response)
end

-- Test notification
function action_test_notification()
    local channel = luci.http.formvalue("channel") or "pushover"
    local response = { success = false, message = "" }
    
    local cmd = string.format('ubus call autonomy notifications test \'{"channel":"%s"}\' 2>&1', channel)
    local result = util.exec(cmd)
    
    if result and result ~= "" then
        local data = json.parse(result)
        if data then
            response.success = data.success or false
            response.message = data.message or result
        else
            response.message = result
        end
    else
        response.message = "Failed to send test notification"
    end
    
    luci.http.prepare_content("application/json")
    luci.http.write_json(response)
end

-- Control actions (start/stop/restart/reload)
function action_control()
    local action = luci.http.formvalue("action")
    local response = { success = false, message = "" }
    
    if action == "start" then
        local result = util.exec("/etc/init.d/autonomy start 2>&1")
        response.success = result:match("Starting") ~= nil
        response.message = result
    elseif action == "stop" then
        local result = util.exec("/etc/init.d/autonomy stop 2>&1")
        response.success = result:match("Stopping") ~= nil
        response.message = result
    elseif action == "restart" then
        local result = util.exec("/etc/init.d/autonomy restart 2>&1")
        response.success = result:match("Restarting") ~= nil
        response.message = result
    elseif action == "reload" then
        local result = util.exec("ubus call autonomy reload 2>&1")
        response.success = result ~= "" and result:match("error") == nil
        response.message = result
    else
        response.message = "Invalid action"
    end
    
    luci.http.prepare_content("application/json")
    luci.http.write_json(response)
end

<<<<<<< HEAD
-- Get audit decisions data
function action_audit_decisions()
    local decisions = {
        decisions = {},
        count = 0,
        since = nil,
        limit = 100
    }
    
    -- Get audit decisions via ubus
    local result = util.exec("ubus call autonomy audit_decisions 2>/dev/null")
    if result and result ~= "" then
        local data = json.parse(result)
        if data then
            decisions.decisions = data.decisions or {}
            decisions.count = data.count or 0
            decisions.since = data.since
            decisions.limit = data.limit or 100
=======
-- Get analytics dashboard data
function action_analytics_data()
    local analytics = {}
    
    -- Get dashboard data
    local dashboard_result = util.exec("ubus call autonomy analytics dashboard 2>/dev/null")
    if dashboard_result and dashboard_result ~= "" then
        local dashboard_data = json.parse(dashboard_result)
        if dashboard_data then
            analytics = dashboard_data
>>>>>>> cursor
        end
    end
    
    luci.http.prepare_content("application/json")
<<<<<<< HEAD
    luci.http.write_json(decisions)
end

-- Get audit patterns data
function action_audit_patterns()
    local patterns = {
        patterns = {},
        count = 0
    }
    
    -- Get audit patterns via ubus
    local result = util.exec("ubus call autonomy audit_patterns 2>/dev/null")
    if result and result ~= "" then
        local data = json.parse(result)
        if data then
            patterns.patterns = data.patterns or {}
            patterns.count = data.count or 0
        end
    end
    
    luci.http.prepare_content("application/json")
    luci.http.write_json(patterns)
end

-- Get audit root cause data
function action_audit_root_cause()
    local rootCauses = {
        root_causes = {},
        count = 0
    }
    
    -- Get audit root cause analysis via ubus
    local result = util.exec("ubus call autonomy audit_root_cause 2>/dev/null")
    if result and result ~= "" then
        local data = json.parse(result)
        if data then
            rootCauses.root_causes = data.root_causes or {}
            rootCauses.count = data.count or 0
        end
    end
    
    luci.http.prepare_content("application/json")
    luci.http.write_json(rootCauses)
end

-- Get audit statistics
function action_audit_stats()
    local stats = {
        total_decisions = 0,
        successful_decisions = 0,
        failed_decisions = 0,
        average_confidence = 0.0,
        average_execution_time = 0,
        success_rate = 0.0,
        patterns_detected = 0,
        decision_types = {},
        triggers = {},
        root_causes = {}
    }
    
    -- Get audit statistics via ubus
    local result = util.exec("ubus call autonomy audit_stats 2>/dev/null")
    if result and result ~= "" then
        local data = json.parse(result)
        if data then
            stats.total_decisions = data.total_decisions or 0
            stats.successful_decisions = data.successful_decisions or 0
            stats.failed_decisions = data.failed_decisions or 0
            stats.average_confidence = data.average_confidence or 0.0
            stats.average_execution_time = data.average_execution_time or 0
            stats.success_rate = data.success_rate or 0.0
            stats.patterns_detected = data.patterns_detected or 0
            stats.decision_types = data.decision_types or {}
            stats.triggers = data.triggers or {}
            stats.root_causes = data.root_causes or {}
        end
    end
    
    luci.http.prepare_content("application/json")
    luci.http.write_json(stats)
=======
    luci.http.write_json(analytics)
end

-- Get member analytics
function action_member_analytics()
    local member = luci.http.formvalue("member")
    local hours = luci.http.formvalue("hours") or "24"
    local analytics = {}
    
    if member then
        local cmd = string.format('ubus call autonomy analytics member \'{"member":"%s","hours":"%s"}\' 2>/dev/null', member, hours)
        local result = util.exec(cmd)
        if result and result ~= "" then
            local data = json.parse(result)
            if data then
                analytics = data
            end
        end
    else
        analytics.error = "Member parameter is required"
    end
    
    luci.http.prepare_content("application/json")
    luci.http.write_json(analytics)
>>>>>>> cursor
end
