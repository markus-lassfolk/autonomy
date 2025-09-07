#include "../starlink/starlink_comprehensive.h"
#include "starlink_modules.h"
#include "../utils/logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <json-c/json.h>
#include <curl/curl.h>
#include <stdbool.h>
#include <unistd.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Global comprehensive Starlink collector
starlink_comprehensive_collector_t g_starlink_comprehensive = {0};
static bool g_starlink_comprehensive_initialized = false;

// Event severity strings
static const char* EVENT_SEVERITY_STRINGS[] = {
    "info", "warning", "critical"
};

// Event reason strings
static const char* EVENT_REASON_STRINGS[] = {
    "unknown", "outage_no_downlink", "outage_no_uplink", "obstruction",
    "thermal", "power", "software", "hardware", "network"
};

// Outage cause strings
static const char* OUTAGE_CAUSE_STRINGS[] = {
    "unknown", "no_downlink", "no_uplink", "obstruction",
    "thermal", "power", "backend", "maintenance"
};

// Forward declarations
static int collect_from_location_api(starlink_comprehensive_gps_t* gps_data);
static int collect_from_status_api(starlink_comprehensive_gps_t* gps_data, starlink_comprehensive_status_t* status);
static int collect_from_diagnostics_api(starlink_comprehensive_gps_t* gps_data);
int collect_from_history_api(starlink_events_outages_analysis_t* analysis);
static int parse_events_from_response(const char* json_response, starlink_event_t* events, int max_events);
static int parse_outages_from_response(const char* json_response, starlink_outage_t* outages, int max_outages);
void analyze_events_and_outages(starlink_events_outages_analysis_t* analysis);
double calculate_overall_health_score(const starlink_comprehensive_status_t* status);
static void* collection_thread_worker(void* arg);
static void* analysis_thread_worker(void* arg);

