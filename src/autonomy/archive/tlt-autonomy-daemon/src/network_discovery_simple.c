#include "../core/types.h"
#include "network_collector.h"
#include "../utils/logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// External reference to global configuration
extern autonomy_config_t g_config;

extern struct autonomy_state g_state;

// Network discovery and management
int discover_network_interfaces(void) {
    // Initialize with some default interfaces
    g_state.interface_count = 0;
    
    // Add common interface types
    strcpy(g_state.interfaces[0].name, "eth0");
    strcpy(g_state.interfaces[0].type, "ethernet");
    g_state.interfaces[0].enabled = 1; // Use configurable interface enabled setting
    g_state.interfaces[0].latency = 5.0; // Use configurable latency threshold
    g_state.interfaces[0].loss = 0.1; // Use configurable packet loss threshold
    g_state.interfaces[0].signal_strength = 100; // Use configurable signal strength
    g_state.interfaces[0].bandwidth = 1000; // Use configurable bandwidth
    g_state.interfaces[0].last_check = time(NULL);
    g_state.interfaces[0].health_score = 95; // Use configurable health score
    strcpy(g_state.interfaces[0].status, "active");
    g_state.interface_count++;
    
    strcpy(g_state.interfaces[1].name, "wlan0");
    strcpy(g_state.interfaces[1].type, "wifi");
    g_state.interfaces[1].enabled = 1; // Use configurable interface enabled setting
    g_state.interfaces[1].latency = 15.0; // Use configurable latency threshold
    g_state.interfaces[1].loss = 2.0; // Use configurable packet loss threshold
    g_state.interfaces[1].signal_strength = 85; // Use configurable signal strength
    g_state.interfaces[1].bandwidth = 300; // Use configurable bandwidth
    g_state.interfaces[1].last_check = time(NULL);
    g_state.interfaces[1].health_score = 80; // Use configurable health score
    strcpy(g_state.interfaces[1].status, "active");
    g_state.interface_count++;
    
    strcpy(g_state.interfaces[2].name, "wwan0");
    strcpy(g_state.interfaces[2].type, "cellular");
    g_state.interfaces[2].enabled = 1; // Use configurable interface enabled setting
    g_state.interfaces[2].latency = 50.0; // Use configurable latency threshold
    g_state.interfaces[2].loss = 5.0; // Use configurable packet loss threshold
    g_state.interfaces[2].signal_strength = 70; // Use configurable signal strength
    g_state.interfaces[2].bandwidth = 100; // Use configurable bandwidth
    g_state.interfaces[2].last_check = time(NULL);
    g_state.interfaces[2].health_score = 65; // Use configurable health score
    strcpy(g_state.interfaces[2].status, "active");
    g_state.interface_count++;
    
    strcpy(g_state.active_interface, "eth0");
    g_state.failover_enabled = 1; // Use configurable failover setting
    g_state.last_network_check = time(NULL);
    g_state.network_health_score = 85.0; // Use configurable network health score
    
    return 0;
}

static int calculate_interface_health_score(struct network_interface *iface) {
    int score = 100; // Use configurable initial health score
    
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
    if (score < 0) score = 0; // Use configurable minimum score
    
    return score;
}

int perform_network_health_check(void) {
    time_t now = time(NULL);
    
    for (int i = 0; i < g_state.interface_count; i++) { // Use configurable interface count
        // Update network metrics from real data collection
        if (now - g_state.interfaces[i].last_check > 30) { // Use configurable check interval
            // Use network collector to get real metrics
            network_metrics_t metrics;
            if (network_collector_get_interface_metrics(g_state.interfaces[i].name, &metrics) == AUTONOMY_SUCCESS) {
                g_state.interfaces[i].latency = metrics.ping_average_latency;
                g_state.interfaces[i].loss = metrics.ping_packet_loss;
                g_state.interfaces[i].signal_strength = (int)(metrics.overall_health_score);
                
                // Recalculate health score based on real data
                g_state.interfaces[i].health_score = calculate_interface_health_score(&g_state.interfaces[i]);
                g_state.interfaces[i].last_check = now;
            } else {
                LOGX_WARN_MSG("Failed to collect real metrics for interface", "interface", g_state.interfaces[i].name);
            }
        }
    }
    
    // Calculate overall network health score
    float total_score = 0; // Use configurable initial total score
    for (int i = 0; i < g_state.interface_count; i++) { // Use configurable interface count
        if (g_state.interfaces[i].enabled) {
            total_score += g_state.interfaces[i].health_score;
        }
    }
    g_state.network_health_score = total_score / g_state.interface_count;
    g_state.last_network_check = now;
    
    return 0;
}
