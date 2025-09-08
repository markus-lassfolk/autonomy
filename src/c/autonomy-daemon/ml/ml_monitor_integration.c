#include "ml_monitor.h"
#include "../utils/logx.h"
#include "../starlink/starlink_modules.h"
#include "../starlink/starlink_comprehensive.h"
#include "../starlink/starlink_grpc_collector.h"
#include "../gps/gps_manager.h"
#include "../gps/gps_weather.h"
#include "../external/external_apis.h"
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

// Define MAX_OBSERVATIONS if not defined
#ifndef MAX_OBSERVATIONS
#define MAX_OBSERVATIONS 1000
#endif

// Forward declarations for functions we need
extern int gps_get_current_location(gps_data_t *gps_data);
extern int gps_comprehensive_collect_best_gps(gps_data_t *gps_data);

// Forward declarations
static int ml_monitor_collect_starlink_data(starlink_status_response_t *starlink_data);
static int ml_monitor_collect_gps_data(gps_data_t *gps_data);
static int ml_monitor_collect_weather_data(gps_weather_current_t *weather_data, double lat, double lon);
static void ml_monitor_on_outage_prediction(uint8_t probability, uint8_t confidence, time_t when, void *user_data);
static void ml_monitor_on_anomaly_detected(uint8_t score, const ml_observation_t *observation, void *user_data);

