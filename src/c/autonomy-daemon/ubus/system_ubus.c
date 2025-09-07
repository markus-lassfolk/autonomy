#include "../core/types.h"
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

extern struct autonomy_state g_state;
extern system_health_t g_system_health;

// System management UBUS method handlers
int autonomy_system_status(struct ubus_context *uctx, struct ubus_object *obj,
                          struct ubus_request_data *req, const char *method,
                          struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    blobmsg_add_string(&bb, "system_status", "operational");
    blobmsg_add_u32(&bb, "uptime", get_system_uptime());
    blobmsg_add_u32(&bb, "memory_mb", get_system_memory_usage());
    blobmsg_add_u32(&bb, "load_average", get_system_load_average());
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_system_health_check(struct ubus_context *uctx, struct ubus_object *obj,
                                struct ubus_request_data *req, const char *method,
                                struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    // Perform the system health check
    perform_system_health_check();
    
    blob_buf_init(&bb, 0);
    blobmsg_add_string(&bb, "result", "health_check_completed");
    blobmsg_add_string(&bb, "overall_status", g_system_health.status);
    blobmsg_add_u32(&bb, "overall_score", g_system_health.overall_score);
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_system_health_details(struct ubus_context *uctx, struct ubus_object *obj,
                                  struct ubus_request_data *req, const char *method,
                                  struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Add detailed health scores
    void *health_scores = blobmsg_open_table(&bb, "health_scores");
    blobmsg_add_u32(&bb, "starlink", g_system_health.starlink_health);
    blobmsg_add_u32(&bb, "uci", g_system_health.uci_health);
    blobmsg_add_u32(&bb, "overlay", g_system_health.overlay_health);
    blobmsg_add_u32(&bb, "services", g_system_health.services_health);
    blobmsg_add_u32(&bb, "network", g_system_health.network_health);
    blobmsg_add_u32(&bb, "database", g_system_health.database_health);
    blobmsg_add_u32(&bb, "time", g_system_health.time_health);
    blobmsg_add_u32(&bb, "logs", g_system_health.logs_health);
    blobmsg_close_table(&bb, health_scores);
    
    blobmsg_add_u32(&bb, "overall_score", g_system_health.overall_score);
    blobmsg_add_string(&bb, "overall_status", g_system_health.status);
    blobmsg_add_u32(&bb, "last_check", g_system_health.last_check);
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_system_maintenance(struct ubus_context *uctx, struct ubus_object *obj,
                               struct ubus_request_data *req, const char *method,
                               struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    int maintenance_tasks = 0;
    int successful_tasks = 0;
    
    blob_buf_init(&bb, 0);
    
    // Real maintenance procedures
    // 1. Clean up old log files
    maintenance_tasks++;
    int ret = system("find /var/log -name '*.log.*' -mtime +7 -delete 2>/dev/null");
    if (ret == 0) {
        successful_tasks++;
    }
    
    // 2. Clean up temporary files
    maintenance_tasks++;
    ret = system("find /tmp -type f -mtime +3 -delete 2>/dev/null");
    if (ret == 0) {
        successful_tasks++;
    }
    
    // 3. Clean up old GPS data files
    maintenance_tasks++;
    ret = system("find /var/lib/autonomy -name '*.old' -mtime +30 -delete 2>/dev/null");
    if (ret == 0) {
        successful_tasks++;
    }
    
    // 4. Optimize database
    maintenance_tasks++;
    ret = system("sqlite3 /var/lib/autonomy/autonomy.db 'VACUUM; ANALYZE;' 2>/dev/null");
    if (ret == 0) {
        successful_tasks++;
    }
    
    // 5. Clean up package cache
    maintenance_tasks++;
    ret = system("opkg clean 2>/dev/null");
    if (ret == 0) {
        successful_tasks++;
    }
    
    // 6. Check and repair filesystem
    maintenance_tasks++;
    ret = system("fsck -n /dev/root 2>/dev/null | grep -q 'clean'");
    if (ret == 0) {
        successful_tasks++;
    }
    
    // 7. Update system time
    maintenance_tasks++;
    ret = system("ntpdate -s time.nist.gov 2>/dev/null");
    if (ret == 0) {
        successful_tasks++;
    }
    
    // 8. Clean up network interfaces
    maintenance_tasks++;
    ret = system("ip route flush cache 2>/dev/null");
    if (ret == 0) {
        successful_tasks++;
    }
    
    // 9. Restart failed services
    maintenance_tasks++;
    ret = system("systemctl --failed --no-legend | awk '{print $1}' | xargs -r systemctl restart 2>/dev/null");
    if (ret == 0) {
        successful_tasks++;
    }
    
    // 10. Clean up memory
    maintenance_tasks++;
    ret = system("sync && echo 3 > /proc/sys/vm/drop_caches 2>/dev/null");
    if (ret == 0) {
        successful_tasks++;
    }
    
    // Report results
    char result_msg[256];
    snprintf(result_msg, sizeof(result_msg), 
            "Maintenance completed: %d/%d tasks successful", 
            successful_tasks, maintenance_tasks);
    
    blobmsg_add_string(&bb, "result", "maintenance_completed");
    blobmsg_add_string(&bb, "message", result_msg);
    blobmsg_add_u32(&bb, "tasks_completed", successful_tasks);
    blobmsg_add_u32(&bb, "total_tasks", maintenance_tasks);
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_system_restart_services(struct ubus_context *uctx, struct ubus_object *obj,
                                    struct ubus_request_data *req, const char *method,
                                    struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    int restart_tasks = 0;
    int successful_restarts = 0;
    
    blob_buf_init(&bb, 0);
    
    // Real service restart procedures
    const char* critical_services[] = {
        "network", "firewall", "dnsmasq", "odhcpd", "autonomy-daemon", 
        "starlink-tracker", "gps-service", "ubus", "logd"
    };
    
    // 1. Restart network services first
    restart_tasks++;
    int ret = system("systemctl restart network 2>/dev/null");
    if (ret == 0) {
        successful_restarts++;
        sleep(2); // Wait for network to stabilize
    }
    
    // 2. Restart firewall
    restart_tasks++;
    ret = system("systemctl restart firewall 2>/dev/null");
    if (ret == 0) {
        successful_restarts++;
    }
    
    // 3. Restart DNS services
    restart_tasks++;
    ret = system("systemctl restart dnsmasq 2>/dev/null");
    if (ret == 0) {
        successful_restarts++;
    }
    
    restart_tasks++;
    ret = system("systemctl restart odhcpd 2>/dev/null");
    if (ret == 0) {
        successful_restarts++;
    }
    
    // 4. Restart GPS service
    restart_tasks++;
    ret = system("systemctl restart gps-service 2>/dev/null");
    if (ret == 0) {
        successful_restarts++;
    }
    
    // 5. Restart Starlink tracker
    restart_tasks++;
    ret = system("systemctl restart starlink-tracker 2>/dev/null");
    if (ret == 0) {
        successful_restarts++;
    }
    
    // 6. Restart log daemon
    restart_tasks++;
    ret = system("systemctl restart logd 2>/dev/null");
    if (ret == 0) {
        successful_restarts++;
    }
    
    // 7. Restart UBUS
    restart_tasks++;
    ret = system("systemctl restart ubus 2>/dev/null");
    if (ret == 0) {
        successful_restarts++;
    }
    
    // 8. Restart autonomy daemon (this service)
    restart_tasks++;
    ret = system("systemctl restart autonomy-daemon 2>/dev/null");
    if (ret == 0) {
        successful_restarts++;
    }
    
    // 9. Verify services are running
    restart_tasks++;
    ret = system("systemctl is-active network firewall dnsmasq gps-service > /dev/null 2>&1");
    if (ret == 0) {
        successful_restarts++;
    }
    
    // 10. Reload UCI configuration
    restart_tasks++;
    ret = system("uci commit 2>/dev/null && /etc/init.d/network reload 2>/dev/null");
    if (ret == 0) {
        successful_restarts++;
    }
    
    // Report results
    char result_msg[256];
    snprintf(result_msg, sizeof(result_msg), 
            "Service restart completed: %d/%d services restarted successfully", 
            successful_restarts, restart_tasks);
    
    blobmsg_add_string(&bb, "result", "services_restart_completed");
    blobmsg_add_string(&bb, "message", result_msg);
    blobmsg_add_u32(&bb, "services_restarted", successful_restarts);
    blobmsg_add_u32(&bb, "total_services", restart_tasks);
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}
