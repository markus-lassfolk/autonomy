#ifndef SPACE_TRACK_CONNECTOR_H
#define SPACE_TRACK_CONNECTOR_H

#include "starlink_tracker.h"
#include <curl/curl.h>
#include <json-c/json.h>

// Space-Track API configuration
typedef struct {
    char username[64];
    char password[64];
    char base_url[256];
    int rate_limit_requests_per_minute;
    int timeout_seconds;
    bool use_cache;
    int cache_duration_hours;
} space_track_config_t;

// HTTP response structure
typedef struct {
    char *data;
    size_t size;
    long response_code;
    char error_message[256];
} http_response_t;

// Rate limiting structure
typedef struct {
    time_t request_times[20]; // Track last 20 requests
    int request_count;
    int current_index;
    pthread_mutex_t rate_limit_mutex;
} rate_limiter_t;

// Space-Track connector structure
typedef struct space_track_connector {
    space_track_config_t config;
    CURL *curl_handle;
    char session_cookie[512];
    bool authenticated;
    time_t auth_time;
    rate_limiter_t rate_limiter;
    
    // Cache management
    char cache_directory[256];
    time_t cache_last_update;
    bool cache_valid;
    
    // Statistics
    int total_requests;
    int successful_requests;
    int rate_limited_requests;
    int auth_failures;
    int cache_hits;
    int cache_misses;
    time_t last_request_time;
    double total_response_time;
    
    // Logging callback
    void (*log_callback)(int level, const char *message, void *user_data);
    void *log_user_data;
} space_track_connector_t;

// API Functions

// Initialization and cleanup
space_track_connector_t* space_track_connector_init(const space_track_config_t *config);
void space_track_connector_cleanup(space_track_connector_t *connector);

// Authentication
int space_track_authenticate(space_track_connector_t *connector);
bool space_track_is_authenticated(const space_track_connector_t *connector);
int space_track_refresh_auth(space_track_connector_t *connector);

// TLE data retrieval
int space_track_get_starlink_tles(space_track_connector_t *connector, constellation_data_t *constellation);
int space_track_get_tle_by_norad_id(space_track_connector_t *connector, const char *norad_id, tle_data_t *tle);
int space_track_get_latest_tles(space_track_connector_t *connector, const char *object_name_filter, constellation_data_t *constellation);

// Cache management
int space_track_load_cache(space_track_connector_t *connector, constellation_data_t *constellation);
int space_track_save_cache(space_track_connector_t *connector, const constellation_data_t *constellation);
bool space_track_is_cache_valid(const space_track_connector_t *connector);
int space_track_clear_cache(space_track_connector_t *connector);

// Rate limiting
int space_track_wait_for_rate_limit(space_track_connector_t *connector);
bool space_track_can_make_request(const space_track_connector_t *connector);

// Statistics
typedef struct {
    int total_requests;
    int successful_requests;
    int rate_limited_requests;
    int auth_failures;
    int cache_hits;
    int cache_misses;
    time_t last_request_time;
    double average_response_time;
} space_track_stats_t;

const space_track_stats_t* space_track_get_stats(const space_track_connector_t *connector);

// Utility functions
int space_track_parse_tle_response(const char *response, constellation_data_t *constellation);
int space_track_validate_tle_data(const tle_data_t *tle);
time_t space_track_parse_tle_epoch(const char *tle_line1);

// Configuration helpers
void space_track_config_init_defaults(space_track_config_t *config);
int space_track_config_from_env(space_track_config_t *config);

// Error codes
#define SPACE_TRACK_SUCCESS                 0
#define SPACE_TRACK_ERROR_INVALID_PARAM    -1
#define SPACE_TRACK_ERROR_NOT_INITIALIZED  -2
#define SPACE_TRACK_ERROR_AUTH_FAILED      -3
#define SPACE_TRACK_ERROR_NETWORK_FAILED   -4
#define SPACE_TRACK_ERROR_RATE_LIMITED     -5
#define SPACE_TRACK_ERROR_PARSE_FAILED     -6
#define SPACE_TRACK_ERROR_CACHE_FAILED     -7
#define SPACE_TRACK_ERROR_TIMEOUT          -8
// Added to match implementation usage
#define SPACE_TRACK_ERROR_MEMORY_FAILURE    -9

// Internal helper functions (not exposed in public API)
// These are implemented in the corresponding .c file

#endif // SPACE_TRACK_CONNECTOR_H