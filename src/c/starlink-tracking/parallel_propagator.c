#include "parallel_propagator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

// Initialize default parallel configuration
void parallel_config_init_defaults(parallel_config_t *config) {
    if (!config) {
        return;
    }
    
    // Detect optimal thread count (number of CPU cores)
    config->num_threads = parallel_propagator_get_optimal_thread_count();
    config->batch_size = 50;  // Satellites per batch
    config->use_thread_pool = true;
    config->max_queue_size = 100;
}

// Get optimal thread count for the system
int parallel_propagator_get_optimal_thread_count(void) {
    // Try to detect number of CPU cores
    long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    if (nprocs > 0) {
        // Use all cores, but cap at 16 for memory reasons
        return (nprocs > 16) ? 16 : (int)nprocs;
    }
    
    // Fallback to 4 threads
    return 4;
}

// Initialize parallel propagator
parallel_propagator_t* parallel_propagator_init(const parallel_config_t *config) {
    parallel_propagator_t *propagator = calloc(1, sizeof(parallel_propagator_t));
    if (!propagator) {
        return NULL;
    }
    
    // Set configuration
    if (config) {
        memcpy(&propagator->config, config, sizeof(parallel_config_t));
    } else {
        parallel_config_init_defaults(&propagator->config);
    }
    
    // Initialize job ID management
    pthread_mutex_init(&propagator->job_id_mutex, NULL);
    propagator->next_job_id = 1;
    
    // Allocate worker threads
    propagator->num_workers = propagator->config.num_threads;
    propagator->workers = calloc(propagator->num_workers, sizeof(worker_thread_t));
    if (!propagator->workers) {
        free(propagator);
        return NULL;
    }
    
    // Initialize each worker thread
    for (int i = 0; i < propagator->num_workers; i++) {
        worker_thread_t *worker = &propagator->workers[i];
        worker->thread_id = i;
        worker->active = false;
        worker->should_exit = false;
        
        // Initialize job queue
        worker->queue_size = propagator->config.max_queue_size;
        worker->job_queue = calloc(worker->queue_size, sizeof(propagation_job_t));
        if (!worker->job_queue) {
            // Cleanup on failure
            for (int j = 0; j < i; j++) {
                free(propagator->workers[j].job_queue);
            }
            free(propagator->workers);
            free(propagator);
            return NULL;
        }
        
        // Initialize synchronization
        pthread_mutex_init(&worker->queue_mutex, NULL);
        pthread_cond_init(&worker->job_available, NULL);
        pthread_cond_init(&worker->queue_not_full, NULL);
    }
    
    propagator->initialized = true;
    propagator->start_time = time(NULL);
    
    return propagator;
}

// Cleanup parallel propagator
void parallel_propagator_cleanup(parallel_propagator_t *propagator) {
    if (!propagator) {
        return;
    }
    
    // Stop thread pool if active
    if (propagator->thread_pool_active) {
        parallel_propagator_stop_thread_pool(propagator);
    }
    
    // Cleanup worker threads
    if (propagator->workers) {
        for (int i = 0; i < propagator->num_workers; i++) {
            worker_thread_t *worker = &propagator->workers[i];
            
            if (worker->job_queue) {
                free(worker->job_queue);
            }
            
            pthread_mutex_destroy(&worker->queue_mutex);
            pthread_cond_destroy(&worker->job_available);
            pthread_cond_destroy(&worker->queue_not_full);
        }
        free(propagator->workers);
    }
    
    pthread_mutex_destroy(&propagator->job_id_mutex);
    free(propagator);
}

// Worker thread main function
static void* worker_thread_main(void *arg) {
    worker_thread_t *worker = (worker_thread_t*)arg;
    worker->active = true;
    
    while (!worker->should_exit) {
        pthread_mutex_lock(&worker->queue_mutex);
        
        // Wait for jobs
        while (worker->jobs_in_queue == 0 && !worker->should_exit) {
            pthread_cond_wait(&worker->job_available, &worker->queue_mutex);
        }
        
        if (worker->should_exit) {
            pthread_mutex_unlock(&worker->queue_mutex);
            break;
        }
        
        // Get next job
        propagation_job_t job = worker->job_queue[worker->queue_head];
        worker->queue_head = (worker->queue_head + 1) % worker->queue_size;
        worker->jobs_in_queue--;
        
        // Signal that queue has space
        pthread_cond_signal(&worker->queue_not_full);
        pthread_mutex_unlock(&worker->queue_mutex);
        
        // Process the job
        struct timeval start_time, end_time;
        gettimeofday(&start_time, NULL);
        
        int result = worker_process_job(worker, &job);
        
        gettimeofday(&end_time, NULL);
        double processing_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0 + 
                                (end_time.tv_usec - start_time.tv_usec) / 1000.0;
        
        // Update statistics
        worker->jobs_completed++;
        worker->total_processing_time += processing_time;
        worker->last_job_time = time(NULL);
    }
    
    worker->active = false;
    return NULL;
}

