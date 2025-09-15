# Autonomy IPK Package System

## Overview

This document describes the comprehensive IPK package system for the Autonomy project, designed for deployment on Teltonika RUTOS devices. The system creates three main packages that can be installed independently or together.

## Package Architecture

### 1. Main Autonomy Package (`tlt-autonomy-daemon`)
- **Purpose**: Core autonomy daemon with integrated VUCI web interface
- **Components**:
  - Autonomy daemon binary (`/usr/bin/autonomy-daemon`)
  - VUCI web interface files (`/www/autonomy/`)
  - VUCI menu integration (`/usr/share/vuci/menu.d/autonomy.json`)
  - Init script (`/etc/init.d/autonomy-daemon`)
  - Configuration files (`/etc/config/autonomy`)
  - Data directories (`/var/lib/autonomy`, `/var/log/autonomy`)

### 2. Starlink Standalone Package (`tlt-starlink-standalone`)
- **Purpose**: Independent Starlink satellite tracking client
- **Components**:
  - Starlink gRPC client binary (`/usr/bin/starlink-grpc-client`)
  - Init script (`/etc/init.d/starlink-standalone`)
  - Configuration files (`/etc/config/starlink-standalone`)
  - Example configuration (`/etc/starlink/starlink-example.conf`)
  - Data directories (`/var/lib/starlink`, `/var/log/starlink`)

### 3. VUCI UI Package (`vuci-app-autonomy-ui`)
- **Purpose**: Dedicated VUCI web interface package
- **⚠️ WARNING**: VUCI UI packages are extremely fragile and often break the web interface login system
- **Components**:
  - Web assets (`/www/autonomy/`)
  - VUCI menu entries (`/usr/share/vuci/menu.d/autonomy-ui.json`)
  - VUCI view files (`/usr/share/vuci/views/`)
  - Static assets (CSS, JS, images)
- **Known Issues**: 
  - Menu entries may not appear in Package Manager
  - Can break device login functionality
  - Requires manual file placement and cleanup

## Package Dependencies

```
tlt-autonomy-daemon
├── cjson
├── libubus
├── libubox
├── libjson-c
├── libuci
├── libcurl
├── libsqlite3
├── libssl
└── libcrypto

tlt-starlink-standalone
├── libcurl
├── libjson-c
├── libubox
├── libubus
└── libuci

vuci-app-autonomy-ui
└── vuci-ui-core
```

## Build System Integration

### Directory Structure
```
/mnt/s/autonomy/
├── package/feeds/autonomy/
│   ├── tlt-autonomy-daemon/
│   │   ├── Makefile
│   │   └── files/
│   │       ├── autonomy-daemon.init
│   │       ├── autonomy.config
│   │       └── autonomy.json
│   ├── tlt-starlink-standalone/
│   │   ├── Makefile
│   │   └── files/
│   │       ├── starlink-standalone.init
│   │       ├── starlink-standalone.config
│   │       └── starlink-example.conf
│   └── vuci-app-autonomy-ui/
│       ├── Makefile
│       └── files/
│           └── autonomy-ui.json
└── VERSION
```

### Feeds Configuration
The `feeds.conf` file in the RUTOS SDK is configured to include the autonomy repository:
```
src-link autonomy /mnt/s/autonomy
```

## Build Process

### 1. Build Script
Use the dedicated IPK build script:
```bash
/mnt/wsl/SDK/build_autonomy_ipk_packages.sh
```

This script:
- Increments version numbers
- Verifies package structure
- Updates and installs feeds
- Cleans previous builds
- Builds dependencies
- Compiles packages in dependency order
- Creates IPK packages
- Provides detailed logging

### 2. Build Order
1. **Dependencies**: cJSON library and other required libraries
2. **Main Autonomy Package**: Built independently (no VUCI UI dependency)
3. **Starlink Standalone Package**: Built independently
4. **VUCI UI Package**: Built separately (optional, fragile)

### 3. Output Location
Built IPK packages are located in:
```
/mnt/wsl/SDK/rutos-ipq40xx-rutx-sdk/bin/packages/arm_cortex-a7_neon-vfpv4/autonomy/
```

## Deployment Process

### 1. Deployment Script
Use the deployment script:
```bash
/mnt/wsl/SDK/deploy_autonomy_ipk_packages.sh [device_ip] [ssh_key] [ssh_user]
```

Default parameters:
- Device IP: `192.168.80.1`
- SSH Key: `~/.ssh/rutos_key`
- SSH User: `root`

### 2. Deployment Steps
1. **Connectivity Check**: Verify device reachability and SSH access
2. **Package Location**: Find built IPK packages
3. **Backup**: Backup existing installation
4. **Service Stop**: Stop existing services
5. **Package Copy**: Copy IPK packages to device
6. **Installation**: Install packages using `opkg`
7. **Service Start**: Start and enable services
8. **Verification**: Verify installation and service status
9. **Cleanup**: Remove temporary files

