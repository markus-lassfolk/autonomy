#include "network_discovery_comprehensive.h"
#include "../utils/logx.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <time.h>
#include <dirent.h>
#include <uci.h>
#include <sys/stat.h>
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <json-c/json.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Global discovery state
static network_discovery_comprehensive_t g_comprehensive_discovery = {0};
static pthread_mutex_t g_comprehensive_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_comprehensive_initialized = false;

// Initialize comprehensive network discovery system
int network_discovery_comprehensive_init(void) {
    if (g_comprehensive_initialized) {
        LOGX_WARN_MSG("Comprehensive network discovery already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_comprehensive_mutex);
    
    // Initialize discovery state
    memset(&g_comprehensive_discovery, 0, sizeof(network_discovery_comprehensive_t));
    g_comprehensive_discovery.enabled = true;
    g_comprehensive_discovery.discovery_interval = g_config.network_check_interval;
    g_comprehensive_discovery.interface_timeout = 300;
    g_comprehensive_discovery.max_interfaces = MAX_INTERFACES;
    g_comprehensive_discovery.last_discovery = 0;
    g_comprehensive_discovery.total_discoveries = 0;
    g_comprehensive_discovery.interface_count = 0;
    
    g_comprehensive_initialized = true;
    pthread_mutex_unlock(&g_comprehensive_mutex);
    
    LOGX_INFO_MSG("Comprehensive network discovery system initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Get comprehensive interface information
int get_comprehensive_interface_info(network_interface_t *interfaces, int *count) {
    if (!g_comprehensive_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    *count = 0;
    
    // Get network interfaces from ubus
    struct ubus_context *ctx = ubus_connect(NULL);
    if (!ctx) {
        LOGX_ERROR_MSG("Failed to connect to ubus");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Get network.interface dump
    uint32_t id;
    if (ubus_lookup_id(ctx, "network.interface", &id) == 0) {
        struct blob_buf req = {0};
        blob_buf_init(&req, 0);
        
        int ret = ubus_invoke(ctx, id, "dump", req.head, NULL, NULL, 1000);
        if (ret == 0) {
            // Parse the response and populate interface info
            parse_network_interfaces_dump(ctx, interfaces, count);
        }
        blob_buf_free(&req);
    }
    
    // Get MWAN3 status
    get_mwan3_interface_info(ctx, interfaces, *count);
    
    // Get device information
    get_device_information(ctx, interfaces, *count);
    
    // Get cellular information
    get_cellular_information(ctx, interfaces, *count);
    
    // Get WiFi information
    get_wifi_information(ctx, interfaces, *count);
    
    // Detect Starlink connections
    detect_starlink_connections(interfaces, *count);
    
    // Detect VPN connections
    detect_vpn_connections(interfaces, *count);
    
    // Get friendly names from UCI
    get_friendly_names_from_uci(interfaces, *count);
    
    ubus_free(ctx);
    
    LOGX_DEBUG_MSG("Comprehensive interface discovery completed, found %d interfaces", *count);
    return AUTONOMY_SUCCESS;
}

// Parse network interfaces dump from ubus
static void parse_network_interfaces_dump(struct ubus_context *ctx, network_interface_t *interfaces, int *count) {
    // This would parse the JSON response from network.interface dump
    // For now, we'll use a simplified approach
    LOGX_DEBUG_MSG("Parsing network interfaces dump");
}

// Get MWAN3 interface information
static void get_mwan3_interface_info(struct ubus_context *ctx, network_interface_t *interfaces, int count) {
    uint32_t id;
    if (ubus_lookup_id(ctx, "mwan3", &id) != 0) {
        LOGX_DEBUG_MSG("MWAN3 not available");
        return;
    }
    
    struct blob_buf req = {0};
    blob_buf_init(&req, 0);
    
    int ret = ubus_invoke(ctx, id, "status", req.head, NULL, NULL, 1000);
    if (ret == 0) {
        // Parse MWAN3 status and update interfaces
        for (int i = 0; i < count; i++) {
            // Check if interface is in MWAN3
            if (is_interface_in_mwan3(interfaces[i].name)) {
                interfaces[i].mwan3_available = true;
                interfaces[i].mwan3_tracking_enabled = true;
                strcpy(interfaces[i].mwan3_name, interfaces[i].name);
                // Get MWAN3 status for this interface
                get_mwan3_interface_status(ctx, &interfaces[i]);
            }
        }
    }
    blob_buf_free(&req);
}

// Check if interface is in MWAN3
static bool is_interface_in_mwan3(const char *interface_name) {
    // Execute mwan3 status command and check if interface is listed
    char command[256];
    snprintf(command, sizeof(command), "mwan3 status 2>/dev/null | grep -q '\"%s\"'", interface_name);
    int ret = system(command);
    return (ret == 0);
}

// Get MWAN3 interface status
static void get_mwan3_interface_status(struct ubus_context *ctx, network_interface_t *iface) {
    // This would parse the MWAN3 status for the specific interface
    // For now, set default values
    strcpy(iface->mwan3_status, "unknown");
    iface->mwan3_metric = 0;
}

// Get device information from network.device
static void get_device_information(struct ubus_context *ctx, network_interface_t *interfaces, int count) {
    uint32_t id;
    if (ubus_lookup_id(ctx, "network.device", &id) != 0) {
        LOGX_DEBUG_MSG("network.device not available");
        return;
    }
    
    struct blob_buf req = {0};
    blob_buf_init(&req, 0);
    
    int ret = ubus_invoke(ctx, id, "status", req.head, NULL, NULL, 1000);
    if (ret == 0) {
        // Parse device information and update interfaces
        for (int i = 0; i < count; i++) {
            update_interface_device_info(ctx, &interfaces[i]);
        }
    }
    blob_buf_free(&req);
}

// Update interface device information
static void update_interface_device_info(struct ubus_context *ctx, network_interface_t *iface) {
    // This would parse device information from network.device
    // For now, determine type based on interface name patterns
    determine_interface_type_comprehensive(iface);
}

// Comprehensive interface type determination
static void determine_interface_type_comprehensive(network_interface_t *iface) {
    if (!iface) {
        return;
    }
    
    // Check interface name patterns
    if (strncmp(iface->name, "eth", 3) == 0) {
        strncpy(iface->type, "ethernet", sizeof(iface->type) - 1);
    } else if (strncmp(iface->name, "wlan", 4) == 0) {
        strncpy(iface->type, "wifi", sizeof(iface->type) - 1);
    } else if (strncmp(iface->name, "wwan", 4) == 0 || 
               strncmp(iface->name, "qmimux", 6) == 0) {
        strncpy(iface->type, "cellular", sizeof(iface->type) - 1);
        strncpy(iface->subtype, "sim", sizeof(iface->subtype) - 1);
    } else if (strncmp(iface->name, "tun", 3) == 0 || strncmp(iface->name, "tap", 3) == 0) {
        strncpy(iface->type, "vpn", sizeof(iface->type) - 1);
        strncpy(iface->subtype, "openvpn", sizeof(iface->subtype) - 1);
        iface->is_vpn = true;
    } else if (strncmp(iface->name, "wg_", 3) == 0) {
        strncpy(iface->type, "vpn", sizeof(iface->type) - 1);
        strncpy(iface->subtype, "wireguard", sizeof(iface->subtype) - 1);
        iface->is_vpn = true;
    } else if (strncmp(iface->name, "br", 2) == 0) {
        strncpy(iface->type, "bridge", sizeof(iface->type) - 1);
    } else if (strncmp(iface->name, "vlan", 4) == 0) {
        strncpy(iface->type, "vlan", sizeof(iface->type) - 1);
    } else {
        strncpy(iface->type, "unknown", sizeof(iface->type) - 1);
    }
}

// Get cellular information
static void get_cellular_information(struct ubus_context *ctx, network_interface_t *interfaces, int count) {
    // Check for cellular interfaces
    for (int i = 0; i < count; i++) {
        if (strcmp(interfaces[i].type, "cellular") == 0) {
            get_cellular_interface_details(ctx, &interfaces[i]);
        }
    }
}

// Get cellular interface details
static void get_cellular_interface_details(struct ubus_context *ctx, network_interface_t *iface) {
    // Try to get modem information
    uint32_t id;
    if (ubus_lookup_id(ctx, "mobifd.modem0", &id) == 0) {
        struct blob_buf req = {0};
        blob_buf_init(&req, 0);
        
        int ret = ubus_invoke(ctx, id, "status", req.head, NULL, NULL, 1000);
        if (ret == 0) {
            // Parse modem information
            // For now, set default values
            strcpy(iface->modem_model, "Unknown");
            strcpy(iface->modem_id, "2-1");
            strcpy(iface->sim_id, "1");
        }
        blob_buf_free(&req);
    }
}

// Get WiFi information
static void get_wifi_information(struct ubus_context *ctx, network_interface_t *interfaces, int count) {
    uint32_t id;
    if (ubus_lookup_id(ctx, "network.wireless", &id) != 0) {
        LOGX_DEBUG_MSG("network.wireless not available");
        return;
    }
    
    struct blob_buf req = {0};
    blob_buf_init(&req, 0);
    
    int ret = ubus_invoke(ctx, id, "status", req.head, NULL, NULL, 1000);
    if (ret == 0) {
        // Parse WiFi information and update interfaces
        for (int i = 0; i < count; i++) {
            if (strcmp(interfaces[i].type, "wifi") == 0) {
                get_wifi_interface_details(ctx, &interfaces[i]);
            }
        }
    }
    blob_buf_free(&req);
}

// Get WiFi interface details
static void get_wifi_interface_details(struct ubus_context *ctx, network_interface_t *iface) {
    // This would parse WiFi details from network.wireless
    // For now, set default values
    strcpy(iface->wifi_mode, "ap");
    strcpy(iface->wifi_encryption, "psk2");
}

// Detect Starlink connections
static void detect_starlink_connections(network_interface_t *interfaces, int count) {
    for (int i = 0; i < count; i++) {
        // Check for Starlink IP range (100.64.0.0/10)
        if (strlen(interfaces[i].ip_address) > 0 && 
            is_starlink_ip_range(interfaces[i].ip_address)) {
            
            interfaces[i].is_starlink = true;
            strcpy(interfaces[i].type, "starlink");
            strcpy(interfaces[i].starlink_ip, interfaces[i].ip_address);
            
            // Try to get Starlink dish information
            get_starlink_dish_info(&interfaces[i]);
        }
    }
}

// Check if IP is in Starlink range
static bool is_starlink_ip_range(const char *ip_address) {
    if (!ip_address) return false;
    
    // Starlink uses 100.64.0.0/10 range
    struct in_addr addr;
    if (inet_aton(ip_address, &addr) == 0) return false;
    
    uint32_t ip = ntohl(addr.s_addr);
    // Check if IP is in 100.64.0.0/10 range (100.64.0.0 to 100.127.255.255)
    return (ip >= 0x64400000 && ip <= 0x647FFFFF);
}

// Get Starlink dish information
static void get_starlink_dish_info(network_interface_t *iface) {
    // Try to get dish information from Starlink API or local config
    // For now, generate a dish ID based on IP
    snprintf(iface->starlink_dish_id, sizeof(iface->starlink_dish_id), 
             "dish_%s", iface->ip_address);
    snprintf(iface->starlink_dish_name, sizeof(iface->starlink_dish_name), 
             "Starlink Dish %s", iface->ip_address);
}

// Detect VPN connections
static void detect_vpn_connections(network_interface_t *interfaces, int count) {
    for (int i = 0; i < count; i++) {
        // Check for WireGuard interfaces
        if (strstr(interfaces[i].name, "wg_") || 
            strstr(interfaces[i].type, "wireguard")) {
            interfaces[i].is_vpn = true;
            strcpy(interfaces[i].vpn_type, "wireguard");
            strcpy(interfaces[i].vpn_name, interfaces[i].name);
        }
        
        // Check for OpenVPN interfaces
        if (strstr(interfaces[i].name, "tun") || 
            strstr(interfaces[i].name, "tap")) {
            interfaces[i].is_vpn = true;
            strcpy(interfaces[i].vpn_type, "openvpn");
            strcpy(interfaces[i].vpn_name, interfaces[i].name);
        }
    }
}

// Get friendly names from UCI
static void get_friendly_names_from_uci(network_interface_t *interfaces, int count) {
    struct uci_context *ctx = uci_alloc_context();
    if (!ctx) {
        LOGX_ERROR_MSG("Failed to allocate UCI context");
        return;
    }
    
    struct uci_package *pkg = NULL;
    int ret = uci_load(ctx, "network", &pkg);
    if (ret != UCI_OK || !pkg) {
        LOGX_ERROR_MSG("Failed to load UCI network package");
        uci_free_context(ctx);
        return;
    }
    
    // Iterate through UCI network sections to get friendly names
    struct uci_element *e;
    uci_foreach_element(&pkg->sections, e) {
        struct uci_section *s = uci_to_section(e);
        const char *type = uci_lookup_option_string(ctx, s, "type");
        const char *ifname = uci_lookup_option_string(ctx, s, "ifname");
        
        if (type && strcmp(type, "interface") == 0 && ifname) {
            // Find matching interface and set friendly name
            for (int i = 0; i < count; i++) {
                if (strcmp(interfaces[i].name, ifname) == 0) {
                    strncpy(interfaces[i].friendly_name, s->e.name, 
                           sizeof(interfaces[i].friendly_name) - 1);
                    break;
                }
            }
        }
    }
    
    uci_unload(ctx, pkg);
    uci_free_context(ctx);
}

// Check if interface should be included in failover
bool should_include_in_failover(const network_interface_t *iface) {
    if (!iface) return false;
    
    // Only include interfaces that are:
    // 1. Tracked by MWAN3
    // 2. Not VPN interfaces (unless specifically configured)
    // 3. Up and available
    return iface->mwan3_tracking_enabled && 
           iface->mwan3_available && 
           iface->up && 
           (!iface->is_vpn || g_config.include_vpn_in_failover);
}

// Get comprehensive interface information for ubus
int network_discovery_get_comprehensive_interfaces(network_interface_t *interfaces, int max_count, int *actual_count) {
    if (!g_comprehensive_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    return get_comprehensive_interface_info(interfaces, actual_count);
}

// Cleanup comprehensive discovery system
void network_discovery_comprehensive_cleanup(void) {
    if (!g_comprehensive_initialized) {
        return;
    }
    
    pthread_mutex_lock(&g_comprehensive_mutex);
    g_comprehensive_initialized = false;
    pthread_mutex_unlock(&g_comprehensive_mutex);
    
    pthread_mutex_destroy(&g_comprehensive_mutex);
    
    LOGX_INFO_MSG("Comprehensive network discovery system cleaned up");
}
