# Starlink Tracking and Obstruction Prediction Feature

## Overview

This feature integrates Space-Track satellite ephemeris data with local Starlink dish obstruction mapping to predict connectivity outages and degradation windows. By combining satellite orbital data with the dish's local obstruction map, we can forecast when connectivity issues are likely to occur.

**Enhanced Implementation Based on Research**: This implementation incorporates critical insights about Starlink's actual obstruction map format - a 123×123 polar projection with 15,129 SNR values representing the complete sky view from the dish's perspective.

## Architecture

### Core Components

1. **StarlinkTracker Module** - Main tracking and prediction engine
2. **SpaceTrackConnector** - External API integration for satellite ephemeris data
3. **StarlinkAPI** - Local dish data collection via gRPC (enhanced existing client)
4. **ObstructionAnalyzer** - Obstruction map processing and satellite intersection
5. **PredictionEngine** - Outage forecasting and risk assessment
6. **ValidationModule** - Prediction accuracy monitoring and threshold tuning

### Data Flow

```
Space-Track API → Satellite Ephemeris → Propagation Engine → Obstruction Analysis → Prediction Output
     ↓
Starlink Dish → Obstruction Map → Local Data Processing → Validation & Tuning
```

## Technical Implementation

### Phase 1: Core Infrastructure (Current Focus)

#### 1.1 StarlinkTracker Module Structure
- `starlink_tracker.h` - Main header with public API
- `starlink_tracker.c` - Core tracking logic
- `space_track_connector.h/c` - Space-Track API integration
- `starlink_api.h/c` - Local dish gRPC communication (enhanced)
- `obstruction_analyzer.h/c` - Obstruction map processing
- `prediction_engine.h/c` - Outage forecasting

#### 1.2 Key Data Structures
```c
typedef struct {
    double latitude;
    double longitude;
    double altitude;
    double boresight_azimuth;
    double boresight_elevation;
} dish_location_t;

typedef struct {
    double azimuth;
    double elevation;
    double snr_quality;
    int is_obstructed;
} obstruction_cell_t;

typedef struct {
    char satellite_id[32];
    double azimuth;
    double elevation;
    double range;
    int is_visible;
    int is_obstructed;
} satellite_position_t;

typedef struct {
    time_t start_time;
    time_t end_time;
    int duration_seconds;
    int risk_level; // 1=low, 2=medium, 3=high
    char description[256];
} outage_prediction_t;
```

#### 1.3 Configuration
```c
typedef struct {
    char space_track_username[64];
    char space_track_password[64];
    char starlink_dish_ip[16];
    int starlink_dish_port;
    int update_interval_minutes;
    int prediction_horizon_hours;
    double min_elevation_degrees;
    double obstruction_threshold;
} starlink_config_t;
```

### Phase 2: Enhanced Features

#### 2.1 TLE Caching and Management
- Daily TLE refresh from Space-Track
- Local cache with expiration handling
- Rate limiting compliance (<20 req/min)

#### 2.2 Advanced Prediction
- Multi-satellite availability analysis
- Weather impact consideration
- Historical accuracy tracking

#### 2.3 Integration Points
- Prometheus metrics export
- Node-RED event streaming
- Grafana dashboard integration

## API Integration Details

### Space-Track API

#### Authentication
- Username/password required
- Rate limit: <20 requests per minute
- Data usage compliance with ODR requirements

#### Endpoints Used
```
GET /basicspacedata/query/class/tle_latest/ORDINAL/1/DECAYED/0/format/tle
GET /basicspacedata/query/class/gp_latest/ORDINAL/1/DECAYED/0/format/gp
```

#### TLE Data Format
```
STARLINK-1234
1 44713U 19074A   24001.50000000  .00000000  00000+0  00000+0    0    0
2 44713  52.9980 180.0000 0001001   0.0000   0.0000 15.05432421    01
```

### Starlink Dish gRPC API

#### Required Commands
```bash
# Get dish location
grpcurl -plaintext -d '{"getLocation":{}}' 192.168.100.1:9200 SpaceX.API.Device.Device/Handle

# Get obstruction map
grpcurl -plaintext -d '{"dishGetObstructionMap":{}}' 192.168.100.1:9200 SpaceX.API.Device.Device/Handle

# Get diagnostics (boresight)
grpcurl -plaintext -d '{"dishGetDiagnostics":{}}' 192.168.100.1:9200 SpaceX.API.Device.Device/Handle

# Get status
grpcurl -plaintext -d '{"getStatus":{}}' 192.168.100.1:9200 SpaceX.API.Device.Device/Handle
```

