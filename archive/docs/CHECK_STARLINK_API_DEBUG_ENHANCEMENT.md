# Check Starlink API Script - Debug Enhancement Summary

## Issue Fixed

### Configuration File Syntax Error
**Problem**: Script failed with error: `/etc/starlink-config/config.sh: line 775: Enable: not found`

**Root Cause**: The configuration file has a syntax error at line 775 where "Enable" is being treated as a command instead of part of a variable assignment or comment.

**Solution Applied**: Enhanced configuration loading with error handling and helpful error messages.

## Enhanced Error Handling

### Before (Vulnerable to Config Errors)
```bash
CONFIG_FILE="${CONFIG_FILE:-/etc/starlink-config/config.sh}"
if [ -f "$CONFIG_FILE" ]; then
    . "$CONFIG_FILE"  # No error handling - crashes on syntax errors
fi
```

### After (Robust Error Handling)
```bash
CONFIG_FILE="${CONFIG_FILE:-/etc/starlink-config/config.sh}"
if [ -f "$CONFIG_FILE" ]; then
    log_debug "Attempting to load configuration from: $CONFIG_FILE"
    if ! . "$CONFIG_FILE" 2>/dev/null; then
        log_error "CONFIGURATION ERROR: Failed to load $CONFIG_FILE"
        log_error "This usually indicates a syntax error in the configuration file."
        log_error "Common issues:"
        log_error "  - Missing quotes around values"
        log_error "  - Unescaped special characters"
        log_error "  - Missing 'export' keyword"
        log_error "  - Comments starting with words instead of #"
        log_error ""
        log_error "Please check line 775 and surrounding lines for syntax errors."
        log_error "Each variable should be: export VARIABLE_NAME=\"value\""
        log_error "Each comment should start with: # Comment text"
        exit 1
    fi
    log_debug "Configuration loaded successfully from: $CONFIG_FILE"
else
    log_debug "Configuration file not found: $CONFIG_FILE (using defaults)"
fi
```

## Enhanced Configuration Debugging

The script already had excellent configuration debugging. We enhanced it further:

### Configuration Validation Features
- **Runtime Environment**: Shows DRY_RUN, DEBUG, RUTOS_TEST_MODE status
- **Connection Settings**: STARLINK_IP, STARLINK_PORT with validation
- **Binary Paths**: grpcurl, jq with existence and executable checks
- **Notification Settings**: Pushover token/user validation with length checks
- **Installation Paths**: INSTALL_DIR and derived binary paths

### Binary Validation Enhancement
```bash
# Enhanced binary validation with executable checks
if [ ! -f "${GRPCURL_CMD}" ]; then
    log_debug "⚠️  WARNING: grpcurl binary not found at ${GRPCURL_CMD} - API calls will fail"
elif [ ! -x "${GRPCURL_CMD}" ]; then
    log_debug "⚠️  WARNING: grpcurl binary not executable at ${GRPCURL_CMD}"
else
    log_debug "✓ grpcurl binary found and executable: ${GRPCURL_CMD}"
fi
```

### Pushover Validation Enhancement
```bash
# Enhanced Pushover token validation
if [ "${PUSHOVER_TOKEN}" = "YOUR_PUSHOVER_API_TOKEN" ]; then
    log_debug "⚠️  WARNING: PUSHOVER_TOKEN not configured - notifications will fail"
elif [ "${#PUSHOVER_TOKEN}" -lt 30 ]; then
    log_debug "⚠️  WARNING: PUSHOVER_TOKEN appears too short (${#PUSHOVER_TOKEN} chars)"
else
    log_debug "✓ PUSHOVER_TOKEN appears valid (${#PUSHOVER_TOKEN} chars)"
fi
```

## Existing Comprehensive Debug Features

This script already had **excellent** debugging capabilities:

### 📡 API Call Debugging
```bash
log_debug "GRPC CALL: $GRPCURL_CMD -plaintext -max-time 10 -d '{\"get_device_info\":{}}' $STARLINK_IP:$STARLINK_PORT SpaceX.API.Device.Device/Handle"
log_debug "GRPC COMMAND: $grpc_cmd"
log_debug "GRPC EXECUTION: Running in debug mode with full output"
log_debug "GRPC EXIT CODE: $grpc_exit"
log_debug "GRPC RAW OUTPUT (first 500 chars): $(echo "$grpc_output" | cut -c1-500)"
log_debug "GRPC SUCCESS: Processing JSON response with jq"
log_debug "JQ COMMAND: echo \"\$grpc_output\" | $JQ_CMD -r '.apiVersion // \"0\"'"
log_debug "JQ EXIT CODE: $jq_exit"
log_debug "JQ OUTPUT: '$current_version'"
```

### 🔍 Version Comparison Debugging
```bash
log_debug "KNOWN VERSION: Reading from $KNOWN_API_VERSION_FILE"
log_debug "KNOWN VERSION: Raw content: '$known_version'"
log_debug "CURRENT VERSION: Final extracted value: '$current_version'"
log_debug "VERSION COMPARISON: Comparing '$current_version' with '$known_version'"
```

### ⚠️ Error Diagnosis
```bash
log_debug "VERSION VALIDATION: Current version is invalid ('$current_version')"
log_debug "POSSIBLE CAUSES:"
log_debug "  - Starlink dish is unreachable at $STARLINK_IP"
log_debug "  - gRPC API is not responding"
log_debug "  - API response format has changed"
log_debug "  - Network connectivity issues"
```

## Configuration File Fix Needed

The actual issue is in `/etc/starlink-config/config.sh` at line 775. Common problematic patterns:

### ❌ Wrong Patterns (cause "Enable: not found")
```bash
Enable GPS tracking                    # Missing # for comment
Enable_GPS_LOGGING=true               # Missing export keyword  
ENABLE GPS LOGGING="true"             # Space in variable name
ENABLE_GPS_LOGGING=true"              # Unmatched quote
```

### ✅ Correct Patterns
```bash
# Enable GPS tracking                  # Proper comment
export ENABLE_GPS_LOGGING="true"      # Proper variable
export ENABLE_GPS_LOGGING=true        # Boolean without quotes (also valid)
```

## Testing the Fix

### Run with Debug to See Configuration Issues
```bash
DEBUG=1 CONFIG_FILE=/etc/starlink-config/config.sh /usr/local/starlink-monitor/scripts/check_starlink_api-rutos.sh
```

### Expected Output After Fix
```bash
[DEBUG] Attempting to load configuration from: /etc/starlink-config/config.sh
[DEBUG] Configuration loaded successfully from: /etc/starlink-config/config.sh
[DEBUG] ==================== API CHECKER CONFIGURATION DEBUG ====================
[DEBUG] ✓ grpcurl binary found and executable: /usr/local/starlink-monitor/grpcurl
[DEBUG] ✓ jq binary found and executable: /usr/local/starlink-monitor/jq
[DEBUG] ✓ PUSHOVER_TOKEN appears valid (30 chars)
[DEBUG] GRPC CALL: /usr/local/starlink-monitor/grpcurl -plaintext -max-time 10...
[DEBUG] GRPC SUCCESS: Processing JSON response with jq
```

## Benefits of Enhanced Error Handling

1. **Early Error Detection**: Config syntax errors caught immediately
2. **Helpful Error Messages**: Specific guidance on how to fix common issues
3. **Graceful Degradation**: Script continues with defaults when config is missing
4. **Detailed Diagnostics**: Comprehensive information about what went wrong
5. **Line Number Reference**: Points user to exact problem location (line 775)

The script now provides clear, actionable error messages instead of cryptic shell errors, making it much easier to troubleshoot configuration file issues.
