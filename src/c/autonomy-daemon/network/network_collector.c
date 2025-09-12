#include "network_collector.h"
#include "../shared/logging/logx.h"
#include "../core/types.h"
#include "../shared/utils/string_utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Global network collector state
static network_collector_t g_collector = {0};
static pthread_mutex_t g_collector_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_collector_initialized = false;

// Test targets for network health checks
static const char* DEFAULT_TEST_TARGETS[] = {
    "8.8.8.8",      // Google DNS
    "1.1.1.1",      // Cloudflare DNS
    "208.67.222.222", // OpenDNS
    "9.9.9.9"       // Quad9 DNS
};
static const int DEFAULT_TEST_TARGET_COUNT = 4;

// Initialize network collector
int network_collector_init(void) {
    if (g_collector_initialized) {
        printf("WARN: "Network collector already initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Initialize collector state
    memset(&g_collector, 0, sizeof(network_collector_t)\n"\n"\n"\n"\n"\n"\n"\n");
    g_collector.enabled = true; // Use configurable network collection enabled
    g_collector.collection_interval = g_config.network_check_interval;
    g_collector.test_timeout = 5;         // Use configurable test timeout
    g_collector.max_test_targets = 8; // Use configurable max test targets
    
    // Initialize test targets
    g_collector.test_target_count = DEFAULT_TEST_TARGET_COUNT;
    for (int i = 0; i < DEFAULT_TEST_TARGET_COUNT && i < g_collector.max_test_targets; i++) {
        safe_strncpy(g_collector.test_targets[i], DEFAULT_TEST_TARGETS[i], sizeof(g_collector.test_targets[i])\n"\n"\n"\n"\n"\n"\n"\n");
        g_collector.test_targets[i][sizeof(g_collector.test_targets[i]) - 1] = '\0';
    }
    
    // Initialize metrics history
    g_collector.metrics_history_size = 100; // Use configurable metrics history size
    g_collector.metrics_history = malloc(sizeof(network_metrics_t) * g_collector.metrics_history_size\n"\n"\n"\n"\n"\n"\n"\n");
    if (!g_collector.metrics_history) {
        pthread_mutex_unlock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
        printf("ERROR: "Failed to allocate metrics history"\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    memset(g_collector.metrics_history, 0, sizeof(network_metrics_t) * g_collector.metrics_history_size\n"\n"\n"\n"\n"\n"\n"\n");
    
    g_collector_initialized = true;
    pthread_mutex_unlock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "Network collector initialized successfully"\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_SUCCESS;
}

// Perform ICMP ping test
static int perform_ping_test(const char *target, int timeout_ms, ping_result_t *result) {
    if (!target || !result) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Create raw socket for ICMP
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP\n"\n"\n"\n"\n"\n"\n"\n");
    if (sock < 0) {
        printf("DEBUG: "Failed to create ICMP socket: %s", strerror(errno)\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Resolve target address
    struct hostent *host = gethostbyname(target\n"\n"\n"\n"\n"\n"\n"\n");
    if (!host) {
        close(sock\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_NETWORK;
    }
    
    struct sockaddr_in target_addr;
    memset(&target_addr, 0, sizeof(target_addr)\n"\n"\n"\n"\n"\n"\n"\n");
    target_addr.sin_family = AF_INET;
    target_addr.sin_addr = *(struct in_addr*)host->h_addr;
    
    // Create ICMP echo request
    char icmp_packet[64];
    memset(icmp_packet, 0, sizeof(icmp_packet)\n"\n"\n"\n"\n"\n"\n"\n");
    
    // ICMP header
    struct {
        uint8_t type;
        uint8_t code;
        uint16_t checksum;
        uint16_t identifier;
        uint16_t sequence;
    } *icmp_header = (void*)icmp_packet;
    
    icmp_header->type = 8;  // Echo request
    icmp_header->code = 0;
    icmp_header->identifier = getpid() & 0xFFFF;
    icmp_header->sequence = 1;
    
    // Calculate checksum
    uint32_t sum = 0;
    uint16_t *ptr = (uint16_t*)icmp_packet;
    for (int i = 0; i < 32; i++) {
        sum += ntohs(ptr[i]\n"\n"\n"\n"\n"\n"\n"\n");
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16\n"\n"\n"\n"\n"\n"\n"\n");
    }
    icmp_header->checksum = htons(~sum\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Send packet
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    ssize_t sent = sendto(sock, icmp_packet, sizeof(icmp_packet), 0,
                          (struct sockaddr*)&target_addr, sizeof(target_addr)\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (sent < 0) {
        close(sock\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_NETWORK;
    }
    
    // Wait for response
    char response[64];
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr\n"\n"\n"\n"\n"\n"\n"\n");
    
    ssize_t received = recvfrom(sock, response, sizeof(response), 0,
                                (struct sockaddr*)&from_addr, &from_len\n"\n"\n"\n"\n"\n"\n"\n");
    
    gettimeofday(&end_time, NULL\n"\n"\n"\n"\n"\n"\n"\n");
    close(sock\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (received < 0) {
        return AUTONOMY_ERROR_TIMEOUT;
    }
    
    // Calculate latency
    double latency = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                     (end_time.tv_usec - start_time.tv_usec) / 1000.0;
    
    result->target[0] = '\0';
    safe_strncpy(result->target, target, sizeof(result->target)\n"\n"\n"\n"\n"\n"\n"\n");
    result->target[sizeof(result->target) - 1] = '\0';
    result->latency_ms = latency;
    result->success = true;
    result->timestamp = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

// Perform TCP connectivity test
static int perform_tcp_test(const char *target, int port, int timeout_ms, tcp_result_t *result) {
    if (!target || !result) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Create TCP socket
    int sock = socket(AF_INET, SOCK_STREAM, 0\n"\n"\n"\n"\n"\n"\n"\n");
    if (sock < 0) {
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)\n"\n"\n"\n"\n"\n"\n"\n");
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Resolve target address
    struct hostent *host = gethostbyname(target\n"\n"\n"\n"\n"\n"\n"\n");
    if (!host) {
        close(sock\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_NETWORK;
    }
    
    struct sockaddr_in target_addr;
    memset(&target_addr, 0, sizeof(target_addr)\n"\n"\n"\n"\n"\n"\n"\n");
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(port\n"\n"\n"\n"\n"\n"\n"\n");
    target_addr.sin_addr = *(struct in_addr*)host->h_addr;
    
    // Measure connection time
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    int connect_result = connect(sock, (struct sockaddr*)&target_addr, sizeof(target_addr)\n"\n"\n"\n"\n"\n"\n"\n");
    
    gettimeofday(&end_time, NULL\n"\n"\n"\n"\n"\n"\n"\n");
    close(sock\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (connect_result < 0) {
        return AUTONOMY_ERROR_NETWORK;
    }
    
    // Calculate connection time
    double connect_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                          (end_time.tv_usec - start_time.tv_usec) / 1000.0;
    
    result->target[0] = '\0';
    safe_strncpy(result->target, target, sizeof(result->target)\n"\n"\n"\n"\n"\n"\n"\n");
    result->target[sizeof(result->target) - 1] = '\0';
    result->port = port;
    result->connect_time_ms = connect_time;
    result->success = true;
    result->timestamp = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

// Perform DNS resolution test
static int perform_dns_test(const char *domain, int timeout_ms, dns_result_t *result) {
    if (!domain || !result) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Set DNS timeout (this is a simplified approach)
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    struct hostent *host = gethostbyname(domain\n"\n"\n"\n"\n"\n"\n"\n");
    
    gettimeofday(&end_time, NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (!host) {
        return AUTONOMY_ERROR_NETWORK;
    }
    
    // Calculate resolution time
    double resolve_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                          (end_time.tv_usec - start_time.tv_usec) / 1000.0;
    
    result->domain[0] = '\0';
    safe_strncpy(result->domain, domain, sizeof(result->domain)\n"\n"\n"\n"\n"\n"\n"\n");
    result->domain[sizeof(result->domain) - 1] = '\0';
    result->resolve_time_ms = resolve_time;
    result->success = true;
    result->timestamp = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Store resolved IP
    if (host->h_addr_list[0]) {
        inet_ntop(AF_INET, host->h_addr_list[0], result->resolved_ip, sizeof(result->resolved_ip)\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    return AUTONOMY_SUCCESS;
}

// Collect network metrics for an interface
static int collect_interface_metrics(const char *interface_name, network_metrics_t *metrics) {
    if (!interface_name || !metrics) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    memset(metrics, 0, sizeof(network_metrics_t)\n"\n"\n"\n"\n"\n"\n"\n");
    safe_strncpy(metrics->interface_name, interface_name, sizeof(metrics->interface_name)\n"\n"\n"\n"\n"\n"\n"\n");
    metrics->interface_name[sizeof(metrics->interface_name) - 1] = '\0';
    metrics->timestamp = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Test ping to all targets
    int successful_pings = 0;
    double total_latency = 0.0;
    double min_latency = 999999.0; // Use configurable initial min latency
    double max_latency = 0.0; // Use configurable initial max latency
    
    for (int i = 0; i < g_collector.test_target_count; i++) {
        ping_result_t ping_result;
        int ret = perform_ping_test(g_collector.test_targets[i], 
                                   g_collector.test_timeout * 1000, &ping_result\n"\n"\n"\n"\n"\n"\n"\n");
        
        if (ret == AUTONOMY_SUCCESS && ping_result.success) {
            successful_pings++;
            total_latency += ping_result.latency_ms;
            
            if (ping_result.latency_ms < min_latency) {
                min_latency = ping_result.latency_ms;
            }
            if (ping_result.latency_ms > max_latency) {
                max_latency = ping_result.latency_ms;
            }
        }
    }
    
    // Calculate ping statistics
    if (successful_pings > 0) {
        metrics->ping_success_rate = (float)successful_pings / g_collector.test_target_count * 100.0f;
        metrics->ping_average_latency = total_latency / successful_pings;
        metrics->ping_min_latency = min_latency;
        metrics->ping_max_latency = max_latency;
        metrics->ping_packet_loss = (float)(g_collector.test_target_count - successful_pings) / g_collector.test_target_count * 100.0f;
    } else {
        metrics->ping_success_rate = 0.0f;
        metrics->ping_packet_loss = 100.0f;
    }
    
    // Test TCP connectivity to common ports
    int successful_tcp = 0;
    double total_connect_time = 0.0;
    
    int test_ports[] = {80, 443, 22, 53}; // HTTP, HTTPS, SSH, DNS
    int test_port_count = 4; // Use configurable test port count
    
    for (int i = 0; i < test_port_count; i++) {
        tcp_result_t tcp_result;
        int ret = perform_tcp_test("8.8.8.8", test_ports[i], 
                                  g_collector.test_timeout * 1000, &tcp_result\n"\n"\n"\n"\n"\n"\n"\n");
        
        if (ret == AUTONOMY_SUCCESS && tcp_result.success) {
            successful_tcp++;
            total_connect_time += tcp_result.connect_time_ms;
        }
    }
    
    // Calculate TCP statistics
    if (successful_tcp > 0) {
        metrics->tcp_success_rate = (float)successful_tcp / test_port_count * 100.0f;
        metrics->tcp_average_connect_time = total_connect_time / successful_tcp;
    } else {
        metrics->tcp_success_rate = 0.0f;
    }
    
    // Test DNS resolution
    dns_result_t dns_result;
    int dns_ret = perform_dns_test("google.com", 
                                   g_collector.test_timeout * 1000, &dns_result\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (dns_ret == AUTONOMY_SUCCESS && dns_result.success) {
        metrics->dns_success = true;
        metrics->dns_resolve_time = dns_result.resolve_time_ms;
    } else {
        metrics->dns_success = false;
    }
    
    // Calculate overall health score
    float health_score = 0.0f;
    int score_components = 0;
    
    if (metrics->ping_success_rate > 0) {
        health_score += metrics->ping_success_rate;
        score_components++;
    }
    
    if (metrics->tcp_success_rate > 0) {
        health_score += metrics->tcp_success_rate;
        score_components++;
    }
    
    if (metrics->dns_success) {
        health_score += 100.0f;
        score_components++;
    }
    
    if (score_components > 0) {
        metrics->overall_health_score = health_score / score_components;
    } else {
        metrics->overall_health_score = 0.0f;
    }
    
    return AUTONOMY_SUCCESS;
}

// Collect network metrics for all interfaces
int network_collector_collect_metrics(void) {
    if (!g_collector_initialized) {
        printf("ERROR: "Network collector not initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    if (!g_collector.enabled) {
        printf("DEBUG: "Network collector disabled"\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Get current time
    time_t now = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Check if it's time to collect
    if (g_collector.last_collection > 0 && 
        (now - g_collector.last_collection) < g_collector.collection_interval) {
        pthread_mutex_unlock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_SUCCESS;
    }
    
    printf("DEBUG: "Starting network metrics collection"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Collect metrics for each interface
    for (int i = 0; i < g_collector.interface_count && i < MAX_INTERFACES; i++) {
        network_metrics_t metrics;
        int ret = collect_interface_metrics(g_collector.interfaces[i].name, &metrics\n"\n"\n"\n"\n"\n"\n"\n");
        
        if (ret == AUTONOMY_SUCCESS) {
            // Store in history
            int history_index = g_collector.metrics_history_index;
            memcpy(&g_collector.metrics_history[history_index], &metrics, sizeof(network_metrics_t)\n"\n"\n"\n"\n"\n"\n"\n");
            
            g_collector.metrics_history_index = (g_collector.metrics_history_index + 1) % g_collector.metrics_history_size;
            
            // Update interface with latest metrics
            memcpy(&g_collector.interfaces[i].metrics, &metrics, sizeof(network_metrics_t)\n"\n"\n"\n"\n"\n"\n"\n");
            
            printf("DEBUG: "Collected metrics for interface %s: health=%.1f%%, ping_loss=%.1f%%", 
                      metrics.interface_name, metrics.overall_health_score, metrics.ping_packet_loss\n"\n"\n"\n"\n"\n"\n"\n");
        } else {
            printf("WARN: "Failed to collect metrics for interface %s", g_collector.interfaces[i].name\n"\n"\n"\n"\n"\n"\n"\n");
        }
    }
    
    g_collector.last_collection = now;
    g_collector.total_collections++;
    
    pthread_mutex_unlock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("DEBUG: "Network metrics collection completed"\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_SUCCESS;
}

// Get latest metrics for an interface
int network_collector_get_interface_metrics(const char *interface_name, network_metrics_t *metrics) {
    if (!g_collector_initialized || !interface_name || !metrics) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Find interface
    for (int i = 0; i < g_collector.interface_count; i++) {
        if (strcmp(g_collector.interfaces[i].name, interface_name) == 0) {
            memcpy(metrics, &g_collector.interfaces[i].metrics, sizeof(network_metrics_t)\n"\n"\n"\n"\n"\n"\n"\n");
            pthread_mutex_unlock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
            return AUTONOMY_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_ERROR_NOT_FOUND;
}

// Get metrics history for an interface
int network_collector_get_metrics_history(const char *interface_name, network_metrics_t *history, 
                                        int max_count, int *actual_count) {
    if (!g_collector_initialized || !interface_name || !history || !actual_count) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    *actual_count = 0;
    
    // Find matching metrics in history
    for (int i = 0; i < g_collector.metrics_history_size && *actual_count < max_count; i++) {
        int index = (g_collector.metrics_history_index - 1 - i + g_collector.metrics_history_size) % g_collector.metrics_history_size;
        
        if (g_collector.metrics_history[index].timestamp > 0 &&
            strcmp(g_collector.metrics_history[index].interface_name, interface_name) == 0) {
            memcpy(&history[*actual_count], &g_collector.metrics_history[index], sizeof(network_metrics_t)\n"\n"\n"\n"\n"\n"\n"\n");
            (*actual_count)++;
        }
    }
    
    pthread_mutex_unlock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_SUCCESS;
}

// Add test target
int network_collector_add_test_target(const char *target) {
    if (!g_collector_initialized || !target) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (g_collector.test_target_count >= g_collector.max_test_targets) {
        pthread_mutex_unlock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_ALREADY_EXISTS;
    }
    
    // Check if target already exists
    for (int i = 0; i < g_collector.test_target_count; i++) {
        if (strcmp(g_collector.test_targets[i], target) == 0) {
            pthread_mutex_unlock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
            return AUTONOMY_ERROR_ALREADY_EXISTS;
        }
    }
    
    // Add new target
    strncpy(g_collector.test_targets[g_collector.test_target_count], target, 
             sizeof(g_collector.test_targets[0]) - 1\n"\n"\n"\n"\n"\n"\n"\n");
    g_collector.test_target_count++;
    
    pthread_mutex_unlock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "Added test target: %s", target\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_SUCCESS;
}

// Remove test target
int network_collector_remove_test_target(const char *target) {
    if (!g_collector_initialized || !target) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    for (int i = 0; i < g_collector.test_target_count; i++) {
        if (strcmp(g_collector.test_targets[i], target) == 0) {
            // Remove target by shifting remaining targets
            for (int j = i; j < g_collector.test_target_count - 1; j++) {
                strcpy(g_collector.test_targets[j], g_collector.test_targets[j + 1]\n"\n"\n"\n"\n"\n"\n"\n");
            }
            g_collector.test_target_count--;
            
            pthread_mutex_unlock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
            printf("INFO: "Removed test target: %s", target\n"\n"\n"\n"\n"\n"\n"\n");
            return AUTONOMY_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_ERROR_NOT_FOUND;
}

// Set collection interval
int network_collector_set_interval(int interval_seconds) {
    if (!g_collector_initialized || interval_seconds < 5) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    g_collector.collection_interval = interval_seconds;
    pthread_mutex_unlock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "Network collection interval set to %d seconds", interval_seconds\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_SUCCESS;
}

// Set test timeout
int network_collector_set_timeout(int timeout_seconds) {
    if (!g_collector_initialized || timeout_seconds < 1) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    g_collector.test_timeout = timeout_seconds;
    pthread_mutex_unlock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "Network test timeout set to %d seconds", timeout_seconds\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_SUCCESS;
}

// Enable/disable collector
int network_collector_set_enabled(bool enabled) {
    if (!g_collector_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    g_collector.enabled = enabled;
    pthread_mutex_unlock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "Network collector %s", enabled ? "enabled" : "disabled"\n"\n"\n"\n"\n"\n"\n"\n");
    return AUTONOMY_SUCCESS;
}

// Get collector status
int network_collector_get_status(network_collector_status_t *status) {
    if (!g_collector_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    status->enabled = g_collector.enabled;
    status->collection_interval = g_collector.collection_interval;
    status->test_timeout = g_collector.test_timeout;
    status->test_target_count = g_collector.test_target_count;
    status->interface_count = g_collector.interface_count;
    status->total_collections = g_collector.total_collections;
    status->last_collection = g_collector.last_collection;
    
    pthread_mutex_unlock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

// Cleanup network collector
void network_collector_cleanup(void) {
    if (!g_collector_initialized) {
        return;
    }
    
    pthread_mutex_lock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (g_collector.metrics_history) {
        free(g_collector.metrics_history\n"\n"\n"\n"\n"\n"\n"\n");
        g_collector.metrics_history = NULL;
    }
    
    g_collector_initialized = false;
    
    pthread_mutex_unlock(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    pthread_mutex_destroy(&g_collector_mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "Network collector cleaned up"\n"\n"\n"\n"\n"\n"\n"\n");
}
