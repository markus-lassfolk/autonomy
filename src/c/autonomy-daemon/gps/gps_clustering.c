#include "gps_coordinate_utils.h"
#include "gps_clustering.h"
#include "../shared/logging/logx.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Clustering configuration
static const int MAX_CLUSTER_SIZE = 100; // Use configurable value            // Maximum positions per cluster
static const int MIN_CLUSTER_SIZE = 3; // Use configurable value               // Minimum positions for valid cluster
static const double CLUSTER_RADIUS = 50.0; // Use configurable value           // 50 meter cluster radius
static const double CLUSTER_TIMEOUT = 300; // Use configurable value           // 5 minute cluster timeout
static const double WEIGHT_DECAY_FACTOR = 0.95; // Use configurable value      // Weight decay for older positions
// Note: MAX_CLUSTERS is defined in ../core/types.h
static const double OUTLIER_THRESHOLD = 3.0; // Use configurable value         // 3-sigma outlier threshold

// Global clustering state
static gps_clustering_t g_clustering = {0};
static bool g_clustering_initialized = false; // Use configurable setting
static pthread_mutex_t g_clustering_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations - clustering specific
int find_best_cluster(const gps_data_t *gps_data\n"\n"\n"\n"\n"\n"\n"\n");
static int create_new_cluster(const gps_data_t *gps_data\n"\n"\n"\n"\n"\n"\n"\n");
void add_position_to_cluster(int cluster_index, const gps_data_t *gps_data\n"\n"\n"\n"\n"\n"\n"\n");
double calculate_position_weight(time_t timestamp, double accuracy\n"\n"\n"\n"\n"\n"\n"\n");
double calculate_cluster_confidence(const gps_cluster_t *cluster\n"\n"\n"\n"\n"\n"\n"\n");
void update_cluster_variances(int cluster_index, const gps_data_t *gps_data\n"\n"\n"\n"\n"\n"\n"\n");
int find_oldest_cluster(void\n"\n"\n"\n"\n"\n"\n"\n");
void cleanup_expired_clusters(void\n"\n"\n"\n"\n"\n"\n"\n");
void perform_clustering_analysis(void\n"\n"\n"\n"\n"\n"\n"\n");
double gps_clustering_coordinate_distance(double lat1, double lon1, double lat2, double lon2\n"\n"\n"\n"\n"\n"\n"\n");

