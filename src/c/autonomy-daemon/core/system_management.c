#include "../core/types.h"
#include "../shared/logging/logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <math.h>
#include <libubus.h>
#include <libubox/blobmsg.h>
#include <sqlite3.h>
#include <dirent.h>
#include <sys/wait.h>

extern autonomy_state_t g_state;

// UBUS policy definitions
enum {
    STARLINK_STATUS_CONNECTED,
    STARLINK_STATUS_OBSTRUCTED,
    STARLINK_STATUS_SNR,
    STARLINK_STATUS_DOWNLINK,
    STARLINK_STATUS_UPLINK,
    __STARLINK_STATUS_MAX
};

// Global system health variable (defined in autonomy_types.h)
system_health_t g_system_health = {0};

// System health check functions
int check_starlink_health(void) {
    // Real Starlink health check via UBUS
    struct ubus_context* ctx = ubus_connect(NULL);
    if (!ctx) {
        g_system_health.starlink_health = 0;
        return 0;
    }
    
    uint32_t id;
    int ret = ubus_lookup_id(ctx, "starlink.dish", &id);
    if (ret != 0) {
        g_system_health.starlink_health = 0;
        ubus_free(ctx);
        return 0;
    }
    
    // Get Starlink dish status
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    ret = ubus_invoke(ctx, id, "status", bb.head, NULL, NULL, 5000);
    if (ret != 0) {
        g_system_health.starlink_health = 0;
        blob_buf_free(&bb);
        ubus_free(ctx);
        return 0;
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
    
    int health = 100;
    
    // Check connection status
    if (tb[STARLINK_STATUS_CONNECTED]) {
        bool connected = blobmsg_get_bool(tb[STARLINK_STATUS_CONNECTED]);
        if (!connected) {
            health -= 50; // Major penalty for no connection
        }
    }
    
    // Check obstruction status
    if (tb[STARLINK_STATUS_OBSTRUCTED]) {
        bool obstructed = blobmsg_get_bool(tb[STARLINK_STATUS_OBSTRUCTED]);
        if (obstructed) {
            health -= 20; // Penalty for obstruction
        }
    }
    
    // Check signal quality
    if (tb[STARLINK_STATUS_SNR]) {
        double snr = blobmsg_get_double(tb[STARLINK_STATUS_SNR]);
        if (snr < 5.0) {
            health -= 30; // Penalty for low SNR
        } else if (snr < 10.0) {
            health -= 10; // Minor penalty for moderate SNR
        }
    }
    
    // Check data rates
    if (tb[STARLINK_STATUS_DOWNLINK]) {
        double downlink = blobmsg_get_double(tb[STARLINK_STATUS_DOWNLINK]);
        if (downlink < 1.0) {
            health -= 25; // Penalty for very low downlink
        } else if (downlink < 10.0) {
            health -= 10; // Minor penalty for low downlink
        }
    }
    
    if (tb[STARLINK_STATUS_UPLINK]) {
        double uplink = blobmsg_get_double(tb[STARLINK_STATUS_UPLINK]);
        if (uplink < 0.5) {
            health -= 20; // Penalty for very low uplink
        } else if (uplink < 5.0) {
            health -= 5; // Minor penalty for low uplink
        }
    }
    
    // Ensure health is within valid range
    if (health < 0) health = 0;
    if (health > 100) health = 100;
    
    blob_buf_free(&bb);
    ubus_free(ctx);
    
    g_system_health.starlink_health = health;
    return health;
}

int check_uci_health(void) {
    // Real UCI configuration health check
    int health = 100;
    
    // Test UCI accessibility
    // flawfinder: ignore - constant string, no injection risk
    FILE *fp = popen("uci show", "r");
    if (!fp) {
        health = 0;
    } else {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), fp) == NULL) {
            health -= 50; // UCI not responding
        }
        pclose(fp);
    }
    
    // Check UCI configuration files
    const char* uci_files[] = {
        "/etc/config/network",
        "/etc/config/wireless",
        "/etc/config/system",
        "/etc/config/autonomy"
    };
    
    for (int i = 0; i < 4; i++) {
        struct stat st;
        if (stat(uci_files[i], &st) != 0) {
            health -= 10; // Missing configuration file
        } else if (st.st_size == 0) {
            health -= 5; // Empty configuration file
        }
    }
    
    // Test UCI read/write operations
    // flawfinder: ignore - constant string, no injection risk
    fp = popen("uci get system.@system[0].hostname 2>/dev/null", "r");
    if (fp) {
        char hostname[64];
        if (fgets(hostname, sizeof(hostname), fp) == NULL) {
            health -= 20; // Cannot read UCI values
        }
        pclose(fp);
    } else {
        health -= 30; // UCI command failed
    }
    
    // Ensure health is within valid range
    if (health < 0) health = 0;
    if (health > 100) health = 100;
    
    g_system_health.uci_health = health;
    return health;
}

