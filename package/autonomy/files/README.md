# autonomy - Autonomous Multi-Interface Failover Daemon

## Overview

The autonomy daemon provides intelligent failover between multiple network interfaces including Starlink, cellular, WiFi, and LAN connections. It features advanced monitoring, predictive switching, and comprehensive health management.

## Features

- **Multi-Interface Failover**: Automatic switching between Starlink, cellular, WiFi, and LAN
- **Predictive Switching**: Machine learning-based decision making
- **Starlink Integration**: Full API integration with diagnostics and health monitoring
- **Real-time Monitoring**: Comprehensive telemetry and health tracking
- **Notifications**: Pushover integration for network events
- **mwan3 Integration**: Seamless integration with OpenWrt multi-WAN

## Quick Start

1. **Install the package**:
   ```bash
   opkg install autonomy
   ```

2. **Configure interfaces** (edit `/etc/config/autonomy`):
   ```bash
   uci set autonomy.starlink.name='wan'
   uci set autonomy.cellular.name='wwan'
   uci set autonomy.wifi.name='wlan'
   uci commit autonomy
   ```

3. **Start the service**:
   ```bash
   /etc/init.d/autonomy start
   ```

4. **Check status**:
   ```bash
   autonomyctl status
   ```

## Configuration

### Main Configuration (`/etc/config/autonomy`)

The main configuration file uses UCI format and includes:

- **Core Settings**: Polling intervals, memory limits, decision parameters
- **Thresholds**: Failover and restore conditions
- **Interface Configurations**: Network interface definitions
- **Starlink API**: Connection settings for Starlink dish
- **Monitoring**: MQTT and notification settings

### Watchdog Configuration (`/etc/autonomy/watch.conf`)

Optional watchdog configuration for:
- Automatic bug reporting (opt-in)
- System health monitoring
- Crash detection and recovery
- External notifications

## Usage

### Service Management

```bash
# Start the daemon
/etc/init.d/autonomy start

# Stop the daemon
/etc/init.d/autonomy stop

# Restart the daemon
/etc/init.d/autonomy restart

# Check status
/etc/init.d/autonomy status

# Reload configuration
/etc/init.d/autonomy reload
```

### Command Line Interface

```bash
# Show service status
autonomyctl status

# Show configuration
autonomyctl config

# Show interface information
autonomyctl interfaces

# Show telemetry data
autonomyctl telemetry

# Show health information
autonomyctl health

# Reload configuration
autonomyctl reload
```

### UCI Configuration

```bash
# Enable the service
uci set autonomy.main.enable='1'

# Set log level
uci set autonomy.main.log_level='info'

# Configure Starlink API
uci set autonomy.api.host='192.168.100.1'
uci set autonomy.api.port='9200'

# Set failover thresholds
uci set autonomy.failover.loss='8'
uci set autonomy.failover.latency='1500'

# Commit changes
uci commit autonomy
```

## Monitoring

### Logs

- **Service logs**: `/var/log/autonomyd.log`
- **System logs**: `logread | grep autonomy`

### Health Checks

```bash
# Perform health check
/etc/init.d/autonomy health

# Show detailed information
/etc/init.d/autonomy info
```

### Telemetry

Access real-time telemetry via ubus:
```bash
ubus call autonomy telemetry
ubus call autonomy health
ubus call autonomy status
```

## Troubleshooting

### Common Issues

1. **Service won't start**:
   - Check configuration: `/etc/init.d/autonomy test`
   - Verify dependencies: `opkg list-installed | grep ubus`

2. **No failover occurring**:
   - Check interface names: `autonomyctl interfaces`
   - Verify thresholds: `autonomyctl config`
   - Check logs: `/etc/init.d/autonomy logs`

3. **Starlink integration issues**:
   - Verify dish IP: `ping 192.168.100.1`
   - Check API access: `curl http://192.168.100.1:9200/status`

### Debug Mode

Enable debug logging:
```bash
uci set autonomy.main.log_level='debug'
uci commit autonomy
/etc/init.d/autonomy restart
```

## Files and Directories

- **Binary**: `/usr/sbin/autonomyd`
- **Configuration**: `/etc/config/autonomy`
- **Watchdog Config**: `/etc/autonomy/watch.conf`
- **Control Script**: `/usr/libexec/autonomyctl`
- **Service Script**: `/etc/init.d/autonomy`
- **Logs**: `/var/log/autonomyd.log`
- **Data Directory**: `/var/lib/autonomy`

## Support

For more information and documentation:
- **Project Repository**: https://github.com/autonomy/autonomy
- **Documentation**: `/usr/share/autonomy/`
- **Configuration Examples**: `/etc/autonomy/`

## License

GPL-3.0-or-later

