# Unified Scripts Migration Guide

<!-- Version: 2.7.0 - Auto-updated documentation -->

## Overview

The RUTOS Starlink Failover system now includes unified scripts that combine all functionality from the
previous basic and enhanced versions into single, configuration-driven scripts.

## New Unified Scripts

### `starlink_monitor_unified-rutos.sh`

- **Replaces**: `starlink_monitor-rutos.sh` + `starlink_monitor_enhanced-rutos.sh`
- **Features**: All monitoring capabilities controlled by configuration flags
- **Backward Compatible**: Default behavior matches original basic script

### `starlink_logger_unified-rutos.sh`

- **Replaces**: `starlink_logger-rutos.sh` + `starlink_logger_enhanced-rutos.sh`
- **Features**: All logging capabilities controlled by configuration flags
- **Backward Compatible**: Default behavior matches original basic script

## Migration Steps

### 1. Update Configuration

Enhanced features are now integrated into the main `config.sh` file. No separate configuration file needed!

Edit `/etc/starlink-config/config.sh` and find the "UNIFIED SCRIPTS ENHANCED FEATURES" section:

```bash
# Basic installation (default - no changes needed)
ENABLE_GPS_TRACKING="false"
ENABLE_CELLULAR_TRACKING="false"
ENABLE_ENHANCED_METRICS="false"
ENABLE_STATISTICAL_AGGREGATION="false"

# Enhanced installation (enable desired features)
ENABLE_GPS_TRACKING="true"
ENABLE_CELLULAR_TRACKING="true"
ENABLE_ENHANCED_METRICS="true"
ENABLE_STATISTICAL_AGGREGATION="true"
```

### 2. Update Cron Entries

Replace existing cron entries with unified scripts:

```bash
# OLD cron entries:
*/5 * * * * /usr/local/starlink-monitor/scripts/starlink_monitor-rutos.sh
*/1 * * * * /usr/local/starlink-monitor/scripts/starlink_logger-rutos.sh

# NEW cron entries:
*/5 * * * * /usr/local/starlink-monitor/scripts/starlink_monitor_unified-rutos.sh
*/1 * * * * /usr/local/starlink-monitor/scripts/starlink_logger_unified-rutos.sh
```

### 3. Test Migration

1. **Test unified scripts**:

   ```bash
   # Test monitor
   DEBUG=1 /usr/local/starlink-monitor/scripts/starlink_monitor_unified-rutos.sh

   # Test logger
   DEBUG=1 /usr/local/starlink-monitor/scripts/starlink_logger_unified-rutos.sh
   ```

2. **Verify configuration**:

   ```bash
   # Check which features are enabled
   grep "ENABLE_.*=" /etc/starlink-config/config.sh
   ```

3. **Monitor logs**:

   ```bash
   # Check for any errors
   tail -f /var/log/syslog | grep -E "(StarlinkMonitor|StarlinkLogger)"
   ```

## Configuration Examples

### Basic Stationary Installation

```bash
# All enhanced features disabled (original behavior)
ENABLE_GPS_TRACKING="false"
ENABLE_CELLULAR_TRACKING="false"
ENABLE_ENHANCED_METRICS="false"
ENABLE_STATISTICAL_AGGREGATION="false"
```

### Enhanced Stationary Installation

```bash
# Better metrics and reboot detection
ENABLE_GPS_TRACKING="false"
ENABLE_CELLULAR_TRACKING="false"
ENABLE_ENHANCED_METRICS="true"
ENABLE_STATISTICAL_AGGREGATION="true"
```

### Mobile/RV Installation

```bash
# Full feature set for mobile use
ENABLE_GPS_TRACKING="true"
ENABLE_CELLULAR_TRACKING="true"
ENABLE_MULTI_SOURCE_GPS="true"
ENABLE_ENHANCED_FAILOVER="true"
ENABLE_GPS_LOGGING="true"
ENABLE_CELLULAR_LOGGING="true"
ENABLE_ENHANCED_METRICS="true"
ENABLE_STATISTICAL_AGGREGATION="true"
```

## Feature Benefits

| Feature                          | Benefit                              | Use Case              |
| -------------------------------- | ------------------------------------ | --------------------- |
| `ENABLE_GPS_TRACKING`            | Location-aware failover decisions    | Mobile installations  |
| `ENABLE_CELLULAR_TRACKING`       | Backup connection intelligence       | RV/maritime use       |
| `ENABLE_ENHANCED_METRICS`        | Better threshold tuning data         | All installations     |
| `ENABLE_STATISTICAL_AGGREGATION` | Long-term analytics (60:1 reduction) | Data analysis         |
| `ENABLE_ENHANCED_FAILOVER`       | Multi-factor failover logic          | Mobile/complex setups |

## Backward Compatibility

The unified scripts maintain full backward compatibility:

- **Default behavior**: Identical to original basic scripts
- **CSV format**: Same columns when enhanced features disabled
- **Performance**: No overhead when features disabled
- **Configuration**: Existing configs work without changes

## Legacy Script Support

The original scripts remain available for compatibility:

- `starlink_monitor-rutos.sh` (basic monitoring)
- `starlink_logger-rutos.sh` (basic logging)
- `starlink_monitor_enhanced-rutos.sh` (enhanced monitoring)
- `starlink_logger_enhanced-rutos.sh` (enhanced logging)

However, **unified scripts are recommended** for all new installations.

## Troubleshooting

### Feature Not Working

1. Verify feature flag is set to `"true"` in config.sh
2. Check script debug output: `DEBUG=1 script_name.sh`
3. Ensure required tools are available (gpsctl, gsmctl)

### Performance Issues

1. Disable unused features to reduce overhead
2. Increase cron intervals if needed
3. Monitor system resource usage

### CSV Format Changes

1. Enhanced features add columns to CSV files
2. Use `ENABLE_ENHANCED_METRICS="false"` for original format
3. Backup existing CSV files before migration

## Support

For issues with unified scripts:

1. Check debug output with `DEBUG=1`
2. Verify configuration flags
3. Test with minimal feature set first
4. Review logs in `/var/log/syslog`

The unified approach provides maximum flexibility while maintaining the simplicity and reliability of the
original RUTOS Starlink Failover system.