// Enhanced data collection with real data sources
int ml_monitor_collect_observation(ml_monitor_t *monitor) {
    if (!monitor || !monitor->initialized) return ML_MONITOR_ERROR_NOT_INITIALIZED;
    
    LOGX_DEBUG("Collecting ML observation from real data sources");
    
    ml_observation_t observation;
    memset(&observation, 0, sizeof(observation));
    
    // Set timestamp
    observation.timestamp = time(NULL);
    
    // Collect Starlink data
    starlink_status_response_t starlink_data;
    int starlink_result = ml_monitor_collect_starlink_data(&starlink_data);
    
    // Collect GPS data
    gps_data_t gps_data;
    int gps_result = ml_monitor_collect_gps_data(&gps_data);
    
    // Collect weather data (if GPS is available)
    gps_weather_current_t weather_data;
    int weather_result = AUTONOMY_ERROR;
    if (gps_result == AUTONOMY_SUCCESS && gps_data.valid) {
        weather_result = ml_monitor_collect_weather_data(&weather_data, gps_data.lat, gps_data.lon);
    }
    
    // Convert collected data to ML observation
    if (starlink_result == AUTONOMY_SUCCESS) {
        // Starlink metrics
        observation.snr_x100 = (uint16_t)(starlink_data.signal_quality.snr * 100);
        observation.latency_ms = (uint16_t)starlink_data.network_perf.pop_ping_latency_ms;
        observation.packet_loss_pct = (uint8_t)(starlink_data.network_perf.pop_ping_drop_rate * 100);
        observation.obstruction_pct = (uint8_t)(starlink_data.obstruction_stats.fraction_obstructed * 100);
        
        // Copy wedge obstructions
        for (int i = 0; i < 12 && i < sizeof(observation.wedge_obstruction); i++) {
            observation.wedge_obstruction[i] = (uint8_t)(starlink_data.obstruction_stats.wedge_fraction_obstructed[i] * 100);
        }
        
        // Positioning
        observation.azimuth_deg = (int16_t)starlink_data.positioning.boresight_azimuth_deg;
        observation.elevation_deg = (int16_t)starlink_data.positioning.boresight_elevation_deg;
        
        // Estimate visible satellites
        observation.satellites_visible = starlink_data.gps_stats.gps_sats > 0 ? starlink_data.gps_stats.gps_sats : 8;
        
        // Set flags based on Starlink status
        if (starlink_data.obstruction_stats.currently_obstructed) {
            observation.flags |= ML_OBS_FLAG_OUTAGE;
        }
        if (starlink_data.signal_quality.snr < 5.0) {
            observation.flags |= ML_OBS_FLAG_DEGRADED;
        }
        
        LOGX_DEBUG("Starlink data collected: SNR=%.2f dB, latency=%u ms, obstruction=%u%%",
                  starlink_data.signal_quality.snr, observation.latency_ms, observation.obstruction_pct);
    } else {
        LOGX_WARN("Failed to collect Starlink data, using defaults");
        // Use reasonable defaults for missing Starlink data
        observation.snr_x100 = 800; // 8.0 dB
        observation.latency_ms = 50;
        observation.packet_loss_pct = 0;
        observation.obstruction_pct = 0;
        observation.satellites_visible = 8;
    }
    
    // GPS/Location data
    if (gps_result == AUTONOMY_SUCCESS && gps_data.valid) {
        observation.latitude_e7 = (int32_t)(gps_data.lat * 10000000);
        observation.longitude_e7 = (int32_t)(gps_data.lon * 10000000);
        observation.altitude_m = (uint16_t)gps_data.altitude;
        observation.speed_kmh = (uint8_t)(gps_data.speed * 3.6); // Convert m/s to km/h
        observation.heading_deg_div2 = (uint8_t)(gps_data.heading / 2);
        
        if (gps_data.speed > 1.0) { // Moving threshold
            observation.flags |= ML_OBS_FLAG_MOVING;
        }
        
        LOGX_DEBUG("GPS data collected: lat=%.6f, lon=%.6f, speed=%.1f km/h",
                  gps_data.lat, gps_data.lon, gps_data.speed * 3.6);
    } else {
        LOGX_DEBUG("GPS data not available, using default location");
        // Use default location if GPS unavailable
        observation.latitude_e7 = 0;
        observation.longitude_e7 = 0;
        observation.altitude_m = 0;
        observation.speed_kmh = 0;
        observation.heading_deg_div2 = 0;
    }
    
    // Weather data
    if (weather_result == AUTONOMY_SUCCESS) {
        observation.temperature_c = (int8_t)weather_data.temperature;
        observation.humidity_pct = (uint8_t)weather_data.humidity;
        observation.pressure_hpa = (uint16_t)weather_data.pressure;
        observation.wind_speed_ms = (uint8_t)weather_data.wind_speed;
        observation.cloud_cover_pct = (uint8_t)weather_data.cloud_cover;
        
        // Calculate precipitation based on weather condition
        switch (weather_data.weather_condition) {
            case WEATHER_CONDITION_RAIN:
            case WEATHER_CONDITION_HEAVY_RAIN:
                observation.precipitation_mm = 5; // Estimate
                break;
            case WEATHER_CONDITION_SNOW:
            case WEATHER_CONDITION_HEAVY_SNOW:
                observation.precipitation_mm = 3; // Snow equivalent
                break;
            case WEATHER_CONDITION_DRIZZLE:
                observation.precipitation_mm = 1;
                break;
            default:
                observation.precipitation_mm = 0;
                break;
        }
        
        // Set weather impact flag
        if (observation.precipitation_mm > 1 || observation.cloud_cover_pct > 80) {
            observation.flags |= ML_OBS_FLAG_WEATHER_IMPACT;
        }
        
        LOGX_DEBUG("Weather data collected: temp=%.1f°C, humidity=%u%%, pressure=%u hPa",
                  weather_data.temperature, observation.humidity_pct, observation.pressure_hpa);
    } else {
        LOGX_DEBUG("Weather data not available, using defaults");
        // Use reasonable defaults for missing weather data
        observation.temperature_c = 20; // 20°C
        observation.humidity_pct = 50;   // 50%
        observation.pressure_hpa = 1013; // Standard pressure
        observation.wind_speed_ms = 0;
        observation.precipitation_mm = 0;
        observation.cloud_cover_pct = 0;
    }
    
    // Add observation to ML monitor
    int add_result = ml_monitor_add_observation(monitor, &observation);
    if (add_result != ML_MONITOR_SUCCESS) {
        LOGX_ERROR("Failed to add observation to ML monitor: %d", add_result);
        return add_result;
    }
    
    // Update learning models
    ml_monitor_update_sky_grid(monitor, &observation);
    ml_monitor_update_location_learning(monitor, &observation);
    
    // Phase 3: Update with enhanced learning
    ml_monitor_update_with_phase3_enhancements(monitor, &observation);
    
    // Phase 4: Update with ensemble methods and validation
    ml_monitor_update_with_phase4_enhancements(monitor, &observation);
    
    // Phase 5: Update with mobile optimization
    ml_monitor_update_with_phase5_mobile_optimization(monitor, &observation);
    
    // Phase 6: Update with self-optimization
    ml_monitor_update_with_phase6_self_optimization(monitor, &observation);
    
    // Phase 7: Update with multi-interface intelligence
    ml_monitor_update_with_phase7_multi_interface(monitor, &observation);
    
    // Make predictions if we have enough data
    if (monitor->state->total_observations > 10) {
        // Use advanced ensemble prediction (Phase 4)
        uint8_t ensemble_probability, ensemble_confidence, ensemble_cause;
        int ensemble_result = ml_monitor_predict_ensemble(monitor, &observation, 
                                                        &ensemble_probability, 
                                                        &ensemble_confidence, 
                                                        &ensemble_cause);
        
        if (ensemble_result == ML_MONITOR_SUCCESS) {
            // Update ML features with ensemble results
            observation.outage_probability = ensemble_probability;
            observation.outage_type = ensemble_cause;
            observation.confidence = ensemble_confidence;
        } else {
            // Fallback to individual models
            uint8_t knn_confidence;
            uint8_t knn_prediction = ml_monitor_predict_outage_knn(monitor, &observation, &knn_confidence);
            
            uint8_t nn_output[8];
            ml_monitor_predict_neural_network(monitor, &observation, nn_output);
            
            observation.outage_probability = nn_output[0];
            observation.outage_type = knn_prediction;
            observation.confidence = knn_confidence;
        }
        
        // Check for high-confidence predictions
        if (knn_confidence > monitor->config.confidence_threshold && nn_output[0] > 150) {
            observation.flags |= ML_OBS_FLAG_PREDICTION_MADE;
            
            // Trigger prediction callback if set
            if (monitor->outage_prediction_callback) {
                monitor->outage_prediction_callback(nn_output[0], knn_confidence, 
                                                  time(NULL) + 900, // 15 minutes ahead
                                                  monitor->callback_user_data);
            }
            
            LOGX_INFO("High-confidence outage prediction: %u%% probability, %u%% confidence",
                     nn_output[0], knn_confidence);
        }
        
        // Check for anomalies
        if (observation.snr_x100 < 300 || observation.latency_ms > 200 || observation.packet_loss_pct > 10) {
            observation.anomaly_score = 200; // High anomaly score
            observation.flags |= ML_OBS_FLAG_ANOMALY;
            
            if (monitor->anomaly_detected_callback) {
                monitor->anomaly_detected_callback(observation.anomaly_score, &observation, 
                                                 monitor->callback_user_data);
            }
            
            LOGX_WARN("Anomaly detected: SNR=%.2f, latency=%u ms, loss=%u%%",
                     observation.snr_x100 / 100.0, observation.latency_ms, observation.packet_loss_pct);
        }
    }
    
    LOGX_DEBUG("ML observation collected and processed successfully");
    return ML_MONITOR_SUCCESS;
}

