#include "starlink_grpc_daemon_integration.h"
#include "../shared/logging/logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

// Global flag for graceful shutdown
static volatile bool g_shutdown_requested = false;

// Signal handler for graceful shutdown
static void signal_handler(int sig) {
    (void)sig; // Unused parameter
    g_shutdown_requested = true;
    LOGX_INFO_MSG("Shutdown signal received");
}

// Example usage of the comprehensive gRPC client in daemon context
int main(int argc, char *argv[]) {
    (void)argc; (void)argv; // Unused parameters
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    LOGX_INFO_MSG("Starting Starlink gRPC daemon example");
    
    // Configure the daemon integration
    starlink_grpc_daemon_config_t daemon_config = {0};
    
    // Set up client configuration
    safe_strncpy(daemon_config.client_config.host, "192.168.100.1", sizeof(daemon_config.client_config.host));
    daemon_config.client_config.port = 9200;
    daemon_config.client_config.timeout = 10;
    daemon_config.client_config.retries = 3;
    
    // Enable some useful flags for daemon operation
    daemon_config.client_config.timestamp_mode = true;
    daemon_config.client_config.debug_mode = false; // Disable in production
    daemon_config.client_config.log_file = strdup("/var/log/starlink-grpc.log");
    
    // Set up daemon-specific configuration
    daemon_config.auto_retry = true;
    daemon_config.max_retries = 3;
    daemon_config.retry_delay_ms = 1000;
    daemon_config.enable_monitoring = true;
    daemon_config.monitoring_interval_seconds = 30;
    safe_strncpy(daemon_config.log_prefix, "STARLINK-GRPC", sizeof(daemon_config.log_prefix));
    
    // Initialize the daemon integration
    if (starlink_grpc_daemon_integration_init(&daemon_config) != 0) {
        LOGX_ERROR_MSG("Failed to initialize daemon integration");
        return 1;
    }
    
    LOGX_INFO_MSG("Daemon integration initialized successfully");
    
    // Example 1: Get device info
    LOGX_INFO_MSG("=== Example 1: Getting device info ===");
    starlink_device_info_t device_info;
    if (starlink_grpc_daemon_get_device_info(&device_info) == 0) {
        LOGX_INFO_MSG("Device ID: %s", device_info.id);
        LOGX_INFO_MSG("Hardware Version: %s", device_info.hardware_version);
        LOGX_INFO_MSG("Software Version: %s", device_info.software_version);
        LOGX_INFO_MSG("Country Code: %s", device_info.country_code);
    } else {
        LOGX_ERROR_MSG("Failed to get device info");
    }
    
    // Example 2: Get status
    LOGX_INFO_MSG("=== Example 2: Getting status ===");
    starlink_status_response_t status;
    if (starlink_grpc_daemon_get_status(&status) == 0) {
        LOGX_INFO_MSG("Device uptime: %llu seconds", (unsigned long long)status.device_state.uptime_s);
        LOGX_INFO_MSG("Device ID: %s", status.device_info.id);
    } else {
        LOGX_ERROR_MSG("Failed to get status");
    }
    
    // Example 3: Get location
    LOGX_INFO_MSG("=== Example 3: Getting location ===");
    starlink_lla_position_t location;
    if (starlink_grpc_daemon_get_location(&location) == 0) {
        LOGX_INFO_MSG("Location: Lat=%.6f, Lon=%.6f, Alt=%.2f", 
                     location.lat, location.lon, location.alt);
    } else {
        LOGX_ERROR_MSG("Failed to get location");
    }
    
    // Example 4: Start monitoring
    LOGX_INFO_MSG("=== Example 4: Starting monitoring ===");
    if (starlink_grpc_daemon_start_monitoring() == 0) {
        LOGX_INFO_MSG("Monitoring started successfully");
        
        // Run for a while to demonstrate monitoring
        int monitoring_cycles = 0;
        while (!g_shutdown_requested && monitoring_cycles < 5) {
            sleep(10);
            monitoring_cycles++;
            LOGX_INFO_MSG("Monitoring cycle %d/5", monitoring_cycles);
        }
        
        // Stop monitoring
        starlink_grpc_daemon_stop_monitoring();
        LOGX_INFO_MSG("Monitoring stopped");
    } else {
        LOGX_ERROR_MSG("Failed to start monitoring");
    }
    
    // Example 5: Update configuration dynamically
    LOGX_INFO_MSG("=== Example 5: Updating configuration ===");
    daemon_config.client_config.debug_mode = true; // Enable debug for this example
    daemon_config.monitoring_interval_seconds = 60; // Change monitoring interval
    
    if (starlink_grpc_daemon_update_config(&daemon_config) == 0) {
        LOGX_INFO_MSG("Configuration updated successfully");
        
        // Test with new configuration
        if (starlink_grpc_daemon_get_device_info(&device_info) == 0) {
            LOGX_INFO_MSG("Device info with new config: %s", device_info.id);
        }
    } else {
        LOGX_ERROR_MSG("Failed to update configuration");
    }
    
    // Example 6: Demonstrate error handling
    LOGX_INFO_MSG("=== Example 6: Error handling ===");
    
    // Try to get diagnostics (might fail if not available)
    starlink_diagnostics_response_t diagnostics;
    if (starlink_grpc_daemon_get_diagnostics(&diagnostics) == 0) {
        LOGX_INFO_MSG("Diagnostics retrieved successfully");
    } else {
        LOGX_WARN_MSG("Diagnostics not available (this is normal)");
    }
    
    // Cleanup
    LOGX_INFO_MSG("Cleaning up daemon integration");
    starlink_grpc_daemon_integration_cleanup();
    
    LOGX_INFO_MSG("Starlink gRPC daemon example completed");
    return 0;
}







