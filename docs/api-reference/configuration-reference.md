# ⚙️ Complete Configuration Reference

## Overview

This document provides a complete reference for all UCI configuration options
available in the Autonomy system.

## 📋 Configuration Structure

The Autonomy configuration is organized into logical sections:

```text
autonomy
├── main           # Core system settings
├── interfaces     # Network interface configuration  
├── starlink       # Starlink-specific settings
├── cellular       # Cellular network configuration
├── wifi           # WiFi optimization settings
├── gps            # GPS and location services
├── notifications  # Alert and notification settings
├── predictive     # Machine learning and prediction
├── security       # Security monitoring settings
├── performance    # Performance and resource limits
├── logging        # Logging configuration
├── cache          # Caching settings
├── failover       # Failover behavior
└── external_apis  # External API credentials
```

## 🔧 Core System Settings (`autonomy.main`)

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `enabled` | boolean | `0` | Enable/disable autonomy service |
| `log_level` | string | `info` | Logging level: `debug`, `info`, `warn`, `error` |
| `check_interval` | integer | `30` | Health check interval (seconds) |
| `failover_delay` | integer | `5` | Delay before failover (seconds) |
| `startup_delay` | integer | `30` | Delay after boot before starting (seconds) |
| `config_reload_interval` | integer | `300` | Configuration reload interval (seconds) |

```bash
# Example configuration
uci set autonomy.main.enabled='1'
uci set autonomy.main.log_level='info'
uci set autonomy.main.check_interval='30'
uci set autonomy.main.failover_delay='5'
```

## 🌐 Network Interfaces (`autonomy.interfaces`)

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `auto_discovery` | boolean | `1` | Automatically discover network interfaces |
| `starlink_priority` | integer | `1` | Starlink interface priority (1=highest) |
| `cellular_priority` | integer | `2` | Cellular interface priority |
| `wifi_priority` | integer | `3` | WiFi interface priority |
| `lan_priority` | integer | `4` | LAN interface priority |
| `exclude_interfaces` | list | `[]` | Interfaces to exclude from management |

```bash
# Example configuration
uci set autonomy.interfaces.auto_discovery='1'
uci set autonomy.interfaces.starlink_priority='1'
uci set autonomy.interfaces.cellular_priority='2'
```

## 🛰️ Starlink Configuration (`autonomy.starlink`)

### Basic Settings

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `enabled` | boolean | `0` | Enable Starlink integration |
| `host` | string | `192.168.100.1` | Starlink dish IP address |
| `port` | integer | `9200` | Starlink gRPC port |
| `timeout` | integer | `10` | Connection timeout (seconds) |
| `check_interval` | integer | `60` | Status check interval (seconds) |

### Tracking Settings

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `tracking_enabled` | boolean | `1` | Enable satellite tracking |
| `obstruction_analysis` | boolean | `1` | Enable obstruction prediction |
| `prediction_window` | integer | `1440` | Prediction window (minutes) |
| `obstruction_threshold` | float | `0.6` | Obstruction sensitivity (0.0-1.0) |
| `min_elevation` | float | `25.0` | Minimum elevation angle (degrees) |
| `min_satellites` | integer | `3` | Minimum visible satellites |

### OAuth Settings (if required)

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `oauth_enabled` | boolean | `0` | Enable OAuth authentication |
| `oauth_token` | string | `''` | OAuth access token |
| `oauth_refresh_interval` | integer | `3600` | Token refresh interval (seconds) |

```bash
# Example Starlink configuration
uci set autonomy.starlink.enabled='1'
uci set autonomy.starlink.host='192.168.100.1'
uci set autonomy.starlink.tracking_enabled='1'
uci set autonomy.starlink.obstruction_threshold='0.6'
```

## 📱 Cellular Configuration (`autonomy.cellular`)

### Cellular Basic Settings

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `enabled` | boolean | `1` | Enable cellular monitoring |
| `primary_operator` | string | `''` | Primary cellular operator |
| `backup_operator` | string | `''` | Backup cellular operator |
| `signal_monitoring` | boolean | `1` | Enable signal quality monitoring |
| `data_usage_tracking` | boolean | `1` | Track data usage |
| `roaming_detection` | boolean | `1` | Detect roaming status |

### Signal Thresholds

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `rsrp_threshold` | integer | `-110` | RSRP threshold (dBm) |
| `rsrq_threshold` | integer | `-15` | RSRQ threshold (dB) |
| `sinr_threshold` | integer | `5` | SINR threshold (dB) |
| `signal_weight` | float | `0.4` | Signal quality weight in scoring |