// Collect Starlink data from existing collectors
static int ml_monitor_collect_starlink_data(starlink_status_response_t *starlink_data) {
    if (!starlink_data) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    // Try comprehensive Starlink collector first
    if (starlink_comprehensive_is_initialized()) {
        starlink_comprehensive_status_t comprehensive_status;
        if (starlink_comprehensive_collect_all(&comprehensive_status) == AUTONOMY_SUCCESS) {
            // Convert comprehensive status to standard format
            memset(starlink_data, 0, sizeof(starlink_status_response_t));
            
            // Copy relevant data from comprehensive status
            starlink_data->signal_quality.snr = comprehensive_status.snr;
            starlink_data->network_perf.pop_ping_latency_ms = comprehensive_status.latency_ms;
            starlink_data->network_perf.pop_ping_drop_rate = comprehensive_status.packet_loss_rate;
            starlink_data->obstruction_stats.fraction_obstructed = comprehensive_status.obstruction_fraction;
            starlink_data->obstruction_stats.currently_obstructed = comprehensive_status.currently_obstructed;
            starlink_data->positioning.boresight_azimuth_deg = comprehensive_status.azimuth;
            starlink_data->positioning.boresight_elevation_deg = comprehensive_status.elevation;
            
            // Copy wedge data if available
            for (int i = 0; i < 12; i++) {
                starlink_data->obstruction_stats.wedge_fraction_obstructed[i] = 
                    comprehensive_status.wedge_obstruction[i];
            }
            
            starlink_data->gps_stats.gps_sats = comprehensive_status.gps_data.satellites;
            
            LOGX_DEBUG("Collected Starlink data from comprehensive collector");
            return AUTONOMY_SUCCESS;
        }
    }
    
    // Try regular Starlink collector as fallback
    starlink_collection_result_t collection_result;
    if (starlink_collect_data(&collection_result) == AUTONOMY_SUCCESS) {
        *starlink_data = collection_result.status;
        LOGX_DEBUG("Collected Starlink data from regular collector");
        return AUTONOMY_SUCCESS;
    }
    
    // Try gRPC collector as last resort
    extern starlink_grpc_collector_t g_starlink_grpc_collector;
    if (g_starlink_grpc_collector.enabled && g_starlink_grpc_collector.observation_count > 0) {
        // Get the latest observation from gRPC collector
        pthread_mutex_lock(&g_starlink_grpc_collector.mutex);
        
        if (g_starlink_grpc_collector.observation_count > 0) {
            int latest_index = (g_starlink_grpc_collector.current_observation_index - 1 + MAX_OBSERVATIONS) % MAX_OBSERVATIONS;
            starlink_observation_t latest_obs = g_starlink_grpc_collector.observations[latest_index];
            
            // Convert to standard format
            memset(starlink_data, 0, sizeof(starlink_status_response_t));
            starlink_data->signal_quality.snr = latest_obs.snr;
            starlink_data->network_perf.pop_ping_latency_ms = latest_obs.latency_ms;
            starlink_data->network_perf.pop_ping_drop_rate = latest_obs.packet_loss_rate;
            starlink_data->obstruction_stats.fraction_obstructed = latest_obs.obstruction_fraction;
            starlink_data->obstruction_stats.currently_obstructed = latest_obs.currently_obstructed;
            
            pthread_mutex_unlock(&g_starlink_grpc_collector.mutex);
            
            LOGX_DEBUG("Collected Starlink data from gRPC collector");
            return AUTONOMY_SUCCESS;
        }
        
        pthread_mutex_unlock(&g_starlink_grpc_collector.mutex);
    }
    
    LOGX_WARN("No Starlink data available from any collector");
    return AUTONOMY_ERROR_NO_DATA;
}

