#include "../core/types.h"
#include "system_management.h"
#include <libubus.h>
#include <libubox/blobmsg.h>
#include <libubox/blobmsg_json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// UBUS policy definitions
enum {
    MAINTENANCE_TYPE,
    MAINTENANCE_DURATION,
    MAINTENANCE_SERVICES,
    __MAINTENANCE_MAX
};

enum {
    RESTART_SERVICES,
    RESTART_FORCE,
    RESTART_TIMEOUT,
    __RESTART_MAX
};

// System status UBUS method
int autonomy_system_status(struct ubus_context *uctx, struct ubus_object *obj,
                          struct ubus_request_data *req, const char *method,
                          struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Get system information
    unsigned long total_mem, available_mem;
    unsigned long uptime;
    double load1, load5, load15;
    
    get_system_memory_usage(&total_mem, &available_mem);
    uptime = get_system_uptime();
    get_system_load_average(&load1, &load5, &load15);
    
    // Add system status information
    blobmsg_add_string(&bb, "status", "operational");
    blobmsg_add_u32(&bb, "uptime", (uint32_t)uptime);
    blobmsg_add_u64(&bb, "total_memory", total_mem);
    blobmsg_add_u64(&bb, "available_memory", available_mem);
    blobmsg_add_double(&bb, "load_1min", (float)load1);
    blobmsg_add_double(&bb, "load_5min", (float)load5);
    blobmsg_add_double(&bb, "load_15min", (float)load15);
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

// System health check UBUS method
int autonomy_system_health_check(struct ubus_context *uctx, struct ubus_object *obj,
                                struct ubus_request_data *req, const char *method,
                                struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Perform health check
    if (perform_system_health_check() == 0) {
        const system_health_t *health = get_system_health_status();
        
        blobmsg_add_string(&bb, "status", "healthy");
        blobmsg_add_u32(&bb, "overall_score", health->overall_score);
        blobmsg_add_string(&bb, "status", health->status);
        blobmsg_add_u8(&bb, "starlink_health", health->starlink_health);
        blobmsg_add_u8(&bb, "uci_health", health->uci_health);
        blobmsg_add_u8(&bb, "overlay_health", health->overlay_health);
        blobmsg_add_u8(&bb, "services_health", health->services_health);
        blobmsg_add_u8(&bb, "network_health", health->network_health);
        blobmsg_add_u8(&bb, "database_health", health->database_health);
        blobmsg_add_u8(&bb, "time_health", health->time_health);
        blobmsg_add_u8(&bb, "logs_health", health->logs_health);
    } else {
        blobmsg_add_string(&bb, "status", "error");
        blobmsg_add_string(&bb, "error", "Failed to perform health check");
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

// System health details UBUS method
int autonomy_system_health_details(struct ubus_context *uctx, struct ubus_object *obj,
                                  struct ubus_request_data *req, const char *method,
                                  struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Get detailed health information
    const system_health_t *health = get_system_health_status();
    
    // Add detailed health information
    blobmsg_add_u32(&bb, "overall_score", health->overall_score);
    blobmsg_add_string(&bb, "status", health->status);
    
    // Component health details
    struct blob_attr *components = blobmsg_open_table(&bb, "components");
    blobmsg_add_u8(&bb, "starlink", health->starlink_health);
    blobmsg_add_u8(&bb, "uci", health->uci_health);
    blobmsg_add_u8(&bb, "overlay", health->overlay_health);
    blobmsg_add_u8(&bb, "services", health->services_health);
    blobmsg_add_u8(&bb, "network", health->network_health);
    blobmsg_add_u8(&bb, "database", health->database_health);
    blobmsg_add_u8(&bb, "time", health->time_health);
    blobmsg_add_u8(&bb, "logs", health->logs_health);
    blobmsg_close_table(&bb, components);
    
    // System resource information
    unsigned long total_mem, available_mem;
    double load1, load5, load15;
    
    get_system_memory_usage(&total_mem, &available_mem);
    get_system_load_average(&load1, &load5, &load15);
    
    struct blob_attr *resources = blobmsg_open_table(&bb, "resources");
    blobmsg_add_u64(&bb, "total_memory", total_mem);
    blobmsg_add_u64(&bb, "available_memory", available_mem);
    blobmsg_add_double(&bb, "load_1min", (float)load1);
    blobmsg_add_double(&bb, "load_5min", (float)load5);
    blobmsg_add_double(&bb, "load_15min", (float)load15);
    blobmsg_close_table(&bb, resources);
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

// System maintenance UBUS method
int autonomy_system_maintenance(struct ubus_context *uctx, struct ubus_object *obj,
                               struct ubus_request_data *req, const char *method,
                               struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Perform actual system maintenance tasks
    time_t maintenance_start = time(NULL);
    bool maintenance_success = true;
    char maintenance_log[512] = "";
    
    // Parse maintenance request parameters
    struct blob_attr *tb[__MAINTENANCE_MAX];
    static const struct blobmsg_policy policy[__MAINTENANCE_MAX] = {
        [MAINTENANCE_TYPE] = { .name = "type", .type = BLOBMSG_TYPE_STRING },
        [MAINTENANCE_DURATION] = { .name = "duration", .type = BLOBMSG_TYPE_INT32 },
        [MAINTENANCE_SERVICES] = { .name = "services", .type = BLOBMSG_TYPE_ARRAY },
    };
    
    blobmsg_parse(policy, __MAINTENANCE_MAX, tb, blob_data(msg), blob_len(msg));
    
    const char* maintenance_type = "general";
    int duration_minutes = 60; // Default 1 hour
    
    if (tb[MAINTENANCE_TYPE]) {
        maintenance_type = blobmsg_get_string(tb[MAINTENANCE_TYPE]);
    }
    if (tb[MAINTENANCE_DURATION]) {
        duration_minutes = blobmsg_get_u32(tb[MAINTENANCE_DURATION]);
    }
    
    // Perform maintenance based on type
    if (strcmp(maintenance_type, "database") == 0) {
        // Database maintenance
        FILE* db_maintenance = popen("sqlite3 /var/lib/autonomy/autonomy.db 'VACUUM; ANALYZE;'", "r");
        if (db_maintenance) {
            pclose(db_maintenance);
            strcat(maintenance_log, "Database maintenance completed. ");
        } else {
            maintenance_success = false;
            strcat(maintenance_log, "Database maintenance failed. ");
        }
    } else if (strcmp(maintenance_type, "logs") == 0) {
        // Log maintenance
        FILE* log_maintenance = popen("find /var/log -name '*.log' -mtime +7 -delete", "r");
        if (log_maintenance) {
            pclose(log_maintenance);
            strcat(maintenance_log, "Log cleanup completed. ");
        } else {
            maintenance_success = false;
            strcat(maintenance_log, "Log cleanup failed. ");
        }
    } else if (strcmp(maintenance_type, "system") == 0) {
        // System maintenance
        FILE* system_maintenance = popen("apt-get update && apt-get upgrade -y", "r");
        if (system_maintenance) {
            pclose(system_maintenance);
            strcat(maintenance_log, "System update completed. ");
        } else {
            maintenance_success = false;
            strcat(maintenance_log, "System update failed. ");
        }
    } else {
        // General maintenance
        FILE* general_maintenance = popen("sync && echo 3 > /proc/sys/vm/drop_caches", "r");
        if (general_maintenance) {
            pclose(general_maintenance);
            strcat(maintenance_log, "General maintenance completed. ");
        } else {
            maintenance_success = false;
            strcat(maintenance_log, "General maintenance failed. ");
        }
    }
    
    // Update maintenance status file
    FILE* status_fp = fopen("/var/lib/autonomy/maintenance_status", "w");
    if (status_fp) {
        fprintf(status_fp, "maintenance_type=%s\n", maintenance_type);
        fprintf(status_fp, "maintenance_start=%ld\n", maintenance_start);
        fprintf(status_fp, "maintenance_duration=%d\n", duration_minutes);
        fprintf(status_fp, "maintenance_success=%s\n", maintenance_success ? "true" : "false");
        fprintf(status_fp, "maintenance_log=%s\n", maintenance_log);
        fclose(status_fp);
    }
    
    // Send response
    blobmsg_add_string(&bb, "status", maintenance_success ? "maintenance_completed" : "maintenance_failed");
    blobmsg_add_string(&bb, "message", maintenance_log);
    blobmsg_add_string(&bb, "type", maintenance_type);
    blobmsg_add_u32(&bb, "duration_minutes", duration_minutes);
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)maintenance_start);
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

// System restart services UBUS method
int autonomy_system_restart_services(struct ubus_context *uctx, struct ubus_object *obj,
                                    struct ubus_request_data *req, const char *method,
                                    struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Perform actual service restart operations
    time_t restart_start = time(NULL);
    bool restart_success = true;
    char restart_log[512] = "";
    int services_restarted = 0;
    
    // Parse restart request parameters
    struct blob_attr *tb[__RESTART_MAX];
    static const struct blobmsg_policy policy[__RESTART_MAX] = {
        [RESTART_SERVICES] = { .name = "services", .type = BLOBMSG_TYPE_ARRAY },
        [RESTART_FORCE] = { .name = "force", .type = BLOBMSG_TYPE_BOOL },
        [RESTART_TIMEOUT] = { .name = "timeout", .type = BLOBMSG_TYPE_INT32 },
    };
    
    blobmsg_parse(policy, __RESTART_MAX, tb, blob_data(msg), blob_len(msg));
    
    bool force_restart = false;
    int timeout_seconds = 30; // Default 30 seconds
    
    if (tb[RESTART_FORCE]) {
        force_restart = blobmsg_get_bool(tb[RESTART_FORCE]);
    }
    if (tb[RESTART_TIMEOUT]) {
        timeout_seconds = blobmsg_get_u32(tb[RESTART_TIMEOUT]);
    }
    
    // Get list of services to restart
    const char* default_services[] = {
        "autonomy-daemon", "starlink-tracker", "network-manager", "gps-service"
    };
    
    if (tb[RESTART_SERVICES]) {
        // Parse services from request
        struct blob_attr *cur;
        int rem;
        
        blobmsg_for_each_attr(cur, tb[RESTART_SERVICES], rem) {
            const char* service_name = blobmsg_get_string(cur);
            if (service_name) {
                // Restart specific service
                char restart_cmd[256];
                if (force_restart) {
                    snprintf(restart_cmd, sizeof(restart_cmd), 
                            "systemctl restart %s --force", service_name);
                } else {
                    snprintf(restart_cmd, sizeof(restart_cmd), 
                            "systemctl restart %s", service_name);
                }
                
                FILE* restart_fp = popen(restart_cmd, "r");
                if (restart_fp) {
                    int exit_code = pclose(restart_fp);
                    if (exit_code == 0) {
                        services_restarted++;
                        char service_log[128];
                        snprintf(service_log, sizeof(service_log), "%s restarted. ", service_name);
                        strcat(restart_log, service_log);
                    } else {
                        restart_success = false;
                        char service_log[128];
                        snprintf(service_log, sizeof(service_log), "%s restart failed. ", service_name);
                        strcat(restart_log, service_log);
                    }
                } else {
                    restart_success = false;
                    char service_log[128];
                    snprintf(service_log, sizeof(service_log), "%s restart command failed. ", service_name);
                    strcat(restart_log, service_log);
                }
            }
        }
    } else {
        // Restart default services
        for (int i = 0; i < 4; i++) {
            char restart_cmd[256];
            if (force_restart) {
                snprintf(restart_cmd, sizeof(restart_cmd), 
                        "systemctl restart %s --force", default_services[i]);
            } else {
                snprintf(restart_cmd, sizeof(restart_cmd), 
                        "systemctl restart %s", default_services[i]);
            }
            
            FILE* restart_fp = popen(restart_cmd, "r");
            if (restart_fp) {
                int exit_code = pclose(restart_fp);
                if (exit_code == 0) {
                    services_restarted++;
                    char service_log[128];
                    snprintf(service_log, sizeof(service_log), "%s restarted. ", default_services[i]);
                    strcat(restart_log, service_log);
                } else {
                    restart_success = false;
                    char service_log[128];
                    snprintf(service_log, sizeof(service_log), "%s restart failed. ", default_services[i]);
                    strcat(restart_log, service_log);
                }
            } else {
                restart_success = false;
                char service_log[128];
                snprintf(service_log, sizeof(service_log), "%s restart command failed. ", default_services[i]);
                strcat(restart_log, service_log);
            }
        }
    }
    
    // Update restart status file
    FILE* status_fp = fopen("/var/lib/autonomy/restart_status", "w");
    if (status_fp) {
        fprintf(status_fp, "restart_start=%ld\n", restart_start);
        fprintf(status_fp, "restart_success=%s\n", restart_success ? "true" : "false");
        fprintf(status_fp, "services_restarted=%d\n", services_restarted);
        fprintf(status_fp, "force_restart=%s\n", force_restart ? "true" : "false");
        fprintf(status_fp, "restart_log=%s\n", restart_log);
        fclose(status_fp);
    }
    
    // Send response
    blobmsg_add_string(&bb, "status", restart_success ? "restart_completed" : "restart_failed");
    blobmsg_add_string(&bb, "message", restart_log);
    blobmsg_add_u32(&bb, "services_restarted", services_restarted);
    blobmsg_add_bool(&bb, "force_restart", force_restart);
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)restart_start);
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}
