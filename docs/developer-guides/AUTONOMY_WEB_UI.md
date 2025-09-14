# Autonomy Web UI Documentation

## Overview

The Autonomy Web UI provides a comprehensive web-based interface for monitoring, configuring, and controlling the Autonomy autonomous networking system on RUTOS devices. Built using VuCI (Vue.js + OpenWrt Configuration Interface), it offers a modern, responsive interface accessible through any web browser.

## Features

### 🖥️ System Status Dashboard

- **Real-time Monitoring**: Live status of all system components
- **Health Indicators**: Color-coded status cards for quick assessment
- **System Metrics**: Uptime, version, and performance data
- **Auto-refresh**: Automatic status updates every 30 seconds

### 🛰️ Starlink Monitoring

- **Health Metrics**: Latency, obstruction percentage, SNR, uptime
- **Status Indicators**: Visual health status with color coding
- **Performance Tracking**: Historical performance data
- **Configuration**: Starlink API settings and thresholds

### 📍 GPS & Location Services

- **GPS Status**: Fix status, coordinates, altitude, satellite count
- **Location Services**: Google Location API, OpenCELLID integration
- **Data Submission**: Automatic OpenCELLID data contribution
- **Configuration**: API keys, submission settings, device settings

### 🌐 Network Management

- **Interface Status**: Active network interface monitoring
- **Failover Management**: Multi-WAN failover status
- **Performance Metrics**: Network performance indicators
- **Configuration**: Network interface settings

### ⚙️ Configuration Management

- **Core Settings**: System enable/disable, logging, polling intervals
- **Starlink Configuration**: Host, port, timeouts, health check settings
- **GPS Configuration**: Device settings, API keys, submission options
- **Real-time Updates**: Configuration changes applied immediately

### 📊 Log Management

- **Multi-log Support**: Separate logs for autonomy, GPS, OpenCELLID, health
- **Real-time Viewing**: Live log content with auto-refresh
- **Log Clearing**: Easy log management and cleanup
- **Tabbed Interface**: Organized log viewing by component

### 🔧 Control Actions

- **Manual Health Checks**: On-demand system health assessment
- **OpenCELLID Submission**: Manual data submission to OpenCELLID
- **Status Refresh**: Manual status updates
- **Service Control**: Start, stop, restart services

## Installation

### Prerequisites

- RUTOS device (RUTX50, RUTX11, etc.)
- Autonomy system installed and running
- Network access to the device

### Automated Installation

1. **Build and Deploy**:

   ```powershell
   .\scripts\build-autonomy-webui.ps1
   ```

2. **Manual Installation** (if automated fails):

   ```bash
   # Build packages using RUTOS SDK
   cd /path/to/rutos-sdk
   make package/vuci-app-autonomy-api/compile
   make package/vuci-app-autonomy-ui/compile
   
   # Install packages
   opkg install /path/to/vuci-app-autonomy-api*.ipk
   opkg install /path/to/vuci-app-autonomy-ui*.ipk
   
   # Start services
   /etc/init.d/autonomy-api enable
   /etc/init.d/autonomy-api start
   /etc/init.d/uhttpd restart
   ```

### Access

- **URL**: `http://[device-ip]/cgi-bin/luci/admin/autonomy`
- **Default**: `http://192.168.80.1/cgi-bin/luci/admin/autonomy`

## Configuration

### Core Configuration

- **Enable/Disable**: Toggle the autonomy system
- **Log Level**: Set logging verbosity (debug, info, warn, error)
- **Poll Interval**: Health check frequency in milliseconds
- **History Window**: Time window for historical data
- **Predictive Failover**: Enable trend-based failover

### Starlink Configuration

- **Host**: Starlink dish IP address (default: 192.168.100.1)
- **Port**: API port (default: 9200)
- **Timeout**: Request timeout in seconds
- **Health Check Interval**: Starlink health check frequency
- **Thresholds**: Obstruction and latency warning thresholds

