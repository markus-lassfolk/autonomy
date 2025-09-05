#include "starlink_tracker_standalone.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <sys/time.h>

static bool running = true;
static standalone_tracker_t *g_tracker = NULL;

// Signal handler
void signal_handler(int sig) {
    printf("\n🛑 Received signal %d, shutting down gracefully...\n", sig);
    running = false;
}

// Outage callback
void outage_callback(const standalone_outage_prediction_t *prediction, void *user_data) {
    printf("\n🚨 OUTAGE ALERT 🚨\n");
    printf("  Time: %s", ctime(&prediction->start_time));
    printf("  Duration: %d seconds (%d minutes)\n", 
           prediction->duration_seconds, prediction->duration_seconds / 60);
    printf("  Risk Level: %d ", prediction->risk_level);
    
    switch (prediction->risk_level) {
        case 1: printf("(Low)\n"); break;
        case 2: printf("(Medium)\n"); break;
        case 3: printf("(High)\n"); break;
        case 4: printf("(Critical)\n"); break;
        default: printf("(Unknown)\n"); break;
    }
    
    printf("  Description: %s\n", prediction->description);
    printf("  Available Satellites: %d\n", prediction->predicted_available_sats);
    printf("  Confidence: %.2f\n", prediction->confidence_score);
    printf("\n");
}

