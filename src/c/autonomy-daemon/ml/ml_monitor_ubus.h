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
#define ML_MONITOR_UBUS_METHOD_GET_ENSEMBLE     "get_ensemble_status"
#define ML_MONITOR_UBUS_METHOD_GET_VALIDATION   "get_validation_metrics"
#define ML_MONITOR_UBUS_METHOD_TRIGGER_OPTIMIZATION "trigger_optimization"
#define ML_MONITOR_UBUS_METHOD_GET_MOBILE_STATUS "get_mobile_status"
#define ML_MONITOR_UBUS_METHOD_EXPORT_FIELD_DATA "export_field_data"
#define ML_MONITOR_UBUS_METHOD_ENABLE_FIELD_TEST "enable_field_test"
#define ML_MONITOR_UBUS_METHOD_GET_SYSTEM_STATUS "get_system_status"
#define ML_MONITOR_UBUS_METHOD_RUN_PRODUCTION_VALIDATION "run_production_validation"
#define ML_MONITOR_UBUS_METHOD_ENABLE_AUTONOMOUS_MODE "enable_autonomous_mode"
#define ML_MONITOR_UBUS_METHOD_GET_MULTI_INTERFACE_STATUS "get_multi_interface_status"
#define ML_MONITOR_UBUS_METHOD_PREDICT_INTERFACE_OUTAGE "predict_interface_outage"
#define ML_MONITOR_UBUS_METHOD_UPDATE_MWAN3_WEIGHTS "update_mwan3_weights"
#define ML_MONITOR_UBUS_METHOD_VALIDATE_FAILOVER "validate_failover_prediction"
#define ML_MONITOR_UBUS_METHOD_GET_ANALYTICS_SUMMARY "get_analytics_summary"
#define ML_MONITOR_UBUS_METHOD_GET_INTERFACE_SCORE_HISTORY "get_interface_score_history"
#define ML_MONITOR_UBUS_METHOD_GET_ACCURACY_TRENDS "get_accuracy_trends"
#define ML_MONITOR_UBUS_METHOD_GET_IMPACT_SUMMARY "get_impact_summary"
#define ML_MONITOR_UBUS_METHOD_GET_CURRENT_INTERFACE_SCORES "get_current_interface_scores"

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

// Phase 4 UBUS method handlers
int ml_monitor_ubus_get_ensemble_status(struct ubus_context *ctx, struct ubus_object *obj,
                                       struct ubus_request_data *req, const char *method,
                                       struct blob_attr *msg);

int ml_monitor_ubus_get_validation_metrics(struct ubus_context *ctx, struct ubus_object *obj,
                                          struct ubus_request_data *req, const char *method,
                                          struct blob_attr *msg);

int ml_monitor_ubus_trigger_optimization(struct ubus_context *ctx, struct ubus_object *obj,
                                        struct ubus_request_data *req, const char *method,
                                        struct blob_attr *msg);

// Phase 5 UBUS method handlers
int ml_monitor_ubus_get_mobile_status(struct ubus_context *ctx, struct ubus_object *obj,
                                     struct ubus_request_data *req, const char *method,
                                     struct blob_attr *msg);

int ml_monitor_ubus_export_field_data(struct ubus_context *ctx, struct ubus_object *obj,
                                     struct ubus_request_data *req, const char *method,
                                     struct blob_attr *msg);

int ml_monitor_ubus_enable_field_test(struct ubus_context *ctx, struct ubus_object *obj,
                                     struct ubus_request_data *req, const char *method,
                                     struct blob_attr *msg);

// Phase 6 UBUS method handlers
int ml_monitor_ubus_get_system_status(struct ubus_context *ctx, struct ubus_object *obj,
                                     struct ubus_request_data *req, const char *method,
                                     struct blob_attr *msg);

int ml_monitor_ubus_run_production_validation(struct ubus_context *ctx, struct ubus_object *obj,
                                             struct ubus_request_data *req, const char *method,
                                             struct blob_attr *msg);

int ml_monitor_ubus_enable_autonomous_mode(struct ubus_context *ctx, struct ubus_object *obj,
                                          struct ubus_request_data *req, const char *method,
                                          struct blob_attr *msg);

// Phase 7 UBUS method handlers
int ml_monitor_ubus_get_multi_interface_status(struct ubus_context *ctx, struct ubus_object *obj,
                                              struct ubus_request_data *req, const char *method,
                                              struct blob_attr *msg);

int ml_monitor_ubus_predict_interface_outage(struct ubus_context *ctx, struct ubus_object *obj,
                                            struct ubus_request_data *req, const char *method,
                                            struct blob_attr *msg);

int ml_monitor_ubus_update_mwan3_weights(struct ubus_context *ctx, struct ubus_object *obj,
                                        struct ubus_request_data *req, const char *method,
                                        struct blob_attr *msg);

int ml_monitor_ubus_validate_failover_prediction(struct ubus_context *ctx, struct ubus_object *obj,
                                                struct ubus_request_data *req, const char *method,
                                                struct blob_attr *msg);

int ml_monitor_ubus_get_analytics_summary(struct ubus_context *ctx, struct ubus_object *obj,
                                         struct ubus_request_data *req, const char *method,
                                         struct blob_attr *msg);

int ml_monitor_ubus_get_interface_score_history(struct ubus_context *ctx, struct ubus_object *obj,
                                               struct ubus_request_data *req, const char *method,
                                               struct blob_attr *msg);

int ml_monitor_ubus_get_accuracy_trends(struct ubus_context *ctx, struct ubus_object *obj,
                                       struct ubus_request_data *req, const char *method,
                                       struct blob_attr *msg);

int ml_monitor_ubus_get_impact_summary(struct ubus_context *ctx, struct ubus_object *obj,
                                      struct ubus_request_data *req, const char *method,
                                      struct blob_attr *msg);

int ml_monitor_ubus_get_current_interface_scores(struct ubus_context *ctx, struct ubus_object *obj,
                                                struct ubus_request_data *req, const char *method,
                                                struct blob_attr *msg);

// UBUS initialization and cleanup
int ml_monitor_ubus_init(struct ubus_context *ctx);
void ml_monitor_ubus_cleanup(struct ubus_context *ctx);

// UBUS object registration
// Note: ml_monitor_ubus_add_object is no longer needed with static initialization
void ml_monitor_ubus_remove_object(struct ubus_context *ctx);

#endif // ML_MONITOR_UBUS_H