# 🔔 Notification Configuration Guide

## 🎯 Overview

autonomy supports multiple notification channels to ensure you never miss critical network events. This guide covers configuration for all supported notification methods.

## 🏗️ Supported Notification Channels

- **📱 Pushover** - Mobile push notifications with priority levels
- **📧 Email** - HTML formatted email alerts with SMTP support
- **💬 Slack** - Rich Slack messages with attachments
- **🎮 Discord** - Embedded Discord messages with colors
- **📲 Telegram** - Markdown formatted Telegram messages
- **🔗 Webhook** - Generic webhook for custom integrations

## ⚙️ UCI Configuration

### Basic Configuration Structure

```bash
# Enable notification channels
uci set autonomy.notifications.pushover_enabled='1'
uci set autonomy.notifications.email_enabled='1'
uci set autonomy.notifications.slack_enabled='1'
uci set autonomy.notifications.discord_enabled='1'
uci set autonomy.notifications.telegram_enabled='1'
uci set autonomy.notifications.webhook_enabled='1'

# General notification settings
uci set autonomy.notifications.notify_on_failover='1'
uci set autonomy.notifications.notify_on_failback='1'
uci set autonomy.notifications.notify_on_member_down='1'
uci set autonomy.notifications.notify_on_critical='1'
uci set autonomy.notifications.priority_threshold='warning'
uci set autonomy.notifications.notification_cooldown_s='300'
uci set autonomy.notifications.max_notifications_hour='20'

uci commit autonomy
```

## 📱 Pushover Configuration

```bash
# Pushover Settings
uci set autonomy.notifications.pushover_enabled='1'
uci set autonomy.notifications.pushover_token='your_app_token_here'
uci set autonomy.notifications.pushover_user='your_user_key_here'
uci set autonomy.notifications.pushover_device='your_device_name'  # Optional

# Priority Settings
uci set autonomy.notifications.priority_failover='2'    # Emergency
uci set autonomy.notifications.priority_critical='2'    # Emergency
uci set autonomy.notifications.priority_member_down='1' # High
uci set autonomy.notifications.priority_failback='0'    # Normal
uci set autonomy.notifications.priority_recovery='0'    # Normal

uci commit autonomy
```

