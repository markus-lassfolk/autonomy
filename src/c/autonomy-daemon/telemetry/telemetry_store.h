#ifndef TELEMETRY_STORE_H
#define TELEMETRY_STORE_H

#include "../core/types.h"
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

// Telemetry sample
typedef struct {
    char member_name[128];
    time_t timestamp;
    double latency_ms;
    double loss_percent;
    double jitter_ms;
    double signal_strength;
    double obstruction_pct;
    int rsrp;
    int rsrq;
    double throughput_mbps;
    double score;
    bool has_latency;
    bool has_loss;
    bool has_jitter;
    bool has_signal;
    bool has_obstruction;
    bool has_rsrp;
    bool has_rsrq;
    bool has_throughput;
    bool has_score;
} telemetry_sample_t;

// System event
typedef struct {
    char id[64];
    char type[64];
    char member_name[128];
    char description[512];
    time_t timestamp;
    char details_json[1024];
} telemetry_event_t;

// Ring buffer for telemetry data
typedef struct {
    void** data;
    int capacity;
    int head;
    int tail;
    int size;
    time_t last_add;
    pthread_mutex_t* mutex;
} telemetry_ring_buffer_t;

// Telemetry store configuration
typedef struct {
    int retention_hours;
    int max_ram_mb;
    int max_samples_per_member;
    int max_events;
    time_t cleanup_interval_seconds;
    bool enable_downsampling;
    int downsample_ratio;
} telemetry_store_config_t;

// Telemetry store status
typedef struct {
    bool enabled;
    int total_members;
    int total_samples;
    int total_events;
    int memory_usage_mb;
    time_t last_cleanup;
    time_t last_sample_time;
    time_t last_event_time;
} telemetry_store_status_t;

// Telemetry store structure
typedef struct {
    telemetry_store_config_t config;
    
    // Data storage
    telemetry_ring_buffer_t** member_buffers;
    char member_names[64][128];
    int member_count;
    int max_members;
    
    telemetry_ring_buffer_t* events_buffer;
    
    // Memory tracking
    int64_t memory_usage;
    time_t last_cleanup;
    
    // Statistics
    telemetry_store_status_t status;
    
    // Thread management
    pthread_t cleanup_thread;
    bool thread_running;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} telemetry_store_t;

// Initialize telemetry store
int telemetry_store_init(const telemetry_store_config_t* config);

// Clean up telemetry store
void telemetry_store_cleanup(void);

// Add telemetry sample
int telemetry_store_add_sample(const char* member_name, const telemetry_sample_t* sample);

// Add system event
int telemetry_store_add_event(const telemetry_event_t* event);

// Get samples for member since timestamp
int telemetry_store_get_samples(const char* member_name, time_t since, 
                               telemetry_sample_t* samples, int max_samples);

// Get events since timestamp
int telemetry_store_get_events(time_t since, telemetry_event_t* events, int max_events);

// Get all member names
int telemetry_store_get_members(char member_names[][128], int max_members);

// Get memory usage in MB
int telemetry_store_get_memory_usage(void);

// Perform cleanup of old data
void telemetry_store_perform_cleanup(void);

// Get telemetry store status
void telemetry_store_get_status(telemetry_store_status_t* status);

// Export telemetry data as JSON
void telemetry_store_export_json(time_t since, char* json_output, size_t max_size);

// Check if telemetry store is initialized
bool telemetry_store_is_initialized(void);

// Get telemetry store instance
telemetry_store_t* telemetry_store_get_instance(void);

// Ring buffer functions
telemetry_ring_buffer_t* telemetry_ring_buffer_create(int capacity);
void telemetry_ring_buffer_destroy(telemetry_ring_buffer_t* buffer);
int telemetry_ring_buffer_add(telemetry_ring_buffer_t* buffer, void* item);
int telemetry_ring_buffer_get_since(telemetry_ring_buffer_t* buffer, time_t since, 
                                   void** items, int max_items);
int telemetry_ring_buffer_size(telemetry_ring_buffer_t* buffer);
int telemetry_ring_buffer_remove_before(telemetry_ring_buffer_t* buffer, time_t before);

#endif // TELEMETRY_STORE_H
