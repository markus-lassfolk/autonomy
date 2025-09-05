#ifndef STARLINK_COMPREHENSIVE_H
#define STARLINK_COMPREHENSIVE_H

#include "starlink/starlink_types.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// Starlink event severity levels
typedef enum {
    STARLINK_EVENT_SEVERITY_INFO = 0,
    STARLINK_EVENT_SEVERITY_WARNING,
    STARLINK_EVENT_SEVERITY_CRITICAL,
    STARLINK_EVENT_SEVERITY_MAX
} starlink_event_severity_t;

// Starlink event reasons
typedef enum {
    STARLINK_EVENT_REASON_UNKNOWN = 0,
    STARLINK_EVENT_REASON_OUTAGE_NO_DOWNLINK,
    STARLINK_EVENT_REASON_OUTAGE_NO_UPLINK,
    STARLINK_EVENT_REASON_OBSTRUCTION,
    STARLINK_EVENT_REASON_THERMAL,
    STARLINK_EVENT_REASON_POWER,
    STARLINK_EVENT_REASON_SOFTWARE,
    STARLINK_EVENT_REASON_HARDWARE,
    STARLINK_EVENT_REASON_NETWORK,
    STARLINK_EVENT_REASON_MAX
} starlink_event_reason_t;

// Starlink outage causes
typedef enum {
    STARLINK_OUTAGE_CAUSE_UNKNOWN = 0,
    STARLINK_OUTAGE_CAUSE_NO_DOWNLINK,
    STARLINK_OUTAGE_CAUSE_NO_UPLINK,
    STARLINK_OUTAGE_CAUSE_OBSTRUCTION,
    STARLINK_OUTAGE_CAUSE_THERMAL,
    STARLINK_OUTAGE_CAUSE_POWER,
    STARLINK_OUTAGE_CAUSE_BACKEND,
    STARLINK_OUTAGE_CAUSE_MAINTENANCE,
    STARLINK_OUTAGE_CAUSE_MAX
} starlink_outage_cause_t;

// Starlink event structure
typedef struct {
    starlink_event_severity_t severity;    // Event severity
    starlink_event_reason_t reason;        // Event reason
    uint64_t start_timestamp_ns;           // Start timestamp in nanoseconds
    uint64_t duration_ns;                  // Duration in nanoseconds
    char message[256];                     // Human-readable message
    bool ongoing;                          // Whether event is ongoing
    time_t recorded_at;                    // When event was recorded
} starlink_event_t;

// Starlink outage structure
typedef struct {
    starlink_outage_cause_t cause;         // Outage cause
    uint64_t start_timestamp_ns;           // Start timestamp in nanoseconds
    uint64_t duration_ns;                  // Duration in nanoseconds
    bool did_switch;                       // Whether failover occurred
    time_t recorded_at;                    // When outage was recorded
    char cause_description[128];           // Cause description
} starlink_outage_t;

// Comprehensive Starlink GPS data (combining all APIs)
typedef struct {
    // Core location data (from get_location)
    double latitude;                       // Latitude
    double longitude;                      // Longitude
    double altitude;                       // Altitude in meters
    double accuracy;                       // Accuracy in meters
    double horizontal_speed_mps;           // Horizontal speed in m/s
    double vertical_speed_mps;             // Vertical speed in m/s
    char gps_source[32];                   // GPS source (GNC_FUSED, etc.)
    
    // Satellite data (from get_status)
    bool gps_valid;                        // GPS fix validity
    int gps_satellites;                    // Number of satellites
    bool no_sats_after_ttff;               // No satellites after TTFF
    bool inhibit_gps;                      // GPS inhibited
    
    // Enhanced location data (from get_diagnostics)
    bool location_enabled;                 // Location service enabled
    double uncertainty_meters;             // Uncertainty in meters
    bool uncertainty_meters_valid;         // Uncertainty validity
    double gps_time_s;                     // GPS time in seconds
    
    // Quality assessment
    bool valid;                            // Overall validity
    double confidence;                     // Confidence score (0.0-1.0)
    char quality_score[16];                // excellent, good, fair, poor
    
    // Collection metadata
    char data_sources[256];                // Comma-separated list of APIs used
    time_t collected_at;                   // When data was collected
    double collection_ms;                  // Collection time in milliseconds
} starlink_comprehensive_gps_t;

