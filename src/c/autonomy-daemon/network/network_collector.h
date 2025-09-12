#ifndef NETWORK_COLLECTOR_H
#define NETWORK_COLLECTOR_H

#include "../core/types.h"
#include "../utils/logx.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <sys/socket.h>

// MAX_INTERFACES is defined in ../core/types.h

// Maximum number of test targets
#define MAX_TEST_TARGETS 8

// Ping test result
typedef struct {
    char target[64];
    double latency_ms;
    bool success;
    time_t timestamp;
} ping_result_t;

// TCP connectivity test result
typedef struct {
    char target[64];
    int port;
    double connect_time_ms;
    bool success;
    time_t timestamp;
} tcp_result_t;

// DNS resolution test result
typedef struct {
    char domain[64];
    char resolved_ip[16];
    double resolve_time_ms;
    bool success;
    time_t timestamp;
} dns_result_t;

// Network metrics for an interface - using network_metrics_t from ../core/types.h

// Network collector status
typedef struct {
    bool enabled;
    int collection_interval;
    int test_timeout;
    int test_target_count;
    int interface_count;
    uint64_t total_collections;
    time_t last_collection;
} network_collector_status_t;

// Network collector state
typedef struct {
    bool enabled;
    int collection_interval;
    int test_timeout;
    int max_test_targets;
    int test_target_count;
    char test_targets[MAX_TEST_TARGETS][64];
    
    network_interface_t interfaces[MAX_INTERFACES];
    int interface_count;
    
    network_metrics_t *metrics_history;
    int metrics_history_size;
    int metrics_history_index;
    
    uint64_t total_collections;
    time_t last_collection;
} network_collector_t;

// Network archive statistics
typedef struct {
    int total_entries;
    float avg_latency_ms;
    float avg_packet_loss_pct;
    float avg_bandwidth_mbps;
    float avg_jitter_ms;
    time_t oldest_entry;
    time_t newest_entry;
} network_archive_stats_t;

// Initialize network collector
int network_collector_init(void);

// Collect network metrics for all interfaces
int network_collector_collect_metrics(void);

// Get latest metrics for an interface
int network_collector_get_interface_metrics(const char *interface_name, network_metrics_t *metrics);

// Get metrics history for an interface
int network_collector_get_metrics_history(const char *interface_name, network_metrics_t *history, 
                                        int max_count, int *actual_count);

// Add test target
int network_collector_add_test_target(const char *target);

// Remove test target
int network_collector_remove_test_target(const char *target);

// Set collection interval
int network_collector_set_interval(int interval_seconds);

// Set test timeout
int network_collector_set_timeout(int timeout_seconds);

// Enable/disable collector
int network_collector_set_enabled(bool enabled);

// Get collector status
int network_collector_get_status(network_collector_status_t *status);

// Cleanup network collector
void network_collector_cleanup(void);

// Archive-specific functions
int network_collector_archive_metrics(const network_metrics_t *metrics, const char *archive_path);
int network_collector_load_archived_metrics(const char *archive_path, network_metrics_t *metrics, int max_count);
int network_collector_cleanup_archived_metrics(const char *archive_path, int keep_days);
int network_collector_get_archive_stats(const char *archive_path, network_archive_stats_t *stats);

#endif // NETWORK_COLLECTOR_H