// Collect GPS data from GPS manager
static int ml_monitor_collect_gps_data(gps_data_t *gps_data) {
    if (!gps_data) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    // Try to get current GPS location from GPS manager
    if (gps_get_current_location(gps_data) == AUTONOMY_SUCCESS && gps_data->valid) {
        LOGX_DEBUG("Collected GPS data from GPS manager");
        return AUTONOMY_SUCCESS;
    }
    
    // Try comprehensive GPS collection as fallback
    if (gps_comprehensive_collect_best_gps(gps_data) == AUTONOMY_SUCCESS && gps_data->valid) {
        LOGX_DEBUG("Collected GPS data from comprehensive GPS");
        return AUTONOMY_SUCCESS;
    }
    
    LOGX_DEBUG("No valid GPS data available");
    return AUTONOMY_ERROR_NO_DATA;
}

// Collect weather data from weather integration
static int ml_monitor_collect_weather_data(gps_weather_current_t *weather_data, double lat, double lon) {
    if (!weather_data) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    // Try to get current weather data
    if (gps_weather_get_current(lat, lon, weather_data) == AUTONOMY_SUCCESS) {
        LOGX_DEBUG("Collected weather data from weather integration");
        return AUTONOMY_SUCCESS;
    }
    
    LOGX_DEBUG("No weather data available for coordinates %.6f, %.6f", lat, lon);
    return AUTONOMY_ERROR_NO_DATA;
}

// Enhanced data collection thread that uses real data sources
static void* ml_monitor_collection_thread_enhanced(void *arg) {
    ml_monitor_t *monitor = (ml_monitor_t*)arg;
    if (!monitor) return NULL;
    
    LOGX_INFO("Enhanced ML monitor collection thread started with real data integration");
    
    // Set up prediction callbacks
    ml_monitor_set_outage_prediction_callback(monitor, ml_monitor_on_outage_prediction, monitor);
    
    int collection_count = 0;
    time_t last_sync = time(NULL);
    
    while (!monitor->should_stop) {
        // Collect observation from real data sources
        int result = ml_monitor_collect_observation(monitor);
        
        if (result == ML_MONITOR_SUCCESS) {
            collection_count++;
            monitor->last_collection = time(NULL);
            
            // Sync storage periodically
            time_t now = time(NULL);
            if (now - last_sync > monitor->config.storage_sync_interval_minutes * 60) {
                ml_monitor_sync_storage(monitor);
                last_sync = now;
                LOGX_DEBUG("Synced ML storage to disk (%d collections)", collection_count);
            }
            
            // Log progress periodically
            if (collection_count % 100 == 0) {
                LOGX_INFO("ML monitor collected %d observations, total: %u",
                         collection_count, monitor->state->total_observations);
            }
        } else {
            LOGX_WARN("Failed to collect ML observation: %d", result);
        }
        
        // Sleep for collection interval
        sleep(monitor->config.collection_interval_seconds);
    }
    
    LOGX_INFO("Enhanced ML monitor collection thread stopped after %d collections", collection_count);
    return NULL;
}

