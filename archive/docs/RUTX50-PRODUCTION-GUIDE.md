# RUTX50 Production Deployment Guide

**Version:** 2.6.0 | **Updated:** 2025-07-24

This guide provides step-by-step instructions for deploying the Starlink failover solution on a RUTX50 router based on
real-world production configuration analysis.

## 📋 Pre-deployment Checklist

### Hardware Configuration Verified

- ✅ **RUTX50** router with latest firmware (`RUTX_R_00.07.15.2` or newer)
- ✅ **Starlink dish** in Bypass Mode connected to WAN port
- ✅ **Dual SIM setup** (Primary: Telia, Backup: Roaming SIM)
- ✅ **GPS enabled** and functioning
- ✅ **Network interfaces** properly configured

### Current Network Setup Analysis

Your configuration shows the following setup that we'll enhance:

```bash
# Interface Priority (from your mwan3 config)
member1 (wan)        - Starlink     - metric=1 (highest priority)
member3 (mob1s1a1)   - SIM Telia    - metric=2 (primary cellular)
member4 (mob1s2a1)   - SIM Roaming  - metric=4 (backup cellular)
```

## 🚀 Enhanced Deployment Steps

### 1. Install Enhanced Solution

```bash
# Download the advanced deployment script
curl -fL https://raw.githubusercontent.com/markus-lassfolk/autonomy/main/deploy-starlink-solution-rutos.sh -o deploy-starlink-solution-rutos.sh

# Make executable
chmod +x deploy-starlink-solution-rutos.sh

# Run enhanced deployment
./deploy-starlink-solution-rutos.sh --advanced --rutx50-optimized
```

### 2. Configure for Your Environment

```bash
# Use the advanced configuration template
cp /root/starlink-monitor/config/config.advanced.template.sh /root/starlink-monitor/config/config.sh

# Edit with your specific settings
nano /root/starlink-monitor/config/config.sh
```

**Key settings to customize:**

```bash
# Your Pushover credentials
PUSHOVER_TOKEN="your-pushover-token"
PUSHOVER_USER="your-pushover-user-key"

# Match your cellular setup
CELLULAR_PRIMARY_IFACE="mob1s1a1"    # Telia SIM
CELLULAR_BACKUP_IFACE="mob1s2a1"     # Roaming SIM

# GPS integration (if using MQTT like your config shows)
ENABLE_MQTT_LOGGING=1
MQTT_BROKER="your-mqtt-broker-ip"          # Your existing MQTT broker
```

### 3. Enhanced mwan3 Configuration

Your current mwan3 config is good, but we'll optimize the health checks:

```bash
# Enhanced Starlink monitoring (more aggressive than standard ping)
uci delete mwan3.@condition[1]  # Remove existing wan condition
uci add mwan3 condition
uci set mwan3.@condition[-1].interface='wan'
uci set mwan3.@condition[-1].track_method='ping'
uci set mwan3.@condition[-1].track_ip='1.0.0.1' '8.8.8.8' '1.1.1.1'
uci set mwan3.@condition[-1].reliability='2'      # Require 2/3 to succeed
uci set mwan3.@condition[-1].timeout='1'
uci set mwan3.@condition[-1].interval='5'         # Check every 5 seconds
uci set mwan3.@condition[-1].count='3'
uci set mwan3.@condition[-1].family='ipv4'
uci set mwan3.@condition[-1].up='2'               # 2 successful checks to mark up
uci set mwan3.@condition[-1].down='3'             # 3 failed checks to mark down

# Enhanced recovery settings for mobile environment
uci set mwan3.wan.recovery_wait='15'              # Wait 15s before recovery

# Commit changes
uci commit mwan3
mwan3 restart
```

### 4. GPS-Enhanced Failover Integration

Based on your GPS configuration, enable location-aware failover:

