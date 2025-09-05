#ifndef WIFI_ENHANCED_H
#define WIFI_ENHANCED_H

#include "types.h"
#include "gps_comprehensive.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

// WiFi band types
typedef enum {
    WIFI_BAND_24GHZ = 0,
    WIFI_BAND_5GHZ,
    WIFI_BAND_6GHZ,
    WIFI_BAND_MAX
} wifi_band_t;

// WiFi channel width types
typedef enum {
    WIFI_WIDTH_20MHZ = 0,
    WIFI_WIDTH_40MHZ,
    WIFI_WIDTH_80MHZ,
    WIFI_WIDTH_160MHZ,
    WIFI_WIDTH_MAX
} wifi_width_t;

// WiFi security types
typedef enum {
    WIFI_SECURITY_NONE = 0,
    WIFI_SECURITY_WEP,
    WIFI_SECURITY_WPA,
    WIFI_SECURITY_WPA2,
    WIFI_SECURITY_WPA3,
    WIFI_SECURITY_WPA2_WPA3,
    WIFI_SECURITY_MAX
} wifi_security_t;

// WiFi access point information (from UBUS iwinfo scan)
typedef struct {
    char ssid[64];                         // Network SSID
    char bssid[18];                        // MAC address (XX:XX:XX:XX:XX:XX)
    int channel;                           // Channel number
    int signal;                            // Signal strength in dBm (negative)
    char htmode[16];                       // HT mode (HT20, HT40, VHT80, HE80, etc.)
    int bandwidth;                         // Channel bandwidth in MHz
    uint64_t frequency;                    // Frequency in Hz
    int center_chan1;                      // Center channel 1 (for wide channels)
    int center_chan2;                      // Center channel 2 (for 80+80)
    wifi_security_t security;              // Security type
    bool hidden;                           // Whether SSID is hidden
    int quality;                           // Signal quality (0-100)
    time_t last_seen;                      // When AP was last seen
} wifi_access_point_t;

// WiFi channel utilization (from UBUS iwinfo survey)
typedef struct {
    int channel;                           // Channel number
    uint64_t frequency;                    // Frequency in Hz
    int noise;                             // Noise floor in dBm
    uint64_t active_time;                  // Active time in microseconds
    uint64_t busy_time;                    // Busy time in microseconds
    uint64_t rx_time;                      // RX time in microseconds
    uint64_t tx_time;                      // TX time in microseconds
    double utilization_percent;            // Channel utilization percentage
    time_t measurement_time;               // When measurement was taken
} wifi_channel_utilization_t;

// Enhanced channel score (matching Go implementation)
typedef struct {
    int channel;                           // Channel number
    wifi_band_t band;                      // WiFi band
    
    // Raw scoring data
    int co_channel_aps;                    // Number of co-channel APs
    int overlap_aps;                       // Number of overlapping APs
    double utilization_percent;            // Channel utilization
    int noise_floor;                       // Noise floor in dBm
    
    // Weighted penalties
    double co_channel_penalty;             // Co-channel interference penalty
    double overlap_penalty;                // Adjacent channel interference penalty
    double utilization_penalty;            // Channel utilization penalty
    double noise_penalty;                  // Noise floor penalty
    
    // Final scoring
    double raw_score;                      // Raw score (0-100, higher is better)
    int stars;                             // Star rating (1-5)
    char rating[16];                       // Rating string (poor, fair, good, excellent)
    
    // Interferer details
    wifi_access_point_t strong_interferers[10]; // Strong interfering APs
    int strong_interferer_count;           // Number of strong interferers
    wifi_access_point_t weak_interferers[20]; // Weak interfering APs
    int weak_interferer_count;             // Number of weak interferers
    
    // Analysis metadata
    time_t analysis_time;                  // When analysis was performed
    char analysis_method[32];              // Analysis method used
} wifi_enhanced_channel_score_t;

