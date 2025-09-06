#ifndef OPENCELLID_UBUS_H
#define OPENCELLID_UBUS_H

#include "../core/types.h"
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <stdbool.h>
#include <math.h>
#include <fcntl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get current position via triangulation
 * Returns: {
 *   "success": true,
 *   "position": {
 *     "latitude": 59.3293,
 *     "longitude": 18.0686,
 *     "accuracy": 150.0,
 *     "confidence": 0.85,
 *     "method": "triangulation",
 *     "cells_used": 4,
 *     "timestamp": 1703123456
 *   },
 *   "primary_cell": {
 *     "mcc": 240, "mnc": 1, "lac": 12345, "cell_id": 67890,
 *     "radio": "LTE", "rsrp": -85, "rsrq": -12
 *   }
 * }
 */
int opencellid_ubus_get_position(struct ubus_context *ctx, struct ubus_object *obj,
                                struct ubus_request_data *req, const char *method,
                                struct blob_attr *msg);

/**
 * Get visible cell towers for map display
 * Parameters: {
 *   "center_lat": 59.3293,     // Center latitude (optional, uses current if not provided)
 *   "center_lon": 18.0686,     // Center longitude (optional)
 *   "radius": 5000,            // Search radius in meters (default: 2000)
 *   "max_towers": 20           // Maximum towers to return (default: 10)
 * }
 * Returns: {
 *   "success": true,
 *   "towers": [
 *     {
 *       "mcc": 240, "mnc": 1, "lac": 12345, "cell_id": 67890,
 *       "radio": "LTE", "latitude": 59.3293, "longitude": 18.0686,
 *       "range": 500, "confidence": 0.9, "samples": 150,
 *       "is_serving": true, "is_neighbor": false,
 *       "rsrp": -85, "rsrq": -12, "distance": 0
 *     }
 *   ],
 *   "center": {"latitude": 59.3293, "longitude": 18.0686},
 *   "accuracy_circle": {"radius": 150.0, "confidence": 0.85},
 *   "count": 5
 * }
 */
int opencellid_ubus_get_visible_towers(struct ubus_context *ctx, struct ubus_object *obj,
                                      struct ubus_request_data *req, const char *method,
                                      struct blob_attr *msg);

/**
 * Get cellular environment (serving + neighbor cells)
 * Returns: {
 *   "success": true,
 *   "serving_cell": {
 *     "mcc": 240, "mnc": 1, "lac": 12345, "cell_id": 67890,
 *     "radio": "LTE", "pci": 123, "earfcn": 1850,
 *     "rsrp": -85, "rsrq": -12, "sinr": 15,
 *     "timing_advance": 5, "timing_advance_distance": 750.0,
 *     "operator": "Telia", "registered": true
 *   },
 *   "neighbor_cells": [
 *     {
 *       "mcc": 240, "mnc": 1, "lac": 12346, "cell_id": 67891,
 *       "radio": "LTE", "pci": 124, "earfcn": 1850,
 *       "rsrp": -95, "rsrq": -15, "type": "intra"
 *     }
 *   ],
 *   "scan_time": 1703123456,
 *   "environment_hash": "a1b2c3d4...",
 *   "gps_location": {"latitude": 59.3293, "longitude": 18.0686, "accuracy": 10.0}
 * }
 */
int opencellid_ubus_get_cellular_environment(struct ubus_context *ctx, struct ubus_object *obj,
                                           struct ubus_request_data *req, const char *method,
                                           struct blob_attr *msg);

/**
 * Get OpenCellID system statistics
 * Returns: {
 *   "success": true,
 *   "statistics": {
 *     "api": {
 *       "total_lookups": 1234,
 *       "successful_lookups": 1200,
 *       "failed_lookups": 34,
 *       "rate_limited_lookups": 5,
 *       "average_response_time_ms": 250.5
 *     },
 *     "cache": {
 *       "hits": 5678, "misses": 1234,
 *       "hit_ratio": 0.82, "entries": 15000,
 *       "size_mb": 12.5
 *     },
 *     "triangulation": {
 *       "total_performed": 456,
 *       "single_cell": 123, "multi_cell": 333,
 *       "average_accuracy": 185.5, "average_confidence": 0.78
 *     },
 *     "contribution": {
 *       "total_sent": 89, "successful": 85, "failed": 4,
 *       "queued": 2
 *     },
 *     "health": {
 *       "healthy": true, "consecutive_failures": 0,
 *       "last_success": 1703123456
 *     }
 *   }
 * }
 */
int opencellid_ubus_get_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                  struct ubus_request_data *req, const char *method,
                                  struct blob_attr *msg);

