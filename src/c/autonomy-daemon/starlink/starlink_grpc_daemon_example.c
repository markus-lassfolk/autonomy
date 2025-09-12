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
    printf("INFO: "Shutdown signal received"\n"\n"\n"\n"\n"\n"\n"\n");
}

// Example usage of the comprehensive gRPC client in daemon context
int main(int argc, char *argv[]) {
    (void)argc; (void)argv; // Unused parameters
    
    // Setup signal handlers
    signal(SIGINT, signal_handler\n"\n"\n"\n"\n"\n"\n"\n");
    signal(SIGTERM, signal_handler\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "Starting Starlink gRPC daemon example"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Configure the daemon integration
    starlink_grpc_daemon_config_t daemon_config = {0};
    
    // Set up client configuration
    strcpy(daemon_config.client_config.host, "192.168.100.1"\n"\n"\n"\n"\n"\n"\n"\n");
    daemon_config.client_config.port = 9200;
    daemon_config.client_config.timeout = 10;
    daemon_config.client_config.retries = 3;
    
    // Enable some useful flags for daemon operation
    daemon_config.client_config.timestamp_mode = true;
    daemon_config.client_config.debug_mode = false; // Disable in production
    daemon_config.client_config.log_file = strdup("/var/log/starlink-grpc.log"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Set up daemon-specific configuration
    daemon_config.auto_retry = true;
    daemon_config.max_retries = 3;
    daemon_config.retry_delay_ms = 1000;
    daemon_config.enable_monitoring = true;
    daemon_config.monitoring_interval_seconds = 30;
    strcpy(daemon_config.log_prefix, "STARLINK-GRPC"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Initialize the daemon integration
    if (starlink_grpc_daemon_integration_init(&daemon_config) != 0) {
        printf("ERROR: "Failed to initialize daemon integration"\n"\n"\n"\n"\n"\n"\n"\n");
        return 1;
    }
    
    printf("INFO: "Daemon integration initialized successfully"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Example 1: Get device info
    printf("INFO: "=== Example 1: Getting device info ==="\n"\n"\n"\n"\n"\n"\n"\n");
    starlink_device_info_t device_info;
    if (starlink_grpc_daemon_get_device_info(&device_info) == 0) {
        printf("INFO: "Device ID: %s", device_info.id\n"\n"\n"\n"\n"\n"\n"\n");
        printf("INFO: "Hardware Version: %s", device_info.hardware_version\n"\n"\n"\n"\n"\n"\n"\n");
        printf("INFO: "Software Version: %s", device_info.software_version\n"\n"\n"\n"\n"\n"\n"\n");
        printf("INFO: "Country Code: %s", device_info.country_code\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        printf("ERROR: "Failed to get device info"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Example 2: Get status
    printf("INFO: "=== Example 2: Getting status ==="\n"\n"\n"\n"\n"\n"\n"\n");
    starlink_status_response_t status;
    if (starlink_grpc_daemon_get_status(&status) == 0) {
        printf("INFO: "Device uptime: %llu seconds", (unsigned long long)status.device_state.uptime_s\n"\n"\n"\n"\n"\n"\n"\n");
        printf("INFO: "Device ID: %s", status.device_info.id\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        printf("ERROR: "Failed to get status"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Example 3: Get location
    printf("INFO: "=== Example 3: Getting location ==="\n"\n"\n"\n"\n"\n"\n"\n");
    starlink_lla_position_t location;
    if (starlink_grpc_daemon_get_location(&location) == 0) {
        printf("INFO: "Location: Lat=%.6f, Lon=%.6f, Alt=%.2f", 
                     location.lat, location.lon, location.alt\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        printf("ERROR: "Failed to get location"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Example 4: Start monitoring
    printf("INFO: "=== Example 4: Starting monitoring ==="\n"\n"\n"\n"\n"\n"\n"\n");
    if (starlink_grpc_daemon_start_monitoring() == 0) {
        printf("INFO: "Monitoring started successfully"\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Run for a while to demonstrate monitoring
        int monitoring_cycles = 0;
        while (!g_shutdown_requested && monitoring_cycles < 5) {
            sleep(10\n"\n"\n"\n"\n"\n"\n"\n");
            monitoring_cycles++;
            printf("INFO: "Monitoring cycle %d/5", monitoring_cycles\n"\n"\n"\n"\n"\n"\n"\n");
        }
        
        // Stop monitoring
        starlink_grpc_daemon_stop_monitoring(\n"\n"\n"\n"\n"\n"\n"\n");
        printf("INFO: "Monitoring stopped"\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        printf("ERROR: "Failed to start monitoring"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Example 5: Update configuration dynamically
    printf("INFO: "=== Example 5: Updating configuration ==="\n"\n"\n"\n"\n"\n"\n"\n");
    daemon_config.client_config.debug_mode = true; // Enable debug for this example
    daemon_config.monitoring_interval_seconds = 60; // Change monitoring interval
    
    if (starlink_grpc_daemon_update_config(&daemon_config) == 0) {
        printf("INFO: "Configuration updated successfully"\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Test with new configuration
        if (starlink_grpc_daemon_get_device_info(&device_info) == 0) {
            printf("INFO: "Device info with new config: %s", device_info.id\n"\n"\n"\n"\n"\n"\n"\n");
        }
    } else {
        printf("ERROR: "Failed to update configuration"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Example 6: Demonstrate error handling
    printf("INFO: "=== Example 6: Error handling ==="\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Try to get diagnostics (might fail if not available)
    starlink_diagnostics_response_t diagnostics;
    if (starlink_grpc_daemon_get_diagnostics(&diagnostics) == 0) {
        printf("INFO: "Diagnostics retrieved successfully"\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        printf("WARN: "Diagnostics not available (this is normal)"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Cleanup
    printf("INFO: "Cleaning up daemon integration"\n"\n"\n"\n"\n"\n"\n"\n");
    starlink_grpc_daemon_integration_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "Starlink gRPC daemon example completed"\n"\n"\n"\n"\n"\n"\n"\n");
    return 0;
}