// WiFi interface information
typedef struct {
    char name[32];                         // Interface name (e.g., "wlan0")
    char device[32];                       // Device name (e.g., "radio0")
    wifi_band_t band;                      // WiFi band
    char frequency[16];                    // Frequency string
    bool active;                           // Whether interface is active
    bool ap_mode;                          // Whether in AP mode
    bool sta_mode;                         // Whether in STA mode
    int current_channel;                   // Current channel
    wifi_width_t current_width;            // Current channel width
    int tx_power;                          // TX power in dBm
    char country_code[4];                  // Regulatory country code
    
    // Status information
    bool enabled;                          // Whether interface is enabled
    bool connected;                        // Whether connected (STA mode)
    char connected_bssid[18];              // Connected BSSID (STA mode)
    int signal_strength;                   // Current signal strength
    int noise_floor;                       // Current noise floor
    
    // Statistics
    uint64_t tx_packets;                   // Transmitted packets
    uint64_t rx_packets;                   // Received packets
    uint64_t tx_bytes;                     // Transmitted bytes
    uint64_t rx_bytes;                     // Received bytes
    uint64_t tx_errors;                    // Transmission errors
    uint64_t rx_errors;                    // Reception errors
    
    time_t last_update;                    // Last update time
} wifi_interface_t;

// WiFi optimization configuration
typedef struct {
    bool enabled;                          // Enable WiFi optimization
    double movement_threshold_m;           // Movement threshold for optimization
    int stationary_time_s;                 // Time to be stationary before optimization
    bool nightly_optimization;             // Enable nightly optimization
    int nightly_time_seconds;              // Nightly optimization time (seconds since midnight)
    int min_improvement;                   // Minimum score improvement to apply changes
    int dwell_time_s;                      // Wait time after applying changes
    int noise_default;                     // Default noise floor
    int vht80_threshold;                   // VHT80 selection threshold
    int vht40_threshold;                   // VHT40 selection threshold
    bool use_dfs;                          // Allow DFS channels
    bool dry_run;                          // Test mode without applying changes
    
    // Enhanced scanning configuration
    bool use_enhanced_scanner;             // Use RUTOS-native enhanced scanning
    int strong_rssi_threshold;             // Strong interferer threshold (-60dBm)
    int weak_rssi_threshold;               // Weak interferer threshold (-80dBm)
    int utilization_weight;                // Weight for channel utilization penalty
    int excellent_threshold;               // Score threshold for 5 stars (90)
    int good_threshold;                    // Score threshold for 4 stars (75)
    int fair_threshold;                    // Score threshold for 3 stars (50)
    int poor_threshold;                    // Score threshold for 2 stars (25)
    double overlap_penalty_ratio;          // Overlap penalty as ratio of co-channel (0.5)
    
    // GPS integration
    bool gps_integration_enabled;          // Enable GPS-based optimization
    double gps_movement_threshold_m;       // GPS movement threshold
    int gps_stationary_time_s;             // GPS stationary time requirement
    int optimization_cooldown_s;           // Cooldown between optimizations
} wifi_optimization_config_t;

// WiFi channel plan
typedef struct {
    int channel_24;                        // 2.4GHz channel
    int channel_5;                         // 5GHz channel
    wifi_width_t width_5;                  // 5GHz channel width
    int score_24;                          // 2.4GHz score
    int score_5;                           // 5GHz score
    int total_score;                       // Total score
    time_t applied_at;                     // When plan was applied
    char country[4];                       // Country code
    char reg_domain[16];                   // Regulatory domain
    char trigger[32];                      // What triggered optimization
    bool successful;                       // Whether application was successful
} wifi_channel_plan_t;

// WiFi optimization statistics
typedef struct {
    uint64_t total_optimizations;          // Total optimizations performed
    uint64_t successful_optimizations;     // Successful optimizations
    uint64_t failed_optimizations;         // Failed optimizations
    uint64_t skipped_optimizations;        // Skipped optimizations (insufficient improvement)
    
    uint64_t scans_performed;              // Total scans performed
    uint64_t successful_scans;             // Successful scans
    uint64_t failed_scans;                 // Failed scans
    
    double average_scan_time_ms;           // Average scan time
    double average_optimization_time_ms;   // Average optimization time
    double average_improvement;            // Average score improvement
    
    uint64_t movement_triggered;           // Movement-triggered optimizations
    uint64_t scheduled_triggered;          // Schedule-triggered optimizations
    uint64_t manual_triggered;             // Manual-triggered optimizations
    
    time_t last_optimization;              // Last optimization time
    time_t last_scan;                      // Last scan time
    time_t stats_reset_time;               // When stats were reset
} wifi_optimization_statistics_t;

