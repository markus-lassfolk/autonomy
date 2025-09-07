#ifndef GPS_OPENCELLID_ENHANCED_H
#define GPS_OPENCELLID_ENHANCED_H

#include "gps_opencellid.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// Enhanced rate limiting strategies
typedef enum {
    RATE_LIMIT_STRATEGY_SIMPLE = 0,
    RATE_LIMIT_STRATEGY_DUAL,
    RATE_LIMIT_STRATEGY_RATIO_BASED,
    RATE_LIMIT_STRATEGY_ADAPTIVE,
    RATE_LIMIT_STRATEGY_MAX
} rate_limit_strategy_t;

// Enhanced cache management
typedef struct {
    bool enabled;
    int max_size_mb;
    int ttl_hours;
    int negative_cache_ttl_hours;
    double hit_ratio_threshold;
    bool intelligent_eviction;
    bool predictive_prefetch;
} enhanced_cache_config_t;

// Rate limiter configuration
typedef struct {
    rate_limit_strategy_t strategy;
    int requests_per_hour;
    int requests_per_day;
    int burst_limit;
    double success_ratio_threshold;
    int adaptive_window_minutes;
    bool emergency_bypass;
} rate_limiter_config_t;

// Cellular data fusion configuration
typedef struct {
    bool enabled;
    double confidence_threshold;
    int min_cells_for_triangulation;
    double timing_advance_weight;
    double signal_strength_weight;
    double neighbor_cell_weight;
    int fusion_window_seconds;
} cellular_fusion_config_t;

// Contribution management configuration
typedef struct {
    bool enabled;
    int contribution_interval_minutes;
    double min_gps_accuracy_meters;
    double movement_threshold_meters;
    double rsrp_change_threshold_db;
    bool timing_advance_enabled;
    double max_speed_kmh;
    int batch_size;
    int retry_attempts;
} contribution_config_t;

// Enhanced OpenCellID configuration
typedef struct {
    opencellid_config_t base_config;
    enhanced_cache_config_t cache_config;
    rate_limiter_config_t rate_limiter_config;
    cellular_fusion_config_t fusion_config;
    contribution_config_t contribution_config;
    
    // Health monitoring
    bool health_monitoring_enabled;
    int health_check_interval_minutes;
    double min_success_rate;
    int max_consecutive_failures;
    
    // Advanced features
    bool hysteresis_enabled;
    int consecutive_good_threshold;
    int consecutive_bad_threshold;
    bool predictive_caching;
    bool location_clustering;
} enhanced_opencellid_config_t;

// API statistics with detailed metrics
typedef struct {
    // Request statistics
    uint64_t total_requests;
    uint64_t successful_requests;
    uint64_t failed_requests;
    uint64_t rate_limited_requests;
    uint64_t cached_responses;
    
    // Performance metrics
    double average_response_time_ms;
    double success_rate;
    double cache_hit_rate;
    
    // Rate limiting stats
    uint64_t rate_limit_violations;
    time_t last_rate_limit;
    int current_requests_per_hour;
    
    // Contribution statistics
    uint64_t contributions_sent;
    uint64_t contribution_failures;
    time_t last_contribution;
    
    // Health metrics
    int consecutive_failures;
    int consecutive_successes;
    time_t last_health_check;
    bool healthy;
    
    // Timing
    time_t last_request;
    time_t first_request;
    time_t stats_reset_time;
} enhanced_opencellid_stats_t;

// Enhanced OpenCellID structure
typedef struct {
    enhanced_opencellid_config_t config;
    enhanced_opencellid_stats_t stats;
    
    // Components
    opencellid_t* base_opencellid;
    
    // Enhanced cache (would be implemented separately)
    void* enhanced_cache;
    
    // Rate limiters
    void* simple_rate_limiter;
    void* dual_rate_limiter;
    void* ratio_rate_limiter;
    void* adaptive_rate_limiter;
    
    // Cellular fusion engine
    void* fusion_engine;
    
    // Contribution manager
    void* contribution_manager;
    
    // Health monitor
    void* health_monitor;
    
    // Thread safety
    pthread_mutex_t mutex;
    
    // Background threads
    pthread_t health_thread;
    pthread_t contribution_thread;
    bool threads_running;
} enhanced_opencellid_t;

// Function prototypes for enhanced OpenCellID

/**
 * Initialize enhanced OpenCellID system
 * @param config Enhanced configuration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_opencellid_enhanced_init(const enhanced_opencellid_config_t* config);

/**
 * Cleanup enhanced OpenCellID system
 */
void gps_opencellid_enhanced_cleanup(void);

/**
 * Lookup cell tower location with enhanced features
 * @param cell_key Cell tower key
 * @param response Response structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_opencellid_enhanced_lookup(const opencellid_cell_key_t* cell_key, opencellid_response_t* response);

/**
 * Perform cellular data fusion for improved accuracy
 * @param serving_cell Serving cell information
 * @param neighbor_cells Array of neighbor cells
 * @param neighbor_count Number of neighbor cells
 * @param fused_location Fused location result
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_opencellid_enhanced_fusion(const void* serving_cell, const void* neighbor_cells, 
                                   int neighbor_count, opencellid_response_t* fused_location);

/**
 * Start background contribution manager
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_opencellid_enhanced_start_contribution_manager(void);

/**
 * Stop background contribution manager
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_opencellid_enhanced_stop_contribution_manager(void);

/**
 * Get enhanced statistics
 * @param stats Statistics structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_opencellid_enhanced_get_stats(enhanced_opencellid_stats_t* stats);

/**
 * Perform health check
 * @return AUTONOMY_SUCCESS if healthy, error code if unhealthy
 */
int gps_opencellid_enhanced_health_check(void);

/**
 * Check if enhanced OpenCellID is initialized
 * @return true if initialized, false otherwise
 */
bool gps_opencellid_enhanced_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_OPENCELLID_ENHANCED_H