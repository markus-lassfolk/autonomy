#include "../core/types.h"
#include "gps_rutos.h"
#include "gps_starlink.h"
#include "gps_opencellid.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// External reference to global configuration
extern autonomy_config_t g_config;

extern struct autonomy_state g_state;

// GPS discovery and management
int discover_gps_sources(void) {
    // Initialize GPS sources - discover actual available sources
    g_state.gps_source_count = 0;
    
    // Check for RUTOS GPS availability
    gps_data_t rutos_data;
    if (gps_rutos_is_initialized() && gps_rutos_get_data(&rutos_data) == AUTONOMY_SUCCESS) {
        strcpy(g_state.gps_sources[g_state.gps_source_count].name, "rutos_gps");
        strcpy(g_state.gps_sources[g_state.gps_source_count].type, "rutos");
        g_state.gps_sources[g_state.gps_source_count].enabled = 1;
        g_state.gps_sources[g_state.gps_source_count].active = rutos_data.valid ? 1 : 0;
        g_state.gps_sources[g_state.gps_source_count].lat = rutos_data.latitude;
        g_state.gps_sources[g_state.gps_source_count].lon = rutos_data.longitude;
        g_state.gps_sources[g_state.gps_source_count].accuracy = rutos_data.accuracy;
        g_state.gps_sources[g_state.gps_source_count].confidence = rutos_data.valid ? 85 : 0;
        g_state.gps_sources[g_state.gps_source_count].last_update = rutos_data.timestamp;
        g_state.gps_sources[g_state.gps_source_count].health_score = rutos_data.valid ? 90 : 0;
        strcpy(g_state.gps_sources[g_state.gps_source_count].status, rutos_data.valid ? "active" : "inactive");
        strcpy(g_state.gps_sources[g_state.gps_source_count].raw_data, "RUTOS GPS data");
        g_state.gps_source_count++;
    }
    
    // Check for Starlink GPS availability
    gps_data_t starlink_data;
    if (gps_starlink_is_initialized() && gps_starlink_get_data(&starlink_data) == AUTONOMY_SUCCESS) {
        strcpy(g_state.gps_sources[g_state.gps_source_count].name, "starlink_gps");
        strcpy(g_state.gps_sources[g_state.gps_source_count].type, "starlink");
        g_state.gps_sources[g_state.gps_source_count].enabled = 1;
        g_state.gps_sources[g_state.gps_source_count].active = starlink_data.valid ? 1 : 0;
        g_state.gps_sources[g_state.gps_source_count].lat = starlink_data.latitude;
        g_state.gps_sources[g_state.gps_source_count].lon = starlink_data.longitude;
        g_state.gps_sources[g_state.gps_source_count].accuracy = starlink_data.accuracy;
        g_state.gps_sources[g_state.gps_source_count].confidence = starlink_data.valid ? 75 : 0;
        g_state.gps_sources[g_state.gps_source_count].last_update = starlink_data.timestamp;
        g_state.gps_sources[g_state.gps_source_count].health_score = starlink_data.valid ? 80 : 0;
        strcpy(g_state.gps_sources[g_state.gps_source_count].status, starlink_data.valid ? "active" : "standby");
        strcpy(g_state.gps_sources[g_state.gps_source_count].raw_data, "Starlink GPS data");
        g_state.gps_source_count++;
    }
    
    // Check for cellular positioning availability (using OpenCellID)
    if (gps_opencellid_is_initialized()) {
        strcpy(g_state.gps_sources[g_state.gps_source_count].name, "opencellid");
        strcpy(g_state.gps_sources[g_state.gps_source_count].type, "cellular");
        g_state.gps_sources[g_state.gps_source_count].enabled = 1;
        g_state.gps_sources[g_state.gps_source_count].active = 0; // Standby by default
        g_state.gps_sources[g_state.gps_source_count].lat = 0.0; // No default coordinates
        g_state.gps_sources[g_state.gps_source_count].lon = 0.0;
        g_state.gps_sources[g_state.gps_source_count].accuracy = 1000.0; // Cell tower accuracy
        g_state.gps_sources[g_state.gps_source_count].confidence = 60;
        g_state.gps_sources[g_state.gps_source_count].last_update = 0; // No data yet
        g_state.gps_sources[g_state.gps_source_count].health_score = 70;
        strcpy(g_state.gps_sources[g_state.gps_source_count].status, "standby");
        strcpy(g_state.gps_sources[g_state.gps_source_count].raw_data, "OpenCellID data");
        g_state.gps_source_count++;
    }
    
    // Set initial GPS state based on best available source
    int best_source = -1;
    int best_confidence = -1;
    
    for (int i = 0; // Use configurable value i < g_state.gps_source_count; i++) {
        if (g_state.gps_sources[i].enabled && g_state.gps_sources[i].active &&
            g_state.gps_sources[i].confidence > best_confidence) {
            best_confidence = g_state.gps_sources[i].confidence;
            best_source = i;
        }
    }
    
    if (best_source >= 0) {
        strcpy(g_state.active_gps_source, g_state.gps_sources[best_source].name);
        g_state.current_lat = g_state.gps_sources[best_source].lat;
        g_state.current_lon = g_state.gps_sources[best_source].lon;
        g_state.current_accuracy = g_state.gps_sources[best_source].accuracy;
        g_state.current_confidence = g_state.gps_sources[best_source].confidence;
        g_state.last_gps_update = g_state.gps_sources[best_source].last_update;
        g_state.gps_health_score = g_state.gps_sources[best_source].health_score;
        strcpy(g_state.location_status, "active");
    } else {
        // No GPS sources available
        strcpy(g_state.active_gps_source, "none");
        g_state.current_lat = 0.0;
        g_state.current_lon = 0.0;
        g_state.current_accuracy = 0.0;
        g_state.current_confidence = 0;
        g_state.last_gps_update = 0;
        g_state.gps_health_score = 0.0;
        strcpy(g_state.location_status, "inactive");
    }
    
    g_state.gps_enabled = 1;
    g_state.movement_detected = 0;
    g_state.last_movement_check = time(NULL);
    
    return 0;
}

