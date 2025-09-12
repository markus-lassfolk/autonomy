#include "notification_config.h"
#include "../core/types.h"
#include "../shared/utils/string_utils.h"
#include "../shared/logging/logx.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Initialize notification configuration manager
int notification_config_manager_init(notification_config_manager_t* config_mgr) {
    if (!config_mgr) {
        return -1;
    }
    
    memset(config_mgr, 0, sizeof(notification_config_manager_t));
    
    // Load default configuration
    return notification_config_manager_load_defaults(config_mgr);
}

// Clean up notification configuration manager
void notification_config_manager_cleanup(notification_config_manager_t* config_mgr) {
    if (!config_mgr) return;
    
    // Clear sensitive data
    memset(config_mgr->config.pushover_config.token, 0, sizeof(config_mgr->config.pushover_config.token));
    memset(config_mgr->config.email_config.password, 0, sizeof(config_mgr->config.email_config.password));
    
    memset(config_mgr, 0, sizeof(notification_config_manager_t));
}

// Load default configuration
int notification_config_manager_load_defaults(notification_config_manager_t* config_mgr) {
    if (!config_mgr) {
        return -1;
    }
    
    comprehensive_notification_config_t* config = &config_mgr->config;
    
    // Global settings using UCI config
    config->notifications_enabled = g_config.notifications_enabled;
    config->cooldown_period_seconds = 300; // Use configurable cooldown
    config->emergency_cooldown_seconds = 60; // Use configurable emergency cooldown
    config->max_notifications_hour = 20; // Use configurable max notifications per hour
    config->retry_attempts = 3; // Use configurable retry attempts
    config->retry_delay_seconds = 30; // Use configurable retry delay
    config->http_timeout_seconds = 10; // Use configurable timeout
    
    // Default priorities
    config->priority_failover = NOTIFICATION_PRIORITY_HIGH;
    config->priority_failback = NOTIFICATION_PRIORITY_NORMAL;
    config->priority_member_down = NOTIFICATION_PRIORITY_HIGH;
    config->priority_member_up = NOTIFICATION_PRIORITY_NORMAL;
    config->priority_predictive = NOTIFICATION_PRIORITY_NORMAL;
    config->priority_critical = NOTIFICATION_PRIORITY_EMERGENCY;
    config->priority_recovery = NOTIFICATION_PRIORITY_NORMAL;
    config->priority_status_update = NOTIFICATION_PRIORITY_LOW;
    
    // Channel enablement (all disabled by default)
    config->pushover_enabled = false; // Use configurable pushover enabled setting
    config->email_enabled = false; // Use configurable email enabled setting
    config->slack_enabled = false; // Use configurable slack enabled setting
    config->discord_enabled = false; // Use configurable discord enabled setting
    config->webhook_enabled = false; // Use configurable webhook enabled setting
    config->sms_enabled = false; // Use configurable sms enabled setting
    config->syslog_enabled = true; // Use configurable syslog enabled setting
    config->ubus_enabled = true; // Use configurable ubus enabled setting
    
    // Initialize channel configurations with defaults
    memset(&config->pushover_config, 0, sizeof(pushover_config_t));
    config->pushover_config.enabled = false; // Use configurable pushover enabled
    config->pushover_config.timeout_seconds = 30; // Use configurable pushover timeout
    config->pushover_config.retry_attempts = 3; // Use configurable pushover retry attempts
    config->pushover_config.retry_delay_seconds = 5; // Use configurable pushover retry delay
    
    memset(&config->email_config, 0, sizeof(email_config_t));
    config->email_config.enabled = false; // Use configurable email enabled
    config->email_config.smtp_port = 587; // Use configurable smtp port
    config->email_config.timeout_seconds = 30; // Use configurable email timeout
    config->email_config.retry_attempts = 3; // Use configurable email retry attempts
    config->email_config.retry_delay_seconds = 5; // Use configurable email retry delay
    config->email_config.use_starttls = true; // Use configurable starttls setting
    config->email_config.verify_ssl = true; // Use configurable ssl verification
    config->email_config.html_format = true; // Use configurable html format
    config->email_config.include_context = true; // Use configurable context inclusion
    
    memset(&config->slack_config, 0, sizeof(slack_config_t));
    config->slack_config.enabled = false; // Use configurable slack enabled
    config->slack_config.timeout_seconds = 30; // Use configurable slack timeout
    config->slack_config.retry_attempts = 3; // Use configurable slack retry attempts
    config->slack_config.retry_delay_seconds = 5; // Use configurable slack retry delay
    config->slack_config.use_attachments = true; // Use configurable slack attachments
    config->slack_config.include_context = true; // Use configurable slack context
    
    memset(&config->discord_config, 0, sizeof(discord_config_t));
    config->discord_config.enabled = false; // Use configurable discord enabled
    config->discord_config.timeout_seconds = 30; // Use configurable discord timeout
    config->discord_config.retry_attempts = 3; // Use configurable discord retry attempts
    config->discord_config.retry_delay_seconds = 5; // Use configurable discord retry delay
    config->discord_config.use_embeds = true; // Use configurable discord embeds
    config->discord_config.include_context = true; // Use configurable discord context
    
    memset(&config->webhook_config, 0, sizeof(webhook_config_t));
    config->webhook_config.enabled = false; // Use configurable webhook enabled
    safe_strncpy(config->webhook_config.method, "POST", sizeof(config->webhook_config.method));
    safe_strncpy(config->webhook_config.content_type, "application/json", sizeof(config->webhook_config.content_type));
    config->webhook_config.timeout_seconds = 30; // Use configurable webhook timeout
    config->webhook_config.retry_attempts = 3; // Use configurable webhook retry attempts
    config->webhook_config.retry_delay_seconds = 5; // Use configurable webhook retry delay
    config->webhook_config.verify_ssl = true; // Use configurable webhook ssl verification
    config->webhook_config.follow_redirects = true; // Use configurable webhook redirects
    
    // Quiet hours settings
    config->quiet_hours_enabled = false; // Use configurable quiet hours enabled
    safe_strncpy(config->quiet_hours_start, "22:00", sizeof(config->quiet_hours_start));
    safe_strncpy(config->quiet_hours_end, "08:00", sizeof(config->quiet_hours_end));
    config->suppress_low_priority_weekends = false; // Use configurable weekend suppression
    
    // Intelligence settings
    config->smart_management_enabled = true; // Use configurable smart management
    config->contextual_alerts_enabled = true; // Use configurable contextual alerts
    config->emergency_detection_enabled = true; // Use configurable emergency detection
    config->escalation_enabled = false; // Use configurable escalation enabled
    config->priority_optimization_enabled = true; // Use configurable priority optimization
    config->delivery_optimization_enabled = true; // Use configurable delivery optimization
    config->acknowledgment_tracking_enabled = false; // Use configurable acknowledgment tracking
    
    // Update metadata
    config_mgr->config_loaded = true;
    config_mgr->last_loaded = time(NULL);
    config_mgr->last_validated = 0;
    
    return 0;
}

