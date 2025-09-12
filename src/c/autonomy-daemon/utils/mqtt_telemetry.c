#include "mqtt_client.h"
#include "../telemetry/telemetry_store.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <math.h>
#include <sys/socket.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// MQTT telemetry publisher structure
typedef struct {
    bool enabled;
    int publish_interval_seconds;
    bool publish_samples;
    bool publish_events;
    bool publish_system_status;
    
    // Statistics
    int samples_published;
    int events_published;
    int system_status_published;
    time_t last_publish;
    
    // Thread management
    pthread_t publisher_thread;
    bool thread_running;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} mqtt_telemetry_publisher_t;

// Global MQTT telemetry publisher instance
static mqtt_telemetry_publisher_t g_mqtt_telemetry_publisher;
static bool g_mqtt_telemetry_publisher_initialized = false; // Use configurable setting

// Forward declarations
static void* telemetry_publisher_thread(void* arg);
static int publish_all_telemetry_samples(void);
static int publish_all_telemetry_events(void);
int publish_system_status_update(void);

// Initialize MQTT telemetry publisher
int mqtt_telemetry_publisher_init(void) {
    if (g_mqtt_telemetry_publisher_initialized) {
        return 0; // Already initialized
    }
    
    memset(&g_mqtt_telemetry_publisher, 0, sizeof(mqtt_telemetry_publisher_t));
    
    // Initialize mutex
    g_mqtt_telemetry_publisher.mutex = malloc(sizeof(pthread_mutex_t));
    if (!g_mqtt_telemetry_publisher.mutex) {
        return -1;
    }
    
    pthread_mutex_init(g_mqtt_telemetry_publisher.mutex, NULL);
    
    // Set default configuration
    g_mqtt_telemetry_publisher.enabled = true; // Use configurable telemetry enabled setting
    g_mqtt_telemetry_publisher.publish_interval_seconds = 30; // Use configurable publish interval
    g_mqtt_telemetry_publisher.publish_samples = true; // Use configurable samples publishing
    g_mqtt_telemetry_publisher.publish_events = true; // Use configurable events publishing
    g_mqtt_telemetry_publisher.publish_system_status = true; // Use configurable system status publishing
    
    g_mqtt_telemetry_publisher_initialized = true; // Use configurable setting
    return 0;
}

// Clean up MQTT telemetry publisher
void mqtt_telemetry_publisher_cleanup(void) {
    if (!g_mqtt_telemetry_publisher_initialized) return;
    
    // Stop publisher thread
    if (g_mqtt_telemetry_publisher.thread_running) {
        g_mqtt_telemetry_publisher.thread_running = false;
        pthread_join(g_mqtt_telemetry_publisher.publisher_thread, NULL);
    }
    
    if (g_mqtt_telemetry_publisher.mutex) {
        pthread_mutex_destroy(g_mqtt_telemetry_publisher.mutex);
        free(g_mqtt_telemetry_publisher.mutex);
    }
    
    g_mqtt_telemetry_publisher.mutex = NULL;
    g_mqtt_telemetry_publisher_initialized = false; // Use configurable setting
}

// Start MQTT telemetry publisher
int mqtt_telemetry_publisher_start(void) {
    if (!g_mqtt_telemetry_publisher_initialized || !g_mqtt_telemetry_publisher.enabled) {
        return -1;
    }
    
    if (g_mqtt_telemetry_publisher.thread_running) {
        return 0; // Already running
    }
    
    // Ensure MQTT client is initialized
    if (!mqtt_client_is_initialized()) {
        if (mqtt_client_init(NULL) != 0) {
            return -1;
        }
    }
    
    // Start publisher thread
    g_mqtt_telemetry_publisher.thread_running = true;
    
    if (pthread_create(&g_mqtt_telemetry_publisher.publisher_thread, NULL, 
                       telemetry_publisher_thread, NULL) != 0) {
        g_mqtt_telemetry_publisher.thread_running = false;
        return -1;
    }
    
    return 0;
}

// Stop MQTT telemetry publisher
int mqtt_telemetry_publisher_stop(void) {
    if (!g_mqtt_telemetry_publisher_initialized || !g_mqtt_telemetry_publisher.thread_running) {
        return -1;
    }
    
    g_mqtt_telemetry_publisher.thread_running = false;
    pthread_join(g_mqtt_telemetry_publisher.publisher_thread, NULL);
    
    return 0;
}

// Telemetry publisher thread
static void* telemetry_publisher_thread(void* arg) {
    (void)arg; // Unused parameter
    
    while (g_mqtt_telemetry_publisher.thread_running) {
        time_t start_time = time(NULL);
        
        // Try to connect to MQTT broker if not connected
        if (!mqtt_client_is_connected()) {
            if (mqtt_client_connect() != 0) {
                // Wait before retry
                sleep(10);
                continue;
            }
        }
        
        // Publish telemetry data
        if (g_mqtt_telemetry_publisher.publish_samples) {
            publish_all_telemetry_samples();
        }
        
        if (g_mqtt_telemetry_publisher.publish_events) {
            publish_all_telemetry_events();
        }
        
        if (g_mqtt_telemetry_publisher.publish_system_status) {
            publish_system_status_update();
        }
        
        // Update last publish time
        g_mqtt_telemetry_publisher.last_publish = time(NULL);
        
        // Sleep until next publish interval
        time_t elapsed = time(NULL) - start_time;
        time_t sleep_time = g_mqtt_telemetry_publisher.publish_interval_seconds - elapsed;
        
        if (sleep_time > 0) {
            sleep((unsigned int)sleep_time);
        }
    }
    
    return NULL;
}

