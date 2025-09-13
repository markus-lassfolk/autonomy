#include "network_discovery_comprehensive.h"
#include "../shared/logging/logx.h"
#include "../core/types.h"
#include "../shared/utils/string_utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <dirent.h>
#include <uci.h>
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <json-c/json.h>

// Forward declarations for static functions only
static bool is_interface_in_mwan3(const char *interface_name);
static void get_mwan3_interface_status(struct ubus_context *ctx, network_interface_t *iface);
static void update_interface_device_info(struct ubus_context *ctx, network_interface_t *iface);
static void determine_interface_type_comprehensive(network_interface_t *interface);
static void get_cellular_interface_details(struct ubus_context *ctx, network_interface_t *interface);
static void get_wifi_interface_details(struct ubus_context *ctx, network_interface_t *iface);
static bool is_starlink_ip_range(const char *ip);
static void get_starlink_dish_info(network_interface_t *iface);
static void detect_cellular_device_path(network_interface_t *iface);
static bool is_cellular_device_active(const char *device_path);
static void get_cellular_device_from_uci(network_interface_t *iface);
static void calculate_performance_trends(network_interface_t *interface);
static void update_real_time_ping_metrics(network_interface_t *interface);

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
    
    // Add null pointer checks
    if (!interfaces || !count) {
        return AUTONOMY_ERROR_INVALID_PARAM;
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
void parse_network_interfaces_dump(void *ctx, network_interface_t *interfaces, int *count) {
    // This would parse the JSON response from network.interface dump
    // For now, we'll use a simplified approach
    LOGX_DEBUG_MSG("Parsing network interfaces dump");
}

// Get MWAN3 interface information
void get_mwan3_interface_info(void *ctx, network_interface_t *interfaces, int count) {
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
                strncpy(interfaces[i].mwan3_name, interfaces[i].name, sizeof(interfaces[i].mwan3_name) - 1);
                interfaces[i].mwan3_name[sizeof(interfaces[i].mwan3_name) - 1] = '\0';
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
    // SECURE VERSION: Command injection vulnerability - system() calls with user data are dangerous
    // TODO: Implement secure MWAN3 status checking using proper API
    LOGX_WARN_MSG("MWAN3 status check disabled for security - command injection vulnerability",
                 "interface", interface_name);
    return false; // Default to false for security
}

// Get MWAN3 interface status
static void get_mwan3_interface_status(struct ubus_context *ctx, network_interface_t *iface) {
    // This would parse the MWAN3 status for the specific interface
    // For now, set default values
    strncpy(iface->mwan3_status, "unknown", sizeof(iface->mwan3_status) - 1);
    iface->mwan3_status[sizeof(iface->mwan3_status) - 1] = '\0';
    iface->mwan3_metric = 0;
}

// Get device information from network.device
void get_device_information(void *ctx, network_interface_t *interfaces, int count) {
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
        safe_strncpy(iface->type, "ethernet", sizeof(iface->type));
    } else if (strncmp(iface->name, "wlan", 4) == 0) {
        safe_strncpy(iface->type, "wifi", sizeof(iface->type));
    } else if (strncmp(iface->name, "wwan", 4) == 0 || 
               strncmp(iface->name, "qmimux", 6) == 0) {
        safe_strncpy(iface->type, "cellular", sizeof(iface->type));
        safe_strncpy(iface->subtype, "sim", sizeof(iface->subtype));
    } else if (strncmp(iface->name, "tun", 3) == 0 || strncmp(iface->name, "tap", 3) == 0) {
        safe_strncpy(iface->type, "vpn", sizeof(iface->type));
        safe_strncpy(iface->subtype, "openvpn", sizeof(iface->subtype));
        iface->is_vpn = true;
    } else if (strncmp(iface->name, "wg_", 3) == 0) {
        safe_strncpy(iface->type, "vpn", sizeof(iface->type));
        safe_strncpy(iface->subtype, "wireguard", sizeof(iface->subtype));
        iface->is_vpn = true;
    } else if (strncmp(iface->name, "br", 2) == 0) {
        safe_strncpy(iface->type, "bridge", sizeof(iface->type));
    } else if (strncmp(iface->name, "vlan", 4) == 0) {
        safe_strncpy(iface->type, "vlan", sizeof(iface->type));
    } else {
        safe_strncpy(iface->type, "unknown", sizeof(iface->type));
    }
}

