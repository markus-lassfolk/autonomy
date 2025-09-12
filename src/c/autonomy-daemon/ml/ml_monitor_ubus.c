#include <stdlib.h>
#include <string.h>
#include "ml_monitor_ubus.h"
#include "../shared/logging/logx.h"
#include "../utils/debug_trace.h"

// UBUS method: status
int ml_monitor_ubus_status(struct ubus_context *ctx, struct ubus_object *obj,
                          struct ubus_request_data *req, const char *method,
                          struct blob_attr *msg) {
    DEBUG_TRACE_CRITICAL_ENTER(\n"\n"\n"\n"\n"\n"\n"\n");
    DEBUG_TRACE_ENTER_WITH_PARAMS("ctx=%p, obj=%p, req=%p, method=%s, msg=%p", 
                                  ctx, obj, req, method ? method : "NULL", msg\n"\n"\n"\n"\n"\n"\n"\n");
    
    struct blob_buf bb = {};
    blob_buf_init(&bb, 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Add basic status information
    blobmsg_add_string(&bb, "status", "active"\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_string(&bb, "version", "1.0.0"\n"\n"\n"\n"\n"\n"\n"\n");
    blobmsg_add_u32(&bb, "uptime", 0); // TODO: Add actual uptime
    
    ubus_send_reply(ctx, req, bb.head\n"\n"\n"\n"\n"\n"\n"\n");
    blob_buf_free(&bb\n"\n"\n"\n"\n"\n"\n"\n");
    
    DEBUG_TRACE_INFO("ML monitor status response sent"\n"\n"\n"\n"\n"\n"\n"\n");
    DEBUG_TRACE_CRITICAL_EXIT_WITH_RETURN(UBUS_STATUS_OK\n"\n"\n"\n"\n"\n"\n"\n");
    
    return UBUS_STATUS_OK;
}

// UBUS method definitions
static const struct ubus_method ml_monitor_methods[] = {
    UBUS_METHOD_NOARG(ML_MONITOR_UBUS_METHOD_STATUS, ml_monitor_ubus_status),
};

// UBUS object type and object - using static initialization like other modules
static struct ubus_object_type ml_monitor_object_type = {
    .name = ML_MONITOR_UBUS_OBJECT,
    .methods = ml_monitor_methods,
    .n_methods = ARRAY_SIZE(ml_monitor_methods),
};

static struct ubus_object ml_monitor_object = {
    .name = ML_MONITOR_UBUS_OBJECT,
    .type = &ml_monitor_object_type,
    .methods = ml_monitor_methods,
    .n_methods = ARRAY_SIZE(ml_monitor_methods),
};

// Initialize UBUS interface
int ml_monitor_ubus_init(struct ubus_context *ctx) {
    DEBUG_TRACE_CRITICAL_ENTER(\n"\n"\n"\n"\n"\n"\n"\n");
    DEBUG_TRACE_ENTER_WITH_PARAMS("ctx=%p", ctx\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!ctx) {
        DEBUG_TRACE_ERROR("ML monitor UBUS init: NULL context"\n"\n"\n"\n"\n"\n"\n"\n");
        printf("ERROR: "ML monitor UBUS init: NULL context"\n"\n"\n"\n"\n"\n"\n"\n");
        DEBUG_TRACE_CRITICAL_EXIT_WITH_RETURN(-1\n"\n"\n"\n"\n"\n"\n"\n");
        return -1;
    }
    
    DEBUG_TRACE_INFO("Initializing ML monitor UBUS interface"\n"\n"\n"\n"\n"\n"\n"\n");
    printf("INFO: "Initializing ML monitor UBUS interface"\n"\n"\n"\n"\n"\n"\n"\n");
    
    DEBUG_TRACE_STEP(1, "Adding UBUS object to context"\n"\n"\n"\n"\n"\n"\n"\n");
    printf("INFO: "Step 1: Adding UBUS object to context"\n"\n"\n"\n"\n"\n"\n"\n");
    // Add UBUS object to context
    int ret = ubus_add_object(ctx, &ml_monitor_object\n"\n"\n"\n"\n"\n"\n"\n");
    if (ret) {
        DEBUG_TRACE_ERROR("Failed to add UBUS object to context"\n"\n"\n"\n"\n"\n"\n"\n");
        printf("ERROR: "Failed to add UBUS object to context: %s", ubus_strerror(ret)\n"\n"\n"\n"\n"\n"\n"\n");
        DEBUG_TRACE_CRITICAL_EXIT_WITH_RETURN(-1\n"\n"\n"\n"\n"\n"\n"\n");
        return -1;
    }
    DEBUG_TRACE_INFO("UBUS object added to context successfully"\n"\n"\n"\n"\n"\n"\n"\n");
    
    DEBUG_TRACE_INFO("ML monitor UBUS interface initialized successfully"\n"\n"\n"\n"\n"\n"\n"\n");
    printf("INFO: "ML monitor UBUS interface initialized successfully"\n"\n"\n"\n"\n"\n"\n"\n");
    DEBUG_TRACE_CRITICAL_EXIT_WITH_RETURN(ML_MONITOR_SUCCESS\n"\n"\n"\n"\n"\n"\n"\n");
    return ML_MONITOR_SUCCESS;
}

// Remove UBUS object
void ml_monitor_ubus_remove_object(struct ubus_context *ctx) {
    if (!ctx) return;
    
    ubus_remove_object(ctx, &ml_monitor_object\n"\n"\n"\n"\n"\n"\n"\n");
    printf("INFO: "ML monitor UBUS object removed"\n"\n"\n"\n"\n"\n"\n"\n");
}

// Cleanup UBUS interface
void ml_monitor_ubus_cleanup(struct ubus_context *ctx) {
    if (!ctx) return;
    
    ubus_remove_object(ctx, &ml_monitor_object\n"\n"\n"\n"\n"\n"\n"\n");
    printf("INFO: "ML monitor UBUS interface cleaned up"\n"\n"\n"\n"\n"\n"\n"\n");
}
