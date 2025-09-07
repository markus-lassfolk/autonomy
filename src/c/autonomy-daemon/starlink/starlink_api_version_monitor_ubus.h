#ifndef STARLINK_API_VERSION_MONITOR_UBUS_H
#define STARLINK_API_VERSION_MONITOR_UBUS_H

#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get current Starlink API version
 * Returns: {
 *   "success": true,
 *   "current_version": {
 *     "software_version": "2023.26.0.mr7526",
 *     "hardware_version": "rev1_pre_production",
 *     "software_part_number": "100-00000-r1",
 *     "generation_number": 2,
 *     "parsed_version": {
 *       "major": 2023, "minor": 26, "patch": 0, "build": "mr7526"
 *     },
 *     "first_detected": 1703123456, "last_seen": 1703123800,
 *     "is_current": true
 *   }
 * }
 */
int starlink_api_version_ubus_get_current_version(struct ubus_context *ctx, struct ubus_object *obj,
                                                 struct ubus_request_data *req, const char *method,
                                                 struct blob_attr *msg);

/**
 * Get API version change history
 * Returns: {
 *   "success": true,
 *   "version_changes": [
 *     {
 *       "change_id": "api_change_1703123456_1",
 *       "detected_at": 1703123456, "endpoint": "get_status",
 *       "old_version": "2023.25.0.mr7425",
 *       "new_version": "2023.26.0.mr7526",
 *       "severity": "moderate", "breaking_change_suspected": false,
 *       "notification_sent": true, "api_still_functional": true,
 *       "impact_assessment": "MODERATE version change detected...",
 *       "recommended_actions": "1. Test Starlink API functionality..."
 *     }
 *   ],
 *   "total_changes": 5
 * }
 */
int starlink_api_version_ubus_get_change_history(struct ubus_context *ctx, struct ubus_object *obj,
                                                struct ubus_request_data *req, const char *method,
                                                struct blob_attr *msg);

/**
 * Get API version monitor statistics
 * Returns: {
 *   "success": true,
 *   "statistics": {
 *     "total_version_checks": 1250, "version_changes_detected": 5,
 *     "minor_changes": 3, "moderate_changes": 2, "major_changes": 0,
 *     "notifications_sent": 5, "validation_attempts": 5,
 *     "validation_failures": 0, "avg_check_time_ms": 125.5,
 *     "last_version_check": 1703123800, "last_version_change": 1703123456,
 *     "monitoring_duration_hours": 168.5
 *   }
 * }
 */
int starlink_api_version_ubus_get_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                            struct ubus_request_data *req, const char *method,
                                            struct blob_attr *msg);

/**
 * Force immediate API version check
 * Returns: {
 *   "success": true,
 *   "version_check": {
 *     "performed_at": 1703123456, "version_detected": "2023.26.0.mr7526",
 *     "version_changed": false, "check_time_ms": 245.5,
 *     "api_functional": true
 *   }
 * }
 */
int starlink_api_version_ubus_force_check(struct ubus_context *ctx, struct ubus_object *obj,
                                         struct ubus_request_data *req, const char *method,
                                         struct blob_attr *msg);

/**
 * Get API version monitor configuration
 * Returns: {
 *   "success": true,
 *   "config": {
 *     "enabled": true, "check_interval_s": 3600,
 *     "notifications": {
 *       "notify_on_minor_changes": false, "notify_on_moderate_changes": true,
 *       "notify_on_major_changes": true, "notify_on_unknown_changes": true,
 *       "send_immediate_notifications": true, "send_summary_notifications": true
 *     },
 *     "validation": {
 *       "perform_validation_on_change": true, "validation_timeout_s": 30,
 *       "max_validation_retries": 3
 *     },
 *     "storage": {
 *       "max_version_history": 20, "max_change_records": 50,
 *       "version_storage_file": "/var/lib/autonomy/starlink_api_versions.txt"
 *     }
 *   }
 * }
 */
int starlink_api_version_ubus_get_config(struct ubus_context *ctx, struct ubus_object *obj,
                                        struct ubus_request_data *req, const char *method,
                                        struct blob_attr *msg);

/**
 * Validate Starlink API endpoints
 * Returns: {
 *   "success": true,
 *   "validation": {
 *     "overall_functional": true, "validation_time": 1703123456,
 *     "endpoints": {
 *       "get_status": {"functional": true, "response_time_ms": 125.5},
 *       "get_location": {"functional": true, "response_time_ms": 95.2},
 *       "get_diagnostics": {"functional": true, "response_time_ms": 185.8},
 *       "get_history": {"functional": false, "error": "timeout"}
 *     },
 *     "recommendation": "All critical endpoints functional"
 *   }
 * }
 */
int starlink_api_version_ubus_validate_endpoints(struct ubus_context *ctx, struct ubus_object *obj,
                                                struct ubus_request_data *req, const char *method,
                                                struct blob_attr *msg);

/**
 * Perform API version monitor health check
 * Returns: {
 *   "success": true,
 *   "health": {
 *     "monitor_status": "healthy", "current_version_known": true,
 *     "last_check": 1703123800, "check_frequency": "normal",
 *     "recent_changes": 0, "api_functional": true,
 *     "recommendation": "monitor_healthy"
 *   }
 * }
 */
int starlink_api_version_ubus_health_check(struct ubus_context *ctx, struct ubus_object *obj,
                                          struct ubus_request_data *req, const char *method,
                                          struct blob_attr *msg);

// UBUS method definitions
extern const struct ubus_method starlink_api_version_ubus_methods[];
extern const int starlink_api_version_ubus_methods_count;

#ifdef __cplusplus
}
#endif

#endif // STARLINK_API_VERSION_MONITOR_UBUS_H