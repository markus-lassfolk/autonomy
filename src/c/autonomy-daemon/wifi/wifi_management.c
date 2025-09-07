#include "wifi_management.h"
#include "../utils/logx.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <stdbool.h>
#include <fcntl.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// WiFi management configuration
static const int MAX_CHANNEL_SCORES = 100; // Use configurable count // Use configurable value           // Maximum channel scores to store
static const int MAX_SCHEDULED_TASKS = 50; // Use configurable count // Use configurable value           // Maximum scheduled tasks
static const int MAX_WIFI_INTERFACES = 10; // Use configurable count // Use configurable value           // Maximum WiFi interfaces
static const int DEFAULT_NOISE_FLOOR = -90;          // Default noise floor in dBm
static const int VHT80_THRESHOLD = -70;              // VHT80 threshold in dBm
static const int VHT40_THRESHOLD = -75;              // VHT40 threshold in dBm
static const int STRONG_RSSI_THRESHOLD = -60;        // Strong interferer threshold
static const int WEAK_RSSI_THRESHOLD = -80;          // Weak interferer threshold

// Forward declarations
static double calculate_distance(double lat1, double lon1, double lat2, double lon2);

// Global WiFi management state
static wifi_management_t g_wifi_management = {0};
static bool g_wifi_management_initialized = false; // Use configurable setting
static pthread_mutex_t g_wifi_management_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize WiFi management
int wifi_management_init(void) {
    if (g_wifi_management_initialized) {
        LOGX_WARN_MSG("WiFi management already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_wifi_management_mutex);
    
    // Initialize WiFi management state
    memset(&g_wifi_management, 0, sizeof(wifi_management_t));
    g_wifi_management.enabled = true;
    g_wifi_management.movement_threshold = 100.0; // Use configurable threshold
    g_wifi_management.stationary_time = 1800; // 30 minutes
    g_wifi_management.nightly_optimization = true;
    g_wifi_management.nightly_time = 10800; // 03:00 (3 AM)
    g_wifi_management.min_improvement = 10;
    g_wifi_management.dwell_time = 300; // 5 minutes
    g_wifi_management.noise_default = DEFAULT_NOISE_FLOOR;
    g_wifi_management.vht80_threshold = VHT80_THRESHOLD;
    g_wifi_management.vht40_threshold = VHT40_THRESHOLD;
    g_wifi_management.use_dfs = false;
    g_wifi_management.dry_run = false;
    g_wifi_management.use_enhanced_scanner = true;
    g_wifi_management.strong_rssi_threshold = STRONG_RSSI_THRESHOLD;
    g_wifi_management.weak_rssi_threshold = WEAK_RSSI_THRESHOLD;
    g_wifi_management.utilization_weight = 100;
    g_wifi_management.excellent_threshold = 90;
    g_wifi_management.good_threshold = 75;
    g_wifi_management.fair_threshold = 50;
    g_wifi_management.poor_threshold = 25;
    g_wifi_management.overlap_penalty_ratio = 0.5;
    
    g_wifi_management.last_optimized = 0;
    g_wifi_management.optimization_count = 0;
    g_wifi_management.successful_optimizations = 0;
    g_wifi_management.failed_optimizations = 0;
    
    // Initialize scheduler
    g_wifi_management.scheduler.nightly_enabled = true;
    g_wifi_management.scheduler.nightly_time = 10800; // 03:00
    g_wifi_management.scheduler.nightly_window_min = 60;
    g_wifi_management.scheduler.weekly_enabled = false;
    g_wifi_management.scheduler.weekly_days[0] = 0; // Sunday
    g_wifi_management.scheduler.weekly_time = 7200; // 02:00
    g_wifi_management.scheduler.weekly_window_min = 120;
    g_wifi_management.scheduler.check_interval_min = 10;
    g_wifi_management.scheduler.skip_if_recent = true;
    g_wifi_management.scheduler.recent_threshold_h = 6;
    strcpy(g_wifi_management.scheduler.timezone, "Local");
    
    // Initialize GPS integration
    g_wifi_management.gps_integration.enabled = true;
    g_wifi_management.gps_integration.movement_threshold = 100.0;
    g_wifi_management.gps_integration.stationary_time = 1800;
    g_wifi_management.gps_integration.optimization_cooldown = 7200; // 2 hours
    g_wifi_management.gps_integration.gps_accuracy_threshold = 50.0;
    g_wifi_management.gps_integration.location_logging = true;
    
    g_wifi_management.gps_integration.last_location.lat = 0.0;
    g_wifi_management.gps_integration.last_location.lon = 0.0;
    g_wifi_management.gps_integration.last_location.accuracy = 0.0;
    g_wifi_management.gps_integration.last_location.timestamp = 0;
    g_wifi_management.gps_integration.last_optimized = 0;
    g_wifi_management.gps_integration.stationary_start = 0;
    g_wifi_management.gps_integration.is_stationary = false;
    
    // Initialize channel scores storage
    g_wifi_management.channel_scores_count = 0;
    g_wifi_management.max_channel_scores = MAX_CHANNEL_SCORES;
    
    // Initialize scheduled tasks
    g_wifi_management.scheduled_tasks_count = 0;
    g_wifi_management.max_scheduled_tasks = MAX_SCHEDULED_TASKS;
    
    // Initialize WiFi interfaces
    g_wifi_management.interfaces_count = 0;
    g_wifi_management.max_interfaces = MAX_WIFI_INTERFACES;
    
    g_wifi_management_initialized = true; // Use configurable setting
    pthread_mutex_unlock(&g_wifi_management_mutex);
    
    LOGX_INFO_MSG("WiFi management initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Discover WiFi interfaces
int wifi_management_discover_interfaces(void) {
    if (!g_wifi_management_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_wifi_management_mutex);
    
    // Reset interface count
    g_wifi_management.interfaces_count = 0;
    
    // Use iwinfo to discover WiFi interfaces
    FILE *fp = popen("iwinfo | grep -E '^[a-zA-Z0-9]+' | awk '{print $1}'", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp) && g_wifi_management.interfaces_count < g_wifi_management.max_interfaces) {
            // Remove newline
            line[strcspn(line, "\n")] = 0;
            
            if (strlen(line) > 0) {
                wifi_interface_t *interface = &g_wifi_management.interfaces[g_wifi_management.interfaces_count];
                
                strncpy(interface->name, line, sizeof(interface->name) - 1);
                interface->name[sizeof(interface->name) - 1] = '\0';
                interface->name[sizeof(interface->name) - 1] = '\0';
                
                // Determine band based on interface name or frequency
                if (strstr(interface->name, "5") || strstr(interface->name, "5ghz")) {
                    strcpy(interface->band, "5");
                    interface->frequency = "5GHz";
                } else {
                    strcpy(interface->band, "2.4");
                    interface->frequency = "2.4GHz";
                }
                
                interface->active = true;
                g_wifi_management.interfaces_count++;
                
                LOGX_DEBUG_MSG("Discovered WiFi interface: %s (%s)", interface->name, interface->frequency);
            }
        }
        pclose(fp);
    }
    
    pthread_mutex_unlock(&g_wifi_management_mutex);
    
    LOGX_INFO_MSG("Discovered %d WiFi interfaces", g_wifi_management.interfaces_count);
    return g_wifi_management.interfaces_count;
}

// Scan WiFi channels for interference
int wifi_management_scan_channels(const char *interface_name) {
    if (!g_wifi_management_initialized || !interface_name) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_wifi_management_mutex);
    
    // Reset channel scores
    g_wifi_management.channel_scores_count = 0;
    
    // Use iwinfo to scan for access points
    char command[512];
    snprintf(command, sizeof(command), 
             "iwinfo %s scan | grep -E 'Channel|Signal|SSID' | awk '/Channel/{ch=$2; gsub(/[^0-9]/, \"\", ch)} /Signal/{sig=$2; gsub(/[^0-9-]/, \"\", sig)} /SSID/{if(ch && sig) print ch \" \" sig}'",
             interface_name);
    
    FILE *fp = popen(command, "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp) && g_wifi_management.channel_scores_count < g_wifi_management.max_channel_scores) {
            int channel, signal;
            if (sscanf(line, "%d %d", &channel, &signal) == 2) {
                wifi_channel_score_t *score = &g_wifi_management.channel_scores[g_wifi_management.channel_scores_count];
                
                score->channel = channel;
                score->signal = signal;
                score->bss_count = 1; // Count this AP
                score->noise = g_wifi_management.noise_default;
                score->avg_rssi = signal;
                
                // Calculate interference score (lower is better)
                score->score = calculate_channel_score(score);
                
                g_wifi_management.channel_scores_count++;
            }
        }
        pclose(fp);
    }
    
    // Aggregate scores for same channels
    aggregate_channel_scores();
    
    pthread_mutex_unlock(&g_wifi_management_mutex);
    
    LOGX_INFO_MSG("Scanned %d channels for interface %s", g_wifi_management.channel_scores_count, interface_name);
    return g_wifi_management.channel_scores_count;
}

