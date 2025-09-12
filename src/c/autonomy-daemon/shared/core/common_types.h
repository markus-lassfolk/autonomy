#ifndef SHARED_COMMON_TYPES_H
#define SHARED_COMMON_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <fcntl.h>
#include <sys/socket.h>

// Shared type definitions for autonomy daemon
// This file consolidates common types used across multiple modules

// Forward declarations
typedef struct gps_data_unified gps_data_unified_t;
typedef struct network_interface_unified network_interface_unified_t;
typedef struct network_metrics_unified network_metrics_unified_t;

// Common error codes (consolidated from core/types.h)
#define AUTONOMY_SUCCESS 0
#define AUTONOMY_ERROR_NOT_INITIALIZED -1
#define AUTONOMY_ERROR_INVALID_PARAM -2
#define AUTONOMY_ERROR_TOO_FREQUENT -3
#define AUTONOMY_ERROR_SCAN_FAILED -4
#define AUTONOMY_ERROR_NOT_FOUND -5
#define AUTONOMY_ERROR_ALREADY_EXISTS -6
#define AUTONOMY_ERROR_TIMEOUT -7
#define AUTONOMY_ERROR_NETWORK -8
#define AUTONOMY_ERROR_INVALID_DATA -9
#define AUTONOMY_ERROR_NO_RESOURCES -10
#define AUTONOMY_ERROR_INVALID_FORMAT -11
#define AUTONOMY_ERROR_NOT_SUPPORTED -12
#define AUTONOMY_ERROR_NO_DATA -13
#define AUTONOMY_ERROR_SYSTEM -14
#define AUTONOMY_ERROR_PARSE_FAILED -15
#define AUTONOMY_ERROR_PARSE -15           // Alias for consistency
#define AUTONOMY_ERROR_EXTERNAL_API -16   // External API error
#define AUTONOMY_ERROR_CALCULATION -17    // Calculation error
#define AUTONOMY_ERROR_NOT_CONFIGURED -18 // Service not configured
#define AUTONOMY_ERROR_CONNECTION_FAILED -26
#define AUTONOMY_ERROR_API_LIMIT_EXCEEDED -27
#define AUTONOMY_ERROR_INVALID_RESPONSE -28
#define AUTONOMY_ERROR_SERVICE_UNAVAILABLE -19
#define AUTONOMY_ERROR_NO_MEMORY -20
#define AUTONOMY_ERROR_OPTIMIZATION_FAILED -21
#define AUTONOMY_ERROR_NO_OPTIMIZATION_NEEDED -22
#define AUTONOMY_ERROR_FEATURE_DISABLED -23
#define AUTONOMY_ERROR_GPS_ACCURACY_INSUFFICIENT -24
#define AUTONOMY_ERROR_NO_INTERFACES -25
#define AUTONOMY_ERROR_NOT_ENABLED -26
#define AUTONOMY_ERROR_INTERNAL -27
#define AUTONOMY_ERROR_API_FAILED -28
#define AUTONOMY_ERROR_CONFIG -29
#define AUTONOMY_ERROR -30

// GPS source types (unified)
typedef enum {
    GPS_SOURCE_UNKNOWN = 0,
    GPS_SOURCE_RUTOS,
    GPS_SOURCE_STARLINK,
    GPS_SOURCE_OPENCELLID,
    GPS_SOURCE_GOOGLE,
    GPS_SOURCE_COMBINED,
    GPS_SOURCE_EXTERNAL,
    GPS_SOURCE_UNWIREDLABS,
    GPS_SOURCE_CELL_TOWER,
    GPS_SOURCE_MAX
} gps_source_type_t;

