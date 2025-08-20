# 🔍 RUTOS Cron Monitoring System

A comprehensive monitoring solution for cron jobs in the RUTOS Starlink Failover project. This system provides error detection, webhook notifications, health monitoring, and comprehensive logging for all cron-based automation.

## ✨ Features

### 🛡️ **Error Detection & Notifications**
- **Real-time error detection** for all wrapped cron jobs
- **Webhook notifications** with detailed error information
- **Pushover integration** for mobile alerts
- **Rate limiting** to prevent notification spam (configurable)
- **Timeout protection** for hung scripts

### 📊 **Health Monitoring**
- **Cron daemon health** monitoring
- **Missing execution detection** for expected scripts
- **Hung process detection** and alerting
- **System resource monitoring** (disk space, memory)
- **Automatic health reports**

### 📝 **Comprehensive Logging**
- **Individual script logs** with full stdout/stderr capture
- **Execution timing** and performance metrics
- **Structured JSON** webhook payloads
- **Health monitoring logs** with trends
- **Debug mode** for troubleshooting

## 🚀 Quick Start

### **1. Easy Setup**
```bash
# Basic setup
./scripts/setup-cron-monitoring-rutos.sh

# Setup with webhook URL
./scripts/setup-cron-monitoring-rutos.sh --webhook "https://your.webhook.url"

# Auto-wrap existing cron jobs
./scripts/setup-cron-monitoring-rutos.sh --auto-wrap
```

### **2. Manual Cron Job Wrapping**
```bash
# Before (original cron job):
*/5 * * * * /root/starlink-monitor/starlink_monitor-rutos.sh

# After (with monitoring):
*/5 * * * * /root/starlink-monitor/scripts/cron-monitor-wrapper-rutos.sh /root/starlink-monitor/starlink_monitor-rutos.sh
```

### **3. Test the System**
```bash
# Test with a simple command
/root/starlink-monitor/scripts/cron-monitor-wrapper-rutos.sh /bin/echo "Hello World"

# Test error handling
/root/starlink-monitor/scripts/cron-monitor-wrapper-rutos.sh /bin/false

# Check logs
tail -f /var/log/cron-monitor/false.log
```

## 📋 Components

### **1. Cron Monitor Wrapper** (`cron-monitor-wrapper-rutos.sh`)
Wraps around any cron job to provide monitoring capabilities.

**Features:**
- Captures stdout/stderr from wrapped scripts
- Detects exit codes and runtime errors
- Sends webhook/Pushover notifications on failures
- Logs all executions with timestamps
- Timeout protection for hung scripts
- Rate limiting to prevent notification spam

**Usage:**
```bash
# Basic usage
/path/to/cron-monitor-wrapper-rutos.sh /path/to/your/script.sh

# With custom webhook
WEBHOOK_URL="https://your.webhook.url" /path/to/cron-monitor-wrapper-rutos.sh /path/to/your/script.sh

# With environment variables
DEBUG=1 DRY_RUN=1 /path/to/cron-monitor-wrapper-rutos.sh /path/to/your/script.sh
```

### **2. Cron Health Monitor** (`cron-health-monitor-rutos.sh`)
Monitors the overall health of the cron system.

**Features:**
- Checks cron daemon health
- Detects missing executions for expected scripts
- Monitors for hung/zombie processes
- Validates crontab syntax
- Generates daily health reports

**Cron Setup:**
```bash
# Add to crontab (runs every 15 minutes)
*/15 * * * * /root/starlink-monitor/scripts/cron-health-monitor-rutos.sh
```

### **3. Setup Script** (`setup-cron-monitoring-rutos.sh`)
Easy setup and configuration tool.

**Options:**
```bash
./setup-cron-monitoring-rutos.sh [OPTIONS]

Options:
    --webhook URL       Set webhook URL for notifications
    --auto-wrap         Automatically wrap existing cron jobs
    --dry-run           Show what would be done without making changes
    --help              Show help message
```

## ⚙️ Configuration

### **Main Configuration** (`/etc/starlink-config/config.sh`)
```bash
# Cron Monitoring Configuration
CRON_MONITOR_WEBHOOK_URL="https://your.webhook.url"
CRON_HEALTH_WEBHOOK_URL="https://your.webhook.url"
MAX_NOTIFICATIONS_PER_HOUR="5"
SCRIPT_TIMEOUT="300"
NOTIFY_ON_SUCCESS="0"
PUSHOVER_ON_SUCCESS="0"

# Pushover notifications (optional)
PUSHOVER_TOKEN="your_pushover_token"
PUSHOVER_USER="your_pushover_user"
```

