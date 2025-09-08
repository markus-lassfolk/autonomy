#ifndef ML_MONITOR_H
#define ML_MONITOR_H

#include "../core/types.h"
#include "../starlink/starlink_types.h"
#include "../external/external_apis.h"
#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/mman.h>

// ML Monitor configuration - integrated with UCI
typedef struct {
    // Core ML settings
    bool enabled;                           // Enable ML monitoring
    int collection_interval_seconds;        // Data collection interval (15 seconds default)
    int prediction_horizon_minutes;         // How far ahead to predict (15 minutes default)
    int max_observations;                   // Maximum observations to store (10000 default)
    
    // Learning parameters
    int learning_rate;                      // Learning rate 0-255 (128 default)
    int confidence_threshold;               // Confidence threshold 0-255 (128 default)
    int pattern_library_size;               // Pattern library size (1000 default)
    int neural_network_size;                // Neural network size in KB (10 default)
    
    // Sky grid settings
    int sky_grid_azimuth_resolution;        // Azimuth resolution in degrees (4 default)
    int sky_grid_elevation_resolution;      // Elevation resolution in degrees (4 default)
    int sky_grid_learning_rate;             // Sky grid learning rate 0-255 (64 default)
    
    // Mobile optimization
    bool mobile_mode_enabled;               // Enable mobile optimizations
    int location_change_threshold_meters;   // Location change threshold (100 default)
    int stationary_time_threshold_minutes;  // Time to consider stationary (60 default)
    
    // Performance tuning
    bool auto_tuning_enabled;               // Enable automatic parameter tuning
    int performance_evaluation_interval_hours; // Performance evaluation interval (24 default)
    int memory_limit_kb;                    // Memory limit in KB (1024 default)
    
    // Storage settings
    char storage_path[256];                 // Path to ML data storage (/tmp/ml_monitor.dat default)
    bool use_memory_mapped_storage;         // Use memory-mapped files (true default)
    int storage_sync_interval_minutes;      // Storage sync interval (5 default)
    
    // Debug settings
    bool debug_logging_enabled;             // Enable debug logging
    bool save_raw_observations;             // Save raw observations for debugging
    char debug_log_path[256];               // Debug log path
    
} ml_monitor_config_t;

// Compact ML observation structure (56 bytes total as per MasterPlan)
typedef struct __attribute__((packed)) {
    uint32_t timestamp;           // 4 bytes - Unix timestamp
    
    // Starlink metrics (28 bytes)
    uint16_t snr_x100;           // 2 bytes - SNR * 100 (fixed point)
    uint16_t latency_ms;         // 2 bytes
    uint8_t packet_loss_pct;     // 1 byte - percentage
    uint8_t obstruction_pct;     // 1 byte - percentage
    uint8_t wedge_obstruction[12]; // 12 bytes - per wedge
    int16_t azimuth_deg;         // 2 bytes
    int16_t elevation_deg;       // 2 bytes
    uint8_t satellites_visible;  // 1 byte
    uint8_t flags;               // 1 byte - bit flags
    
    // Weather (8 bytes)
    int8_t temperature_c;        // 1 byte
    uint8_t humidity_pct;        // 1 byte
    uint16_t pressure_hpa;       // 2 bytes
    uint8_t wind_speed_ms;       // 1 byte
    uint8_t precipitation_mm;    // 1 byte
    uint8_t cloud_cover_pct;     // 1 byte
    uint8_t weather_flags;       // 1 byte - conditions
    
    // Location (12 bytes)
    int32_t latitude_e7;         // 4 bytes - lat * 10^7
    int32_t longitude_e7;        // 4 bytes - lon * 10^7
    uint16_t altitude_m;         // 2 bytes
    uint8_t speed_kmh;           // 1 byte
    uint8_t heading_deg_div2;    // 1 byte - heading / 2
    
    // ML features (8 bytes)
    uint8_t outage_probability;  // 1 byte - percentage
    uint8_t outage_type;         // 1 byte - classification
    uint8_t confidence;          // 1 byte - percentage
    uint8_t anomaly_score;       // 1 byte
    uint16_t pattern_hash;       // 2 bytes - pattern signature
    uint16_t reserved;           // 2 bytes - future use
    
} ml_observation_t;  // Total: 56 bytes

