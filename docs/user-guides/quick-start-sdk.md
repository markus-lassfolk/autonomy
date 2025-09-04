# Quick Start Guide - RUTOS SDK Integration

## Overview

This guide provides quick step-by-step instructions for building and installing the autonomy daemon and VuCI web interface packages for RUTOS devices.

## Prerequisites

### Required Software

- **Go 1.19+**: For building the autonomy daemon
- **RUTOS SDK**: Teltonika RUTX50 SDK (optional, for full IPK generation)
- **Linux/WSL**: Build environment (Windows with WSL recommended)

### System Requirements

- **RAM**: 4GB+ for building
- **Disk Space**: 2GB+ for build artifacts
- **Network**: Internet connection for downloading dependencies

## Quick Build (5 minutes)

### 1. Clone Repository

```bash
git clone https://github.com/autonomy/autonomy.git
cd autonomy
```

### 2. Build Binary

```bash
# Build for ARM architecture (RUTOS devices)
CGO_ENABLED=0 GOOS=linux GOARCH=arm GOARM=7 go build -ldflags="-s -w" -o build/autonomyd ./cmd/autonomyd
```

### 3. Build Packages

```bash
# Use the automated build script
./scripts/build-packages.sh build --manual
```

### 4. Install on Device

```bash
# Copy binary to device
scp build/autonomyd root@your-router:/usr/sbin/

# Copy package files
scp -r package/autonomy/files/* root@your-router:/tmp/autonomy/

# On the device, install files
ssh root@your-router
cd /tmp/autonomy
chmod +x autonomy.init autonomyctl 99-autonomy-defaults
cp autonomy.init /etc/init.d/autonomy
cp autonomyctl /usr/libexec/
cp 99-autonomy-defaults /etc/uci-defaults/
cp autonomy.config /etc/config/autonomy
mkdir -p /etc/autonomy
cp autonomy.watchdog.example /etc/autonomy/watch.conf.example

# Enable and start service
/etc/init.d/autonomy enable
/etc/init.d/autonomy start
```

## Full Build with SDK (15 minutes)

### 1. Setup RUTOS SDK

```bash
# Set SDK path
export RUTOS_SDK_PATH="/path/to/rutos-ipq40xx-rutx-sdk"

# Initialize SDK environment
cd $RUTOS_SDK_PATH
source scripts/env.sh
```

### 2. Build with SDK

```bash
# Build everything with SDK
./scripts/build-packages.sh build --sdk
```

### 3. Install IPK Packages

```bash
# Copy IPK files to device
scp build/packages/*.ipk root@your-router:/tmp/

# On the device, install packages
ssh root@your-router
cd /tmp
opkg install autonomy_*.ipk
opkg install vuci-app-autonomy_*.ipk

# Configure and start
uci set autonomy.main.enable='1'
uci commit autonomy
/etc/init.d/autonomy start
```

## Web Interface Access

### 1. Access VuCI Interface

Open your web browser and navigate to:
```
http://your-router/admin/network/autonomy/
```

### 2. Available Pages

- **Status**: Real-time monitoring dashboard
- **Configuration**: UCI configuration management
- **Interfaces**: Network interface status
- **Telemetry**: Performance metrics
- **Logs**: Real-time log viewer
- **Resources**: System resource monitoring

### 3. Mobile Access

The web interface is fully responsive and works on mobile devices:
```
http://your-router/admin/network/autonomy/status
```

## Configuration

### 1. Basic Configuration

```bash
# Enable the service
uci set autonomy.main.enable='1'

# Set log level
uci set autonomy.main.log_level='info'

# Configure interfaces
uci set autonomy.starlink.name='wan'
uci set autonomy.cellular.name='wwan'
uci set autonomy.wifi.name='wlan'

# Commit changes
uci commit autonomy
```

### 2. Advanced Configuration

Edit the configuration file directly:
```bash
vi /etc/config/autonomy
```

Or use the web interface for visual configuration.

### 3. Watchdog Configuration (Optional)

```bash
# Copy example configuration
cp /etc/autonomy/watch.conf.example /etc/autonomy/watch.conf

# Edit configuration
vi /etc/autonomy/watch.conf
```

## Service Management

### 1. Service Control

```bash
# Start service
/etc/init.d/autonomy start

# Stop service
/etc/init.d/autonomy stop

# Restart service
/etc/init.d/autonomy restart

# Check status
/etc/init.d/autonomy status

# Reload configuration
/etc/init.d/autonomy reload
```

### 2. Command Line Interface

