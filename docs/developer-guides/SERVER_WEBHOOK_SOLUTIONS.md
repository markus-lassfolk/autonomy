# Server-Side Webhook Receiver Solutions

This document provides comprehensive solutions for deploying a server-side webhook receiver that can receive alerts from the `starwatch` sidecar and automatically create GitHub issues with intelligent filtering and GitHub Copilot integration.

## 🎯 Overview

The webhook receiver will:
- Receive HMAC-signed alerts from `starwatch`
- Filter out irrelevant issues (old versions, configuration errors)
- Create GitHub issues with proper labels and assignments
- Integrate with GitHub Copilot for autonomous issue resolution
- Provide easy deployment options

## 🚀 Solution Options

### Option 1: GitHub Actions Webhook Receiver (Recommended)
**Pros**: Free, native GitHub integration, easy deployment
**Cons**: Limited to GitHub ecosystem

### Option 2: Azure Function Webhook Receiver
**Pros**: Scalable, cost-effective, cloud-native
**Cons**: Requires Azure account (free credits available)

## 📋 Requirements

### Webhook Payload Structure
The receiver expects the following JSON payload from `starwatch`:

```json
{
  "device_id": "rutx50-van-01",
  "fw": "RUTX_R_00.07.17",
  "severity": "critical|warn|info",
  "scenario": "daemon_down|daemon_hung|crash_loop|system_degraded|slow|post_reboot",
  "overlay_pct": 96,
  "mem_avail_mb": 12,
  "load1": 5.3,
  "ubus_ok": false,
  "actions": ["restart", "hold_down"],
  "note": "heartbeat stale; restarted",
  "ts": 1699999999
}
```

### Filtering Criteria
1. **Version Filtering**: Only process issues from supported firmware versions
2. **Configuration vs Code Issues**: Filter out user configuration errors
3. **Duplicate Detection**: Prevent spam from repeated alerts
4. **Severity Threshold**: Only create issues for critical/warn scenarios

## 🔧 GitHub Actions Solution

### Quick Deploy (One-Click)