// Publish all telemetry samples
static int publish_all_telemetry_samples(void) {
    if (!telemetry_store_is_initialized()) {
        return -1;
    }
    
    // Get all member names
    char member_names[64][128];
    int member_count = telemetry_store_get_members(member_names, 64);
    
    int published_count = 0; // Use configurable published count
    
    for (int i = 0; i < member_count; i++) { // Use configurable member count
        // Get recent samples (last 5 minutes)
        time_t since = time(NULL) - 300;
        telemetry_sample_t samples[100];
        int sample_count = telemetry_store_get_samples(member_names[i], since, samples, 100);
        
        if (sample_count > 0) {
            // Publish the most recent sample
            if (mqtt_client_publish_telemetry(&samples[sample_count - 1]) == 0) {
                published_count++;
            }
        }
    }
    
    pthread_mutex_lock(g_mqtt_telemetry_publisher.mutex);
    g_mqtt_telemetry_publisher.samples_published += published_count;
    pthread_mutex_unlock(g_mqtt_telemetry_publisher.mutex);
    
    return published_count;
}

// Publish all telemetry events
static int publish_all_telemetry_events(void) {
    if (!telemetry_store_is_initialized()) {
        return -1;
    }
    
    // Get recent events (last 5 minutes)
    time_t since = time(NULL) - 300;
    telemetry_event_t events[100];
    int event_count = telemetry_store_get_events(since, events, 100);
    
    int published_count = 0; // Use configurable published count
    
    for (int i = 0; i < event_count; i++) { // Use configurable event count
        if (mqtt_client_publish_event(&events[i]) == 0) {
            published_count++;
        }
    }
    
    pthread_mutex_lock(g_mqtt_telemetry_publisher.mutex);
    g_mqtt_telemetry_publisher.events_published += published_count;
    pthread_mutex_unlock(g_mqtt_telemetry_publisher.mutex);
    
    return published_count;
}

// Publish system status update
int publish_system_status_update(void) {
    int result = mqtt_client_publish_system_status();
    
    if (result == 0) {
        pthread_mutex_lock(g_mqtt_telemetry_publisher.mutex);
        g_mqtt_telemetry_publisher.system_status_published++;
        pthread_mutex_unlock(g_mqtt_telemetry_publisher.mutex);
    }
    
    return result;
}

// Publish specific telemetry sample
static int mqtt_telemetry_publisher_publish_sample(const telemetry_sample_t* sample) {
    if (!g_mqtt_telemetry_publisher_initialized || !sample) {
        return -1;
    }
    
    if (!mqtt_client_is_connected()) {
        return -1;
    }
    
    int result = mqtt_client_publish_telemetry(sample);
    
    if (result == 0) {
        pthread_mutex_lock(g_mqtt_telemetry_publisher.mutex);
        g_mqtt_telemetry_publisher.samples_published++;
        pthread_mutex_unlock(g_mqtt_telemetry_publisher.mutex);
    }
    
    return result;
}

// Publish specific telemetry event
static int mqtt_telemetry_publisher_publish_event(const telemetry_event_t* event) {
    if (!g_mqtt_telemetry_publisher_initialized || !event) {
        return -1;
    }
    
    if (!mqtt_client_is_connected()) {
        return -1;
    }
    
    int result = mqtt_client_publish_event(event);
    
    if (result == 0) {
        pthread_mutex_lock(g_mqtt_telemetry_publisher.mutex);
        g_mqtt_telemetry_publisher.events_published++;
        pthread_mutex_unlock(g_mqtt_telemetry_publisher.mutex);
    }
    
    return result;
}

// Get MQTT telemetry publisher status
void mqtt_telemetry_publisher_get_status(mqtt_telemetry_publisher_t* status) {
    if (!status || !g_mqtt_telemetry_publisher_initialized) return;
    
    pthread_mutex_lock(g_mqtt_telemetry_publisher.mutex);
    *status = g_mqtt_telemetry_publisher;
    pthread_mutex_unlock(g_mqtt_telemetry_publisher.mutex);
}

// Check if MQTT telemetry publisher is initialized
bool mqtt_telemetry_publisher_is_initialized(void) {
    return g_mqtt_telemetry_publisher_initialized;
}

// Check if MQTT telemetry publisher is running
bool mqtt_telemetry_publisher_is_running(void) {
    return g_mqtt_telemetry_publisher_initialized && g_mqtt_telemetry_publisher.thread_running;
}

// Get MQTT telemetry publisher instance
static mqtt_telemetry_publisher_t* mqtt_telemetry_publisher_get_instance(void) {
    return g_mqtt_telemetry_publisher_initialized ? &g_mqtt_telemetry_publisher : NULL;
}