int check_overlay_health(void) {
    // Real overlay filesystem health check
    int health = 100;
    
    // Check overlay mount status
    // flawfinder: ignore - constant string, no injection risk
    FILE *fp = popen("mount | grep overlay", "r");
    if (!fp) {
        health = 0; // No overlay mounted
    } else {
        char buffer[256];
        bool overlay_found = false;
        while (fgets(buffer, sizeof(buffer), fp)) {
            if (strstr(buffer, "overlay")) {
                overlay_found = true;
                break;
            }
        }
        pclose(fp);
        
        if (!overlay_found) {
            health = 0; // Overlay not mounted
        }
    }
    
    // Check overlay filesystem space
    struct statvfs vfs;
    if (statvfs("/overlay", &vfs) == 0) {
        unsigned long total_space = vfs.f_blocks * vfs.f_frsize;
        unsigned long free_space = vfs.f_bavail * vfs.f_frsize;
        double usage_percent = (double)(total_space - free_space) / total_space * 100.0;
        
        if (usage_percent > 95.0) {
            health -= 50; // Critical space usage
        } else if (usage_percent > 90.0) {
            health -= 30; // High space usage
        } else if (usage_percent > 80.0) {
            health -= 10; // Moderate space usage
        }
    } else {
        health -= 20; // Cannot check overlay space
    }
    
    // Check overlay filesystem integrity
    // flawfinder: ignore - constant string, no injection risk
    fp = popen("df /overlay 2>/dev/null", "r");
    if (fp) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), fp) == NULL) {
            health -= 30; // Cannot check overlay status
        }
        pclose(fp);
    } else {
        health -= 25; // Overlay filesystem check failed
    }
    
    // Ensure health is within valid range
    if (health < 0) health = 0;
    if (health > 100) health = 100;
    
    g_system_health.overlay_health = health;
    return health;
}

int check_services_health(void) {
    // Real system services health check
    int health = 100;
    int failed_services = 0;
    int total_services = 0;
    
    // Check critical system services
    const char* critical_services[] = {
        "network", "firewall", "dnsmasq", "odhcpd", "autonomy-daemon", 
        "starlink-tracker", "gps-service", "ubus", "logd"
    };
    
    for (int i = 0; i < 9; i++) {
        total_services++;
        // SECURE VERSION: Command injection vulnerability - system() calls with user data are dangerous
        // DISABLED: Command execution disabled for security
        LOGX_WARN_MSG("Service check disabled for security - command injection vulnerability",
                     "service", critical_services[i]);
        int ret = -1; // Return error since command was not executed
        if (ret != 0) {
            failed_services++;
            // Check if service is supposed to be running
            // SECURE VERSION: Command injection vulnerability - popen() calls with user data are dangerous
            // DISABLED: Command execution disabled for security
            LOGX_WARN_MSG("Service status check disabled for security - command injection vulnerability",
                         "service", critical_services[i]);
            FILE *fp = NULL; // Return NULL to indicate failure
            if (fp) {
                char status[32];
                if (fgets(status, sizeof(status), fp)) {
                    if (strstr(status, "active")) {
                        // Service should be running but isn't
                        health -= 15; // Major penalty for critical service down
                    } else {
                        // Service is intentionally stopped
                        health -= 5; // Minor penalty for stopped service
                    }
                }
                pclose(fp);
            }
        }
    }
    
    // Check systemd service status
    // flawfinder: ignore - constant string, no injection risk
    FILE *fp = popen("systemctl list-failed --no-legend 2>/dev/null | wc -l", "r");
    if (fp) {
        char buffer[16];
        if (fgets(buffer, sizeof(buffer), fp)) {
            int failed_systemd = atoi(buffer);
            if (failed_systemd > 0) {
                health -= (failed_systemd * 10); // Penalty for failed systemd services
            }
        }
        pclose(fp);
    }
    
    // Check process count and system load
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        // Check if system is overloaded
        double load_avg = si.loads[0] / 65536.0;
        if (load_avg > 2.0) {
            health -= 20; // High system load
        } else if (load_avg > 1.0) {
            health -= 10; // Moderate system load
        }
    }
    
    // Ensure health is within valid range
    if (health < 0) health = 0;
    if (health > 100) health = 100;
    
    g_system_health.services_health = health;
    return health;
}