// Unified GPS data structure (combining all variants)
typedef struct gps_data_unified {
    // Position data
    double lat;                    // Latitude in decimal degrees
    double lon;                    // Longitude in decimal degrees  
    double latitude;               // Alternative field name
    double longitude;              // Alternative field name
    double altitude;               // Altitude in meters
    
    // Quality and accuracy
    float accuracy;                // Position accuracy in meters
    float hdop;                    // Horizontal dilution of precision
    float vdop;                    // Vertical dilution of precision
    int satellites;                // Number of satellites in view
    int fix_quality;               // GPS fix quality
    bool valid;                    // Whether GPS data is valid
    
    // Motion data
    float speed;                   // Speed in m/s
    double horizontal_speed_mps;   // Alternative field name
    float heading;                 // Heading/bearing in degrees
    float course;                  // Course over ground
    
    // Timing
    time_t timestamp;              // When GPS data was obtained
    
    // Metadata
    char source[32];               // Source of GPS data
    int source_id;                 // Numeric source ID
    gps_source_type_t source_type; // Source type enum
    int confidence;                // Confidence level (0-100)
    bool is_moving;                // Whether device is in motion
    float reliability_score;       // Reliability score (0.0-1.0)
} gps_data_unified_t;

// Unified network metrics structure
typedef struct network_metrics_unified {
    char interface_name[32];            // Interface name
    double ping_latency_ms;             // Ping latency in milliseconds
    double ping_packet_loss;            // Ping packet loss percentage
    double ping_jitter_ms;              // Ping jitter in milliseconds
    uint64_t bytes_received;            // Bytes received
    uint64_t bytes_transmitted;         // Bytes transmitted
    uint64_t packets_received;          // Packets received
    uint64_t packets_transmitted;       // Packets transmitted
    uint64_t errors_received;           // Receive errors
    uint64_t errors_transmitted;        // Transmit errors
    double throughput_mbps;             // Throughput in Mbps
    double utilization_percent;         // Interface utilization
    double overall_health_score;        // Overall health score
    time_t timestamp;                   // Metrics timestamp
    float ping_success_rate;            // Ping success rate
    float ping_average_latency;         // Average ping latency
    float ping_min_latency;             // Minimum ping latency
    float ping_max_latency;             // Maximum ping latency
    float tcp_success_rate;             // TCP connection success rate
    float tcp_average_connect_time;     // Average TCP connect time
    bool dns_success;                   // DNS resolution success
    double dns_resolve_time;            // DNS resolution time
} network_metrics_unified_t;

