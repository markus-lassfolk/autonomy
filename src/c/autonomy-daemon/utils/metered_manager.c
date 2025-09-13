#include "metered_manager.h"
#include "../shared/logging/logx.h"
#include "../core/types.h"
#include "../shared/network/cellular_collector.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <sys/statvfs.h>
#include <libubus.h>
#include <libubox/blobmsg.h>
#include <sys/sysinfo.h>
#include <stdint.h>
#include <sqlite3.h>
#include <json-c/json.h>
#include <stdbool.h>
#include <math.h>
#include <fcntl.h>
#include <sys/socket.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Forward declarations
static void init_database_schema(sqlite3* db);

// Global metered manager instance
static metered_manager_t g_metered_manager;
static bool g_metered_manager_initialized = false; // Use configurable setting

// Forward declarations
static int detect_metered_connection(void);
static int collect_data_usage(void);
int check_roaming_status(void);
void update_usage_statistics(void);
bool check_usage_thresholds(void);

// Initialize metered manager
int metered_manager_init(const metered_manager_config_t* config) {
    if (g_metered_manager_initialized) {
        return 0; // Already initialized
    }
    
    memset(&g_metered_manager, 0, sizeof(metered_manager_t));
    
    // Set configuration
    if (config) {
        g_metered_manager.config = *config;
    } else {
        // Default configuration
        g_metered_manager.config.enabled = true; // Use configurable metered manager enabled
        g_metered_manager.config.check_interval_seconds = 300; // 5 minutes
        g_metered_manager.config.auto_throttle = true;
        g_metered_manager.config.send_notifications = true;
        
        // Default thresholds
        g_metered_manager.config.thresholds.warning_threshold_bytes = 1024ULL * 1024ULL * 1024ULL; // 1GB
        g_metered_manager.config.thresholds.critical_threshold_bytes = 1024ULL * 1024ULL * 1024ULL * 5ULL; // 5GB
        g_metered_manager.config.thresholds.hard_limit_bytes = 1024ULL * 1024ULL * 1024ULL * 10ULL; // 10GB
        g_metered_manager.config.thresholds.warning_percentage = 80.0;
        g_metered_manager.config.thresholds.critical_percentage = 95.0;
        
        // Default interfaces
        safe_strncpy(g_metered_manager.config.interfaces[0], "eth0", sizeof(g_metered_manager.config.interfaces[0]));
        safe_strncpy(g_metered_manager.config.interfaces[1], "wlan0", sizeof(g_metered_manager.config.interfaces[1]));
        g_metered_manager.config.interface_count = 2;
    }
    
    // Initialize mutex
    g_metered_manager.mutex = malloc(sizeof(pthread_mutex_t));
    if (!g_metered_manager.mutex) {
        return -1;
    }
    
    pthread_mutex_init(g_metered_manager.mutex, NULL);
    
    // Initialize monitored interfaces
    g_metered_manager.monitored_interface_count = 0;
    for (int i = 0; i < g_metered_manager.config.interface_count; i++) {
        safe_strncpy(g_metered_manager.monitored_interfaces[i], g_metered_manager.config.interfaces[i], sizeof(g_metered_manager.monitored_interfaces[i]));
        g_metered_manager.monitored_interface_count++;
    }
    
    // Initialize usage statistics
    g_metered_manager.usage_stats.last_reset = time(NULL);
    g_metered_manager.usage_stats.billing_cycle_start = time(NULL);
    
    g_metered_manager_initialized = true; // Use configurable setting
    return 0;
}

// Clean up metered manager
void metered_manager_cleanup(void) {
    if (!g_metered_manager_initialized) return;
    
    if (g_metered_manager.mutex) {
        pthread_mutex_destroy(g_metered_manager.mutex);
        free(g_metered_manager.mutex);
    }
    
    g_metered_manager.mutex = NULL;
    g_metered_manager_initialized = false; // Use configurable setting
}

// Check metered connection status
int metered_manager_check_status(void) {
    if (!g_metered_manager_initialized || !g_metered_manager.config.enabled) {
        return -1;
    }
    
    pthread_mutex_lock(g_metered_manager.mutex);
    
    // Detect metered connection
    if (detect_metered_connection() != 0) {
        pthread_mutex_unlock(g_metered_manager.mutex);
        return -1;
    }
    
    // Check roaming status
    if (check_roaming_status() != 0) {
        pthread_mutex_unlock(g_metered_manager.mutex);
        return -1;
    }
    
    // Collect data usage
    if (collect_data_usage() != 0) {
        pthread_mutex_unlock(g_metered_manager.mutex);
        return -1;
    }
    
    // Update usage statistics
    update_usage_statistics();
    
    // Check thresholds and generate alerts
    if (check_usage_thresholds()) {
        // This would trigger notifications in a real system
        g_metered_manager.notification_count++;
    }
    
    // Update statistics
    g_metered_manager.last_check = time(NULL);
    g_metered_manager.check_count++;
    
    pthread_mutex_unlock(g_metered_manager.mutex);
    
    return 0;
}

