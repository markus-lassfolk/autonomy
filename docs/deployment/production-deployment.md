# autonomy Production Deployment Guide

A comprehensive guide for deploying autonomy in production environments with automated deployment, monitoring, and disaster recovery procedures.

## Table of Contents

1. [Overview](#overview)
2. [Prerequisites](#prerequisites)
3. [Deployment Automation](#deployment-automation)
4. [Monitoring Setup](#monitoring-setup)
5. [Backup and Recovery](#backup-and-recovery)
6. [Production Deployment Workflow](#production-deployment-workflow)
7. [Troubleshooting](#troubleshooting)
8. [Maintenance Procedures](#maintenance-procedures)

## Overview

This guide covers the production deployment of autonomy using automated scripts that provide:

- **Automated Deployment**: One-command deployment with validation and rollback
- **Comprehensive Monitoring**: Health checks, alerting, and dashboards
- **Disaster Recovery**: Automated backup and recovery procedures
- **Production Safety**: Validation, testing, and rollback capabilities

## Prerequisites

### Development Environment

- **Go 1.22+** - Required for building from source
- **SSH/SCP** - For remote deployment
- **Bash** - For running deployment scripts
- **Git** - For version control

### Target System Requirements

- **RutOS** (Teltonika) or **OpenWrt** (modern releases)
- **SSH access** with key-based authentication (recommended)
- **mwan3** package installed (recommended)
- **ubus** available (required)
- **procd** init system (required)
- **Minimum 32MB RAM**, recommended 64MB+
- **Minimum 8MB flash**, recommended 16MB+

### Network Requirements

- **SSH access** to target router
- **Internet connectivity** for building and downloading dependencies
- **Stable network connection** during deployment

## Deployment Automation

### Production Deployment Script

The `scripts/deploy-production.sh` script provides comprehensive deployment automation with built-in safety features.

#### Basic Usage

```bash
# Deploy to a router with SSH key authentication
./scripts/deploy-production.sh --host 192.168.1.1 --key ~/.ssh/id_rsa

# Deploy with custom user
./scripts/deploy-production.sh --host router.example.com --user admin --key ~/.ssh/id_rsa

# Dry run to see what would be done
./scripts/deploy-production.sh --host 192.168.1.1 --key ~/.ssh/id_rsa --dry-run
```

#### Advanced Options

```bash
# Deploy with custom configuration
./scripts/deploy-production.sh \
  --host 192.168.1.1 \
  --key ~/.ssh/id_rsa \
  --binary autonomyd-custom \
  --config /etc/config/autonomy-custom \
  --service autonomy-custom

# Deploy without backup (not recommended for production)
./scripts/deploy-production.sh \
  --host 192.168.1.1 \
  --key ~/.ssh/id_rsa \
  --no-backup

# Deploy without monitoring setup
./scripts/deploy-production.sh \
  --host 192.168.1.1 \
  --key ~/.ssh/id_rsa \
  --no-monitoring
```

#### Deployment Process

The deployment script performs the following steps:

1. **Environment Initialization**
   - Creates necessary directories
   - Validates required tools
   - Sets up logging

2. **Target Validation**
   - Tests SSH connectivity
   - Checks system requirements
   - Validates OS and architecture

3. **Binary Building**
   - Uses existing build script
   - Cross-compiles for target architecture
   - Validates build output

4. **Configuration Backup**
   - Creates timestamped backup
   - Includes configuration, binary, and scripts
   - Stores backup locally

5. **File Deployment**
   - Stops existing service
   - Deploys binary and scripts
   - Sets proper permissions

6. **Deployment Validation**
   - Tests binary functionality
   - Validates file permissions
   - Checks service configuration

7. **Service Startup**
   - Starts autonomy service
   - Waits for service to initialize
   - Validates service status

8. **Monitoring Setup**
   - Deploys monitoring scripts
   - Configures health checks
   - Sets up cron jobs

9. **Health Check**
   - Performs comprehensive health check
   - Validates ubus interface
   - Tests API functionality

10. **Rollback Capability**
    - Automatic rollback on failure
    - Restores from backup
    - Maintains system stability

#### Safety Features

- **Automatic Backup**: Creates backup before deployment
- **Validation**: Multiple validation steps throughout process
- **Rollback**: Automatic rollback on any failure
- **Dry Run**: Test deployment without making changes
- **Logging**: Comprehensive logging of all operations

## Monitoring Setup

### Monitoring Configuration Script

The `scripts/monitoring-setup.sh` script configures comprehensive monitoring for autonomy.

#### Basic Usage

```bash
# Setup complete monitoring
./scripts/monitoring-setup.sh --host 192.168.1.1 --key ~/.ssh/id_rsa

# Setup monitoring without dashboard
./scripts/monitoring-setup.sh --host 192.168.1.1 --key ~/.ssh/id_rsa --no-dashboard

# Dry run to see monitoring setup
./scripts/monitoring-setup.sh --host 192.168.1.1 --key ~/.ssh/id_rsa --dry-run
```

#### Monitoring Components

1. **Health Check Script**
   - Service status monitoring
   - System resource monitoring
   - Network interface monitoring
   - ubus interface validation

2. **Alert Configuration**
   - Email notifications
   - Pushover notifications
   - Slack/Discord webhooks
   - Telegram notifications
   - Custom webhook support

3. **Cron Jobs**
   - Health checks every 2 minutes
   - System status every 5 minutes
   - Performance monitoring every 10 minutes
   - Log rotation daily
   - Configuration backup daily

4. **Dashboard**
   - Web-based status dashboard
   - Real-time metrics display
   - Service status overview
   - Performance indicators

#### Alert Configuration

Edit `/etc/autonomy/alerts.conf` to configure notifications:

```bash
# Email notifications
EMAIL_ENABLED=true
EMAIL_SMTP_SERVER="smtp.gmail.com"
EMAIL_SMTP_PORT=587
EMAIL_USERNAME="your-email@gmail.com"
EMAIL_PASSWORD="your-app-password"
EMAIL_FROM="autonomy@yourdomain.com"
EMAIL_TO="admin@yourdomain.com"

# Pushover notifications
PUSHOVER_ENABLED=true
PUSHOVER_TOKEN="your-pushover-token"
PUSHOVER_USER="your-pushover-user"

# Slack notifications
SLACK_ENABLED=true
SLACK_WEBHOOK_URL="https://hooks.slack.com/services/YOUR/WEBHOOK/URL"
SLACK_CHANNEL="#alerts"
```

#### Health Check Thresholds

Configure monitoring thresholds in `/etc/autonomy/alerts.conf`:

```bash
# Performance thresholds
HIGH_CPU_THRESHOLD=80
HIGH_MEMORY_THRESHOLD=85
HIGH_DISK_THRESHOLD=90
LOW_DISK_THRESHOLD=10

# Health check settings
HEALTH_CHECK_INTERVAL=30
HEALTH_CHECK_TIMEOUT=10
HEALTH_CHECK_RETRIES=3
```

## Backup and Recovery

### Backup and Recovery Script

The `scripts/backup-recovery.sh` script provides comprehensive backup and disaster recovery capabilities.

#### Backup Types

1. **Full Backup** (default)
   - Configuration files
   - Binary and scripts
   - Logs (last 7 days)
   - System information

2. **Configuration Backup**
   - Configuration files only
   - UCI configuration export
   - Smaller, faster backups

3. **Binary Backup**
   - Binary and scripts only
   - System architecture information
   - For binary updates

#### Basic Usage

```bash
# Create full backup
./scripts/backup-recovery.sh backup --host 192.168.1.1 --key ~/.ssh/id_rsa

# Create configuration backup
./scripts/backup-recovery.sh backup --host 192.168.1.1 --key ~/.ssh/id_rsa --type config

# List available backups
./scripts/backup-recovery.sh list-backups --host 192.168.1.1 --key ~/.ssh/id_rsa

# Verify backup integrity
./scripts/backup-recovery.sh verify-backup /var/backups/autonomy/configs/autonomy-config-backup-20250120.tar.gz --host 192.168.1.1 --key ~/.ssh/id_rsa
```

#### Recovery Procedures

```bash
# Recover from backup
./scripts/backup-recovery.sh recover /var/backups/autonomy/configs/autonomy-config-backup-20250120.tar.gz --host 192.168.1.1 --key ~/.ssh/id_rsa

# Test recovery procedure
./scripts/backup-recovery.sh test-recovery --host 192.168.1.1 --key ~/.ssh/id_rsa
```

#### Encrypted Backups

```bash
# Create encrypted backup
./scripts/backup-recovery.sh backup \
  --host 192.168.1.1 \
  --key ~/.ssh/id_rsa \
  --encrypt \
  --passphrase "your-secure-passphrase"

# Recover from encrypted backup
./scripts/backup-recovery.sh recover \
  /var/backups/autonomy/configs/autonomy-config-backup-20250120.tar.gz.gpg \
  --host 192.168.1.1 \
  --key ~/.ssh/id_rsa \
  --passphrase "your-secure-passphrase"
```

#### Automated Backups

The script can set up automated daily backups:

```bash
# Setup automated backup (runs daily at 2 AM)
./scripts/backup-recovery.sh setup-automated-backup --host 192.168.1.1 --key ~/.ssh/id_rsa
```

## Production Deployment Workflow

### Recommended Deployment Process

1. **Pre-Deployment Planning**
   ```bash
   # Review target system
   ./scripts/deploy-production.sh --host 192.168.1.1 --key ~/.ssh/id_rsa --dry-run
   
   # Check system requirements
   ssh -i ~/.ssh/id_rsa root@192.168.1.1 "uname -m && free -h && df -h"
   ```

2. **Initial Deployment**
   ```bash
   # Deploy with full monitoring
   ./scripts/deploy-production.sh --host 192.168.1.1 --key ~/.ssh/id_rsa
   
   # Verify deployment
   ssh -i ~/.ssh/id_rsa root@192.168.1.1 "ubus call autonomy status"
   ```

3. **Configuration Setup**
   ```bash
   # Configure autonomy
   ssh -i ~/.ssh/id_rsa root@192.168.1.1 "autonomyctl config set interfaces.starlink.enabled true"
   
   # Test configuration
   ssh -i ~/.ssh/id_rsa root@192.168.1.1 "autonomyctl status"
   ```

4. **Monitoring Verification**
   ```bash
   # Check monitoring setup
   ssh -i ~/.ssh/id_rsa root@192.168.1.1 "crontab -l | grep autonomy"
   
   # Test health check
   ssh -i ~/.ssh/id_rsa root@192.168.1.1 "/usr/local/bin/autonomy-health-check"
   ```

5. **Backup Verification**
   ```bash
   # Create initial backup
   ./scripts/backup-recovery.sh backup --host 192.168.1.1 --key ~/.ssh/id_rsa --type full
   
   # Test recovery
   ./scripts/backup-recovery.sh test-recovery --host 192.168.1.1 --key ~/.ssh/id_rsa
   ```

### Update Deployment

For updating existing deployments:

```bash
# Create backup before update
./scripts/backup-recovery.sh backup --host 192.168.1.1 --key ~/.ssh/id_rsa --type full

# Deploy update
./scripts/deploy-production.sh --host 192.168.1.1 --key ~/.ssh/id_rsa

# Verify update
ssh -i ~/.ssh/id_rsa root@192.168.1.1 "autonomyctl status"
```

### Rollback Procedure

If deployment fails or issues arise:

```bash
# Automatic rollback (if deployment failed)
# The deployment script automatically rolls back on failure

# Manual rollback
./scripts/backup-recovery.sh recover /var/backups/autonomy/autonomy-full-backup-20250120_143022.tar.gz --host 192.168.1.1 --key ~/.ssh/id_rsa
```

## Troubleshooting

### Common Deployment Issues

1. **SSH Connection Failed**
   ```bash
   # Test SSH connectivity
   ssh -i ~/.ssh/id_rsa -o ConnectTimeout=10 root@192.168.1.1 "echo 'SSH OK'"
   
   # Check SSH key permissions
   chmod 600 ~/.ssh/id_rsa
   ```

2. **Build Failed**
   ```bash
   # Check Go installation
   go version
   
   # Check build environment
   ./scripts/build.sh --target linux/arm --dry-run
   ```

3. **Service Won't Start**
   ```bash
   # Check service logs
   ssh -i ~/.ssh/id_rsa root@192.168.1.1 "logread | grep autonomy"
   
   # Check binary permissions
   ssh -i ~/.ssh/id_rsa root@192.168.1.1 "ls -la /usr/sbin/autonomyd"
   ```

4. **ubus Interface Not Available**
   ```bash
   # Check ubus availability
   ssh -i ~/.ssh/id_rsa root@192.168.1.1 "ubus list | grep autonomy"
   
   # Check service status
   ssh -i ~/.ssh/id_rsa root@192.168.1.1 "/etc/init.d/autonomy status"
   ```

### Monitoring Issues

1. **Health Check Failing**
   ```bash
   # Run health check manually
   ssh -i ~/.ssh/id_rsa root@192.168.1.1 "/usr/local/bin/autonomy-health-check"
   
   # Check health check logs
   ssh -i ~/.ssh/id_rsa root@192.168.1.1 "tail -f /var/log/autonomy/health.log"
   ```

2. **Alerts Not Working**
   ```bash
   # Check alert configuration
   ssh -i ~/.ssh/id_rsa root@192.168.1.1 "cat /etc/autonomy/alerts.conf"
   
   # Test notification manually
   ssh -i ~/.ssh/id_rsa root@192.168.1.1 "echo 'test' | logger -t autonomy-test"
   ```

### Recovery Issues

1. **Backup Verification Failed**
   ```bash
   # Check backup file integrity
   ./scripts/backup-recovery.sh verify-backup /path/to/backup.tar.gz --host 192.168.1.1 --key ~/.ssh/id_rsa
   
   # Check disk space
   ssh -i ~/.ssh/id_rsa root@192.168.1.1 "df -h /var/backups"
   ```

2. **Recovery Failed**
   ```bash
   # Check recovery logs
   ssh -i ~/.ssh/id_rsa root@192.168.1.1 "tail -f /var/log/autonomy/recovery.log"
   
   # Manual recovery steps
   ssh -i ~/.ssh/id_rsa root@192.168.1.1 "/etc/init.d/autonomy stop"
   # ... manual recovery steps ...
   ssh -i ~/.ssh/id_rsa root@192.168.1.1 "/etc/init.d/autonomy start"
   ```

## Maintenance Procedures

### Regular Maintenance

1. **Daily Checks**
   ```bash
   # Check service status
   ssh -i ~/.ssh/id_rsa root@192.168.1.1 "autonomyctl status"
   
   # Check health logs
   ssh -i ~/.ssh/id_rsa root@192.168.1.1 "tail -20 /var/log/autonomy/health.log"
   ```

2. **Weekly Maintenance**
   ```bash
   # Review backup status
   ./scripts/backup-recovery.sh list-backups --host 192.168.1.1 --key ~/.ssh/id_rsa
   
   # Check disk usage
   ssh -i ~/.ssh/id_rsa root@192.168.1.1 "df -h && du -sh /var/log/autonomy"
   ```

3. **Monthly Maintenance**
   ```bash
   # Test recovery procedure
   ./scripts/backup-recovery.sh test-recovery --host 192.168.1.1 --key ~/.ssh/id_rsa
   
   # Review monitoring configuration
   ssh -i ~/.ssh/id_rsa root@192.168.1.1 "cat /etc/autonomy/alerts.conf"
   ```

### Performance Tuning

1. **Resource Optimization**
   ```bash
   # Check resource usage
   ssh -i ~/.ssh/id_rsa root@192.168.1.1 "top -bn1 | head -20"
   
   # Optimize log rotation
   ssh -i ~/.ssh/id_rsa root@192.168.1.1 "cat /etc/logrotate.d/autonomy"
   ```

2. **Monitoring Tuning**
   ```bash
   # Adjust health check frequency
   ssh -i ~/.ssh/id_rsa root@192.168.1.1 "crontab -l | grep autonomy"
   
   # Tune alert thresholds
   ssh -i ~/.ssh/id_rsa root@192.168.1.1 "vi /etc/autonomy/alerts.conf"
   ```

### Security Considerations

1. **SSH Security**
   - Use key-based authentication
   - Disable password authentication
   - Use non-standard SSH port
   - Implement fail2ban

2. **Backup Security**
   - Encrypt sensitive backups
   - Store backups securely
   - Rotate backup encryption keys
   - Test backup restoration regularly

3. **Monitoring Security**
   - Secure alert endpoints
   - Use HTTPS for webhooks
   - Implement alert authentication
   - Monitor for unauthorized access

## Conclusion

This deployment guide provides comprehensive procedures for deploying autonomy in production environments. The automated scripts ensure consistent, reliable deployments with built-in safety features and disaster recovery capabilities.

For additional support, refer to:
- [User Guide](USER_GUIDE.md) - Complete user documentation
- [API Reference](API_REFERENCE.md) - Programmatic interfaces
- [Troubleshooting Guide](TROUBLESHOOTING.md) - Common issues and solutions
- [Operations Guide](OPERATIONS_GUIDE.md) - System administration procedures
