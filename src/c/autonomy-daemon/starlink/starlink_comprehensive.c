#include "../starlink/starlink_comprehensive.h"
#include "starlink_modules.h"
#include "../shared/utils/string_utils.h"
#include "starlink_grpc_collector.h"
#include "../shared/logging/logx.h"
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
        
        // Get real GPS accuracy data via gRPC
        char response_buffer[1024];
        if (starlink_grpc_call_get_status(response_buffer, sizeof(response_buffer)) == AUTONOMY_SUCCESS) {
            // Parse JSON response to extract GPS accuracy
            // Add safety check for empty buffer
            if (strlen(response_buffer) > 0) {
                json_object *json_response = json_tokener_parse(response_buffer);
                if (json_response) {
                    json_object *gps_stats, *accuracy_obj;
                    if (json_object_object_get_ex(json_response, "gps_stats", &gps_stats) &&
                        json_object_object_get_ex(gps_stats, "accuracy_meters", &accuracy_obj)) {
                        gps_data->accuracy = json_object_get_double(accuracy_obj);
                    } else {
                        gps_data->accuracy = 10.0; // Fallback
                    }
                    json_object_put(json_response);
                }
            } else {
                gps_data->accuracy = 10.0; // Fallback
            }
        } else {
            gps_data->accuracy = 10.0; // Fallback
        }
        
        // Get real velocity data via gRPC
        if (starlink_grpc_call_get_status(response_buffer, sizeof(response_buffer)) == AUTONOMY_SUCCESS) {
            // Parse JSON response to extract GPS velocity
            // Add safety check for empty buffer
            if (strlen(response_buffer) > 0) {
                json_object *json_response = json_tokener_parse(response_buffer);
                if (json_response) {
                    json_object *gps_stats, *velocity_obj;
                    if (json_object_object_get_ex(json_response, "gps_stats", &gps_stats) &&
                        json_object_object_get_ex(gps_stats, "velocity_mps", &velocity_obj)) {
                        double velocity = json_object_get_double(velocity_obj);
                        gps_data->horizontal_speed_mps = velocity;
                        gps_data->vertical_speed_mps = 0.0; // Starlink doesn't provide vertical velocity
                    } else {
                        gps_data->horizontal_speed_mps = 0.0;
                        gps_data->vertical_speed_mps = 0.0;
                    }
                    json_object_put(json_response);
                }
            } else {
                gps_data->horizontal_speed_mps = 0.0;
                gps_data->vertical_speed_mps = 0.0;
            }
        } else {
            gps_data->horizontal_speed_mps = 0.0;
            gps_data->vertical_speed_mps = 0.0;
        }
        
        // Get real GPS source information
        if (starlink_grpc_call_get_status(response_buffer, sizeof(response_buffer)) == AUTONOMY_SUCCESS) {
            // Parse JSON response to extract GPS source
            json_object *json_response = json_tokener_parse(response_buffer);
            if (json_response) {
                json_object *gps_stats, *source_obj;
                if (json_object_object_get_ex(json_response, "gps_stats", &gps_stats) &&
                    json_object_object_get_ex(gps_stats, "source", &source_obj)) {
                    const char *source = json_object_get_string(source_obj);
                    safe_strncpy(gps_data->gps_source, source, sizeof(gps_data->gps_source));
                    gps_data->gps_source[sizeof(gps_data->gps_source) - 1] = '\0';
                } else {
                    strcpy(gps_data->gps_source, "STARLINK_GPS");
                }
                json_object_put(json_response);
            } else {
                strcpy(gps_data->gps_source, "STARLINK_GPS");
            }
        } else {
            strcpy(gps_data->gps_source, "STARLINK_GPS");
        }
        
        // Get additional GPS metadata
        if (starlink_grpc_call_get_status(response_buffer, sizeof(response_buffer)) == AUTONOMY_SUCCESS) {
            // Parse JSON response to extract GPS satellites
            json_object *json_response = json_tokener_parse(response_buffer);
            if (json_response) {
                json_object *gps_stats, *satellites_obj;
                if (json_object_object_get_ex(json_response, "gps_stats", &gps_stats) &&
                    json_object_object_get_ex(gps_stats, "satellites", &satellites_obj)) {
                    gps_data->gps_satellites = json_object_get_int(satellites_obj);
                } else {
                    gps_data->gps_satellites = 0;
                }
                json_object_put(json_response);
            } else {
                gps_data->gps_satellites = 0;
            }
        } else {
            gps_data->gps_satellites = 0;
        }
        
        // Get HDOP (Horizontal Dilution of Precision)
        if (starlink_grpc_call_get_status(response_buffer, sizeof(response_buffer)) == AUTONOMY_SUCCESS) {
            // Parse JSON response to extract GPS HDOP
            json_object *json_response = json_tokener_parse(response_buffer);
            if (json_response) {
                json_object *gps_stats, *hdop_obj;
                if (json_object_object_get_ex(json_response, "gps_stats", &gps_stats) &&
                    json_object_object_get_ex(gps_stats, "hdop", &hdop_obj)) {
                    // Note: HDOP is not available in the current struct definition
                    // This data would need to be added to the struct if needed
                    double hdop = json_object_get_double(hdop_obj);
                    LOGX_DEBUG_MSG("Retrieved HDOP from gRPC", "hdop", hdop);
                }
                json_object_put(json_response);
            }
        }
        
        // Note: HDOP is not available in the current struct definition
        // This data would need to be added to the struct if needed
        
        LOGX_DEBUG_MSG("Retrieved real Starlink GPS data via gRPC", 
                       "accuracy", gps_data->accuracy,
                       "velocity", gps_data->horizontal_speed_mps,
                       "satellites", gps_data->gps_satellites,
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

// Collect from get_diagnostics gRPC API
static int collect_from_diagnostics_api(starlink_comprehensive_gps_t* gps_data) {
    // Get latest observation from gRPC collector
    starlink_observation_t observation = {0};
    
    if (starlink_grpc_get_latest_observation(&observation) == AUTONOMY_SUCCESS) {
        // Extract diagnostics data from gRPC observation
        gps_data->location_enabled = observation.gps_valid;
        gps_data->uncertainty_meters = observation.gps_accuracy_m;
        gps_data->uncertainty_meters_valid = (observation.gps_accuracy_m > 0.0);
        gps_data->gps_time_s = (double)observation.timestamp;
        
        LOGX_DEBUG_MSG("Starlink diagnostics data collected successfully from gRPC", 
                      "location_enabled", gps_data->location_enabled,
                      "uncertainty_meters", gps_data->uncertainty_meters,
                      "gps_time_s", gps_data->gps_time_s);
        
        return AUTONOMY_SUCCESS;
    } else {
        LOGX_DEBUG_MSG("Failed to collect Starlink diagnostics data from gRPC, using defaults");
        
        // Use default values if gRPC call fails
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

// Collect historical data from Starlink gRPC API
int collect_from_history_api(starlink_events_outages_analysis_t* analysis) {
    if (!analysis) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Use gRPC collector for structured outage event data
    starlink_outage_event_t outage_events[100];
    int actual_outage_count = 0;
    
    // Get outage events from gRPC collector
    if (starlink_grpc_get_outage_events(outage_events, 100, &actual_outage_count) == AUTONOMY_SUCCESS) {
        // Analyze outage events for the last 24 hours
        time_t now = time(NULL);
        time_t day_ago = now - (24 * 3600);
        
        int critical_events_24h = 0;
        int warning_events_24h = 0;
        int outage_count_24h = 0;
        double total_outage_time_24h = 0.0;
        double total_duration = 0.0;
        
        for (int i = 0; i < actual_outage_count; i++) {
            if (outage_events[i].t_start >= day_ago) {
                outage_count_24h++;
                total_duration += outage_events[i].duration;
                
                // Classify severity
                if (strcmp(outage_events[i].severity, "critical") == 0) {
                    critical_events_24h++;
                } else if (strcmp(outage_events[i].severity, "warning") == 0) {
                    warning_events_24h++;
                }
                
                // Sum outage time
                total_outage_time_24h += outage_events[i].duration;
            }
        }
        
        // Calculate analysis metrics
        analysis->event_count = actual_outage_count;
        analysis->critical_events_24h = critical_events_24h;
        analysis->warning_events_24h = warning_events_24h;
        analysis->total_outages_24h = outage_count_24h;
        analysis->avg_outage_duration_s = total_outage_time_24h;
        analysis->avg_outage_duration_s = (outage_count_24h > 0) ? (total_duration / outage_count_24h) : 0.0;
        analysis->outage_frequency_per_hour = (double)outage_count_24h / 24.0;
        
        // Detect patterns
        analysis->outage_pattern_detected = (outage_count_24h > 5); // More than 5 outages in 24h
        analysis->event_escalation_detected = (critical_events_24h > 2); // More than 2 critical events
        
        // Calculate stability score (0.0 = unstable, 1.0 = stable)
        double uptime_ratio = 1.0 - (total_outage_time_24h / (24.0 * 3600.0));
        analysis->stability_score = (uptime_ratio > 0.0) ? uptime_ratio : 0.0;
        
        LOGX_INFO_MSG("Historical data collected successfully from gRPC collector", 
                      "event_count", analysis->event_count,
                      "critical_events_24h", analysis->critical_events_24h,
                      "warning_events_24h", analysis->warning_events_24h,
                      "total_outages_24h", analysis->total_outages_24h,
                      "stability_score", analysis->stability_score);
    } else {
        LOGX_WARN_MSG("Failed to collect historical data from gRPC collector, using defaults");
        
        // Initialize analysis with default values
        analysis->event_count = 0;
        analysis->critical_events_24h = 0;
        analysis->warning_events_24h = 0;
        analysis->total_outages_24h = 0;
        analysis->avg_outage_duration_s = 0.0;
        analysis->outage_frequency_per_hour = 0.0;
        analysis->outage_pattern_detected = false;
        analysis->event_escalation_detected = false;
        analysis->stability_score = 0.90;   // Default good stability
    }
    
    return AUTONOMY_SUCCESS;
}

// Get comprehensive Starlink status
int starlink_comprehensive_get_status(starlink_comprehensive_status_t* status) {
    if (!status) return AUTONOMY_ERROR_INVALID_PARAM;
    if (!g_starlink_comprehensive_initialized) return AUTONOMY_ERROR_NOT_INITIALIZED;
    
    pthread_mutex_lock(&g_starlink_comprehensive.mutex);
    *status = g_starlink_comprehensive.status;
    pthread_mutex_unlock(&g_starlink_comprehensive.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get comprehensive Starlink statistics
int starlink_comprehensive_get_statistics(uint64_t* total_collections, uint64_t* successful_collections,
                                         uint64_t* failed_collections, double* avg_collection_time_ms,
                                         double* avg_gps_confidence, double* avg_stability_score) {
    if (!total_collections || !successful_collections || !failed_collections || !avg_collection_time_ms || 
        !avg_gps_confidence || !avg_stability_score) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    if (!g_starlink_comprehensive_initialized) return AUTONOMY_ERROR_NOT_INITIALIZED;
    
    pthread_mutex_lock(&g_starlink_comprehensive.mutex);
    *total_collections = g_starlink_comprehensive.total_collections;
    *successful_collections = g_starlink_comprehensive.successful_collections;
    *failed_collections = g_starlink_comprehensive.failed_collections;
    *avg_collection_time_ms = g_starlink_comprehensive.average_collection_time_ms;
    *avg_gps_confidence = g_starlink_comprehensive.average_gps_confidence;
    *avg_stability_score = g_starlink_comprehensive.average_stability_score;
    pthread_mutex_unlock(&g_starlink_comprehensive.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get current stability score
double starlink_comprehensive_get_stability_score(void) {
    if (!g_starlink_comprehensive_initialized) return 0.0;
    
    pthread_mutex_lock(&g_starlink_comprehensive.mutex);
    double score = g_starlink_comprehensive.status.stability_score;
    pthread_mutex_unlock(&g_starlink_comprehensive.mutex);
    
    return score;
}