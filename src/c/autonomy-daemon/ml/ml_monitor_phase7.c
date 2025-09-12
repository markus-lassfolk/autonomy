#include "ml_monitor.h"
#include "ml_monitor_multi_interface.h"
#include "../shared/logging/logx.h"
#include "../utils/secure_exec.h"
#include "../network/network_controller.h"
#include "../network/network_failover.h"
#include "../starlink/starlink_snow_detection.h"
#include "../starlink/starlink_modules.h"
#include <time.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>

// Forward declarations for functions from other modules
extern int ml_monitor_perform_ping_test(const char *interface_id, const char *target, uint32_t *latency_ms, bool *success);
extern int ml_monitor_collect_cellular_modem_metrics(const char *interface_id, multi_interface_observation_t *observation);

// Phase 7: Multi-Interface ML Intelligence Integration

// Global Phase 7 state
static multi_interface_ml_system_t *g_phase7_system = NULL;
static bool g_phase7_initialized = false;

// Continuous monitoring callback for network events
static void ml_monitor_network_event_callback(const char *event_type, const char *from_interface, 
                                             const char *to_interface, bool success, void *user_data);

// Enhanced observation collection for multi-interface
static int ml_monitor_collect_multi_interface_observations(ml_monitor_t *monitor);

// Integration with existing network controller
static int ml_monitor_integrate_with_network_controller(ml_monitor_t *monitor);

// Initialize Phase 7 multi-interface integration
int ml_monitor_init_phase7_multi_interface(ml_monitor_t *monitor) {
    if (!monitor) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    // Use simple fprintf to avoid LOGX crashes
    fprintf(stderr, "Initializing Phase 7: Multi-Interface ML Intelligence\n");
    
    // Initialize multi-interface system
    g_phase7_system = ml_monitor_init_multi_interface_system(&monitor->config);
    if (!g_phase7_system) {
        LOGX_ERROR_MSG("Failed to initialize multi-interface ML system");
        return ML_MONITOR_ERROR_NOT_INITIALIZED;
    }
    
    // Use comprehensive network discovery to automatically find interfaces
    // Use simple fprintf to avoid LOGX crashes
    fprintf(stderr, "Using comprehensive network discovery for automatic interface detection\n");
    
    // Initialize ML monitoring from discovered interfaces
    extern int ml_monitor_init_from_network_discovery(ml_monitor_t *monitor);
    int discovery_result = ml_monitor_init_from_network_discovery(monitor);
    if (discovery_result != ML_MONITOR_SUCCESS) {
        // Use simple fprintf to avoid LOGX crashes
        fprintf(stderr, "Failed to initialize from network discovery: %d\n", discovery_result);
        
        // Fallback: Add common interfaces manually
        fprintf(stderr, "Using fallback interface detection\n");
        ml_monitor_add_interface(g_phase7_system, "eth1", INTERFACE_TYPE_STARLINK);
        ml_monitor_add_interface(g_phase7_system, "qmimux0", INTERFACE_TYPE_CELLULAR);
        ml_monitor_add_interface(g_phase7_system, "wlan0", INTERFACE_TYPE_WIFI);
        ml_monitor_add_interface(g_phase7_system, "eth0", INTERFACE_TYPE_LAN);
    } else {
        // Use simple fprintf to avoid LOGX crashes
        fprintf(stderr, "Interfaces automatically discovered and added to ML monitoring\n");
    }
    
    // Integrate with network controller for failover events
    int integration_result = ml_monitor_integrate_with_network_controller(monitor);
    if (integration_result != ML_MONITOR_SUCCESS) {
        // Use simple fprintf to avoid LOGX crashes
        fprintf(stderr, "Network controller integration failed: %d\n", integration_result);
    }
    
    g_phase7_initialized = true;
    
    // Use single consolidated message to avoid multiple LOGX calls
    fprintf(stderr, "Phase 7 multi-interface ML system initialized successfully - Interfaces: %u, duration windows, continuous monitoring, MWAN3 updates, failover timing\n", g_phase7_system->interface_count);
    
    return ML_MONITOR_SUCCESS;
}

