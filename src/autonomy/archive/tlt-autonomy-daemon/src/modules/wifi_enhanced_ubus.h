#ifndef WIFI_ENHANCED_UBUS_H
#define WIFI_ENHANCED_UBUS_H

#include <libubus.h>
#include <libubox/blobmsg_json.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get WiFi interfaces and their status
 * Returns: {
 *   "success": true,
 *   "interfaces": [
 *     {
 *       "name": "wlan0", "device": "radio0", "band": "2.4GHz",
 *       "active": true, "enabled": true, "ap_mode": true,
 *       "current_channel": 6, "current_width": "HT20",
 *       "tx_power": 20, "country_code": "US",
 *       "signal_strength": -45, "noise_floor": -90,
 *       "tx_packets": 12345, "rx_packets": 54321,
 *       "last_update": 1703123456
 *     }
 *   ]
 * }
 */
int wifi_enhanced_ubus_get_interfaces(struct ubus_context *ctx, struct ubus_object *obj,
                                     struct ubus_request_data *req, const char *method,
                                     struct blob_attr *msg);

/**
 * Perform WiFi channel scan and analysis
 * Parameters: {
 *   "device": "radio0"  // Optional, scans all devices if not specified
 * }
 * Returns: {
 *   "success": true,
 *   "scan_results": {
 *     "device": "radio0", "band": "2.4GHz",
 *     "scan_time": 1703123456, "aps_found": 15,
 *     "channels": [
 *       {
 *         "channel": 1, "score": 85.5, "stars": 4, "rating": "good",
 *         "co_channel_aps": 2, "overlap_aps": 5,
 *         "utilization_percent": 15.2, "noise_floor": -92,
 *         "strong_interferers": 1, "weak_interferers": 4
 *       }
 *     ]
 *   }
 * }
 */
int wifi_enhanced_ubus_scan_channels(struct ubus_context *ctx, struct ubus_object *obj,
                                    struct ubus_request_data *req, const char *method,
                                    struct blob_attr *msg);

/**
 * Optimize WiFi channels using enhanced algorithms
 * Parameters: {
 *   "trigger": "manual",     // Optional trigger reason
 *   "force": false          // Optional force optimization
 * }
 * Returns: {
 *   "success": true,
 *   "optimization": {
 *     "trigger": "manual", "performed": true,
 *     "improvement": 25.5, "min_improvement": 10,
 *     "old_plan": {"channel_24": 6, "channel_5": 36, "score": 65.2},
 *     "new_plan": {"channel_24": 1, "channel_5": 149, "score": 90.7},
 *     "applied": true, "optimization_time": 1703123456
 *   }
 * }
 */
int wifi_enhanced_ubus_optimize_channels(struct ubus_context *ctx, struct ubus_object *obj,
                                        struct ubus_request_data *req, const char *method,
                                        struct blob_attr *msg);

/**
 * Get current WiFi channel plan
 * Returns: {
 *   "success": true,
 *   "current_plan": {
 *     "channel_24": 6, "channel_5": 149, "width_5": "VHT80",
 *     "score_24": 85, "score_5": 92, "total_score": 177,
 *     "applied_at": 1703123456, "country": "US",
 *     "reg_domain": "FCC", "trigger": "gps_movement",
 *     "successful": true
 *   }
 * }
 */
int wifi_enhanced_ubus_get_current_plan(struct ubus_context *ctx, struct ubus_object *obj,
                                       struct ubus_request_data *req, const char *method,
                                       struct blob_attr *msg);

/**
 * Get WiFi optimization statistics
 * Returns: {
 *   "success": true,
 *   "statistics": {
 *     "total_optimizations": 45, "successful_optimizations": 42,
 *     "failed_optimizations": 3, "skipped_optimizations": 15,
 *     "scans_performed": 125, "successful_scans": 120, "failed_scans": 5,
 *     "avg_scan_time_ms": 8500.5, "avg_optimization_time_ms": 15000.2,
 *     "avg_improvement": 22.8, "movement_triggered": 25,
 *     "scheduled_triggered": 15, "manual_triggered": 5,
 *     "last_optimization": 1703123456, "last_scan": 1703123500
 *   }
 * }
 */
int wifi_enhanced_ubus_get_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                     struct ubus_request_data *req, const char *method,
                                     struct blob_attr *msg);

