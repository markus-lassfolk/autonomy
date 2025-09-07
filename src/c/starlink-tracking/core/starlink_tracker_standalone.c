#include "starlink_tracker_standalone.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <sys/stat.h>
#include <errno.h>

// Initialize default configuration
void standalone_config_init_defaults(standalone_config_t *config) {
    if (!config) {
        return;
    }
    
    memset(config, 0, sizeof(standalone_config_t));
    
    // Default values
    // Load Starlink configuration from UCI
    FILE *uci_fp = popen("uci show autonomy.starlink 2>/dev/null", "r");
    if (uci_fp) {
        char line[512];
        while (fgets(line, sizeof(line), uci_fp)) {
            // Parse UCI output format: autonomy.starlink.option='value'
            char *option_start = strchr(line, '.');
            if (!option_start) continue;
            option_start++; // Skip the dot
            
            char *value_start = strchr(option_start, '=');
            if (!value_start) continue;
            *value_start = '\0';
            value_start++;
            
            // Remove quotes and newline
            char *value_end = strchr(value_start, '\'');
            if (value_end) *value_end = '\0';
            char *newline = strchr(value_start, '\n');
            if (newline) *newline = '\0';
            
            if (strcmp(option_start, "host") == 0) {
                strncpy(config->starlink_dish_ip, value_start, sizeof(config->starlink_dish_ip) - 1);
                config->starlink_dish_ip[sizeof(config->starlink_dish_ip) - 1] = '\0';
            } else if (strcmp(option_start, "port") == 0) {
                config->starlink_dish_port = atoi(value_start);
            }
        }
        pclose(uci_fp);
    } else {
        // Fallback to defaults
        strncpy(config->starlink_dish_ip, "192.168.100.1", sizeof(config->starlink_dish_ip) - 1);
        config->starlink_dish_port = 9200;
    }
    config->update_interval_minutes = 60;
    config->prediction_horizon_hours = 24;
    config->min_elevation_degrees = 25.0;
    config->obstruction_threshold = 0.7;
    config->validation_enabled = true;
    config->cache_duration_hours = 24;
    config->rate_limit_requests_per_minute = 15;
    
    // Standalone-specific defaults
    strncpy(config->config_file, "/var/lib/autonomy/starlink_tracker.conf", sizeof(config->config_file) - 1);
    strncpy(config->log_file, "/var/log/autonomy/starlink_tracker.log", sizeof(config->log_file) - 1);
    strncpy(config->cache_directory, "/var/lib/autonomy/starlink_cache", sizeof(config->cache_directory) - 1);
    config->http_api_port = 8080;
    config->enable_web_interface = true;
    config->log_level = 1; // Info level
}

// Load configuration from file
int standalone_config_load_from_file(const char *filename, standalone_config_t *config) {
    if (!filename || !config) {
        return STANDALONE_ERROR_INVALID_PARAM;
    }
    
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        // File doesn't exist, use defaults
        standalone_config_init_defaults(config);
        return STANDALONE_SUCCESS;
    }
    
    char line[512];
    standalone_config_init_defaults(config); // Start with defaults
    
    while (fgets(line, sizeof(line), fp)) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }
        
        // Parse key=value pairs
        char *equals = strchr(line, '=');
        if (!equals) {
            continue;
        }
        
        *equals = '\0';
        char *key = line;
        char *value = equals + 1;
        
        // Remove whitespace
        while (*key == ' ' || *key == '\t') key++;
        while (*value == ' ' || *value == '\t') value++;
        
        // Parse configuration values
        if (strcmp(key, "space_track_username") == 0) {
            strncpy(config->space_track_username, value, sizeof(config->space_track_username) - 1);
        } else if (strcmp(key, "space_track_password") == 0) {
            strncpy(config->space_track_password, value, sizeof(config->space_track_password) - 1);
        } else if (strcmp(key, "starlink_dish_ip") == 0) {
            strncpy(config->starlink_dish_ip, value, sizeof(config->starlink_dish_ip) - 1);
        } else if (strcmp(key, "starlink_dish_port") == 0) {
            config->starlink_dish_port = atoi(value);
        } else if (strcmp(key, "update_interval_minutes") == 0) {
            config->update_interval_minutes = atoi(value);
        } else if (strcmp(key, "prediction_horizon_hours") == 0) {
            config->prediction_horizon_hours = atoi(value);
        } else if (strcmp(key, "min_elevation_degrees") == 0) {
            config->min_elevation_degrees = atof(value);
        } else if (strcmp(key, "obstruction_threshold") == 0) {
            config->obstruction_threshold = atof(value);
        } else if (strcmp(key, "http_api_port") == 0) {
            config->http_api_port = atoi(value);
        } else if (strcmp(key, "log_level") == 0) {
            config->log_level = atoi(value);
        }
    }
    
    fclose(fp);
    return STANDALONE_SUCCESS;
}

