# Automatic Error Reporting to GitHub - Design Document

## 🎯 Overview

This document outlines the design for implementing automatic error reporting from the autonomy daemon to GitHub, creating issues with crash details, diagnostic information, and enabling autonomous issue resolution through GitHub Copilot integration.

## 🏗️ System Architecture

### High-Level Flow
```
[Autonomy Daemon] → [Crash Detection] → [Webhook Client] → [GitHub Actions/Azure Function] → [GitHub Issues API] → [GitHub Issue Created]
```

### Components

1. **Crash Detection & Reporting** (Daemon-side)
   - Enhanced crash handlers with detailed context
   - Diagnostic bundle generation
   - Webhook payload preparation
   - Secure transmission with HMAC signatures

2. **Webhook Receiver** (Server-side)
   - GitHub Actions workflow or Azure Function
   - HMAC signature validation
   - Intelligent filtering and deduplication
   - GitHub Issues API integration

3. **GitHub Integration**
   - Automatic issue creation with templates
   - Label assignment and categorization
   - GitHub Copilot integration for autonomous fixes
   - Diagnostic bundle attachment

## 📋 Implementation Requirements

### Prerequisites

#### 1. GitHub Repository Setup
- **Repository**: Must have Issues enabled
- **GitHub Token**: Personal Access Token with `repo` and `workflow` permissions
- **Webhook Secret**: HMAC secret for secure communication
- **GitHub Actions**: Enabled for workflow-based receiver

#### 2. Daemon Configuration
- **Webhook URL**: Configured in autonomy daemon config
- **HMAC Secret**: Same secret used by both daemon and receiver
- **Error Reporting**: Enabled in daemon settings
- **Diagnostic Collection**: Enabled for bundle generation

#### 3. Network Requirements
- **Outbound HTTPS**: Daemon must reach GitHub API (api.github.com)
- **Firewall**: Allow HTTPS traffic on port 443
- **DNS Resolution**: Reliable DNS for GitHub domains

### Technical Limitations

#### 1. GitHub API Rate Limits
- **Authenticated Requests**: 5,000 requests/hour
- **Issue Creation**: ~1,667 issues/hour (3 requests per issue)
- **File Uploads**: 1,000 requests/hour for diagnostic bundles
- **Mitigation**: Implement exponential backoff and request queuing

#### 2. Network Constraints
- **Embedded Router**: Limited bandwidth and processing power
- **Connection Timeouts**: Must handle network failures gracefully
- **Retry Logic**: Implement retry with exponential backoff
- **Offline Mode**: Queue reports for later transmission

#### 3. Storage Limitations
- **Diagnostic Bundles**: Size limits (GitHub: 100MB per file)
- **Local Storage**: Limited space on router for crash dumps
- **Cleanup**: Automatic cleanup of old diagnostic files

#### 4. Privacy & Security
- **Data Sensitivity**: Crash dumps may contain sensitive information
- **User Consent**: Opt-in error reporting with clear disclosure
- **Data Retention**: Automatic cleanup of old issues and attachments
- **Anonymization**: Remove or hash sensitive data before transmission

## 🔧 Implementation Design

### Phase 1: Enhanced Crash Detection

#### 1.1 Crash Handler Enhancement
```c
// Enhanced crash handler with GitHub reporting
static void enhanced_crash_handler(int sig, siginfo_t *info, void *context) {
    // Existing crash detection code...
    
    // Generate diagnostic bundle
    diagnostic_bundle_t bundle = generate_diagnostic_bundle(sig, info, context);
    
    // Prepare webhook payload
    webhook_payload_t payload = prepare_github_issue_payload(sig, info, context, &bundle);
    
    // Send to GitHub (async, non-blocking)
    send_error_report_async(&payload);
    
    // Continue with existing crash handling...
}
```

#### 1.2 Diagnostic Bundle Generation
```c
typedef struct {
    char bundle_path[256];
    char bundle_id[64];
    size_t bundle_size;
    time_t created_time;
    bool uploaded;
} diagnostic_bundle_t;

diagnostic_bundle_t generate_diagnostic_bundle(int sig, siginfo_t *info, void *context) {
    // Create unique bundle ID
    char bundle_id[64];
    snprintf(bundle_id, sizeof(bundle_id), "crash-%s-%ld", 
             get_device_id(), time(NULL));
    
    // Generate bundle contents:
    // - Crash context and registers
    // - Stack trace (if available)
    // - Memory map
    // - System logs (last 100 lines)
    // - Configuration files (sanitized)
    // - Network status
    // - Process list
    // - Disk usage
    
    return bundle;
}
```

#### 1.3 Webhook Payload Preparation
```c
typedef struct {
    char device_id[64];
    char firmware_version[32];
    char crash_type[32];
    char crash_address[32];
    char stack_trace[4096];
    char memory_map[8192];
    char system_info[1024];
    char bundle_id[64];
    time_t timestamp;
    int severity; // 1=info, 2=warn, 3=critical
} github_issue_payload_t;
```

### Phase 2: Webhook Receiver Implementation

