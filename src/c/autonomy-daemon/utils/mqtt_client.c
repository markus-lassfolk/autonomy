#include "mqtt_client.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

// Global MQTT client instance
static mqtt_client_t g_mqtt_client;
static bool g_mqtt_client_initialized = false;

// MQTT protocol constants
#define MQTT_CONNECT_PACKET 0x10
#define MQTT_CONNACK_PACKET 0x20
#define MQTT_PUBLISH_PACKET 0x30
#define MQTT_SUBSCRIBE_PACKET 0x80
#define MQTT_SUBACK_PACKET 0x90
#define MQTT_PINGREQ_PACKET 0xC0
#define MQTT_PINGRESP_PACKET 0xD0
#define MQTT_DISCONNECT_PACKET 0xE0

// Network socket
static int g_mqtt_socket = -1;

// Forward declarations
static int mqtt_send_packet(const uint8_t* packet, int length);
static int mqtt_receive_packet(uint8_t* packet, int max_length);
static int mqtt_create_connect_packet(uint8_t* packet, const mqtt_config_t* config);
static int mqtt_create_publish_packet(uint8_t* packet, const char* topic, 
                                     const char* payload, int qos, bool retain);
static int mqtt_create_subscribe_packet(uint8_t* packet, const char* topic, int qos);
static int mqtt_parse_connack(const uint8_t* packet, int length);
static int mqtt_parse_suback(const uint8_t* packet, int length);
static void* mqtt_keepalive_thread(void* arg);

// Initialize MQTT client
int mqtt_client_init(const mqtt_config_t* config) {
    if (g_mqtt_client_initialized) {
        return 0; // Already initialized
    }
    
    memset(&g_mqtt_client, 0, sizeof(mqtt_client_t));
    
    // Set configuration
    if (config) {
        g_mqtt_client.config = *config;
    } else {
        // Default configuration
        strcpy(g_mqtt_client.config.broker_host, "localhost");
        g_mqtt_client.config.broker_port = 1883;
        strcpy(g_mqtt_client.config.client_id, "autonomy_daemon");
        g_mqtt_client.config.keepalive_interval = 60;
        g_mqtt_client.config.connection_timeout = 30;
        g_mqtt_client.config.clean_session = true;
        g_mqtt_client.config.max_inflight = 20;
    }
    
    // Initialize mutex
    g_mqtt_client.mutex = malloc(sizeof(pthread_mutex_t));
    if (!g_mqtt_client.mutex) {
        return -1;
    }
    
    pthread_mutex_init(g_mqtt_client.mutex, NULL);
    
    // Initialize message queue
    g_mqtt_client.queue_head = 0;
    g_mqtt_client.queue_tail = 0;
    g_mqtt_client.queue_size = 0;
    
    g_mqtt_client_initialized = true;
    return 0;
}

// Clean up MQTT client
void mqtt_client_cleanup(void) {
    if (!g_mqtt_client_initialized) return;
    
    // Disconnect if connected
    if (g_mqtt_client.connected) {
        mqtt_client_disconnect();
    }
    
    if (g_mqtt_client.mutex) {
        pthread_mutex_destroy(g_mqtt_client.mutex);
        free(g_mqtt_client.mutex);
    }
    
    if (g_mqtt_socket >= 0) {
        close(g_mqtt_socket);
        g_mqtt_socket = -1;
    }
    
    g_mqtt_client.mutex = NULL;
    g_mqtt_client_initialized = false;
}