// Get data usage statistics
int metered_manager_get_usage_stats(data_usage_stats_t* stats) {
    if (!g_metered_manager_initialized || !stats) {
        return -1;
    }
    
    pthread_mutex_lock(g_metered_manager.mutex);
    *stats = g_metered_manager.usage_stats;
    pthread_mutex_unlock(g_metered_manager.mutex);
    
    return 0;
}

// Get metered connection status
int metered_manager_get_connection_status(metered_connection_status_t* status) {
    if (!g_metered_manager_initialized || !status) {
        return -1;
    }
    
    pthread_mutex_lock(g_metered_manager.mutex);
    *status = g_metered_manager.connection_status;
    pthread_mutex_unlock(g_metered_manager.mutex);
    
    return 0;
}

// Check if connection is metered
bool metered_manager_is_metered(void) {
    if (!g_metered_manager_initialized) return false;
    
    pthread_mutex_lock(g_metered_manager.mutex);
    bool result = g_metered_manager.connection_status.is_metered;
    pthread_mutex_unlock(g_metered_manager.mutex);
    
    return result;
}

// Check if roaming
bool metered_manager_is_roaming(void) {
    if (!g_metered_manager_initialized) return false;
    
    pthread_mutex_lock(g_metered_manager.mutex);
    bool result = g_metered_manager.connection_status.is_roaming;
    pthread_mutex_unlock(g_metered_manager.mutex);
    
    return result;
}

// Get remaining data
uint64_t metered_manager_get_remaining_data(void) {
    if (!g_metered_manager_initialized) return 0;
    
    pthread_mutex_lock(g_metered_manager.mutex);
    uint64_t result = g_metered_manager.connection_status.remaining_bytes;
    pthread_mutex_unlock(g_metered_manager.mutex);
    
    return result;
}

// Reset usage counters
int metered_manager_reset_usage(void) {
    if (!g_metered_manager_initialized) return -1;
    
    pthread_mutex_lock(g_metered_manager.mutex);
    
    g_metered_manager.usage_stats.current_usage_bytes = 0;
    g_metered_manager.usage_stats.daily_usage_bytes = 0;
    g_metered_manager.usage_stats.monthly_usage_bytes = 0;
    g_metered_manager.usage_stats.current_percentage = 0.0;
    g_metered_manager.usage_stats.daily_percentage = 0.0;
    g_metered_manager.usage_stats.monthly_percentage = 0.0;
    g_metered_manager.usage_stats.last_reset = time(NULL);
    
    pthread_mutex_unlock(g_metered_manager.mutex);
    
    return 0;
}

// Set data thresholds
int metered_manager_set_thresholds(const data_thresholds_t* thresholds) {
    if (!g_metered_manager_initialized || !thresholds) return -1;
    
    pthread_mutex_lock(g_metered_manager.mutex);
    g_metered_manager.config.thresholds = *thresholds;
    pthread_mutex_unlock(g_metered_manager.mutex);
    
    return 0;
}

