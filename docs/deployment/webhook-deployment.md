# Webhook Receiver Deployment Guide

This guide provides step-by-step instructions for deploying the autonomy webhook receiver solutions.

## 🚀 Quick Start

### Option 1: GitHub Actions (Recommended - Free)

1. **Fork this repository** to your GitHub account
2. **Configure repository secrets**:
   - Go to Settings → Secrets and variables → Actions
   - Add `WEBHOOK_SECRET` (your HMAC secret)
   - Add `GITHUB_TOKEN` (personal access token with repo permissions)
3. **Configure repository variables**:
   - Go to Settings → Secrets and variables → Actions → Variables
   - Add `SUPPORTED_VERSIONS` (comma-separated firmware versions)
   - Add `MIN_SEVERITY` (warn/critical)
4. **Test the webhook**:
   - Go to Actions → autonomy Webhook Receiver
   - Click "Run workflow" with test payload

### Option 2: Azure Function (Cost-effective)

1. **Click the "Deploy to Azure" button** in the main documentation
2. **Fill in the parameters**:
   - Resource Group: Create new or use existing
   - Function App Name: Choose a unique name
   - Webhook Secret: Your HMAC secret
   - GitHub Token: Personal access token
3. **Wait for deployment** (2-3 minutes)
4. **Get your webhook URL** from the deployment outputs

## 📋 Prerequisites

### GitHub Token Setup

