# Installation Guide

## Prerequisites

- OpenWrt or RUTOS router
- At least 25MB free space
- Network connectivity for package downloads

## Quick Installation

### OpenWrt/RUTOS

1. **Download the package**:
   ```bash
   wget https://github.com/markus-lassfolk/autonomy/releases/latest/download/autonomy-openwrt.ipk
   ```

2. **Install the package**:
   ```bash
   opkg install autonomy-openwrt.ipk
   ```

3. **Configure the service**:
   ```bash
   uci set autonomy.config.enabled=1
   uci commit autonomy
   ```

4. **Start the service**:
   ```bash
   /etc/init.d/autonomy start
   ```

## Manual Installation

### Building from Source

1. **Clone the repository**:
   ```bash
   git clone https://github.com/markus-lassfolk/autonomy.git
   cd autonomy
   ```

2. **Build for your platform**:
   ```bash
   make build-package
   ```

3. **Install the built package**:
   ```bash
   opkg install package/autonomy_*.ipk
   ```

## Docker Installation

```bash
docker pull markuslassfolk/autonomy:latest
docker run -d --name autonomy --network host markuslassfolk/autonomy:latest
```

## Verification

After installation, verify the service is running:

```bash
# Check service status
/etc/init.d/autonomy status

# Check ubus API
ubus call autonomy status

# Check logs
logread | grep autonomy
```

## Next Steps

- [Configuration Guide](configuration.md)
- [API Reference](api.md)
- [Troubleshooting](troubleshooting.md)
