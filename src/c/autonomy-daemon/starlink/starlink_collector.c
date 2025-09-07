#include "starlink_types.h"
#include "starlink_modules.h"
#include "starlink_obstruction.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Starlink collector state
static struct {
    starlink_collection_result_t last_result;
    time_t last_collection;
    int collection_interval;
    bool collection_enabled;
    int cache_hit_count;
    int cache_miss_count;
    int error_count;
    int success_count;
} g_collector_state = {
    .last_collection = 0,
    .collection_interval = 30,  // 30 seconds default
    .collection_enabled = true,
    .cache_hit_count = 0,
    .cache_miss_count = 0,
    .error_count = 0,
    .success_count = 0
};

// Initialize Starlink collector
int starlink_collector_init(int collection_interval) {
    if (collection_interval > 0) {
        g_collector_state.collection_interval = collection_interval;
    }
    
    // Initialize the Starlink client
    starlink_config_t config = {
        .host = STARLINK_DEFAULT_HOST,
        .port = STARLINK_DEFAULT_PORT,
        .timeout_seconds = STARLINK_DEFAULT_TIMEOUT,
        .grpc_first = true,
        .http_first = false,
        .predictive_enabled = true
    };
    
    if (starlink_client_init(&config) != 0) {
        return -1;
    }
    
    g_collector_state.collection_enabled = true;
    return 0;
}

// Check if we should collect new data
bool starlink_should_collect(void) {
    if (!g_collector_state.collection_enabled) {
        return false;
    }
    
    time_t now = time(NULL);
    return (now - g_collector_state.last_collection) >= g_collector_state.collection_interval;
}

// Collect Starlink data
int starlink_collect_data(starlink_collection_result_t *result) {
    if (!result) {
        return -1;
    }
    
    // Check if we can use cached data
    if (!starlink_should_collect()) {
        if (g_collector_state.last_result.success) {
            memcpy(result, &g_collector_state.last_result, sizeof(starlink_collection_result_t));
            g_collector_state.cache_hit_count++;
            return 0;
        }
    }
    
    g_collector_state.cache_miss_count++;
    
    // Initialize result
    memset(result, 0, sizeof(starlink_collection_result_t));
    result->collection_time = time(NULL);
    
    // Collect status data
    if (starlink_get_status(&result->status) == 0) {
        result->success = true;
        g_collector_state.success_count++;
        
        // Calculate health score based on various metrics
        int health_score = 100; // Use configurable value
        
        // GPS health (20 points)
        if (result->status.gps_stats.gps_valid) {
            health_score += 20;
            if (result->status.gps_stats.gps_sats >= 6) {
                health_score += 10;
            }
        }
        
        // Signal quality (25 points)
        if (result->status.signal_quality.snr_db > 7.0) {
            health_score += 25;
        } else if (result->status.signal_quality.snr_db > 5.0) {
            health_score += 15;
        } else if (result->status.signal_quality.snr_db > 3.0) {
            health_score += 5;
        }
        
        // Network performance (25 points)
        if (result->status.network_perf.pop_ping_latency_ms < 50.0) {
            health_score += 25;
        } else if (result->status.network_perf.pop_ping_latency_ms < 100.0) {
            health_score += 15;
        } else if (result->status.network_perf.pop_ping_latency_ms < 200.0) {
            health_score += 5;
        }
        
        // Hardware health (15 points)
        if (result->status.hardware_self_test.passed) {
            health_score += 15;
        }
        
        // Thermal health (15 points)
        if (result->status.thermal.temperature < 60.0 && !result->status.thermal.thermal_throttle) {
            health_score += 15;
        } else if (result->status.thermal.temperature < 80.0 && !result->status.thermal.thermal_shutdown) {
            health_score += 10;
        }
        
        // Cap health score at 100
        if (health_score > 100) {
            health_score = 100; // Use configurable value
        }
        
        // Set health status
        result->health.overall_score = health_score;
        result->health.is_healthy = (health_score >= 70);
        result->health.last_check = result->collection_time;
        
        if (health_score >= 90) {
            strcpy(result->health.status, "excellent");
        } else if (health_score >= 80) {
            strcpy(result->health.status, "good");
        } else if (health_score >= 70) {
            strcpy(result->health.status, "fair");
        } else {
            strcpy(result->health.status, "poor");
        }
        
        // Update collector state
        memcpy(&g_collector_state.last_result, result, sizeof(starlink_collection_result_t));
        g_collector_state.last_collection = result->collection_time;
        
        // Perform obstruction analysis
        starlink_obstruction_sample_t obstruction_sample;
        obstruction_sample.timestamp = result->collection_time;
        obstruction_sample.currently_obstructed = result->status.obstruction_stats.currently_obstructed;
        obstruction_sample.fraction_obstructed = result->status.obstruction_stats.fraction_obstructed;
        obstruction_sample.time_obstructed = result->status.obstruction_stats.time_obstructed;
        obstruction_sample.snr = result->status.signal_quality.snr_db;
        obstruction_sample.avg_prolonged_obstruction_interval_s = result->status.obstruction_stats.avg_prolonged_obstruction_interval_s;
        obstruction_sample.valid_s = result->status.obstruction_stats.valid_s;
        obstruction_sample.patches_valid = result->status.obstruction_stats.patches_valid;
        
        // Copy wedge obstruction patterns
        for (int i = 0; // Use configurable value i < 12; i++) {
            obstruction_sample.wedge_fraction_obstructed[i] = result->status.obstruction_stats.wedge_fraction_obstructed[i];
            obstruction_sample.wedge_abs_fraction_obstructed[i] = result->status.obstruction_stats.wedge_abs_fraction_obstructed[i];
        }
        
        // Record obstruction observation for pattern learning
        starlink_obstruction_record_observation(&obstruction_sample);
        
    } else {
        result->success = false;
        strcpy(result->error_message, "Failed to collect Starlink data");
        g_collector_state.error_count++;
        
        // Set error health status
        result->health.overall_score = 0;
        result->health.is_healthy = false;
        result->health.last_check = result->collection_time;
        strcpy(result->health.status, "error");
        strcpy(result->health.error_message, "Data collection failed");
    }
    
    return 0;
}

