# Complete Starlink Solution Deployment Guide for RUTOS

**Version:** 2.7.1 | **Updated:** 2025-07-27

**Version:** 2.6.0 | **Updated:** 2025-07-24

**Version:** 2.5.0 | **Updated:** 2025-07-24

**Version:** 2.4.12 | **Updated:** 2025-07-21

**Version:** 2.4.11 | **Updated:** 2025-07-21

**Version:** 2.4.10 | **Updated:** 2025-07-21

**Version:** 2.4.9 | **Updated:** 2025-07-21

**Version:** 2.4.8 | **Updated:** 2025-07-21

**Version:** 2.4.7 | **Updated:** 2025-07-21

This guide provides both automated and manual installation methods for deploying a comprehensive Starlink monitoring and
failover solution on RUTOS devices.

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Quick Automated Installation](#quick-automated-installation)
3. [Manual Step-by-Step Installation](#manual-step-by-step-installation)
4. [Configuration](#configuration)
5. [Verification](#verification)
6. [Troubleshooting](#troubleshooting)
7. [Maintenance](#maintenance)

## Prerequisites

### Hardware Requirements

- RUTOS device (RUTX50 or compatible) with internet connectivity
- Starlink dish connected to RUTOS device
- At least 10MB free storage space
- Root/admin access to RUTOS device

### Network Setup

- RUTOS device accessible via SSH
- Starlink dish management interface accessible (typically 192.168.100.1)
- Internet connectivity for downloading binaries and Azure integration

### Software Requirements

- RUTOS firmware with UCI configuration system
- OpenWrt package system (opkg)
- Cron service enabled

## Quick Automated Installation

### Step 1: Download and Execute Installation Script

```bash
# SSH into your RUTOS device as root
ssh root@<RUTOS_IP>

# Download the installation script
curl -fL https://raw.githubusercontent.com/markus-lassfolk/autonomy/main/deploy-starlink-solution.sh -o deploy-starlink-solution.sh

# Make it executable
chmod +x deploy-starlink-solution.sh

# Run the installation
./deploy-starlink-solution.sh
```

### Step 2: Follow Interactive Configuration

The script will prompt you for:

1. **Azure Integration** (optional)

   - Enable Azure cloud logging: `true/false`
   - Azure Function endpoint URL (if enabled)

2. **Starlink Monitoring**

   - Enable performance monitoring: `true/false` (recommended: true)

3. **GPS Integration** (optional)

   - Enable GPS tracking: `true/false`
   - RUTOS device credentials (if enabled)

4. **Pushover Notifications** (optional)

   - Enable notifications: `true/false`
   - Pushover application token and user key (if enabled)

5. **Network Configuration**
   - Starlink IP address (default: 192.168.100.1)

### Step 3: Verify Installation

```bash
# Run the verification script
/root/verify-starlink-setup.sh
```

## Manual Step-by-Step Installation

If you prefer manual installation or need to customize the setup:

### Step 1: System Preparation

```bash
# Update package list
opkg update

# Install required packages
opkg install curl bc

# Create directories
mkdir -p /root
mkdir -p /etc/hotplug.d/iface
mkdir -p /tmp/run
```

### Step 2: Download Required Binaries

```bash
# Download grpcurl (ARM binary for RUTX50)
curl -fL https://github.com/fullstorydev/grpcurl/releases/download/v1.9.3/grpcurl_1.9.3_linux_armv7.tar.gz -o /tmp/grpcurl.tar.gz
tar -zxf /tmp/grpcurl.tar.gz -C /root/ grpcurl
chmod +x /root/grpcurl
rm /tmp/grpcurl.tar.gz

# Download jq (ARM binary)
curl -fL https://github.com/jqlang/jq/releases/download/jq-1.7.1/jq-linux-armhf -o /root/jq
chmod +x /root/jq

# Verify installations
/root/grpcurl --version
/root/jq --version
```

### Step 3: Configure System Logging

```bash
# Setup persistent logging
uci set system.@system[0].log_type='file'
uci set system.@system[0].log_file='/overlay/messages'
uci set system.@system[0].log_size='5120'
uci commit system

# Restart logging service
/etc/init.d/log restart
```

### Step 4: Configure Network Routes

```bash
# Add route to Starlink management interface
uci add network route
uci set network.@route[-1].interface='wan'
uci set network.@route[-1].target='192.168.100.1'
uci set network.@route[-1].netmask='255.255.255.255'
uci commit network

# Apply immediately
ip route add 192.168.100.1 dev $(uci get network.wan.ifname 2>/dev/null || echo "eth1")
```

### Step 5: Configure Multi-WAN (mwan3)

```bash
# Set Starlink priority
uci set mwan3.member1.metric='1'
uci set mwan3.member3.metric='2'
uci set mwan3.member4.metric='4'

# Configure tracking
uci set mwan3.@condition[1].interface='wan'
uci set mwan3.@condition[1].track_method='ping'
uci set mwan3.@condition[1].track_ip='1.0.0.1' '8.8.8.8'
uci set mwan3.@condition[1].reliability='1'
uci set mwan3.@condition[1].timeout='1'
uci set mwan3.@condition[1].interval='1'
uci set mwan3.@condition[1].count='1'
uci set mwan3.@condition[1].down='2'
uci set mwan3.@condition[1].up='3'

uci commit mwan3
```

### Step 6: Create Configuration File

```bash
cat > /root/config.sh << 'EOF'
#!/bin/bash
# Starlink Solution Configuration

# Network Configuration
STARLINK_IP="192.168.100.1"
MWAN_IFACE="wan"
MWAN_MEMBER="member1"

# Quality Thresholds
PACKET_LOSS_THRESHOLD="5"
OBSTRUCTION_THRESHOLD="3"
LATENCY_THRESHOLD="150"
STABILITY_CHECKS_REQUIRED="5"

# Failover Metrics
METRIC_GOOD="1"
METRIC_BAD="100"

# Feature Toggles (configure as needed)
PUSHOVER_ENABLED="false"
PUSHOVER_TOKEN=""
PUSHOVER_USER=""

AZURE_ENABLED="false"
AZURE_ENDPOINT=""

GPS_ENABLED="false"
RUTOS_IP="your-router-ip"
RUTOS_USERNAME=""
RUTOS_PASSWORD=""

STARLINK_MONITORING_ENABLED="true"
EOF

chmod 600 /root/config.sh
```

### Step 7: Deploy Monitoring Scripts

#### Main Quality Monitor Script

```bash
cat > /root/starlink_monitor.sh << 'EOF'
#!/bin/bash
# Starlink Quality Monitor
set -euo pipefail

# Load configuration
CONFIG_FILE="/root/config.sh"
if [ -f "$CONFIG_FILE" ]; then
    . "$CONFIG_FILE"
fi

# Default configuration
STARLINK_IP="${STARLINK_IP:-192.168.100.1}"
STARLINK_PORT="${STARLINK_PORT:-9200}"
MWAN_IFACE="${MWAN_IFACE:-wan}"
MWAN_MEMBER="${MWAN_MEMBER:-member1}"
METRIC_GOOD="${METRIC_GOOD:-1}"
METRIC_BAD="${METRIC_BAD:-100}"
PACKET_LOSS_THRESHOLD="${PACKET_LOSS_THRESHOLD:-0.05}"
OBSTRUCTION_THRESHOLD="${OBSTRUCTION_THRESHOLD:-0.001}"
LATENCY_THRESHOLD_MS="${LATENCY_THRESHOLD_MS:-150}"
STABILITY_CHECKS_REQUIRED="${STABILITY_CHECKS_REQUIRED:-5}"

# State files
STATE_FILE="/tmp/run/starlink_monitor.state"
STABILITY_FILE="/tmp/run/starlink_monitor.stability"
LOG_TAG="StarlinkMonitor"

# Create state directory
mkdir -p "$(dirname "$STATE_FILE")"

# Logging function
log() {
    logger -t "$LOG_TAG" -- "$1"
}

# Main monitoring logic
main() {
    log "Starting quality check"

    # Read current state
    last_state=$(cat "$STATE_FILE" 2>/dev/null || echo "up")
    stability_count=$(cat "$STABILITY_FILE" 2>/dev/null || echo "0")
    current_metric=$(uci -q get mwan3."$MWAN_MEMBER".metric 2>/dev/null || echo "$METRIC_GOOD")

    # Gather Starlink data
    if [ -x "/root/grpcurl" ] && [ -x "/root/jq" ]; then
        # Get status data
        status_json=$(timeout 10 /root/grpcurl -plaintext -max-time 5 \
            -d '{"get_status":{}}' "$STARLINK_IP" SpaceX.API.Device.Device/Handle 2>/dev/null || echo "")

        # Get history data for packet loss
        history_json=$(timeout 10 /root/grpcurl -plaintext -max-time 5 \
            -d '{"get_history":{}}' "$STARLINK_IP" SpaceX.API.Device.Device/Handle 2>/dev/null || echo "")

        if [ -n "$status_json" ] && [ -n "$history_json" ]; then
            # Extract metrics
            latency=$(echo "$status_json" | /root/jq -r '.dishGetStatus.popPingLatencyMs // 0' 2>/dev/null || echo "0")
            obstruction=$(echo "$status_json" | /root/jq -r '.dishGetStatus.obstructionStats.currentlyObstructed // false' 2>/dev/null || echo "false")

            # Calculate packet loss from history
            packet_loss=$(echo "$history_json" | /root/jq -r '
                [.dishGetHistory.popPingDropRate // empty] |
                if length > 0 then (add / length) else 0 end
            ' 2>/dev/null || echo "0")

            # Evaluate quality
            quality_good=true

            if [ "$(echo "$latency > $LATENCY_THRESHOLD_MS" | bc 2>/dev/null || echo 0)" -eq 1 ]; then
                quality_good=false
                log "Quality issue: High latency ($latency ms > $LATENCY_THRESHOLD_MS ms)"
            fi

            if [ "$(echo "$packet_loss > $PACKET_LOSS_THRESHOLD" | bc 2>/dev/null || echo 0)" -eq 1 ]; then
                quality_good=false
                log "Quality issue: High packet loss ($packet_loss > $PACKET_LOSS_THRESHOLD)"
            fi

            if [ "$obstruction" = "true" ]; then
                quality_good=false
                log "Quality issue: Dish obstructed"
            fi

            # State machine logic
            if [ "$quality_good" = "true" ]; then
                if [ "$last_state" = "down" ]; then
                    stability_count=$((stability_count + 1))
                    if [ "$stability_count" -ge "$STABILITY_CHECKS_REQUIRED" ]; then
                        # Failback to good quality
                        uci set mwan3."$MWAN_MEMBER".metric="$METRIC_GOOD"
                        uci commit mwan3
                        mwan3 restart >/dev/null 2>&1
                        echo "up" > "$STATE_FILE"
                        echo "0" > "$STABILITY_FILE"
                        log "FAILBACK: Connection quality restored"
                        /etc/hotplug.d/iface/99-pushover_notify >/dev/null 2>&1 || true
                    else
                        echo "$stability_count" > "$STABILITY_FILE"
                        log "Quality good, stability count: $stability_count/$STABILITY_CHECKS_REQUIRED"
                    fi
                else
                    echo "0" > "$STABILITY_FILE"
                    log "Quality check passed"
                fi
            else
                if [ "$last_state" = "up" ]; then
                    # Failover due to poor quality
                    uci set mwan3."$MWAN_MEMBER".metric="$METRIC_BAD"
                    uci commit mwan3
                    mwan3 restart >/dev/null 2>&1
                    echo "down" > "$STATE_FILE"
                    echo "0" > "$STABILITY_FILE"
                    log "FAILOVER: Connection quality degraded"
                    /etc/hotplug.d/iface/99-pushover_notify >/dev/null 2>&1 || true
                else
                    log "Quality still poor, staying failed over"
                fi
            fi
        else
            log "Warning: Unable to get Starlink API data"
        fi
    else
        log "Error: grpcurl or jq not available"
    fi
}

# Run main function
main
EOF

chmod +x /root/starlink_monitor.sh
```

#### Performance Logger Script

```bash
cat > /root/starlink_logger.sh << 'EOF'
#!/bin/bash
# Starlink Performance Logger
set -euo pipefail

# Configuration
STARLINK_IP="${STARLINK_IP:-192.168.100.1}"
STARLINK_PORT="${STARLINK_PORT:-9200}"
OUTPUT_CSV="/root/starlink_performance_log.csv"
LAST_SAMPLE_FILE="/tmp/run/starlink_last_sample.ts"
LOG_TAG="StarlinkLogger"

# Create state directory
mkdir -p "$(dirname "$LAST_SAMPLE_FILE")"

# Logging function
log() {
    logger -t "$LOG_TAG" -- "$1"
}

# Create CSV header if file doesn't exist
if [ ! -f "$OUTPUT_CSV" ]; then
    echo "timestamp,latency_ms,packet_loss_rate,obstruction_percent,throughput_down_mbps,throughput_up_mbps" > "$OUTPUT_CSV"
fi

# Main logging logic
main() {
    log "Starting performance data collection"

    if [ -x "/root/grpcurl" ] && [ -x "/root/jq" ]; then
        # Get status data
        status_json=$(timeout 10 /root/grpcurl -plaintext -max-time 5 \
            -d '{"get_status":{}}' "$STARLINK_IP" SpaceX.API.Device.Device/Handle 2>/dev/null || echo "")

        # Get history data
        history_json=$(timeout 10 /root/grpcurl -plaintext -max-time 5 \
            -d '{"get_history":{}}' "$STARLINK_IP" SpaceX.API.Device.Device/Handle 2>/dev/null || echo "")

        if [ -n "$status_json" ] && [ -n "$history_json" ]; then
            # Extract current timestamp from status
            current_timestamp=$(echo "$status_json" | /root/jq -r '.dishGetStatus.uptimeS // 0' 2>/dev/null || echo "0")

            # Check if this is a new sample
            last_timestamp=$(cat "$LAST_SAMPLE_FILE" 2>/dev/null || echo "0")

            if [ "$current_timestamp" != "$last_timestamp" ]; then
                # Extract metrics
                latency=$(echo "$status_json" | /root/jq -r '.dishGetStatus.popPingLatencyMs // 0' 2>/dev/null || echo "0")

                # Calculate packet loss
                packet_loss=$(echo "$history_json" | /root/jq -r '
                    [.dishGetHistory.popPingDropRate // empty] |
                    if length > 0 then (add / length) else 0 end
                ' 2>/dev/null || echo "0")

                # Get obstruction percentage
                obstruction=$(echo "$status_json" | /root/jq -r '.dishGetStatus.obstructionStats.fractionObstructed // 0' 2>/dev/null || echo "0")

                # Get throughput
                throughput_down=$(echo "$status_json" | /root/jq -r '.dishGetStatus.downlinkThroughputBps // 0' 2>/dev/null || echo "0")
                throughput_up=$(echo "$status_json" | /root/jq -r '.dishGetStatus.uplinkThroughputBps // 0' 2>/dev/null || echo "0")

                # Convert to Mbps
                throughput_down_mbps=$(echo "scale=2; $throughput_down / 1000000" | bc 2>/dev/null || echo "0")
                throughput_up_mbps=$(echo "scale=2; $throughput_up / 1000000" | bc 2>/dev/null || echo "0")

                # Create timestamp
                timestamp=$(date '+%Y-%m-%d %H:%M:%S')

                # Append to CSV
                echo "$timestamp,$latency,$packet_loss,$obstruction,$throughput_down_mbps,$throughput_up_mbps" >> "$OUTPUT_CSV"

                # Update last sample timestamp
                echo "$current_timestamp" > "$LAST_SAMPLE_FILE"

                log "Performance data logged: latency=${latency}ms, packet_loss=${packet_loss}, obstruction=${obstruction}"
            else
                log "No new data available, skipping"
            fi
        else
            log "Warning: Unable to get Starlink API data"
        fi
    else
        log "Error: grpcurl or jq not available"
    fi
}

# Run main function
main
EOF

chmod +x /root/starlink_logger.sh
```

#### API Change Detector Script

```bash
cat > /root/check_starlink_api.sh << 'EOF'
#!/bin/bash
# Starlink API Change Detector
set -euo pipefail

# Configuration
STARLINK_IP="${STARLINK_IP:-192.168.100.1}"
STARLINK_PORT="${STARLINK_PORT:-9200}"
API_VERSION_FILE="/tmp/starlink_api_version"
LOG_TAG="StarlinkAPIChecker"

# Logging function
log() {
    logger -t "$LOG_TAG" -- "$1"
}

# Main checking logic
main() {
    log "Checking for Starlink API changes"

    if [ -x "/root/grpcurl" ] && [ -x "/root/jq" ]; then
        # Get current API response structure
        current_response=$(timeout 10 /root/grpcurl -plaintext -max-time 5 \
            -d '{"get_status":{}}' "$STARLINK_IP" SpaceX.API.Device.Device/Handle 2>/dev/null || echo "")

        if [ -n "$current_response" ]; then
            # Create a simple hash of the response structure
            current_hash=$(echo "$current_response" | /root/jq -r 'keys_unsorted | @json' 2>/dev/null | md5sum | cut -d' ' -f1)

            if [ -f "$API_VERSION_FILE" ]; then
                last_hash=$(cat "$API_VERSION_FILE")

                if [ "$current_hash" != "$last_hash" ]; then
                    log "WARNING: Starlink API structure has changed!"
                    log "This may require script updates to maintain compatibility"

                    # Trigger notification
                    /etc/hotplug.d/iface/99-pushover_notify >/dev/null 2>&1 || true
                else
                    log "API structure unchanged"
                fi
            else
                log "First run, recording API structure"
            fi

            echo "$current_hash" > "$API_VERSION_FILE"
        else
            log "Warning: Unable to get Starlink API data"
        fi
    else
        log "Error: grpcurl or jq not available"
    fi
}

# Run main function
main
EOF

chmod +x /root/check_starlink_api.sh
```

### Step 8: Setup Cron Jobs

```bash
# Remove any existing starlink-related cron jobs
(crontab -l 2>/dev/null | grep -v "starlink" || true) | crontab -

# Add monitoring jobs
(
    crontab -l 2>/dev/null
    echo "# Starlink Quality Monitoring (every minute)"
    echo "* * * * * /root/starlink_monitor.sh"
    echo "# Starlink Performance Logging (every minute)"
    echo "* * * * * /root/starlink_logger.sh"
    echo "# Starlink API Change Detection (daily at 5:30 AM)"
    echo "30 5 * * * /root/check_starlink_api.sh"
) | crontab -

# Restart cron service
/etc/init.d/cron restart
```

### Step 9: Optional - Setup Pushover Notifications

If you want push notifications for failover events:

```bash
# Update configuration with your Pushover credentials
sed -i 's/PUSHOVER_ENABLED="false"/PUSHOVER_ENABLED="true"/' /root/config.sh
sed -i 's/PUSHOVER_TOKEN=""/PUSHOVER_TOKEN="your_app_token_here"/' /root/config.sh
sed -i 's/PUSHOVER_USER=""/PUSHOVER_USER="your_user_key_here"/' /root/config.sh

# Create notification script
cat > /etc/hotplug.d/iface/99-pushover_notify << 'EOF'
#!/bin/bash
# Pushover Notification Script

# Load configuration
CONFIG_FILE="/root/config.sh"
if [ -f "$CONFIG_FILE" ]; then
    . "$CONFIG_FILE"
fi

# Only proceed if Pushover is configured
if [ -z "$PUSHOVER_TOKEN" ] || [ -z "$PUSHOVER_USER" ] || [ "$PUSHOVER_ENABLED" != "true" ]; then
    exit 0
fi

# Notification function
send_notification() {
    local title="$1"
    local message="$2"
    local priority="${3:-0}"

    curl -s \
        --form-string "token=$PUSHOVER_TOKEN" \
        --form-string "user=$PUSHOVER_USER" \
        --form-string "title=$title" \
        --form-string "message=$message" \
        --form-string "priority=$priority" \
        https://api.pushover.net/1/messages.json >/dev/null 2>&1
}

# Determine notification type and send
if grep -q "FAILOVER" /var/log/messages | tail -1; then
    send_notification "Starlink Failover" "Connection quality degraded, switched to backup" "1"
elif grep -q "FAILBACK" /var/log/messages | tail -1; then
    send_notification "Starlink Failback" "Connection quality restored, switched back to Starlink" "0"
elif grep -q "API.*changed" /var/log/messages | tail -1; then
    send_notification "Starlink API Change" "API structure has changed, scripts may need updates" "1"
fi
EOF

chmod +x /etc/hotplug.d/iface/99-pushover_notify
```

### Step 10: Optional - Setup Azure Integration

If you want to ship logs and performance data to Azure:

```bash
# Create Azure configuration section in UCI
touch /etc/config/azure

# Configure Azure settings (replace with your endpoint)
uci set azure.system=azure_config
uci set azure.system.endpoint="https://your-function-app.azurewebsites.net/api/HttpTrigger"
uci set azure.system.enabled='1'
uci set azure.system.log_file='/overlay/messages'
uci set azure.system.max_size='1048576'

uci set azure.starlink=starlink_config
uci set azure.starlink.endpoint="https://your-function-app.azurewebsites.net/api/HttpTrigger"
uci set azure.starlink.enabled='1'
uci set azure.starlink.csv_file='/root/starlink_performance_log.csv'
uci set azure.starlink.max_size='1048576'
uci set azure.starlink.starlink_ip="192.168.100.1"
uci set azure.starlink.starlink_port="9200"

uci commit azure

# Create log shipper script
cat > /root/log-shipper.sh << 'EOF'
#!/bin/bash
# Azure Log Shipper
set -euo pipefail

# Configuration from UCI
AZURE_ENDPOINT="$(uci get azure.system.endpoint 2>/dev/null || echo "")"
LOG_FILE="$(uci get azure.system.log_file 2>/dev/null || echo "/overlay/messages")"
MAX_SIZE="$(uci get azure.system.max_size 2>/dev/null || echo "1048576")"

# Exit if Azure not configured
if [ -z "$AZURE_ENDPOINT" ]; then
    exit 0
fi

# Main log shipping logic
if [ -f "$LOG_FILE" ] && [ -s "$LOG_FILE" ]; then
    # Get file size
    file_size=$(stat -f%z "$LOG_FILE" 2>/dev/null || stat -c%s "$LOG_FILE" 2>/dev/null || echo "0")

    if [ "$file_size" -gt 100 ]; then
        # Send logs to Azure
        curl -X POST "$AZURE_ENDPOINT" \
            -H "Content-Type: text/plain" \
            -d "@$LOG_FILE" \
            --max-time 30 >/dev/null 2>&1

        # Rotate log if it's too large
        if [ "$file_size" -gt "$MAX_SIZE" ]; then
            echo "$(date): Log rotated" > "$LOG_FILE"
        else
            # Clear the log file
            > "$LOG_FILE"
        fi

        logger -t "LogShipper" "Logs shipped to Azure ($file_size bytes)"
    fi
fi
EOF

chmod +x /root/log-shipper.sh

# Add Azure shipping to cron (every 5 minutes)
(
    crontab -l 2>/dev/null
    echo "# Azure Log Shipping (every 5 minutes)"
    echo "*/5 * * * * /root/log-shipper.sh"
) | crontab -

# Update configuration
sed -i 's/AZURE_ENABLED="false"/AZURE_ENABLED="true"/' /root/config.sh
sed -i 's|AZURE_ENDPOINT=""|AZURE_ENDPOINT="https://your-function-app.azurewebsites.net/api/HttpTrigger"|' /root/config.sh
```

## Configuration

### Quality Thresholds

Edit `/root/config.sh` to adjust monitoring sensitivity:

```bash
# Latency threshold in milliseconds
LATENCY_THRESHOLD_MS="150"

# Packet loss threshold (0.05 = 5%)
PACKET_LOSS_THRESHOLD="0.05"

# Number of consecutive good checks required before failback
STABILITY_CHECKS_REQUIRED="5"
```

### mwan3 Metrics

```bash
# Priority when connection is good (lower = higher priority)
METRIC_GOOD="1"

# Priority when connection is bad (higher = lower priority)
METRIC_BAD="100"
```

### Network Interfaces

```bash
# WAN interface name
MWAN_IFACE="wan"

# mwan3 member name
MWAN_MEMBER="member1"
```

## Verification

### Create and Run Verification Script

```bash
cat > /root/verify-starlink-setup.sh << 'EOF'
#!/bin/bash
# Starlink Solution Verification Script
set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Test counters
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_WARNED=0

# Logging functions
log_test() {
    echo -e "${BLUE}[TEST]${NC} $1"
}

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
    TESTS_WARNED=$((TESTS_WARNED + 1))
}

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

# Test functions
test_binaries() {
    log_test "Testing required binaries..."

    if [ -x "/root/grpcurl" ]; then
        local version
        version=$(/root/grpcurl --version 2>&1 | head -1)
        log_pass "grpcurl available: $version"
    else
        log_fail "grpcurl not found or not executable"
    fi

    if [ -x "/root/jq" ]; then
        local version
        version=$(/root/jq --version 2>&1)
        log_pass "jq available: $version"
    else
        log_fail "jq not found or not executable"
    fi

    if command -v bc >/dev/null 2>&1; then
        log_pass "bc calculator available"
    else
        log_warn "bc calculator not available (may affect some calculations)"
    fi
}

test_scripts() {
    log_test "Testing deployed scripts..."

    local scripts=(
        "/root/starlink_monitor.sh"
        "/root/starlink_logger.sh"
        "/root/check_starlink_api.sh"
    )

    for script in "${scripts[@]}"; do
        if [ -x "$script" ]; then
            log_pass "$(basename "$script") deployed and executable"
        else
            log_fail "$(basename "$script") missing or not executable"
        fi
    done

    # Check Pushover notifier
    if [ -x "/etc/hotplug.d/iface/99-pushover_notify" ]; then
        log_pass "Pushover notifier deployed"
    else
        log_warn "Pushover notifier not found (notifications disabled)"
    fi

    # Check Azure scripts
    if [ -x "/root/log-shipper.sh" ]; then
        log_pass "Azure log shipper deployed"
    else
        log_warn "Azure log shipper not found (Azure logging disabled)"
    fi
}

test_configuration() {
    log_test "Testing system configuration..."

    # Test UCI configuration
    if uci show system | grep -q "log_type='file'"; then
        log_pass "Persistent logging configured"
    else
        log_fail "Persistent logging not configured"
    fi

    # Test network routes
    if ip route show | grep -q "192.168.100.1"; then
        log_pass "Starlink route configured"
        local route_info
        route_info=$(ip route show | grep "192.168.100.1" | head -1)
        log_info "Route: $route_info"
    else
        log_fail "No route to Starlink management interface"
    fi

    # Test mwan3 configuration
    if uci show mwan3 | grep -q "member1"; then
        log_pass "mwan3 configuration found"
    else
        log_warn "mwan3 configuration may not be complete"
    fi
}

test_connectivity() {
    log_test "Testing connectivity..."

    # Test internet connectivity
    if ping -c 1 8.8.8.8 >/dev/null 2>&1; then
        log_pass "Internet connectivity working"
    else
        log_fail "No internet connectivity"
    fi

    # Test Starlink management interface
    if ping -c 1 192.168.100.1 >/dev/null 2>&1; then
        log_pass "Starlink management interface reachable"
    else
        log_warn "Starlink management interface not reachable"
    fi

    # Test Starlink API
    if [ -x "/root/grpcurl" ]; then
        local api_response
        api_response=$(timeout 10 /root/grpcurl -plaintext -max-time 5 \
            -d '{"get_status":{}}' 192.168.100.1:9200 SpaceX.API.Device.Device/Handle 2>/dev/null || echo "")

        if [ -n "$api_response" ]; then
            log_pass "Starlink API responding"
        else
            log_warn "Starlink API not responding (check dish connection)"
        fi
    fi
}

test_cron_jobs() {
    log_test "Testing scheduled jobs..."

    local cron_jobs
    cron_jobs=$(crontab -l 2>/dev/null || echo "")

    if echo "$cron_jobs" | grep -q "starlink_monitor.sh"; then
        log_pass "Quality monitoring scheduled"
    else
        log_fail "Quality monitoring not scheduled"
    fi

    if echo "$cron_jobs" | grep -q "starlink_logger.sh"; then
        log_pass "Performance logging scheduled"
    else
        log_warn "Performance logging not scheduled"
    fi

    if echo "$cron_jobs" | grep -q "check_starlink_api.sh"; then
        log_pass "API change detection scheduled"
    else
        log_warn "API change detection not scheduled"
    fi

    # Check cron service
    if pgrep crond >/dev/null; then
        log_pass "Cron service running"
    else
        log_fail "Cron service not running"
    fi
}

test_logs() {
    log_test "Testing logging system..."

    # Test log files
    if [ -f "/overlay/messages" ]; then
        log_pass "System log file exists"
        local log_size
        log_size=$(stat -f%z "/overlay/messages" 2>/dev/null || stat -c%s "/overlay/messages" 2>/dev/null || echo "0")
        log_info "Log file size: $log_size bytes"
    else
        log_fail "System log file not found"
    fi

    # Test performance log
    if [ -f "/root/starlink_performance_log.csv" ]; then
        log_pass "Performance log file exists"
    else
        log_warn "Performance log file not yet created"
    fi

    # Test recent log entries
    if logread | grep -q "StarlinkMonitor\|StarlinkLogger" | tail -1; then
        log_pass "Recent monitoring activity found in logs"
    else
        log_warn "No recent monitoring activity in logs (may need time to start)"
    fi
}

# Main verification
main() {
    echo "========================================="
    echo "Starlink Solution Verification"
    echo "========================================="
    echo

    test_binaries
    echo
    test_scripts
    echo
    test_configuration
    echo
    test_connectivity
    echo
    test_cron_jobs
    echo
    test_logs
    echo

    # Summary
    echo "========================================="
    echo "Verification Summary"
    echo "========================================="
    echo -e "${GREEN}Tests Passed: $TESTS_PASSED${NC}"
    echo -e "${YELLOW}Tests Warned: $TESTS_WARNED${NC}"
    echo -e "${RED}Tests Failed: $TESTS_FAILED${NC}"
    echo

    if [ "$TESTS_FAILED" -eq 0 ]; then
        echo -e "${GREEN}✓ Verification completed successfully!${NC}"
        echo "The Starlink monitoring solution is properly deployed and configured."
        echo
        echo "Next steps:"
        echo "1. Wait 5-10 minutes for initial data collection"
        echo "2. Check logs: logread | grep Starlink"
        echo "3. Monitor performance: tail -f /root/starlink_performance_log.csv"
        echo "4. Test failover by setting low thresholds temporarily"
        return 0
    else
        echo -e "${RED}✗ Verification found issues that need attention${NC}"
        echo "Please review the failed tests above and address any configuration issues."
        return 1
    fi
}

# Run verification
main
EOF

chmod +x /root/verify-starlink-setup.sh

# Run the verification
/root/verify-starlink-setup.sh
```

## Troubleshooting

### Common Issues

#### 1. grpcurl or jq not working

```bash
# Check if binaries are executable
ls -la /root/grpcurl /root/jq

# Test manually
/root/grpcurl --version
/root/jq --version

# Re-download if needed
curl -fL https://github.com/fullstorydev/grpcurl/releases/download/v1.9.3/grpcurl_1.9.3_linux_armv7.tar.gz -o /tmp/grpcurl.tar.gz
tar -zxf /tmp/grpcurl.tar.gz -C /root/ grpcurl
chmod +x /root/grpcurl
```

#### 2. Starlink API not accessible

```bash
# Check network connectivity to Starlink
ping -c 3 192.168.100.1

# Check route
ip route show | grep 192.168.100.1

# Test API manually
/root/grpcurl -plaintext -d '{"get_status":{}}' 192.168.100.1:9200 SpaceX.API.Device.Device/Handle
```

#### 3. mwan3 not working

```bash
# Check mwan3 status
mwan3 status

# Restart mwan3
mwan3 restart

# Check mwan3 configuration
uci show mwan3
```

#### 4. Cron jobs not running

```bash
# Check cron service
/etc/init.d/cron status

# Restart cron
/etc/init.d/cron restart

# Check crontab
crontab -l

# Check cron logs
logread | grep cron
```

#### 5. No logs being generated

```bash
# Check logging configuration
uci show system | grep log

# Check log file permissions
ls -la /overlay/messages

# Test logging manually
logger -t "TEST" "This is a test message"
logread | grep TEST
```

### Diagnostic Commands

```bash
# View real-time monitoring
logread -f | grep Starlink

# Check current quality metrics
/root/starlink_monitor.sh

# View performance data
tail -n 20 /root/starlink_performance_log.csv

# Check mwan3 status
mwan3 status

# View network routes
ip route show

# Check cron job execution
logread | grep cron | tail -10
```

## Maintenance

### Regular Monitoring

```bash
# Check system health (run weekly)
/root/verify-starlink-setup.sh

# Review performance trends
head -1 /root/starlink_performance_log.csv && tail -20 /root/starlink_performance_log.csv

# Check for failover events
logread | grep -E "FAILOVER|FAILBACK" | tail -10

# Monitor disk space
df -h /overlay
```

### Log Rotation

```bash
# Manual log rotation if needed
cp /overlay/messages /overlay/messages.backup
echo "$(date): Log rotated manually" > /overlay/messages

# Performance log rotation
head -1 /root/starlink_performance_log.csv > /root/starlink_performance_log.csv.new
tail -1000 /root/starlink_performance_log.csv >> /root/starlink_performance_log.csv.new
mv /root/starlink_performance_log.csv.new /root/starlink_performance_log.csv
```

### Configuration Updates

```bash
# Update quality thresholds
vi /root/config.sh

# Apply configuration changes (scripts will pick up automatically)
# No restart needed for threshold changes

# Update monitoring scripts
# Re-run relevant sections of the installation if needed
```

### Backup and Restore

```bash
# Create backup
tar -czf /root/starlink-backup-$(date +%Y%m%d).tar.gz \
    /root/config.sh \
    /root/starlink_*.sh \
    /root/check_starlink_api.sh \
    /root/log-shipper.sh \
    /etc/hotplug.d/iface/99-pushover_notify \
    /etc/config/azure

# Restore from backup (adjust paths as needed)
tar -xzf /root/starlink-backup-YYYYMMDD.tar.gz -C /
chmod +x /root/*.sh
chmod +x /etc/hotplug.d/iface/99-pushover_notify
```

## Advanced Configuration

### Custom Quality Metrics

You can extend the quality checking logic by modifying `/root/starlink_monitor.sh`:

```bash
# Add custom metrics like uptime monitoring
uptime_hours=$(echo "$status_json" | /root/jq -r '.dishGetStatus.uptimeS // 0' | awk '{print int($1/3600)}')

if [ "$uptime_hours" -lt 1 ]; then
    quality_good=false
    log "Quality issue: Recent restart (uptime: ${uptime_hours}h)"
fi
```

### Integration with External Systems

The solution provides several integration points:

1. **Syslog Integration**: All events are logged to syslog with tags
2. **File-based Data**: CSV files for performance data analysis
3. **State Files**: Machine-readable state in `/tmp/run/`
4. **Exit Codes**: Scripts return appropriate exit codes for automation

### Multi-WAN Priority Adjustment

For more complex failover scenarios, you can adjust the mwan3 configuration:

```bash
# Set fine-grained priorities
uci set mwan3.member1.metric='1'    # Starlink (highest priority)
uci set mwan3.member2.metric='2'    # LTE backup
uci set mwan3.member3.metric='3'    # Ethernet backup
uci set mwan3.member4.metric='10'   # Lowest priority backup

uci commit mwan3
mwan3 restart
```

This deployment guide provides both automated and manual installation options for a complete Starlink monitoring and
failover solution. The automated script handles all the complexity, while the manual steps give you full control over
each aspect of the deployment.