```bash
# Create GPS integration script
cat > /root/starlink-monitor/scripts/gps-enhanced-failover.sh << 'EOF'
#!/bin/bash

source /root/starlink-monitor/config/config.sh

get_gps_location() {
    # Use your existing GPS API (matching your config)
    local gps_data
    gps_data=$(curl -s -H "Authorization: Bearer $(get_api_token)" \
                    "http://your-router-ip/api/gps/position/status" 2>/dev/null)

    if [[ -n "$gps_data" ]] && echo "$gps_data" | jq -e '.latitude' >/dev/null 2>&1; then
        echo "$gps_data"
        return 0
    fi
    return 1
}

calculate_movement() {
    local current_lat current_lon last_lat last_lon
    local gps_data="$1"

    current_lat=$(echo "$gps_data" | jq -r '.latitude')
    current_lon=$(echo "$gps_data" | jq -r '.longitude')

    if [[ -f "/tmp/last_gps_position.json" ]]; then
        last_lat=$(jq -r '.latitude' /tmp/last_gps_position.json)
        last_lon=$(jq -r '.longitude' /tmp/last_gps_position.json)

        # Calculate distance using haversine formula
        local distance
        distance=$(python3 -c "
import math
lat1, lon1 = math.radians($last_lat), math.radians($last_lon)
lat2, lon2 = math.radians($current_lat), math.radians($current_lon)
dlat, dlon = lat2 - lat1, lon2 - lon1
a = math.sin(dlat/2)**2 + math.cos(lat1) * math.cos(lat2) * math.sin(dlon/2)**2
distance = 6371000 * 2 * math.asin(math.sqrt(a))
print(int(distance))
        ")

        echo "$distance"
    else
        echo "0"
    fi

    # Save current position
    echo "$gps_data" > /tmp/last_gps_position.json
}

# Main GPS-enhanced failover logic
main() {
    local gps_data movement_distance

    gps_data=$(get_gps_location)
    if [[ $? -eq 0 ]]; then
        movement_distance=$(calculate_movement "$gps_data")

        # If moved more than threshold, reset Starlink obstruction map
        if [[ "$movement_distance" -gt "${STARLINK_OBSTRUCTION_RESET_DISTANCE:-500}" ]]; then
            logger -t "GPS-Failover" "Moved ${movement_distance}m, resetting Starlink obstruction map"

            # Reset Starlink obstruction map via gRPC
            grpcurl -plaintext -d '{"stow": {"unstow": false}}' \
                "${STARLINK_IP}" SpaceX.API.Device.Device/Handle >/dev/null 2>&1
            sleep 2
            grpcurl -plaintext -d '{"stow": {"unstow": true}}' \
                "${STARLINK_IP}" SpaceX.API.Device.Device/Handle >/dev/null 2>&1

            # Send notification
            if [[ "${NOTIFY_ON_GPS_STATUS:-0}" == "1" ]]; then
                send_pushover_notification "GPS Movement Detected" \
                    "Moved ${movement_distance}m, reset Starlink obstruction map" "normal"
            fi
        fi
    fi
}

main "$@"
EOF

chmod +x /root/starlink-monitor/scripts/gps-enhanced-failover.sh
```

### 5. Integration with Your Existing Systems

#### MQTT Integration (based on your mosquitto config)

```bash
# Enable MQTT logging in your config
sed -i 's/ENABLE_MQTT_LOGGING=0/ENABLE_MQTT_LOGGING=1/' /root/starlink-monitor/config/config.sh

# Create MQTT publisher script
cat > /root/starlink-monitor/scripts/mqtt-publisher.sh << 'EOF'
#!/bin/bash

source /root/starlink-monitor/config/config.sh

publish_starlink_status() {
    local status="$1"
    local topic="${MQTT_TOPIC_PREFIX:-starlink}/status"

    # Publish to your existing MQTT broker
    mosquitto_pub -h "${MQTT_BROKER}" -p "${MQTT_PORT:-1883}" \
                  -t "$topic" -m "$status" -q 1
}

# Integrate with main monitoring script
# Add this to your starlink_monitor.sh calls
EOF

chmod +x /root/starlink-monitor/scripts/mqtt-publisher.sh
```

#### RMS Integration (based on your rms_mqtt config)

