#include "system_management.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <dirent.h>

// System health status structure is defined in system_management.h

// Global system health state
static system_health_t g_system_health = {0};

// Check if Starlink is healthy
static bool check_starlink_health(void) {
    // For now, assume healthy - this would integrate with Starlink module
    return true;
}

// Check UCI configuration health
static bool check_uci_health(void) {
    // Check if UCI is accessible and working
    FILE *fp = popen("uci show", "r");
    if (!fp) return false;
    
    char buffer[128];
    bool healthy = false;
    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        healthy = true;
    }
    pclose(fp);
    return healthy;
}

// Check overlay filesystem health
static bool check_overlay_health(void) {
    // Check if overlay is mounted and writable
    struct statvfs stat;
    if (statvfs("/overlay", &stat) != 0) {
        return false;
    }
    return (stat.f_blocks > 0 && stat.f_bavail > 0);
}

// Check critical services health
static bool check_services_health(void) {
    // Check if key services are running
    const char *services[] = {"ubus", "uci", "network", NULL};
    bool healthy = true;
    
    for (int i = 0; services[i] != NULL; i++) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "pgrep -x %s > /dev/null", services[i]);
        if (system(cmd) != 0) {
            healthy = false;
            break;
        }
    }
    return healthy;
}

// Check network health
static bool check_network_health(void) {
    // Check if network interfaces are up
    FILE *fp = popen("ip link show | grep -c 'state UP'", "r");
    if (!fp) return false;
    
    char buffer[128];
    int up_interfaces = 0;
    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        up_interfaces = atoi(buffer);
    }
    pclose(fp);
    
    return up_interfaces > 0;
}

// Check database health (if applicable)
static bool check_database_health(void) {
    // For now, assume healthy - would check actual database if present
    return true;
}

// Check system time health
static bool check_time_health(void) {
    // Check if system time is reasonable (not 1970)
    time_t now = time(NULL);
    return (now > 1600000000); // After 2020
}

// Check logs health
static bool check_logs_health(void) {
    // Check if log directory is writable
    struct statvfs stat;
    if (statvfs("/var/log", &stat) != 0) {
        return false;
    }
    return (stat.f_bavail > 0);
}

// Perform comprehensive system health check
int perform_system_health_check(void) {
    // Reset health status
    memset(&g_system_health, 0, sizeof(g_system_health));
    
    // Check individual components
    g_system_health.starlink_healthy = check_starlink_health();
    g_system_health.uci_healthy = check_uci_health();
    g_system_health.overlay_healthy = check_overlay_health();
    g_system_health.services_healthy = check_services_health();
    g_system_health.network_healthy = check_network_health();
    g_system_health.database_healthy = check_database_health();
    g_system_health.time_healthy = check_time_health();
    g_system_health.logs_healthy = check_logs_health();
    
    // Calculate overall score (0-100)
    int healthy_count = 0;
    int total_checks = 8;
    
    if (g_system_health.starlink_healthy) healthy_count++;
    if (g_system_health.uci_healthy) healthy_count++;
    if (g_system_health.overlay_healthy) healthy_count++;
    if (g_system_health.services_healthy) healthy_count++;
    if (g_system_health.network_healthy) healthy_count++;
    if (g_system_health.database_healthy) healthy_count++;
    if (g_system_health.time_healthy) healthy_count++;
    if (g_system_health.logs_healthy) healthy_count++;
    
    g_system_health.overall_score = (healthy_count * 100) / total_checks;
    
    // Set status message
    if (g_system_health.overall_score >= 90) {
        strcpy(g_system_health.status_message, "Excellent");
    } else if (g_system_health.overall_score >= 75) {
        strcpy(g_system_health.status_message, "Good");
    } else if (g_system_health.overall_score >= 50) {
        strcpy(g_system_health.status_message, "Fair");
    } else {
        strcpy(g_system_health.status_message, "Poor");
    }
    
    return 0;
}

// Get system memory usage
int get_system_memory_usage(unsigned long *total, unsigned long *available) {
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        return -1;
    }
    
    if (total) *total = info.totalram * info.mem_unit;
    if (available) *available = info.freeram * info.mem_unit;
    
    return 0;
}

// Get system uptime
unsigned long get_system_uptime(void) {
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        return 0;
    }
    return info.uptime;
}

// Get system load average
int get_system_load_average(double *load1, double *load5, double *load15) {
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        return -1;
    }
    
    if (load1) *load1 = (double)info.loads[0] / (1 << SI_LOAD_SHIFT);
    if (load5) *load5 = (double)info.loads[1] / (1 << SI_LOAD_SHIFT);
    if (load15) *load15 = (double)info.loads[2] / (1 << SI_LOAD_SHIFT);
    
    return 0;
}

// Get current system health status
const system_health_t* get_system_health_status(void) {
    return &g_system_health;
}
