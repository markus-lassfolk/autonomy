# 🔗 Advanced Webhook Integration Guide

## 🎯 Overview

The Enhanced autonomy Notification System includes a powerful generic webhook client that makes it easy to integrate with any custom system, monitoring platform, or third-party service. This guide provides comprehensive examples for common integration scenarios.

## 🏗️ Webhook Configuration Structure

```json
{
  "webhook": {
    "enabled": true,
    "url": "https://your-service.com/webhook",
    "method": "POST",
    "content_type": "application/json",
    
    // Advanced Configuration
    "template": "{{.Title}}: {{.Message}}",
    "template_format": "json",
    "auth_type": "bearer",
    "auth_token": "your-token-here",
    "timeout": 30,
    "retry_attempts": 3,
    "retry_delay": 5,
    "verify_ssl": true,
    "follow_redirects": true,
    
    // Filtering & Transformation
    "priority_filter": [1, 2],
    "type_filter": ["failover", "critical_error"],
    "field_mapping": {"Title": "alert_title", "Message": "description"},
    "exclude_fields": ["source", "version"],
    "include_raw_data": false,
    
    // Metadata
    "name": "My Custom Integration",
    "description": "Integration with monitoring system",
    "tags": ["monitoring", "alerts"]
  }
}
```

## 🔧 Integration Examples

### 1. 📊 **Grafana Webhook Integration**

```json
{
  "webhook": {
    "enabled": true,
    "name": "Grafana Alerts",
    "url": "https://grafana.example.com/api/webhooks/autonomy",
    "method": "POST",
    "content_type": "application/json",
    "auth_type": "bearer",
    "auth_token": "glsa_your_grafana_token_here",
    "template": "{\"title\": \"{{.Title}}\", \"message\": \"{{.Message}}\", \"priority\": {{.Priority}}, \"timestamp\": \"{{.Timestamp.Format \"2006-01-02T15:04:05Z\"}}\", \"tags\": [\"autonomy\", \"{{.Type}}\"], \"source\": \"autonomy-daemon\"}",
    "priority_filter": [1, 2],
    "retry_attempts": 3
  }
}
```

### 2. 🚨 **PagerDuty Integration**

```json
{
  "webhook": {
    "enabled": true,
    "name": "PagerDuty Events",
    "url": "https://events.pagerduty.com/v2/enqueue",
    "method": "POST",
    "content_type": "application/json",
    "headers": {
      "Authorization": "Token token=your-integration-key"
    },
    "template": "{\"routing_key\": \"your-integration-key\", \"event_action\": \"trigger\", \"payload\": {\"summary\": \"{{.Title}}\", \"source\": \"autonomy\", \"severity\": \"{{if eq .Priority 2}}critical{{else if eq .Priority 1}}warning{{else}}info{{end}}\", \"component\": \"network\", \"group\": \"connectivity\", \"class\": \"{{.Type}}\", \"custom_details\": {{.Context | toJson}}}}",
    "priority_filter": [1, 2],
    "type_filter": ["failover", "critical_error", "member_down"]
  }
}
```

### 3. 📈 **Datadog Integration**

```json
{
  "webhook": {
    "enabled": true,
    "name": "Datadog Events",
    "url": "https://api.datadoghq.com/api/v1/events",
    "method": "POST",
    "content_type": "application/json",
    "headers": {
      "DD-API-KEY": "your-datadog-api-key"
    },
    "template": "{\"title\": \"{{.Title}}\", \"text\": \"{{.Message}}\", \"priority\": \"{{if eq .Priority 2}}high{{else}}normal{{end}}\", \"tags\": [\"source:autonomy\", \"type:{{.Type}}\"], \"alert_type\": \"{{if eq .Priority 2}}error{{else if eq .Priority 1}}warning{{else}}info{{end}}\", \"source_type_name\": \"autonomy\"}",
    "field_mapping": {
      "Type": "event_type",
      "Priority": "alert_priority"
    }
  }
}
```

