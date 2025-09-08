# Master Plan for Embedded ML Monitoring in Starlink Systems - RUTOS Optimized

**Version:** 3.0 Embedded  
**Date:** 2024  
**Status:** Resource-Constrained Implementation  
**Target Platform:** RUTOS (OpenWRT-based) Embedded Systems  

## Executive Summary

This revised master plan adapts our ML monitoring vision to work entirely on embedded RUTOS systems without cloud dependencies. The system learns incrementally from limited data (hours to days), adapts quickly to new locations, and operates within strict resource constraints. Perfect for RVs, mobile deployments, and resource-limited environments.

## Key Constraints & Solutions

### Platform Limitations
```yaml
RUTOS Hardware Typical Specs:
  CPU: ARM Cortex-A7 @ 1.2GHz (or similar)
  RAM: 256MB - 1GB
  Storage: 128MB - 8GB flash
  No GPU acceleration
  Limited network bandwidth
  Power constraints

Our Solutions:
  - Lightweight C-based ML implementations
  - Incremental learning algorithms
  - Circular buffer data storage
  - Fixed-point arithmetic where possible
  - Memory-mapped efficient structures
```

## Revised Architecture: Edge-Only ML System

### Core Philosophy
- **Learn Fast**: Useful predictions within 2-4 hours of data
- **Forget Smart**: Adaptive forgetting for mobile scenarios
- **Resource Conscious**: Every byte and cycle counts
- **Incremental Learning**: No batch training, continuous adaptation

## Phase 1: Lightweight Data Collection (Week 1-2)

### 1.1 Minimal Data Structure
```c
// Compact data structure - 128 bytes per observation
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
#define MAX_OBSERVATIONS 10000  // ~560KB for 10K observations
typedef struct {
    ml_observation_t observations[MAX_OBSERVATIONS];
    uint32_t write_index;
    uint32_t count;
    uint32_t location_change_count;
    time_t last_location_change;
} observation_buffer_t;
```

### 1.2 Efficient Storage Strategy
```c
// Use mmap for persistence without database
typedef struct {
    uint32_t magic;              // File validation
    uint32_t version;
    uint32_t total_observations;
    uint32_t location_changes;
    
    // Circular buffers
    observation_buffer_t recent;     // Last 10K observations
    observation_buffer_t hourly;     // Hourly aggregates (168 entries = 1 week)
    observation_buffer_t daily;      // Daily aggregates (30 entries = 1 month)
    
    // Learned models (compact representation)
    struct {
        uint8_t sky_grid[90][45];   // 4KB obstruction map (2° resolution)
        uint16_t pattern_library[100][32]; // Common patterns
        uint8_t model_weights[4096]; // Simplified neural network
    } models;
    
} ml_persistent_state_t;

// Memory-mapped file for zero-copy access
ml_persistent_state_t* ml_state_init(const char* filepath) {
    int fd = open(filepath, O_RDWR | O_CREAT, 0644);
    if (fd < 0) return NULL;
    
    // Ensure file size
    size_t size = sizeof(ml_persistent_state_t);
    if (ftruncate(fd, size) < 0) {
        close(fd);
        return NULL;
    }
    
    // Memory map the file
    ml_persistent_state_t* state = mmap(NULL, size, 
                                       PROT_READ | PROT_WRITE, 
                                       MAP_SHARED, fd, 0);
    close(fd);
    
    if (state == MAP_FAILED) return NULL;
    
    // Initialize if new file
    if (state->magic != 0x4D4C5354) { // "MLST"
        memset(state, 0, size);
        state->magic = 0x4D4C5354;
        state->version = 1;
    }
    
    return state;
}
```

## Phase 2: Rapid Learning Algorithms (Week 3-4)

