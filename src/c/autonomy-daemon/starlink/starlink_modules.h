#ifndef STARLINK_MODULES_H
#define STARLINK_MODULES_H

#include "starlink_types.h"

// Starlink client functions
int starlink_client_init(const starlink_config_t *config);
int starlink_connect(void);
void starlink_disconnect(void);
int starlink_send_request(starlink_method_t method, char *response, size_t response_size);
int starlink_get_status(starlink_status_response_t *status);
int starlink_get_device_info(starlink_device_info_t *device_info);
int starlink_get_location(starlink_lla_position_t *location);
bool starlink_is_healthy(void);
const starlink_config_t* starlink_get_config(void);
void starlink_client_cleanup(void);

// Starlink collector functions
int starlink_collector_init(int collection_interval);
bool starlink_should_collect(void);
int starlink_collect_data(starlink_collection_result_t *result);
int starlink_get_cached_data(starlink_collection_result_t *result);
int starlink_get_collector_stats(int *cache_hits, int *cache_misses, int *errors, int *successes);
void starlink_set_collection_interval(int interval_seconds);
void starlink_set_collection_enabled(bool enabled);
int starlink_force_collect(starlink_collection_result_t *result);
int starlink_get_location(starlink_lla_position_t *location);
int starlink_get_health(starlink_health_t *health);
void starlink_collector_cleanup(void);

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

// Starlink cluster management functions
int starlink_cluster_init(void);
int starlink_cluster_add(const char *id, const starlink_config_t *config);
int starlink_cluster_remove(const char *id);
int starlink_cluster_find_best_starlink(void);
int starlink_cluster_failover_to(int index, const char *reason);
int starlink_cluster_check_failover(void);
int starlink_cluster_update_instance(int index, const starlink_collection_result_t *result);
int starlink_cluster_get_status(starlink_cluster_t *cluster);
const starlink_instance_t* starlink_cluster_get_active(void);
const starlink_instance_t* starlink_cluster_get_by_id(const char *id);
const starlink_instance_t* starlink_cluster_get_by_index(int index);
void starlink_cluster_set_config(bool auto_failover, int failover_threshold, float min_health_score);
void starlink_cluster_cleanup(void);

// Starlink obstruction analysis functions
int starlink_obstruction_init(void);
int starlink_obstruction_record_observation(const starlink_obstruction_sample_t *sample);
int starlink_obstruction_get_status(starlink_obstruction_status_t *status);
int starlink_obstruction_get_patterns(starlink_environmental_pattern_t *patterns, int max_patterns);
int starlink_obstruction_get_active_matches(starlink_active_match_t *matches, int max_matches);
int starlink_obstruction_get_match_history(starlink_match_result_t *results, int max_results);
int starlink_obstruction_get_config(starlink_obstruction_config_t *config);
int starlink_obstruction_set_config(const starlink_obstruction_config_t *config);
int starlink_obstruction_set_enabled(bool enabled);
int starlink_obstruction_reset(void);
void starlink_obstruction_cleanup(void);

// Starlink cluster UBUS method handlers
int autonomy_starlink_cluster_status(struct ubus_context *uctx, struct ubus_object *obj,
                                    struct ubus_request_data *req, const char *method,
                                    struct blob_attr *msg);
int autonomy_starlink_cluster_add(struct ubus_context *uctx, struct ubus_object *obj,
                                 struct ubus_request_data *req, const char *method,
                                 struct blob_attr *msg);
int autonomy_starlink_cluster_remove(struct ubus_context *uctx, struct ubus_object *obj,
                                    struct ubus_request_data *req, const char *method,
                                    struct blob_attr *msg);
int autonomy_starlink_cluster_failover(struct ubus_context *uctx, struct ubus_object *obj,
                                      struct ubus_request_data *req, const char *method,
                                      struct blob_attr *msg);
int autonomy_starlink_cluster_check_failover(struct ubus_context *uctx, struct ubus_object *obj,
                                            struct ubus_request_data *req, const char *method,
                                            struct blob_attr *msg);
int autonomy_starlink_cluster_config(struct ubus_context *uctx, struct ubus_object *obj,
                                    struct ubus_request_data *req, const char *method,
                                    struct blob_attr *msg);

#endif // STARLINK_MODULES_H
