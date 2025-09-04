#ifndef GPS_COMPREHENSIVE_UBUS_H
#define GPS_COMPREHENSIVE_UBUS_H

#include <libubus.h>
#include <libubox/blobmsg_json.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get comprehensive GPS status with all sources and fusion data
 * Returns: {
 *   "success": true,
 *   "current_position": {
 *     "latitude": 59.329444, "longitude": 18.068611,
 *     "accuracy": 8.5, "confidence": 0.92,
 *     "source": "rutos", "method": "hybrid_prioritization",
 *     "timestamp": 1703123456, "age_seconds": 2.1
 *   },
 *   "fusion_data": {
 *     "method": "confidence_weighted", "sources_used": 3,
 *     "fusion_confidence": 0.88, "fusion_accuracy": 12.3,
 *     "outliers_removed": 0, "consensus": true
 *   },
 *   "movement": {
 *     "is_moving": false, "speed_ms": 0.0,
 *     "distance_traveled_m": 1250.5, "movement_events": 15,
 *     "stationary_duration_s": 1800
 *   },
 *   "source_health": [
 *     {
 *       "source": "rutos", "available": true, "healthy": true,
 *       "health_score": 0.95, "success_rate": 0.98,
 *       "avg_accuracy": 6.2, "consecutive_successes": 45
 *     }
 *   ]
 * }
 */
int gps_comprehensive_ubus_get_status(struct ubus_context *ctx, struct ubus_object *obj,
                                     struct ubus_request_data *req, const char *method,
                                     struct blob_attr *msg);

/**
 * Collect GPS data from best source using comprehensive collector
 * Returns: {
 *   "success": true,
 *   "gps_data": {
 *     "latitude": 59.329444, "longitude": 18.068611,
 *     "altitude": 25.5, "accuracy": 8.5, "confidence": 0.92,
 *     "satellites_used": 12, "satellites_visible": 15,
 *     "hdop": 0.8, "vdop": 1.2, "pdop": 1.4,
 *     "speed": 0.0, "heading": 0.0, "climb": 0.0,
 *     "fix_type": "3d", "fix_quality": "gps",
 *     "source": "rutos", "source_priority": 1,
 *     "timestamp": 1703123456, "age_seconds": 1.2,
 *     "collection_duration_ms": 245.5
 *   }
 * }
 */
int gps_comprehensive_ubus_collect_best(struct ubus_context *ctx, struct ubus_object *obj,
                                       struct ubus_request_data *req, const char *method,
                                       struct blob_attr *msg);

/**
 * Collect GPS data from all sources and perform fusion
 * Returns: {
 *   "success": true,
 *   "fusion_result": {
 *     "fused_data": { ... },
 *     "source_data": [
 *       {"source": "rutos", "latitude": 59.329444, ...},
 *       {"source": "starlink", "latitude": 59.329401, ...}
 *     ],
 *     "sources_used": 2, "fusion_method": "confidence_weighted",
 *     "fusion_confidence": 0.88, "fusion_time": 1703123456
 *   }
 * }
 */
int gps_comprehensive_ubus_collect_all_and_fuse(struct ubus_context *ctx, struct ubus_object *obj,
                                               struct ubus_request_data *req, const char *method,
                                               struct blob_attr *msg);

/**
 * Get GPS source health status for all sources
 * Returns: {
 *   "success": true,
 *   "sources": [
 *     {
 *       "source": "rutos", "type": "rutos",
 *       "available": true, "healthy": true, "health_score": 0.95,
 *       "total_collections": 1250, "successful_collections": 1225,
 *       "failed_collections": 25, "success_rate": 0.98,
 *       "avg_collection_time_ms": 180.5, "avg_accuracy": 6.2,
 *       "avg_confidence": 0.89, "consecutive_successes": 45,
 *       "best_accuracy": 2.1, "worst_accuracy": 25.8,
 *       "last_success": 1703123456, "last_failure": 1703120000
 *     }
 *   ]
 * }
 */
int gps_comprehensive_ubus_get_source_health(struct ubus_context *ctx, struct ubus_object *obj,
                                           struct ubus_request_data *req, const char *method,
                                           struct blob_attr *msg);

