#include "starlink_types.h"
#include "starlink_modules.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Stub implementations for starlink collector functions

static bool collector_initialized = false;
static int collection_interval = 30;
static bool collection_enabled = true;
static int cache_hits = 0;
static int cache_misses = 0;
static int errors = 0;
static int successes = 0;

int starlink_collector_init(int collection_interval_seconds) {
    // Stub implementation
    collection_interval = collection_interval_seconds;
    collection_enabled = true;
    collector_initialized = true;
    return 0;
}

bool starlink_should_collect(void) {
    // Stub implementation
    return collection_enabled;
}

int starlink_collect_data(starlink_collection_result_t *result) {
    // Stub implementation
    if (!result) return -1;
    
    memset(result, 0, sizeof(starlink_collection_result_t));
    
    // Simulate collection
    result->success = 1;
    result->collection_time = time(NULL);
    result->health.overall_score = 85 + (rand() % 20);
    if (result->health.overall_score > 100) result->health.overall_score = 100;
    
    strcpy(result->health.status, "good");
    result->health.is_healthy = 1;
    result->health.last_check = time(NULL);
    
    // Fill in status data
    strcpy(result->status.device_info.id, "stub-device-001");
    strcpy(result->status.device_info.hardware_version, "v1.0");
    strcpy(result->status.device_info.software_version, "v2.0");
    strcpy(result->status.device_info.country_code, "US");
    result->status.device_info.lat = 37.7749;
    result->status.device_info.lon = -122.4194;
    result->status.device_state.uptime_s = time(NULL);
    result->status.gps_stats.gps_valid = 1;
    result->status.gps_stats.gps_sats = 12;
    result->status.network_perf.pop_ping_latency_ms = 30.0;
    result->status.network_perf.downlink_throughput_bps = 100000000; // 100 Mbps
    result->status.network_perf.uplink_throughput_bps = 20000000;    // 20 Mbps
    result->status.signal_quality.snr_db = 15.0;
    result->status.signal_quality.is_snr_above_noise_floor = 1;
    result->status.signal_quality.is_snr_persistently_low = 0;
    result->status.positioning.boresight_azimuth_deg = 180.0;
    result->status.positioning.boresight_elevation_deg = 45.0;
    
    successes++;
    return 0;
}

int starlink_get_cached_data(starlink_collection_result_t *result) {
    // Stub implementation
    if (!result) return -1;
    
    // Return cached data (same as collect_data for stub)
    return starlink_collect_data(result);
}

void starlink_get_collector_stats(int *cache_hits_out, int *cache_misses_out, int *errors_out, int *successes_out) {
    // Stub implementation
    if (cache_hits_out) *cache_hits_out = cache_hits;
    if (cache_misses_out) *cache_misses_out = cache_misses;
    if (errors_out) *errors_out = errors;
    if (successes_out) *successes_out = successes;
}

void starlink_set_collection_interval(int interval_seconds) {
    // Stub implementation
    collection_interval = interval_seconds;
}

void starlink_set_collection_enabled(bool enabled) {
    // Stub implementation
    collection_enabled = enabled;
}

int starlink_force_collect(starlink_collection_result_t *result) {
    // Stub implementation
    return starlink_collect_data(result);
}

int starlink_get_location(starlink_lla_position_t *location) {
    // Stub implementation
    if (!location) return -1;
    
    location->lat = 37.7749;
    location->lon = -122.4194;
    location->alt = 0.0;
    
    return 0;
}

int starlink_get_health(starlink_health_t *health) {
    // Stub implementation
    if (!health) return -1;
    
    health->overall_score = 85 + (rand() % 20);
    if (health->overall_score > 100) health->overall_score = 100;
    
    strcpy(health->status, "good");
    health->is_healthy = 1;
    health->last_check = time(NULL);
    strcpy(health->error_message, "");
    
    return 0;
}

void starlink_collector_cleanup(void) {
    // Stub implementation
    collector_initialized = false;
    collection_enabled = false;
}