[![Deploy to GitHub](https://github.com/markus-lassfolk/autonomy/actions/workflows/webhook-receiver.yml/badge.svg)](https://github.com/markus-lassfolk/autonomy/actions/workflows/webhook-receiver.yml)

### Manual Setup

1. **Fork this repository** or create a new repository
2. **Add the webhook receiver workflow** (see `/.github/workflows/webhook-receiver.yml`)
3. **Configure repository secrets**:
   - `WEBHOOK_SECRET`: Your HMAC secret (same as in `starwatch` config)
   - `GITHUB_TOKEN`: Personal access token with repo permissions
4. **Enable GitHub Pages** for the webhook endpoint
5. **Configure Copilot integration** (see Copilot Setup section)

### Features
- ✅ HMAC signature validation
- ✅ Version filtering (configurable supported versions)
- ✅ Intelligent issue deduplication
- ✅ Automatic label assignment
- ✅ GitHub Copilot integration
- ✅ Diagnostic bundle attachment
- ✅ Rate limiting and spam protection

## ☁️ Azure Function Solution

### Quick Deploy (One-Click)

[![Deploy to Azure](https://aka.ms/deploytoazurebutton)](https://portal.azure.com/#create/Microsoft.Template/uri/https%3A%2F%2Fraw.githubusercontent.com%2Fmarkus-lassfolk%2Fautonomy%2Fmain%2Fazure%2Fwebhook-function%2Fazuredeploy.json)

### Manual Setup

1. **Create Azure Function App**:
   ```bash
   az group create --name autonomy-webhook --location eastus
   az functionapp create --name autonomy-webhook --storage-account autonomywebhook --consumption-plan-location eastus --resource-group autonomy-webhook --runtime node --functions-version 4
   ```

2. **Deploy the function code** (see `/azure/webhook-function/`)

3. **Configure application settings**:
   ```bash
   az functionapp config appsettings set --name autonomy-webhook --resource-group autonomy-webhook --settings WEBHOOK_SECRET="your-secret" GITHUB_TOKEN="your-token" SUPPORTED_VERSIONS="RUTX_R_00.07.17,RUTX_R_00.07.18"
   ```

### Features
- ✅ Serverless scalability
- ✅ Built-in monitoring and logging
- ✅ Cost-effective (free tier available)
- ✅ Same filtering and Copilot integration as GitHub solution

## 🤖 GitHub Copilot Integration

### Automatic Issue Assignment

Configure GitHub Copilot to automatically:
1. **Analyze issues** and suggest fixes
2. **Create pull requests** for identified problems
3. **Test solutions** before merging
4. **Update documentation** when needed

### Setup Instructions

1. **Enable GitHub Copilot** in your repository settings
2. **Configure issue templates** with Copilot-friendly structure
3. **Set up branch protection** for automated PRs
4. **Configure Copilot rules** for autonomous fixes

### Copilot Configuration

Add `.github/copilot.yml` to your repository:

```yaml
# Copilot configuration for autonomous issue resolution
rules:
  - name: "Auto-fix autonomyd issues"
    description: "Automatically fix common autonomyd issues"
    patterns:
      - "daemon_down"
      - "crash_loop"
      - "system_degraded"
    actions:
      - create_pull_request
      - run_tests
      - update_documentation
    allowed_files:
      - "pkg/**/*.go"
      - "cmd/**/*.go"
      - "scripts/**/*"
      - "docs/**/*.md"
```

## 🔍 Intelligent Filtering

### Version Filtering
```javascript
const SUPPORTED_VERSIONS = [
  'RUTX_R_00.07.17',
  'RUTX_R_00.07.18',
  'RUTX_R_00.08.00'
];

function isSupportedVersion(fw) {
  return SUPPORTED_VERSIONS.some(version => fw.startsWith(version));
}
```

### Configuration vs Code Issues
```javascript
const CONFIG_ERRORS = [
  'configuration_error',
  'user_misconfiguration',
  'network_setup_error'
];

function isCodeIssue(scenario, note) {
  // Filter out configuration errors
  if (CONFIG_ERRORS.some(error => note.toLowerCase().includes(error))) {
    return false;
  }
  
  // Focus on system-level issues
  const systemIssues = ['daemon_down', 'crash_loop', 'system_degraded'];
  return systemIssues.includes(scenario);
}
```

### Duplicate Detection
```javascript
function generateIssueKey(payload) {
  return `${payload.device_id}-${payload.scenario}-${payload.severity}`;
}

function isDuplicate(issueKey, existingIssues) {
  const recentIssues = existingIssues.filter(issue => 
    issue.created_at > new Date(Date.now() - 24 * 60 * 60 * 1000) // Last 24 hours
  );
  
  return recentIssues.some(issue => 
    issue.title.includes(issueKey) && issue.state === 'open'
  );
}
```

## 📊 Monitoring and Analytics

### GitHub Actions Metrics
- Issue creation rate
- Filtering effectiveness
- Copilot resolution rate
- Response time metrics

### Azure Function Metrics
- Function execution count
- Error rates
- Response times
- Cost tracking

## 🔧 Configuration

### Environment Variables

| Variable | Description | Required |
|----------|-------------|----------|
| `WEBHOOK_SECRET` | HMAC secret for signature validation | Yes |
| `GITHUB_TOKEN` | Personal access token with repo permissions | Yes |
| `SUPPORTED_VERSIONS` | Comma-separated list of supported firmware versions | Yes |
| `MIN_SEVERITY` | Minimum severity to create issues (info/warn/critical) | No |
| `COPILOT_ENABLED` | Enable GitHub Copilot integration | No |
| `AUTO_ASSIGN` | Automatically assign issues to Copilot | No |

### Webhook Configuration

In your `starwatch` configuration (`/etc/autonomy/watch.conf`):

```ini
WEBHOOK_URL=https://your-username.github.io/your-repo/webhook
WATCH_SECRET=your-secret-key
```

## 🚀 Deployment Instructions

### GitHub Actions (Recommended)

1. **Click the "Deploy to GitHub" button above**
2. **Configure repository secrets**:
   - Go to Settings → Secrets and variables → Actions
   - Add `WEBHOOK_SECRET` and `GITHUB_TOKEN`
3. **Enable GitHub Pages**:
   - Go to Settings → Pages
   - Set source to "GitHub Actions"
4. **Test the webhook**:
   ```bash
   curl -X POST https://your-username.github.io/your-repo/webhook \
     -H "Content-Type: application/json" \
     -H "X-Starwatch-Signature: sha256=test" \
     -d '{"test": "payload"}'
   ```

### Azure Function

1. **Click the "Deploy to Azure" button above**
2. **Configure application settings** in Azure Portal
3. **Get the function URL** from the function app overview
4. **Update your starwatch configuration** with the function URL

## 📈 Success Metrics

- **Issue Creation Rate**: 95% of valid alerts create issues
- **False Positive Rate**: <5% of created issues are configuration errors
- **Copilot Resolution Rate**: 60% of issues resolved autonomously
- **Response Time**: <30 seconds from alert to issue creation
- **Uptime**: 99.9% webhook availability

## 🔒 Security Considerations

- **HMAC Validation**: All webhooks must be signed
- **Rate Limiting**: Prevent spam and abuse
- **Input Validation**: Sanitize all webhook payloads
- **Secret Management**: Use secure secret storage
- **Access Control**: Limit repository access to necessary permissions

## 📞 Support

For issues with the webhook receiver:
1. Check the GitHub Actions logs or Azure Function logs
2. Verify webhook configuration in `starwatch`
3. Test with the provided test scripts
4. Create an issue in this repository

---

**Last Updated**: 2025-01-20
**Version**: 1.0.0
