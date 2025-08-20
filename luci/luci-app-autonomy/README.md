# LuCI App for autonomy Multi-Interface Failover

A comprehensive web-based management interface for the autonomy multi-interface failover system, providing real-time monitoring, configuration, and control capabilities.

## Features

### 📊 **Overview Dashboard**
- Real-time system status monitoring
- Current failover member display
- Daemon control (start/stop/restart/reload)
- System health indicators
- Auto-refresh capabilities

### 🔧 **Configuration Management**
- Complete UCI-based configuration interface
- Main system settings (poll intervals, decision intervals, etc.)
- Monitoring server configuration
- Adaptive sampling settings
- Multi-channel notification configuration
- Rule engine settings

### 📡 **Member Management**
- Real-time member status display
- Interface health monitoring
- Failover history tracking
- Performance metrics visualization
- Connection quality indicators

### 📈 **Telemetry & Analytics**
- Real-time data collection monitoring
- Performance trend analysis
- System metrics visualization
- Historical data review
- Export capabilities

### 📝 **Log Management**
- Real-time log viewing
- Log filtering and search
- Log level configuration
- Log rotation settings
- Error tracking and analysis

### 🔔 **Notification System**
- Multi-channel notification management
- Pushover, Email, Slack, Discord, Telegram, Webhook, SMS support
- Notification history and statistics
- Test notification functionality
- Priority-based delivery
- Rate limiting and deduplication

### ⚡ **Adaptive Sampling**
- Dynamic sampling rate monitoring
- Connection type-based optimization
- Performance-based adjustments
- Battery and data usage awareness
- Real-time adaptation tracking

### 🎯 **Rule Engine**
- Automated rule management
- Pre-configured rule templates
- Custom rule creation
- Execution history and statistics
- Rule performance monitoring
- Priority and cooldown management

## Installation

### Prerequisites
- OpenWrt/LEDE system with LuCI
- autonomy daemon installed and configured
- Required dependencies:
  - `luci-base`
  - `luci-compat`
  - `luci-theme-bootstrap`

### Installation Steps

1. **Build the package:**
   ```bash
   cd luci/
   ./build-luci.sh
   ```

2. **Install the package:**
   ```bash
   opkg install luci-app-autonomy_*.ipk
   ```

3. **Access the interface:**
   - Navigate to `System` → `Software` in LuCI
   - Install the package if not already installed
   - Access via `Network` → `autonomy Failover`

## Configuration

### Basic Setup

1. **Enable autonomy:**
   - Go to `Network` → `autonomy Failover` → `Configuration`
   - Check "Enable autonomy"
   - Configure basic settings (poll intervals, decision intervals)

2. **Configure Members:**
   - Add network interfaces as failover members
   - Set priorities and weights
   - Configure health checks

3. **Set up Notifications:**
   - Enable desired notification channels
   - Configure API keys and endpoints
   - Test notification delivery

4. **Configure Adaptive Sampling:**
   - Enable adaptive sampling
   - Set base intervals and thresholds
   - Configure connection type rules

5. **Set up Rule Engine:**
   - Enable rule engine
   - Use pre-configured templates
   - Create custom rules as needed

### Advanced Configuration

#### Notification Channels

**Pushover:**
- Enable Pushover notifications
- Enter your Pushover token and user key
- Optionally specify a device

**Email:**
- Configure SMTP settings
- Set up authentication
- Configure TLS/STARTTLS

**Slack:**
- Create a Slack webhook
- Configure channel and username
- Test integration

**Discord:**
- Create a Discord webhook
- Configure username and avatar
- Test message delivery

**Telegram:**
- Create a Telegram bot
- Get chat ID
- Configure bot token

**Webhook:**
- Configure webhook URL
- Set HTTP method and content type
- Add custom headers if needed

#### Adaptive Sampling

Configure dynamic sampling rates based on:
- Connection type (Starlink, Cellular, WiFi, LAN)
- Data usage thresholds
- Battery level
- Performance metrics

#### Rule Engine

Create automated rules for:
- Location-based actions (movement detection)
- Time-based maintenance
- Performance-based failover
- Emergency procedures

## Usage

### Dashboard Overview

The main dashboard provides:
- **System Status:** Current daemon state
- **Current Member:** Active failover interface
- **Member Count:** Total and active members
- **Control Buttons:** Start, stop, restart, reload

### Real-time Monitoring

All pages feature:
- **Auto-refresh:** Automatic data updates
- **Real-time data:** Live system information
- **Interactive controls:** Direct system control
- **Status indicators:** Visual health indicators

### Configuration Management

The configuration page provides:
- **UCI Integration:** Native OpenWrt configuration
- **Validation:** Input validation and error checking
- **Dependencies:** Conditional field visibility
- **Defaults:** Sensible default values

## API Integration

The LuCI interface integrates with autonomy's ubus API:

### Available Endpoints
- `autonomy status` - System status
- `autonomy members` - Member information
- `autonomy telemetry` - Telemetry data
- `autonomy notifications` - Notification management
- `autonomy adaptive_sampling` - Adaptive sampling status
- `autonomy rules` - Rule engine management

### AJAX Endpoints
- `/admin/network/autonomy/status` - Status data
- `/admin/network/autonomy/members_data` - Member data
- `/admin/network/autonomy/telemetry_data` - Telemetry data
- `/admin/network/autonomy/notifications_data` - Notification data
- `/admin/network/autonomy/adaptive_data` - Adaptive sampling data
- `/admin/network/autonomy/rules_data` - Rule engine data

## Troubleshooting

### Common Issues

1. **Interface not accessible:**
   - Check if LuCI app is installed
   - Verify autonomy daemon is running
   - Check file permissions

2. **Configuration not saving:**
   - Verify UCI permissions
   - Check configuration file syntax
   - Restart autonomy daemon

3. **Notifications not working:**
   - Verify API keys and endpoints
   - Check network connectivity
   - Review notification logs

4. **Data not updating:**
   - Check ubus connectivity
   - Verify daemon status
   - Review system logs

### Debug Mode

Enable debug logging:
1. Go to Configuration → Log Level
2. Set to "Debug"
3. Check `/var/log/autonomyd.log`

### Log Analysis

View logs in real-time:
1. Go to Logs page
2. Monitor for errors
3. Check system logs: `logread | grep autonomy`

## Development

### Building from Source

```bash
# Clone the repository
git clone https://github.com/your-repo/autonomy.git
cd autonomy/luci

# Build the package
./build-luci.sh

# Install
opkg install luci-app-autonomy_*.ipk
```

### Customization

The LuCI interface can be customized by:
- Modifying view templates in `htdocs/luci-static/`
- Adding new controller functions
- Extending configuration models
- Creating custom themes

### Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## License

This project is licensed under the GPL-3.0-or-later License - see the LICENSE file for details.

## Support

For support and questions:
- Check the documentation
- Review the troubleshooting section
- Open an issue on GitHub
- Contact the development team

## Changelog

### Version 2.0.0
- Added comprehensive notification management
- Implemented adaptive sampling interface
- Added rule engine management
- Enhanced configuration options
- Improved real-time monitoring
- Added multi-channel support

### Version 1.0.0
- Initial release
- Basic failover management
- Member monitoring
- Configuration interface
- Log viewing
