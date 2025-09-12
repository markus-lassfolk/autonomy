#include "ml_monitor.h"
#include "ml_monitor_multi_interface.h"
#include "../network/network_discovery_comprehensive.h"
#include "../utils/logx.h"
#include <string.h>
#include <stdlib.h>

// Integration with Comprehensive Network Discovery System

// Map network discovery interface type to ML interface type
interface_type_t ml_monitor_map_interface_type(const network_interface_t *interface) {
    if (!interface) return INTERFACE_TYPE_UNKNOWN;
    
    // Use the enhanced interface detection from network discovery
    if (interface->is_starlink) {
        return INTERFACE_TYPE_STARLINK;
    } else if (strcmp(interface->type, "cellular") == 0) {
        return INTERFACE_TYPE_CELLULAR;
    } else if (strcmp(interface->type, "wifi") == 0) {
        return INTERFACE_TYPE_WIFI;
    } else if (strcmp(interface->type, "ethernet") == 0) {
        return INTERFACE_TYPE_LAN;
    } else if (strcmp(interface->type, "vpn") == 0) {
        return INTERFACE_TYPE_UNKNOWN; // Don't monitor VPN interfaces for ML
    }
    
    return INTERFACE_TYPE_UNKNOWN;
}

// Initialize ML monitoring for discovered interfaces
int ml_monitor_init_from_network_discovery(ml_monitor_t *monitor) {
    if (!monitor) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    // Use printf as fallback to avoid LOGX crashes
    printf("INFO: Integrating ML monitoring with comprehensive network discovery\n");
    
    // Get discovered interfaces from network discovery system
    network_interface_t discovered_interfaces[MAX_INTERFACES];
    int interface_count = 0;
    
    // Use enhanced discovery to get detailed metrics
    extern int get_enhanced_comprehensive_interface_info(network_interface_t *interfaces, int *count);
    
    // Add null pointer checks
    if (!discovered_interfaces || !&interface_count) {
        printf("ERROR: Invalid parameters for network discovery\n");
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    // Initialize interface count to 0
    interface_count = 0;
    
    int discovery_result = get_enhanced_comprehensive_interface_info(discovered_interfaces, &interface_count);
    if (discovery_result != AUTONOMY_SUCCESS) {
        printf("ERROR: Failed to get enhanced comprehensive interface info: %d\n", discovery_result);
        return ML_MONITOR_ERROR_NOT_INITIALIZED;
    }
    
    // Validate interface count
    if (interface_count < 0 || interface_count > MAX_INTERFACES) {
        printf("ERROR: Invalid interface count: %d\n", interface_count);
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    printf("INFO: Discovered %d network interfaces for ML monitoring\n", interface_count);
    
    // Initialize multi-interface system if not already done
    multi_interface_ml_system_t *multi_system = ml_monitor_get_multi_interface_system();
    if (!multi_system) {
        multi_system = ml_monitor_init_multi_interface_system(&monitor->config);
        if (!multi_system) {
            printf("ERROR: Failed to initialize multi-interface ML system\n");
            return ML_MONITOR_ERROR_NOT_INITIALIZED;
        }
    }
    
    // Add each discovered interface to ML monitoring
    int ml_interfaces_added = 0;
    
    for (int i = 0; i < interface_count; i++) {
        network_interface_t *interface = &discovered_interfaces[i];
        
        // Only monitor interfaces that are suitable for ML
        if (!interface->up || !interface->enabled) {
            printf("DEBUG: Skipping interface %s: not up or enabled\n", interface->name);
            continue;
        }
        
        // Skip VPN interfaces unless specifically configured
        if (strcmp(interface->type, "vpn") == 0) {
            printf("DEBUG: Skipping VPN interface %s\n", interface->name);
            continue;
        }
        
        // Only monitor interfaces that are tracked by MWAN3 (for failover relevance)
        if (!interface->mwan3_tracking_enabled) {
            printf("DEBUG: Skipping interface %s: not tracked by MWAN3\n", interface->name);
            continue;
        }
        
        // Map to ML interface type
        interface_type_t ml_type = ml_monitor_map_interface_type(interface);
        if (ml_type == INTERFACE_TYPE_UNKNOWN) {
            printf("DEBUG: Skipping interface %s: unknown type for ML\n", interface->name);
            continue;
        }
        
        // Add interface to ML monitoring
        int add_result = ml_monitor_add_interface(multi_system, interface->name, ml_type);
        if (add_result == ML_MONITOR_MULTI_SUCCESS) {
            ml_interfaces_added++;
            
            printf("INFO: Added %s (%s) to ML monitoring: %s, MWAN3=%s, health=%.1f\n",
                     interface->name, interface->type, 
                     interface->friendly_name, interface->mwan3_name, interface->health_score);
            
            // Initialize MWAN3 integration for this interface
            if (strlen(interface->mwan3_name) > 0) {
                // Add to MWAN3 integration tracking
                for (int j = 0; j < MAX_INTERFACES; j++) {
                    if (strlen(multi_system->mwan3_integration.mwan3_interfaces[j].interface_name) == 0) {
                        strncpy(multi_system->mwan3_integration.mwan3_interfaces[j].interface_name,
                               interface->mwan3_name, 
                               sizeof(multi_system->mwan3_integration.mwan3_interfaces[j].interface_name) - 1);
                        multi_system->mwan3_integration.mwan3_interfaces[j].base_weight = 10; // Default weight
                        multi_system->mwan3_integration.mwan3_interfaces[j].current_weight = 10;
                        multi_system->mwan3_integration.mwan3_interface_count++;
                        break;
                    }
                }
            }
        } else {
            printf("WARN: Failed to add interface %s to ML monitoring: %d\n", interface->name, add_result);
        }
    }
    
    printf("INFO: ML monitoring initialized for %d interfaces (from %d discovered)\n", 
             ml_interfaces_added, interface_count);
    
    // Log interface summary
    printf("INFO: ML Interface Summary:\n");
    for (int i = 0; i < interface_count; i++) {
        network_interface_t *interface = &discovered_interfaces[i];
        const char* ml_status = "not monitored";
        
        if (interface->up && interface->enabled && interface->mwan3_tracking_enabled) {
            interface_type_t ml_type = ml_monitor_map_interface_type(interface);
            if (ml_type != INTERFACE_TYPE_UNKNOWN) {
                ml_status = "monitored";
            }
        }
        
        printf("  - %s (%s): %s, MWAN3=%s, status=%s\n",
                 interface->name, interface->type, interface->friendly_name,
                 interface->mwan3_name, ml_status);
    }
    
    return ML_MONITOR_SUCCESS;
}

// Update ML interface list based on network discovery changes
int ml_monitor_sync_with_network_discovery(ml_monitor_t *monitor) {
    if (!monitor) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    // Get current discovered interfaces
    network_interface_t current_interfaces[MAX_INTERFACES];
    int current_count = 0;
    
    int discovery_result = get_comprehensive_interface_info(current_interfaces, &current_count);
    if (discovery_result != AUTONOMY_SUCCESS) {
        printf("WARN: Failed to sync with network discovery: %d\n", discovery_result);
        return discovery_result;
    }
    
    multi_interface_ml_system_t *multi_system = ml_monitor_get_multi_interface_system();
    if (!multi_system) {
        printf("WARN: Multi-interface system not initialized\n");
        return ML_MONITOR_ERROR_NOT_INITIALIZED;
    }
    
    // Check for new interfaces to add
    for (int i = 0; i < current_count; i++) {
        network_interface_t *interface = &current_interfaces[i];
        
        // Check if this interface should be monitored
        if (!interface->up || !interface->enabled || !interface->mwan3_tracking_enabled) {
            continue;
        }
        
        interface_type_t ml_type = ml_monitor_map_interface_type(interface);
        if (ml_type == INTERFACE_TYPE_UNKNOWN) {
            continue;
        }
        
        // Check if interface is already being monitored
        bool already_monitored = false;
        for (int j = 0; j < multi_system->interface_count; j++) {
            if (strcmp(multi_system->interface_models[j].interface_id, interface->name) == 0) {
                already_monitored = true;
                break;
            }
        }
        
        // Add new interface if not already monitored
        if (!already_monitored) {
            int add_result = ml_monitor_add_interface(multi_system, interface->name, ml_type);
            if (add_result == ML_MONITOR_MULTI_SUCCESS) {
                printf("INFO: Added new interface to ML monitoring: %s (%s)\n", 
                         interface->name, interface->type);
            }
        }
    }
    
    // TODO: Remove interfaces that are no longer available
    // This would check if any monitored interfaces are no longer in the discovered list
    
    return ML_MONITOR_SUCCESS;
}

// Get ML monitoring recommendations for network discovery
int ml_monitor_get_interface_recommendations(const char *interface_name,
                                           double *reliability_score,
                                           int *recommended_mwan3_weight,
                                           bool *recommend_for_failover) {
    if (!interface_name) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    multi_interface_ml_system_t *multi_system = ml_monitor_get_multi_interface_system();
    if (!multi_system) {
        return ML_MONITOR_ERROR_NOT_INITIALIZED;
    }
    
    // Get ML prediction for this interface
    uint8_t outage_prob, performance_score, confidence;
    int pred_result = ml_monitor_predict_interface_performance(multi_system, interface_name,
                                                             &outage_prob, &performance_score, &confidence);
    
    if (pred_result == ML_MONITOR_MULTI_SUCCESS) {
        if (reliability_score) {
            *reliability_score = (255 - outage_prob) / 255.0; // Convert to 0-1 reliability
        }
        
        if (recommended_mwan3_weight) {
            // Get MWAN3 weight recommendation
            double weight_confidence;
            ml_monitor_get_mwan3_weight_recommendation_multi(multi_system, interface_name, 
                                                     recommended_mwan3_weight, &weight_confidence);
        }
        
        if (recommend_for_failover) {
            // Recommend for failover if reliability is good and confidence is high
            *recommend_for_failover = (*reliability_score > 0.7) && (confidence > 150);
        }
        
        return ML_MONITOR_SUCCESS;
    }
    
    return pred_result;
}

// Convert network discovery interface to multi-interface observation
int ml_monitor_convert_network_interface_to_observation(const network_interface_t *interface,
                                                       multi_interface_observation_t *observation) {
    if (!interface || !observation) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    memset(observation, 0, sizeof(multi_interface_observation_t));
    
    observation->timestamp = time(NULL);
    strncpy(observation->interface_id, interface->name, sizeof(observation->interface_id) - 1);
    observation->interface_type = ml_monitor_map_interface_type(interface);
    
    // Convert network discovery data to ML observation (prioritize real-time metrics)
    if (interface->real_time_metrics.ping_latency_ms > 0) {
        observation->latency_ms = (uint16_t)interface->real_time_metrics.ping_latency_ms;
    } else {
        observation->latency_ms = (uint16_t)interface->latency;
    }
    
    // Use real-time packet loss if available
    if (interface->real_time_metrics.ping_success_rate > 0) {
        observation->packet_loss_pct = 100 - interface->real_time_metrics.ping_success_rate;
    } else {
        observation->packet_loss_pct = (uint8_t)(interface->packet_loss * 100);
    }
    
    observation->connection_stability = (uint8_t)(interface->health_score * 2.55); // Scale to 0-255
    observation->connection_health = observation->connection_stability;
    observation->quality_score = observation->connection_stability;
    observation->reliability_score = observation->connection_stability;
    
    // Add performance trend information
    if (interface->performance_history.history_count >= 10) {
        // Adjust reliability based on trends
        double trend_factor = 1.0;
        trend_factor += interface->performance_history.latency_trend * 0.1; // Latency trend impact
        trend_factor += interface->performance_history.loss_trend * 0.1;    // Loss trend impact
        trend_factor += interface->performance_history.health_trend * 0.2;  // Health trend impact
        
        observation->reliability_score = (uint8_t)(observation->reliability_score * fmax(0.5, fmin(1.5, trend_factor)));
    }
    
    // Interface-specific metrics from network discovery
    switch (observation->interface_type) {
        case INTERFACE_TYPE_STARLINK:
            if (strlen(interface->starlink_dish_id) > 0) {
                // Starlink-specific data available
                observation->interface_specific.starlink.snr_x100 = interface->signal_strength * 10; // Estimate
                // Other Starlink metrics would come from Starlink collector
            }
            break;
            
        case INTERFACE_TYPE_CELLULAR:
            if (strlen(interface->modem_model) > 0) {
                // Use enhanced cellular metrics if available
                if (interface->enhanced_cellular_info.signal_strength_dbm != 0) {
                    observation->interface_specific.cellular.signal_strength_dbm = interface->enhanced_cellular_info.signal_strength_dbm;
                    observation->interface_specific.cellular.signal_quality = interface->enhanced_cellular_info.signal_quality;
                    observation->interface_specific.cellular.rsrp_dbm = interface->enhanced_cellular_info.rsrp_dbm;
                    observation->interface_specific.cellular.rsrq_db = interface->enhanced_cellular_info.rsrq_db;
                    observation->interface_specific.cellular.sinr_db = interface->enhanced_cellular_info.sinr_db;
                    
                    // Set network technology
                    if (strcmp(interface->enhanced_cellular_info.network_technology, "4G") == 0) {
                        observation->interface_specific.cellular.network_type = 4;
                    } else if (strcmp(interface->enhanced_cellular_info.network_technology, "5G") == 0) {
                        observation->interface_specific.cellular.network_type = 5;
                    } else {
                        observation->interface_specific.cellular.network_type = 3; // 3G default
                    }
                } else {
                    // Fallback to basic metrics
                    observation->interface_specific.cellular.signal_strength_dbm = interface->signal_strength;
                    observation->interface_specific.cellular.signal_quality = (uint8_t)(interface->health_score * 2.55);
                }
            }
            break;
            
        case INTERFACE_TYPE_WIFI:
            if (strlen(interface->ssid) > 0) {
                observation->interface_specific.wifi.rssi_dbm = interface->signal_strength;
                // Additional WiFi metrics would come from iw commands
            }
            break;
            
        case INTERFACE_TYPE_LAN:
            // LAN metrics from interface statistics
            observation->interface_specific.lan.link_speed_mbps = (uint8_t)(interface->metrics.throughput_mbps); // Use throughput from metrics
            observation->interface_specific.lan.duplex = 1; // Assume full duplex
            observation->interface_specific.lan.cable_quality = observation->connection_health;
            break;
            
        default:
            break;
    }
    
    return ML_MONITOR_SUCCESS;
}

// Get interface type string for logging
const char* ml_monitor_get_interface_type_string(interface_type_t type) {
    switch (type) {
        case INTERFACE_TYPE_STARLINK: return "Starlink";
        case INTERFACE_TYPE_CELLULAR: return "Cellular";
        case INTERFACE_TYPE_WIFI: return "WiFi";
        case INTERFACE_TYPE_LAN: return "LAN";
        default: return "Unknown";
    }
}

// Periodic sync with network discovery (call this periodically)
int ml_monitor_periodic_network_discovery_sync(ml_monitor_t *monitor) {
    if (!monitor) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    static time_t last_sync = 0;
    time_t now = time(NULL);
    
    // Sync every 5 minutes to detect new/removed interfaces
    if (now - last_sync < 300) {
        return ML_MONITOR_SUCCESS;
    }
    
    printf("DEBUG: Performing periodic sync with network discovery\n");
    
    int sync_result = ml_monitor_sync_with_network_discovery(monitor);
    if (sync_result == ML_MONITOR_SUCCESS) {
        printf("DEBUG: Network discovery sync completed successfully\n");
    } else {
        printf("WARN: Network discovery sync failed: %d\n", sync_result);
    }
    
    last_sync = now;
    return sync_result;
}

// Check if interface is suitable for ML monitoring based on network discovery data
bool ml_monitor_is_interface_suitable_for_ml(const network_interface_t *interface) {
    if (!interface) return false;
    
    // Must be up and enabled
    if (!interface->up || !interface->enabled) {
        return false;
    }
    
    // Must be tracked by MWAN3 (indicates it's relevant for failover)
    if (!interface->mwan3_tracking_enabled) {
        return false;
    }
    
    // Must be a type we can monitor
    interface_type_t ml_type = ml_monitor_map_interface_type(interface);
    if (ml_type == INTERFACE_TYPE_UNKNOWN) {
        return false;
    }
    
    // Must have reasonable health score
    if (interface->health_score < 50.0) {
        return false;
    }
    
    return true;
}

// Determine ML monitoring strategy based on MWAN3 ping frequency
typedef enum {
    ML_MONITORING_STRATEGY_FULL,        // Full ML monitoring with our own pings
    ML_MONITORING_STRATEGY_MWAN3_BASED, // Use MWAN3 ping results for ML
    ML_MONITORING_STRATEGY_HYBRID,      // Combination of both
    ML_MONITORING_STRATEGY_MINIMAL      // Minimal monitoring (cost-sensitive)
} ml_monitoring_strategy_t;

ml_monitoring_strategy_t ml_monitor_determine_monitoring_strategy(const network_interface_t *interface) {
    if (!interface) return ML_MONITORING_STRATEGY_MINIMAL;
    
    // For cellular interfaces, be cost-conscious
    if (strcmp(interface->type, "cellular") == 0) {
        if (interface->real_time_metrics.mwan3_ping_active && 
            interface->real_time_metrics.mwan3_ping_interval <= 10) {
            // MWAN3 is pinging frequently (every 10 seconds or less)
            // Use MWAN3 results to avoid data costs
            return ML_MONITORING_STRATEGY_MWAN3_BASED;
        } else {
            // MWAN3 not pinging frequently, use minimal monitoring
            // Focus on modem metrics (SNR, RSRP, etc.) which don't cost data
            return ML_MONITORING_STRATEGY_MINIMAL;
        }
    }
    
    // For Starlink, WiFi, LAN - no data cost concerns
    if (strcmp(interface->type, "starlink") == 0 || 
        strcmp(interface->type, "wifi") == 0 || 
        strcmp(interface->type, "ethernet") == 0) {
        
        if (interface->real_time_metrics.mwan3_ping_active && 
            interface->real_time_metrics.mwan3_ping_interval <= 5) {
            // MWAN3 pinging very frequently, use hybrid approach
            return ML_MONITORING_STRATEGY_HYBRID;
        } else {
            // Do full ML monitoring
            return ML_MONITORING_STRATEGY_FULL;
        }
    }
    
    return ML_MONITORING_STRATEGY_MINIMAL;
}

// Get monitoring frequency recommendation based on interface and MWAN3 status
int ml_monitor_get_monitoring_frequency_recommendation(const network_interface_t *interface) {
    if (!interface) return 60; // Default 1 minute
    
    ml_monitoring_strategy_t strategy = ml_monitor_determine_monitoring_strategy(interface);
    
    switch (strategy) {
        case ML_MONITORING_STRATEGY_FULL:
            // Full monitoring for no-cost interfaces
            if (strcmp(interface->type, "starlink") == 0) {
                return 1; // Every second for Starlink (streaming protection)
            } else if (strcmp(interface->type, "wifi") == 0 || strcmp(interface->type, "ethernet") == 0) {
                return 1; // Every second for LAN/WiFi
            }
            return 5; // Every 5 seconds default
            
        case ML_MONITORING_STRATEGY_MWAN3_BASED:
            // Use MWAN3 ping frequency
            return interface->real_time_metrics.mwan3_ping_interval;
            
        case ML_MONITORING_STRATEGY_HYBRID:
            // Complement MWAN3 pings with our own less frequent monitoring
            return interface->real_time_metrics.mwan3_ping_interval * 2;
            
        case ML_MONITORING_STRATEGY_MINIMAL:
            // Minimal monitoring for cost-sensitive interfaces
            if (strcmp(interface->type, "cellular") == 0) {
                return 300; // Every 5 minutes for cellular
            }
            return 60; // Every minute for others
    }
    
    return 60; // Default fallback
}

// Check if we should use MWAN3 ping results instead of our own
bool ml_monitor_should_use_mwan3_ping_results(const network_interface_t *interface) {
    if (!interface || !interface->real_time_metrics.mwan3_ping_active) {
        return false;
    }
    
    ml_monitoring_strategy_t strategy = ml_monitor_determine_monitoring_strategy(interface);
    
    // Use MWAN3 results for cellular (to avoid data costs) or when MWAN3 is very frequent
    return (strategy == ML_MONITORING_STRATEGY_MWAN3_BASED || 
            strategy == ML_MONITORING_STRATEGY_HYBRID);
}

// Get enhanced interface information for ML monitoring
int ml_monitor_get_enhanced_interface_info(const char *interface_name,
                                         char *friendly_name,
                                         char *mwan3_name,
                                         bool *mwan3_tracking_enabled,
                                         double *health_score,
                                         interface_type_t *ml_type) {
    if (!interface_name) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    // Get interface info from network discovery
    network_interface_t discovered_interfaces[MAX_INTERFACES];
    int interface_count = 0;
    
    int discovery_result = get_comprehensive_interface_info(discovered_interfaces, &interface_count);
    if (discovery_result != AUTONOMY_SUCCESS) {
        return discovery_result;
    }
    
    // Find the requested interface
    for (int i = 0; i < interface_count; i++) {
        if (strcmp(discovered_interfaces[i].name, interface_name) == 0) {
            network_interface_t *interface = &discovered_interfaces[i];
            
            if (friendly_name) {
                strncpy(friendly_name, interface->friendly_name, 64);
            }
            if (mwan3_name) {
                strncpy(mwan3_name, interface->mwan3_name, 32);
            }
            if (mwan3_tracking_enabled) {
                *mwan3_tracking_enabled = interface->mwan3_tracking_enabled;
            }
            if (health_score) {
                *health_score = interface->health_score;
            }
            if (ml_type) {
                *ml_type = ml_monitor_map_interface_type(interface);
            }
            
            return ML_MONITOR_SUCCESS;
        }
    }
    
    return ML_MONITOR_MULTI_ERROR_NOT_FOUND;
}