```bash
# Show status
autonomyctl status

# Show configuration
autonomyctl config

# Show interfaces
autonomyctl interfaces

# Show telemetry
autonomyctl telemetry

# Show health
autonomyctl health

# Reload configuration
autonomyctl reload
```

### 3. ubus Interface

```bash
# List available methods
ubus list | grep autonomy

# Call methods
ubus call autonomy status
ubus call autonomy config
ubus call autonomy interfaces
ubus call autonomy telemetry
ubus call autonomy health
```

## Monitoring and Logs

### 1. View Logs

```bash
# Service logs
tail -f /var/log/autonomyd.log

# System logs
logread | grep autonomy

# Web interface logs
/etc/init.d/autonomy logs
```

### 2. Health Checks

```bash
# Perform health check
/etc/init.d/autonomy health

# Show detailed information
/etc/init.d/autonomy info
```

### 3. Resource Monitoring

```bash
# Check system resources
autonomyctl resources

# Monitor process
top -p $(cat /var/run/autonomyd.pid)
```

## Troubleshooting

### 1. Service Won't Start

```bash
# Check configuration
/etc/init.d/autonomy test

# Check dependencies
opkg list-installed | grep ubus

# Check logs
tail -f /var/log/autonomyd.log
```

### 2. Web Interface Issues

```bash
# Check LuCI installation
opkg list-installed | grep luci

# Reload LuCI
luci-reload

# Check web server
logread | grep uhttpd
```

### 3. Network Issues

```bash
# Test interface connectivity
ping -I wan 8.8.8.8
ping -I wwan 8.8.8.8

# Check interface status
ip link show
ip addr show
```

### 4. Performance Issues

```bash
# Enable debug logging
uci set autonomy.main.log_level='debug'
uci commit autonomy
/etc/init.d/autonomy restart

# Monitor resources
top -p $(cat /var/run/autonomyd.pid)
free -h
df -h
```

## Development

### 1. Build for Development

```bash
# Build with debug symbols
go build -o build/autonomyd-debug ./cmd/autonomyd

# Run tests
go test ./pkg/...

# Run integration tests
./scripts/run-comprehensive-tests.ps1
```

### 2. Modify VuCI Interface

```bash
# Edit controller
vi vuci-app-autonomy/root/usr/lib/lua/luci/controller/autonomy.lua

# Edit view templates
vi vuci-app-autonomy/root/usr/lib/lua/luci/view/autonomy/status.htm

# Rebuild and install
./scripts/build-packages.sh build --manual
```

### 3. Debug RPC Daemon

```bash
# Test RPC methods
rpcd -i autonomy status
rpcd -i autonomy config

# Check RPC daemon logs
logread | grep rpcd
```

## Package Distribution

### 1. Create Package Repository

```bash
# Build packages
./scripts/build-packages.sh build --sdk

# Create repository
./scripts/build-packages.sh repository
```

### 2. Host Package Repository

```bash
# Set up web server
mkdir -p /var/www/autonomy-feed
cp build/repository/* /var/www/autonomy-feed/

# Generate Packages file
cd /var/www/autonomy-feed
opkg-make-index . > Packages
```

### 3. Configure Package Feed

On the RUTOS device:
```bash
# Add custom feed
echo "src/gz autonomy-feed http://your-server/autonomy-feed" >> /etc/opkg/customfeeds.conf

# Update and install
opkg update
opkg install autonomy
opkg install vuci-app-autonomy
```

## Support

### 1. Documentation

- **SDK Integration**: `docs/SDK_INTEGRATION.md`
- **VuCI Development**: `docs/VUCI_DEVELOPMENT.md`
- **API Reference**: `docs/API_REFERENCE.md`
- **User Guide**: `docs/USER_GUIDE.md`

### 2. Logs and Debugging

- **Service Logs**: `/var/log/autonomyd.log`
- **System Logs**: `logread | grep autonomy`
- **Web Interface**: Browser developer tools
- **RPC Debug**: `rpcd -i`

### 3. Community Support

- **GitHub Issues**: Report bugs and feature requests
- **Documentation**: Check `/usr/share/autonomy/` on device
- **Configuration Examples**: See `configs/` directory

## Next Steps

1. **Configure Interfaces**: Set up your network interfaces
2. **Test Failover**: Verify automatic failover functionality
3. **Monitor Performance**: Use the web interface for monitoring
4. **Customize Configuration**: Adjust thresholds and settings
5. **Set Up Notifications**: Configure alerts and notifications

---

**Need Help?** Check the troubleshooting section or refer to the comprehensive documentation in the `docs/` directory.