// Starlink events and outages analysis
typedef struct {
    starlink_event_t events[50];           // Recent events
    int event_count;                       // Number of events
    starlink_outage_t outages[20];         // Recent outages
    int outage_count;                      // Number of outages
    
    // Analysis results
    int critical_events_24h;               // Critical events in last 24h
    int warning_events_24h;                // Warning events in last 24h
    int total_outages_24h;                 // Total outages in last 24h
    double avg_outage_duration_s;          // Average outage duration
    double outage_frequency_per_hour;      // Outage frequency
    
    // Patterns and trends
    bool outage_pattern_detected;          // Outage pattern detected
    bool event_escalation_detected;        // Event severity escalation
    starlink_outage_cause_t primary_cause; // Primary outage cause
    double stability_score;                // Overall stability score (0.0-1.0)
    
    time_t last_analysis;                  // Last analysis time
} starlink_events_outages_analysis_t;

// Enhanced Starlink status (combining all data)
typedef struct {
    // Device information
    starlink_device_info_t device_info;    // Device information
    starlink_device_state_t device_state;  // Device state
    
    // Network performance
    double pop_ping_latency_ms;            // PoP ping latency
    double downlink_throughput_bps;        // Downlink throughput
    double uplink_throughput_bps;          // Uplink throughput
    
    // Obstruction data
    starlink_obstruction_stats_t obstruction_stats; // Obstruction statistics
    
    // GPS data
    starlink_comprehensive_gps_t gps_data; // Comprehensive GPS data
    
    // Events and outages
    starlink_events_outages_analysis_t events_analysis; // Events and outages analysis
    
    // Health and quality metrics
    double overall_health_score;           // Overall health score (0.0-1.0)
    double network_quality_score;          // Network quality score
    double gps_quality_score;              // GPS quality score
    double stability_score;                // Stability score based on events/outages
    
    // Collection metadata
    time_t last_update;                    // Last update time
    double collection_duration_ms;         // Total collection time
    char collection_status[64];            // Collection status message
    bool collection_successful;            // Whether collection was successful
} starlink_comprehensive_status_t;

// Starlink comprehensive collector configuration
typedef struct {
    bool enabled;                          // Enable comprehensive collection
    char host[64];                         // Starlink host
    int port;                              // Starlink port
    int timeout_seconds;                   // Request timeout
    int collection_interval_s;             // Collection interval
    
    // API selection
    bool collect_location;                 // Collect from get_location
    bool collect_status;                   // Collect from get_status
    bool collect_diagnostics;              // Collect from get_diagnostics
    bool collect_history;                  // Collect from get_history
    
    // Events and outages analysis
    bool enable_events_analysis;           // Enable events analysis
    bool enable_outages_analysis;          // Enable outages analysis
    int max_events;                        // Maximum events to store
    int max_outages;                       // Maximum outages to store
    int analysis_window_hours;             // Analysis window in hours
    
    // Quality thresholds
    double min_gps_confidence;             // Minimum GPS confidence
    double min_network_quality;            // Minimum network quality
    double min_stability_score;            // Minimum stability score
    
    // Health monitoring
    bool enable_health_monitoring;         // Enable health monitoring
    int health_check_interval_s;           // Health check interval
    int max_consecutive_failures;          // Max consecutive failures
} starlink_comprehensive_config_t;

// Starlink comprehensive collector structure
typedef struct {
    starlink_comprehensive_config_t config; // Configuration
    starlink_comprehensive_status_t status; // Current status
    
    // Statistics
    uint64_t total_collections;            // Total collections
    uint64_t successful_collections;       // Successful collections
    uint64_t failed_collections;           // Failed collections
    uint64_t api_calls_location;           // get_location API calls
    uint64_t api_calls_status;             // get_status API calls
    uint64_t api_calls_diagnostics;        // get_diagnostics API calls
    uint64_t api_calls_history;            // get_history API calls
    
    double average_collection_time_ms;     // Average collection time
    double average_gps_confidence;         // Average GPS confidence
    double average_stability_score;        // Average stability score
    
    // Threading
    pthread_mutex_t mutex;                 // Main mutex
    pthread_t collection_thread;           // Collection thread
    pthread_t analysis_thread;             // Events/outages analysis thread
    bool threads_running;                  // Thread status
    
    // State
    bool initialized;                      // Initialization status
    time_t last_collection;                // Last collection time
    time_t last_analysis;                  // Last analysis time
} starlink_comprehensive_collector_t;

// Function prototypes

