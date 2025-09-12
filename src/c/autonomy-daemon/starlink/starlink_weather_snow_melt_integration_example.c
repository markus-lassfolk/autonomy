/**
 * Starlink Weather-Based Snow Melt Control Integration Example
 * 
 * This example demonstrates how to integrate the weather-based snow melt control
 * system with the main autonomy daemon.
 */

#include "starlink_weather_snow_melt_control.h"
#include "starlink_weather_snow_melt_control_ubus.h"
#include "../core/types.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

// Global flag for graceful shutdown
static volatile bool g_running = true;

// Signal handler for graceful shutdown
static void signal_handler(int sig) {
    printf("INFO: Received signal %d, shutting down gracefully...\n", sig);
    g_running = false;
}

// Example: Initialize snow melt control system
int example_init_snow_melt_control(void) {
    printf("=== Starlink Weather-Based Snow Melt Control Example ===\n");
    
    // Initialize the snow melt control system
    int result = starlink_weather_snow_melt_control_init();
    if (result != AUTONOMY_SUCCESS) {
        printf("ERROR: Failed to initialize snow melt control system: %d\n", result);
        return result;
    }
    
    printf("INFO: Snow melt control system initialized successfully\n");
    
    // Initialize UBUS interface (pass NULL context for example)
    result = starlink_weather_snow_melt_ubus_init(NULL);
    if (result != AUTONOMY_SUCCESS) {
        printf("WARN: Failed to initialize UBUS interface: %d\n", result);
        // Continue without UBUS - system will still work
    } else {
        printf("INFO: UBUS interface initialized successfully\n");
    }
    
    return AUTONOMY_SUCCESS;
}

// Example: Configure snow melt control system
int example_configure_snow_melt_control(void) {
    printf("INFO: Configuring snow melt control system...\n");
    
    // Get current configuration
    starlink_weather_snow_melt_config_t config;
    int result = starlink_weather_snow_melt_control_get_config(&config);
    if (result != AUTONOMY_SUCCESS) {
        printf("ERROR: Failed to get current configuration: %d\n", result);
        return result;
    }
    
    // Update configuration with example values
    config.enabled = true;
    config.temperature_threshold_celsius = 5.0;  // Snow melt OFF when above +5C
    config.weather_check_interval_minutes = 15;  // Check weather every 15 minutes
    config.preheat_duration_minutes = 30;        // Pre-heat for 30 minutes
    config.use_forecast = true;                  // Use weather forecast
    config.forecast_hours_ahead = 1;             // Check forecast 1 hour ahead
    config.debug_mode = true;                    // Enable debug logging
    
    // Set your OpenWeatherMap API key here
    strncpy(config.weather_api_key, "your_openweathermap_api_key_here", 
            sizeof(config.weather_api_key) - 1);
    config.weather_api_key[sizeof(config.weather_api_key) - 1] = '\0';
    
    // Set Starlink dish IP (default is usually 192.168.100.1)
    strncpy(config.starlink_host, "192.168.100.1", sizeof(config.starlink_host) - 1);
    config.starlink_host[sizeof(config.starlink_host) - 1] = '\0';
    config.starlink_port = 9200;
    
    // Apply configuration
    result = starlink_weather_snow_melt_control_set_config(&config);
    if (result != AUTONOMY_SUCCESS) {
        printf("ERROR: Failed to set configuration: %d\n", result);
        return result;
    }
    
    printf("INFO: Snow melt control system configured successfully\n");
    printf("INFO: Temperature threshold: %.1fC\n", config.temperature_threshold_celsius);
    printf("INFO: Weather check interval: %d minutes\n", config.weather_check_interval_minutes);
    printf("INFO: Preheat duration: %d minutes\n", config.preheat_duration_minutes);
    printf("INFO: Using forecast: %s\n", config.use_forecast ? "Yes" : "No");
    printf("INFO: Starlink host: %s:%d\n", config.starlink_host, config.starlink_port);
    
    return AUTONOMY_SUCCESS;
}