1. **Create a Personal Access Token**:
   - Go to GitHub Settings → Developer settings → Personal access tokens → Tokens (classic)
   - Click "Generate new token (classic)"
   - Select scopes: `repo`, `workflow`
   - Copy the token (you won't see it again)

2. **Token Permissions**:
   - `repo` - Full control of private repositories
   - `workflow` - Update GitHub Action workflows

### Webhook Secret

Generate a secure HMAC secret:

```bash
# Generate a random 32-byte secret
openssl rand -hex 32
```

## 🔧 Detailed Setup

### GitHub Actions Setup

#### 1. Repository Configuration

```bash
# Clone the repository
git clone https://github.com/your-username/autonomy.git
cd autonomy

# The webhook receiver is already included in .github/workflows/webhook-receiver.yml
```

#### 2. Environment Variables

Configure these in your repository settings:

| Variable | Description | Example |
|----------|-------------|---------|
| `WEBHOOK_SECRET` | HMAC secret for validation | `a1b2c3d4e5f6...` |
| `GITHUB_TOKEN` | Personal access token | `ghp_xxxxxxxxxx` |
| `SUPPORTED_VERSIONS` | Supported firmware versions | `RUTX_R_00.07.17,RUTX_R_00.07.18` |
| `MIN_SEVERITY` | Minimum severity to create issues | `warn` |
| `COPILOT_ENABLED` | Enable GitHub Copilot | `true` |
| `AUTO_ASSIGN` | Auto-assign to Copilot | `true` |

#### 3. Test the Setup

```bash
# Test with curl
curl -X POST https://api.github.com/repos/your-username/autonomy/dispatches \
  -H "Authorization: token YOUR_GITHUB_TOKEN" \
  -H "Accept: application/vnd.github.v3+json" \
  -d '{
    "event_type": "starwatch_alert",
    "client_payload": {
      "payload": {
        "device_id": "test-device",
        "fw": "RUTX_R_00.07.17",
        "severity": "critical",
        "scenario": "daemon_down",
        "note": "Test alert",
        "overlay_pct": 95,
        "mem_avail_mb": 25,
        "load1": 4.2,
        "ubus_ok": false,
        "actions": ["restart"],
        "ts": 1705776000
      },
      "signature": "sha256=YOUR_HMAC_SIGNATURE"
    }
  }'
```

### Azure Function Setup

#### 1. Prerequisites

- Azure subscription (free tier available)
- Azure CLI installed
- Node.js 18+ installed

#### 2. Manual Deployment

```bash
# Login to Azure
az login

# Create resource group
az group create --name autonomy-webhook --location eastus

# Deploy the function
az deployment group create \
  --resource-group autonomy-webhook \
  --template-file azure/webhook-function/azuredeploy.json \
  --parameters \
    webhookSecret="YOUR_WEBHOOK_SECRET" \
    githubToken="YOUR_GITHUB_TOKEN" \
    supportedVersions="RUTX_R_00.07.17,RUTX_R_00.07.18" \
    minSeverity="warn" \
    copilotEnabled="true" \
    autoAssign="true"
```

#### 3. Get Function URL

```bash
# Get the function URL
az functionapp function show \
  --resource-group autonomy-webhook \
  --name YOUR_FUNCTION_APP_NAME \
  --function-name webhook \
  --query "invokeUrlTemplate"
```

#### 4. Test the Function

```bash
# Test with curl
curl -X POST "YOUR_FUNCTION_URL" \
  -H "Content-Type: application/json" \
  -H "X-Starwatch-Signature: sha256=YOUR_HMAC_SIGNATURE" \
  -d '{
    "device_id": "test-device",
    "fw": "RUTX_R_00.07.17",
    "severity": "critical",
    "scenario": "daemon_down",
    "note": "Test alert",
    "overlay_pct": 95,
    "mem_avail_mb": 25,
    "load1": 4.2,
    "ubus_ok": false,
    "actions": ["restart"],
    "ts": 1705776000
  }'
```

## 🤖 GitHub Copilot Integration

### 1. Enable Copilot

1. **Enable GitHub Copilot** in your repository:
   - Go to Settings → Copilot
   - Enable "Allow GitHub Copilot to suggest code"
   - Enable "Allow GitHub Copilot to suggest code in public repositories"

2. **Configure Copilot**:
   - The `.github/copilot.yml` file is already configured
   - Copilot will automatically analyze issues and create PRs

### 2. Issue Assignment

Configure automatic issue assignment:

1. **Create a GitHub App** (optional):
   - Go to Settings → Developer settings → GitHub Apps
   - Create a new app with issue permissions
   - Install it in your repository

2. **Or use repository settings**:
   - Go to Settings → General → Features
   - Enable "Automatically delete head branches"
   - Enable "Allow auto-merge"

### 3. Branch Protection

Set up branch protection for automated PRs:

1. **Go to Settings → Branches**
2. **Add rule for `main` branch**:
   - Require pull request reviews
   - Require status checks to pass
   - Include administrators
   - Allow force pushes: Disabled
   - Allow deletions: Disabled

## 🧪 Testing

### Run Test Suite

```bash
# Install dependencies
npm install

# Run webhook tests
node scripts/test-webhook.js

# Set custom webhook URL
WEBHOOK_URL="https://your-function.azurewebsites.net/api/webhook" \
WEBHOOK_SECRET="your-secret" \
node scripts/test-webhook.js
```

### Manual Testing

```bash
# Test GitHub Actions webhook
gh workflow run "autonomy Webhook Receiver" \
  -f test_payload='{"device_id":"test","fw":"RUTX_R_00.07.17","severity":"critical","scenario":"daemon_down","note":"Test","overlay_pct":95,"mem_avail_mb":25,"load1":4.2,"ubus_ok":false,"actions":["restart"],"ts":1705776000}'
```

## 📊 Monitoring

### GitHub Actions Metrics

- **Go to Actions** → autonomy Webhook Receiver
- **View run history** and success rates
- **Check logs** for any errors

### Azure Function Metrics

- **Go to Azure Portal** → Your Function App
- **Monitor** → Metrics
- **Add metrics**:
  - Function execution count
  - Function execution units
  - Response time
  - Error rate

### Logs

#### GitHub Actions Logs

```bash
# View recent workflow runs
gh run list --workflow="autonomy Webhook Receiver" --limit 10

# View specific run logs
gh run view RUN_ID --log
```

#### Azure Function Logs

```bash
# View function logs
az webapp log tail --name YOUR_FUNCTION_APP --resource-group autonomy-webhook

# Download logs
az webapp log download --name YOUR_FUNCTION_APP --resource-group autonomy-webhook
```

## 🔒 Security

### HMAC Validation

The webhook receiver validates HMAC signatures:

```bash
# Generate HMAC signature
echo -n '{"test":"payload"}' | openssl dgst -sha256 -hmac "your-secret" -binary | xxd -p -c 256
```

### Rate Limiting

- **GitHub Actions**: 1000 requests per hour per repository
- **Azure Functions**: 1 million executions per month (free tier)

### Access Control

- **GitHub Token**: Use minimal required permissions
- **Azure Function**: Use managed identity when possible
- **Webhook Secret**: Keep secure and rotate regularly

## 🚨 Troubleshooting

### Common Issues

#### 1. HMAC Validation Fails

```bash
# Check signature generation
echo -n "$PAYLOAD" | openssl dgst -sha256 -hmac "$SECRET" -binary | xxd -p -c 256
```

#### 2. GitHub Token Issues

```bash
# Test token permissions
curl -H "Authorization: token YOUR_TOKEN" \
  https://api.github.com/user
```

#### 3. Azure Function Not Responding

```bash
# Check function status
az functionapp show --name YOUR_FUNCTION_APP --resource-group autonomy-webhook

# Check function logs
az webapp log tail --name YOUR_FUNCTION_APP --resource-group autonomy-webhook
```

#### 4. Issue Creation Fails

- **Check GitHub token permissions**
- **Verify repository access**
- **Check issue creation limits**

### Debug Mode

Enable debug logging:

```bash
# GitHub Actions
# Add to workflow environment variables:
DEBUG: "true"

# Azure Function
# Add to application settings:
DEBUG: "true"
```

## 📈 Performance Optimization

### GitHub Actions

- **Use caching** for dependencies
- **Optimize workflow** to run only when needed
- **Monitor execution time**

### Azure Function

- **Use consumption plan** for cost optimization
- **Enable application insights** for monitoring
- **Configure auto-scaling** if needed

## 🔄 Updates and Maintenance

### Updating the Webhook Receiver

```bash
# Pull latest changes
git pull origin main

# Update dependencies (Azure Function)
cd azure/webhook-function
npm update

# Redeploy (if needed)
az functionapp deployment source config-zip \
  --resource-group autonomy-webhook \
  --name YOUR_FUNCTION_APP \
  --src dist.zip
```

### Monitoring and Alerts

Set up monitoring for:

- **Webhook receiver availability**
- **Issue creation success rate**
- **Response time metrics**
- **Error rates**

## 📞 Support

For issues with the webhook receiver:

1. **Check the logs** (GitHub Actions or Azure Function)
2. **Run the test suite** to validate functionality
3. **Verify configuration** (secrets, tokens, permissions)
4. **Create an issue** in this repository

---

**Last Updated**: 2025-01-20
**Version**: 1.0.0
