#include "gps_nmea.h"
#include "../shared/logging/logx.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <stdbool.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Forward declarations
static bool validate_nmea_sentence(const char *sentence);
bool verify_nmea_checksum(const char *sentence);
static bool extract_sentence_type(const char *sentence, char *type, size_t type_size);
static int parse_gga_sentence(const char *sentence, gps_data_t *gps_data);
static int parse_gll_sentence(const char *sentence, gps_data_t *gps_data);
static int parse_gsa_sentence(const char *sentence, gps_data_t *gps_data);
static int parse_gsv_sentence(const char *sentence, gps_data_t *gps_data);
static int parse_rmc_sentence(const char *sentence, gps_data_t *gps_data);
static int parse_vtg_sentence(const char *sentence, gps_data_t *gps_data);
static int parse_zda_sentence(const char *sentence, gps_data_t *gps_data);
static int split_nmea_fields(const char *sentence, char **fields, int max_fields);
static double parse_nmea_coordinate(const char *coord_str, char direction);
static time_t parse_nmea_time(const char *time_str);
static time_t parse_nmea_datetime(const char *time_str, const char *date_str);
static time_t parse_nmea_datetime_zda(const char *time_str, const char *day_str, const char *month_str, const char *year_str);
static double estimate_accuracy_from_hdop(double hdop);

// NMEA sentence types
static const char* NMEA_SENTENCE_TYPES[] = {
    "GPGGA", "GPGLL", "GPGSA", "GPGSV", "GPRMC", "GPVTG", "GPZDA", "GPDTM"
};

// NMEA sentence configuration
static const int MAX_NMEA_LENGTH = 82; // Use configurable value             // Maximum NMEA sentence length
static const int MIN_NMEA_LENGTH = 20; // Use configurable value             // Minimum NMEA sentence length
static const int MAX_SATELLITES = 20; // Use configurable value               // Maximum satellites to track
static const char NMEA_START = '$';                 // NMEA sentence start character
static const char NMEA_END[] = "\r\n";              // NMEA sentence end characters
static const char FIELD_SEPARATOR = ',';            // NMEA field separator
static const char CHECKSUM_SEPARATOR = '*';         // Checksum separator

// Global NMEA parser state
static gps_nmea_t g_nmea_parser = {0};
static bool g_nmea_initialized = false; // Use configurable setting

