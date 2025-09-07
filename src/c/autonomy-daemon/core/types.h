#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <fcntl.h>
#include <sys/socket.h>

// Common type definitions for autonomy daemon

// Log levels
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} log_level_t;

// GPS source structure
struct gps_source {
    char name[32];
    char type[16];
    int enabled;
    int active;
    float lat;
    float lon;
    float accuracy;
    int confidence;
    time_t last_update;
    int health_score;
    char status[16];
    char raw_data[256];
};

// Network interface structure
struct network_interface {
    char name[32];
    char type[16];
    int enabled;
    float latency;
    float loss;
    int signal_strength;
    int bandwidth;
    time_t last_check;
    int health_score;
    char status[16];
};

// Global configuration
struct autonomy_config {
    char log_level[16];
    int enable_gps;
    int enable_notifications;
    int health_check_interval;
    char config_file[128];
};

// Autonomy daemon state
struct autonomy_state {
    time_t start_time;
    char version[32];
    char status[32];
    int member_count;
    char current_member[64];
    time_t last_failover;
    float memory_mb;
    int goroutines;
    char device_id[64];
    int health_checks_run;
    int health_issues_found;
    
    // Network management
    struct network_interface interfaces[10];
    int interface_count;
    char active_interface[32];
    int failover_enabled;
    time_t last_network_check;
    float network_health_score;
    
    // GPS & Location management
    struct gps_source gps_sources[8];
    int gps_source_count;
    char active_gps_source[32];
    float current_lat;
    float current_lon;
    float current_accuracy;
    int current_confidence;
    time_t last_gps_update;
    float gps_health_score;
    char location_status[16];
    int movement_detected;
    time_t last_movement_check;
    int gps_enabled;
};

// Return codes
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
#define AUTONOMY_ERROR_CONNECTION_FAILED -16
#define AUTONOMY_ERROR_API_LIMIT_EXCEEDED -17
#define AUTONOMY_ERROR_INVALID_RESPONSE -18
#define AUTONOMY_ERROR_SERVICE_UNAVAILABLE -19
#define AUTONOMY_ERROR_NO_MEMORY -20
#define AUTONOMY_ERROR_OPTIMIZATION_FAILED -21
#define AUTONOMY_ERROR_NO_OPTIMIZATION_NEEDED -22
#define AUTONOMY_ERROR_FEATURE_DISABLED -23
#define AUTONOMY_ERROR_GPS_ACCURACY_INSUFFICIENT -24
#define AUTONOMY_ERROR_NO_INTERFACES -25
#define AUTONOMY_ERROR_NOT_ENABLED -21
#define AUTONOMY_ERROR_INTERNAL -22
#define AUTONOMY_ERROR_API_FAILED -23
#define AUTONOMY_ERROR_CONFIG -24
#define AUTONOMY_ERROR -25

// GPS source types
typedef enum {
    GPS_SOURCE_UNKNOWN = 0,
    GPS_SOURCE_RUTOS,
    GPS_SOURCE_STARLINK,
    GPS_SOURCE_OPENCELLID,
    GPS_SOURCE_GOOGLE,
    GPS_SOURCE_COMBINED,
    GPS_SOURCE_EXTERNAL,
    GPS_SOURCE_SIMULATED,
    GPS_SOURCE_MAX
} gps_source_type_t;

// GPS integration source types (alternative naming)
typedef enum {
    GPS_SOURCE_TYPE_UNKNOWN = 0,
    GPS_SOURCE_TYPE_RUTOS,
    GPS_SOURCE_TYPE_STARLINK,
    GPS_SOURCE_TYPE_EXTERNAL,
    GPS_SOURCE_TYPE_SIMULATED,
    GPS_SOURCE_TYPE_CUSTOM
} gps_integration_source_type_t;

// GPS data structure - comprehensive GPS information
typedef struct {
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
} gps_data_t;

