#ifndef AUTONOMY_MODULES_H
#define AUTONOMY_MODULES_H

#include <libubus.h>
#include <blobmsg_json.h>
#include <uci.h>
#include "autonomy_types.h"
#include "starlink/starlink_types.h"
#include "starlink/starlink_tracker.h"

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

// System management functions
int get_system_uptime(void);
int get_system_memory_usage(void);
int get_system_load_average(void);
int perform_system_health_check(void);
int perform_network_health_check(void);
int perform_gps_health_check(void);

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

// Starlink function declarations
int starlink_collect_data(starlink_collection_result_t *result);
int starlink_get_location(starlink_lla_position_t *location);
int starlink_get_health(starlink_health_t *health);
int starlink_tracker_ubus_init(struct ubus_context *ctx, starlink_tracker_t *tracker);
starlink_tracker_t* starlink_tracker_init_from_uci(struct uci_context *uci_ctx);
int starlink_client_init(const starlink_config_t *config);
int starlink_get_status(starlink_status_response_t *status);
int starlink_get_collector_stats(int *cache_hits, int *cache_misses, int *errors, int *successes);
int starlink_force_collect(starlink_collection_result_t *result);
void starlink_client_cleanup(void);
int starlink_cluster_find_best_starlink(void);
int starlink_cluster_failover_to(int index, const char *reason);

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
