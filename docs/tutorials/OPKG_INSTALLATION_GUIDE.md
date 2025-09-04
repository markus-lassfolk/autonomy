# OpenWrt Package (opkg) Installation Guide

This guide covers building, installing, and testing the autonomy package using opkg on OpenWrt systems.

## Quick Start

### 1. Build the Package

```bash
# In WSL environment
cd /mnt/d/GitCursor/autonomy
chmod +x test/build-simple-package.sh
./test/build-simple-package.sh
```

### 2. Install the Package

```bash
# Copy package to OpenWrt device
scp build-openwrt/packages/autonomy_1.0.0_x86_64.ipk root@your-router:/tmp/

# Install on OpenWrt device
opkg install /tmp/autonomy_1.0.0_x86_64.ipk
```

### 3. Test the Installation

```bash
# Start the service
/etc/init.d/autonomy start

# Check status
/etc/init.d/autonomy status

# Test the binary
/usr/bin/autonomysysmgmt --help
```

## Package Contents

The autonomy package includes:

- **Binary:** `/usr/bin/autonomysysmgmt` - Main autonomy system binary
- **Init Script:** `/etc/init.d/autonomy` - Service management script
- **Configuration:** `/etc/config/autonomy` - Default configuration file
- **Dependencies:** luci-base (for web interface integration)

## Package Details

### Package Information
- **Name:** autonomy
- **Version:** 1.0.0
- **Architecture:** x86_64
- **Size:** ~4.6MB
- **Dependencies:** luci-base

### Configuration Structure

```bash
config autonomy 'main'
    option enabled '1'
    option log_level 'info'

config autonomy 'starlink'
    option enabled '1'
    option api_key ''

config autonomy 'cellular'
    option enabled '1'
    option interface 'wwan0'
```

## Installation Methods

### Method 1: Direct Installation

```bash
# Install package directly
opkg install autonomy_1.0.0_x86_64.ipk

# Verify installation
opkg list-installed | grep autonomy
```

### Method 2: Package Repository

```bash
# Add custom repository
echo "src/gz autonomy-feed http://your-server/autonomy-feed" >> /etc/opkg/customfeeds.conf

# Update package lists
opkg update

# Install from repository
opkg install autonomy
```

### Method 3: Manual Installation

```bash
# Extract package manually
mkdir -p /tmp/autonomy-install
cd /tmp/autonomy-install
ar x autonomy_1.0.0_x86_64.ipk

# Extract data
tar -xzf data.tar.gz -C /

# Extract control
tar -xzf control.tar.gz -C /tmp/autonomy-install

# Run postinst script
chmod +x /tmp/autonomy-install/postinst
/tmp/autonomy-install/postinst
```

## Service Management

### Start/Stop Service

```bash
# Start service
/etc/init.d/autonomy start

# Stop service
/etc/init.d/autonomy stop

# Restart service
/etc/init.d/autonomy restart

# Check status
/etc/init.d/autonomy status
```

### Enable/Disable Service

```bash
# Enable service (start on boot)
/etc/init.d/autonomy enable

# Disable service
/etc/init.d/autonomy disable
```

### Service Configuration

The service uses procd for process management:

```bash
# Service configuration in /etc/init.d/autonomy
START=95          # Start order
STOP=15           # Stop order
USE_PROCD=1       # Use procd for process management
```

## Testing the Installation

### 1. Basic Functionality Test

```bash
# Test binary execution
/usr/bin/autonomysysmgmt --help

# Test configuration loading
/usr/bin/autonomysysmgmt --config /etc/config/autonomy --dry-run

# Test service start
/etc/init.d/autonomy start
sleep 2
ps aux | grep autonomysysmgmt
```

### 2. Configuration Test

```bash
# Check configuration
cat /etc/config/autonomy

# Test configuration parsing
/usr/bin/autonomysysmgmt --config /etc/config/autonomy --check
```

### 3. Service Integration Test

```bash
# Test ubus integration (if available)
ubus call autonomy status

# Test uci integration
uci show autonomy

# Test service restart
/etc/init.d/autonomy restart
```

## Troubleshooting

### Common Issues