// Process a single propagation job
static int worker_process_job(worker_thread_t *worker, const propagation_job_t *job) {
    if (!worker || !job || !job->elements || !job->time_array || !job->results) {
        return PARALLEL_ERROR_INVALID_PARAM;
    }
    
    // Propagate satellite for each time point
    for (int i = 0; i < job->num_times; i++) {
        int prop_result = prediction_engine_propagate_satellite(
            job->elements,
            job->time_array[i],
            &job->results[i]
        );
        
        if (prop_result != PREDICTION_SUCCESS) {
            // Mark this result as invalid
            job->results[i].timestamp = 0;
        }
    }
    
    return PARALLEL_SUCCESS;
}

// Start thread pool
int parallel_propagator_start_thread_pool(parallel_propagator_t *propagator) {
    if (!propagator || propagator->thread_pool_active) {
        return PARALLEL_ERROR_INVALID_PARAM;
    }
    
    // Start all worker threads
    for (int i = 0; i < propagator->num_workers; i++) {
        worker_thread_t *worker = &propagator->workers[i];
        worker->should_exit = false;
        
        if (pthread_create(&worker->thread, NULL, worker_thread_main, worker) != 0) {
            // Stop already started threads
            for (int j = 0; j < i; j++) {
                propagator->workers[j].should_exit = true;
                pthread_cond_signal(&propagator->workers[j].job_available);
                pthread_join(propagator->workers[j].thread, NULL);
            }
            return PARALLEL_ERROR_THREAD_FAILED;
        }
    }
    
    propagator->thread_pool_active = true;
    
    if (propagator->log_callback) {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Started parallel propagator with %d worker threads", 
                propagator->num_workers);
        propagator->log_callback(1, log_msg, propagator->log_user_data);
    }
    
    return PARALLEL_SUCCESS;
}

// Stop thread pool
int parallel_propagator_stop_thread_pool(parallel_propagator_t *propagator) {
    if (!propagator || !propagator->thread_pool_active) {
        return PARALLEL_SUCCESS;
    }
    
    // Signal all workers to exit
    for (int i = 0; i < propagator->num_workers; i++) {
        worker_thread_t *worker = &propagator->workers[i];
        worker->should_exit = true;
        pthread_cond_signal(&worker->job_available);
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < propagator->num_workers; i++) {
        pthread_join(propagator->workers[i].thread, NULL);
    }
    
    propagator->thread_pool_active = false;
    
    return PARALLEL_SUCCESS;
}

// Calculate satellite positions in parallel
int parallel_propagator_calculate_positions(
    parallel_propagator_t *propagator,
    const constellation_data_t *constellation,
    const dish_location_t *observer,
    time_t *time_array,
    int num_times,
    parallel_result_t *result) {
    
    if (!propagator || !constellation || !observer || !time_array || !result) {
        return PARALLEL_ERROR_INVALID_PARAM;
    }
    
    if (!propagator->thread_pool_active) {
        int start_result = parallel_propagator_start_thread_pool(propagator);
        if (start_result != PARALLEL_SUCCESS) {
            return start_result;
        }
    }
    
    struct timeval calc_start, calc_end;
    gettimeofday(&calc_start, NULL);
    result->calculation_start = time(NULL);
    
    // Allocate result arrays
    result->positions = calloc(constellation->num_satellites * num_times, sizeof(satellite_position_t));
    if (!result->positions) {
        return PARALLEL_ERROR_MEMORY_FAILED;
    }
    
    // Create jobs for each satellite
    int jobs_submitted = 0;
    for (int sat_idx = 0; sat_idx < constellation->num_satellites; sat_idx++) {
        if (!constellation->satellites[sat_idx].is_valid) {
            continue;
        }
        
        // Find least busy worker
        int best_worker = 0;
        int min_queue_size = propagator->workers[0].jobs_in_queue;
        for (int w = 1; w < propagator->num_workers; w++) {
            if (propagator->workers[w].jobs_in_queue < min_queue_size) {
                min_queue_size = propagator->workers[w].jobs_in_queue;
                best_worker = w;
            }
        }
        
        worker_thread_t *worker = &propagator->workers[best_worker];
        
        // Create job
        propagation_job_t job;
        
        // Parse TLE into orbital elements
        orbital_elements_t elements;
        int parse_result = prediction_engine_parse_tle(&constellation->satellites[sat_idx], &elements);
        if (parse_result != PREDICTION_SUCCESS) {
            continue;
        }
        
        job.elements = &elements;
        job.time_array = time_array;
        job.num_times = num_times;
        job.results = &result->positions[sat_idx * num_times];
        job.satellite_index = sat_idx;
        
        pthread_mutex_lock(&propagator->job_id_mutex);
        job.job_id = propagator->next_job_id++;
        pthread_mutex_unlock(&propagator->job_id_mutex);
        
        // Submit job to worker
        int submit_result = worker_submit_job(worker, &job);
        if (submit_result == PARALLEL_SUCCESS) {
            jobs_submitted++;
        }
    }
    
    // Wait for all jobs to complete
    bool all_complete = false;
    while (!all_complete) {
        all_complete = true;
        for (int i = 0; i < propagator->num_workers; i++) {
            if (propagator->workers[i].jobs_in_queue > 0) {
                all_complete = false;
                break;
            }
        }
        
        if (!all_complete) {
            usleep(10000); // 10ms
        }
    }
    
    gettimeofday(&calc_end, NULL);
    result->calculation_end = time(NULL);
    result->processing_time_seconds = (calc_end.tv_sec - calc_start.tv_sec) + 
                                     (calc_end.tv_usec - calc_start.tv_usec) / 1000000.0;
    
    // Convert satellite states to positions with Az/El
    result->num_positions = 0;
    for (int i = 0; i < constellation->num_satellites * num_times; i++) {
        if (result->positions[i].timestamp > 0) {
            // Convert ECI to topocentric coordinates
            topocentric_coords_t topo;
            satellite_state_t sat_state = {
                .position = {result->positions[i].range, 0, 0}, // Simplified for now
                .timestamp = result->positions[i].timestamp
            };
            
            if (prediction_engine_eci_to_topocentric(&sat_state, observer, &topo) == PREDICTION_SUCCESS) {
                result->positions[result->num_positions].azimuth = topo.azimuth;
                result->positions[result->num_positions].elevation = topo.elevation;
                result->positions[result->num_positions].range = topo.range;
                result->positions[result->num_positions].timestamp = result->positions[i].timestamp;
                result->num_positions++;
            }
        }
    }
    
    // Update statistics
    propagator->total_jobs_submitted += jobs_submitted;
    propagator->total_jobs_completed += jobs_submitted; // Assume all completed
    
    if (propagator->log_callback) {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), 
                "Parallel propagation completed: %d jobs in %.2f seconds (%.1f jobs/sec)", 
                jobs_submitted, result->processing_time_seconds, 
                jobs_submitted / result->processing_time_seconds);
        propagator->log_callback(1, log_msg, propagator->log_user_data);
    }
    
    return PARALLEL_SUCCESS;
}

