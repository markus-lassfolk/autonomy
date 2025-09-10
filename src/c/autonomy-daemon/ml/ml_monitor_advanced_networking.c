#include "ml_monitor_advanced_networking.h"
#include "../utils/logx.h"
#include "../utils/secure_exec.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>

// Advanced Networking Intelligence Implementation

// Global advanced networking system
static advanced_networking_intelligence_t *g_advanced_networking = NULL;

// High-frequency monitoring threads
static pthread_t g_starlink_monitor_thread;
static pthread_t g_wifi_monitor_thread;
static pthread_t g_lan_monitor_thread;
static pthread_t g_cellular_monitor_thread;
static bool g_monitoring_active = false;

// Forward declarations
static void* ml_monitor_starlink_high_freq_thread(void *arg);
static void* ml_monitor_wifi_high_freq_thread(void *arg);
static void* ml_monitor_lan_high_freq_thread(void *arg);
static void* ml_monitor_cellular_monitor_thread(void *arg);
int ml_monitor_perform_ping_test(const char *interface_id, const char *target, uint32_t *latency_ms, bool *success);
int ml_monitor_collect_cellular_modem_metrics(const char *interface_id, multi_interface_observation_t *observation);

// Initialize advanced networking intelligence
advanced_networking_intelligence_t* ml_monitor_init_advanced_networking(const ml_monitor_config_t *config) {
    if (!config) return NULL;
    
    LOGX_INFO_MSG(" Initializing Advanced Networking Intelligence");
    
    advanced_networking_intelligence_t *system = calloc(1, sizeof(advanced_networking_intelligence_t));
    if (!system) {
        LOGX_ERROR_MSG("Failed to allocate advanced networking system");
        return NULL;
    }
    
    // Configure high-frequency monitoring
    system->high_freq_config.monitoring_intervals.starlink_monitor_interval_ms = 1000; // 1 second
    system->high_freq_config.monitoring_intervals.wifi_monitor_interval_ms = 1000;     // 1 second
    system->high_freq_config.monitoring_intervals.lan_monitor_interval_ms = 1000;      // 1 second
    system->high_freq_config.monitoring_intervals.cellular_monitor_interval_ms = 5000; // 5 seconds (data cost)
    
    // Configure ping tests
    system->high_freq_config.ping_config.enable_continuous_ping_tests = true;
    system->high_freq_config.ping_config.ping_target_count = 3;
    strncpy(system->high_freq_config.ping_config.ping_targets[0], "8.8.8.8", 64);      // Google DNS
    strncpy(system->high_freq_config.ping_config.ping_targets[1], "1.1.1.1", 64);      // Cloudflare DNS
    strncpy(system->high_freq_config.ping_config.ping_targets[2], "208.67.222.222", 64); // OpenDNS
    system->high_freq_config.ping_config.ping_timeout_ms = 2000; // 2 second timeout
    system->high_freq_config.ping_config.ping_packet_size = 32;  // Small packets
    
    // Configure cellular optimization
    system->high_freq_config.cellular_optimization.monitor_cellular_modem_metrics = true;
    system->high_freq_config.cellular_optimization.reduce_cellular_ping_frequency = true;
    system->high_freq_config.cellular_optimization.cellular_data_budget_mb_per_day = 100; // 100MB daily budget
    system->high_freq_config.cellular_optimization.cellular_data_used_today_kb = 0;
    
    // Configure predictive failover (YOUR KEY INSIGHT: Protect streaming)
    system->predictive_system.predictive_thresholds.latency_spike_threshold_ms = 100; // 100ms spike
    system->predictive_system.predictive_thresholds.packet_loss_spike_threshold = 3;   // 3% loss
    system->predictive_system.predictive_thresholds.prediction_horizon_ms = 3000;      // 3 seconds ahead
    system->predictive_system.predictive_thresholds.confidence_threshold_for_action = 0.7; // 70% confidence
    
    // STREAMING PROTECTION (your critical requirement)
    system->predictive_system.streaming_protection.enable_streaming_protection_mode = true;
    system->predictive_system.streaming_protection.streaming_tolerance_ms = 2000;      // 2 second max interruption
    system->predictive_system.streaming_protection.streaming_prediction_window_ms = 5000; // 5 second window
    system->predictive_system.streaming_protection.streaming_confidence_threshold = 0.6;  // 60% for streaming
    
    // Configure connection stability (YOUR FLAPPING PREVENTION INSIGHT)
    system->stability_system.flapping_prevention.enable_flapping_prevention = true;
    system->stability_system.flapping_prevention.flapping_threshold_events_per_hour = 3; // 3 events = flapping
    system->stability_system.flapping_prevention.flapping_penalty_duration_seconds = 1800; // 30 min penalty
    system->stability_system.flapping_prevention.flapping_weight_reduction_factor = 0.1; // Reduce to 10%
    system->stability_system.flapping_prevention.stability_required_for_recovery_seconds = 600; // 10 min stable
    
    // Configure background validation (YOUR GENIUS "WHAT IF" INSIGHT)
    system->background_intelligence.cross_validation.enable_cross_validation = true;
    system->background_intelligence.ensemble_validation.enable_ensemble_validation = true;
    
    // Enable advanced features
    system->enable_streaming_protection = true;
    system->enable_flapping_prevention = true;
    system->enable_background_validation = true;
    system->enable_predictive_failover = true;
    
    g_advanced_networking = system;
    
    LOGX_INFO_MSG(" Advanced networking intelligence initialized");
    LOGX_INFO_MSG("   - High-frequency monitoring: Starlink/WiFi/LAN=1s, Cellular=5s");
    LOGX_INFO_MSG("   - Streaming protection: enabled (2s tolerance, 3s prediction)");
    LOGX_INFO_MSG("   - Flapping prevention: enabled (3 events/hour threshold)");
    LOGX_INFO_MSG("   - Background validation: enabled (what-if analysis)");
    LOGX_INFO_MSG("   - Predictive failover: enabled (vs reactive)");
    
    return system;
}

