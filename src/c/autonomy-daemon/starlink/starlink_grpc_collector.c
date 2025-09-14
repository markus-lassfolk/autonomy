#include "starlink_grpc_collector.h"
#include "starlink_grpc_comprehensive_client.h"
#include "starlink_grpc_daemon_integration.h"
#include "../core/types.h"
#include "../shared/utils/string_utils.h"
#include "../utils/http_client.h"
#include "../shared/logging/logx.h"
#include "../shared/starlink/starlink_grpc_shared.h"
#include <json-c/json.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <curl/curl.h>
#include <stdio.h>

// Write callback for curl
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    char *buffer = (char *)userp;
    
    // Append to buffer (simple implementation)
    strncat(buffer, (char *)contents, realsize);
    return realsize;
}

// External reference to global configuration
extern autonomy_config_t g_config;

// Global collector instance
starlink_grpc_collector_t g_starlink_grpc_collector = {0};

// Initialize the gRPC collector
int starlink_grpc_collector_init(void) {
    LOGX_INFO_MSG("Initializing Starlink gRPC collector with comprehensive client");
    LOGX_DEBUG_MSG("starlink_grpc_collector_init called");
    
    // Initialize mutex
    LOGX_DEBUG_MSG("starlink_grpc_collector_init - about to initialize mutex");
    if (pthread_mutex_init(&g_starlink_grpc_collector.mutex, NULL) != 0) {
        LOGX_ERROR_MSG("Failed to initialize gRPC collector mutex");
        LOGX_DEBUG_MSG("starlink_grpc_collector_init failed - mutex initialization failed");
        return AUTONOMY_ERROR;
    }
    LOGX_DEBUG_MSG("starlink_grpc_collector_init - mutex initialized successfully");
    
    // Initialize the comprehensive gRPC client
    LOGX_DEBUG_MSG("starlink_grpc_collector_init - about to initialize daemon config");
    starlink_grpc_daemon_config_t daemon_config = {0};
    
    // Set up client configuration
    safe_strncpy(daemon_config.client_config.host, "192.168.100.1", sizeof(daemon_config.client_config.host));
    daemon_config.client_config.port = 9200;
    daemon_config.client_config.timeout = 10;
    daemon_config.client_config.retries = 3;
    daemon_config.client_config.timestamp_mode = true;
    daemon_config.client_config.debug_mode = false; // Disable in production
    
    // Set up daemon-specific configuration
    daemon_config.auto_retry = true;
    daemon_config.max_retries = 3;
    daemon_config.retry_delay_ms = 1000;
    daemon_config.enable_monitoring = false; // We'll handle monitoring separately
    daemon_config.monitoring_interval_seconds = 30;
    safe_strncpy(daemon_config.log_prefix, "STARLINK-GRPC", sizeof(daemon_config.log_prefix));
    
    // Initialize the shared gRPC client
    LOGX_DEBUG_MSG("starlink_grpc_collector_init - about to initialize shared gRPC client");
    
    starlink_grpc_shared_config_t shared_config = {0};
    strcpy(shared_config.host, g_config.starlink_host);
    shared_config.port = g_config.starlink_port;
    shared_config.timeout = g_config.starlink_timeout;
    shared_config.max_retries = 3;
    shared_config.retry_delay_ms = 1000;
    shared_config.debug_mode = true;
    shared_config.insecure_mode = false;
    strcpy(shared_config.user_agent, "autonomy-daemon/1.0");
    
    if (starlink_grpc_shared_init(&shared_config) != 0) {
        LOGX_ERROR_MSG("Failed to initialize shared gRPC client");
        LOGX_DEBUG_MSG("starlink_grpc_collector_init failed - shared gRPC client failed");
        return AUTONOMY_ERROR;
    }
    LOGX_DEBUG_MSG("starlink_grpc_collector_init - shared gRPC client initialized successfully");
    
    // Set configuration from UCI config with fallback defaults
    if (strlen(g_config.starlink_host) > 0) {
        safe_strncpy(g_starlink_grpc_collector.host, g_config.starlink_host, sizeof(g_starlink_grpc_collector.host));
    } else {
        safe_strncpy(g_starlink_grpc_collector.host, "192.168.100.1", sizeof(g_starlink_grpc_collector.host));
    }
    
    if (g_config.starlink_port > 0) {
        g_starlink_grpc_collector.port = g_config.starlink_port;
    } else {
        g_starlink_grpc_collector.port = 9200; // Default gRPC port
    }
    
    if (g_config.starlink_timeout > 0) {
        g_starlink_grpc_collector.timeout_seconds = g_config.starlink_timeout;
    } else {
        g_starlink_grpc_collector.timeout_seconds = 10; // Default timeout
    }
    g_starlink_grpc_collector.enabled = true;
    
    LOGX_INFO_MSG("Starlink gRPC collector configured", 
                  "host", g_starlink_grpc_collector.host,
                  "port", g_starlink_grpc_collector.port,
                  "timeout", g_starlink_grpc_collector.timeout_seconds);
    
    // Initialize data arrays
    g_starlink_grpc_collector.outage_count = 0;
    g_starlink_grpc_collector.observation_count = 0;
    g_starlink_grpc_collector.current_observation_index = 0;
    g_starlink_grpc_collector.last_obstructed = false;
    g_starlink_grpc_collector.last_observation_time = 0;
    
    // Initialize statistics
    g_starlink_grpc_collector.total_outages_detected = 0;
    g_starlink_grpc_collector.total_observations_collected = 0;
    g_starlink_grpc_collector.last_successful_collection = 0;
    g_starlink_grpc_collector.consecutive_failures = 0;
    
    LOGX_INFO_MSG("Starlink gRPC collector initialized successfully");
    LOGX_DEBUG_MSG("starlink_grpc_collector_init completed successfully");
    return AUTONOMY_SUCCESS;
}