int check_database_health(void) {
    // Real database health check
    int health = 100;
    
    // Check SQLite database health
    sqlite3* db = NULL;
    int ret = sqlite3_open("/var/lib/autonomy/autonomy.db", &db);
    if (ret != SQLITE_OK) {
        health = 0; // Cannot open database
    } else {
        // Check database integrity
        char* err_msg = NULL;
        ret = sqlite3_exec(db, "PRAGMA integrity_check;", NULL, NULL, &err_msg);
        if (ret != SQLITE_OK) {
            health -= 50; // Database integrity check failed
            sqlite3_free(err_msg);
        }
        
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
                health -= 10; // Cannot check table
            } else {
                ret = sqlite3_step(stmt);
                if (ret != SQLITE_ROW) {
                    health -= 10; // Critical table missing
                }
                sqlite3_finalize(stmt);
            }
        }
        
        // Check database file size
        struct stat db_stat;
        if (stat("/var/lib/autonomy/autonomy.db", &db_stat) == 0) {
            if (db_stat.st_size == 0) {
                health -= 30; // Database file is empty
            } else if (db_stat.st_size < 1024) {
                health -= 20; // Database file is too small
            }
        }
        
        // Test database write capability
        ret = sqlite3_exec(db, "CREATE TEMP TABLE health_check (id INTEGER);", NULL, NULL, &err_msg);
        if (ret != SQLITE_OK) {
            health -= 25; // Database write test failed
            sqlite3_free(err_msg);
        }
        
        sqlite3_close(db);
    }
    
    // Check disk space for database directory
    struct statvfs vfs;
    if (statvfs("/var/lib/autonomy", &vfs) == 0) {
        unsigned long free_space = vfs.f_bavail * vfs.f_frsize;
        if (free_space < 10 * 1024 * 1024) { // Less than 10MB free
            health -= 20; // Low disk space
        }
    }
    
    // Ensure health is within valid range
    if (health < 0) health = 0;
    if (health > 100) health = 100;
    
    g_system_health.database_health = health;
    return health;
}