// WiFi movement integration state
typedef struct {
    bool gps_integration_enabled;          // GPS integration enabled
    bool is_stationary;                    // Currently stationary
    time_t stationary_start;               // When became stationary
    time_t last_movement;                  // Last movement detected
    standardized_gps_data_t last_location; // Last known location
    double total_distance_moved_m;         // Total distance moved
    int movement_events;                   // Number of movement events
    time_t last_optimization;              // Last GPS-triggered optimization
    bool location_trigger_active;          // Whether location trigger is active
} wifi_movement_state_t;

// Main WiFi enhanced management structure
typedef struct {
    wifi_optimization_config_t config;     // Configuration
    wifi_optimization_statistics_t stats;  // Statistics
    wifi_movement_state_t movement_state;  // Movement integration state
    
    // Interface management
    wifi_interface_t interfaces[16];       // WiFi interfaces
    int interface_count;                   // Number of interfaces
    
    // Channel analysis
    wifi_enhanced_channel_score_t channel_scores_24[14]; // 2.4GHz channel scores
    wifi_enhanced_channel_score_t channel_scores_5[64];  // 5GHz channel scores
    int scores_24_count;                   // Number of 2.4GHz scores
    int scores_5_count;                    // Number of 5GHz scores
    
    // Current state
    wifi_channel_plan_t current_plan;      // Current channel plan
    wifi_channel_plan_t last_plan;         // Last applied plan
    time_t last_scan_time;                 // Last scan time
    time_t last_optimization_time;         // Last optimization time
    
    // Threading
    pthread_mutex_t mutex;                 // Main mutex
    pthread_t optimization_thread;         // Optimization thread
    pthread_t scheduler_thread;            // Scheduler thread
    bool threads_running;                  // Thread status
    
    // State
    bool initialized;                      // Initialization status
} wifi_enhanced_management_t;

// Function prototypes

/**
 * Initialize enhanced WiFi management system
 * @param config WiFi optimization configuration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int wifi_enhanced_init(const wifi_optimization_config_t* config);

/**
 * Cleanup enhanced WiFi management system
 */
void wifi_enhanced_cleanup(void);

/**
 * Discover WiFi interfaces using RUTOS iwinfo
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int wifi_enhanced_discover_interfaces(void);

/**
 * Perform enhanced WiFi channel scan using RUTOS ubus iwinfo
 * @param device Device name (e.g., "radio0")
 * @param scores Array to store channel scores
 * @param max_scores Maximum scores to return
 * @return Number of scores returned, or negative error code
 */
int wifi_enhanced_scan_channels(const char* device, wifi_enhanced_channel_score_t* scores, int max_scores);

/**
 * Optimize WiFi channels using enhanced algorithms
 * @param trigger Trigger reason
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int wifi_enhanced_optimize_channels(const char* trigger);

/**
 * Get current channel plan
 * @param plan Channel plan structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int wifi_enhanced_get_current_plan(wifi_channel_plan_t* plan);

/**
 * Apply channel plan
 * @param plan Channel plan to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int wifi_enhanced_apply_channel_plan(const wifi_channel_plan_t* plan);

/**
 * Get WiFi interface information
 * @param interfaces Array to store interface information
 * @param max_interfaces Maximum interfaces to return
 * @return Number of interfaces returned, or negative error code
 */
int wifi_enhanced_get_interfaces(wifi_interface_t* interfaces, int max_interfaces);