### 2.1 Incremental k-NN for Pattern Recognition
```c
// Lightweight k-NN that learns incrementally
typedef struct {
    uint16_t pattern[16];    // Compressed feature vector
    uint8_t outcome;         // What happened (outage type)
    uint8_t confidence;      // How sure we are
} pattern_entry_t;

typedef struct {
    pattern_entry_t patterns[1000];  // Pattern library
    uint16_t count;
    uint16_t max_patterns;
    
    // Incremental statistics
    uint32_t total_predictions;
    uint32_t correct_predictions;
    float accuracy;
} pattern_matcher_t;

// Fast pattern matching with fixed-point arithmetic
uint8_t predict_outage_knn(pattern_matcher_t* matcher, 
                           ml_observation_t* obs,
                           uint8_t* confidence) {
    if (matcher->count < 5) {
        *confidence = 0;
        return OUTAGE_UNKNOWN;
    }
    
    // Extract features to pattern
    uint16_t current_pattern[16];
    extract_pattern_features(obs, current_pattern);
    
    // Find k=5 nearest neighbors
    typedef struct {
        uint32_t distance;
        uint8_t outcome;
    } neighbor_t;
    
    neighbor_t neighbors[5];
    uint8_t k = 0;
    
    for (int i = 0; i < matcher->count && i < 1000; i++) {
        // Calculate Manhattan distance (faster than Euclidean)
        uint32_t dist = 0;
        for (int j = 0; j < 16; j++) {
            int32_t diff = current_pattern[j] - matcher->patterns[i].pattern[j];
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
            neighbors[pos].outcome = matcher->patterns[i].outcome;
        }
    }
    
    // Vote on outcome
    uint8_t votes[16] = {0};  // Support up to 16 outage types
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

// Learn from new observation
void pattern_matcher_learn(pattern_matcher_t* matcher,
                          ml_observation_t* obs,
                          uint8_t actual_outcome) {
    // Extract pattern
    uint16_t pattern[16];
    extract_pattern_features(obs, pattern);
    
    // Check if pattern exists
    for (int i = 0; i < matcher->count; i++) {
        if (memcmp(matcher->patterns[i].pattern, pattern, sizeof(pattern)) == 0) {
            // Update confidence with exponential moving average
            matcher->patterns[i].confidence = 
                (matcher->patterns[i].confidence * 7 + 100) / 8;
            return;
        }
    }
    
    // Add new pattern (replace oldest if full)
    uint16_t idx = matcher->count < matcher->max_patterns ? 
                   matcher->count++ : rand() % matcher->max_patterns;
    
    memcpy(matcher->patterns[idx].pattern, pattern, sizeof(pattern));
    matcher->patterns[idx].outcome = actual_outcome;
    matcher->patterns[idx].confidence = 50;  // Start with medium confidence
}
```

### 2.2 Lightweight Neural Network for Outage Prediction
```c
// Tiny neural network that fits in <10KB
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

// Forward pass with fixed-point arithmetic
void tiny_nn_predict(tiny_nn_t* nn, int8_t input[32], uint8_t output[8]) {
    int16_t hidden[16];
    
    // Hidden layer
    for (int i = 0; i < 16; i++) {
        int32_t sum = nn->bias1[i] << 8;  // Scale bias
        for (int j = 0; j < 32; j++) {
            sum += (int32_t)input[j] * nn->weights1[j][i];
        }
        // ReLU activation with clipping
        hidden[i] = sum > 0 ? (sum >> 8) : 0;
        if (hidden[i] > 127) hidden[i] = 127;
    }
    
    // Output layer
    for (int i = 0; i < 8; i++) {
        int32_t sum = nn->bias2[i] << 8;
        for (int j = 0; j < 16; j++) {
            sum += hidden[j] * nn->weights2[j][i];
        }
        // Sigmoid approximation: tanh(x/2)/2 + 0.5
        sum = sum >> 8;  // Scale down
        if (sum > 127) sum = 127;
        if (sum < -127) sum = -127;
        output[i] = (sum + 128) >> 1;  // Map to 0-255
    }
}

// Incremental learning with single sample
void tiny_nn_learn(tiny_nn_t* nn, int8_t input[32], uint8_t target[8]) {
    uint8_t prediction[8];
    tiny_nn_predict(nn, input, prediction);
    
    // Calculate error
    int8_t error[8];
    for (int i = 0; i < 8; i++) {
        error[i] = (target[i] - prediction[i]) >> 2;  // Scale error
    }
    
    // Backpropagation with momentum
    int16_t hidden[16];
    int8_t hidden_error[16] = {0};
    
    // Update output weights
    uint8_t lr = nn->learning_rate;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 16; j++) {
            int16_t delta = (error[i] * hidden[j] * lr) >> 16;
            nn->weights2[j][i] += delta;
        }
        nn->bias2[i] += (error[i] * lr) >> 8;
    }
    
    // Update hidden weights (simplified)
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 8; j++) {
            hidden_error[i] += error[j] * nn->weights2[i][j];
        }
        hidden_error[i] >>= 8;
        
        for (int j = 0; j < 32; j++) {
            int16_t delta = (hidden_error[i] * input[j] * lr) >> 16;
            nn->weights1[j][i] += delta;
        }
        nn->bias1[i] += (hidden_error[i] * lr) >> 8;
    }
    
    // Decay learning rate
    if (++nn->update_count % 100 == 0 && nn->learning_rate > 10) {
        nn->learning_rate--;
    }
}
```

