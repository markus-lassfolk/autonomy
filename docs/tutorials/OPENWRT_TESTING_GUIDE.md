# OpenWrt Testing Environment Guide

This guide covers setting up and using the OpenWrt testing environment in WSL for the autonomy project.

## Quick Start

### 1. Setup WSL Environment (No Sudo Required)

```bash
# Run the no-sudo setup script
cd /mnt/d/GitCursor/autonomy
chmod +x test/setup-wsl-no-sudo.sh
./test/setup-wsl-no-sudo.sh

# Activate the environment
source ~/.bashrc
~/autonomy/test-environment.sh
```

### 2. Access LuCI Web Interface

**URL:** `http://172.26.83.101`

The LuCI-style web interface provides:
- System overview with real-time status
- Autonomy system management
- Network interface monitoring
- Configuration management
- Responsive design for mobile/desktop

### 3. Test Your Autonomy System

```bash
# Build the autonomy system
cd /mnt/d/GitCursor/autonomy
go build -o ~/autonomy/bin/autonomysysmgmt ./cmd/autonomysysmgmt

# Test ubus integration
~/autonomy/bin/ubus call autonomy status

# Test uci integration
~/autonomy/bin/uci show system
```

## Environment Features

### No-Sudo Development Environment

The setup creates a user-space development environment that doesn't require sudo for testing:

- **User-space directories:** `~/autonomy/`
- **Mock commands:** `~/autonomy/bin/ubus`, `~/autonomy/bin/uci`, `~/autonomy/bin/opkg`
- **Configuration files:** `~/autonomy/etc/config/`
- **Logs:** `~/autonomy/logs/`

### Available Commands

```bash
# Test commands (no sudo required)
ubus-test version
uci-test show system
opkg-test list-installed

# Development commands
autonomy-test    # Run Go tests
autonomy-build   # Build autonomy binary
autonomy-run     # Run autonomy system
```

### Environment Variables

```bash
export GOPATH=/home/markus/go
export AUTONOMY_CONFIG=/home/markus/autonomy/etc/config
export AUTONOMY_LOGS=/home/markus/autonomy/logs
export PATH=/home/markus/autonomy/bin:$PATH
```

## LuCI Web Interface

### Features

1. **System Overview**
   - Real-time uptime display
   - System information (hostname, architecture, kernel)
   - Network interface status
   - Service status (ubus, uci, mwan3)

2. **Autonomy System Management**
   - Start/Stop/Restart service controls
   - Real-time status display
   - Configuration viewing
   - Network interface monitoring

3. **System Configuration**
   - Hostname configuration
   - Timezone selection
   - Configuration forms

4. **Network Interfaces**
   - Interface status display
   - IP address information
   - MAC address details

### API Endpoints

The web interface includes mock API endpoints for testing:

- `GET /api/autonomy/status` - Get autonomy system status
- `POST /api/autonomy/start` - Start autonomy service
- `POST /api/autonomy/stop` - Stop autonomy service
- `POST /api/autonomy/restart` - Restart autonomy service
- `GET /api/system/config` - Get system configuration
- `POST /api/system/config` - Save system configuration

## Testing Workflows

### 1. Basic Testing

```bash
# Enter WSL environment
wsl -d openwrt-test

# Activate environment
source ~/.bashrc

# Test mock commands
ubus-test version
uci-test show system
opkg-test list-installed

# Test environment
~/autonomy/test-environment.sh
```

### 2. Development Testing

```bash
# Mount project directory
cd /mnt/d/GitCursor/autonomy

# Build and test
go build ./cmd/autonomysysmgmt
./autonomysysmgmt --config ~/autonomy/etc/config/autonomy

# Run tests
go test ./...
```

### 3. Web Interface Testing

1. Open browser to `http://172.26.83.101`
2. Navigate through different sections
3. Test autonomy system controls
4. Verify configuration forms
5. Check real-time status updates

## Troubleshooting

### Common Issues

1. **WSL not starting**
   ```bash
   wsl --shutdown
   wsl -d openwrt-test
   ```

2. **Environment not loaded**
   ```bash
   source ~/.bashrc
   ```

3. **Web interface not accessible**
   ```bash
   sudo systemctl status nginx
   sudo systemctl restart nginx
   ```

4. **Mock commands not working**
   ```bash
   chmod +x ~/autonomy/bin/*
   export PATH=/home/markus/autonomy/bin:$PATH
   ```

### Reset Environment

```bash
# Remove and recreate WSL instance
wsl --unregister openwrt-test
# Then run the setup script again
```

## Advanced Configuration

### Custom Mock Commands

You can extend the mock commands in `~/autonomy/bin/`:

```bash
# Example: Enhanced ubus mock
cat > ~/autonomy/bin/ubus << 'EOF'
#!/bin/bash
case "$1" in
    "call")
        if [ "$2" = "autonomy" ] && [ "$3" = "status" ]; then
            echo '{"status": "running", "version": "1.0.0", "uptime": "0:00:00"}'
        fi
        ;;
    *)
        echo '{"result": "success"}'
        ;;
esac
EOF
chmod +x ~/autonomy/bin/ubus
```

### Custom Configuration

Add custom OpenWrt configuration files:

```bash
# Create custom autonomy config
cat > ~/autonomy/etc/config/autonomy << 'EOF'
config autonomy 'main'
    option enabled '1'
    option log_level 'info'
    option config_path '/home/markus/autonomy/etc/config'
EOF
```

## Integration with Real Hardware

When testing on real OpenWrt hardware:

1. Replace mock commands with real ones
2. Update configuration paths to `/etc/config/`
3. Use real ubus and uci commands
4. Test with actual network interfaces

## Performance Considerations

- **Memory usage:** ~25MB for WSL environment
- **Disk space:** ~500MB for full setup
- **CPU usage:** Minimal when idle
- **Network:** Uses WSL2 networking

## Security Notes

- Mock commands are for testing only
- No real system modifications
- User-space environment isolation
- No privileged operations required

## Next Steps

1. **Test your autonomy system** using the mock environment
2. **Develop new features** with the no-sudo workflow
3. **Use the web interface** for monitoring and control
4. **Deploy to real hardware** when ready for production testing

For more information, see the [Virtual RUTOS Testing Guide](VIRTUAL_RUTOS_TESTING_GUIDE.md) for advanced testing scenarios.