// Save configuration to file
int standalone_config_save_to_file(const char *filename, const standalone_config_t *config) {
    if (!filename || !config) {
        return STANDALONE_ERROR_INVALID_PARAM;
    }
    
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        return STANDALONE_ERROR_CONFIG_FAILED;
    }
    
    fprintf(fp, "# Starlink Tracker Standalone Configuration\n");
    fprintf(fp, "# Generated: %s\n", ctime(&(time_t){time(NULL)}));
    fprintf(fp, "\n");
    
    fprintf(fp, "# Space-Track API credentials\n");
    fprintf(fp, "space_track_username=%s\n", config->space_track_username);
    fprintf(fp, "space_track_password=%s\n", config->space_track_password);
    fprintf(fp, "\n");
    
    fprintf(fp, "# Starlink dish connection\n");
    fprintf(fp, "starlink_dish_ip=%s\n", config->starlink_dish_ip);
    fprintf(fp, "starlink_dish_port=%d\n", config->starlink_dish_port);
    fprintf(fp, "\n");
    
    fprintf(fp, "# Tracking parameters\n");
    fprintf(fp, "update_interval_minutes=%d\n", config->update_interval_minutes);
    fprintf(fp, "prediction_horizon_hours=%d\n", config->prediction_horizon_hours);
    fprintf(fp, "min_elevation_degrees=%.1f\n", config->min_elevation_degrees);
    fprintf(fp, "obstruction_threshold=%.2f\n", config->obstruction_threshold);
    fprintf(fp, "\n");
    
    fprintf(fp, "# Web interface\n");
    fprintf(fp, "http_api_port=%d\n", config->http_api_port);
    fprintf(fp, "\n");
    
    fprintf(fp, "# Logging\n");
    fprintf(fp, "log_level=%d\n", config->log_level);
    
    fclose(fp);
    return STANDALONE_SUCCESS;
}

// Initialize standalone tracker
standalone_tracker_t* standalone_tracker_init(const standalone_config_t *config) {
    if (!config) {
        return NULL;
    }
    
    standalone_tracker_t *tracker = calloc(1, sizeof(standalone_tracker_t));
    if (!tracker) {
        return NULL;
    }
    
    // Copy configuration
    memcpy(&tracker->config, config, sizeof(standalone_config_t));
    
    // Create cache directory
    mkdir(config->cache_directory, 0755);
    
    // Initialize mutex
    if (pthread_mutex_init(&tracker->data_mutex, NULL) != 0) {
        free(tracker);
        return NULL;
    }
    
    // Initialize curl globally
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    tracker->initialized = true;
    
    return tracker;
}

// Cleanup standalone tracker
void standalone_tracker_cleanup(standalone_tracker_t *tracker) {
    if (!tracker) {
        return;
    }
    
    // Stop monitoring
    if (tracker->monitoring_active) {
        standalone_tracker_stop_monitoring(tracker);
    }
    
    // Free memory
    if (tracker->current_positions) {
        free(tracker->current_positions);
    }
    
    if (tracker->predictions) {
        free(tracker->predictions);
    }
    
    if (tracker->constellation.satellites) {
        free(tracker->constellation.satellites);
    }
    
    if (tracker->obstruction_map.snr_data) {
        free(tracker->obstruction_map.snr_data);
    }

    if (tracker->prediction_engine) {
        prediction_engine_cleanup(tracker->prediction_engine);
    }
    
    pthread_mutex_destroy(&tracker->data_mutex);
    
    // Cleanup curl
    curl_global_cleanup();
    
    free(tracker);
}

