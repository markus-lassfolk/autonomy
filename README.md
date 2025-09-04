# 🤖 Autonomy - Intelligent Multi-Interface Failover System

> **Production-ready autonomous network failover with Starlink tracking and predictive analytics**

## Overview

Autonomy is a comprehensive network management system designed for **RutOS** routers that provides intelligent multi-interface failover with advanced predictive capabilities. The system combines real-time monitoring, machine learning-based predictions, and seamless integration with Starlink, cellular, Wi-Fi, and LAN connections.

> **⚠️ OpenWrt Compatibility**: While built on OpenWrt foundations, Autonomy is specifically designed for RUTOS and relies on RUTOS-specific components. OpenWrt support is limited and experimental. See [OpenWrt Support Status](docs/user-guides/openwrt-support-status.md) for details.

## 🚀 Key Features

### Core Functionality
- **Intelligent Auto-Discovery** of network interfaces and mwan3 members
- **Multi-Interface Support**: Starlink, Cellular (multi-SIM), Wi-Fi STA/tethering, LAN
- **Predictive Failover** with health scoring and trend analysis
- **Native Integration** with UCI, ubus, procd, and mwan3

### Advanced Monitoring
- **Starlink Integration**: Native gRPC client with obstruction prediction
- **Cellular Intelligence**: RSRP, RSRQ, SINR monitoring with OpenCellID geolocation
- **Wi-Fi Optimization**: Channel analysis and interference detection
- **GPS Integration**: Multi-source GPS with movement detection

### System Reliability
- **Watchdog System**: Independent health monitoring and recovery
- **Self-Healing**: Automatic recovery from failures
- **Security Auditing**: Threat detection and prevention
- **Performance Monitoring**: Resource usage optimization

## 📁 Repository Structure

```
├── src/                          # Source code
│   ├── c/                        # C components
│   │   ├── starlink-tracking/    # Starlink tracking modules
│   │   ├── autonomy-daemon/      # Main autonomy daemon
│   │   ├── visualization-server/ # Web visualization server
│   │   └── shared/              # Shared libraries and headers
│   ├── web/                     # Web UI components
│   │   ├── autonomy-ui/         # Main autonomy web interface
│   │   ├── starlink-viz/        # Starlink visualization
│   │   └── examples/            # VUCI examples
│   └── api/                     # API components
│       ├── autonomy-api/        # Autonomy API (Lua/RPCD)
│       └── shared/              # Shared API utilities
├── build/                       # Build system
│   ├── scripts/                 # Build and deployment scripts
│   ├── configs/                 # Build configurations
│   └── packages/                # Generated packages
├── docs/                        # Documentation
│   ├── user-guides/             # User documentation
│   ├── tutorials/               # Setup and usage guides
│   ├── api-reference/           # API documentation
│   ├── developer-guides/        # Developer documentation
│   ├── deployment/              # Deployment guides
│   └── architecture/            # System architecture docs
├── tools/                       # Development tools
└── archive/                     # Archived/legacy files
```

## 🛠️ Quick Start

### Prerequisites
- **RutOS 7.17.1+** (RUTX series recommended)
- ARM Cortex-A7 architecture (RUTX50, RUTX11, etc.)
- Internet connection for satellite data

**⚠️ OpenWrt Compatibility**: While built on OpenWrt foundations, Autonomy is specifically designed for RutOS and relies on RutOS-specific components. OpenWrt support is limited and experimental. See [OpenWrt Support Status](docs/user-guides/openwrt-support-status.md) for details.


### Installation
```bash
# Install dependencies
opkg update
opkg install libcurl libjson-c

# Install autonomy package
opkg install autonomy_*.ipk

# Start the service
/etc/init.d/autonomy start
/etc/init.d/autonomy enable
```

### Basic Usage
```bash
# Check system status
ubus call autonomy status

# View network interfaces
ubus call autonomy interfaces

# Get Starlink predictions
ubus call starlink_tracker predictions

# Manual failover
ubus call autonomy switch '{"interface": "cellular"}'
```

## 📖 Documentation

### For Users
- [Getting Started](docs/tutorials/getting-started.md) - Quick start guide
- [Quick Setup](docs/tutorials/quick-setup.md) - 5-minute setup reference  
- [Configuration Guide](docs/tutorials/configuration-guide.md) - Complete configuration
- [API Integrations](docs/tutorials/api-integrations-guide.md) - External API setup
- [User Guide](docs/tutorials/USER_GUIDE.md) - Complete user manual

#### Feature Guides
- [Starlink Tracking](docs/user-guides/starlink-tracking-overview.md) - Satellite tracking and prediction
- [WiFi Optimization](docs/user-guides/wifi-optimization-guide.md) - WiFi channel optimization
- [GPS System](docs/user-guides/gps-system-guide.md) - Multi-source GPS and location
- [Cellular Monitoring](docs/user-guides/cellular-monitoring-guide.md) - Cellular signal optimization
- [Predictive Failover](docs/user-guides/predictive-failover-guide.md) - Understanding predictions
- [Security Features](docs/user-guides/security-features-guide.md) - Security monitoring
- [Troubleshooting](docs/user-guides/troubleshooting-guide.md) - Common issues and solutions

### For Developers
- [SDK Master Guide](docs/tutorials/sdk-master-guide.md)
- [Configuration Reference](docs/api-reference/configuration-reference.md) - Complete UCI settings
- [API Reference](docs/api-reference/)
- [Architecture Documentation](docs/architecture/)
- [Development Setup](docs/developer-guides/DEVELOPMENT.md)

### For Deployment
- [Production Deployment](docs/deployment/)
- [RUTX50 Production Guide](docs/tutorials/rutx50-production-guide.md)
- [Configuration Guide](docs/developer-guides/CONFIGURATION.md)

## 🏗️ Architecture

The system consists of three main components:

1. **C Core**: High-performance monitoring and prediction engine
2. **Web UI**: Vue.js-based management interface (VUCI)
3. **API Layer**: Lua-based RPCD integration for system communication

## 🤝 Contributing

See [Contributing Guide](docs/developer-guides/CONTRIBUTING.md) for development setup and guidelines.

## 📄 License

GPL v2 compatible with RutOS/OpenWrt ecosystem.

---

**Status**: ✅ **Production Ready** - Comprehensive testing completed on RUTX50 platform.