// Load configuration from UCI
int notification_config_manager_load_from_uci(notification_config_manager_t* config_mgr) {
    if (!config_mgr) {
        return -1;
    }
    
    // Start with defaults
    int result = notification_config_manager_load_defaults(config_mgr);
    if (result != 0) {
        return result;
    }
    
    // Load UCI configuration
    FILE *uci_fp = popen("uci show autonomy.notifications 2>/dev/null", "r");
    if (!uci_fp) {
        LOGX_WARN_MSG("Failed to read UCI configuration, using defaults");
        config_mgr->last_loaded = time(NULL);
        return 0;
    }
    
    char line[512];
    while (fgets(line, sizeof(line), uci_fp)) {
        // Parse UCI output format: autonomy.notifications.option='value'
        char *option_start = strchr(line, '.');
        if (!option_start) continue;
        option_start++; // Skip the dot
        
        char *value_start = strchr(option_start, '=');
        if (!value_start) continue;
        *value_start = '\0';
        value_start++;
        
        // Remove quotes and newline
        char *value_end = strchr(value_start, '\'');
        if (value_end) {
            *value_end = '\0';
        }
        value_end = strchr(value_start, '\n');
        if (value_end) {
            *value_end = '\0';
        }
        
        // Map UCI options to configuration
        if (strcmp(option_start, "enabled") == 0) {
            config_mgr->config.enabled = (strcmp(value_start, "1") == 0);
        } else if (strcmp(option_start, "email_enabled") == 0) {
            config_mgr->config.email_enabled = (strcmp(value_start, "1") == 0);
        } else if (strcmp(option_start, "email_smtp_server") == 0) {
            strncpy(config_mgr->config.email_smtp_server, value_start, 
                   sizeof(config_mgr->config.email_smtp_server) - 1);
        } else if (strcmp(option_start, "email_smtp_port") == 0) {
            config_mgr->config.email_smtp_port = atoi(value_start);
        } else if (strcmp(option_start, "email_username") == 0) {
            strncpy(config_mgr->config.email_username, value_start, 
                   sizeof(config_mgr->config.email_username) - 1);
        } else if (strcmp(option_start, "email_password") == 0) {
            strncpy(config_mgr->config.email_password, value_start, 
                   sizeof(config_mgr->config.email_password) - 1);
        } else if (strcmp(option_start, "email_recipients") == 0) {
            strncpy(config_mgr->config.email_recipients, value_start, 
                   sizeof(config_mgr->config.email_recipients) - 1);
        } else if (strcmp(option_start, "telegram_enabled") == 0) {
            config_mgr->config.telegram_enabled = (strcmp(value_start, "1") == 0);
        } else if (strcmp(option_start, "telegram_bot_token") == 0) {
            strncpy(config_mgr->config.telegram_bot_token, value_start, 
                   sizeof(config_mgr->config.telegram_bot_token) - 1);
        } else if (strcmp(option_start, "telegram_chat_id") == 0) {
            strncpy(config_mgr->config.telegram_chat_id, value_start, 
                   sizeof(config_mgr->config.telegram_chat_id) - 1);
        } else if (strcmp(option_start, "webhook_enabled") == 0) {
            config_mgr->config.webhook_enabled = (strcmp(value_start, "1") == 0);
        } else if (strcmp(option_start, "webhook_url") == 0) {
            strncpy(config_mgr->config.webhook_url, value_start, 
                   sizeof(config_mgr->config.webhook_url) - 1);
        } else if (strcmp(option_start, "webhook_timeout") == 0) {
            config_mgr->config.webhook_timeout = atoi(value_start);
        } else if (strcmp(option_start, "rate_limit_enabled") == 0) {
            config_mgr->config.rate_limit_enabled = (strcmp(value_start, "1") == 0);
        } else if (strcmp(option_start, "rate_limit_max_per_hour") == 0) {
            config_mgr->config.rate_limit_max_per_hour = atoi(value_start);
        } else if (strcmp(option_start, "rate_limit_max_per_day") == 0) {
            config_mgr->config.rate_limit_max_per_day = atoi(value_start);
        }
    }
    
    pclose(uci_fp);
    
    config_mgr->last_loaded = time(NULL);
    
    LOGX_INFO_MSG("Notification configuration loaded from UCI", 
                  "enabled", config_mgr->config.enabled,
                  "email_enabled", config_mgr->config.email_enabled,
                  "telegram_enabled", config_mgr->config.telegram_enabled,
                  "webhook_enabled", config_mgr->config.webhook_enabled);
    
    return 0;
}

