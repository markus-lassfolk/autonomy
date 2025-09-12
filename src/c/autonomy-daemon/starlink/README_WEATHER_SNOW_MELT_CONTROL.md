# Starlink Weather-Based Snow Melt Control System

This system provides intelligent, weather-based control of Starlink dish snow melt functionality using gRPC communication and external weather APIs.

## Features

- **Automatic Temperature-Based Control**: Snow melt is automatically disabled when temperature is above +5°C
- **Weather Forecast Integration**: Uses OpenWeatherMap API to predict precipitation and pre-heat the dish
- **Intelligent Mode Selection**: Automatically switches between OFF, AUTOMATIC, and PREHEAT modes based on conditions
- **gRPC Communication**: Direct communication with Starlink dish via gRPC protocol
- **UBUS Interface**: Full UBUS API for remote control and monitoring
- **UCI Configuration**: Persistent configuration storage via OpenWrt UCI system
- **Comprehensive Logging**: Detailed logging and statistics tracking

## Snow Melt Control Logic

The system implements the following control logic based on weather conditions:

### Mode 1: SNOW_MELT_OFF
- **Condition**: Temperature is above +5°C
- **Action**: Disable snow melt heating
- **Reasoning**: No risk of snow accumulation at higher temperatures

### Mode 2: SNOW_MELT_AUTOMATIC  
- **Condition**: Temperature is below +5°C but no precipitation expected
- **Action**: Enable automatic snow melt mode
- **Reasoning**: Prepare for potential snow while conserving energy

### Mode 3: SNOW_MELT_PREHEAT
- **Condition**: Snow or heavy rain expected within 30 minutes (or 1 hour if forecast available)
- **Action**: Enable pre-heat mode for specified duration
- **Reasoning**: Proactively prevent snow/ice accumulation

### Mode 4: SNOW_MELT_PREHEAT (Current Weather)
- **Condition**: Current weather shows snow, heavy snow, heavy rain, or sleet
- **Action**: Enable pre-heat mode immediately
- **Reasoning**: Active precipitation requires immediate heating

## Files

### Core Implementation
- `starlink_weather_snow_melt_control.h` - Header file with data structures and function declarations
- `starlink_weather_snow_melt_control.c` - Main implementation with weather logic and gRPC communication
- `starlink_weather_snow_melt_control_ubus.h` - UBUS interface header
- `starlink_weather_snow_melt_control_ubus.c` - UBUS interface implementation
- `starlink_weather_snow_melt_integration_example.c` - Complete integration example

### Documentation
- `README_WEATHER_SNOW_MELT_CONTROL.md` - This documentation file

## Configuration

### UCI Configuration

The system uses OpenWrt UCI for persistent configuration:

```bash
# Enable the system
uci set autonomy.snow_melt_control.enabled='1'

# Set temperature threshold (default: 5.0°C)
uci set autonomy.snow_melt_control.temperature_threshold='5.0'

# Set weather check interval (default: 15 minutes)
uci set autonomy.snow_melt_control.weather_check_interval='15'

# Set preheat duration (default: 30 minutes)
uci set autonomy.snow_melt_control.preheat_duration='30'

# Enable forecast usage (default: true)
uci set autonomy.snow_melt_control.use_forecast='1'

# Set forecast hours ahead (default: 1 hour)
uci set autonomy.snow_melt_control.forecast_hours_ahead='1'

# Set your OpenWeatherMap API key
uci set autonomy.snow_melt_control.weather_api_key='your_api_key_here'

# Set Starlink dish IP (default: 192.168.100.1)
uci set autonomy.snow_melt_control.starlink_host='192.168.100.1'

# Set Starlink dish port (default: 9200)
uci set autonomy.snow_melt_control.starlink_port='9200'

# Enable debug mode (default: false)
uci set autonomy.snow_melt_control.debug_mode='0'

# Commit changes
uci commit autonomy
```

### Configuration Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `enabled` | boolean | true | Enable/disable the snow melt control system |
| `temperature_threshold` | double | 5.0 | Temperature threshold in Celsius for automatic mode |
| `weather_check_interval` | integer | 15 | Weather check interval in minutes |
| `preheat_duration` | integer | 30 | Pre-heat duration in minutes |
| `use_forecast` | boolean | true | Use weather forecast for pre-heating |
| `forecast_hours_ahead` | integer | 1 | Hours ahead to check forecast |
| `weather_api_key` | string | "" | OpenWeatherMap API key |
| `starlink_host` | string | "192.168.100.1" | Starlink dish IP address |
| `starlink_port` | integer | 9200 | Starlink dish gRPC port |
| `debug_mode` | boolean | false | Enable debug logging |

