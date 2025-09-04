# 🔌 API Integrations Guide

## Overview

The Autonomy system integrates with several external APIs to provide enhanced functionality. This guide covers all API integrations, where to sign up, and how to configure them.

## 🛰️ Space-Track API (Satellite Data)

### What it provides
- Real-time Starlink satellite orbital data (TLE format)
- Satellite positions for obstruction prediction
- Essential for Starlink tracking features

### Sign up
1. **Visit**: [https://www.space-track.org](https://www.space-track.org)
2. **Create Account**: Free registration required
3. **Verify Email**: Check your email and verify account
4. **Accept Terms**: Agree to data usage terms

### Configuration
```bash
# Method 1: Environment variables
export SPACE_TRACK_USERNAME=your_username
export SPACE_TRACK_PASSWORD=your_password

# Method 2: UCI configuration
uci set autonomy.space_track.username='your_username'
uci set autonomy.space_track.password='your_password'
uci set autonomy.space_track.enabled='1'
uci commit autonomy
```

### Rate Limits & Usage
- **Free Tier**: 200 requests per hour
- **Autonomy Usage**: <20 requests/hour (well within limits)
- **Data Caching**: 24-hour local caching reduces API calls
- **Cost**: Free for non-commercial use

---

## 📡 OpenCellID API (Cellular Location)

### What it provides
- Cell tower location data for GPS fallback
- Cellular triangulation when satellite GPS unavailable
- Global database of cell tower locations

### Sign up
1. **Visit**: [https://opencellid.org](https://opencellid.org)
2. **Create Account**: Free registration
3. **Get API Key**: Generate API key in dashboard
4. **Optional**: Download Android app to contribute data

### Configuration
```bash
# UCI configuration
uci set autonomy.opencellid.enabled='1'
uci set autonomy.opencellid.api_key='your_opencellid_key'
uci set autonomy.opencellid.contribution_enabled='1'  # Help improve database
uci set autonomy.opencellid.cache_ttl='3600'         # 1 hour
uci commit autonomy
```

### Rate Limits & Usage
- **Free Tier**: 1,000 requests per day
- **Autonomy Usage**: Intelligent rate limiting with 8:1 lookup-to-contribution ratio
- **Contribution**: System automatically contributes data to maintain free access
- **Cost**: Free with contribution, paid plans available

---

## 🔔 Pushover API (Notifications)

### What it provides
- Real-time push notifications to mobile devices
- Critical alerts and system status updates
- Cross-platform mobile notifications

### Sign up
1. **Visit**: [https://pushover.net](https://pushover.net)
2. **Create Account**: $5 one-time fee per platform
3. **Get User Key**: Found in your dashboard
4. **Create Application**: Get application token
5. **Install App**: Download mobile app

### Configuration
```bash
# UCI configuration
uci set autonomy.notifications.pushover_enabled='1'
uci set autonomy.notifications.pushover_token='your-app-token'
uci set autonomy.notifications.pushover_user='your-user-key'
uci set autonomy.notifications.pushover_priority='1'     # -2 to 2
uci set autonomy.notifications.pushover_sound='pushover' # notification sound
uci commit autonomy

# Alert configuration
uci set autonomy.notifications.alert_levels='critical,warning,info'
uci set autonomy.notifications.failover_alerts='1'
uci set autonomy.notifications.outage_predictions='1'
uci set autonomy.notifications.location_alerts='1'
uci commit autonomy
```

### Rate Limits & Usage
- **Free Tier**: 10,000 messages per month
- **Autonomy Usage**: Intelligent deduplication reduces message count
- **Priority Levels**: Emergency notifications can bypass quiet hours
- **Cost**: $5 one-time per platform (iOS/Android)

---

## 🌐 Google Geolocation API (Optional)

### What it provides
- WiFi and cellular-based location services
- High-accuracy location in urban areas
- Fallback location service

### Sign up
1. **Visit**: [Google Cloud Console](https://console.cloud.google.com)
2. **Create Project**: Set up new Google Cloud project
3. **Enable API**: Enable Geolocation API
4. **Get API Key**: Create credentials → API key
5. **Set Restrictions**: Restrict key to Geolocation API

### Configuration
```bash
# UCI configuration (optional)
uci set autonomy.google.geolocation_enabled='1'
uci set autonomy.google.api_key='your_google_api_key'
uci set autonomy.google.fallback_priority='3'  # Lower priority fallback
uci commit autonomy
```

### Rate Limits & Usage
- **Free Tier**: $200 credit monthly (≈40,000 requests)
- **Autonomy Usage**: Only used as fallback, minimal usage
- **Cost**: $5 per 1,000 requests after free tier

---

## 🐙 GitHub API (Development/CI)

### What it provides
- Automated issue creation for critical alerts
- CI/CD integration for deployments
- Development workflow automation

### Sign up
1. **GitHub Account**: Free account at [github.com](https://github.com)
2. **Personal Access Token**:
   - Go to Settings → Developer settings → Personal access tokens
   - Generate new token (classic)
   - Select scopes: `repo`, `workflow`, `write:packages`
3. **Repository Access**: Fork or access autonomy repository

### Configuration
```bash
# Environment variables (for CI/CD)
export GITHUB_TOKEN=ghp_your_github_token
export GITHUB_REPOSITORY=your-username/autonomy

# Webhook configuration
uci set autonomy.webhook.github_enabled='1'
uci set autonomy.webhook.github_token='ghp_your_github_token'
uci set autonomy.webhook.github_repo='your-username/autonomy'
uci set autonomy.webhook.create_issues='1'
uci commit autonomy
```

### Rate Limits & Usage
- **Free Tier**: 5,000 API requests per hour
- **GitHub Actions**: 2,000 minutes per month
- **Autonomy Usage**: Minimal API usage for critical alerts only
- **Cost**: Free for public repositories

---

## 📧 SMTP Email Integration

### What it provides
- Email notifications for critical events
- Detailed logs and reports via email
- Integration with existing email systems

### Setup Options

#### Gmail/Google Workspace
```bash
# App password required (not regular password)
uci set autonomy.notifications.smtp_server='smtp.gmail.com'
uci set autonomy.notifications.smtp_port='587'
uci set autonomy.notifications.smtp_username='your-email@gmail.com'
uci set autonomy.notifications.smtp_password='your-app-password'
uci set autonomy.notifications.smtp_encryption='tls'
uci commit autonomy
```

#### Office 365/Outlook
```bash
uci set autonomy.notifications.smtp_server='smtp-mail.outlook.com'
uci set autonomy.notifications.smtp_port='587'
uci set autonomy.notifications.smtp_username='your-email@outlook.com'
uci set autonomy.notifications.smtp_password='your-password'
uci set autonomy.notifications.smtp_encryption='tls'
uci commit autonomy
```

#### Custom SMTP Server
```bash
uci set autonomy.notifications.smtp_server='mail.yourcompany.com'
uci set autonomy.notifications.smtp_port='587'
uci set autonomy.notifications.smtp_username='alerts@yourcompany.com'
uci set autonomy.notifications.smtp_password='your-password'
uci set autonomy.notifications.smtp_encryption='tls'
uci commit autonomy
```

---

## 💬 Slack Integration (Optional)

### What it provides
- Team notifications in Slack channels
- Integration with incident management workflows
- Rich message formatting with links and status

### Setup
1. **Create Slack App**: [https://api.slack.com/apps](https://api.slack.com/apps)
2. **Add Incoming Webhooks**: Enable incoming webhooks feature
3. **Create Webhook URL**: Generate webhook URL for your channel
4. **Configure Permissions**: Add necessary bot permissions

### Configuration
```bash
uci set autonomy.notifications.slack_enabled='1'
uci set autonomy.notifications.slack_webhook='https://hooks.slack.com/services/YOUR/WEBHOOK/URL'
uci set autonomy.notifications.slack_channel='#network-alerts'
uci set autonomy.notifications.slack_username='Autonomy Bot'
uci commit autonomy
```

---

## 📱 Telegram Integration (Optional)

### What it provides
- Instant messaging notifications
- Bot commands for remote control
- Free alternative to Pushover

### Setup
1. **Create Bot**: Message @BotFather on Telegram
2. **Get Token**: Follow BotFather instructions to get bot token
3. **Get Chat ID**: Message your bot and get chat ID from API
4. **Test Bot**: Send test message to verify setup

### Configuration
```bash
uci set autonomy.notifications.telegram_enabled='1'
uci set autonomy.notifications.telegram_token='your-bot-token'
uci set autonomy.notifications.telegram_chat_id='your-chat-id'
uci set autonomy.notifications.telegram_parse_mode='Markdown'
uci commit autonomy
```

---

## 🔧 API Configuration Summary

### Required APIs
| API | Purpose | Cost | Setup Difficulty |
|-----|---------|------|------------------|
| Space-Track | Satellite tracking | Free | Easy |

### Recommended APIs
| API | Purpose | Cost | Setup Difficulty |
|-----|---------|------|------------------|
| OpenCellID | Cellular location | Free* | Easy |
| Pushover | Mobile notifications | $5 one-time | Easy |

### Optional APIs
| API | Purpose | Cost | Setup Difficulty |
|-----|---------|------|------------------|
| Google Geolocation | Location fallback | $5/1k requests | Medium |
| GitHub | Development/CI | Free | Medium |
| SMTP Email | Email notifications | Varies | Easy |
| Slack | Team notifications | Free | Medium |
| Telegram | Free notifications | Free | Easy |

*Free with data contribution

## 🛠️ Configuration Tools

### UCI Commands
```bash
# View all autonomy configuration
uci show autonomy

# Export configuration
uci export autonomy > autonomy-config.conf

# Import configuration  
uci import autonomy < autonomy-config.conf

# Validate configuration
autonomy-cli config validate
```

### Web Interface
Access configuration via web UI: `https://router-ip/cgi-bin/luci/admin/autonomy/config`

## 🔍 Troubleshooting

### Common Issues

#### API Authentication Failures
```bash
# Test Space-Track credentials
curl -c /tmp/cookies.txt \
  -d "identity=$SPACE_TRACK_USERNAME&password=$SPACE_TRACK_PASSWORD" \
  https://www.space-track.org/ajaxauth/login

# Test OpenCellID API key
curl "https://opencellid.org/cell/get?key=$OPENCELLID_KEY&mcc=240&mnc=1&lac=1&cellid=1"

# Test Pushover token
curl -X POST https://api.pushover.net/1/messages.json \
  -d "token=$PUSHOVER_TOKEN&user=$PUSHOVER_USER&message=Test"
```

#### Configuration Issues
```bash
# Reset to defaults
autonomy-cli config reset

# Validate current config
autonomy-cli config validate

# Check service logs
logread | grep autonomy | tail -20
```

## 🔗 Related Documentation

- [Configuration Reference](configuration-guide.md) - Detailed UCI settings
- [Getting Started](getting-started.md) - Basic setup
- [API Reference](../api-reference/) - Complete API documentation
- [Troubleshooting](../developer-guides/TROUBLESHOOTING.md) - Common issues

---

**Next**: [Configuration Guide](configuration-guide.md) for detailed UCI settings