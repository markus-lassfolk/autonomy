#ifndef AUTONOMY_TYPES_H
#define AUTONOMY_TYPES_H

#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include "types.h"

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

// GPS source types (defined early for use in gps_data_t)
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

// Note: log_level_t is defined in types.h

// Note: gps_source and network_interface structures are defined in types.h

// Note: autonomy_config and autonomy_state structures are defined in types.h

// System health structure
struct system_health {
    char status[32];
    int starlink_health;
    int uci_health;
    int overlay_health;
    int services_health;
    int network_health;
    int database_health;
    int time_health;
    int logs_health;
    int gps_health;
    int overall_health;
    int overall_score;
    time_t last_check;
};

// Note: log_message function is declared in types.h

#endif // AUTONOMY_TYPES_H