```bash
# Enable RMS monitoring if desired
cat > /root/starlink-monitor/scripts/rms-integration.sh << 'EOF'
#!/bin/bash

source /root/starlink-monitor/config/config.sh

send_rms_alert() {
    local alert_type="$1"
    local message="$2"

    # Send alert via RMS system (using your existing RMS config)
    # This integrates with your rms_mqtt.rms_connect_mqtt setup
    echo "$message" > /tmp/rms_starlink_alert.txt

    # Log to system for RMS pickup
    logger -t "RMS-Starlink" "$alert_type: $message"
}
EOF

chmod +x /root/starlink-monitor/scripts/rms-integration.sh
```

### 6. Enhanced Monitoring with Your Periodic Reboot

Based on your `periodic_reboot` config (Thursday 04:00), let's add intelligent scheduling:

```bash
# Create intelligent reboot script
cat > /root/starlink-monitor/scripts/intelligent-reboot.sh << 'EOF'
#!/bin/bash

source /root/starlink-monitor/config/config.sh

# Check if Starlink is having issues before scheduled reboot
check_starlink_health() {
    local issues=0

    # Get Starlink status
    local starlink_status
    starlink_status=$(grpcurl -plaintext -d '{"get_status":{}}' \
                             "${STARLINK_IP}" SpaceX.API.Device.Device/Handle 2>/dev/null)

    if [[ -n "$starlink_status" ]]; then
        # Check for obstruction issues
        local obstructed
        obstructed=$(echo "$starlink_status" | jq -r '.dishGetStatus.obstructed // false')

        if [[ "$obstructed" == "true" ]]; then
            ((issues++))
            logger -t "Intelligent-Reboot" "Starlink obstructed, reboot may help"
        fi

        # Check uptime - if recently rebooted, skip
        local uptime_hours
        uptime_hours=$(awk '{print int($1/3600)}' /proc/uptime)

        if [[ "$uptime_hours" -lt 2 ]]; then
            logger -t "Intelligent-Reboot" "Recent reboot detected, skipping scheduled reboot"
            exit 0
        fi
    fi

    return $issues
}

# Only reboot if there are issues or it's been more than 7 days
if check_starlink_health || [[ $(awk '{print int($1/86400)}' /proc/uptime) -gt 7 ]]; then
    logger -t "Intelligent-Reboot" "Performing intelligent reboot"
    reboot
else
    logger -t "Intelligent-Reboot" "System healthy, skipping reboot"
fi
EOF

chmod +x /root/starlink-monitor/scripts/intelligent-reboot.sh

# Update your periodic reboot to use intelligent script
uci set periodic_reboot.@reboot_instance[0].action='2'  # Custom script
uci set periodic_reboot.@reboot_instance[0].script_path='/root/starlink-monitor/scripts/intelligent-reboot.sh'
uci commit periodic_reboot
```

### 7. Final Validation

```bash
# Run comprehensive validation
/root/starlink-monitor/scripts/validate-config.sh --advanced

# Test all components
/root/starlink-monitor/tests/test-suite.sh --rutx50

# Verify integration with your systems
/root/starlink-monitor/scripts/test-integration.sh --mqtt --gps --rms
```

## 📊 Monitoring Your Enhanced Setup

### Dashboard Integration

Based on your system, you can monitor:

1. **WebUI Integration**: Enhanced overview widgets
2. **MQTT Monitoring**: Real-time status via your MQTT broker
3. **SMS Status**: Use your existing SMS utils for remote status
4. **RMS Dashboard**: Integration with Teltonika RMS

### Performance Monitoring

```bash
# Real-time monitoring
watch -n 5 'cat /tmp/run/starlink_monitor.health'

# Historical analysis
tail -f /var/log/starlink_performance.log | grep -E "(FAIL|RECOVER|GPS)"

# Integration status
mqtt_sub -h your-mqtt-broker-ip -t "starlink/+"
```

## 🔧 Maintenance

### Weekly Checks

- Review performance logs
- Check GPS movement tracking
- Verify MQTT integration
- Monitor cellular data usage

### Monthly Optimization

- Analyze failover patterns
- Optimize thresholds based on usage
- Update firmware and scripts
- Review notification effectiveness

This enhanced setup leverages your existing RUTX50 configuration while adding intelligent Starlink-specific monitoring
and failover capabilities.
