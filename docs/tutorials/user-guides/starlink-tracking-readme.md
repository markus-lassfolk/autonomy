# 🛰️ Starlink Tracker - Standalone Version

## Overview

This is a **standalone version** of the Starlink tracking and obstruction prediction system. It runs independently without requiring the full autonomy daemon infrastructure, making it perfect for:

- 🧪 **Testing and development**
- 🔬 **Research and experimentation**
- 🚀 **Quick deployment** on any Linux system
- 📊 **Demonstration** of tracking capabilities

## Features

- ✅ **Complete independence** - No UBUS/UCI dependencies
- ✅ **Simple configuration** - File-based or command-line config
- ✅ **Web interface** - Beautiful visualization at <http://localhost:8080>
- ✅ **HTTP API** - RESTful API for integration
- ✅ **Real-time tracking** - Live satellite monitoring
- ✅ **Outage prediction** - 12-24 hour forecasting

## Quick Start

### 1. Install Dependencies

```bash
# Ubuntu/Debian
sudo apt-get install libcurl4-openssl-dev libjson-c-dev libmicrohttpd-dev

# OpenWrt
opkg install libcurl libjson-c libmicrohttpd
```

### 2. Build

```bash
cd starlink_standalone/
make
```

### 3. Configure

```bash
# Set Space-Track credentials
export SPACE_TRACK_USERNAME=your_username
export SPACE_TRACK_PASSWORD=your_password

# Or use command line
./starlink_tracker --username your_username --password your_password
```

### 4. Run

```bash
# Basic run
./starlink_tracker

# With custom settings
./starlink_tracker --ip 192.168.100.1 --web-port 8080 --verbose

# With config file
./starlink_tracker --config /etc/starlink_tracker.conf
```

## Configuration

### Command Line Options

```text
Usage: starlink_tracker [OPTIONS]

Options:
  -c, --config FILE     Configuration file path
  -u, --username USER   Space-Track username
  -p, --password PASS   Space-Track password
  -i, --ip IP           Starlink dish IP (default: 192.168.100.1)
  -P, --port PORT       Starlink dish port (default: 9200)
  -w, --web-port PORT   HTTP API port (default: 8080)
  -h, --help            Show help
  -v, --verbose         Enable verbose logging
  -q, --quiet           Quiet mode (errors only)
```

### Configuration File Format

```ini
# Starlink Tracker Configuration
space_track_username=your_username
space_track_password=your_password
starlink_dish_ip=192.168.100.1
starlink_dish_port=9200
update_interval_minutes=60
prediction_horizon_hours=24
min_elevation_degrees=25.0
obstruction_threshold=0.7
http_api_port=8080
log_level=1
```

### Environment Variables

```bash
export SPACE_TRACK_USERNAME=your_username
export SPACE_TRACK_PASSWORD=your_password
```

## API Endpoints

### GET /api/status

Returns current tracker status and statistics.

**Response:**

```json
{
    "status": "monitoring",
    "visible_satellites": 12,
    "unobstructed_satellites": 8,
    "obstruction_percentage": 25.0,
    "accuracy_percentage": 85.2,
    "last_update": 1642684800
}
```

### GET /api/predictions

Returns current outage predictions.

**Response:**

```json
{
    "count": 2,
    "predictions": [
        {
            "start_time": 1642688400,
            "end_time": 1642689300,
            "duration_seconds": 900,
            "risk_level": 3,
            "description": "High obstruction area may block satellites",
            "predicted_available_sats": 0,
            "confidence_score": 0.85
        }
    ]
}
```

### GET /api/satellites

Returns current satellite positions (simplified in standalone version).

## Web Interface

Open your browser to `http://localhost:8080` to access the interactive visualization:

- 🌍 **Ground View** - Look up at the sky from your dish
- 🛰️ **Satellite View** - Look down from space at your coverage area
- 📊 **Real-time Metrics** - Live satellite counts and predictions
- 🎛️ **Interactive Controls** - Time slider, view switching, settings

## Usage Examples

### Basic Monitoring

```bash
# Start with defaults
./starlink_tracker

# Check status (press 's' + Enter while running)
s
```

### Custom Configuration

```bash
# Run with custom settings
./starlink_tracker \
    --username myuser \
    --password mypass \
    --ip 192.168.100.1 \
    --web-port 8080 \
    --verbose
```

### Integration Example

```bash
# Get status via API
curl http://localhost:8080/api/status

# Get predictions
curl http://localhost:8080/api/predictions

# Use in scripts
OUTAGES=$(curl -s http://localhost:8080/api/predictions | jq '.count')
if [ "$OUTAGES" -gt 0 ]; then
    echo "⚠️ $OUTAGES outages predicted"
fi
```

## Differences from Integrated Version

| Feature | Standalone | Integrated (Autonomy Daemon) |
|---------|------------|-------------------------------|
| **Dependencies** | Minimal (curl, json-c) | Full autonomy stack |
| **Configuration** | File/CLI based | UCI based |
| **API** | HTTP REST | UBUS |
| **Integration** | Independent | Part of autonomy system |
| **Use Case** | Testing, development | Production deployment |

## Troubleshooting

### Common Issues

#### "Failed to connect to UBUS"

This error doesn't apply to standalone version - it uses HTTP API instead.

#### "Space-Track authentication failed"

```bash
# Test credentials manually
curl -c cookies.txt -b cookies.txt \
  -d "identity=$SPACE_TRACK_USERNAME&password=$SPACE_TRACK_PASSWORD" \
  https://www.space-track.org/ajaxauth/login
```

#### "Dish connection failed"

```bash
# Test dish connectivity
grpcurl -plaintext -d '{"getStatus":{}}' 192.168.100.1:9200 SpaceX.API.Device.Device/Handle
```

### Debug Mode

```bash
# Run with verbose logging
./starlink_tracker --verbose

# Check log file
tail -f /tmp/starlink_tracker.log
```

## Build Options

```bash
# Development build (with debug symbols)
make dev

# Clean build
make clean && make

# Check dependencies
make check-deps

# Create distribution package
make dist
```

## Performance

The standalone version is optimized for:

- **Memory usage**: <30MB typical
- **CPU usage**: <3% on modern systems  
- **Network usage**: <1MB/hour (Space-Track API)
- **Startup time**: <10 seconds

## License

GPL v2 compatible - same as the main autonomy daemon project.

---

**Perfect for testing the Starlink tracking system without the complexity of the full autonomy daemon!** 🚀