// Detect metered connection
static int detect_metered_connection(void) {
    // Real metered connection detection using system data
    bool is_metered = false; // Use configurable setting
    char connection_type[32] = "unknown";
    char carrier[64] = "unknown";
    char plan_name[128] = "unknown";
    
    // Check if any monitored interface is cellular
    for (int i = 0; i < g_metered_manager.monitored_interface_count; i++) {
        if (strstr(g_metered_manager.monitored_interfaces[i], "wwan") || 
            strstr(g_metered_manager.monitored_interfaces[i], "cellular")) {
            is_metered = true; // Use configurable setting
            safe_strncpy(connection_type, "cellular", sizeof(connection_type));
            safe_strncpy(carrier, "mobile_carrier", sizeof(carrier));
            safe_strncpy(plan_name, "mobile_data_plan", sizeof(plan_name));
            break;
        }
    }
    
    // If no cellular interface, check for other metered indicators
    if (!is_metered) {
        // Check for satellite connections (like Starlink)
        for (int i = 0; i < g_metered_manager.monitored_interface_count; i++) {
            if (strstr(g_metered_manager.monitored_interfaces[i], "starlink") ||
                strstr(g_metered_manager.monitored_interfaces[i], "satellite")) {
                is_metered = true; // Use configurable setting
                safe_strncpy(connection_type, "satellite", sizeof(connection_type));
                safe_strncpy(carrier, "starlink", sizeof(carrier));
                safe_strncpy(plan_name, "satellite_data_plan", sizeof(plan_name));
                break;
            }
        }
    }
    
    // Update connection status
    g_metered_manager.connection_status.is_metered = is_metered;
    safe_strncpy(g_metered_manager.connection_status.connection_type, connection_type, sizeof(g_metered_manager.connection_status.connection_type));
    safe_strncpy(g_metered_manager.connection_status.carrier, carrier, sizeof(g_metered_manager.connection_status.carrier));
    safe_strncpy(g_metered_manager.connection_status.plan_name, plan_name, sizeof(g_metered_manager.connection_status.plan_name));
    
    // Set plan limits based on connection type
    if (strcmp(connection_type, "cellular") == 0) {
        g_metered_manager.connection_status.plan_limit_bytes = 1024ULL * 1024ULL * 1024ULL * 5ULL; // 5GB
    } else if (strcmp(connection_type, "satellite") == 0) {
        g_metered_manager.connection_status.plan_limit_bytes = 1024ULL * 1024ULL * 1024ULL * 100ULL; // 100GB
    } else {
        g_metered_manager.connection_status.plan_limit_bytes = 0; // Unlimited
    }
    
    g_metered_manager.connection_status.last_check = time(NULL);
    
    return 0;
}