// Circular buffer for observations
typedef struct {
    ml_observation_t *observations;
    uint32_t write_index;
    uint32_t count;
    uint32_t max_observations;
    uint32_t location_change_count;
    time_t last_location_change;
} observation_buffer_t;

// Compact sky grid (4KB total for 90x45 = 4050 bytes)
typedef struct {
    uint8_t obstruction_prob[90][45];  // 0-255 probability (4° resolution)
    uint8_t sample_count[90][45];      // Capped at 255
    uint32_t last_update;
    
    // Location-specific learning
    int32_t learned_lat_e7;
    int32_t learned_lon_e7;
    uint16_t learning_time_hours;
} compact_sky_grid_t;

// Pattern entry for k-NN learning
typedef struct {
    uint16_t pattern[16];    // Compressed feature vector
    uint8_t outcome;         // What happened (outage type)
    uint8_t confidence;      // How sure we are
} pattern_entry_t;

// Pattern matcher for k-NN
typedef struct {
    pattern_entry_t *patterns;
    uint16_t count;
    uint16_t max_patterns;
    
    // Incremental statistics
    uint32_t total_predictions;
    uint32_t correct_predictions;
    float accuracy;
} pattern_matcher_t;

// Tiny neural network (quantized to int8 for embedded systems)
typedef struct {
    // Fixed architecture: 32 inputs -> 16 hidden -> 8 outputs
    int8_t weights1[32][16];  // 512 bytes (quantized to int8)
    int8_t weights2[16][8];   // 128 bytes
    int8_t bias1[16];         // 16 bytes
    int8_t bias2[8];          // 8 bytes
    
    // Running statistics for normalization
    int16_t input_mean[32];   // 64 bytes
    int16_t input_std[32];    // 64 bytes
    
    // Learning rate decay
    uint16_t update_count;
    uint8_t learning_rate;    // Fixed point: actual = lr/255
} tiny_nn_t;

// Location learning for mobile scenarios
typedef struct {
    // Current location profile
    int32_t current_lat_e7;
    int32_t current_lon_e7;
    time_t arrival_time;
    uint32_t observations_here;
    
    // Quick profile of current location
    struct {
        uint8_t typical_snr;
        uint8_t typical_latency;
        uint8_t obstruction_level;
        uint8_t reliability_score;
        uint8_t learned;  // 0-255 how well we know this spot
    } profile;
    
    // Historical location memory (last 10 locations)
    struct {
        int32_t lat_e7;
        int32_t lon_e7;
        uint8_t profile_hash[16];  // Compressed profile
        time_t last_visit;
    } history[10];
    uint8_t history_count;
    
} location_learner_t;

// Performance monitoring and auto-tuning
typedef struct {
    // Performance tracking
    uint32_t predictions_made;
    uint32_t predictions_correct;
    uint32_t false_positives;
    uint32_t false_negatives;
    
    // Model performance
    struct {
        uint8_t accuracy_pct;
        uint8_t precision_pct;
        uint8_t recall_pct;
        uint32_t last_evaluation;
    } metrics;
    
    // Auto-tuning parameters
    struct {
        uint8_t learning_rate;
        uint8_t confidence_threshold;
        uint8_t prediction_horizon;
        uint8_t feature_weights[16];
    } params;
    
    // Resource usage
    struct {
        uint32_t cpu_cycles_used;
        uint32_t memory_peak_kb;
        uint32_t storage_used_kb;
    } resources;
    
} performance_monitor_t;

