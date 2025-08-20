# 🚀 RUTOS Starlink Failover

**Version:** 2.7.1 | **Updated:** 2025-07-27

**Version:** 2.6.0 | **Updated:** 2025-07-24

**Version:** 2.5.0 | **Updated:** 2025-07-24

**Version:** 2.4.12 | **Updated:** 2025-07-21

**Version:** 2.4.11 | **Updated:** 2025-07-21

**Version:** 2.4.10 | **Updated:** 2025-07-21

**Version:** 2.4.9 | **Updated:** 2025-07-21

**Version:** 2.4.8 | **Updated:** 2025-07-21

**Version:** 2.4.7 | **Updated:** 2025-07-21

> **📍 Victron GPS Integration Moved!**  
> The Victron GPS failover functionality has been split into its own repository for better maintainability.  
> **New location:** [rutos-victron-gps](https://github.com/markus-lassfolk/rutos-victron-gps)

[![Shell Script Quality](https://github.com/markus-lassfolk/autonomy/actions/workflows/shellcheck-format.yml/badge.svg)](https://github.com/markus-lassfolk/autonomy/actions/workflows/shellcheck-format.yml)
[![Security Scan](https://github.com/markus-lassfolk/autonomy/actions/workflows/security-scan.yml/badge.svg)](https://github.com/markus-lassfolk/autonomy/actions/workflows/security-scan.yml)
[![Python Code Quality](https://github.com/markus-lassfolk/autonomy/actions/workflows/python-quality.yml/badge.svg)](https://github.com/markus-lassfolk/autonomy/actions/workflows/python-quality.yml)
[![PowerShell Validation](https://github.com/markus-lassfolk/autonomy/actions/workflows/powershell-validation.yml/badge.svg)](https://github.com/markus-lassfolk/autonomy/actions/workflows/powershell-validation.yml)

[![Documentation Check](https://github.com/markus-lassfolk/autonomy/actions/workflows/docs-check.yml/badge.svg)](https://github.com/markus-lassfolk/autonomy/actions/workflows/docs-check.yml)
[![Configuration Validation](https://github.com/markus-lassfolk/autonomy/actions/workflows/config-validation.yml/badge.svg)](https://github.com/markus-lassfolk/autonomy/actions/workflows/config-validation.yml)
[![Infrastructure Tests](https://github.com/markus-lassfolk/autonomy/actions/workflows/infrastructure-validation.yml/badge.svg)](https://github.com/markus-lassfolk/autonomy/actions/workflows/infrastructure-validation.yml)
[![Integration Tests](https://github.com/markus-lassfolk/autonomy/actions/workflows/integration-tests.yml/badge.svg)](https://github.com/markus-lassfolk/autonomy/actions/workflows/integration-tests.yml)

[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Azure](https://img.shields.io/badge/Azure-Ready-0078d4.svg)](Starlink-RUTOS-Failover/AzureLogging/)
[![RUTOS](https://img.shields.io/badge/RUTOS-Compatible-orange.svg)](https://teltonika-networks.com/)
[![Starlink](https://img.shields.io/badge/Starlink-Integrated-brightgreen.svg)](https://starlink.com/)

A comprehensive solution for **RV and boat owners** seeking robust internet connectivity. This repository provides
intelligent failover systems for Starlink connections on RUTOS routers.

## 🌟 Features

### 🔄 Proactive Starlink Failover

- **Real-time quality monitoring** using Starlink's internal API
- **Soft failover** preserving existing connections
- **Intelligent recovery** with stability checks
- **Comprehensive notifications** via Pushover
- **Data logging** for threshold optimization

## 🚀 Quick Start

### Option 1: Automated Installation (Recommended)

```bash
# Download and run the installer
curl -fL https://raw.githubusercontent.com/markus-lassfolk/autonomy/main/scripts/install-rutos.sh | sh

# Configure the system
nano /root/starlink-monitor/config/config.sh

# Validate configuration
/root/starlink-monitor/scripts/validate-config.sh

# Configure mwan3 (see documentation)
```

### Option 2: Manual Installation

1. **Clone the repository**

   ```bash
   git clone https://github.com/markus-lassfolk/autonomy.git
   cd autonomy
   ```

2. **Install dependencies**

   ```bash
   # Install grpcurl (ARMv7)
   curl -fL https://github.com/fullstorydev/grpcurl/releases/download/v1.9.3/grpcurl_1.9.3_linux_armv7.tar.gz -o /tmp/grpcurl.tar.gz
   tar -zxf /tmp/grpcurl.tar.gz -C /root/ grpcurl
   chmod +x /root/grpcurl

   # Install jq (ARMv7)
   curl -fL https://github.com/jqlang/jq/releases/download/jq-1.7.1/jq-linux-armhf -o /root/jq
   chmod +x /root/jq
   ```

3. **Configure the system**

   ```bash
   cp config/config.template.sh config/config.sh
   nano config/config.sh
   ```

4. **Deploy scripts**

   ```bash
   cp Starlink-RUTOS-Failover/starlink_monitor.sh /root/
   cp Starlink-RUTOS-Failover/99-pushover_notify /etc/hotplug.d/iface/99-pushover_notify
   chmod +x /root/starlink_monitor.sh /etc/hotplug.d/iface/99-pushover_notify
   ```

## 📋 Prerequisites

### Hardware Requirements

- **Teltonika RUTX50** or similar OpenWrt/RUTOS router
- **Starlink dish** in Bypass Mode
- **Cellular backup** connection

### Software Requirements

- **RUTOS firmware** (latest stable)
- **mwan3 package** configured
- **Pushover account** for notifications

## 🛠️ Configuration

### 1. Basic Configuration

For standard setups, edit `/root/starlink-monitor/config/config.sh`:

```bash
# Network settings
STARLINK_IP="192.168.100.1"
STARLINK_PORT="9200"
MWAN_IFACE="wan"
MWAN_MEMBER="member1"

# Pushover notifications
PUSHOVER_TOKEN="your_pushover_token"
PUSHOVER_USER="your_pushover_user_key"

# Failover thresholds
PACKET_LOSS_THRESHOLD=5       # 5%
OBSTRUCTION_THRESHOLD=3       # 3%
LATENCY_THRESHOLD=150         # 150ms
```

### 1.1 Advanced Configuration (RUTX50 Production)

For RUTX50 routers with dual SIM and GPS, use the advanced template:

```bash
# Use advanced configuration template
cp config/config.advanced.template.sh config/config.sh
nano config/config.sh

# Run UCI optimizer for your existing setup
scripts/uci-optimizer.sh analyze           # Analyze current config
scripts/uci-optimizer.sh optimize          # Apply optimizations
```

**Advanced features include:**

- **GPS-enhanced failover** with movement detection
- **Dual SIM integration** with automatic switching
- **MQTT logging** for integration with existing systems
- **Intelligent reboot scheduling** based on system health
- **Enhanced cellular optimization** for mobile environments

📖 **See [RUTX50 Production Guide](docs/RUTX50-PRODUCTION-GUIDE.md) for detailed setup**

### 2. mwan3 Configuration

```bash
# Set interface priorities
uci set mwan3.member1.metric='1'     # Starlink (highest priority)
uci set mwan3.member3.metric='2'     # Primary SIM
uci set mwan3.member4.metric='4'     # Backup SIM

# Configure health checks
uci set mwan3.wan.track_ip='1.0.0.1' '8.8.8.8'
uci set mwan3.wan.reliability='1'
uci set mwan3.wan.timeout='1'
uci set mwan3.wan.interval='1'
uci set mwan3.wan.down='2'
uci set mwan3.wan.up='3'

# Commit changes
uci commit mwan3
mwan3 restart
```

### 3. Static Route for Starlink

```bash
# Add static route to Starlink management interface
uci add network route
uci set network.@route[-1].interface='wan'
uci set network.@route[-1].target='192.168.100.1'
uci set network.@route[-1].netmask='255.255.255.255'
uci commit network
/etc/init.d/network reload
```

## 🚨 Starlink API Change Response

If you receive a daily notification about a Starlink API schema change:

1. Run `scripts/check_starlink_api_change.sh` to confirm and view the new schema.
2. Compare `/root/starlink_api_schema_last.json` and `/tmp/starlink_api_schema_current.json` for differences.
3. Update `starlink_monitor.sh` and related scripts to match any new/renamed fields.
4. Validate with `scripts/validate-config.sh` and `tests/test-suite.sh`.
5. See `docs/API_CHANGE_RESPONSE.md` for a full step-by-step process.

## 🔧 Advanced Features

### Enhanced Monitoring

The enhanced monitoring system includes:

- **Centralized configuration** management
- **Rate limiting** for notifications
- **Health checks** and diagnostics
- **Graceful error handling**
- **Comprehensive logging**

### Security Features

- **No hardcoded credentials**
- **Configuration validation**
- **Secure defaults**
- **Rate limiting** protection

### Observability

- **Structured logging** with rotation
- **Health status** tracking
- **Performance metrics** collection
- **Notification history**

## 📊 Monitoring and Troubleshooting

### View System Status

```bash
# Check monitor status
logread | grep StarlinkMonitor

# View health status
cat /tmp/run/starlink_monitor.health

# Check recent notifications
tail -f /var/log/notifications.log
```

### Test Notifications

```bash
# Test notification system
/etc/hotplug.d/iface/99-pushover_notify test

# Test Starlink API
grpcurl -plaintext -d '{"get_status":{}}' 192.168.100.1:9200 SpaceX.API.Device.Device/Handle
```

### Performance Analysis

```bash
# View performance data
cat /root/starlink_performance_log.csv

# Generate API documentation
/root/starlink-monitor/scripts/generate_api_docs.sh
```

## 🔒 Security Considerations

- **Never commit** real credentials to version control
- **Use secure connections** where possible
- **Regularly update** dependencies
- **Monitor logs** for security events
- **Limit API access** to necessary services

## 📚 Documentation

### Project Structure

```text
├── .github/workflows/          # CI/CD workflows
├── config/                     # Configuration templates
│   ├── config.template.sh      # Basic configuration template
│   └── config.advanced.template.sh  # Advanced RUTX50 template
├── scripts/                    # Installation and utility scripts
│   ├── install.sh             # Automated installation
│   ├── validate-config.sh     # Configuration validation
│   └── uci-optimizer.sh       # UCI configuration optimizer
├── docs/                       # Documentation
│   ├── RUTX50-PRODUCTION-GUIDE.md  # RUTX50 specific guide
│   ├── TROUBLESHOOTING.md      # Troubleshooting guide
│   └── API_REFERENCE.md        # API documentation
└── Starlink-RUTOS-Failover/    # Failover system
    ├── starlink_monitor.sh     # Main monitoring script
    ├── 99-pushover_notify      # Notification system
    └── AzureLogging/           # Azure integration
```

### Key Scripts

| Script                | Purpose                                            |
| --------------------- | -------------------------------------------------- |
| `starlink_monitor.sh` | Advanced monitoring with centralized configuration |
| `99-pushover_notify`  | Intelligent notification system                    |
| `starlink_logger.sh`  | Data collection for threshold optimization         |
| `install.sh`          | Automated installation script                      |
| `validate-config.sh`  | Configuration validation                           |

## 🤝 Contributing

1. **Fork** the repository
2. **Create** a feature branch
3. **Test** your changes thoroughly
4. **Submit** a pull request

### Development Guidelines

- **Follow shell script best practices**
- **Add comprehensive error handling**
- **Include documentation** for new features
- **Test on actual hardware** when possible

## 📜 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## ⚠️ Disclaimer

This software is provided "as-is" without warranty. Test thoroughly in your environment before production use. The
author is not responsible for any damage or service interruption.

## 🙏 Acknowledgments

- **Starlink** for the unofficial API
- **Teltonika** for excellent hardware
- **Community contributors** for testing and feedback

---

**Need help?** Check the [documentation](docs/) or open an
[issue](https://github.com/markus-lassfolk/autonomy/issues).
