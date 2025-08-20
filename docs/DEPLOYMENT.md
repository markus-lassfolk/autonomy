# autonomy Deployment Guide

This guide explains how to build, install, and configure the autonomy multi-interface failover daemon on RutOS and OpenWrt systems.

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Building from Source](#building-from-source)
3. [Installation](#installation)
4. [Configuration](#configuration)
5. [Service Management](#service-management)
6. [Testing](#testing)
7. [Troubleshooting](#troubleshooting)
8. [Upgrading](#upgrading)

## Prerequisites

### Development Environment

- **Go 1.22+** - Required for building from source
- **Git** - For cloning the repository
- **Make** - For build automation (optional)
- **Cross-compilation tools** - For building for different architectures

### Target System

- **RutOS** (Teltonika) or **OpenWrt** (modern releases)
- **mwan3** package installed (recommended)
- **ubus** available (required)
- **procd** init system (required)

### System Requirements

- **RAM**: Minimum 32MB, recommended 64MB+
- **Flash**: Minimum 8MB, recommended 16MB+
- **CPU**: Any ARM or x86 processor supported by OpenWrt

## Building from Source

### 1. Clone the Repository

```bash
git clone https://github.com/markus-lassfolk/autonomy.git
cd autonomy
```

### 2. Build for Target Architecture

#### Using the Build Script (Recommended)

```bash
# Build for all supported architectures
./scripts/build.sh --all

# Build for specific architecture (e.g., ARM)
./scripts/build.sh --target linux/arm --strip --package

# Build with custom version
./scripts/build.sh --version 1.0.1 --target linux/arm64
```

#### Manual Build

```bash
# For ARM (RutOS/OpenWrt)
export CGO_ENABLED=0
export GOOS=linux
export GOARCH=arm
export GOARM=7
go build -ldflags "-s -w" -o autonomyd ./cmd/autonomyd

# For ARM64
export GOOS=linux
export GOARCH=arm64
go build -ldflags "-s -w" -o autonomyd ./cmd/autonomyd

# For x86_64
export GOOS=linux
export GOARCH=amd64
go build -ldflags "-s -w" -o autonomyd ./cmd/autonomyd
```

### 3. Build Output

The build script creates:
- Binary files in `build/` directory
- Package files (`.tar.gz`) if `--package` is used
- Stripped binaries if `--strip` is used

## Installation

### Method 1: Manual Installation

1. **Copy the binary**:
   ```bash
   scp build/autonomyd-linux-armv7 root@192.168.1.1:/usr/sbin/autonomyd
   ```

2. **Make it executable**:
   ```bash
   ssh root@192.168.1.1 "chmod 755 /usr/sbin/autonomyd"
   ```

3. **Copy the CLI**:
   ```bash
   scp scripts/autonomyctl root@192.168.1.1:/usr/sbin/autonomyctl
   ssh root@192.168.1.1 "chmod 755 /usr/sbin/autonomyctl"
   ```

4. **Copy the init script**:
   ```bash
   scp scripts/autonomy.init root@192.168.1.1:/etc/init.d/autonomy
   ssh root@192.168.1.1 "chmod 755 /etc/init.d/autonomy"
   ```

### Method 2: Using Package

1. **Extract the package**:
   ```bash
   tar -xzf build/autonomy-1.0.0-linux-arm.tar.gz -C /
   ```

2. **Set permissions**:
   ```bash
   chmod 755 /usr/sbin/autonomyd
   chmod 755 /usr/sbin/autonomyctl
   chmod 755 /etc/init.d/autonomy
   ```

### Method 3: OpenWrt Package

For OpenWrt systems, you can create an `.ipk` package:

```bash
# Create package structure
mkdir -p autonomy_1.0.0/usr/sbin
mkdir -p autonomy_1.0.0/etc/init.d
mkdir -p autonomy_1.0.0/etc/config

# Copy files
cp build/autonomyd-linux-armv7 autonomy_1.0.0/usr/sbin/autonomyd
cp scripts/autonomyctl autonomy_1.0.0/usr/sbin/
cp scripts/autonomy.init autonomy_1.0.0/etc/init.d/autonomy
cp configs/autonomy.example autonomy_1.0.0/etc/config/autonomy

# Create control file
cat > autonomy_1.0.0/CONTROL/control << EOF
Package: autonomy
Version: 1.0.0
Depends: mwan3
Architecture: arm_cortex-a7
Installed-Size: 1024
Description: Multi-interface failover daemon
EOF

# Build package
tar -czf data.tar.gz -C autonomy_1.0.0 .
tar -czf control.tar.gz -C autonomy_1.0.0/CONTROL .
echo "2.0" > debian-binary
ar -r autonomy_1.0.0_arm_cortex-a7.ipk debian-binary control.tar.gz data.tar.gz
```

## Configuration

### 1. Create Configuration File

```bash
# Copy sample configuration
cp configs/autonomy.example /etc/config/autonomy
```

### 2. Edit Configuration

Edit `/etc/config/autonomy` to match your setup:

```uci
config autonomy 'main'
    option enable '1'
    option use_mwan3 '1'
    option poll_interval_ms '1500'
    option predictive '1'
    option ml_enabled '1'
    option ml_model_path '/etc/autonomy/models.json'
    option switch_margin '10'
    option log_level 'info'

# Configure your interfaces
config member 'starlink_any'
    option detect 'auto'
    option class 'starlink'
    option weight '100'

config member 'cellular_any'
    option detect 'auto'
    option class 'cellular'
    option weight '80'
    option metered '1'
```

The `ml_model_path` option must point to a writable location containing
JSON-formatted model definitions. If the file exists, models are loaded
at startup; otherwise, new models will be trained and saved to this path.

### 3. Configure mwan3 (Recommended)

Ensure mwan3 is properly configured:

```bash
# Install mwan3 if not already installed
opkg update
opkg install mwan3

# Configure mwan3 interfaces
uci set mwan3.wan_starlink=interface
uci set mwan3.wan_starlink.enabled=1
uci set mwan3.wan_starlink.track_method=ping
uci set mwan3.wan_starlink.track_ip=8.8.8.8

uci set mwan3.wan_cell=interface
uci set mwan3.wan_cell.enabled=1
uci set mwan3.wan_cell.track_method=ping
uci set mwan3.wan_cell.track_ip=8.8.8.8

# Commit changes
uci commit mwan3
```

## Service Management

### 1. Start the Service

```bash
# Start autonomy
/etc/init.d/autonomy start

# Enable at boot
/etc/init.d/autonomy enable
```

### 2. Check Status

```bash
# Check service status
/etc/init.d/autonomy status

# Check daemon status
autonomyctl status

# Check members
autonomyctl members
```

### 3. Service Commands

```bash
# Start/stop/restart
/etc/init.d/autonomy start
/etc/init.d/autonomy stop
/etc/init.d/autonomy restart

# Reload configuration
/etc/init.d/autonomy reload

# Health check
/etc/init.d/autonomy health

# Show information
/etc/init.d/autonomy info
```

## Testing

### 1. Basic Functionality

```bash
# Check if daemon is running
ps aux | grep autonomyd

# Check ubus service
ubus list | grep autonomy

# Test ubus calls
ubus call autonomy status
ubus call autonomy members
```

### 2. Interface Testing

```bash
# Test specific member
autonomyctl metrics wan_starlink

# Check events
autonomyctl events 10

# Manual failover test
autonomyctl failover
```

### 3. Log Monitoring

```bash
# Monitor logs
logread -f | grep autonomy

# Check daemon logs
tail -f /var/log/autonomyd.log
```

### 4. Performance Testing

```bash
# Check memory usage
ps aux | grep autonomyd

# Check CPU usage
top -p $(pgrep autonomyd)

# Check telemetry storage
autonomyctl info
```

## Troubleshooting

### Common Issues

#### 1. Daemon Won't Start

**Symptoms**: Service fails to start
**Solutions**:
```bash
# Check binary permissions
ls -la /usr/sbin/autonomyd

# Check configuration
/etc/init.d/autonomy test

# Check logs
logread | grep autonomy

# Run manually for debugging
/usr/sbin/autonomyd -config /etc/config/autonomy -log-level debug
```

#### 2. No Members Discovered

**Symptoms**: `autonomyctl members` returns empty
**Solutions**:
```bash
# Check mwan3 configuration
ubus call mwan3 status

# Check network interfaces
ip link show

# Check UCI configuration
uci show mwan3
```

#### 3. ubus Service Not Available

**Symptoms**: `ubus list` doesn't show autonomy
**Solutions**:
```bash
# Check if daemon is running
ps aux | grep autonomyd

# Check ubus socket
ls -la /var/run/ubus.sock

# Restart ubus if needed
/etc/init.d/ubus restart
```

#### 4. High Memory Usage

**Symptoms**: Daemon using too much RAM
**Solutions**:
```bash
# Reduce telemetry retention
uci set autonomy.main.retention_hours=12
uci set autonomy.main.max_ram_mb=8
uci commit autonomy

# Restart service
/etc/init.d/autonomy restart
```

### Debug Mode

Enable debug logging for troubleshooting:

```bash
# Set debug level
autonomyctl setlog debug

# Monitor logs
logread -f | grep autonomy

# Check debug output
autonomyctl info
```

### Log Analysis

```bash
# Show recent events
autonomyctl events 50

# Check member history
autonomyctl history wan_starlink 3600

# Analyze metrics
autonomyctl metrics wan_starlink
```

## Upgrading

### 1. Backup Configuration

```bash
# Backup current configuration
cp /etc/config/autonomy /etc/config/autonomy.backup

# Backup telemetry data (if needed)
cp -r /tmp/autonomy /tmp/autonomy.backup
```

### 2. Stop Service

```bash
/etc/init.d/autonomy stop
```

### 3. Install New Version

```bash
# Copy new binary
cp autonomyd-new /usr/sbin/autonomyd
chmod 755 /usr/sbin/autonomyd

# Update scripts if needed
cp autonomyctl-new /usr/sbin/autonomyctl
cp autonomy.init-new /etc/init.d/autonomy
```

### 4. Start Service

```bash
/etc/init.d/autonomy start
/etc/init.d/autonomy status
```

### 5. Verify Upgrade

```bash
# Check version
autonomyctl status

# Test functionality
autonomyctl members
autonomyctl events 10
```

## Performance Tuning

### Memory Optimization

```uci
config autonomy 'main'
    # Reduce telemetry retention
    option retention_hours '12'
    option max_ram_mb '8'
    
    # Increase polling interval
    option poll_interval_ms '2000'
```

### Network Optimization

```uci
config autonomy 'main'
    # Use conservative data cap mode
    option data_cap_mode 'conservative'
    
    # Reduce switch margin for faster failover
    option switch_margin '5'
```

### CPU Optimization

```uci
config autonomy 'main'
    # Increase polling interval
    option poll_interval_ms '3000'
    
    # Disable predictive mode if not needed
    option predictive '0'
```

## Security Considerations

### 1. File Permissions

```bash
# Set correct permissions
chmod 755 /usr/sbin/autonomyd
chmod 755 /usr/sbin/autonomyctl
chmod 755 /etc/init.d/autonomy
chmod 644 /etc/config/autonomy
```

### 2. Network Security

- The daemon binds to localhost only for metrics/health endpoints
- ubus calls are restricted to local system
- No external network access required

### 3. Configuration Security

- Keep configuration files secure
- Don't expose sensitive information in logs
- Use appropriate log levels in production

## Support

For issues and questions:

1. Check the troubleshooting section above
2. Review logs with debug level enabled
3. Check the project documentation
4. Open an issue on GitHub with detailed information

### Useful Commands

```bash
# System information
autonomyctl info

# Service health
/etc/init.d/autonomy health

# Configuration validation
/etc/init.d/autonomy test

# Performance monitoring
ps aux | grep autonomyd
top -p $(pgrep autonomyd)
```