#### 2.1 GitHub Actions Workflow
```yaml
# .github/workflows/error-reporting-receiver.yml
name: Error Reporting Receiver

on:
  repository_dispatch:
    types: [autonomy-error-report]
  workflow_dispatch:
    inputs:
      test_payload:
        description: 'Test payload JSON'
        required: false

jobs:
  process-error-report:
    runs-on: ubuntu-latest
    steps:
      - name: Validate HMAC Signature
        run: |
          # Validate webhook signature
          # Extract payload
          # Verify authenticity
      
      - name: Filter and Deduplicate
        run: |
          # Check version support
          # Deduplicate by device+crash+time
          # Apply severity filters
      
      - name: Create GitHub Issue
        run: |
          # Generate issue title and body
          # Assign appropriate labels
          # Create issue via GitHub API
      
      - name: Upload Diagnostic Bundle
        run: |
          # Download bundle from daemon
          # Upload as issue attachment
          # Clean up temporary files
      
      - name: Notify GitHub Copilot
        run: |
          # Add Copilot-specific labels
          # Trigger autonomous analysis
```

#### 2.2 Azure Function Alternative
```javascript
// Azure Function: error-reporting-receiver
module.exports = async function (context, req) {
    // Validate HMAC signature
    const isValid = validateSignature(req.body, req.headers['x-autonomy-signature']);
    if (!isValid) {
        context.res = { status: 401, body: 'Invalid signature' };
        return;
    }
    
    // Process error report
    const issueId = await createGitHubIssue(req.body);
    
    // Upload diagnostic bundle
    await uploadDiagnosticBundle(req.body.bundle_id, issueId);
    
    context.res = { status: 200, body: { issue_id: issueId } };
};
```

### Phase 3: GitHub Integration

#### 3.1 Issue Template
```markdown
## 🚨 Autonomy Daemon Crash Report

**Device ID**: {{device_id}}  
**Firmware**: {{firmware_version}}  
**Crash Type**: {{crash_type}}  
**Severity**: {{severity}}  
**Timestamp**: {{timestamp}}  

### 🔍 Crash Details
- **Signal**: {{signal}} ({{signal_name}})
- **Fault Address**: {{fault_address}}
- **Process ID**: {{pid}}
- **Thread ID**: {{tid}}

### 📊 System Status
- **Memory Usage**: {{memory_usage}}%
- **Load Average**: {{load_avg}}
- **Disk Usage**: {{disk_usage}}%
- **Network Status**: {{network_status}}

### 📋 Stack Trace
```
{{stack_trace}}
```

### 🗺️ Memory Map
```
{{memory_map}}
```

### 📦 Diagnostic Bundle
- **Bundle ID**: {{bundle_id}}
- **Size**: {{bundle_size}} bytes
- **Download**: [Diagnostic Bundle]({{bundle_url}})

### 🤖 Copilot Analysis
<!-- GitHub Copilot will analyze this crash and suggest fixes -->

---
*Auto-generated by Autonomy Error Reporting System*
```

#### 3.2 Label Strategy
```yaml
# Automatic label assignment
labels:
  - "bug"
  - "crash"
  - "severity:{{severity}}"
  - "device:{{device_type}}"
  - "firmware:{{firmware_version}}"
  - "component:{{crash_component}}"
  - "copilot:analyze"
```

## 🔒 Security Considerations

### 1. Authentication & Authorization
- **HMAC Signatures**: All webhook requests must include valid HMAC-SHA256 signature
- **GitHub Token**: Stored as repository secret, minimal required permissions
- **Rate Limiting**: Implement rate limiting to prevent abuse
- **IP Whitelisting**: Optional IP whitelist for webhook endpoints

### 2. Data Privacy
- **User Consent**: Clear opt-in mechanism with privacy disclosure
- **Data Minimization**: Only collect necessary diagnostic information
- **Sensitive Data**: Remove/hash passwords, keys, and personal information
- **Retention Policy**: Automatic cleanup of old issues and attachments

### 3. Network Security
- **HTTPS Only**: All communications over TLS 1.2+
- **Certificate Validation**: Proper SSL certificate validation
- **Timeout Handling**: Reasonable timeouts to prevent hanging connections
- **Error Handling**: Graceful handling of network failures

## 📊 Monitoring & Analytics

### 1. Success Metrics
- **Issue Creation Rate**: % of valid crashes that create issues
- **Response Time**: Time from crash to issue creation
- **False Positive Rate**: % of issues that are configuration errors
- **Resolution Rate**: % of issues resolved by Copilot

### 2. Failure Handling
- **Network Failures**: Retry with exponential backoff
- **API Rate Limits**: Queue requests and retry later
- **Invalid Signatures**: Log and reject malicious requests
- **Storage Full**: Cleanup old diagnostic bundles

### 3. Dashboard Metrics
- **Daily Crash Count**: Track crash frequency
- **Top Crash Types**: Most common crash patterns
- **Device Distribution**: Crashes by device type/firmware
- **Resolution Time**: Average time to fix issues