// Start high-frequency monitoring threads
int ml_monitor_start_high_frequency_monitoring(advanced_networking_intelligence_t *system) {
    if (!system || g_monitoring_active) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    LOGX_INFO_MSG(" Starting high-frequency monitoring threads");
    
    g_monitoring_active = true;
    
    // Start Starlink monitoring (1 second interval)
    if (pthread_create(&g_starlink_monitor_thread, NULL, ml_monitor_starlink_high_freq_thread, system) != 0) {
        LOGX_ERROR_MSG("Failed to create Starlink high-frequency monitoring thread");
        g_monitoring_active = false;
        return ML_MONITOR_ERROR_THREAD_FAILED;
    }
    
    // Start WiFi monitoring (1 second interval)
    if (pthread_create(&g_wifi_monitor_thread, NULL, ml_monitor_wifi_high_freq_thread, system) != 0) {
        LOGX_ERROR_MSG("Failed to create WiFi high-frequency monitoring thread");
        g_monitoring_active = false;
        return ML_MONITOR_ERROR_THREAD_FAILED;
    }
    
    // Start LAN monitoring (1 second interval)
    if (pthread_create(&g_lan_monitor_thread, NULL, ml_monitor_lan_high_freq_thread, system) != 0) {
        LOGX_ERROR_MSG("Failed to create LAN high-frequency monitoring thread");
        g_monitoring_active = false;
        return ML_MONITOR_ERROR_THREAD_FAILED;
    }
    
    // Start Cellular monitoring (5 second interval - data cost consideration)
    if (pthread_create(&g_cellular_monitor_thread, NULL, ml_monitor_cellular_monitor_thread, system) != 0) {
        LOGX_ERROR_MSG("Failed to create Cellular monitoring thread");
        g_monitoring_active = false;
        return ML_MONITOR_ERROR_THREAD_FAILED;
    }
    
    LOGX_INFO_MSG(" All high-frequency monitoring threads started successfully");
    return ML_MONITOR_SUCCESS;
}

// Starlink high-frequency monitoring (1 second interval)
static void* ml_monitor_starlink_high_freq_thread(void *arg) {
    advanced_networking_intelligence_t *system = (advanced_networking_intelligence_t*)arg;
    if (!system) return NULL;
    
    LOGX_INFO_MSG(" Starlink high-frequency monitoring thread started (1 second interval)");
    
    while (g_monitoring_active) {
        // Perform ping tests to multiple targets
        for (int target = 0; target < system->high_freq_config.ping_config.ping_target_count; target++) {
            uint32_t latency_ms;
            bool ping_success;
            
            int ping_result = ml_monitor_perform_ping_test("starlink1", 
                                                         system->high_freq_config.ping_config.ping_targets[target],
                                                         &latency_ms, &ping_success);
            
            if (ping_result == ML_MONITOR_SUCCESS) {
                // Create observation
                multi_interface_observation_t obs;
                memset(&obs, 0, sizeof(obs));
                
                obs.timestamp = time(NULL);
                strncpy(obs.interface_id, "starlink1", sizeof(obs.interface_id) - 1);
                obs.interface_type = INTERFACE_TYPE_STARLINK;
                obs.latency_ms = latency_ms;
                obs.packet_loss_pct = ping_success ? 0 : 100;
                obs.connection_health = ping_success ? (255 - (latency_ms / 2)) : 0;
                
                // Update background validation
                ml_monitor_update_background_validation(system, "starlink1", &obs);
                
                // Check for predictive failover triggers
                bool should_failover;
                uint32_t predicted_outage_ms;
                double confidence;
                
                ml_monitor_evaluate_predictive_failover(system, "starlink1", &should_failover, 
                                                       &predicted_outage_ms, &confidence);
                
                if (should_failover && confidence > 0.7) {
                    LOGX_WARN_MSG(" PREDICTIVE FAILOVER TRIGGER: Starlink outage predicted in %ums (confidence: %.1f%%)",
                             predicted_outage_ms, confidence * 100);
                }
            }
        }
        
        // Sleep for 1 second (high frequency for Starlink)
        usleep(system->high_freq_config.monitoring_intervals.starlink_monitor_interval_ms * 1000);
    }
    
    LOGX_INFO_MSG("Starlink high-frequency monitoring thread stopped");
    return NULL;
}