/**
 * Get WiFi movement integration status
 * Returns: {
 *   "success": true,
 *   "movement": {
 *     "gps_integration_enabled": true, "is_stationary": false,
 *     "stationary_start": 1703123000, "last_movement": 1703123456,
 *     "last_location": {"latitude": 59.329444, "longitude": 18.068611},
 *     "total_distance_moved_m": 2500.5, "movement_events": 15,
 *     "last_optimization": 1703123200, "location_trigger_active": true
 *   },
 *   "config": {
 *     "movement_threshold_m": 100.0, "stationary_time_s": 1800,
 *     "optimization_cooldown_s": 3600
 *   }
 * }
 */
int wifi_enhanced_ubus_get_movement_status(struct ubus_context *ctx, struct ubus_object *obj,
                                          struct ubus_request_data *req, const char *method,
                                          struct blob_attr *msg);

/**
 * Get WiFi optimization configuration
 * Returns: {
 *   "success": true,
 *   "config": {
 *     "enabled": true, "movement_threshold_m": 100.0,
 *     "stationary_time_s": 1800, "nightly_optimization": true,
 *     "nightly_time_seconds": 10800, "min_improvement": 10,
 *     "dwell_time_s": 300, "use_dfs": false, "dry_run": false,
 *     "enhanced_scanner": {
 *       "enabled": true, "strong_rssi_threshold": -60,
 *       "weak_rssi_threshold": -80, "utilization_weight": 100,
 *       "excellent_threshold": 90, "overlap_penalty_ratio": 0.5
 *     },
 *     "gps_integration": {
 *       "enabled": true, "movement_threshold_m": 100.0,
 *       "stationary_time_s": 1800, "optimization_cooldown_s": 3600
 *     }
 *   }
 * }
 */
int wifi_enhanced_ubus_get_config(struct ubus_context *ctx, struct ubus_object *obj,
                                 struct ubus_request_data *req, const char *method,
                                 struct blob_attr *msg);

/**
 * Set WiFi optimization configuration
 * Parameters: {
 *   "enabled": true,
 *   "movement_threshold_m": 150.0,
 *   "nightly_optimization": true,
 *   "min_improvement": 15,
 *   "dry_run": false,
 *   "gps_integration_enabled": true
 * }
 * Returns: {
 *   "success": true,
 *   "message": "WiFi configuration updated successfully"
 * }
 */
int wifi_enhanced_ubus_set_config(struct ubus_context *ctx, struct ubus_object *obj,
                                 struct ubus_request_data *req, const char *method,
                                 struct blob_attr *msg);

/**
 * Update GPS location for movement-based optimization
 * Parameters: {
 *   "gps_data": {
 *     "latitude": 59.329444, "longitude": 18.068611,
 *     "accuracy": 8.5, "timestamp": 1703123456
 *   }
 * }
 * Returns: {
 *   "success": true,
 *   "movement_detected": true,
 *   "optimization_triggered": false,
 *   "reason": "cooldown_active"
 * }
 */
int wifi_enhanced_ubus_update_gps_location(struct ubus_context *ctx, struct ubus_object *obj,
                                          struct ubus_request_data *req, const char *method,
                                          struct blob_attr *msg);

/**
 * Reset WiFi optimization statistics
 * Returns: {
 *   "success": true,
 *   "message": "WiFi statistics reset successfully"
 * }
 */
int wifi_enhanced_ubus_reset_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                       struct ubus_request_data *req, const char *method,
                                       struct blob_attr *msg);

/**
 * Perform WiFi health check
 * Returns: {
 *   "success": true,
 *   "health": {
 *     "overall": "healthy", "interfaces_active": 2,
 *     "optimization_enabled": true, "gps_integration": "working",
 *     "last_optimization": 1703123456, "optimization_success_rate": 0.93
 *   }
 * }
 */
int wifi_enhanced_ubus_health_check(struct ubus_context *ctx, struct ubus_object *obj,
                                   struct ubus_request_data *req, const char *method,
                                   struct blob_attr *msg);

// UBUS method definitions
extern const struct ubus_method wifi_enhanced_ubus_methods[];
extern const int wifi_enhanced_ubus_methods_count;

#ifdef __cplusplus
}
#endif

#endif // WIFI_ENHANCED_UBUS_H