// Log callback
void log_callback(int level, const char *message, void *user_data) {
    const char *level_names[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    const char *level_name = (level >= 0 && level <= 3) ? level_names[level] : "UNKNOWN";
    
    printf("[%s] %s\n", level_name, message);
}

// Print usage
void print_usage(const char *program_name) {
    printf("🛰️ Starlink Tracker - Standalone Version\n");
    printf("=========================================\n\n");
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("Options:\n");
    printf("  -c, --config FILE     Configuration file path\n");
    printf("  -u, --username USER   Space-Track username\n");
    printf("  -p, --password PASS   Space-Track password\n");
    printf("  -i, --ip IP           Starlink dish IP (default: 192.168.100.1)\n");
    printf("  -P, --port PORT       Starlink dish port (default: 9200)\n");
    printf("  -w, --web-port PORT   HTTP API port (default: 8080)\n");
    printf("  -h, --help            Show this help\n");
    printf("  -v, --verbose         Enable verbose logging\n");
    printf("  -q, --quiet           Quiet mode (errors only)\n");
    printf("\n");
    printf("Environment Variables:\n");
    printf("  SPACE_TRACK_USERNAME  Space-Track username\n");
    printf("  SPACE_TRACK_PASSWORD  Space-Track password\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --username myuser --password mypass\n", program_name);
    printf("  %s --config /etc/starlink_tracker.conf\n", program_name);
    printf("  %s --web-port 8080 --verbose\n", program_name);
    printf("\n");
}

// Print status
void print_status(standalone_tracker_t *tracker) {
    if (!tracker) {
        return;
    }
    
    standalone_stats_t stats = standalone_tracker_get_stats(tracker);
    
    printf("\n📊 Current Status:\n");
    printf("  ├─ Visible satellites: %d\n", stats.visible_satellites);
    printf("  ├─ Unobstructed satellites: %d\n", stats.unobstructed_satellites);
    printf("  ├─ Obstruction level: %.1f%%\n", stats.obstruction_percentage);
    printf("  ├─ Total predictions: %d\n", stats.total_predictions);
    printf("  ├─ Accuracy: %.1f%%\n", stats.accuracy_percentage);
    printf("  └─ Last update: %s", ctime(&stats.last_update));
    
    // Show recent predictions
    standalone_outage_prediction_t *predictions;
    int num_predictions = standalone_tracker_get_predictions(tracker, &predictions);
    
    if (num_predictions > 0) {
        printf("\n🔮 Active Predictions:\n");
        for (int i = 0; i < num_predictions; i++) {
            printf("  %d. %s", i + 1, ctime(&predictions[i].start_time));
            printf("     Duration: %d minutes, Risk: %d, Confidence: %.2f\n",
                   predictions[i].duration_seconds / 60,
                   predictions[i].risk_level,
                   predictions[i].confidence_score);
            printf("     %s\n", predictions[i].description);
        }
        
        standalone_tracker_free_predictions(predictions, num_predictions);
    } else {
        printf("\n✅ No outages predicted!\n");
    }
    
    printf("\n");
}

int main(int argc, char *argv[]) {
    printf("🛰️ Starlink Tracker - Standalone Version\n");
    printf("==========================================\n\n");
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Parse command line options
    standalone_config_t config;
    standalone_config_init_defaults(&config);
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"username", required_argument, 0, 'u'},
        {"password", required_argument, 0, 'p'},
        {"ip", required_argument, 0, 'i'},
        {"port", required_argument, 0, 'P'},
        {"web-port", required_argument, 0, 'w'},
        {"help", no_argument, 0, 'h'},
        {"verbose", no_argument, 0, 'v'},
        {"quiet", no_argument, 0, 'q'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "c:u:p:i:P:w:hvq", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c':
                strncpy(config.config_file, optarg, sizeof(config.config_file) - 1);
                break;
            case 'u':
                strncpy(config.space_track_username, optarg, sizeof(config.space_track_username) - 1);
                break;
            case 'p':
                strncpy(config.space_track_password, optarg, sizeof(config.space_track_password) - 1);
                break;
            case 'i':
                strncpy(config.starlink_dish_ip, optarg, sizeof(config.starlink_dish_ip) - 1);
                break;
            case 'P':
                config.starlink_dish_port = atoi(optarg);
                break;
            case 'w':
                config.http_api_port = atoi(optarg);
                break;
            case 'v':
                config.log_level = 0; // Debug
                break;
            case 'q':
                config.log_level = 3; // Error only
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Load configuration file if specified
    if (config.config_file[0]) {
        printf("📋 Loading configuration from %s...\n", config.config_file);
        standalone_config_load_from_file(config.config_file, &config);
    }
    
    // Override with environment variables if present
    const char *env_username = getenv("SPACE_TRACK_USERNAME");
    const char *env_password = getenv("SPACE_TRACK_PASSWORD");
    
    if (env_username) {
        strncpy(config.space_track_username, env_username, sizeof(config.space_track_username) - 1);
    }
    
    if (env_password) {
        strncpy(config.space_track_password, env_password, sizeof(config.space_track_password) - 1);
    }
    
    // Validate configuration
    if (!config.space_track_username[0] || !config.space_track_password[0]) {
        printf("❌ Error: Space-Track credentials required!\n\n");
        printf("Set them via:\n");
        printf("  1. Command line: --username USER --password PASS\n");
        printf("  2. Environment: export SPACE_TRACK_USERNAME=USER SPACE_TRACK_PASSWORD=PASS\n");
        printf("  3. Config file: --config /path/to/config.conf\n\n");
        return 1;
    }
    
    printf("✅ Configuration loaded successfully\n");
    printf("  📡 Dish: %s:%d\n", config.starlink_dish_ip, config.starlink_dish_port);
    printf("  🌐 Web API: http://localhost:%d\n", config.http_api_port);
    printf("  🔄 Update interval: %d minutes\n", config.update_interval_minutes);
    printf("  🔮 Prediction horizon: %d hours\n\n", config.prediction_horizon_hours);
    
    // Initialize tracker
    printf("🔧 Initializing Starlink tracker...\n");
    g_tracker = standalone_tracker_init(&config);
    if (!g_tracker) {
        printf("❌ Failed to initialize tracker\n");
        return 1;
    }
    
    // Set callbacks
    standalone_tracker_set_log_callback(g_tracker, log_callback, NULL);
    standalone_tracker_set_outage_callback(g_tracker, outage_callback, NULL);
    
    printf("✅ Tracker initialized successfully\n\n");
    
    // Initial data collection
    printf("📡 Collecting initial data...\n");
    
    printf("  📍 Updating dish location...");
    fflush(stdout);
    int location_result = standalone_tracker_update_dish_location(g_tracker);
    printf(" %s\n", (location_result == STANDALONE_SUCCESS) ? "✅" : "❌");
    
    printf("  🗺️ Updating obstruction map...");
    fflush(stdout);
    int obstruction_result = standalone_tracker_update_obstruction_map(g_tracker);
    printf(" %s\n", (obstruction_result == STANDALONE_SUCCESS) ? "✅" : "❌");
    
    printf("  🛰️ Fetching satellite data...");
    fflush(stdout);
    int constellation_result = standalone_tracker_update_constellation_data(g_tracker);
    printf(" %s\n", (constellation_result == STANDALONE_SUCCESS) ? "✅" : "❌");
    
    printf("  🔮 Calculating predictions...");
    fflush(stdout);
    int prediction_result = standalone_tracker_calculate_predictions(g_tracker);
    printf(" %s\n", (prediction_result == STANDALONE_SUCCESS) ? "✅" : "❌");
    
    // Show initial status
    print_status(g_tracker);
    
    // Start monitoring
    printf("🔄 Starting continuous monitoring...\n");
    int monitoring_result = standalone_tracker_start_monitoring(g_tracker);
    if (monitoring_result == STANDALONE_SUCCESS) {
        printf("✅ Monitoring started successfully\n\n");
        
        if (config.enable_web_interface) {
            printf("🌐 Web interface available at: http://localhost:%d\n", config.http_api_port);
            printf("📊 API endpoints:\n");
            printf("   http://localhost:%d/api/status\n", config.http_api_port);
            printf("   http://localhost:%d/api/predictions\n", config.http_api_port);
            printf("   http://localhost:%d/api/satellites\n\n", config.http_api_port);
        }
        
        printf("Press Ctrl+C to stop, 's' + Enter for status...\n\n");
        
        // Main loop
        char input[10];
        while (running) {
            // Non-blocking input check
            fd_set readfds;
            struct timeval timeout;
            FD_ZERO(&readfds);
            FD_SET(STDIN_FILENO, &readfds);
            timeout.tv_sec = 5;
            timeout.tv_usec = 0;
            
            int result = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
            
            if (result > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
                if (fgets(input, sizeof(input), stdin)) {
                    if (input[0] == 's' || input[0] == 'S') {
                        print_status(g_tracker);
                    }
                }
            }
            
            if (!running) {
                break;
            }
        }
        
        // Stop monitoring
        printf("🛑 Stopping monitoring...\n");
        standalone_tracker_stop_monitoring(g_tracker);
    } else {
        printf("❌ Failed to start monitoring\n");
    }
    
    // Cleanup
    printf("🧹 Cleaning up...\n");
    standalone_tracker_cleanup(g_tracker);
    
    printf("✅ Standalone tracker stopped successfully\n");
    return 0;
}