### 3. Manual Installation
If you prefer manual installation:
```bash
# Copy packages to device
scp -i ~/.ssh/rutos_key *.ipk root@192.168.80.1:/tmp/

# Install packages (in dependency order)
ssh -i ~/.ssh/rutos_key root@192.168.80.1 "opkg install /tmp/tlt-autonomy-daemon_*.ipk"
ssh -i ~/.ssh/rutos_key root@192.168.80.1 "opkg install /tmp/tlt-starlink-standalone_*.ipk"
# VUCI UI package is optional and fragile - install with caution
ssh -i ~/.ssh/rutos_key root@192.168.80.1 "opkg install /tmp/vuci-app-autonomy-ui_*.ipk"

# Start services
ssh -i ~/.ssh/rutos_key root@192.168.80.1 "/etc/init.d/autonomy-daemon enable && /etc/init.d/autonomy-daemon start"
ssh -i ~/.ssh/rutos_key root@192.168.80.1 "/etc/init.d/starlink-standalone enable && /etc/init.d/starlink-standalone start"
```

## Configuration

### 1. Autonomy Daemon Configuration
Configuration file: `/etc/config/autonomy`
```bash
config autonomy 'main'
    option enabled '1'
    option log_level 'info'
    option data_dir '/var/lib/autonomy'
    option log_dir '/var/log/autonomy'

config ml_monitor 'main'
    option enabled '1'
    option collection_interval '60'
    option prediction_interval '300'

config network_monitor 'main'
    option enabled '1'
    option interface_check_interval '30'
    option connection_timeout '10'

config gps_tracker 'main'
    option enabled '1'
    option update_interval '5'
    option accuracy_threshold '10'

config analytics 'main'
    option enabled '1'
    option aggregation_interval '300'
    option report_interval '3600'

config starlink 'main'
    option enabled '0'
    option grpc_endpoint '192.168.100.1:9200'
    option update_interval '10'
    option prediction_horizon '3600'
```

### 2. Starlink Standalone Configuration
Configuration file: `/etc/config/starlink-standalone`
```bash
config starlink 'main'
    option enabled '1'
    option grpc_endpoint '192.168.100.1:9200'
    option update_interval '10'
    option prediction_horizon '3600'
    option log_level 'info'

config tracking 'main'
    option enabled '1'
    option satellite_filter 'active'
    option elevation_threshold '10'
    option azimuth_range '0,360'

config prediction 'main'
    option enabled '1'
    option obstruction_threshold '0.1'
    option outage_threshold '0.05'
    option weather_correction '1'
```

## VUCI Integration

### 1. Menu Structure
The VUCI menu is configured in `/usr/share/vuci/menu.d/autonomy.json`:
```json
{
  "services/autonomy": {
    "title": "Autonomy System",
    "index": 100,
    "view": "services/Autonomy",
    "acls": ["services/autonomy"]
  },
  "services/autonomy/dashboard": {
    "title": "Dashboard",
    "index": 10,
    "view": "services/Autonomy/Dashboard",
    "acls": ["services/autonomy"]
  },
  "services/autonomy/ml-analytics": {
    "title": "ML Analytics",
    "index": 20,
    "view": "services/Autonomy/MLAnalytics",
    "acls": ["services/autonomy"]
  }
}
```

### 2. Web Interface Access
⚠️ **WARNING**: VUCI UI integration is fragile and may not work properly.

If VUCI UI package is installed and working:
- **URL**: `http://[device_ip]`
- **Navigation**: Services → Autonomy System
- **Dashboard**: Real-time system monitoring
- **ML Analytics**: Machine learning insights
- **Network Monitor**: Network interface status
- **GPS Tracker**: Location and tracking data
- **Starlink Integration**: Satellite tracking and prediction
- **Settings**: System configuration

**Note**: If VUCI UI breaks the web interface, uninstall it and use command-line tools instead.

## Service Management

### 1. Autonomy Daemon Service
```bash
# Start service
/etc/init.d/autonomy-daemon start

# Stop service
/etc/init.d/autonomy-daemon stop

# Restart service
/etc/init.d/autonomy-daemon restart

# Enable auto-start
/etc/init.d/autonomy-daemon enable

# Disable auto-start
/etc/init.d/autonomy-daemon disable

# Check status
/etc/init.d/autonomy-daemon status
```

### 2. Starlink Standalone Service
```bash
# Start service
/etc/init.d/starlink-standalone start

# Stop service
/etc/init.d/starlink-standalone stop

# Restart service
/etc/init.d/starlink-standalone restart

# Enable auto-start
/etc/init.d/starlink-standalone enable

# Disable auto-start
/etc/init.d/starlink-standalone disable

# Check status
/etc/init.d/starlink-standalone status
```

