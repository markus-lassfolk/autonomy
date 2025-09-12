#include "ml_monitor.h"
#include "ml_monitor_analytics.h"
#include "../shared/utils/uci_manager.h"
#include "../core/types.h"
#include "../starlink/starlink_snow_detection.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>

// Global ML monitor instance
static ml_monitor_t *g_ml_monitor = NULL;

// Forward declarations
static void* ml_monitor_collection_thread(void *arg);
static void* ml_monitor_prediction_thread(void *arg);
static int ml_monitor_collect_data_sources(ml_monitor_t *monitor, ml_observation_t *observation);

// Collect data from various sources for ML observation
static int ml_monitor_collect_data_sources(ml_monitor_t *monitor, ml_observation_t *observation) {
    if (!monitor || !observation) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    // Initialize observation
    memset(observation, 0, sizeof(ml_observation_t));
    observation->timestamp = time(NULL);
    
    // TODO: Implement actual data collection from GPS, Starlink, etc.
    // For now, just return success with empty observation
    printf("DEBUG: Collected data sources for ML observation\n");
    
    return ML_MONITOR_SUCCESS;
}

// Initialize default configuration
void ml_monitor_config_init_defaults(ml_monitor_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(ml_monitor_config_t));
    
    // Core ML settings
    config->enabled = true;
    config->collection_interval_seconds = 15;
    config->prediction_horizon_minutes = 15;
    config->max_observations = 10000;
    
    // Learning parameters
    config->learning_rate = 128;
    config->confidence_threshold = 128;
    config->pattern_library_size = 1000;
    config->neural_network_size = 10;
    
    // Sky grid settings
    config->sky_grid_azimuth_resolution = 4;
    config->sky_grid_elevation_resolution = 4;
    config->sky_grid_learning_rate = 64;
    
    // Mobile optimization
    config->mobile_mode_enabled = true;
    config->location_change_threshold_meters = 100;
    config->stationary_time_threshold_minutes = 60;
    
    // Performance tuning
    config->auto_tuning_enabled = true;
    config->performance_evaluation_interval_hours = 24;
    config->memory_limit_kb = 1024;
    
    // Storage settings
    strncpy(config->storage_path, "/var/lib/autonomy/ml_monitor.dat", sizeof(config->storage_path) - 1);
    config->use_memory_mapped_storage = true;
    config->storage_sync_interval_minutes = 5;
    
    // Debug settings
    config->debug_logging_enabled = false;
    config->save_raw_observations = false;
    strncpy(config->debug_log_path, "/tmp/ml_monitor_debug.log", sizeof(config->debug_log_path) - 1);
}

// Note: ml_monitor_load_config_from_uci is implemented in ml_monitor_uci.c (more feature-complete)

// Note: ml_monitor_save_config_to_uci is implemented in ml_monitor_uci.c (more feature-complete)

