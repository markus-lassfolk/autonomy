#include "../core/types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/if_link.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>

// External reference to global configuration
extern autonomy_config_t g_config;

extern struct autonomy_state g_state;

// Network discovery and management
int discover_network_interfaces(void) {
    // Real network interface discovery
    g_state.interface_count = 0;
    
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        return -1;
    }
    
    // First pass: collect all interfaces
    for (ifa = ifaddr; ifa != NULL && g_state.interface_count < MAX_INTERFACES; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        
        // Skip loopback interface
        if (strcmp(ifa->ifa_name, "lo") == 0) continue;
        
        // Check if we already have this interface
        bool interface_exists = false;
        for (int i = 0; i < g_state.interface_count; i++) {
            if (strcmp(g_state.interfaces[i].name, ifa->ifa_name) == 0) {
                interface_exists = true;
                break;
            }
        }
        
        if (!interface_exists) {
            struct network_interface *iface = &g_state.interfaces[g_state.interface_count];
            
            // Copy interface name
            strncpy(iface->name, ifa->ifa_name, sizeof(iface->name) - 1);
            iface->name[sizeof(iface->name) - 1] = '\0';
            
            // Determine interface type
            if (strncmp(ifa->ifa_name, "eth", 3) == 0 || strncmp(ifa->ifa_name, "en", 2) == 0) {
                strcpy(iface->type, "ethernet");
            } else if (strncmp(ifa->ifa_name, "wlan", 4) == 0 || strncmp(ifa->ifa_name, "wl", 2) == 0) {
                strcpy(iface->type, "wifi");
            } else if (strncmp(ifa->ifa_name, "wwan", 4) == 0 || strncmp(ifa->ifa_name, "usb", 3) == 0) {
                strcpy(iface->type, "cellular");
            } else if (strncmp(ifa->ifa_name, "ppp", 3) == 0) {
                strcpy(iface->type, "ppp");
            } else {
                strcpy(iface->type, "unknown");
            }
            
            // Check if interface is up
            iface->enabled = (ifa->ifa_flags & IFF_UP) ? 1 : 0;
            iface->last_check = time(NULL);
            
            // Initialize metrics (will be updated by real measurements)
            iface->latency = 0.0;
            iface->loss = 0.0;
            iface->signal_strength = 0;
            iface->bandwidth = 0;
            iface->health_score = 0;
            
            // Set initial status
            if (iface->enabled) {
                strcpy(iface->status, "active");
            } else {
                strcpy(iface->status, "inactive");
            }
            
            g_state.interface_count++;
        }
    }
    
    freeifaddrs(ifaddr);
    
    // Set default active interface (prefer ethernet, then wifi, then cellular)
    strcpy(g_state.active_interface, "");
    for (int i = 0; i < g_state.interface_count; i++) {
        if (g_state.interfaces[i].enabled) {
            if (strcmp(g_state.interfaces[i].type, "ethernet") == 0) {
                strcpy(g_state.active_interface, g_state.interfaces[i].name);
                break;
            }
        }
    }
    
    if (strlen(g_state.active_interface) == 0) {
        for (int i = 0; i < g_state.interface_count; i++) {
            if (g_state.interfaces[i].enabled) {
                if (strcmp(g_state.interfaces[i].type, "wifi") == 0) {
                    strcpy(g_state.active_interface, g_state.interfaces[i].name);
                    break;
                }
            }
        }
    }
    
    if (strlen(g_state.active_interface) == 0) {
        for (int i = 0; i < g_state.interface_count; i++) {
            if (g_state.interfaces[i].enabled) {
                strcpy(g_state.active_interface, g_state.interfaces[i].name);
                break;
            }
        }
    }
    
    g_state.failover_enabled = 1;
    g_state.last_network_check = time(NULL);
    g_state.network_health_score = 0.0; // Will be calculated by real measurements
    
    return 0;
}

int calculate_interface_health_score(struct network_interface *iface) {
    int score = 100; // Use configurable value
    
    // Deduct points for high latency
    if (iface->latency > 100) score -= 30;
    else if (iface->latency > 50) score -= 20;
    else if (iface->latency > 20) score -= 10;
    
    // Deduct points for packet loss
    if (iface->loss > 10) score -= 40;
    else if (iface->loss > 5) score -= 25;
    else if (iface->loss > 2) score -= 15;
    
    // Deduct points for low signal strength
    if (iface->signal_strength < 30) score -= 30;
    else if (iface->signal_strength < 50) score -= 20;
    else if (iface->signal_strength < 70) score -= 10;
    
    // Ensure score doesn't go below 0
    if (score < 0) score = 0; // Use configurable value
    
    return score;
}

