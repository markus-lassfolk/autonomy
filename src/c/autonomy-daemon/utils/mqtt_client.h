#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include "../telemetry/telemetry_store.h"
#include <stdbool.h>
#include <time.h>

// MQTT configuration
typedef struct {
    char broker_host[256];
    int broker_port;
    char client_id[128];
    char username[128];
    char password[128];
    bool use_tls;
    char ca_cert_path[256];
    char client_cert_path[256];
    char client_key_path[256];
    int keepalive_interval;
    int connection_timeout;
    bool clean_session;
    int max_inflight;
} mqtt_config_t;

// MQTT message
typedef struct {
    char topic[256];
    char payload[2048];
    int qos;
    bool retain;
    time_t timestamp;
} mqtt_message_t;

// MQTT client structure
typedef struct {
    mqtt_config_t config;
    
    // Connection state
    bool connected;
    bool connecting;
    time_t last_connect_attempt;
    time_t last_activity;
    
    // Statistics
    int messages_sent;
    int messages_received;
    int connection_attempts;
    int successful_connections;
    int failed_connections;
    
    // Message queue
    mqtt_message_t message_queue[100];
    int queue_head;
    int queue_tail;
    int queue_size;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} mqtt_client_t;

// Initialize MQTT client
int mqtt_client_init(const mqtt_config_t* config);

// Clean up MQTT client
void mqtt_client_cleanup(void);

// Connect to MQTT broker
int mqtt_client_connect(void);

// Disconnect from MQTT broker
int mqtt_client_disconnect(void);

// Publish message
int mqtt_client_publish(const char* topic, const char* payload, int qos, bool retain);

// Subscribe to topic
int mqtt_client_subscribe(const char* topic, int qos);

// Unsubscribe from topic
int mqtt_client_unsubscribe(const char* topic);

// Publish telemetry data
int mqtt_client_publish_telemetry(const telemetry_sample_t* sample);

// Publish event data
int mqtt_client_publish_event(const telemetry_event_t* event);

// Publish system status
int mqtt_client_publish_system_status(void);

// Get MQTT client status
void mqtt_client_get_status(mqtt_client_t* status);

// Check if MQTT client is initialized
bool mqtt_client_is_initialized(void);

// Check if MQTT client is connected
bool mqtt_client_is_connected(void);

// Get MQTT client instance
mqtt_client_t* mqtt_client_get_instance(void);

#endif // MQTT_CLIENT_H
