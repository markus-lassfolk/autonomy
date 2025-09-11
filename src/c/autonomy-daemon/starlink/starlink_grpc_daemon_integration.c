#include "starlink_grpc_daemon_integration.h"
#include "../utils/logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <json-c/json.h>

// Global daemon configuration
starlink_grpc_daemon_config_t g_starlink_grpc_daemon_config = {0};

// Monitoring thread state
static pthread_t g_monitoring_thread = 0;
static bool g_monitoring_active = false;
static bool g_monitoring_should_stop = false;

// Forward declarations
static void* monitoring_thread_worker(void* arg);
static int parse_json_to_observation_partial(const char *json_data, const char *api_method, starlink_observation_t *observation);

// Initialize daemon integration
int starlink_grpc_daemon_integration_init(const starlink_grpc_daemon_config_t *config) {
    fprintf(stderr, "DEBUG: starlink_grpc_daemon_integration_init called\n");
    if (!config) {
        LOGX_ERROR_MSG("Invalid configuration provided to daemon integration");
        fprintf(stderr, "DEBUG: starlink_grpc_daemon_integration_init failed - NULL config\n");
        return -1;
    }
    
    // Copy configuration
    fprintf(stderr, "DEBUG: starlink_grpc_daemon_integration_init - copying config\n");
    memcpy(&g_starlink_grpc_daemon_config, config, sizeof(starlink_grpc_daemon_config_t));
    fprintf(stderr, "DEBUG: starlink_grpc_daemon_integration_init - config copied\n");
    
    // Initialize the comprehensive client
    fprintf(stderr, "DEBUG: starlink_grpc_daemon_integration_init - about to initialize comprehensive client\n");
    if (starlink_grpc_comprehensive_client_init(&g_starlink_grpc_daemon_config.client_config) != 0) {
        LOGX_ERROR_MSG("Failed to initialize comprehensive gRPC client");
        fprintf(stderr, "DEBUG: starlink_grpc_daemon_integration_init failed - comprehensive client init failed\n");
        return -1;
    }
    fprintf(stderr, "DEBUG: starlink_grpc_daemon_integration_init - comprehensive client initialized\n");
    
    LOGX_INFO_MSG("Starlink gRPC daemon integration initialized");
    fprintf(stderr, "DEBUG: starlink_grpc_daemon_integration_init completed successfully\n");
    return 0;
}

// Daemon-specific gRPC calls with retry logic
int starlink_grpc_daemon_get_observation(starlink_observation_t *observation) {
    if (!observation) {
        return -1;
    }
    
    // Initialize observation structure
    memset(observation, 0, sizeof(starlink_observation_t));
    observation->timestamp = time(NULL);
    
    int success_count = 0;
    int total_calls = 0;
    
    // Make multiple API calls to get comprehensive data
    const char* api_calls[] = {
        "get_status",           // Core status and signal metrics
        "get_device_info",      // Device information
        "get_location",         // GPS and location data
        "get_diagnostics",      // Diagnostic information
        "dish_get_context"      // Additional dish context
    };
    
    const int num_calls = sizeof(api_calls) / sizeof(api_calls[0]);
    
    for (int i = 0; i < num_calls; i++) {
        starlink_grpc_response_t response;
        int retries = 0;
        int max_retries = g_starlink_grpc_daemon_config.max_retries;
        bool call_success = false;
        
        while (retries <= max_retries && !call_success) {
            total_calls++;
            
            // Make the gRPC call
            if (starlink_grpc_comprehensive_call(api_calls[i], NULL, 0, &response) == 0) {
                if (response.success && response.response_data) {
                    // Parse response and merge into observation
                    if (parse_json_to_observation_partial(response.response_data, api_calls[i], observation) == 0) {
                        starlink_grpc_daemon_log_response(api_calls[i], &response);
                        call_success = true;
                        success_count++;
                    }
                }
            }
            
            if (!call_success) {
                retries++;
                if (retries <= max_retries) {
                    LOGX_WARN_MSG("gRPC call %s failed, retrying (%d/%d)", api_calls[i], retries, max_retries);
                    usleep(g_starlink_grpc_daemon_config.retry_delay_ms * 1000);
                }
            }
            
            if (response.response_data) {
                free(response.response_data);
            }
        }
        
        if (!call_success) {
            LOGX_WARN_MSG("Failed to get data from %s after %d retries", api_calls[i], max_retries);
        }
    }
    
    // Consider it successful if we got at least the core status data
    if (success_count > 0) {
        LOGX_INFO_MSG("Starlink observation collected: %d/%d API calls successful", success_count, total_calls);
        return 0;
    } else {
        LOGX_ERROR_MSG("Failed to collect any Starlink data from %d API calls", total_calls);
        return -1;
    }
}