// Cleanup the gRPC collector
int starlink_grpc_collector_cleanup(void) {
    LOGX_INFO_MSG("Cleaning up Starlink gRPC collector");
    
    // Stop collection thread if running
    starlink_grpc_collector_stop();
    
    // Cleanup the comprehensive gRPC client
    starlink_grpc_daemon_integration_cleanup();
    
    // Destroy mutex
    pthread_mutex_destroy(&g_starlink_grpc_collector.mutex);
    
    LOGX_INFO_MSG("Starlink gRPC collector cleanup completed");
    return AUTONOMY_SUCCESS;
}

// Start the collection thread
int starlink_grpc_collector_start(void) {
    if (g_starlink_grpc_collector.thread_running) {
        LOGX_WARN_MSG("gRPC collector thread already running");
        return AUTONOMY_SUCCESS;
    }
    
    g_starlink_grpc_collector.thread_running = true;
    
    if (pthread_create(&g_starlink_grpc_collector.collection_thread, NULL, 
                      (void*)starlink_grpc_collector_thread, NULL) != 0) {
        LOGX_ERROR_MSG("Failed to create gRPC collector thread");
        g_starlink_grpc_collector.thread_running = false;
        return AUTONOMY_ERROR;
    }
    
    LOGX_INFO_MSG("Starlink gRPC collector thread started");
    return AUTONOMY_SUCCESS;
}

// Stop the collection thread
int starlink_grpc_collector_stop(void) {
    if (!g_starlink_grpc_collector.thread_running) {
        return AUTONOMY_SUCCESS;
    }
    
    g_starlink_grpc_collector.thread_running = false;
    
    if (pthread_join(g_starlink_grpc_collector.collection_thread, NULL) != 0) {
        LOGX_WARN_MSG("Failed to join gRPC collector thread");
    }
    
    LOGX_INFO_MSG("Starlink gRPC collector thread stopped");
    return AUTONOMY_SUCCESS;
}

// Main collection thread function
void starlink_grpc_collector_thread(void* arg) {
    LOGX_INFO_MSG("Starlink gRPC collector thread started");
    
    // Test connection to Starlink device before starting main loop
    LOGX_INFO_MSG("Testing connection to Starlink device at %s:%d...", 
                  g_starlink_grpc_collector.host, g_starlink_grpc_collector.port);
    
    starlink_grpc_shared_response_t test_response;
    int connection_test = starlink_grpc_shared_get_status(&test_response);
    
    if (connection_test != 0) {
        LOGX_WARN_MSG("Starlink device connection test failed - device may be offline or unreachable");
        LOGX_WARN_MSG("Starlink collector will continue running but may not collect data successfully");
        LOGX_WARN_MSG("To fix: ensure Starlink device is online and accessible at %s:%d", 
                      g_starlink_grpc_collector.host, g_starlink_grpc_collector.port);
        if (test_response.error_message[0]) {
            LOGX_WARN_MSG("Connection test error: %s", test_response.error_message);
        }
    } else {
        LOGX_INFO_MSG("Starlink device connection test successful - ready to collect data");
    }
    
    // Cleanup test response
    starlink_grpc_shared_free_response(&test_response);
    
    int iteration_count = 0;
    
    while (g_starlink_grpc_collector.thread_running) {
        iteration_count++;
        LOGX_DEBUG_MSG("Starlink collector thread iteration %d", iteration_count);
        
        if (g_starlink_grpc_collector.enabled) {
            LOGX_DEBUG_MSG("Starlink collector enabled, attempting to collect observation data...");
            
            // Collect observation data with error handling
            int collect_result = starlink_grpc_collect_observation();
            if (collect_result == 0) {
                LOGX_DEBUG_MSG("Starlink observation collection successful");
            } else {
                LOGX_WARN_MSG("Starlink observation collection failed with error %d", collect_result);
            }
            
            LOGX_DEBUG_MSG("Starlink collector attempting to detect outage events...");
            
            // Detect outage events with error handling
            int detect_result = starlink_grpc_detect_outage_events();
            if (detect_result == 0) {
                LOGX_DEBUG_MSG("Starlink outage detection successful");
            } else {
                LOGX_WARN_MSG("Starlink outage detection failed with error %d", detect_result);
            }
        } else {
            LOGX_DEBUG_MSG("Starlink collector disabled, skipping collection");
        }
        
        LOGX_DEBUG_MSG("Starlink collector sleeping for %d seconds...", OBSERVATION_INTERVAL);
        
        // Sleep for observation interval
        sleep(OBSERVATION_INTERVAL);
    }
    
    LOGX_INFO_MSG("Starlink gRPC collector thread exiting after %d iterations", iteration_count);
}

