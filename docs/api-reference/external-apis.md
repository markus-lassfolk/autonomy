# 🌐 External API Integrations

## Overview

The Autonomy system integrates with several external APIs to provide enhanced functionality. This reference covers all external APIs, their purposes, and integration details.

## 🛰️ Required APIs

### Space-Track API
**Purpose**: Satellite orbital data for Starlink tracking  
**Website**: [https://www.space-track.org](https://www.space-track.org)  
**Cost**: Free (registration required)  
**Rate Limit**: 200 requests/hour  
**Autonomy Usage**: <20 requests/hour  

**Signup Process**:
1. Visit space-track.org
2. Click "Request Account Access"
3. Fill out registration form
4. Verify email address
5. Accept terms of service

**Configuration**:
```bash
uci set autonomy.external_apis.space_track_username='your_username'
uci set autonomy.external_apis.space_track_password='your_password'
uci set autonomy.external_apis.space_track_enabled='1'
```

**Required For**:
- Starlink satellite tracking
- Obstruction prediction
- Outage forecasting

---

## 📱 Recommended APIs

### Pushover (Mobile Notifications)
**Purpose**: Real-time mobile push notifications  
**Website**: [https://pushover.net](https://pushover.net)  
**Cost**: $5 one-time per platform (iOS/Android)  
**Rate Limit**: 10,000 messages/month  
**Autonomy Usage**: <100 messages/month with deduplication  

**Signup Process**:
1. Create account at pushover.net
2. Purchase mobile app ($5 iOS/Android)
3. Create application in dashboard
4. Get application token and user key

**Configuration**:
```bash
uci set autonomy.notifications.pushover_enabled='1'
uci set autonomy.notifications.pushover_token='your_app_token'
uci set autonomy.notifications.pushover_user='your_user_key'
```

**Features**:
- Critical system alerts
- Failover notifications
- Outage predictions
- Security alerts

### OpenCellID (Cellular Location)
**Purpose**: Cell tower location database for GPS fallback  
**Website**: [https://opencellid.org](https://opencellid.org)  
**Cost**: Free with data contribution  
**Rate Limit**: 1,000 requests/day (free tier)  
**Autonomy Usage**: Intelligent rate limiting with contribution  

**Signup Process**:
1. Register at opencellid.org
2. Generate API key in dashboard
3. Optional: Download mobile app for contribution

**Configuration**:
```bash
uci set autonomy.external_apis.opencellid_enabled='1'
uci set autonomy.external_apis.opencellid_api_key='your_api_key'
uci set autonomy.external_apis.opencellid_contribution='1'
```

**Features**:
- GPS fallback when satellite GPS unavailable
- Cellular triangulation
- Location services in remote areas

---

## 🔧 Optional APIs

### Google Geolocation API
**Purpose**: WiFi and cellular-based location services  
**Website**: [Google Cloud Console](https://console.cloud.google.com)  
**Cost**: $5 per 1,000 requests after $200 monthly credit  
**Rate Limit**: No specific limit  
**Autonomy Usage**: Fallback only, minimal usage  

**Signup Process**:
1. Create Google Cloud account
2. Create new project
3. Enable Geolocation API
4. Create API key credentials
5. Restrict key to Geolocation API

**Configuration**:
```bash
uci set autonomy.external_apis.google_enabled='1'
uci set autonomy.external_apis.google_api_key='your_google_api_key'
```

### GitHub API (Development)
**Purpose**: Automated issue creation and CI/CD integration  
**Website**: [https://github.com](https://github.com)  
**Cost**: Free for public repositories  
**Rate Limit**: 5,000 requests/hour  
**Autonomy Usage**: Critical alerts only  

**Signup Process**:
1. Create GitHub account
2. Go to Settings → Developer settings → Personal access tokens
3. Generate token with `repo` and `workflow` scopes

**Configuration**:
```bash
uci set autonomy.external_apis.github_enabled='1'
uci set autonomy.external_apis.github_token='ghp_your_token'
uci set autonomy.external_apis.github_repo='username/repo'
```

### Slack API (Team Notifications)
**Purpose**: Team notifications and incident management  
**Website**: [https://api.slack.com](https://api.slack.com)  
**Cost**: Free tier available  
**Rate Limit**: Varies by method  
**Autonomy Usage**: Notification webhooks  

**Signup Process**:
1. Create Slack app at api.slack.com/apps
2. Enable incoming webhooks
3. Create webhook URL for channel
4. Configure bot permissions

**Configuration**:
```bash
uci set autonomy.notifications.slack_enabled='1'
uci set autonomy.notifications.slack_webhook='https://hooks.slack.com/services/YOUR/WEBHOOK/URL'
```

### Telegram Bot API
**Purpose**: Free mobile notifications alternative  
**Website**: [https://core.telegram.org/bots](https://core.telegram.org/bots)  
**Cost**: Free  
**Rate Limit**: 30 messages/second  
**Autonomy Usage**: <10 messages/day  

**Signup Process**:
1. Message @BotFather on Telegram
2. Create new bot with `/newbot`
3. Get bot token
4. Get your chat ID

**Configuration**:
```bash
uci set autonomy.notifications.telegram_enabled='1'
uci set autonomy.notifications.telegram_token='your_bot_token'
uci set autonomy.notifications.telegram_chat_id='your_chat_id'
```

---

## 📊 API Usage Summary

### Autonomy API Call Patterns
| API | Frequency | Purpose | Data Usage |
|-----|-----------|---------|------------|
| Space-Track | Every 24 hours | Satellite data refresh | <1MB/day |
| OpenCellID | As needed | Location lookup | <100KB/day |
| Pushover | Event-driven | Critical notifications | <1KB/notification |
| Google Geolocation | Fallback only | Location services | <10KB/request |
| GitHub | Critical events | Issue creation | <5KB/issue |

### Cost Estimates (Monthly)
| API | Typical Usage | Cost |
|-----|---------------|------|
| Space-Track | 30 requests | Free |
| OpenCellID | 100 requests + contribution | Free |
| Pushover | 50 notifications | Free (within limit) |
| Google Geolocation | 10 requests | Free (within credit) |
| GitHub | 5 issues | Free |
| **Total** | | **~$0/month** |

## 🔐 Security Best Practices

### API Key Management
- Store credentials in UCI configuration, not environment variables
- Use least-privilege API keys with minimal required scopes
- Rotate API keys regularly (quarterly recommended)
- Monitor API usage for unusual patterns

### Rate Limiting
- Autonomy implements intelligent rate limiting for all APIs
- Local caching reduces external API calls
- Contribution systems (OpenCellID) maintain free access
- Graceful degradation when rate limits exceeded

### Error Handling
- Automatic retry with exponential backoff
- Fallback to alternative APIs when available
- Graceful degradation when APIs unavailable
- Comprehensive error logging and alerting

## 🛠️ Testing API Integrations

### Test All APIs
```bash
# Test Space-Track
curl -c /tmp/cookies.txt \
  -d "identity=$USERNAME&password=$PASSWORD" \
  https://www.space-track.org/ajaxauth/login

# Test OpenCellID  
curl "https://opencellid.org/cell/get?key=$API_KEY&mcc=240&mnc=1&lac=1&cellid=1"

# Test Pushover
curl -X POST https://api.pushover.net/1/messages.json \
  -d "token=$TOKEN&user=$USER&message=Test"

# Test via Autonomy
autonomy-cli api test-all
```

### Verify Integration
```bash
# Check API status
ubus call autonomy api_status

# View API usage statistics
ubus call autonomy api_stats

# Test specific API
ubus call autonomy test_api '{"api": "space_track"}'
```

## 📚 Related Documentation

- [API Integrations Guide](../tutorials/api-integrations-guide.md) - Setup instructions
- [Configuration Reference](configuration-reference.md) - Complete UCI reference
- [Troubleshooting](../developer-guides/TROUBLESHOOTING.md) - Common API issues

---

**Note**: All API integrations are optional except Space-Track (required for Starlink tracking). The system gracefully degrades functionality when APIs are unavailable.