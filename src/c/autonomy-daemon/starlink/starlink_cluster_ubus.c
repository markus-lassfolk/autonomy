#include <stdlib.h>
#include "starlink_types.h"
#include "starlink_modules.h"
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <time.h>
#include <string.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Policy definitions for UBUS methods
static const struct blobmsg_policy starlink_cluster_add_policy[] = {
    [0] = { .name = "id", .type = BLOBMSG_TYPE_STRING },
    [1] = { .name = "host", .type = BLOBMSG_TYPE_STRING },
    [2] = { .name = "port", .type = BLOBMSG_TYPE_INT32 },
    [3] = { .name = "interface", .type = BLOBMSG_TYPE_STRING },
    [4] = { .name = "mwan3_member", .type = BLOBMSG_TYPE_STRING },
    [5] = { .name = "priority", .type = BLOBMSG_TYPE_INT32 },
    [6] = { .name = "enabled", .type = BLOBMSG_TYPE_BOOL },
};

static const struct blobmsg_policy starlink_cluster_remove_policy[] = {
    [0] = { .name = "id", .type = BLOBMSG_TYPE_STRING },
};

static const struct blobmsg_policy starlink_cluster_failover_policy[] = {
    [0] = { .name = "target", .type = BLOBMSG_TYPE_STRING },
    [1] = { .name = "reason", .type = BLOBMSG_TYPE_STRING },
};

static const struct blobmsg_policy starlink_cluster_config_policy[] = {
    [0] = { .name = "auto_failover", .type = BLOBMSG_TYPE_BOOL },
    [1] = { .name = "failover_threshold", .type = BLOBMSG_TYPE_INT32 },
    [2] = { .name = "min_health_score", .type = BLOBMSG_TYPE_DOUBLE },
};