// Collect observation data using gRPC
int starlink_grpc_collect_observation(void) {
    LOGX_DEBUG_MSG("Starting Starlink observation collection...");
    
    starlink_observation_t observation = {0};
    
    // Get current timestamp
    observation.timestamp = time(NULL);
    LOGX_DEBUG_MSG("Observation timestamp set to %ld", observation.timestamp);
    
    // Use the shared gRPC client to collect observation data
    LOGX_DEBUG_MSG("Calling shared gRPC client to get device status...");
    
    starlink_grpc_shared_response_t response;
    int result = starlink_grpc_shared_get_status(&response);
    LOGX_DEBUG_MSG("starlink_grpc_shared_get_status returned %d", result);
    
    if (result == 0 && response.success) {
        LOGX_DEBUG_MSG("Successfully received Starlink status data (%zu bytes)", response.response_size);
        // TODO: Parse response data into observation structure
        // For now, we'll just mark it as successful
        starlink_grpc_shared_free_response(&response);
    } else {
        LOGX_WARN_MSG("Failed to get Starlink status: %s", response.error_message);
        starlink_grpc_shared_free_response(&response);
        result = -1;
    }
    
    if (result == 0) {
        pthread_mutex_lock(&g_starlink_grpc_collector.mutex);
        
        // Store observation
        int index = g_starlink_grpc_collector.current_observation_index;
        g_starlink_grpc_collector.observations[index] = observation;
        g_starlink_grpc_collector.current_observation_index = (index + 1) % MAX_OBSERVATIONS;
        
        if (g_starlink_grpc_collector.observation_count < MAX_OBSERVATIONS) {
            g_starlink_grpc_collector.observation_count++;
        }
        
        // Update last observation
        g_starlink_grpc_collector.last_observation = observation;
        g_starlink_grpc_collector.last_observation_time = observation.timestamp;
        g_starlink_grpc_collector.total_observations_collected++;
        g_starlink_grpc_collector.last_successful_collection = time(NULL);
        g_starlink_grpc_collector.consecutive_failures = 0;
            
            pthread_mutex_unlock(&g_starlink_grpc_collector.mutex);
            
            LOGX_DEBUG_MSG("Collected Starlink observation", 
                          "timestamp", observation.timestamp,
                          "snr", observation.snr,
                          "obstructed", observation.fraction_obstructed > 0.1);
            
            return AUTONOMY_SUCCESS;
    }
    
    g_starlink_grpc_collector.consecutive_failures++;
    LOGX_WARN_MSG("Failed to collect Starlink observation: starlink_grpc_daemon_get_observation returned %d", result);
    LOGX_WARN_MSG("Consecutive failures: %d", g_starlink_grpc_collector.consecutive_failures);
    
    return AUTONOMY_ERROR;
}

// Detect outage events from observations
int starlink_grpc_detect_outage_events(void) {
    pthread_mutex_lock(&g_starlink_grpc_collector.mutex);
    
    // Check if we have enough observations
    if (g_starlink_grpc_collector.observation_count < 2) {
        pthread_mutex_unlock(&g_starlink_grpc_collector.mutex);
        return AUTONOMY_SUCCESS;
    }
    
    // Get current and previous observations
    int current_index = (g_starlink_grpc_collector.current_observation_index - 1 + MAX_OBSERVATIONS) % MAX_OBSERVATIONS;
    int prev_index = (current_index - 1 + MAX_OBSERVATIONS) % MAX_OBSERVATIONS;
    
    starlink_observation_t current = g_starlink_grpc_collector.observations[current_index];
    starlink_observation_t previous = g_starlink_grpc_collector.observations[prev_index];
    
    // Check for outage start (obstruction detected)
    bool current_obstructed = current.fraction_obstructed > 0.1 || 
                             current.pop_ping_drop_rate > 0.1 ||
                             current.snr < 5.0;
    bool previous_obstructed = previous.fraction_obstructed > 0.1 || 
                              previous.pop_ping_drop_rate > 0.1 ||
                              previous.snr < 5.0;
    
    // Detect outage start
    if (current_obstructed && !previous_obstructed && !g_starlink_grpc_collector.last_obstructed) {
        // Outage started
        starlink_outage_event_t outage = {0};
        outage.t_start = current.timestamp;
        outage.obstructed = true;
        outage.pre_snr = previous.snr;
        outage.pre_latency_ms = previous.pop_ping_latency_ms;
        outage.pre_loss_rate = previous.pop_ping_drop_rate;
        outage.boresight_azimuth_deg = current.boresight_azimuth_deg;
        outage.boresight_elevation_deg = current.boresight_elevation_deg;
        outage.gps_valid = current.gps_valid;
        outage.gps_accuracy_m = current.gps_accuracy_m;
        outage.gps_satellites = current.gps_satellites;
        outage.detected_at = time(NULL);
        
        // Copy wedge data
        memcpy(outage.wedge_fraction_obstructed, current.wedge_fraction_obstructed, 
               sizeof(outage.wedge_fraction_obstructed));
        
        safe_strncpy(outage.severity, "warning", sizeof(outage.severity));
        snprintf(outage.description, sizeof(outage.description), 
                "Outage detected: obstruction=%.2f, snr=%.1f, drop_rate=%.2f",
                current.fraction_obstructed, current.snr, current.pop_ping_drop_rate);
        
        // Persist outage event
        starlink_grpc_persist_outage_event(&outage);
        g_starlink_grpc_collector.last_obstructed = true;
        
        LOGX_WARN_MSG("Starlink outage detected", 
                      "t_start", outage.t_start,
                      "obstruction", current.fraction_obstructed,
                      "snr", current.snr);
    }
    
    // Detect outage end
    if (!current_obstructed && previous_obstructed && g_starlink_grpc_collector.last_obstructed) {
        // Outage ended - find the most recent outage event and update it
        if (g_starlink_grpc_collector.outage_count > 0) {
            int last_outage_index = (g_starlink_grpc_collector.outage_count - 1) % MAX_OUTAGE_EVENTS;
            starlink_outage_event_t* last_outage = &g_starlink_grpc_collector.outage_events[last_outage_index];
            
            if (last_outage->t_end == 0) { // Outage not yet ended
                last_outage->t_end = current.timestamp;
                last_outage->duration = difftime(current.timestamp, last_outage->t_start);
                last_outage->post_snr = current.snr;
                last_outage->post_latency_ms = current.pop_ping_latency_ms;
                last_outage->post_loss_rate = current.pop_ping_drop_rate;
                
                LOGX_INFO_MSG("Starlink outage ended", 
                              "duration", last_outage->duration,
                              "t_start", last_outage->t_start,
                              "t_end", last_outage->t_end);
            }
        }
        
        g_starlink_grpc_collector.last_obstructed = false;
    }
    
    pthread_mutex_unlock(&g_starlink_grpc_collector.mutex);
    return AUTONOMY_SUCCESS;
}