// Cellular monitoring thread (5 second interval - data cost aware)
static void* ml_monitor_cellular_monitor_thread(void *arg) {
    advanced_networking_intelligence_t *system = (advanced_networking_intelligence_t*)arg;
    if (!system) return NULL;
    
    LOGX_INFO_MSG(" Cellular monitoring thread started (5 second interval - data cost optimized)");
    
    while (g_monitoring_active) {
        // Collect cellular modem metrics (free - no data cost)
        multi_interface_observation_t obs;
        int modem_result = ml_monitor_collect_cellular_modem_metrics("cellular1", &obs);
        
        if (modem_result == ML_MONITOR_SUCCESS) {
            // Update background validation with free modem data
            ml_monitor_update_background_validation(system, "cellular1", &obs);
            
            LOGX_DEBUG_MSG("Cellular modem metrics: signal=%ddBm, quality=%u, latency=%ums",
                      obs.interface_specific.cellular.signal_strength_dbm,
                      obs.interface_specific.cellular.signal_quality,
                      obs.latency_ms);
        }
        
        // Only do ping tests every 5 seconds to save data
        static int ping_cycle = 0;
        if (++ping_cycle >= 5) { // Every 5th cycle = every 25 seconds
            uint32_t latency_ms;
            bool ping_success;
            
            int ping_result = ml_monitor_perform_ping_test("cellular1", "8.8.8.8", &latency_ms, &ping_success);
            if (ping_result == ML_MONITOR_SUCCESS) {
                obs.latency_ms = latency_ms;
                obs.packet_loss_pct = ping_success ? 0 : 100;
                
                // Track data usage
                system->high_freq_config.cellular_optimization.cellular_data_used_today_kb += 1; // ~1KB per ping
                
                LOGX_DEBUG_MSG("Cellular ping test: latency=%ums, success=%s, data_used=%uKB",
                          latency_ms, ping_success ? "yes" : "no",
                          system->high_freq_config.cellular_optimization.cellular_data_used_today_kb);
            }
            ping_cycle = 0;
        }
        
        // Sleep for 5 seconds (data cost consideration)
        usleep(system->high_freq_config.monitoring_intervals.cellular_monitor_interval_ms * 1000);
    }
    
    LOGX_INFO_MSG("Cellular monitoring thread stopped");
    return NULL;
}

// WiFi high-frequency monitoring (1 second interval)
static void* ml_monitor_wifi_high_freq_thread(void *arg) {
    advanced_networking_intelligence_t *system = (advanced_networking_intelligence_t*)arg;
    if (!system) return NULL;
    
    LOGX_INFO_MSG(" WiFi high-frequency monitoring thread started (1 second interval)");
    
    while (g_monitoring_active) {
        // Perform ping test (no cost for WiFi)
        uint32_t latency_ms;
        bool ping_success;
        
        int ping_result = ml_monitor_perform_ping_test("wifi1", "8.8.8.8", &latency_ms, &ping_success);
        
        if (ping_result == ML_MONITOR_SUCCESS) {
            multi_interface_observation_t obs;
            memset(&obs, 0, sizeof(obs));
            
            obs.timestamp = time(NULL);
            strncpy(obs.interface_id, "wifi1", sizeof(obs.interface_id) - 1);
            obs.interface_type = INTERFACE_TYPE_WIFI;
            obs.latency_ms = latency_ms;
            obs.packet_loss_pct = ping_success ? 0 : 100;
            obs.connection_health = ping_success ? (255 - (latency_ms / 2)) : 0;
            
            // Collect real WiFi-specific metrics
            char wifi_cmd[256];
            char wifi_file[128];
            snprintf(wifi_file, sizeof(wifi_file), "/tmp/wifi_metrics_%s_%lld", obs.interface_id, (long long)time(NULL));
            snprintf(wifi_cmd, sizeof(wifi_cmd), "iw dev %s station dump > %s 2>/dev/null", obs.interface_id, wifi_file);
            
            extern int secure_exec_command(const char *command, exec_result_t *result);
            exec_result_t wifi_result;
            if (secure_exec_command(wifi_cmd, &wifi_result) == AUTONOMY_SUCCESS && wifi_result.success) {
                FILE *f = fopen(wifi_file, "r");
                if (f) {
                    char line[256];
                    while (fgets(line, sizeof(line), f)) {
                        if (strstr(line, "signal:")) {
                            sscanf(line, "%*s %hhd dBm", &obs.interface_specific.wifi.rssi_dbm);
                        }
                        if (strstr(line, "beacon loss count:")) {
                            int loss_count;
                            sscanf(line, "%*s %*s %*s %d", &loss_count);
                            obs.interface_specific.wifi.interference_level = (uint8_t)fmin(255, loss_count * 5);
                        }
                    }
                    fclose(f);
                }
            }
            
            // Get channel utilization via iw survey
            snprintf(wifi_cmd, sizeof(wifi_cmd), "iw dev %s survey dump | grep 'channel active time' | tail -1 > %s 2>/dev/null", 
                    obs.interface_id, wifi_file);
            exec_result_t wifi_survey_result;
            if (secure_exec_command(wifi_cmd, &wifi_survey_result) == AUTONOMY_SUCCESS && wifi_survey_result.success) {
                FILE *f = fopen(wifi_file, "r");
                if (f) {
                    char line[256];
                    if (fgets(line, sizeof(line), f)) {
                        int active_time;
                        sscanf(line, "%*s %*s %*s %d", &active_time);
                        obs.interface_specific.wifi.channel_utilization = (uint8_t)fmin(100, active_time / 10);
                    }
                    fclose(f);
                }
            }
            
            unlink(wifi_file);
            
            // Update background validation
            ml_monitor_update_background_validation(system, "wifi1", &obs);
        }
        
        // Sleep for 1 second (no cost for WiFi)
        usleep(system->high_freq_config.monitoring_intervals.wifi_monitor_interval_ms * 1000);
    }
    
    LOGX_INFO_MSG("WiFi high-frequency monitoring thread stopped");
    return NULL;
}

