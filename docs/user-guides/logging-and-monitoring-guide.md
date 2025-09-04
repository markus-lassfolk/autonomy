# autonomy Logging and Monitoring Guide

This document describes the comprehensive logging system and monitoring capabilities of the autonomy daemon.

## Overview

autonomy provides extensive logging capabilities with multiple levels of verbosity to help with troubleshooting and monitoring. The logging system is designed to provide:

- **Structured JSON logging** for easy parsing and analysis
- **Multiple log levels** from error-only to full trace logging
- **Component-based logging** with context fields
- **Performance monitoring** with timing information
- **System call logging** for debugging external commands
- **API call logging** for network troubleshooting
- **State change tracking** for understanding system behavior

## Log Levels

### Available Log Levels

1. **error** - Only error messages (minimal output)
2. **warn** - Warnings and errors
3. **info** - General information, warnings, and errors
4. **debug** - Detailed debugging information
5. **trace** - Most verbose level, includes all details

### Log Level Usage

```bash
# Run with specific log level
autonomyd --log-level=debug

# Run with verbose logging (equivalent to trace)
autonomyd --verbose

# Run in monitoring mode (trace level with extra context)
autonomyd --monitor
```

## Monitoring Mode

### What is Monitoring Mode?

Monitoring mode provides the most comprehensive logging for troubleshooting and development. It includes:

- **Trace-level logging** for all operations
- **Performance timing** for all major operations
- **Resource usage tracking** (memory, CPU, goroutines)
- **System call logging** with full command details
- **API call logging** with response times
- **State change tracking** with full context
- **Data flow logging** between components

### Running in Monitoring Mode

#### Using the Monitoring Script

```bash
# Run main daemon in monitoring mode
./scripts/monitor.sh

# Run system management in monitoring mode
./scripts/monitor.sh -s

# Run with custom configuration
./scripts/monitor.sh -c /tmp/test.conf -l trace

# Run with profiling enabled
./scripts/monitor.sh -p
```

#### Direct Command Line

```bash
# Main daemon monitoring mode
autonomyd --monitor --foreground --log-level=trace

# System management monitoring mode
autonomysysmgmt --monitor --foreground --log-level=trace
```

## Log Message Structure

### JSON Format

All log messages are in structured JSON format for easy parsing:

```json
{
  "ts": "2025-01-27T10:30:45.123Z",
  "level": "info",
  "msg": "member_discovered",
  "component": "autonomyd",
  "member": "wan_starlink",
  "class": "starlink",
  "iface": "wan_starlink",
  "policy": "wan_starlink_m1"
}
```

### Common Fields

- **ts** - Timestamp in RFC3339 format
- **level** - Log level (error, warn, info, debug, trace)
- **msg** - Log message type
- **component** - Component name (autonomyd, autonomysysmgmt)
- **error** - Error message (when applicable)
- **duration_ms** - Operation duration in milliseconds
- **operation** - Operation being performed

## Log Message Types

### Core System Messages

#### Startup and Shutdown
```json
{"msg": "startup", "version": "1.0.0", "config_path": "/etc/config/autonomy"}
{"msg": "shutdown", "reason": "SIGTERM", "uptime": "2h30m15s"}
```

#### Configuration
```json
{"msg": "configuration", "action": "loaded", "config_path": "/etc/config/autonomy", "valid": true}
{"msg": "configuration", "action": "reloaded", "source": "SIGHUP", "changes": 3}
```

### Member Discovery

#### Member Discovery
```json
{"msg": "discovery", "member": "wan_starlink", "class": "starlink", "iface": "wan_starlink"}
{"msg": "discovery", "member": "wan_cell", "class": "cellular", "iface": "wwan0"}
```

#### Member State Changes
```json
{"msg": "state_change", "component": "member", "from_state": "unknown", "to_state": "eligible", "reason": "discovered"}
```

### Metrics Collection

#### Sample Collection
```json
{"msg": "sample", "member": "wan_starlink", "lat_ms": 53.2, "loss_pct": 0.1, "obstruction_pct": 2.1}
{"msg": "sample", "member": "wan_cell", "lat_ms": 89.5, "loss_pct": 1.2, "rsrp": -95, "rsrq": -9}
```

#### Provider Selection
```json
{"msg": "provider", "member": "wan_cell", "provider": "rutos.mobiled", "status": "connected"}
```

### Decision Making

#### Decision Evaluation
```json
{"msg": "decision", "decision_type": "evaluation", "from": "wan_starlink", "to": "wan_cell", "reason": "predictive", "delta": 12.4}
```

#### Failover Events
```json
{"msg": "switch", "from": "wan_starlink", "to": "wan_cell", "reason": "predictive", "delta": 12.4, "fail_window_s": 11}
```

### Performance Monitoring

#### Timing Information
```json
{"msg": "timing", "operation": "member_discovery", "duration_ms": 45, "member_count": 4}
{"msg": "timing", "operation": "decision_engine_tick", "duration_ms": 12}
```

#### Resource Usage
```json
{"msg": "resource_usage", "resource_type": "memory", "usage": 25.4, "limit": 100.0, "unit": "MB", "usage_pct": 25.4}
{"msg": "resource_usage", "resource_type": "goroutines", "usage": 45, "limit": 500, "unit": "count", "usage_pct": 9.0}
```

### System Calls

#### External Commands
```json
{"msg": "system_call", "command": "ubus", "args": ["call", "network.device", "status"], "exit_code": 0, "duration_ms": 15}
{"msg": "system_call", "command": "ping", "args": ["-c", "1", "8.8.8.8"], "exit_code": 0, "duration_ms": 120}
```

