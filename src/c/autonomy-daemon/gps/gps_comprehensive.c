#include "gps_comprehensive.h"
#include "gps_rutos.h"
#include "gps_starlink.h"
#include "gps_opencellid.h"
#include "opencellid_complete.h"
#include "gps_google_api.h"
#include "../external/external_apis.h"
#include "../starlink/starlink_grpc_collector.h"
#include "../utils/logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Global comprehensive GPS collector instance
static gps_comprehensive_collector_t g_gps_collector = {0};
static bool g_collector_initialized = false;

// Source type strings
static const char* SOURCE_TYPE_STRINGS[] = {
    "rutos", "starlink", "opencellid", "google", "external"
};

// Fix type strings
static const char* FIX_TYPE_STRINGS[] = {
    "none", "2d", "3d", "dgps", "rtk_float", "rtk_fixed"
};

// Fix quality strings
static const char* FIX_QUALITY_STRINGS[] = {
    "invalid", "gps", "dgps", "pps", "rtk", "rtk_float", "estimated", "manual"
};

// Forward declarations
static int collect_from_source(gps_source_type_t source_type, standardized_gps_data_t* data);
static int perform_multi_source_fusion(gps_fusion_result_t* result);
static int collect_with_hybrid_prioritization(standardized_gps_data_t* result);
static double calculate_source_confidence(const standardized_gps_data_t* data, 
                                         const gps_source_health_t* health);
static void update_source_health(gps_source_type_t source_type, bool success,
                                double collection_time_ms, const standardized_gps_data_t* data);
static void finalize_gps_data(standardized_gps_data_t* data);
static bool validate_gps_coordinates(double latitude, double longitude);
static bool validate_gps_accuracy(double accuracy);
static bool validate_gps_timestamp(time_t timestamp);
static void* collection_thread_worker(void* arg);
static void* health_monitor_thread_worker(void* arg);

