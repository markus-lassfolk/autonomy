#ifndef DELIVERY_OPTIMIZER_H
#define DELIVERY_OPTIMIZER_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "notification_types.h"

// Delivery optimization system

// Optimization configuration
typedef struct {
    bool enable_load_balancing;
    bool enable_failover;
    bool enable_retry_optimization;
    int max_retry_attempts;
    int retry_delay_seconds;
    int failover_timeout_seconds;
    float success_rate_threshold;
} delivery_optimizer_config_t;

// Delivery route structure
typedef struct {
    char route_id[64];
    notification_method_t method;
    char endpoint[512];
    int priority;
    float success_rate;
    int average_latency_ms;
    bool is_available;
    time_t last_used;
    int consecutive_failures;
} delivery_route_t;

// Function declarations
int delivery_optimizer_init(const delivery_optimizer_config_t *config);
void delivery_optimizer_cleanup(void);
int delivery_optimizer_select_route(const notification_t *notification, delivery_route_t *route);
int delivery_optimizer_register_route(const delivery_route_t *route);
int delivery_optimizer_update_route_stats(const char *route_id, bool success, int latency_ms);
int delivery_optimizer_get_available_routes(delivery_route_t *routes, int max_routes);
int delivery_optimizer_optimize_routes(void);

#endif // DELIVERY_OPTIMIZER_H
