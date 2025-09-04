# ⚙️ Complete Configuration Guide

## Overview

This guide covers all configuration settings for the Autonomy system, including UCI configuration, external API integrations, and advanced features.

## 🔧 Basic Configuration

### System Configuration
```bash
# Enable autonomy service
uci set autonomy.main.enabled='1'
uci set autonomy.main.log_level='info'  # debug, info, warn, error
uci set autonomy.main.check_interval='30'  # seconds
uci set autonomy.main.failover_delay='5'   # seconds before failover
uci commit autonomy
```

### Network Interface Configuration
```bash
# Configure interface priorities (1=highest, 10=lowest)
uci set autonomy.interfaces.starlink_priority='1'
uci set autonomy.interfaces.cellular_priority='2'
uci set autonomy.interfaces.wifi_priority='3'
uci set autonomy.interfaces.lan_priority='4'

# Health check settings
uci set autonomy.monitoring.ping_targets='1.1.1.1,8.8.8.8,1.0.0.1'
uci set autonomy.monitoring.ping_timeout='3'
uci set autonomy.monitoring.ping_count='3'
uci set autonomy.monitoring.health_threshold='75'  # 0-100 score
uci commit autonomy
```

## 🛰️ Starlink Configuration

### Basic Starlink Setup
```bash
# Enable Starlink integration
uci set autonomy.starlink.enabled='1'
uci set autonomy.starlink.host='192.168.100.1'  # Starlink dish IP
uci set autonomy.starlink.port='9200'            # gRPC port
uci set autonomy.starlink.timeout='10'           # seconds
uci set autonomy.starlink.check_interval='60'    # seconds

# Starlink tracking features
uci set autonomy.starlink.tracking_enabled='1'
uci set autonomy.starlink.obstruction_analysis='1'
uci set autonomy.starlink.prediction_window='1440'  # minutes (24 hours)
uci commit autonomy
```

### Advanced Starlink Settings
```bash
# Prediction thresholds
uci set autonomy.starlink.obstruction_threshold='0.6'   # 0.0-1.0
uci set autonomy.starlink.min_elevation='25'            # degrees
uci set autonomy.starlink.min_satellites='3'            # minimum visible

# OAuth configuration (if required)
uci set autonomy.starlink.oauth_enabled='1'
uci set autonomy.starlink.oauth_token='your-starlink-token'
uci set autonomy.starlink.oauth_refresh_interval='3600'  # seconds
uci commit autonomy
```

## 📱 Cellular Configuration

### Basic Cellular Setup
```bash
# Enable cellular monitoring
uci set autonomy.cellular.enabled='1'
uci set autonomy.cellular.primary_operator='Telia'
uci set autonomy.cellular.backup_operator='Roaming'

# Signal monitoring
uci set autonomy.cellular.signal_monitoring='1'
uci set autonomy.cellular.rsrp_threshold='-110'    # dBm
uci set autonomy.cellular.rsrq_threshold='-15'     # dB
uci set autonomy.cellular.sinr_threshold='5'       # dB

# Data usage tracking
uci set autonomy.cellular.data_usage_tracking='1'
uci set autonomy.cellular.monthly_limit='50'       # GB
uci set autonomy.cellular.warning_threshold='80'   # percent
uci commit autonomy
```

### Multi-SIM Configuration
```bash
# Configure multiple SIM cards
uci set autonomy.cellular.multi_sim='1'
uci set autonomy.cellular.sim1_operator='Telia'
uci set autonomy.cellular.sim2_operator='Telenor'
uci set autonomy.cellular.auto_switch='1'
uci set autonomy.cellular.switch_threshold='60'    # health score
uci commit autonomy
```

## 📍 GPS Configuration

### Multi-Source GPS Setup
```bash
# Enable GPS system
uci set autonomy.gps.enabled='1'
uci set autonomy.gps.sources='rutos,starlink,cellular'  # comma-separated
uci set autonomy.gps.fusion_algorithm='weighted_average'
uci set autonomy.gps.accuracy_threshold='50'            # meters
uci set autonomy.gps.update_interval='60'               # seconds

# RUTOS GPS settings
uci set autonomy.gps.rutos_enabled='1'
uci set autonomy.gps.rutos_device='/dev/ttyUSB0'
uci set autonomy.gps.rutos_baudrate='9600'

# Starlink GPS settings  
uci set autonomy.gps.starlink_enabled='1'
uci set autonomy.gps.starlink_fallback='1'

# Cellular GPS settings
uci set autonomy.gps.cellular_enabled='1'
uci set autonomy.gps.cellular_fallback='1'
uci commit autonomy
```

