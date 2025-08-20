# RUTOS Testing Guide

**Version:** 2.7.1 | **Updated:** 2025-07-27

**Version:** 2.6.0 | **Updated:** 2025-07-24

**Version:** 2.5.0 | **Updated:** 2025-07-24

**Version:** 2.4.12 | **Updated:** 2025-07-21

**Version:** 2.4.11 | **Updated:** 2025-07-21

**Version:** 2.4.10 | **Updated:** 2025-07-21

**Version:** 2.4.9 | **Updated:** 2025-07-21

**Version:** 2.4.8 | **Updated:** 2025-07-21

**Version:** 2.4.7 | **Updated:** 2025-07-21

This guide helps you test the Starlink monitoring scripts on the actual RUTOS environment.

## Quick RUTOS Color Test

First, test if colors work in your RUTOS environment:

```bash
# Upload and run the color test script
./scripts/test-rutos-colors.sh

# If colors don't show, force them:
FORCE_COLOR=1 ./scripts/test-rutos-colors.sh

# If you prefer no colors:
NO_COLOR=1 ./scripts/test-rutos-colors.sh
```

## Environment Differences: WSL vs RUTOS

### WSL/Development Environment

- Full bash with advanced features
- Rich terminal emulator support
- More environment variables set
- Different command availability

### RUTOS/Production Environment

- BusyBox shell (limited POSIX compliance)
- Basic terminal support over SSH
- Minimal environment variables
- Embedded system constraints

## Testing Approach

### 1. Color Functionality Test

```bash
# On RUTOS router:
./scripts/test-rutos-colors.sh
```

Expected behavior:

- **Via SSH**: Colors should auto-detect and work
- **Direct console**: May need `FORCE_COLOR=1`
- **No color support**: Falls back to plain text gracefully

### 2. Connectivity Test (Safe Mode)

```bash
# Test without actually changing anything
DEBUG=1 ./scripts/test-connectivity-rutos.sh
```

This will show:

- Environment detection
- Color support status
- Configuration loading
- All test results

### 3. Full Validation

```bash
# Complete system validation
./scripts/validate-config-rutos.sh
```

## Common RUTOS Issues

### Colors Not Showing

```bash
# Force colors
FORCE_COLOR=1 ./script.sh

# Check environment
echo "TERM: $TERM"
echo "SSH_CLIENT: $SSH_CLIENT"
```

### gRPC API Failures

1. **Check basic connectivity first**:

   ```bash
   ping 192.168.100.1
   nc -z 192.168.100.1 9200
   ```

2. **Verify grpcurl is available**:

   ```bash
   which grpcurl
   /usr/local/starlink-monitor/grpcurl --help
   ```

3. **Test manually**:

   ```bash
   grpcurl -plaintext -d '{}' 192.168.100.1:9200 SpaceX.API.Device.Device/GetStatus
   ```

### Configuration Issues

1. **Check config file exists**:

   ```bash
   ls -la /etc/starlink-config/config.sh
   ```

2. **Verify permissions**:

   ```bash
   chmod +x /usr/local/starlink-monitor/scripts/*.sh
   ```

3. **Test config loading**:

   ```bash
   DEBUG=1 ./scripts/validate-config-rutos.sh
   ```

## Debug Mode Usage

Enable debug mode for detailed troubleshooting:

```bash
# Full debug output
DEBUG=1 ./scripts/test-connectivity-rutos.sh

# With forced colors
DEBUG=1 FORCE_COLOR=1 ./scripts/test-connectivity-rutos.sh
```

Debug mode shows:

- Environment variables
- Color detection logic
- Configuration loading process
- Individual test details
- Error details

## File Transfer to RUTOS

### Using SCP

```bash
# From development machine
scp scripts/test-connectivity-rutos.sh root@192.168.1.1:/usr/local/starlink-monitor/scripts/
scp scripts/test-rutos-colors.sh root@192.168.1.1:/usr/local/starlink-monitor/scripts/
```

### Using curl (if scripts are on GitHub)

```bash
# On RUTOS router
curl -fsSL https://raw.githubusercontent.com/your-repo/main/scripts/test-connectivity-rutos.sh \
  -o /usr/local/starlink-monitor/scripts/test-connectivity-rutos.sh
chmod +x /usr/local/starlink-monitor/scripts/test-connectivity-rutos.sh
```

## Expected Results

### Working System

```text
[INFO] Starting Starlink Monitor Connectivity Tests v1.0.2
[SUCCESS] System requirements check passed
[SUCCESS] Network connectivity established
[SUCCESS] Starlink dish reachable (ping)
[SUCCESS] Starlink HTTP port accessible
[SUCCESS] Starlink gRPC API responsive
[INFO] Connectivity tests completed
```

### Common Issues

- **Literal color codes**: Environment doesn't support colors
- **gRPC timeouts**: Network or API issues
- **Missing commands**: Installation incomplete
- **Permission errors**: Script not executable

## Troubleshooting Commands

```bash
# Check system info
uname -a
cat /etc/openwrt_release

# Check network
ip route show
ping -c 1 8.8.8.8

# Check processes
ps | grep starlink
netstat -tuln | grep :9200

# Check logs
logread | grep starlink
tail -f /var/log/messages
```

## Recovery Commands

If something goes wrong:

```bash
# Reset permissions
chmod +x /usr/local/starlink-monitor/scripts/*.sh

# Reload configuration
. /etc/starlink-config/config.sh

# Restart networking (if needed)
/etc/init.d/network restart

# Clear temporary files
rm -f /tmp/*.$$
```

This guide ensures you can properly test and debug the scripts in the actual RUTOS environment where they'll be deployed.