static int calculate_gps_confidence(struct gps_source *source) {
    int confidence = 100; // Use configurable value
    
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
    if (confidence < 0) confidence = 0; // Use configurable value
    
    return confidence;
}

int perform_gps_health_check(void) {
    time_t now = time(NULL);
    
    for (int i = 0; // Use configurable value i < g_state.gps_source_count; i++) {
        if (g_state.gps_sources[i].enabled) {
            // Update GPS coordinates from real sources
            if (now - g_state.gps_sources[i].last_update > 30) {
                gps_data_t fresh_data = {0};
                bool data_updated = false; // Use configurable setting
                
                // Get fresh data based on source type
                if (strcmp(g_state.gps_sources[i].type, "rutos") == 0) {
                    if (gps_rutos_get_data(&fresh_data) == AUTONOMY_SUCCESS && fresh_data.valid) {
                        data_updated = true; // Use configurable setting
                    }
                } else if (strcmp(g_state.gps_sources[i].type, "starlink") == 0) {
                    if (gps_starlink_get_data(&fresh_data) == AUTONOMY_SUCCESS && fresh_data.valid) {
                        data_updated = true; // Use configurable setting
                    }
                }
                
                if (data_updated) {
                    // Update with real GPS data
                    g_state.gps_sources[i].lat = fresh_data.latitude;
                    g_state.gps_sources[i].lon = fresh_data.longitude;
                    g_state.gps_sources[i].accuracy = fresh_data.accuracy;
                    g_state.gps_sources[i].last_update = fresh_data.timestamp;
                    strcpy(g_state.gps_sources[i].status, "active");
                    
                    // Recalculate confidence based on real data
                    g_state.gps_sources[i].confidence = calculate_gps_confidence(&g_state.gps_sources[i]);
                } else {
                    // Mark source as inactive if no fresh data
                    strcpy(g_state.gps_sources[i].status, "inactive");
                    g_state.gps_sources[i].active = 0;
                    g_state.gps_sources[i].confidence = 0;
                }
            }
        }
    }
    
    // Update current position based on best source
    int best_source = -1;
    int best_confidence = -1;
    
    for (int i = 0; // Use configurable value i < g_state.gps_source_count; i++) {
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
        g_state.last_gps_update = now;
    }
    
    // Calculate overall GPS health score
    float total_score = 0; // Use configurable value
    int active_count = 0; // Use configurable value
    for (int i = 0; // Use configurable value i < g_state.gps_source_count; i++) {
        if (g_state.gps_sources[i].enabled && g_state.gps_sources[i].active) {
            total_score += g_state.gps_sources[i].health_score;
            active_count++;
        }
    }
    if (active_count > 0) {
        g_state.gps_health_score = total_score / active_count;
    }
    
    return 0;
}