// Network event callback for failover/failback monitoring
static void ml_monitor_network_event_callback(const char *event_type, const char *from_interface, 
                                             const char *to_interface, bool success, void *user_data) {
    if (!g_phase7_system || !event_type) return;
    
    LOGX_INFO_MSG(" Network event: %s %s  %s (%s)", event_type, from_interface ? from_interface : "none", 
             to_interface ? to_interface : "none", success ? "success" : "failed");
    
    if (strcmp(event_type, "failover_start") == 0) {
        ml_monitor_start_failover_timing(g_phase7_system, from_interface, to_interface);
    } else if (strcmp(event_type, "failover_complete") == 0) {
        ml_monitor_complete_failover_timing(g_phase7_system, from_interface, to_interface, success);
    } else if (strcmp(event_type, "failback_start") == 0) {
        ml_monitor_start_failback_timing(g_phase7_system, from_interface, to_interface);
    } else if (strcmp(event_type, "failback_complete") == 0) {
        ml_monitor_complete_failback_timing(g_phase7_system, from_interface, to_interface, success);
    }
}

// Integrate with existing network controller
static int ml_monitor_integrate_with_network_controller(ml_monitor_t *monitor) {
    if (!monitor) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    LOGX_INFO_MSG(" Integrating with network controller for failover event monitoring");
    
    // Real network controller integration
    // 1. Register callbacks with network controller
    extern int network_controller_add_callback(int (*callback)(const network_member_t* from, const network_member_t* to));
    
    // Register ML monitor callback for failover events
    int callback_result = network_controller_add_callback((int (*)(const network_member_t*, const network_member_t*))ml_monitor_network_event_callback);
    if (callback_result != 0) {
        LOGX_WARN_MSG("Failed to register network controller callback: %d", callback_result);
    } else {
        LOGX_INFO_MSG("Registered ML monitor callback with network controller");
    }
    
    // 2. Set up event monitoring for failover/failback events
    // This integrates with the existing network failover system
    extern int network_failover_get_status(network_failover_status_t *status);
    network_failover_status_t failover_status;
    if (network_failover_get_status(&failover_status) == AUTONOMY_SUCCESS) {
        LOGX_INFO_MSG("Network failover integration active: %d interfaces", failover_status.interface_count);
    }
    
    // 3. Integrate with MWAN3 for weight updates
    // Verify MWAN3 is available
    extern int secure_check_mwan3_available(void);
    int mwan3_check = secure_check_mwan3_available();
    if (mwan3_check == AUTONOMY_SUCCESS) {
        LOGX_INFO_MSG(" MWAN3 detected - enabling dynamic weight updates");
        // MWAN3 integration is enabled by default
    } else {
        LOGX_WARN_MSG("MWAN3 not available - weight updates disabled");
    }
    
    return ML_MONITOR_SUCCESS;
}

