#include "telemetry_store.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

// Global telemetry store instance
static telemetry_store_t g_telemetry_store;
static bool g_telemetry_store_initialized = false;

// Forward declarations
static void* cleanup_thread(void* arg);
void update_memory_usage(void);
void check_memory_pressure(void);
static void downsample_old_data(void);
int find_member_index(const char* member_name);

// Create ring buffer
telemetry_ring_buffer_t* telemetry_ring_buffer_create(int capacity) {
    telemetry_ring_buffer_t* buffer = malloc(sizeof(telemetry_ring_buffer_t));
    if (!buffer) return NULL;
    
    buffer->data = malloc(capacity * sizeof(void*));
    if (!buffer->data) {
        free(buffer);
        return NULL;
    }
    
    buffer->mutex = malloc(sizeof(pthread_mutex_t));
    if (!buffer->mutex) {
        free(buffer->data);
        free(buffer);
        return NULL;
    }
    
    pthread_mutex_init(buffer->mutex, NULL);
    buffer->capacity = capacity;
    buffer->head = 0;
    buffer->tail = 0;
    buffer->size = 0;
    buffer->last_add = 0;
    
    return buffer;
}

// Destroy ring buffer
void telemetry_ring_buffer_destroy(telemetry_ring_buffer_t* buffer) {
    if (!buffer) return;
    
    if (buffer->mutex) {
        pthread_mutex_destroy(buffer->mutex);
        free(buffer->mutex);
    }
    
    if (buffer->data) {
        // Free all stored items
        for (int i = 0; i < buffer->size; i++) {
            int idx = (buffer->head + i) % buffer->capacity;
            if (buffer->data[idx]) {
                free(buffer->data[idx]);
            }
        }
        free(buffer->data);
    }
    
    free(buffer);
}

// Add item to ring buffer
int telemetry_ring_buffer_add(telemetry_ring_buffer_t* buffer, void* item) {
    if (!buffer || !item) return -1;
    
    pthread_mutex_lock(buffer->mutex);
    
    // Free old item if buffer is full
    if (buffer->size == buffer->capacity) {
        if (buffer->data[buffer->tail]) {
            free(buffer->data[buffer->tail]);
        }
        buffer->head = (buffer->head + 1) % buffer->capacity;
    } else {
        buffer->size++;
    }
    
    buffer->data[buffer->tail] = item;
    buffer->tail = (buffer->tail + 1) % buffer->capacity;
    buffer->last_add = time(NULL);
    
    pthread_mutex_unlock(buffer->mutex);
    return 0;
}

// Get items since timestamp
int telemetry_ring_buffer_get_since(telemetry_ring_buffer_t* buffer, time_t since, 
                                   void** items, int max_items) {
    if (!buffer || !items || max_items <= 0) return -1;
    
    pthread_mutex_lock(buffer->mutex);
    
    int count = 0;
    for (int i = 0; i < buffer->size && count < max_items; i++) {
        int idx = (buffer->head + i) % buffer->capacity;
        void* item = buffer->data[idx];
        
        // Check timestamp based on item type
        time_t item_timestamp = 0;
        if (item) {
            telemetry_sample_t* sample = (telemetry_sample_t*)item;
            item_timestamp = sample->timestamp;
        }
        
        if (item_timestamp > since) {
            items[count] = item;
            count++;
        }
    }
    
    pthread_mutex_unlock(buffer->mutex);
    return count;
}

// Get ring buffer size
int telemetry_ring_buffer_size(telemetry_ring_buffer_t* buffer) {
    if (!buffer) return 0;
    
    pthread_mutex_lock(buffer->mutex);
    int size = buffer->size;
    pthread_mutex_unlock(buffer->mutex);
    
    return size;
}

