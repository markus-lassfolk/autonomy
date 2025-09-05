#include "network_failover.h"
#include "logx.h"
#include "types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

// Global failover state
static network_failover_t g_failover = {0};
static pthread_mutex_t g_failover_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_failover_initialized = false;
static pthread_t g_failover_thread = 0;
static bool g_failover_thread_running = false;

// Failover thresholds
static const float DEFAULT_HEALTH_THRESHOLD = 70.0f;      // Minimum health score
static const float DEFAULT_FAILOVER_THRESHOLD = 50.0f;    // Health score to trigger failover
static const int DEFAULT_FAILOVER_TIMEOUT = 60;           // Seconds to wait before failover
static const int DEFAULT_RECOVERY_TIMEOUT = 300;          // Seconds to wait before recovery
static const int DEFAULT_CHECK_INTERVAL = 10;             // Seconds between health checks

// Initialize network failover system
static int network_failover_init(void) {
    if (g_failover_initialized) {
        LOGX_WARN("Network failover already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_failover_mutex);
    
    // Initialize failover state
    memset(&g_failover, 0, sizeof(network_failover_t));
    g_failover.enabled = true;
    g_failover.auto_failover = true;
    g_failover.health_threshold = DEFAULT_HEALTH_THRESHOLD;
    g_failover.failover_threshold = DEFAULT_FAILOVER_THRESHOLD;
    g_failover.failover_timeout = DEFAULT_FAILOVER_TIMEOUT;
    g_failover.recovery_timeout = DEFAULT_RECOVERY_TIMEOUT;
    g_failover.check_interval = DEFAULT_CHECK_INTERVAL;
    g_failover.active_interface_index = -1;
    g_failover.failover_in_progress = false;
    g_failover.last_failover = 0;
    g_failover.total_failovers = 0;
    
    g_failover_initialized = true;
    pthread_mutex_unlock(&g_failover_mutex);
    
    LOGX_INFO("Network failover system initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Start failover monitoring thread
static int network_failover_start_monitoring(void) {
    if (!g_failover_initialized) {
        LOGX_ERROR("Network failover not initialized");
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    if (g_failover_thread_running) {
        LOGX_WARN("Failover monitoring already running");
        return AUTONOMY_SUCCESS;
    }
    
    // Create monitoring thread
    int ret = pthread_create(&g_failover_thread, NULL, failover_monitor_thread, NULL);
    if (ret != 0) {
        LOGX_ERROR("Failed to create failover monitoring thread");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    g_failover_thread_running = true;
    LOGX_INFO("Network failover monitoring started");
    
    return AUTONOMY_SUCCESS;
}

// Stop failover monitoring
static void network_failover_stop_monitoring(void) {
    if (!g_failover_thread_running) {
        return;
    }
    
    g_failover_thread_running = false;
    
    if (g_failover_thread != 0) {
        pthread_join(g_failover_thread, NULL);
        g_failover_thread = 0;
    }
    
    LOGX_INFO("Network failover monitoring stopped");
}

// Failover monitoring thread
static void* failover_monitor_thread(void *arg) {
    (void)arg;
    
    LOGX_INFO("Failover monitoring thread started");
    
    while (g_failover_thread_running) {
        // Check interface health
        network_failover_check_health();
        
        // Sleep for check interval
        for (int i = 0; i < g_failover.check_interval && g_failover_thread_running; i++) {
            sleep(1);
        }
    }
    
    LOGX_INFO("Failover monitoring thread stopped");
    return NULL;
}

// Check health of all interfaces
static int network_failover_check_health(void) {
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
            LOGX_INFO("Failover timeout expired, allowing new failover");
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
                LOGX_WARN("Interface %s health below threshold (%.1f%% < %.1f%%), triggering failover",
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
            LOGX_INFO("No active interface, activating best interface: %s",
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

// Find the best interface based on health score
static int find_best_interface(void) {
    int best_index = -1;
    float best_score = -1.0f;
    
    for (int i = 0; i < g_failover.interface_count; i++) {
        network_interface_t *iface = &g_failover.interfaces[i];
        
        // Skip disabled interfaces
        if (!iface->enabled) {
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
    
    return best_index;
}

// Perform failover to specified interface
static int perform_failover(int target_interface) {
    if (target_interface < 0 || target_interface >= g_failover.interface_count) {
        LOGX_ERROR("Invalid target interface index: %d", target_interface);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    network_interface_t *target = &g_failover.interfaces[target_interface];
    
    LOGX_INFO("Performing failover to interface: %s (health: %.1f%%)",
              target->name, target->metrics.overall_health_score);
    
    // Deactivate current interface
    if (g_failover.active_interface_index >= 0) {
        network_interface_t *current = &g_failover.interfaces[g_failover.active_interface_index];
        LOGX_INFO("Deactivating current interface: %s", current->name);
        
        // Update routing (this would integrate with MWAN3)
        if (deactivate_interface_routing(g_failover.active_interface_index) != AUTONOMY_SUCCESS) {
            LOGX_WARN("Failed to deactivate routing for interface: %s", current->name);
        }
    }
    
    // Activate target interface
    int ret = activate_interface(target_interface);
    if (ret == AUTONOMY_SUCCESS) {
        g_failover.active_interface_index = target_interface;
        LOGX_INFO("Failover completed successfully to interface: %s", target->name);
        return AUTONOMY_SUCCESS;
    } else {
        LOGX_ERROR("Failed to activate target interface: %s", target->name);
        return ret;
    }
}

// Activate an interface
static int activate_interface(int interface_index) {
    if (interface_index < 0 || interface_index >= g_failover.interface_count) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    network_interface_t *iface = &g_failover.interfaces[interface_index];
    
    LOGX_INFO("Activating interface: %s", iface->name);
    
    // Update routing (this would integrate with MWAN3)
    if (activate_interface_routing(interface_index) != AUTONOMY_SUCCESS) {
        LOGX_WARN("Failed to activate routing for interface: %s", iface->name);
        return AUTONOMY_ERROR_NETWORK;
    }
    
    // Update interface status
    iface->up = true;
    iface->is_default_route = true;
    
    LOGX_INFO("Interface %s activated successfully", iface->name);
    return AUTONOMY_SUCCESS;
}

// Activate interface routing (MWAN3 integration)
static int activate_interface_routing(int interface_index) {
    if (interface_index < 0 || interface_index >= g_failover.interface_count) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    network_interface_t *iface = &g_failover.interfaces[interface_index];
    
    // This would integrate with MWAN3 to set the interface as active
    // For now, we'll simulate the integration
    
    char command[256];
    snprintf(command, sizeof(command), 
             "ubus call mwan3 set_status '{\"interface\":\"%s\",\"status\":\"online\"}'", 
             iface->name);
    
    LOGX_DEBUG("Executing MWAN3 command: %s", command);
    
    // In a real implementation, this would execute the MWAN3 command
    // For now, we'll just log it
    LOGX_INFO("Would execute MWAN3 command to activate interface: %s", iface->name);
    
    return AUTONOMY_SUCCESS;
}

// Deactivate interface routing (MWAN3 integration)
static int deactivate_interface_routing(int interface_index) {
    if (interface_index < 0 || interface_index >= g_failover.interface_count) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    network_interface_t *iface = &g_failover.interfaces[interface_index];
    
    // This would integrate with MWAN3 to set the interface as offline
    // For now, we'll simulate the integration
    
    char command[256];
    snprintf(command, sizeof(command), 
             "ubus call mwan3 set_status '{\"interface\":\"%s\",\"status\":\"offline\"}'", 
             iface->name);
    
    LOGX_DEBUG("Executing MWAN3 command: %s", command);
    
    // In a real implementation, this would execute the MWAN3 command
    // For now, we'll just log it
    LOGX_INFO("Would execute MWAN3 command to deactivate interface: %s", iface->name);
    
    return AUTONOMY_SUCCESS;
}

// Add interface to failover system
static int network_failover_add_interface(const network_interface_t *interface) {
    if (!g_failover_initialized || !interface) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_failover_mutex);
    
    if (g_failover.interface_count >= MAX_INTERFACES) {
        pthread_mutex_unlock(&g_failover_mutex);
        LOGX_ERROR("Maximum number of interfaces reached");
        return AUTONOMY_ERROR_ALREADY_EXISTS;
    }
    
    // Check if interface already exists
    for (int i = 0; i < g_failover.interface_count; i++) {
        if (strcmp(g_failover.interfaces[i].name, interface->name) == 0) {
            pthread_mutex_unlock(&g_failover_mutex);
            LOGX_WARN("Interface %s already exists in failover system", interface->name);
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
        LOGX_INFO("First interface %s set as active", interface->name);
    }
    
    pthread_mutex_unlock(&g_failover_mutex);
    
    LOGX_INFO("Interface %s added to failover system", interface->name);
    return AUTONOMY_SUCCESS;
}

// Remove interface from failover system
static int network_failover_remove_interface(const char *interface_name) {
    if (!g_failover_initialized || !interface_name) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_failover_mutex);
    
    for (int i = 0; i < g_failover.interface_count; i++) {
        if (strcmp(g_failover.interfaces[i].name, interface_name) == 0) {
            // If this is the active interface, we need to failover first
            if (i == g_failover.active_interface_index) {
                LOGX_WARN("Cannot remove active interface %s, failover required first", interface_name);
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
            LOGX_INFO("Interface %s removed from failover system", interface_name);
            return AUTONOMY_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&g_failover_mutex);
    return AUTONOMY_ERROR_NOT_FOUND;
}

// Force failover to specified interface
static int network_failover_force_failover(const char *interface_name) {
    if (!g_failover_initialized || !interface_name) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_failover_mutex);
    
    // Find interface
    int target_index = -1;
    for (int i = 0; i < g_failover.interface_count; i++) {
        if (strcmp(g_failover.interfaces[i].name, interface_name) == 0) {
            target_index = i;
            break;
        }
    }
    
    if (target_index == -1) {
        pthread_mutex_unlock(&g_failover_mutex);
        LOGX_ERROR("Interface %s not found in failover system", interface_name);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    // Check if interface is enabled and healthy
    network_interface_t *target = &g_failover.interfaces[target_index];
    if (!target->enabled) {
        pthread_mutex_unlock(&g_failover_mutex);
        LOGX_ERROR("Interface %s is disabled", interface_name);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (target->metrics.overall_health_score < g_failover.health_threshold) {
        pthread_mutex_unlock(&g_failover_mutex);
        LOGX_WARN("Interface %s health below threshold (%.1f%% < %.1f%%)", 
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
static int network_failover_get_status(network_failover_status_t *status) {
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
static int network_failover_set_config(const network_failover_config_t *config) {
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
    
    LOGX_INFO("Failover configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable failover system
static int network_failover_set_enabled(bool enabled) {
    if (!g_failover_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_failover_mutex);
    g_failover.enabled = enabled;
    pthread_mutex_unlock(&g_failover_mutex);
    
    LOGX_INFO("Network failover system %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Cleanup failover system
static void network_failover_cleanup(void) {
    if (!g_failover_initialized) {
        return;
    }
    
    // Stop monitoring thread
    network_failover_stop_monitoring();
    
    pthread_mutex_lock(&g_failover_mutex);
    g_failover_initialized = false;
    pthread_mutex_unlock(&g_failover_mutex);
    
    pthread_mutex_destroy(&g_failover_mutex);
    
    LOGX_INFO("Network failover system cleaned up");
}