// GPS constants
#define MAX_GPS_SOURCES 10
#define OPENCELLID_API_KEY_LEN 256
#define OPENCELLID_MAX_API_KEY_LEN 256
#define MAX_FUSION_SOURCES 10
#define MAX_CLUSTERS 20
#define MAX_EVENTS 50
#define MAX_PERFORMANCE_HISTORY 100
#define MAX_INTEGRATION_SOURCES 10
#define DEFAULT_CACHE_SIZE 1000
#define MAX_CACHE_ENTRIES 1000
#define MAX_GEOFENCES 20
#define MAX_WEATHER_CACHE 50
#define MAX_TERRAIN_CACHE 50
#define MAX_INTERFACES 32

// Network metrics structure
typedef struct {
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
} network_metrics_t;

// Network interface structure
typedef struct {
    char name[32];                      // Interface name (e.g., "eth0", "wlan0")
    bool is_up;                         // Interface is up
    bool is_default_route;              // Is default route
    char ip_address[16];                // IP address
    char netmask[16];                   // Netmask
    char gateway[16];                   // Gateway
    char mac_address[18];               // MAC address
    int mtu;                            // MTU size
    uint64_t rx_bytes;                  // Received bytes
    uint64_t tx_bytes;                  // Transmitted bytes
    uint64_t rx_packets;                // Received packets
    uint64_t tx_packets;                // Transmitted packets
    uint64_t rx_errors;                 // Receive errors
    uint64_t tx_errors;                 // Transmit errors
    double health_score;                // Interface health score
    time_t last_check;                  // Last health check
    network_metrics_t metrics;          // Current metrics
    bool enabled;                       // Interface enabled for failover
    bool up;                            // Interface is up (alias for is_up)
    time_t last_seen;                   // Last time interface was seen
    int index;                          // Interface index
    bool discovered;                    // Interface was discovered
    char type[32];                      // Interface type (ethernet, wifi, cellular, etc.)
    uint64_t rx_dropped;                // Dropped received packets
    uint64_t tx_dropped;                // Dropped transmitted packets
} network_interface_t;

// GPS geofence system status
typedef struct {
    bool enabled;                       // Geofencing enabled
    int geofence_count;                 // Total geofences
    int active_geofences;               // Active geofences
    int total_events;                   // Total events
    time_t last_check;                  // Last check timestamp
    int active_geofence_count;          // Number of active geofences
    void* geofences[20];                // Active geofences (gps_geofence_definition_t)
} gps_geofence_system_status_t;

// GPS connector status
typedef struct {
    bool enabled;                       // Connector enabled
    int module_count;                   // Total modules
    int active_modules;                 // Active modules
    int total_operations;               // Total operations
    time_t last_check;                  // Last check timestamp
    double system_health;               // Overall system health
    int active_module_count;            // Number of active modules
    void* modules[20];                  // GPS modules (gps_connector_module_t)
} gps_connector_status_t;

// GPS validation result
typedef struct {
    bool is_valid;
    double confidence;
    time_t timestamp;
    uint32_t flags;
    char error_message[256];
    char warning_message[256];
} gps_validation_result_t;

// GPS source status
typedef enum {
    GPS_SOURCE_STATUS_UNKNOWN = 0,
    GPS_SOURCE_STATUS_EXCELLENT,
    GPS_SOURCE_STATUS_GOOD,
    GPS_SOURCE_STATUS_POOR,
    GPS_SOURCE_STATUS_CRITICAL,
    GPS_SOURCE_STATUS_FAILED
} gps_source_status_t;

// Note: gps_source_health_t and gps_movement_state_t are defined in gps/gps_comprehensive.h

// Note: OpenCellID types defined in gps/opencellid_complete.h

// GPS fix types
typedef enum {
    GPS_FIX_TYPE_NONE = 0,
    GPS_FIX_TYPE_2D,
    GPS_FIX_TYPE_3D,
    GPS_FIX_TYPE_DGPS,
    GPS_FIX_TYPE_RTK_FLOAT,
    GPS_FIX_TYPE_RTK_FIXED,
    GPS_FIX_TYPE_MAX
} gps_fix_type_t;