### 4. 🔔 **Microsoft Teams Integration**

```json
{
  "webhook": {
    "enabled": true,
    "name": "Microsoft Teams",
    "url": "https://outlook.office.com/webhook/your-teams-webhook-url",
    "method": "POST",
    "content_type": "application/json",
    "template": "{\"@type\": \"MessageCard\", \"@context\": \"http://schema.org/extensions\", \"themeColor\": \"{{if eq .Priority 2}}FF0000{{else if eq .Priority 1}}FFA500{{else}}0078D4{{end}}\", \"summary\": \"{{.Title}}\", \"sections\": [{\"activityTitle\": \"🛰️ autonomy Alert\", \"activitySubtitle\": \"{{.Title}}\", \"activityImage\": \"https://example.com/autonomy-icon.png\", \"facts\": [{\"name\": \"Priority\", \"value\": \"{{if eq .Priority 2}}🚨 Emergency{{else if eq .Priority 1}}⚠️ High{{else}}ℹ️ Normal{{end}}\"}, {\"name\": \"Type\", \"value\": \"{{.Type}}\"}, {\"name\": \"Time\", \"value\": \"{{.Timestamp.Format \"2006-01-02 15:04:05 UTC\"}}\"}], \"markdown\": true, \"text\": \"{{.Message}}\"}]}",
    "retry_attempts": 2
  }
}
```

### 5. 🐙 **GitHub Issues Integration**

```json
{
  "webhook": {
    "enabled": true,
    "name": "GitHub Issues",
    "url": "https://api.github.com/repos/your-org/autonomy-alerts/issues",
    "method": "POST",
    "content_type": "application/json",
    "auth_type": "bearer",
    "auth_token": "ghp_your_github_token",
    "template": "{\"title\": \"[{{.Type}}] {{.Title}}\", \"body\": \"## Alert Details\\n\\n**Message:** {{.Message}}\\n\\n**Priority:** {{.Priority}}\\n\\n**Timestamp:** {{.Timestamp.Format \"2006-01-02 15:04:05 UTC\"}}\\n\\n**Type:** {{.Type}}\\n\\n## Context\\n\\n```json\\n{{.Context | toJson}}\\n```\", \"labels\": [\"autonomy\", \"{{.Type}}\", \"{{if eq .Priority 2}}critical{{else if eq .Priority 1}}high{{else}}normal{{end}}\"], \"assignees\": [\"network-admin\"]}",
    "priority_filter": [1, 2],
    "type_filter": ["failover", "critical_error"]
  }
}
```

### 6. 📋 **Jira Integration**

```json
{
  "webhook": {
    "enabled": true,
    "name": "Jira Tickets",
    "url": "https://your-domain.atlassian.net/rest/api/2/issue",
    "method": "POST",
    "content_type": "application/json",
    "auth_type": "basic",
    "auth_username": "your-email@company.com",
    "auth_password": "your-api-token",
    "template": "{\"fields\": {\"project\": {\"key\": \"NET\"}, \"summary\": \"{{.Title}}\", \"description\": \"{{.Message}}\\n\\nTimestamp: {{.Timestamp.Format \"2006-01-02 15:04:05 UTC\"}}\\nType: {{.Type}}\\nPriority: {{.Priority}}\", \"issuetype\": {\"name\": \"{{if eq .Priority 2}}Incident{{else}}Task{{end}}\"}, \"priority\": {\"name\": \"{{if eq .Priority 2}}Highest{{else if eq .Priority 1}}High{{else}}Medium{{end}}\"}, \"labels\": [\"autonomy\", \"{{.Type}}\"]}}",
    "priority_filter": [1, 2]
  }
}
```

### 7. 🔍 **Elasticsearch Integration**