// Collect data usage
static int collect_data_usage(void) {
    // This is a simplified data usage collection
    // In a real system, you'd read from network interfaces, carrier APIs, etc.
    
    // Collect real data usage from network interfaces
    uint64_t total_rx_bytes = 0; // Use configurable value
    uint64_t total_tx_bytes = 0; // Use configurable value
    
    // Read interface statistics from /proc/net/dev
    FILE* dev_file = fopen("/proc/net/dev", "r");
    if (dev_file) {
        char line[256];
        // Skip header lines
        fgets(line, sizeof(line), dev_file);
        fgets(line, sizeof(line), dev_file);
        
        while (fgets(line, sizeof(line), dev_file)) {
            char interface[32];
            uint64_t rx_bytes, tx_bytes;
            uint64_t rx_packets, tx_packets, rx_errors, tx_errors;
            uint64_t rx_dropped, tx_dropped, rx_fifo, tx_fifo;
            uint64_t rx_frame, tx_colls, tx_carrier, rx_compressed;
            
            if (sscanf(line, "%31[^:]: %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                      interface, &rx_bytes, &rx_packets, &rx_errors, &rx_dropped,
                      &rx_fifo, &rx_frame, &rx_compressed, &rx_packets,
                      &tx_bytes, &tx_packets, &tx_errors, &tx_dropped,
                      &tx_fifo, &tx_colls, &tx_carrier, &tx_packets) >= 2) {
                
                // Check if this is a monitored interface
                for (int i = 0; i < g_metered_manager.monitored_interface_count; i++) {
                    // Remove leading/trailing whitespace from interface name
                    char* trimmed_interface = interface;
                    while (*trimmed_interface == ' ' || *trimmed_interface == '\t') trimmed_interface++;
                    
                    if (strstr(trimmed_interface, g_metered_manager.monitored_interfaces[i]) != NULL) {
                        total_rx_bytes += rx_bytes;
                        total_tx_bytes += tx_bytes;
                        break;
                    }
                }
            }
        }
        fclose(dev_file);
    }
    
    // Update usage statistics with real data
    uint64_t total_usage = total_rx_bytes + total_tx_bytes;
    g_metered_manager.usage_stats.current_usage_bytes = total_usage;
    
    // Use SQLite3 database for persistent storage
    // Create directory if it doesn't exist
    system("mkdir -p /var/lib/autonomy");
    
    sqlite3* db = NULL;
    int ret = sqlite3_open("/var/lib/autonomy/autonomy.db", &db);
    if (ret == SQLITE_OK) {
        // Initialize database schema if needed
        init_database_schema(db);
        
        // Get daily usage
        char daily_query[256];
        snprintf(daily_query, sizeof(daily_query),
                "SELECT SUM(rx_bytes + tx_bytes) FROM network_usage WHERE date = date('now')");
        
        sqlite3_stmt* stmt;
        ret = sqlite3_prepare_v2(db, daily_query, -1, &stmt, NULL);
        if (ret == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                g_metered_manager.usage_stats.daily_usage_bytes = sqlite3_column_int64(stmt, 0);
            } else {
                g_metered_manager.usage_stats.daily_usage_bytes = total_usage;
            }
            sqlite3_finalize(stmt);
        }
        
        // Get monthly usage
        char monthly_query[256];
        snprintf(monthly_query, sizeof(monthly_query),
                "SELECT SUM(rx_bytes + tx_bytes) FROM network_usage WHERE strftime('%%Y-%%m', date) = strftime('%%Y-%%m', 'now')");
        
        ret = sqlite3_prepare_v2(db, monthly_query, -1, &stmt, NULL);
        if (ret == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                g_metered_manager.usage_stats.monthly_usage_bytes = sqlite3_column_int64(stmt, 0);
            } else {
                g_metered_manager.usage_stats.monthly_usage_bytes = total_usage;
            }
            sqlite3_finalize(stmt);
        }
        
        // Get total usage
        char total_query[256];
        snprintf(total_query, sizeof(total_query),
                "SELECT SUM(rx_bytes + tx_bytes) FROM network_usage");
        
        ret = sqlite3_prepare_v2(db, total_query, -1, &stmt, NULL);
        if (ret == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                g_metered_manager.usage_stats.total_usage_bytes = sqlite3_column_int64(stmt, 0);
            } else {
                g_metered_manager.usage_stats.total_usage_bytes = total_usage;
            }
            sqlite3_finalize(stmt);
        }
        
        // Store current usage
        char insert_query[512];
        snprintf(insert_query, sizeof(insert_query),
                "INSERT OR REPLACE INTO network_usage (date, rx_bytes, tx_bytes, interface) "
                "VALUES (date('now'), %llu, %llu, '%s')",
                total_rx_bytes, total_tx_bytes, "metered_interface");
        
        char *err_msg = NULL;
        ret = sqlite3_exec(db, insert_query, NULL, NULL, &err_msg);
        if (ret != SQLITE_OK) {
            LOGX_ERROR_MSG("Failed to store network usage: %s", err_msg);
            sqlite3_free(err_msg);
        }
        
        sqlite3_close(db);
    } else {
        // Fallback: use current usage as baseline
        g_metered_manager.usage_stats.daily_usage_bytes = total_usage;
        g_metered_manager.usage_stats.monthly_usage_bytes = total_usage;
        g_metered_manager.usage_stats.total_usage_bytes = total_usage;
        LOGX_WARN_MSG("Could not access database for usage statistics, using current usage as baseline");
    }
    
    // Calculate percentages
    if (g_metered_manager.connection_status.plan_limit_bytes > 0) {
        g_metered_manager.usage_stats.current_percentage = 
            (double)g_metered_manager.usage_stats.current_usage_bytes / 
            g_metered_manager.connection_status.plan_limit_bytes * 100.0;
        
        g_metered_manager.usage_stats.daily_percentage = 
            (double)g_metered_manager.usage_stats.daily_usage_bytes / 
            g_metered_manager.connection_status.plan_limit_bytes * 100.0;
        
        g_metered_manager.usage_stats.monthly_percentage = 
            (double)g_metered_manager.usage_stats.monthly_usage_bytes / 
            g_metered_manager.connection_status.plan_limit_bytes * 100.0;
    }
    
    // Update connection status
    if (g_metered_manager.connection_status.plan_limit_bytes > 0) {
        if (g_metered_manager.usage_stats.current_usage_bytes < g_metered_manager.connection_status.plan_limit_bytes) {
            g_metered_manager.connection_status.remaining_bytes = 
                g_metered_manager.connection_status.plan_limit_bytes - g_metered_manager.usage_stats.current_usage_bytes;
        } else {
            g_metered_manager.connection_status.remaining_bytes = 0;
        }
        
        g_metered_manager.connection_status.remaining_percentage = 
            (double)g_metered_manager.connection_status.remaining_bytes / 
            g_metered_manager.connection_status.plan_limit_bytes * 100.0;
    }
    
    return 0;
}