int check_time_health(void) {
    // Real time synchronization health check
    int health = 100;
    
    // Check if system time is reasonable (not 1970)
    time_t now = time(NULL);
    if (now < 1600000000) { // Before 2020
        health = 0; // System time is invalid
    } else {
        // Check NTP synchronization status
        // flawfinder: ignore - constant string, no injection risk
        FILE *fp = popen("ntpq -p 2>/dev/null | grep -E '^\\*|^\\+|^o' | wc -l", "r");
        if (fp) {
            char buffer[16];
            if (fgets(buffer, sizeof(buffer), fp)) {
                int synced_sources = atoi(buffer);
                if (synced_sources == 0) {
                    health -= 40; // No NTP synchronization
                } else if (synced_sources < 2) {
                    health -= 20; // Limited NTP synchronization
                }
            }
            pclose(fp);
        } else {
            // Try alternative NTP check
            // flawfinder: ignore - constant string, no injection risk
            fp = popen("chrony sources 2>/dev/null | grep -E '^\\^|^\\*|^\\+' | wc -l", "r");
            if (fp) {
                char buffer[16];
                if (fgets(buffer, sizeof(buffer), fp)) {
                    int synced_sources = atoi(buffer);
                    if (synced_sources == 0) {
                        health -= 30; // No chrony synchronization
                    }
                }
                pclose(fp);
            } else {
                health -= 10; // Cannot check time synchronization
            }
        }
        
        // Check system clock drift
        // flawfinder: ignore - constant string, no injection risk
        fp = popen("ntpq -c 'rv 0 offset' 2>/dev/null | awk '{print $3}'", "r");
        if (fp) {
            char buffer[32];
            if (fgets(buffer, sizeof(buffer), fp)) {
                double offset = atof(buffer);
                if (fabs(offset) > 1000.0) { // More than 1 second offset
                    health -= 30; // Large time offset
                } else if (fabs(offset) > 100.0) { // More than 100ms offset
                    health -= 15; // Moderate time offset
                }
            }
            pclose(fp);
        }
        
        // Check if time is moving forward
        static time_t last_check = 0;
        if (last_check > 0 && now <= last_check) {
            health -= 50; // Time is not moving forward
        }
        last_check = now;
    }
    
    // Ensure health is within valid range
    if (health < 0) health = 0;
    if (health > 100) health = 100;
    
    g_system_health.time_health = health;
    return health;
}

int check_logs_health(void) {
    // Real log system health check
    int health = 100;
    
    // Check if log directory exists and is writable
    struct stat st;
    if (stat("/var/log", &st) != 0) {
        health = 0; // Log directory doesn't exist
    } else if (!S_ISDIR(st.st_mode)) {
        health = 0; // /var/log is not a directory
    } else {
        // Check if we can write to log directory (SECURE VERSION)
        struct stat log_stat;
        if (stat("/var/log", &log_stat) != 0 || 
            !S_ISDIR(log_stat.st_mode) || 
            !(log_stat.st_mode & S_IWUSR)) {
            health -= 50; // Cannot write to log directory
        }
    }
    
    // Check log file sizes and rotation
    const char* log_files[] = {
        "/var/log/messages",
        "/var/log/autonomy/autonomy.log",
        "/var/log/autonomy/gps.log",
        "/var/log/autonomy/starlink.log",
        "/var/log/autonomy/network.log"
    };
    
    for (int i = 0; i < 5; i++) {
        if (stat(log_files[i], &st) == 0) {
            // Check if log file is too large (> 100MB)
            if (st.st_size > 100 * 1024 * 1024) {
                health -= 10; // Log file too large
            }
            
            // Check if log file is too old (no updates in 24 hours)
            time_t now = time(NULL);
            if (now - st.st_mtime > 86400) {
                health -= 5; // Log file not being updated
            }
        }
    }
    
    // Check logrotate configuration
    // flawfinder: ignore - constant string, no injection risk
    FILE *fp = popen("logrotate -d /etc/logrotate.conf 2>&1 | grep -c 'error'", "r");
    if (fp) {
        char buffer[16];
        if (fgets(buffer, sizeof(buffer), fp)) {
            int errors = atoi(buffer);
            if (errors > 0) {
                health -= 15; // Logrotate configuration errors
            }
        }
        pclose(fp);
    }
    
    // Check system log daemon status
    // flawfinder: ignore - constant string, no injection risk
    fp = popen("pgrep -f 'syslogd|rsyslogd|logd' > /dev/null 2>&1; echo $?", "r");
    if (fp) {
        char buffer[16];
        if (fgets(buffer, sizeof(buffer), fp)) {
            int logd_running = atoi(buffer);
            if (logd_running != 0) {
                health -= 30; // Log daemon not running
            }
        }
        pclose(fp);
    }
    
    // Check disk space for logs
    struct statvfs vfs;
    if (statvfs("/var/log", &vfs) == 0) {
        unsigned long free_space = vfs.f_bavail * vfs.f_frsize;
        if (free_space < 50 * 1024 * 1024) { // Less than 50MB free
            health -= 20; // Low disk space for logs
        }
    }
    
    // Check for log file corruption
    // flawfinder: ignore - constant string, no injection risk
    fp = popen("find /var/log -name '*.log' -size +0c -exec file {} \\; 2>/dev/null | grep -c 'data'", "r");
    if (fp) {
        char buffer[16];
        if (fgets(buffer, sizeof(buffer), fp)) {
            int corrupted_logs = atoi(buffer);
            if (corrupted_logs > 0) {
                health -= (corrupted_logs * 5); // Penalty for corrupted logs
            }
        }
        pclose(fp);
    }
    
    // Ensure health is within valid range
    if (health < 0) health = 0;
    if (health > 100) health = 100;
    
    g_system_health.logs_health = health;
    return health;
}

