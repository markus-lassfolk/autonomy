# Starlink Tracking Usage Guide

## Quick Start

### 1. Prerequisites

#### Required Dependencies
```bash
# Ubuntu/Debian
sudo apt-get install libcurl4-openssl-dev libjson-c-dev libssl-dev

# OpenWrt/RUTOS
opkg install libcurl libjson-c openssl-utils
```

#### Space-Track Account
1. Register at [https://www.space-track.org](https://www.space-track.org)
2. Accept the user agreement and terms of service
3. Note your username and password for configuration

#### Starlink Dish Access
- Ensure your Starlink dish is accessible on the local network
- Default IP: `192.168.100.1`, Port: `9200`
- Verify gRPC access with: `grpcurl -plaintext -d '{"getStatus":{}}' 192.168.100.1:9200 SpaceX.API.Device.Device/Handle`

### 2. Configuration

#### Environment Variables
```bash
export SPACE_TRACK_USERNAME=your_username
export SPACE_TRACK_PASSWORD=your_password
export STARLINK_DISH_IP=192.168.100.1  # Optional, defaults to 192.168.100.1
export STARLINK_DISH_PORT=9200          # Optional, defaults to 9200
```

#### UCI Configuration (for autonomy daemon integration)
```bash
uci set autonomy.starlink_tracking=section
uci set autonomy.starlink_tracking.enabled=1
uci set autonomy.starlink_tracking.space_track_username="$SPACE_TRACK_USERNAME"
uci set autonomy.starlink_tracking.space_track_password="$SPACE_TRACK_PASSWORD"
uci set autonomy.starlink_tracking.prediction_horizon_hours=24
uci set autonomy.starlink_tracking.update_interval_minutes=60
uci set autonomy.starlink_tracking.min_elevation_degrees=10
uci set autonomy.starlink_tracking.obstruction_threshold=0.7
uci commit autonomy
```

### 3. Testing the Implementation

#### Standalone Test
```bash
# Compile the test program
./compile_tracking_test.sh

# Run the test (make sure environment variables are set)
./starlink_tracking_test
```

#### Integration with Autonomy Daemon
```bash
# Build the enhanced autonomy daemon
cd package/utils/tlt-autonomy-daemon
make

# Install and start the daemon
./autonomy-daemon

# Test UBUS interface
ubus call starlink_tracker status
ubus call starlink_tracker predictions
ubus call starlink_tracker satellites
ubus call starlink_tracker start_monitoring
```

## API Usage

### UBUS Interface

#### Get Tracker Status
```bash
ubus call starlink_tracker status
```

**Response:**
```json
{
    "status": "monitoring",
    "initialized": true,
    "monitoring_active": true,
    "visible_satellites": 12,
    "unobstructed_satellites": 8,
    "total_predictions": 15,
    "correct_predictions": 12,
    "accuracy_percentage": 80.0,
    "last_update": 1642684800,
    "dish_location": {
        "latitude": 40.7128,
        "longitude": -74.0060,
        "altitude": 10.0,
        "boresight_azimuth": 45.0,
        "boresight_elevation": 30.0
    },
    "obstruction_map": {
        "total_cells": 60,
        "obstructed_cells": 15,
        "obstruction_percentage": 25.0,
        "average_snr": 0.75
    }
}
```

#### Get Outage Predictions
```bash
ubus call starlink_tracker predictions
```

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
            "description": "Predicted outage: 0 satellites available, 900 second duration",
            "predicted_available_sats": 0,
            "confidence_score": 0.85
        },
        {
            "start_time": 1642695600,
            "end_time": 1642696200,
            "duration_seconds": 600,
            "risk_level": 2,
            "description": "Predicted degradation: 1 satellites available, 600 second duration",
            "predicted_available_sats": 1,
            "confidence_score": 0.72
        }
    ]
}
```

#### Get Current Satellite Positions
```bash
ubus call starlink_tracker satellites
```

**Response:**
```json
{
    "count": 15,
    "visible_count": 12,
    "unobstructed_count": 8,
    "satellites": [
        {
            "satellite_id": "STARLINK-1234",
            "azimuth": 45.5,
            "elevation": 25.3,
            "range": 1200.5,
            "is_visible": true,
            "is_obstructed": false,
            "signal_quality": 0.82
        }
    ]
}
```

#### Control Monitoring
```bash
# Start monitoring
ubus call starlink_tracker start_monitoring