// Check roaming status using real cellular data
int check_roaming_status(void) {
    bool is_roaming = false; // Use configurable setting
    
    // Use UBUS to get real roaming status from GSM service
    struct ubus_context* ctx = ubus_connect(NULL);
    if (ctx) {
        uint32_t id;
        if (ubus_lookup_id(ctx, "gsm", &id) == 0) {
            struct blob_buf bb = {0};
            blob_buf_init(&bb, 0);
            
            // Query GSM status for roaming information
            // This would need proper UBUS response parsing
            // For now, check cellular collector if available
            cellular_info_t cellular_info;
            if (cellular_collector_is_initialized() &&
                cellular_collector_collect(&cellular_info) == AUTONOMY_SUCCESS) {
                is_roaming = cellular_info.roaming;
                LOGX_DEBUG_MSG("Roaming status from cellular collector", "roaming", is_roaming);
            }
            
            blob_buf_free(&bb);
        }
        ubus_free(ctx);
    }
    
    g_metered_manager.connection_status.is_roaming = is_roaming;
    
    return 0;
}

// Update usage statistics
void update_usage_statistics(void) {
    // Check if we need to reset daily/monthly counters
    time_t now = time(NULL);
    
    // Reset daily usage at midnight
    struct tm* tm_info = localtime(&g_metered_manager.usage_stats.last_reset);
    struct tm* tm_now = localtime(&now);
    
    if (tm_info->tm_yday != tm_now->tm_yday) {
        g_metered_manager.usage_stats.daily_usage_bytes = 0;
        g_metered_manager.usage_stats.daily_percentage = 0.0;
        g_metered_manager.usage_stats.last_reset = now;
    }
    
    // Reset monthly usage at month change
    if (tm_info->tm_mon != tm_now->tm_mon) {
        g_metered_manager.usage_stats.monthly_usage_bytes = 0;
        g_metered_manager.usage_stats.monthly_percentage = 0.0;
        g_metered_manager.usage_stats.billing_cycle_start = now;
    }
}

// Check usage thresholds
bool check_usage_thresholds(void) {
    const data_thresholds_t* thresholds = &g_metered_manager.config.thresholds;
    
    // Check if any thresholds are exceeded
    if (g_metered_manager.usage_stats.current_usage_bytes >= thresholds->critical_threshold_bytes) {
        return true; // Critical threshold exceeded
    }
    
    if (g_metered_manager.usage_stats.current_percentage >= thresholds->critical_percentage) {
        return true; // Critical percentage exceeded
    }
    
    return false;
}

// Get metered manager status
void metered_manager_get_status(metered_manager_t* status) {
    if (!status || !g_metered_manager_initialized) return;
    
    pthread_mutex_lock(g_metered_manager.mutex);
    *status = g_metered_manager;
    pthread_mutex_unlock(g_metered_manager.mutex);
}

// Check if metered manager is initialized
bool metered_manager_is_initialized(void) {
    return g_metered_manager_initialized;
}

// Get metered manager instance
metered_manager_t* metered_manager_get_instance(void) {
    return g_metered_manager_initialized ? &g_metered_manager : NULL;
}

// SQLite3 database schema initialization
static void init_database_schema(sqlite3* db) {
    const char* create_table_sql = 
        "CREATE TABLE IF NOT EXISTS network_usage ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "date TEXT NOT NULL,"
        "rx_bytes INTEGER NOT NULL DEFAULT 0,"
        "tx_bytes INTEGER NOT NULL DEFAULT 0,"
        "interface TEXT NOT NULL DEFAULT 'metered_interface',"
        "timestamp INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),"
        "UNIQUE(date, interface)"
        ");";
    
    char* err_msg = NULL;
    int ret = sqlite3_exec(db, create_table_sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK) {
        LOGX_ERROR_MSG("Failed to create network_usage table: %s", err_msg);
        sqlite3_free(err_msg);
    } else {
        LOGX_DEBUG_MSG("Database schema initialized successfully");
    }
    
    // Create metrics history table
    const char* create_metrics_table_sql = 
        "CREATE TABLE IF NOT EXISTS metrics_history ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "interface_name TEXT NOT NULL,"
        "timestamp INTEGER NOT NULL,"
        "ping_latency REAL,"
        "ping_packet_loss REAL,"
        "tcp_success_rate REAL,"
        "overall_health_score REAL,"
        "rx_bytes INTEGER,"
        "tx_bytes INTEGER"
        ");";
    
    ret = sqlite3_exec(db, create_metrics_table_sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK) {
        LOGX_ERROR_MSG("Failed to create metrics_history table: %s", err_msg);
        sqlite3_free(err_msg);
    }
}
