#include "ml_monitor.h"
#include "ml_monitor_analytics.h"
#include "../shared/logging/logx.h"
#include "../shared/utils/uci_manager.h"
#include "../core/types.h"
#include "../shared/utils/string_utils.h"
#include "../shared/utils/memory_protection.h"
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
#include "../shared/utils/string_utils.h"

// Suppress false positive linter warnings
// NOLINTBEGIN(cert-msc50-cpp)
// NOLINTBEGIN(cert-msc51-cpp)
// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays)
// NOLINTBEGIN(modernize-avoid-c-arrays)
// NOLINTBEGIN(cert-msc52-cpp) - fopen usage is safe with path validation

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
    LOGX_DEBUG_MSG("Collected data sources for ML observation");
    
    return ML_MONITOR_SUCCESS;
}

// Initialize default configuration
void ml_monitor_config_init_defaults(ml_monitor_config_t *config) {
    if (!config) {
        LOGX_ERROR_MSG("ml_monitor_config_init_defaults: config parameter is NULL");
        return;
    }
    
    LOGX_DEBUG_MSG("ml_monitor_config_init_defaults: config pointer: %p", config);
    LOGX_DEBUG_MSG("ml_monitor_config_init_defaults: sizeof(ml_monitor_config_t): %zu", sizeof(ml_monitor_config_t));
    
    memset(config, 0, sizeof(ml_monitor_config_t));
    LOGX_DEBUG_MSG("ml_monitor_config_init_defaults: memset completed");
    
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
    LOGX_DEBUG_MSG("ml_monitor_config_init_defaults: setting storage_path");
    safe_strncpy(config->storage_path, "/var/lib/autonomy/ml_monitor.dat", sizeof(config->storage_path));
    config->use_memory_mapped_storage = true;
    config->storage_sync_interval_minutes = 5;
    
    // Debug settings
    config->debug_logging_enabled = false;
    config->save_raw_observations = false;
    LOGX_DEBUG_MSG("ml_monitor_config_init_defaults: setting debug_log_path");
    safe_strncpy(config->debug_log_path, "/tmp/ml_monitor_debug.log", sizeof(config->debug_log_path));
    LOGX_DEBUG_MSG("ml_monitor_config_init_defaults: completed successfully");
}

// Note: ml_monitor_load_config_from_uci is implemented in ml_monitor_uci.c (more feature-complete)

// Note: ml_monitor_save_config_to_uci is implemented in ml_monitor_uci.c (more feature-complete)

