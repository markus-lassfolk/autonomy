#ifndef PARALLEL_PROPAGATOR_H
#define PARALLEL_PROPAGATOR_H

#include "starlink_tracker.h"
#include "prediction_engine.h"
#include <pthread.h>
#include <semaphore.h>

// Parallel processing configuration
typedef struct {
    int num_threads;                // Number of worker threads
    int batch_size;                 // Satellites per batch
    bool use_thread_pool;           // Use persistent thread pool
    int max_queue_size;             // Maximum job queue size
} parallel_config_t;

// Job structure for parallel propagation
typedef struct {
    orbital_elements_t *elements;   // Satellite orbital elements
    time_t *time_array;            // Array of times to propagate
    int num_times;                 // Number of time points
    satellite_state_t *results;    // Output array
    int satellite_index;           // Index in constellation
    int job_id;                    // Unique job identifier
} propagation_job_t;

// Worker thread data
typedef struct {
    int thread_id;
    pthread_t thread;
    bool active;
    bool should_exit;
    
    // Job queue
    propagation_job_t *job_queue;
    int queue_size;
    int queue_head;
    int queue_tail;
    int jobs_in_queue;
    
    // Synchronization
    pthread_mutex_t queue_mutex;
    pthread_cond_t job_available;
    pthread_cond_t queue_not_full;
    
    // Statistics
    int jobs_completed;
    double total_processing_time;
    time_t last_job_time;
} worker_thread_t;

// Parallel propagator structure
typedef struct {
    parallel_config_t config;
    worker_thread_t *workers;
    int num_workers;
    bool initialized;
    bool thread_pool_active;
    
    // Global job management
    int next_job_id;
    pthread_mutex_t job_id_mutex;
    
    // Performance statistics
    int total_jobs_submitted;
    int total_jobs_completed;
    double total_processing_time;
    time_t start_time;
    
    // Logging callback
    void (*log_callback)(int level, const char *message, void *user_data);
    void *log_user_data;
} parallel_propagator_t;

// Result aggregation structure
typedef struct {
    satellite_position_t *positions;
    int num_positions;
    connectivity_assessment_t *assessments;
    int num_assessments;
    time_t calculation_start;
    time_t calculation_end;
    double processing_time_seconds;
} parallel_result_t;

// API Functions

// Initialization and cleanup
parallel_propagator_t* parallel_propagator_init(const parallel_config_t *config);
void parallel_propagator_cleanup(parallel_propagator_t *propagator);

// Configuration
void parallel_config_init_defaults(parallel_config_t *config);
int parallel_propagator_update_config(parallel_propagator_t *propagator, const parallel_config_t *config);

// Thread pool management
int parallel_propagator_start_thread_pool(parallel_propagator_t *propagator);
int parallel_propagator_stop_thread_pool(parallel_propagator_t *propagator);
bool parallel_propagator_is_thread_pool_active(const parallel_propagator_t *propagator);

// Parallel propagation functions
int parallel_propagator_calculate_positions(
    parallel_propagator_t *propagator,
    const constellation_data_t *constellation,
    const dish_location_t *observer,
    time_t *time_array,
    int num_times,
    parallel_result_t *result
);

int parallel_propagator_batch_connectivity_assessment(
    parallel_propagator_t *propagator,
    prediction_engine_t *engine,
    time_t start_time,
    time_t end_time,
    int time_step_seconds,
    parallel_result_t *result
);

// Performance monitoring
typedef struct {
    int total_jobs_submitted;
    int total_jobs_completed;
    int jobs_in_queue;
    double average_job_time_ms;
    double throughput_jobs_per_second;
    int active_threads;
    double cpu_utilization_percent;
    time_t last_update;
} parallel_performance_stats_t;

parallel_performance_stats_t parallel_propagator_get_performance_stats(const parallel_propagator_t *propagator);

// Utility functions
int parallel_propagator_get_optimal_thread_count(void);
int parallel_propagator_estimate_batch_size(int num_satellites, int num_threads);

// Result management
void parallel_result_init(parallel_result_t *result);
void parallel_result_cleanup(parallel_result_t *result);

// Worker thread functions (internal)
// These functions are implemented in the corresponding .c file

// Error codes
#define PARALLEL_SUCCESS                 0
#define PARALLEL_ERROR_INVALID_PARAM    -1
#define PARALLEL_ERROR_NOT_INITIALIZED  -2
#define PARALLEL_ERROR_THREAD_FAILED    -3
#define PARALLEL_ERROR_QUEUE_FULL       -4
#define PARALLEL_ERROR_MEMORY_FAILED    -5
#define PARALLEL_ERROR_TIMEOUT          -6

#endif // PARALLEL_PROPAGATOR_H