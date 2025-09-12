#ifndef SHARED_INTERFACE_UTILS_H
#define SHARED_INTERFACE_UTILS_H

#include "../core/common_types.h"
#include <stdbool.h>

// Shared network interface utilities
// Consolidates common network interface operations

// Interface discovery and management
int interface_utils_init(void);
void interface_utils_cleanup(void);

// Interface discovery
int interface_discover_all(network_interface_unified_t* interfaces, int max_interfaces);
int interface_discover_by_type(const char* type, network_interface_unified_t* interfaces, int max_interfaces);
bool interface_exists(const char* interface_name);
bool interface_is_up(const char* interface_name);

// Interface information
int interface_get_info(const char* interface_name, network_interface_unified_t* interface);
int interface_get_ip_address(const char* interface_name, char* ip_address, size_t size);
int interface_get_mac_address(const char* interface_name, char* mac_address, size_t size);
int interface_get_mtu(const char* interface_name, int* mtu);

// Interface metrics
int interface_get_metrics(const char* interface_name, network_metrics_unified_t* metrics);
int interface_get_statistics(const char* interface_name, uint64_t* rx_bytes, uint64_t* tx_bytes,
                             uint64_t* rx_packets, uint64_t* tx_packets);

// Interface health and status
double interface_calculate_health_score(const network_interface_unified_t* interface);
bool interface_is_healthy(const char* interface_name, double min_health_threshold);
int interface_ping_test(const char* interface_name, const char* target_host, 
                       double* latency_ms, double* packet_loss);

// Interface control (if supported)
int interface_bring_up(const char* interface_name);
int interface_bring_down(const char* interface_name);
int interface_restart(const char* interface_name);

// MWAN3 integration helpers
bool interface_is_mwan3_tracked(const char* interface_name);
int interface_get_mwan3_status(const char* interface_name, char* status, size_t size);
int interface_set_mwan3_metric(const char* interface_name, int metric);

// Interface type detection
bool interface_is_cellular(const char* interface_name);
bool interface_is_wifi(const char* interface_name);
bool interface_is_ethernet(const char* interface_name);
bool interface_is_starlink(const char* interface_name);
bool interface_is_vpn(const char* interface_name);

// Utility functions
const char* interface_type_to_string(const char* interface_name);
bool interface_name_is_valid(const char* interface_name);
int interface_compare_health(const network_interface_unified_t* a, const network_interface_unified_t* b);

// Common interface patterns
#define INTERFACE_CELLULAR_PREFIX "wwan"
#define INTERFACE_WIFI_PREFIX "wlan"
#define INTERFACE_ETHERNET_PREFIX "eth"
#define INTERFACE_STARLINK_PREFIX "starlink"
#define INTERFACE_VPN_WG_PREFIX "wg"
#define INTERFACE_VPN_OVPN_PREFIX "tun"

// Error handling
const char* interface_utils_get_last_error(void);
void interface_utils_clear_error(void);

#endif // SHARED_INTERFACE_UTILS_H