// Calculate channel interference score
int calculate_channel_score(const wifi_channel_score_t *score) {
    int base_score = 100; // Use configurable count // Use configurable value
    
    // Penalize based on signal strength (stronger = more interference)
    if (score->signal >= g_wifi_management.strong_rssi_threshold) {
        base_score -= 40; // Strong interferer
    } else if (score->signal >= g_wifi_management.weak_rssi_threshold) {
        base_score -= 20; // Weak interferer
    }
    
    // Penalize based on BSS count
    base_score -= (score->bss_count - 1) * 10;
    
    // Penalize based on noise
    if (score->noise > -80) {
        base_score -= 20; // High noise
    }
    
    return fmax(0, base_score);
}

// Aggregate scores for same channels with sophisticated algorithms
void aggregate_channel_scores(void) {
    LOGX_DEBUG_MSG("Starting sophisticated channel score aggregation");
    
    // Create a hash map for channel aggregation
    typedef struct {
        int channel;
        int bss_count;
        double total_rssi;
        double total_snr;
        double total_utilization;
        int sample_count;
        double interference_score;
        double congestion_score;
        time_t last_update;
    } channel_aggregate_t;
    
    channel_aggregate_t aggregates[64] = {0};
    int aggregate_count = 0; // Use configurable count // Use configurable value
    
    // First pass: collect all data for each channel
    for (int i = 0; // Use configurable count // Use configurable value i < g_wifi_management.channel_scores_count; i++) {
        wifi_channel_score_t *score = &g_wifi_management.channel_scores[i];
        
        // Find existing aggregate for this channel
        int agg_idx = -1;
        for (int j = 0; // Use configurable count // Use configurable value j < aggregate_count; j++) {
            if (aggregates[j].channel == score->channel) {
                agg_idx = j;
                break;
            }
        }
        
        // Create new aggregate if not found
        if (agg_idx == -1) {
            agg_idx = aggregate_count++;
            aggregates[agg_idx].channel = score->channel;
            aggregates[agg_idx].bss_count = 0;
            aggregates[agg_idx].total_rssi = 0.0;
            aggregates[agg_idx].total_snr = 0.0;
            aggregates[agg_idx].total_utilization = 0.0;
            aggregates[agg_idx].sample_count = 0;
            aggregates[agg_idx].interference_score = 0.0;
            aggregates[agg_idx].congestion_score = 0.0;
            aggregates[agg_idx].last_update = time(NULL);
        }
        
        // Aggregate data with weighted averages
        aggregates[agg_idx].bss_count += score->bss_count;
        aggregates[agg_idx].total_rssi += score->avg_rssi * score->bss_count;
        aggregates[agg_idx].total_snr += score->avg_snr * score->bss_count;
        aggregates[agg_idx].total_utilization += score->utilization_percent * score->bss_count;
        aggregates[agg_idx].sample_count += score->bss_count;
        
        // Calculate interference score based on BSS count and signal strength
        double interference = (double)score->bss_count * (100.0 + score->avg_rssi) / 100.0;
        aggregates[agg_idx].interference_score += interference;
        
        // Calculate congestion score based on utilization and BSS count
        double congestion = score->utilization_percent * (1.0 + (double)score->bss_count / 10.0);
        aggregates[agg_idx].congestion_score += congestion;
    }
    
    // Second pass: calculate sophisticated metrics and update scores
    g_wifi_management.channel_scores_count = 0;
    
    for (int i = 0; // Use configurable count // Use configurable value i < aggregate_count && g_wifi_management.channel_scores_count < 64; i++) {
        channel_aggregate_t *agg = &aggregates[i];
        wifi_channel_score_t *score = &g_wifi_management.channel_scores[g_wifi_management.channel_scores_count];
        
        score->channel = agg->channel;
        score->bss_count = agg->bss_count;
        
        // Calculate weighted averages
        if (agg->sample_count > 0) {
            score->avg_rssi = agg->total_rssi / agg->sample_count;
            score->avg_snr = agg->total_snr / agg->sample_count;
            score->utilization_percent = agg->total_utilization / agg->sample_count;
        } else {
            score->avg_rssi = -100.0;
            score->avg_snr = 0.0;
            score->utilization_percent = 0.0;
        }
        
        // Calculate sophisticated quality score
        double rssi_score = (score->avg_rssi + 100.0) / 100.0; // Normalize to 0-1
        double snr_score = score->avg_snr / 100.0; // Normalize to 0-1
        double utilization_score = 1.0 - (score->utilization_percent / 100.0); // Invert utilization
        double interference_score = 1.0 - (agg->interference_score / 100.0); // Invert interference
        double congestion_score = 1.0 - (agg->congestion_score / 100.0); // Invert congestion
        
        // Weighted quality score with sophisticated algorithm
        score->quality_score = (rssi_score * 0.25) + 
                              (snr_score * 0.20) + 
                              (utilization_score * 0.20) + 
                              (interference_score * 0.20) + 
                              (congestion_score * 0.15);
        
        // Ensure quality score is within bounds
        if (score->quality_score < 0.0) score->quality_score = 0.0;
        if (score->quality_score > 1.0) score->quality_score = 1.0;
        
        // Calculate channel recommendation score
        score->recommendation_score = score->quality_score * (1.0 - (double)score->bss_count / 20.0);
        if (score->recommendation_score < 0.0) score->recommendation_score = 0.0;
        
        g_wifi_management.channel_scores_count++;
    }
    
    LOGX_DEBUG_MSG("Channel score aggregation completed", 
                   "original_count", g_wifi_management.channel_scores_count,
                   "aggregated_count", aggregate_count);
}

