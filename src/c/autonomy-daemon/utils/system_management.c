#include "system_management.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>
#include <libubus.h>
#include <libubox/blobmsg.h>
#include <sqlite3.h>

// UBUS policy definitions
enum {
    STARLINK_STATUS_CONNECTED,
    STARLINK_STATUS_OBSTRUCTED,
    STARLINK_STATUS_SNR,
    STARLINK_STATUS_DOWNLINK,
    STARLINK_STATUS_UPLINK,
    __STARLINK_STATUS_MAX
};

// System health status structure is defined in system_management.h

// Global system health state
static system_health_t g_system_health = {0};

// Check if Starlink is healthy using real Starlink integration
static bool check_starlink_health(void) {
    // Check Starlink dish connectivity and status
    struct ubus_context* ctx = ubus_connect(NULL);
    if (!ctx) {
        LOGX_WARN_MSG("Failed to connect to UBUS for Starlink health check");
        return false;
    }
    
    uint32_t id;
    int ret = ubus_lookup_id(ctx, "starlink.dish", &id);
    if (ret != 0) {
        LOGX_WARN_MSG("Starlink dish service not available via UBUS");
        ubus_free(ctx);
        return false;
    }
    
    // Get Starlink dish status
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    ret = ubus_invoke(ctx, id, "status", bb.head, NULL, NULL, 5000);
    if (ret != 0) {
        LOGX_WARN_MSG("Failed to get Starlink dish status", "error", ret);
        blob_buf_free(&bb);
        ubus_free(ctx);
        return false;
    }
    
    // Parse Starlink status response
    struct blob_attr *tb[__STARLINK_STATUS_MAX];
    static const struct blobmsg_policy policy[__STARLINK_STATUS_MAX] = {
        [STARLINK_STATUS_CONNECTED] = { .name = "connected", .type = BLOBMSG_TYPE_BOOL },
        [STARLINK_STATUS_OBSTRUCTED] = { .name = "obstructed", .type = BLOBMSG_TYPE_BOOL },
        [STARLINK_STATUS_SNR] = { .name = "snr", .type = BLOBMSG_TYPE_DOUBLE },
        [STARLINK_STATUS_DOWNLINK] = { .name = "downlink", .type = BLOBMSG_TYPE_DOUBLE },
        [STARLINK_STATUS_UPLINK] = { .name = "uplink", .type = BLOBMSG_TYPE_DOUBLE },
    };
    
    blobmsg_parse(policy, __STARLINK_STATUS_MAX, tb, blob_data(bb.head), blob_len(bb.head));
    
    bool healthy = true;
    
    // Check connection status
    if (tb[STARLINK_STATUS_CONNECTED]) {
        bool connected = blobmsg_get_bool(tb[STARLINK_STATUS_CONNECTED]);
        if (!connected) {
            LOGX_WARN_MSG("Starlink dish not connected");
            healthy = false;
        }
    }
    
    // Check obstruction status
    if (tb[STARLINK_STATUS_OBSTRUCTED]) {
        bool obstructed = blobmsg_get_bool(tb[STARLINK_STATUS_OBSTRUCTED]);
        if (obstructed) {
            LOGX_WARN_MSG("Starlink dish is obstructed");
            // Obstruction doesn't necessarily mean unhealthy, but worth noting
        }
    }
    
    // Check signal quality
    if (tb[STARLINK_STATUS_SNR]) {
        double snr = blobmsg_get_double(tb[STARLINK_STATUS_SNR]);
        if (snr < 5.0) { // Low SNR threshold
            LOGX_WARN_MSG("Starlink SNR is low", "snr", snr);
            healthy = false;
        }
    }
    
    // Check data rates
    if (tb[STARLINK_STATUS_DOWNLINK]) {
        double downlink = blobmsg_get_double(tb[STARLINK_STATUS_DOWNLINK]);
        if (downlink < 1.0) { // Less than 1 Mbps downlink
            LOGX_WARN_MSG("Starlink downlink speed is low", "downlink", downlink);
            healthy = false;
        }
    }
    
    if (tb[STARLINK_STATUS_UPLINK]) {
        double uplink = blobmsg_get_double(tb[STARLINK_STATUS_UPLINK]);
        if (uplink < 0.5) { // Less than 0.5 Mbps uplink
            LOGX_WARN_MSG("Starlink uplink speed is low", "uplink", uplink);
            healthy = false;
        }
    }
    
    blob_buf_free(&bb);
    ubus_free(ctx);
    
    LOGX_DEBUG_MSG("Starlink health check completed", "healthy", healthy);
    return healthy;
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

// Check database health using real database integration
static bool check_database_health(void) {
    // Check SQLite database health
    sqlite3* db = NULL;
    int ret = sqlite3_open("/var/lib/autonomy/autonomy.db", &db);
    if (ret != SQLITE_OK) {
        LOGX_WARN_MSG("Failed to open autonomy database", "error", sqlite3_errmsg(db));
        if (db) sqlite3_close(db);
        return false;
    }
    
    // Check database integrity
    char* err_msg = NULL;
    ret = sqlite3_exec(db, "PRAGMA integrity_check;", NULL, NULL, &err_msg);
    if (ret != SQLITE_OK) {
        LOGX_WARN_MSG("Database integrity check failed", "error", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return false;
    }
    sqlite3_free(err_msg);
    
    // Check if critical tables exist
    const char* critical_tables[] = {
        "gps_data", "starlink_data", "network_data", "telemetry_data", "notifications"
    };
    
    for (int i = 0; i < 5; i++) {
        char query[256];
        snprintf(query, sizeof(query), 
                "SELECT name FROM sqlite_master WHERE type='table' AND name='%s';", 
                critical_tables[i]);
        
        sqlite3_stmt* stmt;
        ret = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
        if (ret != SQLITE_OK) {
            LOGX_WARN_MSG("Failed to prepare table check query", "table", critical_tables[i]);
            sqlite3_close(db);
            return false;
        }
        
        ret = sqlite3_step(stmt);
        if (ret != SQLITE_ROW) {
            LOGX_WARN_MSG("Critical table missing", "table", critical_tables[i]);
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return false;
        }
        
        sqlite3_finalize(stmt);
    }
    
    // Check database file size and disk space
    struct stat db_stat;
    if (stat("/var/lib/autonomy/autonomy.db", &db_stat) == 0) {
        if (db_stat.st_size == 0) {
            LOGX_WARN_MSG("Database file is empty");
            sqlite3_close(db);
            return false;
        }
        
        // Check if database is growing (not corrupted)
        if (db_stat.st_size < 1024) { // Less than 1KB
            LOGX_WARN_MSG("Database file is too small", "size", db_stat.st_size);
            sqlite3_close(db);
            return false;
        }
    }
    
    // Check disk space for database directory
    struct statvfs vfs;
    if (statvfs("/var/lib/autonomy", &vfs) == 0) {
        unsigned long free_space = vfs.f_bavail * vfs.f_frsize;
        if (free_space < 10 * 1024 * 1024) { // Less than 10MB free
            LOGX_WARN_MSG("Low disk space for database", "free_space", free_space);
            sqlite3_close(db);
            return false;
        }
    }
    
    // Test database write capability
    ret = sqlite3_exec(db, "CREATE TEMP TABLE health_check (id INTEGER);", NULL, NULL, &err_msg);
    if (ret != SQLITE_OK) {
        LOGX_WARN_MSG("Database write test failed", "error", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return false;
    }
    sqlite3_free(err_msg);
    
    sqlite3_close(db);
    
    LOGX_DEBUG_MSG("Database health check completed successfully");
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
    g_system_health.starlink_health = check_starlink_health();
    g_system_health.uci_health = check_uci_health();
    g_system_health.overlay_health = check_overlay_health();
    g_system_health.services_health = check_services_health();
    g_system_health.network_health = check_network_health();
    g_system_health.database_health = check_database_health();
    g_system_health.time_health = check_time_health();
    g_system_health.logs_health = check_logs_health();
    
    // Calculate overall score (0-100)
    int healthy_count = 0;
    int total_checks = 8;
    
    if (g_system_health.starlink_health) healthy_count++;
    if (g_system_health.uci_health) healthy_count++;
    if (g_system_health.overlay_health) healthy_count++;
    if (g_system_health.services_health) healthy_count++;
    if (g_system_health.network_health) healthy_count++;
    if (g_system_health.database_health) healthy_count++;
    if (g_system_health.time_health) healthy_count++;
    if (g_system_health.logs_health) healthy_count++;
    
    g_system_health.overall_score = (healthy_count * 100) / total_checks;
    
    // Set status message
    if (g_system_health.overall_score >= 90) {
        strcpy(g_system_health.status, "Excellent");
    } else if (g_system_health.overall_score >= 75) {
        strcpy(g_system_health.status, "Good");
    } else if (g_system_health.overall_score >= 50) {
        strcpy(g_system_health.status, "Fair");
    } else {
        strcpy(g_system_health.status, "Poor");
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
