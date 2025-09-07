#include "gps_obstruction.h"
#include "../utils/logx.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>

// GPS obstruction configuration
static const int MAX_OBSTRUCTION_RECORDS = 1000;          // Maximum obstruction records
static const int OBSTRUCTION_ANALYSIS_INTERVAL = 30;       // 30 second analysis interval
static const double MIN_SIGNAL_QUALITY = 0.3;              // Minimum signal quality threshold
static const double OBSTRUCTION_DETECTION_THRESHOLD = 0.5; // Obstruction detection threshold
static const int MAX_SATELLITE_OBSTRUCTIONS = 50;          // Maximum satellite obstructions

// Obstruction types
static const char* OBSTRUCTION_TYPE_NAMES[] = {
    "unknown", "building", "tunnel", "mountain", "forest", "urban_canyon",
    "indoor", "underground", "vehicle", "weather", "interference", "multipath"
};

// Global GPS obstruction state
static gps_obstruction_t g_obstruction = {0};
static bool g_obstruction_initialized = false;
static pthread_mutex_t g_obstruction_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations
double calculate_signal_quality(const gps_data_t *gps_data);
static gps_obstruction_type_t detect_obstruction_type(const gps_data_t *gps_data, double signal_quality, double *confidence);
void record_obstruction(gps_obstruction_type_t obstruction_type, double confidence, double signal_quality, const gps_data_t *gps_data);
void analyze_satellite_obstructions(const gps_data_t *gps_data);