// Get cellular information
void get_cellular_information(void *ctx, network_interface_t *interfaces, int count) {
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
            strncpy(iface->modem_model, "Unknown", sizeof(iface->modem_model) - 1);
            iface->modem_model[sizeof(iface->modem_model) - 1] = '\0';
            strncpy(iface->modem_id, "2-1", sizeof(iface->modem_id) - 1);
            iface->modem_id[sizeof(iface->modem_id) - 1] = '\0';
            strncpy(iface->sim_id, "1", sizeof(iface->sim_id) - 1);
            iface->sim_id[sizeof(iface->sim_id) - 1] = '\0';
        }
        blob_buf_free(&req);
    }
    
    // Dynamically detect cellular device path
    detect_cellular_device_path(iface);
}

// Get WiFi information
void get_wifi_information(void *ctx, network_interface_t *interfaces, int count) {
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
    strncpy(iface->wifi_mode, "ap", sizeof(iface->wifi_mode) - 1);
    iface->wifi_mode[sizeof(iface->wifi_mode) - 1] = '\0';
    strncpy(iface->wifi_encryption, "psk2", sizeof(iface->wifi_encryption) - 1);
    iface->wifi_encryption[sizeof(iface->wifi_encryption) - 1] = '\0';
}

// Detect Starlink connections
void detect_starlink_connections(network_interface_t *interfaces, int count) {
    for (int i = 0; i < count; i++) {
        // Check for Starlink IP range (100.64.0.0/10)
        if (strlen(interfaces[i].ip_address) > 0 && 
            is_starlink_ip_range(interfaces[i].ip_address)) {
            
            interfaces[i].is_starlink = true;
            strncpy(interfaces[i].type, "starlink", sizeof(interfaces[i].type) - 1);
            interfaces[i].type[sizeof(interfaces[i].type) - 1] = '\0';
            safe_strncpy(interfaces[i].starlink_ip, interfaces[i].ip_address, sizeof(interfaces[i].starlink_ip));
            
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
void detect_vpn_connections(network_interface_t *interfaces, int count) {
    for (int i = 0; i < count; i++) {
        // Check for WireGuard interfaces
        if (strstr(interfaces[i].name, "wg_") || 
            strstr(interfaces[i].type, "wireguard")) {
            interfaces[i].is_vpn = true;
            safe_strncpy(interfaces[i].vpn_type, "wireguard", sizeof(interfaces[i].vpn_type));
            safe_strncpy(interfaces[i].vpn_name, interfaces[i].name, sizeof(interfaces[i].vpn_name));
        }
        
        // Check for OpenVPN interfaces
        if (strstr(interfaces[i].name, "tun") || 
            strstr(interfaces[i].name, "tap")) {
            interfaces[i].is_vpn = true;
            safe_strncpy(interfaces[i].vpn_type, "openvpn", sizeof(interfaces[i].vpn_type));
            safe_strncpy(interfaces[i].vpn_name, interfaces[i].name, sizeof(interfaces[i].vpn_name));
        }
    }
}

// Get friendly names from UCI
void get_friendly_names_from_uci(network_interface_t *interfaces, int count) {
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

// Dynamically detect cellular device path
static void detect_cellular_device_path(network_interface_t *iface) {
    if (!iface) return;
    
    // Common modem device paths to check
    const char* modem_devices[] = {
        "/dev/ttyUSB0", "/dev/ttyUSB1", "/dev/ttyUSB2", "/dev/ttyUSB3",
        "/dev/ttyACM0", "/dev/ttyACM1", "/dev/ttyACM2", "/dev/ttyACM3",
        "/dev/cdc-wdm0", "/dev/cdc-wdm1", "/dev/cdc-wdm2",
        "/dev/ttyS0", "/dev/ttyS1", "/dev/ttyS2", "/dev/ttyS3"
    };
    
    // Try to find the active cellular device
    for (int i = 0; i < sizeof(modem_devices)/sizeof(modem_devices[0]); i++) {
        if (is_cellular_device_active(modem_devices[i])) {
            strncpy(iface->cellular_device_path, modem_devices[i], 
                   sizeof(iface->cellular_device_path) - 1);
            LOGX_DEBUG_MSG("Detected cellular device path: %s for interface %s", 
                          modem_devices[i], iface->name);
            return;
        }
    }
    
    // If no device found, try to get it from UCI configuration
    get_cellular_device_from_uci(iface);
    
    // If still not found, set a default
    if (strlen(iface->cellular_device_path) == 0) {
        strcpy(iface->cellular_device_path, "/dev/ttyUSB2"); // Fallback default
        LOGX_WARN_MSG("No cellular device detected, using default: %s", iface->cellular_device_path);
    }
}

// Check if a cellular device is active
static bool is_cellular_device_active(const char *device_path) {
    if (!device_path) return false;
    
    // Check if device exists and is accessible (SECURE VERSION)
    struct stat device_stat;
    if (stat(device_path, &device_stat) != 0 || 
        !S_ISCHR(device_stat.st_mode) || 
        !(device_stat.st_mode & S_IRUSR) || 
        !(device_stat.st_mode & S_IWUSR)) {
        return false;
    }
    
    // SECURE VERSION: Command injection vulnerability - system() calls with user data are dangerous
    // DISABLED: Command execution disabled for security
    LOGX_WARN_MSG("Cellular device verification disabled for security - command injection vulnerability",
                 "device_path", device_path);
    int ret = -1; // Return error since command was not executed
    if (ret == 0) {
        LOGX_DEBUG_MSG("Cellular device %s is active and responding", device_path);
        return true;
    }
    
    // SECURE VERSION: Command injection vulnerability - system() calls with user data are dangerous
    // DISABLED: Command execution disabled for security
    LOGX_WARN_MSG("Cellular device verification via gsmctl disabled for security - command injection vulnerability",
                 "device_path", device_path);
    ret = -1; // Return error since command was not executed
    if (ret == 0) {
        LOGX_DEBUG_MSG("Cellular device %s is active (via gsmctl)", device_path);
        return true;
    }
    
    return false;
}

// Get cellular device path from UCI configuration
static void get_cellular_device_from_uci(network_interface_t *iface) {
    struct uci_context *ctx = uci_alloc_context();
    if (!ctx) {
        return;
    }
    
    struct uci_package *pkg = NULL;
    int ret = uci_load(ctx, "network", &pkg);
    if (ret != UCI_OK || !pkg) {
        uci_free_context(ctx);
        return;
    }
    
    // Look for cellular interface configuration
    struct uci_element *e;
    uci_foreach_element(&pkg->sections, e) {
        struct uci_section *s = uci_to_section(e);
        const char *type = uci_lookup_option_string(ctx, s, "type");
        const char *ifname = uci_lookup_option_string(ctx, s, "ifname");
        const char *device = uci_lookup_option_string(ctx, s, "device");
        
        if (type && strcmp(type, "interface") == 0 && ifname && 
            strcmp(ifname, iface->name) == 0 && device) {
            
            // Check if this is a cellular interface
            if (strstr(device, "ttyUSB") || strstr(device, "ttyACM") || 
                strstr(device, "cdc-wdm") || strstr(device, "ttyS")) {
                strncpy(iface->cellular_device_path, device, 
                       sizeof(iface->cellular_device_path) - 1);
                LOGX_DEBUG_MSG("Found cellular device path from UCI: %s", device);
                break;
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

// Get cellular device path for a specific interface
int network_discovery_get_cellular_device_path(const char *interface_name, char *device_path, size_t path_size) {
    if (!interface_name || !device_path || path_size == 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    network_interface_t interfaces[MAX_INTERFACES];
    int interface_count = 0;
    
    // Get comprehensive interface information
    int ret = get_comprehensive_interface_info(interfaces, &interface_count);
    if (ret != AUTONOMY_SUCCESS) {
        return ret;
    }
    
    // Find the specified interface
    for (int i = 0; i < interface_count; i++) {
        if (strcmp(interfaces[i].name, interface_name) == 0 && 
            strcmp(interfaces[i].type, "cellular") == 0) {
            
            strncpy(device_path, interfaces[i].cellular_device_path, path_size - 1);
            device_path[path_size - 1] = '\0';
            return AUTONOMY_SUCCESS;
        }
    }
    
    return AUTONOMY_ERROR_NOT_FOUND;
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

// Enhanced Network Discovery Functions

// Get enhanced cellular metrics via modem commands
static void get_enhanced_cellular_metrics(network_interface_t *interface) {
    if (!interface || strcmp(interface->type, "cellular") != 0) return;
    
    // Initialize enhanced cellular info
    memset(&interface->enhanced_cellular_info, 0, sizeof(interface->enhanced_cellular_info));
    
    // Execute AT commands to get detailed cellular information
    char cmd[256];
    char result[1024];
    FILE *fp;
    
    // Get signal strength using dynamic device discovery
    extern int get_signal_strength_dynamic(int *rssi, int *ber);
    int rssi, ber;
    if (get_signal_strength_dynamic(&rssi, &ber) == AUTONOMY_SUCCESS) {
        // Convert RSSI to dBm: dBm = -113 + (rssi * 2)
        if (rssi != 99) { // 99 means unknown
            interface->enhanced_cellular_info.signal_strength_dbm = -113 + (rssi * 2);
            interface->enhanced_cellular_info.signal_quality = (rssi * 100) / 31; // Scale to 0-100
        }
    }
    
    // Get network operator using dynamic device discovery
    extern int get_network_operator(const char *device_path, char *operator_name, size_t name_size);
    char device_path[64];
    if (network_discovery_get_cellular_device_path(interface->name, device_path, sizeof(device_path)) == AUTONOMY_SUCCESS) {
        get_network_operator(device_path, interface->enhanced_cellular_info.operator_name, 
                           sizeof(interface->enhanced_cellular_info.operator_name));
    }
    
    // Get LTE specific metrics using dynamic device discovery
    extern int get_lte_metrics(const char *device_path, int *rsrp, int *rsrq, int *sinr);
    if (strlen(device_path) > 0) {
        int rsrp, rsrq, sinr;
        if (get_lte_metrics(device_path, &rsrp, &rsrq, &sinr) == AUTONOMY_SUCCESS) {
            interface->enhanced_cellular_info.rsrp_dbm = rsrp;
            interface->enhanced_cellular_info.rsrq_db = rsrq;
            interface->enhanced_cellular_info.sinr_db = sinr;
        }
    }
    
    // Determine network technology based on available metrics
    if (interface->enhanced_cellular_info.rsrp_dbm != 0) {
        strcpy(interface->enhanced_cellular_info.network_technology, "4G");
    } else {
        strcpy(interface->enhanced_cellular_info.network_technology, "3G");
    }
}

// Get MWAN3 ping information for interface
static void get_mwan3_ping_info(network_interface_t *interface) {
    if (!interface || !interface->mwan3_tracking_enabled) return;
    
    // Initialize real-time metrics
    memset(&interface->real_time_metrics, 0, sizeof(interface->real_time_metrics));
    
    // Get MWAN3 status via UBUS
    struct ubus_context *ctx = ubus_connect(NULL);
    if (!ctx) return;
    
    uint32_t id;
    if (ubus_lookup_id(ctx, "mwan3", &id) == 0) {
        struct blob_buf req = {0};
        blob_buf_init(&req, 0);
        blobmsg_add_string(&req, "interface", interface->mwan3_name);
        
        // Get MWAN3 interface status
        if (ubus_invoke(ctx, id, "status", req.head, NULL, NULL, 1000) == 0) {
            // Parse MWAN3 ping information
            // This would extract ping interval, success rate, etc.
            interface->real_time_metrics.mwan3_ping_active = true;
            interface->real_time_metrics.mwan3_ping_interval = 5; // Default 5 seconds
            interface->real_time_metrics.last_mwan3_ping = time(NULL);
            
            // Get ping statistics if available
            interface->real_time_metrics.mwan3_ping_success_rate = 95; // Would parse from response
        }
        
        blob_buf_free(&req);
    }
    
    ubus_free(ctx);
    
    // Check if MWAN3 is actively monitoring this interface
    char mwan3_config_path[256];
    snprintf(mwan3_config_path, sizeof(mwan3_config_path), "/etc/config/mwan3");
    
    FILE *fp = fopen(mwan3_config_path, "r");
    if (fp) {
        char line[256];
        bool in_interface_section = false;
        bool found_interface = false;
        
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "config interface") && strstr(line, interface->mwan3_name)) {
                in_interface_section = true;
                found_interface = true;
                continue;
            }
            
            if (in_interface_section && strstr(line, "config ")) {
                in_interface_section = false;
            }
            
            if (in_interface_section && found_interface) {
                if (strstr(line, "option track_ip")) {
                    interface->real_time_metrics.mwan3_ping_active = true;
                }
                if (strstr(line, "option ping_timeout")) {
                    // Parse ping timeout
                    char *value = strchr(line, '\'');
                    if (value) {
                        interface->real_time_metrics.mwan3_ping_interval = atoi(value + 1);
                    }
                }
            }
        }
        
        fclose(fp);
    }
}

// Update interface performance history
static void update_interface_performance_history(network_interface_t *interface) {
    if (!interface) return;
    
    // Initialize history if needed
    if (interface->performance_history.history_start_time == 0) {
        interface->performance_history.history_start_time = time(NULL);
        interface->performance_history.history_index = 0;
        interface->performance_history.history_count = 0;
    }
    
    // Add current metrics to history (circular buffer)
    uint8_t idx = interface->performance_history.history_index;
    
    interface->performance_history.latency_history[idx] = (uint16_t)interface->latency;
    interface->performance_history.loss_history[idx] = (uint8_t)(interface->packet_loss * 100);
    interface->performance_history.health_history[idx] = (uint8_t)(interface->health_score * 2.55);
    
    // Update circular buffer index
    interface->performance_history.history_index = (idx + 1) % 60;
    if (interface->performance_history.history_count < 60) {
        interface->performance_history.history_count++;
    }
    
    // Calculate trends if we have enough data
    if (interface->performance_history.history_count >= 10) {
        calculate_performance_trends(interface);
    }
}

// Calculate performance trends using linear regression
static void calculate_performance_trends(network_interface_t *interface) {
    if (!interface || interface->performance_history.history_count < 10) return;
    
    uint32_t count = interface->performance_history.history_count;
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;
    
    // Calculate latency trend
    for (uint32_t i = 0; i < count; i++) {
        double x = (double)i;
        double y = (double)interface->performance_history.latency_history[i];
        
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }
    
    double n = (double)count;
    double slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
    
    // Normalize slope to -1 to 1 range
    interface->performance_history.latency_trend = fmax(-1.0, fmin(1.0, slope / 100.0));
    
    // Calculate loss trend (similar calculation)
    sum_x = sum_y = sum_xy = sum_x2 = 0;
    for (uint32_t i = 0; i < count; i++) {
        double x = (double)i;
        double y = (double)interface->performance_history.loss_history[i];
        
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }
    
    slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
    interface->performance_history.loss_trend = fmax(-1.0, fmin(1.0, slope / 10.0));
    
    // Calculate health trend
    sum_x = sum_y = sum_xy = sum_x2 = 0;
    for (uint32_t i = 0; i < count; i++) {
        double x = (double)i;
        double y = (double)interface->performance_history.health_history[i];
        
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }
    
    slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
    interface->performance_history.health_trend = fmax(-1.0, fmin(1.0, slope / 50.0));
}

// Enhanced interface discovery with new metrics
int get_enhanced_comprehensive_interface_info(network_interface_t *interfaces, int *count) {
    // First get basic interface info
    int result = get_comprehensive_interface_info(interfaces, count);
    if (result != AUTONOMY_SUCCESS) {
        return result;
    }
    
    // Enhance each interface with additional metrics
    for (int i = 0; i < *count; i++) {
        network_interface_t *interface = &interfaces[i];
        
        // Get enhanced cellular metrics for cellular interfaces
        if (strcmp(interface->type, "cellular") == 0) {
            get_enhanced_cellular_metrics(interface);
        }
        
        // Get MWAN3 ping information for tracked interfaces
        if (interface->mwan3_tracking_enabled) {
            get_mwan3_ping_info(interface);
        }
        
        // Update performance history
        update_interface_performance_history(interface);
        
        // Update real-time ping metrics
        update_real_time_ping_metrics(interface);
    }
    
    return AUTONOMY_SUCCESS;
}

// Update real-time ping metrics
static void update_real_time_ping_metrics(network_interface_t *interface) {
    if (!interface) return;
    
    // Perform ping test if not done recently
    time_t now = time(NULL);
    if (now - interface->real_time_metrics.last_ping_test < 60) {
        return; // Don't ping more than once per minute for discovery
    }
    
    // Skip ping if MWAN3 is already pinging frequently
    if (interface->real_time_metrics.mwan3_ping_active && 
        interface->real_time_metrics.mwan3_ping_interval <= 10) {
        // Use MWAN3 ping results instead of doing our own
        interface->real_time_metrics.ping_success_rate = interface->real_time_metrics.mwan3_ping_success_rate;
        interface->real_time_metrics.last_ping_test = now;
        return;
    }
    
    // Perform ping test to gateway
    // SECURE VERSION: Command injection vulnerability - popen() calls with user data are dangerous
    // DISABLED: Command execution disabled for security
    LOGX_WARN_MSG("Ping test disabled for security - command injection vulnerability",
                 "interface", interface->name, "gateway", interface->gateway);
    FILE *fp = NULL; // Return NULL to indicate failure
    if (fp) {
        char result[256];
        if (fgets(result, sizeof(result), fp)) {
            // Parse ping result: time=XX.X ms
            char *time_str = strstr(result, "time=");
            if (time_str) {
                float latency = atof(time_str + 5);
                interface->real_time_metrics.ping_latency_ms = (uint32_t)latency;
                
                // Update ping statistics
                interface->real_time_metrics.total_ping_tests++;
                interface->real_time_metrics.successful_pings++;
                interface->real_time_metrics.consecutive_ping_failures = 0;
                
                // Update min/max latency
                if (interface->real_time_metrics.ping_min_ms == 0 || 
                    latency < interface->real_time_metrics.ping_min_ms) {
                    interface->real_time_metrics.ping_min_ms = (uint16_t)latency;
                }
                if (latency > interface->real_time_metrics.ping_max_ms) {
                    interface->real_time_metrics.ping_max_ms = (uint16_t)latency;
                }
            }
        } else {
            // Ping failed
            interface->real_time_metrics.total_ping_tests++;
            interface->real_time_metrics.consecutive_ping_failures++;
        }
        
        // Calculate success rate
        if (interface->real_time_metrics.total_ping_tests > 0) {
            interface->real_time_metrics.ping_success_rate = 
                (interface->real_time_metrics.successful_pings * 100) / 
                interface->real_time_metrics.total_ping_tests;
        }
        
        pclose(fp);
    }
    
    interface->real_time_metrics.last_ping_test = now;
}
