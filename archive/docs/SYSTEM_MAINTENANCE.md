# System Maintenance Script for RUTOS

## Overview

The `system-maintenance-rutos.sh` script is a generic system health checker and issue resolver for RUTOS systems.
It automatically detects common problems and can fix them without manual intervention.

## Features

- **Automated Issue Detection**: Scans for common system problems
- **Automatic Fixes**: Can automatically resolve detected issues
- **Detailed Logging**: Comprehensive logging of all actions taken
- **Multiple Modes**: Check-only, fix, or report generation modes
- **Extensible**: Easy to add new checks and fixes

## Current Checks

1. **Missing /var/lock directory** - Fixes the "can't create /var/lock/qmimux.lock" error
2. **Missing /var/run directory** - Ensures critical runtime directory exists
3. **Missing critical system directories** - Creates essential system directories
4. **Large log files** - Rotates and truncates oversized log files
5. **Old temporary files** - Cleans up files older than 7 days
6. **High memory usage** - Clears caches when memory usage is high
7. **Database optimization loop** - Fixes database spam loop issues (nlbwmon/ip_block)
8. **"Can't open database" spam** - Fixes database corruption issues causing spam errors
9. **Network interface issues** - Reports interfaces that are down
10. **System service health** - Restarts critical services if they're down
11. **Disk space monitoring** - Cleans up when disk usage is high
12. **Permission issues** - Fixes incorrect permissions on critical files

## Configuration Controls

The maintenance script behavior can be controlled through configuration variables in your `config.sh` file.
These controls allow you to customize how aggressive the maintenance system should be:

### Basic Configuration Controls

```bash
# Master control for automatic fixes
MAINTENANCE_AUTO_FIX_ENABLED=true          # Allow maintenance script to fix issues automatically (true/false)

# Control automatic reboot behavior
MAINTENANCE_AUTO_REBOOT_ENABLED=false      # Allow system reboot for persistent critical issues (true/false)
MAINTENANCE_REBOOT_THRESHOLD=5             # Number of consecutive critical runs before reboot

# Fine-grained fix controls
MAINTENANCE_SERVICE_RESTART_ENABLED=true   # Allow service restarts during maintenance (true/false)
MAINTENANCE_DATABASE_FIX_ENABLED=true      # Allow database fixes during maintenance (true/false)

# Override run mode
MAINTENANCE_MODE_OVERRIDE=""               # Force specific mode: auto, check, fix, report (empty = use default)

# Safety controls
MAINTENANCE_MAX_FIXES_PER_RUN=10           # Maximum fixes to attempt in single run
MAINTENANCE_COOLDOWN_AFTER_FIXES=300       # Cooldown period (seconds) after performing fixes
```

### Configuration Scenarios

**Conservative Setup (Monitoring Only):**

```bash
MAINTENANCE_AUTO_FIX_ENABLED=false         # Only detect issues, don't fix
MAINTENANCE_AUTO_REBOOT_ENABLED=false      # Never reboot automatically
MAINTENANCE_MODE_OVERRIDE="check"          # Always run in check-only mode
```

**Aggressive Setup (Full Automation):**

```bash
MAINTENANCE_AUTO_FIX_ENABLED=true          # Fix all detected issues
MAINTENANCE_AUTO_REBOOT_ENABLED=true       # Allow reboots for persistent issues
MAINTENANCE_REBOOT_THRESHOLD=3             # Lower threshold for reboots
MAINTENANCE_SERVICE_RESTART_ENABLED=true   # Allow service restarts
MAINTENANCE_DATABASE_FIX_ENABLED=true      # Allow database fixes
```

**Service-Only Fixes:**

```bash
MAINTENANCE_AUTO_FIX_ENABLED=true          # Allow fixes
MAINTENANCE_SERVICE_RESTART_ENABLED=true   # Allow service restarts
MAINTENANCE_DATABASE_FIX_ENABLED=false     # Don't fix databases
MAINTENANCE_AUTO_REBOOT_ENABLED=false      # Don't reboot
```

### Advanced Configuration (Advanced Template)

The advanced configuration template provides more aggressive defaults suitable for experienced users:

- `MAINTENANCE_AUTO_REBOOT_ENABLED=true` - Reboots enabled by default
- `MAINTENANCE_REBOOT_THRESHOLD=3` - Lower threshold for reboots
- `MAINTENANCE_MAX_FIXES_PER_RUN=15` - Higher fix limit
- `MAINTENANCE_COOLDOWN_AFTER_FIXES=180` - Shorter cooldown period

## Usage

```bash
# Run automatic maintenance (check and fix issues)
/usr/local/starlink-monitor/scripts/system-maintenance-rutos.sh

# Check for issues only (no fixes)
/usr/local/starlink-monitor/scripts/system-maintenance-rutos.sh check

# Generate maintenance report
/usr/local/starlink-monitor/scripts/system-maintenance-rutos.sh report

# Show help
/usr/local/starlink-monitor/scripts/system-maintenance-rutos.sh help

# Run with debug output
DEBUG=1 /usr/local/starlink-monitor/scripts/system-maintenance-rutos.sh
```

## Cron Schedule

The script is automatically scheduled to run every 6 hours:

```cron
0 */6 * * * /usr/local/starlink-monitor/scripts/system-maintenance-rutos.sh auto
```

## Log Files

