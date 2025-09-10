# Archived Files

This directory contains files that were moved from the main autonomy-daemon source tree to clean up the repository and build process.

## Backup Files
These are backup files that were created during development and are no longer needed:
- `gps_error_recovery.c.backup`
- `gps_location_services.c.backup`
- `ml_monitor.c.backup`
- `ml_monitor_advanced_networking.c.backup`
- `ml_monitor_analytics.c.backup`
- `ml_monitor_cli.c.backup`
- `starlink_cluster.c.backup`
- `ubus_monitor.c.backup`

## Excluded Module Files
These are module files that were excluded from the build due to conflicts or being less feature-complete than their alternatives:

### Replaced by More Feature-Complete Versions:
- `starlink_tracker.c` → Replaced by `shared/starlink-tracking/starlink_tracker.c` (more features)
- `utils/cellular_collector.c` → Replaced by `network/cellular_collector.c` (more comprehensive)
- `ubus/system_ubus.c` → Replaced by `utils/system_ubus.c` (more features)
- `utils/system_management.c` → Replaced by `core/system_management.c` (more comprehensive)
- `utils/http_client.c` → Replaced by `utils/http_client_libcurl.c` (uses libcurl)

### Conflicting with Core Modules:
- `network/network.c` → Functions integrated into `core/system_management.c`
- `gps/gps.c` → Functions integrated into `core/system_management.c`

### Cancelled Modules:
- `network_discovery_simple.c` → Cancelled per user request (not needed)
- `gps_discovery_simple.c` → Cancelled per user request (not needed)

### CLI Tool (Separate from Main Daemon):
- `ml_monitor_cli.c` → CLI tool with its own main() function, conflicts with daemon

## Archive Date
Files archived on: $(date)

## Reason for Archiving
These files were moved to:
1. Clean up the repository structure
2. Remove confusion about which modules are active
3. Simplify the build process
4. Remove backup files that are no longer needed
5. Document the module selection decisions made during development

## Note
These files are preserved in case they need to be referenced in the future, but they are not part of the active build process.