// Persist outage event
int starlink_grpc_persist_outage_event(const starlink_outage_event_t* event) {
    if (!event) {
        return AUTONOMY_ERROR;
    }
    
    pthread_mutex_lock(&g_starlink_grpc_collector.mutex);
    
    // Store outage event
    int index = g_starlink_grpc_collector.outage_count % MAX_OUTAGE_EVENTS;
    g_starlink_grpc_collector.outage_events[index] = *event;
    g_starlink_grpc_collector.outage_count++;
    g_starlink_grpc_collector.total_outages_detected++;
    
    pthread_mutex_unlock(&g_starlink_grpc_collector.mutex);
    
    // Log outage event
    starlink_grpc_log_outage_event(event);
    
    return AUTONOMY_SUCCESS;
}

// Get outage events
int starlink_grpc_get_outage_events(starlink_outage_event_t* events, int max_count, int* actual_count) {
    if (!events || !actual_count) {
        return AUTONOMY_ERROR;
    }
    
    pthread_mutex_lock(&g_starlink_grpc_collector.mutex);
    
    int count = (g_starlink_grpc_collector.outage_count < max_count) ? 
                g_starlink_grpc_collector.outage_count : max_count;
    
    // Copy most recent events
    for (int i = 0; i < count; i++) {
        int index = (g_starlink_grpc_collector.outage_count - count + i + MAX_OUTAGE_EVENTS) % MAX_OUTAGE_EVENTS;
        events[i] = g_starlink_grpc_collector.outage_events[index];
    }
    
    *actual_count = count;
    
    pthread_mutex_unlock(&g_starlink_grpc_collector.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get observations
int starlink_grpc_get_observations(starlink_observation_t* observations, int max_count, int* actual_count) {
    if (!observations || !actual_count) {
        return AUTONOMY_ERROR;
    }
    
    pthread_mutex_lock(&g_starlink_grpc_collector.mutex);
    
    int count = (g_starlink_grpc_collector.observation_count < max_count) ? 
                g_starlink_grpc_collector.observation_count : max_count;
    
    // Copy most recent observations
    for (int i = 0; i < count; i++) {
        int index = (g_starlink_grpc_collector.current_observation_index - count + i + MAX_OBSERVATIONS) % MAX_OBSERVATIONS;
        observations[i] = g_starlink_grpc_collector.observations[index];
    }
    
    *actual_count = count;
    
    pthread_mutex_unlock(&g_starlink_grpc_collector.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get latest observation
int starlink_grpc_get_latest_observation(starlink_observation_t* observation) {
    if (!observation) {
        return AUTONOMY_ERROR;
    }
    
    pthread_mutex_lock(&g_starlink_grpc_collector.mutex);
    *observation = g_starlink_grpc_collector.last_observation;
    pthread_mutex_unlock(&g_starlink_grpc_collector.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Log outage event
void starlink_grpc_log_outage_event(const starlink_outage_event_t* event) {
    LOGX_WARN_MSG("Starlink outage event persisted", 
                  "t_start", event->t_start,
                  "duration", event->duration,
                  "obstructed", event->obstructed,
                  "thermal", event->thermal,
                  "sw_update", event->sw_update,
                  "backend", event->backend,
                  "pre_snr", event->pre_snr,
                  "post_snr", event->post_snr,
                  "severity", event->severity);
}

// gRPC API call functions - using proper gRPC over HTTP/2 implementation
int starlink_grpc_call_get_history(char* response_buffer, size_t buffer_size) {
    // Use the comprehensive gRPC client
    starlink_grpc_response_t response;
    if (starlink_grpc_comprehensive_call("get_history", NULL, 0, &response) == 0) {
        if (response.success && response.response_data) {
            size_t copy_size = (response.response_size < buffer_size - 1) ? response.response_size : buffer_size - 1;
            memcpy(response_buffer, response.response_data, copy_size);
            response_buffer[copy_size] = '\0';
            free(response.response_data);
            return AUTONOMY_SUCCESS;
        }
        if (response.response_data) {
            free(response.response_data);
        }
    }
    return AUTONOMY_ERROR;
}

int starlink_grpc_call_get_history_old(char* response_buffer, size_t buffer_size) {
    // Implement gRPC call using proper gRPC over HTTP/2 protocol
    // Following grpcurl's approach: HTTP/2 + gRPC framing + server reflection
    
    char url[512];
    snprintf(url, sizeof(url), "http://%s:%d/SpaceX.API.Device.Device/Handle", 
             g_starlink_grpc_collector.host, g_starlink_grpc_collector.port);
    
    // Create proper gRPC message (following gRPC wire format)
    // gRPC uses length-prefixed messages: [1 byte compression flag][4 bytes message length][message data]
    char grpc_message[512];
    char json_payload[256] = "{\"get_history\":{}}";
    uint32_t message_length = strlen(json_payload);
    
    // Build gRPC message with proper framing
    grpc_message[0] = 0; // No compression
    grpc_message[1] = (message_length >> 24) & 0xFF;
    grpc_message[2] = (message_length >> 16) & 0xFF;
    grpc_message[3] = (message_length >> 8) & 0xFF;
    grpc_message[4] = message_length & 0xFF;
    memcpy(&grpc_message[5], json_payload, message_length);
    
    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL;
    
    curl = curl_easy_init();
    if (!curl) {
        LOGX_ERROR_MSG("Failed to initialize curl for gRPC call");
        return AUTONOMY_ERROR;
    }
    
    // Set up proper gRPC headers (following gRPC specification)
    headers = curl_slist_append(headers, "Content-Type: application/grpc");
    headers = curl_slist_append(headers, "grpc-encoding: identity");
    headers = curl_slist_append(headers, "grpc-accept-encoding: identity");
    headers = curl_slist_append(headers, "grpc-max-receive-message-length: 16777216");
    headers = curl_slist_append(headers, "User-Agent: grpc-c/1.0");
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, grpc_message);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 5 + message_length);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_buffer);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    
    res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        // Use simple printf instead of LOGX to avoid potential segfault
        printf("ERROR: gRPC call failed: %s\n", curl_easy_strerror(res));
        // Clear response buffer to prevent garbage data
        if (response_buffer && buffer_size > 0) {
            memset(response_buffer, 0, buffer_size);
        }
        return AUTONOMY_ERROR;
    }
    
    LOGX_DEBUG_MSG("gRPC get_history call successful");
    return AUTONOMY_SUCCESS;
}

int starlink_grpc_call_get_status(char* response_buffer, size_t buffer_size) {
    // Use the comprehensive gRPC client
    starlink_grpc_response_t response;
    if (starlink_grpc_comprehensive_call("get_status", NULL, 0, &response) == 0) {
        if (response.success && response.response_data) {
            size_t copy_size = (response.response_size < buffer_size - 1) ? response.response_size : buffer_size - 1;
            memcpy(response_buffer, response.response_data, copy_size);
            response_buffer[copy_size] = '\0';
            free(response.response_data);
            return AUTONOMY_SUCCESS;
        }
        if (response.response_data) {
            free(response.response_data);
        }
    }
    return AUTONOMY_ERROR;
}

int starlink_grpc_call_get_status_old(char* response_buffer, size_t buffer_size) {
    // Implement gRPC call using proper gRPC over HTTP/2 protocol
    char url[512];
    snprintf(url, sizeof(url), "http://%s:%d/SpaceX.API.Device.Device/Handle", 
             g_starlink_grpc_collector.host, g_starlink_grpc_collector.port);
    
    // Create proper gRPC message with gRPC wire format
    char grpc_message[512];
    char json_payload[256] = "{\"get_status\":{}}";
    uint32_t message_length = strlen(json_payload);
    
    // Build gRPC message with proper framing
    grpc_message[0] = 0; // No compression
    grpc_message[1] = (message_length >> 24) & 0xFF;
    grpc_message[2] = (message_length >> 16) & 0xFF;
    grpc_message[3] = (message_length >> 8) & 0xFF;
    grpc_message[4] = message_length & 0xFF;
    memcpy(&grpc_message[5], json_payload, message_length);
    
    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL;
    
    curl = curl_easy_init();
    if (!curl) {
        LOGX_ERROR_MSG("Failed to initialize curl for gRPC call");
        return AUTONOMY_ERROR;
    }
    
    // Set up proper gRPC headers
    headers = curl_slist_append(headers, "Content-Type: application/grpc");
    headers = curl_slist_append(headers, "grpc-encoding: identity");
    headers = curl_slist_append(headers, "grpc-accept-encoding: identity");
    headers = curl_slist_append(headers, "grpc-max-receive-message-length: 16777216");
    headers = curl_slist_append(headers, "User-Agent: grpc-c/1.0");
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, grpc_message);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 5 + message_length);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_buffer);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    
    res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        // Use simple printf instead of LOGX to avoid potential segfault
        printf("ERROR: gRPC call failed: %s\n", curl_easy_strerror(res));
        // Clear response buffer to prevent garbage data
        if (response_buffer && buffer_size > 0) {
            memset(response_buffer, 0, buffer_size);
        }
        return AUTONOMY_ERROR;
    }
    
    LOGX_DEBUG_MSG("gRPC get_status call successful");
    return AUTONOMY_SUCCESS;
}