int starlink_grpc_daemon_get_status(starlink_observation_t *observation) {
    return starlink_grpc_daemon_get_observation(observation);
}

int starlink_grpc_daemon_get_device_info(starlink_observation_t *observation) {
    return starlink_grpc_daemon_get_observation(observation);
}

int starlink_grpc_daemon_get_location(starlink_observation_t *observation) {
    return starlink_grpc_daemon_get_observation(observation);
}

int starlink_grpc_daemon_get_diagnostics(starlink_observation_t *observation) {
    return starlink_grpc_daemon_get_observation(observation);
}

// Monitoring functions
int starlink_grpc_daemon_start_monitoring(void) {
    if (g_monitoring_active) {
        LOGX_WARN_MSG("Monitoring is already active");
        return 0;
    }
    
    if (!g_starlink_grpc_daemon_config.enable_monitoring) {
        LOGX_WARN_MSG("Monitoring is disabled in configuration");
        return -1;
    }
    
    g_monitoring_should_stop = false;
    
    if (pthread_create(&g_monitoring_thread, NULL, monitoring_thread_worker, NULL) != 0) {
        LOGX_ERROR_MSG("Failed to create monitoring thread");
        return -1;
    }
    
    g_monitoring_active = true;
    LOGX_INFO_MSG("Started Starlink gRPC monitoring thread");
    return 0;
}

int starlink_grpc_daemon_stop_monitoring(void) {
    if (!g_monitoring_active) {
        return 0;
    }
    
    g_monitoring_should_stop = true;
    
    if (pthread_join(g_monitoring_thread, NULL) != 0) {
        LOGX_ERROR_MSG("Failed to join monitoring thread");
        return -1;
    }
    
    g_monitoring_active = false;
    LOGX_INFO_MSG("Stopped Starlink gRPC monitoring thread");
    return 0;
}

bool starlink_grpc_daemon_is_monitoring(void) {
    return g_monitoring_active;
}

// Configuration management
int starlink_grpc_daemon_update_config(const starlink_grpc_daemon_config_t *config) {
    if (!config) {
        return -1;
    }
    
    // Update configuration
    memcpy(&g_starlink_grpc_daemon_config, config, sizeof(starlink_grpc_daemon_config_t));
    
    // Reinitialize the comprehensive client with new config
    starlink_grpc_comprehensive_client_cleanup();
    if (starlink_grpc_comprehensive_client_init(&g_starlink_grpc_daemon_config.client_config) != 0) {
        LOGX_ERROR_MSG("Failed to reinitialize comprehensive gRPC client with new config");
        return -1;
    }
    
    LOGX_INFO_MSG("Updated Starlink gRPC daemon configuration");
    return 0;
}

const starlink_grpc_daemon_config_t* starlink_grpc_daemon_get_config(void) {
    return &g_starlink_grpc_daemon_config;
}

// Utility functions for daemon
void starlink_grpc_daemon_log_response(const char *method, const starlink_grpc_response_t *response) {
    if (!method || !response) {
        return;
    }
    
    char log_message[1024];
    snprintf(log_message, sizeof(log_message), 
             "%s: %s (HTTP %d, %zu bytes)", 
             g_starlink_grpc_daemon_config.log_prefix,
             method, 
             response->http_status, 
             response->response_size);
    
    if (response->success) {
        LOGX_INFO_MSG("%s", log_message);
    } else {
        LOGX_ERROR_MSG("%s - %s", log_message, response->error_message);
    }
}

int starlink_grpc_daemon_parse_response_to_observation(const starlink_grpc_response_t *response, starlink_observation_t *observation) {
    if (!response || !observation || !response->response_data) {
        return -1;
    }
    
    // Use the partial parser with a generic method name
    return parse_json_to_observation_partial(response->response_data, "generic", observation);
}