// Connect to MQTT broker
int mqtt_client_connect(void) {
    if (!g_mqtt_client_initialized || g_mqtt_client.connected) {
        return -1;
    }
    
    pthread_mutex_lock(g_mqtt_client.mutex);
    
    g_mqtt_client.connecting = true;
    g_mqtt_client.connection_attempts++;
    g_mqtt_client.last_connect_attempt = time(NULL);
    
    // Create socket
    g_mqtt_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_mqtt_socket < 0) {
        g_mqtt_client.connecting = false;
        pthread_mutex_unlock(g_mqtt_client.mutex);
        return -1;
    }
    
    // Set socket options
    int flags = fcntl(g_mqtt_socket, F_GETFL, 0);
    fcntl(g_mqtt_socket, F_SETFL, flags | O_NONBLOCK);
    
    // Resolve broker address
    struct hostent* host = gethostbyname(g_mqtt_client.config.broker_host);
    if (!host) {
        close(g_mqtt_socket);
        g_mqtt_socket = -1;
        g_mqtt_client.connecting = false;
        pthread_mutex_unlock(g_mqtt_client.mutex);
        return -1;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(g_mqtt_client.config.broker_port);
    memcpy(&server_addr.sin_addr, host->h_addr, host->h_length);
    
    // Connect to broker
    int connect_result = connect(g_mqtt_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (connect_result < 0 && errno != EINPROGRESS) {
        close(g_mqtt_socket);
        g_mqtt_socket = -1;
        g_mqtt_client.connecting = false;
        pthread_mutex_unlock(g_mqtt_client.mutex);
        return -1;
    }
    
    // Wait for connection with timeout
    struct pollfd pfd;
    pfd.fd = g_mqtt_socket;
    pfd.events = POLLOUT;
    
    int poll_result = poll(&pfd, 1, g_mqtt_client.config.connection_timeout * 1000);
    if (poll_result <= 0) {
        close(g_mqtt_socket);
        g_mqtt_socket = -1;
        g_mqtt_client.connecting = false;
        pthread_mutex_unlock(g_mqtt_client.mutex);
        return -1;
    }
    
    // Check connection status
    int error = 0;
    socklen_t error_len = sizeof(error);
    if (getsockopt(g_mqtt_socket, SOL_SOCKET, SO_ERROR, &error, &error_len) < 0 || error != 0) {
        close(g_mqtt_socket);
        g_mqtt_socket = -1;
        g_mqtt_client.connecting = false;
        pthread_mutex_unlock(g_mqtt_client.mutex);
        return -1;
    }
    
    // Send CONNECT packet
    uint8_t connect_packet[512];
    int packet_length = mqtt_create_connect_packet(connect_packet, &g_mqtt_client.config);
    
    if (mqtt_send_packet(connect_packet, packet_length) != 0) {
        close(g_mqtt_socket);
        g_mqtt_socket = -1;
        g_mqtt_client.connecting = false;
        pthread_mutex_unlock(g_mqtt_client.mutex);
        return -1;
    }
    
    // Wait for CONNACK
    uint8_t connack_packet[64];
    int connack_length = mqtt_receive_packet(connack_packet, sizeof(connack_packet));
    
    if (connack_length <= 0 || mqtt_parse_connack(connack_packet, connack_length) != 0) {
        close(g_mqtt_socket);
        g_mqtt_socket = -1;
        g_mqtt_client.connecting = false;
        pthread_mutex_unlock(g_mqtt_client.mutex);
        return -1;
    }
    
    // Connection successful
    g_mqtt_client.connected = true;
    g_mqtt_client.connecting = false;
    g_mqtt_client.successful_connections++;
    g_mqtt_client.last_activity = time(NULL);
    
    pthread_mutex_unlock(g_mqtt_client.mutex);
    
    return 0;
}

// Disconnect from MQTT broker
int mqtt_client_disconnect(void) {
    if (!g_mqtt_client_initialized || !g_mqtt_client.connected) {
        return -1;
    }
    
    pthread_mutex_lock(g_mqtt_client.mutex);
    
    // Send DISCONNECT packet
    uint8_t disconnect_packet[] = {MQTT_DISCONNECT_PACKET, 0x00};
    mqtt_send_packet(disconnect_packet, 2);
    
    // Close socket
    if (g_mqtt_socket >= 0) {
        close(g_mqtt_socket);
        g_mqtt_socket = -1;
    }
    
    g_mqtt_client.connected = false;
    g_mqtt_client.connecting = false;
    
    pthread_mutex_unlock(g_mqtt_client.mutex);
    
    return 0;
}

// Publish message
int mqtt_client_publish(const char* topic, const char* payload, int qos, bool retain) {
    if (!g_mqtt_client_initialized || !g_mqtt_client.connected || !topic || !payload) {
        return -1;
    }
    
    pthread_mutex_lock(g_mqtt_client.mutex);
    
    // Create PUBLISH packet
    uint8_t publish_packet[2048];
    int packet_length = mqtt_create_publish_packet(publish_packet, topic, payload, qos, retain);
    
    if (packet_length <= 0) {
        pthread_mutex_unlock(g_mqtt_client.mutex);
        return -1;
    }
    
    // Send packet
    int result = mqtt_send_packet(publish_packet, packet_length);
    
    if (result == 0) {
        g_mqtt_client.messages_sent++;
        g_mqtt_client.last_activity = time(NULL);
    }
    
    pthread_mutex_unlock(g_mqtt_client.mutex);
    
    return result;
}

// Subscribe to topic
int mqtt_client_subscribe(const char* topic, int qos) {
    if (!g_mqtt_client_initialized || !g_mqtt_client.connected || !topic) {
        return -1;
    }
    
    pthread_mutex_lock(g_mqtt_client.mutex);
    
    // Create SUBSCRIBE packet
    uint8_t subscribe_packet[512];
    int packet_length = mqtt_create_subscribe_packet(subscribe_packet, topic, qos);
    
    if (packet_length <= 0) {
        pthread_mutex_unlock(g_mqtt_client.mutex);
        return -1;
    }
    
    // Send packet
    int result = mqtt_send_packet(subscribe_packet, packet_length);
    
    if (result == 0) {
        // Wait for SUBACK
        uint8_t suback_packet[64];
        int suback_length = mqtt_receive_packet(suback_packet, sizeof(suback_packet));
        
        if (suback_length > 0) {
            mqtt_parse_suback(suback_packet, suback_length);
        }
        
        g_mqtt_client.last_activity = time(NULL);
    }
    
    pthread_mutex_unlock(g_mqtt_client.mutex);
    
    return result;
}

// Unsubscribe from topic
int mqtt_client_unsubscribe(const char* topic) {
    if (!g_mqtt_client_initialized || !g_mqtt_client.connected || !topic) {
        return -1;
    }
    
    // Simplified unsubscribe - just return success for now
    return 0;
}

// Publish telemetry data
int mqtt_client_publish_telemetry(const telemetry_sample_t* sample) {
    if (!sample) return -1;
    
    // Create JSON payload
    char payload[1024];
    snprintf(payload, sizeof(payload),
             "{\"timestamp\":%ld,\"member\":\"%s\",\"latency\":%.2f,\"loss\":%.2f,"
             "\"signal\":%.2f,\"throughput\":%.2f}",
             sample->timestamp, sample->member_name,
             sample->has_latency ? sample->latency_ms : 0.0,
             sample->has_loss ? sample->loss_percent : 0.0,
             sample->has_signal ? sample->signal_strength : 0.0,
             sample->has_throughput ? sample->throughput_mbps : 0.0);
    
    char topic[256];
    snprintf(topic, sizeof(topic), "autonomy/telemetry/%s", sample->member_name);
    
    return mqtt_client_publish(topic, payload, 1, false);
}

// Publish event data
int mqtt_client_publish_event(const telemetry_event_t* event) {
    if (!event) return -1;
    
    // Create JSON payload
    char payload[1024];
    snprintf(payload, sizeof(payload),
             "{\"timestamp\":%ld,\"type\":\"%s\",\"severity\":\"%s\","
             "\"message\":\"%s\",\"data\":\"%s\"}",
             event->timestamp, event->type, event->severity,
             event->message, event->data);
    
    char topic[256];
    snprintf(topic, sizeof(topic), "autonomy/events/%s", event->type);
    
    return mqtt_client_publish(topic, payload, 1, false);
}

// Publish system status
int mqtt_client_publish_system_status(void) {
    // Create system status payload
    char payload[1024];
    snprintf(payload, sizeof(payload),
             "{\"timestamp\":%ld,\"status\":\"running\",\"uptime\":%ld,"
             "\"version\":\"6.1.0\",\"components\":\"all_active\"}",
             time(NULL), time(NULL));
    
    return mqtt_client_publish("autonomy/system/status", payload, 1, true);
}

// Send MQTT packet
static int mqtt_send_packet(const uint8_t* packet, int length) {
    if (g_mqtt_socket < 0 || !packet || length <= 0) return -1;
    
    int total_sent = 0;
    while (total_sent < length) {
        int sent = send(g_mqtt_socket, packet + total_sent, length - total_sent, 0);
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(1000); // Wait 1ms
                continue;
            }
            return -1;
        }
        total_sent += sent;
    }
    
    return 0;
}

