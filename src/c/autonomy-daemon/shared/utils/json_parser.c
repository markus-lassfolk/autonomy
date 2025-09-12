#include "json_parser.h"
#include "../logging/logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

// Global error state
static char g_json_error[256] = {0};

// Set error message
static void set_json_error(const char* format, ...) {
    va_list args;
    va_start(args, format\n"\n"\n"\n"\n"\n"\n"\n");
    vsnprintf(g_json_error, sizeof(g_json_error), format, args\n"\n"\n"\n"\n"\n"\n"\n");
    va_end(args\n"\n"\n"\n"\n"\n"\n"\n");
}
static bool g_json_parser_initialized = false;

// Clear error state
void json_clear_error(void) {
    g_json_error[0] = '\0';
}

// Get last error
const char* json_get_last_error(void) {
    return g_json_error;
}

// Set error message
static void json_set_error(const char* format, ...) {
    va_list args;
    va_start(args, format\n"\n"\n"\n"\n"\n"\n"\n");
    vsnprintf(g_json_error, sizeof(g_json_error), format, args\n"\n"\n"\n"\n"\n"\n"\n");
    va_end(args\n"\n"\n"\n"\n"\n"\n"\n");
}

// Initialize JSON parser
int json_parser_init(void) {
    if (g_json_parser_initialized) {
        return 0;
    }

    json_clear_error(\n"\n"\n"\n"\n"\n"\n"\n");
    g_json_parser_initialized = true;
    
    printf("INFO: "JSON parser initialized"\n"\n"\n"\n"\n"\n"\n"\n");
    return 0;
}

// Cleanup JSON parser
void json_parser_cleanup(void) {
    if (!g_json_parser_initialized) {
        return;
    }

    json_clear_error(\n"\n"\n"\n"\n"\n"\n"\n");
    g_json_parser_initialized = false;
    
    printf("INFO: "JSON parser cleaned up"\n"\n"\n"\n"\n"\n"\n"\n");
}

// Parse JSON string
json_document_t* json_parse_string(const char* json_string) {
    if (!json_string) {
        json_set_error("JSON string is NULL"\n"\n"\n"\n"\n"\n"\n"\n");
        return NULL;
    }

    json_document_t* doc = malloc(sizeof(json_document_t)\n"\n"\n"\n"\n"\n"\n"\n");
    if (!doc) {
        json_set_error("Failed to allocate memory for JSON document"\n"\n"\n"\n"\n"\n"\n"\n");
        return NULL;
    }

    memset(doc, 0, sizeof(json_document_t)\n"\n"\n"\n"\n"\n"\n"\n");
    
    doc->root = cJSON_Parse(json_string\n"\n"\n"\n"\n"\n"\n"\n");
    if (!doc->root) {
        const char* error_ptr = cJSON_GetErrorPtr(\n"\n"\n"\n"\n"\n"\n"\n");
        if (error_ptr) {
            snprintf(doc->error_msg, sizeof(doc->error_msg), "JSON parse error: %.200s", error_ptr\n"\n"\n"\n"\n"\n"\n"\n");
            json_set_error("JSON parse error: %.200s", error_ptr\n"\n"\n"\n"\n"\n"\n"\n");
        } else {
            strncpy(doc->error_msg, "Unknown JSON parse error", sizeof(doc->error_msg) - 1\n"\n"\n"\n"\n"\n"\n"\n");
            json_set_error("Unknown JSON parse error"\n"\n"\n"\n"\n"\n"\n"\n");
        }
        doc->valid = false;
        return doc;
    }

    doc->valid = true;
    return doc;
}

// Parse JSON file
json_document_t* json_parse_file(const char* filename) {
    if (!filename) {
        json_set_error("Filename is NULL"\n"\n"\n"\n"\n"\n"\n"\n");
        return NULL;
    }

    FILE* file = fopen(filename, "r"\n"\n"\n"\n"\n"\n"\n"\n");
    if (!file) {
        json_set_error("Failed to open file: %s", strerror(errno)\n"\n"\n"\n"\n"\n"\n"\n");
        return NULL;
    }

    // Get file size
    fseek(file, 0, SEEK_END\n"\n"\n"\n"\n"\n"\n"\n");
    long file_size = ftell(file\n"\n"\n"\n"\n"\n"\n"\n");
    fseek(file, 0, SEEK_SET\n"\n"\n"\n"\n"\n"\n"\n");

    if (file_size <= 0) {
        fclose(file\n"\n"\n"\n"\n"\n"\n"\n");
        json_set_error("File is empty or invalid size"\n"\n"\n"\n"\n"\n"\n"\n");
        return NULL;
    }

    char* json_string = malloc(file_size + 1\n"\n"\n"\n"\n"\n"\n"\n");
    if (!json_string) {
        fclose(file\n"\n"\n"\n"\n"\n"\n"\n");
        json_set_error("Failed to allocate memory for file content"\n"\n"\n"\n"\n"\n"\n"\n");
        return NULL;
    }

    size_t bytes_read = fread(json_string, 1, file_size, file\n"\n"\n"\n"\n"\n"\n"\n");
    fclose(file\n"\n"\n"\n"\n"\n"\n"\n");

    if (bytes_read != (size_t)file_size) {
        free(json_string\n"\n"\n"\n"\n"\n"\n"\n");
        json_set_error("Failed to read complete file"\n"\n"\n"\n"\n"\n"\n"\n");
        return NULL;
    }

    json_string[file_size] = '\0';

    json_document_t* doc = json_parse_string(json_string\n"\n"\n"\n"\n"\n"\n"\n");
    free(json_string\n"\n"\n"\n"\n"\n"\n"\n");

    return doc;
}

// Free JSON document
void json_document_free(json_document_t* doc) {
    if (!doc) return;

    if (doc->root) {
        cJSON_Delete(doc->root\n"\n"\n"\n"\n"\n"\n"\n");
    }

    free(doc\n"\n"\n"\n"\n"\n"\n"\n");
}

// Get object by path (supports dot notation)
cJSON* json_get_object_by_path(cJSON* root, const char* path) {
    if (!root || !path) return NULL;

    char* path_copy = strdup(path\n"\n"\n"\n"\n"\n"\n"\n");
    if (!path_copy) return NULL;

    cJSON* current = root;
    char* token = strtok(path_copy, "."\n"\n"\n"\n"\n"\n"\n"\n");

    while (token && current) {
        if (cJSON_IsArray(current)) {
            // Handle array index
            int index = atoi(token\n"\n"\n"\n"\n"\n"\n"\n");
            current = cJSON_GetArrayItem(current, index\n"\n"\n"\n"\n"\n"\n"\n");
        } else if (cJSON_IsObject(current)) {
            current = cJSON_GetObjectItem(current, token\n"\n"\n"\n"\n"\n"\n"\n");
        } else {
            current = NULL;
        }
        token = strtok(NULL, "."\n"\n"\n"\n"\n"\n"\n"\n");
    }

    free(path_copy\n"\n"\n"\n"\n"\n"\n"\n");
    return current;
}

// Check if path exists
bool json_path_exists(cJSON* root, const char* path) {
    return json_get_object_by_path(root, path) != NULL;
}

// Get string value
bool json_get_string(json_document_t* doc, const char* path, char* value, size_t max_len) {
    if (!doc || !doc->valid || !path || !value || max_len == 0) {
        return false;
    }

    cJSON* item = json_get_object_by_path(doc->root, path\n"\n"\n"\n"\n"\n"\n"\n");
    if (!item || !cJSON_IsString(item)) {
        return false;
    }

    const char* str_value = cJSON_GetStringValue(item\n"\n"\n"\n"\n"\n"\n"\n");
    if (!str_value) {
        return false;
    }

    strncpy(value, str_value, max_len - 1\n"\n"\n"\n"\n"\n"\n"\n");
    value[max_len - 1] = '\0';
    return true;
}

// Get double value
bool json_get_double(json_document_t* doc, const char* path, double* value) {
    if (!doc || !doc->valid || !path || !value) {
        return false;
    }

    cJSON* item = json_get_object_by_path(doc->root, path\n"\n"\n"\n"\n"\n"\n"\n");
    if (!item || !cJSON_IsNumber(item)) {
        return false;
    }

    *value = cJSON_GetNumberValue(item\n"\n"\n"\n"\n"\n"\n"\n");
    return true;
}

// Get integer value
bool json_get_int(json_document_t* doc, const char* path, int* value) {
    if (!doc || !doc->valid || !path || !value) {
        return false;
    }

    cJSON* item = json_get_object_by_path(doc->root, path\n"\n"\n"\n"\n"\n"\n"\n");
    if (!item || !cJSON_IsNumber(item)) {
        return false;
    }

    *value = (int)cJSON_GetNumberValue(item\n"\n"\n"\n"\n"\n"\n"\n");
    return true;
}

// Get boolean value
bool json_get_bool(json_document_t* doc, const char* path, bool* value) {
    if (!doc || !doc->valid || !path || !value) {
        return false;
    }

    cJSON* item = json_get_object_by_path(doc->root, path\n"\n"\n"\n"\n"\n"\n"\n");
    if (!item || !cJSON_IsBool(item)) {
        return false;
    }

    *value = cJSON_IsTrue(item\n"\n"\n"\n"\n"\n"\n"\n");
    return true;
}

// Get array size
int json_get_array_size(json_document_t* doc, const char* path) {
    if (!doc || !doc->valid || !path) {
        return -1;
    }

    cJSON* item = json_get_object_by_path(doc->root, path\n"\n"\n"\n"\n"\n"\n"\n");
    if (!item || !cJSON_IsArray(item)) {
        return -1;
    }

    return cJSON_GetArraySize(item\n"\n"\n"\n"\n"\n"\n"\n");
}

// Get array string element
bool json_get_array_string(json_document_t* doc, const char* path, int index, char* value, size_t max_len) {
    if (!doc || !doc->valid || !path || !value || max_len == 0 || index < 0) {
        return false;
    }

    cJSON* array = json_get_object_by_path(doc->root, path\n"\n"\n"\n"\n"\n"\n"\n");
    if (!array || !cJSON_IsArray(array)) {
        return false;
    }

    cJSON* item = cJSON_GetArrayItem(array, index\n"\n"\n"\n"\n"\n"\n"\n");
    if (!item || !cJSON_IsString(item)) {
        return false;
    }

    const char* str_value = cJSON_GetStringValue(item\n"\n"\n"\n"\n"\n"\n"\n");
    if (!str_value) {
        return false;
    }

    strncpy(value, str_value, max_len - 1\n"\n"\n"\n"\n"\n"\n"\n");
    value[max_len - 1] = '\0';
    return true;
}

// Create JSON object
json_document_t* json_create_object(void) {
    json_document_t* doc = malloc(sizeof(json_document_t)\n"\n"\n"\n"\n"\n"\n"\n");
    if (!doc) return NULL;

    memset(doc, 0, sizeof(json_document_t)\n"\n"\n"\n"\n"\n"\n"\n");
    doc->root = cJSON_CreateObject(\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!doc->root) {
        free(doc\n"\n"\n"\n"\n"\n"\n"\n");
        return NULL;
    }

    doc->valid = true;
    return doc;
}

// Convert to string
char* json_to_string(json_document_t* doc) {
    if (!doc || !doc->valid || !doc->root) {
        return NULL;
    }

    return cJSON_Print(doc->root\n"\n"\n"\n"\n"\n"\n"\n");
}

// Convert to formatted string
char* json_to_string_formatted(json_document_t* doc) {
    if (!doc || !doc->valid || !doc->root) {
        return NULL;
    }

    return cJSON_Print(doc->root\n"\n"\n"\n"\n"\n"\n"\n");
}

// Free JSON string
void json_free_string(char* json_str) {
    if (json_str) {
        free(json_str\n"\n"\n"\n"\n"\n"\n"\n");
    }
}

// Starlink status parser
bool json_parse_starlink_status(const char* json_str, starlink_status_t* status) {
    if (!json_str || !status) return false;

    json_document_t* doc = json_parse_string(json_str\n"\n"\n"\n"\n"\n"\n"\n");
    if (!doc || !doc->valid) {
        if (doc) json_document_free(doc\n"\n"\n"\n"\n"\n"\n"\n");
        return false;
    }

    memset(status, 0, sizeof(starlink_status_t)\n"\n"\n"\n"\n"\n"\n"\n");

    // Parse standard Starlink status fields
    json_get_bool(doc, "dishGetStatus.deviceState.uptimeS", &status->connected\n"\n"\n"\n"\n"\n"\n"\n");
    json_get_double(doc, "dishGetStatus.deviceInfo.uptimeS", &status->uptime\n"\n"\n"\n"\n"\n"\n"\n");
    json_get_double(doc, "dishGetStatus.deviceState.downlinkThroughputBps", &status->downlink_throughput_bps\n"\n"\n"\n"\n"\n"\n"\n");
    json_get_double(doc, "dishGetStatus.deviceState.uplinkThroughputBps", &status->uplink_throughput_bps\n"\n"\n"\n"\n"\n"\n"\n");
    json_get_double(doc, "dishGetStatus.deviceState.popPingDropRate", &status->ping_drop_rate\n"\n"\n"\n"\n"\n"\n"\n");
    json_get_double(doc, "dishGetStatus.deviceState.popPingLatencyMs", &status->ping_latency_ms\n"\n"\n"\n"\n"\n"\n"\n");
    json_get_double(doc, "dishGetStatus.obstructionStats.fractionObstructed", &status->obstruction_duration\n"\n"\n"\n"\n"\n"\n"\n");
    
    json_get_string(doc, "dishGetStatus.deviceInfo.hardwareVersion", status->hardware_version, sizeof(status->hardware_version)\n"\n"\n"\n"\n"\n"\n"\n");
    json_get_string(doc, "dishGetStatus.deviceInfo.softwareVersion", status->software_version, sizeof(status->software_version)\n"\n"\n"\n"\n"\n"\n"\n");
    json_get_string(doc, "dishGetStatus.deviceInfo.id", status->dish_id, sizeof(status->dish_id)\n"\n"\n"\n"\n"\n"\n"\n");

    json_document_free(doc\n"\n"\n"\n"\n"\n"\n"\n");
    return true;
}

// Starlink location parser
bool json_parse_starlink_location(const char* json_str, starlink_location_t* location) {
    if (!json_str || !location) return false;

    json_document_t* doc = json_parse_string(json_str\n"\n"\n"\n"\n"\n"\n"\n");
    if (!doc || !doc->valid) {
        if (doc) json_document_free(doc\n"\n"\n"\n"\n"\n"\n"\n");
        return false;
    }

    memset(location, 0, sizeof(starlink_location_t)\n"\n"\n"\n"\n"\n"\n"\n");

    json_get_double(doc, "dishGetStatus.deviceState.location.latitude", &location->latitude\n"\n"\n"\n"\n"\n"\n"\n");
    json_get_double(doc, "dishGetStatus.deviceState.location.longitude", &location->longitude\n"\n"\n"\n"\n"\n"\n"\n");
    json_get_double(doc, "dishGetStatus.deviceState.location.altitude", &location->altitude\n"\n"\n"\n"\n"\n"\n"\n");
    json_get_string(doc, "dishGetStatus.deviceState.location.countryCode", location->country_code, sizeof(location->country_code)\n"\n"\n"\n"\n"\n"\n"\n");

    json_document_free(doc\n"\n"\n"\n"\n"\n"\n"\n");
    return true;
}

// OpenWeatherMap parser
bool json_parse_openweather_current(const char* json_str, weather_data_t* weather) {
    if (!json_str || !weather) return false;

    json_document_t* doc = json_parse_string(json_str\n"\n"\n"\n"\n"\n"\n"\n");
    if (!doc || !doc->valid) {
        if (doc) json_document_free(doc\n"\n"\n"\n"\n"\n"\n"\n");
        return false;
    }

    memset(weather, 0, sizeof(weather_data_t)\n"\n"\n"\n"\n"\n"\n"\n");

    json_get_double(doc, "main.temp", &weather->temperature\n"\n"\n"\n"\n"\n"\n"\n");
    json_get_double(doc, "main.humidity", &weather->humidity\n"\n"\n"\n"\n"\n"\n"\n");
    json_get_double(doc, "main.pressure", &weather->pressure\n"\n"\n"\n"\n"\n"\n"\n");
    json_get_double(doc, "wind.speed", &weather->wind_speed\n"\n"\n"\n"\n"\n"\n"\n");
    json_get_double(doc, "wind.deg", &weather->wind_direction\n"\n"\n"\n"\n"\n"\n"\n");
    json_get_double(doc, "rain.1h", &weather->precipitation);  // 1-hour precipitation
    json_get_double(doc, "clouds.all", &weather->cloud_cover);  // Cloud cover percentage
    
    json_get_string(doc, "weather.0.description", weather->description, sizeof(weather->description)\n"\n"\n"\n"\n"\n"\n"\n");
    json_get_string(doc, "weather.0.icon", weather->icon, sizeof(weather->icon)\n"\n"\n"\n"\n"\n"\n"\n");

    json_document_free(doc\n"\n"\n"\n"\n"\n"\n"\n");
    return true;
}

// Google geocoding parser
bool json_parse_google_geocoding(const char* json_str, geocoding_result_t* result) {
    if (!json_str || !result) return false;

    json_document_t* doc = json_parse_string(json_str\n"\n"\n"\n"\n"\n"\n"\n");
    if (!doc || !doc->valid) {
        if (doc) json_document_free(doc\n"\n"\n"\n"\n"\n"\n"\n");
        return false;
    }

    memset(result, 0, sizeof(geocoding_result_t)\n"\n"\n"\n"\n"\n"\n"\n");

    json_get_string(doc, "results.0.formatted_address", result->formatted_address, sizeof(result->formatted_address)\n"\n"\n"\n"\n"\n"\n"\n");
    json_get_double(doc, "results.0.geometry.location.lat", &result->latitude\n"\n"\n"\n"\n"\n"\n"\n");
    json_get_double(doc, "results.0.geometry.location.lng", &result->longitude\n"\n"\n"\n"\n"\n"\n"\n");

    // Parse address components
    int components_size = json_get_array_size(doc, "results.0.address_components"\n"\n"\n"\n"\n"\n"\n"\n");
    for (int i = 0; i < components_size; i++) {
        char path[128];
        char type[64];
        char long_name[256];

        snprintf(path, sizeof(path), "results.0.address_components.%d.types.0", i\n"\n"\n"\n"\n"\n"\n"\n");
        if (json_get_string(doc, path, type, sizeof(type))) {
            snprintf(path, sizeof(path), "results.0.address_components.%d.long_name", i\n"\n"\n"\n"\n"\n"\n"\n");
            if (json_get_string(doc, path, long_name, sizeof(long_name))) {
                if (strcmp(type, "country") == 0) {
                    strncpy(result->country, long_name, sizeof(result->country) - 1\n"\n"\n"\n"\n"\n"\n"\n");
                } else if (strcmp(type, "administrative_area_level_1") == 0) {
                    strncpy(result->state, long_name, sizeof(result->state) - 1\n"\n"\n"\n"\n"\n"\n"\n");
                } else if (strcmp(type, "locality") == 0) {
                    strncpy(result->city, long_name, sizeof(result->city) - 1\n"\n"\n"\n"\n"\n"\n"\n");
                } else if (strcmp(type, "postal_code") == 0) {
                    strncpy(result->postal_code, long_name, sizeof(result->postal_code) - 1\n"\n"\n"\n"\n"\n"\n"\n");
                }
            }
        }
    }

    json_document_free(doc\n"\n"\n"\n"\n"\n"\n"\n");
    return true;
}

// GPS data parser
bool json_parse_gps_data(const char* json_str, gps_data_t* gps) {
    if (!json_str || !gps) return false;

    json_document_t* doc = json_parse_string(json_str\n"\n"\n"\n"\n"\n"\n"\n");
    if (!doc || !doc->valid) {
        if (doc) json_document_free(doc\n"\n"\n"\n"\n"\n"\n"\n");
        return false;
    }

    memset(gps, 0, sizeof(gps_data_t)\n"\n"\n"\n"\n"\n"\n"\n");

    json_get_double(doc, "latitude", &gps->latitude\n"\n"\n"\n"\n"\n"\n"\n");
    json_get_double(doc, "longitude", &gps->longitude\n"\n"\n"\n"\n"\n"\n"\n");
    json_get_double(doc, "altitude", &gps->altitude\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Handle float fields with temporary double variables
    double temp_accuracy, temp_speed, temp_heading;
    if (json_get_double(doc, "accuracy", &temp_accuracy)) {
        gps->accuracy = (float)temp_accuracy;
    }
    if (json_get_double(doc, "speed", &temp_speed)) {
        gps->speed = (float)temp_speed;
    }
    if (json_get_double(doc, "heading", &temp_heading)) {
        gps->heading = (float)temp_heading;
    }
    json_get_string(doc, "provider", gps->source, sizeof(gps->source)\n"\n"\n"\n"\n"\n"\n"\n");

    int timestamp_int;
    if (json_get_int(doc, "timestamp", &timestamp_int)) {
        gps->timestamp = (time_t)timestamp_int;
    }

    json_document_free(doc\n"\n"\n"\n"\n"\n"\n"\n");
    return true;
}

// Validation helpers
bool json_validate_required_fields(json_document_t* doc, const char** required_fields, int field_count) {
    if (!doc || !doc->valid || !required_fields) {
        return false;
    }

    for (int i = 0; i < field_count; i++) {
        if (!json_path_exists(doc->root, required_fields[i])) {
            json_set_error("Required field missing: %s", required_fields[i]\n"\n"\n"\n"\n"\n"\n"\n");
            return false;
        }
    }

    return true;
}

// Check if key exists
bool json_has_key(json_document_t* doc, const char* path) {
    if (!doc || !doc->valid || !path) {
        return false;
    }

    return json_path_exists(doc->root, path\n"\n"\n"\n"\n"\n"\n"\n");
}

// Common JSON object creation patterns (reduces duplication)
cJSON* json_create_notification_payload(const char* type, const char* title, const char* message, 
                                        int priority, const char* timestamp, const char* source, 
                                        const char* version, const char* hostname) {
    if (!type || !title || !message) {
        set_json_error("Invalid parameters for notification payload"\n"\n"\n"\n"\n"\n"\n"\n");
        return NULL;
    }
    
    cJSON* root = cJSON_CreateObject(\n"\n"\n"\n"\n"\n"\n"\n");
    if (!root) {
        set_json_error("Failed to create JSON object"\n"\n"\n"\n"\n"\n"\n"\n");
        return NULL;
    }
    
    cJSON_AddStringToObject(root, "type", type\n"\n"\n"\n"\n"\n"\n"\n");
    cJSON_AddStringToObject(root, "title", title\n"\n"\n"\n"\n"\n"\n"\n");
    cJSON_AddStringToObject(root, "message", message\n"\n"\n"\n"\n"\n"\n"\n");
    cJSON_AddNumberToObject(root, "priority", priority\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (timestamp) cJSON_AddStringToObject(root, "timestamp", timestamp\n"\n"\n"\n"\n"\n"\n"\n");
    if (source) cJSON_AddStringToObject(root, "source", source\n"\n"\n"\n"\n"\n"\n"\n");
    if (version) cJSON_AddStringToObject(root, "version", version\n"\n"\n"\n"\n"\n"\n"\n");
    if (hostname) cJSON_AddStringToObject(root, "hostname", hostname\n"\n"\n"\n"\n"\n"\n"\n");
    
    return root;
}

cJSON* json_create_simple_object(const char* key1, const char* value1, 
                                 const char* key2, const char* value2,
                                 const char* key3, const char* value3) {
    cJSON* root = cJSON_CreateObject(\n"\n"\n"\n"\n"\n"\n"\n");
    if (!root) {
        set_json_error("Failed to create JSON object"\n"\n"\n"\n"\n"\n"\n"\n");
        return NULL;
    }
    
    if (key1 && value1) cJSON_AddStringToObject(root, key1, value1\n"\n"\n"\n"\n"\n"\n"\n");
    if (key2 && value2) cJSON_AddStringToObject(root, key2, value2\n"\n"\n"\n"\n"\n"\n"\n");
    if (key3 && value3) cJSON_AddStringToObject(root, key3, value3\n"\n"\n"\n"\n"\n"\n"\n");
    
    return root;
}

cJSON* json_create_status_object(const char* status, const char* message, 
                                double timestamp, const char* module) {
    cJSON* root = cJSON_CreateObject(\n"\n"\n"\n"\n"\n"\n"\n");
    if (!root) {
        set_json_error("Failed to create JSON object"\n"\n"\n"\n"\n"\n"\n"\n");
        return NULL;
    }
    
    if (status) cJSON_AddStringToObject(root, "status", status\n"\n"\n"\n"\n"\n"\n"\n");
    if (message) cJSON_AddStringToObject(root, "message", message\n"\n"\n"\n"\n"\n"\n"\n");
    if (timestamp > 0) cJSON_AddNumberToObject(root, "timestamp", timestamp\n"\n"\n"\n"\n"\n"\n"\n");
    if (module) cJSON_AddStringToObject(root, "module", module\n"\n"\n"\n"\n"\n"\n"\n");
    
    return root;
}

// WiFi access point JSON creation (consolidates duplicate patterns)
cJSON* json_create_wifi_ap_object(const char* bssid, int signal_strength, int channel, int age) {
    if (!bssid) {
        set_json_error("Invalid BSSID for WiFi AP object"\n"\n"\n"\n"\n"\n"\n"\n");
        return NULL;
    }
    
    cJSON* ap_obj = cJSON_CreateObject(\n"\n"\n"\n"\n"\n"\n"\n");
    if (!ap_obj) {
        set_json_error("Failed to create WiFi AP JSON object"\n"\n"\n"\n"\n"\n"\n"\n");
        return NULL;
    }
    
    cJSON_AddStringToObject(ap_obj, "macAddress", bssid\n"\n"\n"\n"\n"\n"\n"\n");
    cJSON_AddNumberToObject(ap_obj, "signalStrength", signal_strength\n"\n"\n"\n"\n"\n"\n"\n");
    cJSON_AddNumberToObject(ap_obj, "age", age\n"\n"\n"\n"\n"\n"\n"\n");
    cJSON_AddNumberToObject(ap_obj, "channel", channel\n"\n"\n"\n"\n"\n"\n"\n");
    cJSON_AddNumberToObject(ap_obj, "signalToNoiseRatio", signal_strength); // Using signal as SNR approximation
    
    return ap_obj;
}