### **Expected Scripts** (`/etc/starlink-config/cron-expected.conf`)
Define which scripts should be monitored for missing executions:
```bash
# Format: script_name|interval_minutes|description
starlink_monitor-rutos|5|Starlink quality monitoring
system-maintenance-rutos|60|System maintenance tasks
backup-logs-rutos|1440|Daily log backup
```

## 🔔 Webhook Integration

### **Webhook Payload Format**
```json
{
    "timestamp": "2025-07-29 10:30:00",
    "hostname": "router",
    "script": "starlink_monitor-rutos",
    "script_path": "/root/starlink-monitor/starlink_monitor-rutos.sh",
    "exit_code": 1,
    "execution_time": "15",
    "error_output": "Error: Starlink API connection failed",
    "severity": "error",
    "source": "rutos-cron-monitor"
}
```

### **Health Alert Payload**
```json
{
    "timestamp": "2025-07-29 10:30:00",
    "hostname": "router",
    "alert_type": "MISSING_EXECUTION",
    "message": "Script starlink_monitor-rutos is 30 minutes overdue",
    "severity": "warning",
    "source": "rutos-cron-health-monitor"
}
```

### **Popular Webhook Services**
- **Discord**: Create webhook in channel settings
- **Slack**: Create webhook app in workspace
- **Microsoft Teams**: Create incoming webhook connector
- **Custom API**: Any HTTP endpoint accepting JSON POST

## 📊 Monitoring & Logs

### **Log Locations**
```bash
# Execution logs (per script)
/var/log/cron-monitor/script-name.log

# Health monitoring logs
/var/log/cron-health/cron-health.log

# Daily health reports
/var/log/cron-health/health-report-YYYYMMDD.log

# Rate limiting state
/tmp/cron-monitor/rate_limit_script-name
```

### **Log Format Example**
```
==================== EXECUTION LOG ====================
Timestamp: 2025-07-29 10:30:00
Script: /root/starlink-monitor/starlink_monitor-rutos.sh
Exit Code: 0
Execution Time: 12s
PID: 1234

--- STDOUT ---
[INFO] Starting Starlink monitoring...
[SUCCESS] Starlink quality check passed

--- STDERR ---
[DEBUG] API connection established

==================== END LOG ====================
```

## 🔧 Advanced Usage

### **Environment Variables**
```bash
# Monitoring behavior
DRY_RUN=1                    # Test mode (no actual execution)
DEBUG=1                      # Enable debug logging
RUTOS_TEST_MODE=1           # Enhanced trace logging
NOTIFY_ON_SUCCESS=1         # Send notifications on success too
PUSHOVER_ON_SUCCESS=1       # Send Pushover on success too

# Timeouts and limits
SCRIPT_TIMEOUT=600          # Script timeout in seconds
WEBHOOK_TIMEOUT=10          # Webhook request timeout
MAX_NOTIFICATIONS_PER_HOUR=10  # Rate limiting

# Custom paths
MONITOR_LOG_DIR="/custom/log/path"
WEBHOOK_URL="https://custom.webhook.url"
```

### **Multiple Webhook Support**
```bash
# Different webhooks for different scripts
*/5 * * * * WEBHOOK_URL="https://starlink.webhook.url" /path/to/cron-monitor-wrapper-rutos.sh /path/to/starlink_monitor-rutos.sh
*/60 * * * * WEBHOOK_URL="https://backup.webhook.url" /path/to/cron-monitor-wrapper-rutos.sh /path/to/backup-script.sh
```

### **Conditional Notifications**
```bash
# Only notify on failures (default)
NOTIFY_ON_SUCCESS=0 PUSHOVER_ON_SUCCESS=0

# Notify on both success and failure
NOTIFY_ON_SUCCESS=1 PUSHOVER_ON_SUCCESS=1

# Different notification levels
WEBHOOK_URL="https://all-events.webhook.url" NOTIFY_ON_SUCCESS=1 /path/to/wrapper.sh /path/to/script.sh
```

## 🛠️ Troubleshooting

### **Common Issues**

