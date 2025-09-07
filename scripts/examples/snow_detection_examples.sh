#!/bin/bash

# Starlink Snow Detection System - UBUS Usage Examples
# This script demonstrates how to use the UBUS interface for the snow detection system

echo "=== Starlink Snow Detection System - UBUS Examples ==="
echo

# Check if UBUS service is available
echo "1. Checking UBUS service availability..."
ubus list | grep -q "starlink.snow_detection"
if [ $? -eq 0 ]; then
    echo "✓ Snow detection UBUS service is available"
else
    echo "✗ Snow detection UBUS service is not available"
    echo "Please ensure the snow detection system is running"
    exit 1
fi
echo

# Get current status
echo "2. Getting current snow detection status..."
ubus call starlink.snow_detection status
echo

# Get current configuration
echo "3. Getting current configuration..."
ubus call starlink.snow_detection config
echo

# Get statistics
echo "4. Getting statistics..."
ubus call starlink.snow_detection statistics
echo

# Enable snow detection
echo "5. Enabling snow detection..."
ubus call starlink.snow_detection enable
echo

# Force a check
echo "6. Forcing a snow detection check..."
ubus call starlink.snow_detection force_check
echo

# Start heating manually (for testing)
echo "7. Starting heating manually (for testing)..."
ubus call starlink.snow_detection start_heating
echo

# Wait a bit
echo "8. Waiting 10 seconds..."
sleep 10

# Stop heating
echo "9. Stopping heating..."
ubus call starlink.snow_detection stop_heating
echo

# Update configuration
echo "10. Updating configuration..."
ubus call starlink.snow_detection set_config '{
    "enabled": true,
    "detection_samples": 3,
    "obstruction_threshold": 0.03,
    "snr_degradation_threshold": 0.015,
    "temperature_threshold": 1.5,
    "verification_time": 180,
    "melt_timeout": 1200
}'
echo

# Get updated configuration
echo "11. Getting updated configuration..."
ubus call starlink.snow_detection get_config
echo

# Reset statistics
echo "12. Resetting statistics..."
ubus call starlink.snow_detection reset_stats
echo

# Get final status
echo "13. Getting final status..."
ubus call starlink.snow_detection status
echo

echo "=== Examples completed ==="
echo
echo "Additional UBUS commands you can use:"
echo "- ubus call starlink.snow_detection status"
echo "- ubus call starlink.snow_detection config"
echo "- ubus call starlink.snow_detection enable"
echo "- ubus call starlink.snow_detection disable"
echo "- ubus call starlink.snow_detection force_check"
echo "- ubus call starlink.snow_detection start_heating"
echo "- ubus call starlink.snow_detection stop_heating"
echo "- ubus call starlink.snow_detection statistics"
echo "- ubus call starlink.snow_detection reset_stats"
echo "- ubus call starlink.snow_detection set_config '{...}'"
echo "- ubus call starlink.snow_detection get_config"
echo
echo "UCI configuration commands:"
echo "- uci show autonomy.snow_detection"
echo "- uci set autonomy.snow_detection.enabled='1'"
echo "- uci set autonomy.snow_detection.detection_samples='5'"
echo "- uci set autonomy.snow_detection.obstruction_threshold='0.05'"
echo "- uci commit autonomy"