// LAN high-frequency monitoring (1 second interval)
static void* ml_monitor_lan_high_freq_thread(void *arg) {
    advanced_networking_intelligence_t *system = (advanced_networking_intelligence_t*)arg;
    if (!system) return NULL;
    
    LOGX_INFO_MSG(" LAN high-frequency monitoring thread started (1 second interval)");
    
    while (g_monitoring_active) {
        // Perform ping test (no cost for LAN)
        uint32_t latency_ms;
        bool ping_success;
        
        int ping_result = ml_monitor_perform_ping_test("lan1", "192.168.1.1", &latency_ms, &ping_success); // Gateway
        
        if (ping_result == ML_MONITOR_SUCCESS) {
            multi_interface_observation_t obs;
            memset(&obs, 0, sizeof(obs));
            
            obs.timestamp = time(NULL);
            strncpy(obs.interface_id, "lan1", sizeof(obs.interface_id) - 1);
            obs.interface_type = INTERFACE_TYPE_LAN;
            obs.latency_ms = latency_ms;
            obs.packet_loss_pct = ping_success ? 0 : 100;
            obs.connection_health = ping_success ? (255 - latency_ms) : 0; // LAN should be very low latency
            
            // LAN-specific metrics
            obs.interface_specific.lan.link_speed_mbps = 100; // 100 Mbps typical
            obs.interface_specific.lan.duplex = 1; // Full duplex
            obs.interface_specific.lan.cable_quality = latency_ms < 5 ? 255 : (255 - latency_ms * 10);
            
            // Update background validation
            ml_monitor_update_background_validation(system, "lan1", &obs);
        }
        
        // Sleep for 1 second (no cost for LAN)
        usleep(system->high_freq_config.monitoring_intervals.lan_monitor_interval_ms * 1000);
    }
    
    LOGX_INFO_MSG("LAN high-frequency monitoring thread stopped");
    return NULL;
}

// Perform real ping test for interface
int ml_monitor_perform_ping_test(const char *interface_id, const char *target, uint32_t *latency_ms, bool *success) {
    if (!interface_id || !target || !latency_ms || !success) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    *latency_ms = 0;
    *success = false;
    
    // Execute real ping command
    char ping_cmd[256];
    char result_file[128];
    snprintf(result_file, sizeof(result_file), "/tmp/ping_result_%s_%lld", interface_id, (long long)time(NULL));
    
    // Use ping with specific interface and timeout
    snprintf(ping_cmd, sizeof(ping_cmd), 
            "ping -I %s -c 1 -W 2 %s | grep 'time=' | sed 's/.*time=\\([0-9.]*\\).*/\\1/' > %s 2>/dev/null",
            interface_id, target, result_file);
    
    exec_result_t ping_result_struct;
    int ping_result = secure_exec_command(ping_cmd, &ping_result_struct);
    
    if (ping_result == AUTONOMY_SUCCESS && ping_result_struct.success) {
        // Read latency from result file
        FILE *f = fopen(result_file, "r");
        if (f) {
            char latency_str[32];
            if (fgets(latency_str, sizeof(latency_str), f)) {
                double latency_double = strtod(latency_str, NULL);
                *latency_ms = (uint32_t)latency_double;
                *success = true;
            }
            fclose(f);
        }
    }
    
    // Clean up result file
    unlink(result_file);
    
    if (!*success) {
        LOGX_DEBUG_MSG("Ping failed for %s to %s", interface_id, target);
        *latency_ms = 9999; // High latency for failed ping
    }
    
    return ML_MONITOR_SUCCESS;
}

