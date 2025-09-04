# Security Guidelines

This document outlines security best practices for the autonomy project and how to handle sensitive information.

## 🔐 Security Best Practices

### 1. API Keys and Tokens

**Never commit API keys or tokens to version control.**

- Use environment variables for sensitive configuration
- Store API keys in secure configuration files
- Use placeholder values in examples and documentation

**Example:**
```bash
# ❌ WRONG - Hardcoded API key
option google_api_key 'your-actual-api-key-here'

# ✅ CORRECT - Environment variable or placeholder
option google_api_key '${GOOGLE_API_KEY}'
# or
option google_api_key 'your-api-key-here'
```

### 2. SSH Keys and Certificates

**Never commit SSH private keys or certificates.**

- Use environment variables for SSH key paths
- Store SSH keys outside the project directory
- Use placeholder paths in scripts

**Example:**
```bash
# ❌ WRONG - Hardcoded SSH key path
$SSH_KEY = "C:\path\to\your\private\key"

# ✅ CORRECT - Environment variable
$SSH_KEY = $env:SSH_KEY_PATH ?? "C:\path\to\your\ssh\key"
```

### 3. Passwords and Credentials

**Never commit passwords or credentials.**

- Use environment variables for passwords
- Store credentials in secure configuration files
- Use placeholder values in examples

**Example:**
```bash
# ❌ WRONG - Hardcoded password
EMAIL_PASSWORD="your-actual-password"

# ✅ CORRECT - Environment variable or placeholder
EMAIL_PASSWORD="${EMAIL_PASSWORD}"
# or
EMAIL_PASSWORD="your-app-password"
```

### 4. Network Information

**Be careful with internal network information.**

- Use placeholder IP addresses in examples
- Avoid exposing internal network structure
- Use configuration variables for network settings

**Example:**
```bash
# ❌ WRONG - Hardcoded internal IP
RUTOS_HOST = "your-router-ip"

# ✅ CORRECT - Environment variable or placeholder
RUTOS_HOST = $env:RUTOS_HOST ?? "192.168.1.1"
```

## 🛡️ Security Checklist

Before committing code, ensure:

- [ ] No API keys or tokens are hardcoded
- [ ] No SSH private keys are included
- [ ] No passwords or credentials are exposed
- [ ] No internal network IPs are hardcoded
- [ ] All examples use placeholder values
- [ ] Sensitive files are in `.gitignore`
- [ ] Environment variables are used for secrets

## 🔧 Configuration Management

### Environment Variables

Use environment variables for sensitive configuration:

```bash
# Set environment variables
export GOOGLE_API_KEY="your-actual-api-key"
export SSH_KEY_PATH="/path/to/your/ssh/key"
export RUTOS_HOST="192.168.1.1"

# Use in scripts
./scripts/deploy-production.sh --host $RUTOS_HOST --key $SSH_KEY_PATH
```

### Configuration Files

Store sensitive configuration in separate files:

```bash
# configs/autonomy.local (not committed)
option google_api_key 'your-actual-api-key'
option email_password 'your-actual-password'

# configs/autonomy.example (committed)
option google_api_key 'your-api-key-here'
option email_password 'your-app-password'
```

## 🚨 Incident Response

If you accidentally commit sensitive information:

1. **Immediate Actions:**
   - Revoke/rotate the exposed credentials immediately
   - Remove the sensitive data from the commit history
   - Notify affected parties

2. **Clean Up:**
   - Use `git filter-branch` or `git filter-repo` to remove sensitive data
   - Force push to update remote repository
   - Update any documentation that referenced the exposed data

3. **Prevention:**
   - Review and update security practices
   - Add additional checks to CI/CD pipeline
   - Update `.gitignore` if needed

## 🔍 Security Scanning

### Pre-commit Checks

The project includes security scanning in CI/CD:

- Automated detection of potential secrets
- Validation of configuration files
- Scanning for hardcoded credentials

### Manual Scanning

Before committing, run security checks:

```bash
# Check for potential secrets
grep -r -i "password\|token\|key\|secret" . --exclude-dir=.git

# Check for hardcoded IPs
grep -r "192\.168\|10\.0\|172\.16" . --exclude-dir=.git

# Check for personal file paths
grep -r "C:\\Users\\" . --exclude-dir=.git
```

## 📋 Security Configuration

### Required Environment Variables

Set these environment variables for production deployment:

```bash
# API Keys
export GOOGLE_API_KEY="your-google-api-key"
export OPENCELLID_API_KEY="your-opencellid-api-key"

# SSH Configuration
export SSH_KEY_PATH="/path/to/your/ssh/key"
export RUTOS_HOST="your-router-ip"

# Email Configuration
export EMAIL_PASSWORD="your-email-app-password"

# Notification Tokens
export PUSHOVER_TOKEN="your-pushover-token"
export TELEGRAM_BOT_TOKEN="your-telegram-bot-token"
```

### Secure Configuration Files

Create secure configuration files (not committed):

```bash
# configs/autonomy.production
config autonomy 'main'
    option google_api_key '${GOOGLE_API_KEY}'
    option opencellid_api_key '${OPENCELLID_API_KEY}'
    option email_password '${EMAIL_PASSWORD}'
```

## 🔒 Additional Security Measures

### File Permissions

Ensure proper file permissions:

```bash
# SSH keys should be readable only by owner
chmod 600 ~/.ssh/id_rsa

# Configuration files should be readable only by owner
chmod 600 configs/autonomy.production
```

### Network Security

- Use SSH key-based authentication
- Disable password authentication
- Use non-standard SSH ports
- Implement fail2ban for SSH protection

### Monitoring and Logging

- Monitor for unauthorized access attempts
- Log security events
- Regularly review access logs
- Implement alerting for suspicious activity

## 📞 Security Contacts

If you discover a security vulnerability:

1. **Do not** create a public issue
2. **Do not** discuss in public channels
3. Contact the maintainers privately
4. Provide detailed information about the vulnerability

## 📚 Additional Resources

- [GitHub Security Best Practices](https://docs.github.com/en/github/authenticating-to-github/keeping-your-account-and-data-secure)
- [OpenWrt Security Guidelines](https://openwrt.org/docs/guide-user/security)
- [SSH Security Best Practices](https://www.ssh.com/academy/ssh/security)
