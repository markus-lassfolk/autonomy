# 🤖 Autonomy Enhanced Web Interface

> **Comprehensive dashboard for the Telia Autonomy Network Management System**

## Overview

The Autonomy Enhanced Web Interface provides a modern, responsive dashboard for monitoring and controlling the Autonomy Network Management Daemon. It offers real-time insights into network performance, ML predictions, system health, and comprehensive configuration management.

## 🚀 Features

### 📊 Real-Time Monitoring

- **Live Network Status**: Monitor all network interfaces in real-time
- **Performance Metrics**: Track latency, throughput, and health scores
- **System Health**: CPU, memory, and service monitoring
- **Interactive Charts**: Visualize trends and performance data

### 🛰️ Starlink Integration

- **Obstruction Monitoring**: Real-time obstruction level tracking
- **Satellite Status**: Active satellite count and signal quality
- **Data Usage**: Bandwidth consumption and trends
- **Performance Analytics**: SNR, latency, and throughput metrics

### 🧠 ML Intelligence Dashboard

- **Prediction Accuracy**: Real-time ML model performance
- **Interface Scoring**: Health scores for all network interfaces
- **Learning Analytics**: Observation counts and learning rates
- **Phase Monitoring**: Track ML system phases and capabilities

### 🛰️ GPS & Location Services

- **Multi-Source GPS**: Monitor all GPS sources and accuracy
- **Location Tracking**: Real-time position and movement detection
- **Health Monitoring**: GPS system status and quality metrics

### ⚙️ Configuration Management

- **Live Configuration**: Modify daemon settings in real-time
- **Validation**: Input validation and error handling
- **Backup/Restore**: Configuration backup and restore capabilities
- **Default Reset**: Quick reset to default settings

### 📋 System Administration

- **Service Control**: Start, stop, restart daemon services
- **Log Management**: View, download, and clear system logs
- **Health Checks**: Comprehensive system health monitoring
- **Maintenance Tools**: System maintenance and optimization

## 🎨 User Interface

### Modern Design

- **Responsive Layout**: Works on desktop, tablet, and mobile devices
- **Glass Morphism**: Modern glassmorphism design with backdrop blur
- **Smooth Animations**: Fluid transitions and hover effects
- **Dark/Light Theme**: Adaptive color scheme

### Navigation

- **Tabbed Interface**: Organized into logical sections
- **Quick Access**: Fast navigation between different views
- **Breadcrumbs**: Clear navigation hierarchy
- **Search**: Quick find functionality

### Data Visualization

- **Interactive Charts**: Chart.js powered visualizations
- **Real-Time Updates**: Live data updates every 5 seconds
- **Export Capabilities**: Download data and logs
- **Customizable Views**: Configurable dashboard layouts

## 🔧 Technical Architecture

### Frontend Technologies

- **HTML5**: Semantic markup and modern web standards
- **CSS3**: Advanced styling with flexbox and grid layouts
- **JavaScript ES6+**: Modern JavaScript with async/await
- **Chart.js**: Interactive data visualization
- **Fetch API**: Modern HTTP client for API calls

### Backend Integration

- **UBUS API**: Direct communication with the daemon
- **RESTful Design**: Clean API interface
- **Real-Time Updates**: WebSocket-like functionality via polling
- **Error Handling**: Comprehensive error management

### Build Integration

- **SDK Toolchain**: Integrated with RUTOS SDK build process
- **Minification**: Optimized JavaScript and CSS
- **Compression**: Gzipped assets for faster loading
- **IPK Packaging**: Included in the daemon installation package

## 📱 Responsive Design

### Desktop (1200px+)

- **Multi-Column Layout**: Optimal use of screen real estate
- **Sidebar Navigation**: Persistent navigation panel
- **Large Charts**: Full-size data visualizations
- **Hover Effects**: Rich interactive feedback

### Tablet (768px - 1199px)

- **Adaptive Grid**: Responsive grid system
- **Touch-Friendly**: Optimized for touch interaction
- **Collapsible Panels**: Space-efficient design
- **Swipe Navigation**: Touch gesture support

### Mobile (< 768px)

- **Single Column**: Stacked layout for small screens
- **Touch Targets**: Large, accessible buttons
- **Simplified Navigation**: Streamlined menu system
- **Optimized Charts**: Mobile-friendly visualizations

## 🔌 API Integration

### UBUS Methods Used

#### Core Daemon Methods

- `autonomy.status` - Daemon status and uptime
- `autonomy.health` - System health check
- `autonomy.config` - Configuration management
- `autonomy.start/stop/restart` - Service control

#### Network Methods

- `network_status` - Network interface status
- `network_interfaces` - Interface details
- `network_health_check` - Network health metrics
- `network_failover` - Failover status

#### Starlink Methods

- `starlink_status` - Starlink connection status
- `starlink_health` - Health metrics
- `starlink_location` - Location data
- `starlink_collector_stats` - Statistics
- `starlink_force_collect` - Force data collection

#### ML Monitoring Methods

- `ml_monitor.status` - ML system status
- `ml_monitor.start/stop/restart` - ML control
- `ml_monitor.get_config/set_config` - Configuration
- `ml_monitor.get_predictions` - Prediction data
- `ml_monitor.get_statistics` - Performance stats
- `ml_monitor.get_analytics_summary` - Analytics data

#### GPS Methods

- `gps_status` - GPS system status
- `gps_sources` - Active GPS sources
- `gps_health_check` - GPS health metrics

#### System Methods

- `system_status` - System status
- `system_health_check` - Health metrics
- `system_health_details` - Detailed health info
- `system_maintenance` - Maintenance tools