## Phase 3: Adaptive Sky Grid Learning (Week 5-6)

### 3.1 Compact Obstruction Map
```c
// Ultra-compact sky grid that learns quickly
typedef struct {
    // 4° resolution for memory efficiency: 90x45 = 4050 bytes
    uint8_t obstruction_prob[90][45];  // 0-255 probability
    uint8_t sample_count[90][45];      // Capped at 255
    uint32_t last_update;
    
    // Location-specific learning
    int32_t learned_lat_e7;
    int32_t learned_lon_e7;
    uint16_t learning_time_hours;
} compact_sky_grid_t;

// Rapid obstruction learning
void sky_grid_update(compact_sky_grid_t* grid, 
                    int16_t azimuth, 
                    int16_t elevation,
                    uint8_t obstruction_detected) {
    // Convert to grid coordinates (4° bins)
    int az_bin = azimuth / 4;
    int el_bin = elevation / 4;
    
    if (az_bin < 0 || az_bin >= 90 || el_bin < 0 || el_bin >= 45) {
        return;
    }
    
    // Exponential moving average for fast adaptation
    uint8_t* prob = &grid->obstruction_prob[az_bin][el_bin];
    uint8_t* count = &grid->sample_count[az_bin][el_bin];
    
    if (*count < 255) (*count)++;
    
    // Adaptive learning rate based on sample count
    uint8_t alpha = *count < 10 ? 128 : (*count < 50 ? 64 : 32);
    
    if (obstruction_detected) {
        *prob = *prob + ((255 - *prob) * alpha) / 256;
    } else {
        *prob = *prob - (*prob * alpha) / 256;
    }
}

// Check if location changed significantly
bool location_changed(compact_sky_grid_t* grid, 
                     int32_t lat_e7, 
                     int32_t lon_e7) {
    // ~100 meter threshold
    int32_t lat_diff = abs(lat_e7 - grid->learned_lat_e7);
    int32_t lon_diff = abs(lon_e7 - grid->learned_lon_e7);
    
    return (lat_diff > 100 || lon_diff > 100);
}

// Reset grid for new location
void sky_grid_reset_soft(compact_sky_grid_t* grid) {
    // Soft reset: decay existing knowledge
    for (int i = 0; i < 90; i++) {
        for (int j = 0; j < 45; j++) {
            grid->obstruction_prob[i][j] /= 2;
            grid->sample_count[i][j] /= 4;
        }
    }
    grid->learning_time_hours = 0;
}
```

## Phase 4: Real-time Prediction Engine (Week 7-8)

