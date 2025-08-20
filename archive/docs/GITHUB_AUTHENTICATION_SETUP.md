# GitHub Authentication Setup for Autonomous System

## Overview

Your autonomous system can create GitHub issues using multiple authentication methods. Here's how to set it up properly.

## Quick Setup (Recommended)

### Option 1: Use Your Existing GitHub CLI Authentication

If you already use `gh auth login` for your PowerShell script:

```bash
# Check current authentication
gh auth status

# If authenticated, extract and save token for autonomous system
./setup-github-token-rutos.sh

# Test the setup
./github-auth-integration-rutos.sh test
```

### Option 2: Create Personal Access Token

1. **Go to GitHub Settings:**
   - Visit: https://github.com/settings/personal-access-tokens/tokens
   - Click "Generate new token" → "Fine-grained personal access token"

2. **Configure Token:**
   - **Repository:** `markus-lassfolk/autonomy`
   - **Expiration:** 90 days (or longer for production)
   - **Permissions:**
     - Issues: `Read and write`
     - Metadata: `Read`
     - Pull requests: `Read and write` (for autonomous fixes)

3. **Save Token:**
   ```bash
   # Method A: Environment variable
   export GITHUB_TOKEN="your_token_here"
   echo 'export GITHUB_TOKEN="your_token_here"' >> ~/.bashrc
   
   # Method B: Use setup script
   ./setup-github-token-rutos.sh
   # Then paste your token when prompted
   ```

## Authentication Methods (Priority Order)

Your autonomous system checks authentication in this order:

### 1. Environment Variable (Highest Priority)
```bash
export GITHUB_TOKEN="ghp_xxxxxxxxxxxxxxxxxxxx"
```

### 2. Saved Token File
```bash
# Automatically created by setup script
/etc/autonomous-system/github-token
```

### 3. GitHub CLI Integration
```bash
# If you've run: gh auth login
gh auth token  # Used automatically
```

### 4. PowerShell Integration (Fallback)
```bash
# Uses your existing PowerShell script
automation/create-copilot-issues-optimized.ps1
```

## Testing Your Setup

### Test All Methods
```bash
./github-auth-integration-rutos.sh test
```

Expected output:
```
=== Testing All GitHub Authentication Methods ===
✓ Environment GITHUB_TOKEN is valid
✓ Saved token file is valid  
✓ GitHub CLI authentication is valid
✓ PowerShell and issue creation script available

Methods available: 4/4
✅ Multiple authentication methods available (redundancy: good)
```

### Test Issue Creation
```bash
# Test hybrid issue creation
./github-auth-integration-rutos.sh create-issue "Test error from setup"

# Test PowerShell integration
./github-auth-integration-rutos.sh powershell
```

### Test Autonomous Error Monitor
```bash
# This should work now with your token setup
./autonomous-system/autonomous-error-monitor-rutos.sh
```

## Integration with Your Autonomous System

### Current Script Updates

Your `autonomous-error-monitor-rutos.sh` now supports:

1. **Multiple Authentication Sources:** Checks environment, saved files, GitHub CLI
2. **Fallback to PowerShell:** Uses your existing PowerShell script if direct API fails
3. **Automatic Token Loading:** No manual configuration needed after setup

### Deployment Integration

Your bootstrap deployment scripts will now:

```bash
# During deployment, authentication is loaded automatically
curl -fsSL https://raw.githubusercontent.com/.../bootstrap-deploy-v3-rutos.sh | sh

# If GitHub token is available, error monitoring is automatic
# If not available, errors are still logged but no issues created
```

## Configuration Files

### Autonomous System Config
```bash
# /etc/autonomous-system/github-token
export GITHUB_TOKEN="ghp_xxxxxxxxxxxxxxxxxxxx"
```

### Auth Loader Script
```bash
# /etc/autonomous-system/load-github-auth.sh
#!/bin/sh
if [ -f "/etc/autonomous-system/github-token" ]; then
    . "/etc/autonomous-system/github-token"
    export GITHUB_TOKEN
else
    echo "ERROR: GitHub token not found" >&2
    exit 1
fi
```

## Security Best Practices

### Token Permissions
- ✅ **Minimal Scope:** Only repository-specific permissions
- ✅ **Fine-grained:** Use fine-grained tokens (not classic)
- ✅ **Expiration:** Set reasonable expiration dates
- ✅ **Monitoring:** GitHub notifies of token usage

### File Security
```bash
# Token files are automatically secured
ls -la /etc/autonomous-system/github-token
-rw------- 1 root root 123 Aug  1 12:34 github-token
```

### Environment Variables
```bash
# For development/testing
export GITHUB_TOKEN="token"

# For production (in systemd service files)
Environment=GITHUB_TOKEN=token
```

## Troubleshooting

### "GITHUB_TOKEN must be defined"
```bash
# Check authentication status
./github-auth-integration-rutos.sh test

# If no methods work, run setup
./setup-github-token-rutos.sh
```

### "GitHub CLI not authenticated"
```bash
# Authenticate with GitHub CLI
gh auth login

# Extract token for autonomous system
./setup-github-token-rutos.sh
```

### Token validation fails
```bash
# Test token manually
curl -H "Authorization: Bearer $GITHUB_TOKEN" \
  https://api.github.com/repos/markus-lassfolk/autonomy

# Should return repository information
```

### PowerShell fallback not working
```bash
# Check PowerShell availability
pwsh --version
# or
powershell --version

# Check script exists
ls -la automation/create-copilot-issues-optimized.ps1
```

## Production Deployment

### Option A: Environment Variable
```bash
# In your deployment script
export GITHUB_TOKEN="your_production_token"
curl -fsSL https://raw.githubusercontent.com/.../bootstrap-deploy-v3-rutos.sh | sh
```

### Option B: Pre-deployed Token
```bash
# Deploy token first
./setup-github-token-rutos.sh

# Then deploy autonomous system
curl -fsSL https://raw.githubusercontent.com/.../bootstrap-deploy-v3-rutos.sh | sh
```

### Option C: GitHub CLI on RUTOS Device
```bash
# If RUTOS device has GitHub CLI
gh auth login
./autonomous-system/autonomous-error-monitor-rutos.sh
```

## Integration Summary

✅ **Seamless:** Works with your existing PowerShell workflow  
✅ **Redundant:** Multiple authentication methods  
✅ **Secure:** Proper token storage and permissions  
✅ **Automatic:** No manual configuration needed after setup  
✅ **Backward Compatible:** Doesn't break existing scripts  

Your autonomous system now has robust GitHub integration that can create issues automatically when errors are detected, while falling back to your proven PowerShell approach if needed.