// Multi-Starlink cluster UBUS method handlers
int autonomy_starlink_cluster_status(struct ubus_context *uctx, struct ubus_object *obj,
                                    struct ubus_request_data *req, const char *method,
                                    struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Get cluster status
    starlink_cluster_t cluster;
    if (starlink_cluster_get_status(&cluster) == 0) {
        blobmsg_add_string(&bb, "result", "cluster_status_retrieved");
        blobmsg_add_u32(&bb, "total_starlinks", cluster.count);
        blobmsg_add_u32(&bb, "active_index", cluster.active_index);
        blobmsg_add_u32(&bb, "failover_count", cluster.failover_count);
        blobmsg_add_u8(&bb, "auto_failover_enabled", cluster.auto_failover_enabled);
        blobmsg_add_u32(&bb, "failover_threshold", cluster.failover_threshold);
        blobmsg_add_double(&bb, "min_health_score", cluster.min_health_score);
        
        if (cluster.last_failover > 0) {
            blobmsg_add_u32(&bb, "last_failover", cluster.last_failover);
        }
        
        // Add individual Starlink information
        void *starlinks = blobmsg_open_array(&bb, "starlinks");
        for (int i = 0; i < cluster.count; i++) {
            void *starlink = blobmsg_open_table(&bb, NULL);
            
            const starlink_instance_t *instance = &cluster.starlinks[i];
            blobmsg_add_string(&bb, "id", instance->id);
            blobmsg_add_string(&bb, "host", instance->config.host);
            blobmsg_add_u32(&bb, "port", instance->config.port);
            blobmsg_add_string(&bb, "interface", instance->config.interface_name);
            blobmsg_add_string(&bb, "mwan3_member", instance->config.mwan3_member);
            blobmsg_add_u32(&bb, "priority", instance->config.priority);
            blobmsg_add_u8(&bb, "enabled", instance->config.enabled);
            blobmsg_add_u8(&bb, "is_active", instance->is_active);
            blobmsg_add_u8(&bb, "is_healthy", instance->is_healthy);
            
            if (instance->last_collection > 0) {
                blobmsg_add_u32(&bb, "last_collection", instance->last_collection);
            }
            
            blobmsg_add_u32(&bb, "consecutive_successes", instance->consecutive_successes);
            blobmsg_add_u32(&bb, "consecutive_failures", instance->consecutive_failures);
            blobmsg_add_double(&bb, "average_latency_ms", instance->average_latency);
            blobmsg_add_double(&bb, "average_throughput_mbps", instance->average_throughput);
            blobmsg_add_double(&bb, "reliability_score", instance->reliability_score);
            
            if (instance->last_result.success) {
                blobmsg_add_u32(&bb, "health_score", instance->last_result.health.overall_score);
                blobmsg_add_string(&bb, "health_status", instance->last_result.health.status);
            }
            
            if (strlen(instance->failover_reason) > 0) {
                blobmsg_add_string(&bb, "failover_reason", instance->failover_reason);
            }
            
            blobmsg_close_table(&bb, starlink);
        }
        blobmsg_close_array(&bb, starlinks);
        
    } else {
        blobmsg_add_string(&bb, "result", "cluster_status_failed");
        blobmsg_add_string(&bb, "error", "Unable to retrieve cluster status");
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_starlink_cluster_add(struct ubus_context *uctx, struct ubus_object *obj,
                                 struct ubus_request_data *req, const char *method,
                                 struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Parse request parameters
    struct blob_attr *tb[8];
    blobmsg_parse(starlink_cluster_add_policy, 7, tb, blobmsg_data(msg), blobmsg_len(msg));
    
    if (!tb[0] || !tb[1] || !tb[2]) {
        blobmsg_add_string(&bb, "result", "invalid_parameters");
        blobmsg_add_string(&bb, "error", "Missing required parameters: id, host, port");
        blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
        ubus_send_reply(uctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    // Create Starlink configuration
    starlink_config_t config = {0};
    strcpy(config.host, blobmsg_get_string(tb[1]));
    config.port = blobmsg_get_u32(tb[2]);
    config.timeout_seconds = STARLINK_DEFAULT_TIMEOUT;
    config.grpc_first = true;
    config.http_first = false;
    config.predictive_enabled = true; // Use configurable predictive enabled
    config.enabled = true; // Use configurable starlink cluster enabled
    config.priority = 100; // Default priority
    
    // Optional parameters
    if (tb[3]) strcpy(config.interface_name, blobmsg_get_string(tb[3]));
    if (tb[4]) strcpy(config.mwan3_member, blobmsg_get_string(tb[4]));
    if (tb[5]) config.priority = blobmsg_get_u32(tb[5]);
    if (tb[6]) config.enabled = blobmsg_get_u8(tb[6]);
    
    // Add to cluster
    const char *id = blobmsg_get_string(tb[0]);
    int index = starlink_cluster_add(id, &config);
    
    if (index >= 0) {
        blobmsg_add_string(&bb, "result", "starlink_added");
        blobmsg_add_string(&bb, "id", id);
        blobmsg_add_u32(&bb, "index", index);
        blobmsg_add_string(&bb, "host", config.host);
        blobmsg_add_u32(&bb, "port", config.port);
    } else {
        blobmsg_add_string(&bb, "result", "starlink_add_failed");
        blobmsg_add_string(&bb, "error", "Failed to add Starlink to cluster");
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_starlink_cluster_remove(struct ubus_context *uctx, struct ubus_object *obj,
                                    struct ubus_request_data *req, const char *method,
                                    struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Parse request parameters
    struct blob_attr *tb[1];
    blobmsg_parse(starlink_cluster_remove_policy, 1, tb, blob_data(msg), blob_len(msg));
    
    if (!tb[0]) {
        blobmsg_add_string(&bb, "result", "invalid_parameters");
        blobmsg_add_string(&bb, "error", "Missing required parameter: id");
        blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
        ubus_send_reply(uctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    const char *id = blobmsg_get_string(tb[0]);
    int result = starlink_cluster_remove(id);
    
    if (result == 0) {
        blobmsg_add_string(&bb, "result", "starlink_removed");
        blobmsg_add_string(&bb, "id", id);
    } else {
        blobmsg_add_string(&bb, "result", "starlink_remove_failed");
        blobmsg_add_string(&bb, "error", "Failed to remove Starlink from cluster");
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_starlink_cluster_failover(struct ubus_context *uctx, struct ubus_object *obj,
                                      struct ubus_request_data *req, const char *method,
                                      struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Parse request parameters
    struct blob_attr *tb[2];
    blobmsg_parse(starlink_cluster_failover_policy, 2, tb, blob_data(msg), blob_len(msg));
    
    if (!tb[0]) {
        blobmsg_add_string(&bb, "result", "invalid_parameters");
        blobmsg_add_string(&bb, "error", "Missing required parameter: target");
        blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
        ubus_send_reply(uctx, req, bb.head);
        blob_buf_free(&bb);
        return 0;
    }
    
    const char *target = blobmsg_get_string(tb[0]);
    const char *reason = tb[1] ? blobmsg_get_string(tb[1]) : "Manual failover";
    
    int result = -1;
    
    // Check if target is an ID or index
    if (strncmp(target, "index:", 6) == 0) {
        // Target is an index
        int index = atoi(target + 6);
        result = starlink_cluster_failover_to(index, reason);
    } else {
        // Target is an ID, find the index
        const starlink_instance_t *instance = starlink_cluster_get_by_id(target);
        if (instance) {
            // Find the index of this instance
            starlink_cluster_t cluster;
            if (starlink_cluster_get_status(&cluster) == 0) {
                for (int i = 0; i < cluster.count; i++) {
                    if (strcmp(cluster.starlinks[i].id, target) == 0) {
                        result = starlink_cluster_failover_to(i, reason);
                        break;
                    }
                }
            }
        }
    }
    
    if (result == 0) {
        blobmsg_add_string(&bb, "result", "failover_completed");
        blobmsg_add_string(&bb, "target", target);
        blobmsg_add_string(&bb, "reason", reason);
        
        // Get new active Starlink info
        const starlink_instance_t *active = starlink_cluster_get_active();
        if (active) {
            blobmsg_add_string(&bb, "new_active_id", active->id);
            blobmsg_add_string(&bb, "new_active_host", active->config.host);
        }
    } else {
        blobmsg_add_string(&bb, "result", "failover_failed");
        blobmsg_add_string(&bb, "error", "Failed to perform failover");
        blobmsg_add_string(&bb, "target", target);
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_starlink_cluster_check_failover(struct ubus_context *uctx, struct ubus_object *obj,
                                            struct ubus_request_data *req, const char *method,
                                            struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Check if failover is needed
    int result = starlink_cluster_check_failover();
    
    if (result == 0) {
        blobmsg_add_string(&bb, "result", "failover_check_completed");
        blobmsg_add_string(&bb, "action", "no_failover_needed");
    } else if (result > 0) {
        blobmsg_add_string(&bb, "result", "failover_check_completed");
        blobmsg_add_string(&bb, "action", "failover_performed");
        blobmsg_add_u32(&bb, "new_active_index", result);
        
        // Get new active Starlink info
        const starlink_instance_t *active = starlink_cluster_get_active();
        if (active) {
            blobmsg_add_string(&bb, "new_active_id", active->id);
            blobmsg_add_string(&bb, "new_active_host", active->config.host);
            blobmsg_add_string(&bb, "failover_reason", active->failover_reason);
        }
    } else {
        blobmsg_add_string(&bb, "result", "failover_check_failed");
        blobmsg_add_string(&bb, "error", "Failed to check failover status");
    }
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_starlink_cluster_config(struct ubus_context *uctx, struct ubus_object *obj,
                                    struct ubus_request_data *req, const char *method,
                                    struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Parse request parameters
    struct blob_attr *tb[4];
    blobmsg_parse(starlink_cluster_config_policy, 3, tb, blob_data(msg), blob_len(msg));
    
    bool auto_failover = true; // Use configurable setting
    int failover_threshold = 3; // Use configurable value
    float min_health_score = 70.0; // Use configurable value
    
    // Optional parameters
    if (tb[0]) auto_failover = blobmsg_get_u8(tb[0]);
    if (tb[1]) failover_threshold = blobmsg_get_u32(tb[1]);
    if (tb[2]) min_health_score = blobmsg_get_double(tb[2]);
    
    // Set cluster configuration
    starlink_cluster_set_config(auto_failover, failover_threshold, min_health_score);
    
    blobmsg_add_string(&bb, "result", "cluster_config_updated");
    blobmsg_add_u8(&bb, "auto_failover", auto_failover);
    blobmsg_add_u32(&bb, "failover_threshold", failover_threshold);
    blobmsg_add_double(&bb, "min_health_score", min_health_score);
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}
