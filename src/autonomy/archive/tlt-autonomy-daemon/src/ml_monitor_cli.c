#include "ml_monitor_analytics.h"
#include "ml_monitor_network_discovery_integration.h"
#include "../utils/logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>

// Command-line ML monitoring tool

static bool g_running = true;

void signal_handler(int sig) {
    g_running = false;
    printf("\n Stopping ML monitor...\n");
}

void print_usage(const char *program_name) {
    printf("ML Network Intelligence Monitor\n");
    printf("===============================\n\n");
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("Options:\n");
    printf("  -s, --summary              Show overall ML summary\n");
    printf("  -i, --interface <name>     Show specific interface details\n");
    printf("  -a, --accuracy [hours]     Show prediction accuracy (default: 24h)\n");
    printf("  -I, --impact [hours]       Show ML impact summary (default: 24h)\n");
    printf("  -w, --watch [seconds]      Watch mode with refresh interval (default: 5s)\n");
    printf("  -l, --list                 List all monitored interfaces\n");
    printf("  -e, --export <format>      Export data (json, csv, binary)\n");
    printf("  -h, --help                 Show this help\n\n");
    printf("Examples:\n");
    printf("  %s --summary               # Show overall summary\n", program_name);
    printf("  %s --interface eth1        # Show eth1 details\n", program_name);
    printf("  %s --watch 10              # Watch mode, update every 10s\n", program_name);
    printf("  %s --accuracy 6            # Show 6-hour accuracy trend\n", program_name);
    printf("  %s --export json           # Export data as JSON\n", program_name);
}

void print_summary() {
    printf(" ML Network Intelligence Summary\n");
    printf("==================================\n\n");
    
    ml_analytics_data_t analytics;
    int result = ml_monitor_analytics_get_data(&analytics);
    
    if (result != ML_MONITOR_SUCCESS) {
        printf(" Failed to get analytics data (code: %d)\n", result);
        return;
    }
    
    // Overall statistics
    printf(" Overall Statistics:\n");
    printf("  Total Predictions: %u\n", analytics.summary_stats.total_predictions);
    printf("  Correct Predictions: %u\n", analytics.summary_stats.correct_predictions);
    printf("  Overall Accuracy: %.1f%%\n", analytics.summary_stats.overall_accuracy_pct);
    printf("  ML Actions Taken: %u\n", analytics.summary_stats.ml_triggered_actions);
    printf("  Successful Optimizations: %u\n", analytics.summary_stats.successful_optimizations);
    
    if (analytics.summary_stats.total_improvement_ms > 0) {
        if (analytics.summary_stats.total_improvement_ms >= 60000) {
            printf("  Total Time Saved: %.1f minutes\n", analytics.summary_stats.total_improvement_ms / 60000.0);
        } else if (analytics.summary_stats.total_improvement_ms >= 1000) {
            printf("  Total Time Saved: %.1f seconds\n", analytics.summary_stats.total_improvement_ms / 1000.0);
        } else {
            printf("  Total Time Saved: %d milliseconds\n", analytics.summary_stats.total_improvement_ms);
        }
    }
    
    printf("  Average User Experience: %.1f/100\n", analytics.summary_stats.average_user_experience);
    
    time_t uptime = time(NULL) - analytics.summary_stats.stats_start_time;
    printf("  System Uptime: %lld hours\n\n", (long long)(uptime / 3600));
    
    // Per-interface summary
    printf(" Interface Summary:\n");
    bool found_active = false;
    for (int i = 0; i < MAX_INTERFACES; i++) {
        if (!analytics.interface_summary[i].is_active) continue;
        found_active = true;
        
        printf("  %s:\n", analytics.interface_summary[i].interface_id);
        printf("    Predictions: %u (%u correct)\n", 
               analytics.interface_summary[i].predictions_made,
               analytics.interface_summary[i].predictions_correct);
        printf("    Accuracy: %.1f%%\n", analytics.interface_summary[i].accuracy_pct);
        printf("    Current Score: %.1f\n", analytics.interface_summary[i].current_score);
        printf("    Best Score: %.1f\n", analytics.interface_summary[i].best_score);
        printf("    Worst Score: %.1f\n", analytics.interface_summary[i].worst_score);
        
        time_t last_update_ago = time(NULL) - analytics.interface_summary[i].last_update;
        printf("    Last Update: %lld minutes ago\n", (long long)(last_update_ago / 60));
        printf("\n");
    }
    
    if (!found_active) {
        printf("  No active interfaces found\n\n");
    }
}

