#ifndef PREDICTIVE_ENGINE_H
#define PREDICTIVE_ENGINE_H

#include <stdbool.h>
#include <time.h>

// Prediction types
typedef enum {
    PREDICTION_TYPE_FAILOVER,
    PREDICTION_TYPE_PERFORMANCE,
    PREDICTION_TYPE_MAINTENANCE,
    PREDICTION_TYPE_CAPACITY
} prediction_type_t;

// Prediction result
typedef struct {
    prediction_type_t type;
    char target[64];
    double probability;
    time_t predicted_time;
    char description[256];
    double confidence;
    char mitigation[256];
} prediction_result_t;

// Predictive model configuration
typedef struct {
    bool enabled;
    int prediction_horizon_hours;
    double confidence_threshold;
    bool enable_machine_learning;
    int training_data_points;
    int update_interval_seconds;
} predictive_model_config_t;

// Predictive engine structure
typedef struct {
    predictive_model_config_t config;
    
    // Prediction results
    prediction_result_t predictions[32];
    int prediction_count;
    
    // Model statistics
    time_t last_training;
    int training_count;
    double accuracy_rate;
    int successful_predictions;
    int total_predictions;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} predictive_engine_t;

// Initialize predictive engine
int predictive_engine_init(const predictive_model_config_t* config);

// Clean up predictive engine
void predictive_engine_cleanup(void);

// Generate predictions
int predictive_engine_generate_predictions(void);

// Get predictions
int predictive_engine_get_predictions(prediction_result_t* predictions, int max_predictions);

// Train predictive models
int predictive_engine_train_models(void);

// Update prediction accuracy
int predictive_engine_update_accuracy(bool prediction_correct);

// Get predictive engine status
void predictive_engine_get_status(predictive_engine_t* status);

// Check if predictive engine is initialized
bool predictive_engine_is_initialized(void);

// Get predictive engine instance
predictive_engine_t* predictive_engine_get_instance(void);

#endif // PREDICTIVE_ENGINE_H