#### Response Fields
- `DishGetObstructionMapResponse.obstructionMap` - Angular quality grid
- `DishGetDiagnosticsResponse.alignmentStats.boresightAzimuthDeg` - Boresight azimuth
- `DishGetDiagnosticsResponse.alignmentStats.boresightElevationDeg` - Boresight elevation
- `GetLocationResponse.latitude/longitude` - Dish coordinates

## Implementation Phases

### Phase 1: Core POC (Current)
- [x] Documentation and planning
- [ ] Basic StarlinkTracker module structure
- [ ] Space-Track connector implementation
- [ ] Local dish gRPC communication enhancement
- [ ] Basic obstruction analysis
- [ ] Simple prediction engine

### Phase 2: Production Ready
- [ ] TLE caching and management
- [ ] Rate limiting and error handling
- [ ] Configuration management
- [ ] Logging and monitoring
- [ ] Unit tests

### Phase 3: Advanced Features
- [ ] Historical accuracy tracking
- [ ] Machine learning threshold tuning
- [ ] Weather integration
- [ ] Advanced visualization
- [ ] Performance optimization

## Usage Examples

### Basic Initialization
```c
#include "starlink_tracker.h"

starlink_config_t config = {
    .space_track_username = "your_username",
    .space_track_password = "your_password",
    .starlink_dish_ip = "192.168.100.1",
    .starlink_dish_port = 9200,
    .update_interval_minutes = 60,
    .prediction_horizon_hours = 24,
    .min_elevation_degrees = 10.0,
    .obstruction_threshold = 0.7
};

starlink_tracker_t *tracker = starlink_tracker_init(&config);
if (!tracker) {
    // Handle initialization error
}
```

### Running Predictions
```c
// Get predictions for next 24 hours
outage_prediction_t *predictions;
int num_predictions = starlink_tracker_get_predictions(tracker, &predictions);

for (int i = 0; i < num_predictions; i++) {
    printf("Outage: %s - %s (Risk: %d)\n",
           ctime(&predictions[i].start_time),
           ctime(&predictions[i].end_time),
           predictions[i].risk_level);
}

// Free predictions array
starlink_tracker_free_predictions(predictions, num_predictions);
```

### Real-time Monitoring
```c
// Start continuous monitoring
starlink_tracker_start_monitoring(tracker);

// Set callback for outage alerts
starlink_tracker_set_outage_callback(tracker, handle_outage_alert);

// Stop monitoring
starlink_tracker_stop_monitoring(tracker);
```

## Configuration

### Environment Variables
```bash
SPACE_TRACK_USERNAME=your_username
SPACE_TRACK_PASSWORD=your_password
STARLINK_DISH_IP=192.168.100.1
STARLINK_DISH_PORT=9200
```

### Configuration File
```json
{
    "space_track": {
        "username": "your_username",
        "password": "your_password",
        "rate_limit_requests_per_minute": 15
    },
    "starlink_dish": {
        "ip": "192.168.100.1",
        "port": 9200,
        "update_interval_minutes": 60
    },
    "prediction": {
        "horizon_hours": 24,
        "min_elevation_degrees": 10.0,
        "obstruction_threshold": 0.7,
        "update_interval_seconds": 300
    }
}
```

## Dependencies

### Required Libraries
- `libcurl` - HTTP client for Space-Track API
- `libjson-c` - JSON parsing and generation
- `libssl` - HTTPS support for Space-Track
- `libm` - Math functions for orbital calculations

### Build Dependencies
```bash
# Ubuntu/Debian
sudo apt-get install libcurl4-openssl-dev libjson-c-dev libssl-dev

# OpenWrt
opkg install libcurl libjson-c openssl-utils
```

## Testing

### Unit Tests
- Mock Space-Track API responses
- Simulated obstruction maps
- Prediction accuracy validation
- Error handling scenarios

### Integration Tests
- Live Space-Track API testing
- Local Starlink dish communication
- End-to-end prediction workflow

### Performance Tests
- Memory usage profiling
- CPU utilization monitoring
- Network bandwidth analysis

## Troubleshooting

### Common Issues

#### Space-Track API Errors
- **401 Unauthorized**: Check username/password
- **429 Too Many Requests**: Reduce update frequency
- **503 Service Unavailable**: Wait and retry