### GPS Configuration

- **Enable GPS**: Toggle GPS functionality
- **API Keys**: Google Location API and OpenCELLID API keys
- **Device Settings**: GPS device path and baud rate
- **Location Services**: Configure available location services
- **Data Submission**: OpenCELLID submission settings

## API Endpoints

The web UI communicates with the autonomy system through REST API endpoints:

### Status Endpoints

- `GET /api/autonomy/status` - System status
- `GET /api/autonomy/starlink` - Starlink status
- `GET /api/autonomy/gps` - GPS status
- `GET /api/autonomy/network` - Network status

### Control Endpoints

- `POST /api/autonomy/health-check` - Run health check
- `POST /api/autonomy/opencellid/submit` - Submit to OpenCELLID

### Configuration Endpoints

- `GET /api/autonomy/config/{section}` - Get configuration
- `POST /api/autonomy/config/{section}` - Update configuration

### Log Endpoints

- `GET /api/autonomy/logs/{type}` - Get log content
- `POST /api/autonomy/logs/clear` - Clear logs

## Troubleshooting

### Common Issues

1. **Web UI Not Loading**
   - Check if uhttpd is running: `/etc/init.d/uhttpd status`
   - Restart uhttpd: `/etc/init.d/uhttpd restart`
   - Check firewall settings

2. **API Not Responding**
   - Check if autonomy-api service is running: `/etc/init.d/autonomy-api status`
   - Restart API service: `/etc/init.d/autonomy-api restart`
   - Check logs: `tail -f /var/log/autonomy-api/autonomy-api.log`

3. **Configuration Not Saving**
   - Check UCI permissions
   - Verify configuration syntax
   - Check autonomy daemon status

4. **Status Not Updating**
   - Check autonomy daemon is running: `pgrep autonomyd`
   - Verify ubus connectivity: `ubus list`
   - Check autonomyctl: `/usr/local/bin/autonomyctl status`

### Log Files

- **API Logs**: `/var/log/autonomy-api/autonomy-api.log`
- **Autonomy Logs**: `/var/log/autonomy/autonomy.log`
- **GPS Logs**: `/var/log/autonomy/gps.log`
- **OpenCELLID Logs**: `/var/log/autonomy/opencellid.log`
- **Health Logs**: `/var/log/autonomy/health.log`

### Debug Mode

Enable debug logging in the web UI configuration to get detailed information about API calls and system interactions.

## Security

### Authentication

- The web UI uses RUTOS authentication system
- Access requires valid user credentials
- API endpoints respect user permissions

### API Security

- All API calls are validated
- Input sanitization prevents injection attacks
- Rate limiting prevents abuse

### Configuration Security

- API keys are stored securely
- Sensitive data is masked in the UI
- Configuration changes are logged

## Development

### Architecture

- **Frontend**: Vue.js with VuCI components
- **Backend**: Lua API with UCI integration
- **Communication**: REST API over HTTP
- **Configuration**: UCI-based configuration management

### Customization

The web UI can be customized by modifying:

- Vue.js components in `vuci-app-autonomy-ui/src/src/views/`
- API endpoints in `vuci-app-autonomy-api/src/autonomy-api.lua`
- Configuration schemas in UCI files

### Building from Source

```bash
# Clone the repository
git clone [repository-url]
cd autonomy

# Build web UI packages
./scripts/build-autonomy-webui.ps1

# Deploy to device
./scripts/deploy-autonomy-webui.ps1
```

## Support

For issues and questions:

1. Check the troubleshooting section
2. Review log files for error messages
3. Verify configuration settings
4. Test API endpoints manually
5. Check system requirements and dependencies

## Changelog

### Version 1.0.0

- Initial release
- Complete monitoring dashboard
- Configuration management
- Log viewing and management
- Real-time status updates
- Starlink and GPS integration
- OpenCELLID data submission
