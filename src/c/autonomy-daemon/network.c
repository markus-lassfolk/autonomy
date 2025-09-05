#include "autonomy_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern struct autonomy_state g_state;

// Network discovery and management
int discover_network_interfaces(void) {
    // Initialize with some default interfaces
    g_state.interface_count = 0;
    
    // Add common interface types
    strcpy(g_state.interfaces[0].name, "eth0");
    strcpy(g_state.interfaces[0].type, "ethernet");
    g_state.interfaces[0].enabled = 1;
    g_state.interfaces[0].latency = 5.0;
    g_state.interfaces[0].loss = 0.1;
    g_state.interfaces[0].signal_strength = 100;
    g_state.interfaces[0].bandwidth = 1000;
    g_state.interfaces[0].last_check = time(NULL);
    g_state.interfaces[0].health_score = 95;
    strcpy(g_state.interfaces[0].status, "active");
    g_state.interface_count++;
    
    strcpy(g_state.interfaces[1].name, "wlan0");
    strcpy(g_state.interfaces[1].type, "wifi");
    g_state.interfaces[1].enabled = 1;
    g_state.interfaces[1].latency = 15.0;
    g_state.interfaces[1].loss = 2.0;
    g_state.interfaces[1].signal_strength = 85;
    g_state.interfaces[1].bandwidth = 300;
    g_state.interfaces[1].last_check = time(NULL);
    g_state.interfaces[1].health_score = 80;
    strcpy(g_state.interfaces[1].status, "active");
    g_state.interface_count++;
    
    strcpy(g_state.interfaces[2].name, "wwan0");
    strcpy(g_state.interfaces[2].type, "cellular");
    g_state.interfaces[2].enabled = 1;
    g_state.interfaces[2].latency = 50.0;
    g_state.interfaces[2].loss = 5.0;
    g_state.interfaces[2].signal_strength = 70;
    g_state.interfaces[2].bandwidth = 100;
    g_state.interfaces[2].last_check = time(NULL);
    g_state.interfaces[2].health_score = 65;
    strcpy(g_state.interfaces[2].status, "active");
    g_state.interface_count++;
    
    strcpy(g_state.active_interface, "eth0");
    g_state.failover_enabled = 1;
    g_state.last_network_check = time(NULL);
    g_state.network_health_score = 85.0;
    
    return 0;
}

static int calculate_interface_health_score(struct network_interface *iface) {
    int score = 100;
    
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
    if (score < 0) score = 0;
    
    return score;
}

int perform_network_health_check(void) {
    time_t now = time(NULL);
    
    for (int i = 0; i < g_state.interface_count; i++) {
        // Simulate network metrics update
        if (now - g_state.interfaces[i].last_check > 30) {
            // Update latency (simulate variation)
            g_state.interfaces[i].latency += (rand() % 10 - 5) * 0.1;
            if (g_state.interfaces[i].latency < 1.0) g_state.interfaces[i].latency = 1.0;
            
            // Update packet loss (simulate variation)
            g_state.interfaces[i].loss += (rand() % 6 - 3) * 0.1;
            if (g_state.interfaces[i].loss < 0.0) g_state.interfaces[i].loss = 0.0;
            
            // Update signal strength (simulate variation)
            g_state.interfaces[i].signal_strength += (rand() % 6 - 3);
            if (g_state.interfaces[i].signal_strength > 100) g_state.interfaces[i].signal_strength = 100;
            if (g_state.interfaces[i].signal_strength < 10) g_state.interfaces[i].signal_strength = 10;
            
            // Recalculate health score
            g_state.interfaces[i].health_score = calculate_interface_health_score(&g_state.interfaces[i]);
            g_state.interfaces[i].last_check = now;
        }
    }
    
    // Calculate overall network health score
    float total_score = 0;
    for (int i = 0; i < g_state.interface_count; i++) {
        if (g_state.interfaces[i].enabled) {
            total_score += g_state.interfaces[i].health_score;
        }
    }
    g_state.network_health_score = total_score / g_state.interface_count;
    g_state.last_network_check = now;
    
    return 0;
}
