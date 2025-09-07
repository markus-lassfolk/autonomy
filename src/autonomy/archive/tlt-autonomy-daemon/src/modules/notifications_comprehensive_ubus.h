#ifndef NOTIFICATIONS_COMPREHENSIVE_UBUS_H
#define NOTIFICATIONS_COMPREHENSIVE_UBUS_H

#include <libubus.h>
#include <libubox/blobmsg_json.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Send comprehensive notification
 * Parameters: {
 *   "type": "network_failover",
 *   "priority": "high",
 *   "title": "Network Interface Failed",
 *   "message": "Starlink interface has failed, switching to cellular",
 *   "context": {"interface": "starlink", "failover_to": "cellular"},
 *   "source_module": "network_controller"
 * }
 * Returns: {
 *   "success": true,
 *   "notification": {
 *     "id": "notif_1_1703123456_42",
 *     "status": "sent", "sent_at": 1703123456,
 *     "channels_used": ["pushover", "email"],
 *     "delivery_confidence": 0.85,
 *     "processing_time_ms": 125.5,
 *     "priority_optimized": false,
 *     "channels_optimized": true
 *   }
 * }
 */
int notifications_comprehensive_ubus_send(struct ubus_context *ctx, struct ubus_object *obj,
                                         struct ubus_request_data *req, const char *method,
                                         struct blob_attr *msg);

/**
 * Send emergency notification
 * Parameters: {
 *   "title": "Critical System Failure",
 *   "message": "All network interfaces have failed",
 *   "context": {"severity": "critical", "all_interfaces_down": true},
 *   "source_module": "network_monitor"
 * }
 * Returns: {
 *   "success": true,
 *   "emergency_notification": {
 *     "id": "notif_emergency_1703123456_1",
 *     "bypass_used": true, "channels_used": ["pushover", "email", "sms"],
 *     "delivery_confidence": 0.95, "processing_time_ms": 85.2
 *   }
 * }
 */
int notifications_comprehensive_ubus_send_emergency(struct ubus_context *ctx, struct ubus_object *obj,
                                                   struct ubus_request_data *req, const char *method,
                                                   struct blob_attr *msg);

/**
 * Get notification status and delivery tracking
 * Parameters: {
 *   "notification_id": "notif_1_1703123456_42"
 * }
 * Returns: {
 *   "success": true,
 *   "notification": {
 *     "id": "notif_1_1703123456_42", "type": "network_failover",
 *     "priority": "high", "title": "Network Interface Failed",
 *     "status": "delivered", "created_at": 1703123456,
 *     "sent_at": 1703123460, "delivered_at": 1703123465,
 *     "delivery_tracking": {
 *       "pushover": {"sent": true, "success": true},
 *       "email": {"sent": true, "success": true},
 *       "sms": {"sent": false, "success": false}
 *     },
 *     "intelligence": {
 *       "priority_optimized": false, "channels_optimized": true,
 *       "delivery_confidence": 0.85, "processing_time_ms": 125.5
 *     },
 *     "acknowledgment": {
 *       "required": true, "acknowledged": false,
 *       "expires_at": 1703127056
 *     }
 *   }
 * }
 */
int notifications_comprehensive_ubus_get_status(struct ubus_context *ctx, struct ubus_object *obj,
                                               struct ubus_request_data *req, const char *method,
                                               struct blob_attr *msg);

