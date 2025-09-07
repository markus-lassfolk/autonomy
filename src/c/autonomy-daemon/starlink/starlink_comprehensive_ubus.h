#ifndef STARLINK_COMPREHENSIVE_UBUS_H
#define STARLINK_COMPREHENSIVE_UBUS_H

#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <stdbool.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get comprehensive Starlink status with all data sources
 * Returns: {
 *   "success": true,
 *   "comprehensive_status": {
 *     "device_info": {
 *       "id": "ut01000000-00000000-00000000",
 *       "hardware_version": "rev1_pre_production",
 *       "software_version": "2023.26.0.mr7526",
 *       "country_code": "US",
 *       "generation_number": 2
 *     },
 *     "gps_data": {
 *       "latitude": 59.329444, "longitude": 18.068611,
 *       "accuracy": 8.5, "confidence": 0.92,
 *       "gps_valid": true, "gps_satellites": 12,
 *       "data_sources": "get_location,get_status,get_diagnostics",
 *       "quality_score": "excellent", "collection_ms": 245.5
 *     },
 *     "network_performance": {
 *       "pop_ping_latency_ms": 35.2,
 *       "downlink_throughput_bps": 150000000,
 *       "uplink_throughput_bps": 25000000
 *     },
 *     "obstruction_stats": {
 *       "currently_obstructed": false,
 *       "fraction_obstructed": 0.0038656357,
 *       "last24h_obstructed_s": 139,
 *       "avg_prolonged_obstruction_interval_s": 0
 *     },
 *     "events_analysis": {
 *       "event_count": 5, "outage_count": 2,
 *       "critical_events_24h": 0, "warning_events_24h": 3,
 *       "total_outages_24h": 2, "avg_outage_duration_s": 15.5,
 *       "outage_frequency_per_hour": 0.083,
 *       "outage_pattern_detected": false,
 *       "stability_score": 0.85
 *     },
 *     "health_scores": {
 *       "overall_health": 0.89, "gps_quality": 0.92,
 *       "network_quality": 0.85, "stability": 0.85
 *     }
 *   }
 * }
 */
int starlink_comprehensive_ubus_get_status(struct ubus_context *ctx, struct ubus_object *obj,
                                          struct ubus_request_data *req, const char *method,
                                          struct blob_attr *msg);

/**
 * Get comprehensive Starlink GPS data from all APIs
 * Returns: {
 *   "success": true,
 *   "gps_data": {
 *     "latitude": 59.329444, "longitude": 18.068611,
 *     "altitude": 25.5, "accuracy": 8.5,
 *     "horizontal_speed_mps": 0.0, "vertical_speed_mps": 0.0,
 *     "gps_source": "GNC_FUSED", "gps_valid": true,
 *     "gps_satellites": 12, "no_sats_after_ttff": false,
 *     "location_enabled": true, "uncertainty_meters": 5.2,
 *     "data_sources": "get_location,get_status,get_diagnostics",
 *     "confidence": 0.92, "quality_score": "excellent",
 *     "collection_ms": 245.5, "valid": true
 *   }
 * }
 */
int starlink_comprehensive_ubus_get_gps_data(struct ubus_context *ctx, struct ubus_object *obj,
                                            struct ubus_request_data *req, const char *method,
                                            struct blob_attr *msg);

/**
 * Get Starlink events and outages analysis
 * Returns: {
 *   "success": true,
 *   "events_analysis": {
 *     "events": [
 *       {
 *         "severity": "warning", "reason": "outage_no_downlink",
 *         "start_timestamp_ns": "1755222161320426611",
 *         "duration_ns": "2239918151", "ongoing": false,
 *         "message": "Temporary downlink outage", "recorded_at": 1703123456
 *       }
 *     ],
 *     "outages": [
 *       {
 *         "cause": "no_downlink", "start_timestamp_ns": "1439315790020446356",
 *         "duration_ns": "939969177", "did_switch": true,
 *         "cause_description": "No downlink signal", "recorded_at": 1703123456
 *       }
 *     ],
 *     "analysis": {
 *       "critical_events_24h": 0, "warning_events_24h": 3,
 *       "total_outages_24h": 2, "avg_outage_duration_s": 15.5,
 *       "outage_frequency_per_hour": 0.083, "primary_cause": "no_downlink",
 *       "outage_pattern_detected": false, "event_escalation_detected": false,
 *       "stability_score": 0.85
 *     }
 *   }
 * }
 */
