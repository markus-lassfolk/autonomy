#ifndef EXTERNAL_APIS_UBUS_H
#define EXTERNAL_APIS_UBUS_H

#include <libubus.h>
#include <libubox/blobmsg_json.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get elevation data for coordinates
 * Parameters: {
 *   "latitude": 59.329444,
 *   "longitude": 18.068611
 * }
 * Returns: {
 *   "success": true,
 *   "elevation": {
 *     "elevation": 25.5,
 *     "resolution": 30.0,
 *     "source": "open_elevation",
 *     "timestamp": 1703123456
 *   }
 * }
 */
int external_apis_ubus_get_elevation(struct ubus_context *ctx, struct ubus_object *obj,
                                    struct ubus_request_data *req, const char *method,
                                    struct blob_attr *msg);

/**
 * Get weather data for coordinates
 * Parameters: {
 *   "latitude": 59.329444,
 *   "longitude": 18.068611
 * }
 * Returns: {
 *   "success": true,
 *   "weather": {
 *     "temperature_celsius": 15.5,
 *     "humidity_percent": 65.0,
 *     "pressure_hpa": 1013.25,
 *     "wind_speed_ms": 3.2,
 *     "wind_direction_deg": 225.0,
 *     "visibility_km": 10.0,
 *     "description": "partly cloudy",
 *     "icon": "02d",
 *     "source": "openweathermap",
 *     "timestamp": 1703123456
 *   }
 * }
 */
int external_apis_ubus_get_weather(struct ubus_context *ctx, struct ubus_object *obj,
                                  struct ubus_request_data *req, const char *method,
                                  struct blob_attr *msg);

/**
 * Get reverse geocoding for coordinates
 * Parameters: {
 *   "latitude": 59.329444,
 *   "longitude": 18.068611
 * }
 * Returns: {
 *   "success": true,
 *   "location": {
 *     "latitude": 59.329444,
 *     "longitude": 18.068611,
 *     "accuracy": 50.0,
 *     "confidence": 0.9,
 *     "formatted_address": "Gamla Stan, Stockholm, Sweden",
 *     "country": "Sweden",
 *     "region": "Stockholm County",
 *     "city": "Stockholm",
 *     "postal_code": "111 29",
 *     "source": "google_geocoding",
 *     "timestamp": 1703123456
 *   }
 * }
 */
int external_apis_ubus_get_reverse_geocoding(struct ubus_context *ctx, struct ubus_object *obj,
                                           struct ubus_request_data *req, const char *method,
                                           struct blob_attr *msg);

/**
 * Get external API statistics
 * Returns: {
 *   "success": true,
 *   "apis": [
 *     {
 *       "api": "google_elevation",
 *       "enabled": true,
 *       "status": "healthy",
 *       "total_requests": 1250,
 *       "successful_requests": 1200,
 *       "failed_requests": 50,
 *       "success_rate": 0.96,
 *       "avg_response_time_ms": 250.5,
 *       "requests_this_hour": 45,
 *       "requests_this_day": 1250,
 *       "cost_this_month": 6.25,
 *       "consecutive_failures": 0,
 *       "last_success": 1703123456,
 *       "health_score": 0.96
 *     }
 *   ]
 * }
 */
int external_apis_ubus_get_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                     struct ubus_request_data *req, const char *method,
                                     struct blob_attr *msg);

/**
 * Get external API configuration
 * Returns: {
 *   "success": true,
 *   "apis": [
 *     {
 *       "api": "google_elevation",
 *       "enabled": true,
 *       "api_key_configured": true,
 *       "base_url": "https://maps.googleapis.com/maps/api/elevation",
 *       "timeout_seconds": 30,
 *       "max_requests_per_hour": 2500,
 *       "max_requests_per_day": 100000,
 *       "cost_per_request": 0.005,
 *       "health_monitoring": true
 *     }
 *   ]
 * }
 */
int external_apis_ubus_get_config(struct ubus_context *ctx, struct ubus_object *obj,
                                 struct ubus_request_data *req, const char *method,
                                 struct blob_attr *msg);

/**
 * Configure external API
 * Parameters: {
 *   "api": "google_elevation",
 *   "enabled": true,
 *   "api_key": "your-api-key",
 *   "timeout_seconds": 30,
 *   "max_requests_per_hour": 2500
 * }
 * Returns: {
 *   "success": true,
 *   "message": "API configuration updated successfully"
 * }
 */
int external_apis_ubus_configure(struct ubus_context *ctx, struct ubus_object *obj,
                                struct ubus_request_data *req, const char *method,
                                struct blob_attr *msg);

/**
 * Perform health check for all external APIs
 * Returns: {
 *   "success": true,
 *   "health_check": {
 *     "overall_health": "good",
 *     "apis_healthy": 5,
 *     "apis_degraded": 1,
 *     "apis_failed": 0,
 *     "apis_disabled": 2
 *   },
 *   "api_health": [
 *     {
 *       "api": "google_elevation",
 *       "status": "healthy",
 *       "success_rate": 0.96,
 *       "avg_response_time_ms": 250.5,
 *       "consecutive_failures": 0
 *     }
 *   ]
 * }
 */
int external_apis_ubus_health_check(struct ubus_context *ctx, struct ubus_object *obj,
                                   struct ubus_request_data *req, const char *method,
                                   struct blob_attr *msg);

/**
 * Reset external API statistics
 * Parameters: {
 *   "api": "google_elevation"  // Optional, resets all if not specified
 * }
 * Returns: {
 *   "success": true,
 *   "message": "Statistics reset successfully"
 * }
 */
int external_apis_ubus_reset_statistics(struct ubus_context *ctx, struct ubus_object *obj,
                                       struct ubus_request_data *req, const char *method,
                                       struct blob_attr *msg);

/**
 * Test external API connection
 * Parameters: {
 *   "api": "google_elevation",
 *   "test_coordinates": {
 *     "latitude": 59.329444,
 *     "longitude": 18.068611
 *   }
 * }
 * Returns: {
 *   "success": true,
 *   "test_result": {
 *     "api_responsive": true,
 *     "response_time_ms": 245.5,
 *     "data_quality": "good",
 *     "error_message": null
 *   }
 * }
 */
int external_apis_ubus_test_connection(struct ubus_context *ctx, struct ubus_object *obj,
                                     struct ubus_request_data *req, const char *method,
                                     struct blob_attr *msg);

// UBUS method definitions
extern const struct ubus_method external_apis_ubus_methods[];
extern const int external_apis_ubus_methods_count;

#ifdef __cplusplus
}
#endif

#endif // EXTERNAL_APIS_UBUS_H