// Update dish location via gRPC
int standalone_tracker_update_dish_location(standalone_tracker_t *tracker) {
    if (!tracker) {
        return STANDALONE_ERROR_INVALID_PARAM;
    }
    
    char grpc_command[512];
    snprintf(grpc_command, sizeof(grpc_command), 
            "grpcurl -plaintext -d '{\"getLocation\":{}}' %s:%d SpaceX.API.Device.Device/Handle 2>/dev/null",
            tracker->config.starlink_dish_ip, tracker->config.starlink_dish_port);
    
    FILE *fp = popen(grpc_command, "r");
    if (!fp) {
        return STANDALONE_ERROR_API_FAILED;
    }
    
    char response[4096];
    size_t bytes_read = fread(response, 1, sizeof(response) - 1, fp);
    response[bytes_read] = '\0';
    
    int status = pclose(fp);
    if (status != 0) {
        return STANDALONE_ERROR_API_FAILED;
    }
    
    // Parse JSON response
    json_object *root = json_tokener_parse(response);
    if (!root) {
        return STANDALONE_ERROR_API_FAILED;
    }
    
    pthread_mutex_lock(&tracker->data_mutex);
    
    // Extract location data
    json_object *lat_obj, *lon_obj, *alt_obj;
    if (json_object_object_get_ex(root, "latitude", &lat_obj)) {
        tracker->dish_location.latitude = json_object_get_double(lat_obj);
    }
    if (json_object_object_get_ex(root, "longitude", &lon_obj)) {
        tracker->dish_location.longitude = json_object_get_double(lon_obj);
    }
    if (json_object_object_get_ex(root, "altitude", &alt_obj)) {
        tracker->dish_location.altitude = json_object_get_double(alt_obj);
    }
    
    tracker->dish_location.last_update = time(NULL);
    
    pthread_mutex_unlock(&tracker->data_mutex);
    
    json_object_put(root);
    return STANDALONE_SUCCESS;
}

// Update obstruction map via gRPC
int standalone_tracker_update_obstruction_map(standalone_tracker_t *tracker) {
    if (!tracker) {
        return STANDALONE_ERROR_INVALID_PARAM;
    }
    
    char grpc_command[512];
    snprintf(grpc_command, sizeof(grpc_command), 
            "grpcurl -plaintext -d '{\"dishGetObstructionMap\":{}}' %s:%d SpaceX.API.Device.Device/Handle 2>/dev/null",
            tracker->config.starlink_dish_ip, tracker->config.starlink_dish_port);
    
    FILE *fp = popen(grpc_command, "r");
    if (!fp) {
        return STANDALONE_ERROR_API_FAILED;
    }
    
    char response[32768]; // Large buffer for obstruction map
    size_t bytes_read = fread(response, 1, sizeof(response) - 1, fp);
    response[bytes_read] = '\0';
    
    int status = pclose(fp);
    if (status != 0) {
        return STANDALONE_ERROR_API_FAILED;
    }
    
    // Parse obstruction map
    json_object *root = json_tokener_parse(response);
    if (!root) {
        return STANDALONE_ERROR_API_FAILED;
    }
    
    pthread_mutex_lock(&tracker->data_mutex);
    
    json_object *obstruction_response;
    if (json_object_object_get_ex(root, "dishGetObstructionMap", &obstruction_response)) {
        json_object *obstruction_map_obj;
        if (json_object_object_get_ex(obstruction_response, "obstructionMap", &obstruction_map_obj)) {
            json_object *snr_array;
            if (json_object_object_get_ex(obstruction_map_obj, "data", &snr_array)) {
                int array_length = json_object_array_length(snr_array);
                
                if (array_length == 15129) { // 123×123
                    // Allocate SNR data if not already allocated
                    if (!tracker->obstruction_map.snr_data) {
                        tracker->obstruction_map.snr_data = calloc(15129, sizeof(double));
                    }
                    
                    if (tracker->obstruction_map.snr_data) {
                        // Parse SNR values
                        for (int i = 0; i < array_length; i++) {
                            json_object *snr_value = json_object_array_get_idx(snr_array, i);
                            tracker->obstruction_map.snr_data[i] = json_object_get_double(snr_value);
                        }
                        
                        // Set map parameters
                        tracker->obstruction_map.map_diameter = 123;
                        tracker->obstruction_map.center_pixel = 61;
                        tracker->obstruction_map.max_radius_pixels = 61.5;
                        tracker->obstruction_map.min_elevation_deg = 25.0;
                        tracker->obstruction_map.max_elevation_deg = 90.0;
                        tracker->obstruction_map.last_update = time(NULL);
                    }
                }
            }
        }
    }
    
    pthread_mutex_unlock(&tracker->data_mutex);
    
    json_object_put(root);
    return STANDALONE_SUCCESS;
}