```json
{
  "webhook": {
    "enabled": true,
    "name": "Elasticsearch Logs",
    "url": "https://elasticsearch.example.com/autonomy-alerts/_doc",
    "method": "POST",
    "content_type": "application/json",
    "auth_type": "basic",
    "auth_username": "elastic",
    "auth_password": "your-password",
    "template": "{\"@timestamp\": \"{{.Timestamp.Format \"2006-01-02T15:04:05Z\"}}\", \"level\": \"{{if eq .Priority 2}}ERROR{{else if eq .Priority 1}}WARN{{else}}INFO{{end}}\", \"message\": \"{{.Message}}\", \"title\": \"{{.Title}}\", \"type\": \"{{.Type}}\", \"priority\": {{.Priority}}, \"source\": \"autonomy\", \"context\": {{.Context | toJson}}}",
    "include_raw_data": true
  }
}
```

### 8. 🌐 **Generic REST API Integration**

```json
{
  "webhook": {
    "enabled": true,
    "name": "Custom Monitoring API",
    "url": "https://api.yourcompany.com/v1/alerts",
    "method": "POST",
    "content_type": "application/json",
    "auth_type": "api_key",
    "auth_header": "X-API-Key",
    "auth_token": "your-api-key-here",
    "headers": {
      "X-Source": "autonomy",
      "X-Version": "1.0.0"
    },
    "field_mapping": {
      "Title": "alert_name",
      "Message": "alert_description",
      "Type": "alert_category",
      "Priority": "severity_level",
      "Timestamp": "created_at"
    },
    "exclude_fields": ["source", "version"],
    "timeout": 15,
    "retry_attempts": 5,
    "retry_delay": 3
  }
}
```

### 9. 📊 **InfluxDB Integration**

```json
{
  "webhook": {
    "enabled": true,
    "name": "InfluxDB Metrics",
    "url": "https://influxdb.example.com/write?db=autonomy&precision=s",
    "method": "POST",
    "content_type": "text/plain",
    "auth_type": "bearer",
    "auth_token": "your-influx-token",
    "template": "autonomy_alerts,type={{.Type}},priority={{.Priority}} title=\"{{.Title}}\",message=\"{{.Message}}\" {{.Timestamp.Unix}}",
    "template_format": "text"
  }
}
```

### 10. 🔔 **Custom Slack App Integration**

```json
{
  "webhook": {
    "enabled": true,
    "name": "Custom Slack App",
    "url": "https://hooks.slack.com/services/YOUR/SLACK/WEBHOOK",
    "method": "POST",
    "content_type": "application/json",
    "template": "{\"text\": \"{{.Title}}\", \"attachments\": [{\"color\": \"{{if eq .Priority 2}}danger{{else if eq .Priority 1}}warning{{else}}good{{end}}\", \"fields\": [{\"title\": \"Message\", \"value\": \"{{.Message}}\", \"short\": false}, {\"title\": \"Priority\", \"value\": \"{{.Priority}}\", \"short\": true}, {\"title\": \"Type\", \"value\": \"{{.Type}}\", \"short\": true}, {\"title\": \"Time\", \"value\": \"{{.Timestamp.Format \"2006-01-02 15:04:05 UTC\"}}\", \"short\": true}], \"footer\": \"autonomy Daemon\", \"ts\": {{.Timestamp.Unix}}}]}",
    "priority_filter": [0, 1, 2]
  }
}
```

## 🎨 Template Syntax Guide

### Available Variables
- `{{.Type}}` - Notification type (failover, critical_error, etc.)
- `{{.Title}}` - Alert title
- `{{.Message}}` - Alert message
- `{{.Priority}}` - Priority level (0-2)
- `{{.Timestamp}}` - Timestamp object
- `{{.Context}}` - Context data map
- `{{.Source}}` - Always "autonomy"
- `{{.Version}}` - Daemon version

### Template Functions
- `{{.Timestamp.Format "2006-01-02T15:04:05Z"}}` - Format timestamp
- `{{.Timestamp.Unix}}` - Unix timestamp
- `{{.Context | toJson}}` - Convert context to JSON
- `{{if eq .Priority 2}}critical{{else}}normal{{end}}` - Conditional logic

