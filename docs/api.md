# API Reference

## Overview

Autonomy provides a comprehensive API through ubus for monitoring, configuration, and control. The API is designed for integration with OpenWrt/RUTOS systems.

## API Endpoints

### Core Status Endpoints

#### `autonomy.status`
Get overall system status and health information.

**Request:**
```json
{}
```

**Response:**
```json
{
  "status": "healthy",
  "uptime": 3600,
  "version": "1.0.0",
  "interfaces": {
    "starlink": {
      "status": "active",
      "signal_quality": 85,
      "latency": 25,
      "throughput": 150
    },
    "cellular": {
      "status": "standby",
      "signal_strength": -75,
      "data_usage": 1024
    }
  },
  "gps": {
    "status": "active",
    "latitude": 40.7128,
    "longitude": -74.0060,
    "accuracy": 5
  }
}
```

#### `autonomy.interfaces`
Get detailed information about all network interfaces.

**Request:**
```json
{}
```

**Response:**
```json
{
  "interfaces": [
    {
      "name": "starlink",
      "type": "starlink",
      "status": "active",
      "primary": true,
      "metrics": {
        "signal_quality": 85,
        "latency": 25,
        "throughput": 150,
        "obstructions": 0.1
      }
    },
    {
      "name": "wwan0",
      "type": "cellular",
      "status": "standby",
      "primary": false,
      "metrics": {
        "signal_strength": -75,
        "rsrp": -85,
        "rsrq": -10,
        "sinr": 15
      }
    }
  ]
}
```

### Configuration Endpoints

#### `autonomy.config.get`
Get current configuration values.

**Request:**
```json
{
  "section": "starlink"
}
```

**Response:**
```json
{
  "enabled": true,
  "api_endpoint": "grpc://192.168.100.1:9200",
  "health_check_interval": 30,
  "obstruction_threshold": 5
}
```

#### `autonomy.config.set`
Set configuration values.

**Request:**
```json
{
  "section": "starlink",
  "values": {
    "enabled": true,
    "obstruction_threshold": 10
  }
}
```

**Response:**
```json
{
  "success": true,
  "message": "Configuration updated successfully"
}
```

### Control Endpoints

#### `autonomy.control.switch`
Manually switch to a specific interface.

**Request:**
```json
{
  "interface": "cellular",
  "reason": "manual_switch"
}
```

**Response:**
```json
{
  "success": true,
  "message": "Switched to cellular interface",
  "previous_interface": "starlink",
  "new_interface": "cellular"
}
```

#### `autonomy.control.restart`
Restart the autonomy service.

**Request:**
```json
{}
```

**Response:**
```json
{
  "success": true,
  "message": "Service restarting"
}
```

### Monitoring Endpoints

#### `autonomy.metrics`
Get performance metrics and statistics.

**Request:**
```json
{
  "duration": "1h"
}
```

**Response:**
```json
{
  "period": "1h",
  "failover_count": 2,
  "average_latency": 30,
  "total_data_transferred": 2048,
  "interface_usage": {
    "starlink": 0.8,
    "cellular": 0.2
  },
  "performance": {
    "cpu_usage": 15,
    "memory_usage": 25,
    "disk_usage": 10
  }
}
```

#### `autonomy.logs`
Get recent log entries.

**Request:**
```json
{
  "level": "info",
  "limit": 50
}
```

**Response:**
```json
{
  "logs": [
    {
      "timestamp": "2024-01-15T10:30:00Z",
      "level": "info",
      "message": "Interface switch: starlink -> cellular",
      "source": "failover"
    },
    {
      "timestamp": "2024-01-15T10:29:00Z",
      "level": "warn",
      "message": "Starlink obstruction detected: 15%",
      "source": "starlink"
    }
  ]
}
```

### GPS Endpoints

#### `autonomy.gps.location`
Get current GPS location.

**Request:**
```json
{}
```

**Response:**
```json
{
  "latitude": 40.7128,
  "longitude": -74.0060,
  "accuracy": 5,
  "timestamp": "2024-01-15T10:30:00Z",
  "source": "starlink"
}
```

#### `autonomy.gps.history`
Get GPS location history.

**Request:**
```json
{
  "duration": "24h",
  "limit": 100
}
```

**Response:**
```json
{
  "locations": [
    {
      "latitude": 40.7128,
      "longitude": -74.0060,
      "accuracy": 5,
      "timestamp": "2024-01-15T10:30:00Z"
    }
  ]
}
```

### Notification Endpoints

#### `autonomy.notifications.send`
Send a notification.

**Request:**
```json
{
  "type": "failover",
  "message": "Switched to cellular due to Starlink obstruction",
  "priority": "high"
}
```

**Response:**
```json
{
  "success": true,
  "message": "Notification sent successfully",
  "notification_id": "12345"
}
```

#### `autonomy.notifications.status`
Get notification status and history.

**Request:**
```json
{}
```

**Response:**
```json
{
  "enabled": true,
  "last_sent": "2024-01-15T10:30:00Z",
  "total_sent": 15,
  "rate_limited": false,
  "channels": {
    "webhook": true,
    "email": false,
    "sms": false
  }
}
```

## Error Handling

All API endpoints return consistent error responses:

```json
{
  "error": true,
  "code": "INVALID_PARAMETER",
  "message": "Invalid parameter: interface",
  "details": {
    "parameter": "interface",
    "value": "invalid_interface"
  }
}
```

### Common Error Codes

- `INVALID_PARAMETER`: Invalid parameter value
- `INTERFACE_NOT_FOUND`: Specified interface not found
- `SERVICE_UNAVAILABLE`: Service temporarily unavailable
- `PERMISSION_DENIED`: Insufficient permissions
- `CONFIGURATION_ERROR`: Configuration error

## Usage Examples

### Using ubus CLI

```bash
# Get system status
ubus call autonomy status

# Get interface information
ubus call autonomy interfaces

# Switch to cellular
ubus call autonomy control switch '{"interface": "cellular"}'

# Get configuration
ubus call autonomy config get '{"section": "starlink"}'

# Set configuration
ubus call autonomy config set '{"section": "starlink", "values": {"enabled": true}}'
```

### Using curl (if HTTP API enabled)

```bash
# Get status
curl -X POST http://localhost:8080/api/status

# Switch interface
curl -X POST http://localhost:8080/api/control/switch \
  -H "Content-Type: application/json" \
  -d '{"interface": "cellular"}'
```

### Using Go

```go
package main

import (
    "fmt"
    "github.com/markus-lassfolk/autonomy/pkg/ubus"
)

func main() {
    client := ubus.NewClient()
    
    // Get status
    status, err := client.Call("autonomy", "status", nil)
    if err != nil {
        fmt.Printf("Error: %v\n", err)
        return
    }
    
    fmt.Printf("Status: %+v\n", status)
}
```

## Rate Limiting

API calls are rate-limited to prevent abuse:

- **Status endpoints**: 10 requests per minute
- **Configuration endpoints**: 5 requests per minute
- **Control endpoints**: 2 requests per minute
- **Monitoring endpoints**: 20 requests per minute

## Authentication

The API uses ubus authentication. Ensure proper permissions are set:

```bash
# Allow ubus access for autonomy
ubus call autonomy status
```

## Next Steps

- [Configuration Guide](configuration.md)
- [Troubleshooting](troubleshooting.md)
- [Integration Examples](integration.md)
