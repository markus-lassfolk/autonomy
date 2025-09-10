#ifndef STARLINK_TYPES_H
#define STARLINK_TYPES_H

#include <time.h>
#include <stdint.h>
#include <stdbool.h>

// Forward declaration for tracking types
typedef struct dish_location dish_location_t;

// Starlink API configuration
#define STARLINK_DEFAULT_HOST "192.168.100.1"
#define STARLINK_DEFAULT_PORT 9200
#define STARLINK_DEFAULT_TIMEOUT 30

// Starlink API methods
typedef enum {
    STARLINK_METHOD_GET_STATUS = 0,
    STARLINK_METHOD_GET_HISTORY,
    STARLINK_METHOD_GET_DEVICE_INFO,
    STARLINK_METHOD_GET_LOCATION,
    STARLINK_METHOD_GET_DIAGNOSTICS
} starlink_method_t;

// Device Information
typedef struct {
    char id[64];
    char hardware_version[32];
    char software_version[32];
    char country_code[8];
    int32_t utc_offset_s;
    char software_part_number[32];
    int32_t generation_number;
    bool dish_cohoused;
    int64_t utcns_offset_ns;
    double lat;
    double lon;
} starlink_device_info_t;

// Device State
typedef struct {
    uint64_t uptime_s;
} starlink_device_state_t;

// Obstruction Statistics
typedef struct {
    bool currently_obstructed;
    double fraction_obstructed;
    int32_t valid_s;
    double wedge_fraction_obstructed[12];  // 12 wedges
    double wedge_abs_fraction_obstructed[12];
    int32_t last24h_obstructed_s;
    double time_obstructed;
    int32_t patches_valid;
    double avg_prolonged_obstruction_interval_s;
} starlink_obstruction_stats_t;

// GPS Statistics
typedef struct {
    bool gps_valid;
    int32_t gps_sats;
    int32_t no_sats_after_ttff;
    bool inhibit_gps;
} starlink_gps_stats_t;

// Location data
typedef struct {
    double lat;
    double lon;
    double alt;
} starlink_lla_position_t;

// Comprehensive Starlink GPS data structure
typedef struct {
    // Core Location Data (from get_location)
    double latitude;
    double longitude;
    double altitude;
    double accuracy;                    // sigmaM from get_location
    double horizontal_speed_mps;
    double vertical_speed_mps;
    char gps_source[32];               // GPS source (GNC_FUSED, etc.)
    
    // Satellite Data (from get_status)
    bool gps_valid;
    int32_t gps_satellites;
    bool no_sats_after_ttff;
    bool inhibit_gps;
    
    // Enhanced Data (from get_diagnostics)
    bool location_enabled;
    double uncertainty_meters;
    bool uncertainty_meters_valid;
    double gps_time_s;
    
    // Metadata
    char data_sources[128];            // Which APIs provided data
    time_t collected_at;               // When data was collected
    int64_t collection_ms;             // Time taken to collect all data
    bool valid;                        // Overall validity
    double confidence;                 // Confidence score 0.0-1.0
    char quality_score[16];            // excellent, good, fair, poor
} starlink_comprehensive_gps_t;

typedef struct {
    double x;
    double y;
    double z;
} starlink_ecef_position_t;

// Outage Information
typedef struct {
    int32_t last_outage_s;
    int32_t outage_count;
    int32_t outage_duration;
} starlink_outage_t;

// Network Performance
typedef struct {
    double pop_ping_latency_ms;
    double downlink_throughput_bps;
    double uplink_throughput_bps;
    double pop_ping_drop_rate;
    int32_t eth_speed_mbps;
} starlink_network_perf_t;

// Signal Quality
typedef struct {
    double snr;
    double snr_db;
    int32_t seconds_since_last_snr;
    bool is_snr_above_noise_floor;
    bool is_snr_persistently_low;
} starlink_signal_quality_t;

// Positioning
typedef struct {
    double boresight_azimuth_deg;
    double boresight_elevation_deg;
} starlink_positioning_t;

// Hardware Status
typedef struct {
    bool passed;
    char test_results[8][64];  // Up to 8 test results
    int32_t last_test_time;
} starlink_hardware_self_test_t;