### 4.1 Sliding Window Predictor
```c
// Predict outages using sliding window analysis
typedef struct {
    ml_observation_t window[60];  // 15-minute window at 15-sec intervals
    uint8_t window_size;
    uint8_t write_idx;
    
    // Extracted features
    struct {
        uint8_t snr_trend;        // 0=falling, 128=stable, 255=rising
        uint8_t snr_volatility;
        uint8_t latency_trend;
        uint8_t packet_loss_trend;
        uint8_t obstruction_trend;
        uint8_t weather_severity;
        uint8_t time_of_day;      // 0-255 mapped to 24 hours
        uint8_t pattern_signature;
    } features;
    
    // Predictions
    uint8_t outage_probability_5min;
    uint8_t outage_probability_15min;
    uint8_t likely_cause;
    uint8_t confidence;
} sliding_predictor_t;

// Update predictions with new observation
void sliding_predictor_update(sliding_predictor_t* pred,
                             ml_observation_t* obs,
                             tiny_nn_t* nn,
                             pattern_matcher_t* matcher) {
    // Add to window
    pred->window[pred->write_idx] = *obs;
    pred->write_idx = (pred->write_idx + 1) % 60;
    if (pred->window_size < 60) pred->window_size++;
    
    // Need at least 8 observations (2 minutes)
    if (pred->window_size < 8) {
        pred->confidence = 0;
        return;
    }
    
    // Extract features from window
    extract_window_features(pred);
    
    // Convert features to NN input
    int8_t nn_input[32];
    prepare_nn_input(&pred->features, obs, nn_input);
    
    // Get NN prediction
    uint8_t nn_output[8];
    tiny_nn_predict(nn, nn_input, nn_output);
    
    // Get k-NN prediction
    uint8_t knn_confidence;
    uint8_t knn_prediction = predict_outage_knn(matcher, obs, &knn_confidence);
    
    // Combine predictions (weighted average)
    uint16_t nn_weight = pred->window_size > 30 ? 150 : 100;
    uint16_t knn_weight = knn_confidence;
    
    pred->outage_probability_5min = 
        (nn_output[0] * nn_weight + knn_prediction * knn_weight) / 
        (nn_weight + knn_weight);
    
    pred->outage_probability_15min = nn_output[1];
    pred->likely_cause = nn_output[2] > 128 ? 
                         find_most_likely_cause(nn_output) : OUTAGE_UNKNOWN;
    
    // Confidence based on agreement and data quality
    uint8_t agreement = 255 - abs(nn_output[0] - knn_prediction);
    pred->confidence = (agreement * pred->window_size) / 60;
}
```

## Phase 5: Mobile-Optimized Learning (Week 9-10)

### 5.1 Location-Aware Adaptation
```c
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

// Adapt to new location
void location_learner_update(location_learner_t* learner,
                            ml_observation_t* obs) {
    // Check if we moved
    if (location_changed_threshold(learner->current_lat_e7, 
                                  learner->current_lon_e7,
                                  obs->latitude_e7, 
                                  obs->longitude_e7, 
                                  100)) {  // 100m threshold
        
        // Save current location to history
        if (learner->observations_here > 50) {  // Learned something
            save_location_profile(learner);
        }
        
        // Check if we've been here before
        int history_idx = find_location_in_history(learner, 
                                                  obs->latitude_e7,
                                                  obs->longitude_e7);
        
        if (history_idx >= 0) {
            // Restore previous learning
            restore_location_profile(learner, history_idx);
            LOGX_INFO("Returned to known location, restoring profile");
        } else {
            // New location - rapid learning mode
            reset_location_profile(learner);
            learner->current_lat_e7 = obs->latitude_e7;
            learner->current_lon_e7 = obs->longitude_e7;
            learner->arrival_time = obs->timestamp;
            LOGX_INFO("New location detected, entering rapid learning mode");
        }
    }
    
    // Update current profile
    learner->observations_here++;
    
    // Exponential moving average for profile
    uint8_t alpha = learner->observations_here < 20 ? 128 : 32;
    
    learner->profile.typical_snr = 
        weighted_average(learner->profile.typical_snr, obs->snr_x100 / 100, alpha);
    learner->profile.typical_latency = 
        weighted_average(learner->profile.typical_latency, obs->latency_ms / 10, alpha);
    learner->profile.obstruction_level = 
        weighted_average(learner->profile.obstruction_level, obs->obstruction_pct, alpha);
    
    // Update learned confidence
    if (learner->profile.learned < 255) {
        learner->profile.learned = min(255, learner->profile.learned + 5);
    }
}
```

