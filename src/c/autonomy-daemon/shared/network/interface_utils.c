#include "interface_utils.h"

// NOLINTBEGIN(cert-msc50-cpp,cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
// NOLINTBEGIN(cert-msc51-cpp) - sprintf usage is safe with format validation
#include "../logging/logx.h"
#include "../utils/string_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Global error state
static char g_interface_error[256] = {0};
static bool g_initialized = false;

// Set error message
static void set_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(g_interface_error, sizeof(g_interface_error), format, args);
    va_end(args);
}

// Initialize interface utilities
int interface_utils_init(void) {
    if (g_initialized) {
        LOGX_WARN_MSG("Interface utilities already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    g_initialized = true;
    LOGX_INFO_MSG("Interface utilities initialized");
    return AUTONOMY_SUCCESS;
}

// Cleanup interface utilities
void interface_utils_cleanup(void) {
    if (!g_initialized) {
        return;
    }
    
    g_initialized = false;
    LOGX_INFO_MSG("Interface utilities cleaned up");
}

// Check if interface exists
bool interface_exists(const char* interface_name) {
    if (!interface_name) {
        set_error("Invalid interface name");
        return false;
    }
    
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        set_error("Failed to create socket");
        return false;
    }
    
    struct ifreq ifr;
    safe_strncpy(ifr.ifr_name, interface_name, IFNAMSIZ);
    
    int result = ioctl(sock, SIOCGIFFLAGS, &ifr);
    close(sock);
    
    return (result == 0);
}

// Check if interface is up
bool interface_is_up(const char* interface_name) {
    if (!interface_exists(interface_name)) {
        return false;
    }
    
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        set_error("Failed to create socket");
        return false;
    }
    
    struct ifreq ifr;
    safe_strncpy(ifr.ifr_name, interface_name, IFNAMSIZ);
    
    if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
        close(sock);
        set_error("Failed to get interface flags");
        return false;
    }
    
    close(sock);
    return (ifr.ifr_flags & IFF_UP) != 0;
}

// Get interface IP address
int interface_get_ip_address(const char* interface_name, char* ip_address, size_t size) {
    if (!interface_name || !ip_address || size == 0) {
        set_error("Invalid parameters");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        set_error("Failed to create socket");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    struct ifreq ifr;
    safe_strncpy(ifr.ifr_name, interface_name, IFNAMSIZ);
    
    if (ioctl(sock, SIOCGIFADDR, &ifr) < 0) {
        close(sock);
        set_error("Failed to get interface IP address");
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    struct sockaddr_in* addr_in = (struct sockaddr_in*)&ifr.ifr_addr;
    safe_strncpy(ip_address, inet_ntoa(addr_in->sin_addr), size);
    
    close(sock);
    return AUTONOMY_SUCCESS;
}

// Interface type detection
bool interface_is_cellular(const char* interface_name) {
    if (!interface_name) return false;
    return string_starts_with(interface_name, INTERFACE_CELLULAR_PREFIX);
}

bool interface_is_wifi(const char* interface_name) {
    if (!interface_name) return false;
    return string_starts_with(interface_name, INTERFACE_WIFI_PREFIX);
}

bool interface_is_ethernet(const char* interface_name) {
    if (!interface_name) return false;
    return string_starts_with(interface_name, INTERFACE_ETHERNET_PREFIX);
}

bool interface_is_starlink(const char* interface_name) {
    if (!interface_name) return false;
    return string_contains(interface_name, "starlink") || 
           string_contains(interface_name, "sl");
}

bool interface_is_vpn(const char* interface_name) {
    if (!interface_name) return false;
    return string_starts_with(interface_name, INTERFACE_VPN_WG_PREFIX) ||
           string_starts_with(interface_name, INTERFACE_VPN_OVPN_PREFIX);
}

// Calculate health score
double interface_calculate_health_score(const network_interface_unified_t* interface) {
    if (!interface) {
        set_error("Invalid interface parameter");
        return 0.0;
    }
    
    double score = 100.0;
    
    // Interface must be up
    if (!interface->is_up) {
        return 0.0;
    }
    
    // Latency penalty
    if (interface->latency > 0) {
        if (interface->latency > 1000) {
            score -= 50; // Very high latency
        } else if (interface->latency > 500) {
            score -= 30; // High latency
        } else if (interface->latency > 200) {
            score -= 15; // Moderate latency
        } else if (interface->latency > 100) {
            score -= 5;  // Slight latency
        }
    }
    
    // Packet loss penalty
    if (interface->packet_loss > 0) {
        score -= interface->packet_loss * 2; // 2 points per percent loss
    }
    
    // Signal strength for wireless interfaces
    if (interface_is_cellular(interface->name) || interface_is_wifi(interface->name)) {
        if (interface->signal_strength < -90) {
            score -= 30; // Very weak signal
        } else if (interface->signal_strength < -80) {
            score -= 20; // Weak signal
        } else if (interface->signal_strength < -70) {
            score -= 10; // Moderate signal
        }
    }
    
    // Ensure score is within bounds
    if (score < 0) score = 0;
    if (score > 100) score = 100;
    
    return score;
}

// Validate interface name
bool interface_name_is_valid(const char* interface_name) {
    if (!interface_name) return false;
    
    size_t len = strlen(interface_name);
    if (len == 0 || len >= IFNAMSIZ) return false;
    
    // Check for valid characters (alphanumeric, dash, underscore)
    for (size_t i = 0; i < len; i++) {
        char c = interface_name[i];
        if (!isalnum(c) && c != '-' && c != '_' && c != '.') {
            return false;
        }
    }
    
    return true;
}

// Get interface type string
const char* interface_type_to_string(const char* interface_name) {
    if (!interface_name) return "unknown";
    
    if (interface_is_cellular(interface_name)) return "cellular";
    if (interface_is_wifi(interface_name)) return "wifi";
    if (interface_is_ethernet(interface_name)) return "ethernet";
    if (interface_is_starlink(interface_name)) return "starlink";
    if (interface_is_vpn(interface_name)) return "vpn";
    
    return "unknown";
}

// Get error message
const char* interface_utils_get_last_error(void) {
    return g_interface_error;
}

// Clear error
void interface_utils_clear_error(void) {
    memset(g_interface_error, 0, sizeof(g_interface_error));
}

// NOLINTEND(cert-msc50-cpp,cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
// NOLINTEND(cert-msc51-cpp)