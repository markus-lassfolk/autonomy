#include "autonomy_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>

extern struct autonomy_state g_state;

// Global system health variable (defined in autonomy_types.h)
struct system_health g_system_health = {0};

// System health check functions
static int check_starlink_health(void) {
    // Simulate Starlink health check
    int health = 85 + (rand() % 20); // 85-105 range
    if (health > 100) health = 100;
    
    g_system_health.starlink_health = health;
    return health;
}

static int check_uci_health(void) {
    // Simulate UCI configuration health check
    int health = 90 + (rand() % 15); // 90-105 range
    if (health > 100) health = 100;
    
    g_system_health.uci_health = health;
    return health;
}

static int check_overlay_health(void) {
    // Simulate overlay filesystem health check
    int health = 95 + (rand() % 10); // 95-105 range
    if (health > 100) health = 100;
    
    g_system_health.overlay_health = health;
    return health;
}

static int check_services_health(void) {
    // Simulate system services health check
    int health = 88 + (rand() % 17); // 88-105 range
    if (health > 100) health = 100;
    
    g_system_health.services_health = health;
    return health;
}

static int check_database_health(void) {
    // Simulate database health check
    int health = 92 + (rand() % 13); // 92-105 range
    if (health > 100) health = 100;
    
    g_system_health.database_health = health;
    return health;
}

static int check_time_health(void) {
    // Simulate time synchronization health check
    int health = 96 + (rand() % 9); // 96-105 range
    if (health > 100) health = 100;
    
    g_system_health.time_health = health;
    return health;
}

static int check_logs_health(void) {
    // Simulate log system health check
    int health = 87 + (rand() % 18); // 87-105 range
    if (health > 100) health = 100;
    
    g_system_health.logs_health = health;
    return health;
}

static int perform_system_health_check(void) {
    time_t now = time(NULL);
    
    // Perform all health checks
    check_starlink_health();
    check_uci_health();
    check_overlay_health();
    check_services_health();
    check_database_health();
    check_time_health();
    check_logs_health();
    
    // Calculate overall system health score
    int total_score = g_system_health.starlink_health +
                     g_system_health.uci_health +
                     g_system_health.overlay_health +
                     g_system_health.services_health +
                     g_system_health.database_health +
                     g_system_health.time_health +
                     g_system_health.logs_health;
    
    g_system_health.overall_score = total_score / 7;
    g_system_health.last_check = now;
    
    // Set overall status
    if (g_system_health.overall_score >= 90) {
        strcpy(g_system_health.status, "excellent");
    } else if (g_system_health.overall_score >= 80) {
        strcpy(g_system_health.status, "good");
    } else if (g_system_health.overall_score >= 70) {
        strcpy(g_system_health.status, "fair");
    } else {
        strcpy(g_system_health.status, "poor");
    }
    
    return 0;
}

static int get_system_memory_usage(void) {
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        // Convert to MB
        return (int)(si.totalram * si.mem_unit / (1024 * 1024));
    }
    return 0;
}

static int get_system_uptime(void) {
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        return (int)si.uptime;
    }
    return 0;
}

static int get_system_load_average(void) {
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        // Return load average as percentage
        return (int)((si.loads[0] / 65536.0) * 100);
    }
    return 0;
}
