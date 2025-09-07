#include "space_track_connector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <pthread.h>

// Default Space-Track configuration
#define SPACE_TRACK_BASE_URL "https://www.space-track.org"
#define SPACE_TRACK_LOGIN_URL "/ajaxauth/login"
#define SPACE_TRACK_TLE_URL "/basicspacedata/query/class/tle_latest/ORDINAL/1/DECAYED/0/format/tle"
#define SPACE_TRACK_GP_URL "/basicspacedata/query/class/gp_latest/ORDINAL/1/DECAYED/0/format/gp"
#define SPACE_TRACK_DEFAULT_TIMEOUT 30
#define SPACE_TRACK_DEFAULT_RATE_LIMIT 15
#define SPACE_TRACK_DEFAULT_CACHE_HOURS 24

// Initialize Space-Track connector
space_track_connector_t* space_track_connector_init(const space_track_config_t *config) {
    if (!config || !config->username[0] || !config->password[0]) {
        return NULL;
    }
    
    space_track_connector_t *connector = calloc(1, sizeof(space_track_connector_t));
    if (!connector) {
        return NULL;
    }
    
    // Copy configuration
    memcpy(&connector->config, config, sizeof(space_track_config_t));
    
    // Set default base URL if not provided
    if (!connector->config.base_url[0]) {
        strncpy(connector->config.base_url, SPACE_TRACK_BASE_URL, sizeof(connector->config.base_url) - 1);
    }
    
    // Initialize curl
    connector->curl_handle = curl_easy_init();
    if (!connector->curl_handle) {
        free(connector);
        return NULL;
    }
    
    // Configure curl for Space-Track
    curl_easy_setopt(connector->curl_handle, CURLOPT_TIMEOUT, connector->config.timeout_seconds);
    curl_easy_setopt(connector->curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(connector->curl_handle, CURLOPT_COOKIEFILE, ""); // Enable cookie engine
    curl_easy_setopt(connector->curl_handle, CURLOPT_USERAGENT, "StarlinkTracker/1.0");
    curl_easy_setopt(connector->curl_handle, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(connector->curl_handle, CURLOPT_SSL_VERIFYHOST, 2L);
    
    // Initialize rate limiter
    pthread_mutex_init(&connector->rate_limiter.rate_limit_mutex, NULL);
    
    // Setup cache directory
    snprintf(connector->cache_directory, sizeof(connector->cache_directory), "/tmp/starlink_tracker_cache");
    mkdir(connector->cache_directory, 0755); // Create if doesn't exist
    
    return connector;
}

// Cleanup Space-Track connector
void space_track_connector_cleanup(space_track_connector_t *connector) {
    if (!connector) {
        return;
    }
    
    if (connector->curl_handle) {
        curl_easy_cleanup(connector->curl_handle);
    }
    
    pthread_mutex_destroy(&connector->rate_limiter.rate_limit_mutex);
    free(connector);
}

// HTTP response write callback
static size_t write_callback(void *contents, size_t size, size_t nmemb, http_response_t *response) {
    size_t realsize = size * nmemb;
    
    char *ptr = realloc(response->data, response->size + realsize + 1);
    if (!ptr) {
        // Out of memory
        return 0;
    }
    
    response->data = ptr;
    memcpy(&(response->data[response->size]), contents, realsize);
    response->size += realsize;
    response->data[response->size] = 0; // Null terminate
    
    return realsize;
}

// Perform HTTP request
static int perform_http_request(space_track_connector_t *connector, const char *url, const char *post_data, http_response_t *response) {
    if (!connector || !url || !response) {
        return SPACE_TRACK_ERROR_INVALID_PARAM;
    }
    
    // Initialize response
    response->data = malloc(1);
    response->size = 0;
    response->response_code = 0;
    response->error_message[0] = '\0';
    
    if (!response->data) {
        return SPACE_TRACK_ERROR_MEMORY_FAILURE;
    }
    
    // Set URL
    curl_easy_setopt(connector->curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(connector->curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(connector->curl_handle, CURLOPT_WRITEDATA, response);
    
    // Set POST data if provided
    if (post_data) {
        curl_easy_setopt(connector->curl_handle, CURLOPT_POSTFIELDS, post_data);
    } else {
        curl_easy_setopt(connector->curl_handle, CURLOPT_HTTPGET, 1L);
    }
    
    // Perform request
    CURLcode res = curl_easy_perform(connector->curl_handle);
    
    if (res != CURLE_OK) {
        snprintf(response->error_message, sizeof(response->error_message), 
                "CURL error: %s", curl_easy_strerror(res));
        free(response->data);
        response->data = NULL;
        return SPACE_TRACK_ERROR_NETWORK_FAILED;
    }
    
    // Get response code
    curl_easy_getinfo(connector->curl_handle, CURLINFO_RESPONSE_CODE, &response->response_code);
    
    // Update statistics
    connector->total_requests++;
    if (response->response_code == 200) {
        connector->successful_requests++;
    }
    
    return SPACE_TRACK_SUCCESS;
}

// Check and enforce rate limiting
static int check_rate_limit(space_track_connector_t *connector) {
    pthread_mutex_lock(&connector->rate_limiter.rate_limit_mutex);
    
    time_t now = time(NULL);
    time_t minute_ago = now - 60;
    
    // Count requests in the last minute
    int requests_in_last_minute = 0;
    for (int i = 0; i < connector->rate_limiter.request_count; i++) {
        if (connector->rate_limiter.request_times[i] > minute_ago) {
            requests_in_last_minute++;
        }
    }
    
    // Check if we're over the rate limit
    if (requests_in_last_minute >= connector->config.rate_limit_requests_per_minute) {
        pthread_mutex_unlock(&connector->rate_limiter.rate_limit_mutex);
        
        // Calculate sleep time
        time_t oldest_request = connector->rate_limiter.request_times[0];
        for (int i = 1; i < connector->rate_limiter.request_count; i++) {
            if (connector->rate_limiter.request_times[i] < oldest_request) {
                oldest_request = connector->rate_limiter.request_times[i];
            }
        }
        
        int sleep_seconds = (oldest_request + 60) - now + 1;
        if (sleep_seconds > 0) {
            sleep(sleep_seconds);
        }
        
        connector->rate_limited_requests++;
        return SPACE_TRACK_ERROR_RATE_LIMITED;
    }
    
    pthread_mutex_unlock(&connector->rate_limiter.rate_limit_mutex);
    return SPACE_TRACK_SUCCESS;
}

// Update rate limiter with new request
static void update_rate_limit(space_track_connector_t *connector) {
    pthread_mutex_lock(&connector->rate_limiter.rate_limit_mutex);
    
    time_t now = time(NULL);
    
    // Add current request time
    if (connector->rate_limiter.request_count < 20) {
        connector->rate_limiter.request_times[connector->rate_limiter.request_count] = now;
        connector->rate_limiter.request_count++;
    } else {
        // Ring buffer - overwrite oldest
        connector->rate_limiter.request_times[connector->rate_limiter.current_index] = now;
        connector->rate_limiter.current_index = (connector->rate_limiter.current_index + 1) % 20;
    }
    
    pthread_mutex_unlock(&connector->rate_limiter.rate_limit_mutex);
}

// Authenticate with Space-Track
int space_track_authenticate(space_track_connector_t *connector) {
    if (!connector) {
        return SPACE_TRACK_ERROR_INVALID_PARAM;
    }
    
    // Check rate limit
    int rate_check = check_rate_limit(connector);
    if (rate_check != SPACE_TRACK_SUCCESS) {
        return rate_check;
    }
    
    // Prepare login URL
    char login_url[512];
    snprintf(login_url, sizeof(login_url), "%s%s", connector->config.base_url, SPACE_TRACK_LOGIN_URL);
    
    // Prepare POST data
    char post_data[256];
    snprintf(post_data, sizeof(post_data), "identity=%s&password=%s", 
             connector->config.username, connector->config.password);
    
    // Perform authentication request
    http_response_t response;
    int result = perform_http_request(connector, login_url, post_data, &response);
    
    update_rate_limit(connector);
    
    if (result != SPACE_TRACK_SUCCESS) {
        return result;
    }
    
    // Check authentication success
    if (response.response_code == 200) {
        connector->authenticated = true;
        connector->auth_time = time(NULL);
        
        if (connector->log_callback) {
            connector->log_callback(1, "Space-Track authentication successful", connector->log_user_data);
        }
    } else {
        connector->auth_failures++;
        snprintf(response.error_message, sizeof(response.error_message), 
                "Authentication failed with HTTP %ld", response.response_code);
        
        if (connector->log_callback) {
            connector->log_callback(3, response.error_message, connector->log_user_data);
        }
        
        result = SPACE_TRACK_ERROR_AUTH_FAILED;
    }
    
    if (response.data) {
        free(response.data);
    }
    
    return result;
}

// Check if authenticated and auth is still valid
bool space_track_is_authenticated(const space_track_connector_t *connector) {
    if (!connector || !connector->authenticated) {
        return false;
    }
    
    // Check if auth is expired (24 hours)
    time_t now = time(NULL);
    if ((now - connector->auth_time) > (24 * 3600)) {
        return false;
    }
    
    return true;
}

// Get Starlink TLE data
int space_track_get_starlink_tles(space_track_connector_t *connector, constellation_data_t *constellation) {
    if (!connector || !constellation) {
        return SPACE_TRACK_ERROR_INVALID_PARAM;
    }
    
    // Check cache first
    if (connector->config.use_cache && space_track_is_cache_valid(connector)) {
        int cache_result = space_track_load_cache(connector, constellation);
        if (cache_result == SPACE_TRACK_SUCCESS) {
            if (connector->log_callback) {
                connector->log_callback(1, "Loaded Starlink TLEs from cache", connector->log_user_data);
            }
            return SPACE_TRACK_SUCCESS;
        }
    }
    
    // Ensure we're authenticated
    if (!space_track_is_authenticated(connector)) {
        int auth_result = space_track_authenticate(connector);
        if (auth_result != SPACE_TRACK_SUCCESS) {
            return auth_result;
        }
    }
    
    // Check rate limit
    int rate_check = check_rate_limit(connector);
    if (rate_check != SPACE_TRACK_SUCCESS) {
        return rate_check;
    }
    
    // Build TLE request URL for Starlink satellites
    char tle_url[1024];
    snprintf(tle_url, sizeof(tle_url), 
             "%s%s/OBJECT_NAME/STARLINK~~/orderby/OBJECT_NAME", 
             connector->config.base_url, SPACE_TRACK_TLE_URL);
    
    // Perform TLE request
    http_response_t response;
    int result = perform_http_request(connector, tle_url, NULL, &response);
    
    update_rate_limit(connector);
    
    if (result != SPACE_TRACK_SUCCESS) {
        return result;
    }
    
    if (response.response_code != 200) {
        if (response.data) {
            free(response.data);
        }
        return SPACE_TRACK_ERROR_NETWORK_FAILED;
    }
    
    // Parse TLE response
    result = space_track_parse_tle_response(response.data, constellation);
    
    if (result == SPACE_TRACK_SUCCESS) {
        constellation->last_update = time(NULL);
        constellation->cache_valid = true;
        
        // Save to cache
        if (connector->config.use_cache) {
            space_track_save_cache(connector, constellation);
        }
        
        if (connector->log_callback) {
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg), "Fetched %d Starlink TLEs from Space-Track", 
                    constellation->num_satellites);
            connector->log_callback(1, log_msg, connector->log_user_data);
        }
    }
    
    if (response.data) {
        free(response.data);
    }
    
    return result;
}

// Parse TLE response from Space-Track
int space_track_parse_tle_response(const char *response, constellation_data_t *constellation) {
    if (!response || !constellation) {
        return SPACE_TRACK_ERROR_INVALID_PARAM;
    }
    
    // Count lines to estimate satellite count
    int line_count = 0;
    for (const char *p = response; *p; p++) {
        if (*p == '\n') {
            line_count++;
        }
    }
    
    // Each satellite has 3 lines (name + 2 TLE lines)
    int estimated_sats = line_count / 3;
    if (estimated_sats == 0) {
        return SPACE_TRACK_ERROR_PARSE_FAILED;
    }
    
    // Allocate memory for satellites
    constellation->satellites = calloc(estimated_sats, sizeof(tle_data_t));
    if (!constellation->satellites) {
        return SPACE_TRACK_ERROR_MEMORY_FAILURE;
    }
    
    // Parse TLE data
    char *response_copy = strdup(response);
    char *line = strtok(response_copy, "\n");
    int sat_index = 0;
    int line_in_tle = 0;
    
    while (line && sat_index < estimated_sats) {
        // Remove trailing whitespace
        char *end = line + strlen(line) - 1;
        while (end > line && (*end == ' ' || *end == '\r' || *end == '\n')) {
            *end = '\0';
            end--;
        }
        
        if (strlen(line) == 0) {
            line = strtok(NULL, "\n");
            continue;
        }
        
        tle_data_t *current_tle = &constellation->satellites[sat_index];
        
        switch (line_in_tle) {
            case 0: // Satellite name
                strncpy(current_tle->satellite_name, line, sizeof(current_tle->satellite_name) - 1);
                break;
                
            case 1: // TLE Line 1
                if (strlen(line) == 69 && line[0] == '1') {
                    strncpy(current_tle->line1, line, sizeof(current_tle->line1) - 1);
                    // Parse epoch from line 1
                    current_tle->epoch = space_track_parse_tle_epoch(line);
                } else {
                    // Invalid TLE line 1, skip this satellite
                    line_in_tle = -1;
                }
                break;
                
            case 2: // TLE Line 2
                if (strlen(line) == 69 && line[0] == '2') {
                    strncpy(current_tle->line2, line, sizeof(current_tle->line2) - 1);
                    current_tle->fetched_time = time(NULL);
                    current_tle->is_valid = true;
                    sat_index++;
                }
                line_in_tle = -1; // Reset for next satellite
                break;
        }
        
        line_in_tle++;
        line = strtok(NULL, "\n");
    }
    
    free(response_copy);
    constellation->num_satellites = sat_index;
    
    if (sat_index == 0) {
        free(constellation->satellites);
        constellation->satellites = NULL;
        return SPACE_TRACK_ERROR_PARSE_FAILED;
    }
    
    return SPACE_TRACK_SUCCESS;
}

// Parse TLE epoch from line 1
time_t space_track_parse_tle_epoch(const char *tle_line1) {
    if (!tle_line1 || strlen(tle_line1) < 69) {
        return 0;
    }
    
    // Extract epoch from positions 19-32 (YYDDDDDDDDd.dddddddd)
    char epoch_str[15];
    strncpy(epoch_str, &tle_line1[18], 14);
    epoch_str[14] = '\0';
    
    // Parse year
    char year_str[3];
    strncpy(year_str, epoch_str, 2);
    year_str[2] = '\0';
    int year = atoi(year_str);
    
    // Convert 2-digit year to 4-digit (assume 2000-2099 range)
    year += (year < 57) ? 2000 : 1900;
    
    // Parse day of year
    char day_str[13];
    strncpy(day_str, &epoch_str[2], 12);
    day_str[12] = '\0';
    double day_of_year = atof(day_str);
    
    // Convert to Unix timestamp
    struct tm tm_epoch = {0};
    tm_epoch.tm_year = year - 1900;
    tm_epoch.tm_mon = 0; // January
    tm_epoch.tm_mday = 1; // 1st day
    
    time_t year_start = mktime(&tm_epoch);
    time_t epoch_time = year_start + (time_t)((day_of_year - 1) * 86400);
    
    return epoch_time;
}

// Check if cache is valid
bool space_track_is_cache_valid(const space_track_connector_t *connector) {
    if (!connector || !connector->config.use_cache) {
        return false;
    }
    
    time_t now = time(NULL);
    time_t cache_expiry = connector->cache_last_update + (connector->config.cache_duration_hours * 3600);
    
    return (now < cache_expiry);
}

// Load constellation data from cache
int space_track_load_cache(space_track_connector_t *connector, constellation_data_t *constellation) {
    if (!connector || !constellation) {
        return SPACE_TRACK_ERROR_INVALID_PARAM;
    }
    
    char cache_file[512];
    snprintf(cache_file, sizeof(cache_file), "%s/starlink_tles.cache", connector->cache_directory);
    
    FILE *fp = fopen(cache_file, "rb");
    if (!fp) {
        return SPACE_TRACK_ERROR_CACHE_FAILED;
    }
    
    // Read header with metadata
    int num_satellites;
    time_t cache_time;
    
    if (fread(&num_satellites, sizeof(int), 1, fp) != 1 ||
        fread(&cache_time, sizeof(time_t), 1, fp) != 1) {
        fclose(fp);
        return SPACE_TRACK_ERROR_CACHE_FAILED;
    }
    
    // Check cache validity
    time_t now = time(NULL);
    if ((now - cache_time) > (connector->config.cache_duration_hours * 3600)) {
        fclose(fp);
        return SPACE_TRACK_ERROR_CACHE_FAILED;
    }
    
    // Allocate memory for satellites
    constellation->satellites = calloc(num_satellites, sizeof(tle_data_t));
    if (!constellation->satellites) {
        fclose(fp);
        return SPACE_TRACK_ERROR_MEMORY_FAILURE;
    }
    
    // Read satellite data
    if (fread(constellation->satellites, sizeof(tle_data_t), num_satellites, fp) != num_satellites) {
        free(constellation->satellites);
        constellation->satellites = NULL;
        fclose(fp);
        return SPACE_TRACK_ERROR_CACHE_FAILED;
    }
    
    fclose(fp);
    
    constellation->num_satellites = num_satellites;
    constellation->last_update = cache_time;
    constellation->cache_valid = true;
    
    return SPACE_TRACK_SUCCESS;
}

// Save constellation data to cache
int space_track_save_cache(space_track_connector_t *connector, const constellation_data_t *constellation) {
    if (!connector || !constellation || !constellation->satellites) {
        return SPACE_TRACK_ERROR_INVALID_PARAM;
    }
    
    char cache_file[512];
    snprintf(cache_file, sizeof(cache_file), "%s/starlink_tles.cache", connector->cache_directory);
    
    FILE *fp = fopen(cache_file, "wb");
    if (!fp) {
        return SPACE_TRACK_ERROR_CACHE_FAILED;
    }
    
    // Write header with metadata
    time_t now = time(NULL);
    if (fwrite(&constellation->num_satellites, sizeof(int), 1, fp) != 1 ||
        fwrite(&now, sizeof(time_t), 1, fp) != 1) {
        fclose(fp);
        return SPACE_TRACK_ERROR_CACHE_FAILED;
    }
    
    // Write satellite data
    if (fwrite(constellation->satellites, sizeof(tle_data_t), constellation->num_satellites, fp) != constellation->num_satellites) {
        fclose(fp);
        return SPACE_TRACK_ERROR_CACHE_FAILED;
    }
    
    fclose(fp);
    
    // Update cache timestamp
    connector->cache_last_update = now;
    
    return SPACE_TRACK_SUCCESS;
}

// Initialize default configuration
void space_track_config_init_defaults(space_track_config_t *config) {
    if (!config) {
        return;
    }
    
    memset(config, 0, sizeof(space_track_config_t));
    strncpy(config->base_url, SPACE_TRACK_BASE_URL, sizeof(config->base_url) - 1);
    config->rate_limit_requests_per_minute = SPACE_TRACK_DEFAULT_RATE_LIMIT;
    config->timeout_seconds = SPACE_TRACK_DEFAULT_TIMEOUT;
    config->use_cache = true;
    config->cache_duration_hours = SPACE_TRACK_DEFAULT_CACHE_HOURS;
}

// Load configuration from environment variables
int space_track_config_from_env(space_track_config_t *config) {
    if (!config) {
        return SPACE_TRACK_ERROR_INVALID_PARAM;
    }
    
    // Initialize defaults first
    space_track_config_init_defaults(config);
    
    // Load from environment
    const char *username = getenv("SPACE_TRACK_USERNAME");
    const char *password = getenv("SPACE_TRACK_PASSWORD");
    
    if (username) {
        strncpy(config->username, username, sizeof(config->username) - 1);
    }
    
    if (password) {
        strncpy(config->password, password, sizeof(config->password) - 1);
    }
    
    // Check if we have required credentials
    if (!config->username[0] || !config->password[0]) {
        return SPACE_TRACK_ERROR_INVALID_PARAM;
    }
    
    return SPACE_TRACK_SUCCESS;
}

// Get statistics
const space_track_stats_t* space_track_get_stats(const space_track_connector_t *connector) {
    static space_track_stats_t stats;
    
    if (!connector) {
        memset(&stats, 0, sizeof(stats));
        return &stats;
    }
    
    stats.total_requests = connector->total_requests;
    stats.successful_requests = connector->successful_requests;
    stats.rate_limited_requests = connector->rate_limited_requests;
    stats.auth_failures = connector->auth_failures;
    stats.cache_hits = connector->cache_hits;
    stats.cache_misses = connector->cache_misses;
    stats.last_request_time = connector->last_request_time;
    
    // Calculate average response time
    if (connector->total_requests > 0) {
        stats.average_response_time = connector->total_response_time / connector->total_requests;
    } else {
        stats.average_response_time = 0.0;
    }
    
    return &stats;
}