# Stop monitoring
ubus call starlink_tracker stop_monitoring

# Force data update
ubus call starlink_tracker update_data
```

### C API Usage

#### Basic Usage
```c
#include "starlink_tracker.h"

// Initialize configuration
starlink_tracker_config_t config = {
    .space_track_username = "your_username",
    .space_track_password = "your_password",
    .starlink_dish_ip = "192.168.100.1",
    .starlink_dish_port = 9200,
    .update_interval_minutes = 60,
    .prediction_horizon_hours = 24,
    .min_elevation_degrees = 10.0,
    .obstruction_threshold = 0.7,
    .validation_enabled = true
};

// Initialize tracker
starlink_tracker_t *tracker = starlink_tracker_init(&config);
if (!tracker) {
    // Handle initialization error
    return -1;
}

// Set outage callback
starlink_tracker_set_outage_callback(tracker, handle_outage_alert, user_data);

// Start monitoring
starlink_tracker_start_monitoring(tracker);

// Get predictions
outage_prediction_t *predictions;
int num_predictions = starlink_tracker_get_predictions(tracker, &predictions);

for (int i = 0; i < num_predictions; i++) {
    printf("Outage: %s - %s (Risk: %d)\n",
           ctime(&predictions[i].start_time),
           ctime(&predictions[i].end_time),
           predictions[i].risk_level);
}

starlink_tracker_free_predictions(predictions, num_predictions);

// Cleanup
starlink_tracker_stop_monitoring(tracker);
starlink_tracker_cleanup(tracker);
```

#### Advanced Usage with Validation
```c
// Enable validation and monitoring
starlink_tracker_config_t config = {
    // ... basic config ...
    .validation_enabled = true
};

starlink_tracker_t *tracker = starlink_tracker_init(&config);

// Monitor actual outages and validate predictions
while (monitoring) {
    // Get current connectivity state
    bool actual_outage = check_actual_connectivity();
    
    // Get recent prediction
    outage_prediction_t *predictions;
    int num_predictions = starlink_tracker_get_predictions(tracker, &predictions);
    
    if (num_predictions > 0) {
        // Validate the most recent prediction
        time_t now = time(NULL);
        for (int i = 0; i < num_predictions; i++) {
            if (now >= predictions[i].start_time && now <= predictions[i].end_time) {
                starlink_tracker_validate_prediction(tracker, &predictions[i], actual_outage);
                break;
            }
        }
        
        starlink_tracker_free_predictions(predictions, num_predictions);
    }
    
    sleep(60); // Check every minute
}

// Get final accuracy statistics
const tracking_stats_t *stats = starlink_tracker_get_stats(tracker);
printf("Final Accuracy: %.1f%% (%d/%d correct)\n", 
       stats->accuracy_percentage, 
       stats->correct_predictions, 
       stats->total_predictions);
```

## Configuration Options

### Tracker Configuration
| Parameter | Default | Description |
|-----------|---------|-------------|
| `space_track_username` | - | Space-Track.org username (required) |
| `space_track_password` | - | Space-Track.org password (required) |
| `starlink_dish_ip` | "192.168.100.1" | Starlink dish IP address |
| `starlink_dish_port` | 9200 | Starlink dish gRPC port |
| `update_interval_minutes` | 60 | How often to update data |
| `prediction_horizon_hours` | 24 | How far ahead to predict |
| `min_elevation_degrees` | 10.0 | Minimum satellite elevation |
| `obstruction_threshold` | 0.7 | SNR threshold for obstruction |
| `validation_enabled` | true | Enable prediction validation |
| `cache_duration_hours` | 24 | TLE cache duration |
| `rate_limit_requests_per_minute` | 15 | Space-Track API rate limit |

### Tuning Parameters

#### Elevation Threshold
- **Lower values (5-10°)**: More satellites considered, but lower quality
- **Higher values (15-20°)**: Fewer satellites, but better signal quality
- **Recommended**: Start with 10° and adjust based on your location

#### Obstruction Threshold
- **Lower values (0.5-0.6)**: More conservative, may predict false outages
- **Higher values (0.8-0.9)**: More aggressive, may miss real outages
- **Recommended**: Start with 0.7 and let validation auto-tune

#### Update Intervals
- **Faster updates (15-30 min)**: More current data, higher API usage
- **Slower updates (60-120 min)**: Less API usage, slightly stale data
- **Recommended**: 60 minutes for most use cases

## Troubleshooting

### Common Issues

#### "Authentication Failed"
```bash
# Check credentials
echo $SPACE_TRACK_USERNAME
echo $SPACE_TRACK_PASSWORD