// GPS fix quality
typedef enum {
    GPS_FIX_QUALITY_NONE = 0,
    GPS_FIX_QUALITY_GPS = 1,
    GPS_FIX_QUALITY_DGPS = 2,
    GPS_FIX_QUALITY_PPS = 3,
    GPS_FIX_QUALITY_RTK = 4,
    GPS_FIX_QUALITY_RTK_FLOAT = 5,
    GPS_FIX_QUALITY_ESTIMATED = 6,
    GPS_FIX_QUALITY_MANUAL = 7,
    GPS_FIX_QUALITY_SIMULATED = 8,
    GPS_FIX_QUALITY_MAX
} gps_fix_quality_t;

// Comprehensive GPS type definitions for remaining files

// GPS module types
typedef enum {
    GPS_MODULE_TYPE_UNKNOWN = 0,
    GPS_MODULE_TYPE_INTEGRATION,
    GPS_MODULE_TYPE_MANAGER,
    GPS_MODULE_TYPE_RUTOS,
    GPS_MODULE_TYPE_STARLINK,
    GPS_MODULE_TYPE_CONFIDENCE,
    GPS_MODULE_TYPE_ACCURACY,
    GPS_MODULE_TYPE_NMEA,
    GPS_MODULE_TYPE_MOVEMENT,
    GPS_MODULE_TYPE_CLUSTERING,
    GPS_MODULE_TYPE_HEALTH,
    GPS_MODULE_TYPE_FUSION,
    GPS_MODULE_TYPE_GEOFENCE,
    GPS_MODULE_TYPE_EVENTS,
    GPS_MODULE_TYPE_LOCATION_SERVICES,
    GPS_MODULE_TYPE_COORDINATE_UTILS,
    GPS_MODULE_TYPE_OBSTRUCTION,
    GPS_MODULE_TYPE_ADAPTIVE_CACHE,
    GPS_MODULE_TYPE_GOOGLE_API,
    GPS_MODULE_TYPE_CELL_TOWER,
    GPS_MODULE_TYPE_WEATHER,
    GPS_MODULE_TYPE_TERRAIN,
    GPS_MODULE_TYPE_PERFORMANCE,
    GPS_MODULE_TYPE_ERROR_RECOVERY,
    GPS_MODULE_TYPE_MAX
} gps_module_type_t;

// GPS error types
typedef enum {
    GPS_ERROR_NONE = 0,
    GPS_ERROR_NO_FIX,
    GPS_ERROR_POOR_ACCURACY,
    GPS_ERROR_TIMEOUT,
    GPS_ERROR_COMMUNICATION,
    GPS_ERROR_INVALID_DATA,
    GPS_ERROR_TYPE_UNKNOWN,
    GPS_ERROR_TYPE_AUTHENTICATION_ERROR,
    GPS_ERROR_TYPE_RATE_LIMIT,
    GPS_ERROR_TYPE_SERVER_ERROR,
    GPS_ERROR_MAX
} gps_error_type_t;

// Note: gps_cluster_t is defined in gps/gps_clustering.h

// GPS performance metrics
typedef struct {
    double avg_accuracy;
    double avg_response_time;
    int success_count;
    int failure_count;
    time_t measurement_time;
} gps_performance_metrics_t;

// Note: movement_metrics_t is defined in gps/gps_movement.h

// Note: gps_movement_pattern_t is defined in gps/gps_movement.h

// Note: gps_fusion_source_t is defined in gps/gps_fusion.h

// GPS source error tracking
typedef struct {
    int source_id;
    gps_error_type_t error_type;
    int error_count;
    time_t first_error;
    time_t last_error;
    double error_rate;
    bool needs_recovery;
    int status;                         // Source status (source_status_t)
} gps_source_error_t;