### Error Handling

- **Network Errors**: Graceful handling of connection issues
- **API Errors**: User-friendly error messages
- **Timeout Handling**: Automatic retry mechanisms
- **Fallback Data**: Mock data for development

## 🚀 Installation & Setup

### Automatic Installation

The UI is automatically installed with the Autonomy Complete System:

```bash
# Install the complete system
opkg install tlt-autonomy-complete_*.ipk

# Run setup
/usr/bin/autonomy-setup
```

### Manual Installation

If installing separately:

```bash
# Install daemon first
opkg install tlt-autonomy-daemon_*.ipk

# Install UI
opkg install tlt-autonomy-ui_*.ipk

# Configure web server
/etc/init.d/autonomy-ui
```

### Access Methods

- **Direct URL**: `http://[router-ip]/autonomy`
- **Short URL**: `http://[router-ip]/autonomy.html`
- **VUCI Menu**: Services > Autonomy

## 🔧 Configuration

### Web Server Configuration

The UI automatically configures the web server:

```bash
# Enable UBUS support
uci set uhttpd.main.ubus_prefix="/ubus"
uci set uhttpd.main.ubus_timeout=30

# Add Autonomy UI location
uci add_list uhttpd.main.location="/autonomy"
uci add_list uhttpd.main.location="/autonomy/"

# Set document root
uci set uhttpd.main.home="/www/autonomy"

# Apply changes
uci commit uhttpd
/etc/init.d/uhttpd restart
```

### VUCI Integration

The UI integrates with VUCI (Vue.js based OpenWrt Configuration Interface):

```json
{
  "services/autonomy": {
    "title": "Autonomy",
    "index": 50,
    "view": "services/Autonomy",
    "acls": ["services/autonomy"]
  }
}
```

## 🛠️ Development

### Local Development

For local development without the daemon:

```bash
# Serve the UI locally
python3 -m http.server 8080

# Access at http://localhost:8080
```

### Mock Data

The UI includes mock data for development:

```javascript
// Enable mock mode
window.MOCK_MODE = true;

// Mock data will be used instead of UBUS calls
```

### Building

The UI is built as part of the complete system:

```bash
# Build complete system
./build/scripts/build-autonomy-complete.sh

# Or build UI separately
./build/scripts/build-autonomy-ui.sh
```

## 📊 Performance

### Optimization Features

- **Lazy Loading**: Load data only when needed
- **Caching**: Client-side caching of static data
- **Compression**: Gzipped assets
- **Minification**: Minified JavaScript and CSS
- **CDN**: External CDN for Chart.js and date-fns

### Performance Metrics

- **Load Time**: < 2 seconds on 3G
- **Update Frequency**: 5-second intervals
- **Memory Usage**: < 50MB
- **CPU Usage**: < 5% during normal operation

## 🔒 Security

### Security Features

- **Input Validation**: All inputs are validated
- **XSS Protection**: Content Security Policy headers
- **CSRF Protection**: Token-based protection
- **Access Control**: VUCI ACL integration

### Best Practices

- **HTTPS**: Use HTTPS in production
- **Authentication**: Integrate with router authentication
- **Logging**: Comprehensive audit logging
- **Updates**: Regular security updates

## 🐛 Troubleshooting

### Common Issues

#### UI Not Loading

```bash
# Check web server status
/etc/init.d/uhttpd status

# Restart web server
/etc/init.d/uhttpd restart

# Check configuration
uci show uhttpd
```

#### UBUS Connection Issues

```bash
# Check daemon status
/etc/init.d/autonomy status

# Check UBUS connectivity
ubus call autonomy status

# Restart daemon
/etc/init.d/autonomy restart
```

#### VUCI Integration Issues

```bash
# Check VUCI menu
ls -la /usr/share/vuci/menu.d/

# Restart VUCI
/etc/init.d/uhttpd restart
```

### Debug Mode

Enable debug mode for troubleshooting:

```bash
# Enable debug logging
uci set autonomy.general.debug_mode=1
uci commit autonomy
/etc/init.d/autonomy restart
```

## 📈 Future Enhancements

### Planned Features

- **Real-Time Notifications**: WebSocket-based live updates
- **Custom Dashboards**: User-configurable layouts
- **Advanced Analytics**: Historical data analysis
- **Mobile App**: Native mobile application
- **API Documentation**: Interactive API docs
- **Plugin System**: Extensible architecture

### Roadmap

- **Q1 2024**: Real-time notifications
- **Q2 2024**: Custom dashboards
- **Q3 2024**: Advanced analytics
- **Q4 2024**: Mobile app

## 📞 Support

### Documentation

- **User Guide**: [docs/user-guides/autonomy-ui.md](../../docs/user-guides/autonomy-ui.md)
- **API Reference**: [docs/api-reference/ui-api.md](../../docs/api-reference/ui-api.md)
- **Developer Guide**: [docs/developer-guides/ui-development.md](../../docs/developer-guides/ui-development.md)

### Community

- **GitHub Issues**: [github.com/markus-lassfolk/autonomy/issues](https://github.com/markus-lassfolk/autonomy/issues)
- **Discussions**: [github.com/markus-lassfolk/autonomy/discussions](https://github.com/markus-lassfolk/autonomy/discussions)

### Professional Support

- **Email**: [support@telia.com](mailto:support@telia.com)
- **Documentation**: [docs.telia.com/autonomy](https://docs.telia.com/autonomy)

---

### Built with ❤️ for the Telia Autonomy Network Management System