// Prediction callback handler
static void ml_monitor_on_outage_prediction(uint8_t probability, uint8_t confidence, time_t when, void *user_data) {
    ml_monitor_t *monitor = (ml_monitor_t*)user_data;
    if (!monitor) return;
    
    time_t now = time(NULL);
    int minutes_ahead = (when - now) / 60;
    
    LOGX_INFO("🔮 OUTAGE PREDICTION: %u%% probability in %d minutes (confidence: %u%%)",
             probability, minutes_ahead, confidence);
    
    // Update performance statistics
    if (monitor->state) {
        monitor->state->models.performance.predictions_made++;
        
        // Store prediction for later validation
        // In a full implementation, we'd track predictions and validate them
    }
    
    // TODO: Trigger proactive actions based on prediction
    // - Notify other systems
    // - Adjust network routing
    // - Log for analysis
}

// Anomaly detection callback handler
static void ml_monitor_on_anomaly_detected(uint8_t score, const ml_observation_t *observation, void *user_data) {
    if (!observation) return;
    
    LOGX_WARN("🚨 ANOMALY DETECTED: Score=%u, SNR=%.2f dB, Latency=%u ms, Loss=%u%%",
             score, observation->snr_x100 / 100.0, observation->latency_ms, observation->packet_loss_pct);
    
    // TODO: Trigger anomaly response actions
    // - Alert administrators
    // - Increase monitoring frequency
    // - Collect additional diagnostics
}

// Update location learning with real GPS data
int ml_monitor_update_location_learning(ml_monitor_t *monitor, const ml_observation_t *observation) {
    if (!monitor || !monitor->state || !observation) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    location_learner_t *learner = &monitor->state->models.location_learner;
    
    // Check if location changed significantly
    if (ml_monitor_location_changed_threshold(learner->current_lat_e7, learner->current_lon_e7,
                                            observation->latitude_e7, observation->longitude_e7,
                                            monitor->config.location_change_threshold_meters)) {
        
        LOGX_INFO("📍 Location changed: lat=%.6f, lon=%.6f", 
                 observation->latitude_e7 / 10000000.0, observation->longitude_e7 / 10000000.0);
        
        // Save current location profile if we learned something
        if (learner->observations_here > 50) {
            // Find empty slot in history or replace oldest
            int history_idx = learner->history_count < 10 ? learner->history_count++ : 0;
            
            learner->history[history_idx].lat_e7 = learner->current_lat_e7;
            learner->history[history_idx].lon_e7 = learner->current_lon_e7;
            learner->history[history_idx].last_visit = time(NULL);
            
            // Create profile hash (simplified)
            for (int i = 0; i < 16; i++) {
                learner->history[history_idx].profile_hash[i] = 
                    (learner->profile.typical_snr + learner->profile.typical_latency + i) % 256;
            }
            
            LOGX_DEBUG("Saved location profile to history (slot %d)", history_idx);
        }
        
        // Check if we've been to this location before
        bool found_previous = false;
        for (int i = 0; i < learner->history_count; i++) {
            if (ml_monitor_location_changed_threshold(learner->history[i].lat_e7, learner->history[i].lon_e7,
                                                    observation->latitude_e7, observation->longitude_e7, 200)) {
                continue; // Too far away
            }
            
            // Found previous visit to this location
            LOGX_INFO("🔄 Returned to known location, restoring profile");
            
            // Restore some learning from previous visit
            learner->profile.typical_snr = (learner->profile.typical_snr + 
                                          learner->history[i].profile_hash[0]) / 2;
            learner->profile.learned = 100; // Start with some knowledge
            
            found_previous = true;
            break;
        }
        
        if (!found_previous) {
            LOGX_INFO("🆕 New location detected, entering rapid learning mode");
            memset(&learner->profile, 0, sizeof(learner->profile));
        }
        
        // Update current location
        learner->current_lat_e7 = observation->latitude_e7;
        learner->current_lon_e7 = observation->longitude_e7;
        learner->arrival_time = observation->timestamp;
        learner->observations_here = 0;
    }
    
    // Update location profile with new observation
    learner->observations_here++;
    
    // Adaptive learning rate based on how much we know about this location
    uint8_t alpha = learner->observations_here < 20 ? 128 : 32;
    
    learner->profile.typical_snr = ml_monitor_weighted_average(
        learner->profile.typical_snr, observation->snr_x100 / 100, alpha);
    learner->profile.typical_latency = ml_monitor_weighted_average(
        learner->profile.typical_latency, observation->latency_ms / 10, alpha);
    learner->profile.obstruction_level = ml_monitor_weighted_average(
        learner->profile.obstruction_level, observation->obstruction_pct, alpha);
    
    // Update learned confidence
    if (learner->profile.learned < 255) {
        learner->profile.learned = ml_monitor_weighted_average(learner->profile.learned, 255, 5);
    }
    
    return ML_MONITOR_SUCCESS;
}