### Content Type Support
- **JSON**: `application/json` (default)
- **XML**: `application/xml` 
- **Form Data**: `application/x-www-form-urlencoded`
- **Plain Text**: `text/plain`
- **Custom**: Use template for full control

## 🔐 Authentication Methods

### Bearer Token
```json
{
  "auth_type": "bearer",
  "auth_token": "your-bearer-token"
}
```

### Basic Authentication
```json
{
  "auth_type": "basic",
  "auth_username": "username",
  "auth_password": "password"
}
```

### API Key Header
```json
{
  "auth_type": "api_key",
  "auth_header": "X-API-Key",
  "auth_token": "your-api-key"
}
```

### Custom Headers
```json
{
  "auth_type": "custom",
  "headers": {
    "Authorization": "Custom your-token",
    "X-Custom-Auth": "value"
  }
}
```

## 🎛️ Filtering & Transformation

### Priority Filtering
```json
{
  "priority_filter": [1, 2]  // Only high and emergency
}
```

### Type Filtering
```json
{
  "type_filter": ["failover", "critical_error", "member_down"]
}
```

### Field Mapping
```json
{
  "field_mapping": {
    "Title": "alert_name",
    "Message": "description",
    "Type": "category"
  }
}
```

### Field Exclusion
```json
{
  "exclude_fields": ["source", "version", "hostname"]
}
```

## 🔄 Retry & Reliability

### Retry Configuration
```json
{
  "retry_attempts": 3,     // Number of retry attempts
  "retry_delay": 5,        // Seconds between retries
  "timeout": 30            // Request timeout in seconds
}
```

### SSL & Redirects
```json
{
  "verify_ssl": true,      // Verify SSL certificates
  "follow_redirects": true // Follow HTTP redirects
}
```

## 🧪 Testing Your Webhook

Use the ubus API to test your webhook configuration:

```bash
# Test webhook configuration
ubus call autonomy test_webhook '{
  "webhook_name": "My Custom Integration"
}'

# Send test notification
ubus call autonomy send_test_notification '{
  "channels": ["webhook"],
  "title": "Test Alert",
  "message": "This is a test notification"
}'
```

## 📝 Best Practices

### 1. **Security**
- Use HTTPS URLs whenever possible
- Store sensitive tokens in environment variables
- Implement proper authentication
- Verify SSL certificates in production

### 2. **Reliability**
- Configure appropriate retry settings
- Set reasonable timeouts
- Use filtering to avoid spam
- Monitor webhook endpoint health

### 3. **Performance**
- Use efficient templates
- Filter unnecessary notifications
- Exclude unused fields
- Consider rate limiting on receiving end

### 4. **Monitoring**
- Log webhook responses
- Monitor success/failure rates
- Set up alerts for webhook failures
- Test webhooks regularly

## 🚀 Advanced Use Cases

### Multi-Environment Setup
```json
{
  "webhook": {
    "enabled": true,
    "name": "Environment-Aware Webhook",
    "url": "{{if eq .Context.environment \"production\"}}https://prod-api.com{{else}}https://staging-api.com{{end}}/alerts",
    "template": "{\"env\": \"{{.Context.environment}}\", \"alert\": \"{{.Title}}\"}",
    "priority_filter": "{{if eq .Context.environment \"production\"}}[1,2]{{else}}[0,1,2]{{end}}"
  }
}
```

### Dynamic Routing
```json
{
  "webhook": {
    "enabled": true,
    "name": "Dynamic Router",
    "url": "https://api.example.com/{{.Type}}/alerts",
    "template": "{\"routing_key\": \"{{.Type}}\", \"data\": {{.Context | toJson}}}"
  }
}
```

This comprehensive webhook system makes it easy to integrate autonomy with virtually any external system or service! 🎉