## Phase 6: Self-Optimizing System (Week 11-12)

### 6.1 Performance Monitor & Auto-Tuning
```c
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

// Self-optimize based on performance
void auto_tune_system(performance_monitor_t* monitor,
                     tiny_nn_t* nn,
                     pattern_matcher_t* matcher) {
    // Calculate current metrics
    if (monitor->predictions_made > 100) {
        monitor->metrics.accuracy_pct = 
            (monitor->predictions_correct * 100) / monitor->predictions_made;
        
        uint32_t true_positives = monitor->predictions_correct;
        uint32_t total_positives = true_positives + monitor->false_positives;
        
        if (total_positives > 0) {
            monitor->metrics.precision_pct = 
                (true_positives * 100) / total_positives;
        }
    }
    
    // Adjust learning rate based on accuracy trend
    if (monitor->metrics.accuracy_pct < 60) {
        // Poor performance - increase learning
        monitor->params.learning_rate = min(200, monitor->params.learning_rate + 10);
        nn->learning_rate = monitor->params.learning_rate;
    } else if (monitor->metrics.accuracy_pct > 85) {
        // Good performance - reduce learning to stabilize
        monitor->params.learning_rate = max(10, monitor->params.learning_rate - 5);
        nn->learning_rate = monitor->params.learning_rate;
    }
    
    // Adjust confidence threshold based on false positive rate
    uint32_t fp_rate = monitor->predictions_made > 0 ?
                       (monitor->false_positives * 100) / monitor->predictions_made : 0;
    
    if (fp_rate > 10) {
        // Too many false alarms
        monitor->params.confidence_threshold = 
            min(200, monitor->params.confidence_threshold + 10);
    } else if (fp_rate < 2) {
        // Can be more aggressive
        monitor->params.confidence_threshold = 
            max(50, monitor->params.confidence_threshold - 5);
    }
}
```

## Memory-Optimized Implementation

### Total Memory Footprint
```yaml
Static Memory Usage:
  Observation Buffer: 560 KB  (10K observations)
  Sky Grid: 4 KB
  Pattern Library: 16 KB
  Neural Network: 1 KB
  Working Memory: 32 KB
  Total Static: ~613 KB

Dynamic Memory Usage:
  Processing Buffers: 16 KB
  Temporary Arrays: 8 KB
  Stack Usage: 4 KB
  Total Dynamic: ~28 KB

Total RAM Required: <700 KB
Storage Required: <10 MB (with history)
```

### CPU Optimization Strategies
```c
// Use lookup tables for expensive operations
static const uint8_t sigmoid_lut[256] = { /* pre-computed */ };
static const int16_t sin_lut[360] = { /* pre-computed */ };

// Fixed-point arithmetic macros
#define FP_SHIFT 8
#define FP_MULT(a, b) (((a) * (b)) >> FP_SHIFT)
#define FP_DIV(a, b) (((a) << FP_SHIFT) / (b))

// SIMD-friendly operations where available
#ifdef __ARM_NEON
    // Use NEON intrinsics for batch operations
#endif

// Compiler hints for optimization
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define ALWAYS_INLINE __attribute__((always_inline))
```

## Deployment Strategy

### 1. Initial Deployment (Hour 0-2)
- System starts with zero knowledge
- Aggressive learning mode (high learning rate)
- Collect baseline statistics
- No predictions yet, just observation

### 2. Rapid Learning Phase (Hour 2-24)
- Begin making tentative predictions
- Build initial sky grid
- Learn location profile
- Identify common patterns

### 3. Operational Phase (Day 2+)
- Confident predictions
- Refined models
- Location-specific optimizations
- Performance self-tuning