### Data Limits

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `monthly_limit` | integer | `50` | Monthly data limit (GB) |
| `daily_limit` | integer | `5` | Daily data limit (GB) |
| `warning_threshold` | integer | `80` | Warning threshold (percent) |
| `critical_threshold` | integer | `95` | Critical threshold (percent) |

### Multi-SIM Settings

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `multi_sim` | boolean | `0` | Enable multi-SIM support |
| `sim1_operator` | string | `''` | SIM 1 operator name |
| `sim2_operator` | string | `''` | SIM 2 operator name |
| `auto_switch` | boolean | `1` | Automatically switch SIMs |
| `switch_threshold` | integer | `60` | Health score threshold for SIM switch |

```bash
# Example cellular configuration
uci set autonomy.cellular.enabled='1'
uci set autonomy.cellular.signal_monitoring='1'
uci set autonomy.cellular.rsrp_threshold='-110'
uci set autonomy.cellular.monthly_limit='50'
```

## 📶 WiFi Configuration (`autonomy.wifi`)

### WiFi Basic Settings

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `optimization` | boolean | `1` | Enable WiFi optimization |
| `channel_analysis` | boolean | `1` | Enable channel analysis |
| `interference_detection` | boolean | `1` | Detect interference |
| `auto_channel_switch` | boolean | `0` | Automatically switch channels |

### Configuration - Signal Thresholds

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `rssi_threshold` | integer | `-70` | RSSI threshold (dBm) |
| `noise_threshold` | integer | `-90` | Noise floor threshold (dBm) |
| `channel_utilization` | integer | `80` | Max channel utilization (percent) |
| `interference_threshold` | integer | `60` | Interference threshold (percent) |

### Scanning Settings

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `scan_interval` | integer | `300` | Channel scan interval (seconds) |
| `scan_duration` | integer | `10` | Scan duration (seconds) |
| `passive_scan` | boolean | `1` | Use passive scanning |

```bash
# Example WiFi configuration
uci set autonomy.wifi.optimization='1'
uci set autonomy.wifi.rssi_threshold='-70'
uci set autonomy.wifi.auto_channel_switch='1'
```

## 📍 GPS Configuration (`autonomy.gps`)

### GPS Basic Settings

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `enabled` | boolean | `0` | Enable GPS system |
| `sources` | list | `['rutos']` | GPS sources: `rutos,starlink,cellular` |
| `fusion_algorithm` | string | `weighted_average` | Fusion algorithm |
| `accuracy_threshold` | integer | `50` | Accuracy threshold (meters) |
| `update_interval` | integer | `60` | Update interval (seconds) |

### RUTOS GPS Settings

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `rutos_enabled` | boolean | `1` | Enable RUTOS GPS |
| `rutos_device` | string | `/dev/ttyUSB0` | GPS device path |
| `rutos_baudrate` | integer | `9600` | GPS device baudrate |
| `rutos_timeout` | integer | `10` | GPS timeout (seconds) |

### Starlink GPS Settings

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `starlink_enabled` | boolean | `1` | Enable Starlink GPS |
| `starlink_fallback` | boolean | `1` | Use as fallback source |
| `starlink_timeout` | integer | `15` | Starlink GPS timeout (seconds) |

### Cellular GPS Settings

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `cellular_enabled` | boolean | `1` | Enable cellular location |
| `cellular_fallback` | boolean | `1` | Use as fallback |
| `cellular_accuracy` | integer | `1000` | Expected accuracy (meters) |

### Movement Detection

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `movement_detection` | boolean | `1` | Enable movement detection |
| `movement_threshold` | integer | `100` | Movement threshold (meters) |
| `stationary_timeout` | integer | `300` | Stationary timeout (seconds) |

### Geofencing

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `geofencing` | boolean | `0` | Enable geofencing |
| `home_latitude` | float | `0.0` | Home location latitude |
| `home_longitude` | float | `0.0` | Home location longitude |
| `home_radius` | integer | `1000` | Home geofence radius (meters) |

```bash
# Example GPS configuration
uci set autonomy.gps.enabled='1'
uci set autonomy.gps.sources='rutos,starlink,cellular'
uci set autonomy.gps.movement_detection='1'
```

## 🔔 Notification Configuration (`autonomy.notifications`)

### Pushover Settings

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `pushover_enabled` | boolean | `0` | Enable Pushover notifications |
| `pushover_token` | string | `''` | Pushover application token |
| `pushover_user` | string | `''` | Pushover user key |
| `pushover_priority` | integer | `1` | Priority level (-2 to 2) |
| `pushover_sound` | string | `pushover` | Notification sound |