/**
 * Get comprehensive notification statistics
 * Returns: {
 *   "success": true,
 *   "statistics": {
 *     "overall": {
 *       "total_notifications": 1250, "successful_notifications": 1180,
 *       "failed_notifications": 70, "success_rate": 0.944,
 *       "suppressed_notifications": 45, "deduplicated_notifications": 125,
 *       "rate_limited_notifications": 15
 *     },
 *     "channels": {
 *       "pushover": {"sent": 890, "success_rate": 0.95},
 *       "email": {"sent": 750, "success_rate": 0.92},
 *       "sms": {"sent": 320, "success_rate": 0.88},
 *       "webhook": {"sent": 450, "success_rate": 0.85}
 *     },
 *     "priorities": {
 *       "emergency": 15, "high": 125, "normal": 890, "low": 220
 *     },
 *     "intelligence": {
 *       "priority_optimizations": 85, "channel_optimizations": 340,
 *       "delivery_optimizations": 125, "emergency_detections": 5
 *     },
 *     "performance": {
 *       "avg_processing_time_ms": 95.5, "avg_delivery_time_ms": 1250.5,
 *       "avg_acknowledgment_time_ms": 3600.0, "overall_effectiveness": 0.89
 *     }
 *   }
 * }
 */
int notifications_comprehensive_ubus_get_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                                   struct ubus_request_data *req, const char *method,
                                                   struct blob_attr *msg);

/**
 * Get notification history with filtering
 * Parameters: {
 *   "limit": 50,               // Optional limit (default 20)
 *   "start_time": 1703120000,  // Optional start time filter
 *   "end_time": 1703130000,    // Optional end time filter
 *   "type": "network_failover", // Optional type filter
 *   "priority": "high"         // Optional priority filter
 * }
 * Returns: {
 *   "success": true,
 *   "history": {
 *     "total_matching": 45, "returned": 20,
 *     "notifications": [
 *       {
 *         "id": "notif_1_1703123456_42", "type": "network_failover",
 *         "priority": "high", "title": "Network Interface Failed",
 *         "status": "delivered", "created_at": 1703123456,
 *         "delivery_confidence": 0.85, "channels_used": 2,
 *         "acknowledgment_required": true, "acknowledged": false
 *       }
 *     ]
 *   }
 * }
 */
int notifications_comprehensive_ubus_get_history(struct ubus_context *ctx, struct ubus_object *obj,
                                                struct ubus_request_data *req, const char *method,
                                                struct blob_attr *msg);

/**
 * Acknowledge notification
 * Parameters: {
 *   "notification_id": "notif_1_1703123456_42",
 *   "acknowledged_by": "admin"
 * }
 * Returns: {
 *   "success": true,
 *   "acknowledgment": {
 *     "notification_id": "notif_1_1703123456_42",
 *     "acknowledged_at": 1703123500,
 *     "acknowledged_by": "admin",
 *     "was_expired": false
 *   }
 * }
 */
int notifications_comprehensive_ubus_acknowledge(struct ubus_context *ctx, struct ubus_object *obj,
                                                struct ubus_request_data *req, const char *method,
                                                struct blob_attr *msg);

/**
 * Test all notification channels
 * Parameters: {
 *   "test_message": "Test notification from Autonomy system"  // Optional
 * }
 * Returns: {
 *   "success": true,
 *   "channel_tests": {
 *     "pushover": {"tested": true, "success": true, "response_time_ms": 850.5},
 *     "email": {"tested": true, "success": true, "response_time_ms": 1250.2},
 *     "sms": {"tested": true, "success": false, "error": "No SMS gateway configured"},
 *     "webhook": {"tested": true, "success": true, "response_time_ms": 450.1},
 *     "slack": {"tested": false, "reason": "disabled"},
 *     "discord": {"tested": false, "reason": "disabled"},
 *     "telegram": {"tested": false, "reason": "disabled"}
 *   },
 *   "overall_result": "6/7 channels working"
 * }
 */
int notifications_comprehensive_ubus_test_channels(struct ubus_context *ctx, struct ubus_object *obj,
                                                  struct ubus_request_data *req, const char *method,
                                                  struct blob_attr *msg);