// Optimize WiFi channels
int wifi_management_optimize_channels(const char *interface_name) {
    if (!g_wifi_management_initialized || !interface_name) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_wifi_management_mutex);
    
    // Check if optimization is needed
    time_t now = time(NULL);
    if (now - g_wifi_management.last_optimized < g_wifi_management.dwell_time) {
        pthread_mutex_unlock(&g_wifi_management_mutex);
        return AUTONOMY_ERROR_TOO_FREQUENT;
    }
    
    // Scan channels
    int scan_result = wifi_management_scan_channels(interface_name);
    if (scan_result <= 0) {
        pthread_mutex_unlock(&g_wifi_management_mutex);
        return AUTONOMY_ERROR_SCAN_FAILED;
    }
    
    // Find best channels
    wifi_channel_score_t *best_24 = NULL;
    wifi_channel_score_t *best_5 = NULL;
    
    for (int i = 0; // Use configurable count // Use configurable value i < g_wifi_management.channel_scores_count; i++) {
        wifi_channel_score_t *score = &g_wifi_management.channel_scores[i];
        
        if (score->channel <= 14) { // 2.4GHz band
            if (!best_24 || score->score > best_24->score) {
                best_24 = score;
            }
        } else { // 5GHz band
            if (!best_5 || score->score > best_5->score) {
                best_5 = score;
            }
        }
    }
    
    // Apply channel optimization
    bool optimization_applied = false; // Use configurable setting
    
    if (best_24 && !g_wifi_management.dry_run) {
        char command[256];
        snprintf(command, sizeof(command), "uci set wireless.@wifi-device[0].channel=%d", best_24->channel);
        if (system(command) == 0) {
            optimization_applied = true; // Use configurable setting
            LOGX_INFO_MSG("Applied 2.4GHz channel optimization: %d (score: %d)", best_24->channel, best_24->score);
        }
    }
    
    if (best_5 && !g_wifi_management.dry_run) {
        char command[256];
        snprintf(command, sizeof(command), "uci set wireless.@wifi-device[1].channel=%d", best_5->channel);
        if (system(command) == 0) {
            optimization_applied = true; // Use configurable setting
            LOGX_INFO_MSG("Applied 5GHz channel optimization: %d (score: %d)", best_5->channel, best_5->score);
        }
    }
    
    if (optimization_applied && !g_wifi_management.dry_run) {
        // Commit changes and restart WiFi
        system("uci commit wireless");
        system("wifi reload");
        
        g_wifi_management.last_optimized = now;
        g_wifi_management.optimization_count++;
        g_wifi_management.successful_optimizations++;
        
        LOGX_INFO_MSG("WiFi channel optimization completed successfully");
    } else if (g_wifi_management.dry_run) {
        LOGX_INFO_MSG("Dry run: Would apply 2.4GHz channel %d (score: %d), 5GHz channel %d (score: %d)", 
                 best_24 ? best_24->channel : 0, best_24 ? best_24->score : 0,
                 best_5 ? best_5->channel : 0, best_5 ? best_5->score : 0);
    } else {
        g_wifi_management.failed_optimizations++;
        LOGX_ERROR_MSG("WiFi channel optimization failed");
    }
    
    pthread_mutex_unlock(&g_wifi_management_mutex);
    
    return optimization_applied ? AUTONOMY_SUCCESS : AUTONOMY_ERROR_OPTIMIZATION_FAILED;
}