void print_interface_details(const char *interface_name) {
    printf(" Interface Details: %s\n", interface_name);
    printf("========================\n\n");
    
    ml_interface_score_t score;
    int result = ml_monitor_analytics_calculate_interface_score(interface_name, &score);
    
    if (result != ML_MONITOR_SUCCESS) {
        printf(" Failed to get interface details (code: %d)\n", result);
        return;
    }
    
    // Current score
    const char *rating;
    if (score.overall_score >= 90.0) rating = "EXCELLENT";
    else if (score.overall_score >= 80.0) rating = "VERY GOOD";
    else if (score.overall_score >= 70.0) rating = "GOOD";
    else if (score.overall_score >= 60.0) rating = "FAIR";
    else if (score.overall_score >= 50.0) rating = "POOR";
    else rating = "VERY POOR";
    
    printf(" Overall ML Score: %.1f (%s)\n\n", score.overall_score, rating);
    
    // Component scores
    printf(" Component Scores:\n");
    printf("  Accuracy Score: %.1f\n", score.accuracy_score);
    printf("  Stability Score: %.1f\n", score.stability_score);
    printf("  Performance Score: %.1f\n", score.performance_score);
    printf("  Trend Score: %.1f\n\n", score.trend_score);
    
    // Current metrics
    printf(" Current Metrics:\n");
    printf("  Latency: %u ms\n", score.current_latency_ms);
    printf("  Packet Loss: %u%%\n", score.current_loss_pct);
    printf("  Signal Strength: %d dBm\n", score.current_signal_dbm);
    printf("  Stable Minutes: %u\n", score.consecutive_stable_minutes);
    printf("  Recent Predictions Correct: %u/10\n\n", score.recent_predictions_correct);
    
    // Score contributors
    printf(" Score Contributors (Impact on Overall Score):\n");
    printf("  Latency Impact: %+.1f\n", score.score_contributors.latency_impact);
    printf("  Loss Impact: %+.1f\n", score.score_contributors.loss_impact);
    printf("  Signal Impact: %+.1f\n", score.score_contributors.signal_impact);
    printf("  Prediction Impact: %+.1f\n", score.score_contributors.prediction_impact);
    printf("  Stability Impact: %+.1f\n", score.score_contributors.stability_impact);
    printf("  Trend Impact: %+.1f\n\n", score.score_contributors.trend_impact);
}

