#include "../core/types.h"
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>

extern struct autonomy_state g_state;

// GPS method handlers
int autonomy_gps_status(struct ubus_context *uctx, struct ubus_object *obj,
                       struct ubus_request_data *req, const char *method,
                       struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    blobmsg_add_string(&bb, "active_source", g_state.active_gps_source);
    blobmsg_add_double(&bb, "current_lat", g_state.current_lat);
    blobmsg_add_double(&bb, "current_lon", g_state.current_lon);
    blobmsg_add_double(&bb, "current_accuracy", g_state.current_accuracy);
    blobmsg_add_u32(&bb, "current_confidence", g_state.current_confidence);
    blobmsg_add_double(&bb, "gps_health_score", g_state.gps_health_score);
    blobmsg_add_string(&bb, "location_status", g_state.location_status);
    blobmsg_add_u8(&bb, "movement_detected", g_state.movement_detected);
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_gps_sources(struct ubus_context *uctx, struct ubus_object *obj,
                        struct ubus_request_data *req, const char *method,
                        struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    blob_buf_init(&bb, 0);
    
    // Add GPS sources array
    void *sources = blobmsg_open_array(&bb, "gps_sources");
    for (int i = 0; i < g_state.gps_source_count; i++) {
        void *source = blobmsg_open_table(&bb, NULL);
        blobmsg_add_string(&bb, "name", g_state.gps_sources[i].name);
        blobmsg_add_string(&bb, "type", g_state.gps_sources[i].type);
        blobmsg_add_u8(&bb, "enabled", g_state.gps_sources[i].enabled);
        blobmsg_add_u8(&bb, "active", g_state.gps_sources[i].active);
        blobmsg_add_double(&bb, "lat", g_state.gps_sources[i].lat);
        blobmsg_add_double(&bb, "lon", g_state.gps_sources[i].lon);
        blobmsg_add_double(&bb, "accuracy", g_state.gps_sources[i].accuracy);
        blobmsg_add_u32(&bb, "confidence", g_state.gps_sources[i].confidence);
        blobmsg_add_u32(&bb, "health_score", g_state.gps_sources[i].health_score);
        blobmsg_add_string(&bb, "status", g_state.gps_sources[i].status);
        blobmsg_close_table(&bb, source);
    }
    blobmsg_close_array(&bb, sources);
    
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}

int autonomy_gps_health_check(struct ubus_context *uctx, struct ubus_object *obj,
                             struct ubus_request_data *req, const char *method,
                             struct blob_attr *msg)
{
    struct blob_buf bb = {0};
    
    // Perform the GPS health check
    perform_gps_health_check();
    
    blob_buf_init(&bb, 0);
    blobmsg_add_string(&bb, "result", "gps_health_check_completed");
    blobmsg_add_double(&bb, "gps_health_score", g_state.gps_health_score);
    blobmsg_add_string(&bb, "active_source", g_state.active_gps_source);
    blobmsg_add_u32(&bb, "timestamp", (uint32_t)time(NULL));
    
    ubus_send_reply(uctx, req, bb.head);
    blob_buf_free(&bb);
    return 0;
}