## 🚀 Deployment Strategy

### Phase 1: Foundation (Week 1-2)
1. **Enhanced Crash Detection**
   - Implement diagnostic bundle generation
   - Add webhook payload preparation
   - Integrate with existing crash handlers

2. **Basic Webhook Client**
   - Extend existing webhook client
   - Add GitHub-specific payload formatting
   - Implement secure transmission

### Phase 2: Server-Side (Week 3-4)
1. **GitHub Actions Workflow**
   - Create webhook receiver workflow
   - Implement HMAC validation
   - Add issue creation logic

2. **Testing & Validation**
   - Test with simulated crashes
   - Validate issue creation
   - Test diagnostic bundle uploads

### Phase 3: Integration (Week 5-6)
1. **GitHub Copilot Integration**
   - Add Copilot-specific labels
   - Create analysis templates
   - Test autonomous issue resolution

2. **Production Deployment**
   - Deploy to production routers
   - Monitor success rates
   - Fine-tune filtering rules

## 🔧 Configuration Options

### Daemon Configuration
```ini
[error_reporting]
enabled = true
github_webhook_url = https://api.github.com/repos/owner/repo/dispatches
github_token = ghp_xxxxxxxxxxxx
webhook_secret = your-hmac-secret
max_bundle_size = 50MB
retention_days = 30
min_severity = warn
include_system_logs = true
include_config_files = false
anonymize_data = true
```

### GitHub Repository Settings
```yaml
# Repository secrets
WEBHOOK_SECRET: "your-hmac-secret"
GITHUB_TOKEN: "ghp_xxxxxxxxxxxx"

# Repository variables
SUPPORTED_VERSIONS: "RUTX_R_00.07.17,RUTX_R_00.07.18"
MIN_SEVERITY: "warn"
MAX_ISSUES_PER_DAY: "100"
```

## 🧪 Testing Strategy

### 1. Unit Tests
- **Crash Handler**: Test crash detection and bundle generation
- **Webhook Client**: Test payload preparation and transmission
- **HMAC Validation**: Test signature generation and validation
- **Issue Creation**: Test GitHub API integration

### 2. Integration Tests
- **End-to-End**: Full flow from crash to issue creation
- **Network Failures**: Test retry logic and error handling
- **Rate Limiting**: Test GitHub API rate limit handling
- **Security**: Test HMAC validation and malicious requests

### 3. Load Testing
- **High Crash Volume**: Test with multiple simultaneous crashes
- **Large Bundles**: Test with maximum size diagnostic bundles
- **API Limits**: Test GitHub API rate limit behavior
- **Memory Usage**: Test memory consumption during bundle generation

## 📈 Success Criteria

### Functional Requirements
- ✅ **Crash Detection**: 99% of crashes detected and reported
- ✅ **Issue Creation**: 95% of valid crashes create GitHub issues
- ✅ **Bundle Upload**: 90% of diagnostic bundles successfully uploaded
- ✅ **Security**: 100% of requests properly authenticated

### Performance Requirements
- ✅ **Response Time**: <30 seconds from crash to issue creation
- ✅ **Bundle Size**: <50MB per diagnostic bundle
- ✅ **Memory Usage**: <10MB additional memory for error reporting
- ✅ **Network Usage**: <1MB per crash report

### Quality Requirements
- ✅ **False Positive Rate**: <5% of issues are configuration errors
- ✅ **Data Privacy**: 100% compliance with privacy requirements
- ✅ **Uptime**: 99.9% availability of error reporting system
- ✅ **Resolution Rate**: 80% of issues resolved by Copilot

## 🔄 Future Enhancements

### 1. Advanced Analytics
- **Crash Pattern Analysis**: Identify common crash patterns
- **Predictive Alerts**: Predict crashes before they happen
- **Trend Analysis**: Track crash trends over time
- **Device Health Scoring**: Overall device health metrics

### 2. Enhanced Automation
- **Auto-Fix Integration**: Automatic fixes for common issues
- **Configuration Validation**: Validate configurations before deployment
- **Proactive Monitoring**: Monitor for crash precursors
- **Smart Filtering**: ML-based filtering of irrelevant reports

### 3. Multi-Platform Support
- **Other Routers**: Support for additional router platforms
- **Cloud Integration**: Integration with cloud monitoring services
- **Mobile Apps**: Mobile app for crash report management
- **API Access**: Public API for third-party integrations

## 📚 References

- [GitHub Issues API Documentation](https://docs.github.com/en/rest/issues/issues)
- [GitHub Actions Documentation](https://docs.github.com/en/actions)
- [HMAC Authentication Best Practices](https://tools.ietf.org/html/rfc2104)
- [Azure Functions Documentation](https://docs.microsoft.com/en-us/azure/azure-functions/)
- [OpenWrt Crash Handling](https://openwrt.org/docs/guide-user/troubleshooting/crash_analysis)

---

**Document Version**: 1.0  
**Last Updated**: 2025-09-13  
**Status**: Design Phase  
**Next Review**: 2025-10-13
