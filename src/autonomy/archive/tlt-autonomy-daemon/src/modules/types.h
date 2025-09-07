#ifndef AUTONOMY_TYPES_H
#define AUTONOMY_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Forward declarations
typedef struct autonomy_config autonomy_config_t;
typedef struct autonomy_state autonomy_state_t;
typedef struct network_interface network_interface_t;
typedef struct gps_source gps_source_t;
typedef struct starlink_status starlink_status_t;

// Network interface information
typedef struct network_interface {
    char name[32];                    // Interface name (e.g., "eth0", "wlan0")
    char type[16];                    // Interface type ("ethernet", "wifi", "cellular")
    bool up;                          // Interface is up
    bool enabled;                     // Interface is enabled
    uint64_t rx_bytes;                // Received bytes
    uint64_t tx_bytes;                // Transmitted bytes
    uint32_t rx_packets;              // Received packets
    uint32_t tx_packets;              // Transmitted packets
    uint32_t rx_errors;               // Receive errors
    uint32_t tx_errors;               // Transmit errors
    uint32_t rx_dropped;              // Receive dropped packets
    uint32_t tx_dropped;              // Transmit dropped packets
    uint32_t collisions;              // Collision count
    uint32_t carrier_losses;          // Carrier losses
    time_t last_seen;                 // Last time interface was seen
    float latency_ms;                 // Current latency in milliseconds
    float packet_loss;                // Current packet loss percentage
    float signal_strength;            // Signal strength (for wireless)
    char ip_address[16];              // IP address
    char netmask[16];                 // Netmask
    char gateway[16];                 // Gateway address
    char dns_servers[64];             // DNS servers
    bool is_default_route;            // Is this the default route?
    int mtu;                          // Maximum Transmission Unit
    char mac_address[18];             // MAC address
    char driver[32];                  // Driver name
    char firmware_version[32];        // Firmware version
    char hardware_version[32];        // Hardware version
} network_interface_t;

// GPS source information
typedef struct gps_source {
    char name[32];                    // Source name (e.g., "rutos", "starlink", "google")
    char type[16];                    // Source type ("gps", "api", "cell")
    bool enabled;                     // Source is enabled
    bool active;                      // Source is currently active
    double latitude;                  // Current latitude
    double longitude;                 // Current longitude
    double altitude;                  // Current altitude
    float accuracy;                   // Accuracy in meters
    float speed;                      // Speed in m/s
    float heading;                    // Heading in degrees
    time_t timestamp;                 // Last update timestamp
    uint32_t satellites;              // Number of satellites
    float hdop;                       // Horizontal dilution of precision
    float vdop;                       // Vertical dilution of precision
    bool valid;                       // Data is valid
    char status[32];                  // Status string
    uint32_t consecutive_failures;    // Consecutive failure count
    uint32_t consecutive_successes;   // Consecutive success count
    float reliability_score;          // Reliability score (0-100)
    time_t last_success;              // Last successful update
    time_t last_failure;              // Last failure
    char error_message[256];          // Last error message
} gps_source_t;

// Starlink status information
typedef struct starlink_status {
    bool connected;                   // Connected to Starlink
    bool dish_online;                 // Dish is online
    char dish_id[64];                 // Dish ID
    char hardware_version[32];        // Hardware version
    char software_version[32];        // Software version
    double latitude;                  // Dish latitude
    double longitude;                 // Dish longitude
    float altitude;                   // Dish altitude
    float snr;                        // Signal to noise ratio
    float downlink_throughput;        // Downlink throughput in Mbps
    float uplink_throughput;          // Uplink throughput in Mbps
    float latency;                    // Latency in milliseconds
    bool obstructed;                  // Dish is obstructed
    float obstruction_fraction;       // Obstruction fraction
    time_t last_update;               // Last update timestamp
    char status[32];                  // Status string
    bool healthy;                     // Overall health status
    float health_score;               // Health score (0-100)
} starlink_status_t;

// System resource information
typedef struct system_resources {
    uint64_t total_memory;            // Total memory in bytes
    uint64_t available_memory;        // Available memory in bytes
    uint64_t used_memory;             // Used memory in bytes
    float memory_usage_percent;       // Memory usage percentage
    uint64_t total_disk;              // Total disk space in bytes
    uint64_t available_disk;          // Available disk space in bytes
    uint64_t used_disk;               // Used disk space in bytes
    float disk_usage_percent;         // Disk usage percentage
    float cpu_usage_percent;          // CPU usage percentage
    float load_1min;                  // 1-minute load average
    float load_5min;                  // 5-minute load average
    float load_15min;                 // 15-minute load average
    uint32_t uptime_seconds;          // System uptime in seconds
    uint32_t process_count;           // Number of running processes
    uint32_t thread_count;            // Number of running threads
    time_t last_update;               // Last update timestamp
} system_resources_t;

// Service status information
typedef struct service_status {
    char name[64];                    // Service name
    bool running;                     // Service is running
    bool enabled;                     // Service is enabled
    pid_t pid;                        // Process ID
    uint32_t uptime_seconds;          // Service uptime in seconds
    uint32_t restart_count;           // Number of restarts
    time_t last_start;                // Last start time
    time_t last_stop;                 // Last stop time
    char status[32];                  // Status string
    char version[32];                 // Service version
    bool healthy;                     // Service is healthy
    char error_message[256];          // Last error message
} service_status_t;