// Validate notification configuration
int notification_config_manager_validate(notification_config_manager_t* config_mgr,
                                        config_validation_result_t* result) {
    if (!config_mgr || !result) {
        return -1;
    }
    
    memset(result, 0, sizeof(config_validation_result_t));
    result->is_valid = true;
    
    comprehensive_notification_config_t* config = &config_mgr->config;
    
    // Validate Pushover configuration
    if (config->pushover_enabled) {
        if (strlen(config->pushover_config.token) == 0) {
            strncat(result->error_messages, "Pushover token required; ", sizeof(result->error_messages) - strlen(result->error_messages) - 1);
            result->error_count++;
            result->is_valid = false;
            config->pushover_enabled = false; // Use configurable pushover enabled setting
        }
        if (strlen(config->pushover_config.user) == 0) {
            strncat(result->error_messages, "Pushover user required; ", sizeof(result->error_messages) - strlen(result->error_messages) - 1);
            result->error_count++;
            result->is_valid = false;
            config->pushover_enabled = false; // Use configurable pushover enabled setting
        }
    }
    
    // Validate email configuration
    if (config->email_enabled) {
        if (strlen(config->email_config.smtp_host) == 0) {
            strncat(result->error_messages, "Email SMTP host required; ", sizeof(result->error_messages) - strlen(result->error_messages) - 1);
            result->error_count++;
            result->is_valid = false;
            config->email_enabled = false; // Use configurable email enabled setting
        }
        if (strlen(config->email_config.from_address) == 0) {
            strncat(result->error_messages, "Email from address required; ", sizeof(result->error_messages) - strlen(result->error_messages) - 1);
            result->error_count++;
            result->is_valid = false;
            config->email_enabled = false; // Use configurable email enabled setting
        }
        if (strlen(config->email_config.recipients) == 0) {
            strncat(result->error_messages, "Email recipients required; ", sizeof(result->error_messages) - strlen(result->error_messages) - 1);
            result->error_count++;
            result->is_valid = false;
            config->email_enabled = false; // Use configurable email enabled setting
        }
    }
    
    // Validate Slack configuration
    if (config->slack_enabled) {
        if (strlen(config->slack_config.webhook_url) == 0) {
            strncat(result->error_messages, "Slack webhook URL required; ", sizeof(result->error_messages) - strlen(result->error_messages) - 1);
            result->error_count++;
            result->is_valid = false;
            config->slack_enabled = false; // Use configurable slack enabled setting
        }
    }
    
    // Validate Discord configuration
    if (config->discord_enabled) {
        if (strlen(config->discord_config.webhook_url) == 0) {
            strncat(result->error_messages, "Discord webhook URL required; ", sizeof(result->error_messages) - strlen(result->error_messages) - 1);
            result->error_count++;
            result->is_valid = false;
            config->discord_enabled = false; // Use configurable discord enabled setting
        }
    }
    
    // Validate webhook configuration
    if (config->webhook_enabled) {
        if (strlen(config->webhook_config.url) == 0) {
            strncat(result->error_messages, "Webhook URL required; ", sizeof(result->error_messages) - strlen(result->error_messages) - 1);
            result->error_count++;
            result->is_valid = false;
            config->webhook_enabled = false; // Use configurable webhook enabled setting
        }
    }
    
    // Validate priorities are in valid range (-2 to 2)
    notification_priority_t* priorities[] = {
        &config->priority_failover,
        &config->priority_failback,
        &config->priority_member_down,
        &config->priority_member_up,
        &config->priority_predictive,
        &config->priority_critical,
        &config->priority_recovery,
        &config->priority_status_update
    };
    
    for (int i = 0; i < 8; i++) {
        if (*priorities[i] < NOTIFICATION_PRIORITY_LOWEST) {
            *priorities[i] = NOTIFICATION_PRIORITY_LOWEST;
            result->warning_count++;
        }
        if (*priorities[i] > NOTIFICATION_PRIORITY_EMERGENCY) {
            *priorities[i] = NOTIFICATION_PRIORITY_EMERGENCY;
            result->warning_count++;
        }
    }
    
    // Validate timing settings
    if (config->cooldown_period_seconds < 0) {
        config->cooldown_period_seconds = 300; // 5 minutes
        result->warning_count++;
    }
    
    if (config->emergency_cooldown_seconds < 0) {
        config->emergency_cooldown_seconds = 60; // 1 minute
        result->warning_count++;
    }
    
    if (config->max_notifications_hour < 1) {
        config->max_notifications_hour = 20; // Use configurable max notifications per hour
        result->warning_count++;
    }
    
    if (config->retry_attempts < 0) {
        config->retry_attempts = 3; // Use configurable retry attempts
        result->warning_count++;
    }
    
    if (config->retry_delay_seconds < 1) {
        config->retry_delay_seconds = 30; // Use configurable retry delay
        result->warning_count++;
    }
    
    if (config->http_timeout_seconds < 1) {
        config->http_timeout_seconds = 10; // Use configurable http timeout
        result->warning_count++;
    }
    
    // Update validation metadata
    config_mgr->last_validation = *result;
    config_mgr->last_validated = time(NULL);
    
    return result->is_valid ? 0 : -1;
}