### Email Settings

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `email_enabled` | boolean | `0` | Enable email notifications |
| `smtp_server` | string | `''` | SMTP server hostname |
| `smtp_port` | integer | `587` | SMTP server port |
| `smtp_username` | string | `''` | SMTP username |
| `smtp_password` | string | `''` | SMTP password |
| `smtp_encryption` | string | `tls` | Encryption: `none`, `tls`, `ssl` |
| `email_to` | string | `''` | Recipient email address |
| `email_from` | string | `''` | Sender email address |

### Alert Configuration

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `alert_levels` | list | `['critical','warning']` | Alert levels to send |
| `failover_alerts` | boolean | `1` | Alert on failover events |
| `outage_predictions` | boolean | `1` | Alert on predicted outages |
| `location_alerts` | boolean | `0` | Alert on location changes |
| `security_alerts` | boolean | `1` | Alert on security events |

```bash
# Example notification configuration
uci set autonomy.notifications.pushover_enabled='1'
uci set autonomy.notifications.pushover_token='your-token'
uci set autonomy.notifications.alert_levels='critical,warning,info'
```

## 🔮 Predictive Configuration (`autonomy.predictive`)

### Machine Learning Settings

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `enabled` | boolean | `1` | Enable predictive features |
| `ml_enabled` | boolean | `1` | Enable machine learning |
| `trend_analysis` | boolean | `1` | Enable trend analysis |
| `pattern_recognition` | boolean | `1` | Enable pattern recognition |
| `learning_rate` | float | `0.01` | ML learning rate |
| `training_window` | integer | `604800` | Training window (seconds) |

### Prediction Windows

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `short_term` | integer | `300` | Short-term prediction (seconds) |
| `medium_term` | integer | `1800` | Medium-term prediction (seconds) |
| `long_term` | integer | `86400` | Long-term prediction (seconds) |
| `confidence_threshold` | float | `0.7` | Minimum confidence for predictions |

### Obstruction Management

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `obstruction_enabled` | boolean | `1` | Enable obstruction detection |
| `detection_enabled` | boolean | `1` | Enable predictive detection |
| `warning_window` | integer | `300` | Advance warning time (seconds) |
| `severity_threshold` | float | `0.7` | Severity threshold (0.0-1.0) |

```bash
# Example predictive configuration
uci set autonomy.predictive.enabled='1'
uci set autonomy.predictive.ml_enabled='1'
uci set autonomy.predictive.warning_window='300'
```

## 🔐 Security Configuration (`autonomy.security`)

### Security Monitoring

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `enabled` | boolean | `1` | Enable security monitoring |
| `threat_detection` | boolean | `1` | Enable threat detection |
| `brute_force_protection` | boolean | `1` | Protect against brute force |
| `port_scan_detection` | boolean | `1` | Detect port scans |
| `ddos_protection` | boolean | `1` | DDoS protection |

### Security Thresholds

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `failed_login_threshold` | integer | `5` | Failed login attempts before alert |
| `scan_detection_threshold` | integer | `10` | Port scan threshold |
| `rate_limit_threshold` | integer | `100` | Rate limit (requests/minute) |
| `connection_limit` | integer | `50` | Max concurrent connections |

```bash
# Example security configuration
uci set autonomy.security.enabled='1'
uci set autonomy.security.threat_detection='1'
uci set autonomy.security.failed_login_threshold='5'
```

## ⚡ Performance Configuration (`autonomy.performance`)

### Resource Limits

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `cpu_limit` | integer | `25` | CPU usage limit (percent) |
| `memory_limit` | integer | `128` | Memory limit (MB) |
| `gc_interval` | integer | `300` | Garbage collection interval (seconds) |
| `max_goroutines` | integer | `100` | Maximum goroutines |

### Processing Settings

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `parallel_processing` | boolean | `1` | Enable parallel processing |
| `worker_threads` | integer | `4` | Number of worker threads |
| `batch_processing` | boolean | `1` | Enable batch processing |
| `batch_size` | integer | `10` | Batch processing size |

```bash
# Example performance configuration
uci set autonomy.performance.cpu_limit='25'
uci set autonomy.performance.parallel_processing='1'
uci set autonomy.performance.worker_threads='4'
```

## 💾 Cache Configuration (`autonomy.cache`)

### Cache Settings

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `enabled` | boolean | `1` | Enable caching system |
| `location_ttl` | integer | `300` | Location cache TTL (seconds) |
| `cell_tower_ttl` | integer | `3600` | Cell tower cache TTL (seconds) |
| `gps_ttl` | integer | `60` | GPS cache TTL (seconds) |
| `starlink_ttl` | integer | `300` | Starlink data cache TTL (seconds) |

