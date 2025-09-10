#ifndef PREDICTION_ENGINE_H
#define PREDICTION_ENGINE_H

#include <stdbool.h>
#include <time.h>
#include "starlink_types.h"
#include "starlink_tracker.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct prediction_engine prediction_engine_t;
typedef struct obstruction_analyzer obstruction_analyzer_t;

// Configuration structure
typedef struct {
    bool enable_predictions;
    int prediction_horizon_hours;
    double min_elevation_degrees;
    char tle_data_path[256];
} prediction_engine_config_t;

// Function prototypes
prediction_engine_t* prediction_engine_init(const prediction_engine_config_t* config);
void prediction_engine_cleanup(prediction_engine_t* engine);
void prediction_engine_config_init_defaults(prediction_engine_config_t* config);
int prediction_engine_set_dish_location(prediction_engine_t* engine, const dish_location_t* location);
int prediction_engine_set_obstruction_analyzer(prediction_engine_t* engine, obstruction_analyzer_t* analyzer);
int prediction_engine_load_constellation(prediction_engine_t* engine, const void* constellation);
int prediction_engine_calculate_predictions(prediction_engine_t* engine, time_t start_time, time_t end_time, satellite_position_t* positions, int* num_positions);
int prediction_engine_get_visible_satellites(prediction_engine_t* engine, time_t timestamp, satellite_position_t* positions, int* num_positions);

#ifdef __cplusplus
}
#endif

#endif // PREDICTION_ENGINE_H
