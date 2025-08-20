# Smart Centralized Error Logging System

## Overview

The RUTOS Starlink Failover system now includes intelligent centralized error logging that automatically adapts based on the deployment context:

- **Bootstrap Mode**: Always enabled during installation (when no config exists)
- **Config-Controlled Mode**: Enabled/disabled based on configuration after installation

## How It Works

### 1. Bootstrap Mode (Installation Phase)

When no configuration file exists, the system automatically enables centralized error logging:

```bash
# During installation - no config file exists yet
curl -fsSL https://raw.githubusercontent.com/.../bootstrap-deploy-v3-rutos.sh | sh

# Result: ALL errors are captured to /tmp/rutos-autonomous-errors.log
# This ensures installation problems are captured for troubleshooting
```

**Benefits:**
- Installation errors are always captured regardless of user configuration
- Autonomous deployment systems can monitor and react to installation failures
- No user configuration required for error capture during critical setup phase

### 2. Config-Controlled Mode (Post-Installation)

After installation, centralized error logging is controlled by the configuration:

```bash
# In config.sh - enable autonomous error logging
export ENABLE_AUTONOMOUS_ERROR_LOGGING="true"

# In config.sh - disable autonomous error logging (default)
export ENABLE_AUTONOMOUS_ERROR_LOGGING="false"
# or simply omit the setting
```

**Benefits:**
- Users control whether they want centralized error logging after installation
- Manual installations can disable it to reduce overhead
- Autonomous systems can enable it for self-healing capabilities

### 3. Override Mode (Explicit Control)

Environment variable always takes precedence:

```bash
# Force enable regardless of config
export ENABLE_CENTRALIZED_ERROR_LOGGING="true"

# Force disable regardless of mode/config
export ENABLE_CENTRALIZED_ERROR_LOGGING="false"
```

## Priority Order

The system checks settings in this order:

1. **Explicit Override**: `ENABLE_CENTRALIZED_ERROR_LOGGING` environment variable
2. **Bootstrap Mode**: No config file exists → AUTO-ENABLE
3. **Config Setting**: `ENABLE_AUTONOMOUS_ERROR_LOGGING` in config file
4. **Default**: DISABLED after installation

## Error Log Locations

### Centralized Error Log
- **Location**: `/tmp/rutos-autonomous-errors.log`
- **Purpose**: Comprehensive error capture with full context
- **Format**: Structured entries with environment details, stack traces, error categorization
- **Rotation**: Automatic when file exceeds 10MB (keeps 5 backups)

### Traditional Error Log
- **Location**: Standard stderr/syslog
- **Purpose**: Basic error messages for immediate troubleshooting
- **Format**: Simple error messages with timestamps

## Integration with Autonomous System

When centralized error logging is enabled, errors are captured for:

1. **GitHub Issue Creation**: `autonomous-error-monitor-rutos.sh` processes errors
2. **Self-Healing**: Issues are automatically assigned to GitHub Copilot
3. **Trend Analysis**: Error patterns are tracked for system improvements
4. **Proactive Monitoring**: Critical errors trigger immediate notifications

## Configuration Examples

### Autonomous Deployment System
```bash
# config.sh for autonomous systems
export ENABLE_AUTONOMOUS_ERROR_LOGGING="true"  # Enable centralized logging
export ENABLE_ENHANCED_METRICS="true"          # Enhanced monitoring
export ENABLE_STATISTICAL_AGGREGATION="true"  # Long-term analytics
```

### Manual/Basic Installation
```bash
# config.sh for manual management
export ENABLE_AUTONOMOUS_ERROR_LOGGING="false" # Disable centralized logging
export ENABLE_ENHANCED_METRICS="false"         # Basic monitoring only
```

### Hybrid Approach
```bash
# config.sh for selective automation
export ENABLE_AUTONOMOUS_ERROR_LOGGING="true"  # Capture errors
export ENABLE_ENHANCED_METRICS="false"         # But keep monitoring simple
```

## Testing the System

### Test Bootstrap Mode
```bash
# This should show centralized logging ENABLED (no config exists)
./test-autonomous-logging-rutos.sh
```

### Test Config-Controlled Mode
```bash
# This demonstrates config-controlled behavior
./test-config-controlled-logging-rutos.sh
```

### Test Installation with Centralized Logging
```bash
# Installation with full debug logging
curl -fsSL https://raw.githubusercontent.com/.../bootstrap-deploy-v3-rutos.sh | \
  DEBUG=1 RUTOS_TEST_MODE=1 sh

# Check captured errors
cat /tmp/rutos-autonomous-errors.log
```

## Error Severity Levels

The system categorizes errors for appropriate handling:

- **CRITICAL**: System failures, deployment failures
- **HIGH**: Feature failures, significant operational issues
- **MEDIUM**: Warnings that might indicate problems
- **LOW**: Informational issues, minor inconsistencies

## API for Scripts

Scripts can use centralized error logging through simple functions:

```bash
# Capture errors with specific severity
autonomous_error "CRITICAL" "Database connection failed" "script.sh" "42" "connect_db"
autonomous_error "HIGH" "Network timeout occurred"
autonomous_error "MEDIUM" "Configuration value missing"

# Enhanced error capture functions
capture_critical_error "System critical failure"
capture_high_error "Feature malfunction"
capture_warning "Potential issue detected"

# Check if centralized logging is available
if autonomous_logging_available; then
    echo "Centralized error logging is active"
fi

# Show current status
autonomous_logging_status
```

## Benefits Summary

1. **Smart Behavior**: Automatically adapts to deployment context
2. **Zero Configuration**: Works out of the box during installation
3. **User Control**: Can be disabled for simple/manual installations
4. **Autonomous Ready**: Enables self-healing systems when enabled
5. **Backward Compatible**: Doesn't break existing installations
6. **Performance Conscious**: Only active when needed/wanted
