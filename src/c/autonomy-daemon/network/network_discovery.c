#include "network_discovery.h"
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

// External reference to global configuration
extern autonomy_config_t g_config;

// Network discovery configuration - now uses UCI config values
// Configuration values are loaded from g_config (UCI system)
#define LOCAL_LOCAL_MAX_INTERFACES 32            // Maximum interfaces to track
static const char* INTERFACE_TYPES[] = {
    "ethernet", "wifi", "cellular", "vpn", "bridge", "vlan", "tunnel"
};

// Global discovery state
static network_discovery_t g_discovery = {0};
static pthread_mutex_t g_discovery_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_discovery_initialized = false;
static pthread_t g_discovery_thread = 0;
static bool g_discovery_thread_running = false;

// Initialize network discovery system
int network_discovery_init(void) {
    if (g_discovery_initialized) {
        LOGX_WARN_MSG("Network discovery already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_discovery_mutex);
    
    // Initialize discovery state
    memset(&g_discovery, 0, sizeof(network_discovery_t));
    g_discovery.enabled = true;
    g_discovery.discovery_interval = g_config.network_check_interval;
    g_discovery.interface_timeout = 300; // Use configurable timeout
    g_discovery.max_interfaces = LOCAL_MAX_INTERFACES;
    g_discovery.last_discovery = 0;
    g_discovery.total_discoveries = 0;
    g_discovery.interface_count = 0;
    
    g_discovery_initialized = true;
    pthread_mutex_unlock(&g_discovery_mutex);
    
    LOGX_INFO_MSG("Network discovery system initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Start network discovery monitoring thread
int network_discovery_start_monitoring(void) {
    if (!g_discovery_initialized) {
        LOGX_ERROR_MSG("Network discovery not initialized");
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    if (g_discovery_thread_running) {
        LOGX_WARN_MSG("Network discovery monitoring already running");
        return AUTONOMY_SUCCESS;
    }
    
    // Create monitoring thread
    int ret = pthread_create(&g_discovery_thread, NULL, discovery_monitor_thread_func, NULL);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to create network discovery monitoring thread");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    g_discovery_thread_running = true;
    LOGX_INFO_MSG("Network discovery monitoring started");
    
    return AUTONOMY_SUCCESS;
}

// Stop network discovery monitoring
void network_discovery_stop_monitoring(void) {
    if (!g_discovery_thread_running) {
        return;
    }
    
    g_discovery_thread_running = false;
    
    if (g_discovery_thread != 0) {
        pthread_join(g_discovery_thread, NULL);
        g_discovery_thread = 0;
    }
    
    LOGX_INFO_MSG("Network discovery monitoring stopped");
}

// Network discovery monitoring thread
static void* discovery_monitor_thread_func(void *arg) {
    (void)arg;
    
    LOGX_INFO_MSG("Network discovery monitoring thread started");
    
    while (g_discovery_thread_running) {
        // Perform network discovery
        network_discovery_scan_interfaces();
        
        // Sleep for discovery interval
        for (int i = 0; i < g_discovery.discovery_interval && g_discovery_thread_running; i++) {
            sleep(1);
        }
    }
    
    LOGX_INFO_MSG("Network discovery monitoring thread stopped");
    return NULL;
}

// Scan for available network interfaces
int network_discovery_scan_interfaces(void) {
    if (!g_discovery_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_discovery_mutex);
    
    time_t now = time(NULL);
    
    // Check if it's time to discover
    if (g_discovery.last_discovery > 0 && 
        (now - g_discovery.last_discovery) < g_discovery.discovery_interval) {
        pthread_mutex_unlock(&g_discovery_mutex);
        return AUTONOMY_SUCCESS;
    }
    
    LOGX_DEBUG_MSG("Starting network interface discovery");
    
    // Discover system interfaces
    discover_system_interfaces();
    
    // Discover UCI network interfaces
    discover_uci_interfaces();
    
    // Clean up stale interfaces
    cleanup_stale_interfaces(now);
    
    g_discovery.last_discovery = now;
    g_discovery.total_discoveries++;
    
    pthread_mutex_unlock(&g_discovery_mutex);
    
    LOGX_DEBUG_MSG("Network interface discovery completed, found %d interfaces", g_discovery.interface_count);
    return AUTONOMY_SUCCESS;
}

// Discover system network interfaces
static void discover_system_interfaces(void) {
    struct if_nameindex *if_ni, *i;
    
    if_ni = if_nameindex();
    if (if_ni == NULL) {
        LOGX_ERROR_MSG("Failed to get interface names");
        return;
    }
    
    for (i = if_ni; i->if_index != 0 || i->if_name != NULL; i++) {
        if (g_discovery.interface_count >= g_discovery.max_interfaces) {
            LOGX_WARN_MSG("Maximum interface count reached, skipping %s", i->if_name);
            break;
        }
        
        // Check if interface already exists
        bool exists = false;
        for (int j = 0; j < g_discovery.interface_count; j++) {
            if (strcmp(g_discovery.interfaces[j].name, i->if_name) == 0) {
                exists = true;
                // Update last seen time
                g_discovery.interfaces[j].last_seen = time(NULL);
                break;
            }
        }
        
        if (!exists) {
            // Add new interface
            network_interface_t *iface = &g_discovery.interfaces[g_discovery.interface_count];
            memset(iface, 0, sizeof(network_interface_t));
            
            strncpy(iface->name, i->if_name, sizeof(iface->name) - 1);
            iface->index = i->if_index;
            iface->last_seen = time(NULL);
            iface->discovered = true;
            
            // Get interface details
            get_interface_details(iface);
            
            g_discovery.interface_count++;
            
            LOGX_DEBUG_MSG("Discovered interface: %s (index: %d)", iface->name, iface->index);
        }
    }
    
    if_freenameindex(if_ni);
}

// Discover UCI network interfaces using real UCI integration
static void discover_uci_interfaces(void) {
    // Use UCI to discover configured network interfaces
    struct uci_context *ctx = uci_alloc_context();
    if (!ctx) {
        LOGX_ERROR_MSG("Failed to allocate UCI context for network discovery");
        return;
    }
    
    struct uci_package *pkg = NULL;
    int ret = uci_load(ctx, "network", &pkg);
    if (ret != UCI_OK || !pkg) {
        LOGX_ERROR_MSG("Failed to load UCI network package");
        uci_free_context(ctx);
        return;
    }
    
    // Iterate through UCI network sections
    struct uci_element *e;
    uci_foreach_element(&pkg->sections, e) {
        if (g_discovery.interface_count >= g_discovery.max_interfaces) {
            break;
        }
        
        struct uci_section *s = uci_to_section(e);
        const char *type = uci_lookup_option_string(ctx, s, "type");
        const char *ifname = uci_lookup_option_string(ctx, s, "ifname");
        const char *device = uci_lookup_option_string(ctx, s, "device");
        
        // Only process interface sections
        if (type && strcmp(type, "interface") == 0 && ifname) {
            // Check if interface already exists
            bool exists = false;
            for (int j = 0; j < g_discovery.interface_count; j++) {
                if (strcmp(g_discovery.interfaces[j].name, ifname) == 0) {
                    exists = true;
                break;
            }
        }
        
            if (!exists) {
                // Check if interface actually exists in system
                if (interface_exists_in_system(ifname)) {
                    network_interface_t *iface = &g_discovery.interfaces[g_discovery.interface_count];
                    memset(iface, 0, sizeof(network_interface_t));
                    
                    strncpy(iface->name, ifname, sizeof(iface->name) - 1);
                    iface->last_seen = time(NULL);
                    iface->discovered = true;
                    
                    // Get UCI configuration details
                    const char *proto = uci_lookup_option_string(ctx, s, "proto");
                    const char *ipaddr = uci_lookup_option_string(ctx, s, "ipaddr");
                    const char *netmask = uci_lookup_option_string(ctx, s, "netmask");
                    const char *gateway = uci_lookup_option_string(ctx, s, "gateway");
                    
                    if (proto) {
                        strncpy(iface->protocol, proto, sizeof(iface->protocol) - 1);
                    }
                    if (ipaddr) {
                        strncpy(iface->ip_address, ipaddr, sizeof(iface->ip_address) - 1);
                    }
                    if (netmask) {
                        strncpy(iface->netmask, netmask, sizeof(iface->netmask) - 1);
                    }
                    if (gateway) {
                        strncpy(iface->gateway, gateway, sizeof(iface->gateway) - 1);
                    }
                    if (device) {
                        strncpy(iface->device, device, sizeof(iface->device) - 1);
                    }
                    
                    // Get additional interface details
                    get_interface_details(iface);
                    
                    g_discovery.interface_count++;
                    
                    LOGX_DEBUG_MSG("Discovered UCI interface",
                                  "name", iface->name,
                                  "protocol", iface->protocol,
                                  "ip", iface->ip_address,
                                  "device", iface->device);
                }
            }
        }
    }
    
    uci_unload(ctx, pkg);
    uci_free_context(ctx);
    
    // Fallback: Check for common interfaces if UCI discovery found none
    if (g_discovery.interface_count == 0) {
        LOGX_WARN_MSG("No interfaces found via UCI, using fallback discovery");
        
        const char* common_interfaces[] = {
            "eth0", "eth1", "wlan0", "wlan1", "wwan0", "wwan1", "tun0", "vpn0"
        };
        
        for (int i = 0; i < sizeof(common_interfaces) / sizeof(common_interfaces[0]); i++) {
            if (g_discovery.interface_count >= g_discovery.max_interfaces) {
                break;
            }
            
            // Check if interface already exists
            bool exists = false;
            for (int j = 0; j < g_discovery.interface_count; j++) {
                if (strcmp(g_discovery.interfaces[j].name, common_interfaces[i]) == 0) {
                    exists = true;
                    break;
                }
            }
            
            if (!exists) {
                // Check if interface actually exists in system
                if (interface_exists_in_system(common_interfaces[i])) {
                    network_interface_t *iface = &g_discovery.interfaces[g_discovery.interface_count];
                    memset(iface, 0, sizeof(network_interface_t));
                    
                    strncpy(iface->name, common_interfaces[i], sizeof(iface->name) - 1);
                    iface->last_seen = time(NULL);
                    iface->discovered = true;
                    
                    // Get interface details
                    get_interface_details(iface);
                    
                    g_discovery.interface_count++;
                    
                    LOGX_DEBUG_MSG("Discovered fallback interface: %s", iface->name);
                }
            }
        }
    }
}

// Check if interface exists in system
static bool interface_exists_in_system(const char *interface_name) {
    if (!interface_name) {
        return false;
    }
    
    // Try to get interface flags
    struct ifreq ifr;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return false;
    }
    
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);
    
    if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
        close(sock);
        return true;
    }
    
    close(sock);
    return false;
}

// Get detailed interface information
static void get_interface_details(network_interface_t *iface) {
    if (!iface) {
        return;
    }
    
    // Get interface flags and status
    struct ifreq ifr;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return;
    }
    
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface->name, IFNAMSIZ - 1);
    
    // Get interface flags
    if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
        iface->up = (ifr.ifr_flags & IFF_UP) != 0;
        iface->enabled = (ifr.ifr_flags & IFF_UP) != 0;
    }
    
    // Get interface address
    if (ioctl(sock, SIOCGIFADDR, &ifr) == 0) {
        struct sockaddr_in *addr = (struct sockaddr_in*)&ifr.ifr_addr;
        inet_ntop(AF_INET, &addr->sin_addr, iface->ip_address, sizeof(iface->ip_address));
    }
    
    // Get interface netmask
    if (ioctl(sock, SIOCGIFNETMASK, &ifr) == 0) {
        struct sockaddr_in *addr = (struct sockaddr_in*)&ifr.ifr_netmask;
        inet_ntop(AF_INET, &addr->sin_addr, iface->netmask, sizeof(iface->netmask));
    }
    
    // Get interface MAC address
    if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
        unsigned char *mac = (unsigned char*)ifr.ifr_hwaddr.sa_data;
        snprintf(iface->mac_address, sizeof(iface->mac_address),
                "%02x:%02x:%02x:%02x:%02x:%02x",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    
    // Get interface MTU
    if (ioctl(sock, SIOCGIFMTU, &ifr) == 0) {
        iface->mtu = ifr.ifr_mtu;
    }
    
    close(sock);
    
    // Determine interface type
    determine_interface_type(iface);
    
    // Get interface statistics
    get_interface_statistics(iface);
}

