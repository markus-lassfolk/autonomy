#include "package/utils/tlt-autonomy-daemon/src/modules/starlink/starlink_tracker.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

static bool running = true;

// Signal handler for graceful shutdown
void signal_handler(int sig) {
    printf("Received signal %d, shutting down...\n", sig);
    running = false;
}

// Outage callback function
void outage_alert_callback(const outage_prediction_t *prediction, void *user_data) {
    printf("🚨 OUTAGE ALERT 🚨\n");
    printf("  Time: %s", ctime(&prediction->start_time));
    printf("  Duration: %d seconds\n", prediction->duration_seconds);
    printf("  Risk Level: %d\n", prediction->risk_level);
    printf("  Description: %s\n", prediction->description);
    printf("  Available Satellites: %d\n", prediction->predicted_available_sats);
    printf("  Confidence: %.2f\n", prediction->confidence_score);
    printf("\n");
}

int main(int argc, char *argv[]) {
    printf("🛰️  Starlink Tracking Demo\n");
    printf("==========================\n\n");
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize tracker configuration
    starlink_tracker_config_t config = {
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
    starlink_tracker_t *tracker = starlink_tracker_init(&config);
    if (!tracker) {
        printf("❌ Failed to initialize Starlink tracker\n");
        return 1;
    }
    
    printf("✅ Tracker initialized successfully\n\n");
    
    // Set outage callback
    starlink_tracker_set_outage_callback(tracker, outage_alert_callback, NULL);
    
    // Initial data collection
    printf("📡 Collecting initial data...\n");
    
    printf("  - Updating dish location...");
    int location_result = starlink_tracker_update_dish_location(tracker);
    printf(" %s\n", (location_result == TRACKER_SUCCESS) ? "✅" : "❌");
    
    printf("  - Updating obstruction map...");
    int obstruction_result = starlink_tracker_update_obstruction_map(tracker);
    printf(" %s\n", (obstruction_result == TRACKER_SUCCESS) ? "✅" : "❌");
    
    printf("  - Fetching satellite data...");
    int constellation_result = starlink_tracker_update_constellation_data(tracker);
    printf(" %s\n", (constellation_result == TRACKER_SUCCESS) ? "✅" : "❌");
    
    if (constellation_result != TRACKER_SUCCESS) {
        printf("⚠️  Warning: Could not fetch satellite data. Check Space-Track credentials and network connectivity.\n");
    }
    
    // Calculate initial predictions
    printf("  - Calculating predictions...");
    int prediction_result = starlink_tracker_calculate_predictions(tracker, config.prediction_horizon_hours);
    printf(" %s\n\n", (prediction_result == TRACKER_SUCCESS) ? "✅" : "❌");
    
    // Show current status
    printf("📊 Current Status:\n");
    printf("  - Visible satellites: %d\n", starlink_tracker_get_visible_satellite_count(tracker));
    printf("  - Unobstructed satellites: %d\n", starlink_tracker_get_unobstructed_satellite_count(tracker));
    
    // Get and display predictions
    outage_prediction_t *predictions;
    int num_predictions = starlink_tracker_get_predictions(tracker, &predictions);
    
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
        
        starlink_tracker_free_predictions(predictions, num_predictions);
    } else {
        printf("  ✅ No outages predicted!\n");
    }
    
    // Start monitoring
    printf("\n🔄 Starting continuous monitoring...\n");
    int monitoring_result = starlink_tracker_start_monitoring(tracker);
    if (monitoring_result == TRACKER_SUCCESS) {
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
                printf("  - Visible satellites: %d\n", starlink_tracker_get_visible_satellite_count(tracker));
                printf("  - Unobstructed satellites: %d\n", starlink_tracker_get_unobstructed_satellite_count(tracker));
                
                const tracking_stats_t *stats = starlink_tracker_get_stats(tracker);
                printf("  - Total predictions: %d\n", stats->total_predictions);
                printf("  - Accuracy: %.1f%%\n", stats->accuracy_percentage);
                printf("\n");
            }
        }
        
        // Stop monitoring
        printf("🛑 Stopping monitoring...\n");
        starlink_tracker_stop_monitoring(tracker);
    } else {
        printf("❌ Failed to start monitoring\n");
    }
    
    // Cleanup
    printf("🧹 Cleaning up...\n");
    starlink_tracker_cleanup(tracker);
    
    printf("✅ Demo completed successfully\n");
    return 0;
}