#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

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
    int gps_enabled;
    float gps_health_score;
    char location_status[16];
    int movement_detected;
    time_t last_movement_check;
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

// GPS source types
typedef enum {
    GPS_SOURCE_UNKNOWN = 0,
    GPS_SOURCE_RUTOS,
    GPS_SOURCE_STARLINK,
    GPS_SOURCE_OPENCELLID,
    GPS_SOURCE_GOOGLE,
    GPS_SOURCE_EXTERNAL,
    GPS_SOURCE_SIMULATED,
    GPS_SOURCE_WIFI,
    GPS_SOURCE_CELLULAR,
    GPS_SOURCE_COMBINED,
    GPS_SOURCE_CUSTOM,
    GPS_SOURCE_MAX
} gps_source_type_t;

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

// Function declarations
void log_message(log_level_t level, const char *format, ...);

#endif // TYPES_H