// Initialize GPS obstruction analysis
int gps_obstruction_init(void) {
    if (g_obstruction_initialized) {
        LOGX_WARN_MSG("GPS obstruction analysis already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_obstruction_mutex);
    
    // Initialize obstruction state
    memset(&g_obstruction, 0, sizeof(gps_obstruction_t));
    g_obstruction.enabled = true;
    g_obstruction.max_records = MAX_OBSTRUCTION_RECORDS;
    g_obstruction.analysis_interval = OBSTRUCTION_ANALYSIS_INTERVAL;
    g_obstruction.min_signal_quality = MIN_SIGNAL_QUALITY;
    g_obstruction.detection_threshold = OBSTRUCTION_DETECTION_THRESHOLD;
    g_obstruction.max_satellite_obstructions = MAX_SATELLITE_OBSTRUCTIONS;
    
    g_obstruction.record_count = 0;
    g_obstruction.obstruction_detected = false;
    g_obstruction.last_analysis = 0;
    g_obstruction.total_analyses = 0;
    g_obstruction.obstruction_count = 0;
    
    // Initialize obstruction records array
    for (int i = 0; i < MAX_OBSTRUCTION_RECORDS; i++) {
        g_obstruction.obstruction_records[i].timestamp = 0;
        g_obstruction.obstruction_records[i].obstruction_type = GPS_OBSTRUCTION_TYPE_UNKNOWN;
        g_obstruction.obstruction_records[i].confidence = 0.0;
        g_obstruction.obstruction_records[i].signal_quality = 0.0;
        g_obstruction.obstruction_records[i].satellite_count = 0;
        g_obstruction.obstruction_records[i].accuracy = 0.0;
        g_obstruction.obstruction_records[i].gps_lat = 0.0;
        g_obstruction.obstruction_records[i].gps_lon = 0.0;
    }
    
    // Initialize satellite obstruction tracking
    for (int i = 0; i < MAX_SATELLITE_OBSTRUCTIONS; i++) {
        g_obstruction.satellite_obstructions[i].satellite_id = 0;
        g_obstruction.satellite_obstructions[i].obstruction_type = GPS_OBSTRUCTION_TYPE_UNKNOWN;
        g_obstruction.satellite_obstructions[i].confidence = 0.0;
        g_obstruction.satellite_obstructions[i].last_detected = 0;
        g_obstruction.satellite_obstructions[i].detection_count = 0;
    }
    
    g_obstruction_initialized = true;
    pthread_mutex_unlock(&g_obstruction_mutex);
    
    LOGX_INFO_MSG("GPS obstruction analysis initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Analyze GPS data for obstructions
int gps_obstruction_analyze_gps_data(const gps_data_t *gps_data) {
    if (!g_obstruction_initialized || !gps_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_obstruction_mutex);
    
    time_t now = time(NULL);
    
    // Check if enough time has passed since last analysis
    if ((now - g_obstruction.last_analysis) < g_obstruction.analysis_interval) {
        pthread_mutex_unlock(&g_obstruction_mutex);
        return AUTONOMY_SUCCESS;
    }
    
    g_obstruction.last_analysis = now;
    g_obstruction.total_analyses++;
    
    // Perform obstruction analysis
    gps_obstruction_type_t obstruction_type = GPS_OBSTRUCTION_TYPE_UNKNOWN;
    double confidence = 0.0;
    double signal_quality = calculate_signal_quality(gps_data);
    
    // Detect obstructions based on GPS data characteristics
    obstruction_type = detect_obstruction_type(gps_data, signal_quality, &confidence);
    
    // Record obstruction if detected
    if (obstruction_type != GPS_OBSTRUCTION_TYPE_UNKNOWN && confidence > g_obstruction.detection_threshold) {
        record_obstruction(obstruction_type, confidence, signal_quality, gps_data);
        g_obstruction.obstruction_detected = true;
        g_obstruction.obstruction_count++;
        
        LOGX_INFO_MSG("GPS obstruction detected: type=%d, confidence=%.2f, signal_quality=%.2f", 
                   obstruction_type, confidence, signal_quality);
    } else {
        g_obstruction.obstruction_detected = false;
    }
    
    // Analyze satellite-specific obstructions
    analyze_satellite_obstructions(gps_data);
    
    pthread_mutex_unlock(&g_obstruction_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Calculate GPS signal quality
double calculate_signal_quality(const gps_data_t *gps_data) {
    double signal_quality = 1.0;
    
    // Satellite count factor (0-1)
    double satellite_factor = fmin(gps_data->satellites / 12.0, 1.0);
    signal_quality *= satellite_factor;
    
    // Accuracy factor (0-1)
    double accuracy_factor = 1.0;
    if (gps_data->accuracy > 0) {
        accuracy_factor = fmax(0.0, 1.0 - (gps_data->accuracy / 100.0));
    }
    signal_quality *= accuracy_factor;
    
    // Fix quality factor (0-1)
    double fix_quality_factor = 1.0;
    if (gps_data->fix_quality >= 0) {
        fix_quality_factor = fmax(0.0, gps_data->fix_quality / 2.0);
    }
    signal_quality *= fix_quality_factor;
    
    // Age factor (0-1)
    time_t now = time(NULL);
    double age_factor = 1.0;
    if (gps_data->timestamp > 0) {
        double age_seconds = (double)(now - gps_data->timestamp);
        age_factor = fmax(0.0, 1.0 - (age_seconds / 300.0)); // 5 minute decay
    }
    signal_quality *= age_factor;
    
    return fmax(0.0, fmin(1.0, signal_quality));
}

// Detect obstruction type based on GPS data
static gps_obstruction_type_t detect_obstruction_type(const gps_data_t *gps_data, 
                                                     double signal_quality, double *confidence) {
    *confidence = 0.0;
    
    // Check for indoor/underground conditions
    if (gps_data->satellites < 4) {
        *confidence = 0.8;
        return GPS_OBSTRUCTION_TYPE_INDOOR;
    }
    
    // Check for urban canyon effects
    if (gps_data->satellites >= 4 && gps_data->satellites < 8 && 
        gps_data->accuracy > 50.0) {
        *confidence = 0.7;
        return GPS_OBSTRUCTION_TYPE_URBAN_CANYON;
    }
    
    // Check for multipath interference
    if (gps_data->accuracy > 25.0 && gps_data->satellites >= 6) {
        *confidence = 0.6;
        return GPS_OBSTRUCTION_TYPE_MULTIPATH;
    }
    
    // Check for weather-related issues
    if (signal_quality < 0.5 && gps_data->satellites >= 8) {
        *confidence = 0.5;
        return GPS_OBSTRUCTION_TYPE_WEATHER;
    }
    
    // Check for building obstruction
    if (gps_data->accuracy > 100.0 && gps_data->satellites < 6) {
        *confidence = 0.6;
        return GPS_OBSTRUCTION_TYPE_BUILDING;
    }
    
    // Check for tunnel conditions
    if (gps_data->satellites == 0 || gps_data->fix_quality == 0) {
        *confidence = 0.9;
        return GPS_OBSTRUCTION_TYPE_TUNNEL;
    }
    
    // Check for forest obstruction
    if (gps_data->accuracy > 75.0 && gps_data->satellites >= 4 && gps_data->satellites < 8) {
        *confidence = 0.5;
        return GPS_OBSTRUCTION_TYPE_FOREST;
    }
    
    // Check for mountain obstruction
    if (gps_data->accuracy > 150.0 && gps_data->satellites >= 6) {
        *confidence = 0.4;
        return GPS_OBSTRUCTION_TYPE_MOUNTAIN;
    }
    
    // Check for vehicle obstruction
    if (gps_data->accuracy > 50.0 && gps_data->satellites >= 8) {
        *confidence = 0.3;
        return GPS_OBSTRUCTION_TYPE_VEHICLE;
    }
    
    return GPS_OBSTRUCTION_TYPE_UNKNOWN;
}

// Record obstruction
void record_obstruction(gps_obstruction_type_t obstruction_type, double confidence, 
                              double signal_quality, const gps_data_t *gps_data) {
    // Shift obstruction records array
    for (int i = g_obstruction.max_records - 1; i > 0; i--) {
        memcpy(&g_obstruction.obstruction_records[i], &g_obstruction.obstruction_records[i-1], 
               sizeof(gps_obstruction_record_t));
    }
    
    // Add new record
    g_obstruction.obstruction_records[0].timestamp = time(NULL);
    g_obstruction.obstruction_records[0].obstruction_type = obstruction_type;
    g_obstruction.obstruction_records[0].confidence = confidence;
    g_obstruction.obstruction_records[0].signal_quality = signal_quality;
    g_obstruction.obstruction_records[0].satellite_count = gps_data->satellites;
    g_obstruction.obstruction_records[0].accuracy = gps_data->accuracy;
    g_obstruction.obstruction_records[0].gps_lat = gps_data->lat;
    g_obstruction.obstruction_records[0].gps_lon = gps_data->lon;
    
    if (g_obstruction.record_count < g_obstruction.max_records) {
        g_obstruction.record_count++;
    }
}

// Analyze satellite-specific obstructions
void analyze_satellite_obstructions(const gps_data_t *gps_data) {
    // This is a placeholder for satellite-specific obstruction analysis
    // In a full implementation, this would analyze individual satellite signals
    // and detect obstructions affecting specific satellites
    
    LOGX_DEBUG_MSG("Satellite obstruction analysis would be performed here");
}

// Get obstruction analysis status
int gps_obstruction_get_status(gps_obstruction_status_t *status) {
    if (!g_obstruction_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_obstruction_mutex);
    
    status->enabled = g_obstruction.enabled;
    status->obstruction_detected = g_obstruction.obstruction_detected;
    status->last_analysis = g_obstruction.last_analysis;
    status->total_analyses = g_obstruction.total_analyses;
    status->obstruction_count = g_obstruction.obstruction_count;
    
    // Copy recent obstruction records
    int recent_records = 0;
    for (int i = 0; i < g_obstruction.max_records && recent_records < 50; i++) {
        if (g_obstruction.obstruction_records[i].timestamp > 0) {
            memcpy(&status->recent_records[recent_records], &g_obstruction.obstruction_records[i], 
                   sizeof(gps_obstruction_record_t));
            recent_records++;
        }
    }
    status->recent_record_count = recent_records;
    
    pthread_mutex_unlock(&g_obstruction_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get obstruction analysis configuration
int gps_obstruction_get_config(gps_obstruction_config_t *config) {
    if (!g_obstruction_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_obstruction_mutex);
    
    config->enabled = g_obstruction.enabled;
    config->max_records = g_obstruction.max_records;
    config->analysis_interval = g_obstruction.analysis_interval;
    config->min_signal_quality = g_obstruction.min_signal_quality;
    config->detection_threshold = g_obstruction.detection_threshold;
    config->max_satellite_obstructions = g_obstruction.max_satellite_obstructions;
    
    pthread_mutex_unlock(&g_obstruction_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set obstruction analysis configuration
int gps_obstruction_set_config(const gps_obstruction_config_t *config) {
    if (!g_obstruction_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_obstruction_mutex);
    
    g_obstruction.enabled = config->enabled;
    g_obstruction.max_records = config->max_records;
    g_obstruction.analysis_interval = config->analysis_interval;
    g_obstruction.min_signal_quality = config->min_signal_quality;
    g_obstruction.detection_threshold = config->detection_threshold;
    g_obstruction.max_satellite_obstructions = config->max_satellite_obstructions;
    
    pthread_mutex_unlock(&g_obstruction_mutex);
    
    LOGX_INFO_MSG("GPS obstruction analysis configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable obstruction analysis
int gps_obstruction_set_enabled(bool enabled) {
    if (!g_obstruction_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_obstruction_mutex);
    g_obstruction.enabled = enabled;
    pthread_mutex_unlock(&g_obstruction_mutex);
    
    LOGX_INFO_MSG("GPS obstruction analysis %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Force obstruction analysis
int gps_obstruction_force_analysis(void) {
    if (!g_obstruction_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    // Reset last analysis time to force immediate analysis
    pthread_mutex_lock(&g_obstruction_mutex);
    g_obstruction.last_analysis = 0;
    pthread_mutex_unlock(&g_obstruction_mutex);
    
    LOGX_INFO_MSG("GPS obstruction analysis forced");
    return AUTONOMY_SUCCESS;
}

// Get obstruction statistics
int gps_obstruction_get_statistics(gps_obstruction_stats_t *stats) {
    if (!g_obstruction_initialized || !stats) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_obstruction_mutex);
    
    // Calculate statistics from obstruction records
    memset(stats, 0, sizeof(gps_obstruction_stats_t));
    
    for (int i = 0; i < g_obstruction.record_count; i++) {
        gps_obstruction_record_t *record = &g_obstruction.obstruction_records[i];
        
        // Count obstructions by type
        if (record->obstruction_type < GPS_OBSTRUCTION_TYPE_MAX) {
            stats->obstruction_counts[record->obstruction_type]++;
        }
        
        // Calculate average confidence and signal quality
        stats->total_confidence += record->confidence;
        stats->total_signal_quality += record->signal_quality;
        stats->total_accuracy += record->accuracy;
        stats->total_satellites += record->satellite_count;
    }
    
    if (g_obstruction.record_count > 0) {
        stats->average_confidence = stats->total_confidence / g_obstruction.record_count;
        stats->average_signal_quality = stats->total_signal_quality / g_obstruction.record_count;
        stats->average_accuracy = stats->total_accuracy / g_obstruction.record_count;
        stats->average_satellites = (double)stats->total_satellites / g_obstruction.record_count;
    }
    
    stats->total_obstructions = g_obstruction.obstruction_count;
    stats->total_analyses = g_obstruction.total_analyses;
    
    pthread_mutex_unlock(&g_obstruction_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Reset obstruction analysis
int gps_obstruction_reset(void) {
    if (!g_obstruction_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_obstruction_mutex);
    
    g_obstruction.record_count = 0;
    g_obstruction.obstruction_detected = false;
    g_obstruction.last_analysis = 0;
    g_obstruction.total_analyses = 0;
    g_obstruction.obstruction_count = 0;
    
    // Clear all obstruction records
    for (int i = 0; i < MAX_OBSTRUCTION_RECORDS; i++) {
        g_obstruction.obstruction_records[i].timestamp = 0;
        g_obstruction.obstruction_records[i].obstruction_type = GPS_OBSTRUCTION_TYPE_UNKNOWN;
        g_obstruction.obstruction_records[i].confidence = 0.0;
        g_obstruction.obstruction_records[i].signal_quality = 0.0;
        g_obstruction.obstruction_records[i].satellite_count = 0;
        g_obstruction.obstruction_records[i].accuracy = 0.0;
        g_obstruction.obstruction_records[i].gps_lat = 0.0;
        g_obstruction.obstruction_records[i].gps_lon = 0.0;
    }
    
    // Clear satellite obstructions
    for (int i = 0; i < MAX_SATELLITE_OBSTRUCTIONS; i++) {
        g_obstruction.satellite_obstructions[i].satellite_id = 0;
        g_obstruction.satellite_obstructions[i].obstruction_type = GPS_OBSTRUCTION_TYPE_UNKNOWN;
        g_obstruction.satellite_obstructions[i].confidence = 0.0;
        g_obstruction.satellite_obstructions[i].last_detected = 0;
        g_obstruction.satellite_obstructions[i].detection_count = 0;
    }
    
    pthread_mutex_unlock(&g_obstruction_mutex);
    
    LOGX_INFO_MSG("GPS obstruction analysis reset");
    return AUTONOMY_SUCCESS;
}

// Cleanup obstruction analysis
void gps_obstruction_cleanup(void) {
    if (!g_obstruction_initialized) {
        return;
    }
    
    pthread_mutex_destroy(&g_obstruction_mutex);
    g_obstruction_initialized = false;
    
    LOGX_INFO_MSG("GPS obstruction analysis cleaned up");
}