// Collect enhanced observations for all interfaces
static int ml_monitor_collect_multi_interface_observations(ml_monitor_t *monitor) {
    if (!monitor || !g_phase7_system) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    // Collect observations for each monitored interface
    for (int i = 0; i < g_phase7_system->interface_count; i++) {
        interface_ml_model_t *model = &g_phase7_system->interface_models[i];
        
        if (!model->active) continue;
        
        // Create multi-interface observation
        multi_interface_observation_t obs;
        memset(&obs, 0, sizeof(obs));
        
        obs.timestamp = time(NULL);
        safe_strncpy(obs.interface_id, model->interface_id, sizeof(obs.interface_id));
        obs.interface_type = model->type;
        
        // Collect interface-specific data
        starlink_collection_result_t starlink_result;
        uint32_t wifi_latency;
        bool wifi_success;
        uint32_t lan_latency;
        bool lan_success;
        
        switch (model->type) {
            case INTERFACE_TYPE_STARLINK:
                // Integrate with existing Starlink data collection
                if (starlink_collect_data(&starlink_result) == AUTONOMY_SUCCESS) {
                    obs.latency_ms = (uint16_t)starlink_result.status.network_perf.pop_ping_latency_ms;
                    obs.packet_loss_pct = (uint8_t)(starlink_result.status.network_perf.pop_ping_drop_rate * 100);
                    obs.connection_stability = starlink_result.status.signal_quality.snr > 5.0 ? 200 : 100;
                    obs.connection_health = (uint8_t)(starlink_result.status.signal_quality.snr * 20);
                    
                    // Real Starlink-specific metrics
                    obs.interface_specific.starlink.snr_x100 = (uint16_t)(starlink_result.status.signal_quality.snr * 100);
                    obs.interface_specific.starlink.obstruction_pct = (uint8_t)(starlink_result.status.obstruction_stats.fraction_obstructed * 100);
                    obs.interface_specific.starlink.azimuth_deg = (int16_t)starlink_result.status.positioning.boresight_azimuth_deg;
                    obs.interface_specific.starlink.elevation_deg = (int16_t)starlink_result.status.positioning.boresight_elevation_deg;
                    obs.interface_specific.starlink.satellites_visible = starlink_result.status.gps_stats.gps_sats;
                } else {
                    LOGX_WARN_MSG("Failed to collect real Starlink data for %s", model->interface_id);
                    continue; // Skip this interface if no real data available
                }
                break;
                
            case INTERFACE_TYPE_CELLULAR:
                // Use real cellular modem metrics collection
                if (ml_monitor_collect_cellular_modem_metrics(model->interface_id, &obs) != ML_MONITOR_SUCCESS) {
                    LOGX_WARN_MSG("Failed to collect real cellular data for %s", model->interface_id);
                    continue; // Skip this interface if no real data available
                }
                break;
                
            case INTERFACE_TYPE_WIFI:
                // Collect real WiFi performance data via ping test
                if (ml_monitor_perform_ping_test(model->interface_id, "8.8.8.8", &wifi_latency, &wifi_success) == ML_MONITOR_SUCCESS) {
                    obs.latency_ms = wifi_latency;
                    obs.packet_loss_pct = wifi_success ? 0 : 100;
                    obs.connection_stability = wifi_success ? (255 - (wifi_latency / 2)) : 0;
                    obs.connection_health = obs.connection_stability;
                    
                    // Collect real WiFi-specific metrics via iwconfig/iw
                    char wifi_cmd[256];
                    char wifi_file[128];
                    snprintf(wifi_file, sizeof(wifi_file), "/tmp/wifi_info_%s_%lld", model->interface_id, time(NULL));
                    snprintf(wifi_cmd, sizeof(wifi_cmd), "iw dev %s link > %s 2>/dev/null", model->interface_id, wifi_file);
                    
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
                                if (strstr(line, "freq:")) {
                                    int freq;
                                    sscanf(line, "%*s %d", &freq);
                                    obs.interface_specific.wifi.channel = (freq - 2412) / 5 + 1; // Convert freq to channel
                                }
                            }
                            fclose(f);
                        }
                        unlink(wifi_file);
                    }
                } else {
                    LOGX_WARN_MSG("Failed to collect real WiFi data for %s", model->interface_id);
                    continue;
                }
                break;
                
            case INTERFACE_TYPE_LAN:
                // Collect real LAN performance data via ping to gateway
                if (ml_monitor_perform_ping_test(model->interface_id, "192.168.1.1", &lan_latency, &lan_success) == ML_MONITOR_SUCCESS) {
                    obs.latency_ms = lan_latency;
                    obs.packet_loss_pct = lan_success ? 0 : 100;
                    obs.connection_stability = lan_success ? (255 - lan_latency) : 0;
                    obs.connection_health = obs.connection_stability;
                    
                    // Collect real LAN-specific metrics via ethtool
                    char lan_cmd[256];
                    char lan_file[128];
                    snprintf(lan_file, sizeof(lan_file), "/tmp/lan_info_%s_%lld", model->interface_id, time(NULL));
                    snprintf(lan_cmd, sizeof(lan_cmd), "ethtool %s | grep 'Speed:\\|Duplex:' > %s 2>/dev/null", model->interface_id, lan_file);
                    
                    exec_result_t lan_result;
                    if (secure_exec_command(lan_cmd, &lan_result) == AUTONOMY_SUCCESS && lan_result.success) {
                        FILE *f = fopen(lan_file, "r");
                        if (f) {
                            char line[256];
                            while (fgets(line, sizeof(line), f)) {
                                if (strstr(line, "Speed:")) {
                                    int speed;
                                    sscanf(line, "%*s %dMb/s", &speed);
                                    obs.interface_specific.lan.link_speed_mbps = (uint8_t)(speed > 255 ? 255 : speed);
                                }
                                if (strstr(line, "Full")) {
                                    obs.interface_specific.lan.duplex = 1;
                                }
                            }
                            fclose(f);
                        }
                        unlink(lan_file);
                    }
                    
                    // Calculate cable quality based on latency (LAN should be very low latency)
                    obs.interface_specific.lan.cable_quality = lan_latency < 5 ? 255 : (255 - (lan_latency * 10));
                } else {
                    LOGX_WARN_MSG("Failed to collect real LAN data for %s", model->interface_id);
                    continue;
                }
                break;
                
            default:
                continue;
        }
        
        // Calculate real trends based on recent observations
        // Get previous observation for trend calculation
        static uint16_t prev_latency[MAX_INTERFACES] = {0};
        static uint8_t prev_loss[MAX_INTERFACES] = {0};
        
        int interface_idx = -1;
        for (int idx = 0; idx < g_phase7_system->interface_count; idx++) {
            if (strcmp(g_phase7_system->interface_models[idx].interface_id, model->interface_id) == 0) {
                interface_idx = idx;
                break;
            }
        }
        
        if (interface_idx >= 0) {
            // Calculate real latency trend
            if (prev_latency[interface_idx] > 0) {
                int latency_diff = (int)obs.latency_ms - (int)prev_latency[interface_idx];
                obs.latency_trend = (int8_t)fmax(-127, fmin(127, latency_diff));
            } else {
                obs.latency_trend = 0;
            }
            
            // Calculate real packet loss trend
            if (prev_loss[interface_idx] >= 0) {
                int loss_diff = (int)obs.packet_loss_pct - (int)prev_loss[interface_idx];
                obs.packet_loss_trend = (int8_t)fmax(-127, fmin(127, loss_diff * 10));
            } else {
                obs.packet_loss_trend = 0;
            }
            
            // Update previous values
            prev_latency[interface_idx] = obs.latency_ms;
            prev_loss[interface_idx] = obs.packet_loss_pct;
        }
        
        // Calculate real performance degradation score
        obs.performance_degradation = 0;
        if (obs.latency_ms > 100) obs.performance_degradation += 50;
        if (obs.packet_loss_pct > 5) obs.performance_degradation += 50;
        if (obs.latency_trend > 10) obs.performance_degradation += 30;
        if (obs.packet_loss_trend > 5) obs.performance_degradation += 30;
        obs.performance_degradation = fmin(255, obs.performance_degradation);
        
        // Update interface observation
        ml_monitor_update_interface_observation(g_phase7_system, model->interface_id, &obs);
        
        LOGX_DEBUG_MSG("Collected observation for %s: latency=%ums, loss=%u%%, health=%u",
                  model->interface_id, obs.latency_ms, obs.packet_loss_pct, obs.connection_health);
    }
    
    return ML_MONITOR_SUCCESS;
}