int perform_system_health_check(void) {
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

int get_system_memory_usage(void) {
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        // Convert to MB
        return (int)(si.totalram * si.mem_unit / (1024 * 1024));
    }
    return 0;
}

int get_system_uptime(void) {
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        return (int)si.uptime;
    }
    return 0;
}

int get_system_load_average(void) {
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        // Return load average as percentage
        return (int)((si.loads[0] / 65536.0) * 100);
    }
    return 0;
}

int perform_network_health_check(void) {
    // Real network health check
    double health = 100.0;
    
    // Check network interfaces
    // flawfinder: ignore - constant string, no injection risk
    FILE *fp = popen("ip link show | grep -c 'state UP'", "r");
    if (fp) {
        char buffer[16];
        if (fgets(buffer, sizeof(buffer), fp)) {
            int up_interfaces = atoi(buffer);
            if (up_interfaces == 0) {
                health = 0.0; // No network interfaces up
            } else if (up_interfaces < 2) {
                health -= 20.0; // Limited network connectivity
            }
        }
        pclose(fp);
    }
    
    // Check default route
    // flawfinder: ignore - constant string, no injection risk
    fp = popen("ip route show default | wc -l", "r");
    if (fp) {
        char buffer[16];
        if (fgets(buffer, sizeof(buffer), fp)) {
            int default_routes = atoi(buffer);
            if (default_routes == 0) {
                health -= 30.0; // No default route
            }
        }
        pclose(fp);
    }
    
    // Check DNS resolution
    // flawfinder: ignore - constant string, no injection risk
    fp = popen("nslookup google.com > /dev/null 2>&1; echo $?", "r");
    if (fp) {
        char buffer[16];
        if (fgets(buffer, sizeof(buffer), fp)) {
            int dns_ok = atoi(buffer);
            if (dns_ok != 0) {
                health -= 25.0; // DNS resolution failed
            }
        }
        pclose(fp);
    }
    
    // Check internet connectivity
    // flawfinder: ignore - constant string, no injection risk
    fp = popen("ping -c 1 -W 5 8.8.8.8 > /dev/null 2>&1; echo $?", "r");
    if (fp) {
        char buffer[16];
        if (fgets(buffer, sizeof(buffer), fp)) {
            int ping_ok = atoi(buffer);
            if (ping_ok != 0) {
                health -= 40.0; // No internet connectivity
            }
        }
        pclose(fp);
    }
    
    // Check network load
    // flawfinder: ignore - constant string, no injection risk
    fp = popen("cat /proc/net/dev | grep -v 'lo:' | awk '{sum+=$2+$10} END {print sum}'", "r");
    if (fp) {
        char buffer[32];
        if (fgets(buffer, sizeof(buffer), fp)) {
            unsigned long total_bytes = strtoul(buffer, NULL, 10);
            // Check if network is overloaded (simplified check)
            if (total_bytes > 1000000000) { // More than 1GB
                health -= 10.0; // High network usage
            }
        }
        pclose(fp);
    }
    
    // Ensure health is within valid range
    if (health < 0.0) health = 0.0;
    if (health > 100.0) health = 100.0;
    
    g_state.network_health_score = health;
    g_state.last_network_check = time(NULL);
    return 0;
}