- **Maintenance Log**: `/var/log/system-maintenance.log` - Detailed action log
- **Report**: `/var/log/system-maintenance-report.txt` - Generated reports
- **Syslog**: Actions are also logged to system log with tag "SystemMaintenance"

## Adding New Checks

To add a new maintenance check, follow this pattern:

1. Create a new function in the script following the naming convention `check_*`:

   ```bash
   # Check [N]: [Description]
   check_new_issue() {
       log_debug "Checking for [describe what you're checking]"

       if [ condition_indicating_problem ]; then
           record_action "FOUND" "[Issue description]" "[Fix description]"

           if [ "$RUN_MODE" = "fix" ] || [ "$RUN_MODE" = "auto" ]; then
               if fix_command_here; then
                   record_action "FIXED" "[Issue description]" "[Command used to fix]"
               else
                   log_error "Failed to fix [issue]"
               fi
           fi
       else
           log_debug "[Normal state message]"
           record_action "CHECK" "[What was checked]" "No action needed"
       fi
   }
   ```

2. Add your function to the `run_all_checks()` function:

   ```bash
   run_all_checks() {
       # ... existing checks ...
       check_new_issue
       # Add more checks here in the future
   }
   ```

## Example: The Original Issue

The script was created to fix this specific issue:

**Problem**:

```text
daemon.notice netifd: mob1s1a1 (10544): ./wwan.sh: eval: line 133: can't create /var/lock/qmimux.lock: nonexistent directory
```

**Solution Implemented**:

```bash
check_var_lock_directory() {
    if [ ! -d "/var/lock" ]; then
        # Issue found - directory missing
        if mkdir -p /var/lock 2>/dev/null; then
            chmod 755 /var/lock
            # Issue fixed
        fi
    fi
}
```

## Safety Features

- **Backup Strategy**: Critical files are backed up before modification
- **Conservative Fixes**: Only safe, well-tested fixes are applied automatically
- **Detailed Logging**: Every action is logged for auditing
- **Check Mode**: Can run in check-only mode for safety
- **Error Handling**: Comprehensive error handling prevents system damage

## Integration

- **Automatic Installation**: Installed automatically with `install-rutos.sh`
- **Cron Integration**: Scheduled automatically in system crontab
- **Starlink Integration**: Coordinates with other Starlink monitoring tools
- **RUTOS Compatible**: Designed specifically for RUTOS/OpenWrt environment

## Maintenance

The script is self-maintaining:

- Logs are automatically rotated when they become large
- Temporary files are cleaned up automatically
- Old maintenance logs are pruned to prevent disk space issues

## Future Enhancements

Planned additions:

- Network connectivity validation and repair
- Firewall rule verification and correction
- UCI configuration validation
- Package integrity checks
- Certificate and key validation
- Storage health monitoring

## Database Spam Fixes

The system includes specialized fixes for common RUTX50 database issues:

### Check 7: Database Optimization Loop

- **Issue**: Services like nlbwmon and ip_block get stuck in database optimization loops
- **Symptoms**: Log entries like "Unable to optimize database", "Failed to restore database"
- **Fix**: Stops services, backs up and resets databases, restarts ubus, restarts services
- **Threshold**: ≥5 optimization errors triggers automatic fix

### Check 8: "Can't open database" Spam

- **Issue**: Database corruption causes continuous "Can't open database" errors
- **Symptoms**: Log entries like "user.err: Can't open database"
- **Fix**: Based on proven user solution:
  1. Cleans /log filesystem if >80% full
  2. Finds and recreates small/corrupted databases in /log directory
  3. Stops problematic services (nlbwmon, ip_block, collectd, statistics)
  4. Backs up and resets common database files
  5. Restarts ubus if needed
  6. Restarts services
- **Threshold**: ≥5 "Can't open database" errors triggers automatic fix

### Standalone Database Spam Fix

The database spam fix is also available as a standalone script:

```bash
# Fix database spam issues (both types)
/usr/local/starlink-monitor/scripts/fix-database-spam-rutos.sh

# Check only (no fixes)
/usr/local/starlink-monitor/scripts/fix-database-spam-rutos.sh check

# Show current log status
/usr/local/starlink-monitor/scripts/fix-database-spam-rutos.sh status

# Run with debug output
DEBUG=1 /usr/local/starlink-monitor/scripts/fix-database-spam-rutos.sh fix
```

The standalone script provides more detailed output and verification than the integrated maintenance checks.

## Troubleshooting

If the maintenance script isn't working:

1. Check if it's installed:

   ```bash
   ls -la /usr/local/starlink-monitor/scripts/system-maintenance-rutos.sh
   ```

2. Check cron schedule:

   ```bash
   crontab -l | grep maintenance
   ```

3. Check recent logs:

   ```bash
   tail -50 /var/log/system-maintenance.log
   ```

4. Run manually with debug:

   ```bash
   DEBUG=1 /usr/local/starlink-monitor/scripts/system-maintenance-rutos.sh check
   ```

## Version History

- **v1.0.0**: Initial release with 10 basic system checks
  - Missing directory fixes
  - Log rotation
  - Memory and disk cleanup
  - Service health monitoring
- **v1.1.0**: Enhanced database spam fixes
  - Added Check 7: Database optimization loop detection and fix
  - Added Check 8: "Can't open database" spam detection and fix
  - Enhanced fix-database-spam-rutos.sh to handle both database issues
  - Based on user-reported RUTX50 issues with proven solutions
  - Permission fixes