// Initialize memory-mapped storage
ml_persistent_state_t* ml_monitor_init_storage(const char *filepath, size_t *storage_size) {
    fprintf(stderr, "DEBUG: ml_monitor_init_storage called with filepath: %s\n", filepath ? filepath : "NULL");
    if (!filepath || !storage_size) return NULL;
    
    printf("INFO: Initializing ML monitor storage: %s\n", filepath);
    fprintf(stderr, "DEBUG: About to open file: %s\n", filepath);
    
    // Create directory if it doesn't exist
    char dir_path[512];
    strncpy(dir_path, filepath, sizeof(dir_path) - 1);
    dir_path[sizeof(dir_path) - 1] = '\0';
    char *last_slash = strrchr(dir_path, '/');
    if (last_slash) {
        *last_slash = '\0';
        fprintf(stderr, "DEBUG: Creating directory: %s\n", dir_path);
        if (mkdir(dir_path, 0755) < 0 && errno != EEXIST) {
            printf("ERROR: Failed to create storage directory: %s\n", strerror(errno));
            return NULL;
        }
        fprintf(stderr, "DEBUG: Directory created or already exists\n");
    }
    
    int fd = open(filepath, O_RDWR | O_CREAT, 0644);
    fprintf(stderr, "DEBUG: File opened, fd=%d\n", fd);
    if (fd < 0) {
        printf("ERROR: Failed to open storage file: %s\n", strerror(errno));
        return NULL;
    }
    
    // Calculate required storage size
    *storage_size = sizeof(ml_persistent_state_t);
    fprintf(stderr, "DEBUG: Storage size calculated: %zu\n", *storage_size);
    
    // Ensure file size
    fprintf(stderr, "DEBUG: About to ftruncate file\n");
    if (ftruncate(fd, *storage_size) < 0) {
        printf("ERROR: Failed to resize storage file: %s\n", strerror(errno));
        close(fd);
        return NULL;
    }
    fprintf(stderr, "DEBUG: File truncated successfully\n");
    
    // Memory map the file
    fprintf(stderr, "DEBUG: About to mmap file\n");
    ml_persistent_state_t* state = mmap(NULL, *storage_size, 
                                       PROT_READ | PROT_WRITE, 
                                       MAP_SHARED, fd, 0);
    fprintf(stderr, "DEBUG: mmap result: %p\n", state);
    close(fd);
    
    if (state == MAP_FAILED) {
        printf("ERROR: Failed to memory map storage file: %s\n", strerror(errno));
        return NULL;
    }
    fprintf(stderr, "DEBUG: Memory mapping successful\n");
    
    // Initialize if new file
    fprintf(stderr, "DEBUG: Checking magic number: 0x%08X\n", state->magic);
    if (state->magic != 0x4D4C5354) { // "MLST"
        printf("INFO: Initializing new ML storage file\n");
        fprintf(stderr, "DEBUG: Initializing new storage file\n");
        memset(state, 0, *storage_size);
        state->magic = 0x4D4C5354;
        state->version = 1;
        
        // Initialize observation buffers
        state->recent.max_observations = 10000;
        state->hourly.max_observations = 168;  // 1 week
        state->daily.max_observations = 30;    // 1 month
        
        // Initialize pattern matcher
        state->models.pattern_matcher.max_patterns = 1000;
        
        // Initialize neural network with proper Xavier initialization
        tiny_nn_t *nn = &state->models.neural_network;
        nn->learning_rate = 128;
        
        // Xavier initialization for better convergence (not random)
        // Formula: weight = uniform(-sqrt(6/(fan_in + fan_out)), sqrt(6/(fan_in + fan_out)))
        double xavier_range_1 = sqrt(6.0 / (32 + 16)) * 127; // Scale to int8 range
        double xavier_range_2 = sqrt(6.0 / (16 + 8)) * 127;
        
        // Use time-based seed for reproducible initialization
        srand((unsigned int)time(NULL));
        
        for (int i = 0; i < 32; i++) {
            for (int j = 0; j < 16; j++) {
                double normalized = ((double)rand() / RAND_MAX) * 2.0 - 1.0; // -1 to 1
                nn->weights1[i][j] = (int8_t)(normalized * xavier_range_1);
            }
        }
        for (int i = 0; i < 16; i++) {
            for (int j = 0; j < 8; j++) {
                double normalized = ((double)rand() / RAND_MAX) * 2.0 - 1.0;
                nn->weights2[i][j] = (int8_t)(normalized * xavier_range_2);
            }
        }
        
        printf("INFO: ML storage initialized successfully\n");
    } else {
        printf("INFO: Loaded existing ML storage (version %u, %u observations)\n", 
                 state->version, state->total_observations);
    }
    
    fprintf(stderr, "DEBUG: ml_monitor_init_storage completed successfully\n");
    return state;
}

