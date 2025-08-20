# Configuration Upgrade Script

Upgrade Guide Version: 1.2.6

## Overview

The `upgrade-to-advanced.sh` script allows you to seamlessly upgrade from basic Starlink configuration to advanced
configuration while preserving all your existing settings.

## Features

- ✅ **Preserves all existing settings** - Your current configuration is migrated automatically
- ✅ **Intelligent migration** - Maps basic settings to advanced template
- ✅ **Automatic backup** - Creates timestamped backup of current config
- ✅ **Downloads missing template** - Automatically downloads advanced template if needed
- ✅ **Shows new features** - Displays available advanced features after upgrade
- ✅ **Easy rollback** - Simple command to revert to basic config

## Usage

### Basic Usage

```bash
# Run the upgrade script
/root/starlink-monitor/scripts/upgrade-to-advanced.sh
```

### What It Does

1. **Backup**: Creates backup of current config with timestamp
2. **Download**: Downloads advanced template if not present
3. **Migrate**: Copies all your settings to advanced template
4. **Upgrade**: Replaces basic config with advanced config
5. **Report**: Shows new features available

### Settings Migrated

All basic configuration settings are automatically migrated:

- Network configuration (Starlink IP, MWAN interfaces)
- Notification settings (Pushover credentials, notification flags)
- Thresholds (packet loss, obstruction, latency)
- File paths and directories
- Binary locations
- RUTOS API settings
- Logging configuration
- Advanced timeouts and GPS settings

### New Features Added

After upgrading, you'll have access to:

- **Enhanced notifications** (signal reset, SIM switch, GPS status)
- **Rate limiting** (prevent notification spam)
- **Dual cellular configuration** (primary/backup SIM)
- **Mobile-optimized thresholds** (critical levels for immediate failover)
- **GPS movement detection** (reset obstruction maps when moving)
- **MQTT integration** (external monitoring systems)
- **RMS integration** (Teltonika Remote Management)
- **Cellular optimization** (auto SIM switching, data limits)
- **Advanced debugging** options

## Example Output

```text
=== Upgrade to Advanced Configuration ===

ℹ Creating backup of current configuration...
✅ Backup created: /root/starlink-monitor/config/config.sh.backup.20250714_220130
ℹ Migrating configuration from basic to advanced...
✅ Migrated STARLINK_IP: 192.168.100.1 + STARLINK_PORT: 9200
✅ Migrated PUSHOVER_TOKEN: your_token_here
✅ Migrated NOTIFY_ON_CRITICAL: 1
✅ Migrated PACKET_LOSS_THRESHOLD: 0.05
... (all settings migrated)

✅ Configuration successfully upgraded to advanced!
ℹ Backup of original config: /root/starlink-monitor/config/config.sh.backup.20250714_220130

🚀 Advanced Features Now Available:
   (detailed feature list displayed)

🎉 Upgrade Complete!
```

## Post-Upgrade Steps

1. **Edit configuration**: `vi /root/starlink-monitor/config/config.sh`
2. **Validate configuration**: `/root/starlink-monitor/scripts/validate-config.sh`
3. **Restart monitoring**: `systemctl restart starlink-monitor` (if running)
4. **Test the system** manually

## Rollback

If you need to revert to basic configuration:

```bash
# Find your backup file
ls -la /root/starlink-monitor/config/config.sh.backup.*

# Restore from backup
cp /root/starlink-monitor/config/config.sh.backup.YYYYMMDD_HHMMSS /root/starlink-monitor/config/config.sh
```

## Troubleshooting

### Script not found

```bash
# If script is missing, download it manually
wget -O /root/starlink-monitor/scripts/upgrade-to-advanced.sh \
  https://raw.githubusercontent.com/markus-lassfolk/autonomy/feature/testing-improvements/scripts/upgrade-to-advanced.sh
chmod +x /root/starlink-monitor/scripts/upgrade-to-advanced.sh
```

### Advanced template not found

The script automatically downloads the advanced template if it's missing. If this fails, check your internet connection.

### Permission denied

Make sure the script is executable:

```bash
chmod +x /root/starlink-monitor/scripts/upgrade-to-advanced.sh
```

## Benefits

### Why Upgrade to Advanced?

- **Mobile environments**: Better handling of RV, boat, remote site deployments
- **Complex setups**: Dual SIM, multiple cellular providers
- **Integration needs**: MQTT, RMS, external monitoring systems
- **Better control**: More granular notification and threshold settings
- **GPS awareness**: Movement detection and obstruction map reset

### When to Upgrade

- After basic installation is working
- When you need dual cellular configuration
- When you want better notification control
- When integrating with external systems
- When deploying in mobile environments

## Support

For issues or questions about the upgrade script:

1. Check the backup file was created
2. Verify your basic configuration was working before upgrade
3. Use the validation script to check for configuration errors
4. Review the GitHub repository for updates and documentation