## UBUS API

The system provides a comprehensive UBUS interface for remote control and monitoring:

### Methods

#### Get Status
```bash
ubus call starlink.weather_snow_melt get_status
```

Returns current system status including:
- Current snow melt mode
- Weather conditions
- Temperature readings
- Precipitation expectations
- Last update timestamps

#### Get Configuration
```bash
ubus call starlink.weather_snow_melt get_config
```

Returns current system configuration.

#### Set Configuration
```bash
ubus call starlink.weather_snow_melt set_config '{
    "temperature_threshold": 5.0,
    "weather_check_interval": 15,
    "preheat_duration": 30,
    "use_forecast": true,
    "forecast_hours_ahead": 1,
    "weather_api_key": "your_api_key",
    "starlink_host": "192.168.100.1",
    "starlink_port": 9200,
    "debug_mode": false
}'
```

#### Enable/Disable System
```bash
# Enable
ubus call starlink.weather_snow_melt set_enabled '{"enabled": true}'

# Disable
ubus call starlink.weather_snow_melt set_enabled '{"enabled": false}'
```

#### Set Mode Manually
```bash
# Set to automatic mode
ubus call starlink.weather_snow_melt set_mode '{"mode": "automatic"}'

# Set to preheat mode
ubus call starlink.weather_snow_melt set_mode '{"mode": "preheat"}'

# Set to off mode
ubus call starlink.weather_snow_melt set_mode '{"mode": "off"}'

# Set to manual mode
ubus call starlink.weather_snow_melt set_mode '{"mode": "manual"}'
```

#### Force Weather Check
```bash
ubus call starlink.weather_snow_melt force_update
```

Forces an immediate weather check and mode update.

#### Get Statistics
```bash
ubus call starlink.weather_snow_melt get_statistics
```

Returns system statistics including:
- Total mode changes
- Activation counts by mode
- Weather check statistics
- Performance metrics

#### Reset Statistics
```bash
ubus call starlink.weather_snow_melt reset_statistics
```

Resets all system statistics to zero.

## Integration with Main Daemon

### 1. Add to Makefile

Add the new source files to the daemon Makefile:

```makefile
SOURCES += starlink/starlink_weather_snow_melt_control.c
SOURCES += starlink/starlink_weather_snow_melt_control_ubus.c
```

### 2. Initialize in Main Daemon

Add initialization in the main daemon startup:

```c
// In autonomy-daemon.c main() function
int result = starlink_weather_snow_melt_control_init();
if (result != AUTONOMY_SUCCESS) {
    LOGX_ERROR_MSG("Failed to initialize snow melt control: %d", result);
}

result = starlink_weather_snow_melt_ubus_init();
if (result != AUTONOMY_SUCCESS) {
    LOGX_WARN_MSG("Failed to initialize snow melt UBUS: %d", result);
}
```

### 3. Add Cleanup

Add cleanup in the main daemon shutdown:

```c
// In cleanup function
starlink_weather_snow_melt_ubus_cleanup();
starlink_weather_snow_melt_control_cleanup();
```

### 4. Add Periodic Checks

Add periodic weather checks in the main daemon loop:

```c
// In main daemon loop
static time_t last_snow_melt_check = 0;
time_t now = time(NULL);

if (now - last_snow_melt_check >= 900) { // Every 15 minutes
    starlink_weather_snow_melt_control_force_update();
    last_snow_melt_check = now;
}
```

## Dependencies

### Required Libraries
- `libubus` - UBUS communication
- `libubox` - UBUS utilities
- `libblobmsg_json` - JSON blob message handling
- `libcurl` - HTTP client for weather API calls
- `libcjson` - JSON parsing
- `pthread` - Threading support

### External Services
- **OpenWeatherMap API** - Weather data and forecasts
- **Starlink Dish** - gRPC communication for snow melt control

### Internal Dependencies
- `external_apis` - Weather API integration
- `gps_manager` - Location services for weather queries
- `uci_manager` - Configuration management
- `starlink_grpc_comprehensive_client` - gRPC communication

