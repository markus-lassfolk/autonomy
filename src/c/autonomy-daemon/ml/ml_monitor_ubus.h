#ifndef ML_MONITOR_UBUS_H
#define ML_MONITOR_UBUS_H

#include "ml_monitor.h"
#include <libubus.h>

// UBUS method names
#define ML_MONITOR_UBUS_METHOD_STATUS           "status"
#define ML_MONITOR_UBUS_METHOD_START            "start"
#define ML_MONITOR_UBUS_METHOD_STOP             "stop"
#define ML_MONITOR_UBUS_METHOD_RESTART          "restart"
#define ML_MONITOR_UBUS_METHOD_GET_CONFIG       "get_config"
#define ML_MONITOR_UBUS_METHOD_SET_CONFIG       "set_config"
#define ML_MONITOR_UBUS_METHOD_GET_PREDICTIONS  "get_predictions"
#define ML_MONITOR_UBUS_METHOD_GET_STATISTICS   "get_statistics"
#define ML_MONITOR_UBUS_METHOD_RESET_LEARNING   "reset_learning"
#define ML_MONITOR_UBUS_METHOD_EXPORT_DATA      "export_data"

// UBUS object name
#define ML_MONITOR_UBUS_OBJECT "ml_monitor"

// UBUS method handlers
int ml_monitor_ubus_status(struct ubus_context *ctx, struct ubus_object *obj,
                          struct ubus_request_data *req, const char *method,
                          struct blob_attr *msg);

int ml_monitor_ubus_start(struct ubus_context *ctx, struct ubus_object *obj,
                         struct ubus_request_data *req, const char *method,
                         struct blob_attr *msg);

int ml_monitor_ubus_stop(struct ubus_context *ctx, struct ubus_object *obj,
                        struct ubus_request_data *req, const char *method,
                        struct blob_attr *msg);

int ml_monitor_ubus_restart(struct ubus_context *ctx, struct ubus_object *obj,
                           struct ubus_request_data *req, const char *method,
                           struct blob_attr *msg);

int ml_monitor_ubus_get_config(struct ubus_context *ctx, struct ubus_object *obj,
                              struct ubus_request_data *req, const char *method,
                              struct blob_attr *msg);

int ml_monitor_ubus_set_config(struct ubus_context *ctx, struct ubus_object *obj,
                              struct ubus_request_data *req, const char *method,
                              struct blob_attr *msg);

int ml_monitor_ubus_get_predictions(struct ubus_context *ctx, struct ubus_object *obj,
                                   struct ubus_request_data *req, const char *method,
                                   struct blob_attr *msg);

int ml_monitor_ubus_get_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                  struct ubus_request_data *req, const char *method,
                                  struct blob_attr *msg);

int ml_monitor_ubus_reset_learning(struct ubus_context *ctx, struct ubus_object *obj,
                                  struct ubus_request_data *req, const char *method,
                                  struct blob_attr *msg);

int ml_monitor_ubus_export_data(struct ubus_context *ctx, struct ubus_object *obj,
                               struct ubus_request_data *req, const char *method,
                               struct blob_attr *msg);

// UBUS initialization and cleanup
int ml_monitor_ubus_init(struct ubus_context *ctx);
void ml_monitor_ubus_cleanup(struct ubus_context *ctx);

// UBUS object registration
int ml_monitor_ubus_add_object(struct ubus_context *ctx);
void ml_monitor_ubus_remove_object(struct ubus_context *ctx);

#endif // ML_MONITOR_UBUS_H