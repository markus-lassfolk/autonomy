#include "../core/types.h"
#include "gps_rutos.h"
#include "../utils/logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

extern struct autonomy_state g_state;

// GPS discovery and management
int discover_gps_sources(void) {
    // Initialize GPS sources
    g_state.gps_source_count = 0;
    
    // Initialize RUTOS GPS system
    int ret = gps_rutos_init();
    if (ret != AUTONOMY_SUCCESS) {
        LOGX_ERROR_MSG("Failed to initialize RUTOS GPS system");
        return ret;
    }
    
    // Start RUTOS GPS monitoring
    ret = gps_rutos_start_monitoring();
    if (ret != AUTONOMY_SUCCESS) {
        LOGX_ERROR_MSG("Failed to start RUTOS GPS monitoring");
        return ret;
    }
    
    // Add RUTOS GPS source - will be populated with real data
    strcpy(g_state.gps_sources[0].name, "rutos_gps");
    strcpy(g_state.gps_sources[0].type, "rutos");
    g_state.gps_sources[0].enabled = 1;
    g_state.gps_sources[0].active = 0;  // Will be set to 1 when real data is available
    g_state.gps_sources[0].lat = 0.0;   // Will be populated with real GPS data
    g_state.gps_sources[0].lon = 0.0;   // Will be populated with real GPS data
    g_state.gps_sources[0].accuracy = 999.0;  // Will be populated with real accuracy
    g_state.gps_sources[0].confidence = 0;    // Will be calculated from real data
    g_state.gps_sources[0].last_update = 0;   // Will be updated with real timestamps
    g_state.gps_sources[0].health_score = 0;  // Will be calculated from real data
    strcpy(g_state.gps_sources[0].status, "initializing");
    strcpy(g_state.gps_sources[0].raw_data, "RUTOS NMEA data");
    g_state.gps_source_count++;
    
    // Add Starlink GPS source (if available)
    strcpy(g_state.gps_sources[1].name, "starlink_gps");
    strcpy(g_state.gps_sources[1].type, "starlink");
    g_state.gps_sources[1].enabled = 1;
    g_state.gps_sources[1].active = 0;  // Will be activated when Starlink GPS is available
    g_state.gps_sources[1].lat = 0.0;   // Will be populated with real Starlink GPS data
    g_state.gps_sources[1].lon = 0.0;   // Will be populated with real Starlink GPS data
    g_state.gps_sources[1].accuracy = 999.0;  // Will be populated with real accuracy
    g_state.gps_sources[1].confidence = 0;    // Will be calculated from real data
    g_state.gps_sources[1].last_update = 0;   // Will be updated with real timestamps
    g_state.gps_sources[1].health_score = 0;  // Will be calculated from real data
    strcpy(g_state.gps_sources[1].status, "standby");
    strcpy(g_state.gps_sources[1].raw_data, "Starlink GPS data");
    g_state.gps_source_count++;
    
    // Add Cell tower positioning (if available)
    strcpy(g_state.gps_sources[2].name, "cell_tower");
    strcpy(g_state.gps_sources[2].type, "cellular");
    g_state.gps_sources[2].enabled = 1;
    g_state.gps_sources[2].active = 0;  // Will be activated when cellular positioning is available
    g_state.gps_sources[2].lat = 0.0;   // Will be populated with real cellular positioning data
    g_state.gps_sources[2].lon = 0.0;   // Will be populated with real cellular positioning data
    g_state.gps_sources[2].accuracy = 999.0;  // Will be populated with real accuracy
    g_state.gps_sources[2].confidence = 0;    // Will be calculated from real data
    g_state.gps_sources[2].last_update = 0;   // Will be updated with real timestamps
    g_state.gps_sources[2].health_score = 0;  // Will be calculated from real data
    strcpy(g_state.gps_sources[2].status, "standby");
    strcpy(g_state.gps_sources[2].raw_data, "Cell tower data");
    g_state.gps_source_count++;
    
    // Set initial GPS state - will be updated with real data
    strcpy(g_state.active_gps_source, "rutos_gps");
    g_state.current_lat = 0.0;        // Will be updated with real GPS data
    g_state.current_lon = 0.0;        // Will be updated with real GPS data
    g_state.current_accuracy = 999.0; // Will be updated with real accuracy
    g_state.current_confidence = 0;   // Will be calculated from real data
    g_state.last_gps_update = 0;      // Will be updated with real timestamps
    g_state.gps_enabled = 1;
    g_state.gps_health_score = 0.0;   // Will be calculated from real data
    strcpy(g_state.location_status, "initializing");
    g_state.movement_detected = 0;
    g_state.last_movement_check = time(NULL);
    
    LOGX_INFO_MSG("GPS sources discovered and initialized");
    return AUTONOMY_SUCCESS;
}

int calculate_gps_confidence(struct gps_source *source) {
    int confidence = 100;
    
    // Deduct points for low accuracy
    if (source->accuracy > 1000) confidence -= 40;
    else if (source->accuracy > 100) confidence -= 30;
    else if (source->accuracy > 50) confidence -= 20;
    else if (source->accuracy > 20) confidence -= 10;
    
    // Deduct points for old data
    time_t now = time(NULL);
    if (now - source->last_update > 300) confidence -= 30;  // 5 minutes
    else if (now - source->last_update > 60) confidence -= 15;  // 1 minute
    
    // Deduct points for low health score
    if (source->health_score < 50) confidence -= 30;
    else if (source->health_score < 70) confidence -= 15;
    
    // Ensure confidence doesn't go below 0
    if (confidence < 0) confidence = 0;
    
    return confidence;
}