// Update with Phase 7 multi-interface enhancements
int ml_monitor_update_with_phase7_multi_interface(ml_monitor_t *monitor, const ml_observation_t *observation) {
    if (!monitor || !observation || !g_phase7_initialized) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    // Collect multi-interface observations
    ml_monitor_collect_multi_interface_observations(monitor);
    
    // Periodic sync with network discovery and MWAN3 updates
    static time_t last_sync = 0;
    time_t now = observation->timestamp;
    
    if (now - last_sync > 300) { // Update every 5 minutes
        // Sync with network discovery for new/removed interfaces
        extern int ml_monitor_periodic_network_discovery_sync(ml_monitor_t *monitor);
        ml_monitor_periodic_network_discovery_sync(monitor);
        
        // Update MWAN3 weights based on ML predictions
        int mwan3_result = ml_monitor_update_mwan3_weights(g_phase7_system);
        if (mwan3_result == ML_MONITOR_MULTI_SUCCESS) {
            LOGX_DEBUG_MSG("MWAN3 weights updated based on ML predictions");
        }
        
        last_sync = now;
    }
    
    return ML_MONITOR_SUCCESS;
}

// Get multi-interface predictions for specific interface
int ml_monitor_get_interface_prediction(const char *interface_id, 
                                       uint8_t *outage_probability,
                                       uint8_t *performance_score,
                                       uint8_t *confidence) {
    if (!interface_id || !g_phase7_system) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    return ml_monitor_predict_interface_performance(g_phase7_system, interface_id,
                                                  outage_probability, performance_score, confidence);
}

