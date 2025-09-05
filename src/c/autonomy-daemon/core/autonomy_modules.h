#ifndef AUTONOMY_MODULES_H
#define AUTONOMY_MODULES_H

// Network module functions
int discover_network_interfaces(void);
int perform_network_health_check(void);

// GPS module functions
int discover_gps_sources(void);
int perform_gps_health_check(void);

// Configuration module functions
int load_uci_config(void);

// PID file module functions
int create_pid_file(void);
void remove_pid_file(void);
int check_pid_file(void);

// UBUS method handlers
int autonomy_status(struct ubus_context *uctx, struct ubus_object *obj,
                    struct ubus_request_data *req, const char *method,
                    struct blob_attr *msg);
int autonomy_health(struct ubus_context *uctx, struct ubus_object *obj,
                    struct ubus_request_data *req, const char *method,
                    struct blob_attr *msg);
int autonomy_config(struct ubus_context *uctx, struct ubus_object *obj,
                    struct ubus_request_data *req, const char *method,
                    struct blob_attr *msg);
int autonomy_start(struct ubus_context *uctx, struct ubus_object *obj,
                   struct ubus_request_data *req, const char *method,
                   struct blob_attr *msg);
int autonomy_stop(struct ubus_context *uctx, struct ubus_object *obj,
                  struct ubus_request_data *req, const char *method,
                  struct blob_attr *msg);
int autonomy_restart(struct ubus_context *uctx, struct ubus_object *obj,
                     struct ubus_request_data *req, const char *method,
                     struct blob_attr *msg);
int autonomy_pid_status(struct ubus_context *uctx, struct ubus_object *obj,
                        struct ubus_request_data *req, const char *method,
                        struct blob_attr *msg);
int autonomy_log_status(struct ubus_context *uctx, struct ubus_object *obj,
                        struct ubus_request_data *req, const char *method,
                        struct blob_attr *msg);
int autonomy_config_status(struct ubus_context *uctx, struct ubus_object *obj,
                          struct ubus_request_data *req, const char *method,
                          struct blob_attr *msg);

// Network UBUS methods
int autonomy_network_status(struct ubus_context *uctx, struct ubus_object *obj,
                           struct ubus_request_data *req, const char *method,
                           struct blob_attr *msg);
int autonomy_network_interfaces(struct ubus_context *uctx, struct ubus_object *obj,
                               struct ubus_request_data *req, const char *method,
                               struct blob_attr *msg);
int autonomy_network_health_check(struct ubus_context *uctx, struct ubus_object *obj,
                                 struct ubus_request_data *req, const char *method,
                                 struct blob_attr *msg);
int autonomy_network_failover(struct ubus_context *uctx, struct ubus_object *obj,
                             struct ubus_request_data *req, const char *method,
                             struct blob_attr *msg);

// GPS UBUS methods
int autonomy_gps_status(struct ubus_context *uctx, struct ubus_object *obj,
                       struct ubus_request_data *req, const char *method,
                       struct blob_attr *msg);
int autonomy_gps_sources(struct ubus_context *uctx, struct ubus_object *obj,
                        struct ubus_request_data *req, const char *method,
                        struct blob_attr *msg);
int autonomy_gps_health_check(struct ubus_context *uctx, struct ubus_object *obj,
                             struct ubus_request_data *req, const char *method,
                             struct blob_attr *msg);

// System management UBUS methods
int autonomy_system_status(struct ubus_context *uctx, struct ubus_object *obj,
                          struct ubus_request_data *req, const char *method,
                          struct blob_attr *msg);
int autonomy_system_health_check(struct ubus_context *uctx, struct ubus_object *obj,
                                struct ubus_request_data *req, const char *method,
                                struct blob_attr *msg);
int autonomy_system_health_details(struct ubus_context *uctx, struct ubus_object *obj,
                                  struct ubus_request_data *req, const char *method,
                                  struct blob_attr *msg);
int autonomy_system_maintenance(struct ubus_context *uctx, struct ubus_object *obj,
                               struct ubus_request_data *req, const char *method,
                               struct blob_attr *msg);
int autonomy_system_restart_services(struct ubus_context *uctx, struct ubus_object *obj,
                                    struct ubus_request_data *req, const char *method,
                                    struct blob_attr *msg);

// Starlink UBUS method handlers
int autonomy_starlink_status(struct ubus_context *uctx, struct ubus_object *obj,
                           struct ubus_request_data *req, const char *method,
                           struct blob_attr *msg);
int autonomy_starlink_health(struct ubus_context *uctx, struct ubus_object *obj,
                           struct ubus_request_data *req, const char *method,
                           struct blob_attr *msg);
int autonomy_starlink_location(struct ubus_context *uctx, struct ubus_object *obj,
                             struct ubus_request_data *req, const char *method,
                             struct blob_attr *msg);
int autonomy_starlink_collector_stats(struct ubus_context *uctx, struct ubus_object *obj,
                                    struct ubus_request_data *req, const char *method,
                                    struct blob_attr *msg);
int autonomy_starlink_force_collect(struct ubus_context *uctx, struct ubus_object *obj,
                                  struct ubus_request_data *req, const char *method,
                                  struct blob_attr *msg);
int autonomy_starlink_cluster_status(struct ubus_context *uctx, struct ubus_object *obj,
                                   struct ubus_request_data *req, const char *method,
                                   struct blob_attr *msg);
int autonomy_starlink_cluster_check_failover(struct ubus_context *uctx, struct ubus_object *obj,
                                           struct ubus_request_data *req, const char *method,
                                           struct blob_attr *msg);

#endif // AUTONOMY_MODULES_H
