# autonomy API Reference

This document provides a comprehensive reference for all APIs and interfaces provided by the autonomy intelligent multi-interface failover system.

## Table of Contents

1. [ubus RPC Interface](#ubus-rpc-interface)
2. [Command Line Interface](#command-line-interface)
3. [HTTP Endpoints](#http-endpoints)
4. [Configuration Management](#configuration-management)
5. [Data Structures](#data-structures)
6. [Error Handling](#error-handling)
7. [Code Examples](#code-examples)
8. [Integration Examples](#integration-examples)

## ubus RPC Interface

The autonomyd daemon exposes a ubus RPC interface for control and monitoring. All ubus calls use the `autonomy` namespace.

### Authentication

ubus calls require appropriate permissions. The daemon runs with system privileges and can be accessed by:
- Root user
- Users in the `ubus` group
- Users with explicit ubus permissions

### Available Methods

#### `status`

Returns the current status of the daemon and all members.

**Parameters:** None

**Returns:**
```json
{
  "status": "running",
  "uptime": 3600,
  "version": "1.0.0",
  "active_member": "starlink",
  "members": [
    {
      "name": "starlink",
      "class": "starlink",
      "interface": "wan",
      "status": "active",
      "score": 85.5,
      "uptime": 3600
    }
  ]
}
```

**Example:**
```bash
ubus call autonomy status
```

## Command Line Interface

The autonomy command-line interface (`autonomyctl`) provides easy access to all system functions and monitoring capabilities.

### Basic Commands

#### `status`

Display overall system status.

```bash
autonomyctl status
```

**Output:**
```
autonomy Status Report
=====================
Status: Running
Uptime: 2h 15m 30s
Version: 1.0.0
Active Member: starlink
Total Members: 3
Last Failover: 2025-01-20 14:30:00 UTC
```

#### `members`

List all network members and their status.

```bash
# Basic member list
autonomyctl members

# Detailed member information
autonomyctl members --detailed

# Filter by interface type
autonomyctl members --type starlink
```

**Output:**
```
Network Members
===============
starlink (Starlink) - Active - Score: 85.5 - Priority: 100
cellular (Cellular) - Standby - Score: 72.3 - Priority: 80
wifi (WiFi) - Standby - Score: 65.1 - Priority: 60
```

#### `health`

Display system health information.

```bash
# Overall health
autonomyctl health

# Detailed health metrics
autonomyctl health --detailed

# Health history
autonomyctl health --history --hours 24
```

**Output:**
```
System Health Report
===================
CPU Usage: 12.5%
Memory Usage: 45.2%
Disk Usage: 23.1%
Network Health: Good
Service Status: Healthy
```

### Advanced Commands

#### `failover`

Manual failover control.

```bash
# Failover to specific interface
autonomyctl failover --interface starlink

# Force failover (bypass checks)
autonomyctl failover --interface cellular --force

# Test failover (dry run)
autonomyctl failover --interface wifi --test
```

#### `gps`

GPS and location information.

```bash
# Current GPS status
autonomyctl gps

# Detailed GPS information
autonomyctl gps --detailed

# GPS history
autonomyctl gps --history --hours 6

# Movement detection
autonomyctl gps --movement
```

**Output:**
```
GPS Status Report
================
Status: Available
Coordinates: 40.7128°N, 74.0060°W
Accuracy: ±5m
Last Update: 2025-01-20 15:30:00 UTC
Movement: Stationary
```

#### `telemetry`

Telemetry data access.

```bash
# Current telemetry
autonomyctl telemetry

# Export telemetry data
autonomyctl telemetry --export --format json

# Clean old telemetry
autonomyctl telemetry cleanup --older-than 30d
```

#### `notifications`

Notification system management.

```bash
# Notification status
autonomyctl notifications

# Test notifications
autonomyctl notifications test

# Notification history
autonomyctl notifications --history --limit 20
```

### Configuration Commands

#### `config`

Configuration management.

```bash
# Show current configuration
autonomyctl config show

# Validate configuration
autonomyctl config validate

# Export configuration
autonomyctl config export --format uci

# Backup configuration
autonomyctl config backup --file /backup/autonomy.conf
```

#### `optimize`

System optimization.

```bash
# Run optimization
autonomyctl optimize

# Optimization with report
autonomyctl optimize --report

# Aggressive optimization
autonomyctl optimize --aggressive
```

### Monitoring Commands

#### `metrics`

Performance metrics.

```bash
# Current metrics
autonomyctl metrics

# Metrics for specific interface
autonomyctl metrics --interface starlink

# Performance metrics
autonomyctl metrics --performance

# Export metrics
autonomyctl metrics --export --format csv
```

#### `decisions`

Failover decision history.

```bash
# Recent decisions
autonomyctl decisions

# Decision analysis
autonomyctl decisions --analysis

# Decision history
autonomyctl decisions --history --hours 24
```

### Debug Commands

#### `debug`

Debug and diagnostic functions.

```bash
# Enable debug mode
autonomyctl debug --enable

# Disable debug mode
autonomyctl debug --disable

# Debug information
autonomyctl debug --info

# Debug logs
autonomyctl debug --logs --lines 100
```

#### `diagnostics`

System diagnostics.

```bash
# Run diagnostics
autonomyctl diagnostics

# Generate diagnostic report
autonomyctl diagnostics --report --file /tmp/diagnostics.txt

# Quick health check
autonomyctl diagnostics --quick
```

### Global Options

All commands support these global options:

```bash
# Verbose output
autonomyctl [command] --verbose

# Quiet mode
autonomyctl [command] --quiet

# JSON output
autonomyctl [command] --json

# Help
autonomyctl [command] --help
```

### Command Examples

#### Basic Monitoring

```bash
# Check system status
autonomyctl status

# Monitor interfaces
watch -n 5 'autonomyctl members'

# Check health
autonomyctl health --detailed
```

#### Troubleshooting

```bash
# Run diagnostics
autonomyctl diagnostics --report

# Check logs
autonomyctl debug --logs --lines 50

# Test notifications
autonomyctl notifications test
```

#### Configuration Management

```bash
# Backup configuration
autonomyctl config backup

# Validate configuration
autonomyctl config validate

# Export configuration
autonomyctl config export --format json
```

#### `members`

Returns detailed information about all discovered members.

**Parameters:** None

**Returns:**
```json
{
  "members": [
    {
      "name": "starlink",
      "class": "starlink",
      "interface": "wan",
      "enabled": true,
      "priority": 100,
      "created": "2024-01-01T00:00:00Z",
      "state": "eligible",
      "score": 85.5,
      "metrics": {
        "latency": 50.0,
        "loss": 0.1,
        "signal": -70.0,
        "obstruction": 5.0
      }
    }
  ]
}
```

**Example:**
```bash
ubus call autonomy members
```

#### `metrics`

Returns metrics for a specific member or all members.

**Parameters:**
- `member` (optional): Member name to get metrics for

**Returns:**
```json
{
  "metrics": {
    "starlink": {
      "timestamp": "2024-01-01T00:00:00Z",
      "latency": 50.0,
      "loss": 0.1,
      "jitter": 5.0,
      "bandwidth": 100.0,
      "signal": -70.0,
      "obstruction": 5.0,
      "outages": 0,
      "network_type": "4G",
      "operator": "Test Operator",
      "roaming": false,
      "connected": true
    }
  }
}
```

**Example:**
```bash
# Get metrics for all members
ubus call autonomy metrics

# Get metrics for specific member
ubus call autonomy metrics '{"member": "starlink"}'
```

#### `history`

Returns historical data for a member.

**Parameters:**
- `member`: Member name
- `limit` (optional): Number of samples to return (default: 100)
- `hours` (optional): Hours of history to return (default: 24)

**Returns:**
```json
{
  "member": "starlink",
  "samples": [
    {
      "timestamp": "2024-01-01T00:00:00Z",
      "metrics": {
        "latency": 50.0,
        "loss": 0.1,
        "signal": -70.0
      },
      "score": {
        "instant": 85.0,
        "ewma": 82.0,
        "window_average": 80.0,
        "final": 83.0,
        "trend": "stable",
        "confidence": 0.9
      }
    }
  ]
}
```

**Example:**
```bash
ubus call autonomy history '{"member": "starlink", "limit": 50, "hours": 12}'
```

#### `events`

Returns system events.

**Parameters:**
- `limit` (optional): Number of events to return (default: 100)
- `hours` (optional): Hours of history to return (default: 24)
- `type` (optional): Filter by event type

**Returns:**
```json
{
  "events": [
    {
      "timestamp": "2024-01-01T00:00:00Z",
      "type": "switch",
      "member": "starlink",
      "message": "Switched to starlink",
      "data": {
        "reason": "score_improvement",
        "previous_member": "cellular"
      }
    }
  ]
}
```

**Example:**
```bash
# Get all events
ubus call autonomy events

# Get switch events only
ubus call autonomy events '{"type": "switch", "limit": 10}'
```

#### `failover`

Manually triggers a failover to a specific member.

**Parameters:**
- `member`: Member name to switch to
- `reason` (optional): Reason for the switch

**Returns:**
```json
{
  "success": true,
  "message": "Switched to starlink",
  "previous_member": "cellular"
}
```

**Example:**
```bash
ubus call autonomy failover '{"member": "starlink", "reason": "manual"}'
```

#### `restore`

Restores automatic failover mode.

**Parameters:** None

**Returns:**
```json
{
  "success": true,
  "message": "Automatic failover restored"
}
```

**Example:**
```bash
ubus call autonomy restore
```

#### `recheck`

Forces a recheck of all members.

**Parameters:** None

**Returns:**
```json
{
  "success": true,
  "message": "Recheck completed",
  "members_checked": 3
}
```

**Example:**
```bash
ubus call autonomy recheck
```

#### `setlog`

Sets the log level.

**Parameters:**
- `level`: Log level (debug, info, warn, error)

**Returns:**
```json
{
  "success": true,
  "message": "Log level set to debug",
  "previous_level": "info"
}
```

**Example:**
```bash
ubus call autonomy setlog '{"level": "debug"}'
```

#### `config`

Returns the current configuration.

**Parameters:** None

**Returns:**
```json
{
  "config": {
    "log_level": "info",
    "poll_interval_ms": 1000,
    "decision_interval_ms": 5000,
    "retention_hours": 24,
    "max_ram_mb": 50,
    "predictive": false,
    "use_mwan3": true,
    "members": {
      "starlink": {
        "class": "starlink",
        "interface": "wan",
        "enabled": true,
        "priority": 100
      }
    }
  }
}
```

**Example:**
```bash
ubus call autonomy config
```

#### `info`

Returns detailed system information.

**Parameters:** None

**Returns:**
```json
{
  "info": {
    "version": "1.0.0",
    "go_version": "1.22",
    "uptime": 3600,
    "start_time": "2024-01-01T00:00:00Z",
    "memory_usage": {
      "alloc_bytes": 1048576,
      "sys_bytes": 2097152,
      "heap_alloc_bytes": 524288
    },
    "statistics": {
      "total_switches": 5,
      "decision_cycles": 1000,
      "collection_errors": 2
    }
  }
}
```

**Example:**
```bash
ubus call autonomy info
```

## GPS Methods

### Get GPS Location

Returns the current GPS location from the best available source.

```bash
ubus call autonomy gps
```

**Response:**
```json
{
  "latitude": 40.7128,
  "longitude": -74.0060,
  "accuracy": 5.2,
  "fix_status": "2",
  "datetime": "2025-01-15T12:34:56Z",
  "source": "google",
  "available": true
}
```

**Fields:**
- `latitude`: Latitude in decimal degrees
- `longitude`: Longitude in decimal degrees  
- `accuracy`: Accuracy in meters
- `fix_status`: GPS fix status ("0"=no fix, "1"=2D fix, "2"=3D fix)
- `datetime`: UTC timestamp of the location
- `source`: Source of the location data ("rutos", "starlink", "google")
- `available`: Whether GPS data is available

### Get GPS Status

Returns comprehensive GPS status and statistics.

```bash
ubus call autonomy gps_status
```

**Response:**
```json
{
  "enabled": true,
  "sources": ["rutos", "starlink", "google"],
  "active_source": "google",
  "last_update": "2025-01-15T12:34:56Z",
  "stats": {
    "total_requests": 1250,
    "cache_hits": 890,
    "api_calls_today": 45,
    "successful_queries": 1200,
    "failed_queries": 50,
    "environment_changes": 12,
    "debounced_changes": 8,
    "verified_changes": 10,
    "fallbacks_to_cache": 15,
    "quality_rejections": 25,
    "accepted_locations": 1180,
    "big_move_acceptances": 5,
    "stationary_detections": 95,
    "average_response_time": "2.5s",
    "last_reset_date": "2025-01-01T00:00:00Z"
  }
}
```

### Get GPS Statistics

Returns detailed GPS performance statistics.

```bash
ubus call autonomy gps_stats
```

**Response:**
```json
{
  "total_requests": 1250,
  "cache_hits": 890,
  "api_calls_today": 45,
  "successful_queries": 1200,
  "failed_queries": 50,
  "environment_changes": 12,
  "debounced_changes": 8,
  "verified_changes": 10,
  "fallbacks_to_cache": 15,
  "quality_rejections": 25,
  "accepted_locations": 1180,
  "big_move_acceptances": 5,
  "stationary_detections": 95,
  "average_response_time": "2.5s",
  "last_reset_date": "2025-01-01T00:00:00Z"
}
```

## Data Usage Methods

## HTTP Endpoints

The daemon provides HTTP endpoints for metrics and health monitoring.

### Metrics Endpoint

**URL:** `http://localhost:9090/metrics`

**Method:** GET

**Description:** Returns Prometheus-formatted metrics

**Example:**
```bash
curl http://localhost:9090/metrics
```

**Sample Output:**
```
# HELP autonomy_member_score Current health score for each member
# TYPE autonomy_member_score gauge
autonomy_member_score{member="starlink",class="starlink",interface="wan"} 85.5

# HELP autonomy_member_latency_ms Current latency for each member in milliseconds
# TYPE autonomy_member_latency_ms gauge
autonomy_member_latency_ms{member="starlink",class="starlink",interface="wan"} 50.0
```

### Health Endpoints

#### Basic Health Check

**URL:** `http://localhost:8080/health`

**Method:** GET

**Description:** Returns basic health status

**Example:**
```bash
curl http://localhost:8080/health
```

**Response:**
```json
{
  "status": "healthy",
  "timestamp": "2024-01-01T00:00:00Z",
  "uptime": 3600,
  "version": "1.0.0"
}
```

#### Detailed Health Check

**URL:** `http://localhost:8080/health/detailed`

**Method:** GET

**Description:** Returns detailed health information

**Example:**
```bash
curl http://localhost:8080/health/detailed
```

**Response:**
```json
{
  "status": "healthy",
  "timestamp": "2024-01-01T00:00:00Z",
  "uptime": 3600,
  "version": "1.0.0",
  "components": {
    "controller": {
      "status": "healthy",
      "message": "Controller is operational",
      "last_check": "2024-01-01T00:00:00Z",
      "uptime": 3600
    }
  },
  "members": [
    {
      "name": "starlink",
      "class": "starlink",
      "interface": "wan",
      "status": "excellent",
      "state": "eligible",
      "score": 85.5,
      "active": true,
      "last_seen": "2024-01-01T00:00:00Z",
      "uptime": 3600
    }
  ],
  "statistics": {
    "total_members": 3,
    "active_members": 1,
    "total_switches": 5,
    "total_samples": 1000,
    "total_events": 50
  },
  "memory": {
    "alloc_bytes": 1048576,
    "sys_bytes": 2097152,
    "heap_alloc_bytes": 524288
  }
}
```

#### Readiness Check

**URL:** `http://localhost:8080/health/ready`

**Method:** GET

**Description:** Returns readiness status for load balancers

**Example:**
```bash
curl http://localhost:8080/health/ready
```

**Response:**
```json
{"status":"ready"}
```

#### Liveness Check

**URL:** `http://localhost:8080/health/live`

**Method:** GET

**Description:** Returns liveness status for container orchestration

**Example:**
```bash
curl http://localhost:8080/health/live
```

**Response:**
```json
{"status":"alive"}
```

## Configuration

### UCI Configuration

The daemon uses UCI for configuration management. The configuration is stored in `/etc/config/autonomy`.

#### Main Configuration

```bash
# Set log level
uci set autonomy.main.log_level=debug

# Set poll interval
uci set autonomy.main.poll_interval_ms=1000

# Set decision interval
uci set autonomy.main.decision_interval_ms=5000

# Set retention period
uci set autonomy.main.retention_hours=24

# Set memory limit
uci set autonomy.main.max_ram_mb=50

# Enable predictive failover
uci set autonomy.main.predictive=1

# Use mwan3 for interface control
uci set autonomy.main.use_mwan3=1

# Enable metrics server
uci set autonomy.main.metrics_listener=1
uci set autonomy.main.metrics_port=9090

# Enable health server
uci set autonomy.main.health_listener=1
uci set autonomy.main.health_port=8080

# Commit changes
uci commit autonomy
```

#### Member Configuration

```bash
# Configure Starlink member
uci set autonomy.starlink=member
uci set autonomy.starlink.class=starlink
uci set autonomy.starlink.interface=wan
uci set autonomy.starlink.enabled=1
uci set autonomy.starlink.priority=100

# Configure cellular member
uci set autonomy.cellular=member
uci set autonomy.cellular.class=cellular
uci set autonomy.cellular.interface=wwan0
uci set autonomy.cellular.enabled=1
uci set autonomy.cellular.priority=80

# Configure WiFi member
uci set autonomy.wifi=member
uci set autonomy.wifi.class=wifi
uci set autonomy.wifi.interface=wlan0
uci set autonomy.wifi.enabled=1
uci set autonomy.wifi.priority=60

# Commit changes
uci commit autonomy
```

### MQTT Configuration

```bash
# Enable MQTT
uci set autonomy.mqtt.enabled=1

# Set broker
uci set autonomy.mqtt.broker=localhost
uci set autonomy.mqtt.port=1883

# Set credentials
uci set autonomy.mqtt.username=autonomy
uci set autonomy.mqtt.password=password

# Set topic prefix
uci set autonomy.mqtt.topic_prefix=autonomy

# Set QoS
uci set autonomy.mqtt.qos=1

# Commit changes
uci commit autonomy
```

## Data Structures

### Member

```go
type Member struct {
    Name      string    `json:"name"`
    Interface string    `json:"interface"`
    Class     string    `json:"class"`
    Enabled   bool      `json:"enabled"`
    Priority  int       `json:"priority"`
    Created   time.Time `json:"created"`
}
```

### Metrics

```go
type Metrics struct {
    Timestamp     time.Time `json:"timestamp"`
    Latency       float64   `json:"latency"`
    Loss          float64   `json:"loss"`
    Jitter        float64   `json:"jitter"`
    Bandwidth     float64   `json:"bandwidth"`
    Signal        float64   `json:"signal"`
    Obstruction   float64   `json:"obstruction"`
    Outages       int       `json:"outages"`
    NetworkType   string    `json:"network_type"`
    Operator      string    `json:"operator"`
    Roaming       bool      `json:"roaming"`
    Connected     bool      `json:"connected"`
    LastSeen      time.Time `json:"last_seen"`
}
```

### Score

```go
type Score struct {
    Timestamp     time.Time `json:"timestamp"`
    Instant       float64   `json:"instant"`
    EWMA          float64   `json:"ewma"`
    WindowAverage float64   `json:"window_average"`
    Final         float64   `json:"final"`
    Trend         string    `json:"trend"`
    Confidence    float64   `json:"confidence"`
}
```

### Event

```go
type Event struct {
    Timestamp time.Time              `json:"timestamp"`
    Type      string                 `json:"type"`
    Member    string                 `json:"member"`
    Message   string                 `json:"message"`
    Data      map[string]interface{} `json:"data"`
}
```

## Error Handling

### ubus Error Responses

When a ubus call fails, it returns an error response:

```json
{
  "error": "Invalid member name",
  "code": 400
}
```

Common error codes:
- `400`: Bad Request - Invalid parameters
- `404`: Not Found - Member or resource not found
- `500`: Internal Server Error - Daemon error
- `503`: Service Unavailable - Daemon not ready

### HTTP Error Responses

HTTP endpoints return standard HTTP status codes:

- `200`: Success
- `400`: Bad Request
- `404`: Not Found
- `500`: Internal Server Error
- `503`: Service Unavailable

Error responses include a JSON body:

```json
{
  "error": "Service temporarily unavailable",
  "code": 503,
  "timestamp": "2024-01-01T00:00:00Z"
}
```

## Examples

### Complete Monitoring Script

```bash
#!/bin/bash

# Check daemon status
status=$(ubus call autonomy status)
echo "Daemon Status: $status"

# Get active member
active_member=$(echo "$status" | jq -r '.active_member')
echo "Active Member: $active_member"

# Get metrics for active member
metrics=$(ubus call autonomy metrics "{\"member\": \"$active_member\"}")
echo "Metrics: $metrics"

# Check health endpoint
health=$(curl -s http://localhost:8080/health)
echo "Health: $health"

# Get Prometheus metrics
prometheus_metrics=$(curl -s http://localhost:9090/metrics)
echo "Prometheus Metrics: $prometheus_metrics"
```

### Automated Failover Script

```bash
#!/bin/bash

# Check if current member is healthy
current_metrics=$(ubus call autonomy metrics)
current_score=$(echo "$current_metrics" | jq -r '.metrics.starlink.score.final')

if (( $(echo "$current_score < 50" | bc -l) )); then
    echo "Current member score is low ($current_score), checking alternatives..."
    
    # Get all members
    members=$(ubus call autonomy members)
    
    # Find best alternative
    best_member=$(echo "$members" | jq -r '.members[] | select(.enabled and .score > 70) | .name' | head -1)
    
    if [ -n "$best_member" ]; then
        echo "Switching to $best_member"
        ubus call autonomy failover "{\"member\": \"$best_member\", \"reason\": \"low_score\"}"
    else
        echo "No suitable alternative found"
    fi
fi
```

### MQTT Integration Example

```python
import paho.mqtt.client as mqtt
import json

def on_connect(client, userdata, flags, rc):
    print("Connected to MQTT broker")
    client.subscribe("autonomy/+/sample")
    client.subscribe("autonomy/events/+")

def on_message(client, userdata, msg):
    data = json.loads(msg.payload.decode())
    print(f"Received {msg.topic}: {data}")

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect("localhost", 1883, 60)
client.loop_forever()
```

### Prometheus Alerting Rules

```yaml
groups:
  - name: autonomy
    rules:
      - alert: autonomyMemberDown
        expr: autonomy_member_status == 0
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "Member {{ $labels.member }} is down"
          description: "Member {{ $labels.member }} has been down for more than 5 minutes"

      - alert: autonomyLowScore
        expr: autonomy_member_score < 50
        for: 2m
        labels:
          severity: warning
        annotations:
          summary: "Member {{ $labels.member }} has low score"
          description: "Member {{ $labels.member }} score is {{ $value }}"

      - alert: autonomyHighLatency
        expr: autonomy_member_latency_ms > 200
        for: 1m
        labels:
          severity: warning
        annotations:
          summary: "Member {{ $labels.member }} has high latency"
          description: "Member {{ $labels.member }} latency is {{ $value }}ms"
```

This API reference provides comprehensive documentation for integrating with the autonomyd daemon. For additional examples and use cases, refer to the deployment guide and troubleshooting documentation.