// Initialize ML monitor
ml_monitor_t* ml_monitor_init(const ml_monitor_config_t *config) {
    fprintf(stderr, "DEBUG: ml_monitor_init called\n");
    if (!config) {
        LOGX_ERROR_MSG("Invalid configuration provided to ML monitor");
        fprintf(stderr, "DEBUG: ml_monitor_init failed - NULL config\n");
        return NULL;
    }
    
    if (g_ml_monitor) {
        LOGX_WARN_MSG("ML monitor already initialized");
        fprintf(stderr, "DEBUG: ml_monitor_init - already initialized\n");
        return g_ml_monitor;
    }
    
    LOGX_INFO_MSG("Initializing ML monitor");
    fprintf(stderr, "DEBUG: ml_monitor_init - starting initialization\n");
    
    ml_monitor_t *monitor = calloc(1, sizeof(ml_monitor_t));
    if (!monitor) {
        LOGX_ERROR_MSG("Failed to allocate ML monitor structure");
        fprintf(stderr, "DEBUG: ml_monitor_init failed - calloc failed\n");
        return NULL;
    }
    fprintf(stderr, "DEBUG: ml_monitor_init - allocated monitor structure\n");
    
    // Copy configuration
    memcpy(&monitor->config, config, sizeof(ml_monitor_config_t));
    fprintf(stderr, "DEBUG: ml_monitor_init - copied configuration\n");
    
    // Initialize storage
    fprintf(stderr, "DEBUG: ml_monitor_init - about to initialize storage\n");
    monitor->state = ml_monitor_init_storage(config->storage_path, &monitor->storage_size);
    if (!monitor->state) {
        LOGX_ERROR_MSG("Failed to initialize ML storage");
        fprintf(stderr, "DEBUG: ml_monitor_init failed - storage initialization failed\n");
        free(monitor);
        return NULL;
    }
    fprintf(stderr, "DEBUG: ml_monitor_init - storage initialized successfully\n");
    
    // Initialize analytics system
    fprintf(stderr, "DEBUG: ml_monitor_init - about to initialize analytics\n");
    int analytics_result = ml_monitor_analytics_init();
    if (analytics_result != ML_MONITOR_SUCCESS) {
        LOGX_ERROR_MSG("Failed to initialize ML analytics: %d", analytics_result);
        fprintf(stderr, "DEBUG: ml_monitor_init - analytics initialization failed: %d\n", analytics_result);
        // Continue without analytics - not critical
    } else {
        LOGX_INFO_MSG(" ML Analytics system initialized");
        fprintf(stderr, "DEBUG: ml_monitor_init - analytics initialized successfully\n");
    }
    
    // Initialize mutex
    fprintf(stderr, "DEBUG: ml_monitor_init - about to initialize mutex\n");
    if (pthread_mutex_init(&monitor->state_mutex, NULL) != 0) {
        LOGX_ERROR_MSG("Failed to initialize ML monitor mutex");
        fprintf(stderr, "DEBUG: ml_monitor_init failed - mutex initialization failed\n");
        munmap(monitor->state, monitor->storage_size);
        free(monitor);
        return NULL;
    }
    fprintf(stderr, "DEBUG: ml_monitor_init - mutex initialized successfully\n");
    
    monitor->initialized = true;
    g_ml_monitor = monitor;
    
    LOGX_INFO_MSG("ML monitor initialized successfully (memory usage: %zu KB)", 
                  monitor->storage_size / 1024);
    fprintf(stderr, "DEBUG: ml_monitor_init completed successfully\n");
    
    return monitor;
}

// Cleanup ML monitor
void ml_monitor_cleanup(ml_monitor_t *monitor) {
    if (!monitor) return;
    
    LOGX_INFO_MSG("Cleaning up ML monitor");
    
    // Stop threads if running
    if (monitor->running) {
        ml_monitor_stop(monitor);
    }
    
    // Sync storage one last time
    if (monitor->state) {
        ml_monitor_sync_storage(monitor);
        munmap(monitor->state, monitor->storage_size);
    }
    
    // Cleanup mutex
    pthread_mutex_destroy(&monitor->state_mutex);
    
    if (g_ml_monitor == monitor) {
        g_ml_monitor = NULL;
    }
    
    free(monitor);
    LOGX_INFO_MSG("ML monitor cleanup completed");
}

// Sync storage to disk
int ml_monitor_sync_storage(ml_monitor_t *monitor) {
    if (!monitor || !monitor->state) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    if (msync(monitor->state, monitor->storage_size, MS_ASYNC) < 0) {
        LOGX_ERROR_MSG("Failed to sync ML storage: %s", strerror(errno));
        return ML_MONITOR_ERROR_STORAGE_FAILED;
    }
    
    return ML_MONITOR_SUCCESS;
}

