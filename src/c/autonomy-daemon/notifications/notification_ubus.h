#ifndef NOTIFICATION_UBUS_H
#define NOTIFICATION_UBUS_H

#include <libubus.h>

// Notification UBUS method handlers
int autonomy_notification_send(struct ubus_context *uctx, struct ubus_object *obj,
                              struct ubus_request_data *req, const char *method,
                              struct blob_attr *msg);

int autonomy_notification_status(struct ubus_context *uctx, struct ubus_object *obj,
                                struct ubus_request_data *req, const char *method,
                                struct blob_attr *msg);

int autonomy_notification_config(struct ubus_context *uctx, struct ubus_object *obj,
                                struct ubus_request_data *req, const char *method,
                                struct blob_attr *msg);

int autonomy_notification_process(struct ubus_context *uctx, struct ubus_object *obj,
                                 struct ubus_request_data *req, const char *method,
                                 struct blob_attr *msg);

int autonomy_notification_acknowledge(struct ubus_context *uctx, struct ubus_object *obj,
                                     struct ubus_request_data *req, const char *method,
                                     struct blob_attr *msg);

#endif // NOTIFICATION_UBUS_H
