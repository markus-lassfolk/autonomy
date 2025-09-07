#ifndef TELEMETRY_COMPREHENSIVE_UBUS_H
#define TELEMETRY_COMPREHENSIVE_UBUS_H

#include <libubus.h>
#include <libubox/blobmsg_json.h>

// Forward declarations for UBUS types (if needed)
struct ubus_context;
struct ubus_object;
struct ubus_request_data;
struct blob_attr;
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get telemetry collection statistics
 * Returns: {
 *   "success": true,
 *   "statistics": {
 *     "collection": {
 *       "total_samples_collected": 15420, "samples_with_gps": 14850,
 *       "samples_without_gps": 570, "decision_records_logged": 45,
 *       "collection_start_time": 1703000000, "last_collection": 1703123456
 *     },
 *     "by_interface": {
 *       "starlink_samples": 5140, "cellular_samples": 5140, "wifi_samples": 5140
 *     },
 *     "database": {
 *       "database_inserts": 15420, "database_errors": 0,
 *       "database_size_mb": 45.2, "cleanup_operations": 12
 *     },
 *     "performance": {
 *       "avg_collection_time_ms": 125.5, "memory_usage_bytes": 2048576,
 *       "last_cleanup": 1703120000
 *     }
 *   }
 * }
 */
int telemetry_comprehensive_ubus_get_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                               struct ubus_request_data *req, const char *method,
                                               struct blob_attr *msg);

/**
 * Get historical telemetry samples
 * Parameters: {
 *   "member_name": "starlink",     // Optional member filter
 *   "start_time": 1703000000,      // Start time (Unix timestamp)
 *   "end_time": 1703123456,        // End time (Unix timestamp)
 *   "limit": 100,                  // Optional limit (default 50)
 *   "include_gps": true            // Whether to include GPS data
 * }
 * Returns: {
 *   "success": true,
 *   "historical_data": {
 *     "total_matching": 1250, "returned": 100,
 *     "time_range": {"start": 1703000000, "end": 1703123456},
 *     "samples": [
 *       {
 *         "id": 12345, "timestamp": 1703123456,
 *         "member_name": "starlink", "interface_name": "starlink0",
 *         "gps": {"latitude": 59.329444, "longitude": 18.068611, "accuracy": 8.5},
 *         "network": {"latency_ms": 45.2, "packet_loss_percent": 0.1, "throughput_bps": 150000000},
 *         "starlink": {"obstruction_percent": 2.5, "snr_db": 12.5, "outage_count": 0},
 *         "scores": {"overall_score": 89.5, "reliability_score": 0.92, "predictive_risk": 0.15}
 *       }
 *     ]
 *   }
 * }
 */
int telemetry_comprehensive_ubus_get_historical_samples(struct ubus_context *ctx, struct ubus_object *obj,
                                                       struct ubus_request_data *req, const char *method,
                                                       struct blob_attr *msg);

/**
 * Get failover/failback decision history
 * Parameters: {
 *   "start_time": 1703000000,      // Start time (Unix timestamp)
 *   "end_time": 1703123456,        // End time (Unix timestamp)
 *   "limit": 50,                   // Optional limit (default 20)
 *   "decision_type": "failover"    // Optional type filter
 * }
 * Returns: {
 *   "success": true,
 *   "decision_history": {
 *     "total_matching": 45, "returned": 20,
 *     "decisions": [
 *       {
 *         "id": 123, "decision_id": "failover_1703123456_001",
 *         "timestamp": 1703123456, "decision_type": "failover",
 *         "trigger": "latency_degradation", "reasoning": "Starlink latency exceeded threshold",
 *         "from_interface": "starlink0", "to_interface": "wwan0",
 *         "gps": {"latitude": 59.329444, "longitude": 18.068611},
 *         "performance": {"from_score": 45.2, "to_score": 78.5, "score_difference": 33.3},
 *         "execution": {"success": true, "execution_time_ms": 1250.5},
 *         "context": {"predictive_decision": false, "confidence": 0.85}
 *       }
 *     ]
 *   }
 * }
 */
int telemetry_comprehensive_ubus_get_decision_history(struct ubus_context *ctx, struct ubus_object *obj,
                                                     struct ubus_request_data *req, const char *method,
                                                     struct blob_attr *msg);

/**
 * Get connectivity metrics by GPS location
 * Parameters: {
 *   "latitude": 59.329444,         // Center latitude
 *   "longitude": 18.068611,        // Center longitude
 *   "radius_meters": 1000,         // Search radius
 *   "member_name": "starlink",     // Optional member filter
 *   "limit": 100                   // Optional limit
 * }
 * Returns: {
 *   "success": true,
 *   "location_analysis": {
 *     "center": {"latitude": 59.329444, "longitude": 18.068611},
 *     "radius_meters": 1000, "samples_found": 85,
 *     "performance_summary": {
 *       "avg_latency_ms": 45.8, "avg_packet_loss": 0.15,
 *       "avg_signal_quality": 0.88, "avg_overall_score": 87.2
 *     },
 *     "samples": [...]
 *   }
 * }
 */
int telemetry_comprehensive_ubus_get_samples_by_location(struct ubus_context *ctx, struct ubus_object *obj,
                                                        struct ubus_request_data *req, const char *method,
                                                        struct blob_attr *msg);