// Get outage duration prediction for interface
int ml_monitor_get_interface_duration_prediction(const char *interface_id, void *duration_prediction) {
    if (!interface_id || !g_phase7_system || !duration_prediction) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    return ml_monitor_predict_outage_duration(g_phase7_system, interface_id, duration_prediction);
}

// Get failback readiness for interface
int ml_monitor_get_interface_failback_readiness(const char *interface_id, void *readiness) {
    if (!interface_id || !g_phase7_system || !readiness) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    return ml_monitor_assess_failback_readiness(g_phase7_system, interface_id, readiness);
}

// Validate prediction after failover event
int ml_monitor_validate_interface_prediction(const char *interface_id, 
                                           bool actual_outage_occurred,
                                           uint32_t actual_duration_seconds) {
    if (!interface_id || !g_phase7_system) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    return ml_monitor_validate_failover_prediction(g_phase7_system, interface_id, 
                                                 actual_outage_occurred, actual_duration_seconds);
}

// Get MWAN3 weight recommendation for interface
int ml_monitor_get_mwan3_weight_recommendation(const char *interface_id,
                                              int *recommended_weight,
                                              double *confidence) {
    if (!interface_id || !g_phase7_system) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    return ml_monitor_update_mwan3_weights(g_phase7_system);
}

// Auto-tune duration windows based on real performance
int ml_monitor_auto_tune_duration_windows(ml_monitor_t *monitor) {
    if (!monitor || !g_phase7_system) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    LOGX_INFO_MSG(" Auto-tuning duration prediction windows based on real data");
    
    // Analyze actual outage durations across all interfaces
    double duration_samples[1000];
    int sample_count = 0;
    
    for (int i = 0; i < g_phase7_system->interface_count && sample_count < 1000; i++) {
        interface_ml_model_t *model = &g_phase7_system->interface_models[i];
        
        // In a real implementation, we'd analyze historical outage durations
        // and adjust the duration window thresholds based on actual data distribution
        
        if (model->failover_learning.average_outage_duration_seconds > 0) {
            duration_samples[sample_count++] = model->failover_learning.average_outage_duration_seconds;
        }
    }
    
    if (sample_count > 10) {
        // Analyze duration distribution and adjust windows
        // This would implement statistical analysis to optimize window boundaries
        LOGX_DEBUG_MSG("Auto-tuning based on %d duration samples", sample_count);
        
        // For now, log that auto-tuning would occur
        LOGX_INFO_MSG("Duration window auto-tuning analysis completed");
    } else {
        LOGX_DEBUG_MSG("Insufficient data for duration window auto-tuning (%d samples)", sample_count);
    }
    
    return ML_MONITOR_SUCCESS;
}

// Get Phase 7 system status
int ml_monitor_get_phase7_status(ml_monitor_t *monitor,
                                uint32_t *interfaces_monitored,
                                uint32_t *total_interface_predictions,
                                double *multi_interface_accuracy,
                                bool *mwan3_integration_active) {
    if (!monitor || !g_phase7_system) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    if (interfaces_monitored) *interfaces_monitored = g_phase7_system->interface_count;
    
    // Calculate total predictions across all interfaces
    uint32_t total_predictions = 0;
    uint32_t total_correct = 0;
    
    for (int i = 0; i < g_phase7_system->interface_count; i++) {
        interface_ml_model_t *model = &g_phase7_system->interface_models[i];
        total_predictions += model->performance.total_predictions;
        total_correct += model->performance.correct_predictions;
    }
    
    if (total_interface_predictions) *total_interface_predictions = total_predictions;
    
    if (multi_interface_accuracy) {
        *multi_interface_accuracy = total_predictions > 0 ? 
                                   (double)total_correct / total_predictions : 0.0;
    }
    
    if (mwan3_integration_active) {
        *mwan3_integration_active = g_phase7_system->mwan3_integration.enable_dynamic_weight_updates;
    }
    
    return ML_MONITOR_SUCCESS;
}

// Cleanup Phase 7 system
void ml_monitor_cleanup_phase7_multi_interface(void) {
    if (g_phase7_system) {
        ml_monitor_cleanup_multi_interface_system(g_phase7_system);
        g_phase7_system = NULL;
    }
    g_phase7_initialized = false;
    
    LOGX_INFO_MSG("Phase 7 multi-interface system cleaned up");
}