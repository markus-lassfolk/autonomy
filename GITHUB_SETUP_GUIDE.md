# 🔧 GitHub Configuration Setup Guide

This guide will help you configure all required secrets and variables for the autonomous workflow system.

## 🚨 **Critical Issues Found**

Your verification script found these issues that need immediate attention:

1. ❌ **GitHub Actions are DISABLED** - This is blocking all workflows
2. ❌ **GITHUB_TOKEN secret is missing** - Required for workflow operations
3. ⚠️ **13 workflows are failing** - Likely due to the above issues

## 🔧 **Step-by-Step Fix Instructions**

### **Step 1: Enable GitHub Actions (CRITICAL)**

**This is the most important step - without this, no workflows will run!**

1. Go to your repository: https://github.com/markus-lassfolk/autonomy
2. Click **"Settings"** (top menu bar)
3. Click **"Actions"** in the left sidebar → **"General"**
4. Under **"Actions permissions"**, select:
   - ✅ **"Allow all actions and reusable workflows"**
5. Under **"Workflow permissions"**, select:
   - ✅ **"Read and write permissions"**
   - ✅ **"Allow GitHub Actions to create and approve pull requests"**
6. Click **"Save"**

### **Step 2: Add Missing GITHUB_TOKEN Secret**

1. **Create Personal Access Token:**
   - Go to: https://github.com/settings/tokens
   - Click **"Generate new token"** → **"Generate new token (classic)"**
   - Set **Name**: "Autonomy Workflows"
   - Set **Expiration**: 90 days (or "No expiration" for convenience)
   - Select these **scopes**:
     - ✅ `repo` (Full control of private repositories)
     - ✅ `workflow` (Update GitHub Action workflows)
     - ✅ `write:packages` (Upload packages to GitHub Package Registry)
     - ✅ `read:packages` (Download packages from GitHub Package Registry)
     - ✅ `actions` (Access to GitHub Actions)
   - Click **"Generate token"** and **copy the token**

2. **Add Token as Secret:**
   - Go to: https://github.com/markus-lassfolk/autonomy/settings/secrets/actions
   - Click **"New repository secret"**
   - **Name**: `GITHUB_TOKEN`
   - **Value**: [paste your token]
   - Click **"Add secret"**

### **Step 3: Verify Configuration**

Run the verification script to check your setup:

```powershell
.\scripts\verify-github-config.ps1
```

Then run the quick test:

```powershell
.\scripts\test-github-setup.ps1
```

## 📋 **Current Configuration Status**

### ✅ **Already Configured (Working)**

| Type | Name | Status | Description |
|------|------|--------|-------------|
| Secret | `WEBHOOK_SECRET` | ✅ Present | HMAC secret for webhook validation |
| Secret | `COPILOT_TOKEN` | ✅ Present | GitHub Copilot API access |
| Secret | `DOCKERHUB_USERNAME` | ✅ Present | Docker Hub username |
| Secret | `DOCKERHUB_TOKEN` | ✅ Present | Docker Hub access token |
| Variable | `SUPPORTED_VERSIONS` | ✅ Present | `RUTX_R_00.07.17,RUTX_R_00.07.18,RUTX_R_00.08.00` |
| Variable | `MIN_SEVERITY` | ✅ Present | `warn` |
| Variable | `COPILOT_ENABLED` | ✅ Present | `true` |
| Variable | `AUTO_ASSIGN` | ✅ Present | `true` |
| Variable | `BUILD_PLATFORMS` | ✅ Present | `linux/amd64,linux/arm64,linux/arm/v7` |
| Variable | `DOCKER_REGISTRY` | ✅ Present | `docker.io` |

### ❌ **Missing (Required)**

| Type | Name | Status | Action Required |
|------|------|--------|-----------------|
| Secret | `GITHUB_TOKEN` | ❌ Missing | **Add this secret (Step 2 above)** |
| Setting | GitHub Actions | ❌ Disabled | **Enable in repository settings (Step 1 above)** |

## 🚀 **What Happens After Setup**

Once you complete the setup:

1. **All 28 workflows will be enabled** and can run automatically
2. **Autonomous features will activate:**
   - 🔒 Automatic security scanning
   - ✨ Code quality checks and formatting
   - 🧪 Multi-platform testing (RUTOS/OpenWrt)
   - 🤖 AI-powered issue resolution with Copilot
   - 📦 Automated package building and publishing
   - 📊 Performance monitoring and benchmarking
   - 📚 Documentation automation

3. **Webhook system will be operational** for receiving alerts from your devices

## 🧪 **Testing Your Setup**

### Manual Workflow Testing

After setup, test individual workflows:

```bash
# Test security scanning
gh workflow run "security-scan.yml"

# Test code quality
gh workflow run "code-quality.yml"

# Test package building
gh workflow run "build-packages.yml"

# Test webhook receiver
gh workflow run "webhook-receiver.yml" --field test_payload='{"device_id":"test","severity":"info","scenario":"test","note":"Setup test"}'
```

### Monitor Workflow Runs

Check all workflow runs at: https://github.com/markus-lassfolk/autonomy/actions

## 🔍 **Troubleshooting**

### If workflows still fail after setup:

1. **Check workflow permissions:**
   - Repository Settings → Actions → General
   - Ensure "Read and write permissions" is selected

2. **Verify all secrets are present:**
   - Repository Settings → Secrets and variables → Actions
   - Should see: GITHUB_TOKEN, WEBHOOK_SECRET, COPILOT_TOKEN, DOCKERHUB_USERNAME, DOCKERHUB_TOKEN

3. **Check for specific error messages:**
   - Go to Actions tab → Click on failed workflow → View logs

### Common Issues:

- **"Resource not accessible by integration"** → Check workflow permissions
- **"Bad credentials"** → Regenerate GITHUB_TOKEN with correct scopes
- **"Actions are disabled"** → Enable Actions in repository settings

## 📞 **Support**

If you encounter issues:

1. Run the verification script: `.\scripts\verify-github-config.ps1`
2. Check the workflow logs in the Actions tab
3. Ensure all steps in this guide are completed exactly as described

## 🎉 **Success Indicators**

You'll know everything is working when:

- ✅ Verification script shows "All configurations are correct!"
- ✅ Test script shows all tests passing
- ✅ Workflows run successfully in the Actions tab
- ✅ No failed workflow runs in recent history

Your autonomous workflow system will then be **100% operational**! 🚀