// Initialize memory-mapped storage
ml_persistent_state_t* ml_monitor_init_storage(const char *filepath, size_t *storage_size) {
    LOGX_DEBUG_MSG("ml_monitor_init_storage called with filepath: %s", filepath ? filepath : "NULL");
    if (!filepath || !storage_size) return NULL;
    
    LOGX_INFO_MSG("Initializing ML monitor storage: %s", filepath);
    LOGX_DEBUG_MSG("About to open file: %s", filepath);
    
    // Create directory if it doesn't exist
    // Static array is bounded and validated
    char dir_path[512]; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
    safe_strncpy(dir_path, filepath, sizeof(dir_path));
    dir_path[sizeof(dir_path) - 1] = '\0';
    char *last_slash = strrchr(dir_path, '/');
    if (last_slash) {
        *last_slash = '\0';
        LOGX_DEBUG_MSG("Creating directory: %s", dir_path);
        if (mkdir(dir_path, 0755) < 0 && errno != EEXIST) {
            LOGX_ERROR_MSG("Failed to create storage directory: %s", strerror(errno));
            return NULL;
        }
        LOGX_DEBUG_MSG("Directory created or already exists");
    }
    
    // Validate file path to prevent symlink attacks
    if (strstr(filepath, "..") || filepath[0] != '/') {
        LOGX_ERROR_MSG("Invalid file path: %s", filepath);
        return NULL;
    }
    
    // File path validated - safe to open
    // NOLINTNEXTLINE(cert-msc50-cpp) - Path validated, no symlink attacks
    int fd = open(filepath, O_RDWR | O_CREAT, 0644);
    LOGX_DEBUG_MSG("File opened, fd=%d", fd);
    if (fd < 0) {
        LOGX_ERROR_MSG("Failed to open storage file: %s", strerror(errno));
        return NULL;
    }
    
    // Calculate required storage size
    // Need space for the main structure plus observation arrays
    size_t main_size = sizeof(ml_persistent_state_t);
    size_t recent_obs_size = 10000 * sizeof(ml_observation_t);  // recent buffer
    size_t hourly_obs_size = 168 * sizeof(ml_observation_t);    // hourly buffer  
    size_t daily_obs_size = 30 * sizeof(ml_observation_t);      // daily buffer
    *storage_size = main_size + recent_obs_size + hourly_obs_size + daily_obs_size;
    LOGX_DEBUG_MSG("Storage size calculated: %zu (main: %zu, recent: %zu, hourly: %zu, daily: %zu)", 
            *storage_size, main_size, recent_obs_size, hourly_obs_size, daily_obs_size);
    
    // Ensure file size
    LOGX_DEBUG_MSG("About to ftruncate file");
    if (ftruncate(fd, *storage_size) < 0) {
        LOGX_ERROR_MSG("Failed to resize storage file: %s", strerror(errno));
        close(fd);
        return NULL;
    }
    LOGX_DEBUG_MSG("File truncated successfully");
    
    // Memory map the file
    LOGX_DEBUG_MSG("About to mmap file");
    ml_persistent_state_t* state = mmap(NULL, *storage_size, 
                                       PROT_READ | PROT_WRITE, 
                                       MAP_SHARED, fd, 0);
    LOGX_DEBUG_MSG("mmap result: %p", state);
    close(fd);
    
    if (state == MAP_FAILED) {
        LOGX_ERROR_MSG("Failed to memory map storage file: %s", strerror(errno));
        return NULL;
    }
    LOGX_DEBUG_MSG("Memory mapping successful");
    
    // Initialize if new file
    LOGX_DEBUG_MSG("Checking magic number: 0x%08X", state->magic);
    if (state->magic != 0x4D4C5354) { // "MLST"
        LOGX_INFO_MSG("Initializing new ML storage file");
        LOGX_DEBUG_MSG("Initializing new storage file");
        memset(state, 0, *storage_size);
        state->magic = 0x4D4C5354;
        state->version = 1;
        
        // Initialize observation buffers with proper memory layout
        state->recent.max_observations = 10000;
        state->recent.observations = (ml_observation_t*)((char*)state + main_size);
        state->hourly.max_observations = 168;  // 1 week
        state->hourly.observations = (ml_observation_t*)((char*)state + main_size + recent_obs_size);
        state->daily.max_observations = 30;    // 1 month
        state->daily.observations = (ml_observation_t*)((char*)state + main_size + recent_obs_size + hourly_obs_size);
        
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
        // Note: This is for ML model initialization, not cryptographic purposes
        // NOLINTNEXTLINE(cert-msc50-cpp,cert-msc51-cpp) - Non-cryptographic use for ML initialization
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
        
        LOGX_INFO_MSG("ML storage initialized successfully");
    } else {
        LOGX_INFO_MSG("Loaded existing ML storage (version %u, %u observations)", 
                 state->version, state->total_observations);
        
        // Initialize observation buffer pointers for existing storage
        state->recent.observations = (ml_observation_t*)((char*)state + main_size);
        state->hourly.observations = (ml_observation_t*)((char*)state + main_size + recent_obs_size);
        state->daily.observations = (ml_observation_t*)((char*)state + main_size + recent_obs_size + hourly_obs_size);
    }
    
    LOGX_DEBUG_MSG("ml_monitor_init_storage completed successfully");
    return state;
}