// Remove items before timestamp
int telemetry_ring_buffer_remove_before(telemetry_ring_buffer_t* buffer, time_t before) {
    if (!buffer) return 0;
    
    pthread_mutex_lock(buffer->mutex);
    
    int removed_count = 0;
    
    // Simple approach: remove items from head if they're too old
    while (buffer->size > 0) {
        int head_idx = buffer->head;
        void* item = buffer->data[head_idx];
        
        if (item) {
            telemetry_sample_t* sample = (telemetry_sample_t*)item;
            if (sample->timestamp < before) {
                free(item);
                buffer->data[head_idx] = NULL;
                buffer->head = (buffer->head + 1) % buffer->capacity;
                buffer->size--;
                removed_count++;
            } else {
                break; // Found item that's not old enough
            }
        } else {
            break;
        }
    }
    
    pthread_mutex_unlock(buffer->mutex);
    return removed_count;
}

// Initialize telemetry store
int telemetry_store_init(const telemetry_store_config_t* config) {
    if (g_telemetry_store_initialized) {
        return 0; // Already initialized
    }
    
    if (!config) {
        return -1;
    }
    
    memset(&g_telemetry_store, 0, sizeof(telemetry_store_t));
    
    // Copy configuration
    g_telemetry_store.config = *config;
    
    // Initialize mutex
    g_telemetry_store.mutex = malloc(sizeof(pthread_mutex_t));
    if (!g_telemetry_store.mutex) {
        return -1;
    }
    
    pthread_mutex_init(g_telemetry_store.mutex, NULL);
    
    // Initialize member buffers
    g_telemetry_store.max_members = 64;
    g_telemetry_store.member_buffers = malloc(g_telemetry_store.max_members * sizeof(telemetry_ring_buffer_t*));
    if (!g_telemetry_store.member_buffers) {
        pthread_mutex_destroy(g_telemetry_store.mutex);
        free(g_telemetry_store.mutex);
        return -1;
    }
    
    for (int i = 0; i < g_telemetry_store.max_members; i++) {
        g_telemetry_store.member_buffers[i] = NULL;
    }
    g_telemetry_store.member_count = 0;
    
    // Initialize events buffer
    g_telemetry_store.events_buffer = telemetry_ring_buffer_create(config->max_events);
    if (!g_telemetry_store.events_buffer) {
        free(g_telemetry_store.member_buffers);
        pthread_mutex_destroy(g_telemetry_store.mutex);
        free(g_telemetry_store.mutex);
        return -1;
    }
    
    // Initialize statistics
    memset(&g_telemetry_store.status, 0, sizeof(telemetry_store_status_t));
    g_telemetry_store.status.enabled = true;
    g_telemetry_store.last_cleanup = time(NULL);
    
    // Start cleanup thread
    g_telemetry_store.thread_running = true;
    if (pthread_create(&g_telemetry_store.cleanup_thread, NULL, cleanup_thread, NULL) != 0) {
        telemetry_ring_buffer_destroy(g_telemetry_store.events_buffer);
        free(g_telemetry_store.member_buffers);
        pthread_mutex_destroy(g_telemetry_store.mutex);
        free(g_telemetry_store.mutex);
        return -1;
    }
    
    g_telemetry_store_initialized = true;
    return 0;
}

// Clean up telemetry store
void telemetry_store_cleanup(void) {
    if (!g_telemetry_store_initialized) return;
    
    // Stop cleanup thread
    g_telemetry_store.thread_running = false;
    pthread_join(g_telemetry_store.cleanup_thread, NULL);
    
    // Clean up member buffers
    if (g_telemetry_store.member_buffers) {
        for (int i = 0; i < g_telemetry_store.max_members; i++) {
            if (g_telemetry_store.member_buffers[i]) {
                telemetry_ring_buffer_destroy(g_telemetry_store.member_buffers[i]);
            }
        }
        free(g_telemetry_store.member_buffers);
    }
    
    // Clean up events buffer
    if (g_telemetry_store.events_buffer) {
        telemetry_ring_buffer_destroy(g_telemetry_store.events_buffer);
    }
    
    if (g_telemetry_store.mutex) {
        pthread_mutex_destroy(g_telemetry_store.mutex);
        free(g_telemetry_store.mutex);
    }
    
    g_telemetry_store.member_buffers = NULL;
    g_telemetry_store.events_buffer = NULL;
    g_telemetry_store.mutex = NULL;
    g_telemetry_store.member_count = 0;
    g_telemetry_store.max_members = 0;
    
    g_telemetry_store_initialized = false;
}