### Advanced Caching

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `predictive_loading` | boolean | `1` | Enable predictive cache loading |
| `geographic_clustering` | boolean | `1` | Enable geographic clustering |
| `intelligent_eviction` | boolean | `1` | Smart cache eviction |
| `max_size` | integer | `100` | Maximum cache size (MB) |
| `compression` | boolean | `1` | Enable cache compression |

```bash
# Example cache configuration
uci set autonomy.cache.enabled='1'
uci set autonomy.cache.predictive_loading='1'
uci set autonomy.cache.max_size='100'
```

## 🔄 Failover Configuration (`autonomy.failover`)

### Decision Engine

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `performance_weight` | float | `0.3` | Performance score weight |
| `location_weight` | float | `0.2` | Location accuracy weight |
| `cost_weight` | float | `0.2` | Cost consideration weight |
| `reliability_weight` | float | `0.3` | Reliability history weight |

### Failover Behavior

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `immediate_switch` | boolean | `0` | Enable immediate switching |
| `recovery_delay` | integer | `60` | Recovery delay (seconds) |
| `ping_validation` | boolean | `1` | Validate connectivity before switch |
| `health_recovery_threshold` | integer | `85` | Health score for recovery |
| `max_failovers_per_hour` | integer | `10` | Rate limit failovers |

```bash
# Example failover configuration
uci set autonomy.failover.performance_weight='0.3'
uci set autonomy.failover.recovery_delay='60'
uci set autonomy.failover.ping_validation='1'
```

## 🌍 External API Configuration (`autonomy.external_apis`)

### Space-Track API

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `space_track_enabled` | boolean | `0` | Enable Space-Track integration |
| `space_track_username` | string | `''` | Space-Track username |
| `space_track_password` | string | `''` | Space-Track password |
| `space_track_cache_ttl` | integer | `86400` | Cache TTL (seconds) |

### OpenCellID API

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `opencellid_enabled` | boolean | `0` | Enable OpenCellID integration |
| `opencellid_api_key` | string | `''` | OpenCellID API key |
| `opencellid_contribution` | boolean | `1` | Enable data contribution |
| `opencellid_cache_ttl` | integer | `3600` | Cache TTL (seconds) |

### Google APIs

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `google_enabled` | boolean | `0` | Enable Google APIs |
| `google_api_key` | string | `''` | Google API key |
| `google_fallback_priority` | integer | `3` | Fallback priority |

```bash
# Example external API configuration
uci set autonomy.external_apis.space_track_enabled='1'
uci set autonomy.external_apis.space_track_username='your_username'
uci set autonomy.external_apis.opencellid_enabled='1'
uci set autonomy.external_apis.opencellid_api_key='your_key'
```

## 📝 Logging Configuration (`autonomy.logging`)

### Log Settings

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `level` | string | `info` | Log level: `debug`, `info`, `warn`, `error` |
| `format` | string | `json` | Log format: `json`, `text` |
| `file` | string | `/var/log/autonomy.log` | Log file path |
| `max_size` | integer | `10` | Max log file size (MB) |
| `max_files` | integer | `5` | Number of rotated files |
| `syslog` | boolean | `1` | Also log to syslog |

### Component Logging

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `starlink_debug` | boolean | `0` | Enable Starlink debug logging |
| `gps_debug` | boolean | `0` | Enable GPS debug logging |
| `cellular_debug` | boolean | `0` | Enable cellular debug logging |
| `api_debug` | boolean | `0` | Enable API debug logging |

```bash
# Example logging configuration
uci set autonomy.logging.level='info'
uci set autonomy.logging.format='json'
uci set autonomy.logging.max_size='10'
```

## 🛠️ Configuration Management

### Backup and Restore

```bash
# Backup configuration
uci export autonomy > /tmp/autonomy-backup.conf

# Restore configuration
uci import autonomy < /tmp/autonomy-backup.conf
uci commit autonomy

# Reset to defaults
autonomy-cli config reset
```

### Validation

```bash
# Validate current configuration
autonomy-cli config validate

# Test configuration without applying
autonomy-cli config test

# Show configuration diff
autonomy-cli config diff
```

### Apply Changes

```bash
# Commit configuration changes
uci commit autonomy

# Restart service
/etc/init.d/autonomy restart

# Reload configuration without restart
ubus call autonomy reload_config
```

## 📚 Related Documentation

- [API Integrations Guide](api-integrations-guide.md) - External API setup
- [Getting Started](getting-started.md) - Basic setup
- [Production Guide](rutx50-production-guide.md) - Production configuration
- [Troubleshooting](../developer-guides/TROUBLESHOOTING.md) - Common issues

---

**Note**: Always run `uci commit autonomy` after making configuration changes and restart the service for changes to take effect.