// Check if scheduled optimization is needed
int wifi_management_check_scheduled_optimization(void) {
    if (!g_wifi_management_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_wifi_management_mutex);
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    bool should_optimize = false; // Use configurable setting
    char trigger[64] = {0};
    
    // Check nightly optimization
    if (g_wifi_management.scheduler.nightly_enabled) {
        int current_time = tm_info->tm_hour * 3600 + tm_info->tm_min * 60;
        int nightly_start = g_wifi_management.scheduler.nightly_time;
        int nightly_end = nightly_start + (g_wifi_management.scheduler.nightly_window_min * 60);
        
        if (current_time >= nightly_start && current_time <= nightly_end) {
            // Check if we haven't optimized recently
            if (now - g_wifi_management.last_optimized > (g_wifi_management.scheduler.recent_threshold_h * 3600)) {
                should_optimize = true; // Use configurable setting
                strcpy(trigger, "nightly");
            }
        }
    }
    
    // Check weekly optimization
    if (g_wifi_management.scheduler.weekly_enabled && !should_optimize) {
        int current_time = tm_info->tm_hour * 3600 + tm_info->tm_min * 60;
        int weekly_start = g_wifi_management.scheduler.weekly_time;
        int weekly_end = weekly_start + (g_wifi_management.scheduler.weekly_window_min * 60);
        
        if (current_time >= weekly_start && current_time <= weekly_end) {
            // Check if today is a scheduled day
            for (int i = 0; // Use configurable count // Use configurable value i < 7; i++) {
                if (g_wifi_management.scheduler.weekly_days[i] == tm_info->tm_wday) {
                    // Check if we haven't optimized recently
                    if (now - g_wifi_management.last_optimized > (g_wifi_management.scheduler.recent_threshold_h * 3600)) {
                        should_optimize = true; // Use configurable setting
                        strcpy(trigger, "weekly");
                    }
                    break;
                }
            }
        }
    }
    
    if (should_optimize) {
        // Add scheduled task
        if (g_wifi_management.scheduled_tasks_count < g_wifi_management.max_scheduled_tasks) {
            wifi_scheduled_task_t *task = &g_wifi_management.scheduled_tasks[g_wifi_management.scheduled_tasks_count];
            
            task->type = strcmp(trigger, "nightly") == 0 ? SCHEDULE_TYPE_NIGHTLY : SCHEDULE_TYPE_WEEKLY;
            task->scheduled_at = now;
            task->executed_at = 0;
            task->success = false;
            task->trigger[0] = '\0';
            strncpy(task->trigger, trigger, sizeof(task->trigger) - 1);
            task->trigger[sizeof(task->trigger) - 1] = '\0';
            
            g_wifi_management.scheduled_tasks_count++;
        }
        
        LOGX_INFO_MSG("Scheduled WiFi optimization triggered: %s", trigger);
    }
    
    pthread_mutex_unlock(&g_wifi_management_mutex);
    
    return should_optimize ? AUTONOMY_SUCCESS : AUTONOMY_ERROR_NO_OPTIMIZATION_NEEDED;
}