// Example: Monitor snow melt control system
int example_monitor_snow_melt_control(void) {
    printf("INFO: Starting snow melt control monitoring...\n");
    
    int check_count = 0;
    const int max_checks = 20; // Run for 20 checks (5 hours with 15-minute intervals)
    
    while (g_running && check_count < max_checks) {
        // Get current status
        starlink_weather_snow_melt_status_t status;
        int result = starlink_weather_snow_melt_control_get_status(&status);
        if (result != AUTONOMY_SUCCESS) {
            printf("ERROR: Failed to get status: %d\n", result);
            sleep(60); // Wait 1 minute before retry
            continue;
        }
        
        printf("\n=== Snow Melt Control Status (Check #%d) ===\n", check_count + 1);
        printf("System enabled: %s\n", status.enabled ? "Yes" : "No");
        printf("Current mode: %s\n", starlink_weather_snow_melt_mode_to_string(status.current_mode));
        printf("Previous mode: %s\n", starlink_weather_snow_melt_mode_to_string(status.previous_mode));
        printf("Current temperature: %.1fC\n", status.current_temperature);
        printf("Current weather: %s\n", starlink_weather_snow_melt_weather_condition_to_string(status.current_weather));
        printf("Forecast weather: %s\n", starlink_weather_snow_melt_weather_condition_to_string(status.forecast_weather));
        printf("Precipitation expected: %s\n", status.precipitation_expected ? "Yes" : "No");
         printf("Last weather check: %lld\n", (long long)status.last_weather_check);
        printf("Last weather description: %s\n", status.last_weather_description);
        
        if (status.current_mode == SNOW_MELT_PREHEAT) {
            printf("Preheat remaining: %d minutes\n", status.preheat_remaining_minutes);
        }
        
        // Force a weather check and mode update
        printf("INFO: Forcing weather check and mode update...\n");
        result = starlink_weather_snow_melt_control_force_update();
        if (result != AUTONOMY_SUCCESS) {
            printf("WARN: Weather check failed: %d\n", result);
        } else {
            printf("INFO: Weather check completed successfully\n");
        }
        
        check_count++;
        
        if (g_running && check_count < max_checks) {
            printf("INFO: Waiting 15 minutes until next check...\n");
            sleep(900); // Wait 15 minutes
        }
    }
    
    printf("INFO: Monitoring completed after %d checks\n", check_count);
    return AUTONOMY_SUCCESS;
}

// Example: Get and display statistics
int example_show_statistics(void) {
    printf("INFO: Getting snow melt control statistics...\n");
    
    starlink_weather_snow_melt_stats_t stats;
    int result = starlink_weather_snow_melt_control_get_statistics(&stats);
    if (result != AUTONOMY_SUCCESS) {
        printf("ERROR: Failed to get statistics: %d\n", result);
        return result;
    }
    
    printf("\n=== Snow Melt Control Statistics ===\n");
    printf("Total mode changes: %d\n", stats.total_mode_changes);
    printf("Automatic activations: %d\n", stats.automatic_activations);
    printf("Preheat activations: %d\n", stats.preheat_activations);
    printf("Manual activations: %d\n", stats.manual_activations);
    printf("Weather checks performed: %d\n", stats.weather_checks_performed);
    printf("Successful weather checks: %d\n", stats.successful_weather_checks);
    printf("Failed weather checks: %d\n", stats.failed_weather_checks);
    printf("Average weather check time: %.2f ms\n", stats.average_weather_check_time_ms);
    
    if (stats.last_automatic_activation > 0) {
         printf("Last automatic activation: %lld\n", (long long)stats.last_automatic_activation);
    }
    if (stats.last_preheat_activation > 0) {
        printf("Last preheat activation: %lld\n", (long long)stats.last_preheat_activation);
    }
    if (stats.last_manual_activation > 0) {
        printf("Last manual activation: %lld\n", (long long)stats.last_manual_activation);
    }
    
    return AUTONOMY_SUCCESS;
}