// Cleanup thread
static void* cleanup_thread(void* arg) {
    (void)arg; // Unused parameter
    
    while (g_telemetry_store.thread_running) {
        telemetry_store_perform_cleanup();
        sleep((unsigned int)g_telemetry_store.config.cleanup_interval_seconds);
    }
    
    return NULL;
}

// Find member index
int find_member_index(const char* member_name) {
    for (int i = 0; i < g_telemetry_store.member_count; i++) {
        if (strcmp(g_telemetry_store.member_names[i], member_name) == 0) {
            return i;
        }
    }
    return -1;
}

// Add telemetry sample
int telemetry_store_add_sample(const char* member_name, const telemetry_sample_t* sample) {
    if (!g_telemetry_store_initialized || !member_name || !sample) {
        return -1;
    }
    
    pthread_mutex_lock(g_telemetry_store.mutex);
    
    // Find or create member buffer
    int member_index = find_member_index(member_name);
    if (member_index == -1) {
        // Create new member
        if (g_telemetry_store.member_count >= g_telemetry_store.max_members) {
            pthread_mutex_unlock(g_telemetry_store.mutex);
            return -1; // No space for more members
        }
        
        member_index = g_telemetry_store.member_count;
        strncpy(g_telemetry_store.member_names[member_index], member_name, sizeof(g_telemetry_store.member_names[member_index]) - 1);
        g_telemetry_store.member_names[member_index][sizeof(g_telemetry_store.member_names[member_index]) - 1] = '\0';
        
        g_telemetry_store.member_buffers[member_index] = telemetry_ring_buffer_create(g_telemetry_store.config.max_samples_per_member);
        if (!g_telemetry_store.member_buffers[member_index]) {
            pthread_mutex_unlock(g_telemetry_store.mutex);
            return -1;
        }
        
        g_telemetry_store.member_count++;
        g_telemetry_store.status.total_members++;
    }
    
    // Create sample copy
    telemetry_sample_t* sample_copy = malloc(sizeof(telemetry_sample_t));
    if (!sample_copy) {
        pthread_mutex_unlock(g_telemetry_store.mutex);
        return -1;
    }
    
    *sample_copy = *sample;
    sample_copy->timestamp = time(NULL);
    
    // Add to ring buffer
    if (telemetry_ring_buffer_add(g_telemetry_store.member_buffers[member_index], sample_copy) != 0) {
        free(sample_copy);
        pthread_mutex_unlock(g_telemetry_store.mutex);
        return -1;
    }
    
    // Update statistics
    g_telemetry_store.status.total_samples++;
    g_telemetry_store.status.last_sample_time = sample_copy->timestamp;
    
    // Check memory pressure
    check_memory_pressure();
    
    pthread_mutex_unlock(g_telemetry_store.mutex);
    return 0;
}

// Add system event
int telemetry_store_add_event(const telemetry_event_t* event) {
    if (!g_telemetry_store_initialized || !event) {
        return -1;
    }
    
    pthread_mutex_lock(g_telemetry_store.mutex);
    
    // Create event copy
    telemetry_event_t* event_copy = malloc(sizeof(telemetry_event_t));
    if (!event_copy) {
        pthread_mutex_unlock(g_telemetry_store.mutex);
        return -1;
    }
    
    *event_copy = *event;
    event_copy->timestamp = time(NULL);
    
    // Add to events buffer
    if (telemetry_ring_buffer_add(g_telemetry_store.events_buffer, event_copy) != 0) {
        free(event_copy);
        pthread_mutex_unlock(g_telemetry_store.mutex);
        return -1;
    }
    
    // Update statistics
    g_telemetry_store.status.total_events++;
    g_telemetry_store.status.last_event_time = event_copy->timestamp;
    
    // Check memory pressure
    check_memory_pressure();
    
    pthread_mutex_unlock(g_telemetry_store.mutex);
    return 0;
}