### Advanced GPS Features
```bash
# Movement detection
uci set autonomy.gps.movement_detection='1'
uci set autonomy.gps.movement_threshold='100'      # meters
uci set autonomy.gps.stationary_timeout='300'      # seconds

# Geofencing
uci set autonomy.gps.geofencing='1'
uci set autonomy.gps.home_latitude='59.3293'
uci set autonomy.gps.home_longitude='18.0686'
uci set autonomy.gps.home_radius='1000'            # meters
uci commit autonomy
```

## 📶 WiFi Configuration

### WiFi Optimization Setup
```bash
# Enable WiFi optimization
uci set autonomy.wifi.optimization='1'
uci set autonomy.wifi.channel_analysis='1'
uci set autonomy.wifi.interference_detection='1'
uci set autonomy.wifi.auto_channel_switch='1'

# Signal thresholds
uci set autonomy.wifi.rssi_threshold='-70'         # dBm
uci set autonomy.wifi.noise_threshold='-90'        # dBm
uci set autonomy.wifi.channel_utilization='80'     # percent

# Scanning settings
uci set autonomy.wifi.scan_interval='300'          # seconds
uci set autonomy.wifi.scan_duration='10'           # seconds
uci commit autonomy
```

## 🔔 Notification Configuration

### Pushover Notifications
```bash
# Enable Pushover
uci set autonomy.notifications.pushover_enabled='1'
uci set autonomy.notifications.pushover_token='your-pushover-token'
uci set autonomy.notifications.pushover_user='your-pushover-user'
uci set autonomy.notifications.pushover_priority='1'    # -2 to 2

# Alert levels
uci set autonomy.notifications.alert_levels='critical,warning,info'
uci set autonomy.notifications.location_alerts='1'
uci set autonomy.notifications.failover_alerts='1'
uci set autonomy.notifications.outage_predictions='1'
uci commit autonomy
```

### Email Notifications
```bash
# SMTP configuration
uci set autonomy.notifications.email_enabled='1'
uci set autonomy.notifications.smtp_server='smtp.gmail.com'
uci set autonomy.notifications.smtp_port='587'
uci set autonomy.notifications.smtp_username='your-email@gmail.com'
uci set autonomy.notifications.smtp_password='your-app-password'
uci set autonomy.notifications.email_to='alerts@yourcompany.com'
uci commit autonomy
```

### Webhook Notifications
```bash
# Webhook configuration
uci set autonomy.notifications.webhook_enabled='1'
uci set autonomy.notifications.webhook_url='https://your-webhook-endpoint.com'
uci set autonomy.notifications.webhook_secret='your-webhook-secret'
uci set autonomy.notifications.webhook_timeout='10'  # seconds
uci commit autonomy
```

## 🤖 Predictive Features

### Machine Learning Configuration
```bash
# Enable predictive failover
uci set autonomy.predictive.enabled='1'
uci set autonomy.predictive.ml_enabled='1'
uci set autonomy.predictive.trend_analysis='1'
uci set autonomy.predictive.pattern_recognition='1'

# Prediction windows
uci set autonomy.predictive.short_term='300'       # 5 minutes
uci set autonomy.predictive.medium_term='1800'     # 30 minutes  
uci set autonomy.predictive.long_term='86400'      # 24 hours

# Learning parameters
uci set autonomy.predictive.learning_rate='0.01'
uci set autonomy.predictive.training_window='604800'  # 1 week
uci commit autonomy
```

### Obstruction Management
```bash
# Predictive obstruction detection
uci set autonomy.obstruction.enabled='1'
uci set autonomy.obstruction.detection_enabled='1'
uci set autonomy.obstruction.warning_window='300'     # 5 minutes advance warning
uci set autonomy.obstruction.severity_threshold='0.7' # 0.0-1.0
uci commit autonomy
```

## 💾 Caching Configuration