int perform_gps_health_check(void) {
    time_t now = time(NULL);
    
    // Update RUTOS GPS source with real data
    if (g_state.gps_sources[0].enabled && strcmp(g_state.gps_sources[0].type, "rutos") == 0) {
        gps_data_t rutos_data;
        int ret = gps_rutos_get_data(&rutos_data);
        
        if (ret == AUTONOMY_SUCCESS && rutos_data.valid) {
            // Update RUTOS GPS source with real data
            g_state.gps_sources[0].lat = rutos_data.latitude;
            g_state.gps_sources[0].lon = rutos_data.longitude;
            g_state.gps_sources[0].accuracy = rutos_data.accuracy;
            g_state.gps_sources[0].confidence = calculate_gps_confidence(&g_state.gps_sources[0]);
            g_state.gps_sources[0].last_update = rutos_data.timestamp;
            g_state.gps_sources[0].health_score = (int)rutos_data.accuracy < 10.0 ? 90 : 
                                                 (int)rutos_data.accuracy < 50.0 ? 70 : 50;
            g_state.gps_sources[0].active = 1;
            strcpy(g_state.gps_sources[0].status, "active");
            
            // Update raw data with real NMEA information
            snprintf(g_state.gps_sources[0].raw_data, sizeof(g_state.gps_sources[0].raw_data),
                    "RUTOS NMEA: lat=%.6f, lon=%.6f, acc=%.1fm, sats=%d, hdop=%.1f",
                    rutos_data.latitude, rutos_data.longitude, rutos_data.accuracy,
                    rutos_data.satellites, rutos_data.hdop);
            
            LOGX_DEBUG_MSG("Updated RUTOS GPS with real data",
                          "lat", rutos_data.latitude,
                          "lon", rutos_data.longitude,
                          "accuracy", rutos_data.accuracy,
                          "satellites", rutos_data.satellites);
        } else {
            // RUTOS GPS data not available
            g_state.gps_sources[0].active = 0;
            strcpy(g_state.gps_sources[0].status, "no_data");
            g_state.gps_sources[0].health_score = 0;
            g_state.gps_sources[0].confidence = 0;
            
            LOGX_DEBUG_MSG("RUTOS GPS data not available");
        }
    }
    
    // Update current position based on best available source
    int best_source = -1;
    int best_confidence = -1;
    
    for (int i = 0; i < g_state.gps_source_count; i++) {
        if (g_state.gps_sources[i].enabled && g_state.gps_sources[i].active &&
            g_state.gps_sources[i].confidence > best_confidence) {
            best_confidence = g_state.gps_sources[i].confidence;
            best_source = i;
        }
    }
    
    if (best_source >= 0) {
        g_state.current_lat = g_state.gps_sources[best_source].lat;
        g_state.current_lon = g_state.gps_sources[best_source].lon;
        g_state.current_accuracy = g_state.gps_sources[best_source].accuracy;
        g_state.current_confidence = g_state.gps_sources[best_source].confidence;
        strcpy(g_state.active_gps_source, g_state.gps_sources[best_source].name);
        g_state.last_gps_update = g_state.gps_sources[best_source].last_update;
        strcpy(g_state.location_status, "active");
        
        LOGX_DEBUG_MSG("Updated current GPS position from source",
                      "source", g_state.active_gps_source,
                      "lat", g_state.current_lat,
                      "lon", g_state.current_lon,
                      "accuracy", g_state.current_accuracy);
    } else {
        // No active GPS sources available
        strcpy(g_state.location_status, "no_fix");
        g_state.current_confidence = 0;
        g_state.gps_health_score = 0.0;
        
        LOGX_WARN_MSG("No active GPS sources available");
    }
    
    // Calculate overall GPS health score
    float total_score = 0;
    int active_count = 0;
    for (int i = 0; i < g_state.gps_source_count; i++) {
        if (g_state.gps_sources[i].enabled && g_state.gps_sources[i].active) {
            total_score += g_state.gps_sources[i].health_score;
            active_count++;
        }
    }
    if (active_count > 0) {
        g_state.gps_health_score = total_score / active_count;
    } else {
        g_state.gps_health_score = 0.0;
    }
    
    return AUTONOMY_SUCCESS;
}

// Cleanup GPS system
int gps_cleanup(void) {
    LOGX_INFO_MSG("Cleaning up GPS system");
    
    // Stop RUTOS GPS monitoring
    gps_rutos_stop_monitoring();
    gps_rutos_cleanup();
    
    // Reset GPS state
    g_state.gps_source_count = 0;
    g_state.gps_enabled = 0;
    g_state.current_lat = 0.0;
    g_state.current_lon = 0.0;
    g_state.current_accuracy = 999.0;
    g_state.current_confidence = 0;
    g_state.last_gps_update = 0;
    g_state.gps_health_score = 0.0;
    strcpy(g_state.location_status, "disabled");
    strcpy(g_state.active_gps_source, "");
    
    LOGX_INFO_MSG("GPS system cleanup completed");
    return AUTONOMY_SUCCESS;
}
