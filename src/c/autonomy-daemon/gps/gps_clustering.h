#ifndef GPS_CLUSTERING_H
#define GPS_CLUSTERING_H

#include "types.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// GPS cluster structure
typedef struct {
    bool active;                        // Whether cluster is active
    int position_count;                 // Number of positions in cluster
    time_t last_update;                 // Last update timestamp
    double center_lat;                  // Cluster center latitude
    double center_lon;                  // Cluster center longitude
    double center_altitude;             // Cluster center altitude
    double accuracy;                    // Average accuracy
    double confidence;                  // Cluster confidence (0.0-1.0)
    
    // Weighted sums for center calculation
    double weighted_sum_lat;            // Weighted sum of latitudes
    double weighted_sum_lon;            // Weighted sum of longitudes
    double weighted_sum_alt;            // Weighted sum of altitudes
    double weighted_sum_acc;            // Weighted sum of accuracies
    double total_weight;                // Total weight of all positions
    
    // Statistical variances
    double variance_lat;                // Latitude variance
    double variance_lon;                // Longitude variance
    double variance_alt;                // Altitude variance
} gps_cluster_t;

// Clustering configuration
typedef struct {
    bool enabled;                       // Enable/disable clustering
    int max_cluster_size;               // Maximum positions per cluster
    int min_cluster_size;               // Minimum positions for valid cluster
    double cluster_radius;              // Cluster radius in meters
    int cluster_timeout;                // Cluster timeout in seconds
    double weight_decay_factor;         // Weight decay factor for older positions
    int max_clusters;                   // Maximum number of active clusters
    double outlier_threshold;           // Outlier detection threshold
} gps_clustering_config_t;

// Clustering statistics
typedef struct {
    int total_positions;                // Total positions processed
    int clustered_positions;            // Positions successfully clustered
    int cluster_count;                  // Number of active clusters
    double average_confidence;          // Average cluster confidence
    double clustering_efficiency;       // Clustering efficiency (0.0-1.0)
    time_t last_clustering;             // Last clustering analysis timestamp
} gps_clustering_stats_t;

// Clustering system state
typedef struct {
    bool enabled;                       // Clustering enabled
    int max_cluster_size;               // Maximum cluster size
    int min_cluster_size;               // Minimum cluster size
    double cluster_radius;              // Cluster radius
    int cluster_timeout;                // Cluster timeout
    double weight_decay_factor;         // Weight decay factor
    int max_clusters;                   // Maximum clusters
    double outlier_threshold;           // Outlier threshold
    
    // State
    int cluster_count;                  // Active cluster count
    int total_positions;                // Total positions
    int clustered_positions;            // Clustered positions
    double average_confidence;          // Average confidence
    time_t last_clustering;             // Last clustering
    
    // Clusters array
    gps_cluster_t clusters[10];         // Active clusters
} gps_clustering_t;

// Function prototypes

/**
 * Initialize GPS clustering system
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_clustering_init(void);

/**
 * Add GPS position for clustering
 * @param gps_data GPS data to cluster
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_clustering_add_position(const gps_data_t *gps_data);

/**
 * Get clustered GPS position
 * @param gps_data GPS data structure to populate with clustered position
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_clustering_get_position(gps_data_t *gps_data);

/**
 * Get clustering statistics
 * @param stats Statistics structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_clustering_get_statistics(gps_clustering_stats_t *stats);

/**
 * Get clustering configuration
 * @param config Configuration structure to populate
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_clustering_get_config(gps_clustering_config_t *config);

/**
 * Set clustering configuration
 * @param config Configuration to apply
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_clustering_set_config(const gps_clustering_config_t *config);

/**
 * Enable/disable clustering
 * @param enabled True to enable, false to disable
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_clustering_set_enabled(bool enabled);

/**
 * Force clustering analysis
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_clustering_force_analysis(void);

/**
 * Reset clustering system
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int gps_clustering_reset(void);

/**
 * Cleanup clustering system
 */
void gps_clustering_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_CLUSTERING_H
