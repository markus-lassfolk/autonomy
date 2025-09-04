# 🚀 Getting Started with Autonomy

## Quick Start Guide

Welcome to Autonomy, the intelligent multi-interface failover system for RutOS and OpenWrt routers. This guide will get you up and running in minutes.

## Prerequisites

- **Hardware**: RUTX50 or compatible router with **RutOS 7.17.1+**
- **Network**: Internet connection for initial setup
- **Optional**: Starlink dish for satellite tracking features

> **⚠️ Important**: Autonomy is designed for **RutOS** and relies on RUTOS-specific components. OpenWrt compatibility is limited. See [OpenWrt Support Status](../user-guides/openwrt-support-status.md) before attempting installation on vanilla OpenWrt.

## Installation

### Option 1: Package Installation (Recommended)
```bash
# Update package lists
opkg update

# Install autonomy
opkg install autonomy

# Enable and start the service
/etc/init.d/autonomy enable
/etc/init.d/autonomy start
```

### Option 2: Manual Installation
```bash
# Download latest release
wget https://github.com/your-repo/autonomy/releases/latest/autonomy.ipk

# Install package
opkg install autonomy.ipk

# Configure and start
uci set autonomy.main.enabled='1'
uci commit autonomy
/etc/init.d/autonomy start
```

## Basic Configuration

### 1. Network Interfaces
Autonomy automatically discovers your network interfaces. To verify:

```bash
# Check discovered interfaces
ubus call autonomy interfaces

# View current status
ubus call autonomy status
```

### 2. Starlink Integration (Optional)
If you have a Starlink dish:

```bash
# Enable Starlink tracking
uci set autonomy.starlink.enabled='1'
uci set autonomy.starlink.host='192.168.100.1'
uci commit autonomy

# Restart service
/etc/init.d/autonomy restart
```

### 3. GPS Configuration (Optional)
For location-based features:

```bash
# Enable GPS
uci set autonomy.gps.enabled='1'
uci set autonomy.gps.sources='rutos,starlink'
uci commit autonomy
```

## First Test

### Check System Status
```bash
# Overall system health
ubus call autonomy status

# Network interface details
ubus call autonomy interfaces

# GPS location (if configured)
ubus call gps location
```

### Test Manual Failover
```bash
# Switch to cellular
ubus call autonomy switch '{"interface": "cellular"}'

# Switch back to Starlink
ubus call autonomy switch '{"interface": "starlink"}'
```

### Monitor Logs
```bash
# View system logs
logread | grep autonomy

# Follow live logs
logread -f | grep autonomy
```

## Web Interface

Access the web interface at: `https://your-router-ip/cgi-bin/luci/admin/autonomy`

The web interface provides:
- Real-time network status
- Interface health monitoring
- Starlink satellite visualization
- Configuration management
- Historical performance data

## Next Steps

### Basic Usage
- [User Guide](USER_GUIDE.md) - Complete user manual
- [Configuration Guide](../developer-guides/configuration.md) - Advanced configuration

### Advanced Features
- [Starlink Tracking](../user-guides/starlink-tracking-overview.md) - Satellite prediction
- [GPS Integration](starlink-gps-integration.md) - Multi-source GPS
- [Predictive Failover](../developer-guides/predictive-failover.md) - ML-based predictions

### Development
- [SDK Master Guide](sdk-master-guide.md) - Development environment
- [VUCI Development](../developer-guides/vuci-development.md) - Web UI development

## Support

- **Documentation**: Complete guides in [docs/](../)
- **Issues**: Report bugs and feature requests
- **Community**: Join our development community

---

**Welcome to intelligent network management!** 🎉