**Getting Pushover Credentials:**
1. Create account at [pushover.net](https://pushover.net)
2. Create an application to get your **App Token**
3. Find your **User Key** in your account dashboard
4. Install Pushover app on your devices

## 📧 Email Configuration

```bash
# Email Settings
uci set autonomy.notifications.email_enabled='1'
uci set autonomy.notifications.email_smtp_host='smtp.gmail.com'
uci set autonomy.notifications.email_smtp_port='587'
uci set autonomy.notifications.email_username='your-email@gmail.com'
uci set autonomy.notifications.email_password='your-app-password'
uci set autonomy.notifications.email_from='autonomy@yourcompany.com'
uci set autonomy.notifications.email_use_starttls='1'
uci set autonomy.notifications.email_use_tls='0'

# Multiple recipients (comma-separated)
uci add_list autonomy.notifications.email_to='admin@yourcompany.com'
uci add_list autonomy.notifications.email_to='network-team@yourcompany.com'
uci add_list autonomy.notifications.email_to='alerts@yourcompany.com'

uci commit autonomy
```

**Common SMTP Settings:**

| Provider | SMTP Host | Port | TLS | StartTLS |
|----------|-----------|------|-----|----------|
| Gmail | smtp.gmail.com | 587 | No | Yes |
| Gmail (TLS) | smtp.gmail.com | 465 | Yes | No |
| Outlook | smtp-mail.outlook.com | 587 | No | Yes |
| Yahoo | smtp.mail.yahoo.com | 587 | No | Yes |
| Custom | your-smtp.com | 587/465 | Varies | Varies |

## 💬 Slack Configuration

```bash
# Slack Settings
uci set autonomy.notifications.slack_enabled='1'
uci set autonomy.notifications.slack_webhook_url='https://hooks.slack.com/services/YOUR/SLACK/WEBHOOK'
uci set autonomy.notifications.slack_channel='#network-alerts'     # Optional
uci set autonomy.notifications.slack_username='autonomy'           # Optional
uci set autonomy.notifications.slack_icon_emoji=':satellite:'      # Optional
uci set autonomy.notifications.slack_icon_url='https://example.com/icon.png' # Optional

uci commit autonomy
```

**Setting up Slack Webhook:**
1. Go to [Slack Apps](https://api.slack.com/apps)
2. Create new app → "From scratch"
3. Add "Incoming Webhooks" feature
4. Create webhook for your channel
5. Copy the webhook URL

## 🎮 Discord Configuration

```bash
# Discord Settings
uci set autonomy.notifications.discord_enabled='1'
uci set autonomy.notifications.discord_webhook_url='https://discord.com/api/webhooks/YOUR/WEBHOOK/URL'
uci set autonomy.notifications.discord_username='autonomy'         # Optional
uci set autonomy.notifications.discord_avatar_url='https://example.com/avatar.png' # Optional

uci commit autonomy
```

**Setting up Discord Webhook:**
1. Go to your Discord server
2. Right-click channel → "Edit Channel"
3. Go to "Integrations" → "Webhooks"
4. Click "New Webhook"
5. Copy the webhook URL

## 📲 Telegram Configuration

```bash
# Telegram Settings
uci set autonomy.notifications.telegram_enabled='1'
uci set autonomy.notifications.telegram_token='123456789:ABCdefGHIjklMNOpqrsTUVwxyz'
uci set autonomy.notifications.telegram_chat_id='-1001234567890'

uci commit autonomy
```

**Setting up Telegram Bot:**
1. Message [@BotFather](https://t.me/botfather) on Telegram
2. Send `/newbot` and follow instructions
3. Get your bot token
4. Add bot to your group/channel
5. Get chat ID using [@userinfobot](https://t.me/userinfobot)

## 🔗 Webhook Configuration

```bash
# Basic Webhook Settings
uci set autonomy.notifications.webhook_enabled='1'
uci set autonomy.notifications.webhook_url='https://api.yourservice.com/alerts'
uci set autonomy.notifications.webhook_method='POST'
uci set autonomy.notifications.webhook_content_type='application/json'
uci set autonomy.notifications.webhook_name='Custom Integration'
uci set autonomy.notifications.webhook_description='Integration with monitoring system'

# Authentication
uci set autonomy.notifications.webhook_auth_type='bearer'
uci set autonomy.notifications.webhook_auth_token='your-api-token'

# Advanced Settings
uci set autonomy.notifications.webhook_timeout='30'
uci set autonomy.notifications.webhook_retry_attempts='3'
uci set autonomy.notifications.webhook_retry_delay='5'
uci set autonomy.notifications.webhook_verify_ssl='1'

# Custom Template (optional)
uci set autonomy.notifications.webhook_template='{"alert": "{{.Title}}", "message": "{{.Message}}", "priority": {{.Priority}}}'
uci set autonomy.notifications.webhook_template_format='json'

# Filtering (optional)
uci add_list autonomy.notifications.webhook_priority_filter='1'  # High priority
uci add_list autonomy.notifications.webhook_priority_filter='2'  # Emergency
uci add_list autonomy.notifications.webhook_type_filter='failover'
uci add_list autonomy.notifications.webhook_type_filter='critical_error'

uci commit autonomy
```

## 🎛️ Advanced Configuration Examples

### Multiple Email Recipients
```bash
# Clear existing recipients first
uci delete autonomy.notifications.email_to

# Add multiple recipients
uci add_list autonomy.notifications.email_to='admin@company.com'
uci add_list autonomy.notifications.email_to='network@company.com'
uci add_list autonomy.notifications.email_to='oncall@company.com'

uci commit autonomy
```

### Custom Webhook Headers
```bash
# Note: UCI doesn't directly support maps, so webhook headers 
# need to be configured via JSON config file or ubus API
```

### Priority-Based Filtering
```bash
# Only send high and emergency notifications via webhook
uci add_list autonomy.notifications.webhook_priority_filter='1'
uci add_list autonomy.notifications.webhook_priority_filter='2'

# Only send specific notification types
uci add_list autonomy.notifications.webhook_type_filter='failover'
uci add_list autonomy.notifications.webhook_type_filter='critical_error'
uci add_list autonomy.notifications.webhook_type_filter='member_down'

uci commit autonomy
```

## 🧪 Testing Configuration

### Test All Channels
```bash
# Send test notification to all enabled channels
ubus call autonomy send_test_notification '{
  "title": "Test Notification",
  "message": "This is a test of the autonomy notification system",
  "priority": 0
}'
```

### Test Specific Channel
```bash
# Test only Pushover
ubus call autonomy send_test_notification '{
  "channels": ["pushover"],
  "title": "Pushover Test",
  "message": "Testing Pushover notifications"
}'

# Test only Email
ubus call autonomy send_test_notification '{
  "channels": ["email"],
  "title": "Email Test", 
  "message": "Testing email notifications"
}'

# Test only Webhook
ubus call autonomy send_test_notification '{
  "channels": ["webhook"],
  "title": "Webhook Test",
  "message": "Testing webhook integration"
}'
```

### Check Channel Status
```bash
# Get notification channel status
ubus call autonomy notification_status '{}'

# Get detailed channel configuration
ubus call autonomy notification_config '{}'
```

## 🔧 Troubleshooting

### Common Issues

**Pushover not working:**
- Verify app token and user key are correct
- Check device name (if specified)
- Ensure Pushover app is installed on target devices

**Email not working:**
- Verify SMTP credentials and server settings
- Check if app passwords are required (Gmail, Outlook)
- Verify firewall allows SMTP traffic (ports 587/465)
- Test with telnet: `telnet smtp.gmail.com 587`

**Slack not working:**
- Verify webhook URL is correct and active
- Check channel permissions
- Ensure webhook has proper scope

**Discord not working:**
- Verify webhook URL is correct
- Check channel permissions
- Ensure webhook hasn't been deleted

**Telegram not working:**
- Verify bot token is correct
- Check chat ID (negative for groups/channels)
- Ensure bot is added to the chat
- Bot must have permission to send messages

**Webhook not working:**
- Check URL accessibility from RUTOS device
- Verify authentication credentials
- Check SSL certificate if using HTTPS
- Review webhook endpoint logs

### Debug Commands

```bash
# Check notification configuration
uci show autonomy.notifications

# View notification logs
logread | grep autonomy | grep notification

# Test network connectivity
ping api.pushover.net
nslookup smtp.gmail.com
curl -I https://hooks.slack.com/services/test
```

### Log Analysis

```bash
# Monitor notifications in real-time
logread -f | grep "notification\|pushover\|email\|slack\|discord\|telegram\|webhook"

# Check for authentication errors
logread | grep -i "auth\|credential\|token\|password"

# Check for network errors  
logread | grep -i "timeout\|connection\|dns\|ssl"
```

## 📊 Configuration Examples

### Minimal Setup (Pushover Only)
```bash
uci set autonomy.notifications.pushover_enabled='1'
uci set autonomy.notifications.pushover_token='your_token'
uci set autonomy.notifications.pushover_user='your_user'
uci set autonomy.notifications.notify_on_failover='1'
uci set autonomy.notifications.notify_on_critical='1'
uci commit autonomy
```

### Enterprise Setup (All Channels)
```bash
# Enable all channels
uci set autonomy.notifications.pushover_enabled='1'
uci set autonomy.notifications.email_enabled='1'
uci set autonomy.notifications.slack_enabled='1'
uci set autonomy.notifications.webhook_enabled='1'

# Configure each channel...
# (See individual sections above)

# Set comprehensive notification rules
uci set autonomy.notifications.notify_on_failover='1'
uci set autonomy.notifications.notify_on_failback='1'
uci set autonomy.notifications.notify_on_member_down='1'
uci set autonomy.notifications.notify_on_critical='1'
uci set autonomy.notifications.notify_on_recovery='1'

uci commit autonomy
```

### High-Availability Setup
```bash
# Multiple redundant channels
uci set autonomy.notifications.pushover_enabled='1'
uci set autonomy.notifications.email_enabled='1'
uci set autonomy.notifications.slack_enabled='1'

# Multiple email recipients
uci add_list autonomy.notifications.email_to='primary@company.com'
uci add_list autonomy.notifications.email_to='backup@company.com'
uci add_list autonomy.notifications.email_to='oncall@company.com'

# Emergency priorities
uci set autonomy.notifications.priority_failover='2'
uci set autonomy.notifications.priority_critical='2'

uci commit autonomy
```

This comprehensive configuration system ensures you can set up notifications exactly how you need them for your environment! 🎉