/**
 * Get OpenCellID system configuration
 * Returns: {
 *   "success": true,
 *   "config": {
 *     "enabled": true,
 *     "api_key_configured": true,
 *     "base_url": "https://opencellid.org",
 *     "timeout_seconds": 30,
 *     "cache": {
 *       "max_size_mb": 25, "ttl_hours": 24,
 *       "negative_ttl_hours": 6
 *     },
 *     "rate_limiter": {
 *       "max_lookups_per_hour": 120,
 *       "max_contributions_per_hour": 12
 *     },
 *     "contribution": {
 *       "enabled": true, "interval_minutes": 30,
 *       "min_gps_accuracy": 20.0
 *     },
 *     "triangulation": {
 *       "timing_advance_enabled": true,
 *       "min_cells_for_triangulation": 3
 *     }
 *   }
 * }
 */
int opencellid_ubus_get_config(struct ubus_context *ctx, struct ubus_object *obj,
                              struct ubus_request_data *req, const char *method,
                              struct blob_attr *msg);

/**
 * Update OpenCellID system configuration
 * Parameters: {
 *   "enabled": true,                    // Enable/disable system
 *   "api_key": "your-api-key",         // API key (optional)
 *   "cache_size_mb": 30,               // Cache size (optional)
 *   "contribution_enabled": true,       // Enable contributions (optional)
 *   "contribution_interval": 45,       // Contribution interval in minutes (optional)
 *   "min_gps_accuracy": 15.0           // Min GPS accuracy for contributions (optional)
 * }
 * Returns: {
 *   "success": true,
 *   "message": "Configuration updated successfully"
 * }
 */
int opencellid_ubus_set_config(struct ubus_context *ctx, struct ubus_object *obj,
                              struct ubus_request_data *req, const char *method,
                              struct blob_attr *msg);

/**
 * Perform manual triangulation (useful for testing)
 * Returns: {
 *   "success": true,
 *   "position": {
 *     "latitude": 59.3293, "longitude": 18.0686,
 *     "accuracy": 150.0, "confidence": 0.85,
 *     "method": "triangulation", "cells_used": 4
 *   }
 * }
 */
int opencellid_ubus_triangulate(struct ubus_context *ctx, struct ubus_object *obj,
                               struct ubus_request_data *req, const char *method,
                               struct blob_attr *msg);

/**
 * Force contribution of current environment (if GPS available)
 * Returns: {
 *   "success": true,
 *   "message": "Contribution queued successfully",
 *   "environment": {
 *     "serving_cell": {...},
 *     "neighbor_count": 3,
 *     "gps_accuracy": 8.5
 *   }
 * }
 */
int opencellid_ubus_contribute_now(struct ubus_context *ctx, struct ubus_object *obj,
                                  struct ubus_request_data *req, const char *method,
                                  struct blob_attr *msg);

/**
 * Clear OpenCellID cache
 * Parameters: {
 *   "confirm": true                     // Required confirmation
 * }
 * Returns: {
 *   "success": true,
 *   "message": "Cache cleared successfully",
 *   "entries_removed": 15000
 * }
 */
int opencellid_ubus_clear_cache(struct ubus_context *ctx, struct ubus_object *obj,
                               struct ubus_request_data *req, const char *method,
                               struct blob_attr *msg);

/**
 * Reset OpenCellID statistics
 * Returns: {
 *   "success": true,
 *   "message": "Statistics reset successfully"
 * }
 */
int opencellid_ubus_reset_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                    struct ubus_request_data *req, const char *method,
                                    struct blob_attr *msg);

/**
 * Perform system health check
 * Returns: {
 *   "success": true,
 *   "health": {
 *     "overall": "healthy",
 *     "api": "healthy",
 *     "cache": "healthy", 
 *     "rate_limiter": "healthy",
 *     "contribution_manager": "healthy"
 *   },
 *   "checks": [
 *     {"component": "api", "status": "ok", "message": "API responding normally"},
 *     {"component": "cache", "status": "ok", "message": "Cache operational"},
 *     {"component": "rate_limiter", "status": "warning", "message": "Approaching limits"}
 *   ]
 * }
 */
int opencellid_ubus_health_check(struct ubus_context *ctx, struct ubus_object *obj,
                                struct ubus_request_data *req, const char *method,
                                struct blob_attr *msg);

// UBUS method definitions
extern const struct ubus_method opencellid_ubus_methods[];
extern const int opencellid_ubus_methods_count;

#ifdef __cplusplus
}
#endif

#endif // OPENCELLID_UBUS_H