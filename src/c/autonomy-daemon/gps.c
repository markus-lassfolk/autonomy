#include "autonomy_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern struct autonomy_state g_state;

// GPS discovery and management
int discover_gps_sources(void) {
    // Initialize GPS sources
    g_state.gps_source_count = 0;
    
    // Add RUTOS GPS source
    strcpy(g_state.gps_sources[0].name, "rutos_gps");
    strcpy(g_state.gps_sources[0].type, "rutos");
    g_state.gps_sources[0].enabled = 1;
    g_state.gps_sources[0].active = 1;
    g_state.gps_sources[0].lat = 59.3293;  // Default Stockholm coordinates
    g_state.gps_sources[0].lon = 18.0686;
    g_state.gps_sources[0].accuracy = 10.0;
    g_state.gps_sources[0].confidence = 85;
    g_state.gps_sources[0].last_update = time(NULL);
    g_state.gps_sources[0].health_score = 90;
    strcpy(g_state.gps_sources[0].status, "active");
    strcpy(g_state.gps_sources[0].raw_data, "NMEA data simulation");
    g_state.gps_source_count++;
    
    // Add Starlink GPS source
    strcpy(g_state.gps_sources[1].name, "starlink_gps");
    strcpy(g_state.gps_sources[1].type, "starlink");
    g_state.gps_sources[1].enabled = 1;
    g_state.gps_sources[1].active = 0;
    g_state.gps_sources[1].lat = 59.3293;
    g_state.gps_sources[1].lon = 18.0686;
    g_state.gps_sources[1].accuracy = 15.0;
    g_state.gps_sources[1].confidence = 75;
    g_state.gps_sources[1].last_update = time(NULL);
    g_state.gps_sources[1].health_score = 80;
    strcpy(g_state.gps_sources[1].status, "standby");
    strcpy(g_state.gps_sources[1].raw_data, "Starlink GPS data");
    g_state.gps_source_count++;
    
    // Add Cell tower positioning
    strcpy(g_state.gps_sources[2].name, "cell_tower");
    strcpy(g_state.gps_sources[2].type, "cellular");
    g_state.gps_sources[2].enabled = 1;
    g_state.gps_sources[2].active = 0;
    g_state.gps_sources[2].lat = 59.3293;
    g_state.gps_sources[2].lon = 18.0686;
    g_state.gps_sources[2].accuracy = 1000.0;
    g_state.gps_sources[2].confidence = 60;
    g_state.gps_sources[2].last_update = time(NULL);
    g_state.gps_sources[2].health_score = 70;
    strcpy(g_state.gps_sources[2].status, "standby");
    strcpy(g_state.gps_sources[2].raw_data, "Cell tower data");
    g_state.gps_source_count++;
    
    // Set initial GPS state
    strcpy(g_state.active_gps_source, "rutos_gps");
    g_state.current_lat = 59.3293;
    g_state.current_lon = 18.0686;
    g_state.current_accuracy = 10.0;
    g_state.current_confidence = 85;
    g_state.last_gps_update = time(NULL);
    g_state.gps_enabled = 1;
    g_state.gps_health_score = 85.0;
    strcpy(g_state.location_status, "active");
    g_state.movement_detected = 0;
    g_state.last_movement_check = time(NULL);
    
    return 0;
}

static int calculate_gps_confidence(struct gps_source *source) {
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
    
    for (int i = 0; i < g_state.gps_source_count; i++) {
        if (g_state.gps_sources[i].enabled) {
            // Update GPS coordinates (simulate movement)
            if (now - g_state.gps_sources[i].last_update > 30) {
                // Simulate small coordinate changes
                g_state.gps_sources[i].lat += (rand() % 100 - 50) * 0.0001;
                g_state.gps_sources[i].lon += (rand() % 100 - 50) * 0.0001;
                
                // Update accuracy (simulate variation)
                g_state.gps_sources[i].accuracy += (rand() % 20 - 10) * 0.1;
                if (g_state.gps_sources[i].accuracy < 1.0) g_state.gps_sources[i].accuracy = 1.0;
                
                // Recalculate confidence
                g_state.gps_sources[i].confidence = calculate_gps_confidence(&g_state.gps_sources[i]);
                g_state.gps_sources[i].last_update = now;
            }
        }
    }
    
    // Update current position based on best source
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
        g_state.last_gps_update = now;
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
    }
    
    return 0;
}
