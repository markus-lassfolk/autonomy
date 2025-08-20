# autonomy - Intelligent Multi-Interface Failover System

A production-ready, autonomous, and resource-efficient multi-interface failover daemon for RutOS and OpenWrt routers with advanced predictive capabilities and comprehensive monitoring.

## Overview

autonomy is a Go-based daemon that provides intelligent multi-interface failover management for Starlink, cellular (multi-SIM), Wi-Fi STA/tethering, and LAN uplinks. It uses predictive behavior and machine learning to ensure users never experience network degradation or outages.

## 🚀 Key Features

### Core Functionality
- **Intelligent Auto-Discovery** of mwan3 members and underlying netifd interfaces
- **Multi-Class Support**: Starlink, Cellular (multi-SIM), Wi-Fi, LAN with specialized metrics
- **Predictive Failover** based on health scoring, trend analysis, and pattern recognition
- **Native Integration** with UCI, ubus, procd, and mwan3
- **Resource-Efficient**: minimal CPU wakeups, RAM caps, low traffic on metered links

### Advanced Monitoring & Analytics
- **Starlink API Integration**: Native gRPC client for real-time Starlink metrics
- **Cellular Intelligence**: RSRP, RSRQ, SINR monitoring with production-grade OpenCellID geolocation
- **Wi-Fi Optimization**: Channel analysis, RSSI-weighted scoring, 5-star rating system
- **GPS Integration**: Multi-source GPS data collection and movement detection
- **Obstruction Monitoring**: Predictive obstruction detection and management

### System Reliability
- **Watchdog & Failsafe System**: Independent process for daemon health monitoring
- **Self-Healing**: Automatic recovery from failures and performance degradation
- **Performance Profiling**: CPU/memory usage calculation and GC tuning
- **Security Auditing**: Threat detection (brute force, port scanning, DoS/DDoS)

### Observability & Notifications
- **Structured Logging**: JSON-formatted logs with comprehensive observability
- **Multi-Channel Alerts**: Pushover, Email, Slack, Discord, Telegram, Webhook, SMS
- **Telemetry Store**: RAM-based ring buffers for short-term metric storage
- **MQTT Integration**: Real-time telemetry publishing
- **Comprehensive Metrics**: Health scores, performance data, and system analytics

## 📊 Project Status

- **Production Ready**: Core functionality implemented and tested
- **Active Development**: Enhanced features and optimizations ongoing
- **Comprehensive Testing**: Unit, integration, and system tests implemented
- **Documentation**: Complete technical documentation and deployment guides

## 🚀 Quick Start

Get up and running in minutes with our comprehensive [Quick Start Guide](docs/QUICK_START.md).

## 📚 Documentation

### Getting Started
- **[User Guide](docs/USER_GUIDE.md)** - Complete installation, configuration, and usage guide
- **[Quick Start Guide](docs/QUICK_START.md)** - Fast installation and basic setup
- **[Configuration Reference](docs/CONFIGURATION.md)** - Complete configuration options
- **[Deployment Guide](docs/DEPLOYMENT.md)** - Production deployment instructions

### User Documentation
- **[User Guide](docs/USER_GUIDE.md)** - Comprehensive user documentation with FAQ
- **[Troubleshooting Guide](docs/TROUBLESHOOTING.md)** - Common issues and diagnostic procedures
- **[Operations Guide](docs/OPERATIONS_GUIDE.md)** - System administration and maintenance

### Development & API
- **[API Reference](docs/API_REFERENCE.md)** - Complete API documentation with CLI interface
- **[Development Guide](docs/DEVELOPMENT.md)** - Building, testing, and contributing
- **[Architecture Guide](../ARCHITECTURE.md)** - System design and components

### Project Management
- **[Project Status](../STATUS.md)** - Current implementation status and progress
- **[Development Roadmap](../ROADMAP.md)** - Future plans and development phases
- **[Current Tasks](../TODO.md)** - Immediate priorities and bug fixes
- **[Engineering Specification](../PROJECT_INSTRUCTION.md)** - Detailed technical requirements

### Advanced Topics
- **[GPS Integration](docs/GPS_SYSTEM_COMPLETE.md)** - Multi-source GPS with enhanced OpenCellID geolocation
- **[Notifications](docs/NOTIFICATION_CONFIGURATION.md)** - Alert system setup
- **[Starlink API](docs/STARLINK_API_REFERENCE.md)** - Starlink-specific features
- **[Cellular Monitoring](docs/CELLULAR_STABILITY_MONITORING_COMPLETE.md)** - Cellular intelligence
- **[WiFi Optimization](docs/WIFI_OPTIMIZATION_COMPLETE.md)** - WiFi performance tuning

## 🤝 Contributing

We welcome contributions! Please see our [Development Guide](docs/DEVELOPMENT.md) for:

- Setting up your development environment
- Building and testing the project
- Contributing guidelines and code standards
- Pull request process

## 📄 License

See [LICENSE](LICENSE) file for details.

---

**autonomy** - Making network failover invisible through intelligent prediction and autonomous management.