# Test manual login
curl -c cookies.txt -b cookies.txt \
  -d "identity=$SPACE_TRACK_USERNAME&password=$SPACE_TRACK_PASSWORD" \
  https://www.space-track.org/ajaxauth/login
```

#### "Connection Refused" to Starlink Dish
```bash
# Check dish IP and port
ping 192.168.100.1

# Test gRPC connectivity
grpcurl -plaintext -d '{"getStatus":{}}' 192.168.100.1:9200 SpaceX.API.Device.Device/Handle

# Check if dish is in bypass mode
# (some dishes require bypass mode for local API access)
```

#### "No Satellites Found"
- Check Space-Track credentials and network connectivity
- Verify TLE data is being fetched successfully
- Check cache directory permissions: `/tmp/starlink_tracker_cache`

#### "Poor Prediction Accuracy"
- Enable validation: `validation_enabled = true`
- Let the system auto-tune thresholds over 24-48 hours
- Manually adjust `obstruction_threshold` based on false positive/negative rates

### Debug Logging

#### Enable Debug Mode
```c
// In code
starlink_tracker_set_log_level(tracker, TRACKER_LOG_DEBUG);

// Or via environment
export STARLINK_TRACKER_LOG_LEVEL=debug
```

#### Log Files
- Daemon logs: `/var/log/autonomy.log`
- Tracking logs: `/tmp/starlink_tracker.log`
- Cache directory: `/tmp/starlink_tracker_cache/`

### Performance Monitoring

#### Memory Usage
```bash
# Check daemon memory usage
ps aux | grep autonomy-daemon

# Check cache size
du -h /tmp/starlink_tracker_cache/
```

#### API Usage
```bash
# Monitor Space-Track API calls
tail -f /var/log/autonomy.log | grep "Space-Track"

# Check rate limiting
ubus call starlink_tracker status | grep -E "(total_requests|rate_limited)"
```

## Integration Examples

### Node-RED Integration
```javascript
// Get predictions via UBUS
const predictions = await ubus.call('starlink_tracker', 'predictions');

// Process outage alerts
predictions.predictions.forEach(prediction => {
    if (prediction.risk_level >= 3) {
        // Send high-risk outage alert
        msg.payload = {
            type: 'starlink_outage_alert',
            severity: 'high',
            start_time: prediction.start_time,
            duration: prediction.duration_seconds,
            description: prediction.description
        };
        node.send(msg);
    }
});
```

### Prometheus Metrics
```bash
# Export metrics (example implementation)
curl http://localhost:8080/metrics | grep starlink_

# Example metrics:
# starlink_visible_satellites 12
# starlink_unobstructed_satellites 8
# starlink_prediction_accuracy 0.85
# starlink_outage_predictions_total 5
```

### Shell Script Integration
```bash
#!/bin/bash

# Get current satellite count
SATELLITES=$(ubus call starlink_tracker status | jsonfilter -e '@.unobstructed_satellites')

if [ "$SATELLITES" -lt 3 ]; then
    echo "Warning: Low satellite count ($SATELLITES)"
    # Take action (switch to backup connection, etc.)
fi

# Check for upcoming outages
OUTAGES=$(ubus call starlink_tracker predictions | jsonfilter -e '@.count')

if [ "$OUTAGES" -gt 0 ]; then
    echo "Alert: $OUTAGES outages predicted in next 24 hours"
    # Send notification, prepare backup, etc.
fi
```

## Advanced Features

### Custom Prediction Algorithms

You can extend the prediction engine with custom algorithms:

```c
// Custom risk assessment function
risk_level_t custom_risk_calculator(int available_satellites, double connectivity_score, int duration) {
    // Your custom logic here
    if (available_satellites == 0 && duration > 600) {
        return RISK_LEVEL_CRITICAL;
    }
    // ... more logic
    return RISK_LEVEL_LOW;
}