// Thermal Status
typedef struct {
    double temperature;
    bool thermal_throttle;
    bool thermal_shutdown;
} starlink_thermal_t;

// Power Status
typedef struct {
    double power_draw;
    double voltage;
    double current;
    char power_state[16];
} starlink_power_t;

// Comprehensive Starlink Status Response
typedef struct {
    starlink_device_info_t device_info;
    starlink_device_state_t device_state;
    starlink_obstruction_stats_t obstruction_stats;
    starlink_outage_t outage;
    starlink_network_perf_t network_perf;
    starlink_signal_quality_t signal_quality;
    starlink_positioning_t positioning;
    starlink_hardware_self_test_t hardware_self_test;
    starlink_thermal_t thermal;
    starlink_power_t power;
    starlink_gps_stats_t gps_stats;
    starlink_lla_position_t location;  // GPS location data
} starlink_status_response_t;

// Starlink Client Configuration
typedef struct {
    char host[64];
    int port;
    int timeout_seconds;
    bool grpc_first;
    bool http_first;
    bool predictive_enabled;
    char interface_name[32];  // Network interface name (e.g., "wan_starlink1")
    char mwan3_member[32];    // MWAN3 member name (e.g., "starlink1")
    int priority;              // Priority for failover (lower = higher priority)
    bool enabled;              // Whether this Starlink is enabled
} starlink_config_t;

// Starlink Health Status
typedef struct {
    int overall_score;
    char status[16];
    time_t last_check;
    bool is_healthy;
    char error_message[256];
} starlink_health_t;

// Starlink Collection Result
typedef struct {
    starlink_status_response_t status;
    starlink_health_t health;
    time_t collection_time;
    bool success;
    char error_message[256];
} starlink_collection_result_t;

// Multi-Starlink Management
#define MAX_STARLINKS 8

typedef struct {
    char id[32];                           // Unique identifier
    starlink_config_t config;              // Configuration
    starlink_collection_result_t last_result; // Last collection result
    time_t last_collection;                // Last collection time
    int consecutive_failures;              // Count of consecutive failures
    int consecutive_successes;             // Count of consecutive successes
    float average_latency;                 // Average ping latency
    float average_throughput;              // Average throughput
    float reliability_score;               // Reliability score (0-100)
    bool is_active;                        // Currently active for routing
    bool is_healthy;                       // Overall health status
    char failover_reason[128];             // Reason for failover
} starlink_instance_t;

typedef struct {
    starlink_instance_t starlinks[MAX_STARLINKS];
    int count;                             // Number of configured Starlinks
    int active_index;                      // Index of currently active Starlink
    time_t last_failover;                  // Last failover time
    int failover_count;                    // Total failover count
    bool auto_failover_enabled;            // Enable automatic failover
    int failover_threshold;                // Failover threshold (consecutive failures)
    float min_health_score;                // Minimum health score to remain active
} starlink_cluster_t;

// Prediction engine constants
#define PREDICTION_SUCCESS 0
#define PREDICTION_ERROR_INVALID_PARAM -1
#define PREDICTION_ERROR_NOT_INITIALIZED -2
#define PREDICTION_ERROR_CALCULATION_FAILED -3

// Risk level constants
#define RISK_LEVEL_LOW 1
#define RISK_LEVEL_MEDIUM 2
#define RISK_LEVEL_HIGH 3
#define RISK_LEVEL_CRITICAL 4

// Enhanced tracking function declarations
int starlink_get_obstruction_map(char *response, size_t response_size);
int starlink_get_diagnostics(char *response, size_t response_size);
int starlink_get_enhanced_location(dish_location_t *location);
int starlink_get_tracking_data(dish_location_t *location, char *obstruction_response, size_t obstruction_size);
int starlink_set_tracking_enabled(bool enabled);
bool starlink_is_tracking_enabled(void);
time_t starlink_get_last_obstruction_update(void);
time_t starlink_get_last_location_update(void);
int starlink_test_tracking_connectivity(void);

#endif // STARLINK_TYPES_H