int starlink_comprehensive_ubus_get_events_analysis(struct ubus_context *ctx, struct ubus_object *obj,
                                                   struct ubus_request_data *req, const char *method,
                                                   struct blob_attr *msg);

/**
 * Get Starlink stability score and health metrics
 * Returns: {
 *   "success": true,
 *   "stability": {
 *     "stability_score": 0.85, "health_score": 0.89,
 *     "gps_quality_score": 0.92, "network_quality_score": 0.85,
 *     "recent_events": 5, "recent_outages": 2,
 *     "pattern_detected": false, "escalation_detected": false,
 *     "primary_issue": "none", "recommendation": "stable"
 *   }
 * }
 */
int starlink_comprehensive_ubus_get_stability(struct ubus_context *ctx, struct ubus_object *obj,
                                             struct ubus_request_data *req, const char *method,
                                             struct blob_attr *msg);

/**
 * Get comprehensive Starlink statistics
 * Returns: {
 *   "success": true,
 *   "statistics": {
 *     "total_collections": 1250, "successful_collections": 1200,
 *     "failed_collections": 50, "collection_success_rate": 0.96,
 *     "api_calls": {
 *       "get_location": 1200, "get_status": 1200,
 *       "get_diagnostics": 1150, "get_history": 250
 *     },
 *     "performance": {
 *       "avg_collection_time_ms": 185.5,
 *       "avg_gps_confidence": 0.88,
 *       "avg_stability_score": 0.82
 *     }
 *   }
 * }
 */
int starlink_comprehensive_ubus_get_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                              struct ubus_request_data *req, const char *method,
                                              struct blob_attr *msg);

/**
 * Force comprehensive Starlink data collection
 * Returns: {
 *   "success": true,
 *   "forced_collection": {
 *     "apis_queried": 4, "apis_successful": 3,
 *     "collection_time_ms": 1250.5,
 *     "gps_confidence": 0.92, "stability_score": 0.85
 *   }
 * }
 */
int starlink_comprehensive_ubus_force_collection(struct ubus_context *ctx, struct ubus_object *obj,
                                                struct ubus_request_data *req, const char *method,
                                                struct blob_attr *msg);

/**
 * Get Starlink configuration
 * Returns: {
 *   "success": true,
 *   "config": {
 *     "enabled": true, "host": "192.168.100.1", "port": 9200,
 *     "timeout_seconds": 30, "collection_interval_s": 60,
 *     "api_collection": {
 *       "collect_location": true, "collect_status": true,
 *       "collect_diagnostics": true, "collect_history": true
 *     },
 *     "analysis": {
 *       "enable_events_analysis": true, "enable_outages_analysis": true,
 *       "max_events": 50, "max_outages": 20, "analysis_window_hours": 24
 *     }
 *   }
 * }
 */
int starlink_comprehensive_ubus_get_config(struct ubus_context *ctx, struct ubus_object *obj,
                                          struct ubus_request_data *req, const char *method,
                                          struct blob_attr *msg);

/**
 * Perform comprehensive Starlink health check
 * Returns: {
 *   "success": true,
 *   "health_check": {
 *     "overall": "healthy", "gps": "excellent", "network": "good",
 *     "stability": "good", "obstruction": "minimal",
 *     "api_connectivity": "working", "data_quality": "high"
 *   }
 * }
 */
int starlink_comprehensive_ubus_health_check(struct ubus_context *ctx, struct ubus_object *obj,
                                            struct ubus_request_data *req, const char *method,
                                            struct blob_attr *msg);

// UBUS method definitions
extern const struct ubus_method starlink_comprehensive_ubus_methods[];
extern const int starlink_comprehensive_ubus_methods_count;

#ifdef __cplusplus
}
#endif

#endif // STARLINK_COMPREHENSIVE_UBUS_H