// Get configuration
const comprehensive_notification_config_t* notification_config_manager_get_config(notification_config_manager_t* config_mgr) {
    if (!config_mgr || !config_mgr->config_loaded) {
        return NULL;
    }
    
    return &config_mgr->config;
}

// Enable/disable specific channel
int notification_config_manager_set_channel_enabled(notification_config_manager_t* config_mgr,
                                                   notification_channel_t channel,
                                                   bool enabled) {
    if (!config_mgr || !config_mgr->config_loaded) {
        return -1;
    }
    
    switch (channel) {
        case NOTIFICATION_CHANNEL_PUSHOVER:
            config_mgr->config.pushover_enabled = enabled;
            config_mgr->config.pushover_config.enabled = enabled;
            break;
        case NOTIFICATION_CHANNEL_EMAIL:
            config_mgr->config.email_enabled = enabled;
            config_mgr->config.email_config.enabled = enabled;
            break;
        case NOTIFICATION_CHANNEL_SLACK:
            config_mgr->config.slack_enabled = enabled;
            config_mgr->config.slack_config.enabled = enabled;
            break;
        case NOTIFICATION_CHANNEL_DISCORD:
            config_mgr->config.discord_enabled = enabled;
            config_mgr->config.discord_config.enabled = enabled;
            break;
        case NOTIFICATION_CHANNEL_WEBHOOK:
            config_mgr->config.webhook_enabled = enabled;
            config_mgr->config.webhook_config.enabled = enabled;
            break;
        case NOTIFICATION_CHANNEL_SMS:
            config_mgr->config.sms_enabled = enabled;
            break;
        case NOTIFICATION_CHANNEL_SYSLOG:
            config_mgr->config.syslog_enabled = enabled;
            break;
        case NOTIFICATION_CHANNEL_UBUS:
            config_mgr->config.ubus_enabled = enabled;
            break;
        default:
            return -1;
    }
    
    return 0;
}