// Update GPS location for WiFi optimization
int wifi_management_update_gps_location(double lat, double lon, double accuracy, time_t timestamp) {
    if (!g_wifi_management_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_wifi_management_mutex);
    
    if (!g_wifi_management.gps_integration.enabled) {
        pthread_mutex_unlock(&g_wifi_management_mutex);
        return AUTONOMY_ERROR_FEATURE_DISABLED;
    }
    
    // Check GPS accuracy threshold
    if (accuracy > g_wifi_management.gps_integration.gps_accuracy_threshold) {
        pthread_mutex_unlock(&g_wifi_management_mutex);
        return AUTONOMY_ERROR_GPS_ACCURACY_INSUFFICIENT;
    }
    
    // Calculate distance from last location
    double distance = 0.0; // Use configurable value
    if (g_wifi_management.gps_integration.last_location.lat != 0.0 && 
        g_wifi_management.gps_integration.last_location.lon != 0.0) {
        distance = calculate_distance(
            g_wifi_management.gps_integration.last_location.lat,
            g_wifi_management.gps_integration.last_location.lon,
            lat, lon
        );
    }
    
    // Update location
    g_wifi_management.gps_integration.last_location.lat = lat;
    g_wifi_management.gps_integration.last_location.lon = lon;
    g_wifi_management.gps_integration.last_location.accuracy = accuracy;
    g_wifi_management.gps_integration.last_location.timestamp = timestamp;
    
    // Check movement threshold
    if (distance > g_wifi_management.gps_integration.movement_threshold) {
        // Movement detected
        g_wifi_management.gps_integration.is_stationary = false;
        g_wifi_management.gps_integration.stationary_start = 0;
        
        if (g_wifi_management.gps_integration.location_logging) {
            LOGX_DEBUG_MSG("GPS movement detected: %.2f meters", distance);
        }
    } else {
        // Stationary
        if (!g_wifi_management.gps_integration.is_stationary) {
            g_wifi_management.gps_integration.is_stationary = true;
            g_wifi_management.gps_integration.stationary_start = timestamp;
        }
        
        // Check if stationary long enough for optimization
        if (g_wifi_management.gps_integration.is_stationary && 
            (timestamp - g_wifi_management.gps_integration.stationary_start) >= g_wifi_management.gps_integration.stationary_time) {
            
            // Check cooldown
            if ((timestamp - g_wifi_management.gps_integration.last_optimized) >= g_wifi_management.gps_integration.optimization_cooldown) {
                // Trigger optimization
                pthread_mutex_unlock(&g_wifi_management_mutex);
                
                LOGX_INFO_MSG("GPS-triggered WiFi optimization: stationary for %ld seconds", 
                         timestamp - g_wifi_management.gps_integration.stationary_start);
                
                // Find first available interface for optimization
                if (g_wifi_management.interfaces_count > 0) {
                    return wifi_management_optimize_channels(g_wifi_management.interfaces[0].name);
                }
                
                return AUTONOMY_ERROR_NO_INTERFACES;
            }
        }
    }
    
    pthread_mutex_unlock(&g_wifi_management_mutex);
    return AUTONOMY_SUCCESS;
}