// Submit job to worker thread
static int worker_submit_job(worker_thread_t *worker, const propagation_job_t *job) {
    if (!worker || !job) {
        return PARALLEL_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&worker->queue_mutex);
    
    // Check if queue is full
    while (worker->jobs_in_queue >= worker->queue_size) {
        pthread_cond_wait(&worker->queue_not_full, &worker->queue_mutex);
    }
    
    // Add job to queue
    int tail_index = (worker->queue_head + worker->jobs_in_queue) % worker->queue_size;
    memcpy(&worker->job_queue[tail_index], job, sizeof(propagation_job_t));
    worker->jobs_in_queue++;
    
    // Signal job available
    pthread_cond_signal(&worker->job_available);
    pthread_mutex_unlock(&worker->queue_mutex);
    
    return PARALLEL_SUCCESS;
}

// Get performance statistics
parallel_performance_stats_t parallel_propagator_get_performance_stats(const parallel_propagator_t *propagator) {
    parallel_performance_stats_t stats = {0};
    
    if (!propagator) {
        return stats;
    }
    
    stats.total_jobs_submitted = propagator->total_jobs_submitted;
    stats.total_jobs_completed = propagator->total_jobs_completed;
    stats.active_threads = 0;
    
    // Count active threads and jobs in queue
    for (int i = 0; i < propagator->num_workers; i++) {
        if (propagator->workers[i].active) {
            stats.active_threads++;
        }
        stats.jobs_in_queue += propagator->workers[i].jobs_in_queue;
    }
    
    // Calculate throughput
    time_t now = time(NULL);
    double elapsed_seconds = difftime(now, propagator->start_time);
    if (elapsed_seconds > 0) {
        stats.throughput_jobs_per_second = (double)stats.total_jobs_completed / elapsed_seconds;
    }
    
    // Calculate average job time
    double total_worker_time = 0.0;
    int total_worker_jobs = 0;
    for (int i = 0; i < propagator->num_workers; i++) {
        total_worker_time += propagator->workers[i].total_processing_time;
        total_worker_jobs += propagator->workers[i].jobs_completed;
    }
    
    if (total_worker_jobs > 0) {
        stats.average_job_time_ms = total_worker_time / total_worker_jobs;
    }
    
    stats.last_update = now;
    
    return stats;
}

// Initialize parallel result structure
void parallel_result_init(parallel_result_t *result) {
    if (!result) {
        return;
    }
    
    memset(result, 0, sizeof(parallel_result_t));
}

// Cleanup parallel result structure
void parallel_result_cleanup(parallel_result_t *result) {
    if (!result) {
        return;
    }
    
    if (result->positions) {
        free(result->positions);
        result->positions = NULL;
    }
    
    if (result->assessments) {
        // Free satellite positions in each assessment
        for (int i = 0; i < result->num_assessments; i++) {
            if (result->assessments[i].satellite_positions) {
                free(result->assessments[i].satellite_positions);
            }
        }
        free(result->assessments);
        result->assessments = NULL;
    }
    
    result->num_positions = 0;
    result->num_assessments = 0;
}