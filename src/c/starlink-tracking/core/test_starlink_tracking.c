#include "starlink_tracker_standalone.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

static bool running = true;

// Signal handler for graceful shutdown
void test_signal_handler(int sig) {
    printf("Received signal %d, shutting down...\n", sig);
    running = false;
}

// Outage callback function
void outage_alert_callback(const standalone_outage_prediction_t *prediction, void *user_data) {
    printf("🚨 OUTAGE ALERT 🚨\n");
    printf("  Time: %s", ctime(&prediction->start_time));
    printf("  Duration: %d seconds\n", prediction->duration_seconds);
    printf("  Risk Level: %d\n", prediction->risk_level);
    printf("  Description: %s\n", prediction->description);
    printf("  Available Satellites: %d\n", prediction->predicted_available_sats);
    printf("  Confidence: %.2f\n", prediction->confidence_score);
    printf("\n");
}

int test_main(int argc, char *argv[]) {
    printf("🛰️  Starlink Tracking Demo\n");
    printf("==========================\n\n");
    
    // Setup signal handlers
    signal(SIGINT, test_signal_handler);
    signal(SIGTERM, test_signal_handler);
    
    // Initialize tracker configuration
    standalone_config_t config = {
        .starlink_dish_ip = "192.168.100.1",
        .starlink_dish_port = 9200,
        .update_interval_minutes = 5, // Shorter interval for demo
        .prediction_horizon_hours = 12, // 12 hour forecast
        .min_elevation_degrees = 10.0,
        .obstruction_threshold = 0.7,
        .validation_enabled = true,
        .cache_duration_hours = 24,
        .rate_limit_requests_per_minute = 15
    };
    
    // Get credentials from environment
    const char *username = getenv("SPACE_TRACK_USERNAME");
    const char *password = getenv("SPACE_TRACK_PASSWORD");
    
    if (!username || !password) {
        printf("❌ Error: Please set SPACE_TRACK_USERNAME and SPACE_TRACK_PASSWORD environment variables\n");
        printf("   Example: export SPACE_TRACK_USERNAME=your_username\n");
        printf("           export SPACE_TRACK_PASSWORD=your_password\n");
        return 1;
    }
    
    strncpy(config.space_track_username, username, sizeof(config.space_track_username) - 1);
    strncpy(config.space_track_password, password, sizeof(config.space_track_password) - 1);
    
    // Initialize tracker
    printf("🔧 Initializing Starlink tracker...\n");
    standalone_tracker_t *tracker = standalone_tracker_init(&config);
    if (!tracker) {
        printf("❌ Failed to initialize Starlink tracker\n");
        return 1;
    }
    
    printf("✅ Tracker initialized successfully\n\n");
    
    // Set outage callback
    standalone_tracker_set_outage_callback(tracker, outage_alert_callback, NULL);
    
    // Initial data collection
    printf("📡 Collecting initial data...\n");
    
    printf("  - Updating dish location...");
    int location_result = standalone_tracker_update_dish_location(tracker);
    printf(" %s\n", (location_result == STANDALONE_SUCCESS) ? "✅" : "❌");
    
    printf("  - Updating obstruction map...");
    int obstruction_result = standalone_tracker_update_obstruction_map(tracker);
    printf(" %s\n", (obstruction_result == STANDALONE_SUCCESS) ? "✅" : "❌");
    
    printf("  - Fetching satellite data...");
    int constellation_result = standalone_tracker_update_constellation_data(tracker);
    printf(" %s\n", (constellation_result == STANDALONE_SUCCESS) ? "✅" : "❌");
    
    if (constellation_result != STANDALONE_SUCCESS) {
        printf("⚠️  Warning: Could not fetch satellite data. Check Space-Track credentials and network connectivity.\n");
    }
    
    // Calculate initial predictions
    printf("  - Calculating predictions...");
    int prediction_result = standalone_tracker_calculate_predictions(tracker);
    printf(" %s\n\n", (prediction_result == STANDALONE_SUCCESS) ? "✅" : "❌");
    
    // Show current status
    printf("📊 Current Status:\n");
    standalone_stats_t stats = standalone_tracker_get_stats(tracker);
    printf("  - Visible satellites: %d\n", stats.visible_satellites);
    printf("  - Unobstructed satellites: %d\n", stats.unobstructed_satellites);
    
    // Get and display predictions
    standalone_outage_prediction_t *predictions;
    int num_predictions = standalone_tracker_get_predictions(tracker, &predictions);
    
    printf("  - Predictions for next %d hours: %d\n", config.prediction_horizon_hours, num_predictions);
    
    if (num_predictions > 0) {
        printf("\n🔮 Outage Predictions:\n");
        for (int i = 0; i < num_predictions; i++) {
            printf("  %d. %s", i + 1, ctime(&predictions[i].start_time));
            printf("     Duration: %d seconds, Risk: %d, Satellites: %d\n", 
                   predictions[i].duration_seconds, 
                   predictions[i].risk_level, 
                   predictions[i].predicted_available_sats);
            printf("     %s\n", predictions[i].description);
        }
        
        standalone_tracker_free_predictions(predictions, num_predictions);
    } else {
        printf("  ✅ No outages predicted!\n");
    }
    
    // Start monitoring
    printf("\n🔄 Starting continuous monitoring...\n");
    int monitoring_result = standalone_tracker_start_monitoring(tracker);
    if (monitoring_result == STANDALONE_SUCCESS) {
        printf("✅ Monitoring started successfully\n");
        printf("   Press Ctrl+C to stop...\n\n");
        
        // Main monitoring loop
        int loop_count = 0;
        while (running) {
            sleep(30); // Update every 30 seconds
            loop_count++;
            
            // Show periodic status
            if (loop_count % 10 == 0) { // Every 5 minutes
                printf("📈 Status Update:\n");
                standalone_stats_t current_stats = standalone_tracker_get_stats(tracker);
                printf("  - Visible satellites: %d\n", current_stats.visible_satellites);
                printf("  - Unobstructed satellites: %d\n", current_stats.unobstructed_satellites);
                printf("  - Total predictions: %d\n", current_stats.total_predictions);
                printf("  - Accuracy: %.1f%%\n", current_stats.accuracy_percentage);
                printf("\n");
            }
        }
        
        // Stop monitoring
        printf("🛑 Stopping monitoring...\n");
        standalone_tracker_stop_monitoring(tracker);
    } else {
        printf("❌ Failed to start monitoring\n");
    }
    
    // Cleanup
    printf("🧹 Cleaning up...\n");
    standalone_tracker_cleanup(tracker);
    
    printf("✅ Demo completed successfully\n");
    return 0;
}

// Note: This file should be compiled as a separate test executable
// The main function is intentionally removed to avoid conflicts with production code