// Parse JSON response to starlink_observation_t structure (partial - for specific API methods)
int parse_json_to_observation_partial(const char *json_data, const char *api_method, starlink_observation_t *observation) {
    if (!json_data || !api_method || !observation) {
        return -1;
    }
    
    json_object *root = json_tokener_parse(json_data);
    if (!root) {
        return -1;
    }
    
    int result = 0;
    
    if (strcmp(api_method, "get_status") == 0) {
        // Parse get_status response
        json_object *dish_get_status = json_object_object_get(root, "dishGetStatus");
        if (dish_get_status) {
            // Device state
            json_object *device_state = json_object_object_get(dish_get_status, "deviceState");
            if (device_state) {
                json_object *uptime;
                if (json_object_object_get_ex(device_state, "uptimeS", &uptime)) {
                    observation->uptime_s = json_object_get_double(uptime);
                }
            }
            
            // GPS data
            json_object *gps = json_object_object_get(dish_get_status, "gps");
            if (gps) {
                json_object *gps_valid;
                if (json_object_object_get_ex(gps, "gpsValid", &gps_valid)) {
                    observation->gps_valid = json_object_get_boolean(gps_valid);
                }
                
                json_object *gps_sats;
                if (json_object_object_get_ex(gps, "gpsSats", &gps_sats)) {
                    observation->gps_satellites = json_object_get_int(gps_sats);
                }
                
                json_object *gps_accuracy;
                if (json_object_object_get_ex(gps, "gpsAccuracyM", &gps_accuracy)) {
                    observation->gps_accuracy_m = json_object_get_double(gps_accuracy);
                }
                
                json_object *inhibit_gps;
                if (json_object_object_get_ex(gps, "inhibitGps", &inhibit_gps)) {
                    observation->inhibit_gps = json_object_get_boolean(inhibit_gps);
                }
            }
            
            // Signal metrics
            json_object *snr = json_object_object_get(dish_get_status, "snr");
            if (snr) {
                observation->snr = json_object_get_double(snr);
            }
            
            json_object *pop_ping_latency;
            if (json_object_object_get_ex(dish_get_status, "popPingLatencyMs", &pop_ping_latency)) {
                observation->pop_ping_latency_ms = json_object_get_double(pop_ping_latency);
            }
            
            json_object *pop_ping_drop_rate;
            if (json_object_object_get_ex(dish_get_status, "popPingDropRate", &pop_ping_drop_rate)) {
                observation->pop_ping_drop_rate = json_object_get_double(pop_ping_drop_rate);
            }
            
            json_object *downlink_throughput;
            if (json_object_object_get_ex(dish_get_status, "downlinkThroughputBps", &downlink_throughput)) {
                observation->downlink_throughput_bps = json_object_get_double(downlink_throughput);
            }
            
            json_object *uplink_throughput;
            if (json_object_object_get_ex(dish_get_status, "uplinkThroughputBps", &uplink_throughput)) {
                observation->uplink_throughput_bps = json_object_get_double(uplink_throughput);
            }
            
            // Obstruction data
            json_object *obstruction = json_object_object_get(dish_get_status, "obstruction");
            if (obstruction) {
                json_object *fraction_obstructed;
                if (json_object_object_get_ex(obstruction, "fractionObstructed", &fraction_obstructed)) {
                    observation->fraction_obstructed = json_object_get_double(fraction_obstructed);
                }
                
                // Parse wedge obstruction data
                json_object *wedge_fraction_obstructed = json_object_object_get(obstruction, "wedgeFractionObstructed");
                if (wedge_fraction_obstructed && json_object_is_type(wedge_fraction_obstructed, json_type_array)) {
                    int array_len = json_object_array_length(wedge_fraction_obstructed);
                    for (int i = 0; i < array_len && i < 12; i++) {
                        json_object *wedge = json_object_array_get_idx(wedge_fraction_obstructed, i);
                        if (wedge) {
                            observation->wedge_fraction_obstructed[i] = json_object_get_double(wedge);
                        }
                    }
                }
                
                json_object *wedge_abs_fraction_obstructed = json_object_object_get(obstruction, "wedgeAbsFractionObstructed");
                if (wedge_abs_fraction_obstructed && json_object_is_type(wedge_abs_fraction_obstructed, json_type_array)) {
                    int array_len = json_object_array_length(wedge_abs_fraction_obstructed);
                    for (int i = 0; i < array_len && i < 12; i++) {
                        json_object *wedge = json_object_array_get_idx(wedge_abs_fraction_obstructed, i);
                        if (wedge) {
                            observation->wedge_abs_fraction_obstructed[i] = json_object_get_double(wedge);
                        }
                    }
                }
            }
            
            // Boresight data
            json_object *boresight_azimuth;
            if (json_object_object_get_ex(dish_get_status, "boresightAzimuthDeg", &boresight_azimuth)) {
                observation->boresight_azimuth_deg = json_object_get_double(boresight_azimuth);
            }
            
            json_object *boresight_elevation;
            if (json_object_object_get_ex(dish_get_status, "boresightElevationDeg", &boresight_elevation)) {
                observation->boresight_elevation_deg = json_object_get_double(boresight_elevation);
            }
            
            // Thermal flags
            json_object *thermal_throttle;
            if (json_object_object_get_ex(dish_get_status, "thermalThrottle", &thermal_throttle)) {
                observation->thermal_throttle = json_object_get_boolean(thermal_throttle);
            }
            
            json_object *thermal_shutdown;
            if (json_object_object_get_ex(dish_get_status, "thermalShutdown", &thermal_shutdown)) {
                observation->thermal_shutdown = json_object_get_boolean(thermal_shutdown);
            }
            
            // Additional flags
            json_object *is_snr_above_noise_floor;
            if (json_object_object_get_ex(dish_get_status, "isSnrAboveNoiseFloor", &is_snr_above_noise_floor)) {
                observation->is_snr_above_noise_floor = json_object_get_boolean(is_snr_above_noise_floor);
            }
            
            json_object *is_snr_persistently_low;
            if (json_object_object_get_ex(dish_get_status, "isSnrPersistentlyLow", &is_snr_persistently_low)) {
                observation->is_snr_persistently_low = json_object_get_boolean(is_snr_persistently_low);
            }
            
            json_object *roaming;
            if (json_object_object_get_ex(dish_get_status, "roaming", &roaming)) {
                observation->roaming = json_object_get_boolean(roaming);
            }
            
            json_object *mast_not_near_vertical;
            if (json_object_object_get_ex(dish_get_status, "mastNotNearVertical", &mast_not_near_vertical)) {
                observation->mast_not_near_vertical = json_object_get_boolean(mast_not_near_vertical);
            }
            
            json_object *unexpected_location;
            if (json_object_object_get_ex(dish_get_status, "unexpectedLocation", &unexpected_location)) {
                observation->unexpected_location = json_object_get_boolean(unexpected_location);
            }
            
            json_object *slow_ethernet_speeds;
            if (json_object_object_get_ex(dish_get_status, "slowEthernetSpeeds", &slow_ethernet_speeds)) {
                observation->slow_ethernet_speeds = json_object_get_boolean(slow_ethernet_speeds);
            }
            
            json_object *software_update_reboot;
            if (json_object_object_get_ex(dish_get_status, "softwareUpdateReboot", &software_update_reboot)) {
                observation->software_update_reboot = json_object_get_boolean(software_update_reboot);
            }
            
            json_object *low_power_mode;
            if (json_object_object_get_ex(dish_get_status, "lowPowerMode", &low_power_mode)) {
                observation->low_power_mode = json_object_get_boolean(low_power_mode);
            }
        }
        
    } else if (strcmp(api_method, "get_device_info") == 0) {
        // Parse get_device_info response
        json_object *get_device_info = json_object_object_get(root, "getDeviceInfo");
        if (get_device_info) {
            json_object *device_info = json_object_object_get(get_device_info, "deviceInfo");
            if (device_info) {
                json_object *id;
                if (json_object_object_get_ex(device_info, "id", &id)) {
                    strncpy(observation->software_version, json_object_get_string(id), sizeof(observation->software_version) - 1);
                }
                
                json_object *hardware_version;
                if (json_object_object_get_ex(device_info, "hardwareVersion", &hardware_version)) {
                    strncpy(observation->hardware_version, json_object_get_string(hardware_version), sizeof(observation->hardware_version) - 1);
                }
                
                json_object *software_version;
                if (json_object_object_get_ex(device_info, "softwareVersion", &software_version)) {
                    strncpy(observation->software_version, json_object_get_string(software_version), sizeof(observation->software_version) - 1);
                }
            }
        }
        
    } else if (strcmp(api_method, "get_location") == 0) {
        // Parse get_location response
        json_object *get_location = json_object_object_get(root, "getLocation");
        if (get_location) {
            json_object *lla = json_object_object_get(get_location, "lla");
            if (lla) {
                json_object *lat, *lon, *alt;
                if (json_object_object_get_ex(lla, "lat", &lat)) {
                    observation->latitude = json_object_get_double(lat);
                }
                if (json_object_object_get_ex(lla, "lon", &lon)) {
                    observation->longitude = json_object_get_double(lon);
                }
                if (json_object_object_get_ex(lla, "alt", &alt)) {
                    observation->altitude = json_object_get_double(alt);
                }
            }
        }
        
    } else if (strcmp(api_method, "get_diagnostics") == 0) {
        // Parse get_diagnostics response
        json_object *dish_get_diagnostics = json_object_object_get(root, "dishGetDiagnostics");
        if (dish_get_diagnostics) {
            // Parse diagnostic information
            json_object *eth_speed;
            if (json_object_object_get_ex(dish_get_diagnostics, "ethSpeedMbps", &eth_speed)) {
                observation->eth_speed_mbps = json_object_get_double(eth_speed);
            }
            
            json_object *mobility_class;
            if (json_object_object_get_ex(dish_get_diagnostics, "mobilityClass", &mobility_class)) {
                strncpy(observation->mobility_class, json_object_get_string(mobility_class), sizeof(observation->mobility_class) - 1);
            }
            
            json_object *class_of_service;
            if (json_object_object_get_ex(dish_get_diagnostics, "classOfService", &class_of_service)) {
                strncpy(observation->class_of_service, json_object_get_string(class_of_service), sizeof(observation->class_of_service) - 1);
            }
        }
        
    } else if (strcmp(api_method, "dish_get_context") == 0) {
        // Parse dish_get_context response
        json_object *dish_get_context = json_object_object_get(root, "dishGetContext");
        if (dish_get_context) {
            // Parse additional context information
            // This API might provide additional fields not available in other APIs
        }
    }
    
    json_object_put(root);
    return result;
}

