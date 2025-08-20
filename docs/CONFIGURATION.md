# Configuration Reference

This document provides a complete reference for all autonomy configuration options using the UCI (Unified Configuration Interface) system.

## Configuration File Location

The main configuration file is located at `/etc/config/autonomy` and follows the UCI format.

## Main Configuration Section

### Basic Settings

```uci
config autonomy 'main'
    option enable '1'                    # Enable/disable the daemon
    option use_mwan3 '1'                 # Use mwan3 for failover control
    option poll_interval_ms '1500'       # Polling interval in milliseconds
    option predictive '1'                # Enable predictive failover
    option switch_margin '10'            # Hysteresis margin for switching
    option log_level 'info'              # Logging level (debug, info, warn, error)
```

### Advanced Settings

```uci
config autonomy 'main'
    # Performance settings
    option max_memory_mb '64'            # Maximum memory usage in MB
    option gc_interval_sec '300'         # Garbage collection interval
    option telemetry_retention_hours '24' # Telemetry data retention
    
    # Decision engine settings
    option decision_timeout_ms '5000'    # Decision timeout
    option health_check_interval_ms '1000' # Health check frequency
    option scoring_algorithm 'weighted'  # Scoring algorithm (weighted, simple)
    
    # Integration settings
    option ubus_timeout_ms '1000'        # ubus operation timeout
    option uci_timeout_ms '500'          # UCI operation timeout
    option mwan3_timeout_ms '2000'       # mwan3 operation timeout
```

## GPS Configuration

```uci
config autonomy 'gps'
    option enable '1'                    # Enable GPS functionality
    option device '/dev/ttyUSB0'         # GPS device path
    option baud_rate '9600'              # GPS baud rate
    option update_interval_ms '5000'     # GPS update interval
    option cache_timeout_sec '300'       # GPS cache timeout
    option fallback_to_cellular '1'      # Use cellular location as fallback
    option opencellid_enable '1'         # Enable OpenCellID integration
    option opencellid_api_key ''         # OpenCellID API key (optional)
```

## Notification Configuration

```uci
config autonomy 'notifications'
    option enable '1'                    # Enable notifications
    option rate_limit_minutes '5'        # Rate limiting interval
    option max_notifications_per_hour '12' # Maximum notifications per hour
    
    # Pushover settings
    option pushover_enable '0'           # Enable Pushover notifications
    option pushover_token ''             # Pushover application token
    option pushover_user_key ''          # Pushover user key
    
    # Email settings
    option email_enable '0'              # Enable email notifications
    option email_smtp_server ''          # SMTP server address
    option email_smtp_port '587'         # SMTP server port
    option email_username ''             # SMTP username
    option email_password ''             # SMTP password
    option email_from ''                 # From email address
    option email_to ''                   # To email address
    
    # Webhook settings
    option webhook_enable '0'            # Enable webhook notifications
    option webhook_url ''                # Webhook URL
    option webhook_timeout_ms '5000'     # Webhook timeout
    option webhook_retry_count '3'       # Webhook retry attempts
```

## Interface-Specific Configuration

### Starlink Configuration

```uci
config autonomy 'starlink'
    option enable '1'                    # Enable Starlink monitoring
    option interface 'starlink'          # Starlink interface name
    option api_timeout_ms '3000'         # Starlink API timeout
    option metrics_interval_ms '2000'    # Metrics collection interval
    option obstruction_threshold '0.1'   # Obstruction threshold
    option signal_quality_weight '0.4'   # Signal quality weight in scoring
    option latency_weight '0.3'          # Latency weight in scoring
    option throughput_weight '0.3'       # Throughput weight in scoring
```

### Cellular Configuration

```uci
config autonomy 'cellular'
    option enable '1'                    # Enable cellular monitoring
    option interface 'wwan0'             # Cellular interface name
    option sim_count '2'                 # Number of SIM cards
    option rsrp_threshold '-110'         # RSRP threshold in dBm
    option rsrq_threshold '-12'          # RSRQ threshold in dB
    option sinr_threshold '5'            # SINR threshold in dB
    option signal_strength_weight '0.5'  # Signal strength weight
    option data_usage_weight '0.3'       # Data usage weight
    option cost_weight '0.2'             # Cost weight
```

### WiFi Configuration

```uci
config autonomy 'wifi'
    option enable '1'                    # Enable WiFi monitoring
    option interface 'wlan0'             # WiFi interface name
    option rssi_threshold '-70'          # RSSI threshold in dBm
    option channel_scan_interval_ms '10000' # Channel scan interval
    option interference_detection '1'    # Enable interference detection
    option rssi_weight '0.6'             # RSSI weight in scoring
    option channel_quality_weight '0.4'  # Channel quality weight
```

### LAN Configuration