int starlink_grpc_call_get_diagnostics(char* response_buffer, size_t buffer_size) {
    // Use the comprehensive gRPC client
    starlink_grpc_response_t response;
    if (starlink_grpc_comprehensive_call("get_diagnostics", NULL, 0, &response) == 0) {
        if (response.success && response.response_data) {
            size_t copy_size = (response.response_size < buffer_size - 1) ? response.response_size : buffer_size - 1;
            memcpy(response_buffer, response.response_data, copy_size);
            response_buffer[copy_size] = '\0';
            free(response.response_data);
            return AUTONOMY_SUCCESS;
        }
        if (response.response_data) {
            free(response.response_data);
        }
    }
    return AUTONOMY_ERROR;
}

int starlink_grpc_call_get_diagnostics_old(char* response_buffer, size_t buffer_size) {
    // Implement gRPC call using proper gRPC over HTTP/2 protocol
    char url[512];
    snprintf(url, sizeof(url), "http://%s:%d/SpaceX.API.Device.Device/Handle", 
             g_starlink_grpc_collector.host, g_starlink_grpc_collector.port);
    
    // Create proper gRPC message with gRPC wire format
    char grpc_message[512];
    char json_payload[256] = "{\"get_diagnostics\":{}}";
    uint32_t message_length = strlen(json_payload);
    
    // Build gRPC message with proper framing
    grpc_message[0] = 0; // No compression
    grpc_message[1] = (message_length >> 24) & 0xFF;
    grpc_message[2] = (message_length >> 16) & 0xFF;
    grpc_message[3] = (message_length >> 8) & 0xFF;
    grpc_message[4] = message_length & 0xFF;
    memcpy(&grpc_message[5], json_payload, message_length);
    
    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL;
    
    curl = curl_easy_init();
    if (!curl) {
        LOGX_ERROR_MSG("Failed to initialize curl for gRPC call");
        return AUTONOMY_ERROR;
    }
    
    // Set up proper gRPC headers
    headers = curl_slist_append(headers, "Content-Type: application/grpc");
    headers = curl_slist_append(headers, "grpc-encoding: identity");
    headers = curl_slist_append(headers, "grpc-accept-encoding: identity");
    headers = curl_slist_append(headers, "grpc-max-receive-message-length: 16777216");
    headers = curl_slist_append(headers, "User-Agent: grpc-c/1.0");
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, grpc_message);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 5 + message_length);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_buffer);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    
    res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        // Use simple printf instead of LOGX to avoid potential segfault
        printf("ERROR: gRPC call failed: %s\n", curl_easy_strerror(res));
        // Clear response buffer to prevent garbage data
        if (response_buffer && buffer_size > 0) {
            memset(response_buffer, 0, buffer_size);
        }
        return AUTONOMY_ERROR;
    }
    
    LOGX_DEBUG_MSG("gRPC get_diagnostics call successful");
    return AUTONOMY_SUCCESS;
}