void print_accuracy_trend(uint32_t hours) {
    printf(" Prediction Accuracy Trend (%u hours)\n", hours);
    printf("======================================\n\n");
    
    double accuracy_pct;
    int trend_direction;
    
    int result = ml_monitor_analytics_get_accuracy_trend(NULL, hours, &accuracy_pct, &trend_direction);
    
    if (result != ML_MONITOR_SUCCESS) {
        printf(" Failed to get accuracy trend (code: %d)\n", result);
        return;
    }
    
    printf("Overall Accuracy: %.1f%%\n", accuracy_pct);
    
    const char *trend_desc;
    const char *trend_icon;
    if (trend_direction > 0) {
        trend_desc = "IMPROVING";
        trend_icon = "";
    } else if (trend_direction < 0) {
        trend_desc = "DECLINING";
        trend_icon = "";
    } else {
        trend_desc = "STABLE";
        trend_icon = "";
    }
    
    printf("Trend: %s %s\n\n", trend_icon, trend_desc);
    
    // Per-interface accuracy
    ml_analytics_data_t analytics;
    result = ml_monitor_analytics_get_data(&analytics);
    if (result == ML_MONITOR_SUCCESS) {
        printf("Per-Interface Accuracy:\n");
        for (int i = 0; i < MAX_INTERFACES; i++) {
            if (!analytics.interface_summary[i].is_active) continue;
            
            double iface_accuracy;
            int iface_trend;
            ml_monitor_analytics_get_accuracy_trend(analytics.interface_summary[i].interface_id, 
                                                   hours, &iface_accuracy, &iface_trend);
            
            const char *iface_trend_icon = iface_trend > 0 ? "" : iface_trend < 0 ? "" : "";
            printf("  %s: %.1f%% %s\n", 
                   analytics.interface_summary[i].interface_id, 
                   iface_accuracy, 
                   iface_trend_icon);
        }
    }
    
    printf("\n");
}

void print_impact_summary(uint32_t hours) {
    printf(" ML Impact Summary (%u hours)\n", hours);
    printf("=============================\n\n");
    
    int32_t total_improvement_ms;
    double stability_improvement_pct;
    uint32_t actions_taken;
    
    int result = ml_monitor_analytics_get_impact_summary(hours, &total_improvement_ms, 
                                                        &stability_improvement_pct, &actions_taken);
    
    if (result != ML_MONITOR_SUCCESS) {
        printf(" Failed to get impact summary (code: %d)\n", result);
        return;
    }
    
    printf("ML Actions Taken: %u\n", actions_taken);
    
    if (total_improvement_ms > 0) {
        if (total_improvement_ms >= 60000) {
            printf("Total Time Saved: %.1f minutes\n", total_improvement_ms / 60000.0);
        } else if (total_improvement_ms >= 1000) {
            printf("Total Time Saved: %.1f seconds\n", total_improvement_ms / 1000.0);
        } else {
            printf("Total Time Saved: %d milliseconds\n", total_improvement_ms);
        }
    } else if (total_improvement_ms == 0) {
        printf("Total Time Saved: No measurable impact\n");
    } else {
        printf("Total Time Impact: %d ms (negative)\n", total_improvement_ms);
    }
    
    printf("Stability Improvement: %.1f%%\n", stability_improvement_pct);
    
    // Impact assessment
    if (total_improvement_ms > 300000) {
        printf("Assessment:  MAJOR IMPROVEMENT\n");
    } else if (total_improvement_ms > 60000) {
        printf("Assessment:  SIGNIFICANT IMPROVEMENT\n");
    } else if (total_improvement_ms > 10000) {
        printf("Assessment:  MODERATE IMPROVEMENT\n");
    } else if (total_improvement_ms > 0) {
        printf("Assessment:  MINOR IMPROVEMENT\n");
    } else if (total_improvement_ms == 0) {
        printf("Assessment:  NO MEASURABLE IMPACT\n");
    } else {
        printf("Assessment:  NEGATIVE IMPACT\n");
    }
    
    printf("\n");
}

