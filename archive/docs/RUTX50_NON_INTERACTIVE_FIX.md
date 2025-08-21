# RUTX50 Non-Interactive Deployment Fix

## Issue Summary
The RUTOS deployment script `deploy-starlink-solution-v3-rutos.sh` was failing when executed via `curl | sh` on the RUTX50 router because it contained interactive prompts that would hang in non-interactive environments.

## Error Details
- **Environment**: RUTX50 router with busybox shell, /usr/local persistent storage (69MB available)
- **Execution Method**: `curl -fsSL https://raw.githubusercontent.com/markus-lassfolk/autonomy/main/deploy-starlink-solution-v3-rutos.sh | sh`
- **Failure Point**: Script prompted for "Starlink IP address [192.168.100.1]:" but stdin was not a terminal
- **Root Cause**: Interactive `read` commands in non-interactive environment

## Solutions Implemented

### 1. Interactive Mode Detection
Added `is_interactive()` function to detect execution environment:
```bash
is_interactive() {
    # Check if stdin is a terminal and not running in non-interactive mode
    [ -t 0 ] && [ "${BATCH_MODE:-0}" != "1" ]
}
```

**Detection Logic:**
- Uses `[ -t 0 ]` to check if stdin is a terminal
- Respects `BATCH_MODE=1` environment variable to force non-interactive mode
- Correctly handles piped execution (`curl | sh`) as non-interactive

### 2. Configuration Collection Enhancements

#### Basic Configuration (`collect_basic_configuration`)
- **Interactive Mode**: Prompts user for all configuration values with defaults
- **Non-Interactive Mode**: Uses environment variables if set, otherwise sensible defaults
- **Environment Variable Support**: All configuration can be pre-set via environment variables

#### Enhanced Configuration (`collect_enhanced_configuration`)
- **Interactive Mode**: Prompts for monitoring mode selection and advanced settings
- **Non-Interactive Mode**: Uses daemon mode with recommended settings
- **Environment Variable Support**: `MONITORING_MODE`, `DAEMON_AUTOSTART`, etc.

### 3. Environment Variable Configuration
The script now supports configuration via environment variables for automated deployments:

#### Network Configuration
- `STARLINK_IP` (default: 192.168.100.1)
- `STARLINK_PORT` (default: 9200)
- `RUTOS_IP` (default: your-router-ip)
- `MWAN_IFACE` (default: starlink)
- `MWAN_MEMBER` (default: starlink_m1_w1)

#### Thresholds
- `LATENCY_THRESHOLD` (default: 1000ms)
- `PACKET_LOSS_THRESHOLD` (default: 10%)
- `OBSTRUCTION_THRESHOLD` (default: 5%)

#### Feature Toggles
- `ENABLE_AZURE` (default: false)
- `ENABLE_PUSHOVER` (default: false)
- `AZURE_ENDPOINT`
- `PUSHOVER_USER_KEY`
- `PUSHOVER_API_TOKEN`

#### Monitoring Configuration
- `MONITORING_MODE` (default: daemon)
- `DAEMON_AUTOSTART` (default: true)
- `MONITORING_INTERVAL` (default: 60s)
- `QUICK_CHECK_INTERVAL` (default: 30s)
- `DEEP_ANALYSIS_INTERVAL` (default: 300s)

## Deployment Examples

### 1. Basic Non-Interactive Deployment
```bash
curl -fsSL https://raw.githubusercontent.com/markus-lassfolk/autonomy/main/deploy-starlink-solution-v3-rutos.sh | sh
```
Uses all defaults, daemon monitoring mode, autostart enabled.

### 2. Custom Configuration Deployment
```bash
export STARLINK_IP="192.168.100.2"
export ENABLE_AZURE="true"
export AZURE_ENDPOINT="https://my-azure-endpoint.com"
curl -fsSL https://raw.githubusercontent.com/markus-lassfolk/autonomy/main/deploy-starlink-solution-v3-rutos.sh | sh
```

### 3. Batch Mode Deployment
```bash
BATCH_MODE=1 ./deploy-starlink-solution-v3-rutos.sh
```
Forces non-interactive mode even when run directly.

## RUTX50 Compatibility
- **Persistent Storage**: Automatically detects and uses `/usr/local` (69MB available)
- **Filesystem**: Compatible with overlay filesystem structure
- **Shell**: Works with busybox shell environment
- **Memory**: Efficient resource usage for router constraints

## Testing Verification
Created test scripts to verify functionality:
- `test-interactive-detection.sh`: Validates interactive mode detection
- All tests pass showing correct detection of:
  - Interactive mode when run directly
  - Non-interactive mode when piped
  - BATCH_MODE=1 override

## Benefits
1. **Automated Deployment**: Full hands-off deployment capability
2. **Environment Variable Control**: Configuration without code modification
3. **Backward Compatibility**: Interactive mode still works for manual installation
4. **RUTX50 Optimized**: Persistent storage detection and resource efficiency
5. **Robust Error Handling**: Graceful fallback to defaults

## Impact
- ✅ **FIXED**: Non-interactive deployment now works on RUTX50
- ✅ **ENHANCED**: Environment variable configuration support
- ✅ **MAINTAINED**: Full backward compatibility
- ✅ **OPTIMIZED**: RUTX50 hardware compatibility confirmed

This fix enables fully automated deployment of the Starlink monitoring solution on RUTX50 routers without any user interaction required.
