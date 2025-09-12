#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <cjson/cJSON.h>
#include "../../core/types.h"

// JSON parser utilities for production use
// Replaces all simplified JSON parsing implementations

// Common JSON parsing functions
typedef struct {
    cJSON* root;
    bool valid;
    char error_msg[256];
} json_document_t;

// Initialize JSON parser
int json_parser_init(void);
void json_parser_cleanup(void);

// Document management
json_document_t* json_parse_string(const char* json_string);
json_document_t* json_parse_file(const char* filename);
void json_document_free(json_document_t* doc);

// Value extraction (with error checking)
bool json_get_string(json_document_t* doc, const char* path, char* value, size_t max_len);
bool json_get_double(json_document_t* doc, const char* path, double* value);
bool json_get_int(json_document_t* doc, const char* path, int* value);
bool json_get_bool(json_document_t* doc, const char* path, bool* value);

// Array handling
int json_get_array_size(json_document_t* doc, const char* path);
bool json_get_array_string(json_document_t* doc, const char* path, int index, char* value, size_t max_len);
bool json_get_array_double(json_document_t* doc, const char* path, int index, double* value);
bool json_get_array_int(json_document_t* doc, const char* path, int index, int* value);

// Object handling
bool json_has_key(json_document_t* doc, const char* path);
char** json_get_object_keys(json_document_t* doc, const char* path, int* key_count);
void json_free_keys(char** keys, int key_count);

// JSON creation utilities
json_document_t* json_create_object(void);
json_document_t* json_create_array(void);

bool json_set_string(json_document_t* doc, const char* path, const char* value);
bool json_set_double(json_document_t* doc, const char* path, double value);
bool json_set_int(json_document_t* doc, const char* path, int value);
bool json_set_bool(json_document_t* doc, const char* path, bool value);

// Common JSON object creation patterns (reduces duplication)
cJSON* json_create_notification_payload(const char* type, const char* title, const char* message, 
                                        int priority, const char* timestamp, const char* source, 
                                        const char* version, const char* hostname);
cJSON* json_create_simple_object(const char* key1, const char* value1, 
                                 const char* key2, const char* value2,
                                 const char* key3, const char* value3);
cJSON* json_create_status_object(const char* status, const char* message, 
                                double timestamp, const char* module);

// WiFi access point JSON creation (consolidates duplicate patterns)
cJSON* json_create_wifi_ap_object(const char* bssid, int signal_strength, int channel, int age);
cJSON* json_create_wifi_ap_array(void* access_points, int ap_count); // void* to avoid circular includes

bool json_add_array_string(json_document_t* doc, const char* path, const char* value);
bool json_add_array_double(json_document_t* doc, const char* path, double value);
bool json_add_array_int(json_document_t* doc, const char* path, int value);

// Serialization
char* json_to_string(json_document_t* doc);
char* json_to_string_formatted(json_document_t* doc);
void json_free_string(char* json_str);

// Validation
bool json_validate_schema(json_document_t* doc, const char* schema);
bool json_validate_required_fields(json_document_t* doc, const char** required_fields, int field_count);

// Starlink-specific parsers
typedef struct {
    bool connected;
    double uptime;
    double downlink_throughput_bps;
    double uplink_throughput_bps;
    double ping_drop_rate;
    double ping_latency_ms;
    double obstruction_duration;
    char hardware_version[64];
    char software_version[64];
    char dish_id[64];
} starlink_status_t;

typedef struct {
    double latitude;
    double longitude;
    double altitude;
    char country_code[8];
} starlink_location_t;

typedef struct {
    double snr;
    double elevation;
    double azimuth;
    bool is_active;
    int sat_id;
} starlink_satellite_t;

bool json_parse_starlink_status(const char* json_str, starlink_status_t* status);
bool json_parse_starlink_location(const char* json_str, starlink_location_t* location);
bool json_parse_starlink_satellites(const char* json_str, starlink_satellite_t* satellites, int max_sats, int* sat_count);

// Weather API parsers
typedef struct {
    double temperature;
    double humidity;
    double pressure;
    double wind_speed;
    double wind_direction;
    double precipitation;
    double cloud_cover;
    char description[64];
    char icon[16];
} weather_data_t;

bool json_parse_openweather_current(const char* json_str, weather_data_t* weather);

// Google Maps API parsers
typedef struct {
    char formatted_address[256];
    char country[64];
    char state[64];
    char city[64];
    char postal_code[16];
    double latitude;
    double longitude;
} geocoding_result_t;

bool json_parse_google_geocoding(const char* json_str, geocoding_result_t* result);

// GPS/Location parsers
// gps_data_t is defined in core/types.h

bool json_parse_gps_data(const char* json_str, gps_data_t* gps);

// Network diagnostics parsers
typedef struct {
    double ping_min;
    double ping_avg;
    double ping_max;
    double packet_loss;
    int packets_sent;
    int packets_received;
} ping_result_t;

bool json_parse_ping_result(const char* json_str, ping_result_t* ping);

// Utility functions for path-based access
cJSON* json_get_object_by_path(cJSON* root, const char* path);
bool json_path_exists(cJSON* root, const char* path);

// Error handling
const char* json_get_last_error(void);
void json_clear_error(void);

#endif // JSON_PARSER_H
