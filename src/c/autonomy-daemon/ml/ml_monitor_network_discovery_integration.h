#ifndef ML_MONITOR_NETWORK_DISCOVERY_INTEGRATION_H
#define ML_MONITOR_NETWORK_DISCOVERY_INTEGRATION_H

#include "ml_monitor.h"
#include "ml_monitor_multi_interface.h"
#include "../network/network_discovery_comprehensive.h"

// Integration with Comprehensive Network Discovery System

// Initialize ML monitoring from discovered network interfaces
int ml_monitor_init_from_network_discovery(ml_monitor_t *monitor);

// Sync ML monitoring with current network discovery state
int ml_monitor_sync_with_network_discovery(ml_monitor_t *monitor);

// Periodic sync with network discovery (call every 5 minutes)
int ml_monitor_periodic_network_discovery_sync(ml_monitor_t *monitor);

// Get ML monitoring recommendations for network discovery
int ml_monitor_get_interface_recommendations(const char *interface_name,
                                           double *reliability_score,
                                           int *recommended_mwan3_weight,
                                           bool *recommend_for_failover);

// Convert network discovery interface to ML observation
int ml_monitor_convert_network_interface_to_observation(const network_interface_t *interface,
                                                       multi_interface_observation_t *observation);

// Check if interface is suitable for ML monitoring
bool ml_monitor_is_interface_suitable_for_ml(const network_interface_t *interface);

// Get enhanced interface information for ML monitoring
int ml_monitor_get_enhanced_interface_info(const char *interface_name,
                                         char *friendly_name,
                                         char *mwan3_name,
                                         bool *mwan3_tracking_enabled,
                                         double *health_score,
                                         interface_type_t *ml_type);

// Get interface type string for logging
const char* ml_monitor_get_interface_type_string(interface_type_t type);

#endif // ML_MONITOR_NETWORK_DISCOVERY_INTEGRATION_H