**1. No Notifications Received**
```bash
# Check webhook URL configuration
grep WEBHOOK_URL /etc/starlink-config/config.sh

# Test webhook manually
curl -X POST "https://your.webhook.url" -H "Content-Type: application/json" -d '{"test": "message"}'

# Check wrapper script logs
tail -f /var/log/cron-monitor/script-name.log
```

**2. Rate Limiting Issues**
```bash
# Check rate limit files
ls -la /tmp/cron-monitor/rate_limit_*

# Reset rate limiting for a script
rm /tmp/cron-monitor/rate_limit_script-name

# Increase rate limit
echo 'MAX_NOTIFICATIONS_PER_HOUR="20"' >> /etc/starlink-config/config.sh
```

**3. Scripts Not Detected as Missing**
```bash
# Check expected scripts configuration
cat /etc/starlink-config/cron-expected.conf

# Verify health monitor is running
crontab -l | grep cron-health-monitor

# Check health monitor logs
tail -f /var/log/cron-health/cron-health.log
```

**4. Permission Issues**
```bash
# Fix script permissions
chmod +x /root/starlink-monitor/scripts/cron-monitor-wrapper-rutos.sh
chmod +x /root/starlink-monitor/scripts/cron-health-monitor-rutos.sh

# Fix log directory permissions
mkdir -p /var/log/cron-monitor /var/log/cron-health
chmod 755 /var/log/cron-monitor /var/log/cron-health
```

### **Debug Mode**
```bash
# Enable debug logging for wrapper
DEBUG=1 /path/to/cron-monitor-wrapper-rutos.sh /path/to/script.sh

# Enable debug logging for health monitor
DEBUG=1 /path/to/cron-health-monitor-rutos.sh

# Test mode (no actual execution)
DRY_RUN=1 DEBUG=1 /path/to/cron-monitor-wrapper-rutos.sh /path/to/script.sh
```

## 🔗 Integration Examples

### **Discord Webhook**
```bash
# Create Discord webhook and use the URL
WEBHOOK_URL="https://discord.com/api/webhooks/YOUR_WEBHOOK_ID/YOUR_WEBHOOK_TOKEN"
```

### **Slack Webhook**
```bash
# Create Slack app with incoming webhook
WEBHOOK_URL="https://hooks.slack.com/services/YOUR/SLACK/WEBHOOK"
```

### **Custom API Integration**
```python
# Example Python Flask webhook receiver
from flask import Flask, request
import json

app = Flask(__name__)

@app.route('/cron-webhook', methods=['POST'])
def handle_cron_webhook():
    data = request.get_json()
    
    if data['severity'] == 'error':
        # Handle cron failure
        print(f"Cron failure: {data['script']} failed with exit code {data['exit_code']}")
        # Send to your alerting system
        
    return 'OK'

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8080)
```

## 📈 Benefits

### **Operational Excellence**
- **Proactive monitoring** of all automated tasks
- **Immediate alerting** when critical scripts fail
- **Historical logging** for troubleshooting and analysis
- **Health trends** to predict issues before they occur

### **Reliability**
- **Timeout protection** prevents hung processes
- **Rate limiting** prevents notification spam
- **Graceful degradation** when monitoring systems fail
- **Multiple notification channels** for redundancy

### **Visibility**
- **Comprehensive logging** of all cron activity
- **Performance metrics** for optimization
- **Health reporting** for system overview
- **Debug capabilities** for troubleshooting

---

## 🎯 Quick Reference

### **Setup Commands**
```bash
# Basic setup
./scripts/setup-cron-monitoring-rutos.sh

# With webhook
./scripts/setup-cron-monitoring-rutos.sh --webhook "https://your.webhook.url"

# Auto-wrap existing jobs
./scripts/setup-cron-monitoring-rutos.sh --auto-wrap
```

### **Manual Wrapper Usage**
```bash
# Basic
/path/to/cron-monitor-wrapper-rutos.sh /path/to/script.sh

# With webhook
WEBHOOK_URL="https://webhook.url" /path/to/cron-monitor-wrapper-rutos.sh /path/to/script.sh
```

### **Log Locations**
```bash
/var/log/cron-monitor/     # Execution logs
/var/log/cron-health/      # Health logs
/tmp/cron-monitor/         # State files
```

### **Configuration Files**
```bash
/etc/starlink-config/config.sh              # Main configuration
/etc/starlink-config/cron-expected.conf     # Expected scripts
```

This monitoring system provides enterprise-level cron job monitoring capabilities while maintaining the simplicity and reliability expected in the RUTOS environment.