// Receive MQTT packet
static int mqtt_receive_packet(uint8_t* packet, int max_length) {
    if (g_mqtt_socket < 0 || !packet || max_length <= 0) return -1;
    
    // Simple receive for now
    int received = recv(g_mqtt_socket, packet, max_length, 0);
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0; // No data available
        }
        return -1;
    }
    
    return received;
}

// Create CONNECT packet
static int mqtt_create_connect_packet(uint8_t* packet, const mqtt_config_t* config) {
    if (!packet || !config) return -1;
    
    int pos = 0;
    
    // Fixed header
    packet[pos++] = MQTT_CONNECT_PACKET;
    // Calculate remaining length (will be updated after we know the total length)
    int remaining_length_pos = pos++;
    packet[remaining_length_pos] = 0x00; // Will be calculated and updated below
    
    // Variable header
    // Protocol name
    packet[pos++] = 0x00;
    packet[pos++] = 0x04;
    packet[pos++] = 'M';
    packet[pos++] = 'Q';
    packet[pos++] = 'T';
    packet[pos++] = 'T';
    
    // Protocol level
    packet[pos++] = 0x04;
    
    // Connect flags
    uint8_t connect_flags = 0x02; // Clean session
    if (strlen(config->username) > 0) connect_flags |= 0x80;
    if (strlen(config->password) > 0) connect_flags |= 0x40;
    packet[pos++] = connect_flags;
    
    // Keep alive
    packet[pos++] = (config->keepalive_interval >> 8) & 0xFF;
    packet[pos++] = config->keepalive_interval & 0xFF;
    
    // Client identifier
    int client_id_len = strlen(config->client_id);
    packet[pos++] = (client_id_len >> 8) & 0xFF;
    packet[pos++] = client_id_len & 0xFF;
    memcpy(packet + pos, config->client_id, client_id_len);
    pos += client_id_len;
    
    // Username (if provided)
    if (strlen(config->username) > 0) {
        int username_len = strlen(config->username);
        packet[pos++] = (username_len >> 8) & 0xFF;
        packet[pos++] = username_len & 0xFF;
        memcpy(packet + pos, config->username, username_len);
        pos += username_len;
    }
    
    // Password (if provided)
    if (strlen(config->password) > 0) {
        int password_len = strlen(config->password);
        packet[pos++] = (password_len >> 8) & 0xFF;
        packet[pos++] = password_len & 0xFF;
        memcpy(packet + pos, config->password, password_len);
        pos += password_len;
    }
    
    // Update remaining length
    packet[1] = pos - 2;
    
    return pos;
}