// Example: Manual mode control
int example_manual_mode_control(void) {
    printf("INFO: Testing manual mode control...\n");
    
    // Test setting different modes
    snow_melt_mode_t test_modes[] = {
        SNOW_MELT_OFF,
        SNOW_MELT_AUTOMATIC,
        SNOW_MELT_PREHEAT,
        SNOW_MELT_MANUAL
    };
    
    const char *mode_names[] = {
        "OFF",
        "AUTOMATIC", 
        "PREHEAT",
        "MANUAL"
    };
    
    for (int i = 0; i < 4; i++) {
        printf("INFO: Setting mode to %s...\n", mode_names[i]);
        
        int result = starlink_weather_snow_melt_control_set_mode(test_modes[i]);
        if (result != AUTONOMY_SUCCESS) {
            printf("ERROR: Failed to set mode %s: %d\n", mode_names[i], result);
        } else {
            printf("INFO: Successfully set mode to %s\n", mode_names[i]);
        }
        
        sleep(5); // Wait 5 seconds between mode changes
    }
    
    // Set back to automatic mode
    printf("INFO: Setting mode back to AUTOMATIC...\n");
    int result = starlink_weather_snow_melt_control_set_mode(SNOW_MELT_AUTOMATIC);
    if (result != AUTONOMY_SUCCESS) {
        printf("ERROR: Failed to set mode back to AUTOMATIC: %d\n", result);
    } else {
        printf("INFO: Successfully set mode back to AUTOMATIC\n");
    }
    
    return AUTONOMY_SUCCESS;
}

// Example: Cleanup
void example_cleanup(void) {
    printf("INFO: Cleaning up snow melt control system...\n");
    
    // Cleanup UBUS interface
    starlink_weather_snow_melt_ubus_cleanup(NULL);
    
    // Cleanup snow melt control system
    starlink_weather_snow_melt_control_cleanup();
    
    printf("INFO: Cleanup completed\n");
}

// Main example function
int starlink_weather_snow_melt_example_main(int argc, char *argv[]) {
    printf("Starlink Weather-Based Snow Melt Control Integration Example\n");
    printf("============================================================\n");
    
    // Set up signal handlers for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize the system
    int result = example_init_snow_melt_control();
    if (result != AUTONOMY_SUCCESS) {
        printf("ERROR: Initialization failed: %d\n", result);
        return 1;
    }
    
    // Configure the system
    result = example_configure_snow_melt_control();
    if (result != AUTONOMY_SUCCESS) {
        printf("ERROR: Configuration failed: %d\n", result);
        example_cleanup();
        return 1;
    }
    
    // Test manual mode control
    result = example_manual_mode_control();
    if (result != AUTONOMY_SUCCESS) {
        printf("WARN: Manual mode control test failed: %d\n", result);
    }
    
    // Monitor the system
    result = example_monitor_snow_melt_control();
    if (result != AUTONOMY_SUCCESS) {
        printf("WARN: Monitoring failed: %d\n", result);
    }
    
    // Show final statistics
    example_show_statistics();
    
    // Cleanup
    example_cleanup();
    
    printf("INFO: Example completed successfully\n");
    return 0;
}