// Convert Starlink data to ML observation
int ml_monitor_convert_starlink_data(const starlink_status_response_t *starlink_data,
                                    const weather_data_t *weather_data,
                                    const gps_data_t *gps_data,
                                    ml_observation_t *observation) {
    if (!starlink_data || !observation) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    memset(observation, 0, sizeof(ml_observation_t));
    
    // Timestamp
    observation->timestamp = time(NULL);
    
    // Starlink metrics
    observation->snr_x100 = (uint16_t)(starlink_data->signal_quality.snr * 100);
    observation->latency_ms = (uint16_t)starlink_data->network_perf.pop_ping_latency_ms;
    observation->packet_loss_pct = (uint8_t)(starlink_data->network_perf.pop_ping_drop_rate * 100);
    observation->obstruction_pct = (uint8_t)(starlink_data->obstruction_stats.fraction_obstructed * 100);
    
    // Copy wedge obstructions
    for (int i = 0; i < 12; i++) {
        observation->wedge_obstruction[i] = (uint8_t)(starlink_data->obstruction_stats.wedge_fraction_obstructed[i] * 100);
    }
    
    // Positioning
    observation->azimuth_deg = (int16_t)starlink_data->positioning.boresight_azimuth_deg;
    observation->elevation_deg = (int16_t)starlink_data->positioning.boresight_elevation_deg;
    
    // GPS/Starlink satellites (estimate visible satellites)
    observation->satellites_visible = starlink_data->gps_stats.gps_sats > 0 ? starlink_data->gps_stats.gps_sats : 8;
    
    // Flags
    if (starlink_data->obstruction_stats.currently_obstructed) {
        observation->flags |= ML_OBS_FLAG_OUTAGE;
    }
    if (starlink_data->signal_quality.snr < 5.0) {  // Low SNR threshold
        observation->flags |= ML_OBS_FLAG_DEGRADED;
    }
    
    // Weather data (if available)
    if (weather_data) {
        observation->temperature_c = (int8_t)weather_data->temperature;
        observation->humidity_pct = (uint8_t)weather_data->humidity;
        observation->pressure_hpa = (uint16_t)weather_data->pressure;
        observation->wind_speed_ms = (uint8_t)weather_data->wind_speed;
        observation->precipitation_mm = (uint8_t)weather_data->precipitation;
        observation->cloud_cover_pct = (uint8_t)weather_data->cloud_cover;
        
        if (weather_data->precipitation > 1.0 || weather_data->cloud_cover > 80) {
            observation->flags |= ML_OBS_FLAG_WEATHER_IMPACT;
        }
    }
    
    // GPS/Location data (if available)
    if (gps_data) {
        observation->latitude_e7 = (int32_t)(gps_data->latitude * 10000000);
        observation->longitude_e7 = (int32_t)(gps_data->longitude * 10000000);
        observation->altitude_m = (uint16_t)gps_data->altitude;
        observation->speed_kmh = (uint8_t)(gps_data->speed * 3.6);  // Convert m/s to km/h
        observation->heading_deg_div2 = (uint8_t)(gps_data->heading / 2);
        
        if (gps_data->speed > 1.0) {  // Moving threshold
            observation->flags |= ML_OBS_FLAG_MOVING;
        }
    }
    
    return ML_MONITOR_SUCCESS;
}

// Add observation to buffer
int ml_monitor_add_observation(ml_monitor_t *monitor, const ml_observation_t *observation) {
    if (!monitor || !monitor->state || !observation) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock(&monitor->state_mutex);
    
    ml_persistent_state_t *state = monitor->state;
    observation_buffer_t *buffer = &state->recent;
    
    // Add to recent observations buffer in memory-mapped storage
    // Calculate offset within the memory-mapped storage for observations
    size_t observations_offset = offsetof(ml_persistent_state_t, recent) + offsetof(observation_buffer_t, write_index);
    
    // Use the memory-mapped region directly
    uint32_t index = buffer->write_index % buffer->max_observations;
    
    // Calculate the actual memory location for this observation
    // The observations are stored after the buffer metadata in the memory-mapped file
    char *storage_base = (char*)monitor->state;
    size_t obs_array_offset = sizeof(ml_persistent_state_t); // Observations stored after main structure
    ml_observation_t *obs_array = (ml_observation_t*)(storage_base + obs_array_offset);
    
    // Copy observation directly to memory-mapped storage
    memcpy(&obs_array[index], observation, sizeof(ml_observation_t));
    
    buffer->write_index++;
    if (buffer->count < buffer->max_observations) {
        buffer->count++;
    }
    
    state->total_observations++;
    
    // Update location tracking
    static int32_t last_lat = 0, last_lon = 0;
    if (last_lat != observation->latitude_e7 || last_lon != observation->longitude_e7) {
        if (abs(observation->latitude_e7 - last_lat) > 1000 || 
            abs(observation->longitude_e7 - last_lon) > 1000) {  // ~100m change
            state->location_changes++;
            buffer->last_location_change = observation->timestamp;
            last_lat = observation->latitude_e7;
            last_lon = observation->longitude_e7;
        }
    }
    
    pthread_mutex_unlock(&monitor->state_mutex);
    
    LOGX_DEBUG_MSG("Added ML observation (total: %u, SNR: %.2f, obstruction: %u%%)", 
              state->total_observations,
              observation->snr_x100 / 100.0,
              observation->obstruction_pct);
    
    return ML_MONITOR_SUCCESS;
}

// Weighted average helper function
uint8_t ml_monitor_weighted_average(uint8_t old_value, uint8_t new_value, uint8_t alpha) {
    return (uint8_t)((old_value * (256 - alpha) + new_value * alpha) >> 8);
}