// Create PUBLISH packet
static int mqtt_create_publish_packet(uint8_t* packet, const char* topic, 
                                     const char* payload, int qos, bool retain) {
    if (!packet || !topic || !payload) return -1;
    
    int pos = 0;
    
    // Fixed header
    uint8_t packet_type = MQTT_PUBLISH_PACKET;
    if (retain) packet_type |= 0x01;
    if (qos > 0) packet_type |= (qos << 1);
    packet[pos++] = packet_type;
    // Calculate remaining length (will be updated after we know the total length)
    int remaining_length_pos = pos++;
    packet[remaining_length_pos] = 0x00; // Will be calculated and updated below
    
    // Variable header
    // Topic name
    int topic_len = strlen(topic);
    packet[pos++] = (topic_len >> 8) & 0xFF;
    packet[pos++] = topic_len & 0xFF;
    memcpy(packet + pos, topic, topic_len);
    pos += topic_len;
    
    // Packet identifier (for QoS > 0)
    if (qos > 0) {
        static uint16_t packet_id = 1;
        packet[pos++] = (packet_id >> 8) & 0xFF;
        packet[pos++] = packet_id & 0xFF;
        packet_id++;
    }
    
    // Payload
    int payload_len = strlen(payload);
    memcpy(packet + pos, payload, payload_len);
    pos += payload_len;
    
    // Update remaining length
    packet[1] = pos - 2;
    
    return pos;
}