// Parse JSON response to starlink_observation_t structure
int parse_json_to_observation(const char *json_data, starlink_observation_t *observation) {
    if (!json_data || !observation) {
        return -1;
    }
    
    // Initialize observation structure
    memset(observation, 0, sizeof(starlink_observation_t));
    observation->timestamp = time(NULL);
    
    // For now, we'll implement a basic parser that extracts key fields
    // This is a simplified implementation - in production, you'd want to use a proper JSON library
    
    // Look for device info fields
    const char *id_start = strstr(json_data, "\"id\":\"");
    if (id_start) {
        id_start += 6; // Skip "\"id\":\""
        const char *id_end = strchr(id_start, '"');
        if (id_end) {
            size_t id_len = id_end - id_start;
            if (id_len < sizeof(observation->software_version)) {
                strncpy(observation->software_version, id_start, id_len);
                observation->software_version[id_len] = '\0';
            }
        }
    }
    
    // Look for hardware version
    const char *hw_start = strstr(json_data, "\"hardwareVersion\":\"");
    if (hw_start) {
        hw_start += 19; // Skip "\"hardwareVersion\":\""
        const char *hw_end = strchr(hw_start, '"');
        if (hw_end) {
            size_t hw_len = hw_end - hw_start;
            if (hw_len < sizeof(observation->hardware_version)) {
                strncpy(observation->hardware_version, hw_start, hw_len);
                observation->hardware_version[hw_len] = '\0';
            }
        }
    }
    
    // Look for software version
    const char *sw_start = strstr(json_data, "\"softwareVersion\":\"");
    if (sw_start) {
        sw_start += 19; // Skip "\"softwareVersion\":\""
        const char *sw_end = strchr(sw_start, '"');
        if (sw_end) {
            size_t sw_len = sw_end - sw_start;
            if (sw_len < sizeof(observation->software_version)) {
                strncpy(observation->software_version, sw_start, sw_len);
                observation->software_version[sw_len] = '\0';
            }
        }
    }
    
    // Look for uptime
    const char *uptime_start = strstr(json_data, "\"uptimeS\":");
    if (uptime_start) {
        uptime_start += 10; // Skip "\"uptimeS\":"
        observation->uptime_s = atof(uptime_start);
    }
    
    // Look for GPS data
    const char *gps_valid_start = strstr(json_data, "\"gpsValid\":");
    if (gps_valid_start) {
        gps_valid_start += 11; // Skip "\"gpsValid\":"
        if (strncmp(gps_valid_start, "true", 4) == 0) {
            observation->gps_valid = true;
        }
    }
    
    const char *gps_sats_start = strstr(json_data, "\"gpsSats\":");
    if (gps_sats_start) {
        gps_sats_start += 10; // Skip "\"gpsSats\":"
        observation->gps_satellites = atoi(gps_sats_start);
    }
    
    // Look for signal metrics
    const char *snr_start = strstr(json_data, "\"snr\":");
    if (snr_start) {
        snr_start += 6; // Skip "\"snr\":"
        observation->snr = atof(snr_start);
    }
    
    const char *latency_start = strstr(json_data, "\"popPingLatencyMs\":");
    if (latency_start) {
        latency_start += 19; // Skip "\"popPingLatencyMs\":"
        observation->pop_ping_latency_ms = atof(latency_start);
    }
    
    const char *drop_rate_start = strstr(json_data, "\"popPingDropRate\":");
    if (drop_rate_start) {
        drop_rate_start += 17; // Skip "\"popPingDropRate\":"
        observation->pop_ping_drop_rate = atof(drop_rate_start);
    }
    
    const char *dl_start = strstr(json_data, "\"downlinkThroughputBps\":");
    if (dl_start) {
        dl_start += 24; // Skip "\"downlinkThroughputBps\":"
        observation->downlink_throughput_bps = atof(dl_start);
    }
    
    const char *ul_start = strstr(json_data, "\"uplinkThroughputBps\":");
    if (ul_start) {
        ul_start += 22; // Skip "\"uplinkThroughputBps\":"
        observation->uplink_throughput_bps = atof(ul_start);
    }
    
    // Look for obstruction data
    const char *obstruct_start = strstr(json_data, "\"fractionObstructed\":");
    if (obstruct_start) {
        obstruct_start += 21; // Skip "\"fractionObstructed\":"
        observation->fraction_obstructed = atof(obstruct_start);
    }
    
    // Look for boresight data
    const char *az_start = strstr(json_data, "\"boresightAzimuthDeg\":");
    if (az_start) {
        az_start += 22; // Skip "\"boresightAzimuthDeg\":"
        observation->boresight_azimuth_deg = atof(az_start);
    }
    
    const char *el_start = strstr(json_data, "\"boresightElevationDeg\":");
    if (el_start) {
        el_start += 24; // Skip "\"boresightElevationDeg\":"
        observation->boresight_elevation_deg = atof(el_start);
    }
    
    return 0;
}