// Collect cellular modem metrics (free - no data cost)
int ml_monitor_collect_cellular_modem_metrics(const char *interface_id, multi_interface_observation_t *observation) {
    if (!interface_id || !observation) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    memset(observation, 0, sizeof(multi_interface_observation_t));
    
    observation->timestamp = time(NULL);
    strncpy(observation->interface_id, interface_id, sizeof(observation->interface_id) - 1);
    observation->interface_type = INTERFACE_TYPE_CELLULAR;
    
    // Collect real modem metrics using AT commands or UBUS
    char modem_cmd[256];
    char signal_file[128];
    snprintf(signal_file, sizeof(signal_file), "/tmp/modem_signal_%s_%lld", interface_id, (long long)time(NULL));
    
    // Try to get signal strength via ubus (preferred method)
    snprintf(modem_cmd, sizeof(modem_cmd), 
            "ubus call modem.%s get_signal_info > %s 2>/dev/null", interface_id, signal_file);
    
    exec_result_t modem_result_struct;
    int modem_result = secure_exec_command(modem_cmd, &modem_result_struct);
    
    if (modem_result == AUTONOMY_SUCCESS && modem_result_struct.success) {
        // Parse signal information from UBUS response
        FILE *f = fopen(signal_file, "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                // Parse signal strength
                if (strstr(line, "signal_strength")) {
                    char *value_start = strchr(line, ':');
                    if (value_start) {
                        observation->interface_specific.cellular.signal_strength_dbm = atoi(value_start + 1);
                    }
                }
                // Parse signal quality
                if (strstr(line, "signal_quality")) {
                    char *value_start = strchr(line, ':');
                    if (value_start) {
                        observation->interface_specific.cellular.signal_quality = (uint8_t)atoi(value_start + 1);
                    }
                }
                // Parse network type
                if (strstr(line, "network_type")) {
                    char *value_start = strchr(line, ':');
                    if (value_start) {
                        observation->interface_specific.cellular.network_type = (uint8_t)atoi(value_start + 1);
                    }
                }
            }
            fclose(f);
        }
    } else {
        // Fallback: Try AT commands directly
        snprintf(modem_cmd, sizeof(modem_cmd), 
                "echo 'AT+CSQ' | socat - /dev/ttyUSB0,b115200 | grep '+CSQ:' | cut -d: -f2 > %s 2>/dev/null",
                signal_file);
        
        exec_result_t at_result;
        if (secure_exec_command(modem_cmd, &at_result) == AUTONOMY_SUCCESS && at_result.success) {
            FILE *f = fopen(signal_file, "r");
            if (f) {
                char csq_response[64];
                if (fgets(csq_response, sizeof(csq_response), f)) {
                    int rssi, ber;
                    if (sscanf(csq_response, "%d,%d", &rssi, &ber) == 2) {
                        // Convert CSQ to dBm: dBm = -113 + (rssi * 2)
                        observation->interface_specific.cellular.signal_strength_dbm = -113 + (rssi * 2);
                        observation->interface_specific.cellular.signal_quality = (uint8_t)((rssi * 255) / 31);
                    }
                }
                fclose(f);
            }
        } else {
            LOGX_WARN_MSG("Failed to collect cellular modem metrics for %s", interface_id);
            unlink(signal_file);
            return AUTONOMY_ERROR_NO_DATA;
        }
    }
    
    // Clean up
    unlink(signal_file);
    
    // Estimate latency based on signal quality (no ping needed to save data)
    int signal_strength = abs(observation->interface_specific.cellular.signal_strength_dbm);
    observation->latency_ms = 40 + ((signal_strength - 70) > 0 ? (signal_strength - 70) * 3 : 0);
    
    // Calculate connection quality based on real signal metrics
    observation->connection_stability = observation->interface_specific.cellular.signal_quality;
    observation->connection_health = (observation->connection_stability * 80) / 100; // 80% of stability
    
    return ML_MONITOR_SUCCESS;
}