// HTTP response callback for curl
static size_t curl_write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    char **response = (char **)userp;
    
    *response = realloc(*response, strlen(*response) + realsize + 1);
    if (*response == NULL) {
        return 0;
    }
    
    strncat(*response, (char *)contents, realsize);
    return realsize;
}

// Update constellation data from Space-Track or CelesTrak
int standalone_tracker_update_constellation_data(standalone_tracker_t *tracker) {
    if (!tracker) {
        return STANDALONE_ERROR_INVALID_PARAM;
    }
    
    // Use CelesTrak for simplicity (no authentication required)
    const char *url = "https://celestrak.org/NORAD/elements/gp.php?GROUP=starlink&FORMAT=tle";
    
    CURL *curl = curl_easy_init();
    if (!curl) {
        return STANDALONE_ERROR_NETWORK_FAILED;
    }
    
    char *response = calloc(1, 1);
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        free(response);
        return STANDALONE_ERROR_NETWORK_FAILED;
    }
    
    // Parse TLE data
    pthread_mutex_lock(&tracker->data_mutex);
    
    // Free existing constellation data
    if (tracker->constellation.satellites) {
        free(tracker->constellation.satellites);
    }
    
    // Count satellites (3 lines per satellite)
    int line_count = 0;
    for (char *p = response; *p; p++) {
        if (*p == '\n') line_count++;
    }
    
    int num_satellites = line_count / 3;
    if (num_satellites > 0) {
        tracker->constellation.satellites = calloc(num_satellites, sizeof(standalone_tle_data_t));
        
        if (tracker->constellation.satellites) {
            // Parse TLE data
            char *line = strtok(response, "\n");
            int sat_index = 0;
            int line_in_tle = 0;
            
            while (line && sat_index < num_satellites) {
                standalone_tle_data_t *tle = &tracker->constellation.satellites[sat_index];
                
                switch (line_in_tle) {
                    case 0: // Satellite name
                        strncpy(tle->satellite_name, line, sizeof(tle->satellite_name) - 1);
                        break;
                    case 1: // TLE Line 1
                        if (strlen(line) == 69 && line[0] == '1') {
                            strncpy(tle->line1, line, sizeof(tle->line1) - 1);
                        }
                        break;
                    case 2: // TLE Line 2
                        if (strlen(line) == 69 && line[0] == '2') {
                            strncpy(tle->line2, line, sizeof(tle->line2) - 1);
                            tle->fetched_time = time(NULL);
                            tle->is_valid = true;
                            sat_index++;
                        }
                        line_in_tle = -1;
                        break;
                }
                
                line_in_tle++;
                line = strtok(NULL, "\n");
            }
            
            tracker->constellation.num_satellites = sat_index;
            tracker->constellation.last_update = time(NULL);
            tracker->constellation.cache_valid = true;
        }
    }
    
    pthread_mutex_unlock(&tracker->data_mutex);
    
    free(response);
    return STANDALONE_SUCCESS;
}