/**
 * Get movement detection status and statistics
 * Returns: {
 *   "success": true,
 *   "movement": {
 *     "is_moving": false, "was_moving": true,
 *     "movement_start": 1703123000, "stationary_start": 1703123300,
 *     "last_latitude": 59.329444, "last_longitude": 18.068611,
 *     "total_distance_m": 2500.75, "current_speed_ms": 0.0,
 *     "average_speed_ms": 1.25, "max_speed_ms": 15.8,
 *     "movement_events": 25, "last_movement_event": 1703123300
 *   },
 *   "detection_config": {
 *     "threshold_m": 50.0, "hysteresis_m": 10.0,
 *     "check_interval_s": 30.0, "stationary_threshold_s": 300.0
 *   }
 * }
 */
int gps_comprehensive_ubus_get_movement_status(struct ubus_context *ctx, struct ubus_object *obj,
                                             struct ubus_request_data *req, const char *method,
                                             struct blob_attr *msg);

/**
 * Get GPS fusion engine statistics
 * Returns: {
 *   "success": true,
 *   "fusion_stats": {
 *     "total_fusions": 456, "successful_fusions": 445, "failed_fusions": 11,
 *     "outliers_detected": 8, "consensus_failures": 3,
 *     "avg_fusion_time_ms": 12.5, "avg_sources_per_fusion": 2.3,
 *     "avg_fusion_accuracy": 15.8, "avg_fusion_confidence": 0.82,
 *     "method_usage": {
 *       "single_source": 123, "weighted_average": 89,
 *       "confidence_weighted": 234, "kalman_filter": 0
 *     },
 *     "last_fusion": 1703123456
 *   }
 * }
 */
int gps_comprehensive_ubus_get_fusion_stats(struct ubus_context *ctx, struct ubus_object *obj,
                                          struct ubus_request_data *req, const char *method,
                                          struct blob_attr *msg);

/**
 * Get comprehensive GPS system statistics
 * Returns: {
 *   "success": true,
 *   "comprehensive_stats": {
 *     "total_collections": 2500, "successful_collections": 2450,
 *     "failed_collections": 50, "fusion_operations": 456,
 *     "movement_detections": 25, "collection_success_rate": 0.98,
 *     "avg_collection_time_ms": 185.5, "last_collection": 1703123456
 *   }
 * }
 */
int gps_comprehensive_ubus_get_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                        struct ubus_request_data *req, const char *method,
                                        struct blob_attr *msg);

/**
 * Force GPS collection from all sources (useful for testing)
 * Returns: {
 *   "success": true,
 *   "forced_collection": {
 *     "sources_attempted": 4, "sources_successful": 3,
 *     "best_source": "rutos", "fusion_performed": true,
 *     "collection_time_ms": 1250.5
 *   }
 * }
 */
int gps_comprehensive_ubus_force_collection(struct ubus_context *ctx, struct ubus_object *obj,
                                          struct ubus_request_data *req, const char *method,
                                          struct blob_attr *msg);

/**
 * Reset GPS comprehensive statistics
 * Returns: {
 *   "success": true,
 *   "message": "GPS statistics reset successfully"
 * }
 */
int gps_comprehensive_ubus_reset_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                          struct ubus_request_data *req, const char *method,
                                          struct blob_attr *msg);

/**
 * Perform comprehensive GPS health check
 * Returns: {
 *   "success": true,
 *   "health_check": {
 *     "overall_health": "excellent", "sources_healthy": 3,
 *     "sources_unhealthy": 0, "sources_available": 3,
 *     "movement_detection": "working", "fusion_engine": "working",
 *     "comprehensive_collector": "working"
 *   }
 * }
 */
int gps_comprehensive_ubus_health_check(struct ubus_context *ctx, struct ubus_object *obj,
                                       struct ubus_request_data *req, const char *method,
                                       struct blob_attr *msg);

// UBUS method definitions
extern const struct ubus_method gps_comprehensive_ubus_methods[];
extern const int gps_comprehensive_ubus_methods_count;

#ifdef __cplusplus
}
#endif

#endif // GPS_COMPREHENSIVE_UBUS_H