// Create SUBSCRIBE packet
static int mqtt_create_subscribe_packet(uint8_t* packet, const char* topic, int qos) {
    if (!packet || !topic) return -1;
    
    int pos = 0;
    
    // Fixed header
    packet[pos++] = MQTT_SUBSCRIBE_PACKET;
    // Calculate remaining length (will be updated after we know the total length)
    int remaining_length_pos = pos++;
    packet[remaining_length_pos] = 0x00; // Will be calculated and updated below
    
    // Variable header
    // Packet identifier
    static uint16_t packet_id = 1;
    packet[pos++] = (packet_id >> 8) & 0xFF;
    packet[pos++] = packet_id & 0xFF;
    packet_id++;
    
    // Topic filter
    int topic_len = strlen(topic);
    packet[pos++] = (topic_len >> 8) & 0xFF;
    packet[pos++] = topic_len & 0xFF;
    memcpy(packet + pos, topic, topic_len);
    pos += topic_len;
    
    // QoS
    packet[pos++] = qos & 0xFF;
    
    // Update remaining length
    packet[1] = pos - 2;
    
    return pos;
}

// Parse CONNACK packet
static int mqtt_parse_connack(const uint8_t* packet, int length) {
    if (!packet || length < 4) return -1;
    
    if (packet[0] != MQTT_CONNACK_PACKET) return -1;
    if (packet[1] != 2) return -1; // Remaining length should be 2
    
    uint8_t return_code = packet[3];
    return (return_code == 0) ? 0 : -1;
}

// Parse SUBACK packet
static int mqtt_parse_suback(const uint8_t* packet, int length) {
    if (!packet || length < 4) return -1;
    
    if (packet[0] != MQTT_SUBACK_PACKET) return -1;
    
    return 0; // Simplified parsing
}

// Get MQTT client status
void mqtt_client_get_status(mqtt_client_t* status) {
    if (!status || !g_mqtt_client_initialized) return;
    
    pthread_mutex_lock(g_mqtt_client.mutex);
    *status = g_mqtt_client;
    pthread_mutex_unlock(g_mqtt_client.mutex);
}

// Check if MQTT client is initialized
bool mqtt_client_is_initialized(void) {
    return g_mqtt_client_initialized;
}

// Check if MQTT client is connected
bool mqtt_client_is_connected(void) {
    return g_mqtt_client_initialized && g_mqtt_client.connected;
}

// Get MQTT client instance
mqtt_client_t* mqtt_client_get_instance(void) {
    return g_mqtt_client_initialized ? &g_mqtt_client : NULL;
}