/**
 * Get channel effectiveness scores
 * Returns: {
 *   "success": true,
 *   "channel_effectiveness": {
 *     "pushover": {"effectiveness": 0.95, "recent_success_rate": 0.97, "avg_response_time_ms": 850.5},
 *     "email": {"effectiveness": 0.92, "recent_success_rate": 0.94, "avg_response_time_ms": 1250.2},
 *     "sms": {"effectiveness": 0.88, "recent_success_rate": 0.90, "avg_response_time_ms": 2500.1},
 *     "webhook": {"effectiveness": 0.85, "recent_success_rate": 0.87, "avg_response_time_ms": 450.1}
 *   },
 *   "recommendations": [
 *     {"channel": "pushover", "recommendation": "optimal_performance"},
 *     {"channel": "sms", "recommendation": "slow_response_time"}
 *   ]
 * }
 */
int notifications_comprehensive_ubus_get_channel_effectiveness(struct ubus_context *ctx, struct ubus_object *obj,
                                                              struct ubus_request_data *req, const char *method,
                                                              struct blob_attr *msg);

/**
 * Get comprehensive notification configuration
 * Returns: {
 *   "success": true,
 *   "config": {
 *     "enabled": true, "intelligence_enabled": true,
 *     "acknowledgment_tracking_enabled": true,
 *     "delivery_optimization_enabled": true,
 *     "rate_limiting": {
 *       "max_per_hour": 100, "max_per_minute": 10, "burst_limit": 5,
 *       "emergency_rate_limit": 20, "high_rate_limit": 50,
 *       "normal_rate_limit": 80, "low_rate_limit": 100
 *     },
 *     "deduplication": {
 *       "enabled": true, "window_seconds": 300, "similarity_threshold": 0.8
 *     },
 *     "channels": {
 *       "pushover": true, "email": true, "sms": false,
 *       "webhook": true, "slack": false, "discord": false, "telegram": false
 *     },
 *     "intelligence": {
 *       "priority_optimization": true, "channel_intelligence": true,
 *       "emergency_detection": true, "learning_enabled": true
 *     }
 *   }
 * }
 */
int notifications_comprehensive_ubus_get_config(struct ubus_context *ctx, struct ubus_object *obj,
                                               struct ubus_request_data *req, const char *method,
                                               struct blob_attr *msg);

/**
 * Set comprehensive notification configuration
 * Parameters: {
 *   "enabled": true,
 *   "intelligence_enabled": true,
 *   "max_per_hour": 120,
 *   "deduplication_enabled": true,
 *   "pushover_enabled": true,
 *   "email_enabled": true
 * }
 * Returns: {
 *   "success": true,
 *   "message": "Comprehensive notifications configuration updated"
 * }
 */
int notifications_comprehensive_ubus_set_config(struct ubus_context *ctx, struct ubus_object *obj,
                                               struct ubus_request_data *req, const char *method,
                                               struct blob_attr *msg);

/**
 * Reset comprehensive notification statistics
 * Returns: {
 *   "success": true,
 *   "message": "Comprehensive notifications statistics reset"
 * }
 */
int notifications_comprehensive_ubus_reset_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                                     struct ubus_request_data *req, const char *method,
                                                     struct blob_attr *msg);

/**
 * Perform comprehensive notifications health check
 * Returns: {
 *   "success": true,
 *   "health": {
 *     "overall": "healthy", "intelligence_engine": "working",
 *     "delivery_system": "optimal", "acknowledgment_tracker": "working",
 *     "channels": {
 *       "pushover": "healthy", "email": "healthy", "sms": "disabled",
 *       "webhook": "healthy", "slack": "disabled"
 *     },
 *     "performance": {
 *       "avg_processing_time_ms": 95.5, "success_rate": 0.944,
 *       "channel_effectiveness": 0.89
 *     }
 *   }
 * }
 */
int notifications_comprehensive_ubus_health_check(struct ubus_context *ctx, struct ubus_object *obj,
                                                 struct ubus_request_data *req, const char *method,
                                                 struct blob_attr *msg);

// UBUS method definitions
extern const struct ubus_method notifications_comprehensive_ubus_methods[];
extern const int notifications_comprehensive_ubus_methods_count;

#ifdef __cplusplus
}
#endif

#endif // NOTIFICATIONS_COMPREHENSIVE_UBUS_H