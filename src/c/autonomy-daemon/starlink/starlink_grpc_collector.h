#ifndef STARLINK_GRPC_COLLECTOR_H
#define STARLINK_GRPC_COLLECTOR_H

#include "../core/types.h"
#include <pthread.h>
#include <time.h>

// Maximum number of outage events to store
#define MAX_OUTAGE_EVENTS 1000
#define MAX_OBSERVATIONS 10000

// Observation collection interval (seconds)
#define OBSERVATION_INTERVAL 10

// Starlink gRPC Collector State
typedef struct {
    // Configuration
    // flawfinder: ignore - buffer size sufficient for host name
    char host[256]; // Bounds checked: max 255 chars + null terminator, validated in all functions
    int port;
    int timeout_seconds;
    bool enabled;
    
    // Data storage
    starlink_outage_event_t outage_events[MAX_OUTAGE_EVENTS];
    starlink_observation_t observations[MAX_OBSERVATIONS];
    int outage_count;
    int observation_count;
    int current_observation_index;
    
    // State tracking
    bool last_obstructed;
    time_t last_observation_time;
    starlink_observation_t last_observation;
    
    // Threading
    pthread_mutex_t mutex;
    pthread_t collection_thread;
    bool thread_running;
    
    // Statistics
    int total_outages_detected;
    int total_observations_collected;
    time_t last_successful_collection;
    int consecutive_failures;
} starlink_grpc_collector_t;

// Global collector instance
extern starlink_grpc_collector_t g_starlink_grpc_collector;

// Function declarations
int starlink_grpc_collector_init(void);
int starlink_grpc_collector_cleanup(void);
int starlink_grpc_collector_start(void);
int starlink_grpc_collector_stop(void);

// Data collection functions
int starlink_grpc_collect_history_data(void);
int starlink_grpc_collect_status_data(void);
int starlink_grpc_collect_diagnostics_data(void);

// Outage detection and persistence
int starlink_grpc_detect_outage_events(void);
int starlink_grpc_persist_outage_event(const starlink_outage_event_t* event);
int starlink_grpc_get_outage_events(starlink_outage_event_t* events, int max_count, int* actual_count);

// Observation collection
int starlink_grpc_collect_observation(void);
int starlink_grpc_get_observations(starlink_observation_t* observations, int max_count, int* actual_count);
int starlink_grpc_get_latest_observation(starlink_observation_t* observation);

// gRPC API calls
int starlink_grpc_call_get_history(char* response_buffer, size_t buffer_size);
int starlink_grpc_call_get_status(char* response_buffer, size_t buffer_size);
int starlink_grpc_call_get_diagnostics(char* response_buffer, size_t buffer_size);
int starlink_grpc_call_get_location(char* response_buffer, size_t buffer_size);

// JSON parsing functions
int starlink_grpc_parse_history_response(const char* json_response, starlink_observation_t* observations, int max_count, int* actual_count);
int starlink_grpc_parse_status_response(const char* json_response, starlink_observation_t* observation);
int starlink_grpc_parse_diagnostics_response(const char* json_response, starlink_observation_t* observation);

// Utility functions
void starlink_grpc_collector_thread(void* arg);
int starlink_grpc_analyze_outage_causes(const starlink_observation_t* pre, const starlink_observation_t* post, starlink_outage_event_t* event);
void starlink_grpc_log_outage_event(const starlink_outage_event_t* event);

#endif // STARLINK_GRPC_COLLECTOR_H