### API Calls

#### Starlink API
```json
{"msg": "api_call", "method": "GET", "url": "http://192.168.100.1/api/v1/diagnostics", "status_code": 200, "response_time_ms": 45}
```

#### UCI Commands
```json
{"msg": "api_call", "method": "UCI", "url": "get", "status_code": 0, "response_time_ms": 5}
```

## Monitoring Tips

### Key Log Patterns to Watch

1. **Member Discovery**
   ```
   {"msg": "discovery", "member": "..."}
   ```

2. **Metrics Collection**
   ```
   {"msg": "sample", "member": "..."}
   ```

3. **Decision Making**
   ```
   {"msg": "decision", "decision_type": "..."}
   {"msg": "switch", "from": "...", "to": "..."}
   ```

4. **Performance Issues**
   ```
   {"msg": "timing", "duration_ms": >100}
   {"msg": "resource_usage", "usage_pct": >80}
   ```

5. **Errors and Failures**
   ```
   {"level": "error", ...}
   {"level": "warn", ...}
   ```

### Useful Commands While Monitoring

```bash
# Watch logs in real-time
tail -f /var/log/messages | grep autonomy

# Filter by log level
tail -f /var/log/messages | grep '"level":"error"'

# Filter by specific member
tail -f /var/log/messages | grep '"member":"wan_starlink"'

# Filter by operation type
tail -f /var/log/messages | grep '"msg":"switch"'

# Check current status via ubus
ubus call autonomy status

# List discovered members
ubus call autonomy members

# Show recent events
ubus call autonomy events
```

### Performance Monitoring

#### Memory Usage
```bash
# Watch memory usage
tail -f /var/log/messages | grep '"msg":"resource_usage"'

# Check for memory pressure
tail -f /var/log/messages | grep '"usage_pct":>80'
```

#### Timing Analysis
```bash
# Watch for slow operations
tail -f /var/log/messages | grep '"duration_ms":>100'

# Monitor decision engine performance
tail -f /var/log/messages | grep '"operation":"decision_engine_tick"'
```

## Troubleshooting with Logs

### Common Issues and Log Patterns

#### 1. Member Not Discovered
```
{"level": "warn", "msg": "discovery", "error": "interface not found"}
```

#### 2. API Connection Issues
```
{"level": "error", "msg": "api_call", "status_code": 500}
{"level": "error", "msg": "api_call", "error": "connection refused"}
```

#### 3. High Latency
```
{"msg": "sample", "lat_ms": >1000}
{"msg": "timing", "duration_ms": >500}
```

#### 4. Memory Issues
```
{"level": "warn", "msg": "resource_usage", "usage_pct": >90}
{"msg": "memory", "action": "force_gc"}
```

#### 5. Decision Engine Problems
```
{"level": "error", "msg": "decision", "error": "..."}
{"msg": "throttle", "what": "predictive", "cooldown_s": 20}
```

### Debugging Workflow

1. **Start with monitoring mode**
   ```bash
   ./scripts/monitor.sh
   ```

2. **Reproduce the issue** while monitoring

3. **Look for error patterns**
   ```bash
   tail -f /var/log/messages | grep '"level":"error"'
   ```

4. **Check timing information**
   ```bash
   tail -f /var/log/messages | grep '"msg":"timing"'
   ```

5. **Verify member discovery**
   ```bash
   tail -f /var/log/messages | grep '"msg":"discovery"'
   ```

6. **Check decision logic**
   ```bash
   tail -f /var/log/messages | grep '"msg":"decision"'
   ```

## Log Configuration

### UCI Configuration

```uci
config autonomy 'main'
    option log_level 'info'           # debug|info|warn|error|trace
    option log_file ''                # Empty for syslog only
    option metrics_listener '0'       # Enable metrics endpoint
    option health_listener '1'        # Enable health endpoint
```

### Environment Variables

```bash
export autonomy_MONITOR_MODE=true
export autonomy_VERBOSE_LOGGING=true
export autonomy_LOG_LEVEL=trace
```

## Log Analysis Tools

### JSON Log Processing

```bash
# Extract specific fields
jq '.member, .lat_ms' /var/log/messages

# Filter by log level
jq 'select(.level == "error")' /var/log/messages

# Calculate average latency
jq '[.lat_ms] | add / length' /var/log/messages

# Count events by type
jq 'group_by(.msg) | map({msg: .[0].msg, count: length})' /var/log/messages
```

### Real-time Monitoring

```bash
# Monitor specific member
tail -f /var/log/messages | jq 'select(.member == "wan_starlink")'

# Monitor performance
tail -f /var/log/messages | jq 'select(.msg == "timing")'

# Monitor errors
tail -f /var/log/messages | jq 'select(.level == "error")'
```

## Best Practices

1. **Use monitoring mode for troubleshooting**
   - Provides the most detailed information
   - Helps identify root causes quickly

2. **Start with info level in production**
   - Balances information with performance
   - Switch to debug/trace when needed

3. **Monitor resource usage**
   - Watch memory and CPU usage
   - Check for memory leaks or high CPU

4. **Use structured logging**
   - JSON format enables easy parsing
   - Consistent field names across messages

5. **Set up log rotation**
   - Prevent log files from growing too large
   - Archive logs for historical analysis

6. **Monitor key metrics**
   - Decision engine performance
   - API response times
   - Member discovery success rate

## Conclusion

The autonomy logging system provides comprehensive visibility into all aspects of the daemon's operation. By using monitoring mode and understanding the log message structure, you can quickly identify and resolve issues, optimize performance, and ensure reliable operation of your multi-interface failover system.