## Weather API Setup

### OpenWeatherMap API Key

1. Sign up for a free account at [OpenWeatherMap](https://openweathermap.org/api)
2. Get your API key from the dashboard
3. Configure it in UCI:

```bash
uci set autonomy.snow_melt_control.weather_api_key='your_api_key_here'
uci commit autonomy
```

### API Usage

The system uses the following OpenWeatherMap endpoints:
- **Current Weather**: `/data/2.5/weather` - Current weather conditions
- **Forecast**: `/data/2.5/forecast` - Weather forecast (if implemented)

## gRPC Commands

The system sends the following gRPC commands to the Starlink dish:

### OFF Mode
```json
{
    "set_thermal_state": {
        "target_thermal_state": {
            "heater": false
        }
    }
}
```

### AUTOMATIC Mode
```json
{
    "set_thermal_state": {
        "target_thermal_state": {
            "heater": true,
            "heater_mode": "AUTO"
        }
    }
}
```

### PREHEAT Mode
```json
{
    "set_thermal_state": {
        "target_thermal_state": {
            "heater": true,
            "heater_mode": "PREHEAT"
        }
    }
}
```

### MANUAL Mode
```json
{
    "set_thermal_state": {
        "target_thermal_state": {
            "heater": true,
            "heater_mode": "MANUAL"
        }
    }
}
```

## Troubleshooting

### Common Issues

#### 1. Weather API Failures
- **Symptom**: Weather checks fail with API errors
- **Solution**: Verify API key is correct and has sufficient quota
- **Debug**: Enable debug mode and check logs

#### 2. gRPC Communication Failures
- **Symptom**: Snow melt commands fail to reach Starlink dish
- **Solution**: Verify Starlink dish IP and port are correct
- **Debug**: Check network connectivity to dish

#### 3. GPS Location Issues
- **Symptom**: Weather data is inaccurate or unavailable
- **Solution**: Ensure GPS location is available and accurate
- **Debug**: Check GPS status and location data

#### 4. UBUS Interface Not Available
- **Symptom**: UBUS calls fail or object not found
- **Solution**: Ensure UBUS is properly initialized in main daemon
- **Debug**: Check UBUS context and object registration

### Debug Mode

Enable debug mode for detailed logging:

```bash
uci set autonomy.snow_melt_control.debug_mode='1'
uci commit autonomy
```

Debug mode provides:
- Detailed weather API responses
- gRPC command details
- Mode change reasoning
- Performance metrics

### Log Files

Check the following log locations:
- **System Log**: `/var/log/messages`
- **Daemon Log**: `/var/log/autonomy-daemon.log`
- **UBUS Log**: UBUS debug output

## Performance Considerations

### Weather API Rate Limits
- OpenWeatherMap free tier: 1000 calls/day
- System checks every 15 minutes = 96 calls/day
- Well within free tier limits

### gRPC Communication
- Commands are only sent when mode changes
- Minimal network overhead
- Local network communication (no internet required for commands)

### Memory Usage
- Small memory footprint
- Weather data cached for 10 minutes
- No persistent data storage required

## Security Considerations

### API Key Protection
- API key stored in UCI configuration
- Not exposed in logs (unless debug mode enabled)
- Consider using environment variables for production

### Network Security
- gRPC communication is local network only
- No external network access required for dish control
- Weather API uses HTTPS

### Access Control
- UBUS interface respects OpenWrt access controls
- No direct file system access
- Configuration changes require appropriate permissions

## Future Enhancements

### Planned Features
1. **Multiple Weather APIs**: Support for additional weather services
2. **Advanced Forecasting**: More sophisticated precipitation prediction
3. **Machine Learning**: Learn from historical weather patterns
4. **Mobile App Integration**: Remote monitoring and control
5. **Alert System**: Notifications for mode changes and issues

### API Extensions
1. **Historical Data**: Track weather patterns over time
2. **Performance Analytics**: Detailed efficiency metrics
3. **Custom Rules**: User-defined control logic
4. **Integration Hooks**: Callbacks for external systems

## Support

For issues, questions, or contributions:
1. Check the troubleshooting section above
2. Review the integration example code
3. Enable debug mode for detailed logging
4. Check system logs for error messages

## License

This code is part of the Autonomy Daemon project and follows the same licensing terms.