// Get samples for member since timestamp
int telemetry_store_get_samples(const char* member_name, time_t since, 
                               telemetry_sample_t* samples, int max_samples) {
    if (!g_telemetry_store_initialized || !member_name || !samples || max_samples <= 0) {
        return -1;
    }
    
    pthread_mutex_lock(g_telemetry_store.mutex);
    
    int member_index = find_member_index(member_name);
    if (member_index == -1) {
        pthread_mutex_unlock(g_telemetry_store.mutex);
        return 0; // No samples for this member
    }
    
    telemetry_ring_buffer_t* buffer = g_telemetry_store.member_buffers[member_index];
    if (!buffer) {
        pthread_mutex_unlock(g_telemetry_store.mutex);
        return 0;
    }
    
    // Get items from ring buffer
    void* items[max_samples];
    int count = telemetry_ring_buffer_get_since(buffer, since, items, max_samples);
    
    // Copy to output array
    for (int i = 0; i < count; i++) {
        if (items[i]) {
            samples[i] = *(telemetry_sample_t*)items[i];
        }
    }
    
    pthread_mutex_unlock(g_telemetry_store.mutex);
    return count;
}

// Get events since timestamp
int telemetry_store_get_events(time_t since, telemetry_event_t* events, int max_events) {
    if (!g_telemetry_store_initialized || !events || max_events <= 0) {
        return -1;
    }
    
    pthread_mutex_lock(g_telemetry_store.mutex);
    
    if (!g_telemetry_store.events_buffer) {
        pthread_mutex_unlock(g_telemetry_store.mutex);
        return 0;
    }
    
    // Get items from events buffer
    void* items[max_events];
    int count = telemetry_ring_buffer_get_since(g_telemetry_store.events_buffer, since, items, max_events);
    
    // Copy to output array
    for (int i = 0; i < count; i++) {
        if (items[i]) {
            events[i] = *(telemetry_event_t*)items[i];
        }
    }
    
    pthread_mutex_unlock(g_telemetry_store.mutex);
    return count;
}

// Get all member names
int telemetry_store_get_members(char member_names[][128], int max_members) {
    if (!g_telemetry_store_initialized || !member_names || max_members <= 0) {
        return -1;
    }
    
    pthread_mutex_lock(g_telemetry_store.mutex);
    
    int count = (max_members < g_telemetry_store.member_count) ? max_members : g_telemetry_store.member_count;
    
    for (int i = 0; i < count; i++) {
        strncpy(member_names[i], g_telemetry_store.member_names[i], 128 - 1);
    }
    
    pthread_mutex_unlock(g_telemetry_store.mutex);
    return count;
}

// Update memory usage estimation
void update_memory_usage(void) {
    int64_t usage = 0;
    
    // Estimate memory usage for member buffers
    for (int i = 0; i < g_telemetry_store.member_count; i++) {
        if (g_telemetry_store.member_buffers[i]) {
            int size = telemetry_ring_buffer_size(g_telemetry_store.member_buffers[i]);
            usage += size * sizeof(telemetry_sample_t);
        }
    }
    
    // Estimate memory usage for events
    if (g_telemetry_store.events_buffer) {
        int size = telemetry_ring_buffer_size(g_telemetry_store.events_buffer);
        usage += size * sizeof(telemetry_event_t);
    }
    
    g_telemetry_store.memory_usage = usage;
    g_telemetry_store.status.memory_usage_mb = (int)(usage / 1024 / 1024);
}

