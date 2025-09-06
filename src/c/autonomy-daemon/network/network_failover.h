#ifndef NETWORK_FAILOVER_H
#define NETWORK_FAILOVER_H

#include "../core/types.h"
#include "../utils/logx.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Forward declarations
// These functions are implemented in the corresponding .c file

// Network failover configuration
typedef struct {
    float health_threshold;      // Minimum health score for interface to be considered
    float failover_threshold;    // Health score below which failover is triggered
    int failover_timeout;        // Seconds to wait before allowing new failover
    int recovery_timeout;        // Seconds to wait before attempting recovery
    int check_interval;          // Seconds between health checks
    bool auto_failover;          // Enable automatic failover
} network_failover_config_t;

// Network failover status
typedef struct {
    bool enabled;                // Failover system enabled
    bool auto_failover;          // Automatic failover enabled
    float health_threshold;      // Current health threshold
    float failover_threshold;    // Current failover threshold
    int failover_timeout;        // Current failover timeout
    int recovery_timeout;        // Current recovery timeout
    int check_interval;          // Current check interval
    int active_interface_index;  // Index of currently active interface
    bool failover_in_progress;   // Failover currently in progress
    time_t last_failover;        // Timestamp of last failover
    uint64_t total_failovers;    // Total number of failovers performed
    int interface_count;         // Number of interfaces in failover system
} network_failover_status_t;

// Network failover state
typedef struct {
    bool enabled;                // Failover system enabled
    bool auto_failover;          // Automatic failover enabled
    float health_threshold;      // Minimum health score
    float failover_threshold;    // Failover trigger threshold
    int failover_timeout;        // Failover timeout in seconds
    int recovery_timeout;        // Recovery timeout in seconds
    int check_interval;          // Health check interval in seconds
    int active_interface_index;  // Index of active interface
    bool failover_in_progress;   // Failover in progress flag
    time_t last_failover;        // Last failover timestamp
    uint64_t total_failovers;    // Total failover count
    network_interface_t interfaces[MAX_INTERFACES]; // Interface list
    int interface_count;         // Number of interfaces
} network_failover_t;

// Initialize network failover system
int network_failover_init(void);

// Start failover monitoring thread
int network_failover_start_monitoring(void);

// Stop failover monitoring
void network_failover_stop_monitoring(void);

// Check health of all interfaces
int network_failover_check_health(void);

// Add interface to failover system
int network_failover_add_interface(const network_interface_t *interface);

// Remove interface from failover system
int network_failover_remove_interface(const char *interface_name);

// Force failover to specified interface
int network_failover_force_failover(const char *interface_name);

// Get failover status
int network_failover_get_status(network_failover_status_t *status);

// Set failover configuration
int network_failover_set_config(const network_failover_config_t *config);

// Enable/disable failover system
int network_failover_set_enabled(bool enabled);

// Cleanup failover system
void network_failover_cleanup(void);

#endif // NETWORK_FAILOVER_H