// Configuration structure
typedef struct autonomy_config {
    // General settings
    char config_file[256];            // Configuration file path
    bool daemon_mode;                 // Run in daemon mode
    bool debug_mode;                  // Enable debug mode
    uint32_t log_level;               // Log level
    char log_file[256];               // Log file path
    uint32_t pid_file_timeout;        // PID file timeout in seconds
    
    // Network settings
    uint32_t network_check_interval;  // Network check interval in seconds
    uint32_t failover_timeout;        // Failover timeout in seconds
    bool auto_failover;               // Enable automatic failover
    uint32_t min_interface_health;    // Minimum interface health score
    bool mwan3_integration;           // Enable MWAN3 integration
    
    // GPS settings
    uint32_t gps_update_interval;     // GPS update interval in seconds
    uint32_t gps_timeout;             // GPS timeout in seconds
    bool gps_fusion;                  // Enable GPS fusion
    uint32_t gps_cache_timeout;       // GPS cache timeout in seconds
    float min_gps_accuracy;           // Minimum GPS accuracy in meters
    
    // Starlink settings
    char starlink_host[64];           // Starlink host address
    uint16_t starlink_port;           // Starlink port
    uint32_t starlink_timeout;        // Starlink timeout in seconds
    uint32_t starlink_check_interval; // Starlink check interval
    bool starlink_health_monitoring;  // Enable Starlink health monitoring
    
    // System monitoring
    uint32_t system_check_interval;   // System check interval in seconds
    bool resource_monitoring;         // Enable resource monitoring
    bool service_monitoring;          // Enable service monitoring
    uint32_t alert_threshold;         // Alert threshold percentage
    
    // Notification settings
    bool notifications_enabled;        // Enable notifications
    char webhook_url[256];            // Webhook URL
    char email_smtp[64];              // SMTP server
    char email_from[128];             // From email address
    char email_to[256];               // To email addresses
} autonomy_config_t;

// Global state structure
typedef struct autonomy_state {
    // Configuration
    autonomy_config_t config;         // Current configuration
    
    // System state
    bool initialized;                 // System is initialized
    bool running;                     // System is running
    pid_t pid;                        // Current process ID
    time_t start_time;                // Start time
    uint32_t uptime_seconds;          // Uptime in seconds
    
    // Network state
    network_interface_t interfaces[16]; // Network interfaces
    uint32_t interface_count;         // Number of interfaces
    int active_interface_index;       // Index of active interface
    bool failover_in_progress;        // Failover in progress
    time_t last_failover;             // Last failover time
    
    // GPS state
    gps_source_t gps_sources[8];      // GPS sources
    uint32_t gps_source_count;        // Number of GPS sources
    int active_gps_source_index;      // Index of active GPS source
    double current_lat;               // Current latitude
    double current_lon;               // Current longitude
    float current_accuracy;           // Current accuracy
    time_t last_gps_update;           // Last GPS update
    
    // Starlink state
    starlink_status_t starlink;       // Starlink status
    bool starlink_available;          // Starlink is available
    time_t last_starlink_check;       // Last Starlink check
    
    // System resources
    system_resources_t resources;     // System resources
    service_status_t services[32];    // Service statuses
    uint32_t service_count;           // Number of services
    
    // Statistics
    uint64_t total_network_switches;  // Total network switches
    uint64_t total_gps_updates;       // Total GPS updates
    uint64_t total_starlink_checks;   // Total Starlink checks
    uint64_t total_errors;            // Total errors
    time_t last_statistics_update;    // Last statistics update
} autonomy_state_t;

// Error codes
typedef enum {
    AUTONOMY_SUCCESS = 0,
    AUTONOMY_ERROR_INVALID_PARAM = -1,
    AUTONOMY_ERROR_NOT_INITIALIZED = -2,
    AUTONOMY_ERROR_NOT_FOUND = -3,
    AUTONOMY_ERROR_ALREADY_EXISTS = -4,
    AUTONOMY_ERROR_PERMISSION_DENIED = -5,
    AUTONOMY_ERROR_TIMEOUT = -6,
    AUTONOMY_ERROR_NETWORK = -7,
    AUTONOMY_ERROR_CONFIG = -8,
    AUTONOMY_ERROR_SYSTEM = -9
} autonomy_error_t;

// Status codes
typedef enum {
    AUTONOMY_STATUS_UNKNOWN = 0,
    AUTONOMY_STATUS_OFFLINE,
    AUTONOMY_STATUS_ONLINE,
    AUTONOMY_STATUS_DEGRADED,
    AUTONOMY_STATUS_MAINTENANCE,
    AUTONOMY_STATUS_ERROR
} autonomy_status_t;

// Health levels
typedef enum {
    AUTONOMY_HEALTH_UNKNOWN = 0,
    AUTONOMY_HEALTH_EXCELLENT,
    AUTONOMY_HEALTH_GOOD,
    AUTONOMY_HEALTH_FAIR,
    AUTONOMY_HEALTH_POOR,
    AUTONOMY_HEALTH_CRITICAL
} autonomy_health_t;

#endif // AUTONOMY_TYPES_H