#### Starlink Dish Communication
- **Connection Refused**: Verify IP and port
- **gRPC Timeout**: Check network connectivity
- **Authentication Failed**: Verify dish is accessible

#### Prediction Accuracy
- **False Positives**: Adjust obstruction threshold
- **Missed Outages**: Reduce min elevation angle
- **Poor Performance**: Check TLE data freshness

### Debug Logging
```c
// Enable debug logging
starlink_tracker_set_log_level(tracker, LOG_LEVEL_DEBUG);

// Custom log callback
starlink_tracker_set_log_callback(tracker, custom_log_handler);
```

## Performance Considerations

### Memory Management
- TLE data caching with LRU eviction
- Obstruction map memory pooling
- Prediction result buffer management

### CPU Optimization
- SGP4 propagation batching
- Obstruction grid lookup optimization
- Parallel satellite position calculations

### Network Efficiency
- TLE data compression
- Incremental updates
- Connection pooling

## Security Considerations

### API Credentials
- Secure credential storage
- Environment variable usage
- Credential rotation support

### Data Privacy
- Local data processing only
- No external data transmission
- Compliance with Space-Track terms

### Network Security
- HTTPS for all external API calls
- Local network isolation
- Firewall rule recommendations

## Future Enhancements

### Machine Learning Integration
- Historical accuracy pattern recognition
- Dynamic threshold adjustment
- Weather impact prediction

### Advanced Visualization
- 3D satellite trajectory mapping
- Real-time obstruction overlay
- Interactive prediction dashboard

### Multi-Dish Support
- Fleet-wide connectivity monitoring
- Coordinated outage prediction
- Load balancing recommendations

## Contributing

### Development Guidelines
- Follow existing code style
- Add comprehensive unit tests
- Update documentation for new features
- Use proper error handling

### Testing Requirements
- All new features must have tests
- Integration tests for API changes
- Performance benchmarks for optimizations

## License and Compliance

### Space-Track Usage
- Compliant with Space-Track terms of service
- Respect rate limiting guidelines
- Proper attribution for data sources

### Open Source
- GPL v2 compatible
- Proper license headers
- Third-party license compliance

## Support and Maintenance

### Monitoring
- Health check endpoints
- Performance metrics
- Error rate tracking

### Updates
- TLE data refresh scheduling
- Configuration hot-reload
- Graceful degradation handling

### Documentation
- API reference updates
- Troubleshooting guides
- User manual maintenance

## Integration with Existing Autonomy Daemon

### Existing Components to Leverage
- **Starlink Client** (`src/autonomy/pkg/starlink/client.go`) - Already has gRPC integration
- **GPS System** - Can provide location data if dish location unavailable
- **Health Monitoring** - Can integrate prediction accuracy validation
- **Configuration System** - UCI-based config for credentials and settings
- **Logging System** - Enhanced structured logging for debugging

### Integration Points
- **UBUS Methods** - Expose prediction API via existing UBUS infrastructure
- **HTTP API** - Add REST endpoints for prediction queries
- **Metrics Collection** - Integrate with existing telemetry system
- **Event Notifications** - Use existing notification channels for alerts

### Configuration Integration
```json
{
    "starlink_tracking": {
        "enabled": true,
        "space_track": {
            "username": "env:SPACE_TRACK_USERNAME",
            "password": "env:SPACE_TRACK_PASSWORD",
            "rate_limit_requests_per_minute": 15,
            "cache_duration_hours": 24
        },
        "prediction": {
            "horizon_hours": 24,
            "min_elevation_degrees": 10.0,
            "obstruction_threshold": 0.7,
            "update_interval_seconds": 300,
            "validation_enabled": true
        }
    }
}
```

## C Implementation Notes

### Memory Management Strategy
- Use consistent allocation patterns with existing daemon
- Implement proper cleanup for all dynamically allocated structures
- Use memory pools for frequently allocated/deallocated objects

### Error Handling
- Follow existing error handling patterns in autonomy daemon
- Proper logging at all error points
- Graceful degradation when external APIs fail

### Threading Model
- Background thread for TLE updates and cache management
- Separate thread for continuous prediction calculations
- Main thread integration via message queues or callbacks

### Performance Targets
- TLE update: < 30 seconds for full Starlink constellation
- Prediction calculation: < 5 seconds for 24-hour forecast
- Memory usage: < 50MB for full operation including TLE cache
- CPU usage: < 5% during normal operation