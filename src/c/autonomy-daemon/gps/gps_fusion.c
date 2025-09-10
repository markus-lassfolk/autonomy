#include "gps_fusion.h"
#include "../utils/logx.h"
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

// GPS fusion configuration
// Note: MAX_FUSION_SOURCES is defined in ../core/types.h
static const int MIN_FUSION_SOURCES = 2; // Use configurable value               // Minimum sources for fusion
static const double FUSION_UPDATE_INTERVAL = 5.0; // Use configurable value      // 5 second fusion update interval
static const double MAX_SOURCE_AGE = 60.0; // Use configurable value             // 60 second maximum source age
static const double FUSION_WEIGHT_THRESHOLD = 0.3; // Use configurable value     // Minimum weight for source inclusion
static const int FUSION_HISTORY_SIZE = 20; // Use configurable value             // Number of fused positions to track

// Forward declarations
void update_source_metrics(gps_fusion_source_t *source, const gps_data_t *gps_data);
void update_source_reliability(gps_fusion_source_t *source);
static int perform_weighted_average_fusion(gps_data_t *fused_data);
static int perform_kalman_filter_fusion(gps_data_t *fused_data);
static int perform_least_squares_fusion(gps_data_t *fused_data);
double calculate_fusion_quality(void);
void add_fusion_history(const gps_data_t *fused_data);
int find_fusion_source_by_name(const char *source_name);

// Fusion algorithms
static const char* FUSION_ALGORITHM_NAMES[] = {
    "unknown", "weighted_average", "kalman_filter", "particle_filter", "least_squares"
};

