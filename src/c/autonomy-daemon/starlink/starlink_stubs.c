#include "starlink_types.h"
#include "starlink_modules.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Stub implementations for missing starlink functions

int starlink_client_init(const starlink_config_t *config) {
    // Stub implementation
    return 0;
}

int starlink_get_status(starlink_status_response_t *status) {
    // Stub implementation - return mock data
    if (!status) return -1;
    
    memset(status, 0, sizeof(starlink_status_response_t));
    
    // Fill in some mock data
    strcpy(status->device_info.id, "stub-device-001");
    strcpy(status->device_info.hardware_version, "v1.0");
    strcpy(status->device_info.software_version, "v2.0");
    strcpy(status->device_info.country_code, "US");
    status->device_info.lat = 37.7749;
    status->device_info.lon = -122.4194;
    status->device_state.uptime_s = time(NULL);
    status->gps_stats.gps_valid = 1;
    status->gps_stats.gps_sats = 12;
    
    return 0;
}

int starlink_get_collector_stats(int *cache_hits, int *cache_misses, int *errors, int *successes) {
    // Stub implementation
    if (cache_hits) *cache_hits = 100;
    if (cache_misses) *cache_misses = 10;
    if (errors) *errors = 2;
    if (successes) *successes = 108;
    return 0;
}

int starlink_force_collect(starlink_collection_result_t *result) {
    // Stub implementation
    if (!result) return -1;
    
    memset(result, 0, sizeof(starlink_collection_result_t));
    result->success = 1;
    result->collection_time = time(NULL);
    result->health.overall_score = 85;
    strcpy(result->health.status, "good");
    result->health.is_healthy = 1;
    
    // Fill in status data
    starlink_get_status(&result->status);
    
    return 0;
}

void starlink_client_cleanup(void) {
    // Stub implementation
}

int starlink_cluster_find_best_starlink(void) {
    // Stub implementation - return index 0
    return 0;
}

int starlink_cluster_failover_to(int index, const char *reason) {
    // Stub implementation
    return 0;
}
