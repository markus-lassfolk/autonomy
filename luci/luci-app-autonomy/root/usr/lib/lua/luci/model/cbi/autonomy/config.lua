--[[
LuCI - autonomy Configuration Model
Copyright (c) 2024 autonomy Team
Licensed under GPL-3.0-or-later
--]]

local uci = require "luci.model.uci".cursor()

m = Map("autonomy", translate("autonomy Multi-Interface Failover Configuration"))

-- Main configuration section
s = m:section(TypedSection, "main", translate("Main Settings"))
s.anonymous = true

-- Enable/Disable
enable = s:option(Flag, "enable", translate("Enable autonomy"))
enable.default = "1"
enable.rmempty = false

-- Use mwan3 integration
use_mwan3 = s:option(Flag, "use_mwan3", translate("Use mwan3 Integration"))
use_mwan3.default = "1"
use_mwan3.description = translate("Enable integration with mwan3 for policy routing")

-- Poll interval
poll_interval = s:option(Value, "poll_interval_ms", translate("Poll Interval (ms)"))
poll_interval.default = "5000"
poll_interval.datatype = "uinteger"

-- Decision interval
decision_interval = s:option(Value, "decision_interval_ms", translate("Decision Interval (ms)"))
decision_interval.default = "5000"
decision_interval.datatype = "uinteger"

-- Log level
log_level = s:option(ListValue, "log_level", translate("Log Level"))
log_level:value("debug", translate("Debug"))
log_level:value("info", translate("Info"))
log_level:value("warn", translate("Warning"))
log_level:value("error", translate("Error"))
log_level.default = "info"

-- Switch margin
switch_margin = s:option(Value, "switch_margin", translate("Switch Margin"))
switch_margin.default = "10"
switch_margin.datatype = "uinteger"

-- Min uptime
min_uptime = s:option(Value, "min_uptime_s", translate("Minimum Uptime (seconds)"))
min_uptime.default = "30"
min_uptime.datatype = "uinteger"

-- Cooldown
cooldown = s:option(Value, "cooldown_s", translate("Cooldown Period (seconds)"))
cooldown.default = "60"
cooldown.datatype = "uinteger"

-- Metrics section
s2 = m:section(TypedSection, "main", translate("Monitoring"))
s2.anonymous = true

-- Metrics listener
metrics_listener = s2:option(Flag, "metrics_listener", translate("Enable Metrics Server"))
metrics_listener.default = "0"

-- Metrics port
metrics_port = s2:option(Value, "metrics_port", translate("Metrics Port"))
metrics_port.default = "9090"
metrics_port.datatype = "port"
metrics_port:depends("metrics_listener", "1")

-- Health listener
health_listener = s2:option(Flag, "health_listener", translate("Enable Health Server"))
health_listener.default = "1"

-- Health port
health_port = s2:option(Value, "health_port", translate("Health Port"))
health_port.default = "8080"
health_port.datatype = "port"
health_port:depends("health_listener", "1")

-- Adaptive Sampling section
s3 = m:section(TypedSection, "main", translate("Adaptive Sampling"))
s3.anonymous = true

-- Enable adaptive sampling
adaptive_sampling_enabled = s3:option(Flag, "adaptive_sampling_enabled", translate("Enable Adaptive Sampling"))
adaptive_sampling_enabled.default = "1"
adaptive_sampling_enabled.description = translate("Dynamically adjust sampling rates based on connection type and performance")

-- Base interval
adaptive_base_interval = s3:option(Value, "adaptive_base_interval_s", translate("Base Interval (seconds)"))
adaptive_base_interval.default = "5"
adaptive_base_interval.datatype = "uinteger"
adaptive_base_interval:depends("adaptive_sampling_enabled", "1")

-- Min interval
adaptive_min_interval = s3:option(Value, "adaptive_min_interval_s", translate("Minimum Interval (seconds)"))
adaptive_min_interval.default = "1"
adaptive_min_interval.datatype = "uinteger"
adaptive_min_interval:depends("adaptive_sampling_enabled", "1")

-- Max interval
adaptive_max_interval = s3:option(Value, "adaptive_max_interval_s", translate("Maximum Interval (seconds)"))
adaptive_max_interval.default = "120"
adaptive_max_interval.datatype = "uinteger"
adaptive_max_interval:depends("adaptive_sampling_enabled", "1")

-- Data limit threshold
adaptive_data_threshold = s3:option(Value, "adaptive_data_limit_threshold", translate("Data Limit Threshold (%)"))
adaptive_data_threshold.default = "80"
adaptive_data_threshold.datatype = "uinteger"
adaptive_data_threshold:depends("adaptive_sampling_enabled", "1")

-- Battery threshold
adaptive_battery_threshold = s3:option(Value, "adaptive_battery_threshold", translate("Battery Threshold (%)"))
adaptive_battery_threshold.default = "20"
adaptive_battery_threshold.datatype = "uinteger"
adaptive_battery_threshold:depends("adaptive_sampling_enabled", "1")

-- Notifications section
s4 = m:section(TypedSection, "main", translate("Notifications"))
s4.anonymous = true

-- Enable notifications
notifications_enabled = s4:option(Flag, "notifications_enabled", translate("Enable Notifications"))
notifications_enabled.default = "1"
notifications_enabled.description = translate("Enable multi-channel notification system")

-- Pushover settings
pushover_enabled = s4:option(Flag, "pushover_enabled", translate("Enable Pushover"))
pushover_enabled.default = "0"
pushover_enabled:depends("notifications_enabled", "1")