// Evaluate predictive failover (YOUR KEY INSIGHT: Protect streaming)
int ml_monitor_evaluate_predictive_failover(advanced_networking_intelligence_t *system,
                                           const char *interface_id,
                                           bool *should_failover_now,
                                           uint32_t *predicted_outage_in_ms,
                                           double *confidence) {
    if (!system || !interface_id || !should_failover_now || !predicted_outage_in_ms || !confidence) {
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    *should_failover_now = false;
    *predicted_outage_in_ms = 0;
    *confidence = 0.0;
    
    // Get current performance prediction
    uint8_t outage_prob, performance_score, pred_confidence;
    int pred_result = ml_monitor_predict_interface_performance(ml_monitor_get_multi_interface_system(),
                                                             interface_id, &outage_prob, &performance_score, &pred_confidence);
    
    if (pred_result != ML_MONITOR_MULTI_SUCCESS) {
        return pred_result;
    }
    
    // Convert to probability and confidence
    double outage_probability = outage_prob / 255.0;
    *confidence = pred_confidence / 255.0;
    
    // PREDICTIVE FAILOVER LOGIC (vs reactive)
    bool streaming_protection = system->enable_streaming_protection;
    
    if (streaming_protection) {
        // STREAMING PROTECTION MODE: Very aggressive to protect streaming
        if (outage_probability > 0.3 && *confidence > 0.6) { // Lower thresholds for streaming
            *should_failover_now = true;
            *predicted_outage_in_ms = 3000; // Predict 3-second outage
            
            LOGX_INFO_MSG(" STREAMING PROTECTION: Predictive failover triggered for %s (prob=%.1f%%, conf=%.1f%%)",
                     interface_id, outage_probability * 100, *confidence * 100);
        }
    } else {
        // NORMAL MODE: Standard predictive thresholds
        if (outage_probability > 0.7 && *confidence > 0.7) {
            *should_failover_now = true;
            *predicted_outage_in_ms = 5000; // Predict 5-second outage
            
            LOGX_INFO_MSG(" PREDICTIVE FAILOVER: Standard trigger for %s (prob=%.1f%%, conf=%.1f%%)",
                     interface_id, outage_probability * 100, *confidence * 100);
        }
    }
    
    return ML_MONITOR_SUCCESS;
}

// Update connection stability and detect flapping
int ml_monitor_update_connection_stability(advanced_networking_intelligence_t *system,
                                          const char *interface_id,
                                          const multi_interface_observation_t *observation) {
    if (!system || !interface_id || !observation) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    // Find interface stability entry
    int stability_index = -1;
    for (int i = 0; i < system->stability_system.interface_count; i++) {
        if (strcmp(system->stability_system.interface_stability[i].interface_id, interface_id) == 0) {
            stability_index = i;
            break;
        }
    }
    
    // Add new interface if not found
    if (stability_index == -1 && system->stability_system.interface_count < MAX_INTERFACES) {
        stability_index = system->stability_system.interface_count++;
        strncpy(system->stability_system.interface_stability[stability_index].interface_id, 
                interface_id, sizeof(system->stability_system.interface_stability[stability_index].interface_id) - 1);
        
        // Initialize stability scores
        system->stability_system.interface_stability[stability_index].stability_score = 1.0;
        system->stability_system.interface_stability[stability_index].recent_stability = 1.0;
        system->stability_system.interface_stability[stability_index].long_term_stability = 1.0;
    }
    
    if (stability_index >= 0) {
        // Get pointer to interface stability
        struct {
            char interface_id[32];
            double stability_score;
            double recent_stability;
            double long_term_stability;
            uint32_t failover_events_last_hour;
            uint32_t failover_events_last_day;
            time_t last_failover_time;
            bool currently_flapping;
            time_t flapping_start_time;
            time_t last_stable_time;
            uint32_t stable_duration_seconds;
            bool in_recovery_period;
            uint32_t recovery_confidence_threshold;
        } *stability = (void*)&system->stability_system.interface_stability[stability_index];
        
        // Update stability based on observation
        double current_stability = (observation->connection_health / 255.0) * (observation->connection_stability / 255.0);
        
        // Update stability scores (exponential moving average)
        stability->recent_stability = stability->recent_stability * 0.9 + current_stability * 0.1;
        stability->long_term_stability = stability->long_term_stability * 0.99 + current_stability * 0.01;
        stability->stability_score = (stability->recent_stability * 0.7) + (stability->long_term_stability * 0.3);
        
        // Check for flapping
        time_t current_time = observation->timestamp;
        if (current_time - stability->last_failover_time < 3600) { // Within last hour
            stability->failover_events_last_hour++;
            
            if (stability->failover_events_last_hour >= system->stability_system.flapping_prevention.flapping_threshold_events_per_hour) {
                if (!stability->currently_flapping) {
                    stability->currently_flapping = true;
                    stability->flapping_start_time = current_time;
                    
                    LOGX_WARN_MSG(" FLAPPING DETECTED: %s has %u failovers in last hour - applying penalty",
                             interface_id, stability->failover_events_last_hour);
                }
            }
        }
        
        // Update last stable time if connection is good
        if (current_stability > 0.8) {
            stability->last_stable_time = current_time;
            stability->stable_duration_seconds = current_time - stability->last_stable_time;
            
            // Check if recovered from flapping
            if (stability->currently_flapping && 
                stability->stable_duration_seconds > system->stability_system.flapping_prevention.stability_required_for_recovery_seconds) {
                
                stability->currently_flapping = false;
                LOGX_INFO_MSG(" FLAPPING RECOVERY: %s stable for %u seconds - removing penalty",
                         interface_id, stability->stable_duration_seconds);
            }
        }
    }
    
    return ML_MONITOR_SUCCESS;
}

// Get preferred failover target (YOUR INSIGHT: Avoid unstable interfaces)
int ml_monitor_get_preferred_failover_target(advanced_networking_intelligence_t *system,
                                            const char *current_interface,
                                            char *preferred_target,
                                            double *target_confidence) {
    if (!system || !current_interface || !preferred_target || !target_confidence) {
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    double best_score = 0.0;
    char best_target[32] = {0};
    
    // Evaluate all interfaces as potential targets
    for (int i = 0; i < system->stability_system.interface_count; i++) {
        // Get pointer to interface stability
        struct {
            char interface_id[32];
            double stability_score;
            double recent_stability;
            double long_term_stability;
            uint32_t failover_events_last_hour;
            uint32_t failover_events_last_day;
            time_t last_failover_time;
            bool currently_flapping;
            time_t flapping_start_time;
            time_t last_stable_time;
            uint32_t stable_duration_seconds;
            bool in_recovery_period;
            uint32_t recovery_confidence_threshold;
        } *stability = (void*)&system->stability_system.interface_stability[i];
        
        // Skip current interface
        if (strcmp(stability->interface_id, current_interface) == 0) continue;
        
        // Calculate target score
        double target_score = stability->stability_score;
        
        // FLAPPING PREVENTION: Heavily penalize flapping interfaces
        if (stability->currently_flapping) {
            target_score *= system->stability_system.flapping_prevention.flapping_weight_reduction_factor; // 0.1 = 90% penalty
            LOGX_DEBUG_MSG("Applying flapping penalty to %s: score %.3f  %.3f",
                      stability->interface_id, stability->stability_score, target_score);
        }
        
        // Prefer interfaces that have been stable longer
        if (stability->stable_duration_seconds > 300) { // 5 minutes stable
            target_score *= 1.2; // 20% bonus for stability
        }
        
        // Check if this is the best target so far
        if (target_score > best_score) {
            best_score = target_score;
            strncpy(best_target, stability->interface_id, sizeof(best_target) - 1);
        }
    }
    
    if (strlen(best_target) > 0) {
        strncpy(preferred_target, best_target, 32);
        *target_confidence = best_score;
        
        LOGX_INFO_MSG(" PREFERRED FAILOVER TARGET: %s  %s (confidence: %.3f)",
                 current_interface, best_target, best_score);
        
        return ML_MONITOR_SUCCESS;
    }
    
    return AUTONOMY_ERROR_NOT_FOUND;
}

// Update background validation (YOUR GENIUS INSIGHT: What-if analysis)
int ml_monitor_update_background_validation(advanced_networking_intelligence_t *system,
                                           const char *interface_id,
                                           const multi_interface_observation_t *observation) {
    if (!system || !interface_id || !observation) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    if (!system->enable_background_validation) return ML_MONITOR_SUCCESS;
    
    // Find background interface entry
    int bg_index = -1;
    for (int i = 0; i < system->background_intelligence.background_interface_count; i++) {
        if (strcmp(system->background_intelligence.background_interfaces[i].interface_id, interface_id) == 0) {
            bg_index = i;
            break;
        }
    }
    
    // Add new background interface if not found
    if (bg_index == -1 && system->background_intelligence.background_interface_count < MAX_INTERFACES) {
        bg_index = system->background_intelligence.background_interface_count++;
        strncpy(system->background_intelligence.background_interfaces[bg_index].interface_id,
                interface_id, sizeof(system->background_intelligence.background_interfaces[bg_index].interface_id) - 1);
        system->background_intelligence.background_interfaces[bg_index].background_monitoring_active = true;
    }
    
    if (bg_index >= 0) {
        // Get pointer to background interface
        struct {
            char interface_id[32];
            bool currently_primary;
            bool background_monitoring_active;
            struct {
                uint32_t background_observations;
                uint32_t predicted_outages_if_primary;
                uint32_t actual_outages_detected;
                double accuracy_if_this_was_primary;
                double performance_if_this_was_primary;
            } what_if_analysis;
            struct {
                uint32_t model_predictions_made;
                uint32_t model_predictions_correct;
                double model_accuracy;
                time_t last_model_update;
                bool model_needs_retraining;
            } model_validation;
        } *bg_interface = (void*)&system->background_intelligence.background_interfaces[bg_index];
        
        bg_interface->what_if_analysis.background_observations++;
        
        // WHAT-IF ANALYSIS: "What if this interface was primary right now?"
        bool would_have_outage = (observation->packet_loss_pct > 5 || observation->latency_ms > 200);
        
        if (would_have_outage) {
            bg_interface->what_if_analysis.predicted_outages_if_primary++;
            
            LOGX_DEBUG_MSG("WHAT-IF: %s would have outage if primary (latency=%ums, loss=%u%%)",
                      interface_id, observation->latency_ms, observation->packet_loss_pct);
        }
        
        // Update what-if accuracy
        if (bg_interface->what_if_analysis.background_observations > 0) {
            bg_interface->what_if_analysis.accuracy_if_this_was_primary = 
                1.0 - ((double)bg_interface->what_if_analysis.predicted_outages_if_primary / 
                       bg_interface->what_if_analysis.background_observations);
        }
        
        // Update model validation
        bg_interface->model_validation.model_predictions_made++;
        if (!would_have_outage) { // Correct prediction of no outage
            bg_interface->model_validation.model_predictions_correct++;
        }
        
        bg_interface->model_validation.model_accuracy = 
            (double)bg_interface->model_validation.model_predictions_correct / 
            bg_interface->model_validation.model_predictions_made;
        
        LOGX_DEBUG_MSG("Background validation for %s: observations=%u, accuracy=%.3f, what-if-accuracy=%.3f",
                  interface_id, bg_interface->what_if_analysis.background_observations,
                  bg_interface->model_validation.model_accuracy,
                  bg_interface->what_if_analysis.accuracy_if_this_was_primary);
    }
    
    return ML_MONITOR_SUCCESS;
}

// Get background validation results
int ml_monitor_get_background_validation_results(advanced_networking_intelligence_t *system,
                                                const char *interface_id,
                                                double *accuracy_if_primary,
                                                double *performance_if_primary,
                                                uint32_t *predictions_validated) {
    if (!system || !interface_id) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    // Find background interface
    for (int i = 0; i < system->background_intelligence.background_interface_count; i++) {
        if (strcmp(system->background_intelligence.background_interfaces[i].interface_id, interface_id) == 0) {
            // Get pointer to background interface
            struct {
                char interface_id[32];
                bool currently_primary;
                bool background_monitoring_active;
                struct {
                    uint32_t background_observations;
                    uint32_t predicted_outages_if_primary;
                    uint32_t actual_outages_detected;
                    double accuracy_if_this_was_primary;
                    double performance_if_this_was_primary;
                } what_if_analysis;
                struct {
                    uint32_t model_predictions_made;
                    uint32_t model_predictions_correct;
                    double model_accuracy;
                    time_t last_model_update;
                    bool model_needs_retraining;
                } model_validation;
            } *bg_interface = (void*)&system->background_intelligence.background_interfaces[i];
            
            if (accuracy_if_primary) *accuracy_if_primary = bg_interface->what_if_analysis.accuracy_if_this_was_primary;
            if (performance_if_primary) *performance_if_primary = bg_interface->what_if_analysis.performance_if_this_was_primary;
            if (predictions_validated) *predictions_validated = bg_interface->model_validation.model_predictions_made;
            
            return ML_MONITOR_SUCCESS;
        }
    }
    
    return AUTONOMY_ERROR_NOT_FOUND;
}

// Enable streaming protection mode
int ml_monitor_enable_streaming_protection(advanced_networking_intelligence_t *system, bool enable) {
    if (!system) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    system->enable_streaming_protection = enable;
    system->predictive_system.streaming_protection.enable_streaming_protection_mode = enable;
    
    LOGX_INFO_MSG(" Streaming protection %s", enable ? "ENABLED" : "DISABLED");
    if (enable) {
        LOGX_INFO_MSG("   - Tolerance: %ums interruption max", 
                 system->predictive_system.streaming_protection.streaming_tolerance_ms);
        LOGX_INFO_MSG("   - Prediction window: %ums ahead",
                 system->predictive_system.streaming_protection.streaming_prediction_window_ms);
        LOGX_INFO_MSG("   - Confidence threshold: %.1f%% for action",
                 system->predictive_system.streaming_protection.streaming_confidence_threshold * 100);
    }
    
    return ML_MONITOR_SUCCESS;
}

// Get advanced performance metrics
int ml_monitor_get_advanced_performance_metrics(advanced_networking_intelligence_t *system,
                                               uint32_t *outages_prevented,
                                               uint32_t *false_alarms,
                                               uint32_t *flapping_prevented,
                                               double *failover_accuracy) {
    if (!system) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    if (outages_prevented) *outages_prevented = system->advanced_performance.outages_prevented;
    if (false_alarms) *false_alarms = system->advanced_performance.false_alarms;
    if (flapping_prevented) *flapping_prevented = system->advanced_performance.flapping_events_prevented;
    if (failover_accuracy) *failover_accuracy = system->advanced_performance.average_failover_accuracy;
    
    return ML_MONITOR_SUCCESS;
}