1. **Package Installation Fails**
   ```bash
   # Check dependencies
   opkg list-installed | grep luci-base

   # Install missing dependencies
   opkg update
   opkg install luci-base
   ```

2. **Service Won't Start**
   ```bash
   # Check logs
   logread | grep autonomy

   # Test binary manually
   /usr/bin/autonomysysmgmt --config /etc/config/autonomy --foreground
   ```

3. **Configuration Issues**
   ```bash
   # Validate configuration
   uci show autonomy

   # Reset to defaults
   rm /etc/config/autonomy
   opkg install --force-reinstall autonomy_1.0.0_x86_64.ipk
   ```

### Debug Commands

```bash
# Check package installation
opkg list-installed | grep autonomy

# Check file locations
find /usr/bin -name "*autonomy*"
find /etc -name "*autonomy*"

# Check service status
/etc/init.d/autonomy status

# Check process
ps aux | grep autonomysysmgmt

# Check logs
logread | grep autonomy
tail -f /var/log/messages | grep autonomy
```

## Advanced Configuration

### Custom Configuration

```bash
# Edit configuration
vi /etc/config/autonomy

# Example advanced configuration
config autonomy 'main'
    option enabled '1'
    option log_level 'debug'
    option check_interval '30'
    option max_retries '3'

config autonomy 'starlink'
    option enabled '1'
    option api_key 'your-starlink-api-key'
    option health_check_interval '60'

config autonomy 'cellular'
    option enabled '1'
    option interface 'wwan0'
    option apn 'internet'
    option username ''
    option password ''

config autonomy 'wifi'
    option enabled '1'
    option interface 'wlan0'
    option ssid 'your-wifi-ssid'
    option key 'your-wifi-password'
```

### Environment Variables

```bash
# Set environment variables
export AUTONOMY_LOG_LEVEL=debug
export AUTONOMY_CONFIG_PATH=/etc/config/autonomy

# Run with custom environment
AUTONOMY_LOG_LEVEL=debug /usr/bin/autonomysysmgmt --config /etc/config/autonomy
```

## Performance Monitoring

### Resource Usage

```bash
# Monitor CPU and memory
top -p $(pgrep autonomysysmgmt)

# Check memory usage
cat /proc/$(pgrep autonomysysmgmt)/status | grep VmRSS

# Monitor disk usage
du -sh /usr/bin/autonomysysmgmt
```

### Log Monitoring

```bash
# Follow logs in real-time
logread -f | grep autonomy

# Check recent logs
logread | grep autonomy | tail -20

# Clear logs
logread -c
```

## Uninstallation

### Remove Package

```bash
# Remove package
opkg remove autonomy

# Clean up configuration
rm -f /etc/config/autonomy

# Remove binary
rm -f /usr/bin/autonomysysmgmt

# Remove init script
rm -f /etc/init.d/autonomy
```

### Complete Cleanup

```bash
# Force remove
opkg remove --force-depends autonomy

# Clean package cache
opkg clean

# Remove configuration files
rm -rf /etc/config/autonomy*
```

## Integration with LuCI

### Web Interface Access

After installation, the autonomy system is accessible via:

- **URL:** `http://your-router-ip/cgi-bin/luci/admin/system/autonomy`
- **Menu:** System → Autonomy

### LuCI Features

- Service control (start/stop/restart)
- Configuration management
- Status monitoring
- Log viewing

## Security Considerations

### File Permissions

```bash
# Set proper permissions
chmod 755 /usr/bin/autonomysysmgmt
chmod 644 /etc/config/autonomy
chmod 755 /etc/init.d/autonomy

# Set ownership
chown root:root /usr/bin/autonomysysmgmt
chown root:root /etc/config/autonomy
chown root:root /etc/init.d/autonomy
```

### Network Security

- Configure firewall rules for autonomy services
- Use secure API keys for Starlink integration
- Enable HTTPS for web interface access

## Next Steps

1. **Configure the system** for your specific network setup
2. **Test failover scenarios** with your network interfaces
3. **Monitor performance** and adjust configuration as needed
4. **Set up logging** for operational insights
5. **Configure notifications** for system events

For more information, see the [OpenWrt Testing Guide](OPENWRT_TESTING_GUIDE.md) for development and testing workflows.