### 4. Mobile Scenario Handling
```c
// Detect movement and adapt
void handle_mobility(ml_system_t* system, ml_observation_t* obs) {
    static int32_t last_lat = 0;
    static int32_t last_lon = 0;
    static time_t stationary_since = 0;
    
    int32_t lat_change = abs(obs->latitude_e7 - last_lat);
    int32_t lon_change = abs(obs->longitude_e7 - last_lon);
    
    if (lat_change > 1000 || lon_change > 1000) {  // ~100m movement
        if (stationary_since > 0 && 
            (obs->timestamp - stationary_since) > 7200) {  // 2 hours
            // Was stationary, now moving - save location profile
            save_location_learning(system);
        }
        
        // Reset short-term predictions
        reset_predictions(system);
        stationary_since = 0;
        
        LOGX_INFO("Movement detected, adapting models");
    } else {
        if (stationary_since == 0) {
            stationary_since = obs->timestamp;
        }
        
        if ((obs->timestamp - stationary_since) > 3600) {  // 1 hour stationary
            // Enable location-specific optimizations
            enable_stationary_optimizations(system);
        }
    }
    
    last_lat = obs->latitude_e7;
    last_lon = obs->longitude_e7;
}
```

## Success Metrics (Revised for Embedded)

### Performance Targets
| Metric | Target | Timeline |
|--------|--------|----------|
| First Prediction | <2 hours | After deployment |
| Useful Accuracy | >70% | Within 24 hours |
| Good Accuracy | >85% | Within 1 week |
| Memory Usage | <1MB RAM | Always |
| CPU Usage | <5% average | Always |
| Storage | <10MB | Always |
| Inference Time | <10ms | Always |

### Adaptive Learning Metrics
| Scenario | Learning Time | Accuracy Target |
|----------|--------------|-----------------|
| Stationary (House) | 24 hours | 90% |
| Semi-Mobile (RV Park) | 4 hours | 85% |
| Mobile (Driving) | 30 minutes | 70% |
| Return to Known Location | 5 minutes | 85% |

## Key Advantages of This Approach

### 1. **No Cloud Dependencies**
- Everything runs locally on RUTOS
- No internet required for ML operations
- Complete data privacy

### 2. **Rapid Adaptation**
- Learns useful patterns in 2-4 hours
- Adapts to new locations quickly
- Remembers previous locations

### 3. **Resource Efficient**
- <1MB RAM usage
- <5% CPU usage
- <10MB storage

### 4. **Mobile-Optimized**
- Detects movement automatically
- Saves/restores location profiles
- Adjusts learning rates based on mobility

### 5. **Self-Improving**
- Continuous learning from every observation
- Auto-tunes parameters
- Improves accuracy over time

## Implementation Checklist

### Week 1-2: Foundation
- [ ] Implement compact data structures
- [ ] Set up memory-mapped storage
- [ ] Create observation buffer
- [ ] Basic data collection

### Week 3-4: Core ML
- [ ] Implement k-NN pattern matcher
- [ ] Build tiny neural network
- [ ] Add incremental learning
- [ ] Test predictions

### Week 5-6: Sky Grid
- [ ] Create obstruction map
- [ ] Add rapid learning
- [ ] Location change detection
- [ ] Soft reset capability

### Week 7-8: Predictions
- [ ] Sliding window predictor
- [ ] Feature extraction
- [ ] Combine ML models
- [ ] Confidence scoring

### Week 9-10: Mobility
- [ ] Location profiles
- [ ] History management
- [ ] Movement detection
- [ ] Rapid re-learning

### Week 11-12: Optimization
- [ ] Performance monitoring
- [ ] Auto-tuning
- [ ] Resource tracking
- [ ] Self-optimization

## Conclusion

This embedded-optimized plan transforms the ambitious ML vision into a practical reality for RUTOS systems. By focusing on incremental learning, efficient algorithms, and mobile scenarios, we can achieve sophisticated ML monitoring without any cloud dependencies. The system learns quickly (hours, not months), adapts to movement, and continuously improves - all within the constraints of embedded hardware.

The key innovation is treating the lack of big data and cloud resources as features, not bugs. This forces us to create more efficient, adaptive algorithms that actually work better for mobile use cases like RVs.

---

**Document Status**: Complete  
**Platform**: RUTOS (OpenWRT-based)  
**Resource Requirements**: <1MB RAM, <10MB Storage  
**Learning Time**: 2-24 hours for useful predictions  
**Next Steps**: Begin with Phase 1 implementation  