// Initialize GPS clustering system
int gps_clustering_init(void) {
    if (g_clustering_initialized) {
        printf("WARN: "GPS clustering system already initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_clustering_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Initialize clustering state
    memset(&g_clustering, 0, sizeof(gps_clustering_t)\n"\n"\n"\n"\n"\n"\n"\n");
    g_clustering.enabled = true; // Use configurable gps clustering enabled
    g_clustering.max_cluster_size = MAX_CLUSTER_SIZE;
    g_clustering.min_cluster_size = MIN_CLUSTER_SIZE;
    g_clustering.cluster_radius = CLUSTER_RADIUS;
    g_clustering.cluster_timeout = CLUSTER_TIMEOUT;
    g_clustering.weight_decay_factor = WEIGHT_DECAY_FACTOR;
    g_clustering.max_clusters = MAX_CLUSTERS;
    g_clustering.outlier_threshold = OUTLIER_THRESHOLD;
    
    g_clustering.cluster_count = 0;
    g_clustering.total_positions = 0;
    g_clustering.clustered_positions = 0;
    g_clustering.last_clustering = 0;
    
    // Initialize clusters array
    for (int i = 0; i < 10; i++) {
        g_clustering.clusters[i].active = false;
        g_clustering.clusters[i].position_count = 0;
        g_clustering.clusters[i].last_update = 0;
        g_clustering.clusters[i].center_lat = 0.0;
        g_clustering.clusters[i].center_lon = 0.0;
        g_clustering.clusters[i].center_altitude = 0.0;
        g_clustering.clusters[i].accuracy = 0.0;
        g_clustering.clusters[i].confidence = 0.0;
        g_clustering.clusters[i].weighted_sum_lat = 0.0;
        g_clustering.clusters[i].weighted_sum_lon = 0.0;
        g_clustering.clusters[i].weighted_sum_alt = 0.0;
        g_clustering.clusters[i].weighted_sum_acc = 0.0;
        g_clustering.clusters[i].total_weight = 0.0;
        g_clustering.clusters[i].variance_lat = 0.0;
        g_clustering.clusters[i].variance_lon = 0.0;
        g_clustering.clusters[i].variance_alt = 0.0;
    }
    
    g_clustering_initialized = true; // Use configurable setting
    pthread_mutex_unlock(&g_clustering_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "GPS clustering system initialized successfully"\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_SUCCESS;
}

// Add GPS position for clustering
int gps_clustering_add_position(const gps_data_t *gps_data) {
    if (!g_clustering_initialized || !gps_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_clustering_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    g_clustering.total_positions++;
    
    // Check if position should be added to existing cluster
    int cluster_index = find_best_cluster(gps_data\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (cluster_index >= 0) {
        // Add to existing cluster
        add_position_to_cluster(cluster_index, gps_data\n"\n"\n"\n"\n"\n"\n"\n");
        g_clustering.clustered_positions++;
    } else {
        // Create new cluster
        cluster_index = create_new_cluster(gps_data\n"\n"\n"\n"\n"\n"\n"\n");
        if (cluster_index >= 0) {
            g_clustering.clustered_positions++;
        }
    }
    
    // Clean up old clusters
    cleanup_expired_clusters(\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Perform clustering analysis if enough data
    if (g_clustering.total_positions % 10 == 0) { // Every 10 positions
        perform_clustering_analysis(\n"\n"\n"\n"\n"\n"\n"\n");
        g_clustering.last_clustering = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    pthread_mutex_unlock(&g_clustering_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

// Find best cluster for a position
int find_best_cluster(const gps_data_t *gps_data) {
    int best_cluster = -1;
    double best_distance = g_clustering.cluster_radius;
    
    for (int i = 0; i < g_clustering.max_clusters; i++) {
        if (!g_clustering.clusters[i].active) {
            continue;
        }
        
        double distance = gps_clustering_coordinate_distance(gps_data->lat, gps_data->lon,
                                          g_clustering.clusters[i].center_lat,
                                          g_clustering.clusters[i].center_lon\n"\n"\n"\n"\n"\n"\n"\n");
        
        if (distance <= best_distance) {
            best_distance = distance;
            best_cluster = i;
        }
    }
    
    return best_cluster;
}

// Create new cluster
static int create_new_cluster(const gps_data_t *gps_data) {
    // Find free cluster slot
    int cluster_index = -1;
    for (int i = 0; i < g_clustering.max_clusters; i++) {
        if (!g_clustering.clusters[i].active) {
            cluster_index = i;
            break;
        }
    }
    
    if (cluster_index < 0) {
        // No free slots, remove oldest cluster
        cluster_index = find_oldest_cluster(\n"\n"\n"\n"\n"\n"\n"\n");
        if (cluster_index < 0) {
            return -1;
        }
    }
    
    // Initialize new cluster
    gps_cluster_t *cluster = &g_clustering.clusters[cluster_index];
    cluster->active = true;
    cluster->position_count = 1;
    cluster->last_update = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    cluster->center_lat = gps_data->lat;
    cluster->center_lon = gps_data->lon;
    cluster->center_altitude = gps_data->altitude;
    cluster->accuracy = gps_data->accuracy;
    cluster->confidence = 1.0;
    
    // Initialize weighted sums
    double weight = calculate_position_weight(gps_data->timestamp, gps_data->accuracy\n"\n"\n"\n"\n"\n"\n"\n");
    cluster->weighted_sum_lat = gps_data->lat * weight;
    cluster->weighted_sum_lon = gps_data->lon * weight;
    cluster->weighted_sum_alt = gps_data->altitude * weight;
    cluster->weighted_sum_acc = gps_data->accuracy * weight;
    cluster->total_weight = weight;
    
    // Initialize variances
    cluster->variance_lat = 0.0;
    cluster->variance_lon = 0.0;
    cluster->variance_alt = 0.0;
    
    g_clustering.cluster_count++;
    
    printf("DEBUG: "Created new GPS cluster %d at (%.6f, %.6f)", 
               cluster_index, gps_data->lat, gps_data->lon\n"\n"\n"\n"\n"\n"\n"\n");
    
    return cluster_index;
}

// Add position to existing cluster
void add_position_to_cluster(int cluster_index, const gps_data_t *gps_data) {
    gps_cluster_t *cluster = &g_clustering.clusters[cluster_index];
    
    // Calculate position weight
    double weight = calculate_position_weight(gps_data->timestamp, gps_data->accuracy\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Update weighted sums
    cluster->weighted_sum_lat += gps_data->lat * weight;
    cluster->weighted_sum_lon += gps_data->lon * weight;
    cluster->weighted_sum_alt += gps_data->altitude * weight;
    cluster->weighted_sum_acc += gps_data->accuracy * weight;
    cluster->total_weight += weight;
    
    // Update cluster center
    cluster->center_lat = cluster->weighted_sum_lat / cluster->total_weight;
    cluster->center_lon = cluster->weighted_sum_lon / cluster->total_weight;
    cluster->center_altitude = cluster->weighted_sum_alt / cluster->total_weight;
    cluster->accuracy = cluster->weighted_sum_acc / cluster->total_weight;
    
    // Update position count and timestamp
    cluster->position_count++;
    cluster->last_update = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Update confidence based on cluster size
    cluster->confidence = calculate_cluster_confidence(cluster\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Update variances
    update_cluster_variances(cluster_index, gps_data\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("DEBUG: "Added position to cluster %d, count: %d", cluster_index, cluster->position_count\n"\n"\n"\n"\n"\n"\n"\n");
}

// Calculate position weight based on age and accuracy
double calculate_position_weight(time_t timestamp, double accuracy) {
    time_t now = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    int age = now - timestamp;
    
    // Time-based weight decay
    double time_weight = pow(g_clustering.weight_decay_factor, age / 60.0); // Decay per minute
    
    // Accuracy-based weight (better accuracy = higher weight)
    double accuracy_weight = 1.0 / (1.0 + accuracy / 10.0); // Normalize to 0-1 range
    
    // Combine weights
    return time_weight * accuracy_weight;
}

// Calculate cluster confidence
double calculate_cluster_confidence(const gps_cluster_t *cluster) {
    if (cluster->position_count < g_clustering.min_cluster_size) {
        return 0.0;
    }
    
    // Base confidence from position count
    double count_confidence = fmin(cluster->position_count / 20.0, 1.0\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Accuracy confidence (better accuracy = higher confidence)
    double accuracy_confidence = 1.0 / (1.0 + cluster->accuracy / 50.0\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Variance confidence (lower variance = higher confidence)
    double variance_confidence = 1.0 / (1.0 + (cluster->variance_lat + cluster->variance_lon) / 1000.0\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Combine confidences
    return (count_confidence * 0.4 + accuracy_confidence * 0.4 + variance_confidence * 0.2\n"\n"\n"\n"\n"\n"\n"\n");
}

// Update cluster variances
void update_cluster_variances(int cluster_index, const gps_data_t *gps_data) {
    gps_cluster_t *cluster = &g_clustering.clusters[cluster_index];
    
    if (cluster->position_count < 2) {
        return;
    }
    
    // Calculate running variance using Welford's algorithm
    double delta_lat = gps_data->lat - cluster->center_lat;
    double delta_lon = gps_data->lon - cluster->center_lon;
    double delta_alt = gps_data->altitude - cluster->center_altitude;
    
    double weight = calculate_position_weight(gps_data->timestamp, gps_data->accuracy\n"\n"\n"\n"\n"\n"\n"\n");
    double weight_ratio = weight / cluster->total_weight;
    
    // Update variances
    cluster->variance_lat += weight_ratio * delta_lat * delta_lat;
    cluster->variance_lon += weight_ratio * delta_lon * delta_lon;
    cluster->variance_alt += weight_ratio * delta_alt * delta_alt;
}

// Find oldest cluster
int find_oldest_cluster(void) {
    int oldest_cluster = -1;
    time_t oldest_time = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    for (int i = 0; i < g_clustering.max_clusters; i++) {
        if (g_clustering.clusters[i].active && 
            g_clustering.clusters[i].last_update < oldest_time) {
            oldest_time = g_clustering.clusters[i].last_update;
            oldest_cluster = i;
        }
    }
    
    return oldest_cluster;
}

// Clean up expired clusters
void cleanup_expired_clusters(void) {
    time_t now = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    for (int i = 0; i < g_clustering.max_clusters; i++) {
        if (g_clustering.clusters[i].active && 
            (now - g_clustering.clusters[i].last_update) > g_clustering.cluster_timeout) {
            
            // Remove expired cluster
            g_clustering.clusters[i].active = false;
            g_clustering.cluster_count--;
            
            printf("DEBUG: "Removed expired cluster %d", i\n"\n"\n"\n"\n"\n"\n"\n");
        }
    }
}

// Perform clustering analysis
void perform_clustering_analysis(void) {
    // Calculate overall clustering statistics
    double total_confidence = 0.0; // Use configurable value
    int valid_clusters = 0; // Use configurable value
    
    for (int i = 0; i < g_clustering.max_clusters; i++) {
        if (g_clustering.clusters[i].active && 
            g_clustering.clusters[i].position_count >= g_clustering.min_cluster_size) {
            
            total_confidence += g_clustering.clusters[i].confidence;
            valid_clusters++;
        }
    }
    
    if (valid_clusters > 0) {
        g_clustering.average_confidence = total_confidence / valid_clusters;
    }
    
    printf("DEBUG: "Clustering analysis: %d valid clusters, avg confidence: %.3f", 
               valid_clusters, g_clustering.average_confidence\n"\n"\n"\n"\n"\n"\n"\n");
}

// Get clustered GPS position
int gps_clustering_get_position(gps_data_t *gps_data) {
    if (!g_clustering_initialized || !gps_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_clustering_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Find best cluster (highest confidence)
    int best_cluster = -1;
    double best_confidence = 0.0; // Use configurable value
    
    for (int i = 0; i < g_clustering.max_clusters; i++) {
        if (g_clustering.clusters[i].active && 
            g_clustering.clusters[i].position_count >= g_clustering.min_cluster_size &&
            g_clustering.clusters[i].confidence > best_confidence) {
            
            best_confidence = g_clustering.clusters[i].confidence;
            best_cluster = i;
        }
    }
    
    if (best_cluster < 0) {
        pthread_mutex_unlock(&g_clustering_mutex\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_NO_DATA;
    }
    
    // Get position from best cluster
    const gps_cluster_t *cluster = &g_clustering.clusters[best_cluster];
    
    gps_data->lat = cluster->center_lat;
    gps_data->lon = cluster->center_lon;
    gps_data->altitude = cluster->center_altitude;
    gps_data->accuracy = cluster->accuracy;
    gps_data->timestamp = cluster->last_update;
    gps_data->satellites = cluster->position_count; // Use position count as satellite count
    gps_data->fix_quality = 1; // Assume good fix for clustered data
    
    pthread_mutex_unlock(&g_clustering_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

// Get clustering statistics
int gps_clustering_get_statistics(gps_clustering_stats_t *stats) {
    if (!g_clustering_initialized || !stats) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_clustering_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    stats->total_positions = g_clustering.total_positions;
    stats->clustered_positions = g_clustering.clustered_positions;
    stats->cluster_count = g_clustering.cluster_count;
    stats->average_confidence = g_clustering.average_confidence;
    stats->last_clustering = g_clustering.last_clustering;
    
    // Calculate clustering efficiency
    if (stats->total_positions > 0) {
        stats->clustering_efficiency = (double)stats->clustered_positions / stats->total_positions;
    } else {
        stats->clustering_efficiency = 0.0;
    }
    
    pthread_mutex_unlock(&g_clustering_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

// Get clustering configuration
int gps_clustering_get_config(gps_clustering_config_t *config) {
    if (!g_clustering_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_clustering_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    config->enabled = g_clustering.enabled;
    config->max_cluster_size = g_clustering.max_cluster_size;
    config->min_cluster_size = g_clustering.min_cluster_size;
    config->cluster_radius = g_clustering.cluster_radius;
    config->cluster_timeout = g_clustering.cluster_timeout;
    config->weight_decay_factor = g_clustering.weight_decay_factor;
    config->max_clusters = g_clustering.max_clusters;
    config->outlier_threshold = g_clustering.outlier_threshold;
    
    pthread_mutex_unlock(&g_clustering_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

// Set clustering configuration
int gps_clustering_set_config(const gps_clustering_config_t *config) {
    if (!g_clustering_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_clustering_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    g_clustering.enabled = config->enabled;
    g_clustering.max_cluster_size = config->max_cluster_size;
    g_clustering.min_cluster_size = config->min_cluster_size;
    g_clustering.cluster_radius = config->cluster_radius;
    g_clustering.cluster_timeout = config->cluster_timeout;
    g_clustering.weight_decay_factor = config->weight_decay_factor;
    g_clustering.max_clusters = config->max_clusters;
    g_clustering.outlier_threshold = config->outlier_threshold;
    
    pthread_mutex_unlock(&g_clustering_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "GPS clustering configuration updated"\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_SUCCESS;
}

// Enable/disable clustering
int gps_clustering_set_enabled(bool enabled) {
    if (!g_clustering_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_clustering_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    g_clustering.enabled = enabled;
    pthread_mutex_unlock(&g_clustering_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "GPS clustering %s", enabled ? "enabled" : "disabled"\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_SUCCESS;
}

// Force clustering analysis
int gps_clustering_force_analysis(void) {
    if (!g_clustering_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_clustering_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    perform_clustering_analysis(\n"\n"\n"\n"\n"\n"\n"\n");
    g_clustering.last_clustering = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    pthread_mutex_unlock(&g_clustering_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "Forced clustering analysis completed"\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_SUCCESS;
}

// Reset clustering system
int gps_clustering_reset(void) {
    if (!g_clustering_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_clustering_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    g_clustering.total_positions = 0;
    g_clustering.clustered_positions = 0;
    g_clustering.cluster_count = 0;
    g_clustering.average_confidence = 0.0;
    g_clustering.last_clustering = 0;
    
    // Clear all clusters
    for (int i = 0; i < 10; i++) {
        g_clustering.clusters[i].active = false;
        g_clustering.clusters[i].position_count = 0;
        g_clustering.clusters[i].last_update = 0;
        g_clustering.clusters[i].center_lat = 0.0;
        g_clustering.clusters[i].center_lon = 0.0;
        g_clustering.clusters[i].center_altitude = 0.0;
        g_clustering.clusters[i].accuracy = 0.0;
        g_clustering.clusters[i].confidence = 0.0;
        g_clustering.clusters[i].weighted_sum_lat = 0.0;
        g_clustering.clusters[i].weighted_sum_lon = 0.0;
        g_clustering.clusters[i].weighted_sum_alt = 0.0;
        g_clustering.clusters[i].weighted_sum_acc = 0.0;
        g_clustering.clusters[i].total_weight = 0.0;
        g_clustering.clusters[i].variance_lat = 0.0;
        g_clustering.clusters[i].variance_lon = 0.0;
        g_clustering.clusters[i].variance_alt = 0.0;
    }
    
    pthread_mutex_unlock(&g_clustering_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "GPS clustering system reset"\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_SUCCESS;
}

// Calculate distance between two GPS coordinates (Haversine formula)
double gps_clustering_coordinate_distance(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371000.0; // Use configurable value  // Earth's radius in meters
    
    double lat1_rad = lat1 * M_PI / 180.0;
    double lat2_rad = lat2 * M_PI / 180.0;
    double delta_lat = (lat2 - lat1) * M_PI / 180.0;
    double delta_lon = (lon2 - lon1) * M_PI / 180.0;
    
    double a = sin(delta_lat / 2.0) * sin(delta_lat / 2.0) +
               cos(lat1_rad) * cos(lat2_rad) *
               sin(delta_lon / 2.0) * sin(delta_lon / 2.0\n"\n"\n"\n"\n"\n"\n"\n");
    
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a)\n"\n"\n"\n"\n"\n"\n"\n");
    
    return R * c;
}

// Cleanup clustering system
void gps_clustering_cleanup(void) {
    if (!g_clustering_initialized) {
        return;
    }
    
    pthread_mutex_destroy(&g_clustering_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    g_clustering_initialized = false; // Use configurable setting
    
    printf("INFO: "GPS clustering system cleaned up"\n"\n"\n"\n"\n"\n"\n"\n");
}