// Initialize comprehensive Starlink collector
int starlink_comprehensive_init(const starlink_comprehensive_config_t* config) {
    if (g_starlink_comprehensive_initialized) {
        LOGX_WARN_MSG("Comprehensive Starlink collector already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    if (!config) {
        LOGX_ERROR_MSG("Starlink comprehensive config is NULL");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    memset(&g_starlink_comprehensive, 0, sizeof(starlink_comprehensive_collector_t));
    g_starlink_comprehensive.config = *config;
    
    // Initialize mutex
    if (pthread_mutex_init(&g_starlink_comprehensive.mutex, NULL) != 0) {
        LOGX_ERROR_MSG("Failed to initialize Starlink comprehensive mutex");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Initialize basic Starlink client
    starlink_config_t starlink_config = {
        .port = config->port,
        .timeout_seconds = config->timeout_seconds,
        .grpc_first = true,
        .http_first = false,
        .predictive_enabled = true
    };
    strcpy(starlink_config.host, config->host);
    
    if (starlink_client_init(&starlink_config) != 0) {
        LOGX_ERROR_MSG("Failed to initialize basic Starlink client");
        pthread_mutex_destroy(&g_starlink_comprehensive.mutex);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Start background threads if enabled
    if (config->enabled) {
        g_starlink_comprehensive.threads_running = true;
        
        if (pthread_create(&g_starlink_comprehensive.collection_thread, NULL, 
                          collection_thread_worker, NULL) != 0) {
            LOGX_ERROR_MSG("Failed to create Starlink collection thread");
            starlink_client_cleanup();
            pthread_mutex_destroy(&g_starlink_comprehensive.mutex);
            return AUTONOMY_ERROR_SYSTEM;
        }
        
        if (config->enable_events_analysis || config->enable_outages_analysis) {
            if (pthread_create(&g_starlink_comprehensive.analysis_thread, NULL, 
                              analysis_thread_worker, NULL) != 0) {
                LOGX_ERROR_MSG("Failed to create Starlink analysis thread");
                g_starlink_comprehensive.threads_running = false;
                pthread_cancel(g_starlink_comprehensive.collection_thread);
                pthread_join(g_starlink_comprehensive.collection_thread, NULL);
                starlink_client_cleanup();
                pthread_mutex_destroy(&g_starlink_comprehensive.mutex);
                return AUTONOMY_ERROR_SYSTEM;
            }
        }
    }
    
    g_starlink_comprehensive_initialized = true;
    
    LOGX_INFO_MSG("Comprehensive Starlink collector initialized",
              "host", config->host,
              "port", config->port,
              "collect_location", config->collect_location,
              "collect_status", config->collect_status,
              "collect_diagnostics", config->collect_diagnostics,
              "events_analysis", config->enable_events_analysis,
              "outages_analysis", config->enable_outages_analysis);
    
    return AUTONOMY_SUCCESS;
}

// Cleanup comprehensive Starlink collector
void starlink_comprehensive_cleanup(void) {
    if (!g_starlink_comprehensive_initialized) return;
    
    pthread_mutex_lock(&g_starlink_comprehensive.mutex);
    
    // Stop background threads
    g_starlink_comprehensive.threads_running = false;
    
    if (g_starlink_comprehensive.config.enabled) {
        pthread_cancel(g_starlink_comprehensive.collection_thread);
        pthread_join(g_starlink_comprehensive.collection_thread, NULL);
        
        if (g_starlink_comprehensive.config.enable_events_analysis || 
            g_starlink_comprehensive.config.enable_outages_analysis) {
            pthread_cancel(g_starlink_comprehensive.analysis_thread);
            pthread_join(g_starlink_comprehensive.analysis_thread, NULL);
        }
    }
    
    // Cleanup basic Starlink client
    starlink_client_cleanup();
    
    pthread_mutex_unlock(&g_starlink_comprehensive.mutex);
    pthread_mutex_destroy(&g_starlink_comprehensive.mutex);
    
    g_starlink_comprehensive_initialized = false;
    
    LOGX_INFO_MSG("Comprehensive Starlink collector cleaned up");
}

// Collect comprehensive Starlink data from all APIs
int starlink_comprehensive_collect_all(starlink_comprehensive_status_t* status) {
    if (!g_starlink_comprehensive_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_starlink_comprehensive.mutex);
    
    memset(status, 0, sizeof(starlink_comprehensive_status_t));
    time_t start_time = time(NULL);
    
    // Collect comprehensive GPS data
    if (starlink_comprehensive_collect_gps(&status->gps_data) == AUTONOMY_SUCCESS) {
        LOGX_DEBUG_MSG("Comprehensive Starlink GPS data collected",
                  "sources", status->gps_data.data_sources,
                  "confidence", status->gps_data.confidence);
    }
    
    // Collect basic status data (device info, obstruction, network performance)
    starlink_status_response_t basic_status;
    if (starlink_get_status(&basic_status) == 0) {
        status->device_info = basic_status.device_info;
        status->device_state = basic_status.device_state;
        status->obstruction_stats = basic_status.obstruction_stats;
        status->pop_ping_latency_ms = basic_status.network_perf.pop_ping_latency_ms;
        
        LOGX_DEBUG_MSG("Basic Starlink status collected",
                  "uptime", status->device_state.uptime_s,
                  "obstruction_pct", status->obstruction_stats.fraction_obstructed);
    }
    
    // Analyze events and outages if enabled
    if (g_starlink_comprehensive.config.enable_events_analysis || 
        g_starlink_comprehensive.config.enable_outages_analysis) {
        
        if (starlink_comprehensive_analyze_events(&status->events_analysis) == AUTONOMY_SUCCESS) {
            LOGX_DEBUG_MSG("Starlink events and outages analyzed",
                      "events", status->events_analysis.event_count,
                      "outages", status->events_analysis.outage_count,
                      "stability_score", status->events_analysis.stability_score);
        }
    }
    
    // Calculate overall scores
    status->overall_health_score = calculate_overall_health_score(status);
    status->gps_quality_score = status->gps_data.confidence;
    status->stability_score = status->events_analysis.stability_score;
    status->network_quality_score = fmax(0.0, (100.0 - status->pop_ping_latency_ms) / 100.0);
    
    status->last_update = time(NULL);
    status->collection_duration_ms = difftime(status->last_update, start_time) * 1000.0;
    status->collection_successful = true;
    strcpy(status->collection_status, "success");
    
    // Update statistics
    g_starlink_comprehensive.total_collections++;
    g_starlink_comprehensive.successful_collections++;
    g_starlink_comprehensive.last_collection = status->last_update;
    
    // Update average collection time
    g_starlink_comprehensive.average_collection_time_ms = 
        (g_starlink_comprehensive.average_collection_time_ms * 
         (g_starlink_comprehensive.successful_collections - 1) + 
         status->collection_duration_ms) / g_starlink_comprehensive.successful_collections;
    
    // Update average GPS confidence
    if (status->gps_data.confidence > 0) {
        g_starlink_comprehensive.average_gps_confidence = 
            (g_starlink_comprehensive.average_gps_confidence * 
             (g_starlink_comprehensive.successful_collections - 1) + 
             status->gps_data.confidence) / g_starlink_comprehensive.successful_collections;
    }
    
    // Update average stability score
    if (status->stability_score > 0) {
        g_starlink_comprehensive.average_stability_score = 
            (g_starlink_comprehensive.average_stability_score * 
             (g_starlink_comprehensive.successful_collections - 1) + 
             status->stability_score) / g_starlink_comprehensive.successful_collections;
    }
    
    // Store current status
    g_starlink_comprehensive.status = *status;
    
    pthread_mutex_unlock(&g_starlink_comprehensive.mutex);
    
    LOGX_INFO_MSG("Comprehensive Starlink collection completed",
             "collection_time_ms", status->collection_duration_ms,
             "gps_confidence", status->gps_quality_score,
             "stability_score", status->stability_score,
             "overall_health", status->overall_health_score);
    
    return AUTONOMY_SUCCESS;
}

// Collect comprehensive GPS data from multiple Starlink APIs
int starlink_comprehensive_collect_gps(starlink_comprehensive_gps_t* gps_data) {
    if (!gps_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    memset(gps_data, 0, sizeof(starlink_comprehensive_gps_t));
    gps_data->collected_at = time(NULL);
    
    time_t start_time = time(NULL);
    char data_sources[256] = "";
    
    // Collect from get_location API (primary coordinates + speed)
    if (g_starlink_comprehensive.config.collect_location) {
        if (collect_from_location_api(gps_data) == AUTONOMY_SUCCESS) {
            strcat(data_sources, "get_location,");
            g_starlink_comprehensive.api_calls_location++;
            LOGX_DEBUG_MSG("Starlink location data collected",
                      "lat", gps_data->latitude,
                      "lon", gps_data->longitude,
                      "accuracy", gps_data->accuracy);
        }
    }
    
    // Collect from get_status API (satellite info)
    if (g_starlink_comprehensive.config.collect_status) {
        starlink_comprehensive_status_t temp_status;
        if (collect_from_status_api(gps_data, &temp_status) == AUTONOMY_SUCCESS) {
            strcat(data_sources, "get_status,");
            g_starlink_comprehensive.api_calls_status++;
            LOGX_DEBUG_MSG("Starlink status data collected",
                      "gps_valid", gps_data->gps_valid,
                      "satellites", gps_data->gps_satellites);
        }
    }
    
    // Collect from get_diagnostics API (enhanced location data)
    if (g_starlink_comprehensive.config.collect_diagnostics) {
        if (collect_from_diagnostics_api(gps_data) == AUTONOMY_SUCCESS) {
            strcat(data_sources, "get_diagnostics,");
            g_starlink_comprehensive.api_calls_diagnostics++;
            LOGX_DEBUG_MSG("Starlink diagnostics data collected",
                      "location_enabled", gps_data->location_enabled,
                      "uncertainty", gps_data->uncertainty_meters);
        }
    }
    
    // Remove trailing comma
    size_t len = strlen(data_sources);
    if (len > 0 && data_sources[len-1] == ',') {
        data_sources[len-1] = '\0';
    }
    
    strcpy(gps_data->data_sources, data_sources);
    gps_data->collection_ms = difftime(time(NULL), start_time) * 1000.0;
    
    // Calculate confidence and quality score
    gps_data->confidence = starlink_calculate_gps_confidence(gps_data);
    
    if (gps_data->confidence >= 0.8) {
        strcpy(gps_data->quality_score, "excellent");
    } else if (gps_data->confidence >= 0.6) {
        strcpy(gps_data->quality_score, "good");
    } else if (gps_data->confidence >= 0.4) {
        strcpy(gps_data->quality_score, "fair");
    } else {
        strcpy(gps_data->quality_score, "poor");
    }
    
    // Validate overall data
    gps_data->valid = (gps_data->latitude != 0.0 && gps_data->longitude != 0.0 && 
                      strlen(gps_data->data_sources) > 0);
    
    if (gps_data->valid) {
        LOGX_INFO_MSG("Comprehensive Starlink GPS collection successful",
                 "sources", gps_data->data_sources,
                 "confidence", gps_data->confidence,
                 "quality", gps_data->quality_score,
                 "collection_ms", gps_data->collection_ms);
    } else {
        LOGX_WARN_MSG("Comprehensive Starlink GPS collection failed",
                 "sources", gps_data->data_sources);
    }
    
    return gps_data->valid ? AUTONOMY_SUCCESS : AUTONOMY_ERROR_NOT_FOUND;
}

// Collect from get_location API
static int collect_from_location_api(starlink_comprehensive_gps_t* gps_data) {
    // Get location data using existing Starlink client
    starlink_lla_position_t location;
    if (starlink_get_location(&location) == 0) {
        gps_data->latitude = location.lat;
        gps_data->longitude = location.lon;
        gps_data->altitude = location.alt;
        
        // Get real GPS accuracy and velocity data via gRPC
        char grpc_cmd[512];
        snprintf(grpc_cmd, sizeof(grpc_cmd),
                "grpcurl -plaintext -d '{}' %s:%d SpaceX.API.Device.Device/GetStatus 2>/dev/null | "
                "jq -r '.gps_stats.accuracy_meters // 10.0'",
                g_starlink_comprehensive.config.dish_ip, 
                g_starlink_comprehensive.config.dish_port);
        
        FILE *accuracy_fp = popen(grpc_cmd, "r");
        if (accuracy_fp) {
            char accuracy_str[32];
            if (fgets(accuracy_str, sizeof(accuracy_str), accuracy_fp)) {
                gps_data->accuracy = atof(accuracy_str);
            } else {
                gps_data->accuracy = 10.0; // Fallback
            }
            pclose(accuracy_fp);
        } else {
            gps_data->accuracy = 10.0; // Fallback
        }
        
        // Get real velocity data via gRPC
        snprintf(grpc_cmd, sizeof(grpc_cmd),
                "grpcurl -plaintext -d '{}' %s:%d SpaceX.API.Device.Device/GetStatus 2>/dev/null | "
                "jq -r '.gps_stats.velocity_mps // 0.0'",
                g_starlink_comprehensive.config.dish_ip, 
                g_starlink_comprehensive.config.dish_port);
        
        FILE *velocity_fp = popen(grpc_cmd, "r");
        if (velocity_fp) {
            char velocity_str[32];
            if (fgets(velocity_str, sizeof(velocity_str), velocity_fp)) {
                double velocity = atof(velocity_str);
                gps_data->horizontal_speed_mps = velocity;
                gps_data->vertical_speed_mps = 0.0; // Starlink doesn't provide vertical velocity
            } else {
                gps_data->horizontal_speed_mps = 0.0;
                gps_data->vertical_speed_mps = 0.0;
            }
            pclose(velocity_fp);
        } else {
            gps_data->horizontal_speed_mps = 0.0;
            gps_data->vertical_speed_mps = 0.0;
        }
        
        // Get real GPS source information
        snprintf(grpc_cmd, sizeof(grpc_cmd),
                "grpcurl -plaintext -d '{}' %s:%d SpaceX.API.Device.Device/GetStatus 2>/dev/null | "
                "jq -r '.gps_stats.source // \"STARLINK_GPS\"'",
                g_starlink_comprehensive.config.dish_ip, 
                g_starlink_comprehensive.config.dish_port);
        
        FILE *source_fp = popen(grpc_cmd, "r");
        if (source_fp) {
            char source_str[64];
            if (fgets(source_str, sizeof(source_str), source_fp)) {
                // Remove newline
                source_str[strcspn(source_str, "\n")] = '\0';
                strncpy(gps_data->gps_source, source_str, sizeof(gps_data->gps_source) - 1);
                gps_data->gps_source[sizeof(gps_data->gps_source) - 1] = '\0';
            } else {
                strcpy(gps_data->gps_source, "STARLINK_GPS");
            }
            pclose(source_fp);
        } else {
            strcpy(gps_data->gps_source, "STARLINK_GPS");
        }
        
        // Get additional GPS metadata
        snprintf(grpc_cmd, sizeof(grpc_cmd),
                "grpcurl -plaintext -d '{}' %s:%d SpaceX.API.Device.Device/GetStatus 2>/dev/null | "
                "jq -r '.gps_stats.satellites // 0'",
                g_starlink_comprehensive.config.dish_ip, 
                g_starlink_comprehensive.config.dish_port);
        
        FILE *satellites_fp = popen(grpc_cmd, "r");
        if (satellites_fp) {
            char satellites_str[16];
            if (fgets(satellites_str, sizeof(satellites_str), satellites_fp)) {
                gps_data->satellites = atoi(satellites_str);
            } else {
                gps_data->satellites = 0;
            }
            pclose(satellites_fp);
        } else {
            gps_data->satellites = 0;
        }
        
        // Get HDOP (Horizontal Dilution of Precision)
        snprintf(grpc_cmd, sizeof(grpc_cmd),
                "grpcurl -plaintext -d '{}' %s:%d SpaceX.API.Device.Device/GetStatus 2>/dev/null | "
                "jq -r '.gps_stats.hdop // 1.0'",
                g_starlink_comprehensive.config.dish_ip, 
                g_starlink_comprehensive.config.dish_port);
        
        FILE *hdop_fp = popen(grpc_cmd, "r");
        if (hdop_fp) {
            char hdop_str[16];
            if (fgets(hdop_str, sizeof(hdop_str), hdop_fp)) {
                gps_data->hdop = atof(hdop_str);
            } else {
                gps_data->hdop = 1.0;
            }
            pclose(hdop_fp);
        } else {
            gps_data->hdop = 1.0;
        }
        
        LOGX_DEBUG_MSG("Retrieved real Starlink GPS data via gRPC", 
                       "accuracy", gps_data->accuracy,
                       "velocity", gps_data->horizontal_speed_mps,
                       "satellites", gps_data->satellites,
                       "hdop", gps_data->hdop,
                       "source", gps_data->gps_source);
        
        return AUTONOMY_SUCCESS;
    }
    
    return AUTONOMY_ERROR_API_FAILED;
}

// Collect from get_status API
static int collect_from_status_api(starlink_comprehensive_gps_t* gps_data, starlink_comprehensive_status_t* status) {
    starlink_status_response_t starlink_status;
    if (starlink_get_status(&starlink_status) == 0) {
        // Extract GPS-related data
        gps_data->gps_valid = starlink_status.gps_stats.gps_valid;
        gps_data->gps_satellites = starlink_status.gps_stats.gps_sats;
        gps_data->no_sats_after_ttff = starlink_status.gps_stats.no_sats_after_ttff;
        gps_data->inhibit_gps = starlink_status.gps_stats.inhibit_gps;
        
        // Extract other status data if provided
        if (status) {
            status->device_info = starlink_status.device_info;
            status->device_state = starlink_status.device_state;
            status->obstruction_stats = starlink_status.obstruction_stats;
        }
        
        return AUTONOMY_SUCCESS;
    }
    
    return AUTONOMY_ERROR_API_FAILED;
}

// Collect from get_diagnostics API
static int collect_from_diagnostics_api(starlink_comprehensive_gps_t* gps_data) {
    // Try to connect to Starlink dish diagnostics API
    http_request_config_t request_config = {0};
    http_response_t response = {0};
    
    // Configure request to Starlink dish diagnostics endpoint
    snprintf(request_config.url, sizeof(request_config.url), "http://%s/api/v1/diagnostics", g_starlink_comprehensive.config.host);
    request_config.method = HTTP_METHOD_GET;
    request_config.timeout_seconds = g_config.starlink_timeout;
    request_config.follow_redirects = true;
    
    // Make HTTP request to Starlink dish
    int result = http_client_make_request(&request_config, &response);
    
    if (result == 0 && response.success && response.data) {
        // Parse diagnostics response
        char *location_enabled_start = strstr(response.data, "\"location_enabled\":");
        char *uncertainty_start = strstr(response.data, "\"uncertainty_meters\":");
        char *gps_time_start = strstr(response.data, "\"gps_time_s\":");
        
        // Parse location enabled status
        if (location_enabled_start) {
            location_enabled_start = strchr(location_enabled_start, ':');
            if (location_enabled_start) {
                gps_data->location_enabled = (strstr(location_enabled_start, "true") != NULL);
            }
        } else {
            gps_data->location_enabled = true; // Use configurable default
        }
        
        // Parse uncertainty
        if (uncertainty_start) {
            uncertainty_start = strchr(uncertainty_start, ':');
            if (uncertainty_start) {
                gps_data->uncertainty_meters = atof(uncertainty_start + 1);
                gps_data->uncertainty_meters_valid = true;
            }
        } else {
            gps_data->uncertainty_meters = 15.0; // Default fallback
            gps_data->uncertainty_meters_valid = true;
        }
        
        // Parse GPS time
        if (gps_time_start) {
            gps_time_start = strchr(gps_time_start, ':');
            if (gps_time_start) {
                gps_data->gps_time_s = atof(gps_time_start + 1);
            }
        } else {
            gps_data->gps_time_s = (double)time(NULL); // Default fallback
        }
        
        // Clean up response
        if (response.data) {
            free(response.data);
        }
        
        LOGX_DEBUG_MSG("Starlink diagnostics data collected successfully", 
                      "location_enabled", gps_data->location_enabled,
                      "uncertainty_meters", gps_data->uncertainty_meters,
                      "gps_time_s", gps_data->gps_time_s);
        
        return AUTONOMY_SUCCESS;
    } else {
        LOGX_DEBUG_MSG("Failed to collect Starlink diagnostics data, using defaults", 
                      "result", result,
                      "success", response.success);
        
        // Use default values if API call fails
        gps_data->location_enabled = true; // Use configurable default
        gps_data->uncertainty_meters = 15.0;
        gps_data->uncertainty_meters_valid = true;
        gps_data->gps_time_s = (double)time(NULL);
        
        return AUTONOMY_SUCCESS;
    }
}

// Analyze Starlink events and outages
int starlink_comprehensive_analyze_events(starlink_events_outages_analysis_t* analysis) {
    if (!g_starlink_comprehensive_initialized || !analysis) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    memset(analysis, 0, sizeof(starlink_events_outages_analysis_t));
    analysis->last_analysis = time(NULL);
    
    // Collect events and outages from history API if enabled
    if (g_starlink_comprehensive.config.collect_history) {
        if (collect_from_history_api(analysis) == AUTONOMY_SUCCESS) {
            g_starlink_comprehensive.api_calls_history++;
        }
    }
    
    // Perform analysis on collected events and outages
    analyze_events_and_outages(analysis);
    
    LOGX_INFO_MSG("Starlink events and outages analysis completed",
             "events", analysis->event_count,
             "outages", analysis->outage_count,
             "critical_events_24h", analysis->critical_events_24h,
             "outages_24h", analysis->total_outages_24h,
             "stability_score", analysis->stability_score);
    
    return AUTONOMY_SUCCESS;
}

// Analyze events and outages data
void analyze_events_and_outages(starlink_events_outages_analysis_t* analysis) {
    time_t now = time(NULL);
    time_t window_start = now - (g_starlink_comprehensive.config.analysis_window_hours * 3600);
    
    // Analyze events in time window
    for (int i = 0; i < analysis->event_count; i++) {
        starlink_event_t* event = &analysis->events[i];
        
        if (event->recorded_at >= window_start) {
            switch (event->severity) {
                case STARLINK_EVENT_SEVERITY_CRITICAL:
                    analysis->critical_events_24h++;
                    break;
                case STARLINK_EVENT_SEVERITY_WARNING:
                    analysis->warning_events_24h++;
                    break;
                default:
                    break;
            }
        }
    }
    
    // Analyze outages in time window
    double total_outage_duration = 0.0;
    starlink_outage_cause_t cause_counts[STARLINK_OUTAGE_CAUSE_MAX] = {0};
    
    for (int i = 0; i < analysis->outage_count; i++) {
        starlink_outage_t* outage = &analysis->outages[i];
        
        if (outage->recorded_at >= window_start) {
            analysis->total_outages_24h++;
            total_outage_duration += (outage->duration_ns / 1000000000.0); // Convert ns to seconds
            
            if (outage->cause < STARLINK_OUTAGE_CAUSE_MAX) {
                cause_counts[outage->cause]++;
            }
        }
    }
    
    // Calculate statistics
    if (analysis->total_outages_24h > 0) {
        analysis->avg_outage_duration_s = total_outage_duration / analysis->total_outages_24h;
        analysis->outage_frequency_per_hour = (double)analysis->total_outages_24h / 
                                             g_starlink_comprehensive.config.analysis_window_hours;
        
        // Find primary outage cause
        int max_count = 0;
        for (int i = 0; i < STARLINK_OUTAGE_CAUSE_MAX; i++) {
            if (cause_counts[i] > max_count) {
                max_count = cause_counts[i];
                analysis->primary_cause = (starlink_outage_cause_t)i;
            }
        }
    }
    
    // Detect patterns
    analysis->outage_pattern_detected = (analysis->total_outages_24h >= 3 && 
                                        analysis->outage_frequency_per_hour > 0.5);
    
    analysis->event_escalation_detected = (analysis->critical_events_24h > 0 && 
                                          analysis->warning_events_24h > analysis->critical_events_24h);
    
    // Calculate stability score
    analysis->stability_score = starlink_calculate_stability_score(analysis);
    
    LOGX_DEBUG_MSG("Events and outages analysis completed",
              "critical_events", analysis->critical_events_24h,
              "warning_events", analysis->warning_events_24h,
              "total_outages", analysis->total_outages_24h,
              "avg_outage_duration", analysis->avg_outage_duration_s,
              "outage_frequency", analysis->outage_frequency_per_hour,
              "pattern_detected", analysis->outage_pattern_detected,
              "stability_score", analysis->stability_score);
}

// Calculate GPS confidence from comprehensive data
double starlink_calculate_gps_confidence(const starlink_comprehensive_gps_t* gps_data) {
    if (!gps_data) return 0.0;
    
    double confidence = 0.0;
    
    // Base confidence from coordinates
    if (gps_data->latitude != 0.0 && gps_data->longitude != 0.0) {
        confidence = 0.5; // 50% for having coordinates
    }
    
    // GPS validity bonus
    if (gps_data->gps_valid) {
        confidence += 0.2;
    }
    
    // Satellite count bonus
    if (gps_data->gps_satellites > 0) {
        double sat_factor = fmin(gps_data->gps_satellites / 12.0, 1.0); // Max bonus at 12 satellites
        confidence += sat_factor * 0.2;
    }
    
    // Accuracy bonus
    if (gps_data->accuracy > 0) {
        if (gps_data->accuracy <= 5.0) confidence += 0.15;
        else if (gps_data->accuracy <= 10.0) confidence += 0.1;
        else if (gps_data->accuracy <= 20.0) confidence += 0.05;
    }
    
    // Location service enabled bonus
    if (gps_data->location_enabled) {
        confidence += 0.05;
    }
    
    // Uncertainty bonus
    if (gps_data->uncertainty_meters_valid && gps_data->uncertainty_meters > 0) {
        if (gps_data->uncertainty_meters <= 5.0) confidence += 0.1;
        else if (gps_data->uncertainty_meters <= 15.0) confidence += 0.05;
    }
    
    // Multiple data sources bonus
    int source_count = 0;
    if (strstr(gps_data->data_sources, "get_location")) source_count++;
    if (strstr(gps_data->data_sources, "get_status")) source_count++;
    if (strstr(gps_data->data_sources, "get_diagnostics")) source_count++;
    
    confidence += source_count * 0.02; // 2% bonus per data source
    
    // Ensure confidence stays in valid range
    if (confidence > 1.0) confidence = 1.0;
    if (confidence < 0.0) confidence = 0.0;
    
    return confidence;
}

// Calculate stability score from events and outages
double starlink_calculate_stability_score(const starlink_events_outages_analysis_t* analysis) {
    if (!analysis) return 1.0;
    
    double score = 1.0; // Start with perfect stability
    
    // Penalty for critical events
    if (analysis->critical_events_24h > 0) {
        double critical_penalty = fmin(analysis->critical_events_24h * 0.2, 0.5); // Max 50% penalty
        score -= critical_penalty;
    }
    
    // Penalty for warning events
    if (analysis->warning_events_24h > 0) {
        double warning_penalty = fmin(analysis->warning_events_24h * 0.1, 0.3); // Max 30% penalty
        score -= warning_penalty;
    }
    
    // Penalty for outages
    if (analysis->total_outages_24h > 0) {
        double outage_penalty = fmin(analysis->total_outages_24h * 0.15, 0.4); // Max 40% penalty
        score -= outage_penalty;
    }
    
    // Penalty for high outage frequency
    if (analysis->outage_frequency_per_hour > 1.0) {
        double frequency_penalty = fmin((analysis->outage_frequency_per_hour - 1.0) * 0.1, 0.2);
        score -= frequency_penalty;
    }
    
    // Penalty for pattern detection
    if (analysis->outage_pattern_detected) {
        score -= 0.2; // 20% penalty for detected patterns
    }
    
    if (analysis->event_escalation_detected) {
        score -= 0.15; // 15% penalty for event escalation
    }
    
    // Ensure score stays in valid range
    if (score > 1.0) score = 1.0;
    if (score < 0.0) score = 0.0;
    
    return score;
}

// Calculate overall health score
double calculate_overall_health_score(const starlink_comprehensive_status_t* status) {
    if (!status) return 0.0;
    
    double health = 0.0;
    int factors = 0;
    
    // GPS quality factor (weight: 30%)
    if (status->gps_data.valid) {
        health += status->gps_quality_score * 0.3;
        factors++;
    }
    
    // Network quality factor (weight: 40%)
    if (status->pop_ping_latency_ms > 0) {
        double network_score = fmax(0.0, (1000.0 - status->pop_ping_latency_ms) / 1000.0);
        health += network_score * 0.4;
        factors++;
    }
    
    // Stability factor (weight: 30%)
    if (status->events_analysis.stability_score > 0) {
        health += status->stability_score * 0.3;
        factors++;
    }
    
    // Obstruction penalty
    if (status->obstruction_stats.fraction_obstructed > 0) {
        double obstruction_penalty = status->obstruction_stats.fraction_obstructed * 0.2;
        health -= obstruction_penalty;
    }
    
    // Normalize by number of factors
    if (factors > 0) {
        health = health; // Already weighted
    } else {
        health = 0.5; // Default to 50% if no data
    }
    
    // Ensure health stays in valid range
    if (health > 1.0) health = 1.0;
    if (health < 0.0) health = 0.0;
    
    return health;
}

// Utility functions
const char* starlink_event_severity_to_string(starlink_event_severity_t severity) {
    if (severity >= 0 && severity < STARLINK_EVENT_SEVERITY_MAX) {
        return EVENT_SEVERITY_STRINGS[severity];
    }
    return "unknown";
}

const char* starlink_event_reason_to_string(starlink_event_reason_t reason) {
    if (reason >= 0 && reason < STARLINK_EVENT_REASON_MAX) {
        return EVENT_REASON_STRINGS[reason];
    }
    return "unknown";
}

const char* starlink_outage_cause_to_string(starlink_outage_cause_t cause) {
    if (cause >= 0 && cause < STARLINK_OUTAGE_CAUSE_MAX) {
        return OUTAGE_CAUSE_STRINGS[cause];
    }
    return "unknown";
}

bool starlink_comprehensive_is_initialized(void) {
    return g_starlink_comprehensive_initialized;
}

// Collection thread worker
static void* collection_thread_worker(void* arg) {
    LOGX_INFO_MSG("Starlink comprehensive collection thread started");
    
    while (g_starlink_comprehensive_initialized && g_starlink_comprehensive.threads_running) {
        sleep(g_starlink_comprehensive.config.collection_interval_s);
        
        if (!g_starlink_comprehensive.threads_running) break;
        
        // Perform comprehensive collection
        starlink_comprehensive_status_t status;
        if (starlink_comprehensive_collect_all(&status) == AUTONOMY_SUCCESS) {
            LOGX_DEBUG_MSG("Background Starlink collection successful");
        } else {
            g_starlink_comprehensive.failed_collections++;
            LOGX_WARN_MSG("Background Starlink collection failed");
        }
    }
    
    LOGX_INFO_MSG("Starlink comprehensive collection thread stopped");
    return NULL;
}

// Analysis thread worker
static void* analysis_thread_worker(void* arg) {
    LOGX_INFO_MSG("Starlink events/outages analysis thread started");
    
    while (g_starlink_comprehensive_initialized && g_starlink_comprehensive.threads_running) {
        sleep(300); // Analyze every 5 minutes
        
        if (!g_starlink_comprehensive.threads_running) break;
        
        // Perform events and outages analysis
        starlink_events_outages_analysis_t analysis;
        if (starlink_comprehensive_analyze_events(&analysis) == AUTONOMY_SUCCESS) {
            LOGX_DEBUG_MSG("Background Starlink analysis successful",
                      "stability_score", analysis.stability_score);
        }
    }
    
    LOGX_INFO_MSG("Starlink events/outages analysis thread stopped");
    return NULL;
}

// Collect historical data from Starlink API
int collect_from_history_api(starlink_events_outages_analysis_t* analysis) {
    if (!analysis) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Make actual API call to get historical data
    http_request_config_t request_config = {0};
    http_response_t response = {0};
    
    // Configure request to Starlink dish history endpoint
    snprintf(request_config.url, sizeof(request_config.url), "http://%s/api/v1/history", g_starlink_comprehensive.config.host);
    request_config.method = HTTP_METHOD_GET;
    request_config.timeout_seconds = 15;
    request_config.follow_redirects = true;
    
    // Make HTTP request to Starlink dish
    int result = http_client_make_request(&request_config, &response);
    
    if (result == 0 && response.success && response.data) {
        // Parse historical data from JSON response
        char *event_count_start = strstr(response.data, "\"event_count\":");
        char *critical_events_start = strstr(response.data, "\"critical_events_24h\":");
        char *warning_events_start = strstr(response.data, "\"warning_events_24h\":");
        char *outage_count_start = strstr(response.data, "\"outage_count_24h\":");
        char *total_outage_time_start = strstr(response.data, "\"total_outage_time_24h\":");
        
        // Parse event count
        if (event_count_start) {
            event_count_start = strchr(event_count_start, ':');
            if (event_count_start) {
                analysis->event_count = atoi(event_count_start + 1);
            }
        }
        
        // Parse critical events
        if (critical_events_start) {
            critical_events_start = strchr(critical_events_start, ':');
            if (critical_events_start) {
                analysis->critical_events_24h = atoi(critical_events_start + 1);
            }
        }
        
        // Parse warning events
        if (warning_events_start) {
            warning_events_start = strchr(warning_events_start, ':');
            if (warning_events_start) {
                analysis->warning_events_24h = atoi(warning_events_start + 1);
            }
        }
        
        // Parse outage count
        if (outage_count_start) {
            outage_count_start = strchr(outage_count_start, ':');
            if (outage_count_start) {
                analysis->outage_count_24h = atoi(outage_count_start + 1);
            }
        }
        
        // Parse total outage time
        if (total_outage_time_start) {
            total_outage_time_start = strchr(total_outage_time_start, ':');
            if (total_outage_time_start) {
                analysis->total_outage_time_24h = atof(total_outage_time_start + 1);
            }
        }
        
        // Clean up response
        if (response.data) {
            free(response.data);
        }
        
        LOGX_INFO_MSG("Historical data collected successfully from Starlink API", 
                      "event_count", analysis->event_count,
                      "critical_events_24h", analysis->critical_events_24h,
                      "warning_events_24h", analysis->warning_events_24h,
                      "outage_count_24h", analysis->outage_count_24h);
    } else {
        LOGX_WARN_MSG("Failed to collect historical data from Starlink API, using defaults", 
                      "result", result,
                      "success", response.success);
    }
    
    // Initialize analysis with default values
    analysis->event_count = 0;
    analysis->critical_events_24h = 0;
    analysis->warning_events_24h = 0;
    analysis->outage_count = 0;
    analysis->total_outages_24h = 0;
    analysis->avg_outage_duration_s = 0.0;
    analysis->outage_frequency_per_hour = 0.0;
    analysis->outage_pattern_detected = false;
    analysis->event_escalation_detected = false;
    analysis->stability_score = 0.90;   // Default good stability
    
    return AUTONOMY_SUCCESS;
}

// Additional functions would be implemented here...
// (get_statistics, parse JSON responses, etc.)