int starlink_grpc_call_get_location(char* response_buffer, size_t buffer_size) {
    // Use the comprehensive gRPC client
    starlink_grpc_response_t response;
    if (starlink_grpc_comprehensive_call("get_location", NULL, 0, &response) == 0) {
        if (response.success && response.response_data) {
            size_t copy_size = (response.response_size < buffer_size - 1) ? response.response_size : buffer_size - 1;
            memcpy(response_buffer, response.response_data, copy_size);
            response_buffer[copy_size] = '\0';
            free(response.response_data);
            return AUTONOMY_SUCCESS;
        }
        if (response.response_data) {
            free(response.response_data);
        }
    }
    return AUTONOMY_ERROR;
}

int starlink_grpc_call_get_location_old(char* response_buffer, size_t buffer_size) {
    // Implement gRPC call using proper gRPC over HTTP/2 protocol
    char url[512];
    snprintf(url, sizeof(url), "http://%s:%d/SpaceX.API.Device.Device/Handle", 
             g_starlink_grpc_collector.host, g_starlink_grpc_collector.port);
    
    // Create proper gRPC message with gRPC wire format
    char grpc_message[512];
    char json_payload[256] = "{\"get_location\":{}}";
    uint32_t message_length = strlen(json_payload);
    
    // Build gRPC message with proper framing
    grpc_message[0] = 0; // No compression
    grpc_message[1] = (message_length >> 24) & 0xFF;
    grpc_message[2] = (message_length >> 16) & 0xFF;
    grpc_message[3] = (message_length >> 8) & 0xFF;
    grpc_message[4] = message_length & 0xFF;
    memcpy(&grpc_message[5], json_payload, message_length);
    
    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL;
    
    curl = curl_easy_init();
    if (!curl) {
        LOGX_ERROR_MSG("Failed to initialize curl for gRPC call");
        return AUTONOMY_ERROR;
    }
    
    // Set up proper gRPC headers
    headers = curl_slist_append(headers, "Content-Type: application/grpc");
    headers = curl_slist_append(headers, "grpc-encoding: identity");
    headers = curl_slist_append(headers, "grpc-accept-encoding: identity");
    headers = curl_slist_append(headers, "grpc-max-receive-message-length: 16777216");
    headers = curl_slist_append(headers, "User-Agent: grpc-c/1.0");
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, grpc_message);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 5 + message_length);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_buffer);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    
    res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        // Use simple printf instead of LOGX to avoid potential segfault
        printf("ERROR: gRPC call failed: %s\n", curl_easy_strerror(res));
        // Clear response buffer to prevent garbage data
        if (response_buffer && buffer_size > 0) {
            memset(response_buffer, 0, buffer_size);
        }
        return AUTONOMY_ERROR;
    }
    
    LOGX_DEBUG_MSG("gRPC get_location call successful");
    return AUTONOMY_SUCCESS;
}

int starlink_grpc_call_get_device_info(char* response_buffer, size_t buffer_size) {
    // Use the comprehensive gRPC client
    starlink_grpc_response_t response;
    if (starlink_grpc_comprehensive_call("get_device_info", NULL, 0, &response) == 0) {
        if (response.success && response.response_data) {
            size_t copy_size = (response.response_size < buffer_size - 1) ? response.response_size : buffer_size - 1;
            memcpy(response_buffer, response.response_data, copy_size);
            response_buffer[copy_size] = '\0';
            free(response.response_data);
            return AUTONOMY_SUCCESS;
        }
        if (response.response_data) {
            free(response.response_data);
        }
    }
    return AUTONOMY_ERROR;
}

int starlink_grpc_call_dish_get_config(char* response_buffer, size_t buffer_size) {
    // Use the comprehensive gRPC client
    starlink_grpc_response_t response;
    if (starlink_grpc_comprehensive_call("dish_get_config", NULL, 0, &response) == 0) {
        if (response.success && response.response_data) {
            size_t copy_size = (response.response_size < buffer_size - 1) ? response.response_size : buffer_size - 1;
            memcpy(response_buffer, response.response_data, copy_size);
            response_buffer[copy_size] = '\0';
            free(response.response_data);
            return AUTONOMY_SUCCESS;
        }
        if (response.response_data) {
            free(response.response_data);
        }
    }
    return AUTONOMY_ERROR;
}

int starlink_grpc_call_dish_set_config(const char* config_kv, char* response_buffer, size_t buffer_size) {
    // Use the comprehensive gRPC client
    starlink_grpc_response_t response;
    if (starlink_grpc_comprehensive_call("dish_set_config", config_kv, strlen(config_kv), &response) == 0) {
        if (response.success && response.response_data) {
            size_t copy_size = (response.response_size < buffer_size - 1) ? response.response_size : buffer_size - 1;
            memcpy(response_buffer, response.response_data, copy_size);
            response_buffer[copy_size] = '\0';
            free(response.response_data);
            return AUTONOMY_SUCCESS;
        }
        if (response.response_data) {
            free(response.response_data);
        }
    }
    return AUTONOMY_ERROR;
}