```uci
config autonomy 'lan'
    option enable '1'                    # Enable LAN monitoring
    option interface 'eth0'              # LAN interface name
    option latency_threshold_ms '50'     # Latency threshold
    option packet_loss_threshold '0.01'  # Packet loss threshold (1%)
    option bandwidth_weight '0.5'        # Bandwidth weight
    option reliability_weight '0.5'      # Reliability weight
```

## Watchdog Configuration

```uci
config autonomy 'watchdog'
    option enable '1'                    # Enable watchdog functionality
    option check_interval_sec '30'       # Health check interval
    option restart_threshold '3'         # Restart threshold
    option restart_timeout_sec '60'      # Restart timeout
    option memory_threshold_mb '50'      # Memory usage threshold
    option cpu_threshold_percent '80'    # CPU usage threshold
```

## Security Configuration

```uci
config autonomy 'security'
    option enable '1'                    # Enable security features
    option audit_logging '1'             # Enable audit logging
    option threat_detection '1'          # Enable threat detection
    option brute_force_threshold '10'    # Brute force detection threshold
    option port_scan_threshold '100'     # Port scan detection threshold
    option dos_threshold '1000'          # DoS detection threshold
```

## Telemetry Configuration

```uci
config autonomy 'telemetry'
    option enable '1'                    # Enable telemetry collection
    option buffer_size '1000'            # Ring buffer size
    option publish_interval_ms '5000'    # MQTT publish interval
    option mqtt_enable '0'               # Enable MQTT publishing
    option mqtt_broker ''                # MQTT broker address
    option mqtt_port '1883'              # MQTT broker port
    option mqtt_username ''              # MQTT username
    option mqtt_password ''              # MQTT password
    option mqtt_topic 'autonomy/telemetry' # MQTT topic
```

## Performance Configuration

```uci
config autonomy 'performance'
    option profiling_enable '0'          # Enable performance profiling
    option profile_interval_sec '60'     # Profiling interval
    option gc_target_percent '50'        # GC target percentage
    option max_goroutines '100'          # Maximum number of goroutines
    option connection_pool_size '10'     # Connection pool size
```

## Example Complete Configuration

```uci
# Main configuration
config autonomy 'main'
    option enable '1'
    option use_mwan3 '1'
    option poll_interval_ms '1500'
    option predictive '1'
    option switch_margin '10'
    option log_level 'info'
    option max_memory_mb '64'
    option telemetry_retention_hours '24'

# GPS configuration
config autonomy 'gps'
    option enable '1'
    option device '/dev/ttyUSB0'
    option baud_rate '9600'
    option update_interval_ms '5000'
    option fallback_to_cellular '1'

# Notifications
config autonomy 'notifications'
    option enable '1'
    option rate_limit_minutes '5'
    option pushover_enable '1'
    option pushover_token 'your_token_here'
    option pushover_user_key 'your_user_key_here'

# Starlink monitoring
config autonomy 'starlink'
    option enable '1'
    option interface 'starlink'
    option api_timeout_ms '3000'
    option obstruction_threshold '0.1'

# Cellular monitoring
config autonomy 'cellular'
    option enable '1'
    option interface 'wwan0'
    option sim_count '2'
    option rsrp_threshold '-110'

# WiFi monitoring
config autonomy 'wifi'
    option enable '1'
    option interface 'wlan0'
    option rssi_threshold '-70'

# Watchdog
config autonomy 'watchdog'
    option enable '1'
    option check_interval_sec '30'
    option restart_threshold '3'
```

## Configuration Validation

You can validate your configuration using:

```bash
# Validate configuration syntax
autonomyctl config validate

# Test configuration
autonomyctl config test

# Export current configuration
autonomyctl config export

# Import configuration from file
autonomyctl config import /path/to/config.json
```

## Configuration Management

### Reloading Configuration

```bash
# Reload configuration without restart
autonomyctl config reload

# Restart service with new configuration
/etc/init.d/autonomy restart
```

### Configuration Backup

```bash
# Backup current configuration
cp /etc/config/autonomy /etc/config/autonomy.backup

# Restore configuration
cp /etc/config/autonomy.backup /etc/config/autonomy
```

## Troubleshooting Configuration

### Common Issues

1. **Invalid syntax**: Check UCI format and option values
2. **Missing dependencies**: Ensure required packages are installed
3. **Permission issues**: Verify file permissions on configuration files
4. **Interface not found**: Check interface names and availability

### Debug Configuration

```bash
# Show parsed configuration
autonomyctl config show

# Show configuration with defaults
autonomyctl config show --defaults

# Validate configuration with verbose output
autonomyctl config validate --verbose
```

For more detailed troubleshooting, see [Troubleshooting Guide](TROUBLESHOOTING.md).