// Initialize comprehensive GPS collector
int gps_comprehensive_init(const gps_comprehensive_config_t* config) {
    if (g_collector_initialized) {
        LOGX_WARN_MSG("Comprehensive GPS collector already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    if (!config) {
        LOGX_ERROR_MSG("GPS comprehensive config is NULL");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    memset(&g_gps_collector, 0, sizeof(gps_comprehensive_collector_t));
    g_gps_collector.config = *config;
    
    // Initialize mutex
    if (pthread_mutex_init(&g_gps_collector.mutex, NULL) != 0) {
        LOGX_ERROR_MSG("Failed to initialize GPS collector mutex");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Initialize source health tracking
    for (int i = 0; i < GPS_SOURCE_MAX; i++) {
        g_gps_collector.source_health[i].source_type = (gps_source_type_t)i;
        strcpy(g_gps_collector.source_health[i].source_name, gps_source_type_to_string((gps_source_type_t)i));
        g_gps_collector.source_health[i].first_seen = time(NULL);
        g_gps_collector.source_health[i].health_score = 1.0; // Start with full health
        g_gps_collector.source_health[i].best_accuracy = 999999.0; // Initialize to very high value
    }
    
    // Initialize movement state
    g_gps_collector.movement_state.is_moving = false;
    g_gps_collector.movement_state.was_moving = false;
    g_gps_collector.movement_state.stationary_start = time(NULL);
    
    // Start background threads if enabled
    if (config->enable_health_monitoring) {
        g_gps_collector.threads_running = true;
        
        if (pthread_create(&g_gps_collector.health_monitor_thread, NULL, 
                          health_monitor_thread_worker, NULL) != 0) {
            LOGX_ERROR_MSG("Failed to create GPS health monitor thread");
            pthread_mutex_destroy(&g_gps_collector.mutex);
            return AUTONOMY_ERROR_SYSTEM;
        }
    }
    
    g_collector_initialized = true;
    
    LOGX_INFO_MSG("Comprehensive GPS collector initialized",
              "movement_detection", config->enable_movement_detection ? "true" : "false",
              "hybrid_prioritization", config->enable_hybrid_prioritization ? "true" : "false",
              "data_fusion", config->enable_data_fusion ? "true" : "false",
              "health_monitoring", config->enable_health_monitoring ? "true" : "false");
    
    return AUTONOMY_SUCCESS;
}

// Cleanup comprehensive GPS collector
void gps_comprehensive_cleanup(void) {
    if (!g_collector_initialized) return;
    
    pthread_mutex_lock(&g_gps_collector.mutex);
    
    // Stop background threads
    g_gps_collector.threads_running = false;
    
    if (g_gps_collector.config.enable_health_monitoring) {
        pthread_cancel(g_gps_collector.health_monitor_thread);
        pthread_join(g_gps_collector.health_monitor_thread, NULL);
    }
    
    pthread_mutex_unlock(&g_gps_collector.mutex);
    pthread_mutex_destroy(&g_gps_collector.mutex);
    
    g_collector_initialized = false;
    
    LOGX_INFO_MSG("Comprehensive GPS collector cleaned up");
}

// Collect GPS data from best available source
int gps_comprehensive_collect_best(standardized_gps_data_t* result) {
    if (!g_collector_initialized || !result) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_gps_collector.mutex);
    
    memset(result, 0, sizeof(standardized_gps_data_t));
    
    int ret;
    if (g_gps_collector.config.enable_hybrid_prioritization) {
        ret = collect_with_hybrid_prioritization(result);
    } else {
        // Traditional priority-based collection
        ret = AUTONOMY_ERROR_NOT_FOUND;
        
        // Try sources in priority order
        gps_source_type_t priority_order[] = {GPS_SOURCE_RUTOS, GPS_SOURCE_STARLINK, 
                                             GPS_SOURCE_OPENCELLID, GPS_SOURCE_GOOGLE};
        
        for (int i = 0; i < 4; i++) {
            if (collect_from_source(priority_order[i], result) == AUTONOMY_SUCCESS) {
                ret = AUTONOMY_SUCCESS;
                break;
            }
        }
    }
    
    if (ret == AUTONOMY_SUCCESS) {
        finalize_gps_data(result);
        
        // Update movement detection
        if (g_gps_collector.config.enable_movement_detection) {
            gps_comprehensive_detect_movement(result);
        }
        
        // Update statistics
        g_gps_collector.total_collections++;
        g_gps_collector.successful_collections++;
        g_gps_collector.last_collection = time(NULL);
        
        // Store as last known position
        g_gps_collector.last_known = *result;
    } else {
        g_gps_collector.failed_collections++;
    }
    
    pthread_mutex_unlock(&g_gps_collector.mutex);
    
    return ret;
}

// Collect GPS data from all sources and perform fusion
int gps_comprehensive_collect_all_and_fuse(gps_fusion_result_t* result) {
    if (!g_collector_initialized || !result) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_gps_collector.mutex);
    
    memset(result, 0, sizeof(gps_fusion_result_t));
    result->fusion_time = time(NULL);
    
    int ret = perform_multi_source_fusion(result);
    
    if (ret == AUTONOMY_SUCCESS) {
        g_gps_collector.fusion_operations++;
        g_gps_collector.last_fusion = *result;
    }
    
    pthread_mutex_unlock(&g_gps_collector.mutex);
    
    return ret;
}

// Collect with hybrid prioritization (confidence-based fallback)
static int collect_with_hybrid_prioritization(standardized_gps_data_t* result) {
    standardized_gps_data_t all_source_data[GPS_SOURCE_MAX];
    int sources_collected = 0;
    
    LOGX_DEBUG_MSG("Starting hybrid GPS collection",
              "min_confidence", g_gps_collector.config.min_acceptable_confidence,
              "fallback_threshold", g_gps_collector.config.fallback_confidence_threshold);
    
    // Step 1: Try each source in priority order, collecting confidence data
    gps_source_type_t priority_order[] = {GPS_SOURCE_RUTOS, GPS_SOURCE_STARLINK, 
                                         GPS_SOURCE_OPENCELLID, GPS_SOURCE_GOOGLE};
    
    for (int i = 0; i < 4; i++) {
        gps_source_type_t source_type = priority_order[i];
        standardized_gps_data_t data;
        
        if (collect_from_source(source_type, &data) == AUTONOMY_SUCCESS) {
            all_source_data[sources_collected] = data;
            sources_collected++;
            
            LOGX_DEBUG_MSG("GPS source collected",
                      "source", gps_source_type_to_string(source_type),
                      "confidence", data.confidence,
                      "accuracy", data.accuracy,
                      "priority", i + 1);
            
            // Apply hybrid logic
            if (i == 0) { // RUTOS (highest priority)
                if (data.confidence >= g_gps_collector.config.fallback_confidence_threshold) {
                    LOGX_INFO_MSG("GPS source selected",
                             "source", gps_source_type_to_string(source_type),
                             "reason", "high_confidence_primary",
                             "confidence", data.confidence);
                    *result = data;
                    return AUTONOMY_SUCCESS;
                }
                LOGX_DEBUG_MSG("GPS primary source low confidence",
                          "source", gps_source_type_to_string(source_type),
                          "confidence", data.confidence,
                          "threshold", g_gps_collector.config.fallback_confidence_threshold);
            } else if (i == 1) { // Starlink (second priority)
                if (data.confidence >= g_gps_collector.config.fallback_confidence_threshold) {
                    LOGX_INFO_MSG("GPS source selected",
                             "source", gps_source_type_to_string(source_type),
                             "reason", "high_confidence_secondary",
                             "confidence", data.confidence);
                    *result = data;
                    return AUTONOMY_SUCCESS;
                }
            } else { // OpenCellID, Google, or other sources
                if (data.confidence >= g_gps_collector.config.fallback_confidence_threshold) {
                    LOGX_INFO_MSG("GPS source selected",
                             "source", gps_source_type_to_string(source_type),
                             "reason", "high_confidence_fallback",
                             "confidence", data.confidence);
                    *result = data;
                    return AUTONOMY_SUCCESS;
                }
            }
        }
    }
    
    // Step 2: No source met high confidence threshold, pick best available
    if (sources_collected == 0) {
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    // Find source with highest confidence that meets minimum requirements
    standardized_gps_data_t* best_data = NULL;
    for (int i = 0; i < sources_collected; i++) {
        if (all_source_data[i].confidence >= g_gps_collector.config.min_acceptable_confidence) {
            if (best_data == NULL || all_source_data[i].confidence > best_data->confidence) {
                best_data = &all_source_data[i];
            }
        }
    }
    
    // If no source meets minimum confidence, pick best available anyway
    if (best_data == NULL) {
        best_data = &all_source_data[0];
        for (int i = 1; i < sources_collected; i++) {
            if (all_source_data[i].confidence > best_data->confidence) {
                best_data = &all_source_data[i];
            }
        }
        
        LOGX_WARN_MSG("GPS using low confidence source",
                 "source", best_data->source,
                 "confidence", best_data->confidence,
                 "min_required", g_gps_collector.config.min_acceptable_confidence);
    }
    
    *result = *best_data;
    
    LOGX_INFO_MSG("GPS source selected",
             "source", best_data->source,
             "reason", "best_available",
             "confidence", best_data->confidence,
             "accuracy", best_data->accuracy);
    
    return AUTONOMY_SUCCESS;
}

// Collect from specific GPS source
static int collect_from_source(gps_source_type_t source_type, standardized_gps_data_t* data) {
    if (!data) return AUTONOMY_ERROR_INVALID_PARAM;
    
    memset(data, 0, sizeof(standardized_gps_data_t));
    data->source_type = source_type;
    strcpy(data->source, gps_source_type_to_string(source_type));
    data->collection_time = time(NULL);
    
    time_t start_time = time(NULL);
    int ret = AUTONOMY_ERROR_NOT_FOUND;
    
    switch (source_type) {
        case GPS_SOURCE_RUTOS: {
            // Check if RUTOS GPS is available
            gps_data_t rutos_data;
            
            // Try to get GPS data from RUTOS GPS daemon
            FILE *gps_fp = popen("gpspipe -w -n 1 2>/dev/null", "r");
            if (gps_fp) {
                char gps_line[512];
                if (fgets(gps_line, sizeof(gps_line), gps_fp)) {
                    // Parse NMEA or JSON GPS data
                    if (strstr(gps_line, "GPGGA") || strstr(gps_line, "GPRMC")) {
                        // Parse NMEA sentence
                        char *lat_start = strstr(gps_line, ",");
                        if (lat_start) {
                            lat_start = strchr(lat_start + 1, ',');
                            if (lat_start) {
                                lat_start = strchr(lat_start + 1, ',');
                                if (lat_start) {
                                    rutos_data.latitude = atof(lat_start + 1);
                                    char *lon_start = strchr(lat_start + 1, ',');
                                    if (lon_start) {
                                        lon_start = strchr(lon_start + 1, ',');
                                        if (lon_start) {
                                            rutos_data.longitude = atof(lon_start + 1);
                                            rutos_data.valid = true;
                                        }
                                    }
                                }
                            }
                        }
                    } else if (strstr(gps_line, "\"lat\":") && strstr(gps_line, "\"lon\":")) {
                        // Parse JSON GPS data
                        char *lat_start = strstr(gps_line, "\"lat\":");
                        char *lon_start = strstr(gps_line, "\"lon\":");
                        if (lat_start && lon_start) {
                            lat_start = strchr(lat_start, ':');
                            lon_start = strchr(lon_start, ':');
                            if (lat_start && lon_start) {
                                rutos_data.latitude = atof(lat_start + 1);
                                rutos_data.longitude = atof(lon_start + 1);
                                rutos_data.valid = true;
                            }
                        }
                    }
                }
                pclose(gps_fp);
            }
            
            // If GPS parsing failed, try alternative method
            if (!rutos_data.valid) {
                FILE *alt_fp = popen("ubus call gps get_status 2>/dev/null", "r");
                if (alt_fp) {
                    char ubus_line[512];
                    if (fgets(ubus_line, sizeof(ubus_line), alt_fp)) {
                        // Parse UBUS GPS response
                        char *lat_start = strstr(ubus_line, "\"latitude\":");
                        char *lon_start = strstr(ubus_line, "\"longitude\":");
                        if (lat_start && lon_start) {
                            lat_start = strchr(lat_start, ':');
                            lon_start = strchr(lon_start, ':');
                            if (lat_start && lon_start) {
                                rutos_data.latitude = atof(lat_start + 1);
                                rutos_data.longitude = atof(lon_start + 1);
                                rutos_data.valid = true;
                            }
                        }
                    }
                    pclose(alt_fp);
                }
            }
            
            // Set default values if still not valid
            if (!rutos_data.valid) {
                rutos_data.valid = false;
                rutos_data.latitude = 0.0;
                rutos_data.longitude = 0.0;
                rutos_data.accuracy = 0.0;
                rutos_data.satellites = 0;
            } else {
                rutos_data.accuracy = 10.0; // Default accuracy
                rutos_data.satellites = 8;  // Default satellite count
            }
            rutos_data.timestamp = time(NULL);
            
            if (rutos_data.valid) {
                data->latitude = rutos_data.latitude;
                data->longitude = rutos_data.longitude;
                data->altitude = rutos_data.altitude;
                data->accuracy = rutos_data.accuracy;
                data->speed = rutos_data.speed;
                data->heading = rutos_data.heading;
                data->satellites_used = rutos_data.satellites;
                data->hdop = rutos_data.hdop;
                data->vdop = rutos_data.vdop;
                data->fix_quality = (gps_fix_quality_t)rutos_data.fix_quality;
                data->timestamp = rutos_data.timestamp;
                data->valid = true;
                data->source_priority = 1; // Highest priority
                strcpy(data->raw_nmea, "RUTOS NMEA data");
                
                ret = AUTONOMY_SUCCESS;
            }
            break;
        }
        
        case GPS_SOURCE_STARLINK: {
            // Check if Starlink GPS is available
            gps_data_t starlink_data;
            
            // Try to get GPS data from Starlink gRPC API
            starlink_observation_t observation = {0};
            
            if (starlink_grpc_get_latest_observation(&observation) == AUTONOMY_SUCCESS) {
                // Extract GPS data from gRPC observation
                if (observation.gps_valid) {
                    starlink_data.latitude = 0.0; // GPS coordinates not available in status
                    starlink_data.longitude = 0.0; // GPS coordinates not available in status
                    starlink_data.altitude = 0.0;
                    starlink_data.accuracy = observation.gps_accuracy_m;
                    starlink_data.valid = observation.gps_valid;
                    starlink_data.timestamp = observation.timestamp;
                    starlink_data.satellites = observation.gps_satellites;
                    
                    LOGX_DEBUG_MSG("Starlink GPS data from gRPC", 
                                  "gps_valid", observation.gps_valid,
                                  "gps_satellites", observation.gps_satellites,
                                  "gps_accuracy", observation.gps_accuracy_m);
                } else {
                    // Starlink GPS not valid
                    starlink_data.valid = false;
                    starlink_data.latitude = 0.0;
                    starlink_data.longitude = 0.0;
                    starlink_data.altitude = 0.0;
                    starlink_data.accuracy = 0.0;
                    starlink_data.satellites = 0;
                    starlink_data.timestamp = time(NULL);
                }
            } else {
                // Failed to get gRPC observation
                starlink_data.valid = false;
                starlink_data.latitude = 0.0;
                starlink_data.longitude = 0.0;
                starlink_data.altitude = 0.0;
                starlink_data.accuracy = 0.0;
                starlink_data.satellites = 0;
                starlink_data.timestamp = time(NULL);
            }
            
            if (starlink_data.valid) {
                data->latitude = starlink_data.latitude;
                data->longitude = starlink_data.longitude;
                data->altitude = starlink_data.altitude;
                data->accuracy = starlink_data.accuracy;
                data->satellites_used = starlink_data.satellites;
                data->timestamp = starlink_data.timestamp;
                data->valid = true;
                data->source_priority = 2; // Second priority
                strcpy(data->raw_json, "Starlink GPS JSON");
                
                ret = AUTONOMY_SUCCESS;
            }
            break;
        }
        
        case GPS_SOURCE_OPENCELLID: {
            if (opencellid_is_initialized()) {
                opencellid_cellular_environment_t environment;
                opencellid_triangulation_result_t triangulation;
                
                if (opencellid_get_cellular_environment(&environment) == AUTONOMY_SUCCESS &&
                    opencellid_triangulate_position(&environment, &triangulation) == AUTONOMY_SUCCESS) {
                    
                    data->latitude = triangulation.latitude;
                    data->longitude = triangulation.longitude;
                    data->accuracy = triangulation.accuracy;
                    data->confidence = triangulation.confidence;
                    data->timestamp = triangulation.calculation_time;
                    data->valid = true;
                    data->source_priority = 3; // Third priority
                    strcpy(data->raw_json, triangulation.method);
                    
                    ret = AUTONOMY_SUCCESS;
                }
            }
            break;
        }
        
        case GPS_SOURCE_GOOGLE: {
            // Use Google Geolocation API via external_apis if configured
            external_location_data_t location_data = {0};
            int gl_rc = external_apis_get_google_location(NULL, NULL, &location_data);
            if (gl_rc == AUTONOMY_SUCCESS) {
                data->latitude = location_data.latitude;
                data->longitude = location_data.longitude;
                data->accuracy = location_data.accuracy;
                data->confidence = fmax(0.0, fmin(1.0, 1.0 - (location_data.accuracy / 1000.0))); // map 0-1000m
                data->timestamp = location_data.timestamp;
                data->valid = true;
                data->source_priority = 4; // Lower priority than Starlink/OpenCell
                strncpy(data->source, "google_geolocation", sizeof(data->source) - 1);
                strncpy(data->raw_json, location_data.source, sizeof(data->raw_json) - 1);
                ret = AUTONOMY_SUCCESS;
            } else {
                ret = AUTONOMY_ERROR_NOT_FOUND;
            }
            break;
        }
        
        default:
            ret = AUTONOMY_ERROR_INVALID_PARAM;
            break;
    }
    
    // Calculate collection time
    time_t end_time = time(NULL);
    data->collection_duration_ms = difftime(end_time, start_time) * 1000.0;
    
    // Calculate confidence if not already set
    if (ret == AUTONOMY_SUCCESS && data->confidence == 0.0) {
        gps_source_health_t* health = &g_gps_collector.source_health[source_type];
        data->confidence = calculate_source_confidence(data, health);
    }
    
    // Update source health
    update_source_health(source_type, ret == AUTONOMY_SUCCESS, data->collection_duration_ms, data);
    
    return ret;
}

// Perform multi-source fusion
static int perform_multi_source_fusion(gps_fusion_result_t* result) {
    int sources_collected = 0;
    double total_weight = 0.0;
    double weighted_lat = 0.0;
    double weighted_lon = 0.0;
    double weighted_accuracy = 0.0;
    double total_confidence = 0.0;
    
    strcpy(result->fusion_method, "weighted_fusion");
    
    // Collect data from all available sources
    for (int i = 0; i < GPS_SOURCE_MAX; i++) {
        standardized_gps_data_t data;
        if (collect_from_source((gps_source_type_t)i, &data) == AUTONOMY_SUCCESS) {
            result->source_data[sources_collected] = data;
            sources_collected++;
            
            // Calculate weight based on confidence, accuracy, and freshness
            double weight = data.confidence;
            
            // Accuracy weighting (better accuracy = higher weight)
            if (data.accuracy > 0) {
                double accuracy_factor = 1.0 / (1.0 + data.accuracy / 100.0);
                weight *= accuracy_factor * g_gps_collector.config.fusion_weight_accuracy;
            }
            
            // Freshness weighting (newer data = higher weight)
            double age_seconds = difftime(time(NULL), data.timestamp);
            double freshness_factor = exp(-age_seconds / 300.0); // Decay over 5 minutes
            weight *= freshness_factor * g_gps_collector.config.fusion_weight_freshness;
            
            // Source priority weighting
            double priority_factor = 1.0 / data.source_priority;
            weight *= priority_factor;
            
            // Add to weighted fusion
            weighted_lat += data.latitude * weight;
            weighted_lon += data.longitude * weight;
            weighted_accuracy += data.accuracy * weight;
            total_weight += weight;
            total_confidence += data.confidence;
            
            LOGX_DEBUG_MSG("GPS fusion source",
                      "source", data.source,
                      "weight", weight,
                      "confidence", data.confidence,
                      "accuracy", data.accuracy,
                      "age_seconds", age_seconds);
        }
    }
    
    if (sources_collected == 0) {
        strcpy(result->fusion_reasoning, "no_sources_available");
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    if (sources_collected == 1) {
        // Single source, no fusion needed
        result->fused_data = result->source_data[0];
        strcpy(result->fusion_method, "single_source");
        strcpy(result->fusion_reasoning, "only_one_source_available");
    } else {
        // Multi-source fusion
        if (total_weight > 0) {
            result->fused_data.latitude = weighted_lat / total_weight;
            result->fused_data.longitude = weighted_lon / total_weight;
            result->fused_data.accuracy = weighted_accuracy / total_weight;
            result->fused_data.confidence = total_confidence / sources_collected;
        } else {
            // Fallback to simple average
            result->fused_data.latitude = 0.0;
            result->fused_data.longitude = 0.0;
            result->fused_data.accuracy = 0.0;
            
            for (int i = 0; i < sources_collected; i++) {
                result->fused_data.latitude += result->source_data[i].latitude;
                result->fused_data.longitude += result->source_data[i].longitude;
                result->fused_data.accuracy += result->source_data[i].accuracy;
            }
            
            result->fused_data.latitude /= sources_collected;
            result->fused_data.longitude /= sources_collected;
            result->fused_data.accuracy /= sources_collected;
            result->fused_data.confidence = total_confidence / sources_collected;
            
            strcpy(result->fusion_method, "simple_average");
        }
        
        // Set fusion metadata
        result->fused_data.timestamp = time(NULL);
        result->fused_data.valid = true;
        strcpy(result->fused_data.source, "fused");
        result->fused_data.source_type = GPS_SOURCE_MAX; // Special value for fused data
        
        snprintf(result->fusion_reasoning, sizeof(result->fusion_reasoning),
                "fused_%d_sources_weight_%.2f", sources_collected, total_weight);
    }
    
    result->sources_used = sources_collected;
    result->fusion_confidence = result->fused_data.confidence;
    
    LOGX_INFO_MSG("GPS fusion completed",
             "method", result->fusion_method,
             "sources_used", sources_collected,
             "confidence", result->fusion_confidence,
             "accuracy", result->fused_data.accuracy,
             "lat", result->fused_data.latitude,
             "lon", result->fused_data.longitude);
    
    return AUTONOMY_SUCCESS;
}

// Perform movement detection
int gps_comprehensive_detect_movement(const standardized_gps_data_t* current_data) {
    if (!g_collector_initialized || !current_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (!g_gps_collector.last_known.valid) {
        // First GPS reading, not movement
        return AUTONOMY_SUCCESS;
    }
    
    // Calculate distance from last known position
    double distance = gps_calculate_distance(
        g_gps_collector.last_known.latitude, g_gps_collector.last_known.longitude,
        current_data->latitude, current_data->longitude
    );
    
    bool has_moved = distance > g_gps_collector.config.movement_threshold_m;
    bool was_moving = g_gps_collector.movement_state.is_moving;
    
    if (has_moved && !was_moving) {
        // Movement detected
        g_gps_collector.movement_state.is_moving = true;
        g_gps_collector.movement_state.movement_start = time(NULL);
        g_gps_collector.movement_detections++;
        
        // Calculate speed
        double time_diff = difftime(current_data->timestamp, g_gps_collector.last_known.timestamp);
        if (time_diff > 0) {
            double speed = gps_calculate_speed(distance, time_diff);
            g_gps_collector.movement_state.current_speed_ms = speed;
            
            if (speed > g_gps_collector.movement_state.max_speed_ms) {
                g_gps_collector.movement_state.max_speed_ms = speed;
            }
        }
        
        g_gps_collector.movement_state.total_distance_m += distance;
        g_gps_collector.movement_state.movement_events++;
        g_gps_collector.movement_state.last_movement_event = time(NULL);
        
        LOGX_INFO_MSG("Movement detected",
                 "distance_m", distance,
                 "threshold_m", g_gps_collector.config.movement_threshold_m,
                 "speed_ms", g_gps_collector.movement_state.current_speed_ms,
                 "from_lat", g_gps_collector.last_known.latitude,
                 "from_lon", g_gps_collector.last_known.longitude,
                 "to_lat", current_data->latitude,
                 "to_lon", current_data->longitude);
        
    } else if (!has_moved && was_moving) {
        // Stopped moving
        g_gps_collector.movement_state.is_moving = false;
        g_gps_collector.movement_state.stationary_start = time(NULL);
        
        LOGX_INFO_MSG("Movement stopped",
                 "distance_m", distance,
                 "threshold_m", g_gps_collector.config.movement_threshold_m);
    }
    
    g_gps_collector.movement_state.was_moving = was_moving;
    g_gps_collector.movement_state.last_latitude = current_data->latitude;
    g_gps_collector.movement_state.last_longitude = current_data->longitude;
    
    return AUTONOMY_SUCCESS;
}

// Calculate source confidence based on data quality and source health
static double calculate_source_confidence(const standardized_gps_data_t* data, 
                                         const gps_source_health_t* health) {
    double confidence = 0.0;
    
    // Base confidence from data validity
    if (data->valid) {
        confidence = 0.5; // Start with 50% for valid data
        
        // Accuracy bonus (better accuracy = higher confidence)
        if (data->accuracy > 0) {
            if (data->accuracy <= 5.0) confidence += 0.3;
            else if (data->accuracy <= 10.0) confidence += 0.25;
            else if (data->accuracy <= 20.0) confidence += 0.2;
            else if (data->accuracy <= 50.0) confidence += 0.15;
            else if (data->accuracy <= 100.0) confidence += 0.1;
            else confidence += 0.05;
        }
        
        // Satellite bonus (more satellites = higher confidence)
        if (data->satellites_used > 0) {
            if (data->satellites_used >= 12) confidence += 0.15;
            else if (data->satellites_used >= 8) confidence += 0.1;
            else if (data->satellites_used >= 6) confidence += 0.05;
        }
        
        // HDOP bonus (lower HDOP = higher confidence)
        if (data->hdop > 0) {
            if (data->hdop <= 1.0) confidence += 0.1;
            else if (data->hdop <= 2.0) confidence += 0.05;
        }
        
        // Fix quality bonus
        switch (data->fix_quality) {
            case GPS_FIX_QUALITY_RTK:
            case GPS_FIX_QUALITY_RTK_FLOAT:
                confidence += 0.2;
                break;
            case GPS_FIX_QUALITY_DGPS:
                confidence += 0.15;
                break;
            case GPS_FIX_QUALITY_GPS:
                confidence += 0.1;
                break;
            default:
                break;
        }
        
        // Freshness bonus (newer data = higher confidence)
        double age_seconds = difftime(time(NULL), data->timestamp);
        if (age_seconds <= 30) confidence += 0.1;
        else if (age_seconds <= 60) confidence += 0.05;
        else if (age_seconds > 300) confidence -= 0.2; // Penalty for old data
        
        // Source health bonus
        if (health) {
            confidence *= health->health_score; // Scale by source health
            
            // Success rate bonus
            if (health->success_rate > 0.9) confidence += 0.05;
            else if (health->success_rate < 0.5) confidence -= 0.1;
        }
    }
    
    // Ensure confidence stays in valid range
    if (confidence > 1.0) confidence = 1.0;
    if (confidence < 0.0) confidence = 0.0;
    
    return confidence;
}

// Update source health statistics
static void update_source_health(gps_source_type_t source_type, bool success,
                                double collection_time_ms, const standardized_gps_data_t* data) {
    if (source_type >= GPS_SOURCE_MAX) return;
    
    gps_source_health_t* health = &g_gps_collector.source_health[source_type];
    
    health->total_collections++;
    health->last_collection_attempt = time(NULL);
    health->last_seen = time(NULL);
    
    if (success) {
        health->successful_collections++;
        health->consecutive_successes++;
        health->consecutive_failures = 0;
        health->last_success = time(NULL);
        
        if (data) {
            // Update performance metrics
            health->average_collection_time_ms = 
                (health->average_collection_time_ms * (health->successful_collections - 1) + collection_time_ms) /
                health->successful_collections;
            
            if (data->accuracy > 0) {
                health->average_accuracy = 
                    (health->average_accuracy * (health->successful_collections - 1) + data->accuracy) /
                    health->successful_collections;
                
                if (data->accuracy < health->best_accuracy) {
                    health->best_accuracy = data->accuracy;
                }
                if (data->accuracy > health->worst_accuracy) {
                    health->worst_accuracy = data->accuracy;
                }
            }
            
            if (data->confidence > 0) {
                health->average_confidence = 
                    (health->average_confidence * (health->successful_collections - 1) + data->confidence) /
                    health->successful_collections;
            }
        }
    } else {
        health->failed_collections++;
        health->consecutive_failures++;
        health->consecutive_successes = 0;
        health->last_failure = time(NULL);
    }
    
    // Calculate success rate
    if (health->total_collections > 0) {
        health->success_rate = (double)health->successful_collections / health->total_collections;
    }
    
    // Calculate health score
    double health_score = health->success_rate;
    
    // Penalty for consecutive failures
    if (health->consecutive_failures > 0) {
        health_score *= (1.0 - (health->consecutive_failures * 0.1));
    }
    
    // Bonus for consecutive successes
    if (health->consecutive_successes > 5) {
        health_score = fmin(1.0, health_score * 1.1);
    }
    
    health->health_score = fmax(0.0, health_score);
    health->healthy = (health->health_score > g_gps_collector.config.min_health_score);
    health->available = health->healthy && (health->consecutive_failures < g_gps_collector.config.max_consecutive_failures);
    
    LOGX_DEBUG_MSG("GPS source health updated",
              "source", gps_source_type_to_string(source_type),
              "success", success ? "true" : "false",
              "health_score", health->health_score,
              "success_rate", health->success_rate,
              "consecutive_failures", health->consecutive_failures);
}

// Finalize GPS data (final processing)
static void finalize_gps_data(standardized_gps_data_t* data) {
    if (!data) return;
    
    // Calculate age
    data->age_seconds = difftime(time(NULL), data->timestamp);
    
    // Apply staleness penalty
    if (data->age_seconds > g_gps_collector.config.staleness_threshold_s) {
        data->staleness_penalty = data->age_seconds / g_gps_collector.config.staleness_threshold_s;
        data->confidence *= (1.0 - fmin(0.5, data->staleness_penalty * 0.1));
    }
    
    // Apply accuracy bonus
    if (data->accuracy > 0 && data->accuracy <= 10.0) {
        data->accuracy_bonus = 0.1 * (10.0 - data->accuracy) / 10.0;
        data->confidence += data->accuracy_bonus;
    }
    
    // Apply satellite bonus
    if (data->satellites_used >= 8) {
        data->satellite_bonus = 0.05 * (data->satellites_used - 8) / 4.0; // Max bonus for 12+ satellites
        data->confidence += data->satellite_bonus;
    }
    
    // Ensure confidence stays in valid range
    if (data->confidence > 1.0) data->confidence = 1.0;
    if (data->confidence < 0.0) data->confidence = 0.0;
}

// Health check for GPS comprehensive system
int gps_comprehensive_health_check(void) {
    if (!g_collector_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    bool all_healthy = true;
    
    // Check each GPS source health
    for (int i = 0; i < GPS_SOURCE_MAX; i++) {
        gps_source_health_t* health = &g_gps_collector.source_health[i];
        if (!health->healthy) {
            all_healthy = false;
            LOGX_WARN_MSG("GPS source unhealthy", "source", gps_source_type_to_string((gps_source_type_t)i));
        }
    }
    
    return all_healthy ? AUTONOMY_SUCCESS : AUTONOMY_ERROR_SYSTEM;
}

// Get health for specific GPS source
int gps_comprehensive_get_source_health(gps_source_type_t source_type, gps_source_health_t* health) {
    if (!g_collector_initialized || !health || source_type >= GPS_SOURCE_MAX) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_gps_collector.mutex);
    *health = g_gps_collector.source_health[source_type];
    pthread_mutex_unlock(&g_gps_collector.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get health for all GPS sources
int gps_comprehensive_get_all_source_health(gps_source_health_t* health_array, int max_sources) {
    if (!g_collector_initialized || !health_array || max_sources <= 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_gps_collector.mutex);
    
    int count = (max_sources < GPS_SOURCE_MAX) ? max_sources : GPS_SOURCE_MAX;
    for (int i = 0; i < count; i++) {
        health_array[i] = g_gps_collector.source_health[i];
    }
    
    pthread_mutex_unlock(&g_gps_collector.mutex);
    
    return count;
}

// Get movement state
int gps_comprehensive_get_movement_state(gps_movement_state_t* movement_state) {
    if (!g_collector_initialized || !movement_state) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_gps_collector.mutex);
    *movement_state = g_gps_collector.movement_state;
    pthread_mutex_unlock(&g_gps_collector.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Validate GPS data
int gps_comprehensive_validate_data(const standardized_gps_data_t* gps_data) {
    if (!gps_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Check if data is marked as valid
    if (!gps_data->valid) {
        return AUTONOMY_ERROR_INVALID_DATA;
    }
    
    // Check latitude range
    if (gps_data->latitude < -90.0 || gps_data->latitude > 90.0) {
        return AUTONOMY_ERROR_INVALID_DATA;
    }
    
    // Check longitude range  
    if (gps_data->longitude < -180.0 || gps_data->longitude > 180.0) {
        return AUTONOMY_ERROR_INVALID_DATA;
    }
    
    // Check accuracy is reasonable
    if (gps_data->accuracy < 0.0 || gps_data->accuracy > 100000.0) {
        return AUTONOMY_ERROR_INVALID_DATA;
    }
    
    // Check confidence is in valid range
    if (gps_data->confidence < 0.0 || gps_data->confidence > 1.0) {
        return AUTONOMY_ERROR_INVALID_DATA;
    }
    
    return AUTONOMY_SUCCESS;
}

// Calculate GPS data confidence
double gps_comprehensive_calculate_confidence(const standardized_gps_data_t* gps_data, 
                                            const gps_source_health_t* source_health) {
    if (!gps_data || !source_health) {
        return 0.0;
    }
    
    double confidence = 0.5; // Base confidence
    
    // Factor in accuracy (better accuracy = higher confidence)
    if (gps_data->accuracy > 0.0) {
        confidence += (100.0 - gps_data->accuracy) / 200.0; // Max +0.5
    }
    
    // Factor in source health
    confidence *= source_health->health_score;
    
    // Factor in data freshness
    time_t now = time(NULL);
    int age_seconds = now - gps_data->timestamp;
    if (age_seconds < 60) {
        confidence += 0.2; // Fresh data
    } else if (age_seconds < 300) {
        confidence += 0.1; // Reasonably fresh
    } else {
        confidence -= 0.1; // Old data
    }
    
    // Clamp to valid range
    if (confidence < 0.0) confidence = 0.0;
    if (confidence > 1.0) confidence = 1.0;
    
    return confidence;
}

// Utility functions
const char* gps_source_type_to_string(gps_source_type_t source_type) {
    if (source_type >= 0 && source_type < GPS_SOURCE_MAX) {
        return SOURCE_TYPE_STRINGS[source_type];
    }
    return "unknown";
}

gps_source_type_t gps_parse_source_type(const char* source_str) {
    if (!source_str) return GPS_SOURCE_RUTOS;
    
    for (int i = 0; i < GPS_SOURCE_MAX; i++) {
        if (strcasecmp(source_str, SOURCE_TYPE_STRINGS[i]) == 0) {
            return (gps_source_type_t)i;
        }
    }
    
    return GPS_SOURCE_RUTOS;
}

const char* gps_fix_type_to_string(gps_fix_type_t fix_type) {
    if (fix_type >= 0 && fix_type < GPS_FIX_TYPE_MAX) {
        return FIX_TYPE_STRINGS[fix_type];
    }
    return "unknown";
}

const char* gps_fix_quality_to_string(gps_fix_quality_t fix_quality) {
    if (fix_quality >= 0 && fix_quality < GPS_FIX_QUALITY_MAX) {
        return FIX_QUALITY_STRINGS[fix_quality];
    }
    return "unknown";
}

// Calculate distance using Haversine formula
double gps_calculate_distance(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371000; // Earth's radius in meters
    
    double lat1_rad = lat1 * M_PI / 180.0;
    double lat2_rad = lat2 * M_PI / 180.0;
    double delta_lat = (lat2 - lat1) * M_PI / 180.0;
    double delta_lon = (lon2 - lon1) * M_PI / 180.0;
    
    double a = sin(delta_lat / 2) * sin(delta_lat / 2) +
               cos(lat1_rad) * cos(lat2_rad) *
               sin(delta_lon / 2) * sin(delta_lon / 2);
    
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    
    return R * c;
}

// Calculate bearing between two GPS points
double gps_calculate_bearing(double lat1, double lon1, double lat2, double lon2) {
    double lat1_rad = lat1 * M_PI / 180.0;
    double lat2_rad = lat2 * M_PI / 180.0;
    double delta_lon = (lon2 - lon1) * M_PI / 180.0;
    
    double y = sin(delta_lon) * cos(lat2_rad);
    double x = cos(lat1_rad) * sin(lat2_rad) - sin(lat1_rad) * cos(lat2_rad) * cos(delta_lon);
    
    double bearing_rad = atan2(y, x);
    double bearing_deg = bearing_rad * 180.0 / M_PI;
    
    // Normalize to 0-360 degrees
    return fmod(bearing_deg + 360.0, 360.0);
}

// Calculate speed from distance and time
double gps_calculate_speed(double distance, double time_diff) {
    if (time_diff <= 0) return 0.0;
    return distance / time_diff;
}

// Check if comprehensive GPS collector is initialized
bool gps_comprehensive_is_initialized(void) {
    return g_collector_initialized;
}

// Get comprehensive GPS statistics
int gps_comprehensive_get_statistics(uint64_t* total_collections,
                                   uint64_t* successful_collections,
                                   uint64_t* failed_collections,
                                   uint64_t* fusion_operations,
                                   uint64_t* movement_detections) {
    if (!g_collector_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_gps_collector.mutex);
    
    if (total_collections) *total_collections = g_gps_collector.total_collections;
    if (successful_collections) *successful_collections = g_gps_collector.successful_collections;
    if (failed_collections) *failed_collections = g_gps_collector.failed_collections;
    if (fusion_operations) *fusion_operations = g_gps_collector.fusion_operations;
    if (movement_detections) *movement_detections = g_gps_collector.movement_detections;
    
    pthread_mutex_unlock(&g_gps_collector.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Health monitor thread worker
static void* health_monitor_thread_worker(void* arg) {
    (void)arg; // Suppress unused parameter warning
    
    LOGX_INFO_MSG("GPS health monitor thread started");
    
    while (g_collector_initialized && g_gps_collector.threads_running) {
        sleep(g_gps_collector.config.health_check_interval_s);
        
        if (!g_gps_collector.threads_running) break;
        
        // Perform health check for all sources
        gps_comprehensive_health_check();
    }
    
    LOGX_INFO_MSG("GPS health monitor thread stopped");
    return NULL;
}

// Collect GPS data from best available source (alias for collect_best)
int gps_comprehensive_collect_best_gps(standardized_gps_data_t* result) {
    return gps_comprehensive_collect_best(result);
}

// Get current location from comprehensive GPS system
int gps_comprehensive_get_current_location(gps_data_t *location) {
    if (!location) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }

    if (!g_collector_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }

    standardized_gps_data_t standardized_data;
    int result = gps_comprehensive_collect_best(&standardized_data);
    
    if (result == AUTONOMY_SUCCESS) {
        // Convert standardized data to gps_data_t format
        location->lat = standardized_data.latitude;
        location->lon = standardized_data.longitude;
        location->latitude = standardized_data.latitude;
        location->longitude = standardized_data.longitude;
        location->altitude = standardized_data.altitude;
        location->accuracy = standardized_data.accuracy;
        location->speed = standardized_data.speed;
        location->heading = standardized_data.heading;
        location->timestamp = standardized_data.timestamp;
        location->satellites = standardized_data.satellites_used;
        location->hdop = standardized_data.hdop;
        location->vdop = standardized_data.vdop;
        location->valid = standardized_data.valid;
        
        return AUTONOMY_SUCCESS;
    }
    
    // Return default/unknown location on failure
    memset(location, 0, sizeof(gps_data_t));
    location->lat = 0.0;
    location->lon = 0.0;
    location->latitude = 0.0;
    location->longitude = 0.0;
    location->altitude = 0.0;
    location->accuracy = 0.0;
    location->timestamp = time(NULL);
    location->valid = false;
    
    return AUTONOMY_ERROR_NO_DATA;
}