int perform_network_health_check(void) {
    time_t now = time(NULL);
    
    for (int i = 0; i < g_state.interface_count; i++) {
        // Real network metrics update
        if (now - g_state.interfaces[i].last_check > 30) {
            struct network_interface *iface = &g_state.interfaces[i];
            
            // Measure real latency using ping
            char ping_cmd[256];
            snprintf(ping_cmd, sizeof(ping_cmd), 
                    "ping -c 3 -W 2 -I %s 8.8.8.8 2>/dev/null | grep 'rtt' | awk '{print $4}' | cut -d'/' -f2", 
                    iface->name);
            
            FILE *fp = popen(ping_cmd, "r");
            if (fp) {
                char buffer[32];
                if (fgets(buffer, sizeof(buffer), fp)) {
                    iface->latency = atof(buffer);
                }
                pclose(fp);
            }
            
            // Measure packet loss using ping
            snprintf(ping_cmd, sizeof(ping_cmd), 
                    "ping -c 10 -W 2 -I %s 8.8.8.8 2>/dev/null | grep 'packet loss' | awk '{print $6}' | sed 's/%//'", 
                    iface->name);
            
            fp = popen(ping_cmd, "r");
            if (fp) {
                char buffer[32];
                if (fgets(buffer, sizeof(buffer), fp)) {
                    iface->loss = atof(buffer);
                }
                pclose(fp);
            }
            
            // Get signal strength for WiFi interfaces
            if (strcmp(iface->type, "wifi") == 0) {
                char signal_cmd[256];
                snprintf(signal_cmd, sizeof(signal_cmd), 
                        "iwconfig %s 2>/dev/null | grep 'Signal level' | awk '{print $4}' | cut -d'=' -f2", 
                        iface->name);
                
                fp = popen(signal_cmd, "r");
                if (fp) {
                    char buffer[32];
                    if (fgets(buffer, sizeof(buffer), fp)) {
                        // Convert dBm to percentage (rough approximation)
                        int dbm = atoi(buffer);
                        if (dbm >= -30) {
                            iface->signal_strength = 100;
                        } else if (dbm >= -50) {
                            iface->signal_strength = 80;
                        } else if (dbm >= -70) {
                            iface->signal_strength = 60;
                        } else if (dbm >= -80) {
                            iface->signal_strength = 40;
                        } else {
                            iface->signal_strength = 20;
                        }
                    }
                    pclose(fp);
                }
            }
            
            // Get bandwidth information
            char bandwidth_cmd[256];
            snprintf(bandwidth_cmd, sizeof(bandwidth_cmd), 
                    "cat /sys/class/net/%s/speed 2>/dev/null", iface->name);
            
            fp = popen(bandwidth_cmd, "r");
            if (fp) {
                char buffer[32];
                if (fgets(buffer, sizeof(buffer), fp)) {
                    iface->bandwidth = atoi(buffer);
                }
                pclose(fp);
            }
            
            // Check interface status
            char status_cmd[256];
            snprintf(status_cmd, sizeof(status_cmd), 
                    "ip link show %s | grep -q 'state UP' && echo 'active' || echo 'inactive'", 
                    iface->name);
            
            fp = popen(status_cmd, "r");
            if (fp) {
                char buffer[32];
                if (fgets(buffer, sizeof(buffer), fp)) {
                    // Remove newline
                    buffer[strcspn(buffer, "\n")] = 0;
                    strcpy(iface->status, buffer);
                    iface->enabled = (strcmp(buffer, "active") == 0) ? 1 : 0;
                }
                pclose(fp);
            }
            
            // Recalculate health score
            iface->health_score = calculate_interface_health_score(iface);
            iface->last_check = now;
        }
    }
    
    // Calculate overall network health score
    float total_score = 0; // Use configurable value
    int active_interfaces = 0; // Use configurable value
    for (int i = 0; i < g_state.interface_count; i++) {
        if (g_state.interfaces[i].enabled) {
            total_score += g_state.interfaces[i].health_score;
            active_interfaces++;
        }
    }
    
    if (active_interfaces > 0) {
        g_state.network_health_score = total_score / active_interfaces;
    } else {
        g_state.network_health_score = 0.0;
    }
    
    g_state.last_network_check = now;
    
    return 0;
}

