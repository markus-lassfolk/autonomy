# Starlink gRPC Comprehensive Client

This directory contains a comprehensive Starlink gRPC client implementation that provides both standalone command-line usage and daemon integration capabilities.

## Features

### 🚀 **Comprehensive Flag Support**
- **Output & Formatting**: `--raw`, `--pretty`, `--compact`, `--no-header`, `--silent`, `--hex`, `--summary`
- **Network & Connection**: `--timeout`, `--retries`, `--user-agent`, `--insecure`
- **Logging & Monitoring**: `--debug`, `--verbose`, `--log`, `--timestamp`, `--watch`
- **Advanced Features**: `--batch`, `--compare`, `--diff`, `--export`

### 📡 **80+ API Endpoints**
- Basic Status & Info: `get_status`, `get_device_info`, `get_location`, etc.
- Dish Control & Config: `dish_get_config`, `dish_set_config`, `dish_stow`, etc.
- WiFi Management: `wifi_get_clients`, `wifi_get_config`, `wifi_set_config`, etc.
- System Control: `reboot`, `factory_reset`, `software_update`, etc.
- And many more!

### 🔧 **Flexible Endpoint Format**
- Traditional: `192.168.100.1 9200 get_status`
- Modern: `192.168.100.1:9200 get_status`

## Files

### Standalone Client
- `starlink_grpc_standalone.c` - Original standalone client (legacy)
- `starlink_grpc_standalone_v2.c` - New comprehensive standalone client
- `Makefile` - Build system for original client
- `Makefile.v2` - Build system for new client

### Daemon Integration
- `../../src/c/autonomy-daemon/starlink/starlink_grpc_comprehensive_client.h` - Core client library
- `../../src/c/autonomy-daemon/starlink/starlink_grpc_comprehensive_client.c` - Core client implementation
- `../../src/c/autonomy-daemon/starlink/starlink_grpc_daemon_integration.h` - Daemon integration API
- `../../src/c/autonomy-daemon/starlink/starlink_grpc_daemon_integration.c` - Daemon integration implementation
- `../../src/c/autonomy-daemon/starlink/starlink_grpc_daemon_example.c` - Usage example

## Building

### Standalone Client (v2)
```bash
cd tools/starlink-standalone
make -f Makefile.v2
```

### Daemon Integration
```bash
cd src/c/autonomy-daemon
make  # This will build the daemon with comprehensive client support
```

## Usage

### Standalone Client

#### Basic Usage
```bash
# Get device info
./starlink-grpc-client-v2 192.168.100.1:9200 get_device_info

# Get status with pretty printing
./starlink-grpc-client-v2 --pretty 192.168.100.1:9200 get_status

# Get location with timestamps
./starlink-grpc-client-v2 --timestamp 192.168.100.1:9200 get_location
```

#### Advanced Usage
```bash
# Raw hex output
./starlink-grpc-client-v2 --raw 192.168.100.1:9200 get_status

# Debug mode with full request/response details
./starlink-grpc-client-v2 --debug 192.168.100.1:9200 get_status

# Silent mode (no headers)
./starlink-grpc-client-v2 --no-header 192.168.100.1:9200 get_status

# Summary mode
./starlink-grpc-client-v2 --summary 192.168.100.1:9200 get_status

# Log to file
./starlink-grpc-client-v2 --log /tmp/starlink.log 192.168.100.1:9200 get_status

# Custom timeout and retries
./starlink-grpc-client-v2 --timeout 30 --retries 5 192.168.100.1:9200 get_status
```

#### Monitoring
```bash
# Watch mode (poll every 10 seconds)
./starlink-grpc-client-v2 --watch 10 192.168.100.1:9200 get_status
```

### Daemon Integration

#### Basic Usage
```c
#include "starlink_grpc_daemon_integration.h"

// Configure the daemon integration
starlink_grpc_daemon_config_t daemon_config = {0};
strcpy(daemon_config.client_config.host, "192.168.100.1");
daemon_config.client_config.port = 9200;
daemon_config.client_config.timeout = 10;
daemon_config.client_config.retries = 3;
daemon_config.client_config.timestamp_mode = true;
daemon_config.enable_monitoring = true;
daemon_config.monitoring_interval_seconds = 30;

// Initialize
if (starlink_grpc_daemon_integration_init(&daemon_config) != 0) {
    // Handle error
}

// Get device info
starlink_device_info_t device_info;
if (starlink_grpc_daemon_get_device_info(&device_info) == 0) {
    printf("Device ID: %s\n", device_info.id);
    printf("Hardware: %s\n", device_info.hardware_version);
    printf("Software: %s\n", device_info.software_version);
}

// Get status
starlink_status_response_t status;
if (starlink_grpc_daemon_get_status(&status) == 0) {
    printf("Uptime: %llu seconds\n", (unsigned long long)status.device_state.uptime_s);
}

// Get location
starlink_lla_position_t location;
if (starlink_grpc_daemon_get_location(&location) == 0) {
    printf("Location: Lat=%.6f, Lon=%.6f, Alt=%.2f\n", 
           location.lat, location.lon, location.alt);
}

// Start monitoring
starlink_grpc_daemon_start_monitoring();

// ... do other work ...

// Stop monitoring and cleanup
starlink_grpc_daemon_stop_monitoring();
starlink_grpc_daemon_integration_cleanup();
```

