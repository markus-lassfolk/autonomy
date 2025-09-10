#ifndef SPACE_TRACK_CONNECTOR_H
#define SPACE_TRACK_CONNECTOR_H

#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration
typedef struct space_track_connector space_track_connector_t;

// Configuration structure
typedef struct {
    char username[64];
    char password[64];
    char api_url[256];
    int timeout_seconds;
    int rate_limit_requests_per_minute;
    int cache_duration_hours;
} space_track_config_t;

// Return codes
#define SPACE_TRACK_SUCCESS 0
#define SPACE_TRACK_ERROR_INVALID_PARAM -1
#define SPACE_TRACK_ERROR_NOT_INITIALIZED -2
#define SPACE_TRACK_ERROR_NETWORK_FAILURE -3
#define SPACE_TRACK_ERROR_API_FAILURE -4
#define SPACE_TRACK_ERROR_PARSE_FAILURE -5

// Function prototypes
void space_track_config_init_defaults(space_track_config_t* config);
space_track_connector_t* space_track_connector_init(const space_track_config_t* config);
void space_track_connector_cleanup(space_track_connector_t* connector);
int space_track_connector_get_tle_data(space_track_connector_t* connector, const char* norad_id, char* tle_data, size_t max_size);
bool space_track_connector_is_connected(space_track_connector_t* connector);
int space_track_get_starlink_tles(space_track_connector_t* connector, void* constellation);

#ifdef __cplusplus
}
#endif

#endif // SPACE_TRACK_CONNECTOR_H