/**
 * Get WiFi optimization statistics
 * @param stats Statistics structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int wifi_enhanced_get_statistics(wifi_optimization_statistics_t* stats);

/**
 * Reset WiFi optimization statistics
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int wifi_enhanced_reset_statistics(void);

/**
 * Update GPS location for movement-based optimization
 * @param gps_data Current GPS data
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int wifi_enhanced_update_gps_location(const standardized_gps_data_t* gps_data);

/**
 * Get WiFi movement state
 * @param movement_state Movement state structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int wifi_enhanced_get_movement_state(wifi_movement_state_t* movement_state);

/**
 * Perform manual WiFi optimization
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int wifi_enhanced_optimize_now(void);

/**
 * Enable/disable WiFi optimization
 * @param enabled Whether to enable optimization
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int wifi_enhanced_set_enabled(bool enabled);

/**
 * Get WiFi optimization configuration
 * @param config Configuration structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int wifi_enhanced_get_config(wifi_optimization_config_t* config);

/**
 * Set WiFi optimization configuration
 * @param config New configuration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int wifi_enhanced_set_config(const wifi_optimization_config_t* config);

/**
 * Check if enhanced WiFi management is initialized
 * @return true if initialized, false otherwise
 */
bool wifi_enhanced_is_initialized(void);

// RUTOS integration functions

/**
 * Perform UBUS iwinfo scan
 * @param device Device name
 * @param access_points Array to store access points
 * @param max_aps Maximum access points to return
 * @return Number of access points found, or negative error code
 */
int wifi_enhanced_ubus_scan(const char* device, wifi_access_point_t* access_points, int max_aps);

/**
 * Get channel utilization via UBUS iwinfo survey
 * @param device Device name
 * @param utilization Array to store utilization data
 * @param max_channels Maximum channels to return
 * @return Number of channels returned, or negative error code
 */
int wifi_enhanced_get_channel_utilization(const char* device, wifi_channel_utilization_t* utilization, int max_channels);

/**
 * Get WiFi interface info via UBUS iwinfo
 * @param device Device name
 * @param interface Interface structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int wifi_enhanced_get_interface_info(const char* device, wifi_interface_t* interface);

/**
 * Set WiFi channel via UCI
 * @param device Device name
 * @param channel Channel number
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int wifi_enhanced_set_channel(const char* device, int channel);

/**
 * Set WiFi channel width via UCI
 * @param device Device name
 * @param width Channel width
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int wifi_enhanced_set_channel_width(const char* device, wifi_width_t width);

/**
 * Apply WiFi configuration changes
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int wifi_enhanced_apply_changes(void);

// Utility functions

/**
 * Convert WiFi band to string
 * @param band WiFi band
 * @return Band string
 */
const char* wifi_band_to_string(wifi_band_t band);

/**
 * Convert WiFi width to string
 * @param width WiFi width
 * @return Width string (HT20, HT40, VHT80, etc.)
 */
const char* wifi_width_to_string(wifi_width_t width);

/**
 * Convert channel width string to enum
 * @param width_str Width string
 * @return Width enum
 */
wifi_width_t wifi_parse_width(const char* width_str);

/**
 * Get WiFi band from channel number
 * @param channel Channel number
 * @return WiFi band
 */
wifi_band_t wifi_get_band_from_channel(int channel);

/**
 * Calculate channel overlap
 * @param channel1 First channel
 * @param channel2 Second channel
 * @param band WiFi band
 * @return Overlap factor (0.0-1.0)
 */
double wifi_calculate_channel_overlap(int channel1, int channel2, wifi_band_t band);

/**
 * Get regulatory domain channels
 * @param country_code Country code
 * @param band WiFi band
 * @param channels Array to store allowed channels
 * @param max_channels Maximum channels to return
 * @return Number of channels returned
 */
int wifi_get_regulatory_channels(const char* country_code, wifi_band_t band, int* channels, int max_channels);

/**
 * Calculate signal quality score
 * @param rssi Signal strength in dBm
 * @param noise Noise floor in dBm
 * @return Quality score (0-100)
 */
int wifi_calculate_signal_quality(int rssi, int noise);

#ifdef __cplusplus
}
#endif

#endif // WIFI_ENHANCED_H