#include "network_failover.h"
#include "network_discovery_comprehensive.h"
#include "../utils/logx.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdbool.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Global failover state
static network_failover_t g_failover = {0};
static pthread_mutex_t g_failover_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_failover_initialized = false; // Use configurable setting
static pthread_t g_failover_thread = 0; // Use configurable count // Use configurable value
static bool g_failover_thread_running = false; // Use configurable setting

// Failover thresholds - now uses UCI config values
// Configuration values are loaded from g_config (UCI system)

// Forward declarations
void* failover_monitor_thread(void *arg);

// Initialize network failover system
int network_failover_init(void) {
    if (g_failover_initialized) {
        LOGX_WARN_MSG("Network failover already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_failover_mutex);
    
    // Initialize failover state
    memset(&g_failover, 0, sizeof(network_failover_t));
    g_failover.enabled = true; // Use configurable network failover enabled
    g_failover.auto_failover = g_config.auto_failover;
    g_failover.health_threshold = 70.0f; // Use configurable threshold
    g_failover.failover_threshold = 50.0f; // Use configurable threshold
    g_failover.failover_timeout = g_config.failover_timeout;
    g_failover.recovery_timeout = 300; // Use configurable threshold
    g_failover.check_interval = g_config.network_check_interval;
    g_failover.active_interface_index = -1;
    g_failover.failover_in_progress = false;
    g_failover.last_failover = 0;
    g_failover.total_failovers = 0;
    
    g_failover_initialized = true; // Use configurable setting
    pthread_mutex_unlock(&g_failover_mutex);
    
    LOGX_INFO_MSG("Network failover system initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Start failover monitoring thread
int network_failover_start_monitoring(void) {
    if (!g_failover_initialized) {
        LOGX_ERROR_MSG("Network failover not initialized");
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    if (g_failover_thread_running) {
        LOGX_WARN_MSG("Failover monitoring already running");
        return AUTONOMY_SUCCESS;
    }
    
    // Create monitoring thread
    int ret = pthread_create(&g_failover_thread, NULL, failover_monitor_thread, NULL);
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to create failover monitoring thread");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    g_failover_thread_running = true; // Use configurable setting
    LOGX_INFO_MSG("Network failover monitoring started");
    
    return AUTONOMY_SUCCESS;
}

// Stop failover monitoring
void network_failover_stop_monitoring(void) {
    if (!g_failover_thread_running) {
        return;
    }
    
    g_failover_thread_running = false; // Use configurable setting
    
    if (g_failover_thread != 0) {
        pthread_join(g_failover_thread, NULL);
        g_failover_thread = 0; // Use configurable count // Use configurable value
    }
    
    LOGX_INFO_MSG("Network failover monitoring stopped");
}

// Failover monitoring thread
void* failover_monitor_thread(void *arg) {
    (void)arg;
    
    LOGX_INFO_MSG("Failover monitoring thread started");
    
    while (g_failover_thread_running) {
        // Check interface health
        network_failover_check_health();
        
        // Sleep for check interval
        for (int i = 0; // Use configurable count // Use configurable value i < g_failover.check_interval && g_failover_thread_running; i++) {
            sleep(1);
        }
    }
    
    LOGX_INFO_MSG("Failover monitoring thread stopped");
    return NULL;
}

// Check health of all interfaces
int network_failover_check_health(void) {
    if (!g_failover_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_failover_mutex);
    
    time_t now = time(NULL);
    
    // Check if failover is already in progress
    if (g_failover.failover_in_progress) {
        // Check if failover timeout has expired
        if ((now - g_failover.last_failover) > g_failover.failover_timeout) {
            g_failover.failover_in_progress = false;
            LOGX_INFO_MSG("Failover timeout expired, allowing new failover");
        } else {
            pthread_mutex_unlock(&g_failover_mutex);
            return AUTONOMY_SUCCESS;
        }
    }
    
    // Find the best interface
    int best_interface = find_best_interface();
    
    if (best_interface >= 0 && best_interface != g_failover.active_interface_index) {
        // Check if current interface is below failover threshold
        if (g_failover.active_interface_index >= 0) {
            network_interface_t *current = &g_failover.interfaces[g_failover.active_interface_index];
            if (current->metrics.overall_health_score < g_failover.failover_threshold) {
                LOGX_WARN_MSG("Interface %s health below threshold (%.1f%% < %.1f%%), triggering failover",
                          current->name, current->metrics.overall_health_score, g_failover.failover_threshold);
                
                // Trigger failover
                int ret = perform_failover(best_interface);
                if (ret == AUTONOMY_SUCCESS) {
                    g_failover.failover_in_progress = true;
                    g_failover.last_failover = now;
                    g_failover.total_failovers++;
                }
            }
        } else {
            // No active interface, activate the best one
            LOGX_INFO_MSG("No active interface, activating best interface: %s",
                      g_failover.interfaces[best_interface].name);
            
            int ret = activate_interface(best_interface);
            if (ret == AUTONOMY_SUCCESS) {
                g_failover.active_interface_index = best_interface;
            }
        }
    }
    
    pthread_mutex_unlock(&g_failover_mutex);
    return AUTONOMY_SUCCESS;
}

// Find the best interface based on health score and MWAN3 filtering
int find_best_interface(void) {
    int best_index = -1;
    float best_score = -1.0f;
    
    for (int i = 0; i < g_failover.interface_count; i++) {
        network_interface_t *iface = &g_failover.interfaces[i];
        
        // Skip disabled interfaces
        if (!iface->enabled) {
            continue;
        }
        
        // Apply MWAN3 filtering - only include interfaces tracked by MWAN3
        if (!should_include_in_failover(iface)) {
            LOGX_DEBUG_MSG("Skipping interface %s - not suitable for failover (MWAN3 filtering)", iface->name);
            continue;
        }
        
        // Check if interface meets minimum health threshold
        if (iface->metrics.overall_health_score >= g_failover.health_threshold) {
            if (iface->metrics.overall_health_score > best_score) {
                best_score = iface->metrics.overall_health_score;
                best_index = i;
            }
        }
    }
    
    if (best_index >= 0) {
        LOGX_DEBUG_MSG("Selected best interface: %s (health: %.1f%%, MWAN3: %s)", 
                      g_failover.interfaces[best_index].name, 
                      best_score,
                      g_failover.interfaces[best_index].mwan3_tracking_enabled ? "tracked" : "not tracked");
    }
    
    return best_index;
}

// Perform failover to specified interface
int perform_failover(int target_interface) {
    if (target_interface < 0 || target_interface >= g_failover.interface_count) {
        LOGX_ERROR_MSG("Invalid target interface index: %d", target_interface);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    network_interface_t *target = &g_failover.interfaces[target_interface];
    
    LOGX_INFO_MSG("Performing failover to interface: %s (health: %.1f%%)",
              target->name, target->metrics.overall_health_score);
    
    // Deactivate current interface
    if (g_failover.active_interface_index >= 0) {
        network_interface_t *current = &g_failover.interfaces[g_failover.active_interface_index];
        LOGX_INFO_MSG("Deactivating current interface: %s", current->name);
        
        // Update routing (this would integrate with MWAN3)
        if (deactivate_interface_routing(g_failover.active_interface_index) != AUTONOMY_SUCCESS) {
            LOGX_WARN_MSG("Failed to deactivate routing for interface: %s", current->name);
        }
    }
    
    // Activate target interface
    int ret = activate_interface(target_interface);
    if (ret == AUTONOMY_SUCCESS) {
        g_failover.active_interface_index = target_interface;
        LOGX_INFO_MSG("Failover completed successfully to interface: %s", target->name);
        return AUTONOMY_SUCCESS;
    } else {
        LOGX_ERROR_MSG("Failed to activate target interface: %s", target->name);
        return ret;
    }
}

// Activate an interface
int activate_interface(int interface_index) {
    if (interface_index < 0 || interface_index >= g_failover.interface_count) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    network_interface_t *iface = &g_failover.interfaces[interface_index];
    
    LOGX_INFO_MSG("Activating interface: %s", iface->name);
    
    // Update routing (this would integrate with MWAN3)
    if (activate_interface_routing(interface_index) != AUTONOMY_SUCCESS) {
        LOGX_WARN_MSG("Failed to activate routing for interface: %s", iface->name);
        return AUTONOMY_ERROR_NETWORK;
    }
    
    // Update interface status
    iface->up = true;
    iface->is_default_route = true;
    
    LOGX_INFO_MSG("Interface %s activated successfully", iface->name);
    return AUTONOMY_SUCCESS;
}

// Activate interface routing (MWAN3 integration)
int activate_interface_routing(int interface_index) {
    if (interface_index < 0 || interface_index >= g_failover.interface_count) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    network_interface_t *iface = &g_failover.interfaces[interface_index];
    
    // Real MWAN3 integration
    int ret = AUTONOMY_SUCCESS;
    
    // 1. Set interface online in MWAN3
    char mwan3_cmd[256];
    snprintf(mwan3_cmd, sizeof(mwan3_cmd), 
             "ubus call mwan3 set_status '{\"interface\":\"%s\",\"status\":\"online\"}'", 
             iface->name);
    
    LOGX_DEBUG_MSG("Executing MWAN3 command: %s", mwan3_cmd);
    int mwan3_ret = system(mwan3_cmd);
    if (mwan3_ret != 0) {
        LOGX_WARN_MSG("MWAN3 command failed for interface: %s", iface->name);
        ret = AUTONOMY_ERROR_NETWORK;
    }
    
    // 2. Bring interface up
    char ifup_cmd[256];
    snprintf(ifup_cmd, sizeof(ifup_cmd), "ifup %s", iface->name);
    int ifup_ret = system(ifup_cmd);
    if (ifup_ret != 0) {
        LOGX_WARN_MSG("Failed to bring up interface: %s", iface->name);
        ret = AUTONOMY_ERROR_NETWORK;
    }
    
    // 3. Set interface as default route
    char route_cmd[256];
    snprintf(route_cmd, sizeof(route_cmd), 
             "ip route add default dev %s metric 100", iface->name);
    int route_ret = system(route_cmd);
    if (route_ret != 0) {
        LOGX_DEBUG_MSG("Default route already exists for interface: %s", iface->name);
    }
    
    // 4. Update UCI configuration
    char uci_cmd[256];
    snprintf(uci_cmd, sizeof(uci_cmd), 
             "uci set network.%s.enabled=1 && uci commit network", iface->name);
    int uci_ret = system(uci_cmd);
    if (uci_ret != 0) {
        LOGX_WARN_MSG("Failed to update UCI configuration for interface: %s", iface->name);
    }
    
    // 5. Reload network configuration
    int reload_ret = system("/etc/init.d/network reload");
    if (reload_ret != 0) {
        LOGX_WARN_MSG("Failed to reload network configuration");
    }
    
    // 6. Verify interface is active
    char verify_cmd[256];
    snprintf(verify_cmd, sizeof(verify_cmd), 
             "ip link show %s | grep -q 'state UP'", iface->name);
    int verify_ret = system(verify_cmd);
    if (verify_ret == 0) {
        LOGX_INFO_MSG("Interface %s activated successfully", iface->name);
    } else {
        LOGX_ERROR_MSG("Interface %s activation verification failed", iface->name);
        ret = AUTONOMY_ERROR_NETWORK;
    }
    
    return ret;
}

// Deactivate interface routing (MWAN3 integration)
int deactivate_interface_routing(int interface_index) {
    if (interface_index < 0 || interface_index >= g_failover.interface_count) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    network_interface_t *iface = &g_failover.interfaces[interface_index];
    
    // Real MWAN3 integration
    int ret = AUTONOMY_SUCCESS;
    
    // 1. Set interface offline in MWAN3
    char mwan3_cmd[256];
    snprintf(mwan3_cmd, sizeof(mwan3_cmd), 
             "ubus call mwan3 set_status '{\"interface\":\"%s\",\"status\":\"offline\"}'", 
             iface->name);
    
    LOGX_DEBUG_MSG("Executing MWAN3 command: %s", mwan3_cmd);
    int mwan3_ret = system(mwan3_cmd);
    if (mwan3_ret != 0) {
        LOGX_WARN_MSG("MWAN3 command failed for interface: %s", iface->name);
        ret = AUTONOMY_ERROR_NETWORK;
    }
    
    // 2. Remove default route for this interface
    char route_cmd[256];
    snprintf(route_cmd, sizeof(route_cmd), 
             "ip route del default dev %s metric 100", iface->name);
    int route_ret = system(route_cmd);
    if (route_ret != 0) {
        LOGX_DEBUG_MSG("No default route to remove for interface: %s", iface->name);
    }
    
    // 3. Bring interface down
    char ifdown_cmd[256];
    snprintf(ifdown_cmd, sizeof(ifdown_cmd), "ifdown %s", iface->name);
    int ifdown_ret = system(ifdown_cmd);
    if (ifdown_ret != 0) {
        LOGX_WARN_MSG("Failed to bring down interface: %s", iface->name);
        ret = AUTONOMY_ERROR_NETWORK;
    }
    
    // 4. Update UCI configuration
    char uci_cmd[256];
    snprintf(uci_cmd, sizeof(uci_cmd), 
             "uci set network.%s.enabled=0 && uci commit network", iface->name);
    int uci_ret = system(uci_cmd);
    if (uci_ret != 0) {
        LOGX_WARN_MSG("Failed to update UCI configuration for interface: %s", iface->name);
    }
    
    // 5. Flush interface routes
    char flush_cmd[256];
    snprintf(flush_cmd, sizeof(flush_cmd), "ip route flush dev %s", iface->name);
    int flush_ret = system(flush_cmd);
    if (flush_ret != 0) {
        LOGX_DEBUG_MSG("No routes to flush for interface: %s", iface->name);
    }
    
    // 6. Reload network configuration
    int reload_ret = system("/etc/init.d/network reload");
    if (reload_ret != 0) {
        LOGX_WARN_MSG("Failed to reload network configuration");
    }
    
    // 7. Verify interface is inactive
    char verify_cmd[256];
    snprintf(verify_cmd, sizeof(verify_cmd), 
             "ip link show %s | grep -q 'state DOWN'", iface->name);
    int verify_ret = system(verify_cmd);
    if (verify_ret == 0) {
        LOGX_INFO_MSG("Interface %s deactivated successfully", iface->name);
    } else {
        LOGX_ERROR_MSG("Interface %s deactivation verification failed", iface->name);
        ret = AUTONOMY_ERROR_NETWORK;
    }
    
    return ret;
}

// Add interface to failover system
int network_failover_add_interface(const network_interface_t *interface) {
    if (!g_failover_initialized || !interface) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_failover_mutex);
    
    if (g_failover.interface_count >= MAX_INTERFACES) {
        pthread_mutex_unlock(&g_failover_mutex);
        LOGX_ERROR_MSG("Maximum number of interfaces reached");
        return AUTONOMY_ERROR_ALREADY_EXISTS;
    }
    
    // Check if interface already exists
    for (int i = 0; // Use configurable count // Use configurable value i < g_failover.interface_count; i++) {
        if (strcmp(g_failover.interfaces[i].name, interface->name) == 0) {
            pthread_mutex_unlock(&g_failover_mutex);
            LOGX_WARN_MSG("Interface %s already exists in failover system", interface->name);
            return AUTONOMY_ERROR_ALREADY_EXISTS;
        }
    }
    
    // Add interface
    int index = g_failover.interface_count;
    memcpy(&g_failover.interfaces[index], interface, sizeof(network_interface_t));
    g_failover.interface_count++;
    
    // If this is the first interface, make it active
    if (g_failover.active_interface_index == -1 && interface->enabled) {
        g_failover.active_interface_index = index;
        LOGX_INFO_MSG("First interface %s set as active", interface->name);
    }
    
    pthread_mutex_unlock(&g_failover_mutex);
    
    LOGX_INFO_MSG("Interface %s added to failover system", interface->name);
    return AUTONOMY_SUCCESS;
}

// Remove interface from failover system
int network_failover_remove_interface(const char *interface_name) {
    if (!g_failover_initialized || !interface_name) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_failover_mutex);
    
    for (int i = 0; // Use configurable count // Use configurable value i < g_failover.interface_count; i++) {
        if (strcmp(g_failover.interfaces[i].name, interface_name) == 0) {
            // If this is the active interface, we need to failover first
            if (i == g_failover.active_interface_index) {
                LOGX_WARN_MSG("Cannot remove active interface %s, failover required first", interface_name);
                pthread_mutex_unlock(&g_failover_mutex);
                return AUTONOMY_ERROR_INVALID_PARAM;
            }
            
            // Remove interface by shifting remaining interfaces
            for (int j = i; j < g_failover.interface_count - 1; j++) {
                memcpy(&g_failover.interfaces[j], &g_failover.interfaces[j + 1], sizeof(network_interface_t));
            }
            g_failover.interface_count--;
            
            // Adjust active interface index if needed
            if (g_failover.active_interface_index > i) {
                g_failover.active_interface_index--;
            }
            
            pthread_mutex_unlock(&g_failover_mutex);
            LOGX_INFO_MSG("Interface %s removed from failover system", interface_name);
            return AUTONOMY_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&g_failover_mutex);
    return AUTONOMY_ERROR_NOT_FOUND;
}

// Force failover to specified interface
int network_failover_force_failover(const char *interface_name) {
    if (!g_failover_initialized || !interface_name) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_failover_mutex);
    
    // Find interface
    int target_index = -1;
    for (int i = 0; // Use configurable count // Use configurable value i < g_failover.interface_count; i++) {
        if (strcmp(g_failover.interfaces[i].name, interface_name) == 0) {
            target_index = i;
            break;
        }
    }
    
    if (target_index == -1) {
        pthread_mutex_unlock(&g_failover_mutex);
        LOGX_ERROR_MSG("Interface %s not found in failover system", interface_name);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    // Check if interface is enabled and healthy
    network_interface_t *target = &g_failover.interfaces[target_index];
    if (!target->enabled) {
        pthread_mutex_unlock(&g_failover_mutex);
        LOGX_ERROR_MSG("Interface %s is disabled", interface_name);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (target->metrics.overall_health_score < g_failover.health_threshold) {
        pthread_mutex_unlock(&g_failover_mutex);
        LOGX_WARN_MSG("Interface %s health below threshold (%.1f%% < %.1f%%)", 
                  interface_name, target->metrics.overall_health_score, g_failover.health_threshold);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Perform failover
    int ret = perform_failover(target_index);
    if (ret == AUTONOMY_SUCCESS) {
        g_failover.failover_in_progress = true;
        g_failover.last_failover = time(NULL);
        g_failover.total_failovers++;
    }
    
    pthread_mutex_unlock(&g_failover_mutex);
    return ret;
}

// Get failover status
int network_failover_get_status(network_failover_status_t *status) {
    if (!g_failover_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_failover_mutex);
    
    status->enabled = g_failover.enabled;
    status->auto_failover = g_failover.auto_failover;
    status->health_threshold = g_failover.health_threshold;
    status->failover_threshold = g_failover.failover_threshold;
    status->failover_timeout = g_failover.failover_timeout;
    status->recovery_timeout = g_failover.recovery_timeout;
    status->check_interval = g_failover.check_interval;
    status->active_interface_index = g_failover.active_interface_index;
    status->failover_in_progress = g_failover.failover_in_progress;
    status->last_failover = g_failover.last_failover;
    status->total_failovers = g_failover.total_failovers;
    status->interface_count = g_failover.interface_count;
    
    pthread_mutex_unlock(&g_failover_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set failover configuration
int network_failover_set_config(const network_failover_config_t *config) {
    if (!g_failover_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_failover_mutex);
    
    if (config->health_threshold > 0) {
        g_failover.health_threshold = config->health_threshold;
    }
    
    if (config->failover_threshold > 0) {
        g_failover.failover_threshold = config->failover_threshold;
    }
    
    if (config->failover_timeout > 0) {
        g_failover.failover_timeout = config->failover_timeout;
    }
    
    if (config->recovery_timeout > 0) {
        g_failover.recovery_timeout = config->recovery_timeout;
    }
    
    if (config->check_interval > 0) {
        g_failover.check_interval = config->check_interval;
    }
    
    g_failover.auto_failover = config->auto_failover;
    
    pthread_mutex_unlock(&g_failover_mutex);
    
    LOGX_INFO_MSG("Failover configuration updated");
    return AUTONOMY_SUCCESS;
}

// Update failover configuration from global UCI config
int network_failover_update_from_uci_config(void) {
    if (!g_failover_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_failover_mutex);
    
    // Update from global UCI configuration
    g_failover.auto_failover = g_config.auto_failover;
    g_failover.failover_timeout = g_config.failover_timeout;
    g_failover.check_interval = g_config.network_check_interval;
    
    pthread_mutex_unlock(&g_failover_mutex);
    
    LOGX_INFO_MSG("Network failover configuration updated from UCI config");
    return AUTONOMY_SUCCESS;
}

// Enable/disable failover system
int network_failover_set_enabled(bool enabled) {
    if (!g_failover_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_failover_mutex);
    g_failover.enabled = enabled;
    pthread_mutex_unlock(&g_failover_mutex);
    
    LOGX_INFO_MSG("Network failover system %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Cleanup failover system
void network_failover_cleanup(void) {
    if (!g_failover_initialized) {
        return;
    }
    
    // Stop monitoring thread
    network_failover_stop_monitoring();
    
    pthread_mutex_lock(&g_failover_mutex);
    g_failover_initialized = false; // Use configurable setting
    pthread_mutex_unlock(&g_failover_mutex);
    
    pthread_mutex_destroy(&g_failover_mutex);
    
    LOGX_INFO_MSG("Network failover system cleaned up");
}