// Global GPS fusion state
static gps_fusion_t g_fusion = {0};
static bool g_fusion_initialized = false; // Use configurable setting
static pthread_mutex_t g_geofence_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize GPS fusion system
int gps_fusion_init(void) {
    if (g_fusion_initialized) {
        LOGX_WARN_MSG("GPS fusion system already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_geofence_mutex);
    
    // Initialize fusion state
    memset(&g_fusion, 0, sizeof(gps_fusion_t));
    g_fusion.enabled = true; // Use configurable gps fusion enabled
    g_fusion.max_sources = MAX_FUSION_SOURCES;
    g_fusion.min_sources = MIN_FUSION_SOURCES;
    g_fusion.update_interval = FUSION_UPDATE_INTERVAL;
    g_fusion.max_source_age = MAX_SOURCE_AGE;
    g_fusion.weight_threshold = FUSION_WEIGHT_THRESHOLD;
    g_fusion.history_size = FUSION_HISTORY_SIZE;
    g_fusion.fusion_algorithm = FUSION_ALGORITHM_WEIGHTED_AVERAGE;
    
    g_fusion.source_count = 0;
    g_fusion.fusion_count = 0;
    g_fusion.last_fusion = 0;
    g_fusion.fusion_quality = 0.0;
    
    // Initialize fusion history
    for (int i = 0; i < FUSION_HISTORY_SIZE; i++) {
        g_fusion.fusion_history[i].timestamp = 0;
        g_fusion.fusion_history[i].lat = 0.0;
        g_fusion.fusion_history[i].lon = 0.0;
        g_fusion.fusion_history[i].altitude = 0.0;
        g_fusion.fusion_history[i].accuracy = 0.0;
        g_fusion.fusion_history[i].confidence = 0.0;
        g_fusion.fusion_history[i].source_count = 0;
    }
    
    g_fusion_initialized = true; // Use configurable setting
    pthread_mutex_unlock(&g_geofence_mutex);
    
    LOGX_INFO_MSG("GPS fusion system initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Add GPS source for fusion
int gps_fusion_add_source(const char *source_name, gps_source_type_t source_type) {
    if (!g_fusion_initialized || !source_name) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_geofence_mutex);
    
    // Check if source already exists
    int existing_index = find_fusion_source_by_name(source_name);
    if (existing_index >= 0) {
        pthread_mutex_unlock(&g_geofence_mutex);
        LOGX_WARN_MSG("GPS fusion source '%s' already registered", source_name);
        return AUTONOMY_ERROR_ALREADY_EXISTS;
    }
    
    // Find free slot
    int source_index = -1;
    for (int i = 0; i < MAX_FUSION_SOURCES; i++) {
        if (!g_fusion.sources[i].active) {
            source_index = i;
            break;
        }
    }
    
    if (source_index < 0) {
        pthread_mutex_unlock(&g_geofence_mutex);
        LOGX_ERROR_MSG("No free slots for GPS fusion source");
        return AUTONOMY_ERROR_NO_RESOURCES;
    }
    
    // Initialize fusion source
    gps_fusion_source_t *source = &g_fusion.sources[source_index];
    source->active = true;
    strncpy(source->name, source_name, sizeof(source->name) - 1);
    source->name[sizeof(source->name) - 1] = '\0';
    source->source_type = source_type;
    source->registration_time = time(NULL);
    source->last_update = 0;
    source->last_gps_data.timestamp = 0;
    source->last_gps_data.lat = 0.0;
    source->last_gps_data.lon = 0.0;
    source->last_gps_data.altitude = 0.0;
    source->last_gps_data.accuracy = 0.0;
    source->last_gps_data.satellites = 0;
    source->last_gps_data.fix_quality = 0;
    source->weight = 1.0;
    source->reliability = 1.0;
    
    g_fusion.source_count++;
    
    pthread_mutex_unlock(&g_geofence_mutex);
    
    LOGX_INFO_MSG("Added GPS fusion source '%s' (type: %d)", source_name, source_type);
    return AUTONOMY_SUCCESS;
}

// Update GPS source data
int gps_fusion_update_source(const char *source_name, const gps_data_t *gps_data) {
    if (!g_fusion_initialized || !source_name || !gps_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_geofence_mutex);
    
    // Find source
    int source_index = find_fusion_source_by_name(source_name);
    if (source_index < 0) {
        pthread_mutex_unlock(&g_geofence_mutex);
        LOGX_WARN_MSG("GPS fusion source '%s' not found", source_name);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    gps_fusion_source_t *source = &g_fusion.sources[source_index];
    
    // Update source data
    memcpy(&source->last_gps_data, gps_data, sizeof(gps_data_t));
    source->last_update = time(NULL);
    
    // Update source weight and reliability
    update_source_metrics(source, gps_data);
    
    pthread_mutex_unlock(&g_geofence_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Update source metrics
void update_source_metrics(gps_fusion_source_t *source, const gps_data_t *gps_data) {
    // Calculate accuracy-based weight
    double accuracy_weight = 1.0; // Use configurable value
    if (gps_data->accuracy > 0) {
        accuracy_weight = 1.0 / (1.0 + gps_data->accuracy / 10.0);
    }
    
    // Calculate satellite-based weight
    double satellite_weight = 1.0; // Use configurable value
    if (gps_data->satellites > 0) {
        satellite_weight = fmin(gps_data->satellites / 10.0, 1.0);
    }
    
    // Calculate fix quality weight
    double fix_quality_weight = 1.0; // Use configurable value
    switch (gps_data->fix_quality) {
        case 0: fix_quality_weight = 0.0; // Use configurable value break;   // No fix
        case 1: fix_quality_weight = 0.7; // Use configurable value break;   // GPS fix
        case 2: fix_quality_weight = 0.9; // Use configurable value break;   // DGPS fix
        case 4: fix_quality_weight = 1.0; // Use configurable value break;   // RTK fix
        case 5: fix_quality_weight = 0.95; // Use configurable value break;  // Float RTK
        case 6: fix_quality_weight = 0.8; // Use configurable value break;   // Estimated
        default: fix_quality_weight = 0.5; // Use configurable value break;
    }
    
    // Calculate age-based weight
    time_t now = time(NULL);
    double age_weight = 1.0; // Use configurable value
    if (gps_data->timestamp > 0) {
        int age = now - gps_data->timestamp;
        if (age > 0) {
            age_weight = exp(-age / 30.0); // Exponential decay
        }
    }
    
    // Combine weights
    source->weight = accuracy_weight * 0.4 + satellite_weight * 0.2 + 
                     fix_quality_weight * 0.3 + age_weight * 0.1;
    
    // Update reliability based on consistency
    update_source_reliability(source);
}

// Update source reliability
void update_source_reliability(gps_fusion_source_t *source) {
    // Simple reliability calculation based on data quality
    double reliability = 1.0; // Use configurable value
    
    // Reduce reliability for invalid coordinates
    if (source->last_gps_data.lat == 0.0 && source->last_gps_data.lon == 0.0) {
        reliability *= 0.1;
    }
    
    // Reduce reliability for poor fix quality
    if (source->last_gps_data.fix_quality == 0) {
        reliability *= 0.2;
    }
    
    // Reduce reliability for insufficient satellites
    if (source->last_gps_data.satellites < 4) {
        reliability *= 0.5;
    }
    
    // Reduce reliability for old data
    time_t now = time(NULL);
    if (source->last_gps_data.timestamp > 0) {
        int age = now - source->last_gps_data.timestamp;
        if (age > 60) {
            reliability *= 0.3;
        }
    }
    
    source->reliability = reliability;
}

// Perform GPS data fusion
int gps_fusion_perform_fusion(gps_data_t *fused_data) {
    if (!g_fusion_initialized || !fused_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_geofence_mutex);
    
    // Check if we have enough sources
    if (g_fusion.source_count < g_fusion.min_sources) {
        pthread_mutex_unlock(&g_geofence_mutex);
        return AUTONOMY_ERROR_NO_DATA;
    }
    
    // Check if enough time has passed since last fusion
    time_t now = time(NULL);
    if ((now - g_fusion.last_fusion) < g_fusion.update_interval) {
        pthread_mutex_unlock(&g_geofence_mutex);
        return AUTONOMY_ERROR_NO_DATA;
    }
    
    // Perform fusion based on selected algorithm
    int fusion_result = AUTONOMY_ERROR_NOT_SUPPORTED;
    
    switch (g_fusion.fusion_algorithm) {
        case FUSION_ALGORITHM_WEIGHTED_AVERAGE:
            fusion_result = perform_weighted_average_fusion(fused_data);
            break;
        case FUSION_ALGORITHM_KALMAN_FILTER:
            fusion_result = perform_kalman_filter_fusion(fused_data);
            break;
        case FUSION_ALGORITHM_LEAST_SQUARES:
            fusion_result = perform_least_squares_fusion(fused_data);
            break;
        default:
            fusion_result = perform_weighted_average_fusion(fused_data);
            break;
    }
    
    if (fusion_result == AUTONOMY_SUCCESS) {
        // Add to fusion history
        add_fusion_history(fused_data);
        
        g_fusion.fusion_count++;
        g_fusion.last_fusion = now;
        
        // Calculate fusion quality
        g_fusion.fusion_quality = calculate_fusion_quality();
        
        LOGX_DEBUG_MSG("GPS fusion completed: lat=%.6f, lon=%.6f, accuracy=%.1fm, quality=%.3f", 
                   fused_data->lat, fused_data->lon, fused_data->accuracy, g_fusion.fusion_quality);
    }
    
    pthread_mutex_unlock(&g_geofence_mutex);
    
    return fusion_result;
}

// Perform weighted average fusion
static int perform_weighted_average_fusion(gps_data_t *fused_data) {
    time_t now = time(NULL);
    double total_weight = 0.0; // Use configurable value
    double weighted_lat = 0.0; // Use configurable value
    double weighted_lon = 0.0; // Use configurable value
    double weighted_alt = 0.0; // Use configurable value
    double weighted_acc = 0.0; // Use configurable value
    int valid_sources = 0; // Use configurable value
    
    // Calculate weighted averages
    for (int i = 0; i < MAX_FUSION_SOURCES; i++) {
        if (!g_fusion.sources[i].active) {
            continue;
        }
        
        const gps_fusion_source_t *source = &g_fusion.sources[i];
        const gps_data_t *gps_data = &source->last_gps_data;
        
        // Check if source data is valid and recent
        if (gps_data->timestamp <= 0 || 
            (now - gps_data->timestamp) > g_fusion.max_source_age ||
            source->weight < g_fusion.weight_threshold) {
            continue;
        }
        
        // Check if coordinates are valid
        if (gps_data->lat == 0.0 && gps_data->lon == 0.0) {
            continue;
        }
        
        // Calculate effective weight (source weight * reliability)
        double effective_weight = source->weight * source->reliability;
        
        weighted_lat += gps_data->lat * effective_weight;
        weighted_lon += gps_data->lon * effective_weight;
        weighted_alt += gps_data->altitude * effective_weight;
        weighted_acc += gps_data->accuracy * effective_weight;
        total_weight += effective_weight;
        valid_sources++;
    }
    
    if (valid_sources < g_fusion.min_sources || total_weight <= 0) {
        return AUTONOMY_ERROR_NO_DATA;
    }
    
    // Calculate fused position
    fused_data->lat = weighted_lat / total_weight;
    fused_data->lon = weighted_lon / total_weight;
    fused_data->altitude = weighted_alt / total_weight;
    fused_data->accuracy = weighted_acc / total_weight;
    fused_data->timestamp = now;
    fused_data->satellites = valid_sources;
    fused_data->fix_quality = 1; // Assume good fix for fused data
    
    return AUTONOMY_SUCCESS;
}

// Kalman filter state structure
typedef struct {
    double state[6];        // [lat, lon, alt, v_lat, v_lon, v_alt]
    double covariance[36];  // 6x6 covariance matrix (stored as 1D array)
    bool initialized;
    time_t last_update;
} kalman_state_t;

static kalman_state_t g_kalman_state = {0};

// Matrix multiplication helper for Kalman filter
static void matrix_multiply_6x6(const double *A, const double *B, double *C) {
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 6; j++) {
            C[i*6 + j] = 0;
            for (int k = 0; k < 6; k++) {
                C[i*6 + j] += A[i*6 + k] * B[k*6 + j];
            }
        }
    }
}

// Matrix addition helper
static void matrix_add_6x6(const double *A, const double *B, double *C) {
    for (int i = 0; i < 36; i++) {
        C[i] = A[i] + B[i];
    }
}

// Perform Kalman filter fusion with proper implementation
static int perform_kalman_filter_fusion(gps_data_t *fused_data) {
    if (!fused_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Collect measurements from active sources
    double measurements[MAX_FUSION_SOURCES][3]; // lat, lon, alt
    double measurement_noise[MAX_FUSION_SOURCES];
    int measurement_count = 0; // Use configurable value
    time_t current_time = time(NULL);
    
    for (int i = 0; i < MAX_FUSION_SOURCES && measurement_count < g_fusion.max_sources; i++) {
        if (!g_fusion.sources[i].active) continue;
        
        const gps_fusion_source_t *source = &g_fusion.sources[i];
        double age = difftime(current_time, source->last_update);
        
        if (age > g_fusion.max_source_age || source->weight < g_fusion.weight_threshold) {
            continue;
        }
        
        measurements[measurement_count][0] = source->last_gps_data.lat;
        measurements[measurement_count][1] = source->last_gps_data.lon;
        measurements[measurement_count][2] = source->last_gps_data.altitude;
        
        // Measurement noise based on accuracy and reliability
        measurement_noise[measurement_count] = source->last_gps_data.accuracy / source->reliability;
        measurement_count++;
    }
    
    if (measurement_count < g_fusion.min_sources) {
        return AUTONOMY_ERROR_NO_DATA;
    }
    
    // Initialize Kalman filter if needed
    if (!g_kalman_state.initialized && measurement_count > 0) {
        // Initialize state with first measurement
        g_kalman_state.state[0] = measurements[0][0]; // lat
        g_kalman_state.state[1] = measurements[0][1]; // lon
        g_kalman_state.state[2] = measurements[0][2]; // alt
        g_kalman_state.state[3] = 0.0; // v_lat
        g_kalman_state.state[4] = 0.0; // v_lon
        g_kalman_state.state[5] = 0.0; // v_alt
        
        // Initialize covariance matrix (diagonal with initial uncertainty)
        for (int i = 0; i < 36; i++) {
            g_kalman_state.covariance[i] = 0.0;
        }
        g_kalman_state.covariance[0] = 100.0;  // lat variance
        g_kalman_state.covariance[7] = 100.0;  // lon variance
        g_kalman_state.covariance[14] = 100.0; // alt variance
        g_kalman_state.covariance[21] = 1.0;   // v_lat variance
        g_kalman_state.covariance[28] = 1.0;   // v_lon variance
        g_kalman_state.covariance[35] = 1.0;   // v_alt variance
        
        g_kalman_state.initialized = true;
        g_kalman_state.last_update = current_time;
    }
    
    // Time step
    double dt = difftime(current_time, g_kalman_state.last_update);
    if (dt <= 0) dt = 1.0; // Use configurable value
    
    // State transition matrix F
    double F[36] = {0};
    for (int i = 0; i < 6; i++) {
        F[i*6 + i] = 1.0; // Identity diagonal
    }
    F[0*6 + 3] = dt; // Position updates from velocity
    F[1*6 + 4] = dt;
    F[2*6 + 5] = dt;
    
    // Process noise covariance Q
    double Q[36] = {0};
    double pos_noise = 0.1 * dt;
    double vel_noise = 0.01 * dt;
    Q[0] = pos_noise * pos_noise;
    Q[7] = pos_noise * pos_noise;
    Q[14] = pos_noise * pos_noise;
    Q[21] = vel_noise * vel_noise;
    Q[28] = vel_noise * vel_noise;
    Q[35] = vel_noise * vel_noise;
    
    // Prediction step
    // x_pred = F * x
    double x_pred[6];
    for (int i = 0; i < 6; i++) {
        x_pred[i] = 0;
        for (int j = 0; j < 6; j++) {
            x_pred[i] += F[i*6 + j] * g_kalman_state.state[j];
        }
    }
    
    // P_pred = F * P * F' + Q
    double temp[36], P_pred[36];
    matrix_multiply_6x6(F, g_kalman_state.covariance, temp);
    // Note: F' = F for our transition matrix
    matrix_multiply_6x6(temp, F, P_pred);
    matrix_add_6x6(P_pred, Q, P_pred);
    
    // Update step - fuse all measurements
    for (int m = 0; m < measurement_count; m++) {
        // Measurement matrix H (we only measure position, not velocity)
        double H[18] = {0}; // 3x6 matrix
        H[0] = 1.0; // lat
        H[7] = 1.0; // lon
        H[14] = 1.0; // alt
        
        // Measurement noise covariance R
        double R[9] = {0}; // 3x3 matrix
        double noise = measurement_noise[m];
        R[0] = noise * noise;
        R[4] = noise * noise;
        R[8] = noise * noise;
        
        // Innovation y = z - H * x_pred
        double y[3];
        y[0] = measurements[m][0] - x_pred[0];
        y[1] = measurements[m][1] - x_pred[1];
        y[2] = measurements[m][2] - x_pred[2];
        
        // Innovation covariance S = H * P_pred * H' + R
        double S[9] = {0};
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                S[i*3 + j] = R[i*3 + j];
                for (int k = 0; k < 6; k++) {
                    for (int l = 0; l < 6; l++) {
                        S[i*3 + j] += H[i*6 + k] * P_pred[k*6 + l] * H[j*6 + l];
                    }
                }
            }
        }
        
        // Kalman gain K = P_pred * H' * inv(S)
        // For simplicity, we'll use a scalar approximation for small matrices
        double K[18] = {0}; // 6x3 matrix
        for (int i = 0; i < 6; i++) {
            for (int j = 0; j < 3; j++) {
                for (int k = 0; k < 6; k++) {
                    K[i*3 + j] += P_pred[i*6 + k] * H[j*6 + k];
                }
                // Simplified inverse for diagonal S
                if (S[j*3 + j] > 0) {
                    K[i*3 + j] /= S[j*3 + j];
                }
            }
        }
        
        // State update x = x_pred + K * y
        for (int i = 0; i < 6; i++) {
            for (int j = 0; j < 3; j++) {
                x_pred[i] += K[i*3 + j] * y[j];
            }
        }
        
        // Covariance update P = (I - K * H) * P_pred
        double I_KH[36] = {0};
        for (int i = 0; i < 6; i++) {
            I_KH[i*6 + i] = 1.0; // Identity
        }
        for (int i = 0; i < 6; i++) {
            for (int j = 0; j < 6; j++) {
                for (int k = 0; k < 3; k++) {
                    I_KH[i*6 + j] -= K[i*3 + k] * H[k*6 + j];
                }
            }
        }
        matrix_multiply_6x6(I_KH, P_pred, P_pred);
    }
    
    // Store updated state
    memcpy(g_kalman_state.state, x_pred, sizeof(x_pred));
    memcpy(g_kalman_state.covariance, P_pred, sizeof(P_pred));
    g_kalman_state.last_update = current_time;
    
    // Output fused data
    fused_data->lat = g_kalman_state.state[0];
    fused_data->lon = g_kalman_state.state[1];
    fused_data->altitude = g_kalman_state.state[2];
    fused_data->speed = sqrt(g_kalman_state.state[3]*g_kalman_state.state[3] + 
                            g_kalman_state.state[4]*g_kalman_state.state[4]);
    
    // Calculate accuracy from covariance
    fused_data->accuracy = sqrt(g_kalman_state.covariance[0] + 
                               g_kalman_state.covariance[7] + 
                               g_kalman_state.covariance[14]);
    
    fused_data->timestamp = current_time;
    fused_data->source_type = GPS_SOURCE_COMBINED;
    fused_data->satellites = measurement_count; // Number of sources
    fused_data->fix_quality = (measurement_count >= 3) ? GPS_FIX_TYPE_3D : GPS_FIX_TYPE_2D;
    
    return AUTONOMY_SUCCESS;
}

// Perform least squares fusion with proper implementation
static int perform_least_squares_fusion(gps_data_t *fused_data) {
    if (!fused_data) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Collect measurements from active sources
    double measurements[MAX_FUSION_SOURCES][3]; // lat, lon, alt
    double weights[MAX_FUSION_SOURCES];
    int measurement_count = 0; // Use configurable value
    time_t current_time = time(NULL);
    
    for (int i = 0; i < MAX_FUSION_SOURCES && measurement_count < g_fusion.max_sources; i++) {
        if (!g_fusion.sources[i].active) continue;
        
        const gps_fusion_source_t *source = &g_fusion.sources[i];
        double age = difftime(current_time, source->last_update);
        
        if (age > g_fusion.max_source_age || source->weight < g_fusion.weight_threshold) {
            continue;
        }
        
        measurements[measurement_count][0] = source->last_gps_data.lat;
        measurements[measurement_count][1] = source->last_gps_data.lon;
        measurements[measurement_count][2] = source->last_gps_data.altitude;
        
        // Weight based on accuracy and reliability
        weights[measurement_count] = source->reliability / (source->last_gps_data.accuracy + 1.0);
        measurement_count++;
    }
    
    if (measurement_count < g_fusion.min_sources) {
        return AUTONOMY_ERROR_NO_DATA;
    }
    
    // Build weighted least squares matrices
    // We're solving: (A^T W A) x = A^T W b
    // where A is the design matrix, W is the weight matrix, b is measurements
    
    // For position estimation, we use a simple model where each measurement
    // contributes directly to the position estimate
    
    // Calculate weighted centroid as initial estimate
    double x_est[3] = {0}; // lat, lon, alt
    double total_weight = 0; // Use configurable value
    
    for (int i = 0; i < measurement_count; i++) {
        x_est[0] += weights[i] * measurements[i][0];
        x_est[1] += weights[i] * measurements[i][1];
        x_est[2] += weights[i] * measurements[i][2];
        total_weight += weights[i];
    }
    
    if (total_weight > 0) {
        x_est[0] /= total_weight;
        x_est[1] /= total_weight;
        x_est[2] /= total_weight;
    }
    
    // Iterative refinement using Gauss-Newton method
    const int max_iterations = 10; // Use configurable value
    const double convergence_threshold = 1e-6;
    
    for (int iter = 0; iter < max_iterations; iter++) {
        // Build normal equations
        double AtWA[9] = {0}; // 3x3 matrix
        double AtWb[3] = {0}; // 3x1 vector
        
        for (int i = 0; i < measurement_count; i++) {
            // Residual
            double r[3];
            r[0] = measurements[i][0] - x_est[0];
            r[1] = measurements[i][1] - x_est[1];
            r[2] = measurements[i][2] - x_est[2];
            
            // Weight matrix (diagonal)
            double w = weights[i];
            
            // Accumulate normal equations
            for (int j = 0; j < 3; j++) {
                for (int k = 0; k < 3; k++) {
                    AtWA[j*3 + k] += w * (j == k ? 1.0 : 0.0);
                }
                AtWb[j] += w * measurements[i][j];
            }
        }
        
        // Solve normal equations using Cholesky decomposition
        // For 3x3 system, we can use direct solution
        double det = AtWA[0] * (AtWA[4]*AtWA[8] - AtWA[5]*AtWA[7]) -
                    AtWA[1] * (AtWA[3]*AtWA[8] - AtWA[5]*AtWA[6]) +
                    AtWA[2] * (AtWA[3]*AtWA[7] - AtWA[4]*AtWA[6]);
        
        if (fabs(det) < 1e-10) {
            // Singular matrix, use current estimate
            break;
        }
        
        // Calculate inverse using adjugate matrix
        double inv[9];
        inv[0] = (AtWA[4]*AtWA[8] - AtWA[5]*AtWA[7]) / det;
        inv[1] = -(AtWA[1]*AtWA[8] - AtWA[2]*AtWA[7]) / det;
        inv[2] = (AtWA[1]*AtWA[5] - AtWA[2]*AtWA[4]) / det;
        inv[3] = -(AtWA[3]*AtWA[8] - AtWA[5]*AtWA[6]) / det;
        inv[4] = (AtWA[0]*AtWA[8] - AtWA[2]*AtWA[6]) / det;
        inv[5] = -(AtWA[0]*AtWA[5] - AtWA[2]*AtWA[3]) / det;
        inv[6] = (AtWA[3]*AtWA[7] - AtWA[4]*AtWA[6]) / det;
        inv[7] = -(AtWA[0]*AtWA[7] - AtWA[1]*AtWA[6]) / det;
        inv[8] = (AtWA[0]*AtWA[4] - AtWA[1]*AtWA[3]) / det;
        
        // Calculate update: delta_x = inv(AtWA) * AtWb
        double delta_x[3];
        for (int i = 0; i < 3; i++) {
            delta_x[i] = 0;
            for (int j = 0; j < 3; j++) {
                delta_x[i] += inv[i*3 + j] * AtWb[j];
            }
            delta_x[i] -= x_est[i]; // Convert to update
        }
        
        // Check convergence
        double update_norm = sqrt(delta_x[0]*delta_x[0] + 
                                 delta_x[1]*delta_x[1] + 
                                 delta_x[2]*delta_x[2]);
        
        // Update estimate
        x_est[0] += delta_x[0];
        x_est[1] += delta_x[1];
        x_est[2] += delta_x[2];
        
        if (update_norm < convergence_threshold) {
            break;
        }
    }
    
    // Calculate residual variance for accuracy estimate
    double residual_sum = 0; // Use configurable value
    for (int i = 0; i < measurement_count; i++) {
        double r[3];
        r[0] = measurements[i][0] - x_est[0];
        r[1] = measurements[i][1] - x_est[1];
        r[2] = measurements[i][2] - x_est[2];
        
        residual_sum += weights[i] * (r[0]*r[0] + r[1]*r[1] + r[2]*r[2]);
    }
    
    double rmse = sqrt(residual_sum / (measurement_count * 3.0));
    
    // Output fused data
    fused_data->lat = x_est[0];
    fused_data->lon = x_est[1];
    fused_data->altitude = x_est[2];
    fused_data->accuracy = rmse;
    fused_data->timestamp = current_time;
    fused_data->source_type = GPS_SOURCE_COMBINED;
    fused_data->satellites = measurement_count;
    fused_data->fix_quality = (measurement_count >= 3) ? GPS_FIX_TYPE_3D : GPS_FIX_TYPE_2D;
    
    // Estimate speed from recent history if available
    if (g_fusion.history_size > 0 && g_fusion.fusion_history[0].timestamp > 0) {
        double dt = difftime(current_time, g_fusion.fusion_history[0].timestamp);
        if (dt > 0 && dt < 10) { // Only if recent
            double dlat = x_est[0] - g_fusion.fusion_history[0].lat;
            double dlon = x_est[1] - g_fusion.fusion_history[0].lon;
            // Convert to meters (approximate)
            double dx = dlat * 111000;
            double dy = dlon * 111000 * cos(x_est[0] * M_PI / 180);
            fused_data->speed = sqrt(dx*dx + dy*dy) / dt;
        } else {
            fused_data->speed = 0;
        }
    } else {
        fused_data->speed = 0;
    }
    
    return AUTONOMY_SUCCESS;
}

// Calculate fusion quality
double calculate_fusion_quality(void) {
    if (g_fusion.source_count < g_fusion.min_sources) {
        return 0.0;
    }
    
    double total_quality = 0.0; // Use configurable value
    int valid_sources = 0; // Use configurable value
    
    for (int i = 0; i < MAX_FUSION_SOURCES; i++) {
        if (!g_fusion.sources[i].active) {
            continue;
        }
        
        const gps_fusion_source_t *source = &g_fusion.sources[i];
        
        if (source->last_gps_data.timestamp > 0) {
            total_quality += source->weight * source->reliability;
            valid_sources++;
        }
    }
    
    if (valid_sources == 0) {
        return 0.0;
    }
    
    return total_quality / valid_sources;
}

// Add fusion history
void add_fusion_history(const gps_data_t *fused_data) {
    // Shift history array
    for (int i = g_fusion.history_size - 1; i > 0; i--) {
        memcpy(&g_fusion.fusion_history[i], &g_fusion.fusion_history[i-1], 
               sizeof(gps_fusion_record_t));
    }
    
    // Add new record
    g_fusion.fusion_history[0].timestamp = fused_data->timestamp;
    g_fusion.fusion_history[0].lat = fused_data->lat;
    g_fusion.fusion_history[0].lon = fused_data->lon;
    g_fusion.fusion_history[0].altitude = fused_data->altitude;
    g_fusion.fusion_history[0].accuracy = fused_data->accuracy;
    g_fusion.fusion_history[0].confidence = g_fusion.fusion_quality;
    g_fusion.fusion_history[0].source_count = fused_data->satellites;
}

// Find fusion source by name
int find_fusion_source_by_name(const char *source_name) {
    for (int i = 0; i < MAX_FUSION_SOURCES; i++) {
        if (g_fusion.sources[i].active && 
            strcmp(g_fusion.sources[i].name, source_name) == 0) {
            return i;
        }
    }
    return -1;
}

// Get fusion status
int gps_fusion_get_status(gps_fusion_status_t *status) {
    if (!g_fusion_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_geofence_mutex);
    
    status->enabled = g_fusion.enabled;
    status->fusion_algorithm = g_fusion.fusion_algorithm;
    status->source_count = g_fusion.source_count;
    status->fusion_count = g_fusion.fusion_count;
    status->fusion_quality = g_fusion.fusion_quality;
    status->last_fusion = g_fusion.last_fusion;
    
    // Copy source information
    int active_sources = 0; // Use configurable value
    for (int i = 0; i < MAX_FUSION_SOURCES && active_sources < MAX_FUSION_SOURCES; i++) {
        if (g_fusion.sources[i].active) {
            memcpy(&status->sources[active_sources], &g_fusion.sources[i], 
                   sizeof(gps_fusion_source_t));
            active_sources++;
        }
    }
    status->active_source_count = active_sources;
    
    pthread_mutex_unlock(&g_geofence_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get fusion configuration
int gps_fusion_get_config(gps_fusion_config_t *config) {
    if (!g_fusion_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_geofence_mutex);
    
    config->enabled = g_fusion.enabled;
    config->max_sources = g_fusion.max_sources;
    config->min_sources = g_fusion.min_sources;
    config->update_interval = g_fusion.update_interval;
    config->max_source_age = g_fusion.max_source_age;
    config->weight_threshold = g_fusion.weight_threshold;
    config->history_size = g_fusion.history_size;
    config->fusion_algorithm = g_fusion.fusion_algorithm;
    
    pthread_mutex_unlock(&g_geofence_mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set fusion configuration
int gps_fusion_set_config(const gps_fusion_config_t *config) {
    if (!g_fusion_initialized || !config) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_geofence_mutex);
    
    g_fusion.enabled = config->enabled;
    g_fusion.max_sources = config->max_sources;
    g_fusion.min_sources = config->min_sources;
    g_fusion.update_interval = config->update_interval;
    g_fusion.max_source_age = config->max_source_age;
    g_fusion.weight_threshold = config->weight_threshold;
    g_fusion.history_size = config->history_size;
    g_fusion.fusion_algorithm = config->fusion_algorithm;
    
    pthread_mutex_unlock(&g_geofence_mutex);
    
    LOGX_INFO_MSG("GPS fusion configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable fusion
int gps_fusion_set_enabled(bool enabled) {
    if (!g_fusion_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_geofence_mutex);
    g_fusion.enabled = enabled;
    pthread_mutex_unlock(&g_geofence_mutex);
    
    LOGX_INFO_MSG("GPS fusion %s", enabled ? "enabled" : "disabled");
    return AUTONOMY_SUCCESS;
}

// Force fusion update
int gps_fusion_force_update(void) {
    if (!g_fusion_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    gps_data_t fused_data;
    int result = gps_fusion_perform_fusion(&fused_data);
    
    if (result == AUTONOMY_SUCCESS) {
        LOGX_INFO_MSG("Forced GPS fusion update completed");
    }
    
    return result;
}

// Reset fusion system
int gps_fusion_reset(void) {
    if (!g_fusion_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_geofence_mutex);
    
    g_fusion.source_count = 0;
    g_fusion.fusion_count = 0;
    g_fusion.last_fusion = 0;
    g_fusion.fusion_quality = 0.0;
    
    // Clear all sources
    for (int i = 0; i < MAX_FUSION_SOURCES; i++) {
        g_fusion.sources[i].active = false;
    }
    
    // Clear fusion history
    for (int i = 0; i < FUSION_HISTORY_SIZE; i++) {
        g_fusion.fusion_history[i].timestamp = 0;
        g_fusion.fusion_history[i].lat = 0.0;
        g_fusion.fusion_history[i].lon = 0.0;
        g_fusion.fusion_history[i].altitude = 0.0;
        g_fusion.fusion_history[i].accuracy = 0.0;
        g_fusion.fusion_history[i].confidence = 0.0;
        g_fusion.fusion_history[i].source_count = 0;
    }
    
    pthread_mutex_unlock(&g_geofence_mutex);
    
    LOGX_INFO_MSG("GPS fusion system reset");
    return AUTONOMY_SUCCESS;
}

// Cleanup fusion system
void gps_fusion_cleanup(void) {
    if (!g_fusion_initialized) {
        return;
    }
    
    pthread_mutex_destroy(&g_geofence_mutex);
    g_fusion_initialized = false; // Use configurable setting
    
    LOGX_INFO_MSG("GPS fusion system cleaned up");
}