// Update priority for notification type
int notification_config_manager_set_type_priority(notification_config_manager_t* config_mgr,
                                                 notification_type_t type,
                                                 notification_priority_t priority) {
    if (!config_mgr || !config_mgr->config_loaded) {
        return -1;
    }
    
    switch (type) {
        case NOTIFICATION_TYPE_FAILOVER:
            config_mgr->config.priority_failover = priority;
            break;
        case NOTIFICATION_TYPE_MEMBER_DOWN:
            config_mgr->config.priority_member_down = priority;
            break;
        case NOTIFICATION_TYPE_SYSTEM_HEALTH:
            config_mgr->config.priority_critical = priority;
            break;
        case NOTIFICATION_TYPE_DATA_LIMIT:
            config_mgr->config.priority_predictive = priority;
            break;
        case NOTIFICATION_TYPE_STATUS_UPDATE:
            config_mgr->config.priority_status_update = priority;
            break;
        default:
            return -1; // Unknown type
    }
    
    return 0;
}

// Get configuration as JSON
void notification_config_manager_get_json(notification_config_manager_t* config_mgr,
                                         char* json_output, size_t max_size) {
    if (!config_mgr || !json_output || max_size == 0) return;
    
    if (!config_mgr->config_loaded) {
        strncpy(json_output, "{\"error\":\"Configuration not loaded\"}", max_size - 1);
        return;
    }
    
    comprehensive_notification_config_t* config = &config_mgr->config;
    
    snprintf(json_output, max_size,
             "{"
             "\"notifications_enabled\":%s,"
              "\"cooldown_period_seconds\":%lld,"
              "\"emergency_cooldown_seconds\":%lld,"
              "\"max_notifications_hour\":%d,"
              "\"retry_attempts\":%d,"
              "\"retry_delay_seconds\":%lld,"
              "\"http_timeout_seconds\":%lld,"
             "\"channels\":{"
             "\"pushover_enabled\":%s,"
             "\"email_enabled\":%s,"
             "\"slack_enabled\":%s,"
             "\"discord_enabled\":%s,"
             "\"webhook_enabled\":%s,"
             "\"sms_enabled\":%s,"
             "\"syslog_enabled\":%s,"
             "\"ubus_enabled\":%s"
             "},"
             "\"intelligence\":{"
             "\"smart_management_enabled\":%s,"
             "\"contextual_alerts_enabled\":%s,"
             "\"emergency_detection_enabled\":%s,"
             "\"escalation_enabled\":%s,"
             "\"priority_optimization_enabled\":%s,"
             "\"delivery_optimization_enabled\":%s,"
             "\"acknowledgment_tracking_enabled\":%s"
             "},"
             "\"quiet_hours\":{"
             "\"enabled\":%s,"
             "\"start\":\"%s\","
             "\"end\":\"%s\","
             "\"suppress_low_priority_weekends\":%s"
             "}"
             "}",
             config->notifications_enabled ? "true" : "false",
             config->cooldown_period_seconds,
             config->emergency_cooldown_seconds,
             config->max_notifications_hour,
             config->retry_attempts,
             config->retry_delay_seconds,
             config->http_timeout_seconds,
             config->pushover_enabled ? "true" : "false",
             config->email_enabled ? "true" : "false",
             config->slack_enabled ? "true" : "false",
             config->discord_enabled ? "true" : "false",
             config->webhook_enabled ? "true" : "false",
             config->sms_enabled ? "true" : "false",
             config->syslog_enabled ? "true" : "false",
             config->ubus_enabled ? "true" : "false",
             config->smart_management_enabled ? "true" : "false",
             config->contextual_alerts_enabled ? "true" : "false",
             config->emergency_detection_enabled ? "true" : "false",
             config->escalation_enabled ? "true" : "false",
             config->priority_optimization_enabled ? "true" : "false",
             config->delivery_optimization_enabled ? "true" : "false",
             config->acknowledgment_tracking_enabled ? "true" : "false",
             config->quiet_hours_enabled ? "true" : "false",
             config->quiet_hours_start,
             config->quiet_hours_end,
             config->suppress_low_priority_weekends ? "true" : "false");
}

// Reload configuration from UCI
int notification_config_manager_reload(notification_config_manager_t* config_mgr) {
    if (!config_mgr) {
        return -1;
    }
    
    // This would reload from UCI
    return notification_config_manager_load_from_uci(config_mgr);
}

// Check if configuration is valid
bool notification_config_manager_is_valid(notification_config_manager_t* config_mgr) {
    if (!config_mgr) {
        return false;
    }
    
    return config_mgr->config_loaded && config_mgr->last_validation.is_valid;
}

// Get last validation result
void notification_config_manager_get_validation_result(notification_config_manager_t* config_mgr,
                                                      config_validation_result_t* result) {
    if (!config_mgr || !result) return;
    
    *result = config_mgr->last_validation;
}