// Simple logging function
static void standalone_log(standalone_tracker_t *tracker, int level, const char *message) {
    if (!tracker || level < tracker->config.log_level) {
        return;
    }
    
    const char *level_names[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    const char *level_name = (level >= 0 && level <= 3) ? level_names[level] : "UNKNOWN";
    
    time_t now = time(NULL);
    char *time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0'; // Remove newline
    
    // Log to file if configured
    if (tracker->config.log_file[0]) {
        FILE *log_fp = fopen(tracker->config.log_file, "a");
        if (log_fp) {
            fprintf(log_fp, "[%s] %s: %s\n", time_str, level_name, message);
            fclose(log_fp);
        }
    }
    
    // Also log to stderr
    fprintf(stderr, "[%s] %s: %s\n", time_str, level_name, message);
    
    // Call user callback if set
    if (tracker->log_callback) {
        tracker->log_callback(level, message, tracker->callback_user_data);
    }
}

// Calculate predictions using the prediction engine
int standalone_tracker_calculate_predictions(standalone_tracker_t *tracker) {
    if (!tracker || !tracker->constellation.satellites || tracker->constellation.num_satellites == 0) {
        return STANDALONE_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&tracker->data_mutex);

    // Initialize prediction engine if not already done
    if (!tracker->prediction_engine) {
        prediction_config_t engine_config;
        prediction_engine_config_init_defaults(&engine_config);
        engine_config.min_elevation_degrees = tracker->config.min_elevation_degrees;
        tracker->prediction_engine = prediction_engine_init(&engine_config);
    }

    if (!tracker->prediction_engine) {
        pthread_mutex_unlock(&tracker->data_mutex);
        return STANDALONE_ERROR_MEMORY_FAILED;
    }

    // Update prediction engine with latest data
    prediction_engine_set_dish_location(tracker->prediction_engine, &tracker->dish_location);
    // Note: Obstruction analyzer would be set here if integrated
    
    // Load constellation data into the prediction engine
    // This requires converting standalone_tle_data_t to constellation_data_t
    constellation_data_t constellation;
    constellation.num_satellites = tracker->constellation.num_satellites;
    constellation.satellites = calloc(constellation.num_satellites, sizeof(tle_data_t));
    if(!constellation.satellites) {
        pthread_mutex_unlock(&tracker->data_mutex);
        return STANDALONE_ERROR_MEMORY_FAILED;
    }

    for(int i = 0; i < tracker->constellation.num_satellites; i++) {
        strncpy(constellation.satellites[i].line1, tracker->constellation.satellites[i].line1, sizeof(constellation.satellites[i].line1) - 1);
        strncpy(constellation.satellites[i].line2, tracker->constellation.satellites[i].line2, sizeof(constellation.satellites[i].line2) - 1);
        constellation.satellites[i].is_valid = tracker->constellation.satellites[i].is_valid;
    }
    
    prediction_engine_load_constellation(tracker->prediction_engine, &constellation);
    free(constellation.satellites);

    // Free existing predictions
    if (tracker->predictions) {
        free(tracker->predictions);
        tracker->predictions = NULL;
    }

    // Calculate predictions
    int result = prediction_engine_calculate_predictions(
        tracker->prediction_engine,
        time(NULL),
        tracker->config.prediction_horizon_hours,
        &tracker->predictions,
        &tracker->num_predictions
    );

    pthread_mutex_unlock(&tracker->data_mutex);

    if (result == PREDICTION_SUCCESS) {
        standalone_log(tracker, 1, "Predictions calculated successfully");
        return STANDALONE_SUCCESS;
    } else {
        standalone_log(tracker, 3, "Prediction calculation failed");
        return STANDALONE_ERROR_PREDICTION_FAILED;
    }
}

// Get current statistics
standalone_stats_t standalone_tracker_get_stats(const standalone_tracker_t *tracker) {
    standalone_stats_t stats = {0};
    
    if (!tracker) {
        return stats;
    }
    
    pthread_mutex_lock((pthread_mutex_t*)&tracker->data_mutex);
    
    stats.total_predictions = tracker->total_predictions;
    stats.correct_predictions = tracker->correct_predictions;
    stats.accuracy_percentage = tracker->accuracy_percentage;
    stats.last_update = tracker->last_update;
    
    // Calculate satellite counts
    stats.visible_satellites = 0;
    stats.unobstructed_satellites = 0;
    
    for (int i = 0; i < tracker->num_current_positions; i++) {
        if (tracker->current_positions[i].is_visible) {
            stats.visible_satellites++;
            if (!tracker->current_positions[i].is_obstructed) {
                stats.unobstructed_satellites++;
            }
        }
    }
    
    // Calculate obstruction percentage
    if (tracker->obstruction_map.snr_data) {
        int obstructed_count = 0;
        int valid_count = 0;
        
        for (int i = 0; i < 15129; i++) {
            if (tracker->obstruction_map.snr_data[i] >= 0.0) {
                valid_count++;
                if (tracker->obstruction_map.snr_data[i] < tracker->config.obstruction_threshold) {
                    obstructed_count++;
                }
            }
        }
        
        if (valid_count > 0) {
            stats.obstruction_percentage = (double)obstructed_count / valid_count * 100.0;
        }
    }
    
    pthread_mutex_unlock((pthread_mutex_t*)&tracker->data_mutex);
    
    return stats;
}

// Start monitoring thread
int standalone_tracker_start_monitoring(standalone_tracker_t *tracker) {
    if (!tracker || tracker->monitoring_active) {
        return STANDALONE_ERROR_INVALID_PARAM;
    }
    
    tracker->monitoring_active = true;
    
    // Create monitoring thread
    if (pthread_create(&tracker->monitoring_thread, NULL, standalone_monitoring_thread, tracker) != 0) {
        tracker->monitoring_active = false;
        return STANDALONE_ERROR_THREAD_FAILED;
    }
    
    standalone_log(tracker, 1, "Monitoring started");
    return STANDALONE_SUCCESS;
}

// Stop monitoring
int standalone_tracker_stop_monitoring(standalone_tracker_t *tracker) {
    if (!tracker || !tracker->monitoring_active) {
        return STANDALONE_SUCCESS;
    }
    
    tracker->monitoring_active = false;
    pthread_join(tracker->monitoring_thread, NULL);
    
    standalone_log(tracker, 1, "Monitoring stopped");
    return STANDALONE_SUCCESS;
}

// Monitoring thread
static void* standalone_monitoring_thread(void *arg) {
    standalone_tracker_t *tracker = (standalone_tracker_t*)arg;
    
    while (tracker->monitoring_active) {
        // Update dish location
        standalone_tracker_update_dish_location(tracker);
        
        // Update obstruction map
        standalone_tracker_update_obstruction_map(tracker);
        
        // Update constellation data (less frequently)
        time_t now = time(NULL);
        if ((now - tracker->constellation.last_update) > (tracker->config.update_interval_minutes * 60)) {
            standalone_tracker_update_constellation_data(tracker);
        }
        
        // Calculate predictions
        standalone_tracker_calculate_predictions(tracker);
        
        tracker->last_update = now;
        
        // Sleep for update interval
        sleep(tracker->config.update_interval_minutes * 60);
    }
    
    return NULL;
}

// Get predictions (returns copy that must be freed)
int standalone_tracker_get_predictions(const standalone_tracker_t *tracker, 
                                      standalone_outage_prediction_t **predictions) {
    if (!tracker || !predictions) {
        return -1;
    }
    
    pthread_mutex_lock((pthread_mutex_t*)&tracker->data_mutex);
    
    if (!tracker->predictions || tracker->num_predictions == 0) {
        *predictions = NULL;
        pthread_mutex_unlock((pthread_mutex_t*)&tracker->data_mutex);
        return 0;
    }
    
    *predictions = calloc(tracker->num_predictions, sizeof(standalone_outage_prediction_t));
    if (!*predictions) {
        pthread_mutex_unlock((pthread_mutex_t*)&tracker->data_mutex);
        return -1;
    }
    
    memcpy(*predictions, tracker->predictions, tracker->num_predictions * sizeof(standalone_outage_prediction_t));
    int count = tracker->num_predictions;
    
    pthread_mutex_unlock((pthread_mutex_t*)&tracker->data_mutex);
    
    return count;
}

// Free predictions
void standalone_tracker_free_predictions(standalone_outage_prediction_t *predictions, int count) {
    if (predictions) {
        free(predictions);
    }
}

// Set logging callback
void standalone_tracker_set_log_callback(standalone_tracker_t *tracker, 
                                        void (*callback)(int level, const char *message, void *user_data),
                                        void *user_data) {
    if (!tracker) {
        return;
    }
    
    tracker->log_callback = callback;
    tracker->callback_user_data = user_data;
}