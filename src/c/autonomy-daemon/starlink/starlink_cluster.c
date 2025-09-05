#include "starlink_types.h"
#include "starlink_modules.h"
#include "../starlink_obstruction.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// Global Starlink cluster
static starlink_cluster_t g_starlink_cluster = {0};

// Initialize Starlink cluster
int starlink_cluster_init(void) {
    memset(&g_starlink_cluster, 0, sizeof(starlink_cluster_t));
    
    // Set default values
    g_starlink_cluster.auto_failover_enabled = true;
    g_starlink_cluster.failover_threshold = 3;
    g_starlink_cluster.min_health_score = 70.0;
    g_starlink_cluster.active_index = -1;
    
    // Initialize Starlink obstruction analysis
    int result = starlink_obstruction_init();
    if (result != 0) {
        return result;
    }
    
    return 0;
}

// Add a Starlink to the cluster
int starlink_cluster_add(const char *id, const starlink_config_t *config) {
    if (!id || !config || g_starlink_cluster.count >= MAX_STARLINKS) {
        return -1;
    }
    
    int index = g_starlink_cluster.count;
    starlink_instance_t *instance = &g_starlink_cluster.starlinks[index];
    
    // Initialize instance
    memset(instance, 0, sizeof(starlink_instance_t));
    strncpy(instance->id, id, sizeof(instance->id) - 1);
    memcpy(&instance->config, config, sizeof(starlink_config_t));
    
    // Set default values
    instance->is_active = false;
    instance->is_healthy = false;
    instance->reliability_score = 100.0;
    instance->average_latency = 0.0;
    instance->average_throughput = 0.0;
    
    g_starlink_cluster.count++;
    
    // If this is the first Starlink, make it active
    if (g_starlink_cluster.active_index == -1) {
        g_starlink_cluster.active_index = index;
        instance->is_active = true;
        strcpy(instance->failover_reason, "Initial active Starlink");
    }
    
    return index;
}

// Remove a Starlink from the cluster
int starlink_cluster_remove(const char *id) {
    if (!id) {
        return -1;
    }
    
    for (int i = 0; i < g_starlink_cluster.count; i++) {
        if (strcmp(g_starlink_cluster.starlinks[i].id, id) == 0) {
            // If this was the active Starlink, we need to failover
            if (i == g_starlink_cluster.active_index) {
                if (g_starlink_cluster.count > 1) {
                    // Find next best Starlink
                    int next_index = starlink_cluster_find_best_starlink();
                    if (next_index >= 0) {
                        starlink_cluster_failover_to(next_index, "Active Starlink removed");
                    }
                } else {
                    g_starlink_cluster.active_index = -1;
                }
            }
            
            // Remove this Starlink by shifting others
            for (int j = i; j < g_starlink_cluster.count - 1; j++) {
                memcpy(&g_starlink_cluster.starlinks[j], 
                       &g_starlink_cluster.starlinks[j + 1], 
                       sizeof(starlink_instance_t));
            }
            
            g_starlink_cluster.count--;
            
            // Adjust active_index if needed
            if (g_starlink_cluster.active_index > i) {
                g_starlink_cluster.active_index--;
            }
            
            return 0;
        }
    }
    
    return -1;
}

// Find the best Starlink based on performance metrics
int starlink_cluster_find_best_starlink(void) {
    if (g_starlink_cluster.count == 0) {
        return -1;
    }
    
    int best_index = -1;
    float best_score = -1.0;
    
    for (int i = 0; i < g_starlink_cluster.count; i++) {
        starlink_instance_t *instance = &g_starlink_cluster.starlinks[i];
        
        if (!instance->config.enabled || !instance->is_healthy) {
            continue;
        }
        
        // Calculate composite score based on multiple factors
        float score = 0.0;
        
        // Health score (40% weight)
        score += (instance->last_result.health.overall_score * 0.4);
        
        // Reliability score (30% weight)
        score += (instance->reliability_score * 0.3);
        
        // Latency score (20% weight) - lower latency = higher score
        if (instance->average_latency > 0) {
            float latency_score = 100.0 - (instance->average_latency / 10.0); // Normalize to 0-100
            if (latency_score < 0) latency_score = 0;
            score += (latency_score * 0.2);
        }
        
        // Throughput score (10% weight)
        if (instance->average_throughput > 0) {
            float throughput_score = (instance->average_throughput / 100.0); // Normalize to 0-100
            if (throughput_score > 100) throughput_score = 100;
            score += (throughput_score * 0.1);
        }
        
        if (score > best_score) {
            best_score = score;
            best_index = i;
        }
    }
    
    return best_index;
}

// Perform failover to a specific Starlink
int starlink_cluster_failover_to(int index, const char *reason) {
    if (index < 0 || index >= g_starlink_cluster.count) {
        return -1;
    }
    
    // Deactivate current active Starlink
    if (g_starlink_cluster.active_index >= 0 && g_starlink_cluster.active_index < g_starlink_cluster.count) {
        g_starlink_cluster.starlinks[g_starlink_cluster.active_index].is_active = false;
    }
    
    // Activate new Starlink
    g_starlink_cluster.starlinks[index].is_active = true;
    g_starlink_cluster.active_index = index;
    g_starlink_cluster.last_failover = time(NULL);
    g_starlink_cluster.failover_count++;
    
    // Set failover reason
    if (reason) {
        strncpy(g_starlink_cluster.starlinks[index].failover_reason, reason, 
                sizeof(g_starlink_cluster.starlinks[index].failover_reason) - 1);
    }
    
    return 0;
}