// GPS recovery strategies
typedef enum {
    GPS_RECOVERY_NONE = 0,
    GPS_RECOVERY_RETRY,
    GPS_RECOVERY_FALLBACK,
    GPS_RECOVERY_RESET,
    GPS_RECOVERY_DEGRADE,
    GPS_RECOVERY_SWITCH_SOURCE
} gps_recovery_strategy_t;

// Note: OpenCellID radio types defined in gps/opencellid_complete.h

// Note: opencellid_cellular_environment_t is defined in gps/opencellid_complete.h

// Note: opencellid_triangulation_result_t is defined in gps/opencellid_complete.h

// Forward declarations for Starlink obstruction types (Core module pattern)
// Forward declarations for complex Starlink obstruction types (removed - full definitions in starlink_obstruction.h)

// System health structure
typedef struct {
    char status[32];                    // Overall status
    int starlink_health;                // Starlink health score
    int uci_health;                     // UCI health score
    int overlay_health;                 // Overlay health score
    int services_health;                // Services health score
    int network_health;                 // Network health score
    int database_health;                // Database health score
    int time_health;                    // Time sync health score
    int logs_health;                    // Logs health score
    int gps_health;                     // GPS health score
    int overall_health;                 // Overall health score
    double overall_score;               // Overall score (0-100)
    time_t last_check;                  // Last health check time
} system_health_t;

// Autonomy state structure
typedef struct {
    bool running;                       // Whether daemon is running
    bool gps_enabled;                   // GPS system enabled
    double current_lat;                 // Current latitude
    double current_lon;                 // Current longitude
    double current_accuracy;            // Current GPS accuracy
    double current_confidence;          // Current GPS confidence
    time_t last_gps_update;             // Last GPS update time
    char location_status[32];           // Location status string
    bool movement_detected;             // Movement detection flag
    time_t last_movement_check;         // Last movement check time
    time_t start_time;                  // Daemon start time
    int health_checks_run;              // Number of health checks run
    int health_issues_found;            // Number of health issues found
    int interface_count;                // Number of network interfaces
    bool failover_enabled;              // Network failover enabled
    double network_health_score;        // Network health score
    int gps_source_count;               // Number of GPS sources
    double gps_health_score;            // GPS health score
} autonomy_state_t;

// Autonomy daemon configuration
typedef struct {
    // Daemon settings
    char config_file[256];                   // Configuration file path
    bool daemon_mode;                        // Run as daemon
    bool debug_mode;                         // Debug mode enabled
    int log_level;                           // Log level
    char log_file[256];                      // Log file path
    int pid_file_timeout;                    // PID file timeout
    
    // Network settings
    int network_check_interval;              // Network check interval
    int failover_timeout;                    // Failover timeout
    bool auto_failover;                      // Auto failover enabled
    int min_interface_health;                // Minimum interface health
    bool mwan3_integration;                  // MWAN3 integration
    
    // GPS settings
    int gps_update_interval;                 // GPS update interval
    int gps_timeout;                         // GPS timeout
    bool gps_fusion;                         // GPS fusion enabled
    int gps_cache_timeout;                   // GPS cache timeout
    double min_gps_accuracy;                 // Minimum GPS accuracy
    
    // Starlink settings
    int starlink_check_interval;             // Starlink check interval
    bool starlink_health_monitoring;         // Starlink health monitoring
    char starlink_host[256];                 // Starlink host
    int starlink_port;                       // Starlink port
    int starlink_timeout;                    // Starlink timeout
    
    // System monitoring
    int system_check_interval;               // System check interval
    bool resource_monitoring;                // Resource monitoring enabled
    bool service_monitoring;                 // Service monitoring enabled
    int alert_threshold;                     // Alert threshold
    
    // Notifications
    bool notifications_enabled;              // Notifications enabled
    char email_from[256];                    // Email from address
    char email_to[256];                      // Email to address
    char email_smtp[256];                    // SMTP server
    char webhook_url[512];                   // Webhook URL
} autonomy_config_t;


// Function declarations
void log_message(log_level_t level, const char *format, ...);

#endif // TYPES_H