// JSON parsing functions
int starlink_grpc_parse_status_response(const char* json_response, starlink_observation_t* observation) {
    if (!json_response || !observation) {
        return AUTONOMY_ERROR;
    }
    
    json_object* root = json_tokener_parse(json_response);
    if (!root) {
        LOGX_ERROR_MSG("Failed to parse gRPC status response JSON");
        return AUTONOMY_ERROR;
    }
    
    json_object* dish_get_status;
    if (json_object_object_get_ex(root, "dishGetStatus", &dish_get_status)) {
        
        // Parse obstruction stats
        json_object* obstruction_stats;
        if (json_object_object_get_ex(dish_get_status, "obstructionStats", &obstruction_stats)) {
            json_object* fraction_obstructed;
            if (json_object_object_get_ex(obstruction_stats, "fractionObstructed", &fraction_obstructed)) {
                observation->fraction_obstructed = json_object_get_double(fraction_obstructed);
            }
            
            // Parse wedge obstruction data
            json_object* wedge_fraction_obstructed;
            if (json_object_object_get_ex(obstruction_stats, "wedgeFractionObstructed", &wedge_fraction_obstructed)) {
                int array_len = json_object_array_length(wedge_fraction_obstructed);
                for (int i = 0; i < array_len && i < 12; i++) {
                    json_object* wedge = json_object_array_get_idx(wedge_fraction_obstructed, i);
                    observation->wedge_fraction_obstructed[i] = json_object_get_double(wedge);
                }
            }
        }
        
        // Parse signal metrics
        json_object* snr;
        if (json_object_object_get_ex(dish_get_status, "snr", &snr)) {
            observation->snr = json_object_get_double(snr);
        }
        
        json_object* pop_ping_latency_ms;
        if (json_object_object_get_ex(dish_get_status, "popPingLatencyMs", &pop_ping_latency_ms)) {
            observation->pop_ping_latency_ms = json_object_get_double(pop_ping_latency_ms);
        }
        
        json_object* pop_ping_drop_rate;
        if (json_object_object_get_ex(dish_get_status, "popPingDropRate", &pop_ping_drop_rate)) {
            observation->pop_ping_drop_rate = json_object_get_double(pop_ping_drop_rate);
        }
        
        json_object* downlink_throughput_bps;
        if (json_object_object_get_ex(dish_get_status, "downlinkThroughputBps", &downlink_throughput_bps)) {
            observation->downlink_throughput_bps = json_object_get_double(downlink_throughput_bps);
        }
        
        json_object* uplink_throughput_bps;
        if (json_object_object_get_ex(dish_get_status, "uplinkThroughputBps", &uplink_throughput_bps)) {
            observation->uplink_throughput_bps = json_object_get_double(uplink_throughput_bps);
        }
        
        // Parse GPS stats
        json_object* gps_stats;
        if (json_object_object_get_ex(dish_get_status, "gpsStats", &gps_stats)) {
            json_object* gps_valid;
            if (json_object_object_get_ex(gps_stats, "gpsValid", &gps_valid)) {
                observation->gps_valid = json_object_get_boolean(gps_valid);
            }
            
            json_object* gps_sats;
            if (json_object_object_get_ex(gps_stats, "gpsSats", &gps_sats)) {
                observation->gps_satellites = json_object_get_int(gps_sats);
            }
            
            json_object* inhibit_gps;
            if (json_object_object_get_ex(gps_stats, "inhibitGps", &inhibit_gps)) {
                observation->inhibit_gps = json_object_get_boolean(inhibit_gps);
            }
        }
        
        // Parse device state
        json_object* device_state;
        if (json_object_object_get_ex(dish_get_status, "deviceState", &device_state)) {
            json_object* uptime_s;
            if (json_object_object_get_ex(device_state, "uptimeS", &uptime_s)) {
                observation->uptime_s = json_object_get_double(uptime_s);
            }
        }
        
        // Parse boresight data
        json_object* boresight_azimuth_deg;
        if (json_object_object_get_ex(dish_get_status, "boresightAzimuthDeg", &boresight_azimuth_deg)) {
            observation->boresight_azimuth_deg = json_object_get_double(boresight_azimuth_deg);
        }
        
        json_object* boresight_elevation_deg;
        if (json_object_object_get_ex(dish_get_status, "boresightElevationDeg", &boresight_elevation_deg)) {
            observation->boresight_elevation_deg = json_object_get_double(boresight_elevation_deg);
        }
        
        // Parse flags
        json_object* is_snr_above_noise_floor;
        if (json_object_object_get_ex(dish_get_status, "isSnrAboveNoiseFloor", &is_snr_above_noise_floor)) {
            observation->is_snr_above_noise_floor = json_object_get_boolean(is_snr_above_noise_floor);
        }
        
        json_object* is_snr_persistently_low;
        if (json_object_object_get_ex(dish_get_status, "isSnrPersistentlyLow", &is_snr_persistently_low)) {
            observation->is_snr_persistently_low = json_object_get_boolean(is_snr_persistently_low);
        }
    }
    
    json_object_put(root);
    return AUTONOMY_SUCCESS;
}