// Check if automatic failover is needed
int starlink_cluster_check_failover(void) {
    if (!g_starlink_cluster.auto_failover_enabled || g_starlink_cluster.count < 2) {
        return 0;
    }
    
    int current_active = g_starlink_cluster.active_index;
    if (current_active < 0 || current_active >= g_starlink_cluster.count) {
        return 0;
    }
    
    starlink_instance_t *active = &g_starlink_cluster.starlinks[current_active];
    
    // Check if current active Starlink is unhealthy
    bool needs_failover = false;
    char failover_reason[128] = {0};
    
    // Check health score
    if (active->last_result.health.overall_score < g_starlink_cluster.min_health_score) {
        needs_failover = true;
        snprintf(failover_reason, sizeof(failover_reason), 
                "Health score too low: %d < %.1f", 
                active->last_result.health.overall_score, 
                g_starlink_cluster.min_health_score);
    }
    
    // Check consecutive failures
    if (active->consecutive_failures >= g_starlink_cluster.failover_threshold) {
        needs_failover = true;
        snprintf(failover_reason, sizeof(failover_reason), 
                "Too many consecutive failures: %d", active->consecutive_failures);
    }
    
    // Check if Starlink is disabled
    if (!active->config.enabled) {
        needs_failover = true;
        strcpy(failover_reason, "Starlink disabled");
    }
    
    if (needs_failover) {
        // Find best alternative Starlink
        int best_index = starlink_cluster_find_best_starlink();
        if (best_index >= 0 && best_index != current_active) {
            return starlink_cluster_failover_to(best_index, failover_reason);
        }
    }
    
    return 0;
}

// Update Starlink instance with new data
int starlink_cluster_update_instance(int index, const starlink_collection_result_t *result) {
    if (index < 0 || index >= g_starlink_cluster.count || !result) {
        return -1;
    }
    
    starlink_instance_t *instance = &g_starlink_cluster.starlinks[index];
    
    // Update collection result
    memcpy(&instance->last_result, result, sizeof(starlink_collection_result_t));
    instance->last_collection = time(NULL);
    
    // Update health status
    instance->is_healthy = result->health.is_healthy;
    
    // Update consecutive counters
    if (result->success) {
        instance->consecutive_successes++;
        instance->consecutive_failures = 0;
    } else {
        instance->consecutive_failures++;
        instance->consecutive_successes = 0;
    }
    
    // Update performance metrics
    if (result->success) {
        // Update average latency
        float new_latency = result->status.network_perf.pop_ping_latency_ms;
        if (instance->average_latency == 0) {
            instance->average_latency = new_latency;
        } else {
            instance->average_latency = (instance->average_latency * 0.8) + (new_latency * 0.2);
        }
        
        // Update average throughput
        float new_throughput = result->status.network_perf.downlink_throughput_bps / 1000000.0; // Convert to Mbps
        if (instance->average_throughput == 0) {
            instance->average_throughput = new_throughput;
        } else {
            instance->average_throughput = (instance->average_throughput * 0.8) + (new_throughput * 0.2);
        }
        
        // Update reliability score
        if (instance->consecutive_successes > 0) {
            float success_rate = (float)instance->consecutive_successes / 
                               (instance->consecutive_successes + instance->consecutive_failures);
            instance->reliability_score = success_rate * 100.0;
        }
    }
    
    return 0;
}

// Get cluster status
int starlink_cluster_get_status(starlink_cluster_t *cluster) {
    if (!cluster) {
        return -1;
    }
    
    memcpy(cluster, &g_starlink_cluster, sizeof(starlink_cluster_t));
    return 0;
}

// Get active Starlink instance
const starlink_instance_t* starlink_cluster_get_active(void) {
    if (g_starlink_cluster.active_index >= 0 && g_starlink_cluster.active_index < g_starlink_cluster.count) {
        return &g_starlink_cluster.starlinks[g_starlink_cluster.active_index];
    }
    return NULL;
}

// Get Starlink instance by ID
const starlink_instance_t* starlink_cluster_get_by_id(const char *id) {
    if (!id) {
        return NULL;
    }
    
    for (int i = 0; i < g_starlink_cluster.count; i++) {
        if (strcmp(g_starlink_cluster.starlinks[i].id, id) == 0) {
            return &g_starlink_cluster.starlinks[i];
        }
    }
    
    return NULL;
}

// Get Starlink instance by index
const starlink_instance_t* starlink_cluster_get_by_index(int index) {
    if (index >= 0 && index < g_starlink_cluster.count) {
        return &g_starlink_cluster.starlinks[index];
    }
    return NULL;
}

// Set cluster configuration
void starlink_cluster_set_config(bool auto_failover, int failover_threshold, float min_health_score) {
    g_starlink_cluster.auto_failover_enabled = auto_failover;
    g_starlink_cluster.failover_threshold = failover_threshold;
    g_starlink_cluster.min_health_score = min_health_score;
}

// Cleanup cluster
void starlink_cluster_cleanup(void) {
    // Cleanup Starlink obstruction analysis
    starlink_obstruction_cleanup();
    
    memset(&g_starlink_cluster, 0, sizeof(starlink_cluster_t));
}