/**
 * Analyze performance trends
 * Parameters: {
 *   "member_name": "starlink",     // Member to analyze
 *   "hours_back": 24,              // Hours to analyze
 *   "metric": "latency_ms"         // Metric to analyze (optional)
 * }
 * Returns: {
 *   "success": true,
 *   "trend_analysis": {
 *     "member_name": "starlink", "hours_analyzed": 24,
 *     "samples_analyzed": 1440, "trend_slope": -0.05,
 *     "trend_confidence": 0.78, "trend_direction": "improving",
 *     "metrics": {
 *       "latency_ms": {"trend": -0.05, "current": 45.2, "avg": 47.8},
 *       "packet_loss": {"trend": 0.02, "current": 0.1, "avg": 0.08},
 *       "overall_score": {"trend": 1.2, "current": 89.5, "avg": 87.3}
 *     },
 *     "prediction": {
 *       "next_hour_latency": 44.8, "confidence": 0.72,
 *       "performance_outlook": "stable_improving"
 *     }
 *   }
 * }
 */
int telemetry_comprehensive_ubus_analyze_trends(struct ubus_context *ctx, struct ubus_object *obj,
                                               struct ubus_request_data *req, const char *method,
                                               struct blob_attr *msg);

/**
 * Export ML simulation dataset
 * Parameters: {
 *   "start_time": 1703000000,      // Start time for export
 *   "end_time": 1703123456,        // End time for export
 *   "format": "csv",               // Export format (csv, json)
 *   "include_decisions": true,     // Include decision records
 *   "include_gps": true            // Include GPS data
 * }
 * Returns: {
 *   "success": true,
 *   "ml_export": {
 *     "export_path": "/var/lib/autonomy/autonomy_ml_dataset_20231220.csv",
 *     "samples_exported": 15420, "decisions_exported": 45,
 *     "file_size_mb": 12.5, "export_time_ms": 2500.5,
 *     "columns": ["timestamp", "member_name", "latitude", "longitude", ...]
 *   }
 * }
 */
int telemetry_comprehensive_ubus_export_ml_dataset(struct ubus_context *ctx, struct ubus_object *obj,
                                                  struct ubus_request_data *req, const char *method,
                                                  struct blob_attr *msg);

/**
 * Test ML algorithm on historical data
 * Parameters: {
 *   "algorithm_name": "predictive_failover_v1",
 *   "start_time": 1703000000,
 *   "end_time": 1703123456,
 *   "parameters": {"threshold": 0.7, "window_size": 10}
 * }
 * Returns: {
 *   "success": true,
 *   "test_results": {
 *     "algorithm_name": "predictive_failover_v1",
 *     "time_range": {"start": 1703000000, "end": 1703123456},
 *     "samples_analyzed": 15420, "decisions_tested": 67,
 *     "performance": {
 *       "true_positives": 42, "false_positives": 8,
 *       "true_negatives": 15350, "false_negatives": 20,
 *       "accuracy": 0.998, "precision": 0.84, "recall": 0.68,
 *       "f1_score": 0.75
 *     },
 *     "impact_analysis": {
 *       "failovers_prevented": 42, "unnecessary_failovers": 8,
 *       "avg_prediction_lead_time_minutes": 8.5,
 *       "potential_downtime_saved_minutes": 126.5
 *     }
 *   }
 * }
 */
int telemetry_comprehensive_ubus_execute_ml_algorithm(struct ubus_context *ctx, struct ubus_object *obj,
                                                      struct ubus_request_data *req, const char *method,
                                                      struct blob_attr *msg);

/**
 * Force telemetry collection now
 * Returns: {
 *   "success": true,
 *   "forced_collection": {
 *     "samples_collected": 3, "gps_valid": true,
 *     "collection_time_ms": 245.5,
 *     "interfaces": ["starlink0", "wwan0", "wlan0"]
 *   }
 * }
 */
int telemetry_comprehensive_ubus_force_collection(struct ubus_context *ctx, struct ubus_object *obj,
                                                 struct ubus_request_data *req, const char *method,
                                                 struct blob_attr *msg);

/**
 * Perform database cleanup and optimization
 * Returns: {
 *   "success": true,
 *   "cleanup": {
 *     "samples_removed": 1250, "decisions_removed": 15,
 *     "database_size_before_mb": 67.8, "database_size_after_mb": 45.2,
 *     "space_freed_mb": 22.6, "cleanup_time_ms": 1850.5
 *   }
 * }
 */
int telemetry_comprehensive_ubus_force_cleanup(struct ubus_context *ctx, struct ubus_object *obj,
                                              struct ubus_request_data *req, const char *method,
                                              struct blob_attr *msg);

/**
 * Get telemetry system health check
 * Returns: {
 *   "success": true,
 *   "health": {
 *     "overall": "healthy", "database": "working", "collection": "active",
 *     "recent_samples": 145, "recent_errors": 0,
 *     "memory_usage_mb": 2.5, "database_size_mb": 45.2,
 *     "collection_rate": "normal", "recommendation": "system_healthy"
 *   }
 * }
 */
int telemetry_comprehensive_ubus_health_check(struct ubus_context *ctx, struct ubus_object *obj,
                                             struct ubus_request_data *req, const char *method,
                                             struct blob_attr *msg);

// UBUS method definitions
extern const struct ubus_method telemetry_comprehensive_ubus_methods[];
extern const int telemetry_comprehensive_ubus_methods_count;

#ifdef __cplusplus
}
#endif

#endif // TELEMETRY_COMPREHENSIVE_UBUS_H