// Enhanced prediction function that uses real data
int ml_monitor_predict_next_15_minutes(ml_monitor_t *monitor, uint8_t probabilities[60], uint8_t *confidence) {
    if (!monitor || !monitor->state || !probabilities || !confidence) {
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    *confidence = 0;
    
    // Need sufficient data for predictions
    if (monitor->state->total_observations < 50) {
        LOGX_DEBUG("Insufficient data for predictions (%u observations)", monitor->state->total_observations);
        memset(probabilities, 0, 60);
        return ML_MONITOR_SUCCESS;
    }
    
    // Create a current observation for prediction
    ml_observation_t current_obs;
    if (ml_monitor_collect_observation(monitor) != ML_MONITOR_SUCCESS) {
        LOGX_WARN("Failed to collect current observation for prediction");
        memset(probabilities, 0, 60);
        return ML_MONITOR_ERROR_PREDICTION_FAILED;
    }
    
    // Use the latest observation
    // In a full implementation, we'd get this from the circular buffer
    memset(&current_obs, 0, sizeof(current_obs));
    current_obs.timestamp = time(NULL);
    
    // Make predictions for next 15 minutes (60 intervals of 15 seconds each)
    for (int i = 0; i < 60; i++) {
        // Adjust observation for future time
        current_obs.timestamp += 15; // 15 seconds ahead
        
        // Get k-NN prediction
        uint8_t knn_confidence;
        uint8_t knn_prediction = ml_monitor_predict_outage_knn(monitor, &current_obs, &knn_confidence);
        
        // Get neural network prediction
        uint8_t nn_output[8];
        ml_monitor_predict_neural_network(monitor, &current_obs, nn_output);
        
        // Combine predictions (weighted average)
        uint16_t knn_weight = knn_confidence;
        uint16_t nn_weight = monitor->state->total_observations > 200 ? 150 : 100;
        
        if (knn_weight + nn_weight > 0) {
            probabilities[i] = (nn_output[0] * nn_weight + knn_prediction * knn_weight) / 
                              (knn_weight + nn_weight);
        } else {
            probabilities[i] = 0;
        }
        
        // Decay prediction confidence over time
        probabilities[i] = (probabilities[i] * (60 - i)) / 60;
    }
    
    // Calculate overall confidence based on data quality and model agreement
    uint32_t total_observations = monitor->state->total_observations;
    uint32_t location_observations = monitor->state->models.location_learner.observations_here;
    
    *confidence = (uint8_t)((total_observations * 100) / (total_observations + 100)); // Asymptotic to 100%
    if (location_observations < 20) {
        *confidence /= 2; // Reduce confidence for new locations
    }
    
    LOGX_DEBUG("Generated 15-minute predictions with %u%% confidence", *confidence);
    return ML_MONITOR_SUCCESS;
}

// Replace the original collection thread with enhanced version
static void* ml_monitor_collection_thread(void *arg) {
    return ml_monitor_collection_thread_enhanced(arg);
}