// Memory-mapped persistent state (main ML data structure)
typedef struct {
    uint32_t magic;              // File validation (0x4D4C5354 = "MLST")
    uint32_t version;
    uint32_t total_observations;
    uint32_t location_changes;
    
    // Circular buffers
    observation_buffer_t recent;     // Last 10K observations
    observation_buffer_t hourly;     // Hourly aggregates (168 entries = 1 week)
    observation_buffer_t daily;      // Daily aggregates (30 entries = 1 month)
    
    // Learned models (compact representation)
    struct {
        compact_sky_grid_t sky_grid;        // 4KB obstruction map
        pattern_matcher_t pattern_matcher;  // k-NN patterns
        tiny_nn_t neural_network;          // Lightweight NN
        location_learner_t location_learner; // Mobile learning
        performance_monitor_t performance;   // Performance tracking
    } models;
    
} ml_persistent_state_t;

// Main ML monitor structure
typedef struct {
    ml_monitor_config_t config;
    ml_persistent_state_t *state;
    
    // Runtime state
    bool initialized;
    bool running;
    time_t last_collection;
    time_t last_prediction;
    
    // File descriptor for memory-mapped storage
    int storage_fd;
    size_t storage_size;
    
    // Threading
    pthread_t collection_thread;
    pthread_t prediction_thread;
    pthread_mutex_t state_mutex;
    bool should_stop;
    
    // Callbacks
    void (*outage_prediction_callback)(uint8_t probability, uint8_t confidence, time_t when, void *user_data);
    void (*anomaly_detected_callback)(uint8_t score, const ml_observation_t *observation, void *user_data);
    void (*performance_update_callback)(const performance_monitor_t *performance, void *user_data);
    void *callback_user_data;
    
} ml_monitor_t;

// API Functions

// Initialization and cleanup
ml_monitor_t* ml_monitor_init(const ml_monitor_config_t *config);
void ml_monitor_cleanup(ml_monitor_t *monitor);

// Configuration management (UCI integration)
int ml_monitor_load_config_from_uci(ml_monitor_config_t *config);
int ml_monitor_save_config_to_uci(const ml_monitor_config_t *config);
void ml_monitor_config_init_defaults(ml_monitor_config_t *config);
int ml_monitor_update_config(ml_monitor_t *monitor, const ml_monitor_config_t *config);

// Memory-mapped storage management
ml_persistent_state_t* ml_monitor_init_storage(const char *filepath, size_t *storage_size);
int ml_monitor_sync_storage(ml_monitor_t *monitor);
void ml_monitor_cleanup_storage(ml_monitor_t *monitor);

// Data collection
int ml_monitor_collect_observation(ml_monitor_t *monitor);
int ml_monitor_add_observation(ml_monitor_t *monitor, const ml_observation_t *observation);
int ml_monitor_convert_starlink_data(const starlink_status_response_t *starlink_data,
                                    const weather_data_t *weather_data,
                                    const gps_data_t *gps_data,
                                    ml_observation_t *observation);

// Learning algorithms
int ml_monitor_update_sky_grid(ml_monitor_t *monitor, const ml_observation_t *observation);
int ml_monitor_learn_pattern(ml_monitor_t *monitor, const ml_observation_t *observation, uint8_t actual_outcome);
int ml_monitor_train_neural_network(ml_monitor_t *monitor, const ml_observation_t *observation, uint8_t target[8]);
int ml_monitor_update_location_learning(ml_monitor_t *monitor, const ml_observation_t *observation);

// Prediction functions
uint8_t ml_monitor_predict_outage_knn(ml_monitor_t *monitor, const ml_observation_t *observation, uint8_t *confidence);
void ml_monitor_predict_neural_network(ml_monitor_t *monitor, const ml_observation_t *observation, uint8_t output[8]);
int ml_monitor_predict_next_15_minutes(ml_monitor_t *monitor, uint8_t probabilities[60], uint8_t *confidence);

// Sky grid functions
int ml_monitor_sky_grid_update(compact_sky_grid_t *grid, int16_t azimuth, int16_t elevation, uint8_t obstruction_detected);
bool ml_monitor_location_changed(const compact_sky_grid_t *grid, int32_t lat_e7, int32_t lon_e7);
void ml_monitor_sky_grid_reset_soft(compact_sky_grid_t *grid);