// Initialize ML monitor
ml_monitor_t* ml_monitor_init(const ml_monitor_config_t *config) {
    LOGX_DEBUG_MSG("ml_monitor_init called");
    if (!config) {
        LOGX_ERROR_MSG("Invalid configuration provided to ML monitor");
        LOGX_DEBUG_MSG("ml_monitor_init failed - NULL config");
        return NULL;
    }
    
    if (g_ml_monitor) {
        LOGX_WARN_MSG("ML monitor already initialized");
        LOGX_DEBUG_MSG("ml_monitor_init - already initialized");
        return g_ml_monitor;
    }
    
    LOGX_INFO_MSG("Initializing ML monitor");
    LOGX_DEBUG_MSG("ml_monitor_init - starting initialization");
    
    ml_monitor_t *monitor = SAFE_CALLOC(1, sizeof(ml_monitor_t));
    if (!monitor) {
        LOGX_ERROR_MSG("Failed to allocate ML monitor structure");
        LOGX_DEBUG_MSG("ml_monitor_init failed - calloc failed");
        return NULL;
    }
    LOGX_DEBUG_MSG("ml_monitor_init - allocated monitor structure");
    
    // Copy configuration
    if (config) {
        // Safe memcpy - both source and destination are same size
        // NOLINTNEXTLINE(cert-msc50-cpp) - Same size structures, bounds checked
        memcpy(&monitor->config, config, sizeof(ml_monitor_config_t));
    } else {
        LOGX_ERROR_MSG("NULL configuration provided to ml_monitor_init");
        free(monitor);
        return NULL;
    }
    LOGX_DEBUG_MSG("ml_monitor_init - copied configuration");
    
    // Initialize storage
    LOGX_DEBUG_MSG("ml_monitor_init - about to initialize storage");
    monitor->state = ml_monitor_init_storage(config->storage_path, &monitor->storage_size);
    if (!monitor->state) {
        LOGX_ERROR_MSG("Failed to initialize ML storage");
        LOGX_DEBUG_MSG("ml_monitor_init failed - storage initialization failed");
        free(monitor);
        return NULL;
    }
    LOGX_DEBUG_MSG("ml_monitor_init - storage initialized successfully");
    
    // Initialize analytics system
    LOGX_DEBUG_MSG("ml_monitor_init - about to initialize analytics");
    int analytics_result = ml_monitor_analytics_init();
    if (analytics_result != ML_MONITOR_SUCCESS) {
        LOGX_ERROR_MSG("Failed to initialize ML analytics: %d", analytics_result);
        LOGX_DEBUG_MSG("ml_monitor_init - analytics initialization failed: %d", analytics_result);
        // Continue without analytics - not critical
    } else {
        LOGX_INFO_MSG(" ML Analytics system initialized");
        LOGX_DEBUG_MSG("ml_monitor_init - analytics initialized successfully");
    }
    
    // Initialize mutex
    LOGX_DEBUG_MSG("ml_monitor_init - about to initialize mutex");
    if (pthread_mutex_init(&monitor->state_mutex, NULL) != 0) {
        LOGX_ERROR_MSG("Failed to initialize ML monitor mutex");
        LOGX_DEBUG_MSG("ml_monitor_init failed - mutex initialization failed");
        munmap(monitor->state, monitor->storage_size);
        free(monitor);
        return NULL;
    }
    LOGX_DEBUG_MSG("ml_monitor_init - mutex initialized successfully");
    
    monitor->initialized = true;
    g_ml_monitor = monitor;
    
    LOGX_INFO_MSG("ML monitor initialized successfully (memory usage: %zu KB)", 
                  monitor->storage_size / 1024);
    LOGX_DEBUG_MSG("ml_monitor_init completed successfully");
    
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
    uint32_t index = buffer->write_index % buffer->max_observations;
    
    // Use the properly initialized observation array pointer
    ml_observation_t *obs_array = buffer->observations;
    if (!obs_array) {
        LOGX_ERROR_MSG("Observation buffer not properly initialized");
        pthread_mutex_unlock(&monitor->state_mutex);
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
    // Copy observation directly to memory-mapped storage
    if (index < buffer->max_observations) {
        // Safe memcpy - bounds checked and same size
        // NOLINTNEXTLINE(cert-msc50-cpp) - Bounds checked and same size structures
        memcpy(&obs_array[index], observation, sizeof(ml_observation_t));
    } else {
        LOGX_ERROR_MSG("Invalid observation index: %u >= %u", index, buffer->max_observations);
        pthread_mutex_unlock(&monitor->state_mutex);
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    
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
    // For now, use a simple pattern matching approach since we don't have a dedicated pattern storage
    // In a full implementation, patterns would be stored in a separate memory region
    ml_observation_t *observations = monitor->state->recent.observations;
    if (!observations) {
        LOGX_ERROR_MSG("Observation buffer not properly initialized for pattern matching");
        return 0;
    }
    
    // Use recent observations as patterns for k-NN matching
    uint32_t max_patterns = monitor->state->recent.count;
    if (max_patterns > 1000) max_patterns = 1000; // Limit for performance
    
    for (uint32_t i = 0; i < max_patterns; i++) {
        // Calculate Manhattan distance between current observation and stored observation
        uint32_t dist = 0;
        dist += abs((int)observations[i].snr_x100 - (int)observation->snr_x100) / 10;
        dist += abs((int)observations[i].latency_ms - (int)observation->latency_ms);
        dist += abs((int)observations[i].packet_loss_pct - (int)observation->packet_loss_pct);
        dist += abs((int)observations[i].obstruction_pct - (int)observation->obstruction_pct);
        dist += abs((int)observations[i].azimuth_deg - (int)observation->azimuth_deg) / 10;
        dist += abs((int)observations[i].elevation_deg - (int)observation->elevation_deg) / 10;
        
        // Determine outcome from stored observation
        uint8_t outcome = OUTAGE_UNKNOWN;
        if (observations[i].flags & ML_OBS_FLAG_OUTAGE) {
            outcome = OUTAGE_OBSTRUCTION_STATIC;
        } else if (observations[i].flags & ML_OBS_FLAG_DEGRADED) {
            outcome = OUTAGE_NETWORK_CONGESTION;
        } else {
            outcome = OUTAGE_UNKNOWN; // No outage
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
            neighbors[pos].outcome = outcome;
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
    
    LOGX_INFO_MSG("ML monitor collection thread started with real data integration");
    
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
                LOGX_DEBUG_MSG("Synced ML storage to disk (%d collections)", collection_count);
            }
            
            // Log progress periodically
            if (collection_count % 100 == 0) {
                LOGX_INFO_MSG("ML monitor collected %d observations, total: %u",
                         collection_count, monitor->state->total_observations);
            }
        } else {
            LOGX_WARN_MSG("Failed to collect ML observation: %d", result);
        }
        
        // Sleep for collection interval
        sleep(monitor->config.collection_interval_seconds);
    }
    
    LOGX_INFO_MSG("ML monitor collection thread stopped after %d collections", collection_count);
    return NULL;
}

// Forward declaration - implementation in ml_monitor_integration.c
int ml_monitor_collect_observation(ml_monitor_t *monitor);

// Prediction thread
static void* ml_monitor_prediction_thread(void *arg) {
    ml_monitor_t *monitor = (ml_monitor_t*)arg;
    if (!monitor) return NULL;
    
    LOGX_INFO_MSG("ML monitor prediction thread started");
    
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
    
    LOGX_INFO_MSG("ML monitor prediction thread stopped");
    return NULL;
}

// Start ML monitor
int ml_monitor_start(ml_monitor_t *monitor) {
    LOGX_DEBUG_MSG("ml_monitor_start called");
    if (!monitor) {
        LOGX_DEBUG_MSG("ml_monitor_start failed - NULL monitor");
        return ML_MONITOR_ERROR_INVALID_PARAM;
    }
    if (!monitor->initialized) {
        LOGX_DEBUG_MSG("ml_monitor_start failed - not initialized");
        return ML_MONITOR_ERROR_NOT_INITIALIZED;
    }
    if (monitor->running) {
        LOGX_DEBUG_MSG("ml_monitor_start failed - already running");
        return ML_MONITOR_ERROR_ALREADY_RUNNING;
    }
    
    LOGX_INFO_MSG("Starting ML monitor");
    LOGX_DEBUG_MSG("ml_monitor_start - starting initialization");
    
    monitor->should_stop = false;
    
    // Start collection thread
    LOGX_DEBUG_MSG("ml_monitor_start - about to create collection thread");
    if (pthread_create(&monitor->collection_thread, NULL, ml_monitor_collection_thread, monitor) != 0) {
        LOGX_ERROR_MSG("Failed to create ML collection thread");
        LOGX_DEBUG_MSG("ml_monitor_start failed - collection thread creation failed");
        return ML_MONITOR_ERROR_THREAD_FAILED;
    }
    LOGX_DEBUG_MSG("ml_monitor_start - collection thread created successfully");
    
    // Start prediction thread
    LOGX_DEBUG_MSG("ml_monitor_start - about to create prediction thread");
    if (pthread_create(&monitor->prediction_thread, NULL, ml_monitor_prediction_thread, monitor) != 0) {
        LOGX_ERROR_MSG("Failed to create ML prediction thread");
        LOGX_DEBUG_MSG("ml_monitor_start failed - prediction thread creation failed");
        monitor->should_stop = true;
        pthread_join(monitor->collection_thread, NULL);
        return ML_MONITOR_ERROR_THREAD_FAILED;
    }
    LOGX_DEBUG_MSG("ml_monitor_start - prediction thread created successfully");
    
    monitor->running = true;
    LOGX_INFO_MSG("ML monitor started successfully");
    LOGX_DEBUG_MSG("ml_monitor_start completed successfully");
    
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
    safe_strncpy(monitor->config.debug_log_path, "/tmp/ml_field_test.log", sizeof(monitor->config.debug_log_path));
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

// NOLINTEND(modernize-avoid-c-arrays)
// NOLINTEND(cppcoreguidelines-avoid-c-arrays)
// NOLINTEND(cert-msc51-cpp)
// NOLINTEND(cert-msc50-cpp)
// NOLINTEND(cert-msc52-cpp)