/*
 * Integration with Main Daemon
 * ============================
 * 
 * To integrate this snow melt control system with the main autonomy daemon:
 * 
 * 1. Add initialization in main daemon startup:
 *    ```c
 *    // In autonomy-daemon.c main() function
 *    int result = starlink_weather_snow_melt_control_init();
 *    if (result != AUTONOMY_SUCCESS) {
 *        LOGX_ERROR_MSG("Failed to initialize snow melt control: %d", result);
 *    }
 *    
 *    result = starlink_weather_snow_melt_ubus_init();
 *    if (result != AUTONOMY_SUCCESS) {
 *        LOGX_WARN_MSG("Failed to initialize snow melt UBUS: %d", result);
 *    }
 *    ```
 * 
 * 2. Add cleanup in main daemon shutdown:
 *    ```c
 *    // In cleanup function
 *    starlink_weather_snow_melt_ubus_cleanup(NULL);
 *    starlink_weather_snow_melt_control_cleanup();
 *    ```
 * 
 * 3. Add periodic weather checks in main loop:
 *    ```c
 *    // In main daemon loop
 *    static time_t last_snow_melt_check = 0;
 *    time_t now = time(NULL);
 *    
 *    if (now - last_snow_melt_check >= 900) { // Every 15 minutes
 *        starlink_weather_snow_melt_control_force_update();
 *        last_snow_melt_check = now;
 *    }
 *    ```
 * 
 * 4. Add the new source files to the Makefile:
 *    ```makefile
 *    SOURCES += starlink/starlink_weather_snow_melt_control.c
 *    SOURCES += starlink/starlink_weather_snow_melt_control_ubus.c
 *    ```
 * 
 * 5. Configure via UCI:
 *    ```bash
 *    # Enable the system
 *    uci set autonomy.snow_melt_control.enabled='1'
 *    
 *    # Set temperature threshold (default: 5.0C)
 *    uci set autonomy.snow_melt_control.temperature_threshold='5.0'
 *    
 *    # Set weather check interval (default: 15 minutes)
 *    uci set autonomy.snow_melt_control.weather_check_interval='15'
 *    
 *    # Set preheat duration (default: 30 minutes)
 *    uci set autonomy.snow_melt_control.preheat_duration='30'
 *    
 *    # Enable forecast usage (default: true)
 *    uci set autonomy.snow_melt_control.use_forecast='1'
 *    
 *    # Set forecast hours ahead (default: 1 hour)
 *    uci set autonomy.snow_melt_control.forecast_hours_ahead='1'
 *    
 *    # Set your OpenWeatherMap API key
 *    uci set autonomy.snow_melt_control.weather_api_key='your_api_key_here'
 *    
 *    # Set Starlink dish IP (default: 192.168.100.1)
 *    uci set autonomy.snow_melt_control.starlink_host='192.168.100.1'
 *    
 *    # Set Starlink dish port (default: 9200)
 *    uci set autonomy.snow_melt_control.starlink_port='9200'
 *    
 *    # Enable debug mode (default: false)
 *    uci set autonomy.snow_melt_control.debug_mode='0'
 *    
 *    # Commit changes
 *    uci commit autonomy
 *    ```
 * 
 * 6. Control via UBUS:
 *    ```bash
 *    # Get current status
 *    ubus call starlink.weather_snow_melt get_status
 *    
 *    # Get configuration
 *    ubus call starlink.weather_snow_melt get_config
 *    
 *    # Enable/disable system
 *    ubus call starlink.weather_snow_melt set_enabled '{"enabled": true}'
 *    
 *    # Set mode manually
 *    ubus call starlink.weather_snow_melt set_mode '{"mode": "automatic"}'
 *    ubus call starlink.weather_snow_melt set_mode '{"mode": "preheat"}'
 *    ubus call starlink.weather_snow_melt set_mode '{"mode": "off"}'
 *    
 *    # Force weather check and mode update
 *    ubus call starlink.weather_snow_melt force_update
 *    
 *    # Get statistics
 *    ubus call starlink.weather_snow_melt get_statistics
 *    
 *    # Reset statistics
 *    ubus call starlink.weather_snow_melt reset_statistics
 *    ```
 * 
 * Snow Melt Control Logic:
 * =======================
 * 
 * The system implements the following logic based on your requirements:
 * 
 * 1. SNOW_MELT_OFF: When temperature is above +5C
 * 2. SNOW_MELT_AUTOMATIC: When temperature is below +5C but no precipitation
 * 3. SNOW_MELT_PREHEAT: When snow or heavy rain is expected within 30 minutes (or 1 hour if forecast available)
 * 4. SNOW_MELT_PREHEAT: When current weather shows snow or heavy rain
 * 
 * The system uses OpenWeatherMap API to get current weather and forecast data,
 * then sends appropriate gRPC commands to the Starlink dish to control the
 * snow melt heating system.
 */