// Determine interface type based on name and characteristics
static void determine_interface_type(network_interface_t *iface) {
    if (!iface) {
        return;
    }
    
    // Check interface name patterns
    if (strncmp(iface->name, "eth", 3) == 0) {
        strncpy(iface->type, "ethernet", sizeof(iface->type) - 1);
    } else if (strncmp(iface->name, "wlan", 4) == 0) {
        strncpy(iface->type, "wifi", sizeof(iface->type) - 1);
    } else if (strncmp(iface->name, "wwan", 4) == 0) {
        strncpy(iface->type, "cellular", sizeof(iface->type) - 1);
    } else if (strncmp(iface->name, "tun", 3) == 0 || strncmp(iface->name, "tap", 3) == 0) {
        strncpy(iface->type, "vpn", sizeof(iface->type) - 1);
    } else if (strncmp(iface->name, "br", 2) == 0) {
        strncpy(iface->type, "bridge", sizeof(iface->type) - 1);
    } else if (strncmp(iface->name, "vlan", 4) == 0) {
        strncpy(iface->type, "vlan", sizeof(iface->type) - 1);
    } else {
        strncpy(iface->type, "unknown", sizeof(iface->type) - 1);
    }
}

// Get interface statistics
static void get_interface_statistics(network_interface_t *iface) {
    if (!iface) {
        return;
    }
    
    // Read interface statistics from /proc/net/dev
    char path[256];
    snprintf(path, sizeof(path), "/proc/net/dev");
    
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return;
    }
    
    char line[512];
    // Skip header lines
    fgets(line, sizeof(line), fp);
    fgets(line, sizeof(line), fp);
    
    while (fgets(line, sizeof(line), fp)) {
        char ifname[32];
        unsigned long rx_bytes, tx_bytes, rx_packets, tx_packets;
        unsigned long rx_errors, tx_errors, rx_dropped, tx_dropped;
        
        // Parse line: interface rx_bytes rx_packets rx_errors rx_dropped tx_bytes tx_packets tx_errors tx_dropped ...
        if (sscanf(line, "%31s %lu %lu %lu %lu %lu %lu %lu %lu",
                   ifname, &rx_bytes, &rx_packets, &rx_errors, &rx_dropped,
                   &tx_bytes, &tx_packets, &tx_errors, &tx_dropped) >= 8) {
            
            // Remove colon from interface name
            char *colon = strchr(ifname, ':');
            if (colon) *colon = '\0';
            
            if (strcmp(ifname, iface->name) == 0) {
                iface->rx_bytes = rx_bytes;
                iface->tx_bytes = tx_bytes;
                iface->rx_packets = rx_packets;
                iface->tx_packets = tx_packets;
                iface->rx_errors = rx_errors;
                iface->tx_errors = tx_errors;
                iface->rx_dropped = rx_dropped;
                iface->tx_dropped = tx_dropped;
                break;
            }
        }
    }
    
    fclose(fp);
}

