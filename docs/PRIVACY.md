# Privacy and Security

This document explains the privacy and security features of autonomy's error reporting and auto-update system.

## Overview

autonomy includes an opt-in, privacy-safe error reporting system that helps improve the software by collecting anonymous diagnostic information. All privacy features are **disabled by default** and require explicit user consent to enable.

## Privacy Features

### Opt-in Design

- **Disabled by Default**: All reporting features are disabled by default
- **Explicit Consent**: Users must manually enable reporting in configuration
- **Granular Control**: Users can control what information is shared
- **Easy Disable**: Can be disabled at any time without affecting core functionality

### Data Anonymization

#### Device Identification
- **Raw Device IDs**: Never sent in their original form
- **Hashed Identifiers**: Device IDs are hashed with a secret salt
- **Truncated Output**: Only first 12 characters of hash are used
- **Reversible**: Same device always produces same hash (for deduplication)

#### Personal Information Sanitization
The system automatically sanitizes sensitive information from error reports:

- **IP Addresses**: Replaced with `[ip]`
- **MAC Addresses**: Replaced with `[mac]`
- **WiFi SSIDs**: Replaced with `[redacted]`
- **Passwords/Tokens**: Replaced with `[redacted]`
- **Phone Numbers**: Replaced with `[phone]`
- **File Paths**: Collapsed to `[path]/...` (in strict mode)
- **Usernames**: Replaced with `[user]` (in strict mode)

### Privacy Levels

#### Standard Privacy
- Masks IP addresses, MAC addresses, and credentials
- Preserves file paths and usernames for debugging
- Suitable for most users

#### Strict Privacy
- Additional masking of file paths and usernames
- Maximum privacy protection
- May reduce debugging effectiveness

## Data Collection

### What We Collect (When Enabled)

#### Error Reports
- **System Metrics**: Memory usage, disk usage, load average
- **Error Details**: Error type, severity, actions taken
- **Firmware Version**: For issue correlation
- **Anonymized Device ID**: For deduplication only

#### Auto-Update Notifications
- **Update Status**: Success/failure of updates
- **Version Information**: Previous and new versions
- **System Health**: Basic metrics during update

### What We Never Collect

- **Personal Files**: Never access user files or data
- **Network Traffic**: Never monitor or log network content
- **Configuration Details**: Never send full configuration files
- **Log Files**: Never send complete log files (unless explicitly requested)
- **Location Data**: Never collect GPS or location information
- **User Behavior**: Never track user actions or preferences

## Security Measures

### Authentication
- **HMAC Signing**: All webhook payloads are cryptographically signed
- **Secret Keys**: Each device uses a unique secret for signing
- **Timestamp Validation**: Prevents replay attacks
- **GitHub App**: Server-side uses GitHub App for least privilege access

### Data Transmission
- **HTTPS Only**: All communication uses encrypted HTTPS
- **Certificate Validation**: Validates server certificates
- **Timeout Protection**: Prevents hanging connections
- **Rate Limiting**: Prevents abuse and spam

### Server-Side Security
- **Input Validation**: All inputs are validated and sanitized
- **Deduplication**: Prevents duplicate issue spam
- **Generic Labels**: Uses non-identifying issue labels
- **Access Control**: GitHub App has minimal required permissions

## Configuration

### Privacy Settings

Edit `/etc/autonomy/watch.conf` to configure privacy settings:

```bash
# Enable/disable error reporting (default: disabled)
REPORTING_ENABLED=0

# Anonymize device ID (default: enabled)
ANONYMIZE_DEVICE_ID=1

# Privacy level: standard|strict (default: standard)
PRIVACY_LEVEL=standard

# Include diagnostic bundles (default: disabled)
INCLUDE_DIAGNOSTICS=0

# Enable auto-updates (default: disabled)
AUTO_UPDATE_ENABLED=0

# Update channel: stable|beta (default: stable)
UPDATE_CHANNEL=stable
```

### LuCI Web Interface

The LuCI web interface provides easy toggles for all privacy settings:

1. Navigate to **System** → **autonomy** → **Settings**
2. Configure privacy and reporting options
3. Changes are applied immediately

## Data Retention

### Client-Side
- **Error Reports**: Not stored locally
- **Update Logs**: Stored in `/var/log/autonomy-update.log`
- **Configuration**: Stored in `/etc/autonomy/watch.conf`

### Server-Side
- **GitHub Issues**: Follow GitHub's data retention policies
- **Webhook Logs**: Not retained beyond processing
- **Analytics**: No analytics or tracking data collected

## Third-Party Services

### GitHub
- **Issue Creation**: Error reports create GitHub issues
- **Data Processing**: GitHub processes issue data according to their privacy policy
- **Access Control**: Only issue creation/commenting permissions

### No Other Services
- **No Analytics**: No Google Analytics, Mixpanel, etc.
- **No Tracking**: No user behavior tracking
- **No Advertising**: No ad networks or tracking pixels

## Compliance

### GDPR
- **Data Minimization**: Only collects necessary data
- **User Consent**: Explicit opt-in required
- **Right to Deletion**: Users can disable reporting at any time
- **Data Portability**: All data is in standard formats

### CCPA
- **Opt-out Rights**: Users can opt-out at any time
- **Data Disclosure**: Clear documentation of data collection
- **No Sale**: Data is never sold to third parties

## Transparency

### Open Source
- **Full Source Code**: All code is open source and auditable
- **No Hidden Features**: No undisclosed data collection
- **Community Review**: Code is reviewed by the community

### Documentation
- **Clear Policies**: This document explains all data practices
- **Configuration Guide**: Step-by-step configuration instructions
- **FAQ**: Common privacy questions and answers

## Reporting Issues

### Privacy Concerns
If you have privacy concerns:

1. **Disable Reporting**: Set `REPORTING_ENABLED=0` in configuration
2. **Contact Us**: Open a GitHub issue with privacy concerns
3. **Review Code**: All code is open source for review

### Security Issues
For security issues:

1. **Private Report**: Use GitHub's private security reporting
2. **Responsible Disclosure**: We follow responsible disclosure practices
3. **Quick Response**: Security issues are addressed promptly

## Updates

This privacy policy may be updated as the software evolves. Users will be notified of significant changes through:

- **Release Notes**: Updated with privacy changes
- **GitHub Issues**: Notifications for major changes
- **Documentation**: Updated documentation

## Contact

For privacy questions or concerns:

- **GitHub Issues**: Open an issue in the repository
- **Documentation**: Check this document and other docs
- **Code Review**: Review the source code directly

---

*Last updated: 2025-08-20*