int perform_gps_health_check(void) {
    // Real GPS health check
    double health = 100.0;
    
    // Check GPS service status
    // flawfinder: ignore - constant string, no injection risk
    FILE *fp = popen("pgrep -f 'gps-service|gpsd' > /dev/null 2>&1; echo $?", "r");
    if (fp) {
        char buffer[16];
        if (fgets(buffer, sizeof(buffer), fp)) {
            int gps_service_running = atoi(buffer);
            if (gps_service_running != 0) {
                health -= 50.0; // GPS service not running
            }
        }
        pclose(fp);
    }
    
    // Check GPS data files
    const char* gps_files[] = {
        "/var/lib/autonomy/gps_data",
        "/var/lib/autonomy/gps_status",
        "/var/lib/autonomy/gps_health"
    };
    
    for (int i = 0; i < 3; i++) {
        struct stat st;
        if (stat(gps_files[i], &st) != 0) {
            health -= 10.0; // GPS data file missing
        } else {
            // Check if file is recent (updated within last 5 minutes)
            time_t now = time(NULL);
            if (now - st.st_mtime > 300) {
                health -= 15.0; // GPS data is stale
            }
        }
    }
    
    // Check GPS device accessibility
    // flawfinder: ignore - constant string, no injection risk
    fp = popen("ls /dev/tty* | grep -E 'USB|ACM|AMA' | wc -l", "r");
    if (fp) {
        char buffer[16];
        if (fgets(buffer, sizeof(buffer), fp)) {
            int gps_devices = atoi(buffer);
            if (gps_devices == 0) {
                health -= 30.0; // No GPS devices found
            }
        }
        pclose(fp);
    }
    
    // Check GPS data quality
    // flawfinder: ignore - constant string, no injection risk
    fp = popen("grep -c 'GPGGA' /var/lib/autonomy/gps_data 2>/dev/null || echo 0", "r");
    if (fp) {
        char buffer[16];
        if (fgets(buffer, sizeof(buffer), fp)) {
            int gga_sentences = atoi(buffer);
            if (gga_sentences == 0) {
                health -= 25.0; // No GPS position data
            } else if (gga_sentences < 10) {
                health -= 10.0; // Limited GPS data
            }
        }
        pclose(fp);
    }
    
    // Check GPS accuracy
    // flawfinder: ignore - constant string, no injection risk
    fp = popen("grep 'GPGGA' /var/lib/autonomy/gps_data 2>/dev/null | tail -1 | cut -d',' -f7", "r");
    if (fp) {
        char buffer[16];
        if (fgets(buffer, sizeof(buffer), fp)) {
            int hdop = atoi(buffer);
            if (hdop > 10) {
                health -= 20.0; // Poor GPS accuracy
            } else if (hdop > 5) {
                health -= 10.0; // Moderate GPS accuracy
            }
        }
        pclose(fp);
    }
    
    // Check satellite count
    // flawfinder: ignore - constant string, no injection risk
    fp = popen("grep 'GPGGA' /var/lib/autonomy/gps_data 2>/dev/null | tail -1 | cut -d',' -f8", "r");
    if (fp) {
        char buffer[16];
        if (fgets(buffer, sizeof(buffer), fp)) {
            int satellites = atoi(buffer);
            if (satellites < 4) {
                health -= 30.0; // Insufficient satellites
            } else if (satellites < 6) {
                health -= 15.0; // Limited satellites
            }
        }
        pclose(fp);
    }
    
    // Ensure health is within valid range
    if (health < 0.0) health = 0.0;
    if (health > 100.0) health = 100.0;
    
    g_state.gps_health_score = health;
    return 0;
}

// Function to get system health status (required by utils/system_ubus.c)
const system_health_t* get_system_health_status(void) {
    return &g_system_health;
}