/**
 * Initialize comprehensive Starlink collector
 * @param config Starlink configuration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_comprehensive_init(const starlink_comprehensive_config_t* config);

/**
 * Cleanup comprehensive Starlink collector
 */
void starlink_comprehensive_cleanup(void);

/**
 * Collect comprehensive Starlink data from all APIs
 * @param status Comprehensive status structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_comprehensive_collect_all(starlink_comprehensive_status_t* status);

/**
 * Collect comprehensive GPS data from multiple Starlink APIs
 * @param gps_data GPS data structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_comprehensive_collect_gps(starlink_comprehensive_gps_t* gps_data);

/**
 * Collect and analyze Starlink events
 * @param events_analysis Events analysis structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_comprehensive_analyze_events(starlink_events_outages_analysis_t* events_analysis);

/**
 * Collect and analyze Starlink outages
 * @param events_analysis Events analysis structure to fill (outages part)
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_comprehensive_analyze_outages(starlink_events_outages_analysis_t* events_analysis);

/**
 * Get current comprehensive Starlink status
 * @param status Status structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_comprehensive_get_status(starlink_comprehensive_status_t* status);

/**
 * Get Starlink stability score based on events and outages
 * @return Stability score (0.0-1.0)
 */
double starlink_comprehensive_get_stability_score(void);

/**
 * Check if comprehensive Starlink collector is initialized
 * @return true if initialized, false otherwise
 */
bool starlink_comprehensive_is_initialized(void);

/**
 * Get comprehensive Starlink statistics
 * @param total_collections Total collections
 * @param successful_collections Successful collections
 * @param failed_collections Failed collections
 * @param avg_collection_time_ms Average collection time
 * @param avg_gps_confidence Average GPS confidence
 * @param avg_stability_score Average stability score
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_comprehensive_get_statistics(uint64_t* total_collections,
                                         uint64_t* successful_collections,
                                         uint64_t* failed_collections,
                                         double* avg_collection_time_ms,
                                         double* avg_gps_confidence,
                                         double* avg_stability_score);

// Utility functions

/**
 * Convert event severity to string
 * @param severity Event severity
 * @return Severity string
 */
const char* starlink_event_severity_to_string(starlink_event_severity_t severity);

/**
 * Convert event reason to string
 * @param reason Event reason
 * @return Reason string
 */
const char* starlink_event_reason_to_string(starlink_event_reason_t reason);

/**
 * Convert outage cause to string
 * @param cause Outage cause
 * @return Cause string
 */
const char* starlink_outage_cause_to_string(starlink_outage_cause_t cause);

/**
 * Parse event severity from string
 * @param severity_str Severity string
 * @return Event severity enum
 */
starlink_event_severity_t starlink_parse_event_severity(const char* severity_str);

/**
 * Parse event reason from string
 * @param reason_str Reason string
 * @return Event reason enum
 */
starlink_event_reason_t starlink_parse_event_reason(const char* reason_str);

/**
 * Parse outage cause from string
 * @param cause_str Cause string
 * @return Outage cause enum
 */
starlink_outage_cause_t starlink_parse_outage_cause(const char* cause_str);

/**
 * Calculate GPS confidence from comprehensive data
 * @param gps_data GPS data
 * @return Confidence score (0.0-1.0)
 */
double starlink_calculate_gps_confidence(const starlink_comprehensive_gps_t* gps_data);

/**
 * Calculate stability score from events and outages
 * @param analysis Events and outages analysis
 * @return Stability score (0.0-1.0)
 */
double starlink_calculate_stability_score(const starlink_events_outages_analysis_t* analysis);

/**
 * Merge location data from get_location API
 * @param gps_data GPS data structure to update
 * @param location_response JSON response from get_location
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_merge_location_data(starlink_comprehensive_gps_t* gps_data, const char* location_response);

/**
 * Merge status data from get_status API
 * @param gps_data GPS data structure to update
 * @param status_response JSON response from get_status
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_merge_status_data(starlink_comprehensive_gps_t* gps_data, const char* status_response);

/**
 * Merge diagnostics data from get_diagnostics API
 * @param gps_data GPS data structure to update
 * @param diagnostics_response JSON response from get_diagnostics
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int starlink_merge_diagnostics_data(starlink_comprehensive_gps_t* gps_data, const char* diagnostics_response);

#ifdef __cplusplus
}
#endif

#endif // STARLINK_COMPREHENSIVE_H