// Unified network interface structure (combining all variants)
typedef struct network_interface_unified {
    // Basic interface info
    char name[64];                      // Interface name (e.g., "eth0", "wlan0") - extended from 32
    char friendly_name[64];             // Friendly name as seen in RUTOS UI
    char type[32];                      // Interface type (ethernet, wifi, cellular, starlink, vpn)
    char subtype[32];                   // More specific type (sim, wireguard, etc.)
    
    // Network configuration
    bool is_up;                         // Interface is up
    bool is_default_route;              // Is default route
    char ip_address[64];                // IP address - extended from 16
    char netmask[16];                   // Netmask
    char gateway[16];                   // Gateway
    char mac_address[64];               // MAC address - extended from 18
    int mtu;                            // MTU size
    int metric;                         // Route metric
    char dns_servers[128];              // DNS servers (comma-separated)
    char protocol[16];                  // Protocol (static, dhcp, wwan, etc.)
    char device[32];                    // Physical device name
    
    // MWAN3 integration
    char mwan3_name[32];                // MWAN3 interface name
    bool mwan3_tracking_enabled;        // Is this interface tracked by MWAN3?
    bool mwan3_available;               // Is this interface available in MWAN3?
    char mwan3_status[16];              // MWAN3 status (online, offline, standby, etc.)
    int mwan3_metric;                   // MWAN3 metric value
    
    // VPN detection
    bool is_vpn;                        // Is this a VPN interface?
    char vpn_type[32];                  // VPN type (wireguard, openvpn, etc.)
    char vpn_name[64];                  // VPN connection name
    
    // Starlink specific
    bool is_starlink;                   // Is this a Starlink connection?
    char starlink_dish_id[32];          // Starlink dish identifier
    char starlink_dish_name[64];        // Starlink dish friendly name
    char starlink_ip[16];               // Starlink IP address
    
    // Cellular specific
    char modem_model[64];               // Cellular modem model
    char sim_id[16];                    // SIM identifier
    char operator[64];                  // Cellular operator
    int signal_strength;                // Signal strength
    char modem_id[16];                  // Modem ID (e.g., "2-1")
    char cellular_device_path[64];      // Cellular device path (e.g., "/dev/ttyUSB2")
    
    // Enhanced cellular information
    struct {
        char operator_name[32];         // Network operator name
        char network_technology[16];    // 2G/3G/4G/5G
        int signal_strength_dbm;        // Real-time signal strength in dBm
        char cell_id[16];               // Current cell tower ID
        int signal_quality;             // Signal quality (0-100)
        int rsrp_dbm;                   // Reference Signal Received Power
        int rsrq_db;                    // Reference Signal Received Quality
        int sinr_db;                    // Signal-to-Interference-plus-Noise Ratio
    } enhanced_cellular_info;
    
    // WiFi specific
    char ssid[64];                      // WiFi SSID
    char wifi_band[16];                 // WiFi band (2.4G, 5G)
    char wifi_mode[16];                 // WiFi mode (ap, sta)
    char wifi_encryption[16];           // WiFi encryption type
    
    // Statistics and health
    uint64_t rx_bytes;                  // Received bytes
    uint64_t tx_bytes;                  // Transmitted bytes
    uint64_t rx_packets;                // Received packets
    uint64_t tx_packets;                // Transmitted packets
    uint64_t rx_errors;                 // Receive errors
    uint64_t tx_errors;                 // Transmit errors
    uint64_t rx_dropped;                // Dropped received packets
    uint64_t tx_dropped;                // Dropped transmitted packets
    double health_score;                // Interface health score
    double latency;                     // Latency in ms
    double packet_loss;                 // Packet loss percentage
    double bandwidth;                   // Bandwidth (from contextual_alerts.h)
    
    // Enhanced real-time metrics
    struct {
        uint32_t ping_latency_ms;       // Real-time ping latency
        uint8_t ping_success_rate;      // Ping success rate over last minute (0-100)
        time_t last_ping_test;          // Last ping test timestamp
        uint32_t consecutive_ping_failures; // Consecutive ping failures
        uint32_t total_ping_tests;      // Total ping tests performed
        uint32_t successful_pings;      // Successful ping tests
        uint16_t ping_jitter_ms;        // Ping jitter in milliseconds
        uint16_t ping_min_ms;           // Minimum ping latency
        uint16_t ping_max_ms;           // Maximum ping latency
        bool mwan3_ping_active;         // Is MWAN3 actively pinging this interface?
        uint16_t mwan3_ping_interval;   // MWAN3 ping interval in seconds
        time_t last_mwan3_ping;         // Last MWAN3 ping timestamp
        uint8_t mwan3_ping_success_rate; // MWAN3 ping success rate
    } real_time_metrics;
    
    // Interface performance history for trend analysis
    struct {
        uint16_t latency_history[60];   // Last 60 latency measurements (1 per minute)
        uint8_t loss_history[60];       // Last 60 loss measurements (percentage)
        uint8_t health_history[60];     // Last 60 health scores
        uint8_t history_index;          // Current history index (circular buffer)
        time_t history_start_time;      // When history collection started
        uint32_t history_count;         // Number of valid history entries
        double latency_trend;           // Latency trend (-1 to 1, negative = improving)
        double loss_trend;              // Loss trend (-1 to 1, negative = improving)
        double health_trend;            // Health trend (-1 to 1, negative = degrading)
    } performance_history;
    
    // Timestamps and status
    time_t last_check;                  // Last health check
    time_t last_seen;                   // Last time interface was seen
    time_t last_health_check;           // Last health check timestamp
    network_metrics_unified_t metrics; // Current metrics
    bool enabled;                       // Interface enabled for failover
    bool up;                            // Interface is up (alias for is_up)
    int index;                          // Interface index
    bool discovered;                    // Interface was discovered
} network_interface_unified_t;

// Compatibility aliases to maintain backward compatibility
typedef gps_data_unified_t gps_data_t;
typedef network_interface_unified_t network_interface_t;
typedef network_metrics_unified_t network_metrics_t;

#endif // SHARED_COMMON_TYPES_H