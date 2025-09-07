#ifndef TELEMETRY_COMPREHENSIVE_H
#define TELEMETRY_COMPREHENSIVE_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <sqlite3.h>
#include <math.h>
#include <fcntl.h>
#include <sys/socket.h>
#include "telemetry_store.h"

#ifdef __cplusplus
extern "C" {
#endif

// Note: telemetry_sample_t is now defined in telemetry_store.h

// Failover/Failback decision record
typedef struct {
    uint64_t id;                           // Unique decision ID
    time_t timestamp;                      // Decision timestamp
    char decision_id[64];                  // Human-readable decision ID
    char decision_type[32];                // failover, failback, recheck, emergency
    char trigger[128];                     // What triggered the decision
    char reasoning[512];                   // Decision reasoning
    double confidence;                     // Decision confidence (0.0-1.0)
    
    // Interface transition
    char from_interface[32];               // Source interface
    char to_interface[32];                 // Target interface
    char from_member[64];                  // Source member name
    char to_member[64];                    // Target member name
    
    // GPS context (optimized with location reference)
    uint32_t location_reference_id;        // Reference to GPS location table (0 = no GPS)
    double gps_accuracy;                   // GPS accuracy at decision time
    char gps_source[32];                   // GPS source
    double gps_latitude;                   // GPS latitude at decision time
    double gps_longitude;                  // GPS longitude at decision time
    
    // Performance context
    double from_score;                     // Source interface score
    double to_score;                       // Target interface score
    double score_difference;               // Score difference
    double from_latency;                   // Source latency
    double from_loss;                      // Source packet loss
    double to_latency;                     // Target latency
    double to_loss;                        // Target packet loss
    
    // Decision execution
    bool success;                          // Whether decision was successful
    double execution_time_ms;              // Execution time in milliseconds
    char error_message[256];               // Error message if failed
    char root_cause[256];                  // Root cause analysis
    
    // Context and recommendations
    char context_json[1024];               // Additional context as JSON
    char recommendations[512];             // Recommended actions
    bool predictive_decision;              // Whether this was a predictive decision
    double prediction_confidence;          // Prediction confidence if applicable
    
    // Validation
    time_t validation_time;                // When decision was validated
    bool validation_successful;            // Whether validation passed
    char validation_notes[256];            // Validation notes
} decision_record_t;

// Telemetry collection configuration
typedef struct {
    bool enabled;                          // Enable telemetry collection
    int collection_interval_s;             // Collection interval in seconds
    int retention_hours;                   // Data retention in hours
    int max_ram_mb;                        // Maximum RAM usage in MB
    
    // GPS integration
    bool require_gps_for_collection;       // Require GPS for telemetry
    double min_gps_accuracy;               // Minimum GPS accuracy for collection
    bool collect_movement_data;            // Collect movement/speed data
    
    // Collection scope
    bool collect_network_metrics;          // Collect network performance metrics
    bool collect_starlink_metrics;         // Collect Starlink-specific metrics
    bool collect_cellular_metrics;         // Collect cellular-specific metrics
    bool collect_wifi_metrics;             // Collect WiFi-specific metrics
    bool collect_system_metrics;           // Collect system performance metrics
    
    // Storage configuration
    char database_path[256];               // SQLite database path
    bool enable_persistent_storage;        // Enable persistent storage
    int max_samples_per_interface;         // Max samples per interface in DB
    int max_decision_records;              // Max decision records in DB
    
    // Performance optimization
    int batch_insert_size;                 // Batch size for database inserts
    int cleanup_interval_s;                // Cleanup interval in seconds
    bool compress_old_data;                // Compress old data
    int compression_age_days;              // Age threshold for compression
    
    // ML simulation support
    bool enable_ml_dataset_export;         // Enable ML dataset export
    char ml_export_path[256];              // ML dataset export path
    int ml_export_interval_hours;          // ML export interval
} telemetry_collection_config_t;

// Telemetry collection statistics
typedef struct {
    uint64_t total_samples_collected;      // Total samples collected
    uint64_t samples_with_gps;             // Samples with GPS data
    uint64_t samples_without_gps;          // Samples without GPS data
    uint64_t decision_records_logged;      // Decision records logged
    
    uint64_t starlink_samples;             // Starlink samples
    uint64_t cellular_samples;             // Cellular samples
    uint64_t wifi_samples;                 // WiFi samples
    
    uint64_t database_inserts;             // Database insert operations
    uint64_t database_errors;              // Database error count
    uint64_t cleanup_operations;           // Cleanup operations performed
    
    double average_collection_time_ms;     // Average collection time
    double database_size_mb;               // Current database size
    int64_t memory_usage_bytes;            // Current memory usage
    
    time_t collection_start_time;          // When collection started
    time_t last_collection;                // Last collection time
    time_t last_cleanup;                   // Last cleanup time
    time_t last_ml_export;                 // Last ML export time
} telemetry_collection_statistics_t;

// Main telemetry collection manager
typedef struct {
    telemetry_collection_config_t config;  // Configuration
    telemetry_collection_statistics_t stats; // Statistics
    
    // Database connection
    sqlite3* db;                           // SQLite database connection
    bool db_initialized;                   // Database initialization status
    
    // In-memory ring buffers (for performance)
    telemetry_sample_t* samples_buffer;    // Ring buffer for samples
    int samples_buffer_size;               // Buffer size
    int samples_buffer_head;               // Buffer head index
    int samples_buffer_count;              // Number of samples in buffer
    
    decision_record_t* decisions_buffer;   // Ring buffer for decisions
    int decisions_buffer_size;             // Buffer size
    int decisions_buffer_head;             // Buffer head index
    int decisions_buffer_count;            // Number of decisions in buffer
    
    // Threading
    pthread_mutex_t mutex;                 // Main mutex
    pthread_t collection_thread;           // Collection thread
    pthread_t cleanup_thread;              // Cleanup thread
    pthread_t export_thread;               // ML export thread
    bool threads_running;                  // Thread status
    
    // State
    bool initialized;                      // Initialization status
    uint64_t next_sample_id;               // Next sample ID
    uint64_t next_decision_id;             // Next decision ID
} telemetry_comprehensive_t;

// Function prototypes

/**
 * Initialize comprehensive telemetry collection system
 * @param config Telemetry collection configuration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int telemetry_comprehensive_init(const telemetry_collection_config_t* config);

/**
 * Cleanup comprehensive telemetry collection system
 */
void telemetry_comprehensive_cleanup(void);

/**
 * Collect telemetry sample with GPS positioning
 * @param member_name Network member name
 * @param interface_name Interface name
 * @param sample Telemetry sample data
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int telemetry_comprehensive_collect_sample(const char* member_name,
                                          const char* interface_name,
                                          const telemetry_sample_t* sample);

/**
 * Log failover/failback decision with full context
 * @param decision Decision record data
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int telemetry_comprehensive_log_decision(const decision_record_t* decision);

/**
 * Get historical telemetry samples for analysis
 * @param member_name Member name filter (NULL for all)
 * @param start_time Start time filter
 * @param end_time End time filter
 * @param samples Array to store samples
 * @param max_samples Maximum samples to return
 * @return Number of samples returned, or negative error code
 */
int telemetry_comprehensive_get_historical_samples(const char* member_name,
                                                  time_t start_time,
                                                  time_t end_time,
                                                  telemetry_sample_t* samples,
                                                  int max_samples);

/**
 * Get decision history for analysis
 * @param start_time Start time filter
 * @param end_time End time filter
 * @param decisions Array to store decisions
 * @param max_decisions Maximum decisions to return
 * @return Number of decisions returned, or negative error code
 */
int telemetry_comprehensive_get_decision_history(time_t start_time,
                                                time_t end_time,
                                                decision_record_t* decisions,
                                                int max_decisions);

/**
 * Get telemetry statistics
 * @param stats Statistics structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int telemetry_comprehensive_get_statistics(telemetry_collection_statistics_t* stats);

/**
 * Export data for ML simulation
 * @param export_path Export file path
 * @param start_time Start time for export
 * @param end_time End time for export
 * @param format Export format ("csv", "json")
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int telemetry_comprehensive_export_ml_dataset(const char* export_path,
                                             time_t start_time,
                                             time_t end_time,
                                             const char* format);

/**
 * Get connectivity metrics by GPS location
 * @param latitude Latitude center
 * @param longitude Longitude center
 * @param radius_meters Search radius in meters
 * @param member_name Member name filter (NULL for all)
 * @param samples Array to store samples
 * @param max_samples Maximum samples to return
 * @return Number of samples returned, or negative error code
 */
int telemetry_comprehensive_get_samples_by_location(double latitude,
                                                   double longitude,
                                                   double radius_meters,
                                                   const char* member_name,
                                                   telemetry_sample_t* samples,
                                                   int max_samples);

/**
 * Analyze performance trends for interface
 * @param member_name Member name
 * @param hours_back Hours to analyze
 * @param trend_slope Trend slope output
 * @param trend_confidence Trend confidence output
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int telemetry_comprehensive_analyze_trends(const char* member_name,
                                          int hours_back,
                                          double* trend_slope,
                                          double* trend_confidence);

/**
 * Check if telemetry system is initialized
 * @return true if initialized, false otherwise
 */
bool telemetry_comprehensive_is_initialized(void);

/**
 * Force database cleanup and optimization
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int telemetry_comprehensive_force_cleanup(void);

// Utility functions for ML simulation support

/**
 * Test ML algorithm performance on historical data
 * @param algorithm_name Algorithm name for testing
 * @param start_time Start time for test period
 * @param end_time End time for test period
 * @param results_json JSON buffer for results (min 2048 bytes)
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int telemetry_comprehensive_test_ml_algorithm(const char* algorithm_name,
                                             time_t start_time,
                                             time_t end_time,
                                             char* results_json);

/**
 * Get performance correlation with GPS location
 * @param member_name Member name
 * @param correlation_data Buffer for correlation results
 * @param max_locations Maximum locations to analyze
 * @return Number of locations analyzed, or negative error code
 */
int telemetry_comprehensive_get_location_performance_correlation(const char* member_name,
                                                               void* correlation_data,
                                                               int max_locations);

// Database management functions

/**
 * Initialize SQLite database with proper schema
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int telemetry_db_init(void);

/**
 * Close database connection
 */
void telemetry_db_close(void);

/**
 * Execute database cleanup and optimization
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int telemetry_db_cleanup(void);

/**
 * Get database size and statistics
 * @param size_mb Database size in MB
 * @param sample_count Number of samples
 * @param decision_count Number of decisions
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int telemetry_db_get_info(double* size_mb, uint64_t* sample_count, uint64_t* decision_count);

#ifdef __cplusplus
}
#endif

#endif // TELEMETRY_COMPREHENSIVE_H