pushover_token = s4:option(Value, "pushover_token", translate("Pushover Token"))
pushover_token:depends("pushover_enabled", "1")

pushover_user = s4:option(Value, "pushover_user", translate("Pushover User Key"))
pushover_user:depends("pushover_enabled", "1")

pushover_device = s4:option(Value, "pushover_device", translate("Pushover Device"))
pushover_device:depends("pushover_enabled", "1")

-- Email settings
email_enabled = s4:option(Flag, "email_enabled", translate("Enable Email"))
email_enabled.default = "0"
email_enabled:depends("notifications_enabled", "1")

email_smtp_host = s4:option(Value, "email_smtp_host", translate("SMTP Host"))
email_smtp_host:depends("email_enabled", "1")

email_smtp_port = s4:option(Value, "email_smtp_port", translate("SMTP Port"))
email_smtp_port.default = "587"
email_smtp_port.datatype = "port"
email_smtp_port:depends("email_enabled", "1")

email_username = s4:option(Value, "email_username", translate("Email Username"))
email_username:depends("email_enabled", "1")

email_password = s4:option(Value, "email_password", translate("Email Password"))
email_password.password = true
email_password:depends("email_enabled", "1")

email_from = s4:option(Value, "email_from", translate("From Email"))
email_from:depends("email_enabled", "1")

email_to = s4:option(Value, "email_to", translate("To Email (comma-separated)"))
email_to:depends("email_enabled", "1")

email_use_tls = s4:option(Flag, "email_use_tls", translate("Use TLS"))
email_use_tls.default = "1"
email_use_tls:depends("email_enabled", "1")

email_use_starttls = s4:option(Flag, "email_use_starttls", translate("Use STARTTLS"))
email_use_starttls.default = "1"
email_use_starttls:depends("email_enabled", "1")

-- Slack settings
slack_enabled = s4:option(Flag, "slack_enabled", translate("Enable Slack"))
slack_enabled.default = "0"
slack_enabled:depends("notifications_enabled", "1")

slack_webhook_url = s4:option(Value, "slack_webhook_url", translate("Slack Webhook URL"))
slack_webhook_url:depends("slack_enabled", "1")

slack_channel = s4:option(Value, "slack_channel", translate("Slack Channel"))
slack_channel:depends("slack_enabled", "1")

slack_username = s4:option(Value, "slack_username", translate("Slack Username"))
slack_username:depends("slack_enabled", "1")

-- Discord settings
discord_enabled = s4:option(Flag, "discord_enabled", translate("Enable Discord"))
discord_enabled.default = "0"
discord_enabled:depends("notifications_enabled", "1")

discord_webhook_url = s4:option(Value, "discord_webhook_url", translate("Discord Webhook URL"))
discord_webhook_url:depends("discord_enabled", "1")

discord_username = s4:option(Value, "discord_username", translate("Discord Username"))
discord_username:depends("discord_enabled", "1")

-- Telegram settings
telegram_enabled = s4:option(Flag, "telegram_enabled", translate("Enable Telegram"))
telegram_enabled.default = "0"
telegram_enabled:depends("notifications_enabled", "1")

telegram_token = s4:option(Value, "telegram_token", translate("Telegram Bot Token"))
telegram_token:depends("telegram_enabled", "1")

telegram_chat_id = s4:option(Value, "telegram_chat_id", translate("Telegram Chat ID"))
telegram_chat_id:depends("telegram_enabled", "1")

-- Webhook settings
webhook_enabled = s4:option(Flag, "webhook_enabled", translate("Enable Webhook"))
webhook_enabled.default = "0"
webhook_enabled:depends("notifications_enabled", "1")

webhook_url = s4:option(Value, "webhook_url", translate("Webhook URL"))
webhook_url:depends("webhook_enabled", "1")

webhook_method = s4:option(ListValue, "webhook_method", translate("HTTP Method"))
webhook_method:value("POST", "POST")
webhook_method:value("PUT", "PUT")
webhook_method:value("PATCH", "PATCH")
webhook_method.default = "POST"
webhook_method:depends("webhook_enabled", "1")

webhook_content_type = s4:option(Value, "webhook_content_type", translate("Content Type"))
webhook_content_type.default = "application/json"
webhook_content_type:depends("webhook_enabled", "1")

-- Rule Engine section
s5 = m:section(TypedSection, "main", translate("Rule Engine"))
s5.anonymous = true

-- Enable rule engine
rule_engine_enabled = s5:option(Flag, "rule_engine_enabled", translate("Enable Rule Engine"))
rule_engine_enabled.default = "1"
rule_engine_enabled.description = translate("Enable automated rule-based decision making")

-- Max rules
rule_engine_max_rules = s5:option(Value, "rule_engine_max_rules", translate("Maximum Rules"))
rule_engine_max_rules.default = "50"
rule_engine_max_rules.datatype = "uinteger"
rule_engine_max_rules:depends("rule_engine_enabled", "1")

-- Execution timeout
rule_engine_timeout = s5:option(Value, "rule_engine_timeout_s", translate("Execution Timeout (seconds)"))
rule_engine_timeout.default = "30"
rule_engine_timeout.datatype = "uinteger"
rule_engine_timeout:depends("rule_engine_enabled", "1")

-- Max history
rule_engine_max_history = s5:option(Value, "rule_engine_max_history", translate("Maximum History Entries"))
rule_engine_max_history.default = "1000"
rule_engine_max_history.datatype = "uinteger"
rule_engine_max_history:depends("rule_engine_enabled", "1")

return m