// Check memory pressure
void check_memory_pressure(void) {
    update_memory_usage();
    
    int64_t max_memory = (int64_t)g_telemetry_store.config.max_ram_mb * 1024 * 1024;
    
    if (g_telemetry_store.memory_usage > max_memory) {
        // Memory pressure - downsample old data
        if (g_telemetry_store.config.enable_downsampling) {
            downsample_old_data();
        }
    }
    
    // Periodic cleanup
    time_t now = time(NULL);
    if (now - g_telemetry_store.last_cleanup > 3600) { // 1 hour
        telemetry_store_perform_cleanup();
        g_telemetry_store.last_cleanup = now;
    }
}

// Downsample old data
static void downsample_old_data(void) {
    // Simple downsampling: remove every Nth old sample
    time_t cutoff = time(NULL) - (g_telemetry_store.config.retention_hours * 3600 / 2); // Half retention period
    
    for (int i = 0; i < g_telemetry_store.member_count; i++) {
        if (g_telemetry_store.member_buffers[i]) {
            // This would implement downsampling logic
            // For now, just remove very old data
            telemetry_ring_buffer_remove_before(g_telemetry_store.member_buffers[i], cutoff);
        }
    }
}

// Get memory usage in MB
int telemetry_store_get_memory_usage(void) {
    if (!g_telemetry_store_initialized) return 0;
    
    pthread_mutex_lock(g_telemetry_store.mutex);
    update_memory_usage();
    int usage = g_telemetry_store.status.memory_usage_mb;
    pthread_mutex_unlock(g_telemetry_store.mutex);
    
    return usage;
}

// Perform cleanup of old data
void telemetry_store_perform_cleanup(void) {
    if (!g_telemetry_store_initialized) return;
    
    pthread_mutex_lock(g_telemetry_store.mutex);
    
    time_t cutoff = time(NULL) - (g_telemetry_store.config.retention_hours * 3600);
    
    // Cleanup member samples
    for (int i = 0; i < g_telemetry_store.member_count; i++) {
        if (g_telemetry_store.member_buffers[i]) {
            telemetry_ring_buffer_remove_before(g_telemetry_store.member_buffers[i], cutoff);
        }
    }
    
    // Cleanup events
    if (g_telemetry_store.events_buffer) {
        telemetry_ring_buffer_remove_before(g_telemetry_store.events_buffer, cutoff);
    }
    
    // Update memory usage
    update_memory_usage();
    g_telemetry_store.status.last_cleanup = time(NULL);
    
    pthread_mutex_unlock(g_telemetry_store.mutex);
}

// Get telemetry store status
void telemetry_store_get_status(telemetry_store_status_t* status) {
    if (!status || !g_telemetry_store_initialized) return;
    
    pthread_mutex_lock(g_telemetry_store.mutex);
    update_memory_usage();
    *status = g_telemetry_store.status;
    pthread_mutex_unlock(g_telemetry_store.mutex);
}

// Export telemetry data as JSON
void telemetry_store_export_json(time_t since, char* json_output, size_t max_size) {
    if (!g_telemetry_store_initialized || !json_output || max_size == 0) return;
    
    pthread_mutex_lock(g_telemetry_store.mutex);
    
    // Simple JSON export (in production would use proper JSON library)
    snprintf(json_output, max_size,
             "{"
             "\"timestamp\":%ld,"
             "\"members\":%d,"
             "\"total_samples\":%d,"
             "\"total_events\":%d,"
             "\"memory_usage_mb\":%d,"
             "\"retention_hours\":%d"
             "}",
             time(NULL),
             g_telemetry_store.member_count,
             g_telemetry_store.status.total_samples,
             g_telemetry_store.status.total_events,
             g_telemetry_store.status.memory_usage_mb,
             g_telemetry_store.config.retention_hours);
    
    pthread_mutex_unlock(g_telemetry_store.mutex);
}

// Check if telemetry store is initialized
bool telemetry_store_is_initialized(void) {
    return g_telemetry_store_initialized;
}

// Get telemetry store instance
telemetry_store_t* telemetry_store_get_instance(void) {
    return g_telemetry_store_initialized ? &g_telemetry_store : NULL;
}