int starlink_grpc_parse_diagnostics_response(const char* json_response, starlink_observation_t* observation) {
    if (!json_response || !observation) {
        return AUTONOMY_ERROR;
    }
    
    json_object* root = json_tokener_parse(json_response);
    if (!root) {
        LOGX_ERROR_MSG("Failed to parse gRPC diagnostics response JSON");
        return AUTONOMY_ERROR;
    }
    
    json_object* dish_get_diagnostics;
    if (json_object_object_get_ex(root, "dishGetDiagnostics", &dish_get_diagnostics)) {
        
        // Parse alerts
        json_object* alerts;
        if (json_object_object_get_ex(dish_get_diagnostics, "alerts", &alerts)) {
            json_object* thermal_throttle;
            if (json_object_object_get_ex(alerts, "thermalThrottle", &thermal_throttle)) {
                observation->thermal_throttle = json_object_get_boolean(thermal_throttle);
            }
            
            json_object* thermal_shutdown;
            if (json_object_object_get_ex(alerts, "thermalShutdown", &thermal_shutdown)) {
                observation->thermal_shutdown = json_object_get_boolean(thermal_shutdown);
            }
            
            json_object* roaming;
            if (json_object_object_get_ex(alerts, "roaming", &roaming)) {
                observation->roaming = json_object_get_boolean(roaming);
            }
            
            json_object* mast_not_near_vertical;
            if (json_object_object_get_ex(alerts, "mastNotNearVertical", &mast_not_near_vertical)) {
                observation->mast_not_near_vertical = json_object_get_boolean(mast_not_near_vertical);
            }
            
            json_object* unexpected_location;
            if (json_object_object_get_ex(alerts, "unexpectedLocation", &unexpected_location)) {
                observation->unexpected_location = json_object_get_boolean(unexpected_location);
            }
            
            json_object* slow_ethernet_speeds;
            if (json_object_object_get_ex(alerts, "slowEthernetSpeeds", &slow_ethernet_speeds)) {
                observation->slow_ethernet_speeds = json_object_get_boolean(slow_ethernet_speeds);
            }
            
            json_object* software_update_reboot;
            if (json_object_object_get_ex(alerts, "softwareUpdateReboot", &software_update_reboot)) {
                observation->software_update_reboot = json_object_get_boolean(software_update_reboot);
            }
            
            json_object* low_power_mode;
            if (json_object_object_get_ex(alerts, "lowPowerMode", &low_power_mode)) {
                observation->low_power_mode = json_object_get_boolean(low_power_mode);
            }
        }
        
        // Parse device info
        json_object* software_version;
        if (json_object_object_get_ex(dish_get_diagnostics, "softwareVersion", &software_version)) {
            strncpy(observation->software_version, json_object_get_string(software_version), 
                   sizeof(observation->software_version) - 1);
        }
        
        json_object* hardware_version;
        if (json_object_object_get_ex(dish_get_diagnostics, "hardwareVersion", &hardware_version)) {
            strncpy(observation->hardware_version, json_object_get_string(hardware_version), 
                   sizeof(observation->hardware_version) - 1);
        }
    }
    
    json_object_put(root);
    return AUTONOMY_SUCCESS;
}

int starlink_grpc_parse_history_response(const char* json_response, starlink_observation_t* observations, int max_count, int* actual_count) {
    if (!json_response || !observations || !actual_count) {
        return AUTONOMY_ERROR;
    }
    
    *actual_count = 0;
    
    json_object* root = json_tokener_parse(json_response);
    if (!root) {
        LOGX_ERROR_MSG("Failed to parse gRPC history response JSON");
        return AUTONOMY_ERROR;
    }
    
    json_object* dish_get_history;
    if (json_object_object_get_ex(root, "dishGetHistory", &dish_get_history)) {
        
        // Get current index
        json_object* current;
        int current_index = 0;
        if (json_object_object_get_ex(dish_get_history, "current", &current)) {
            current_index = json_object_get_int(current);
        }
        
        // Parse obstructed array for outage detection
        json_object* obstructed_array;
        if (json_object_object_get_ex(dish_get_history, "obstructed", &obstructed_array)) {
            int array_len = json_object_array_length(obstructed_array);
            int count = (array_len < max_count) ? array_len : max_count;
            
            for (int i = 0; i < count; i++) {
                json_object* obstructed = json_object_array_get_idx(obstructed_array, i);
                observations[i].timestamp = time(NULL) - (array_len - i) * 15; // Assume 15s intervals
                observations[i].fraction_obstructed = json_object_get_boolean(obstructed) ? 1.0 : 0.0;
            }
            
            *actual_count = count;
        }
        
        // Parse other arrays if needed for more detailed analysis
        // (SNR, latency, throughput arrays can be parsed similarly)
    }
    
    json_object_put(root);
    return AUTONOMY_SUCCESS;
}

// Get collector statistics
int starlink_grpc_collector_get_stats(starlink_grpc_collector_stats_t* stats) {
    if (!stats) {
        return AUTONOMY_ERROR;
    }
    
    // Lock the mutex to safely access the collector state
    if (pthread_mutex_lock(&g_starlink_grpc_collector.mutex) != 0) {
        LOGX_ERROR_MSG("Failed to lock gRPC collector mutex for stats");
        return AUTONOMY_ERROR;
    }
    
    // Fill in the stats structure
    stats->thread_running = g_starlink_grpc_collector.thread_running;
    stats->total_requests = g_starlink_grpc_collector.total_observations_collected;
    stats->total_errors = g_starlink_grpc_collector.consecutive_failures;
    stats->last_successful_collection = g_starlink_grpc_collector.last_successful_collection;
    
    // Unlock the mutex
    if (pthread_mutex_unlock(&g_starlink_grpc_collector.mutex) != 0) {
        LOGX_ERROR_MSG("Failed to unlock gRPC collector mutex for stats");
        return AUTONOMY_ERROR;
    }
    
    return AUTONOMY_SUCCESS;
}