#### Advanced Usage
```c
// Update configuration dynamically
daemon_config.client_config.debug_mode = true;
daemon_config.monitoring_interval_seconds = 60;
starlink_grpc_daemon_update_config(&daemon_config);

// Check if monitoring is active
if (starlink_grpc_daemon_is_monitoring()) {
    printf("Monitoring is active\n");
}

// Get current configuration
const starlink_grpc_daemon_config_t *config = starlink_grpc_daemon_get_config();
printf("Host: %s:%d\n", config->client_config.host, config->client_config.port);
```

## Configuration Options

### Client Configuration
```c
typedef struct {
    // Connection settings
    char host[256];           // Starlink dish IP address
    int port;                 // gRPC port (default: 9200)
    int timeout;              // Request timeout in seconds
    int retries;              // Number of retries on failure
    
    // Output formatting flags
    bool raw_mode;            // Show raw protobuf data
    bool debug_mode;          // Show detailed debug info
    bool pretty_mode;         // Pretty-print JSON
    bool compact_mode;        // Compact JSON output
    bool no_header;           // Suppress HTTP status line
    bool silent_mode;         // Only show response data
    bool hex_mode;            // Show hex data
    bool summary_mode;        // Show summary instead of full JSON
    bool verbose_mode;        // Verbose output
    bool timestamp_mode;      // Add timestamps
    bool insecure_mode;       // Skip SSL verification
    
    // Advanced features
    bool compare_mode;        // Compare with previous response
    bool diff_mode;           // Show differences
    int watch_interval;       // Polling interval for watch mode
    char *user_agent;         // Custom User-Agent header
    char *fields_filter;      // Filter specific fields
    char *log_file;           // Log file path
    char *batch_file;         // Batch commands file
    char *export_format;      // Export format (CSV/XML/JSON)
} starlink_grpc_client_config_t;
```

### Daemon Configuration
```c
typedef struct {
    starlink_grpc_client_config_t client_config;  // Client configuration
    bool auto_retry;                              // Enable automatic retry
    int max_retries;                              // Maximum retry attempts
    int retry_delay_ms;                           // Delay between retries
    bool enable_monitoring;                       // Enable background monitoring
    int monitoring_interval_seconds;              // Monitoring interval
    char log_prefix[64];                          // Log message prefix
} starlink_grpc_daemon_config_t;
```

## Error Handling

The comprehensive client provides robust error handling:

- **Connection Errors**: Automatic retry with configurable attempts
- **Timeout Handling**: Configurable timeouts with proper error reporting
- **Access Denied**: Detection and reporting of restricted endpoints
- **Invalid Responses**: Graceful handling of malformed responses
- **Resource Management**: Proper cleanup of allocated resources

## Logging

The client supports multiple logging options:

- **Console Output**: Standard printf output with formatting options
- **File Logging**: Persistent logging to specified files
- **Debug Mode**: Detailed request/response logging
- **Timestamp Mode**: Time-stamped output for monitoring
- **Silent Mode**: Minimal output for scripting

## Examples

### Example 1: Basic Status Check
```bash
./starlink-grpc-client-v2 192.168.100.1:9200 get_status
```

### Example 2: Pretty-Printed Location
```bash
./starlink-grpc-client-v2 --pretty 192.168.100.1:9200 get_location
```

### Example 3: Debug Mode
```bash
./starlink-grpc-client-v2 --debug 192.168.100.1:9200 get_device_info
```

### Example 4: Monitoring
```bash
./starlink-grpc-client-v2 --watch 30 --timestamp 192.168.100.1:9200 get_status
```

### Example 5: Raw Data Analysis
```bash
./starlink-grpc-client-v2 --raw 192.168.100.1:9200 get_status | hexdump -C
```

## Integration with Autonomy Daemon

The comprehensive client is designed to integrate seamlessly with the Autonomy daemon:

1. **Shared Library**: Both standalone and daemon use the same core library
2. **Configuration Management**: Unified configuration system
3. **Error Handling**: Consistent error handling across both contexts
4. **Logging**: Integrated with daemon logging system
5. **Monitoring**: Background monitoring capabilities for daemon

## Troubleshooting

### Common Issues

1. **Connection Refused**: Check if Starlink dish is accessible at the specified IP
2. **Access Denied**: Some endpoints may be restricted - check the help for available commands
3. **Timeout**: Increase timeout value with `--timeout` flag
4. **Build Errors**: Ensure all dependencies (curl, json-c, pthread) are installed

### Debug Mode

Use `--debug` flag to see detailed request/response information:
```bash
./starlink-grpc-client-v2 --debug 192.168.100.1:9200 get_status
```

### Log Files

Check log files for persistent error information:
```bash
./starlink-grpc-client-v2 --log /tmp/starlink.log 192.168.100.1:9200 get_status
tail -f /tmp/starlink.log
```

## Contributing

When adding new features:

1. Update the core client library (`starlink_grpc_comprehensive_client.c`)
2. Update the daemon integration if needed
3. Update the standalone client if needed
4. Add tests and examples
5. Update this documentation

## License

This code is part of the Autonomy project and follows the same licensing terms.