void print_interface_list() {
    printf(" Monitored Interfaces\n");
    printf("======================\n\n");
    
    ml_analytics_data_t analytics;
    int result = ml_monitor_analytics_get_data(&analytics);
    
    if (result != ML_MONITOR_SUCCESS) {
        printf(" Failed to get interface list (code: %d)\n", result);
        return;
    }
    
    bool found_active = false;
    for (int i = 0; i < MAX_INTERFACES; i++) {
        if (!analytics.interface_summary[i].is_active) continue;
        found_active = true;
        
        printf(" %s\n", analytics.interface_summary[i].interface_id);
        printf("   Score: %.1f", analytics.interface_summary[i].current_score);
        
        if (analytics.interface_summary[i].current_score >= 90.0) {
            printf(" (EXCELLENT)\n");
        } else if (analytics.interface_summary[i].current_score >= 80.0) {
            printf(" (VERY GOOD)\n");
        } else if (analytics.interface_summary[i].current_score >= 70.0) {
            printf(" (GOOD)\n");
        } else if (analytics.interface_summary[i].current_score >= 60.0) {
            printf(" (FAIR)\n");
        } else {
            printf(" (POOR)\n");
        }
        
        printf("   Accuracy: %.1f%% (%u/%u predictions)\n", 
               analytics.interface_summary[i].accuracy_pct,
               analytics.interface_summary[i].predictions_correct,
               analytics.interface_summary[i].predictions_made);
        printf("\n");
    }
    
    if (!found_active) {
        printf("No interfaces currently being monitored.\n\n");
    }
}

void watch_mode(int refresh_seconds) {
    printf(" ML Monitor Watch Mode (refresh: %ds)\n", refresh_seconds);
    printf("Press Ctrl+C to exit\n\n");
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    while (g_running) {
        // Clear screen
        printf("\033[2J\033[H");
        
        // Show timestamp
        time_t now = time(NULL);
        printf(" %s", ctime(&now));
        
        // Show summary
        print_summary();
        
        // Wait for next refresh
        for (int i = 0; i < refresh_seconds && g_running; i++) {
            sleep(1);
        }
    }
}

int export_data(const char *format) {
    printf(" Exporting ML analytics data (%s format)\n", format);
    
    char filename[256];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    snprintf(filename, sizeof(filename), "ml_analytics_%04d%02d%02d_%02d%02d%02d.%s",
             tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, format);
    
    int result = ml_monitor_analytics_export_data(format, filename);
    
    if (result == ML_MONITOR_SUCCESS) {
        printf(" Data exported to: %s\n", filename);
    } else {
        printf(" Export failed (code: %d)\n", result);
    }
    
    return result;
}

int main(int argc, char *argv[]) {
    static struct option long_options[] = {
        {"summary", no_argument, 0, 's'},
        {"interface", required_argument, 0, 'i'},
        {"accuracy", optional_argument, 0, 'a'},
        {"impact", optional_argument, 0, 'I'},
        {"watch", optional_argument, 0, 'w'},
        {"list", no_argument, 0, 'l'},
        {"export", required_argument, 0, 'e'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    // Initialize ML analytics
    int init_result = ml_monitor_analytics_init();
    if (init_result != ML_MONITOR_SUCCESS) {
        fprintf(stderr, "Failed to initialize ML analytics: %d\n", init_result);
        return 1;
    }
    
    // Parse command line arguments
    while ((c = getopt_long(argc, argv, "si:a::I::w::le:h", long_options, &option_index)) != -1) {
        switch (c) {
            case 's':
                print_summary();
                break;
                
            case 'i':
                print_interface_details(optarg);
                break;
                
            case 'a': {
                uint32_t hours = optarg ? atoi(optarg) : 24;
                if (hours == 0) hours = 24;
                print_accuracy_trend(hours);
                break;
            }
            
            case 'I': {
                uint32_t hours = optarg ? atoi(optarg) : 24;
                if (hours == 0) hours = 24;
                print_impact_summary(hours);
                break;
            }
            
            case 'w': {
                int seconds = optarg ? atoi(optarg) : 5;
                if (seconds < 1) seconds = 5;
                watch_mode(seconds);
                break;
            }
            
            case 'l':
                print_interface_list();
                break;
                
            case 'e':
                export_data(optarg);
                break;
                
            case 'h':
                print_usage(argv[0]);
                break;
                
            case '?':
                print_usage(argv[0]);
                return 1;
                
            default:
                break;
        }
    }
    
    // If no arguments provided, show summary
    if (argc == 1) {
        print_summary();
    }
    
    ml_monitor_analytics_cleanup();
    return 0;
}