## Logging and Monitoring

### 1. Log Files
- **Autonomy Daemon**: `/var/log/autonomy/daemon.log`
- **Starlink Standalone**: `/var/log/starlink/standalone.log`
- **System Logs**: `/var/log/messages`

### 2. Log Monitoring
```bash
# Monitor autonomy daemon logs
tail -f /var/log/autonomy/daemon.log

# Monitor starlink standalone logs
tail -f /var/log/starlink/standalone.log

# Monitor system logs
tail -f /var/log/messages | grep autonomy
```

### 3. Process Monitoring
```bash
# Check if autonomy daemon is running
pgrep -f autonomy-daemon

# Check if starlink standalone is running
pgrep -f starlink-grpc-client

# Check process details
ps aux | grep autonomy
```

## Troubleshooting

### 1. Build Issues
- **Missing dependencies**: Ensure all required libraries are built first
- **Version conflicts**: Check version numbers in VERSION file
- **Permission issues**: Ensure proper file permissions on source files
- **SDK issues**: Verify RUTOS SDK is properly configured

### 2. Installation Issues
- **Package conflicts**: Check for existing installations
- **Dependency issues**: Install packages in correct order
- **Space issues**: Ensure sufficient disk space on device
- **Permission issues**: Verify SSH key and user permissions

### 3. Runtime Issues
- **Service not starting**: Check logs and configuration
- **Web interface not accessible**: Verify VUCI integration
- **Missing binaries**: Check package installation
- **Configuration errors**: Validate configuration files

### 4. Common Commands
```bash
# Check package installation
opkg list-installed | grep autonomy

# Check package files
opkg files tlt-autonomy-daemon

# Reinstall package
opkg install --force-reinstall tlt-autonomy-daemon

# Remove package
opkg remove tlt-autonomy-daemon

# Check service status
/etc/init.d/autonomy-daemon status

# View service logs
logread | grep autonomy
```

## Version Management

### 1. Version Files
- **Main Version**: `/mnt/s/autonomy/VERSION`
- **Header Version**: `/mnt/s/autonomy/src/c/autonomy-daemon/core/version.h`
- **UBUS Version**: `/mnt/s/autonomy/src/c/autonomy-daemon/ubus/ubus_methods.c`

### 2. Version Format
```
AUTONOMY_VERSION=5.8.4
AUTONOMY_VERSION_BUILD=123
AUTONOMY_VERSION_FULL=5.8.4-123
AUTONOMY_DAEMON_VERSION=5.8.4-123
```

### 3. Version Updates
The build script automatically increments the build number and updates all version files to maintain consistency across the system.

## Security Considerations

### 1. File Permissions
- **Binaries**: 755 (executable by all users)
- **Configuration**: 644 (readable by all users)
- **Logs**: 644 (readable by all users)
- **Data**: 755 (accessible by all users)

### 2. Service Security
- **User**: Services run as root (required for system access)
- **Network**: Services bind to localhost by default
- **Authentication**: VUCI handles web interface authentication

### 3. Data Protection
- **Logs**: Rotate automatically to prevent disk space issues
- **Configuration**: Backup before major changes
- **Data**: Regular backups recommended

## Performance Optimization

### 1. Build Optimization
- **Parallel builds**: Use multiple CPU cores
- **Cache management**: Clean caches between builds
- **Dependency optimization**: Build only required dependencies

### 2. Runtime Optimization
- **Resource usage**: Monitor CPU and memory usage
- **Log rotation**: Configure appropriate log rotation
- **Data cleanup**: Regular cleanup of old data files

### 3. Network Optimization
- **Connection pooling**: Reuse connections where possible
- **Timeout configuration**: Optimize timeout values
- **Retry logic**: Implement appropriate retry mechanisms

## Future Enhancements

### 1. Package Improvements
- **Modular packages**: Split into more granular packages
- **Optional dependencies**: Make some dependencies optional
- **Configuration templates**: Provide more configuration examples

### 2. Build System Enhancements
- **Automated testing**: Add package testing
- **Quality checks**: Implement quality gates
- **Documentation generation**: Auto-generate documentation

### 3. Deployment Improvements
- **Rollback capability**: Add rollback functionality
- **Health checks**: Implement comprehensive health checks
- **Monitoring integration**: Add monitoring system integration

## Conclusion

The Autonomy IPK package system provides a comprehensive solution for deploying the autonomy daemon, VUCI web interface, and Starlink standalone client on Teltonika RUTOS devices. The system is designed for reliability, maintainability, and ease of use, with proper dependency management, service integration, and monitoring capabilities.

For support or questions, refer to the project documentation or contact the development team.