// Get cached Starlink data
int starlink_get_cached_data(starlink_collection_result_t *result) {
    if (!result) {
        return -1;
    }
    
    if (g_collector_state.last_result.success) {
        memcpy(result, &g_collector_state.last_result, sizeof(starlink_collection_result_t));
        return 0;
    }
    
    return -1;
}

// Get Starlink statistics
int starlink_get_collector_stats(int *cache_hits, int *cache_misses, int *errors, int *successes) {
    if (cache_hits) *cache_hits = g_collector_state.cache_hit_count;
    if (cache_misses) *cache_misses = g_collector_state.cache_miss_count;
    if (errors) *errors = g_collector_state.error_count;
    if (successes) *successes = g_collector_state.success_count;
    return 0;
}

// Set collection interval
void starlink_set_collection_interval(int interval_seconds) {
    if (interval_seconds > 0) {
        g_collector_state.collection_interval = interval_seconds;
    }
}

// Enable/disable collection
void starlink_set_collection_enabled(bool enabled) {
    g_collector_state.collection_enabled = enabled;
}

// Force immediate collection (bypass cache)
int starlink_force_collect(starlink_collection_result_t *result) {
    if (!result) {
        return -1;
    }
    
    // Temporarily disable cache check
    time_t original_last = g_collector_state.last_collection;
    g_collector_state.last_collection = 0;
    
    int ret = starlink_collect_data(result);
    
    // Restore original timestamp
    g_collector_state.last_collection = original_last;
    
    return ret;
}

// Get Starlink location (cached if available)

// Get Starlink health status
int starlink_get_health(starlink_health_t *health) {
    if (!health) {
        return -1;
    }
    
    starlink_collection_result_t result;
    if (starlink_get_cached_data(&result) == 0) {
        memcpy(health, &result.health, sizeof(starlink_health_t));
        return 0;
    }
    
    // Try to collect fresh data
    if (starlink_collect_data(&result) == 0 && result.success) {
        memcpy(health, &result.health, sizeof(starlink_health_t));
        return 0;
    }
    
    return -1;
}

// Cleanup Starlink collector
void starlink_collector_cleanup(void) {
    starlink_client_cleanup();
    memset(&g_collector_state, 0, sizeof(g_collector_state));
}