// Register custom calculator
prediction_engine_set_risk_calculator(tracker->engine, custom_risk_calculator);
```

### Historical Data Analysis

```c
// Get validation history
validation_sample_t *samples;
int num_samples = validation_module_get_recent_samples(module, 100, &samples);

// Analyze patterns
for (int i = 0; i < num_samples; i++) {
    // Process historical accuracy data
    analyze_prediction_pattern(&samples[i]);
}
```

### Multi-Dish Support

```c
// Initialize multiple trackers for different dishes
starlink_tracker_t *trackers[4];
for (int i = 0; i < 4; i++) {
    starlink_tracker_config_t config = base_config;
    snprintf(config.starlink_dish_ip, sizeof(config.starlink_dish_ip), 
             "192.168.100.%d", i + 1);
    
    trackers[i] = starlink_tracker_init(&config);
}

// Aggregate predictions across all dishes
// ... implementation depends on your use case
```

## Performance Optimization

### Memory Management
- TLE cache is limited to 24 hours by default
- Prediction cache uses ring buffers to limit memory usage
- Satellite position calculations are done on-demand

### CPU Optimization
- Orbital propagation is batched for efficiency
- Obstruction analysis uses interpolation to reduce calculations
- Background threads handle data updates

### Network Efficiency
- TLE data is cached locally to minimize Space-Track API calls
- Rate limiting prevents API quota exhaustion
- gRPC calls to dish are minimized and cached

## Monitoring and Alerts

### Real-time Monitoring
```c
// Set up outage callback
void handle_outage(const outage_prediction_t *prediction, void *user_data) {
    if (prediction->risk_level >= RISK_LEVEL_HIGH) {
        // Send immediate alert
        send_notification("High risk outage predicted", prediction->description);
        
        // Take preventive action
        activate_backup_connection();
    }
}

starlink_tracker_set_outage_callback(tracker, handle_outage, NULL);
```

### Accuracy Monitoring
```c
// Monitor prediction accuracy
const tracking_stats_t *stats = starlink_tracker_get_stats(tracker);

if (stats->accuracy_percentage < 70.0) {
    // Accuracy is low, may need threshold tuning
    log_warning("Low prediction accuracy: %.1f%%", stats->accuracy_percentage);
    
    // Enable auto-tuning
    validation_config_t val_config;
    val_config.auto_tune_thresholds = true;
    validation_module_update_config(validation_module, &val_config);
}
```

## Best Practices

### 1. Gradual Deployment
- Start with monitoring only (no automated actions)
- Validate predictions manually for 1-2 weeks
- Gradually enable automated responses

### 2. Threshold Tuning
- Begin with default thresholds
- Enable validation and auto-tuning
- Monitor accuracy for at least 100 predictions before manual adjustments

### 3. Error Handling
- Always check return codes from API functions
- Implement graceful degradation when Space-Track is unavailable
- Have fallback logic when dish communication fails

### 4. Security
- Store Space-Track credentials securely (environment variables or encrypted config)
- Limit network access to Space-Track API
- Regularly rotate API credentials

### 5. Compliance
- Respect Space-Track rate limits (<20 requests/minute)
- Cache TLE data appropriately (24-hour cache recommended)
- Follow Space-Track terms of service for data usage

## Support and Maintenance

### Regular Maintenance
```bash
# Check tracker health daily
ubus call starlink_tracker status

# Clear old cache weekly
rm -rf /tmp/starlink_tracker_cache/*

# Review accuracy monthly
ubus call starlink_tracker status | jsonfilter -e '@.accuracy_percentage'
```

### Log Monitoring
```bash
# Monitor for errors
tail -f /var/log/autonomy.log | grep -E "(ERROR|WARN)" | grep starlink

# Check API usage
grep "Space-Track" /var/log/autonomy.log | tail -20
```

### Performance Monitoring
```bash
# Check memory usage
cat /proc/$(pgrep autonomy-daemon)/status | grep VmRSS

# Check CPU usage
top -p $(pgrep autonomy-daemon)
```

This comprehensive implementation provides a robust foundation for Starlink outage prediction that can be easily integrated with your existing autonomy daemon infrastructure [[memory:8028948]].