// Clean up stale interfaces
static void cleanup_stale_interfaces(time_t now) {
    for (int i = 0; i < g_discovery.interface_count; i++) {
        if (g_discovery.interfaces[i].last_seen > 0 &&
            (now - g_discovery.interfaces[i].last_seen) > g_discovery.interface_timeout) {
            
            LOGX_DEBUG_MSG("Removing stale interface: %s", g_discovery.interfaces[i].name);
            
            // Remove interface by shifting remaining interfaces
            for (int j = i; j < g_discovery.interface_count - 1; j++) {
                memcpy(&g_discovery.interfaces[j], &g_discovery.interfaces[j + 1], sizeof(network_interface_t));
            }
            g_discovery.interface_count--;
            i--; // Recheck this index
        }
    }
}

// Get discovered interfaces
int network_discovery_get_interfaces(network_interface_t *interfaces, int max_count, int *actual_count) {
    if (!g_discovery_initialized || !interfaces || !actual_count) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_discovery_mutex);
    
    *actual_count = 0;
    int count = (g_discovery.interface_count < max_count) ? g_discovery.interface_count : max_count;
    
    for (int i = 0; i < count; i++) {
        memcpy(&interfaces[i], &g_discovery.interfaces[i], sizeof(network_interface_t));
        (*actual_count)++;
    }
    
    pthread_mutex_unlock(&g_discovery_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get interface by name
int network_discovery_get_interface(const char *interface_name, network_interface_t *interface) {
    if (!g_discovery_initialized || !interface_name || !interface) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_discovery_mutex);
    
    for (int i = 0; i < g_discovery.interface_count; i++) {
        if (strcmp(g_discovery.interfaces[i].name, interface_name) == 0) {
            memcpy(interface, &g_discovery.interfaces[i], sizeof(network_interface_t));
            pthread_mutex_unlock(&g_discovery_mutex);
            return AUTONOMY_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&g_discovery_mutex);
    return AUTONOMY_ERROR_NOT_FOUND;
}

// Get discovery status
int network_discovery_get_status(network_discovery_status_t *status) {
    if (!g_discovery_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_discovery_mutex);
    
    status->enabled = g_discovery.enabled;
    status->discovery_interval = g_discovery.discovery_interval;
    status->interface_timeout = g_discovery.interface_timeout;
    status->max_interfaces = g_discovery.max_interfaces;
    status->last_discovery = g_discovery.last_discovery;
    status->total_discoveries = g_discovery.total_discoveries;
    status->interface_count = g_discovery.interface_count;
    
    pthread_mutex_unlock(&g_discovery_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set discovery configuration
int network_discovery_set_config(const network_discovery_config_t *config) {
    if (!g_discovery_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_discovery_mutex);
    
    if (config->discovery_interval > 0) {
        g_discovery.discovery_interval = config->discovery_interval;
    }
    
    if (config->interface_timeout > 0) {
        g_discovery.interface_timeout = config->interface_timeout;
    }
    
    if (config->max_interfaces > 0) {
        g_discovery.max_interfaces = config->max_interfaces;
    }
    
    g_discovery.enabled = config->enabled;
    
    pthread_mutex_unlock(&g_discovery_mutex);
    
    LOGX_INFO_MSG("Network discovery configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable discovery system
int network_discovery_set_enabled(bool enabled) {
    if (!g_discovery_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_discovery_mutex);
    g_discovery.enabled = enabled;
    pthread_mutex_unlock(&g_discovery_mutex);
    
    LOGX_INFO_MSG("Network discovery system %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Force immediate discovery
int network_discovery_force_scan(void) {
    if (!g_discovery_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    LOGX_INFO_MSG("Forcing immediate network discovery");
    return network_discovery_scan_interfaces();
}

// Cleanup discovery system
void network_discovery_cleanup(void) {
    if (!g_discovery_initialized) {
        return;
    }
    
    // Stop monitoring thread
    network_discovery_stop_monitoring();
    
    pthread_mutex_lock(&g_discovery_mutex);
    g_discovery_initialized = false;
    pthread_mutex_unlock(&g_discovery_mutex);
    
    pthread_mutex_destroy(&g_discovery_mutex);
    
    LOGX_INFO_MSG("Network discovery system cleaned up");
}