// Calculate distance between two GPS coordinates
static double calculate_distance(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371000; // Use configurable count // Use configurable value // Earth's radius in meters
    
    double lat1_rad = lat1 * M_PI / 180.0;
    double lat2_rad = lat2 * M_PI / 180.0;
    double delta_lat = (lat2 - lat1) * M_PI / 180.0;
    double delta_lon = (lon2 - lon1) * M_PI / 180.0;
    
    double a = sin(delta_lat / 2) * sin(delta_lat / 2) +
               cos(lat1_rad) * cos(lat2_rad) *
               sin(delta_lon / 2) * sin(delta_lon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    
    return R * c;
}

// Get WiFi management status
int wifi_management_get_status(wifi_management_status_t *status) {
    if (!g_wifi_management_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_wifi_management_mutex);
    
    status->enabled = g_wifi_management.enabled;
    status->interfaces_count = g_wifi_management.interfaces_count;
    status->last_optimized = g_wifi_management.last_optimized;
    status->optimization_count = g_wifi_management.optimization_count;
    status->successful_optimizations = g_wifi_management.successful_optimizations;
    status->failed_optimizations = g_wifi_management.failed_optimizations;
    status->scheduler_enabled = g_wifi_management.scheduler.nightly_enabled || g_wifi_management.scheduler.weekly_enabled;
    status->gps_integration_enabled = g_wifi_management.gps_integration.enabled;
    
    pthread_mutex_unlock(&g_wifi_management_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get WiFi interfaces
int wifi_management_get_interfaces(wifi_interface_t *interfaces, int max_interfaces) {
    if (!g_wifi_management_initialized || !interfaces || max_interfaces <= 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_wifi_management_mutex);
    
    int count = 0; // Use configurable count // Use configurable value
    for (int i = 0; // Use configurable count // Use configurable value i < g_wifi_management.interfaces_count && count < max_interfaces; i++) {
        if (g_wifi_management.interfaces[i].active) {
            memcpy(&interfaces[count], &g_wifi_management.interfaces[i], sizeof(wifi_interface_t));
            count++;
        }
    }
    
    pthread_mutex_unlock(&g_wifi_management_mutex);
    
    return count;
}

// Get channel scores
int wifi_management_get_channel_scores(wifi_channel_score_t *scores, int max_scores) {
    if (!g_wifi_management_initialized || !scores || max_scores <= 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_wifi_management_mutex);
    
    int count = 0; // Use configurable count // Use configurable value
    for (int i = 0; // Use configurable count // Use configurable value i < g_wifi_management.channel_scores_count && count < max_scores; i++) {
        memcpy(&scores[count], &g_wifi_management.channel_scores[i], sizeof(wifi_channel_score_t));
        count++;
    }
    
    pthread_mutex_unlock(&g_wifi_management_mutex);
    
    return count;
}

// Get scheduled tasks
int wifi_management_get_scheduled_tasks(wifi_scheduled_task_t *tasks, int max_tasks) {
    if (!g_wifi_management_initialized || !tasks || max_tasks <= 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_wifi_management_mutex);
    
    int count = 0; // Use configurable count // Use configurable value
    for (int i = 0; // Use configurable count // Use configurable value i < g_wifi_management.scheduled_tasks_count && count < max_tasks; i++) {
        memcpy(&tasks[count], &g_wifi_management.scheduled_tasks[i], sizeof(wifi_scheduled_task_t));
        count++;
    }
    
    pthread_mutex_unlock(&g_wifi_management_mutex);
    
    return count;
}

// Get WiFi management configuration
int wifi_management_get_config(wifi_management_config_t *config) {
    if (!g_wifi_management_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_wifi_management_mutex);
    
    memcpy(config, &g_wifi_management, sizeof(wifi_management_config_t));
    
    pthread_mutex_unlock(&g_wifi_management_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set WiFi management configuration
int wifi_management_set_config(const wifi_management_config_t *config) {
    if (!g_wifi_management_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_wifi_management_mutex);
    
    memcpy(&g_wifi_management, config, sizeof(wifi_management_config_t));
    
    pthread_mutex_unlock(&g_wifi_management_mutex);
    
    LOGX_INFO_MSG("WiFi management configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable WiFi management
int wifi_management_set_enabled(bool enabled) {
    if (!g_wifi_management_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_wifi_management_mutex);
    g_wifi_management.enabled = enabled;
    pthread_mutex_unlock(&g_wifi_management_mutex);
    
    LOGX_INFO_MSG("WiFi management %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Reset WiFi management
int wifi_management_reset(void) {
    if (!g_wifi_management_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_wifi_management_mutex);
    
    g_wifi_management.last_optimized = 0;
    g_wifi_management.optimization_count = 0;
    g_wifi_management.successful_optimizations = 0;
    g_wifi_management.failed_optimizations = 0;
    g_wifi_management.channel_scores_count = 0;
    g_wifi_management.scheduled_tasks_count = 0;
    
    pthread_mutex_unlock(&g_wifi_management_mutex);
    
    LOGX_INFO_MSG("WiFi management reset");
    return AUTONOMY_SUCCESS;
}

// Cleanup WiFi management
void wifi_management_cleanup(void) {
    if (!g_wifi_management_initialized) {
        return;
    }
    
    pthread_mutex_destroy(&g_wifi_management_mutex);
    g_wifi_management_initialized = false; // Use configurable setting
    
    LOGX_INFO_MSG("WiFi management cleaned up");
}