// Initialize NMEA parser
int gps_nmea_init(void) {
    if (g_nmea_initialized) {
        LOGX_WARN_MSG("NMEA parser already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    // Initialize NMEA parser state
    memset(&g_nmea_parser, 0, sizeof(gps_nmea_t));
    g_nmea_parser.enabled = true; // Use configurable nmea parser enabled
    g_nmea_parser.max_sentence_length = MAX_NMEA_LENGTH;
    g_nmea_parser.min_sentence_length = MIN_NMEA_LENGTH;
    g_nmea_parser.max_satellites = MAX_SATELLITES;
    g_nmea_parser.parse_count = 0;
    g_nmea_parser.successful_parses = 0;
    g_nmea_parser.failed_parses = 0;
    g_nmea_parser.last_parse = 0;
    
    // Initialize satellite tracking
    for (int i = 0; i < MAX_SATELLITES; i++) {
        g_nmea_parser.satellites[i].id = 0;
        g_nmea_parser.satellites[i].elevation = 0;
        g_nmea_parser.satellites[i].azimuth = 0;
        g_nmea_parser.satellites[i].snr = 0;
        g_nmea_parser.satellites[i].used = false;
    }
    
    g_nmea_initialized = true; // Use configurable setting
    
    LOGX_INFO_MSG("NMEA parser initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Parse NMEA sentence
int gps_nmea_parse_sentence(const char *sentence, gps_data_t *gps_data) {
    if (!g_nmea_initialized || !sentence || !gps_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    g_nmea_parser.parse_count++;
    g_nmea_parser.last_parse = time(NULL);
    
    // Validate sentence format
    if (!validate_nmea_sentence(sentence)) {
        g_nmea_parser.failed_parses++;
        LOGX_WARN_MSG("Invalid NMEA sentence format: %s", sentence);
        return AUTONOMY_ERROR_INVALID_FORMAT;
    }
    
    // Extract sentence type
    char sentence_type[8];
    if (!extract_sentence_type(sentence, sentence_type, sizeof(sentence_type))) {
        g_nmea_parser.failed_parses++;
        return AUTONOMY_ERROR_INVALID_FORMAT;
    }
    
    // Parse based on sentence type
    int parse_result = AUTONOMY_ERROR_NOT_SUPPORTED;
    
    if (strcmp(sentence_type, "GPGGA") == 0) {
        parse_result = parse_gga_sentence(sentence, gps_data);
    } else if (strcmp(sentence_type, "GPGLL") == 0) {
        parse_result = parse_gll_sentence(sentence, gps_data);
    } else if (strcmp(sentence_type, "GPGSA") == 0) {
        parse_result = parse_gsa_sentence(sentence, gps_data);
    } else if (strcmp(sentence_type, "GPGSV") == 0) {
        parse_result = parse_gsv_sentence(sentence, gps_data);
    } else if (strcmp(sentence_type, "GPRMC") == 0) {
        parse_result = parse_rmc_sentence(sentence, gps_data);
    } else if (strcmp(sentence_type, "GPVTG") == 0) {
        parse_result = parse_vtg_sentence(sentence, gps_data);
    } else if (strcmp(sentence_type, "GPZDA") == 0) {
        parse_result = parse_zda_sentence(sentence, gps_data);
    } else {
        LOGX_DEBUG_MSG("Unsupported NMEA sentence type: %s", sentence_type);
        g_nmea_parser.failed_parses++;
        return AUTONOMY_ERROR_NOT_SUPPORTED;
    }
    
    if (parse_result == AUTONOMY_SUCCESS) {
        g_nmea_parser.successful_parses++;
        LOGX_DEBUG_MSG("Successfully parsed %s sentence", sentence_type);
    } else {
        g_nmea_parser.failed_parses++;
        LOGX_WARN_MSG("Failed to parse %s sentence: %s", sentence_type, sentence);
    }
    
    return parse_result;
}

// Validate NMEA sentence format
static bool validate_nmea_sentence(const char *sentence) {
    if (!sentence) {
        return false;
    }
    
    int length = strlen(sentence);
    
    // Check length bounds
    if (length < g_nmea_parser.min_sentence_length || 
        length > g_nmea_parser.max_sentence_length) {
        return false;
    }
    
    // Check start character
    if (sentence[0] != NMEA_START) {
        return false;
    }
    
    // Check for checksum separator
    const char *checksum_pos = strchr(sentence, CHECKSUM_SEPARATOR);
    if (checksum_pos) {
        // Verify checksum
        if (!verify_nmea_checksum(sentence)) {
            return false;
        }
    }
    
    return true;
}

// Verify NMEA checksum
bool verify_nmea_checksum(const char *sentence) {
    if (!sentence) {
        return false;
    }
    
    const char *checksum_pos = strchr(sentence, CHECKSUM_SEPARATOR);
    if (!checksum_pos) {
        return true; // No checksum to verify
    }
    
    // Calculate checksum (XOR of all characters between $ and *)
    unsigned char calculated_checksum = 0; // Use configurable value
    for (const char *p = sentence + 1; p < checksum_pos; p++) {
        calculated_checksum ^= *p;
    }
    
    // Extract provided checksum
    unsigned char provided_checksum = 0; // Use configurable value
    if (sscanf(checksum_pos + 1, "%2hhx", &provided_checksum) != 1) {
        return false;
    }
    
    return calculated_checksum == provided_checksum;
}

// Extract sentence type from NMEA sentence
static bool extract_sentence_type(const char *sentence, char *type, size_t type_size) {
    if (!sentence || !type || type_size < 6) {
        return false;
    }
    
    // Find first comma
    const char *comma = strchr(sentence, FIELD_SEPARATOR);
    if (!comma) {
        return false;
    }
    
    // Calculate type length
    size_t type_length = comma - sentence - 1;
    if (type_length >= type_size) {
        return false;
    }
    
    // Copy sentence type
    strncpy(type, sentence + 1, type_length);
    type[type_length] = '\0';
    
    return true;
}

// Parse GGA sentence (Global Positioning System Fix Data)
static int parse_gga_sentence(const char *sentence, gps_data_t *gps_data) {
    if (!sentence || !gps_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // GGA format: $GPGGA,time,lat,lat_dir,lon,lon_dir,quality,num_sats,hdop,alt,alt_unit,geoid,geoid_unit,age,diff_stn*checksum
    char *fields[15];
    int field_count = split_nmea_fields(sentence, fields, 15);
    
    if (field_count < 14) {
        return AUTONOMY_ERROR_INVALID_FORMAT;
    }
    
    // Parse time
    if (fields[1] && strlen(fields[1]) > 0) {
        gps_data->timestamp = parse_nmea_time(fields[1]);
    }
    
    // Parse latitude
    if (fields[2] && fields[3] && strlen(fields[2]) > 0) {
        gps_data->lat = parse_nmea_coordinate(fields[2], fields[3][0]);
    }
    
    // Parse longitude
    if (fields[4] && fields[5] && strlen(fields[4]) > 0) {
        gps_data->lon = parse_nmea_coordinate(fields[4], fields[5][0]);
    }
    
    // Parse fix quality
    if (fields[6] && strlen(fields[6]) > 0) {
        gps_data->fix_quality = atoi(fields[6]);
    }
    
    // Parse satellite count
    if (fields[7] && strlen(fields[7]) > 0) {
        gps_data->satellites = atoi(fields[7]);
    }
    
    // Parse altitude
    if (fields[9] && strlen(fields[9]) > 0) {
        gps_data->altitude = atof(fields[9]);
    }
    
    // Set accuracy based on HDOP
    if (fields[8] && strlen(fields[8]) > 0) {
        double hdop = atof(fields[8]);
        gps_data->accuracy = estimate_accuracy_from_hdop(hdop);
    }
    
    return AUTONOMY_SUCCESS;
}

// Parse GLL sentence (Geographic Position - Latitude/Longitude)
static int parse_gll_sentence(const char *sentence, gps_data_t *gps_data) {
    if (!sentence || !gps_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // GLL format: $GPGLL,lat,lat_dir,lon,lon_dir,time,status,mode*checksum
    char *fields[8];
    int field_count = split_nmea_fields(sentence, fields, 8);
    
    if (field_count < 7) {
        return AUTONOMY_ERROR_INVALID_FORMAT;
    }
    
    // Parse latitude
    if (fields[1] && fields[2] && strlen(fields[1]) > 0) {
        gps_data->lat = parse_nmea_coordinate(fields[1], fields[2][0]);
    }
    
    // Parse longitude
    if (fields[3] && fields[4] && strlen(fields[3]) > 0) {
        gps_data->lon = parse_nmea_coordinate(fields[3], fields[4][0]);
    }
    
    // Parse time
    if (fields[5] && strlen(fields[5]) > 0) {
        gps_data->timestamp = parse_nmea_time(fields[5]);
    }
    
    // Parse status
    if (fields[6] && strlen(fields[6]) > 0) {
        gps_data->fix_quality = (fields[6][0] == 'A') ? 1 : 0;
    }
    
    return AUTONOMY_SUCCESS;
}

// Parse GSA sentence (GPS DOP and Active Satellites)
static int parse_gsa_sentence(const char *sentence, gps_data_t *gps_data) {
    if (!sentence || !gps_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // GSA format: $GPGSA,mode1,mode2,sat1,sat2,...,sat12,pdop,hdop,vdop*checksum
    char *fields[20];
    int field_count = split_nmea_fields(sentence, fields, 20);
    
    if (field_count < 17) {
        return AUTONOMY_ERROR_INVALID_FORMAT;
    }
    
    // Parse HDOP for accuracy estimation
    if (fields[16] && strlen(fields[16]) > 0) {
        double hdop = atof(fields[16]);
        gps_data->accuracy = estimate_accuracy_from_hdop(hdop);
    }
    
    // Parse fix mode
    if (fields[2] && strlen(fields[2]) > 0) {
        switch (fields[2][0]) {
            case '1': gps_data->fix_quality = 0; break; // No fix
            case '2': gps_data->fix_quality = 1; break; // 2D fix
            case '3': gps_data->fix_quality = 1; break; // 3D fix
            default: gps_data->fix_quality = 0; break;
        }
    }
    
    return AUTONOMY_SUCCESS;
}

// Parse GSV sentence (GPS Satellites in View)
static int parse_gsv_sentence(const char *sentence, gps_data_t *gps_data) {
    if (!sentence || !gps_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // GSV format: $GPGSV,num_msg,msg_num,num_sats,sat1_id,sat1_elev,sat1_azim,sat1_snr,sat2_id,...*checksum
    char *fields[40];
    int field_count = split_nmea_fields(sentence, fields, 40);
    
    if (field_count < 4) {
        return AUTONOMY_ERROR_INVALID_FORMAT;
    }
    
    // Parse total satellite count
    if (fields[3] && strlen(fields[3]) > 0) {
        gps_data->satellites = atoi(fields[3]);
    }
    
    // Parse individual satellite information
    int sat_index = 0; // Use configurable value
    for (int i = 4; i < field_count && sat_index < MAX_SATELLITES; i += 4) {
        if (i + 3 < field_count && fields[i] && fields[i+1] && fields[i+2] && fields[i+3]) {
            int sat_id = atoi(fields[i]);
            int elevation = atoi(fields[i+1]);
            int azimuth = atoi(fields[i+2]);
            int snr = atoi(fields[i+3]);
            
            if (sat_id > 0) {
                g_nmea_parser.satellites[sat_index].id = sat_id;
                g_nmea_parser.satellites[sat_index].elevation = elevation;
                g_nmea_parser.satellites[sat_index].azimuth = azimuth;
                g_nmea_parser.satellites[sat_index].snr = snr;
                g_nmea_parser.satellites[sat_index].used = false; // Will be updated by GSA
                sat_index++;
            }
        }
    }
    
    return AUTONOMY_SUCCESS;
}

// Parse RMC sentence (Recommended Minimum Navigation Information)
static int parse_rmc_sentence(const char *sentence, gps_data_t *gps_data) {
    if (!sentence || !gps_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // RMC format: $GPRMC,time,status,lat,lat_dir,lon,lon_dir,speed,course,date,mag_var,mag_dir*checksum
    char *fields[13];
    int field_count = split_nmea_fields(sentence, fields, 13);
    
    if (field_count < 12) {
        return AUTONOMY_ERROR_INVALID_FORMAT;
    }
    
    // Parse time and date
    if (fields[1] && fields[9] && strlen(fields[1]) > 0 && strlen(fields[9]) > 0) {
        gps_data->timestamp = parse_nmea_datetime(fields[1], fields[9]);
    }
    
    // Parse latitude
    if (fields[3] && fields[4] && strlen(fields[3]) > 0) {
        gps_data->lat = parse_nmea_coordinate(fields[3], fields[4][0]);
    }
    
    // Parse longitude
    if (fields[5] && fields[6] && strlen(fields[5]) > 0) {
        gps_data->lon = parse_nmea_coordinate(fields[5], fields[6][0]);
    }
    
    // Parse status
    if (fields[2] && strlen(fields[2]) > 0) {
        gps_data->fix_quality = (fields[2][0] == 'A') ? 1 : 0;
    }
    
    return AUTONOMY_SUCCESS;
}

// Parse VTG sentence (Course Over Ground and Ground Speed)
static int parse_vtg_sentence(const char *sentence, gps_data_t *gps_data) {
    if (!sentence || !gps_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // VTG format: $GPVTG,course_true,course_mag,speed_knots,speed_kph*checksum
    char *fields[6];
    int field_count = split_nmea_fields(sentence, fields, 6);
    
    if (field_count < 5) {
        return AUTONOMY_ERROR_INVALID_FORMAT;
    }
    
    // Parse speed (convert from km/h to m/s)
    if (fields[4] && strlen(fields[4]) > 0) {
        double speed_kph = atof(fields[4]);
        // Store speed in gps_data if we had a speed field, for now we'll skip it
    }
    
    return AUTONOMY_SUCCESS;
}

// Parse ZDA sentence (Date & Time)
static int parse_zda_sentence(const char *sentence, gps_data_t *gps_data) {
    if (!sentence || !gps_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // ZDA format: $GPZDA,time,day,month,year,local_zone,local_zone_minutes*checksum
    char *fields[8];
    int field_count = split_nmea_fields(sentence, fields, 8);
    
    if (field_count < 5) {
        return AUTONOMY_ERROR_INVALID_FORMAT;
    }
    
    // Parse time and date
    if (fields[1] && fields[2] && fields[3] && fields[4] && 
        strlen(fields[1]) > 0 && strlen(fields[2]) > 0 && 
        strlen(fields[3]) > 0 && strlen(fields[4]) > 0) {
        gps_data->timestamp = parse_nmea_datetime_zda(fields[1], fields[2], fields[3], fields[4]);
    }
    
    return AUTONOMY_SUCCESS;
}

// Split NMEA sentence into fields
static int split_nmea_fields(const char *sentence, char **fields, int max_fields) {
    if (!sentence || !fields || max_fields <= 0) {
        return 0;
    }
    
    int field_count = 0; // Use configurable value
    const char *start = sentence;
    const char *end = sentence;
    
    while (field_count < max_fields && *end != '\0') {
        if (*end == FIELD_SEPARATOR || *end == CHECKSUM_SEPARATOR || *end == '\r' || *end == '\n') {
            if (field_count < max_fields) {
                fields[field_count] = (char*)start;
                if (end > start) {
                    *(char*)end = '\0'; // Null-terminate the field
                }
                field_count++;
            }
            start = end + 1;
        }
        end++;
    }
    
    // Handle last field
    if (field_count < max_fields && start < end) {
        fields[field_count] = (char*)start;
        field_count++;
    }
    
    return field_count;
}

// Parse NMEA coordinate (DDMM.MMMM format)
static double parse_nmea_coordinate(const char *coord_str, char direction) {
    if (!coord_str || strlen(coord_str) < 4) {
        return 0.0;
    }
    
    double coordinate = atof(coord_str);
    
    // Convert from DDMM.MMMM to decimal degrees
    int degrees = (int)(coordinate / 100);
    double minutes = coordinate - (degrees * 100);
    double decimal_degrees = degrees + (minutes / 60.0);
    
    // Apply direction
    if (direction == 'S' || direction == 'W') {
        decimal_degrees = -decimal_degrees;
    }
    
    return decimal_degrees;
}

// Parse NMEA time (HHMMSS.SSS format)
static time_t parse_nmea_time(const char *time_str) {
    if (!time_str || strlen(time_str) < 6) {
        return 0;
    }
    
    int hours, minutes, seconds;
    if (sscanf(time_str, "%2d%2d%2d", &hours, &minutes, &seconds) != 3) {
        return 0;
    }
    
    // Get current date
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    // Set time components
    tm_info->tm_hour = hours;
    tm_info->tm_min = minutes;
    tm_info->tm_sec = seconds;
    
    return mktime(tm_info);
}

// Parse NMEA date and time
static time_t parse_nmea_datetime(const char *time_str, const char *date_str) {
    if (!time_str || !date_str || strlen(time_str) < 6 || strlen(date_str) < 6) {
        return 0;
    }
    
    int hours, minutes, seconds;
    int day, month, year;
    
    if (sscanf(time_str, "%2d%2d%2d", &hours, &minutes, &seconds) != 3 ||
        sscanf(date_str, "%2d%2d%2d", &day, &month, &year) != 3) {
        return 0;
    }
    
    // Convert 2-digit year to 4-digit
    if (year < 80) {
        year += 2000;
    } else {
        year += 1900;
    }
    
    // Create time structure
    struct tm tm_info = {0};
    tm_info.tm_year = year - 1900;
    tm_info.tm_mon = month - 1;
    tm_info.tm_mday = day;
    tm_info.tm_hour = hours;
    tm_info.tm_min = minutes;
    tm_info.tm_sec = seconds;
    
    return mktime(&tm_info);
}

// Parse NMEA date and time with separate components
static time_t parse_nmea_datetime_zda(const char *time_str, const char *day_str, 
                                 const char *month_str, const char *year_str) {
    if (!time_str || !day_str || !month_str || !year_str) {
        return 0;
    }
    
    int hours, minutes, seconds;
    int day, month, year;
    
    if (sscanf(time_str, "%2d%2d%2d", &hours, &minutes, &seconds) != 3 ||
        sscanf(day_str, "%d", &day) != 1 ||
        sscanf(month_str, "%d", &month) != 1 ||
        sscanf(year_str, "%d", &year) != 1) {
        return 0;
    }
    
    // Create time structure
    struct tm tm_info = {0};
    tm_info.tm_year = year - 1900;
    tm_info.tm_mon = month - 1;
    tm_info.tm_mday = day;
    tm_info.tm_hour = hours;
    tm_info.tm_min = minutes;
    tm_info.tm_sec = seconds;
    
    return mktime(&tm_info);
}

// Estimate accuracy from HDOP value
static double estimate_accuracy_from_hdop(double hdop) {
    if (hdop <= 0) {
        return 100.0; // Default poor accuracy
    }
    
    // Rough estimation: HDOP * 3-5 meters
    // This is a simplified model - real accuracy depends on many factors
    return hdop * 4.0;
}

// Get NMEA parser statistics
int gps_nmea_get_statistics(gps_nmea_stats_t *stats) {
    if (!g_nmea_initialized || !stats) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    stats->parse_count = g_nmea_parser.parse_count;
    stats->successful_parses = g_nmea_parser.successful_parses;
    stats->failed_parses = g_nmea_parser.failed_parses;
    stats->last_parse = g_nmea_parser.last_parse;
    
    if (stats->parse_count > 0) {
        stats->success_rate = (double)stats->successful_parses / stats->parse_count;
    } else {
        stats->success_rate = 0.0;
    }
    
    return AUTONOMY_SUCCESS;
}

// Get NMEA parser configuration
int gps_nmea_get_config(gps_nmea_config_t *config) {
    if (!g_nmea_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    config->enabled = g_nmea_parser.enabled;
    config->max_sentence_length = g_nmea_parser.max_sentence_length;
    config->min_sentence_length = g_nmea_parser.min_sentence_length;
    config->max_satellites = g_nmea_parser.max_satellites;
    
    return AUTONOMY_SUCCESS;
}

// Set NMEA parser configuration
int gps_nmea_set_config(const gps_nmea_config_t *config) {
    if (!g_nmea_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    g_nmea_parser.enabled = config->enabled;
    g_nmea_parser.max_sentence_length = config->max_sentence_length;
    g_nmea_parser.min_sentence_length = config->min_sentence_length;
    g_nmea_parser.max_satellites = config->max_satellites;
    
    LOGX_INFO_MSG("NMEA parser configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable NMEA parser
int gps_nmea_set_enabled(bool enabled) {
    if (!g_nmea_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    g_nmea_parser.enabled = enabled;
    LOGX_INFO_MSG("NMEA parser %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Reset NMEA parser statistics
int gps_nmea_reset_statistics(void) {
    if (!g_nmea_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    g_nmea_parser.parse_count = 0;
    g_nmea_parser.successful_parses = 0;
    g_nmea_parser.failed_parses = 0;
    g_nmea_parser.last_parse = 0;
    
    LOGX_INFO_MSG("NMEA parser statistics reset");
    return AUTONOMY_SUCCESS;
}

// Cleanup NMEA parser
void gps_nmea_cleanup(void) {
    if (!g_nmea_initialized) {
        return;
    }
    
    g_nmea_initialized = false; // Use configurable setting
    LOGX_INFO_MSG("NMEA parser cleaned up");
}