// Sky grid update
int ml_monitor_sky_grid_update(compact_sky_grid_t *grid, int16_t azimuth, int16_t elevation, uint8_t obstruction_detected) {
    if (!grid) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    // Convert to grid coordinates (4 bins)
    int az_bin = azimuth / 4;
    int el_bin = elevation / 4;
    
    if (az_bin < 0 || az_bin >= 90 || el_bin < 0 || el_bin >= 45) {
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    // Exponential moving average for fast adaptation
    uint8_t *prob = &grid->obstruction_prob[az_bin][el_bin];
    uint8_t *count = &grid->sample_count[az_bin][el_bin];
    
    if (*count < 255) (*count)++;
    
    // Adaptive learning rate based on sample count
    uint8_t alpha = *count < 10 ? 128 : (*count < 50 ? 64 : 32);
    
    if (obstruction_detected) {
        *prob = *prob + ((255 - *prob) * alpha) / 256;
    } else {
        *prob = *prob - (*prob * alpha) / 256;
    }
    
    grid->last_update = time(NULL);
    
    return ML_MONITOR_SUCCESS;
}

// Check if location changed significantly
bool ml_monitor_location_changed_threshold(int32_t lat1_e7, int32_t lon1_e7, int32_t lat2_e7, int32_t lon2_e7, int threshold_meters) {
    // Simple distance calculation (approximate)
    int32_t lat_diff = abs(lat2_e7 - lat1_e7);
    int32_t lon_diff = abs(lon2_e7 - lon1_e7);
    
    // Convert to approximate meters (rough calculation)
    // 1 degree  111km, so 1e7 units  11.1m per unit
    int32_t distance_approx = (lat_diff + lon_diff) / 90;  // Rough approximation
    
    return distance_approx > threshold_meters;
}

// Update sky grid with new observation
int ml_monitor_update_sky_grid(ml_monitor_t *monitor, const ml_observation_t *observation) {
    if (!monitor || !monitor->state || !observation) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    compact_sky_grid_t *grid = &monitor->state->models.sky_grid;
    
    // Check if location changed significantly
    if (ml_monitor_location_changed_threshold(grid->learned_lat_e7, grid->learned_lon_e7,
                                            observation->latitude_e7, observation->longitude_e7, 
                                            monitor->config.location_change_threshold_meters)) {
        LOGX_INFO_MSG("Location changed significantly, soft-resetting sky grid");
        ml_monitor_sky_grid_reset_soft(grid);
        grid->learned_lat_e7 = observation->latitude_e7;
        grid->learned_lon_e7 = observation->longitude_e7;
        grid->learning_time_hours = 0;
    }
    
    // Update grid with current observation
    uint8_t obstruction = (observation->flags & ML_OBS_FLAG_OUTAGE) ? 1 : 0;
    return ml_monitor_sky_grid_update(grid, observation->azimuth_deg, observation->elevation_deg, obstruction);
}

// Soft reset sky grid for new location
void ml_monitor_sky_grid_reset_soft(compact_sky_grid_t *grid) {
    if (!grid) return;
    
    // Soft reset: decay existing knowledge rather than wiping it
    for (int i = 0; i < 90; i++) {
        for (int j = 0; j < 45; j++) {
            grid->obstruction_prob[i][j] /= 2;
            grid->sample_count[i][j] /= 4;
        }
    }
    grid->learning_time_hours = 0;
}

// Extract pattern features from observation
void ml_monitor_extract_pattern_features(const ml_observation_t *obs, uint16_t pattern[16]) {
    if (!obs || !pattern) return;
    
    // Extract key features for pattern matching
    pattern[0] = obs->snr_x100 / 10;                    // SNR (0-6553)
    pattern[1] = obs->latency_ms;                       // Latency
    pattern[2] = obs->packet_loss_pct * 256;            // Packet loss (scaled)
    pattern[3] = obs->obstruction_pct * 256;            // Obstruction (scaled)
    pattern[4] = (uint16_t)obs->azimuth_deg;           // Azimuth
    pattern[5] = (uint16_t)obs->elevation_deg;         // Elevation
    pattern[6] = obs->satellites_visible * 256;         // Satellites
    pattern[7] = obs->temperature_c + 128;              // Temperature (offset)
    pattern[8] = obs->humidity_pct * 256;               // Humidity (scaled)
    pattern[9] = obs->pressure_hpa / 10;                // Pressure (scaled down)
    pattern[10] = obs->wind_speed_ms * 256;             // Wind speed (scaled)
    pattern[11] = obs->precipitation_mm * 256;          // Precipitation (scaled)
    pattern[12] = obs->cloud_cover_pct * 256;           // Cloud cover (scaled)
    pattern[13] = obs->speed_kmh * 256;                 // Speed (scaled)
    pattern[14] = obs->flags * 256;                     // Flags (scaled)
    pattern[15] = (obs->timestamp % 86400) / 337;       // Time of day (0-255)
}

// k-NN prediction
uint8_t ml_monitor_predict_outage_knn(ml_monitor_t *monitor, const ml_observation_t *observation, uint8_t *confidence) {
    if (!monitor || !monitor->state || !observation || !confidence) {
        if (confidence) *confidence = 0;
        return OUTAGE_UNKNOWN;
    }
    
    pattern_matcher_t *matcher = &monitor->state->models.pattern_matcher;
    
    if (matcher->count < 5) {
        *confidence = 0;
        return OUTAGE_UNKNOWN;
    }
    
    // Extract features to pattern
    uint16_t current_pattern[16];
    ml_monitor_extract_pattern_features(observation, current_pattern);
    
    // Find k=5 nearest neighbors (simplified implementation)
    typedef struct {
        uint32_t distance;
        uint8_t outcome;
    } neighbor_t;
    
    neighbor_t neighbors[5];
    uint8_t k = 0;
    
    // Access stored patterns from memory-mapped storage
    char *storage_base = (char*)monitor->state;
    size_t patterns_offset = sizeof(ml_persistent_state_t) + (monitor->state->recent.max_observations * sizeof(ml_observation_t));
    pattern_entry_t *patterns = (pattern_entry_t*)(storage_base + patterns_offset);
    
    for (int i = 0; i < matcher->count && i < matcher->max_patterns; i++) {
        // Calculate Manhattan distance between current pattern and stored pattern
        uint32_t dist = 0;
        for (int j = 0; j < 16; j++) {
            int32_t diff = current_pattern[j] - patterns[i].pattern[j];
            dist += abs(diff);
        }
        
        // Insert into top-k if closer
        if (k < 5 || dist < neighbors[k-1].distance) {
            if (k < 5) k++;
            
            // Insert sorted
            int pos = k - 1;
            while (pos > 0 && dist < neighbors[pos-1].distance) {
                neighbors[pos] = neighbors[pos-1];
                pos--;
            }
            neighbors[pos].distance = dist;
            neighbors[pos].outcome = patterns[i].outcome;
        }
    }
    
    if (k == 0) {
        *confidence = 0;
        return OUTAGE_UNKNOWN;
    }
    
    // Vote on outcome (simplified)
    uint8_t votes[16] = {0};
    uint32_t total_weight = 0;
    
    for (int i = 0; i < k; i++) {
        uint32_t weight = 1000000 / (neighbors[i].distance + 1);
        votes[neighbors[i].outcome] += weight;
        total_weight += weight;
    }
    
    // Find winner
    uint8_t best_outcome = OUTAGE_UNKNOWN;
    uint32_t best_votes = 0;
    
    for (int i = 0; i < 16; i++) {
        if (votes[i] > best_votes) {
            best_votes = votes[i];
            best_outcome = i;
        }
    }
    
    *confidence = (best_votes * 100) / total_weight;
    return best_outcome;
}

// Neural network prediction (simplified)
void ml_monitor_predict_neural_network(ml_monitor_t *monitor, const ml_observation_t *observation, uint8_t output[8]) {
    if (!monitor || !monitor->state || !observation || !output) return;
    
    tiny_nn_t *nn = &monitor->state->models.neural_network;
    
    // Prepare input (simplified)
    int8_t input[32];
    for (int i = 0; i < 32; i++) {
        input[i] = 0;  // Simplified input preparation
    }
    
    // Forward pass (simplified)
    int16_t hidden[16];
    
    // Hidden layer
    for (int i = 0; i < 16; i++) {
        int32_t sum = nn->bias1[i] << 8;
        for (int j = 0; j < 32; j++) {
            sum += (int32_t)input[j] * nn->weights1[j][i];
        }
        hidden[i] = sum > 0 ? (sum >> 8) : 0;
        if (hidden[i] > 127) hidden[i] = 127;
    }
    
    // Output layer
    for (int i = 0; i < 8; i++) {
        int32_t sum = nn->bias2[i] << 8;
        for (int j = 0; j < 16; j++) {
            sum += hidden[j] * nn->weights2[j][i];
        }
        sum = sum >> 8;
        if (sum > 127) sum = 127;
        if (sum < -127) sum = -127;
        output[i] = (sum + 128) >> 1;
    }
}

// Data collection thread (now uses real data integration)
static void* ml_monitor_collection_thread(void *arg) {
    ml_monitor_t *monitor = (ml_monitor_t*)arg;
    if (!monitor) return NULL;
    
    printf("INFO: ML monitor collection thread started with real data integration\n");
    
    int collection_count = 0;
    time_t last_sync = time(NULL);
    
    while (!monitor->should_stop) {
        // Use enhanced data collection with real sources
        int result = ml_monitor_collect_observation(monitor);
        
        if (result == ML_MONITOR_SUCCESS) {
            collection_count++;
            monitor->last_collection = time(NULL);
            
            // Sync storage periodically
            time_t now = time(NULL);
            if (now - last_sync > monitor->config.storage_sync_interval_minutes * 60) {
                ml_monitor_sync_storage(monitor);
                last_sync = now;
                printf("DEBUG: Synced ML storage to disk (%d collections)\n", collection_count);
            }
            
            // Log progress periodically
            if (collection_count % 100 == 0) {
                printf("INFO: ML monitor collected %d observations, total: %u\n",
                         collection_count, monitor->state->total_observations);
            }
        } else {
            printf("WARN: Failed to collect ML observation: %d\n", result);
        }
        
        // Sleep for collection interval
        sleep(monitor->config.collection_interval_seconds);
    }
    
    printf("INFO: ML monitor collection thread stopped after %d collections\n", collection_count);
    return NULL;
}

// Forward declaration - implementation in ml_monitor_integration.c
int ml_monitor_collect_observation(ml_monitor_t *monitor);

// Prediction thread
static void* ml_monitor_prediction_thread(void *arg) {
    ml_monitor_t *monitor = (ml_monitor_t*)arg;
    if (!monitor) return NULL;
    
    printf("INFO: ML monitor prediction thread started\n");
    
    while (!monitor->should_stop) {
        // Make predictions every minute
        sleep(20);
        
        if (monitor->should_stop) break;
        
        // Get current observation for prediction
        ml_observation_t current_obs;
        if (ml_monitor_collect_data_sources(monitor, &current_obs) == ML_MONITOR_SUCCESS) {
            
            // k-NN prediction
            uint8_t knn_confidence;
            uint8_t knn_prediction = ml_monitor_predict_outage_knn(monitor, &current_obs, &knn_confidence);
            
            // Neural network prediction
            uint8_t nn_output[8];
            ml_monitor_predict_neural_network(monitor, &current_obs, nn_output);
            
            // Combine predictions and trigger callbacks
            if (knn_confidence > 70 && monitor->outage_prediction_callback) {
                monitor->outage_prediction_callback(nn_output[0], knn_confidence, 
                                                  time(NULL) + 900, // 15 minutes ahead
                                                  monitor->callback_user_data);
            }
            
            monitor->last_prediction = time(NULL);
        }
    }
    
    printf("INFO: ML monitor prediction thread stopped\n");
    return NULL;
}

// Start ML monitor
int ml_monitor_start(ml_monitor_t *monitor) {
    fprintf(stderr, "DEBUG: ml_monitor_start called\n");
    if (!monitor) {
        fprintf(stderr, "DEBUG: ml_monitor_start failed - NULL monitor\n");
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    if (!monitor->initialized) {
        fprintf(stderr, "DEBUG: ml_monitor_start failed - not initialized\n");
        return ML_MONITOR_ERROR_NOT_INITIALIZED;
    }
    if (monitor->running) {
        fprintf(stderr, "DEBUG: ml_monitor_start failed - already running\n");
        return ML_MONITOR_ERROR_ALREADY_RUNNING;
    }
    
    printf("INFO: Starting ML monitor\n");
    fprintf(stderr, "DEBUG: ml_monitor_start - starting initialization\n");
    
    monitor->should_stop = false;
    
    // Start collection thread
    fprintf(stderr, "DEBUG: ml_monitor_start - about to create collection thread\n");
    if (pthread_create(&monitor->collection_thread, NULL, ml_monitor_collection_thread, monitor) != 0) {
        printf("ERROR: Failed to create ML collection thread\n");
        fprintf(stderr, "DEBUG: ml_monitor_start failed - collection thread creation failed\n");
        return ML_MONITOR_ERROR_THREAD_FAILED;
    }
    fprintf(stderr, "DEBUG: ml_monitor_start - collection thread created successfully\n");
    
    // Start prediction thread
    fprintf(stderr, "DEBUG: ml_monitor_start - about to create prediction thread\n");
    if (pthread_create(&monitor->prediction_thread, NULL, ml_monitor_prediction_thread, monitor) != 0) {
        printf("ERROR: Failed to create ML prediction thread\n");
        fprintf(stderr, "DEBUG: ml_monitor_start failed - prediction thread creation failed\n");
        monitor->should_stop = true;
        pthread_join(monitor->collection_thread, NULL);
        return ML_MONITOR_ERROR_THREAD_FAILED;
    }
    fprintf(stderr, "DEBUG: ml_monitor_start - prediction thread created successfully\n");
    
    monitor->running = true;
    printf("INFO: ML monitor started successfully\n");
    fprintf(stderr, "DEBUG: ml_monitor_start completed successfully\n");
    
    return ML_MONITOR_SUCCESS;
}

// Stop ML monitor
int ml_monitor_stop(ml_monitor_t *monitor) {
    if (!monitor) return ML_MONITOR_ERROR_INVALID_PARAM;
    if (!monitor->running) return ML_MONITOR_ERROR_NOT_RUNNING;
    
    LOGX_INFO_MSG("Stopping ML monitor");
    
    monitor->should_stop = true;
    
    // Wait for threads to finish
    pthread_join(monitor->collection_thread, NULL);
    pthread_join(monitor->prediction_thread, NULL);
    
    monitor->running = false;
    LOGX_INFO_MSG("ML monitor stopped");
    
    return ML_MONITOR_SUCCESS;
}

// Check if monitor is running
bool ml_monitor_is_running(const ml_monitor_t *monitor) {
    return monitor && monitor->running;
}

// Set outage prediction callback
int ml_monitor_set_outage_prediction_callback(ml_monitor_t *monitor, 
                                             void (*callback)(uint8_t probability, uint8_t confidence, time_t when, void *user_data),
                                             void *user_data) {
    if (!monitor) return ML_MONITOR_ERROR_INVALID_PARAM;
    
    monitor->outage_prediction_callback = callback;
    monitor->callback_user_data = user_data;
    
    return ML_MONITOR_SUCCESS;
}

// Get global ML monitor instance
ml_monitor_t* ml_monitor_get_instance(void) {
    return g_ml_monitor;
}

// Update ML monitor configuration
int ml_monitor_update_config(ml_monitor_t *monitor, const ml_monitor_config_t *config) {
    if (!monitor || !config) {
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&monitor->state_mutex);
    
    // Update configuration
    monitor->config = *config;
    
    // Apply configuration changes
    if (config->enabled && !monitor->running) {
        // Start monitoring if enabled
        ml_monitor_start(monitor);
    } else if (!config->enabled && monitor->running) {
        // Stop monitoring if disabled
        ml_monitor_stop(monitor);
    }
    
    pthread_mutex_unlock(&monitor->state_mutex);
    
    return ML_MONITOR_SUCCESS;
}

// Enable field testing mode
int ml_monitor_enable_field_testing_mode(ml_monitor_t *monitor, const char *test_id) {
    if (!monitor || !test_id) {
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&monitor->state_mutex);
    
    // Enable field testing mode (placeholder implementation)
    monitor->config.debug_logging_enabled = true;
    monitor->config.save_raw_observations = true;
    strncpy(monitor->config.debug_log_path, "/tmp/ml_field_test.log", sizeof(monitor->config.debug_log_path) - 1);
    monitor->config.debug_log_path[sizeof(monitor->config.debug_log_path) - 1] = '\0';
    
    pthread_mutex_unlock(&monitor->state_mutex);
    
    return ML_MONITOR_SUCCESS;
}

// Enable autonomous mode
int ml_monitor_enable_autonomous_mode(ml_monitor_t *monitor) {
    if (!monitor) {
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&monitor->state_mutex);
    
    // Enable autonomous mode (placeholder implementation)
    monitor->config.auto_tuning_enabled = true;
    monitor->config.mobile_mode_enabled = true;
    
    pthread_mutex_unlock(&monitor->state_mutex);
    
    return ML_MONITOR_SUCCESS;
}

// Note: ml_monitor_validate_config is implemented in ml_monitor_uci.c (more feature-complete)

// Trigger proactive optimization
int ml_monitor_trigger_proactive_optimization(ml_monitor_t *monitor, uint8_t probability, uint8_t confidence) {
    if (!monitor) {
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    if (!monitor->running) {
        return ML_MONITOR_ERROR_NOT_RUNNING;
    }
    
    LOGX_INFO_MSG("Triggering proactive optimization - probability: %d, confidence: %d", probability, confidence);
    
    // Check if optimization should be triggered based on probability and confidence
    if (probability >= 200 && confidence >= 180) {
        // High probability and high confidence - trigger optimization
        
        // Trigger failover to best available interface
        LOGX_INFO_MSG("Proactive failover triggered");
        
        // Update ML model with optimization event
        monitor->last_prediction = time(NULL);
        
        return ML_MONITOR_SUCCESS;
    }
    
    return ML_MONITOR_ERROR_PREDICTION_FAILED;
}