// Location learning functions
void ml_monitor_location_learner_update(location_learner_t *learner, const ml_observation_t *obs);
bool ml_monitor_location_changed_threshold(int32_t lat1_e7, int32_t lon1_e7, int32_t lat2_e7, int32_t lon2_e7, int threshold_meters);

// Performance monitoring and auto-tuning
void ml_monitor_update_performance(ml_monitor_t *monitor, uint8_t prediction, uint8_t actual, uint8_t confidence);
void ml_monitor_auto_tune_system(ml_monitor_t *monitor);
void ml_monitor_evaluate_model_performance(ml_monitor_t *monitor);

// Control functions
int ml_monitor_start(ml_monitor_t *monitor);
int ml_monitor_stop(ml_monitor_t *monitor);
bool ml_monitor_is_running(const ml_monitor_t *monitor);

// Utility functions
uint8_t ml_monitor_weighted_average(uint8_t old_value, uint8_t new_value, uint8_t alpha);
void ml_monitor_extract_pattern_features(const ml_observation_t *obs, uint16_t pattern[16]);
void ml_monitor_prepare_nn_input(const ml_observation_t *obs, int8_t nn_input[32]);

// Callback management
int ml_monitor_set_outage_prediction_callback(ml_monitor_t *monitor, 
                                             void (*callback)(uint8_t probability, uint8_t confidence, time_t when, void *user_data),
                                             void *user_data);
int ml_monitor_set_anomaly_detection_callback(ml_monitor_t *monitor,
                                             void (*callback)(uint8_t score, const ml_observation_t *observation, void *user_data),
                                             void *user_data);

// Error codes
#define ML_MONITOR_SUCCESS                 0
#define ML_MONITOR_ERROR_INVALID_PARAM    -1
#define ML_MONITOR_ERROR_NOT_INITIALIZED  -2
#define ML_MONITOR_ERROR_MEMORY_FAILED    -3
#define ML_MONITOR_ERROR_STORAGE_FAILED   -4
#define ML_MONITOR_ERROR_CONFIG_FAILED    -5
#define ML_MONITOR_ERROR_THREAD_FAILED    -6
#define ML_MONITOR_ERROR_ALREADY_RUNNING  -7
#define ML_MONITOR_ERROR_NOT_RUNNING      -8
#define ML_MONITOR_ERROR_UCI_FAILED       -9
#define ML_MONITOR_ERROR_PREDICTION_FAILED -10

// Outage type classifications
#define OUTAGE_UNKNOWN                0
#define OUTAGE_OBSTRUCTION_STATIC     1
#define OUTAGE_OBSTRUCTION_DYNAMIC    2
#define OUTAGE_WEATHER_PRECIPITATION  3
#define OUTAGE_WEATHER_ATMOSPHERIC    4
#define OUTAGE_SATELLITE_HARDWARE     5
#define OUTAGE_SATELLITE_SOFTWARE     6
#define OUTAGE_NETWORK_CONGESTION     7
#define OUTAGE_NETWORK_ROUTING        8
#define OUTAGE_TERMINAL_THERMAL       9
#define OUTAGE_TERMINAL_HARDWARE      10
#define OUTAGE_COVERAGE_GAP          11
#define OUTAGE_INTERFERENCE          12

// Flags for ml_observation_t.flags
#define ML_OBS_FLAG_OUTAGE           (1 << 0)
#define ML_OBS_FLAG_DEGRADED         (1 << 1)
#define ML_OBS_FLAG_MOVING           (1 << 2)
#define ML_OBS_FLAG_WEATHER_IMPACT   (1 << 3)
#define ML_OBS_FLAG_HIGH_CONFIDENCE  (1 << 4)
#define ML_OBS_FLAG_ANOMALY          (1 << 5)
#define ML_OBS_FLAG_PREDICTION_MADE  (1 << 6)
#define ML_OBS_FLAG_LEARNING_MODE    (1 << 7)

#endif // ML_MONITOR_H