// Private helper functions
static void* monitoring_thread_worker(void* arg) {
    (void)arg; // Unused parameter
    
    LOGX_INFO_MSG("Starlink gRPC monitoring thread started");
    
    while (!g_monitoring_should_stop) {
        // Collect comprehensive observation data
        starlink_observation_t observation;
        if (starlink_grpc_daemon_get_observation(&observation) == 0) {
            LOGX_DEBUG_MSG("Monitoring: Observation collected successfully (SNR: %.2f, GPS: %s, Uptime: %.0fs)", 
                          observation.snr, 
                          observation.gps_valid ? "valid" : "invalid",
                          observation.uptime_s);
        } else {
            LOGX_WARN_MSG("Monitoring: Failed to collect observation");
        }
        
        // Sleep for monitoring interval
        sleep(g_starlink_grpc_daemon_config.monitoring_interval_seconds);
    }
    
    LOGX_INFO_MSG("Starlink gRPC monitoring thread stopped");
    return NULL;
}


// Cleanup
void starlink_grpc_daemon_integration_cleanup(void) {
    // Stop monitoring if active
    if (g_monitoring_active) {
        starlink_grpc_daemon_stop_monitoring();
    }
    
    // Cleanup comprehensive client
    starlink_grpc_comprehensive_client_cleanup();
    
    LOGX_INFO_MSG("Starlink gRPC daemon integration cleaned up");
}
