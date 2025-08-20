# Quick Start Guide

This guide will get you up and running with autonomy on your RutOS or OpenWrt router.

## Prerequisites

- RutOS or OpenWrt router
- mwan3 package installed
- Go 1.22+ (for building from source)

## Installation

### Building from Source

```bash
# Build for ARM (RutOS/OpenWrt)
export CGO_ENABLED=0
GOOS=linux GOARCH=arm GOARM=7 go build -ldflags "-s -w" -o autonomyd ./cmd/autonomyd
strip autonomyd

# Install
cp autonomyd /usr/sbin/
chmod 755 /usr/sbin/autonomyd
```

### Package Installation

If you have a pre-built package:

```bash
# Install the package
opkg install autonomy

# Or if using a custom repository
opkg install autonomy_1.0.0_arm_cortex-a7.ipk
```

## Configuration

### Basic Configuration

Create `/etc/config/autonomy`:

```uci
config autonomy 'main'
    option enable '1'
    option use_mwan3 '1'
    option poll_interval_ms '1500'
    option predictive '1'
    option switch_margin '10'
    option log_level 'info'
    option gps_enable '1'
    option notifications_enable '1'
    option watchdog_enable '1'
```

### Advanced Configuration

For more detailed configuration options, see [Configuration Reference](CONFIGURATION.md).

## Basic Usage

### Starting the Service

```bash
# Start the daemon
/etc/init.d/autonomy start

# Enable auto-start on boot
/etc/init.d/autonomy enable

# Check service status
/etc/init.d/autonomy status
```

### Command Line Interface

```bash
# Check overall status
autonomyctl status

# View all network members and their health scores
autonomyctl members

# View detailed member information
autonomyctl members --detailed

# Manual failover to a specific interface
autonomyctl failover --interface starlink

# View telemetry data
autonomyctl telemetry

# Check GPS location and status
autonomyctl gps

# View system health
autonomyctl health

# Check notification status
autonomyctl notifications
```

### Monitoring and Debugging

```bash
# View real-time logs
logread -f | grep autonomy

# Check system metrics
autonomyctl metrics

# View decision history
autonomyctl decisions

# Export configuration
autonomyctl config export
```

## Verification

### Check Installation

```bash
# Verify binary is installed
which autonomyd

# Check version
autonomyd --version

# Verify configuration
autonomyctl config validate
```

### Test Basic Functionality

```bash
# Test member discovery
autonomyctl members

# Test metric collection
autonomyctl metrics

# Test GPS functionality (if enabled)
autonomyctl gps
```

## Troubleshooting

### Common Issues

1. **Service won't start**: Check configuration syntax and dependencies
2. **No members discovered**: Verify mwan3 is properly configured
3. **GPS not working**: Check GPS hardware and configuration
4. **Notifications not sending**: Verify notification service configuration

### Debug Mode

```bash
# Run with debug logging
autonomyd --config /etc/config/autonomy --log-level debug

# Check detailed logs
logread | grep autonomy
```

For more detailed troubleshooting, see [Troubleshooting Guide](TROUBLESHOOTING.md).

## Next Steps

- Review [Configuration Reference](CONFIGURATION.md) for advanced options
- Set up [Notifications](NOTIFICATION_CONFIGURATION.md) for alerts
- Configure [GPS Integration](GPS_SYSTEM_COMPLETE.md) for location-based features
- Read [Deployment Guide](DEPLOYMENT.md) for production deployment
- Check [API Reference](API_REFERENCE.md) for programmatic access