### Location Caching
```bash
# Cache settings
uci set autonomy.cache.location_ttl='300'          # 5 minutes
uci set autonomy.cache.cell_tower_ttl='3600'       # 1 hour
uci set autonomy.cache.gps_ttl='60'                # 1 minute

# Advanced caching
uci set autonomy.cache.predictive_loading='1'
uci set autonomy.cache.geographic_clustering='1'
uci set autonomy.cache.intelligent_eviction='1'
uci set autonomy.cache.max_size='100'              # MB
uci commit autonomy
```

## 🔐 Security Configuration

### Security Monitoring
```bash
# Enable security features
uci set autonomy.security.enabled='1'
uci set autonomy.security.threat_detection='1'
uci set autonomy.security.brute_force_protection='1'
uci set autonomy.security.port_scan_detection='1'

# Security thresholds
uci set autonomy.security.failed_login_threshold='5'
uci set autonomy.security.scan_detection_threshold='10'
uci set autonomy.security.rate_limit_threshold='100'   # requests/minute
uci commit autonomy
```

## 📊 Performance Configuration

### Resource Management
```bash
# Performance settings
uci set autonomy.performance.cpu_limit='25'        # percent
uci set autonomy.performance.memory_limit='128'    # MB
uci set autonomy.performance.gc_interval='300'     # seconds

# Optimization settings
uci set autonomy.performance.parallel_processing='1'
uci set autonomy.performance.worker_threads='4'
uci set autonomy.performance.batch_processing='1'
uci commit autonomy
```

### Logging Configuration
```bash
# Logging settings
uci set autonomy.logging.level='info'              # debug, info, warn, error
uci set autonomy.logging.format='json'             # json, text
uci set autonomy.logging.file='/var/log/autonomy.log'
uci set autonomy.logging.max_size='10'             # MB
uci set autonomy.logging.max_files='5'             # rotation
uci set autonomy.logging.syslog='1'                # also log to syslog
uci commit autonomy
```

## 🔄 Failover Configuration

### Decision Engine
```bash
# Hybrid weight system
uci set autonomy.decision.performance_weight='0.3'
uci set autonomy.decision.location_weight='0.2'
uci set autonomy.decision.cost_weight='0.2'
uci set autonomy.decision.reliability_weight='0.3'

# Failover behavior
uci set autonomy.failover.immediate_switch='0'     # gradual failover
uci set autonomy.failover.recovery_delay='60'      # seconds
uci set autonomy.failover.ping_validation='1'      # validate before switch
uci set autonomy.failover.health_recovery_threshold='85'  # score to recover
uci commit autonomy
```

## 📋 Configuration Validation

### Verify Configuration
```bash
# Check all autonomy settings
uci show autonomy

# Validate configuration
autonomy-cli config validate

# Test configuration
autonomy-cli config test

# Export configuration
autonomy-cli config export > autonomy-backup.conf
```

### Apply Configuration
```bash
# Commit all changes
uci commit autonomy

# Restart service to apply
/etc/init.d/autonomy restart

# Verify service status
/etc/init.d/autonomy status
```

## 🏭 Production Configuration Templates

### High Availability Setup
```bash
# Production-ready configuration
uci set autonomy.main.enabled='1'
uci set autonomy.main.log_level='warn'
uci set autonomy.monitoring.health_threshold='80'
uci set autonomy.failover.recovery_delay='30'
uci set autonomy.predictive.enabled='1'
uci set autonomy.notifications.pushover_enabled='1'
uci set autonomy.security.enabled='1'
uci commit autonomy
```

### Development Setup
```bash
# Development configuration
uci set autonomy.main.log_level='debug'
uci set autonomy.monitoring.check_interval='10'
uci set autonomy.failover.immediate_switch='1'
uci set autonomy.performance.parallel_processing='0'
uci commit autonomy
```

### Minimal Setup
```bash
# Basic failover only
uci set autonomy.main.enabled='1'
uci set autonomy.starlink.enabled='0'
uci set autonomy.gps.enabled='0'
uci set autonomy.predictive.enabled='0'
uci set autonomy.notifications.pushover_enabled='0'
uci commit autonomy
```

## 📚 Related Documentation

- [API Integrations Guide](api-integrations-guide.md) - External API setup
- [Getting Started](getting-started.md) - Basic setup
- [User Guide](USER_GUIDE.md) - Complete user manual
- [Production Deployment](../deployment/production-deployment.md) - Production setup

---

**Tip**: Always run `uci commit autonomy` after making changes and restart the service for changes to take effect.